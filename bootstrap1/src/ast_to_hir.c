#include "ast_to_hir.h"
#include "com_assert.h"
#include "com_mem.h"
#include "com_str.h"

// the stuff that actually goes inside the identifier table
typedef struct {
  hir_Expr *expr;
  com_str identifier;
  // array of hir_Exprs that are deferred
  com_vec defers;
} LabelTableElem;

// utility method to allocate some noleak memory from the constructor
static void *hir_alloc(hir_Constructor *constructor, usize len) {
  return com_allocator_handle_get((com_allocator_alloc(
      constructor->_a, (com_allocator_HandleData){
                           .len = len,
                           .flags = com_allocator_defaults(constructor->_a) |
                                    com_allocator_NOLEAK})));
}

#define hir_alloc_obj_m(constructor, type)                                     \
  (type *)hir_alloc((constructor), sizeof(type))

static com_vec hir_alloc_vec(hir_Constructor *constructor) {
  return com_vec_create(com_allocator_alloc(
      constructor->_a,
      (com_allocator_HandleData){
          .len = 20, .flags = com_allocator_defaults(constructor->_a)}));
}

// creates a hir_Constructor using a
hir_Constructor hir_create(ast_Constructor *parser, com_allocator *a) {
  hir_Constructor hc;
  hc._a = a;
  hc._parser = parser;
  hc._label_table = hir_alloc_vec(&hc);
  return hc;
}

typedef struct {
  hir_Expr *expr;
  bool valid;
} MaybeLabel;

// Looks through a scope
static MaybeLabel hir_lookupLabel(hir_Constructor *constructor,
                                  com_str identifier) {
  // start from the last valid index, and iterate towards 0
  for (usize i_plus_one =
           com_vec_len_m(&constructor->_label_table, LabelTableElem);
       i_plus_one > 0; i_plus_one--) {
    const usize i = i_plus_one - 1;

    // get entry
    LabelTableElem entry =
        *com_vec_get_m(&constructor->_label_table, i, LabelTableElem);

    if (com_str_equal(entry.identifier, identifier)) {
      // if it matched our requirements, then set `result` and return true
      return (MaybeLabel){.expr = entry.expr, .valid = true};
    }
  }

  // if we went through all entries in this scope and found nothing
  return (MaybeLabel){.valid = false};
}

static hir_Common hir_getCommon(const ast_Common *in, bool generated, hir_Constructor * constructor) {
  com_vec strs = hir_alloc_vec(constructor) ;
  for(usize i = 0; i < in->metadata_len; i++) {
      com_reader_st
    com_vec_push_m(&strs, in->metadata[i]
  }
}

static com_vec hir_construct_Stmnt(const ast_Stmnt *in, DiagnosticLogger *diagnostics,
                                   hir_Constructor *constructor);

static hir_Expr *hir_construct_Expr(const ast_Expr *in, DiagnosticLogger *diagnostics,
                                    hir_Constructor *constructor);

static hir_Pat *hir_construct_Pat(const ast_Expr *in, DiagnosticLogger *diagnostics,
                                  hir_Constructor *constructor);

static com_vec hir_construct_Stmnt(const ast_Stmnt *in, DiagnosticLogger *diagnostics,
                                   hir_Constructor *constructor) {

  com_vec stmnts = hir_alloc_vec(constructor);
  switch (in->kind) {
  case ast_SK_None: {
    // empty vector
    break;
  }
  case ast_SK_Expr: {
    *com_vec_push_m(&stmnts, hir_Stmnt) =
        (hir_Stmnt){.kind = hir_SK_Expr,
                    .expr = {.expr = hir_construct_Expr(
                                 in->expr.expr, diagnostics, constructor)}};
    break;
  }
  case ast_SK_Assign: {
    *com_vec_push_m(&stmnts, hir_Stmnt) = (hir_Stmnt){
        .kind = hir_SK_Assign,
        .assign = {
            .pat = hir_construct_Pat(in->assign.pat, diagnostics, constructor),
            .val =
                hir_construct_Expr(in->assign.val, diagnostics, constructor)}};
    break;
  }
  case ast_SK_Defer: {
    *com_vec_push_m(&stmnts, hir_Stmnt) = (hir_Stmnt){
        .kind = hir_SK_Defer,
        .source = in,
        .deferStmnt = {.val = hir_construct_Expr(in->deferStmnt.val,
                                                 diagnostics, constructor)},
    };
  }
  }

  return stmnts;
}
