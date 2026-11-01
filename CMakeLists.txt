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

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

include(GNUInstallDirs)

pkg_get_variable(GTK3_LIBDIR gtk+-3.0 libdir)
if(GTK3_LIBDIR)
    set(GTK3_MODULES_DIR "${GTK3_LIBDIR}/gtk-3.0/modules" CACHE PATH "Installation directory for GTK3 modules")
else()
    set(GTK3_MODULES_DIR "${CMAKE_INSTALL_LIBDIR}/gtk-3.0/modules" CACHE PATH "Installation directory for GTK3 modules")
endif()

install(TARGETS appmenu-gtk-module-wayland
    LIBRARY DESTINATION "${GTK3_MODULES_DIR}"
)

install(CODE "
    if(\"\${CMAKE_INSTALL_PREFIX}\" STREQUAL \"/usr\")
        set(_profile_dir \"/etc/profile.d\")
    elseif(IS_ABSOLUTE \"${CMAKE_INSTALL_SYSCONFDIR}\")
        set(_profile_dir \"${CMAKE_INSTALL_SYSCONFDIR}/profile.d\")
    else()
        set(_profile_dir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/profile.d\")
    endif()
    file(INSTALL DESTINATION \"\${_profile_dir}\" TYPE PROGRAM FILES \"${CMAKE_CURRENT_SOURCE_DIR}/appmenu-gtk-module-wayland.sh\")
")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/org.appmenu.gtk-module.gschema.xml"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas"
)

find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas REQUIRED)

install(CODE "
    if(IS_ABSOLUTE \"${CMAKE_INSTALL_DATADIR}\")
        set(_schema_dir \"${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas\")
    else()
        set(_schema_dir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas\")
    endif()
    if(\"\$ENV{DESTDIR}\" STREQUAL \"\")
        message(STATUS \"Compiling GSettings schemas in \${_schema_dir}...\")
        execute_process(COMMAND \"${GLIB_COMPILE_SCHEMAS}\" \"\${_schema_dir}\"
                        RESULT_VARIABLE _compile_res)
        if(NOT _compile_res EQUAL 0)
            message(FATAL_ERROR \"Failed to compile GSettings schemas in \${_schema_dir}\")
        endif()
    endif()
")

set(COMPILED_SCHEMAS_DIR "${CMAKE_CURRENT_BINARY_DIR}/schemas")
file(MAKE_DIRECTORY "${COMPILED_SCHEMAS_DIR}")
add_custom_command(
    OUTPUT "${COMPILED_SCHEMAS_DIR}/gschemas.compiled"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${COMPILED_SCHEMAS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/data/org.appmenu.gtk-module.gschema.xml" "${COMPILED_SCHEMAS_DIR}/"
    COMMAND "${GLIB_COMPILE_SCHEMAS}" "${COMPILED_SCHEMAS_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/org.appmenu.gtk-module.gschema.xml"
)
add_custom_target(compile_schemas ALL DEPENDS "${COMPILED_SCHEMAS_DIR}/gschemas.compiled")

option(BUILD_TESTS "Build test applications" OFF)

if(BUILD_TESTS)
    set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    add_executable(unity-gtk-menu-tester "${TEST_DIR}/demos/unity-gtk-menu-tester.c")
    target_link_libraries(unity-gtk-menu-tester PkgConfig::GTK3)

    enable_testing()
    # Keep the host runner independent of a toolchain's target Python.
    find_program(HOST_PYTHON3 NAMES python3 REQUIRED NO_CMAKE_FIND_ROOT_PATH)
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
    add_test(NAME mnemonics COMMAND ${HOST_PYTHON3} "${TEST_DIR}/run-mnemonics.py"
        ${MNEMONICS_RUNNER_ARGS} ${DBUS_RUN_SESSION} ${XVFB_RUN} $<TARGET_FILE:test-mnemonics>)
    set_tests_properties(mnemonics PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "GDK_BACKEND=x11;NO_AT_BRIDGE=1;GTK3_MODULES=;GTK_MODULES=;MESON_EXE_WRAPPER=;GSETTINGS_SCHEMA_DIR=${CMAKE_CURRENT_BINARY_DIR}/schemas")

    if(TARGET test-mnemonics AND TARGET compile_schemas)
        add_dependencies(test-mnemonics compile_schemas)
    endif()
endif()
