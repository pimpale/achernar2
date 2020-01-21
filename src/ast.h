#ifndef AST_H_
#define AST_H_

#include <stdbool.h>
#include <stdint.h>

#include "identifier.h"

typedef enum {
  StmntFuncDec,
  StmntVarDec,
  StmntStructDec,
  StmntExpr,
  StmntTypeAlias,
} StmntType;

typedef enum {
  ExprIntLiteral,
  ExprFloatLiteral,
  ExprCharLiteral,
  ExprStringLiteral,
  ExprArrayLiteral,
  ExprStructLiteral,
  ExprBinaryOp,
  ExprUnaryOp,
  ExprIndex,
  ExprCall,
  ExprFieldAccess,
  ExprPipe,
  ExprIf,
  ExprIfElse,
  ExprWhile,
  ExprFor,
  ExprWith,
  ExprBreak,
  ExprContinue,
  ExprReturn,
  ExprMatch,
  ExprBlock,
} ExprType;

// Need to forward declare all structs because recursion
struct Expr_s; // Expression

// Literal
struct IntLiteralExpr_s; // Integer Literal Expression
struct FloatLiteralExpr_s; // Float Literal Expression
struct CharLiteralExpr_s; // Float Literal Expression
struct StringLiteralExpr_s; // Float Literal Expression
struct ArrayLiteralExpr_s; // Array Literal Expression
struct StructLiteralExpr_s; // Array Literal Expression
// Operators
struct BinaryOpExpr_s; // Binary Operator Expression
struct UnaryOpExpr_s; // Unary Operator Expression
struct IndexExpr_s; // Index Expresson
// Misc
struct CallExpr_s; // Function Call Expression
struct FieldAccessExpr_s; // Field Access Expression
struct PipeExpr_s; // Pipeline Expression
// Control Flow
struct IfExpr_s; // If Expression
struct IfElseExpr_s; // If Else Expression
struct WhileExpr_s; // While Expression
struct ForExpr_s; // For Expression
struct WithExpr_s; // With Expression
struct BreakExpr_s; // Break Expression
struct ContinueExpr_s; // Continue Expression
struct ReturnExpr_s; // Return Expression
struct MatchExpr_s; // Match Expression
struct BlockExpr_s; // Expression in Parentheses

struct Stmnt_s; // Statement
// Declarations
struct FuncDecStmnt_s; // Function Declaration Statement
struct VarDecStmnt_s; // Variable Declaration Statement
struct StructDecStmnt_s; // Struct Declaration Statement
// Misc Statements
struct ExprStmnt_s; // Expression Statement
struct TypeAliasStmnt_s; // Type Alias Statement

typedef struct Expr_s {
  ExprType type;
  void* value;
} Expr;

typedef struct IntLiteralExpr_s {
  uint64_t value;
} IntLiteralExpr;

typedef struct FloatLiteralExpr_s {
  double value;
} FloatLiteralExpr;

typedef struct CharLiteralExpr_s {
  char value;
} CharLiteralExpr;

typedef struct StringLiteralExpr_s {
  char* value;
  uint64_t length; // Number of characters in string
} StringLiteralExpr;

typedef struct ArrayLiteralExpr_s {
  Identifier* type; // Type of array
  Expr* elements;  // List
  uint64_t length; // Number of elements
} ArrayLiteralExpr;

typedef struct StructLiteralExpr_s {
  Identifier* structName; // Name of struct
  IdentifierValuePair* structAttributes; // List of structAttributes
  uint64_t length; // number of structAttributes specified
} StructLiteralExpr;

typedef enum {
  BinaryOpExprAdd,             // +
  BinaryOpExprSub,             // -
  BinaryOpExprMul,             // *
  BinaryOpExprDiv,             // /
  BinaryOpExprMod,             // %
  BinaryOpExprBitAnd,          // &
  BinaryOpExprBitOr,           // |
  BinaryOpExprBitXor,          // ^
  BinaryOpExprBitShl,          // <<
  BinaryOpExprBitShr,          // >>
  BinaryOpExprLogicalAnd,      // &&
  BinaryOpExprLogicalOr,       // ||
  BinaryOpExprCompEqual,       // ==
  BinaryOpExprCompNotEqual,    // !=
  BinaryOpExprCompLess,        // <
  BinaryOpExprCompLessEqual,   // <=
  BinaryOpExprCompGreater,     // >
  BinaryOpExprCompGreaterEqual // >=
} BinaryOpExprType;

typedef struct BinaryOpExpr_s {
  BinaryOpExprType type;
  Expr* a; // First operand
  Expr* b; // Second operand
} BinaryOpExpr;

typedef enum {
  UnaryOpExprNegate,   // -
  UnaryOpExprLogicalNot, // !
  UnaryOpExprBitNot,   // ~
  UnaryOpExprRef, // $
  UnaryOpExprDeref // @
} UnaryOpExprType;

typedef struct UnaryOpExpr_s {
  UnaryOpExprType type;
  Expr* a; // Operand
} UnaryOpExpr;

typedef struct IndexExpr_s {
  Expr* pointer; // The pointer to be referenced
  Expr* index; // Expression evaluating to the index
} IndexExpr;

typedef struct CallExpr_s {
  Expr* function; // The function being called
  Expr* arguments; // The arguments to this function
  uint64_t length; // Number of arguments
} CallExpr;

typedef struct FieldAccessExpr_s {
  Expr* record;  // the struct
  Identifier* field; // the field of the struct
} FieldAccessExpr;

typedef struct PipeExpr_s {
  Expr* source;
  Expr* transformation;
} PipeExpr;

typedef struct IfExpr_s {
  Expr* condition;
  Expr* body;
} IfExpr;

typedef struct IfElseExpr_s {
  Expr* condition;
  Expr* ifbody;
  Expr* elsebody;
} IfElseExpr;

typedef struct WhileExpr_s {
  Expr* condition;
  Expr* body;
} WhileExpr;

typedef struct ForExpr_s {
  Stmnt_s* init;
  Expr* test;
  Expr* update;
  Expr* body;
} ForExpr;

typedef struct WithExpr_s {
  Stmnt_s* constructor;
  Stmnt_s* destructor;
  Expr* body;
} WithExpr;

typedef struct BreakExpr_s {
} BreakExpr;

typedef struct ContinueExpr_s {
} ContinueExpr;

typedef struct ReturnExpr_s {
  Expr* value;
} ReturnExpr;

typedef struct MatchExpr_s {
  Expr* 


// AST node
// rownum
// colnum
// Either:
//  Literal:
//    Struct
//    Integer
//    Float
//    String
//    List
//  Variable Declarator:
//
//

// Grammar
//  Variable Declarator
//    <identifier>
//    <expression>
//  Expression
//    Literal
//      Integer, String, Struct
//    FunctionCall
//      <identifier>
//      <lparen>
//        (<expression> <comma>) (or more)
//      <rparen>
//    UnaryExpression
//      <operator>
//      <expression>
//    BinaryExpression
//      <expression>
//      <operator>
//      <expression>
//    GroupingExpression
//      <openbracket>
//        <expression> (or more)
//      <closebracket>

#endif
