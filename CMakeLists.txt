cmake_minimum_required(VERSION 3.31)

project(appmenu-gtk-module-wayland)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

set(CMAKE_C_STANDARD 11)

set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")

add_library(appmenu-gtk3-parser SHARED
    # lib_sources
    "${LIB_DIR}/unity-gtk-action-group.c"
    "${LIB_DIR}/unity-gtk-action.c"
    "${LIB_DIR}/unity-gtk-menu-shell.c"
    "${LIB_DIR}/unity-gtk-menu-section.c"
    "${LIB_DIR}/unity-gtk-menu-item.c"
    # lib_private_headers
    "${LIB_DIR}/unity-gtk-action-group-private.h"
    "${LIB_DIR}/unity-gtk-action-private.h"
    "${LIB_DIR}/unity-gtk-menu-shell-private.h"
    "${LIB_DIR}/unity-gtk-menu-section-private.h"
    "${LIB_DIR}/unity-gtk-menu-item-private.h"
    # lib_headers
    "${LIB_DIR}/appmenu-gtk-action-group.h"
    "${LIB_DIR}/appmenu-gtk-menu-shell.h"
    "${LIB_DIR}/appmenu-gtk-parser.h"
)

target_include_directories(appmenu-gtk3-parser PUBLIC ${GTK3_INCLUDE_DIRS})
target_include_directories(appmenu-gtk3-parser PUBLIC ${LIB_DIR})
target_link_libraries(appmenu-gtk3-parser ${GTK3_LIBRARIES})
#target_compile_definitions(appmenu-gtk3-parser PUBLIC ${GTK3_CFLAGS_OTHER})

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(appmenu-gtk-module-wayland  SHARED
    "${SRC_DIR}/appmenu-gtk-module.c"
    "${SRC_DIR}/datastructs.c"
    "${SRC_DIR}/datastructs.h"
    "${SRC_DIR}/datastructs-private.h"
    "${SRC_DIR}/hijack.c"
    "${SRC_DIR}/hijack.h"
    "${SRC_DIR}/support.c"
    "${SRC_DIR}/support.h"
    "${SRC_DIR}/blacklist.c"
    "${SRC_DIR}/blacklist.h"
    "${SRC_DIR}/platform.c"
    "${SRC_DIR}/platform.h"
    "${SRC_DIR}/consts.h"
)

set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(unity-gtk-menu-tester "${TEST_DIR}/demos/unity-gtk-menu-tester.c")
target_include_directories(unity-gtk-menu-tester PUBLIC ${GTK3_INCLUDE_DIRS})
target_link_libraries(unity-gtk-menu-tester ${GTK3_LIBRARIES})
