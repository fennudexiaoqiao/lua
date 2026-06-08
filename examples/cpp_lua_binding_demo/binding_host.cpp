#include "binding_host.h"

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <sstream>

extern "C" {
#include "lauxlib.h"
}

/* ---- IEC type names ---- */

const char *iec_type_name(IecType t) {
  switch (t) {
    case IecType::BOOL:   return "BOOL";
    case IecType::BYTE:   return "BYTE";
    case IecType::WORD:   return "WORD";
    case IecType::DWORD:  return "DWORD";
    case IecType::LWORD:  return "LWORD";
    case IecType::SINT:   return "SINT";
    case IecType::INT:    return "INT";
    case IecType::DINT:   return "DINT";
    case IecType::LINT:   return "LINT";
    case IecType::USINT:  return "USINT";
    case IecType::UINT:   return "UINT";
    case IecType::UDINT:  return "UDINT";
    case IecType::ULINT:  return "ULINT";
    case IecType::REAL:   return "REAL";
    case IecType::LREAL:  return "LREAL";
    case IecType::TIME:   return "TIME";
    case IecType::LTIME:  return "LTIME";
    case IecType::DT:     return "DT";
    case IecType::LDT:    return "LDT";
    case IecType::TOD:    return "TOD";
    case IecType::LTOD:   return "LTOD";
    case IecType::STRING: return "STRING";
    case IecType::WSTRING:return "WSTRING";
    case IecType::CHAR:   return "CHAR";
    case IecType::WCHAR:  return "WCHAR";
  }
  return "?";
}


/* ---- BindingHost ---- */

BindingHost::BindingHost() = default;

void BindingHost::add_property(const std::string &path,
                                IecType type, IecValue initial) {
  Property p;
  p.name  = path;
  p.type  = type;
  p.value = std::move(initial);
  props_[path] = std::move(p);
}

IecValue BindingHost::get(const std::string &path) const {
  auto it = props_.find(path);
  if (it == props_.end())
    throw std::runtime_error("property not found: " + path);
  return it->second.value;
}

void BindingHost::set(const std::string &path, const IecValue &v,
                       bool notify) {
  auto it = props_.find(path);
  if (it == props_.end())
    throw std::runtime_error("property not found: " + path);
  /* skip if value unchanged — breaks binding cycles */
  if (it->second.value == v) return;
  it->second.value = v;
  if (notify) {
    /* copy listeners — they may modify the list during iteration */
    auto listeners = it->second.listeners;
    for (auto &cb : listeners)
      cb(v);
  }
}

void BindingHost::add_listener(const std::string &path,
                                std::function<void(const IecValue &)> cb) {
  auto it = props_.find(path);
  if (it == props_.end())
    throw std::runtime_error("property not found: " + path);
  it->second.listeners.push_back(std::move(cb));
}

Property *BindingHost::find_property(const std::string &path) {
  auto it = props_.find(path);
  return (it != props_.end()) ? &it->second : nullptr;
}


/* ---- Lua metatable glue ---- */

/*
** Registry key for the BindingHost pointer.
*/
static const char *const kBindingHostKey = "lua_binding_host_ptr";

static BindingHost *get_host(lua_State *L) {
  lua_pushstring(L, kBindingHostKey);
  lua_rawget(L, LUA_REGISTRYINDEX);
  auto *p = static_cast<BindingHost *>(
      const_cast<void *>(lua_topointer(L, -1)));
  lua_pop(L, 1);
  return p;
}

/*
** Build full path by combining table's _path field with the key being
** accessed.  E.g. _path="extern.plc.motor" + key="speed"
**       → "extern.plc.motor.speed"
*/
static std::string build_path(lua_State *L, int tbl_idx, const char *key) {
  std::string path;
  /* read _path from the proxy table */
  lua_pushstring(L, "_path");
  if (lua_rawget(L, tbl_idx) == LUA_TSTRING) {
    path = lua_tostring(L, -1);
    if (!path.empty()) path += ".";
  }
  lua_pop(L, 1);
  path += key;
  return path;
}


