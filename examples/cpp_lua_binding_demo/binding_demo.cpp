#include "binding_host.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <sstream>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}


/* ================================================================
** Lua-facing binding functions (called from demo.lua)
** ================================================================ */

/*
** bind(target_path, source_path)
**   One-way: when source changes, target is updated.
**   Usage: bind("ui.speedLabel.text", "extern.plc.motor.speed")
*/
static int l_bind(lua_State *L) {
  const char *target = luaL_checkstring(L, 1);
  const char *source = luaL_checkstring(L, 2);

  /* get the BindingHost from the registry */
  lua_pushstring(L, "lua_binding_host_ptr");
  lua_rawget(L, LUA_REGISTRYINDEX);
  auto *host = static_cast<BindingHost *>(
      const_cast<void *>(lua_topointer(L, -1)));
  lua_pop(L, 1);

  /* register a listener on source → writes to target */
  host->add_listener(source, [host, target = std::string(target)]
                              (const IecValue &v) {
    host->set(target, v, /*notify=*/true);
  });

  /* initial sync: copy source → target */
  host->set(target, host->get(source), /*notify=*/true);

  std::cout << "[bind] " << target << " <- " << source << "\n";
  return 0;
}


/*
** bind2way(path_a, path_b)
**   Two-way: changes in either direction sync to the other.
**   Usage: bind2way("ui.slider.value", "extern.plc.motor.speed")
*/
static int l_bind2way(lua_State *L) {
  const char *path_a = luaL_checkstring(L, 1);
  const char *path_b = luaL_checkstring(L, 2);

  lua_pushstring(L, "lua_binding_host_ptr");
  lua_rawget(L, LUA_REGISTRYINDEX);
  auto *host = static_cast<BindingHost *>(
      const_cast<void *>(lua_topointer(L, -1)));
  lua_pop(L, 1);

  /* A → B */
  host->add_listener(path_a, [host, target = std::string(path_b)]
                              (const IecValue &v) {
    host->set(target, v, /*notify=*/true);
  });

  /* B → A */
  host->add_listener(path_b, [host, target = std::string(path_a)]
                              (const IecValue &v) {
    host->set(target, v, /*notify=*/true);
  });

  /* initial sync: A → B */
  host->set(path_b, host->get(path_a), /*notify=*/true);

  std::cout << "[bind2way] " << path_a << " <=> " << path_b << "\n";
  return 0;
}


/*
** cpp_set(path, value)
**   Simulate external (PLC) writes from C++ side.
*/
static int l_cpp_set(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  IecValue v;

  switch (lua_type(L, 2)) {
    case LUA_TBOOLEAN:
      v = static_cast<bool>(lua_toboolean(L, 2));
      break;
    case LUA_TNUMBER:
      if (lua_isinteger(L, 2))
        v = static_cast<int>(lua_tointeger(L, 2));
      else
        v = static_cast<double>(lua_tonumber(L, 2));
      break;
    case LUA_TSTRING:
      v = std::string(lua_tostring(L, 2));
      break;
    default:
      return luaL_error(L, "unsupported value type for cpp_set");
  }

  lua_pushstring(L, "lua_binding_host_ptr");
  lua_rawget(L, LUA_REGISTRYINDEX);
  auto *host = static_cast<BindingHost *>(
      const_cast<void *>(lua_topointer(L, -1)));
  lua_pop(L, 1);

  host->set(path, v, /*notify=*/true);
  std::cout << "[cpp_set] " << path << " = ";
  std::visit([](auto &&val) { std::cout << val; }, v);
  std::cout << "  (from C++)\n";
  return 0;
}


/*
** cpp_print(path)
**   Print current value of a property from Lua.
*/
static int l_cpp_print(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);

  lua_pushstring(L, "lua_binding_host_ptr");
  lua_rawget(L, LUA_REGISTRYINDEX);
  auto *host = static_cast<BindingHost *>(
      const_cast<void *>(lua_topointer(L, -1)));
  lua_pop(L, 1);

  IecValue v = host->get(path);
  std::cout << "[value] " << path << " = ";
  std::visit([](auto &&val) { std::cout << val; }, v);
  std::cout << "\n";
  return 0;
}


/* ================================================================
** main
** ================================================================ */

int main(int argc, char **argv) {
  lua_State *L = luaL_newstate();
  if (!L) {
    std::cerr << "failed to create Lua state\n";
    return 1;
  }
  luaL_openlibs(L);

  /* ---- create host and register properties ---- */
  BindingHost host;

  /* IEC 61131-10 typed properties */
  host.add_property("extern.plc.motor.speed",  IecType::INT,    0);
  host.add_property("extern.plc.motor.temp",   IecType::REAL,   25.0);
  host.add_property("extern.plc.motor.running",IecType::BOOL,   false);
  host.add_property("extern.plc.motor.name",   IecType::STRING, std::string("MOTOR-01"));

  host.add_property("ui.slider.value",     IecType::INT,    0);
  host.add_property("ui.slider.min",       IecType::INT,    0);
  host.add_property("ui.slider.max",       IecType::INT,    100);
  host.add_property("ui.speedLabel.text",   IecType::STRING, std::string("0 rpm"));
  host.add_property("ui.tempLabel.text",    IecType::STRING, std::string("25.0 °C"));
  host.add_property("ui.statusLed.on",      IecType::BOOL,   false);

  host.add_property("state.pageName",       IecType::STRING, std::string("MotorPanel"));

  /* ---- expose host to Lua ---- */
  lua_pushstring(L, "lua_binding_host_ptr");
  lua_pushlightuserdata(L, &host);
  lua_rawset(L, LUA_REGISTRYINDEX);

  /* create proxy root tables: extern, ui, state */
  host.push_proxy_table(L, "extern");
  host.push_proxy_table(L, "ui");
  host.push_proxy_table(L, "state");

  /* register binding functions */
  lua_register(L, "lbs_bind",     l_bind);
  lua_register(L, "lbs_bind2way", l_bind2way);
  lua_register(L, "cpp_set",      l_cpp_set);
  lua_register(L, "cpp_print",    l_cpp_print);

  /* ---- load and run Lua script ---- */
  const std::filesystem::path script_path =
      argc > 1
          ? std::filesystem::path(argv[1])
          : std::filesystem::absolute(argv[0]).parent_path() / "demo.lua";

  std::cout << "=== C++/Lua Binding Demo ===\n";
  std::cout << "Loading: " << script_path.string() << "\n\n";

  if (luaL_dofile(L, script_path.string().c_str()) != LUA_OK) {
    std::cerr << "Lua error: " << lua_tostring(L, -1) << "\n";
    lua_close(L);
    return 1;
  }

  std::cout << "\n=== Demo Complete ===\n";
  lua_close(L);
  return 0;
}
