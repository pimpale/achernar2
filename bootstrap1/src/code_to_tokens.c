#include "code_to_tokens.h"

#include "com_allocator.h"
#include "com_assert.h"
#include "com_bigdecimal.h"
#include "com_bigint.h"
#include "com_biguint.h"
#include "com_format.h"
#include "com_scan.h"
#include "com_vec.h"
#include "com_writer_vec.h"
#include "constants.h"
#include "diagnostic.h"

// utility methods to handle the incredible amount of edge cases

typedef i16 inband_reader_result;
static inband_reader_result lex_peek(com_reader *r, usize n) {
  com_reader_ReadU8Result ret = com_reader_peek_u8(r, n);
  if (!ret.valid) {
    return -1;
  }
  return ret.value;
}

static bool is_alpha(inband_reader_result c) {
  return c != -1 && com_format_is_alpha((u8)c);
}

static bool is_alphanumeric(inband_reader_result c) {
  return c != -1 && com_format_is_alphanumeric((u8)c);
}

static bool is_digit(inband_reader_result c) {
  return c != -1 && com_format_is_digit((u8)c);
}

static bool is_whitespace(inband_reader_result c) {
  return c != -1 && com_format_is_whitespace((u8)c);
}

// Call this function right before the first hash
// Returns control at the first noncomment area
// Lexes attributes
static Token lexMetadata(com_reader *r,
                         attr_UNUSED DiagnosticLogger *diagnostics,
                         com_allocator *a, bool significant) {

  u8 character = significant ? '$' : '#';

  com_loc_LnCol start = com_reader_position(r);

  com_reader_ReadU8Result c = com_reader_read_u8(r);
  com_assert_m(c.valid && c.value == character,
               significant ? "expected $" : " expected #");

  com_vec data = com_vec_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 10,
                                    .flags = com_allocator_defaults(a) |
                                             com_allocator_REALLOCABLE |
                                             com_allocator_NOLEAK}));

  // Now we determine the type of comment as well as gather the comment data
  c = com_reader_peek_u8(r, 1);
  if (c.valid && c.value == '{') {
    // This is a multi line attribute. It will continue until another is found.
    // These strings are preserved in the AST. They are nestable
    // ${ Attribute }$

    usize stackDepth = 1;

    // Drop initial {
    com_reader_drop_u8(r);

    while (true) {
      com_reader_ReadU8Result cc = com_reader_peek_u8(r, 1);
      com_reader_ReadU8Result nc = com_reader_peek_u8(r, 2);
      if (cc.valid) {
        if (cc.value == '}' && nc.valid && nc.value == character) {
          stackDepth--;
          if (stackDepth == 0) {
            break;
          }
        } else if (cc.value == character && nc.valid && nc.value == '{') {
          stackDepth++;
        }
        *com_vec_push_m(&data, u8) = cc.value;
      } else {
        break;
      }
    }
  } else if (c.valid && c.value == character) {
    // it's a normal single line attribute.
    // These are not nestable, and continue till the end of line.
    // $$ attribute
    while (true) {
      com_reader_ReadU8Result cc = com_reader_peek_u8(r, 1);
      if (!cc.valid || cc.value == '\n') {
        break;
      } else {
        *com_vec_push_m(&data, u8) = (u8)cc.value;
      }
    }
  } else {
    // it's a single word attribute. continues until non alphanumeric
    // $attribute
    while (true) {
      com_reader_ReadU8Result cc = com_reader_peek_u8(r, 1);
      if (!cc.valid || !com_format_is_alphanumeric(cc.value)) {
        break;
      } else {
        *com_vec_push_m(&data, u8) = (u8)cc.value;
      }
    }
  }

  return (Token){
      .kind = tk_Metadata,
      .metadataToken = {.content = com_str_demut(com_vec_to_str(&data)),
                        .significant = significant},
      .span = com_loc_span_m(start, com_reader_position(r)),
  };
}

