/*
 *------------------------------------------------------------------
 * Copyright (c) 2020 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *------------------------------------------------------------------
 */

#ifndef __aesni_h__
#define __aesni_h__

typedef enum
{
  AES_KEY_128 = 0,
  AES_KEY_192 = 1,
  AES_KEY_256 = 2,
} aes_key_size_t;

#define AES_KEY_ROUNDS(x)		(10 + x * 2)
#define AES_KEY_BYTES(x)		(16 + x * 8)

#ifdef __x86_64__

static_always_inline u8x16
aes_block_load (u8 * p)
{
  return (u8x16) _mm_loadu_si128 ((__m128i *) p);
}

static_always_inline u8x16
aes_enc_round (u8x16 a, u8x16 k)
{
  return (u8x16) _mm_aesenc_si128 ((__m128i) a, (__m128i) k);
}

static_always_inline u8x16
aes_enc_last_round (u8x16 a, u8x16 k)
{
  return (u8x16) _mm_aesenclast_si128 ((__m128i) a, (__m128i) k);
}

static_always_inline u8x16
aes_dec_round (u8x16 a, u8x16 k)
{
  return (u8x16) _mm_aesdec_si128 ((__m128i) a, (__m128i) k);
}

static_always_inline u8x16
aes_dec_last_round (u8x16 a, u8x16 k)
{
  return (u8x16) _mm_aesdeclast_si128 ((__m128i) a, (__m128i) k);
}

static_always_inline void
aes_block_store (u8 * p, u8x16 r)
{
  _mm_storeu_si128 ((__m128i *) p, (__m128i) r);
}

static_always_inline u8x16
aes_inv_mix_column (u8x16 a)
{
  return (u8x16) _mm_aesimc_si128 ((__m128i) a);
}

/* AES-NI based AES key expansion based on code samples from
   Intel(r) Advanced Encryption Standard (AES) New Instructions White Paper
   (323641-001) */

static_always_inline void
aes128_key_assist (__m128i * k, __m128i r)
{
  __m128i t = k[-1];
  t ^= _mm_slli_si128 (t, 4);
  t ^= _mm_slli_si128 (t, 4);
  t ^= _mm_slli_si128 (t, 4);
  k[0] = t ^ _mm_shuffle_epi32 (r, 0xff);
}

static_always_inline void
aes128_key_expand (u8x16 * key_schedule, u8 * key)
{
  __m128i *k = (__m128i *) key_schedule;
  k[0] = _mm_loadu_si128 ((const __m128i *) key);
  aes128_key_assist (k + 1, _mm_aeskeygenassist_si128 (k[0], 0x01));
  aes128_key_assist (k + 2, _mm_aeskeygenassist_si128 (k[1], 0x02));
  aes128_key_assist (k + 3, _mm_aeskeygenassist_si128 (k[2], 0x04));
  aes128_key_assist (k + 4, _mm_aeskeygenassist_si128 (k[3], 0x08));
  aes128_key_assist (k + 5, _mm_aeskeygenassist_si128 (k[4], 0x10));
  aes128_key_assist (k + 6, _mm_aeskeygenassist_si128 (k[5], 0x20));
  aes128_key_assist (k + 7, _mm_aeskeygenassist_si128 (k[6], 0x40));
  aes128_key_assist (k + 8, _mm_aeskeygenassist_si128 (k[7], 0x80));
  aes128_key_assist (k + 9, _mm_aeskeygenassist_si128 (k[8], 0x1b));
  aes128_key_assist (k + 10, _mm_aeskeygenassist_si128 (k[9], 0x36));
}

static_always_inline void
aes192_key_assist (__m128i * r1, __m128i * r2, __m128i key_assist)
{
  __m128i t;
  *r1 ^= t = _mm_slli_si128 (*r1, 0x4);
  *r1 ^= t = _mm_slli_si128 (t, 0x4);
  *r1 ^= _mm_slli_si128 (t, 0x4);
  *r1 ^= _mm_shuffle_epi32 (key_assist, 0x55);
  *r2 ^= _mm_slli_si128 (*r2, 0x4);
  *r2 ^= _mm_shuffle_epi32 (*r1, 0xff);
}

