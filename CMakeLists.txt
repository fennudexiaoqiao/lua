cmake_minimum_required(VERSION 3.21)

project(lua LANGUAGES C)

include(CTest)

option(LUA_BUILD_CLI "Build the standalone lua executable" ON)
option(LUA_BUILD_INTERNAL_TESTS "Build the internal ltests module into liblua" OFF)

set(LUA_CORE_SOURCES
  lapi.c
  lcode.c
  lctype.c
  ldebug.c
  ldo.c
  ldump.c
  lfunc.c
  lgc.c
  llex.c
  lmem.c
  lobject.c
  lopcodes.c
  lparser.c
  lstate.c
  lstring.c
  ltable.c
  ltm.c
  lundump.c
  lvm.c
  lzio.c
)

if(LUA_BUILD_INTERNAL_TESTS)
  list(APPEND LUA_CORE_SOURCES ltests.c)
endif()

set(LUA_LIB_SOURCES
  lauxlib.c
  lbaselib.c
  lcorolib.c
  ldblib.c
  linit.c
  liolib.c
  lmathlib.c
  loadlib.c
  loslib.c
  lstrlib.c
  ltablib.c
  lutf8lib.c
)

add_library(lua_static STATIC
  ${LUA_CORE_SOURCES}
  ${LUA_LIB_SOURCES}
)

add_library(lua::static ALIAS lua_static)

set_target_properties(lua_static PROPERTIES
  OUTPUT_NAME lua
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)

target_include_directories(lua_static
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(lua_static PUBLIC LUA_USE_LINUX)
elseif(APPLE)
  target_compile_definitions(lua_static PUBLIC LUA_USE_MACOSX)
endif()

if(MSVC)
  target_compile_definitions(lua_static PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(lua_static PUBLIC m dl)
elseif(APPLE)
  target_link_libraries(lua_static PUBLIC m)
endif()

if(LUA_BUILD_CLI)
  add_executable(lua_cli lua.c)

  set_target_properties(lua_cli PROPERTIES
    OUTPUT_NAME lua
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
  )

  if(MSVC)
    target_compile_definitions(lua_cli PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  target_link_libraries(lua_cli PRIVATE lua_static)

  if(BUILD_TESTING)
    add_test(NAME lua-version COMMAND $<TARGET_FILE:lua_cli> -v)
  endif()
endif()