# Check the profile and the shell commands embedded in the systemd unit.
# This does not require a running systemd user manager.
import json, os, pathlib, shlex, subprocess, tempfile, sys
r=pathlib.Path(__file__).resolve().parents[1]
script=r/'appmenu-gtk-module-wayland.sh'
unit=(r/'data/appmenu-gtk-module-wayland.service.in').read_text().replace('\\\n','')
base={'PATH':os.defpath}
with tempfile.TemporaryDirectory() as tmp:
    mock=pathlib.Path(tmp)/'update-env'
    mock.write_text('#!' + sys.executable + '\nimport json, os\nprint(json.dumps({k:os.environ.get(k) for k in ["GTK_MODULES","UBUNTU_MENUPROXY"]}))\n')
    mock.chmod(0o755)
    commands={}
    for line in unit.splitlines():
        if line.startswith(('ExecCondition=','ExecStart=','ExecStopPost=')):
            k,v=line.split('=',1)
            commands[k]=[s.replace('$$','$').replace('@DBUS_UPDATE_ACTIVATION_ENVIRONMENT@',str(mock)) for s in shlex.split(v)]
    cases=[({},False),({'XDG_SESSION_TYPE':'x11'},False),({'XDG_SESSION_TYPE':'x11','WAYLAND_DISPLAY':'wayland-0'},False),({'XDG_SESSION_TYPE':'tty','WAYLAND_DISPLAY':'wayland-0'},False),({'XDG_SESSION_TYPE':'wayland'},True),({'WAYLAND_DISPLAY':'wayland-0'},True),({'XDG_SESSION_TYPE':'','WAYLAND_DISPLAY':'wayland-0'},True)]
    count=0
    for session,active in cases:
        for modules in ['', 'other', 'appmenu-gtk-module:other', 'other:appmenu-gtk-module-wayland']:
            for proxy in ['', '0', '1']:
                env=base|session|{'GTK_MODULES':modules,'UBUNTU_MENUPROXY':proxy}
                expected=modules
                if active and 'appmenu-gtk-module-wayland' not in modules.split(':'):
                    expected=modules+(':' if modules else '')+'appmenu-gtk-module-wayland'
                expected_proxy=(proxy or '1') if active else proxy
                check=f'. {shlex.quote(str(script))}; . {shlex.quote(str(script))}; exec {shlex.quote(str(mock))}'
                for shell in ['/bin/sh','/bin/bash']:
                    got=json.loads(subprocess.check_output([shell,'-uc',check],env=env,text=True))
                    assert got=={'GTK_MODULES':expected,'UBUNTU_MENUPROXY':expected_proxy},(session,modules,proxy,got)
                    count+=1
                cond=subprocess.run(commands['ExecCondition'],env=env)
                assert (cond.returncode==0)==active
                if active:
                    got=json.loads(subprocess.check_output(commands['ExecStart'],env=env,text=True))
                    assert got=={'GTK_MODULES':expected,'UBUNTU_MENUPROXY':expected_proxy},got
                    stop_env=env|{'GTK_MODULES':expected}
                    got=json.loads(subprocess.check_output(commands['ExecStopPost'],env=stop_env,text=True))
                    expected_stop=':'.join(m for m in expected.split(':') if m and m!='appmenu-gtk-module-wayland')
                    assert got['GTK_MODULES']==expected_stop,got
                    count+=2
    for shell in ['/bin/sh','/bin/bash']:
        got=json.loads(subprocess.check_output([shell,'-uc',f'. {shlex.quote(str(script))}; exec {shlex.quote(str(mock))}'],env=base|{'XDG_SESSION_TYPE':'wayland'},text=True))
        assert got=={'GTK_MODULES':'appmenu-gtk-module-wayland','UBUNTU_MENUPROXY':'1'}
    print(f'PASS: {count} session/profile/service checks plus unset-variable checks')
