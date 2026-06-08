#ifndef BINDING_HOST_H
#define BINDING_HOST_H

#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

extern "C" {
#include "lua.h"
}

/*
** IEC 61131-10 value type — covers the elementary type range.
*/
enum class IecType {
  BOOL,
  BYTE, WORD, DWORD, LWORD,
  SINT, INT, DINT, LINT,
  USINT, UINT, UDINT, ULINT,
  REAL, LREAL,
  TIME, LTIME, DT, LDT, TOD, LTOD,
  STRING, WSTRING, CHAR, WCHAR
};

const char *iec_type_name(IecType t);

/*
** Variant value holder for IEC 61131-10 types.
*/
using IecValue = std::variant<
  bool,
  int, long long,
  unsigned int, unsigned long long,
  double,
  std::string
>;

/*
** A single bound property.
*/
struct Property {
  std::string name;
  IecType type;
  IecValue value;
  /* listeners: called when this property is written */
  std::vector<std::function<void(const IecValue &)>> listeners;
};

/*
** Host-side binding context.
** Owns a set of named properties and exposes them to Lua via metatable proxies.
*/
class BindingHost {
public:
  BindingHost();

  /* property management */
  void add_property(const std::string &path, IecType type, IecValue initial);
  IecValue get(const std::string &path) const;
  void   set(const std::string &path, const IecValue &v, bool notify = true);

  /* listener registration */
  void add_listener(const std::string &path,
                    std::function<void(const IecValue &)> cb);

  /* Lua integration */
  void push_proxy_table(lua_State *L, const char *root_name);
  Property *find_property(const std::string &path);

private:
  /* path → Property */
  std::unordered_map<std::string, Property> props_;
};

/*
** Lua glue: metatable __index / __newindex callbacks.
** These are registered per proxy root (extern, ui, state, vm).
*/
int binding_host_index(lua_State *L);
int binding_host_newindex(lua_State *L);

/*
** Create a nested proxy table rooted at `root_name` with the metatable.
*/
void create_nested_proxy(lua_State *L, BindingHost *host,
                         const char *root_name);

#endif
