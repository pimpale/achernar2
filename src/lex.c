#include "lex.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "vector.h"

/* Token Functions */

Token* newToken(SymType type, void* payload) {
  Token* t = malloc(sizeof(Token));
  t->type = type;
  t->payload = payload;
  return t;
}

void freeToken(Token* token) {
  if (token->type == SymStringLiteral || token->type == SymFloatLiteral ||
      token->type == SymIntLiteral) {
    free(token->payload);
  }
  free(token);
}

typedef struct {
  uint64_t val;
  ErrVal err;
} ResultU64;

// Parses integer with radix
static ResultU64 parseInteger(char* str, size_t len, uint64_t radix) {}

static void lexComment(Parseable* stream, Vector* tokens) {
  UNUSED(tokens);
  if (nextValue(stream) != '#') {
    logError(ErrLevelError,
             "malformed comment found at line " PRIu64 " and column " PRIu64
             "\n",
             stream->lineNumber, stream->charNumber);
  }
  int32_t c;
  while ((c = nextValue(stream)) != EOF) {
    if (c == '\n') {
      break;
    }
  }
}

static void lexStringLiteral(Parseable* stream, Vector* tokens) {
  if (nextValue(stream) != '\"') {
    logError(ErrLevelError, "malformed string: " PRIu64 ", " PRIu64 "\n",
             stream->lineNumber, stream->charNumber);
  }
  Vector* string = newVector();

  int32_t c;
  while ((c = nextValue(stream)) != EOF) {
    if (c == '\"') {
      break;
    } else {
      *VEC_PUSH(string, char) = (char)c;
    }
  }
  // Push null byte
  *VEC_PUSH(string, char) = (char)0;

  // Copy data to token, then delete the vector
  Token* t = newToken(SymStringLiteral, malloc(lengthVector(string)));
  memcpy(t->payload, getVector(string, 0), lengthVector(string));
  deleteVector(string);

  // Put token in token list
  *VEC_PUSH(tokens, Token*) = t;
}

static void parseNumberLiteral(Parseable* stream, Vector* tokens) {
  Vector* data = newVector();

  // Used to determine if float or not
  // decimalPointIndex will only be defined if hasDecimalPoint is true
  bool hasDecimalPoint = false;
  size_t decimalPointIndex = 0;

  size_t length = 0;
  int32_t c;
  while ((c = nextValue(stream)) != EOF) {
    if (isdigit(c)                 // Normal digit
        || c == '.'                // Decimal point
        || (c >= 'a' && c <= 'f')  // Hexadecimal Character
        || c == 'b' || c == 'd' || c == 'o' ||
        c == 'x'  // Character Interpretation
    ) {
      // If there's a decimal point we note the location
      // If this is the second time we've seen it, then the float is malformed
      if (c == '.') {
        if (hasDecimalPoint) {
          logError(ErrLevelError,
                   "malformed float literal: excess decimal point: " PRIu64
                   ", " PRIu64 "\n",
                   stream->lineNumber, stream->charNumber);
          return;
        } else {
          hasDecimalPoint = true;
          decimalPointIndex = length;
        }
      }
      *VEC_PUSH(data, char) = (char)c;
      length++;
    } else {
      break;
    }
  }
  // Push null byte
  *VEC_PUSH(data, char) = '\0';

  // If it's an integer
  if (!hasDecimalPoint) {
    uint64_t radix;
    // this is the null terminated string storing the text of the number
    char* intStr;
    size_t intStrLen;

    // Special Radix
    // More than 2 characters and first character is 0 and second character is
    // not digit
    if (length > 2 && *VEC_GET(data, 0, char) == '0' &&
        !isdigit(*VEC_GET(data, 1, char))) {
      // Set radix to what it has to be
      char secondCharacter = *VEC_GET(data, 1, char);
      switch (secondCharacter) {
        case 'b': {
          radix = 2;
          break;
        }
        case 'd': {
          radix = 10;
          break;
        }
        case 'o': {
          radix = 8;
          break;
        }
        case 'x': {
          radix = 16;
          break;
        }
        default: {
          logError(ErrLevelError,
                   "malformed integer literal: unrecognized special radix "
                   "code: " PRIu64 ", " PRIu64 "\n",
                   stream->lineNumber, stream->charNumber);
          return;
        }
      }
      intStr = VEC_GET(data, 2, char);
      intStrLen = VEC_LEN(data, char) - 2;
    } else {
      radix = 10;
      intStr = VEC_GET(data, 0, char);
      intStrLen = VEC_LEN(data, char);
    }

    ResultU64 ret = parseInteger(intStr, intStrLen, radix);
    if (ret.err != ErrOk) {
      logError(ErrLevelError,
               "malformed integer literal: %s: " PRIu64 ", " PRIu64 "\n",
               strErrVal(ret.err), stream->lineNumber, stream->charNumber);
      return;
    } else {
      Token* t = newToken(SymIntLiteral, malloc(sizeof(ret.val)));
      memcpy(t->payload, &ret.val, sizeof(ret.val));
      *VEC_PUSH(tokens, Token*) = t;
    }
  } else {
    // If it has a decimal point, it must be a float
    // We have already guaranteed that the string only has one decimal point, at
    // decimalPointIndex Floats are always in decimal notation

    // We parse the float as 2 integers, one above the decimal point, and one
    // below
    char* initialPortion = VEC_GET(data, 0, char);
    uint64_t initialPortionLen = decimalPointIndex;

    char* finalPortion = VEC_GET(data, decimalPointIndex + 1, char);
    uint64_t finalPortionLen = VEC_LEN(data, char) - decimalPointIndex;

    double result = 0;

    // Parse the part ahead of the decimal point
    ResultU64 initialRet = parseInteger(initialPortion, initialPortionLen, 10);
    if (initialRet.err == ErrOk) {
      result += initialRet.val;
    } else {
      logError(ErrLevelError,
               "malformed float literal: %s: " PRIu64 ", " PRIu64 "\n",
               strErrVal(initialRet.err), stream->lineNumber,
               stream->charNumber);
      return;
    }
    // If there's a bit after the inital part, then we must add it
    if (finalPortionLen > 0) {
      ResultU64 finalRet = parseInteger(finalPortion, finalPortionLen, 10);
      if (finalRet.err == ErrOk) {
        // don't want to include math.h, so we'll repeatedly divide by 10
        double decimalResult = finalRet.val;
        for(size_t i = 0; i < finalPortionLen; i++) {
          decimalResult /= 10;
        }
        result += decimalResult;
      } else {
        logError(ErrLevelError,
                 "malformed float literal: %s: " PRIu64 ", " PRIu64 "\n",
                 strErrVal(initialRet.err), stream->lineNumber,
                 stream->charNumber);
        return;
      }
    }

    Token* t = newToken(SymIntLiteral, malloc(sizeof(result)));
    memcpy(t->payload, &result, sizeof(result));
    *VEC_PUSH(tokens, Token*) = t;
  }
}

static void parseIdentifier(Parseable* stream, Vector* tokens) {
  
}

void lex(Parseable* stream, Vector* tokens) {
  int32_t c;
  while ((c = peekValue(stream)) != EOF) {
    if (isblank(c) || c == '\n') {
    } else if (c == '#') {
      lexComment(stream, tokens);
    } else if (c == '\"') {
      lexStringLiteral(stream, tokens);
    } else if (isdigit(c)) {
      parseNumberLiteral(stream, tokens);
    } else {
      break;
    }
  }
}
