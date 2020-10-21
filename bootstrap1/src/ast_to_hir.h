#ifndef HIR_CONSTRUCT_H
#define HIR_CONSTRUCT_H

#include "com_allocator.h"
#include "hir.h"
#include "tokens_to_ast.h"

// hir_Constructor should:
// Walk AST Tree, Clone
// Convert into tabular Format
// Resolve Variable Names

typedef struct {
  com_allocator *_a;
  ast_Constructor *_parser;

  // the vector holding labels
  com_vec _label_table;

} hir_Constructor;

hir_Constructor hir_create(ast_Constructor *parser, com_allocator *a);

bool hir_nextStmntAndCheckNext(hir_Stmnt *stmnt, DiagnosticLogger *diagnostics,
                               hir_Constructor *constructor);

void hir_destroy(hir_Constructor *constructor);

#endif
