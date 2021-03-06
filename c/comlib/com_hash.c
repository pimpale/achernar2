#include "com_hash.h"

//-----------------------------------------------------------------------------
// SipHash reference C implementation
//
// Copyright (c) 2012-2016 Jean-Philippe Aumasson
// <jeanphilippe.aumasson@ gmail.com>
// Copyright (c) 2012-2014 Daniel J. Bernstein <djb@ cr.yp.to>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//
// default: SipHash-2-4
//-----------------------------------------------------------------------------
static inline u64 SIP64(const u8 *in, const usize inlen, u64 seed0, u64 seed1) {
#define U8TO64_LE(p)                                                           \
  {                                                                            \
    (((u64)((p)[0])) | ((u64)((p)[1]) << 8) | ((u64)((p)[2]) << 16) |          \
     ((u64)((p)[3]) << 24) | ((u64)((p)[4]) << 32) | ((u64)((p)[5]) << 40) |   \
     ((u64)((p)[6]) << 48) | ((u64)((p)[7]) << 56))                            \
  }
#define U64TO8_LE(p, v)                                                        \
  {                                                                            \
    U32TO8_LE((p), (u32)((v)));                                                \
    U32TO8_LE((p) + 4, (u32)((v) >> 32));                                      \
  }
#define U32TO8_LE(p, v)                                                        \
  {                                                                            \
    (p)[0] = (u8)((v));                                                        \
    (p)[1] = (u8)((v) >> 8);                                                   \
    (p)[2] = (u8)((v) >> 16);                                                  \
    (p)[3] = (u8)((v) >> 24);                                                  \
  }
#define ROTL(x, b) (u64)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND                                                               \
  {                                                                            \
    v0 += v1;                                                                  \
    v1 = ROTL(v1, 13);                                                         \
    v1 ^= v0;                                                                  \
    v0 = ROTL(v0, 32);                                                         \
    v2 += v3;                                                                  \
    v3 = ROTL(v3, 16);                                                         \
    v3 ^= v2;                                                                  \
    v0 += v3;                                                                  \
    v3 = ROTL(v3, 21);                                                         \
    v3 ^= v0;                                                                  \
    v2 += v1;                                                                  \
    v1 = ROTL(v1, 17);                                                         \
    v1 ^= v2;                                                                  \
    v2 = ROTL(v2, 32);                                                         \
  }
  u64 k0 = U8TO64_LE((u8 *)&seed0);
  u64 k1 = U8TO64_LE((u8 *)&seed1);
  u64 v3 = 0x7465646279746573 ^ k1;
  u64 v2 = 0x6c7967656e657261 ^ k0;
  u64 v1 = 0x646f72616e646f6d ^ k1;
  u64 v0 = 0x736f6d6570736575 ^ k0;
  const u8 *end = in + inlen - (inlen % sizeof(u64));
  for (; in != end; in += 8) {
    u64 m = U8TO64_LE(in);
    v3 ^= m;
    SIPROUND
    SIPROUND
    v0 ^= m;
  }
  const int left = inlen & 7;
  u64 b = ((u64)inlen) << 56;
  switch (left) {
  case 7:
    b |= ((u64)in[6]) << 48;
    attr_FALLTHROUGH;
  case 6:
    b |= ((u64)in[5]) << 40;
    attr_FALLTHROUGH;
  case 5:
    b |= ((u64)in[4]) << 32;
    attr_FALLTHROUGH;
  case 4:
    b |= ((u64)in[3]) << 24;
    attr_FALLTHROUGH;
  case 3:
    b |= ((u64)in[2]) << 16;
    attr_FALLTHROUGH;
  case 2:
    b |= ((u64)in[1]) << 8;
    attr_FALLTHROUGH;
  case 1:
    b |= ((u64)in[0]);
    break;
  case 0:
    break;
  }
  v3 ^= b;
  SIPROUND
  SIPROUND
  v0 ^= b;
  v2 ^= 0xff;
  SIPROUND
  SIPROUND
  SIPROUND
  SIPROUND
  b = v0 ^ v1 ^ v2 ^ v3;
  u64 out = 0;
  U64TO8_LE((u8 *)&out, b)
  return out;
}

u64 com_hash_fnv1a(u64 seed, const com_str data) {
  u64 hash = 0xcbf29ce484222325;
  // hash seed
  for(usize i = 0; i < sizeof(seed); i++) {
    hash ^= (seed >> (i*8)) & 0xFF;
    hash *= 0x100000001b3;
  }
  // now hash data
  for (usize i = 0; i < data.len; i++) {
    hash ^= data.data[i];
    hash *= 0x100000001b3;
  }
  return hash;
}

u64 com_hash_sip(u64 seed, const com_str data) {
  // setting the first half of the 128 bit seed to zero
  // not really as secure as it could be, but good enough
  return SIP64(data.data, data.len, 0, seed);
}