static_always_inline void
aes192_key_expand (u8x16 * key_schedule, u8 * key)
{
  __m128i r1, r2, *k = (__m128i *) key_schedule;

  k[0] = r1 = _mm_loadu_si128 ((__m128i *) key);
  /* load the 24-bytes key as 2 * 16-bytes (and ignore last 8-bytes) */
  k[1] = r2 = CLIB_MEM_OVERFLOW_LOAD (_mm_loadu_si128, (__m128i *) key + 1);

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x1));
  k[1] = (__m128i) _mm_shuffle_pd ((__m128d) k[1], (__m128d) r1, 0);
  k[2] = (__m128i) _mm_shuffle_pd ((__m128d) r1, (__m128d) r2, 1);

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x2));
  k[3] = r1;
  k[4] = r2;

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x4));
  k[4] = (__m128i) _mm_shuffle_pd ((__m128d) k[4], (__m128d) r1, 0);
  k[5] = (__m128i) _mm_shuffle_pd ((__m128d) r1, (__m128d) r2, 1);

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x8));
  k[6] = r1;
  k[7] = r2;

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x10));
  k[7] = (__m128i) _mm_shuffle_pd ((__m128d) k[7], (__m128d) r1, 0);
  k[8] = (__m128i) _mm_shuffle_pd ((__m128d) r1, (__m128d) r2, 1);

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x20));
  k[9] = r1;
  k[10] = r2;

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x40));
  k[10] = (__m128i) _mm_shuffle_pd ((__m128d) k[10], (__m128d) r1, 0);
  k[11] = (__m128i) _mm_shuffle_pd ((__m128d) r1, (__m128d) r2, 1);

  aes192_key_assist (&r1, &r2, _mm_aeskeygenassist_si128 (r2, 0x80));
  k[12] = r1;
}

static_always_inline void
aes256_key_assist (__m128i * k, int i, __m128i key_assist)
{
  __m128i r, t;
  k += i;
  r = k[-2];
  r ^= t = _mm_slli_si128 (r, 0x4);
  r ^= t = _mm_slli_si128 (t, 0x4);
  r ^= _mm_slli_si128 (t, 0x4);
  r ^= _mm_shuffle_epi32 (key_assist, 0xff);
  k[0] = r;

  if (i >= 14)
    return;

  r = k[-1];
  r ^= t = _mm_slli_si128 (r, 0x4);
  r ^= t = _mm_slli_si128 (t, 0x4);
  r ^= _mm_slli_si128 (t, 0x4);
  r ^= _mm_shuffle_epi32 (_mm_aeskeygenassist_si128 (k[0], 0x0), 0xaa);
  k[1] = r;
}

static_always_inline void
aes256_key_expand (u8x16 * key_schedule, u8 * key)
{
  __m128i *k = (__m128i *) key_schedule;
  k[0] = _mm_loadu_si128 ((__m128i *) key);
  k[1] = _mm_loadu_si128 ((__m128i *) (key + 16));
  aes256_key_assist (k, 2, _mm_aeskeygenassist_si128 (k[1], 0x01));
  aes256_key_assist (k, 4, _mm_aeskeygenassist_si128 (k[3], 0x02));
  aes256_key_assist (k, 6, _mm_aeskeygenassist_si128 (k[5], 0x04));
  aes256_key_assist (k, 8, _mm_aeskeygenassist_si128 (k[7], 0x08));
  aes256_key_assist (k, 10, _mm_aeskeygenassist_si128 (k[9], 0x10));
  aes256_key_assist (k, 12, _mm_aeskeygenassist_si128 (k[11], 0x20));
  aes256_key_assist (k, 14, _mm_aeskeygenassist_si128 (k[13], 0x40));
}
#endif

#ifdef __aarch64__

static_always_inline u8x16
aes_inv_mix_column (u8x16 a)
{
  return vaesimcq_u8 (a);
}

static const u8x16 aese_prep_mask1 =
  { 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15, 12 };
static const u8x16 aese_prep_mask2 =
  { 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15 };

static inline void
aes128_key_expand_round_neon (u8x16 * rk, u32 rcon)
{
  u8x16 r, t, last_round = rk[-1], z = { };
  r = vqtbl1q_u8 (last_round, aese_prep_mask1);
  r = vaeseq_u8 (r, z);
  r ^= (u8x16) vdupq_n_u32 (rcon);
  r ^= last_round;
  r ^= t = vextq_u8 (z, last_round, 12);
  r ^= t = vextq_u8 (z, t, 12);
  r ^= vextq_u8 (z, t, 12);
  rk[0] = r;
}

void
aes128_key_expand (u8x16 * rk, const u8 * k)
{
  rk[0] = vld1q_u8 (k);
  aes128_key_expand_round_neon (rk + 1, 0x01);
  aes128_key_expand_round_neon (rk + 2, 0x02);
  aes128_key_expand_round_neon (rk + 3, 0x04);
  aes128_key_expand_round_neon (rk + 4, 0x08);
  aes128_key_expand_round_neon (rk + 5, 0x10);
  aes128_key_expand_round_neon (rk + 6, 0x20);
  aes128_key_expand_round_neon (rk + 7, 0x40);
  aes128_key_expand_round_neon (rk + 8, 0x80);
  aes128_key_expand_round_neon (rk + 9, 0x1b);
  aes128_key_expand_round_neon (rk + 10, 0x36);
}

