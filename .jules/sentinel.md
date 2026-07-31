# Sentinel's Journal - Critical Security Learnings Only

This journal contains only critical security learnings for this repository.

## 2026-07-31 - Secure Environment Variable Lookups in Shared GTK Modules
**Vulnerability:** Standard environment variable lookups (like `g_getenv`) in libraries/modules loaded by privileged (SUID/SGID) or capability-enabled binaries can allow unprivileged local users to influence or hijack the execution of those privileged processes, leading to local privilege escalation or security bypass.
**Learning:** Shared libraries and GTK modules are frequently loaded by a variety of system processes, some of which may run with elevated privileges. Using standard `getenv` or `g_getenv` without privilege boundary checks exposes these applications to potential environment manipulation attacks.
**Prevention:** Always use a secure environment lookup wrapper (`secure_getenv_wrapper`) that utilizes `secure_getenv` on GNU/Linux (glibc >= 2.17) and checks `issetugid()` on BSD/macOS systems before retrieving the environment variable value.
