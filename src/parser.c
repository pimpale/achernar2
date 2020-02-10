#include "parser.h"

#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "token.h"
#include "vector.h"

// Creates a new parser based on parsing a file on demand
Parser *createParserLexer(Parser *parser, Lexer *lexer) {
  parser->backing = ParserBackingLexer;
  parser->lex.lexer = lexer;
  parser->lex.loc = 0;
  createVector(&parser->lex.tokVec);
  return parser;
}

// Creates a new parser based on parsing an array.
Parser *createParserMemory(Parser *parser, Token *tokens, size_t tokenCount) {
  parser->backing = ParserBackingMemory;
  parser->mem.ptr = malloc(tokenCount * sizeof(Token));
  memcpy(parser->mem.ptr, tokens, tokenCount * sizeof(Token));
  parser->mem.len = tokenCount;
  parser->mem.loc = 0;
  return parser;
}

// TODO also have to destroy the nested tokens
Parser *destroyParser(Parser *parser) {
  switch (parser->backing) {
  case ParserBackingMemory: {
    free(parser->mem.ptr);
    break;
  }
  case ParserBackingLexer: {
    destroyVector(&parser->lex.tokVec);
    break;
  }
  }
  return parser;
}

// Returns a copy of next token, unless end of file. Returns ErrorEOF if hits
// end of file Will print lexing errors
static Diagnostic advanceParser(Parser *p, Token *token) {
  switch (p->backing) {
  case ParserBackingLexer: {
    if (p->lex.loc < VEC_LEN(&p->lex.tokVec, Token)) {
      // Return cached value
      *token = *VEC_GET(&p->lex.tokVec, p->lex.loc++, Token);
      return (Diagnostic){.type = ErrorOk, .ln = token->ln, .col = token->col};
    } else {
      // Iterate till we get a non broken integer
      while (true) {
        Diagnostic diag = lexNextToken(p->lex.lexer, token);
        if (diag.type == ErrorOk) {
          // Increment the location
          p->lex.loc++;
          // Append it to the vector cache
          *VEC_PUSH(&p->lex.tokVec, Token) = *token;
        }
        return diag;
      }
    }
  }
  case ParserBackingMemory: {
    if (p->mem.loc < p->mem.len) {
      // Increment and return
      *token = p->mem.ptr[p->mem.loc++];
      return (Diagnostic){.type = ErrorOk, .ln = token->ln, .col = token->col};
    } else {
      // Issue Eof Error
      // If we don't have a last token to issue it at, issue it at 0,0
      if (p->mem.len != 0) {
        Token lastToken;
        lastToken = p->mem.ptr[p->mem.len - 1];
        return (Diagnostic){
            .type = ErrorEOF, .ln = lastToken.ln, .col = lastToken.col};
      } else {
        return (Diagnostic){.type = ErrorEOF, .ln = 0, .col = 0};
      }
    }
  }
  }
}

// Gets current token
static Diagnostic peekParser(Parser *p, Token *token) {
  Diagnostic diag = advanceParser(p, token);
  if (diag.type == ErrorOk) {
    switch (p->backing) {
    case ParserBackingLexer: {
      p->lex.loc--;
      break;
    }
    case ParserBackingMemory: {
      p->mem.loc--;
      break;
    }
    }
  }
  return diag;
}

#define EXPECT_NO_ERROR(diag)                                                  \
  do {                                                                         \
    if ((diag).type != ErrorOk) {                                              \
      goto HANDLE_ERROR;                                                       \
    }                                                                          \
  } while (false)

#define EXPECT_TYPE(tokenPtr, tokenType)                                       \
  do {                                                                         \
    if ((tokenPtr)->type != (tokenType)) {                                     \
      return (Diagnostic){.type = ErrorUnexpectedToken,                        \
                          .ln = (tokenPtr)->ln,                                \
                          .col = (tokenPtr)->col};                             \
    }                                                                          \
  } while (false)

// Note that all errors resynch at the statement level
static Diagnostic parseStmntProxy(StmntProxy *expr, Parser *p);
static Diagnostic parseExprProxy(ExprProxy *expr, Parser *p);

