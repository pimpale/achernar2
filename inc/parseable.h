#ifndef PARSEABLE_H
#define PARSEABLE_H

#include <stdint.h>
#include <stdio.h>

typedef enum {
  PARSEABLE_BACKING_MEMORY,
  PARSEABLE_BACKING_FILE,
} ParseableBacking;

/* Do not manually modify any of these values  */

typedef struct {
  /* The backing data structure */
  ParseableBacking backing;
  FILE* file;
  char* memory;
  size_t len;
  /* location in file */
  size_t loc;
  /* Caches the last values of the last char fetched for ungetc */
  int32_t lastVal;
  /* Caches the number of newlines encountered */
  uint64_t lineNumber;
  /* Caches the number of characters encountered in this line*/
  uint64_t charNumber;
} Parseable;


Parseable* newParseableFile(FILE* file);
Parseable* newParseableMemory(char* ptr, size_t len);

int32_t nextValue(Parseable* p);
void backValue(Parseable* p);

void freeParseable(Parseable* p);

#endif /* PARSEABLE_H */
