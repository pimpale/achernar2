#ifndef AST_H_
#define AST_H_

#include <stdbool.h>
#include <stdint.h>

#include "error.h"
#include "lexer.h"
#include "token.h"

typedef enum {
  SK_None,
  SK_Use,
  SK_Namespace,
  SK_ValDecl,
  SK_TypeDecl,
  SK_PatDecl,
  SK_ValExpr,
  SK_TypeExpr,
  SK_PatExpr,
} StmntKind;

typedef enum {
  VEK_None,
  VEK_ConstExpr,
  VEK_StringLiteral,
  VEK_StructLiteral,
  VEK_BinaryOp,
  VEK_UnaryOp,
  VEK_Call,
  VEK_If,
  VEK_While,
  VEK_With,
  VEK_Pass,
  VEK_Break,
  VEK_Continue,
  VEK_Return,
  VEK_Match,
  VEK_Block,
  VEK_FieldAccess,
  VEK_Reference,
} ValueExprKind;

typedef enum {
  TEK_None,        // Error type
  TEK_Omitted,     // Omitted
  TEK_Void,        // void type
  TEK_Reference,   // Reference (primitive or aliased or path)
  TEK_Struct,      // struct
  TEK_Strop,       // Ast type of code
  TEK_Fn,          // function pointer
  TEK_UnaryOp,     // & or @
  TEK_BinaryOp,    // , or |
  TEK_FieldAccess, // .
} TypeExprKind;

typedef enum PatternExprValueRestrictionKind_e {
  PEVRK_CompEqual,        // ==
  PEVRK_CompNotEqual,     // !=
  PEVRK_CompLess,         // <
  PEVRK_CompLessEqual,    // <=
  PEVRK_CompGreater,      // >
  PEVRK_CompGreaterEqual, // >=
} PatternExprValueRestrictionKind;

typedef enum {
  PEK_None,             // Error type
  PEK_ValueRestriction, // matches a constant value, and optionally binds it
  PEK_TypeRestriction,  // matches a type, and optionally binds it
  PEK_Struct,           // a container for struct based patterns
  PEK_StructRest,       // (in struct) all values that were not matched
  PEK_UnaryOp,          // ()
  PEK_BinaryOp,         // , |
} PatternExprKind;

typedef enum ConstExprKind_e {
    CEK_None, // Error type
  CEK_IntLiteral, // 8
  CEK_BoolLiteral, // true
  CEK_FloatLiteral, // 8.32
  CEK_CharLiteral, // 'a'
  CEK_ValueExpr, // ${ 1 + 2 }
} ConstExprKind;

typedef struct TypeExpr_s TypeExpr;
typedef struct ConstExpr_s ConstExpr;
typedef struct ValueExpr_s ValueExpr;
typedef struct PatternExpr_s PatternExpr;
typedef struct Stmnt_s Stmnt;

typedef struct Comment_s {
  Span span;
  char *scope;
  char *data;
} Comment;

typedef struct ConstExpr_s {

  ConstExprKind kind;
  Span span;

  // diagnostics
  Diagnostic *diagnostics;
  size_t diagnostics_len;

  // comments
  Comment *comments;
  size_t comments_len;
  union {
    struct {
      uint64_t value;
    } intLiteral;
    struct {
      bool value;
    } boolLiteral;
    struct {
      double value;
    } floatLiteral;
    struct {
      char value;
    } charLiteral;
    struct {
      ValueExpr *expr;
    } valueExpr;
  };

} ConstExpr;

typedef struct PatternExpr_s {
  PatternExprKind kind;
  Span span;

  // diagnostics
  Diagnostic *diagnostics;
  size_t diagnostics_len;

  // comments
  Comment *comments;
  size_t comments_len;

  union {
    struct {
      PatternExprValueRestrictionKind restriction;
      ConstExpr *constExpr;
    } valueRestriction;
    struct {
      bool has_binding;
      char *binding;
      TypeExpr *type;
    } typeRestriction;
    struct {
      enum PatternStructExprKind_e {
        PESK_Struct,
        PESK_Pack,
        PESK_Union,
      } kind;

      struct PatternStructMemberExpr_s {
        enum PatternStructMemberExprKind_e {
          PSMEK_Field,
          PSMEK_Rest,
        } kind;
        Span span;
        Diagnostic *diagnostics;
        size_t diagnostics_len;
        // comments
        Comment *comments;
        size_t comments_len;
        struct {
            char *field;
            PatternExpr *pattern;
        } field;
      } * members;
      size_t members_len;
    } structExpr;
    struct {
      bool has_bindings;
      char *binding;
    } structRest;
    struct {
      enum PatternExprUnaryOpKind_e {
        PEUOK_Posit,
        PEUOK_Negate,
        PEUOK_Not,
      }
      operator;
      PatternExpr *operand;
    } unaryOp;
    struct {
      enum PatternExprBinaryOpKind_e {
        PEBOK_Tuple,
        PEBOK_Union,
        PEBOK_And,
        PEBOK_Or,
      }
      operator;
      PatternExpr *left_operand;
      PatternExpr *right_operand;
    } binaryOp;
  };
} PatternExpr;

typedef struct Path_s {
  Span span;
  // diagnostics
  Diagnostic *diagnostics;
  size_t diagnostics_len;

  // comments
  Comment *comments;
  size_t comments_len;

  char **pathSegments;
  size_t pathSegments_len;
} Path;