// Call this function right before the first quote of the string literal
// Returns control after the ending quote of this lexer
// This function returns a Token containing the string or an error
static Token lexStringLiteral(com_reader *r, DiagnosticLogger *diagnostics,
                              com_allocator *a) {
  com_loc_LnCol start = com_reader_position(r);

  // Skip first quote
  com_reader_ReadU8Result c = com_reader_read_u8(r);
  com_assert_m(c.valid && c.value == '\"', "expected quotation mark");

  // parse into vec writer until we get a success or a read error log errors
  // along the way

  // create vector writer
  com_vec vec = com_vec_create(com_allocator_alloc(
      a, (com_allocator_HandleData){
             .flags = com_allocator_defaults(a) | com_allocator_NOLEAK |
                      com_allocator_REALLOCABLE,
             // let's just go with 12 for now as initial capacity
             .len = 12}));
  com_writer writer = com_writer_vec_create(&vec);

  while (true) {
    com_scan_CheckedStrResult ret =
        com_scan_checked_str_until_quote(&writer, r);
    switch (ret.result) {
    case com_scan_CheckedStrSuccessful: {
      // We're finished, we can deallocate writer and release vec into a string
      // that we return as an element
      com_writer_destroy(&writer);

      // retrn success
      return (Token){
          .kind = tk_String,
          .span = com_loc_span_m(start, com_reader_position(r)),
          .stringToken = {.data = com_str_demut(com_vec_to_str(&vec))}};
    }
    case com_scan_CheckedStrReadFailed: {
      // deallocate resources
      com_writer_destroy(&writer);
      // give error
      *dlogger_append(diagnostics) =
          (Diagnostic){.span = ret.span,
                       .severity = DSK_Error,
                       .message = com_str_lit_m(
                           "unexpected EOF, expected closing double quote"),
                       .children_len = 0};
      // return invalid
      return (Token){
          .kind = tk_None,
          .span = com_loc_span_m(start, ret.span.end),
      };
    }
    case com_scan_CheckedStrInvalidControlChar: {
      *dlogger_append(diagnostics) = (Diagnostic){
          .span = ret.span,
          .severity = DSK_Error,
          .message = com_str_lit_m("invalid control char after backslash"),
          .children_len = 0};
      break;
    }
    case com_scan_CheckedStrInvalidUnicodeSpecifier: {
      *dlogger_append(diagnostics) =
          (Diagnostic){.span = ret.span,
                       .severity = DSK_Error,
                       .message = com_str_lit_m("invalid unicode point"),
                       .children_len = 0};
      break;
    }
    }
  }
}

// Parses integer with radix
// Radix must be between 2 and 16 inclusive
static com_biguint parseNumBaseComponent(com_reader *r,
                                         DiagnosticLogger *diagnostics,
                                         com_allocator *a, u8 radix) {
  com_biguint integer_value = com_biguint_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 10,
                                    .flags = com_allocator_defaults(a) |
                                             com_allocator_NOLEAK |
                                             com_allocator_REALLOCABLE}));
  while (true) {
    com_loc_Span sp = com_reader_peek_span_u8(r);
    com_reader_ReadU8Result ret = com_reader_peek_u8(r, 1);

    // exit if misread or not a hex number
    if (!ret.valid || !com_format_is_hex(ret.value)) {
      break;
    }

    // ignore underscore
    if (ret.value == '_') {
      com_reader_drop_u8(r);
      continue;
    }

    u8 digit_val = com_format_from_hex(ret.value);

    if (digit_val >= radix) {
      *dlogger_append(diagnostics) = (Diagnostic){
          .span = sp,
          .severity = DSK_Error,
          .message = com_str_lit_m("num literal char value exceeds radix"),
          .children_len = 0};
      // put in dummy for the digit value
      digit_val = radix - 1;
    }

    // integer_value = integer_value * radix + digit_val;

    com_biguint_mul_u32(&integer_value, &integer_value, radix);
    com_biguint_add_u32(&integer_value, &integer_value, digit_val);

    // we can finally move past this char
    com_reader_drop_u8(r);
  }
  return integer_value;
}