static Diagnostic parseVarDeclStmnt(VarDeclStmnt *vdsp, Parser *p) {
  // these variables will be reused
  Token t;
  Diagnostic d;

  // Skip let declaration
  advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenLet);

  // the location of the whole function
  uint64_t ln = d.ln;
  uint64_t col = d.col;

  // This might be a mutable or type
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  if (t.type == TokenMut) {
    vdsp->isMutable = true;
  }
  // Now grab the actual type
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);

  // Now check for any pointer layers
  // Loop will exit with t holding the first nonref token
  vdsp->pointerCount = 0;
  while (true) {
    d = advanceParser(p, &t);
    EXPECT_NO_ERROR(d);
    if (t.type == TokenRef) {
      vdsp->pointerCount++;
    } else {
      break;
    }
  }

  // Copy identifier
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenIdentifier);
  vdsp->name = strdup(t.identifier);

  // Expect Assign or semicolon
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenAssign);

  // Expect Expression (no implicit undefined)
  vdsp->hasValue = true;
  parseExprProxy(&vdsp->value, p);

  // Expect Semicolon
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenSemicolon);

  return (Diagnostic){.type = ErrorOk, .ln = ln, .col = col};
}

static Diagnostic parseFuncDeclStmnt(FuncDeclStmnt *fdsp, Parser *p) {
  // these variables will be reused
  Token t;
  Diagnostic d;

  // Skip fn declaration
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenFunction);

  // the location of the whole function
  uint64_t ln = d.ln;
  uint64_t col = d.col;

  // get name
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenIdentifier);
  fdsp->name = strdup(t.identifier);

  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenParenLeft);

  // Parse the varDeclStatements

  // Note that when parsing these declarations, the let is omitted, and we don't
  // look for a value
  Vector parameterDeclarations;
  createVector(&parameterDeclarations);
  while (true) {
    VarDeclStmnt vds;
    d = advanceParser(p, &t);
    EXPECT_NO_ERROR(d);
    // This might be a mutable or type or closing paren
    if (t.type == TokenMut) {
      vds.isMutable = true;
      // Now grab the actual type
      d = advanceParser(p, &t);
      EXPECT_NO_ERROR(d);

    } else if (t.type == TokenParenRight) {
      // Bailing once we hit the other end
      break;
    }

    // Copy type
    EXPECT_TYPE(&t, TokenIdentifier);
    vds.type = strdup(t.identifier);

    // Now check for any pointer layers
    // Loop will exit with t holding the first nonref token
    vds.pointerCount = 0;
    while (true) {
      d = advanceParser(p, &t);
      EXPECT_NO_ERROR(d);
      if (t.type == TokenRef) {
        vds.pointerCount++;
      } else {
        break;
      }
    }

    // Copy identifier
    d = advanceParser(p, &t);
    EXPECT_NO_ERROR(d);
    EXPECT_TYPE(&t, TokenIdentifier);
    vds.name = strdup(t.identifier);

    // No errors
    vds.errorLength = 0;
    vds.errors = NULL;

    // Insert into array
    *VEC_PUSH(&parameterDeclarations, VarDeclStmnt) = vds;
  }

  // Copy arguments in
  fdsp->arguments_length = VEC_LEN(&parameterDeclarations, VarDeclStmnt);
  fdsp->arguments = releaseVector(&parameterDeclarations);

  // Colon return type delimiter
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenColon);

  // Return type
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenIdentifier);
  fdsp->type = strdup(t.identifier);

  d = parseExprProxy(&fdsp->body, p);
  EXPECT_NO_ERROR(d);

  return (Diagnostic){.type = ErrorOk, .ln = ln, .col = col};
}

static Diagnostic parseBreakExpr(BreakExpr *bep, Parser *p) {
  Token t;
  Diagnostic d;
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenBreak);
  bep->errorLength = 0;
  return (Diagnostic){.type = ErrorOk, .ln = d.ln, .col = d.col};
}

static Diagnostic parseContinueExpr(ContinueExpr *cep, Parser *p) {
  Token t;
  Diagnostic d;
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenContinue);
  cep->errorLength = 0;
  return (Diagnostic){.type = ErrorOk, .ln = d.ln, .col = d.col};
}

static Diagnostic parseWhileExpr(WhileExpr *wep, Parser *p) {
  Token t;
  Diagnostic d;
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenWhile);
  d = parseExprProxy(&wep->condition, p);
  EXPECT_NO_ERROR(d);
  d = parseExprProxy(&wep->body, p);
  EXPECT_NO_ERROR(d);

  // TODO errors
  wep->errorLength = 0;
  return d;
}

static Diagnostic parseForExpr(ForExpr *fep, Parser *p) {
  Token t;
  Diagnostic d;
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenFor);
  d = parseStmntProxy(&fep->init, p);
  EXPECT_NO_ERROR(d);
  d = parseExprProxy(&fep->test, p);
  EXPECT_NO_ERROR(d);
  d = parseStmntProxy(&fep->update, p);
  EXPECT_NO_ERROR(d);
  d = parseExprProxy(&fep->body, p);
  EXPECT_NO_ERROR(d);
  fep->errorLength = 0;
  return d;
}

