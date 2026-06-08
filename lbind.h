/*
** $Id: lbind.h $
** LBS (Lua Binding Superset) — binding metadata structures
** See Copyright Notice in lua.h
*/

#ifndef lbind_h
#define lbind_h

#include "lobject.h"
#include "llex.h"


/*
** Host root object kind.
** Maps to the four pre-defined host proxy roots in LBS.
*/
typedef enum {
  LBS_ROOT_UI      = 0,  /* XML UI object tree */
  LBS_ROOT_STATE   = 1,  /* page or module state */
  LBS_ROOT_EXTERN  = 2,  /* PLC, device, remote variable */
  LBS_ROOT_VM      = 3   /* ViewModel or host-exposed logic object */
} LBS_RootKind;


/*
** Binding direction.
*/
typedef enum {
  LBS_ONEWAY  = 0,  /* '<-'  one-way binding */
  LBS_TWOWAY  = 1   /* '<=>' two-way binding */
} LBS_BindDir;


/*
** Binding mode for bound variables.
*/
typedef enum {
  LBS_MODE_ONEWAY  = 0,
  LBS_MODE_TWOWAY  = 1
} LBS_BindMode;


/*
** A fully-resolved binding path, e.g. ui.label.text.
** Owns an array of TString* segments.
*/
typedef struct LBS_BindPath {
  LBS_RootKind root;       /* host root kind */
  TString **segments;      /* array of TString* path segments */
  int nseg;                /* number of segments in use */
  int sizenseg;            /* capacity of segments array */
} LBS_BindPath;


/*
** A binding declaration:  bind target <- expr
**                          bind target <=> source [converter...] [trigger...] [debounce...]
*/
typedef struct LBS_BindDecl {
  LBS_BindPath *target;    /* left-hand side target path */
  LBS_BindDir dir;         /* direction: one-way or two-way */
  struct expdesc *source_expr;  /* right-hand side expression (B1 placeholder; B3+ refines) */
  TString *converter;      /* converter name, or NULL */
  TString *trigger;        /* trigger name, or NULL */
  int debounce_value;      /* debounce numeric value, or 0 */
  TString *debounce_unit;  /* unit string ("ms", "s"), or NULL */
  LBS_BindPath *condition; /* guard path for conditional binding, or NULL */
  int onetime;             /* true for one-shot binding (evaluate once, no listener) */
  int line;                /* source line number */
  struct LBS_BindDecl *next;   /* linked list */
} LBS_BindDecl;


/*
** A bound variable declaration:
**   var name: Type bind source [mode twoway|oneway] [optional...]
*/
typedef struct LBS_BindVarDecl {
  TString *name;           /* variable name */
  TString *type_name;      /* IEC 61131-10 type name (e.g. "INT", "BOOL") */
  LBS_BindPath *source;    /* bound source path (extern.xxx / ui.xxx / ...) */
  LBS_BindMode mode;       /* oneway or twoway */
  TString *converter;
  TString *trigger;
  int debounce_value;
  TString *debounce_unit;
  int line;
  struct LBS_BindVarDecl *next;
} LBS_BindVarDecl;


/*
** A derived variable declaration:
**   var name: Type <- expr
*/
typedef struct LBS_DerivedVarDecl {
  TString *name;
  TString *type_name;
  TString *expr_text;      /* source text of derivation expression */
  int line;
  struct LBS_DerivedVarDecl *next;
} LBS_DerivedVarDecl;


/*
** Compile-time binding metadata for one compilation unit (script block).
*/
typedef struct LBS_CompileMetadata {
  LBS_BindDecl *bindings;            /* linked list of binding declarations */
  LBS_BindVarDecl *bindvars;         /* linked list of bound variable declarations */
  LBS_DerivedVarDecl *derivedvars;   /* linked list of derived variable declarations */
  int nbindings;
  int nbindvars;
  int nderivedvars;
} LBS_CompileMetadata;


/*
** Full compile result: Lua closure + binding metadata.
*/
typedef struct LBS_CompileResult {
  int success;
  struct LClosure *closure;
  LBS_CompileMetadata *metadata;
} LBS_CompileResult;



/* ---- Lifecycle ---- */

LUAI_FUNC LBS_CompileMetadata *lbsM_newmetadata (lua_State *L);
LUAI_FUNC void lbsM_freemetadata (lua_State *L, LBS_CompileMetadata *m);

/* ---- Path ---- */

LUAI_FUNC LBS_BindPath *lbsM_newpath (lua_State *L);
LUAI_FUNC void lbsM_path_addseg (lua_State *L, LBS_BindPath *path,
                                  TString *seg);
LUAI_FUNC LBS_RootKind lbsM_root_from_string (const char *s);
LUAI_FUNC const char *lbsM_root_to_string (LBS_RootKind root);

/* ---- Decl additions ---- */

LUAI_FUNC LBS_BindDecl *lbsM_newbinddecl (lua_State *L);
LUAI_FUNC void lbsM_addbinding (lua_State *L, LBS_CompileMetadata *m,
                                 LBS_BindDecl *b);
LUAI_FUNC LBS_BindVarDecl *lbsM_newbindvar (lua_State *L);
LUAI_FUNC void lbsM_addbindvar (lua_State *L, LBS_CompileMetadata *m,
                                 LBS_BindVarDecl *v);
LUAI_FUNC LBS_DerivedVarDecl *lbsM_newderivedvar (lua_State *L);
LUAI_FUNC void lbsM_addderivedvar (lua_State *L, LBS_CompileMetadata *m,
                                    LBS_DerivedVarDecl *d);

/* ---- Compile result ---- */

LUAI_FUNC LBS_CompileResult *lbsM_newcompileresult (lua_State *L);
LUAI_FUNC void lbsM_freecompileresult (lua_State *L, LBS_CompileResult *r);

#endif
