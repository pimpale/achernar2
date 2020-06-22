#ifndef HIR_H
#define HIR_H

#include <stdint.h>

#include "ast.h"
#include "vector.h"

typedef enum {
  hir_BK_None,
  hir_BK_Bind,
  hir_BK_Ignore,
} hir_BindingKind;

typedef struct {
  hir_BindingKind kind;
  ast_Binding *source;
  union {
    struct {
      char *full;
      size_t id;
    } binding;
  };
} hir_Binding;

typedef enum {
  hir_RK_None,
  hir_RK_Path,
} hir_ReferenceKind;

typedef struct {
  hir_ReferenceKind kind;
  ast_Reference *source;
  union {
    struct {
      ast_Binding *first_decl;
      size_t id;
    } path;
  };
} hir_Reference;

typedef enum {
  hir_FK_None,
  hir_FK_Field,
} hir_FieldKind;

typedef struct {
  ast_Field *source;

} hir_Field;

typedef enum {
  hir_LK_Omitted,
  hir_LK_Label,
} hir_LabelKind;

typedef struct {
  ast_Label *source;
  hir_LabelKind kind;
  union {
    struct {
      char *label;
    } label;
  };
} hir_Label;

typedef struct hir_Type_s hir_Type;
typedef struct hir_Val_s hir_Val;
typedef struct hir_Pat_s hir_Pat;
typedef struct hir_Stmnt_s hir_Stmnt;

typedef enum {
  hir_PEVRK_CompEqual,        // ==
  hir_PEVRK_CompNotEqual,     // !=
  hir_PEVRK_CompLess,         // <
  hir_PEVRK_CompLessEqual,    // <=
  hir_PEVRK_CompGreater,      // >
  hir_PEVRK_CompGreaterEqual, // >=
} hir_PatValRestrictionKind;

typedef enum {
  hir_PK_None,            // Error type
  hir_PK_ValRestriction,  // matches a constant val
  hir_PK_TypeRestriction, // matches a type, with binding
  hir_PK_Struct,          // permits struct destructuring
  hir_PK_UnaryOp,         // !
  hir_PK_BinaryOp,        // , ||
} hir_PatKind;

typedef enum {
  hir_PEBOK_Tuple,
  hir_PEBOK_Union,
  hir_PEBOK_And,
  hir_PEBOK_Or,
} hir_PatBinaryOpKind;

typedef enum { hir_PEUOK_Not } hir_PatUnaryOpKind;

typedef enum {
  hir_PSMK_None,
  hir_PSMK_Field,
  hir_PSMK_Rest,
} hir_PatStructMemberKind;

typedef struct {
  // where it came from
  ast_PatStructMember *source;

  hir_PatStructMemberKind kind;
  union {
    struct {
      hir_Pat *pattern;
      hir_Field *field;
    } field;
    struct {
      hir_Pat *pattern;
    } rest;
  };
} hir_PatStructMember;

typedef struct hir_Pat_s {
  ast_Pat *source;

  hir_PatKind kind;
  union {
    struct {
      hir_PatValRestrictionKind restriction;
      hir_Val *val;
    } valRestriction;
    struct {
      hir_Type *type;
    } typeRestriction;
    struct {
      hir_Type *type;
      hir_Binding name;
    } typeRestrictionBinding;
    struct {
      hir_PatStructMember *members;
      size_t members_len;
    } structExpr;
    struct {
      hir_PatUnaryOpKind op;
      hir_Pat *operand;
    } unaryOp;
    struct {
      hir_PatBinaryOpKind op;
      hir_Pat *left_operand;
      hir_Pat *right_operand;
    } binaryOp;
  };
} hir_Pat;

typedef enum {
  hir_TK_None,        // Error type
  hir_TK_Omitted,     // Omitted
  hir_TK_Nil,         // Nil type
  hir_TK_Never,       // Never type
  hir_TK_Reference,   // Reference (primitive or aliased or path)
  hir_TK_Struct,      // struct
  hir_TK_Fn,          // function pointer
  hir_TK_UnaryOp,     // & or @
  hir_TK_BinaryOp,    // , or |
  hir_TK_FieldAccess, // .
} hir_TypeKind;