static com_bigdecimal parseNumFractionalComponent(com_reader *r,
                                                  DiagnosticLogger *diagnostics,
                                                  com_allocator *a, u8 radix,
                                                  com_biguint base_component) {

  // the reciprocal of the current decimal place
  // EX: at 0.1 will be 10 when parsing the 1
  // EX: at 0.005 will be 1000 when parsing the 5
  // This is because representing decimals is lossy
  com_bigdecimal fractional_value =
      com_bigdecimal_from(com_bigint_from(base_component, false));

  // fractional component being computed
  com_bigdecimal place = com_bigdecimal_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 12,
                                    .flags = com_allocator_defaults(a) |
                                             com_allocator_REALLOCABLE}));

  com_bigdecimal tmp = com_bigdecimal_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 12,
                                    .flags = com_allocator_defaults(a) |
                                             com_allocator_REALLOCABLE}));

  com_bigdecimal radix_val = com_bigdecimal_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 4,
                                    .flags = com_allocator_defaults(a)}));

  com_bigdecimal digit_val = com_bigdecimal_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 4,
                                    .flags = com_allocator_defaults(a)}));

  com_bigdecimal_set_i64(&place, 1);

  com_bigdecimal_set_i64(&radix_val, radix);

  while (true) {
    com_loc_Span sp = com_reader_peek_span_u8(r);
    com_reader_ReadU8Result ret = com_reader_peek_u8(r, 1);

    // exit if misread or not a hex number
    if (!ret.valid || !com_format_is_hex(ret.value)) {
      break;
    }

    // ignore underscore
    if (ret.value == '_') {
      com_reader_drop_u8(r);
      continue;
    }

    com_bigdecimal_set_i64(&digit_val, com_format_from_hex(ret.value));

    // if radix_val < digit_val
    if (com_bigdecimal_cmp(&digit_val, &radix_val) == com_math_LESS) {
      *dlogger_append(diagnostics) = (Diagnostic){
          .span = sp,
          .severity = DSK_Error,
          .message = com_str_lit_m("num literal char value exceeds radix"),
          .children_len = 0};

      // put in dummy for the digit value
      com_bigdecimal_set_i64(&digit_val, 0);
    }

    // ensure precision is safe
    com_bigdecimal_set_precision(
        &place, com_bigdecimal_get_precision(&fractional_value) + 1);

    // place /= radix_val
    // Radix val has zero precision, so should be valid
    com_bigdecimal_div(&place, &place, &radix_val, a);

    // tmp = digit_val / place
    // digit val has zero precision, so tmp precision should be =
    // fractional_value + 1
    com_bigdecimal_mul(&tmp, &place, &digit_val, a);

    // scale up fractional value's precision to match place
    com_bigdecimal_set_precision(&fractional_value,
                                 com_bigdecimal_get_precision(&place));
    com_bigdecimal_add(&fractional_value, &fractional_value, &tmp);

    // remove any trailing zeros
    com_bigdecimal_remove_trailing_zero(&fractional_value);

    // we can finally move past this char
    com_reader_drop_u8(r);
  }
  return fractional_value;
}

// Call this function right before the first digit of the number literal
// Returns control right after the number is finished
// This function returns a Token or the error
static Token lexNumberLiteral(com_reader *r, DiagnosticLogger *diagnostics,
                              com_allocator *a) {

  com_loc_LnCol start = com_reader_position(r);

  // drop any leading plus sign
  if (lex_peek(r, 1) == '+') {
    com_reader_drop_u8(r);
  }
  // drop any leading minus sign
  bool negative = false;
  if (lex_peek(r, 1) == '-') {
    com_reader_drop_u8(r);
    negative = true;
  }

  u8 radix = 10;
  {
    // char one ahead
    com_reader_ReadU8Result fdigit = com_reader_peek_u8(r, 1);
    // char two ahead
    com_reader_ReadU8Result code = com_reader_peek_u8(r, 2);

    if (fdigit.valid && fdigit.value == '0' && code.valid) {
      switch (code.value) {
      case 'b': {
        radix = 2;
        com_reader_drop_u8(r);
        com_reader_drop_u8(r);
        break;
      }
      case 'o': {
        radix = 8;
        com_reader_drop_u8(r);
        com_reader_drop_u8(r);
        break;
      }
      case 'd': {
        radix = 10;
        com_reader_drop_u8(r);
        com_reader_drop_u8(r);
        break;
      }
      case 'x': {
        radix = 16;
        com_reader_drop_u8(r);
        com_reader_drop_u8(r);
        break;
      }
      default: {
        radix = 10;
        if (!com_format_is_digit(code.value)) {
          // if it's not another digit, then we throw an error about an
          // unrecognized radix code + skip the first 2 chars
          com_reader_drop_u8(r);
          com_reader_drop_u8(r);

          *dlogger_append(diagnostics) = (Diagnostic){
              .span = com_loc_span_m(start, com_reader_position(r)),
              .severity = DSK_Error,
              .message = com_str_lit_m("num literal unrecognized radix code"),
              .children_len = 0};
        }
        // if it was a digit then it parses like a normal decimal number
        break;
      }
      }
    }
  }

  com_biguint base_component = parseNumBaseComponent(r, diagnostics, a, radix);

  bool fractional = false;
  {
    com_reader_ReadU8Result ret = com_reader_peek_u8(r, 1);
    if (ret.valid && ret.value == '.') {
      fractional = true;
      com_reader_drop_u8(r);
    }
  }

  if (fractional) {
    com_bigdecimal decimal =
        parseNumFractionalComponent(r, diagnostics, a, radix, base_component);

    if (negative) {
      com_bigdecimal_negate(&decimal);
    }

    return (Token){.kind = tk_Real,
                   .realToken = {.data = decimal},
                   .span = com_loc_span_m(start, com_reader_position(r))};
  } else {
    return (Token){.kind = tk_Int,
                   .intToken =
                       {
                           .data = com_bigint_from(base_component, negative),
                       },
                   .span = com_loc_span_m(start, com_reader_position(r))};
  }
}