/*
** __index:  proxy.key
**   If the full path exists in the host → push value.
**   Otherwise → create and return a nested proxy table for the extended path.
*/
int binding_host_index(lua_State *L) {
  /* stack: table, key */
  BindingHost *host = get_host(L);
  const char *key = luaL_checkstring(L, 2);

  /* build full property path */
  std::string full_path = build_path(L, 1, key);

  /* check if it's a leaf property */
  Property *prop = host->find_property(full_path);
  if (prop != nullptr) {
    /* leaf: push the value */
    std::visit([L](auto &&v) {
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, bool>)
        lua_pushboolean(L, v ? 1 : 0);
      else if constexpr (std::is_same_v<T, int>)
        lua_pushinteger(L, static_cast<lua_Integer>(v));
      else if constexpr (std::is_same_v<T, long long>)
        lua_pushinteger(L, static_cast<lua_Integer>(v));
      else if constexpr (std::is_same_v<T, unsigned int>)
        lua_pushinteger(L, static_cast<lua_Integer>(v));
      else if constexpr (std::is_same_v<T, unsigned long long>)
        lua_pushinteger(L, static_cast<lua_Integer>(v));
      else if constexpr (std::is_same_v<T, double>)
        lua_pushnumber(L, static_cast<lua_Number>(v));
      else if constexpr (std::is_same_v<T, std::string>)
        lua_pushstring(L, v.c_str());
    }, prop->value);
    return 1;
  }

  /* intermediate node: create nested proxy table */
  create_nested_proxy(L, host, full_path.c_str());
  return 1;
}


/*
** __newindex:  proxy.key = value
**   Build full path, call host->set().
*/
int binding_host_newindex(lua_State *L) {
  /* stack: table, key, value */
  BindingHost *host = get_host(L);
  const char *key = luaL_checkstring(L, 2);

  std::string full_path = build_path(L, 1, key);

  /* parse Lua value → IecValue */
  IecValue v;
  int ltype = lua_type(L, 3);
  switch (ltype) {
    case LUA_TBOOLEAN:
      v = static_cast<bool>(lua_toboolean(L, 3));
      break;
    case LUA_TNUMBER:
      if (lua_isinteger(L, 3))
        v = static_cast<int>(lua_tointeger(L, 3));
      else
        v = static_cast<double>(lua_tonumber(L, 3));
      break;
    case LUA_TSTRING:
      v = std::string(lua_tostring(L, 3));
      break;
    default:
      return luaL_error(L, "unsupported value type for binding write");
  }

  host->set(full_path, v, /*notify=*/true);
  return 0;
}


/*
** Create a nested proxy table for `path` and leave it on the stack.
** Each proxy table has:
**   _path = "extern.plc.motor"   (used to build full paths)
**   metatable with __index / __newindex → binding_host_index / _newindex
*/
void create_nested_proxy(lua_State *L, BindingHost *host,
                         const char *path) {
  lua_newtable(L);                    /* proxy table */
  /* set _path field */
  lua_pushstring(L, "_path");
  lua_pushstring(L, path);
  lua_rawset(L, -3);

  /* set metatable */
  lua_newtable(L);                     /* mt */
  lua_pushcfunction(L, binding_host_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, binding_host_newindex);
  lua_setfield(L, -2, "__newindex");
  /* mark proxy tables so Lua prints them nicely */
  lua_pushstring(L, "binding proxy");
  lua_setfield(L, -2, "__name");
  lua_setmetatable(L, -2);             /* proxy.mt = mt */
}


/*
** Push a root proxy table (e.g. "extern", "ui") onto the stack and
** set it as a global.
*/
void BindingHost::push_proxy_table(lua_State *L, const char *root_name) {
  create_nested_proxy(L, this, root_name);
  lua_setglobal(L, root_name);
}