typedef enum {
  hir_TSK_Struct,
  hir_TSK_Enum,
} hir_TypeStructKind;

typedef enum {
  hir_TSMK_None,
  hir_TSMK_StructMember,
} hir_TypeStructMemberKind;

typedef struct hir_TypeStructMember_s {
  ast_TypeStructMember *source;

  hir_TypeStructMemberKind kind;
  union {
    struct {
      hir_Field *field;
      hir_Type *type;
    } structMember;
  };
} hir_TypeStructMember;

typedef enum {
  hir_TEUOK_Ref,   // $
  hir_TEUOK_Deref, // @
} hir_TypeUnaryOpKind;

typedef enum {
  hir_TEBOK_Tuple, // ,
  hir_TEBOK_Union, // |
} hir_TypeBinaryOpKind;

// essions and operations yielding a type
typedef struct hir_Type_s {
  ast_Type *source;
  hir_TypeKind kind;

  union {
    struct {
      hir_Reference *path;
    } reference;
    struct {
      hir_TypeStructKind kind;
      hir_TypeStructMember *members;
      size_t members_len;
    } structExpr;
    struct {
      hir_Type *parameters;
      size_t parameters_len;
      hir_Type *type;
    } fn;
    struct {
      hir_TypeUnaryOpKind op;
      hir_Type *operand;
    } unaryOp;
    struct {
      hir_TypeBinaryOpKind op;
      hir_Type *left_operand;
      hir_Type *right_operand;
    } binaryOp;
    struct {
      hir_Type *root;
      hir_Field *field;
    } fieldAccess;
  };
} hir_Type;

typedef enum {
  hir_VK_None,
  hir_VK_NilLiteral,
  hir_VK_BoolLiteral,
  hir_VK_IntLiteral,
  hir_VK_FloatLiteral,
  hir_VK_CharLiteral,
  hir_VK_Fn,
  hir_VK_Loop,
  hir_VK_As,
  hir_VK_StringLiteral,
  hir_VK_StructLiteral,
  hir_VK_BinaryOp,
  hir_VK_UnaryOp,
  hir_VK_Call,
  hir_VK_Return,
  hir_VK_Match,
  hir_VK_Block,
  hir_VK_FieldAccess,
  hir_VK_Reference,
} hir_ValKind;

typedef enum {
  hir_MCK_None,
  hir_MCK_Case,
} hir_MatchCaseKind;

typedef struct {
  ast_MatchCase *source;
  hir_MatchCaseKind kind;

  union {
    struct {
      hir_Pat *pattern;
      hir_Val *val;
    } matchCase;
  };

} hir_MatchCase;

typedef enum {
  hir_VSMK_None,
  hir_VSMK_Member,
} hir_ValStructMemberKind;

typedef struct {
  ast_ValStructMember *source;

  hir_ValStructMemberKind kind;

  union {
    struct {
      hir_Field *field;
      hir_Val *val;
    } member;
  };
} hir_ValStructMember;

typedef enum {
  hir_VEUOK_Negate,
  hir_VEUOK_Posit,
  hir_VEUOK_Not,
  hir_VEUOK_Ref,
  hir_VEUOK_Deref,
} hir_ValUnaryOpKind;

typedef enum {
  hir_VEBOK_Add,
  hir_VEBOK_Sub,
  hir_VEBOK_Mul,
  hir_VEBOK_Div,
  hir_VEBOK_Mod,
  hir_VEBOK_And,
  hir_VEBOK_Or,
  hir_VEBOK_CompEqual,
  hir_VEBOK_CompNotEqual,
  hir_VEBOK_CompLess,
  hir_VEBOK_CompLessEqual,
  hir_VEBOK_CompGreater,
  hir_VEBOK_CompGreaterEqual,
  hir_VEBOK_Pipeline,
  hir_VEBOK_Assign,
  hir_VEBOK_AssignAdd,
  hir_VEBOK_AssignSub,
  hir_VEBOK_AssignMul,
  hir_VEBOK_AssignDiv,
  hir_VEBOK_AssignMod,
  hir_VEBOK_Tuple,
} hir_ValBinaryOpKind;

