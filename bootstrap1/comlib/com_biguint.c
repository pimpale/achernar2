#include "com_biguint.h"
#include "com_assert.h"
#include "com_imath.h"

com_biguint com_biguint_create(com_allocator_Handle h) {
  return (com_biguint){._array = com_vec_create(h)};
}

void com_biguint_release(com_biguint *a) {
  com_assert_m(a != NULL, "a is null");
  com_vec_destroy(&a->_array);
}

void com_biguint_set_u64(com_biguint *dest, u64 val) {
  com_assert_m(dest != NULL, "dest is null");
  // u64 means only 2 u32 s are needed in the vector
  com_vec *v = &dest->_array;
  com_vec_set_len_m(v, 2, u32);

  u32 *arr = com_vec_get(v, 0);
  // first is lower 32 bits of input
  arr[0] = val & 0x00000000FFFFFFFFu;
  // next is the upper 32 bits
  arr[1] = val >> (u64)32;
}

u64 com_biguint_get_u64(const com_biguint *a) {
  com_assert_m(a != NULL, "a is null");

  switch (com_vec_len_m(&a->_array, u32)) {
  case 0: {
    return 0;
  }
  case 1: {
    return *com_vec_get_m(&a->_array, 0, u32);
  }
  default: {
    u32 *arr = com_vec_get_m(&a->_array, 0, u32);
    return ((u64)arr[1] << (u64)32) + (u64)arr[0];
  }
  }
}

// Adds together a and b into DEST
// REQUIRES: `dest` is a pointer to a valid com_biguint
// REQUIRES: `a` is a pointer to an array of at least `alen` u32s
// REQUIRES: `b` is a pointer to an array of at least `blen` u32s
// REQUIRES: alen >= blen
// GUARANTEES: `dest` will contain the sum of a and b
static void internal_value_add_u32_arr(com_biguint *dest, u32 *a, usize alen,
                                       u32 *b, usize blen) {
  com_assert_m(alen >= blen, "alen is less than blen");
  // first we have to find the size of the result
  // NOTE: Thanks to carrying, the result size could be 1 larger than this
  // We guarantee that `alen` is the larger len
  usize dest_len = alen;

  // extend dest arr to new size
  com_vec_set_len(&dest->_array, dest_len);

  // get destination array
  u32 *dest_arr = com_vec_get_m(&dest->_array, 0, u32);

  // represents the amount to be carried after adding one word
  u8 carry = 0;

  // procedure is:
  // add together each word, if it overflows then we carry 1 over to the next
  // word

  // in this for loop, a[i] and b[i] are both valid
  for (usize i = 0; i < blen; i++) {
    u32 aval = a[i];
    u32 bval = b[i];
    u64 tmp = aval + bval + carry;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      carry = 1;
      dest_arr[i] = tmp - u32_max_m;
    } else {
      // if the value can fit in a u32
      carry = 0;
      dest_arr[i] = tmp;
    }
  }

  // this is essentially the loop where we are finished adding,
  // and are just handling any times where carrying the 1 forward will cause
  // overflow and require another carry

  // in this for loop, only a[i] is valid
  for (usize i = blen; i < alen; i++) {
    u32 aval = a[i];
    u64 tmp = aval + carry;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      dest_arr[i] = tmp - u32_max_m;
      carry = 1;
    } else {
      // if the value can fit in a u32
      dest_arr[i] = tmp;
      carry = 0;
      // we can break as a shortcut here, since it won't affect anything
      // now that we're finished carrying
      break;
    }
  }

  // in the last part, if we are still carrying, we need to extend the length
  // of dest to handle that
  if (carry != 0) {
    *com_vec_push_m(&dest->_array, u32) = carry;
  }
}

// compares the magnitude of b with reference to a
/// REQUIRES: `a` is a pointer to an array of at least `alen` u32s
/// REQUIRES: `b` is a pointer to an array of at least `blen` u32s
/// REQUIRES: alen >= blen
static com_math_cmptype internal_value_cmp_u32_arr(u32 *a, usize alen, u32 *b,
                                                   usize blen) {
  com_assert_m(alen >= blen, "alen is less than blen");

  // compare backwards

  // first we compare where only A is defined

  for (usize i = alen; i > blen; i--) {
    // in this form,  i will always be 1 greater than what we want
    u32 aval = a[i - 1];
    // since bval is not defined here we default to zero
    u32 bval = 0;

    if (aval < bval) {
      return com_math_GREATER;
    } else if (aval > bval) {
      return com_math_LESS;
    }
  }

  // next compare where both a and b are defined
  for (usize i = blen; i != 0; i--) {
    // note that in this form, i will always be 1 greater than what we want
    u32 aval = a[i - 1];
    u32 bval = b[i - 1];

    if (aval < bval) {
      return com_math_GREATER;
    } else if (aval > bval) {
      return com_math_LESS;
    }
  }

  return com_math_EQUAL;
}

