/*
** $Id: lbind.c $
** LBS (Lua Binding Superset) — binding metadata management
** See Copyright Notice in lua.h
*/

#define lbind_c
#define LUA_CORE

#include "lprefix.h"

#include <string.h>

#include "lua.h"

#include "lbind.h"
#include "lmem.h"
#include "lstring.h"


/* ---- Lifecycle ---- */

LBS_CompileMetadata *lbsM_newmetadata (lua_State *L) {
  LBS_CompileMetadata *m;
  m = luaM_new(L, LBS_CompileMetadata);
  m->bindings = NULL;
  m->bindvars = NULL;
  m->derivedvars = NULL;
  m->nbindings = 0;
  m->nbindvars = 0;
  m->nderivedvars = 0;
  return m;
}


static void free_binddecl_list (lua_State *L, LBS_BindDecl *list) {
  while (list != NULL) {
    LBS_BindDecl *next = list->next;
    if (list->target != NULL) {
      luaM_freearray(L, list->target->segments, list->target->sizenseg);
      luaM_free(L, list->target);
    }
    if (list->condition != NULL) {
      luaM_freearray(L, list->condition->segments, list->condition->sizenseg);
      luaM_free(L, list->condition);
    }
    luaM_free(L, list);
    list = next;
  }
}


static void free_bindvar_list (lua_State *L, LBS_BindVarDecl *list) {
  while (list != NULL) {
    LBS_BindVarDecl *next = list->next;
    if (list->source != NULL) {
      luaM_freearray(L, list->source->segments, list->source->sizenseg);
      luaM_free(L, list->source);
    }
    luaM_free(L, list);
    list = next;
  }
}


static void free_derivedvar_list (lua_State *L, LBS_DerivedVarDecl *list) {
  while (list != NULL) {
    LBS_DerivedVarDecl *next = list->next;
    luaM_free(L, list);
    list = next;
  }
}


void lbsM_freemetadata (lua_State *L, LBS_CompileMetadata *m) {
  if (m == NULL) return;
  free_binddecl_list(L, m->bindings);
  free_bindvar_list(L, m->bindvars);
  free_derivedvar_list(L, m->derivedvars);
  luaM_free(L, m);
}


/* ---- Path ---- */

LBS_BindPath *lbsM_newpath (lua_State *L) {
  LBS_BindPath *p;
  p = luaM_new(L, LBS_BindPath);
  p->root = LBS_ROOT_UI;  /* default */
  p->segments = NULL;
  p->nseg = 0;
  p->sizenseg = 0;
  return p;
}


void lbsM_path_addseg (lua_State *L, LBS_BindPath *path, TString *seg) {
  int i = path->nseg;
  if (i + 1 > path->sizenseg) {  /* need to grow? */
    int newcap = (path->sizenseg == 0) ? 4 : path->sizenseg * 2;
    luaM_growvector(L, path->segments, i, newcap, TString *, INT_MAX,
                    "binding path too long");
    path->sizenseg = newcap;
  }
  path->segments[i] = seg;
  path->nseg++;
}


LBS_RootKind lbsM_root_from_string (const char *s) {
  if (strcmp(s, "ui") == 0)     return LBS_ROOT_UI;
  if (strcmp(s, "state") == 0)  return LBS_ROOT_STATE;
  if (strcmp(s, "extern") == 0) return LBS_ROOT_EXTERN;
  if (strcmp(s, "vm") == 0)     return LBS_ROOT_VM;
  return LBS_ROOT_UI;  /* unreachable (parser validates) */
}


const char *lbsM_root_to_string (LBS_RootKind root) {
  switch (root) {
    case LBS_ROOT_UI:     return "ui";
    case LBS_ROOT_STATE:  return "state";
    case LBS_ROOT_EXTERN: return "extern";
    case LBS_ROOT_VM:     return "vm";
    default:              return "?";
  }
}


/* ---- Decl additions ---- */

LBS_BindDecl *lbsM_newbinddecl (lua_State *L) {
  LBS_BindDecl *b;
  b = luaM_new(L, LBS_BindDecl);
  b->target = NULL;
  b->dir = LBS_ONEWAY;
  b->source_expr = NULL;
  b->converter = NULL;
  b->trigger = NULL;
  b->debounce_value = 0;
  b->debounce_unit = NULL;
  b->condition = NULL;
  b->onetime = 0;
  b->line = 0;
  b->next = NULL;
  return b;
}


void lbsM_addbinding (lua_State *L, LBS_CompileMetadata *m, LBS_BindDecl *b) {
  (void)L;
  b->next = m->bindings;
  m->bindings = b;
  m->nbindings++;
}


LBS_BindVarDecl *lbsM_newbindvar (lua_State *L) {
  LBS_BindVarDecl *v;
  v = luaM_new(L, LBS_BindVarDecl);
  v->name = NULL;
  v->type_name = NULL;
  v->source = NULL;
  v->mode = LBS_MODE_ONEWAY;
  v->converter = NULL;
  v->trigger = NULL;
  v->debounce_value = 0;
  v->debounce_unit = NULL;
  v->line = 0;
  v->next = NULL;
  return v;
}


void lbsM_addbindvar (lua_State *L, LBS_CompileMetadata *m,
                       LBS_BindVarDecl *v) {
  (void)L;
  v->next = m->bindvars;
  m->bindvars = v;
  m->nbindvars++;
}


LBS_DerivedVarDecl *lbsM_newderivedvar (lua_State *L) {
  LBS_DerivedVarDecl *d;
  d = luaM_new(L, LBS_DerivedVarDecl);
  d->name = NULL;
  d->type_name = NULL;
  d->expr_text = NULL;
  d->line = 0;
  d->next = NULL;
  return d;
}


void lbsM_addderivedvar (lua_State *L, LBS_CompileMetadata *m,
                          LBS_DerivedVarDecl *d) {
  (void)L;
  d->next = m->derivedvars;
  m->derivedvars = d;
  m->nderivedvars++;
}


/* ---- Compile result ---- */

LBS_CompileResult *lbsM_newcompileresult (lua_State *L) {
  LBS_CompileResult *r;
  r = luaM_new(L, LBS_CompileResult);
  r->success = 0;
  r->closure = NULL;
  r->metadata = NULL;
  return r;
}


void lbsM_freecompileresult (lua_State *L, LBS_CompileResult *r) {
  if (r == NULL) return;
  if (r->metadata != NULL)
    lbsM_freemetadata(L, r->metadata);
  luaM_free(L, r);
}