typedef struct hir_Val_s {
  ast_Val *source;

  hir_ValKind kind;

  union {
    struct {
      bool value;
    } boolLiteral;
    struct {
      uint64_t value;
    } intLiteral;
    struct {
      double value;
    } floatLiteral;
    struct {
      char value;
    } charLiteral;
    struct {
      char *value;
      size_t value_len;
    } stringLiteral;
    struct {
      hir_ValStructMember *members;
      size_t members_len;
    } structExpr;
    struct {
      hir_Val *root;
      hir_Type *type;
    } as;
    struct {
      hir_Val *body;
      hir_Label *label;
    } loop;
    struct {
      hir_Val *root;
      hir_Field *field;
    } fieldAccess;
    struct {
      hir_Reference *path;
    } reference;
    struct {
      hir_ValUnaryOpKind op;
      hir_Val *operand;
    } unaryOp;
    struct {
      hir_ValBinaryOpKind op;
      hir_Val *left_operand;
      hir_Val *right_operand;
    } binaryOp;
    struct {
      hir_Val *function;
      hir_Val *parameters;
      size_t parameters_len;
    } call;
    struct {
      hir_Pat *parameters;
      size_t parameters_len;
      hir_Type *type;
      hir_Val *body;
    } fn;
    struct {
      hir_Val *value;
      hir_Label *label;
    } returnExpr;
    struct {
      hir_Val *root;
      hir_MatchCase *cases;
      size_t cases_len;
    } match;
    struct {
      hir_Label *label;
      hir_Stmnt *stmnts;
      size_t stmnts_len;
    } block;
  };
} hir_Val;

typedef enum {
  hir_SK_None,
  hir_SK_ValDecl,
  hir_SK_ValDeclDefine,
  hir_SK_TypeDecl,
  hir_SK_Val,
  hir_SK_DeferStmnt,
} hir_StmntKind;

typedef struct hir_Stmnt_s {
  ast_Stmnt *source;

  hir_StmntKind kind;

  union {
    // Declarations
    struct {
      hir_Pat *pat;
    } valDecl;
    struct {
      hir_Binding *name;
      hir_Type *type;
    } typeDecl;
    struct {
      hir_Val *val;
    } val;
  };
} hir_Stmnt;

const char *hir_strPatValRestrictionKind(ast_PatValRestrictionKind val);
const char *hir_strPatKind(ast_PatKind val);
const char *hir_strPatBinaryOpKind(ast_PatBinaryOpKind val);
const char *hir_strPatStructMemberKind(ast_PatStructMemberKind val);
const char *hir_strTypeKind(ast_TypeKind val);
const char *hir_strTypeStructKind(ast_TypeStructKind val);
const char *hir_strTypeStructMemberKind(ast_TypeStructMemberKind val);
const char *hir_strTypeUnaryOpKind(ast_TypeUnaryOpKind val);
const char *hir_strTypeBinaryOpKind(ast_TypeBinaryOpKind val);
const char *hir_strValKind(ast_ValKind val);
const char *hir_strLabelKind(ast_LabelKind val);
const char *hir_strMatchCaseKind(ast_MatchCaseKind val);
const char *hir_strValStructMemberKind(ast_ValStructMemberKind val);
const char *hir_strValUnaryOpKind(ast_ValUnaryOpKind val);
const char *hir_strValBinaryOpKind(ast_ValBinaryOpKind val);
const char *hir_strStmntKind(ast_StmntKind val);
const char *hir_strPatUnaryOpKind(ast_PatUnaryOpKind val);
const char *hir_strBindingKind(ast_BindingKind val);
const char *hir_strFieldKind(ast_FieldKind val);
const char *hir_strReferenceKind(ast_ReferenceKind val);

#endif