static Token lexLabelLiteral(com_reader *r,
                             attr_UNUSED DiagnosticLogger *diagnostics,
                             com_allocator *a) {
  com_loc_LnCol start = com_reader_position(r);
  // Skip first quote
  {
    com_reader_ReadU8Result ret = com_reader_read_u8(r);
    com_assert_m(ret.valid && ret.value == '\'', "expected single quote");
  }
  com_vec label_data = com_vec_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 1,
                                    .flags = com_allocator_defaults(a) |
                                             com_allocator_REALLOCABLE |
                                             com_allocator_NOLEAK}));

  while (true) {
    com_reader_ReadU8Result ret = com_reader_peek_u8(r, 1);
    if (ret.valid &&
        (com_format_is_alphanumeric(ret.value) || ret.value == '_')) {
      *com_vec_push_m(&label_data, u8) = ret.value;
      com_reader_drop_u8(r);
    } else {
      break;
    }
  }

  return (Token){.span = com_loc_span_m(start, com_reader_position(r)),
                 .kind = tk_Label,
                 .labelToken = {
                     .data = com_str_demut(com_vec_to_str(&label_data)),
                 }};
}

// Parses an identifer or macro or builtin
static Token lexWord(com_reader *r, attr_UNUSED DiagnosticLogger *diagnostics,
                     com_allocator *a) {

  com_loc_LnCol start = com_reader_position(r);

  com_vec data = com_vec_create(com_allocator_alloc(
      a, (com_allocator_HandleData){.len = 10,
                                    .flags = com_allocator_defaults(a) |
                                             com_allocator_REALLOCABLE |
                                             com_allocator_NOLEAK}));

  while (true) {
    com_reader_ReadU8Result ret = com_reader_peek_u8(r, 1);
    if (ret.valid) {
      u8 c = ret.value;
      if (com_format_is_alphanumeric(c) || c == '_') {
        *com_vec_push_m(&data, u8) = c;
        com_reader_drop_u8(r);
      } else {
        // we encountered a nonword char
        break;
      }
    } else {
      // means we hit EOF
      break;
    }
  }

  com_loc_Span span = com_loc_span_m(start, com_reader_position(r));

  com_str str = com_str_demut(com_vec_to_str(&data));

  if (com_str_equal(str, com_str_lit_m("_"))) {
    return (Token){.kind = tk_Underscore, .span = span};
  }

  if (com_str_equal(str, com_str_lit_m("true"))) {
    return (Token){.kind = tk_Bool, .boolToken = {true}, .span = span};
  } else if (com_str_equal(str, com_str_lit_m("false"))) {
    return (Token){.kind = tk_Bool, .boolToken = {false}, .span = span};
  }

  Token token;
  token.span = span;

  if (com_str_equal(str, com_str_lit_m("loop"))) {
    token.kind = tk_Loop;
  } else if (com_str_equal(str, com_str_lit_m("match"))) {
    token.kind = tk_Match;
  } else if (com_str_equal(str, com_str_lit_m("new"))) {
    token.kind = tk_New;
  } else if (com_str_equal(str, com_str_lit_m("def"))) {
    token.kind = tk_Def;
  } else if (com_str_equal(str, com_str_lit_m("ret"))) {
    token.kind = tk_Ret;
  } else if (com_str_equal(str, com_str_lit_m("defer"))) {
    token.kind = tk_Defer;
  } else if (com_str_equal(str, com_str_lit_m("fn"))) {
    token.kind = tk_Fn;
  } else if (com_str_equal(str, com_str_lit_m("has"))) {
    token.kind = tk_Has;
  } else if (com_str_equal(str, com_str_lit_m("let"))) {
    token.kind = tk_Let;
  } else if (com_str_equal(str, com_str_lit_m("type"))) {
    token.kind = tk_Type;
  } else if (com_str_equal(str, com_str_lit_m("mod"))) {
    token.kind = tk_Mod;
  } else if (com_str_equal(str, com_str_lit_m("use"))) {
    token.kind = tk_Use;
  } else if (com_str_equal(str, com_str_lit_m("and"))) {
    token.kind = tk_And;
  } else if (com_str_equal(str, com_str_lit_m("or"))) {
    token.kind = tk_Or;
  } else if (com_str_equal(str, com_str_lit_m("xor"))) {
    token.kind = tk_Xor;
  } else if (com_str_equal(str, com_str_lit_m("not"))) {
    token.kind = tk_Not;
  } else if (com_str_equal(str, com_str_lit_m("nil"))) {
    token.kind = tk_Nil;
  } else if (com_str_equal(str, com_str_lit_m("never"))) {
    token.kind = tk_Never;
  } else {
    // It is an identifier, and we need to keep the string
    token.kind = tk_Identifier;
    token.identifierToken.data = str;
    return token;
  }

  return token;
}

