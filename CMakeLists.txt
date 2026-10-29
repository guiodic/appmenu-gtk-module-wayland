cmake_minimum_required(VERSION 3.28)

project(appmenu-gtk-module-wayland)

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

option(BUILD_TESTS "Build test applications" OFF)

if(BUILD_TESTS)
    set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    add_executable(unity-gtk-menu-tester "${TEST_DIR}/demos/unity-gtk-menu-tester.c")
    target_link_libraries(unity-gtk-menu-tester PkgConfig::GTK3)

    enable_testing()
    # The runner needs build-host Python, even when programs are searched only
    # in the target sysroot. Keep the toolchain's search mode outside this block.
    block(SCOPE_FOR VARIABLES PROPAGATE Python3_EXECUTABLE)
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    endblock()
    get_target_property(MODULE_SOURCES appmenu-gtk-module-wayland SOURCES)
    list(REMOVE_ITEM MODULE_SOURCES "${SRC_DIR}/datastructs.c")
    add_executable(test-mnemonics "${TEST_DIR}/test-mnemonics.c" ${MODULE_SOURCES})
    target_include_directories(test-mnemonics PRIVATE "${GENERATED_DIR}" "${LIB_DIR}")
    target_link_libraries(test-mnemonics PkgConfig::GTK3 PkgConfig::DBUSMENU_GTK3
        PkgConfig::DBUSMENU_GLIB PkgConfig::WAYLAND_CLIENT PkgConfig::X11)
    find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED NO_CMAKE_FIND_ROOT_PATH)
    find_program(XVFB_RUN xvfb-run REQUIRED NO_CMAKE_FIND_ROOT_PATH)
    set(MNEMONICS_RUNNER_ARGS --can-run-host-binaries yes)
    if(CMAKE_CROSSCOMPILING)
        set(MNEMONICS_RUNNER_ARGS --cross-build --can-run-host-binaries no)
        foreach(EMULATOR_ARG IN LISTS CMAKE_CROSSCOMPILING_EMULATOR)
            list(APPEND MNEMONICS_RUNNER_ARGS "--exe-wrapper=${EMULATOR_ARG}")
        endforeach()
    endif()
    add_test(NAME mnemonics COMMAND ${Python3_EXECUTABLE} "${TEST_DIR}/run-mnemonics.py"
        ${MNEMONICS_RUNNER_ARGS} ${DBUS_RUN_SESSION} ${XVFB_RUN} $<TARGET_FILE:test-mnemonics>)
    set_tests_properties(mnemonics PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "GDK_BACKEND=x11;NO_AT_BRIDGE=1;GTK3_MODULES=;GTK_MODULES=;MESON_EXE_WRAPPER=")
endif()