com_math_cmptype com_biguint_cmp(const com_biguint *a, const com_biguint *b) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  // get a pointer to the beginning of the array
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  u32 *b_arr = com_vec_get_m(&b->_array, 0, u32);

  // if alen >= blen, compare them forward and return result normally
  // else, we swap them and then reverse the result before returning

  if (alen >= blen) {
    // return the result of the comparison
    return internal_value_cmp_u32_arr(a_arr, alen, b_arr, blen);
  } else {
    // means we have to swap A and B and then swap the result before returning
    // We do this to respect the requirement that the first value provided always is >= than the second value
    usize small_len = alen;
    usize big_len = blen;
    u32 *small_arr = a_arr;
    u32 *big_arr = b_arr;
    com_math_cmptype ret =
        internal_value_cmp_u32_arr(big_arr, big_len, small_arr, small_len);
    // reverse the result before returning
    switch (ret) {
    case com_math_EQUAL: {
      return com_math_EQUAL;
    }
    case com_math_GREATER: {
      return com_math_LESS;
    }
    case com_math_LESS: {
      return com_math_GREATER;
    }
    }
  }
}

// sets DEST to a - b
// REQUIRES: `dest` is a pointer to a valid com_biguint
// REQUIRES: `a` is a pointer to an array of at least `alen` u32s
// REQUIRES: `b` is a pointer to an array of at least `blen` u32s
// REQUIRES: alen >= blen
// REQUIRES: the value held by a is greater than the value held by
// GUARANTEES: `dest` will contain the difference of a and b
static void internal_value_sub_u32_arr(com_biguint *dest, u32 *a, usize alen,
                                       u32 *b, usize blen) {
  com_assert_m(alen >= blen, "alen is less than blen");
  com_assert_m(internal_value_cmp_u32_arr(a, alen, b, blen) != com_math_LESS,
               "a < b");

  // first we have to find the size of the result
  // alen is the MAX width that our result could have, because
  // We've already guaranteed that `alen` is the larger len,
  // and also that a - b will always be less than or equal to a
  com_vec *dest_vec = &dest->_array;

  // resize dest_vec to zero, we will rebuild it back up
  com_vec_set_len(&dest->_array, 0);

  // represents the amount to be borrowed if necessary
  u8 borrow = 0;

  // procedure is:
  // sub each word: a + next_word - b + borrow
  // If it's greater than max length, then we didn't need the borrow, and
  // subtract it before setting the word in the dest array
  // Else, we did need the borrow, and set borrow to -1 so we can
  // subtract it from the next word

  // in this for loop, a[i] and b[i] are both valid
  for (usize i = 0; i < blen; i++) {
    u32 aval = a[i];
    u32 bval = b[i];
    u64 tmp = u32_max_m + aval - bval - borrow;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      // Means we didn't need the borrow
      borrow = 0;
      *com_vec_push_m(dest_vec, u32)  = tmp - u32_max_m;
    } else {
      // if the value can fit in a u32
      // Means we needed the borrow
      borrow = 1;
      *com_vec_push_m(dest_vec, u32) = tmp;
    }
  }

  // this is essentially the loop where we are finished subtracting,
  // and are just handling any times where borrowing the forward will cause
  // problems and require another carry

  // in this for loop, only a[i] is valid
  for (usize i = blen; i < alen; i++) {
    u32 aval = a[i];
    u64 tmp = u32_max_m + aval - borrow;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      *com_vec_push_m(dest_vec, u32) = tmp - u32_max_m;
      // means we didn't need to borrow
      borrow = 0;
      // we can break as a shortcut here, since it won't affect anything
      // now that we're finished borrowing
      break;
    } else {
      // if the value can fit in a u32
      // Means we needed the borrow
      *com_vec_push_m(dest_vec, u32) = tmp;
      borrow = 1;
    }
  }

  com_assert_m(borrow == 0,
               "even after subtraction, we still need a borrow, means a < b");
}

void com_bignum_add_u32(com_biguint *a, u32 b) {
  com_assert_m(a != NULL, "a is null");
  usize a_len = com_vec_len_m(&a->_array, u32);
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  if (a_len == 0) {
     // if a_len is 0, then the value of a is zero, so we can just set it
     com_biguint_set_u64(a, b);
  } else {
    // we treat the `b` like a 1 length array
    internal_value_add_u32_arr(a, a_arr, a_len, &b, 1u);
  }
}

void com_bignum_sub_u32(com_biguint *a, u32 b) {
  com_assert_m(a != NULL, "a is null");
  usize a_len = com_vec_len_m(&a->_array, u32);
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  // we treat the `b` like a 1 length array
  if (a_len == 0) {
      // if a_len == 0, then it means a is zero.
      // negative numbers are invalid because this is a uint
      com_assert_m(b == 0, "trying to subtract a nonzero number from a zero biguint");
      return;
  } else {
    internal_value_sub_u32_arr(a, a_arr, a_len, &b, 1u);
  }
}