#define RESULT_TOKEN(tokenType)                                                \
  (Token) {                                                                    \
    .kind = tokenType, .span = com_loc_span_m(start, com_reader_position(r))   \
  }

#define RETURN_RESULT_TOKEN1(tokenType)                                        \
  {                                                                            \
    com_reader_drop_u8(r);                                                     \
    return RESULT_TOKEN(tokenType);                                            \
  }

#define RETURN_RESULT_TOKEN2(tokenType)                                        \
  {                                                                            \
    com_reader_drop_u8(r);                                                     \
    RETURN_RESULT_TOKEN1(tokenType)                                            \
  }

#define RETURN_RESULT_TOKEN3(tokenType)                                        \
  {                                                                            \
    com_reader_drop_u8(r);                                                     \
    RETURN_RESULT_TOKEN2(tokenType)                                            \
  }

#define RETURN_RESULT_TOKEN4(tokenType)                                        \
  {                                                                            \
    com_reader_drop_u8(r);                                                     \
    RETURN_RESULT_TOKEN3(tokenType)                                            \
  }

#define RETURN_UNKNOWN_TOKEN1()                                                \
  {                                                                            \
    *dlogger_append(diagnostics) =                                             \
        (Diagnostic){.span = com_reader_peek_span_u8(r),                       \
                     .severity = DSK_Error,                                    \
                     .message = com_str_lit_m("lexer unrecognized character"), \
                     .children_len = 0};                                       \
    RETURN_RESULT_TOKEN1(tk_None)                                              \
  }

