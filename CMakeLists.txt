cmake_minimum_required(VERSION 3.28)

project(appmenu-gtk-module-wayland)

# Install system-wide by default, while preserving an explicitly chosen prefix.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Installation prefix" FORCE)
endif()

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(DBUSMENU_GTK3 REQUIRED IMPORTED_TARGET dbusmenu-gtk3-0.4)
pkg_check_modules(DBUSMENU_GLIB REQUIRED IMPORTED_TARGET dbusmenu-glib-0.4)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

set(CMAKE_C_STANDARD 11)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wayland/generated")

add_library(appmenu-gtk-module-wayland  SHARED
    "${SRC_DIR}/appmenu-gtk-module.c"
    "${SRC_DIR}/datastructs.c"
    "${SRC_DIR}/hijack.c"
    "${SRC_DIR}/support.c"
    "${SRC_DIR}/blacklist.c"
    "${SRC_DIR}/platform.c"
    "${GENERATED_DIR}/appmenu.c"
    "${LIB_DIR}/unity-gtk-menu-item.c"
    "${LIB_DIR}/unity-gtk-menu-shell.c"
    "${LIB_DIR}/unity-gtk-action-group.c"
    "${LIB_DIR}/unity-gtk-action.c"
    "${LIB_DIR}/unity-gtk-menu-section.c"
)

target_include_directories(appmenu-gtk-module-wayland PRIVATE "${GENERATED_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/lib")
target_link_libraries(appmenu-gtk-module-wayland PkgConfig::GTK3 PkgConfig::DBUSMENU_GTK3 PkgConfig::DBUSMENU_GLIB PkgConfig::WAYLAND_CLIENT PkgConfig::X11)

# Use GNUInstallDirs defaults and allow distribution-specific destinations.
set(GTK_MODULE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/gtk-3.0/modules" CACHE STRING
    "GTK 3 module installation directory")
set(PROFILE_INSTALL_DIR "/etc/profile.d" CACHE STRING
    "Session environment script installation directory")
set(SCHEMA_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas")
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas REQUIRED NO_CMAKE_FIND_ROOT_PATH)
set(SCHEMA_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/schemas")
file(MAKE_DIRECTORY "${SCHEMA_BUILD_DIR}")
# Remove only the obsolete generated copy when reconfiguring an older build.
file(REMOVE "${SCHEMA_BUILD_DIR}/org.appmenu.gtk-module.gschema.xml")
configure_file(data/org.appmenu.gtk-module.wayland.gschema.xml
    "${SCHEMA_BUILD_DIR}/org.appmenu.gtk-module.wayland.gschema.xml" COPYONLY)
add_custom_command(OUTPUT "${SCHEMA_BUILD_DIR}/gschemas.compiled"
    COMMAND "${GLIB_COMPILE_SCHEMAS}" --strict "${SCHEMA_BUILD_DIR}"
    DEPENDS "${SCHEMA_BUILD_DIR}/org.appmenu.gtk-module.wayland.gschema.xml"
    VERBATIM)
add_custom_target(appmenu-schemas ALL DEPENDS "${SCHEMA_BUILD_DIR}/gschemas.compiled")
install(TARGETS appmenu-gtk-module-wayland LIBRARY DESTINATION "${GTK_MODULE_INSTALL_DIR}")
install(PROGRAMS appmenu-gtk-module-wayland.sh DESTINATION "${PROFILE_INSTALL_DIR}")

# Match Meson's optional systemd user-service installation.
pkg_check_modules(SYSTEMD QUIET systemd)
if(SYSTEMD_FOUND)
    pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemduserunitdir)
    find_program(DBUS_UPDATE_ACTIVATION_ENVIRONMENT dbus-update-activation-environment)
    if(DBUS_UPDATE_ACTIVATION_ENVIRONMENT)
        configure_file(data/appmenu-gtk-module-wayland.service.in
            appmenu-gtk-module-wayland.service @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/appmenu-gtk-module-wayland.service"
            DESTINATION "${SYSTEMD_USER_UNIT_DIR}")
    else()
        message(STATUS "Skipping optional systemd service: dbus-update-activation-environment not found")
    endif()
