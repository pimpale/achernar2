#include "json.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "allocator.h"
#include "vector.h"

// GLORIOUS UTILS
// defined behavior for all values of val

uint64_t j_safe_abs(int64_t val) {
  if(val < 0) {
    return (uint64_t) -val;
  } else {
    return (uint64_t) val;
  }
}

#define UNUSED __attribute__ ((unused))
#define ERROR(k, l) ((j_Error){.kind = k, .loc = l})

// Accepts Vector<char>, pushes as many chars points as needed to encode the
// data
static void encodeUTFPoint(Vector *data, uint32_t utf) {
  if (utf <= 0x7F) { // Plain ASCII
    char *out = vec_push(data, sizeof(char) * 1);
    out[0] = (char)utf;
  } else if (utf <= 0x07FF) {
    // 2-byte unicode
    char *out = vec_push(data, sizeof(char) * 2);
    out[0] = (char)(((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char)(((utf >> 0) & 0x3F) | 0x80);
  } else if (utf <= 0xFFFF) {
    // 3-byte unicode
    char *out = vec_push(data, sizeof(char) * 3);
    out[0] = (char)(((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 0) & 0x3F) | 0x80);
  } else if (utf <= 0x10FFFF) {
    // 4-byte unicode
    char *out = vec_push(data, sizeof(char) * 4);
    out[0] = (char)(((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char)(((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[3] = (char)(((utf >> 0) & 0x3F) | 0x80);
  }
  // TODO gracefully handle error
}

// JSON TO STRING
static void j_unchecked_emitChar(Vector *vptr, char c) {
  *VEC_PUSH(vptr, char) = c;
}

static void j_unchecked_emitStr(Vector *vptr, char *str, size_t len) {
  // length of string in bytes
  memcpy(vec_push(vptr, len), str, len);
}

// Convert from int to string
static void j_emitInt(Vector *vptr, j_Int val) {
  // handle negative numbers
  if (val.negative) {
    *VEC_PUSH(vptr, char) = '-';
  }
  //buffer to push
  char buffer[30];

  uint64_t digit = val.integer;
  int64_t index = 0;
  while (index < 30) {
    int8_t rem = digit % 10;
    buffer[index] = '0' + rem;
    digit /= 10;
    if (digit == 0) {
      break;
    }
    index++;
  }

  // push buffer in reverse order
  while(index >= 0) {
    j_unchecked_emitChar(vptr, buffer[index]);
    index--;
  }

}

static void j_emitNum(Vector *vptr, double number) {
  // up to 328 digits in a float
  char str[350];
  snprintf(str, 350, "%f", number);
  j_unchecked_emitStr(vptr, str, strlen(str));
}

static char toHex(uint8_t x) {
  if (x < 10) {
    return '0' + (char)x;
  } else {
    return 'a' + (char)x;
  }
}

static int8_t fromHex(char c) {
  switch (c) {
  case '0': {
    return 0x0;
  }
  case '1': {
    return 0x1;
  }
  case '2': {
    return 0x2;
  }
  case '3': {
    return 0x3;
  }
  case '4': {
    return 0x4;
  }
  case '5': {
    return 0x5;
  }
  case '6': {
    return 0x6;
  }
  case '7': {
    return 0x7;
  }
  case '8': {
    return 0x8;
  }
  case '9': {
    return 0x9;
  }
  case 'a': {
    return 0xa;
  }
  case 'A': {
    return 0xA;
  }
  case 'b': {
    return 0xb;
  }
  case 'B': {
    return 0xB;
  }
  case 'c': {
    return 0xc;
  }
  case 'C': {
    return 0xC;
  }
  case 'd': {
    return 0xd;
  }
  case 'D': {
    return 0xD;
  }
  case 'e': {
    return 0xe;
  }
  case 'E': {
    return 0xE;
  }
  case 'f': {
    return 0xf;
  }
  case 'F': {
    return 0xF;
  }
  default: {
    return -1;
  }
  }
}

// Checks for special characters
static void j_emitStr(Vector *vptr, j_Str str) {
  j_unchecked_emitChar(vptr, '\"');
  for (size_t i = 0; i < str.length; i++) {
    char c = str.string[i];
    switch (c) {
    case '\b': {
      j_unchecked_emitStr(vptr, "\\b", 2);
      break;
    }
    case '\f': {
      j_unchecked_emitStr(vptr, "\\f", 2);
      break;
    }
    case '\n': {
      j_unchecked_emitStr(vptr, "\\n", 2);
      break;
    }
    case '\r': {
      j_unchecked_emitStr(vptr, "\\r", 2);
      break;
    }
    case '\t': {
      j_unchecked_emitStr(vptr, "\\t", 2);
      break;
    }
    case '\"': {
      j_unchecked_emitStr(vptr, "\\\"", 2);
      break;
    }
    case '\\': {
      j_unchecked_emitStr(vptr, "\\\\", 2);
      break;
    }
    default: {
      if (c <= 0x001F) {
        char *ptr = vec_push(vptr, sizeof(char) * 6);
        ptr[0] = '\\';
        ptr[1] = 'u';
        ptr[2] = '0';
        ptr[3] = '0';
        ptr[4] = toHex((uint8_t)c / 16);
        ptr[5] = toHex((uint8_t)c % 16);
      } else {
        *VEC_PUSH(vptr, char) = c;
      }
      break;
    }
    }
  }
  j_unchecked_emitChar(vptr, '\"');
}

static void j_emitElem(Vector *data, j_Elem *j);

static void j_emitProp(Vector *vptr, j_Prop *prop) {
  j_emitStr(vptr, prop->key);
  j_unchecked_emitChar(vptr, ':');
  j_emitElem(vptr, &prop->value);
}

static void j_emitElem(Vector *data, j_Elem *j) {
  switch (j->kind) {
  case j_NullKind: {
    j_unchecked_emitStr(data, "null", 4);
    break;
  }
  case j_StrKind: {
    j_emitStr(data, j->string);
    break;
  }
  case j_BoolKind: {
    j_unchecked_emitStr(data, j->boolean ? "true" : "false",
                        j->boolean ? 4 : 5);
    break;
  }
  case j_IntKind: {
    // there are a max 20 digits in an integer
    j_emitInt(data, j->integer);
    break;
  }
  case j_NumKind: {
    j_emitNum(data, j->number);
    break;
  }
  case j_ArrayKind: {
    j_unchecked_emitChar(data, '[');
    for (size_t i = 0; i < j->array.length; i++) {
      j_emitElem(data, &j->array.values[i]);
      // if we are not on the ultimate element of the lsit
      if (i + 1 != j->array.length) {
        j_unchecked_emitChar(data, ',');
      }
    }
    j_unchecked_emitChar(data, ']');
    break;
  }
  case j_ObjectKind: {
    j_unchecked_emitChar(data, '{');
    for (size_t i = 0; i < j->object.length; i++) {
      j_emitProp(data, &j->object.props[i]);
      if (i + 1 != j->object.length) {
        j_unchecked_emitChar(data, ',');
      }
    }
    j_unchecked_emitChar(data, '}');
    break;
  }
  }
}

char *j_stringify(j_Elem *j, Allocator *a) {
  Vector data = vec_create(a);
  j_emitElem(&data, j);
  // terminate string
  *VEC_PUSH(&data, char) = '\0';
  return vec_release(&data);
}

// Parsing
static void skipWhitespace(Lexer *l) {
  while (true) {
    switch (lex_peek(l)) {
    case ' ':
    case '\t':
    case '\r':
    case '\n': {
      // discard whitespace
      lex_next(l);
      break;
    }
    default: {
      return;
    }
    }
  }
}

static j_Elem j_certain_parseNumberElem(Lexer *l, Vector *diagnostics,
                                        UNUSED Allocator *a) {
  bool negative = false;
  if (lex_peek(l) == '-') {
    negative = true;
    lex_next(l);
  }

  uint64_t integer_value = 0;
  int32_t c;
  while ((c = lex_peek(l)) != EOF) {
    if (isdigit(c)) {
      integer_value = integer_value * 10 + (uint64_t)(c - '0');
      lex_next(l);
    } else {
      break;
    }
  }

  bool fractional = false;
  if (lex_peek(l) == '.') {
    fractional = true;
    lex_next(l);
  }

  double fractional_component = 0;
  if (fractional) {
    double place = 1;
    while ((c = lex_peek(l)) != EOF) {
      if (isdigit(c)) {
        place *= 10;
        fractional_component += (c - '0')/place;
        lex_next(l);
      } else {
        break;
      }
    }
  }

  bool positive_exponent = false;
  bool negative_exponent = false;
  c = lex_peek(l);
  if (c == 'E' || c == 'e') {
    lex_next(l);
    switch (lex_next(l)) {
    case '+': {
      positive_exponent = true;
      break;
    }
    case '-': {
      negative_exponent = true;
      break;
    }
    default: {
      *VEC_PUSH(diagnostics, j_Error) =
          ERROR(j_NumExponentExpectedSign, l->position);
      break;
    }
    }
  }

  uint32_t exponential_integer = 0;
  if (positive_exponent || negative_exponent) {
    while ((c = lex_peek(l)) != EOF) {
      if (isdigit(c)) {
        exponential_integer = exponential_integer * 10 + (uint8_t)(c - '0');
        lex_next(l);
      } else {
        break;
      }
    }
  }

  if (fractional || negative_exponent || positive_exponent) {
    // Decimalish
    double num = (double)integer_value + fractional_component;
    if (positive_exponent) {
      for (size_t i = 0; i < exponential_integer; i++) {
        num *= 10;
      }
    }
    if (negative_exponent) {
      for (size_t i = 0; i < exponential_integer; i++) {
        num /= 10;
      }
    }
    return J_NUM_ELEM(num);
  } else {
    // Integer
    uint64_t num = integer_value;
    return J_INT_ELEM(J_INT(negative, num));
  }
}

static j_Elem j_certain_parseLiteralElem(Lexer *l, Vector *diagnostics,
                                         UNUSED Allocator *a) {
  LnCol start = l->position;

  // Fixed buffer size
  char buffer[6]; // this is long enough to hold false\0
  bool toolong = false;
  size_t index = 0;
  while (true) {
    int32_t c = lex_peek(l);
    if (isalpha(c)) {
      // Fill up buffer
      if (index < 5) {
        buffer[index] = (char)c;
        index++;
      } else {
        toolong = true;
      }
      // even if the buffer is finished we must continue on (but mark it as
      // (toolong)
      lex_next(l);
    } else {
      break;
    }
  }
  // Terminate with string length
  buffer[5] = '\0';

  if (!toolong && !strcmp("null", buffer)) {
    return J_NULL_ELEM;
  } else if (!toolong && !strcmp("true", buffer)) {
    return J_BOOL_ELEM(true);
  } else if (!toolong && !strcmp("false", buffer)) {
    return J_BOOL_ELEM(false);
  } else {
    *VEC_PUSH(diagnostics, j_Error) = ERROR(j_MalformedLiteral, start);
    return J_NULL_ELEM;
  }
}

static j_Str j_parseStr(Lexer *l, Vector *diagnostics, Allocator *a) {
  LnCol start = l->position;
  skipWhitespace(l);
  int32_t c = lex_next(l);
  if (c != '\"') {
    *VEC_PUSH(diagnostics, j_Error) = ERROR(j_StrExpectedDoubleQuote, start);
  }

  typedef enum {
    StringParserText,
    StringParserBackslash,
    StringParserUnicode,
  } StringParserState;

  Vector data = vec_create(a);

  StringParserState state = StringParserText;

  while (true) {
    switch (state) {
    case StringParserText: {
      c = lex_next(l);
      switch (c) {
      case '\\': {
        state = StringParserBackslash;
        break;
      }
      case '\"': {
        goto LOOPEND;
      }
      case EOF: {
        *VEC_PUSH(diagnostics, j_Error) =
            ERROR(j_StrExpectedDoubleQuote, l->position);
        goto LOOPEND;
      }
      default: {
        *VEC_PUSH(&data, char) = (char)c;
        break;
      }
      }
      break;
    }
    case StringParserBackslash: {
      c = lex_next(l);
      switch (c) {
      case '\"': {
        *VEC_PUSH(&data, char) = '\"';
        state = StringParserText;
        break;
      }
      case '\\': {
        *VEC_PUSH(&data, char) = '\\';
        state = StringParserText;
        break;
      }
      case '/': {
        *VEC_PUSH(&data, char) = '/';
        state = StringParserText;
        break;
      }
      case 'b': {
        *VEC_PUSH(&data, char) = '\b';
        state = StringParserText;
        break;
      }
      case 'f': {
        *VEC_PUSH(&data, char) = '\f';
        state = StringParserText;
        break;
      }
      case 'n': {
        *VEC_PUSH(&data, char) = '\n';
        state = StringParserText;
        break;
      }
      case 'r': {
        *VEC_PUSH(&data, char) = '\r';
        state = StringParserText;
        break;
      }
      case 't': {
        *VEC_PUSH(&data, char) = '\t';
        state = StringParserText;
        break;
      }
      case 'u': {
        state = StringParserUnicode;
        break;
      }
      default: {
        *VEC_PUSH(diagnostics, j_Error) =
            ERROR(j_StrInvalidControlChar, l->position);
        state = StringParserText;
        break;
      }
      }
      break;
    }
    case StringParserUnicode: {
      uint32_t code_point = 0;
      for (int i = 0; i < 4; i++) {
        c = lex_next(l);
        if (c == EOF) {
          *VEC_PUSH(diagnostics, j_Error) =
              ERROR(j_StrExpectedDoubleQuote, l->position);
          goto LOOPEND;
        }
        int8_t value = fromHex((char)c);
        if (value < 0) {
          *VEC_PUSH(diagnostics, j_Error) =
              ERROR(j_StrInvalidUnicodeSpecifier, l->position);
          value = 0;
        }
        code_point += code_point * 16 + (uint8_t)value;
      }
      encodeUTFPoint(&data, code_point);
      state = StringParserText;
      break;
    }
    }
  }

LOOPEND:;
  size_t len = VEC_LEN(&data, char);
  *VEC_PUSH(&data, char) = '\0';
  return J_STR(vec_release(&data), len);
}

static j_Prop j_parseProp(Lexer *l, Vector *diagnostics, Allocator *a);

static j_Elem j_certain_parseArrayElem(Lexer *l, Vector *diagnostics,
                                       Allocator *a) {
  assert(lex_next(l) == '[');

  // vector of elements
  Vector elems = vec_create(a);

  typedef enum {
    ArrayParseStart,
    ArrayParseExpectCommaOrEnd,
    ArrayParseExpectElem,
  } ArrayParseState;

  ArrayParseState state = ArrayParseStart;

  while (true) {
    switch (state) {
    case ArrayParseStart: {
      skipWhitespace(l);
      if (lex_peek(l) == ']') {
        goto CLEANUP;
      } else {
        state = ArrayParseExpectElem;
      }
      break;
    }
    case ArrayParseExpectCommaOrEnd: {
      skipWhitespace(l);
      int32_t c = lex_peek(l);
      switch (c) {
      case ',': {
        lex_next(l);
        state = ArrayParseExpectElem;
        break;
      }
      case ']': {
        lex_next(l);
        goto CLEANUP;
      }
      case EOF: {
        *VEC_PUSH(diagnostics, j_Error) =
            ERROR(j_ArrayExpectedJsonElem, l->position);
        goto CLEANUP;
      }
      default: {
        *VEC_PUSH(diagnostics, j_Error) =
            ERROR(j_ArrayExpectedRightBracket, l->position);
        lex_next(l);
        break;
      }
      }
      break;
    }
    case ArrayParseExpectElem: {
      *VEC_PUSH(&elems, j_Elem) = j_parseElem(l, diagnostics, a);
      state = ArrayParseExpectCommaOrEnd;
      break;
    }
    }
  }
CLEANUP:;
  size_t len = VEC_LEN(&elems, j_Elem);
  return J_ARRAY_ELEM(vec_release(&elems), len);
}

static j_Elem j_certain_parseStrElem(Lexer *l, Vector *diagnostics,
                                     Allocator *a) {
  assert(lex_peek(l) == '\"');
  return J_STR_ELEM(j_parseStr(l, diagnostics, a));
}

static j_Prop j_parseProp(Lexer *l, Vector *diagnostics, Allocator *a) {
  j_Str key = j_parseStr(l, diagnostics, a);
  skipWhitespace(l);
  if (lex_next(l) != ':') {
    *VEC_PUSH(diagnostics, j_Error) = ERROR(j_PropExpectedColon, l->position);
  }
  j_Elem value = j_parseElem(l, diagnostics, a);
  return J_PROP(key, value);
}

static j_Elem j_certain_parseObjectElem(Lexer *l, Vector *diagnostics,
                                        Allocator *a) {
  assert(lex_next(l) == '{');

  // vector of properties
  Vector props = vec_create(a);

  typedef enum {
    ObjectParseStart,
    ObjectParseExpectCommaOrEnd,
    ObjectParseExpectProp,
  } ObjectParseState;

  ObjectParseState state = ObjectParseStart;

  while (true) {
    switch (state) {
    case ObjectParseStart: {
      skipWhitespace(l);
      if (lex_peek(l) == '}') {
        goto CLEANUP;
      } else {
        state = ObjectParseExpectProp;
      }
      break;
    }
    case ObjectParseExpectCommaOrEnd: {
      skipWhitespace(l);
      switch (lex_peek(l)) {
      case ',': {
        lex_next(l);
        state = ObjectParseExpectProp;
        break;
      }
      case '}': {
        lex_next(l);
        goto CLEANUP;
      }
      case EOF: {
        *VEC_PUSH(diagnostics, j_Error) =
            ERROR(j_ArrayExpectedJsonElem, l->position);
        goto CLEANUP;
      }
      default: {
        *VEC_PUSH(diagnostics, j_Error) =
            ERROR(j_ArrayExpectedRightBracket, l->position);
        lex_next(l);
        break;
      }
      }
      break;
    }
    case ObjectParseExpectProp: {
      *VEC_PUSH(&props, j_Prop) = j_parseProp(l, diagnostics, a);
      state = ObjectParseExpectCommaOrEnd;
      break;
    }
    }
  }
CLEANUP:;
  size_t len = VEC_LEN(&props, j_Prop);
  return J_OBJECT_ELEM(vec_release(&props), len);
}

j_Elem j_parseElem(Lexer *l, Vector *diagnostics, Allocator *a) {
  skipWhitespace(l);

  int32_t c = lex_peek(l);
  switch (c) {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case '-': {
    return j_certain_parseNumberElem(l, diagnostics, a);
  }
  case 't':
  case 'f':
  case 'n': {
    return j_certain_parseLiteralElem(l, diagnostics, a);
  }
  case '\"': {
    return j_certain_parseStrElem(l, diagnostics, a);
  }
  case '[': {
    return j_certain_parseArrayElem(l, diagnostics, a);
  }
  case '{': {
    return j_certain_parseObjectElem(l, diagnostics, a);
  }
  case EOF: {
    *VEC_PUSH(diagnostics, j_Error) = ERROR(j_ElemEof, l->position);
    return J_NULL_ELEM;
  }
  default: {
    *VEC_PUSH(diagnostics, j_Error) =
        ERROR(j_ElemUnknownCharacter, l->position);
    lex_next(l);
    return J_NULL_ELEM;
  }
  }
}