Token tk_next(com_reader *r, DiagnosticLogger *diagnostics, com_allocator *a) {
  // always defined after
  inband_reader_result c = -1;

  // Set c to first nonblank character
  while ((c = lex_peek(r, 1)) != -1) {
    // it's guaranteed that c is > 0 in body of while
    if (is_whitespace(c)) {
      com_reader_drop_u8(r);
    } else {
      break;
    }
  }

  com_loc_LnCol start = com_reader_position(r);

  if (is_alpha(c)) {
    return lexWord(r, diagnostics, a);
  } else if (is_digit(c)) {
    return lexNumberLiteral(r, diagnostics, a);
  } else {
    switch (c) {
    case '\'': {
      return lexLabelLiteral(r, diagnostics, a);
    }
    case '\"': {
      return lexStringLiteral(r, diagnostics, a);
    }
    case '#': {
      return lexMetadata(r, diagnostics, a, false);
    }
    case '$': {
      return lexMetadata(r, diagnostics, a, true);
    }
    case '_': {
      inband_reader_result c2 = lex_peek(r, 2);
      if (is_alphanumeric(c2) || c2 == '_') {
        return lexWord(r, diagnostics, a);
      } else {
        RETURN_RESULT_TOKEN1(tk_Underscore)
      }
    }
    case '+': {
      inband_reader_result c2 = lex_peek(r, 2);

      if (is_digit(c2)) {
        return lexNumberLiteral(r, diagnostics, a);
      }

      switch (c2) {
      case '=': {
        RETURN_RESULT_TOKEN2(tk_AssignAdd)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_Add)
      }
      }
    }
    case '-': {
      if (is_digit(lex_peek(r, 2))) {
        return lexNumberLiteral(r, diagnostics, a);
      }

      switch (lex_peek(r, 2)) {
      case '>': {
        RETURN_RESULT_TOKEN2(tk_Pipe)
      }
      case '=': {
        RETURN_RESULT_TOKEN2(tk_AssignSub)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_Sub)
      }
      }
    }
    case '&': {
      RETURN_RESULT_TOKEN1(tk_Ref)
    }
    case '|': {
      RETURN_RESULT_TOKEN1(tk_Sum)
    }
    case ',': {
      RETURN_RESULT_TOKEN1(tk_Product)
    }
    case '!': {
      switch (lex_peek(r, 2)) {
      case '=': {
        RETURN_RESULT_TOKEN2(tk_CompNotEqual)
      }
      default: {
        RETURN_UNKNOWN_TOKEN1()
      }
      }
    }
    case '=': {
      switch (lex_peek(r, 2)) {
      case '=': {
        RETURN_RESULT_TOKEN2(tk_CompEqual)
      }
      case '>': {
        RETURN_RESULT_TOKEN2(tk_Arrow)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_Assign)
      }
      }
    }
    case '<': {
      switch (lex_peek(r, 2)) {
      case '=': {
        RETURN_RESULT_TOKEN2(tk_CompLessEqual)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_CompLess)
      }
      }
    }
    case '>': {
      switch (lex_peek(r, 2)) {
      case '=': {
        RETURN_RESULT_TOKEN2(tk_CompGreaterEqual)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_CompGreater)
      }
      }
    }
    case '*': {
      switch (lex_peek(r, 2)) {
      case '=': {
        RETURN_RESULT_TOKEN2(tk_AssignMul)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_Mul)
      }
      }
    }
    case '/': {
      switch (lex_peek(r, 2)) {
      case '/': {
        switch (lex_peek(r, 3)) {
        case '=': {
          RETURN_RESULT_TOKEN3(tk_AssignIDiv)
        }
        default: {
          RETURN_RESULT_TOKEN2(tk_IDiv)
        }
        }
      }
      case '.': {
        switch (lex_peek(r, 3)) {
        case '=': {
          RETURN_RESULT_TOKEN3(tk_AssignFDiv)
        }
        default: {
          RETURN_RESULT_TOKEN2(tk_FDiv)
        }
        }
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_ModResolution)
      }
      }
    }
    case '%': {
      switch (lex_peek(r, 2)) {
      case '/': {
        switch (lex_peek(r, 3)) {
        case '=': {
          RETURN_RESULT_TOKEN3(tk_AssignIRem)
        }
        default: {
          RETURN_RESULT_TOKEN2(tk_IRem)
        }
        }
      }
      case '.': {
        switch (lex_peek(r, 3)) {
        case '=': {
          RETURN_RESULT_TOKEN3(tk_AssignFRem)
        }
        default: {
          RETURN_RESULT_TOKEN2(tk_FRem)
        }
        }
      }
      default: {
        RETURN_UNKNOWN_TOKEN1()
      }
      }
    }
    case ':': {
      switch (lex_peek(r, 2)) {
      case '=': {
        RETURN_RESULT_TOKEN2(tk_Define)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_Constrain)
      }
      }
    }
    case '.': {
      switch (lex_peek(r, 2)) {
      case '.': {
        switch (lex_peek(r, 3)) {
        case '.': {
          switch (lex_peek(r, 4)) {
          case '=': {
            RETURN_RESULT_TOKEN4(tk_IneqInclusive)
          }
          default: {
            RETURN_RESULT_TOKEN3(tk_Ineq)
          }
          }
        }
        case '=': {
          RETURN_RESULT_TOKEN3(tk_RangeInclusive)
        }
        default: {
          RETURN_RESULT_TOKEN2(tk_Range)
        }
        }
      }
      case '=': {
        RETURN_RESULT_TOKEN2(tk_Record)
      }
      default: {
        RETURN_RESULT_TOKEN1(tk_FieldAccess)
      }
      }
    }
    case '[': {
      RETURN_RESULT_TOKEN1(tk_BracketLeft)
    }
    case ']': {
      RETURN_RESULT_TOKEN1(tk_BracketRight)
    }
    case '@': {
      RETURN_RESULT_TOKEN1(tk_Deref)
    }
    case '(': {
      RETURN_RESULT_TOKEN1(tk_ParenLeft)
    }
    case ')': {
      RETURN_RESULT_TOKEN1(tk_ParenRight)
    }
    case '{': {
      RETURN_RESULT_TOKEN1(tk_BraceLeft)
    }
    case '}': {
      RETURN_RESULT_TOKEN1(tk_BraceRight)
    }
    case '\\': {
      RETURN_RESULT_TOKEN1(tk_Backslash)
    }
    default: {
      RETURN_UNKNOWN_TOKEN1()
    }
    }
  }
}
