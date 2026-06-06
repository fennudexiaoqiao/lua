#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace {

int cpp_log(lua_State* state) {
  const char* message = luaL_checkstring(state, 1);
  std::cout << "[C++] log from Lua: " << message << '\n';
  return 0;
}

int cpp_scale_damage(lua_State* state) {
  const lua_Integer base_damage = luaL_checkinteger(state, 1);
  const lua_Number multiplier = luaL_checknumber(state, 2);
  lua_pushinteger(state, static_cast<lua_Integer>(base_damage * multiplier));
  return 1;
}

void push_player(lua_State* state, const std::string& name,
                 lua_Integer hit_points, lua_Integer attack) {
  lua_createtable(state, 0, 3);

  lua_pushlstring(state, name.c_str(), name.size());
  lua_setfield(state, -2, "name");

  lua_pushinteger(state, hit_points);
  lua_setfield(state, -2, "hp");

  lua_pushinteger(state, attack);
  lua_setfield(state, -2, "attack");
}

void call_lua_battle(lua_State* state) {
  lua_getglobal(state, "simulate_battle_round");
  if (!lua_isfunction(state, -1)) {
    throw std::runtime_error("simulate_battle_round is not defined in Lua");
  }

  push_player(state, "Knight", 120, 18);
  push_player(state, "Slime", 35, 6);

  if (lua_pcall(state, 2, 2, 0) != LUA_OK) {
    throw std::runtime_error(lua_tostring(state, -1));
  }

  const lua_Integer damage = luaL_checkinteger(state, -2);
  const char* summary = luaL_checkstring(state, -1);

  std::cout << "[C++] battle damage from Lua: " << damage << '\n';
  std::cout << "[C++] summary from Lua: " << summary << '\n';

  lua_pop(state, 2);
}

}  // namespace

int main(int argc, char** argv) {
  lua_State* state = luaL_newstate();
  if (state == nullptr) {
    std::cerr << "failed to create lua state\n";
    return 1;
  }

  luaL_openlibs(state);
  lua_register(state, "cpp_log", cpp_log);
  lua_register(state, "cpp_scale_damage", cpp_scale_damage);

  int exit_code = 0;
  const std::filesystem::path script_path =
      argc > 1
          ? std::filesystem::path(argv[1])
          : std::filesystem::absolute(argv[0]).parent_path() / "cpp_lua_demo.lua";

  if (luaL_dofile(state, script_path.string().c_str()) != LUA_OK) {
    std::cerr << "failed to load lua script: " << lua_tostring(state, -1) << '\n';
    lua_close(state);
    return 1;
  }

  try {
    call_lua_battle(state);
  }
  catch (const std::exception& error) {
    std::cerr << "example failed: " << error.what() << '\n';
    exit_code = 1;
  }

  lua_close(state);
  return exit_code;
}