endif()

install(FILES data/org.appmenu.gtk-module.wayland.gschema.xml DESTINATION "${SCHEMA_INSTALL_DIR}")
configure_file(cmake/InstallSchemas.cmake.in InstallSchemas.cmake @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallSchemas.cmake")

option(BUILD_TESTS "Build test applications" OFF)

if(BUILD_TESTS)
    set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    add_executable(unity-gtk-menu-tester "${TEST_DIR}/demos/unity-gtk-menu-tester.c")
    target_link_libraries(unity-gtk-menu-tester PkgConfig::GTK3)

    enable_testing()
    # Keep the host runner independent of a toolchain's target Python.
    find_program(HOST_PYTHON3 NAMES python3 REQUIRED NO_CMAKE_FIND_ROOT_PATH)
    add_test(NAME session-environment COMMAND ${HOST_PYTHON3}
        "${TEST_DIR}/test-session-environment.py")
    get_target_property(MODULE_SOURCES appmenu-gtk-module-wayland SOURCES)
    list(REMOVE_ITEM MODULE_SOURCES "${SRC_DIR}/datastructs.c")
    add_executable(test-mnemonics "${TEST_DIR}/test-mnemonics.c" ${MODULE_SOURCES})
    target_include_directories(test-mnemonics PRIVATE "${GENERATED_DIR}" "${LIB_DIR}")
    target_link_libraries(test-mnemonics PkgConfig::GTK3 PkgConfig::DBUSMENU_GTK3
        PkgConfig::DBUSMENU_GLIB PkgConfig::WAYLAND_CLIENT PkgConfig::X11)
    add_executable(test-settings "${TEST_DIR}/test-settings.c"
        ${MODULE_SOURCES} "${SRC_DIR}/datastructs.c")
    target_include_directories(test-settings PRIVATE "${GENERATED_DIR}" "${LIB_DIR}")
    target_link_libraries(test-settings PkgConfig::GTK3 PkgConfig::DBUSMENU_GTK3
        PkgConfig::DBUSMENU_GLIB PkgConfig::WAYLAND_CLIENT PkgConfig::X11)
    add_dependencies(test-settings appmenu-schemas)
    find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED NO_CMAKE_FIND_ROOT_PATH)
    find_program(XVFB_RUN xvfb-run REQUIRED NO_CMAKE_FIND_ROOT_PATH)
    set(MNEMONICS_RUNNER_ARGS --can-run-host-binaries yes)
    if(CMAKE_CROSSCOMPILING)
        set(MNEMONICS_RUNNER_ARGS --cross-build --can-run-host-binaries no)
        foreach(EMULATOR_ARG IN LISTS CMAKE_CROSSCOMPILING_EMULATOR)
            list(APPEND MNEMONICS_RUNNER_ARGS "--exe-wrapper=${EMULATOR_ARG}")
        endforeach()
    endif()
    add_test(NAME settings COMMAND ${HOST_PYTHON3} "${TEST_DIR}/run-mnemonics.py"
        --single-run ${MNEMONICS_RUNNER_ARGS} ${DBUS_RUN_SESSION} ${XVFB_RUN} $<TARGET_FILE:test-settings>)
    set_tests_properties(settings PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT
        "GSETTINGS_SCHEMA_DIR=${SCHEMA_BUILD_DIR};GSETTINGS_BACKEND=memory;UBUNTU_MENUPROXY=1;GDK_BACKEND=x11;NO_AT_BRIDGE=1;GTK_MODULES=;GTK3_MODULES=;MESON_EXE_WRAPPER=")
    add_test(NAME mnemonics COMMAND ${HOST_PYTHON3} "${TEST_DIR}/run-mnemonics.py"
        ${MNEMONICS_RUNNER_ARGS} ${DBUS_RUN_SESSION} ${XVFB_RUN} $<TARGET_FILE:test-mnemonics>)
    set_tests_properties(mnemonics PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "GDK_BACKEND=x11;NO_AT_BRIDGE=1;GTK3_MODULES=;GTK_MODULES=;MESON_EXE_WRAPPER=")
endif()