static inline void
aes192_key_expand_round_neon (u8x8 * rk, u32 rcon)
{
  u8x8 r, last_round = rk[-1], z = { };
  u8x16 r2, z2 = { };

  r2 = (u8x16) vdupq_lane_u64 ((uint64x1_t) last_round, 0);
  r2 = vqtbl1q_u8 (r2, aese_prep_mask1);
  r2 = vaeseq_u8 (r2, z2);
  r2 ^= (u8x16) vdupq_n_u32 (rcon);

  r = (u8x8) vdup_laneq_u64 ((u64x2) r2, 0);
  r ^= rk[-3];
  r ^= vext_u8 (z, rk[-3], 4);
  rk[0] = r;

  r = rk[-2] ^ vext_u8 (r, z, 4);
  r ^= vext_u8 (z, r, 4);
  rk[1] = r;

  if (rcon == 0x80)
    return;

  r = rk[-1] ^ vext_u8 (r, z, 4);
  r ^= vext_u8 (z, r, 4);
  rk[2] = r;
}

void
aes192_key_expand (u8x16 * ek, const u8 * k)
{
  u8x8 *rk = (u8x8 *) ek;
  ek[0] = vld1q_u8 (k);
  rk[2] = vld1_u8 (k + 16);
  aes192_key_expand_round_neon (rk + 3, 0x01);
  aes192_key_expand_round_neon (rk + 6, 0x02);
  aes192_key_expand_round_neon (rk + 9, 0x04);
  aes192_key_expand_round_neon (rk + 12, 0x08);
  aes192_key_expand_round_neon (rk + 15, 0x10);
  aes192_key_expand_round_neon (rk + 18, 0x20);
  aes192_key_expand_round_neon (rk + 21, 0x40);
  aes192_key_expand_round_neon (rk + 24, 0x80);
}


static inline void
aes256_key_expand_round_neon (u8x16 * rk, u32 rcon)
{
  u8x16 r, t, z = { };

  r = vqtbl1q_u8 (rk[-1], rcon ? aese_prep_mask1 : aese_prep_mask2);
  r = vaeseq_u8 (r, z);
  if (rcon)
    r ^= (u8x16) vdupq_n_u32 (rcon);
  r ^= rk[-2];
  r ^= t = vextq_u8 (z, rk[-2], 12);
  r ^= t = vextq_u8 (z, t, 12);
  r ^= vextq_u8 (z, t, 12);
  rk[0] = r;
}

void
aes256_key_expand (u8x16 * rk, const u8 * k)
{
  rk[0] = vld1q_u8 (k);
  rk[1] = vld1q_u8 (k + 16);
  aes256_key_expand_round_neon (rk + 2, 0x01);
  aes256_key_expand_round_neon (rk + 3, 0);
  aes256_key_expand_round_neon (rk + 4, 0x02);
  aes256_key_expand_round_neon (rk + 5, 0);
  aes256_key_expand_round_neon (rk + 6, 0x04);
  aes256_key_expand_round_neon (rk + 7, 0);
  aes256_key_expand_round_neon (rk + 8, 0x08);
  aes256_key_expand_round_neon (rk + 9, 0);
  aes256_key_expand_round_neon (rk + 10, 0x10);
  aes256_key_expand_round_neon (rk + 11, 0);
  aes256_key_expand_round_neon (rk + 12, 0x20);
  aes256_key_expand_round_neon (rk + 13, 0);
  aes256_key_expand_round_neon (rk + 14, 0x40);
}

#endif

static_always_inline void
aes_key_expand (u8x16 * key_schedule, u8 * key, aes_key_size_t ks)
{
  switch (ks)
    {
    case AES_KEY_128:
      aes128_key_expand (key_schedule, key);
      break;
    case AES_KEY_192:
      aes192_key_expand (key_schedule, key);
      break;
    case AES_KEY_256:
      aes256_key_expand (key_schedule, key);
      break;
    }
}

static_always_inline void
aes_key_enc_to_dec (u8x16 * ke, u8x16 * kd, aes_key_size_t ks)
{
  int rounds = AES_KEY_ROUNDS (ks);

  kd[rounds] = ke[0];
  kd[0] = ke[rounds];

  for (int i = 1; i < (rounds / 2); i++)
    {
      kd[rounds - i] = aes_inv_mix_column (ke[i]);
      kd[i] = aes_inv_mix_column (ke[rounds - i]);
    }

  kd[rounds / 2] = aes_inv_mix_column (ke[rounds / 2]);
}

#endif /* __aesni_h__ */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