// Expressions and operations yielding a type
typedef struct TypeExpr_s {
  TypeExprKind kind;
  Span span;

  // diagnostics
  Diagnostic *diagnostics;
  size_t diagnostics_len;

  // comments
  Comment *comments;
  size_t comments_len;

  union {
    struct {
      Path *path;
    } referenceExpr;
    struct {
      enum TypeStructExprKind_e {
        TSEK_Struct,
        TSEK_Enum,
      } kind;

      struct TypeStructMemberExpr_s {
        Span span;
        Diagnostic *diagnostics;
        size_t diagnostics_len;

        // comments
        Comment *comments;
        size_t comments_len;

        char *name;
        TypeExpr *type;
      } * members;
      size_t members_len;
    } structExpr;
    struct {
      TypeExpr *parameters;
      size_t parameters_len;
      TypeExpr *type;
    } fnExpr;
    struct {
      enum TypeExprUnaryOpKind_e {
        TEUOK_Ref,   // $
        TEUOK_Deref, // @
      }
      operator;
      struct TypeExpr_s *operand;
    } unaryOp;
    struct {
      enum TypeExprBinaryOpKind_e {
        TEBOK_Tuple, // ,
        TEBOK_Union, // |
      }
      operator;
      struct TypeExpr_s *left_operand;
      struct TypeExpr_s *right_operand;
    } binaryOp;
    struct {
      struct TypeExpr_s *value;
      char *field;
    } fieldAccess;
  };
} TypeExpr;

typedef struct ValueExpr_s {
  ValueExprKind kind;
  Span span;

  // diagnostics
  Diagnostic *diagnostics;
  size_t diagnostics_len;

  // comments
  Comment *comments;
  size_t comments_len;

  union {
    struct {
      ConstExpr* constExpr;
    } constExpr;
    struct {
      char *value;
      size_t value_len;
    } stringLiteral;
    struct {
      struct ValueStructMemberExpr_s {
        Span span;
        Diagnostic *diagnostics;
        size_t diagnostics_len;

        // comments
        Comment *comments;
        size_t comments_len;

        char *name;
        ValueExpr *value;
      } * members;
      size_t members_len;
    } structExpr;
    struct {
      ValueExpr *value;
      char *field;
    } fieldAccess;
    struct {
      Path *path;
    } reference;
    struct {
      enum ValueExprUnaryOpKind_e {
        VEUOK_Negate,
        VEUOK_Posit,
        VEUOK_Not,
        VEUOK_Ref,
        VEUOK_Deref,
      }
      operator;
      ValueExpr *operand;
    } unaryOp;
    struct {
      enum ValueExprBinaryOpKind_e {
        VEBOK_Add,
        VEBOK_Sub,
        VEBOK_Mul,
        VEBOK_Div,
        VEBOK_Mod,
        VEBOK_And,
        VEBOK_Or,
        VEBOK_CompEqual,
        VEBOK_CompNotEqual,
        VEBOK_CompLess,
        VEBOK_CompLessEqual,
        VEBOK_CompGreater,
        VEBOK_CompGreaterEqual,
        VEBOK_Pipeline,
        VEBOK_Assign,
        VEBOK_AssignAdd,
        VEBOK_AssignSub,
        VEBOK_AssignMul,
        VEBOK_AssignDiv,
        VEBOK_AssignMod,
        VEBOK_Tuple,
      }
      operator;
      ValueExpr *left_operand;
      ValueExpr *right_operand;
    } binaryOp;
    struct {
      ValueExpr *condition;
      ValueExpr *body;
      bool has_else;
      ValueExpr *else_body;
    } ifExpr;
    struct {
      ValueExpr *condition;
      ValueExpr *body;
    } whileExpr;
    struct {
      ValueExpr *function;
      ValueExpr *parameters;
      size_t parameters_len;
    } callExpr;
    struct {
      PatternExpr *parameters;
      size_t parameters_len;
      TypeExpr *type;
      ValueExpr *body;
    } fnExpr;
    struct {
      ValueExpr *value;
    } returnExpr;
    struct {
      ValueExpr *value;
    } breakExpr;
    struct Match_s {
      ValueExpr *value;
      struct MatchCaseExpr_s {
        Span span;
        Diagnostic *diagnostics;
        size_t diagnostics_len;

        // comments
        Comment *comments;
        size_t comments_len;

        PatternExpr *pattern;
        ValueExpr *value;
      } * cases;
      size_t cases_len;
    } matchExpr;
    struct Block_s {
      Stmnt *statements;
      size_t statements_len;
      bool suppress_value;
    } blockExpr;
  };
} ValueExpr;

typedef struct Stmnt_s {
  StmntKind kind;
  Span span;
  // diagnostics
  Diagnostic *diagnostics;
  size_t diagnostics_len;

  // comments
  Comment *comments;
  size_t comments_len;
  union {
    // Declarations
    struct {
      PatternExpr *pattern;
      bool has_value;
      ValueExpr *value;
    } valDecl;
    struct {
      TypeExpr *type;
      char *name;
    } typeDecl;
    struct {
      PatternExpr *pattern;
      char *name;
    } patDecl;
    // Things
    struct {
      Path* path;
    } useStmnt;
    struct {
      Path* path;
    } namespaceStmnt;
    struct {
      char* name;
    } macroStmnt;
    // Expressions
    struct {
      ValueExpr *value;
    } valExpr;
    struct {
      TypeExpr *type;
    } typeExpr;
    struct {
      PatternExpr *pattern;
    } patExpr;
  };
} Stmnt;

typedef struct TranslationUnit_s {
  Span span; // span of the translation unit

  Diagnostic *diagnostics; // any errors that occur during parsing
  size_t diagnostics_len;

  Stmnt *statements;        // The top level is just a series of statements
  size_t statements_len; // The number of statements

  Comment *comments;      // top level comments
  size_t comments_len; // number of comments

} TranslationUnit;

#endif