static Diagnostic parseMatchCaseExpr(MatchCaseExpr *mcep, Parser *p) {
  Token t;
  Diagnostic d;
  // Get pattern
  d = parseExprProxy(&mcep->pattern, p);
  EXPECT_NO_ERROR(d);
  // Expect colon
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenColon);
  // Get Value
  d = parseExprProxy(&mcep->pattern, p);
  EXPECT_NO_ERROR(d);
  mcep->errorLength = 0;
  return d;
}

static Diagnostic parseMatchExpr(MatchExpr *mep, Parser *p) {
  Token t;
  Diagnostic d;
  // Ensure match
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenMatch);
  // Get expression to match against
  d = parseExprProxy(&mep->value, p);
  // now we must parse the block containing the cases

  // Expect beginning brace
  d = advanceParser(p, &t);
  EXPECT_NO_ERROR(d);
  EXPECT_TYPE(&t, TokenBraceLeft);

  // TODO how do i add commas?
  Vector cases;
  createVector(&cases);
  while(true) {
    d = peekParser(p, &t);
    EXPECT_NO_ERROR(d);
    if(t.type == TokenBraceRight) {
      break;
    }
    d = parseMatchCaseExpr(VEC_PUSH(&cases, MatchCaseExpr), p);
    EXPECT_NO_ERROR(d);
  }

  // Get interior cases
  mep->cases_length = VEC_LEN(&cases, MatchCaseExpr);
  mep->cases = releaseVector(&cases);

  // TODO errors
  mep->errors = 0;
  return d;
}


// Shunting yard algorithm
static Diagnostic parseExprProxy(ExprProxy *expr, Parser *p) {
  Token t;
  Diagnostic d;

  // get thing
  d = peekParser(p, &t);
  EXPECT_NO_ERROR(d);

  switch (t.type) {
  case TokenBreak: {
    expr->type = ExprBreak;
    expr->value = malloc(sizeof(ExprBreak));
    return parseBreakExpr((BreakExpr *)expr->value, p);
  }
  case TokenContinue: {
                       expr->type = ExprContinue;
    expr->value = malloc(sizeof(ExprContinue));
    return parseContinueExpr((ContinueExpr *)expr->value, p);
  }
  case TokenWhile: {
                    expr->type = ExprWhile;
    expr->value = malloc(sizeof(ExprWhile));
    return parseWhileExpr((WhileExpr *)expr->value, p);
  }
  case TokenFor: {
                  expr->value = ExprFor;
    expr->value = malloc(sizeof(ExprFor));
    return parseForExpr((ForExpr*)expr->value, p);
  }
  case TokenMatch: {
                    expr->value = ExprMatch;
    expr->value = malloc(sizeof(ExprMatch));
    return parseMatchExpr((MatchExpr*)expr->value, p);
  }
  }
}

static Diagnostic parseStmntProxy(StmntProxy *s, Parser *p) {
  // these variables will be reused
  Token t;
  Diagnostic d;

  // get thing
  d = peekParser(p, &t);
  EXPECT_NO_ERROR(d);

  switch (t.type) {
  case TokenFunction: {
    // Initialize statement
    s->type = StmntFuncDecl;
    s->value = malloc(sizeof(FuncDeclStmnt));
    return parseFuncDeclStmnt((FuncDeclStmnt *)s->value, p);
  }
  case TokenLet: {
    // Initialize statement
    s->type = StmntVarDecl;
    s->value = malloc(sizeof(StmntVarDecl));
    d = parseVarDeclStmnt((VarDeclStmnt *)s->value, p);
    EXPECT_NO_ERROR(d);
    return (Diagnostic){.type = ErrorOk, .ln = d.ln, .col = d.col};
  }
  default: {
    advanceParser(p, &t);
    // TODO logDiagnostic(p->dl, d);
    return d;
  }
  }
}

Diagnostic parseTranslationUnit(TranslationUnit *tu, Parser *p) {
  // reused
  Diagnostic d;

  Vector data;
  createVector(&data);

  while (true) {
    StmntProxy s;
    d = parseStmntProxy(&s, p);
    if (d.type == ErrorEOF) {
      break;
    } else if (d.type != ErrorOk) {
      // TODO logDiagnostic(p->dl, d);
    }
    *VEC_PUSH(&data, StmntProxy) = s;
  }

  tu->statements_length = VEC_LEN(&data, StmntProxy);
  tu->statements = releaseVector(&data);
  return (Diagnostic){.type = ErrorOk, .ln = 0, .col = 0};
}
