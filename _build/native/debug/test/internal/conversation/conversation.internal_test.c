#ifdef __cplusplus
extern "C" {
#endif

#include "moonbit.h"

#ifdef _MSC_VER
#define _Noreturn __declspec(noreturn)
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wshift-op-parentheses"
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif

MOONBIT_EXPORT _Noreturn void moonbit_panic(void);
MOONBIT_EXPORT void *moonbit_malloc_array(enum moonbit_block_kind kind,
                                          int elem_size_shift, int32_t len);
MOONBIT_EXPORT int moonbit_val_array_equal(const void *lhs, const void *rhs);
MOONBIT_EXPORT moonbit_string_t moonbit_add_string(moonbit_string_t s1,
                                                   moonbit_string_t s2);
MOONBIT_EXPORT void moonbit_unsafe_bytes_blit(moonbit_bytes_t dst,
                                              int32_t dst_start,
                                              moonbit_bytes_t src,
                                              int32_t src_offset, int32_t len);
MOONBIT_EXPORT moonbit_string_t moonbit_unsafe_bytes_sub_string(
    moonbit_bytes_t bytes, int32_t start, int32_t len);
MOONBIT_EXPORT void moonbit_println(moonbit_string_t str);
MOONBIT_EXPORT moonbit_bytes_t *moonbit_get_cli_args(void);
MOONBIT_EXPORT void moonbit_runtime_init(int argc, char **argv);
MOONBIT_EXPORT void moonbit_drop_object(void *);

#define Moonbit_make_regular_object_header(ptr_field_offset, ptr_field_count,  \
                                           tag)                                \
  (((uint32_t)moonbit_BLOCK_KIND_REGULAR << 30) |                              \
   (((uint32_t)(ptr_field_offset) & (((uint32_t)1 << 11) - 1)) << 19) |        \
   (((uint32_t)(ptr_field_count) & (((uint32_t)1 << 11) - 1)) << 8) |          \
   ((tag) & 0xFF))

// header manipulation macros
#define Moonbit_object_ptr_field_offset(obj)                                   \
  ((Moonbit_object_header(obj)->meta >> 19) & (((uint32_t)1 << 11) - 1))

#define Moonbit_object_ptr_field_count(obj)                                    \
  ((Moonbit_object_header(obj)->meta >> 8) & (((uint32_t)1 << 11) - 1))

#if !defined(_WIN64) && !defined(_WIN32)
void *malloc(size_t size);
void free(void *ptr);
#define libc_malloc malloc
#define libc_free free
#endif

// several important runtime functions are inlined
static void *moonbit_malloc_inlined(size_t size) {
  struct moonbit_object *ptr = (struct moonbit_object *)libc_malloc(
      sizeof(struct moonbit_object) + size);
  ptr->rc = 1;
  return ptr + 1;
}

#define moonbit_malloc(obj) moonbit_malloc_inlined(obj)
#define moonbit_free(obj) libc_free(Moonbit_object_header(obj))

static void moonbit_incref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const count = header->rc;
  if (count > 0) {
    header->rc = count + 1;
  }
}

#define moonbit_incref moonbit_incref_inlined

static void moonbit_decref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const count = header->rc;
  if (count > 1) {
    header->rc = count - 1;
  } else if (count == 1) {
    moonbit_drop_object(ptr);
  }
}

#define moonbit_decref moonbit_decref_inlined

#define moonbit_unsafe_make_string moonbit_make_string

// detect whether compiler builtins exist for advanced bitwise operations
#ifdef __has_builtin

#if __has_builtin(__builtin_clz)
#define HAS_BUILTIN_CLZ
#endif

#if __has_builtin(__builtin_ctz)
#define HAS_BUILTIN_CTZ
#endif

#if __has_builtin(__builtin_popcount)
#define HAS_BUILTIN_POPCNT
#endif

#if __has_builtin(__builtin_sqrt)
#define HAS_BUILTIN_SQRT
#endif

#if __has_builtin(__builtin_sqrtf)
#define HAS_BUILTIN_SQRTF
#endif

#if __has_builtin(__builtin_fabs)
#define HAS_BUILTIN_FABS
#endif

#if __has_builtin(__builtin_fabsf)
#define HAS_BUILTIN_FABSF
#endif

#endif

// if there is no builtin operators, use software implementation
#ifdef HAS_BUILTIN_CLZ
static inline int32_t moonbit_clz32(int32_t x) {
  return x == 0 ? 32 : __builtin_clz(x);
}

static inline int32_t moonbit_clz64(int64_t x) {
  return x == 0 ? 64 : __builtin_clzll(x);
}

#undef HAS_BUILTIN_CLZ
#else
// table for [clz] value of 4bit integer.
static const uint8_t moonbit_clz4[] = {4, 3, 2, 2, 1, 1, 1, 1,
                                       0, 0, 0, 0, 0, 0, 0, 0};

int32_t moonbit_clz32(uint32_t x) {
  /* The ideas is to:

     1. narrow down the 4bit block where the most signficant "1" bit lies,
        using binary search
     2. find the number of leading zeros in that 4bit block via table lookup

     Different time/space tradeoff can be made here by enlarging the table
     and do less binary search.
     One benefit of the 4bit lookup table is that it can fit into a single cache
     line.
  */
  int32_t result = 0;
  if (x > 0xffff) {
    x >>= 16;
  } else {
    result += 16;
  }
  if (x > 0xff) {
    x >>= 8;
  } else {
    result += 8;
  }
  if (x > 0xf) {
    x >>= 4;
  } else {
    result += 4;
  }
  return result + moonbit_clz4[x];
}

int32_t moonbit_clz64(uint64_t x) {
  int32_t result = 0;
  if (x > 0xffffffff) {
    x >>= 32;
  } else {
    result += 32;
  }
  return result + moonbit_clz32((uint32_t)x);
}
#endif

#ifdef HAS_BUILTIN_CTZ
static inline int32_t moonbit_ctz32(int32_t x) {
  return x == 0 ? 32 : __builtin_ctz(x);
}

static inline int32_t moonbit_ctz64(int64_t x) {
  return x == 0 ? 64 : __builtin_ctzll(x);
}

#undef HAS_BUILTIN_CTZ
#else
int32_t moonbit_ctz32(int32_t x) {
  /* The algorithm comes from:

       Leiserson, Charles E. et al. “Using de Bruijn Sequences to Index a 1 in a
     Computer Word.” (1998).

     The ideas is:

     1. leave only the least significant "1" bit in the input,
        set all other bits to "0". This is achieved via [x & -x]
     2. now we have [x * n == n << ctz(x)], if [n] is a de bruijn sequence
        (every 5bit pattern occurn exactly once when you cycle through the bit
     string), we can find [ctz(x)] from the most significant 5 bits of [x * n]
 */
  static const uint32_t de_bruijn_32 = 0x077CB531;
  static const uint8_t index32[] = {0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
                                    15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
                                    16, 7,  26, 12, 18, 6,  11, 5,  10, 9};
  return (x == 0) * 32 + index32[(de_bruijn_32 * (x & -x)) >> 27];
}

int32_t moonbit_ctz64(int64_t x) {
  static const uint64_t de_bruijn_64 = 0x0218A392CD3D5DBF;
  static const uint8_t index64[] = {
      0,  1,  2,  7,  3,  13, 8,  19, 4,  25, 14, 28, 9,  34, 20, 40,
      5,  17, 26, 38, 15, 46, 29, 48, 10, 31, 35, 54, 21, 50, 41, 57,
      63, 6,  12, 18, 24, 27, 33, 39, 16, 37, 45, 47, 30, 53, 49, 56,
      62, 11, 23, 32, 36, 44, 52, 55, 61, 22, 43, 51, 60, 42, 59, 58};
  return (x == 0) * 64 + index64[(de_bruijn_64 * (x & -x)) >> 58];
}
#endif

#ifdef HAS_BUILTIN_POPCNT

#define moonbit_popcnt32 __builtin_popcount
#define moonbit_popcnt64 __builtin_popcountll
#undef HAS_BUILTIN_POPCNT

#else
int32_t moonbit_popcnt32(uint32_t x) {
  /* The classic SIMD Within A Register algorithm.
     ref: [https://nimrod.blog/posts/algorithms-behind-popcount/]
 */
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F;
  return (x * 0x01010101) >> 24;
}

int32_t moonbit_popcnt64(uint64_t x) {
  x = x - ((x >> 1) & 0x5555555555555555);
  x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;
  return (x * 0x0101010101010101) >> 56;
}
#endif

/* The following sqrt implementation comes from
   [musl](https://git.musl-libc.org/cgit/musl),
   with some helpers inlined to make it zero dependency.
 */
#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
const uint16_t __rsqrt_tab[128] = {
    0xb451, 0xb2f0, 0xb196, 0xb044, 0xaef9, 0xadb6, 0xac79, 0xab43, 0xaa14,
    0xa8eb, 0xa7c8, 0xa6aa, 0xa592, 0xa480, 0xa373, 0xa26b, 0xa168, 0xa06a,
    0x9f70, 0x9e7b, 0x9d8a, 0x9c9d, 0x9bb5, 0x9ad1, 0x99f0, 0x9913, 0x983a,
    0x9765, 0x9693, 0x95c4, 0x94f8, 0x9430, 0x936b, 0x92a9, 0x91ea, 0x912e,
    0x9075, 0x8fbe, 0x8f0a, 0x8e59, 0x8daa, 0x8cfe, 0x8c54, 0x8bac, 0x8b07,
    0x8a64, 0x89c4, 0x8925, 0x8889, 0x87ee, 0x8756, 0x86c0, 0x862b, 0x8599,
    0x8508, 0x8479, 0x83ec, 0x8361, 0x82d8, 0x8250, 0x81c9, 0x8145, 0x80c2,
    0x8040, 0xff02, 0xfd0e, 0xfb25, 0xf947, 0xf773, 0xf5aa, 0xf3ea, 0xf234,
    0xf087, 0xeee3, 0xed47, 0xebb3, 0xea27, 0xe8a3, 0xe727, 0xe5b2, 0xe443,
    0xe2dc, 0xe17a, 0xe020, 0xdecb, 0xdd7d, 0xdc34, 0xdaf1, 0xd9b3, 0xd87b,
    0xd748, 0xd61a, 0xd4f1, 0xd3cd, 0xd2ad, 0xd192, 0xd07b, 0xcf69, 0xce5b,
    0xcd51, 0xcc4a, 0xcb48, 0xca4a, 0xc94f, 0xc858, 0xc764, 0xc674, 0xc587,
    0xc49d, 0xc3b7, 0xc2d4, 0xc1f4, 0xc116, 0xc03c, 0xbf65, 0xbe90, 0xbdbe,
    0xbcef, 0xbc23, 0xbb59, 0xba91, 0xb9cc, 0xb90a, 0xb84a, 0xb78c, 0xb6d0,
    0xb617, 0xb560,
};

/* returns a*b*2^-32 - e, with error 0 <= e < 1.  */
static inline uint32_t mul32(uint32_t a, uint32_t b) {
  return (uint64_t)a * b >> 32;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float sqrtf(float x) {
  uint32_t ix, m, m1, m0, even, ey;

  ix = *(uint32_t *)&x;
  if (ix - 0x00800000 >= 0x7f800000 - 0x00800000) {
    /* x < 0x1p-126 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7f800000)
      return x;
    if (ix > 0x7f800000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p23f;
    ix = *(uint32_t *)&x;
    ix -= 23 << 23;
  }

  /* x = 4^e m; with int e and m in [1, 4).  */
  even = ix & 0x00800000;
  m1 = (ix << 8) | 0x80000000;
  m0 = (ix << 7) & 0x7fffffff;
  m = even ? m0 : m1;

  /* 2^e is the exponent part of the return value.  */
  ey = ix >> 1;
  ey += 0x3f800000 >> 1;
  ey &= 0x7f800000;

  /* compute r ~ 1/sqrt(m), s ~ sqrt(m) with 2 goldschmidt iterations.  */
  static const uint32_t three = 0xc0000000;
  uint32_t r, s, d, u, i;
  i = (ix >> 17) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r*sqrt(m) - 1| < 0x1p-8 */
  s = mul32(m, r);
  /* |s/sqrt(m) - 1| < 0x1p-8 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r*sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  s = mul32(s, u);
  /* -0x1.03p-28 < s/sqrt(m) - 1 < 0x1.fp-31 */
  s = (s - 1) >> 6;
  /* s < sqrt(m) < s + 0x1.08p-23 */

  /* compute nearest rounded result.  */
  uint32_t d0, d1, d2;
  float y, t;
  d0 = (m << 16) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 31;
  s &= 0x007fffff;
  s |= ey;
  y = *(float *)&s;
  /* handle rounding and inexact exception. */
  uint32_t tiny = d2 == 0 ? 0 : 0x01000000;
  tiny |= (d1 ^ d2) & 0x80000000;
  t = *(float *)&tiny;
  y = y + t;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
/* returns a*b*2^-64 - e, with error 0 <= e < 3.  */
static inline uint64_t mul64(uint64_t a, uint64_t b) {
  uint64_t ahi = a >> 32;
  uint64_t alo = a & 0xffffffff;
  uint64_t bhi = b >> 32;
  uint64_t blo = b & 0xffffffff;
  return ahi * bhi + (ahi * blo >> 32) + (alo * bhi >> 32);
}

double sqrt(double x) {
  uint64_t ix, top, m;

  /* special case handling.  */
  ix = *(uint64_t *)&x;
  top = ix >> 52;
  if (top - 0x001 >= 0x7ff - 0x001) {
    /* x < 0x1p-1022 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7ff0000000000000)
      return x;
    if (ix > 0x7ff0000000000000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p52;
    ix = *(uint64_t *)&x;
    top = ix >> 52;
    top -= 52;
  }

  /* argument reduction:
     x = 4^e m; with integer e, and m in [1, 4)
     m: fixed point representation [2.62]
     2^e is the exponent part of the result.  */
  int even = top & 1;
  m = (ix << 11) | 0x8000000000000000;
  if (even)
    m >>= 1;
  top = (top + 0x3ff) >> 1;

  /* approximate r ~ 1/sqrt(m) and s ~ sqrt(m) when m in [1,4)

     initial estimate:
     7bit table lookup (1bit exponent and 6bit significand).

     iterative approximation:
     using 2 goldschmidt iterations with 32bit int arithmetics
     and a final iteration with 64bit int arithmetics.

     details:

     the relative error (e = r0 sqrt(m)-1) of a linear estimate
     (r0 = a m + b) is |e| < 0.085955 ~ 0x1.6p-4 at best,
     a table lookup is faster and needs one less iteration
     6 bit lookup table (128b) gives |e| < 0x1.f9p-8
     7 bit lookup table (256b) gives |e| < 0x1.fdp-9
     for single and double prec 6bit is enough but for quad
     prec 7bit is needed (or modified iterations). to avoid
     one more iteration >=13bit table would be needed (16k).

     a newton-raphson iteration for r is
       w = r*r
       u = 3 - m*w
       r = r*u/2
     can use a goldschmidt iteration for s at the end or
       s = m*r

     first goldschmidt iteration is
       s = m*r
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     next goldschmidt iteration is
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     and at the end r is not computed only s.

     they use the same amount of operations and converge at the
     same quadratic rate, i.e. if
       r1 sqrt(m) - 1 = e, then
       r2 sqrt(m) - 1 = -3/2 e^2 - 1/2 e^3
     the advantage of goldschmidt is that the mul for s and r
     are independent (computed in parallel), however it is not
     "self synchronizing": it only uses the input m in the
     first iteration so rounding errors accumulate. at the end
     or when switching to larger precision arithmetics rounding
     errors dominate so the first iteration should be used.

     the fixed point representations are
       m: 2.30 r: 0.32, s: 2.30, d: 2.30, u: 2.30, three: 2.30
     and after switching to 64 bit
       m: 2.62 r: 0.64, s: 2.62, d: 2.62, u: 2.62, three: 2.62  */

  static const uint64_t three = 0xc0000000;
  uint64_t r, s, d, u, i;

  i = (ix >> 46) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r sqrt(m) - 1| < 0x1.fdp-9 */
  s = mul32(m >> 32, r);
  /* |s/sqrt(m) - 1| < 0x1.fdp-9 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
  r = r << 32;
  s = mul64(m, r);
  d = mul64(s, r);
  u = (three << 32) - d;
  s = mul64(s, u); /* repr: 3.61 */
  /* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
  s = (s - 2) >> 9; /* repr: 12.52 */
  /* -0x1.09p-52 < s - sqrt(m) < -0x1.fffcp-63 */

  /* s < sqrt(m) < s + 0x1.09p-52,
     compute nearest rounded result:
     the nearest result to 52 bits is either s or s+0x1p-52,
     we can decide by comparing (2^52 s + 0.5)^2 to 2^104 m.  */
  uint64_t d0, d1, d2;
  double y, t;
  d0 = (m << 42) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 63;
  s &= 0x000fffffffffffff;
  s |= top << 52;
  y = *(double *)&s;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
double fabs(double x) {
  union {
    double f;
    uint64_t i;
  } u = {x};
  u.i &= 0x7fffffffffffffffULL;
  return u.f;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float fabsf(float x) {
  union {
    float f;
    uint32_t i;
  } u = {x};
  u.i &= 0x7fffffff;
  return u.f;
}
#endif

#ifdef _MSC_VER
/* MSVC treats syntactic division by zero as fatal error,
   even for float point numbers,
   so we have to use a constant variable to work around this */
static const int MOONBIT_ZERO = 0;
#else
#define MOONBIT_ZERO 0
#endif

#ifdef __cplusplus
}
#endif
struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE;

struct _M0TWssbEu;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTP38clawteam8clawteam2ai7Message6System;

struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails;

struct _M0TPB6Logger;

struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__;

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE3Err;

struct _M0TPB19MulShiftAll64Result;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE3Err;

struct _M0TWEOUsRPB4JsonE;

struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json;

struct _M0DTPC16result6ResultGRPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB7NoErrorE2Ok;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE2Ok;

struct _M0TWEOs;

struct _M0DTP38clawteam8clawteam2ai7Message9Assistant;

struct _M0DTP38clawteam8clawteam2ai7Message4User;

struct _M0TP48clawteam8clawteam8internal6openai11CostDetails;

struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage;

struct _M0TWRPC15error5ErrorEu;

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTP38clawteam8clawteam2ai7Message4Tool;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB7NoErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__;

struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE;

struct _M0DTPC16option6OptionGdE4Some;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal12conversation33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall;

struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE;

struct _M0TWEuQRPC15error5Error;

struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok;

struct _M0TP48clawteam8clawteam8internal12conversation7Message;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal12conversation33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant;

struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGRPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB7NoErrorE3Err;

struct _M0R38String_3a_3aiter_2eanon__u2122__l247__;

struct _M0TP38clawteam8clawteam2ai8ToolCall;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE;

struct _M0DTPC15error5Error115clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0KTPB6ToJsonTPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB7NoErrorE2Ok;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TP38clawteam8clawteam2ai5Usage;

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails;

struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__;

struct _M0DTPB4Json6Object;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam;

struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE2Ok;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails {
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails*(* code)(
    struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails*,
    int32_t
  );
  
};

struct _M0DTPC14json10WriteFrame6Object {
  int32_t $1;
  struct _M0TWEOUsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPB4Json5Array {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal12conversation7Message** $0;
  
};

struct _M0TWssbEu {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TUsiE {
  int32_t $1;
  moonbit_string_t $0;
  
};

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $5;
  
};

struct _M0TWEOc {
  int32_t(* code)(struct _M0TWEOc*);
  
};

struct _M0TPB5ArrayGRPC14json10WriteFrameE {
  int32_t $1;
  void** $0;
  
};

struct _M0TPB17FloatingDecimal64 {
  uint64_t $0;
  int32_t $1;
  
};

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  void* $1;
  
};

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0DTP38clawteam8clawteam2ai7Message6System {
  moonbit_string_t $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails {
  int64_t $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json*,
    struct _M0TP48clawteam8clawteam8internal12conversation7Message*
  );
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool {
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* $0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPB4Json6Number {
  double $0;
  moonbit_string_t $1;
  
};

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*,
    struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
  );
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TPB6ToJson {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE2Ok {
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTP38clawteam8clawteam2ai7Message9Assistant {
  moonbit_string_t $0;
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* $1;
  
};

struct _M0DTP38clawteam8clawteam2ai7Message4User {
  moonbit_string_t $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai11CostDetails {
  void* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage {
  int32_t $0;
  int32_t $4;
  int32_t $6;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* $1;
  void* $2;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* $3;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* $5;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0DTP38clawteam8clawteam2ai7Message4Tool {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE {
  int32_t $1;
  int32_t $2;
  void** $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE {
  int32_t $1;
  struct _M0TP38clawteam8clawteam2ai8ToolCall** $0;
  
};

struct _M0DTPC16option6OptionGdE4Some {
  double $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal12conversation33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text {
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* $0;
  
};

struct _M0TUsRPB6LoggerE {
  moonbit_string_t $0;
  struct _M0BTPB6Logger* $1_0;
  void* $1_1;
  
};

struct _M0TPB13StringBuilder {
  int32_t $1;
  moonbit_bytes_t $0;
  
};

struct _M0TPB9ArrayViewGUsRPB4JsonEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB4JsonE** $0;
  
};

struct _M0TPB5ArrayGUsiEE {
  int32_t $1;
  struct _M0TUsiE** $0;
  
};

struct _M0TWRPC15error5ErrorEs {
  moonbit_string_t(* code)(struct _M0TWRPC15error5ErrorEs*, void*);
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall {
  moonbit_string_t $0;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* $1;
  
};

struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall {
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*(* code)(
    struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*,
    struct _M0TP38clawteam8clawteam2ai8ToolCall*
  );
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE {
  int32_t $1;
  void** $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok {
  void* $0;
  
};

struct _M0TP48clawteam8clawteam8internal12conversation7Message {
  int64_t $0;
  struct _M0TP38clawteam8clawteam2ai5Usage* $1;
  void* $2;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal12conversation33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*,
    void*
  );
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant {
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* $2;
  
};

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2122__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
};

struct _M0TP38clawteam8clawteam2ai8ToolCall {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** $0;
  
};

struct _M0DTPC15error5Error115clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User {
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPB4Json6String {
  moonbit_string_t $0;
  
};

struct _M0KTPB6ToJsonTPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0TPB13SourceLocRepr {
  int32_t $0_1;
  int32_t $0_2;
  int32_t $1_1;
  int32_t $1_2;
  int32_t $2_1;
  int32_t $2_2;
  int32_t $3_1;
  int32_t $3_2;
  int32_t $4_1;
  int32_t $4_2;
  int32_t $5_1;
  int32_t $5_2;
  moonbit_string_t $0_0;
  moonbit_string_t $1_0;
  moonbit_string_t $2_0;
  moonbit_string_t $3_0;
  moonbit_string_t $4_0;
  moonbit_string_t $5_0;
  
};

struct _M0TUsRPB4JsonE {
  moonbit_string_t $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB7NoErrorE2Ok {
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* $0;
  
};

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** $0;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TP38clawteam8clawteam2ai5Usage {
  int32_t $0;
  int32_t $1;
  int32_t $2;
  int64_t $3;
  
};

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails {
  int64_t $0;
  int64_t $1;
  
};

struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System {
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  
};

struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam {
  int64_t $1;
  moonbit_string_t $0;
  
};

struct _M0TPB5EntryGsRPB4JsonE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB4JsonE* $1;
  moonbit_string_t $4;
  void* $5;
  
};

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai19PromptTokensDetailsRPB7NoErrorE2Ok {
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* $0;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0TPB3MapGsRPB4JsonE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRPB4JsonE** $0;
  struct _M0TPB5EntryGsRPB4JsonE* $5;
  
};

struct _M0TPC13ref3RefGiE {
  int32_t $0;
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0TUWEuQRPC15error5ErrorNsE {
  struct _M0TWEuQRPC15error5Error* $0;
  moonbit_string_t* $1;
  
};

struct _M0TPB7Umul128 {
  uint64_t $0;
  uint64_t $1;
  
};

struct _M0TPB8Pow5Pair {
  uint64_t $0;
  uint64_t $1;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0MP38clawteam8clawteam2ai8ToolCall20to__openai_2edyncall(
  struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*,
  struct _M0TP38clawteam8clawteam2ai8ToolCall*
);

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*,
  void*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal12conversation51____test__74797065735f6a736f6e2e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json*,
  struct _M0TP48clawteam8clawteam8internal12conversation7Message*
);

int32_t _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN17error__to__stringS1476(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN14handle__resultS1467(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testC3245l435(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testC3241l436(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal12conversation45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1396(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1391(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1378(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal12conversation28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal12conversation34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

void* _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal12conversation7Message*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal12conversation41____test__74797065735f6a736f6e2e6d6274__0(
  
);

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0MP38clawteam8clawteam2ai5Usage10to__openai(
  struct _M0TP38clawteam8clawteam2ai5Usage*
);

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0MP38clawteam8clawteam2ai5Usage10to__openaiC3025l95(
  struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails*,
  int32_t
);

void* _M0MP38clawteam8clawteam2ai7Message10to__openai(void*);

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0MP38clawteam8clawteam2ai8ToolCall10to__openai(
  struct _M0TP38clawteam8clawteam2ai8ToolCall*
);

struct _M0TP38clawteam8clawteam2ai5Usage* _M0FP38clawteam8clawteam2ai13usage_2einner(
  int32_t,
  int32_t,
  int32_t,
  int64_t
);

void* _M0FP38clawteam8clawteam2ai13user__message(moonbit_string_t);

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(
  void*
);

void* _M0IP48clawteam8clawteam8internal6openai12CacheControlPB6ToJson8to__json(
  int32_t
);

void* _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__json(
  void*
);

int32_t _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1247(
  int32_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  struct _M0TPB3MapGsRPB4JsonE*
);

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

void* _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction*
);

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0MP48clawteam8clawteam8internal6openai15CompletionUsage3new(
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails*,
  void*,
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails*,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails*,
  int32_t
);

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0FP48clawteam8clawteam8internal6openai10tool__call(
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGsE(
  moonbit_string_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai13tool__messageGsE(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai13user__messageGsE(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai15system__messageGsE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai19text__content__part(
  moonbit_string_t,
  int64_t
);

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0MP48clawteam8clawteam8internal6openai19PromptTokensDetails3new(
  int64_t,
  int64_t
);

void* _M0IP48clawteam8clawteam8internal6openai23CompletionTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails*
);

void* _M0IP48clawteam8clawteam8internal6openai11CostDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails*
);

void* _M0IP48clawteam8clawteam8internal6openai19PromptTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails*
);

void* _M0IP48clawteam8clawteam8internal6openai15CompletionUsagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage*
);

void* _M0IP38clawteam8clawteam5clock9TimestampPB6ToJson8to__json(int64_t);

int64_t _M0MP38clawteam8clawteam5clock9Timestamp8from__ms(int64_t);

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson,
  void*,
  moonbit_string_t,
  struct _M0TPB5ArrayGORPB9SourceLocE*
);

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void*,
  int32_t,
  int32_t,
  struct _M0TWsRPB4JsonEORPB4Json*
);

moonbit_string_t _M0FPC14json11indent__str(int32_t, int32_t);

moonbit_string_t _M0FPC14json6escape(moonbit_string_t, int32_t);

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*
);

int32_t _M0MPC15array5Array9is__emptyGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  int32_t,
  int64_t
);

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr*,
  struct _M0TPB6Logger
);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

int32_t _M0IPC16double6DoublePB4Show6output(double, struct _M0TPB6Logger);

moonbit_string_t _M0MPC16double6Double10to__string(double);

moonbit_string_t _M0FPB15ryu__to__string(double);

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(uint64_t, int32_t);

moonbit_string_t _M0FPB9to__chars(struct _M0TPB17FloatingDecimal64*, int32_t);

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(uint64_t, uint32_t);

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t);

int64_t _M0MPC14bool4Bool9to__int64(int32_t);

int32_t _M0MPC14bool4Bool7to__int(int32_t);

int32_t _M0FPB17decimal__length17(uint64_t);

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t);

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t);

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t,
  struct _M0TPB8Pow5Pair,
  int32_t,
  int32_t
);

int32_t _M0FPB18multipleOfPowerOf2(uint64_t, int32_t);

int32_t _M0FPB18multipleOfPowerOf5(uint64_t, int32_t);

int32_t _M0FPB10pow5Factor(uint64_t);

uint64_t _M0FPB13shiftright128(uint64_t, uint64_t, int32_t);

struct _M0TPB7Umul128 _M0FPB7umul128(uint64_t, uint64_t);

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t,
  int32_t,
  int32_t
);

int32_t _M0FPB9log10Pow2(int32_t);

int32_t _M0FPB9log10Pow5(int32_t);

moonbit_string_t _M0FPB18copy__special__str(int32_t, int32_t, int32_t);

int32_t _M0FPB8pow5bits(int32_t);

int32_t _M0IPC13int3IntPB4Hash13hash__combine(int32_t, struct _M0TPB6Hasher*);

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t,
  struct _M0TPB6Hasher*
);

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher*,
  moonbit_string_t
);

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t,
  int32_t
);

void* _M0IPC15array9ArrayViewPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal12conversation7MessageE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE*
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal12conversation7MessageRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE*,
  struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json*
);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0MPC15array5Array3mapGRP38clawteam8clawteam2ai8ToolCallRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE*,
  struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*
);

void* _M0IPC16double6DoublePB6ToJson8to__json(double);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2394l591(
  struct _M0TWEOUsRPB4JsonE*
);

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(struct _M0TPB3MapGsRPB4JsonE*);

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE
);

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TUWEuQRPC15error5ErrorNsE*
);

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  moonbit_string_t,
  void*
);

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map4growGsRPB4JsonE(struct _M0TPB3MapGsRPB4JsonE*);

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TUWEuQRPC15error5ErrorNsE*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  moonbit_string_t,
  void*,
  int32_t
);

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  int32_t,
  struct _M0TPB5EntryGsRPB4JsonE*
);

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  struct _M0TPB5EntryGsRPB4JsonE*,
  int32_t
);

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  int32_t,
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(int32_t);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0MPC16option6Option3mapGiRP48clawteam8clawteam8internal6openai19PromptTokensDetailsE(
  int64_t,
  struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails*
);

moonbit_string_t _M0MPC16option6Option10unwrap__orGsE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t
);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0MPC15array5Array12make__uninitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  int32_t
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2141l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2122l247(struct _M0TWEOc*);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*,
  void*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(int32_t);

moonbit_string_t _M0MPC16string6String6repeat(moonbit_string_t, int32_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t,
  int32_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

int32_t _M0MPC15array9ArrayView6lengthGsE(struct _M0TPB9ArrayViewGsE);

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE
);

int32_t _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE
);

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t,
  int64_t,
  int64_t
);

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView
);

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView,
  struct _M0TPB6Logger
);

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE*,
  int32_t,
  int32_t
);

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t);

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t);

int32_t _M0IPC14byte4BytePB3Sub3sub(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Mod3mod(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Div3div(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Add3add(int32_t, int32_t);

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs*);

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE*
);

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc*);

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(uint64_t, int32_t);

moonbit_string_t _M0MPC15int645Int6418to__string_2einner(int64_t, int32_t);

int32_t _M0FPB22int64__to__string__dec(uint16_t*, uint64_t, int32_t, int32_t);

int32_t _M0FPB26int64__to__string__generic(
  uint16_t*,
  uint64_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0FPB22int64__to__string__hex(uint16_t*, uint64_t, int32_t, int32_t);

int32_t _M0FPB14radix__count64(uint64_t, int32_t);

int32_t _M0FPB12hex__count64(uint64_t);

int32_t _M0FPB12dec__count64(uint64_t);

moonbit_string_t _M0MPC13int3Int18to__string_2einner(int32_t, int32_t);

int32_t _M0FPB14radix__count32(uint32_t, int32_t);

int32_t _M0FPB12hex__count32(uint32_t);

int32_t _M0FPB12dec__count32(uint32_t);

int32_t _M0FPB20int__to__string__dec(uint16_t*, uint32_t, int32_t, int32_t);

int32_t _M0FPB24int__to__string__generic(
  uint16_t*,
  uint32_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0FPB20int__to__string__hex(uint16_t*, uint32_t, int32_t, int32_t);

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs*);

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE*
);

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc*);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(int32_t);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t
);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(uint64_t);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void*
);

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView6length(struct _M0TPC16string10StringView);

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t);

int32_t _M0IP016_24default__implPB4Hash4hashGsE(moonbit_string_t);

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t);

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t);

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher*);

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher*);

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB6Hasher7combineGiE(struct _M0TPB6Hasher*, int32_t);

int32_t _M0MPB6Hasher7combineGsE(struct _M0TPB6Hasher*, moonbit_string_t);

uint64_t _M0MPC13int3Int10to__uint64(int32_t);

void* _M0MPC14json4Json6number(double, moonbit_string_t);

int32_t _M0MPB6Hasher12combine__int(struct _M0TPB6Hasher*, int32_t);

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE*
);

moonbit_string_t _M0MPB9SourceLoc16to__json__string(moonbit_string_t);

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr*
);

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t
);

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t,
  int32_t,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder*,
  double
);

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(moonbit_string_t);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

void** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
);

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t);

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t);

int32_t _M0FPB32code__point__of__surrogate__pair(int32_t, int32_t);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0IPC14byte4BytePB7Default7default();

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t,
  int32_t,
  int32_t
);

int32_t _M0MPC14uint4UInt8to__byte(uint32_t);

uint32_t _M0MPC14char4Char8to__uint(int32_t);

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder*
);

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

#define _M0FPB19unsafe__sub__string moonbit_unsafe_bytes_sub_string

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(int32_t);

int32_t _M0MPC14byte4Byte8to__char(int32_t);

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE**,
  int32_t,
  struct _M0TUsiE**,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void**,
  int32_t,
  void**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t,
  int32_t,
  moonbit_bytes_t,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE**,
  int32_t,
  struct _M0TUsiE**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void**,
  int32_t,
  void**,
  int32_t,
  int32_t
);

int32_t _M0FPB5abortGiE(moonbit_string_t, moonbit_string_t);

int32_t _M0FPB5abortGuE(moonbit_string_t, moonbit_string_t);

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPB5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB6Hasher13combine__uint(struct _M0TPB6Hasher*, uint32_t);

int32_t _M0MPB6Hasher8consume4(struct _M0TPB6Hasher*, uint32_t);

uint32_t _M0FPB4rotl(uint32_t, int32_t);

int32_t _M0IPB7FailurePB4Show6output(void*, struct _M0TPB6Logger);

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t);

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE*);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPC15abort5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t
);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FP15Error10to__string(void*);

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal12conversation7MessageE(
  void*
);

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  moonbit_string_t
);

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    86, 105, 101, 119, 32, 105, 110, 100, 101, 120, 32, 111, 117, 116, 
    32, 111, 102, 32, 98, 111, 117, 110, 100, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 115, 115, 105, 115, 116, 97, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    117, 112, 115, 116, 114, 101, 97, 109, 95, 105, 110, 102, 101, 114, 
    101, 110, 99, 101, 95, 99, 111, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    99, 111, 109, 112, 108, 101, 116, 105, 111, 110, 95, 116, 111, 107, 
    101, 110, 115, 95, 100, 101, 116, 97, 105, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    45, 73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_74 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    116, 105, 109, 101, 115, 116, 97, 109, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 50, 54, 51, 
    58, 53, 45, 50, 54, 51, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    117, 115, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    99, 111, 110, 118, 101, 114, 115, 97, 116, 105, 111, 110, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    99, 111, 110, 118, 101, 114, 115, 97, 116, 105, 111, 110, 95, 106, 
    115, 111, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_104 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 121, 112, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[106]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 105), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 111, 
    110, 118, 101, 114, 115, 97, 116, 105, 111, 110, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 
    114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 
    115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_62 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_53 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    114, 111, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 111, 110, 118, 
    101, 114, 115, 97, 116, 105, 111, 110, 34, 44, 32, 34, 102, 105, 
    108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    99, 97, 99, 104, 101, 95, 99, 111, 110, 116, 114, 111, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    116, 111, 116, 97, 108, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    77, 101, 115, 115, 97, 103, 101, 32, 116, 111, 95, 106, 115, 111, 
    110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    44, 32, 34, 109, 101, 115, 115, 97, 103, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 53, 55, 52, 
    58, 53, 45, 53, 55, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 121, 115, 116, 101, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    109, 101, 115, 115, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    117, 115, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_78 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    101, 112, 104, 101, 109, 101, 114, 97, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 101, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    111, 110, 118, 101, 114, 115, 97, 116, 105, 111, 110, 58, 116, 121, 
    112, 101, 115, 95, 106, 115, 111, 110, 46, 109, 98, 116, 58, 49, 
    53, 58, 49, 54, 45, 49, 53, 58, 50, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    99, 111, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 111, 111, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    111, 110, 118, 101, 114, 115, 97, 116, 105, 111, 110, 58, 116, 121, 
    112, 101, 115, 95, 106, 115, 111, 110, 46, 109, 98, 116, 58, 49, 
    53, 58, 51, 52, 45, 50, 57, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[108]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 107), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 111, 
    110, 118, 101, 114, 115, 97, 116, 105, 111, 110, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    97, 117, 100, 105, 111, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    99, 111, 115, 116, 95, 100, 101, 116, 97, 105, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    99, 97, 99, 104, 101, 100, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    99, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    112, 114, 111, 109, 112, 116, 95, 116, 111, 107, 101, 110, 115, 95, 
    100, 101, 116, 97, 105, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    112, 114, 111, 109, 112, 116, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    114, 101, 97, 115, 111, 110, 105, 110, 103, 95, 116, 111, 107, 101, 
    110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_94 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 114, 103, 117, 109, 101, 110, 116, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    101, 120, 112, 111, 114, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 95, 105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    102, 117, 110, 99, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    111, 110, 118, 101, 114, 115, 97, 116, 105, 111, 110, 58, 116, 121, 
    112, 101, 115, 95, 106, 115, 111, 110, 46, 109, 98, 116, 58, 49, 
    53, 58, 51, 45, 50, 57, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    99, 111, 109, 112, 108, 101, 116, 105, 111, 110, 95, 116, 111, 107, 
    101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    116, 121, 112, 101, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    116, 121, 112, 101, 115, 95, 106, 115, 111, 110, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    72, 101, 108, 108, 111, 44, 32, 119, 111, 114, 108, 100, 33, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails data;
  
} const _M0MP38clawteam8clawteam2ai5Usage10to__openaiC3025l95$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MP38clawteam8clawteam2ai5Usage10to__openaiC3025l95
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN17error__to__stringS1476$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN17error__to__stringS1476
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json data;
  
} const _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json data;
  
} const _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson18to__json_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall data;
  
} const _M0MP38clawteam8clawteam2ai8ToolCall20to__openai_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MP38clawteam8clawteam2ai8ToolCall20to__openai_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json data;
  
} const _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal12conversation51____test__74797065735f6a736f6e2e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal12conversation51____test__74797065735f6a736f6e2e6d6274__0_2edyncall
  };

struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json* _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json*)&_M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal12conversation47____test__74797065735f6a736f6e2e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal12conversation51____test__74797065735f6a736f6e2e6d6274__0_2edyncall$closure.data;

struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*)&_M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0MP38clawteam8clawteam2ai8ToolCall16to__openai_2eclo =
  (struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)&_M0MP38clawteam8clawteam2ai8ToolCall20to__openai_2edyncall$closure.data;

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*)&_M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6Logger data; 
} _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6Logger) >> 2, 0, 0),
    {.$method_0 = _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_1 = _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE,
       .$method_2 = _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_3 = _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger}
  };

struct _M0BTPB6Logger* _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id =
  &_M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0172moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fconversation_2fMessage_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal12conversation7MessageE}
  };

struct _M0BTPB6ToJson* _M0FP0172moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fconversation_2fMessage_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0172moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fconversation_2fMessage_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; uint64_t data[30]; 
} _M0FPB26gDOUBLE__POW5__INV__SPLIT2$object =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 30), 
    1ull, 2305843009213693952ull, 5955668970331000884ull,
    1784059615882449851ull, 8982663654677661702ull, 1380349269358112757ull,
    7286864317269821294ull, 2135987035920910082ull, 7005857020398200553ull,
    1652639921975621497ull, 17965325103354776697ull, 1278668206209430417ull,
    8928596168509315048ull, 1978643211784836272ull, 10075671573058298858ull,
    1530901034580419511ull, 597001226353042382ull, 1184477304306571148ull,
    1527430471115325346ull, 1832889850782397517ull, 12533209867169019542ull,
    1418129833677084982ull, 5577825024675947042ull, 2194449627517475473ull,
    11006974540203867551ull, 1697873161311732311ull, 10313493231639821582ull,
    1313665730009899186ull, 12701016819766672773ull, 2032799256770390445ull
  };

uint64_t* _M0FPB26gDOUBLE__POW5__INV__SPLIT2 =
  _M0FPB26gDOUBLE__POW5__INV__SPLIT2$object.data;

struct { int32_t rc; uint32_t meta; uint32_t data[19]; 
} _M0FPB19gPOW5__INV__OFFSETS$object =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 19),
    1414808916u, 67458373u, 268701696u, 4195348u, 1073807360u, 1091917141u,
    1108u, 65604u, 1073741824u, 1140850753u, 1346716752u, 1431634004u,
    1365595476u, 1073758208u, 16777217u, 66816u, 1364284433u, 89478484u, 
    0u
  };

uint32_t* _M0FPB19gPOW5__INV__OFFSETS =
  _M0FPB19gPOW5__INV__OFFSETS$object.data;

struct { int32_t rc; uint32_t meta; uint64_t data[26]; 
} _M0FPB21gDOUBLE__POW5__SPLIT2$object =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 26), 
    0ull, 1152921504606846976ull, 0ull, 1490116119384765625ull,
    1032610780636961552ull, 1925929944387235853ull, 7910200175544436838ull,
    1244603055572228341ull, 16941905809032713930ull, 1608611746708759036ull,
    13024893955298202172ull, 2079081953128979843ull, 6607496772837067824ull,
    1343575221513417750ull, 17332926989895652603ull, 1736530273035216783ull,
    13037379183483547984ull, 2244412773384604712ull, 1605989338741628675ull,
    1450417759929778918ull, 9630225068416591280ull, 1874621017369538693ull,
    665883850346957067ull, 1211445438634777304ull, 14931890668723713708ull,
    1565756531257009982ull
  };

uint64_t* _M0FPB21gDOUBLE__POW5__SPLIT2 =
  _M0FPB21gDOUBLE__POW5__SPLIT2$object.data;

struct { int32_t rc; uint32_t meta; uint32_t data[21]; 
} _M0FPB14gPOW5__OFFSETS$object =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 21), 
    0u, 0u, 0u, 0u, 1073741824u, 1500076437u, 1431590229u, 1448432917u,
    1091896580u, 1079333904u, 1146442053u, 1146111296u, 1163220304u,
    1073758208u, 2521039936u, 1431721317u, 1413824581u, 1075134801u,
    1431671125u, 1363170645u, 261u
  };

uint32_t* _M0FPB14gPOW5__OFFSETS = _M0FPB14gPOW5__OFFSETS$object.data;

struct { int32_t rc; uint32_t meta; uint64_t data[26]; 
} _M0FPB20gDOUBLE__POW5__TABLE$object =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 26), 
    1ull, 5ull, 25ull, 125ull, 625ull, 3125ull, 15625ull, 78125ull,
    390625ull, 1953125ull, 9765625ull, 48828125ull, 244140625ull,
    1220703125ull, 6103515625ull, 30517578125ull, 152587890625ull,
    762939453125ull, 3814697265625ull, 19073486328125ull, 95367431640625ull,
    476837158203125ull, 2384185791015625ull, 11920928955078125ull,
    59604644775390625ull, 298023223876953125ull
  };

uint64_t* _M0FPB20gDOUBLE__POW5__TABLE =
  _M0FPB20gDOUBLE__POW5__TABLE$object.data;

struct { int32_t rc; uint32_t meta; struct _M0TPB17FloatingDecimal64 data; 
} _M0FPB31ryu__to__string_2erecord_2f1036$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1036 =
  &_M0FPB31ryu__to__string_2erecord_2f1036$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal12conversation48moonbit__test__driver__internal__no__args__tests;

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0L6_2aenvS3280,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L4selfS1246
) {
  return _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(_M0L4selfS1246);
}

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0MP38clawteam8clawteam2ai8ToolCall20to__openai_2edyncall(
  struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2aenvS3279,
  struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0L4selfS1318
) {
  return _M0MP38clawteam8clawteam2ai8ToolCall10to__openai(_M0L4selfS1318);
}

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json* _M0L6_2aenvS3278,
  void* _M0L4selfS1310
) {
  return _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(_M0L4selfS1310);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal12conversation51____test__74797065735f6a736f6e2e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3277
) {
  return _M0FP48clawteam8clawteam8internal12conversation41____test__74797065735f6a736f6e2e6d6274__0();
}

void* _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json* _M0L6_2aenvS3276,
  struct _M0TP48clawteam8clawteam8internal12conversation7Message* _M0L4selfS1351
) {
  return _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson8to__json(_M0L4selfS1351);
}

int32_t _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1497,
  moonbit_string_t _M0L8filenameS1472,
  int32_t _M0L5indexS1475
) {
  struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467* _closure_3758;
  struct _M0TWssbEu* _M0L14handle__resultS1467;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1476;
  void* _M0L11_2atry__errS1491;
  struct moonbit_result_0 _tmp_3760;
  int32_t _handle__error__result_3761;
  int32_t _M0L6_2atmpS3264;
  void* _M0L3errS1492;
  moonbit_string_t _M0L4nameS1494;
  struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1495;
  moonbit_string_t _M0L8_2afieldS3281;
  int32_t _M0L6_2acntS3621;
  moonbit_string_t _M0L7_2anameS1496;
  #line 534 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1472);
  _closure_3758
  = (struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467*)moonbit_malloc(sizeof(struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467));
  Moonbit_object_header(_closure_3758)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467, $1) >> 2, 1, 0);
  _closure_3758->code
  = &_M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN14handle__resultS1467;
  _closure_3758->$0 = _M0L5indexS1475;
  _closure_3758->$1 = _M0L8filenameS1472;
  _M0L14handle__resultS1467 = (struct _M0TWssbEu*)_closure_3758;
  _M0L17error__to__stringS1476
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN17error__to__stringS1476$closure.data;
  moonbit_incref(_M0L12async__testsS1497);
  moonbit_incref(_M0L17error__to__stringS1476);
  moonbit_incref(_M0L8filenameS1472);
  moonbit_incref(_M0L14handle__resultS1467);
  #line 568 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _tmp_3760
  = _M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__test(_M0L12async__testsS1497, _M0L8filenameS1472, _M0L5indexS1475, _M0L14handle__resultS1467, _M0L17error__to__stringS1476);
  if (_tmp_3760.tag) {
    int32_t const _M0L5_2aokS3273 = _tmp_3760.data.ok;
    _handle__error__result_3761 = _M0L5_2aokS3273;
  } else {
    void* const _M0L6_2aerrS3274 = _tmp_3760.data.err;
    moonbit_decref(_M0L12async__testsS1497);
    moonbit_decref(_M0L17error__to__stringS1476);
    moonbit_decref(_M0L8filenameS1472);
    _M0L11_2atry__errS1491 = _M0L6_2aerrS3274;
    goto join_1490;
  }
  if (_handle__error__result_3761) {
    moonbit_decref(_M0L12async__testsS1497);
    moonbit_decref(_M0L17error__to__stringS1476);
    moonbit_decref(_M0L8filenameS1472);
    _M0L6_2atmpS3264 = 1;
  } else {
    struct moonbit_result_0 _tmp_3762;
    int32_t _handle__error__result_3763;
    moonbit_incref(_M0L12async__testsS1497);
    moonbit_incref(_M0L17error__to__stringS1476);
    moonbit_incref(_M0L8filenameS1472);
    moonbit_incref(_M0L14handle__resultS1467);
    #line 571 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    _tmp_3762
    = _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1497, _M0L8filenameS1472, _M0L5indexS1475, _M0L14handle__resultS1467, _M0L17error__to__stringS1476);
    if (_tmp_3762.tag) {
      int32_t const _M0L5_2aokS3271 = _tmp_3762.data.ok;
      _handle__error__result_3763 = _M0L5_2aokS3271;
    } else {
      void* const _M0L6_2aerrS3272 = _tmp_3762.data.err;
      moonbit_decref(_M0L12async__testsS1497);
      moonbit_decref(_M0L17error__to__stringS1476);
      moonbit_decref(_M0L8filenameS1472);
      _M0L11_2atry__errS1491 = _M0L6_2aerrS3272;
      goto join_1490;
    }
    if (_handle__error__result_3763) {
      moonbit_decref(_M0L12async__testsS1497);
      moonbit_decref(_M0L17error__to__stringS1476);
      moonbit_decref(_M0L8filenameS1472);
      _M0L6_2atmpS3264 = 1;
    } else {
      struct moonbit_result_0 _tmp_3764;
      int32_t _handle__error__result_3765;
      moonbit_incref(_M0L12async__testsS1497);
      moonbit_incref(_M0L17error__to__stringS1476);
      moonbit_incref(_M0L8filenameS1472);
      moonbit_incref(_M0L14handle__resultS1467);
      #line 574 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _tmp_3764
      = _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1497, _M0L8filenameS1472, _M0L5indexS1475, _M0L14handle__resultS1467, _M0L17error__to__stringS1476);
      if (_tmp_3764.tag) {
        int32_t const _M0L5_2aokS3269 = _tmp_3764.data.ok;
        _handle__error__result_3765 = _M0L5_2aokS3269;
      } else {
        void* const _M0L6_2aerrS3270 = _tmp_3764.data.err;
        moonbit_decref(_M0L12async__testsS1497);
        moonbit_decref(_M0L17error__to__stringS1476);
        moonbit_decref(_M0L8filenameS1472);
        _M0L11_2atry__errS1491 = _M0L6_2aerrS3270;
        goto join_1490;
      }
      if (_handle__error__result_3765) {
        moonbit_decref(_M0L12async__testsS1497);
        moonbit_decref(_M0L17error__to__stringS1476);
        moonbit_decref(_M0L8filenameS1472);
        _M0L6_2atmpS3264 = 1;
      } else {
        struct moonbit_result_0 _tmp_3766;
        int32_t _handle__error__result_3767;
        moonbit_incref(_M0L12async__testsS1497);
        moonbit_incref(_M0L17error__to__stringS1476);
        moonbit_incref(_M0L8filenameS1472);
        moonbit_incref(_M0L14handle__resultS1467);
        #line 577 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        _tmp_3766
        = _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1497, _M0L8filenameS1472, _M0L5indexS1475, _M0L14handle__resultS1467, _M0L17error__to__stringS1476);
        if (_tmp_3766.tag) {
          int32_t const _M0L5_2aokS3267 = _tmp_3766.data.ok;
          _handle__error__result_3767 = _M0L5_2aokS3267;
        } else {
          void* const _M0L6_2aerrS3268 = _tmp_3766.data.err;
          moonbit_decref(_M0L12async__testsS1497);
          moonbit_decref(_M0L17error__to__stringS1476);
          moonbit_decref(_M0L8filenameS1472);
          _M0L11_2atry__errS1491 = _M0L6_2aerrS3268;
          goto join_1490;
        }
        if (_handle__error__result_3767) {
          moonbit_decref(_M0L12async__testsS1497);
          moonbit_decref(_M0L17error__to__stringS1476);
          moonbit_decref(_M0L8filenameS1472);
          _M0L6_2atmpS3264 = 1;
        } else {
          struct moonbit_result_0 _tmp_3768;
          moonbit_incref(_M0L14handle__resultS1467);
          #line 580 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
          _tmp_3768
          = _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1497, _M0L8filenameS1472, _M0L5indexS1475, _M0L14handle__resultS1467, _M0L17error__to__stringS1476);
          if (_tmp_3768.tag) {
            int32_t const _M0L5_2aokS3265 = _tmp_3768.data.ok;
            _M0L6_2atmpS3264 = _M0L5_2aokS3265;
          } else {
            void* const _M0L6_2aerrS3266 = _tmp_3768.data.err;
            _M0L11_2atry__errS1491 = _M0L6_2aerrS3266;
            goto join_1490;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3264) {
    void* _M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3275 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3275)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3275)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1491
    = _M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3275;
    goto join_1490;
  } else {
    moonbit_decref(_M0L14handle__resultS1467);
  }
  goto joinlet_3759;
  join_1490:;
  _M0L3errS1492 = _M0L11_2atry__errS1491;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1495
  = (struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1492;
  _M0L8_2afieldS3281 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1495->$0;
  _M0L6_2acntS3621
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1495)->rc;
  if (_M0L6_2acntS3621 > 1) {
    int32_t _M0L11_2anew__cntS3622 = _M0L6_2acntS3621 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1495)->rc
    = _M0L11_2anew__cntS3622;
    moonbit_incref(_M0L8_2afieldS3281);
  } else if (_M0L6_2acntS3621 == 1) {
    #line 587 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1495);
  }
  _M0L7_2anameS1496 = _M0L8_2afieldS3281;
  _M0L4nameS1494 = _M0L7_2anameS1496;
  goto join_1493;
  goto joinlet_3769;
  join_1493:;
  #line 588 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN14handle__resultS1467(_M0L14handle__resultS1467, _M0L4nameS1494, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3769:;
  joinlet_3759:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN17error__to__stringS1476(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3263,
  void* _M0L3errS1477
) {
  void* _M0L1eS1479;
  moonbit_string_t _M0L1eS1481;
  #line 557 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3263);
  switch (Moonbit_object_tag(_M0L3errS1477)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1482 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1477;
      moonbit_string_t _M0L8_2afieldS3282 = _M0L10_2aFailureS1482->$0;
      int32_t _M0L6_2acntS3623 =
        Moonbit_object_header(_M0L10_2aFailureS1482)->rc;
      moonbit_string_t _M0L4_2aeS1483;
      if (_M0L6_2acntS3623 > 1) {
        int32_t _M0L11_2anew__cntS3624 = _M0L6_2acntS3623 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1482)->rc
        = _M0L11_2anew__cntS3624;
        moonbit_incref(_M0L8_2afieldS3282);
      } else if (_M0L6_2acntS3623 == 1) {
        #line 558 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1482);
      }
      _M0L4_2aeS1483 = _M0L8_2afieldS3282;
      _M0L1eS1481 = _M0L4_2aeS1483;
      goto join_1480;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1484 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1477;
      moonbit_string_t _M0L8_2afieldS3283 = _M0L15_2aInspectErrorS1484->$0;
      int32_t _M0L6_2acntS3625 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1484)->rc;
      moonbit_string_t _M0L4_2aeS1485;
      if (_M0L6_2acntS3625 > 1) {
        int32_t _M0L11_2anew__cntS3626 = _M0L6_2acntS3625 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1484)->rc
        = _M0L11_2anew__cntS3626;
        moonbit_incref(_M0L8_2afieldS3283);
      } else if (_M0L6_2acntS3625 == 1) {
        #line 558 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1484);
      }
      _M0L4_2aeS1485 = _M0L8_2afieldS3283;
      _M0L1eS1481 = _M0L4_2aeS1485;
      goto join_1480;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1486 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1477;
      moonbit_string_t _M0L8_2afieldS3284 = _M0L16_2aSnapshotErrorS1486->$0;
      int32_t _M0L6_2acntS3627 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1486)->rc;
      moonbit_string_t _M0L4_2aeS1487;
      if (_M0L6_2acntS3627 > 1) {
        int32_t _M0L11_2anew__cntS3628 = _M0L6_2acntS3627 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1486)->rc
        = _M0L11_2anew__cntS3628;
        moonbit_incref(_M0L8_2afieldS3284);
      } else if (_M0L6_2acntS3627 == 1) {
        #line 558 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1486);
      }
      _M0L4_2aeS1487 = _M0L8_2afieldS3284;
      _M0L1eS1481 = _M0L4_2aeS1487;
      goto join_1480;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error115clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1488 =
        (struct _M0DTPC15error5Error115clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1477;
      moonbit_string_t _M0L8_2afieldS3285 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1488->$0;
      int32_t _M0L6_2acntS3629 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1488)->rc;
      moonbit_string_t _M0L4_2aeS1489;
      if (_M0L6_2acntS3629 > 1) {
        int32_t _M0L11_2anew__cntS3630 = _M0L6_2acntS3629 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1488)->rc
        = _M0L11_2anew__cntS3630;
        moonbit_incref(_M0L8_2afieldS3285);
      } else if (_M0L6_2acntS3629 == 1) {
        #line 558 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1488);
      }
      _M0L4_2aeS1489 = _M0L8_2afieldS3285;
      _M0L1eS1481 = _M0L4_2aeS1489;
      goto join_1480;
      break;
    }
    default: {
      _M0L1eS1479 = _M0L3errS1477;
      goto join_1478;
      break;
    }
  }
  join_1480:;
  return _M0L1eS1481;
  join_1478:;
  #line 563 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1479);
}

int32_t _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__executeN14handle__resultS1467(
  struct _M0TWssbEu* _M0L6_2aenvS3249,
  moonbit_string_t _M0L8testnameS1468,
  moonbit_string_t _M0L7messageS1469,
  int32_t _M0L7skippedS1470
) {
  struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467* _M0L14_2acasted__envS3250;
  moonbit_string_t _M0L8_2afieldS3295;
  moonbit_string_t _M0L8filenameS1472;
  int32_t _M0L8_2afieldS3294;
  int32_t _M0L6_2acntS3631;
  int32_t _M0L5indexS1475;
  int32_t _if__result_3772;
  moonbit_string_t _M0L10file__nameS1471;
  moonbit_string_t _M0L10test__nameS1473;
  moonbit_string_t _M0L7messageS1474;
  moonbit_string_t _M0L6_2atmpS3262;
  moonbit_string_t _M0L6_2atmpS3293;
  moonbit_string_t _M0L6_2atmpS3261;
  moonbit_string_t _M0L6_2atmpS3292;
  moonbit_string_t _M0L6_2atmpS3259;
  moonbit_string_t _M0L6_2atmpS3260;
  moonbit_string_t _M0L6_2atmpS3291;
  moonbit_string_t _M0L6_2atmpS3258;
  moonbit_string_t _M0L6_2atmpS3290;
  moonbit_string_t _M0L6_2atmpS3256;
  moonbit_string_t _M0L6_2atmpS3257;
  moonbit_string_t _M0L6_2atmpS3289;
  moonbit_string_t _M0L6_2atmpS3255;
  moonbit_string_t _M0L6_2atmpS3288;
  moonbit_string_t _M0L6_2atmpS3253;
  moonbit_string_t _M0L6_2atmpS3254;
  moonbit_string_t _M0L6_2atmpS3287;
  moonbit_string_t _M0L6_2atmpS3252;
  moonbit_string_t _M0L6_2atmpS3286;
  moonbit_string_t _M0L6_2atmpS3251;
  #line 541 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3250
  = (struct _M0R119_24clawteam_2fclawteam_2finternal_2fconversation_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1467*)_M0L6_2aenvS3249;
  _M0L8_2afieldS3295 = _M0L14_2acasted__envS3250->$1;
  _M0L8filenameS1472 = _M0L8_2afieldS3295;
  _M0L8_2afieldS3294 = _M0L14_2acasted__envS3250->$0;
  _M0L6_2acntS3631 = Moonbit_object_header(_M0L14_2acasted__envS3250)->rc;
  if (_M0L6_2acntS3631 > 1) {
    int32_t _M0L11_2anew__cntS3632 = _M0L6_2acntS3631 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3250)->rc
    = _M0L11_2anew__cntS3632;
    moonbit_incref(_M0L8filenameS1472);
  } else if (_M0L6_2acntS3631 == 1) {
    #line 541 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3250);
  }
  _M0L5indexS1475 = _M0L8_2afieldS3294;
  if (!_M0L7skippedS1470) {
    _if__result_3772 = 1;
  } else {
    _if__result_3772 = 0;
  }
  if (_if__result_3772) {
    
  }
  #line 547 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1471 = _M0MPC16string6String6escape(_M0L8filenameS1472);
  #line 548 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1473 = _M0MPC16string6String6escape(_M0L8testnameS1468);
  #line 549 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1474 = _M0MPC16string6String6escape(_M0L7messageS1469);
  #line 550 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 552 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3262
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1471);
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3293
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3262);
  moonbit_decref(_M0L6_2atmpS3262);
  _M0L6_2atmpS3261 = _M0L6_2atmpS3293;
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3292
  = moonbit_add_string(_M0L6_2atmpS3261, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3261);
  _M0L6_2atmpS3259 = _M0L6_2atmpS3292;
  #line 552 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3260
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1475);
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3291 = moonbit_add_string(_M0L6_2atmpS3259, _M0L6_2atmpS3260);
  moonbit_decref(_M0L6_2atmpS3259);
  moonbit_decref(_M0L6_2atmpS3260);
  _M0L6_2atmpS3258 = _M0L6_2atmpS3291;
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3290
  = moonbit_add_string(_M0L6_2atmpS3258, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3258);
  _M0L6_2atmpS3256 = _M0L6_2atmpS3290;
  #line 552 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3257
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1473);
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3289 = moonbit_add_string(_M0L6_2atmpS3256, _M0L6_2atmpS3257);
  moonbit_decref(_M0L6_2atmpS3256);
  moonbit_decref(_M0L6_2atmpS3257);
  _M0L6_2atmpS3255 = _M0L6_2atmpS3289;
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3288
  = moonbit_add_string(_M0L6_2atmpS3255, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3255);
  _M0L6_2atmpS3253 = _M0L6_2atmpS3288;
  #line 552 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3254
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1474);
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3287 = moonbit_add_string(_M0L6_2atmpS3253, _M0L6_2atmpS3254);
  moonbit_decref(_M0L6_2atmpS3253);
  moonbit_decref(_M0L6_2atmpS3254);
  _M0L6_2atmpS3252 = _M0L6_2atmpS3287;
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3286
  = moonbit_add_string(_M0L6_2atmpS3252, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3252);
  _M0L6_2atmpS3251 = _M0L6_2atmpS3286;
  #line 551 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3251);
  #line 554 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1466,
  moonbit_string_t _M0L8filenameS1463,
  int32_t _M0L5indexS1457,
  struct _M0TWssbEu* _M0L14handle__resultS1453,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1455
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1433;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1462;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1435;
  moonbit_string_t* _M0L5attrsS1436;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1456;
  moonbit_string_t _M0L4nameS1439;
  moonbit_string_t _M0L4nameS1437;
  int32_t _M0L6_2atmpS3248;
  struct _M0TWEOs* _M0L5_2aitS1441;
  struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__* _closure_3781;
  struct _M0TWEOc* _M0L6_2atmpS3239;
  struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__* _closure_3782;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3240;
  struct moonbit_result_0 _result_3783;
  #line 415 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1466);
  moonbit_incref(_M0FP48clawteam8clawteam8internal12conversation48moonbit__test__driver__internal__no__args__tests);
  #line 422 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1462
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal12conversation48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1463);
  if (_M0L7_2abindS1462 == 0) {
    struct moonbit_result_0 _result_3774;
    if (_M0L7_2abindS1462) {
      moonbit_decref(_M0L7_2abindS1462);
    }
    moonbit_decref(_M0L17error__to__stringS1455);
    moonbit_decref(_M0L14handle__resultS1453);
    _result_3774.tag = 1;
    _result_3774.data.ok = 0;
    return _result_3774;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1464 =
      _M0L7_2abindS1462;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1465 =
      _M0L7_2aSomeS1464;
    _M0L10index__mapS1433 = _M0L13_2aindex__mapS1465;
    goto join_1432;
  }
  join_1432:;
  #line 424 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1456
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1433, _M0L5indexS1457);
  if (_M0L7_2abindS1456 == 0) {
    struct moonbit_result_0 _result_3776;
    if (_M0L7_2abindS1456) {
      moonbit_decref(_M0L7_2abindS1456);
    }
    moonbit_decref(_M0L17error__to__stringS1455);
    moonbit_decref(_M0L14handle__resultS1453);
    _result_3776.tag = 1;
    _result_3776.data.ok = 0;
    return _result_3776;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1458 =
      _M0L7_2abindS1456;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1459 = _M0L7_2aSomeS1458;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3299 = _M0L4_2axS1459->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1460 = _M0L8_2afieldS3299;
    moonbit_string_t* _M0L8_2afieldS3298 = _M0L4_2axS1459->$1;
    int32_t _M0L6_2acntS3633 = Moonbit_object_header(_M0L4_2axS1459)->rc;
    moonbit_string_t* _M0L8_2aattrsS1461;
    if (_M0L6_2acntS3633 > 1) {
      int32_t _M0L11_2anew__cntS3634 = _M0L6_2acntS3633 - 1;
      Moonbit_object_header(_M0L4_2axS1459)->rc = _M0L11_2anew__cntS3634;
      moonbit_incref(_M0L8_2afieldS3298);
      moonbit_incref(_M0L4_2afS1460);
    } else if (_M0L6_2acntS3633 == 1) {
      #line 422 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1459);
    }
    _M0L8_2aattrsS1461 = _M0L8_2afieldS3298;
    _M0L1fS1435 = _M0L4_2afS1460;
    _M0L5attrsS1436 = _M0L8_2aattrsS1461;
    goto join_1434;
  }
  join_1434:;
  _M0L6_2atmpS3248 = Moonbit_array_length(_M0L5attrsS1436);
  if (_M0L6_2atmpS3248 >= 1) {
    moonbit_string_t _M0L6_2atmpS3297 = (moonbit_string_t)_M0L5attrsS1436[0];
    moonbit_string_t _M0L7_2anameS1440 = _M0L6_2atmpS3297;
    moonbit_incref(_M0L7_2anameS1440);
    _M0L4nameS1439 = _M0L7_2anameS1440;
    goto join_1438;
  } else {
    _M0L4nameS1437 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3777;
  join_1438:;
  _M0L4nameS1437 = _M0L4nameS1439;
  joinlet_3777:;
  #line 425 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1441 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1436);
  while (1) {
    moonbit_string_t _M0L4attrS1443;
    moonbit_string_t _M0L7_2abindS1450;
    int32_t _M0L6_2atmpS3232;
    int64_t _M0L6_2atmpS3231;
    moonbit_incref(_M0L5_2aitS1441);
    #line 427 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1450 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1441);
    if (_M0L7_2abindS1450 == 0) {
      if (_M0L7_2abindS1450) {
        moonbit_decref(_M0L7_2abindS1450);
      }
      moonbit_decref(_M0L5_2aitS1441);
    } else {
      moonbit_string_t _M0L7_2aSomeS1451 = _M0L7_2abindS1450;
      moonbit_string_t _M0L7_2aattrS1452 = _M0L7_2aSomeS1451;
      _M0L4attrS1443 = _M0L7_2aattrS1452;
      goto join_1442;
    }
    goto joinlet_3779;
    join_1442:;
    _M0L6_2atmpS3232 = Moonbit_array_length(_M0L4attrS1443);
    _M0L6_2atmpS3231 = (int64_t)_M0L6_2atmpS3232;
    moonbit_incref(_M0L4attrS1443);
    #line 428 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1443, 5, 0, _M0L6_2atmpS3231)
    ) {
      int32_t _M0L6_2atmpS3238 = _M0L4attrS1443[0];
      int32_t _M0L4_2axS1444 = _M0L6_2atmpS3238;
      if (_M0L4_2axS1444 == 112) {
        int32_t _M0L6_2atmpS3237 = _M0L4attrS1443[1];
        int32_t _M0L4_2axS1445 = _M0L6_2atmpS3237;
        if (_M0L4_2axS1445 == 97) {
          int32_t _M0L6_2atmpS3236 = _M0L4attrS1443[2];
          int32_t _M0L4_2axS1446 = _M0L6_2atmpS3236;
          if (_M0L4_2axS1446 == 110) {
            int32_t _M0L6_2atmpS3235 = _M0L4attrS1443[3];
            int32_t _M0L4_2axS1447 = _M0L6_2atmpS3235;
            if (_M0L4_2axS1447 == 105) {
              int32_t _M0L6_2atmpS3296 = _M0L4attrS1443[4];
              int32_t _M0L6_2atmpS3234;
              int32_t _M0L4_2axS1448;
              moonbit_decref(_M0L4attrS1443);
              _M0L6_2atmpS3234 = _M0L6_2atmpS3296;
              _M0L4_2axS1448 = _M0L6_2atmpS3234;
              if (_M0L4_2axS1448 == 99) {
                void* _M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3233;
                struct moonbit_result_0 _result_3780;
                moonbit_decref(_M0L17error__to__stringS1455);
                moonbit_decref(_M0L14handle__resultS1453);
                moonbit_decref(_M0L5_2aitS1441);
                moonbit_decref(_M0L1fS1435);
                _M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3233
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3233)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3233)->$0
                = _M0L4nameS1437;
                _result_3780.tag = 0;
                _result_3780.data.err
                = _M0L117clawteam_2fclawteam_2finternal_2fconversation_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3233;
                return _result_3780;
              }
            } else {
              moonbit_decref(_M0L4attrS1443);
            }
          } else {
            moonbit_decref(_M0L4attrS1443);
          }
        } else {
          moonbit_decref(_M0L4attrS1443);
        }
      } else {
        moonbit_decref(_M0L4attrS1443);
      }
    } else {
      moonbit_decref(_M0L4attrS1443);
    }
    continue;
    joinlet_3779:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1453);
  moonbit_incref(_M0L4nameS1437);
  _closure_3781
  = (struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__*)moonbit_malloc(sizeof(struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__));
  Moonbit_object_header(_closure_3781)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__, $0) >> 2, 2, 0);
  _closure_3781->code
  = &_M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testC3245l435;
  _closure_3781->$0 = _M0L14handle__resultS1453;
  _closure_3781->$1 = _M0L4nameS1437;
  _M0L6_2atmpS3239 = (struct _M0TWEOc*)_closure_3781;
  _closure_3782
  = (struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__*)moonbit_malloc(sizeof(struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__));
  Moonbit_object_header(_closure_3782)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__, $0) >> 2, 3, 0);
  _closure_3782->code
  = &_M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testC3241l436;
  _closure_3782->$0 = _M0L17error__to__stringS1455;
  _closure_3782->$1 = _M0L14handle__resultS1453;
  _closure_3782->$2 = _M0L4nameS1437;
  _M0L6_2atmpS3240 = (struct _M0TWRPC15error5ErrorEu*)_closure_3782;
  #line 433 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal12conversation45moonbit__test__driver__internal__catch__error(_M0L1fS1435, _M0L6_2atmpS3239, _M0L6_2atmpS3240);
  _result_3783.tag = 1;
  _result_3783.data.ok = 1;
  return _result_3783;
}

int32_t _M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testC3245l435(
  struct _M0TWEOc* _M0L6_2aenvS3246
) {
  struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__* _M0L14_2acasted__envS3247;
  moonbit_string_t _M0L8_2afieldS3301;
  moonbit_string_t _M0L4nameS1437;
  struct _M0TWssbEu* _M0L8_2afieldS3300;
  int32_t _M0L6_2acntS3635;
  struct _M0TWssbEu* _M0L14handle__resultS1453;
  #line 435 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3247
  = (struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3245__l435__*)_M0L6_2aenvS3246;
  _M0L8_2afieldS3301 = _M0L14_2acasted__envS3247->$1;
  _M0L4nameS1437 = _M0L8_2afieldS3301;
  _M0L8_2afieldS3300 = _M0L14_2acasted__envS3247->$0;
  _M0L6_2acntS3635 = Moonbit_object_header(_M0L14_2acasted__envS3247)->rc;
  if (_M0L6_2acntS3635 > 1) {
    int32_t _M0L11_2anew__cntS3636 = _M0L6_2acntS3635 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3247)->rc
    = _M0L11_2anew__cntS3636;
    moonbit_incref(_M0L4nameS1437);
    moonbit_incref(_M0L8_2afieldS3300);
  } else if (_M0L6_2acntS3635 == 1) {
    #line 435 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3247);
  }
  _M0L14handle__resultS1453 = _M0L8_2afieldS3300;
  #line 435 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1453->code(_M0L14handle__resultS1453, _M0L4nameS1437, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal12conversation41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testC3241l436(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3242,
  void* _M0L3errS1454
) {
  struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__* _M0L14_2acasted__envS3243;
  moonbit_string_t _M0L8_2afieldS3304;
  moonbit_string_t _M0L4nameS1437;
  struct _M0TWssbEu* _M0L8_2afieldS3303;
  struct _M0TWssbEu* _M0L14handle__resultS1453;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3302;
  int32_t _M0L6_2acntS3637;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1455;
  moonbit_string_t _M0L6_2atmpS3244;
  #line 436 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3243
  = (struct _M0R207_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fconversation_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3241__l436__*)_M0L6_2aenvS3242;
  _M0L8_2afieldS3304 = _M0L14_2acasted__envS3243->$2;
  _M0L4nameS1437 = _M0L8_2afieldS3304;
  _M0L8_2afieldS3303 = _M0L14_2acasted__envS3243->$1;
  _M0L14handle__resultS1453 = _M0L8_2afieldS3303;
  _M0L8_2afieldS3302 = _M0L14_2acasted__envS3243->$0;
  _M0L6_2acntS3637 = Moonbit_object_header(_M0L14_2acasted__envS3243)->rc;
  if (_M0L6_2acntS3637 > 1) {
    int32_t _M0L11_2anew__cntS3638 = _M0L6_2acntS3637 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3243)->rc
    = _M0L11_2anew__cntS3638;
    moonbit_incref(_M0L4nameS1437);
    moonbit_incref(_M0L14handle__resultS1453);
    moonbit_incref(_M0L8_2afieldS3302);
  } else if (_M0L6_2acntS3637 == 1) {
    #line 436 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3243);
  }
  _M0L17error__to__stringS1455 = _M0L8_2afieldS3302;
  #line 436 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3244
  = _M0L17error__to__stringS1455->code(_M0L17error__to__stringS1455, _M0L3errS1454);
  #line 436 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1453->code(_M0L14handle__resultS1453, _M0L4nameS1437, _M0L6_2atmpS3244, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal12conversation45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1423,
  struct _M0TWEOc* _M0L6on__okS1424,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1421
) {
  void* _M0L11_2atry__errS1419;
  struct moonbit_result_0 _tmp_3785;
  void* _M0L3errS1420;
  #line 375 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _tmp_3785 = _M0L1fS1423->code(_M0L1fS1423);
  if (_tmp_3785.tag) {
    int32_t const _M0L5_2aokS3229 = _tmp_3785.data.ok;
    moonbit_decref(_M0L7on__errS1421);
  } else {
    void* const _M0L6_2aerrS3230 = _tmp_3785.data.err;
    moonbit_decref(_M0L6on__okS1424);
    _M0L11_2atry__errS1419 = _M0L6_2aerrS3230;
    goto join_1418;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1424->code(_M0L6on__okS1424);
  goto joinlet_3784;
  join_1418:;
  _M0L3errS1420 = _M0L11_2atry__errS1419;
  #line 383 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1421->code(_M0L7on__errS1421, _M0L3errS1420);
  joinlet_3784:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1378;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1391;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1396;
  struct _M0TUsiE** _M0L6_2atmpS3228;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1403;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1404;
  moonbit_string_t _M0L6_2atmpS3227;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1405;
  int32_t _M0L7_2abindS1406;
  int32_t _M0L2__S1407;
  #line 193 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1378 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1391
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1396 = 0;
  _M0L6_2atmpS3228 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1403
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1403)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1403->$0 = _M0L6_2atmpS3228;
  _M0L16file__and__indexS1403->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1404
  = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1391(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1391);
  #line 284 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3227 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1404, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1405
  = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1396(_M0L51moonbit__test__driver__internal__split__mbt__stringS1396, _M0L6_2atmpS3227, 47);
  _M0L7_2abindS1406 = _M0L10test__argsS1405->$1;
  _M0L2__S1407 = 0;
  while (1) {
    if (_M0L2__S1407 < _M0L7_2abindS1406) {
      moonbit_string_t* _M0L8_2afieldS3306 = _M0L10test__argsS1405->$0;
      moonbit_string_t* _M0L3bufS3226 = _M0L8_2afieldS3306;
      moonbit_string_t _M0L6_2atmpS3305 =
        (moonbit_string_t)_M0L3bufS3226[_M0L2__S1407];
      moonbit_string_t _M0L3argS1408 = _M0L6_2atmpS3305;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1409;
      moonbit_string_t _M0L4fileS1410;
      moonbit_string_t _M0L5rangeS1411;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1412;
      moonbit_string_t _M0L6_2atmpS3224;
      int32_t _M0L5startS1413;
      moonbit_string_t _M0L6_2atmpS3223;
      int32_t _M0L3endS1414;
      int32_t _M0L1iS1415;
      int32_t _M0L6_2atmpS3225;
      moonbit_incref(_M0L3argS1408);
      #line 288 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1409
      = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1396(_M0L51moonbit__test__driver__internal__split__mbt__stringS1396, _M0L3argS1408, 58);
      moonbit_incref(_M0L16file__and__rangeS1409);
      #line 289 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1410
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1409, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1411
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1409, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1412
      = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1396(_M0L51moonbit__test__driver__internal__split__mbt__stringS1396, _M0L5rangeS1411, 45);
      moonbit_incref(_M0L15start__and__endS1412);
      #line 294 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3224
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1412, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1413
      = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1378(_M0L45moonbit__test__driver__internal__parse__int__S1378, _M0L6_2atmpS3224);
      #line 295 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3223
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1412, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1414
      = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1378(_M0L45moonbit__test__driver__internal__parse__int__S1378, _M0L6_2atmpS3223);
      _M0L1iS1415 = _M0L5startS1413;
      while (1) {
        if (_M0L1iS1415 < _M0L3endS1414) {
          struct _M0TUsiE* _M0L8_2atupleS3221;
          int32_t _M0L6_2atmpS3222;
          moonbit_incref(_M0L4fileS1410);
          _M0L8_2atupleS3221
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3221)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3221->$0 = _M0L4fileS1410;
          _M0L8_2atupleS3221->$1 = _M0L1iS1415;
          moonbit_incref(_M0L16file__and__indexS1403);
          #line 297 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1403, _M0L8_2atupleS3221);
          _M0L6_2atmpS3222 = _M0L1iS1415 + 1;
          _M0L1iS1415 = _M0L6_2atmpS3222;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1410);
        }
        break;
      }
      _M0L6_2atmpS3225 = _M0L2__S1407 + 1;
      _M0L2__S1407 = _M0L6_2atmpS3225;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1405);
    }
    break;
  }
  return _M0L16file__and__indexS1403;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1396(
  int32_t _M0L6_2aenvS3202,
  moonbit_string_t _M0L1sS1397,
  int32_t _M0L3sepS1398
) {
  moonbit_string_t* _M0L6_2atmpS3220;
  struct _M0TPB5ArrayGsE* _M0L3resS1399;
  struct _M0TPC13ref3RefGiE* _M0L1iS1400;
  struct _M0TPC13ref3RefGiE* _M0L5startS1401;
  int32_t _M0L3valS3215;
  int32_t _M0L6_2atmpS3216;
  #line 261 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3220 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1399
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1399)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1399->$0 = _M0L6_2atmpS3220;
  _M0L3resS1399->$1 = 0;
  _M0L1iS1400
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1400)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1400->$0 = 0;
  _M0L5startS1401
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1401)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1401->$0 = 0;
  while (1) {
    int32_t _M0L3valS3203 = _M0L1iS1400->$0;
    int32_t _M0L6_2atmpS3204 = Moonbit_array_length(_M0L1sS1397);
    if (_M0L3valS3203 < _M0L6_2atmpS3204) {
      int32_t _M0L3valS3207 = _M0L1iS1400->$0;
      int32_t _M0L6_2atmpS3206;
      int32_t _M0L6_2atmpS3205;
      int32_t _M0L3valS3214;
      int32_t _M0L6_2atmpS3213;
      if (
        _M0L3valS3207 < 0
        || _M0L3valS3207 >= Moonbit_array_length(_M0L1sS1397)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3206 = _M0L1sS1397[_M0L3valS3207];
      _M0L6_2atmpS3205 = _M0L6_2atmpS3206;
      if (_M0L6_2atmpS3205 == _M0L3sepS1398) {
        int32_t _M0L3valS3209 = _M0L5startS1401->$0;
        int32_t _M0L3valS3210 = _M0L1iS1400->$0;
        moonbit_string_t _M0L6_2atmpS3208;
        int32_t _M0L3valS3212;
        int32_t _M0L6_2atmpS3211;
        moonbit_incref(_M0L1sS1397);
        #line 270 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3208
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1397, _M0L3valS3209, _M0L3valS3210);
        moonbit_incref(_M0L3resS1399);
        #line 270 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1399, _M0L6_2atmpS3208);
        _M0L3valS3212 = _M0L1iS1400->$0;
        _M0L6_2atmpS3211 = _M0L3valS3212 + 1;
        _M0L5startS1401->$0 = _M0L6_2atmpS3211;
      }
      _M0L3valS3214 = _M0L1iS1400->$0;
      _M0L6_2atmpS3213 = _M0L3valS3214 + 1;
      _M0L1iS1400->$0 = _M0L6_2atmpS3213;
      continue;
    } else {
      moonbit_decref(_M0L1iS1400);
    }
    break;
  }
  _M0L3valS3215 = _M0L5startS1401->$0;
  _M0L6_2atmpS3216 = Moonbit_array_length(_M0L1sS1397);
  if (_M0L3valS3215 < _M0L6_2atmpS3216) {
    int32_t _M0L8_2afieldS3307 = _M0L5startS1401->$0;
    int32_t _M0L3valS3218;
    int32_t _M0L6_2atmpS3219;
    moonbit_string_t _M0L6_2atmpS3217;
    moonbit_decref(_M0L5startS1401);
    _M0L3valS3218 = _M0L8_2afieldS3307;
    _M0L6_2atmpS3219 = Moonbit_array_length(_M0L1sS1397);
    #line 276 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3217
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1397, _M0L3valS3218, _M0L6_2atmpS3219);
    moonbit_incref(_M0L3resS1399);
    #line 276 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1399, _M0L6_2atmpS3217);
  } else {
    moonbit_decref(_M0L5startS1401);
    moonbit_decref(_M0L1sS1397);
  }
  return _M0L3resS1399;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1391(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384
) {
  moonbit_bytes_t* _M0L3tmpS1392;
  int32_t _M0L6_2atmpS3201;
  struct _M0TPB5ArrayGsE* _M0L3resS1393;
  int32_t _M0L1iS1394;
  #line 250 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1392
  = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3201 = Moonbit_array_length(_M0L3tmpS1392);
  #line 254 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1393 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3201);
  _M0L1iS1394 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3197 = Moonbit_array_length(_M0L3tmpS1392);
    if (_M0L1iS1394 < _M0L6_2atmpS3197) {
      moonbit_bytes_t _M0L6_2atmpS3308;
      moonbit_bytes_t _M0L6_2atmpS3199;
      moonbit_string_t _M0L6_2atmpS3198;
      int32_t _M0L6_2atmpS3200;
      if (
        _M0L1iS1394 < 0 || _M0L1iS1394 >= Moonbit_array_length(_M0L3tmpS1392)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3308 = (moonbit_bytes_t)_M0L3tmpS1392[_M0L1iS1394];
      _M0L6_2atmpS3199 = _M0L6_2atmpS3308;
      moonbit_incref(_M0L6_2atmpS3199);
      #line 256 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3198
      = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384, _M0L6_2atmpS3199);
      moonbit_incref(_M0L3resS1393);
      #line 256 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1393, _M0L6_2atmpS3198);
      _M0L6_2atmpS3200 = _M0L1iS1394 + 1;
      _M0L1iS1394 = _M0L6_2atmpS3200;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1392);
    }
    break;
  }
  return _M0L3resS1393;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1384(
  int32_t _M0L6_2aenvS3111,
  moonbit_bytes_t _M0L5bytesS1385
) {
  struct _M0TPB13StringBuilder* _M0L3resS1386;
  int32_t _M0L3lenS1387;
  struct _M0TPC13ref3RefGiE* _M0L1iS1388;
  #line 206 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1386 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1387 = Moonbit_array_length(_M0L5bytesS1385);
  _M0L1iS1388
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1388)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1388->$0 = 0;
  while (1) {
    int32_t _M0L3valS3112 = _M0L1iS1388->$0;
    if (_M0L3valS3112 < _M0L3lenS1387) {
      int32_t _M0L3valS3196 = _M0L1iS1388->$0;
      int32_t _M0L6_2atmpS3195;
      int32_t _M0L6_2atmpS3194;
      struct _M0TPC13ref3RefGiE* _M0L1cS1389;
      int32_t _M0L3valS3113;
      if (
        _M0L3valS3196 < 0
        || _M0L3valS3196 >= Moonbit_array_length(_M0L5bytesS1385)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3195 = _M0L5bytesS1385[_M0L3valS3196];
      _M0L6_2atmpS3194 = (int32_t)_M0L6_2atmpS3195;
      _M0L1cS1389
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1389)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1389->$0 = _M0L6_2atmpS3194;
      _M0L3valS3113 = _M0L1cS1389->$0;
      if (_M0L3valS3113 < 128) {
        int32_t _M0L8_2afieldS3309 = _M0L1cS1389->$0;
        int32_t _M0L3valS3115;
        int32_t _M0L6_2atmpS3114;
        int32_t _M0L3valS3117;
        int32_t _M0L6_2atmpS3116;
        moonbit_decref(_M0L1cS1389);
        _M0L3valS3115 = _M0L8_2afieldS3309;
        _M0L6_2atmpS3114 = _M0L3valS3115;
        moonbit_incref(_M0L3resS1386);
        #line 215 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1386, _M0L6_2atmpS3114);
        _M0L3valS3117 = _M0L1iS1388->$0;
        _M0L6_2atmpS3116 = _M0L3valS3117 + 1;
        _M0L1iS1388->$0 = _M0L6_2atmpS3116;
      } else {
        int32_t _M0L3valS3118 = _M0L1cS1389->$0;
        if (_M0L3valS3118 < 224) {
          int32_t _M0L3valS3120 = _M0L1iS1388->$0;
          int32_t _M0L6_2atmpS3119 = _M0L3valS3120 + 1;
          int32_t _M0L3valS3129;
          int32_t _M0L6_2atmpS3128;
          int32_t _M0L6_2atmpS3122;
          int32_t _M0L3valS3127;
          int32_t _M0L6_2atmpS3126;
          int32_t _M0L6_2atmpS3125;
          int32_t _M0L6_2atmpS3124;
          int32_t _M0L6_2atmpS3123;
          int32_t _M0L6_2atmpS3121;
          int32_t _M0L8_2afieldS3310;
          int32_t _M0L3valS3131;
          int32_t _M0L6_2atmpS3130;
          int32_t _M0L3valS3133;
          int32_t _M0L6_2atmpS3132;
          if (_M0L6_2atmpS3119 >= _M0L3lenS1387) {
            moonbit_decref(_M0L1cS1389);
            moonbit_decref(_M0L1iS1388);
            moonbit_decref(_M0L5bytesS1385);
            break;
          }
          _M0L3valS3129 = _M0L1cS1389->$0;
          _M0L6_2atmpS3128 = _M0L3valS3129 & 31;
          _M0L6_2atmpS3122 = _M0L6_2atmpS3128 << 6;
          _M0L3valS3127 = _M0L1iS1388->$0;
          _M0L6_2atmpS3126 = _M0L3valS3127 + 1;
          if (
            _M0L6_2atmpS3126 < 0
            || _M0L6_2atmpS3126 >= Moonbit_array_length(_M0L5bytesS1385)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3125 = _M0L5bytesS1385[_M0L6_2atmpS3126];
          _M0L6_2atmpS3124 = (int32_t)_M0L6_2atmpS3125;
          _M0L6_2atmpS3123 = _M0L6_2atmpS3124 & 63;
          _M0L6_2atmpS3121 = _M0L6_2atmpS3122 | _M0L6_2atmpS3123;
          _M0L1cS1389->$0 = _M0L6_2atmpS3121;
          _M0L8_2afieldS3310 = _M0L1cS1389->$0;
          moonbit_decref(_M0L1cS1389);
          _M0L3valS3131 = _M0L8_2afieldS3310;
          _M0L6_2atmpS3130 = _M0L3valS3131;
          moonbit_incref(_M0L3resS1386);
          #line 222 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1386, _M0L6_2atmpS3130);
          _M0L3valS3133 = _M0L1iS1388->$0;
          _M0L6_2atmpS3132 = _M0L3valS3133 + 2;
          _M0L1iS1388->$0 = _M0L6_2atmpS3132;
        } else {
          int32_t _M0L3valS3134 = _M0L1cS1389->$0;
          if (_M0L3valS3134 < 240) {
            int32_t _M0L3valS3136 = _M0L1iS1388->$0;
            int32_t _M0L6_2atmpS3135 = _M0L3valS3136 + 2;
            int32_t _M0L3valS3152;
            int32_t _M0L6_2atmpS3151;
            int32_t _M0L6_2atmpS3144;
            int32_t _M0L3valS3150;
            int32_t _M0L6_2atmpS3149;
            int32_t _M0L6_2atmpS3148;
            int32_t _M0L6_2atmpS3147;
            int32_t _M0L6_2atmpS3146;
            int32_t _M0L6_2atmpS3145;
            int32_t _M0L6_2atmpS3138;
            int32_t _M0L3valS3143;
            int32_t _M0L6_2atmpS3142;
            int32_t _M0L6_2atmpS3141;
            int32_t _M0L6_2atmpS3140;
            int32_t _M0L6_2atmpS3139;
            int32_t _M0L6_2atmpS3137;
            int32_t _M0L8_2afieldS3311;
            int32_t _M0L3valS3154;
            int32_t _M0L6_2atmpS3153;
            int32_t _M0L3valS3156;
            int32_t _M0L6_2atmpS3155;
            if (_M0L6_2atmpS3135 >= _M0L3lenS1387) {
              moonbit_decref(_M0L1cS1389);
              moonbit_decref(_M0L1iS1388);
              moonbit_decref(_M0L5bytesS1385);
              break;
            }
            _M0L3valS3152 = _M0L1cS1389->$0;
            _M0L6_2atmpS3151 = _M0L3valS3152 & 15;
            _M0L6_2atmpS3144 = _M0L6_2atmpS3151 << 12;
            _M0L3valS3150 = _M0L1iS1388->$0;
            _M0L6_2atmpS3149 = _M0L3valS3150 + 1;
            if (
              _M0L6_2atmpS3149 < 0
              || _M0L6_2atmpS3149 >= Moonbit_array_length(_M0L5bytesS1385)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3148 = _M0L5bytesS1385[_M0L6_2atmpS3149];
            _M0L6_2atmpS3147 = (int32_t)_M0L6_2atmpS3148;
            _M0L6_2atmpS3146 = _M0L6_2atmpS3147 & 63;
            _M0L6_2atmpS3145 = _M0L6_2atmpS3146 << 6;
            _M0L6_2atmpS3138 = _M0L6_2atmpS3144 | _M0L6_2atmpS3145;
            _M0L3valS3143 = _M0L1iS1388->$0;
            _M0L6_2atmpS3142 = _M0L3valS3143 + 2;
            if (
              _M0L6_2atmpS3142 < 0
              || _M0L6_2atmpS3142 >= Moonbit_array_length(_M0L5bytesS1385)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3141 = _M0L5bytesS1385[_M0L6_2atmpS3142];
            _M0L6_2atmpS3140 = (int32_t)_M0L6_2atmpS3141;
            _M0L6_2atmpS3139 = _M0L6_2atmpS3140 & 63;
            _M0L6_2atmpS3137 = _M0L6_2atmpS3138 | _M0L6_2atmpS3139;
            _M0L1cS1389->$0 = _M0L6_2atmpS3137;
            _M0L8_2afieldS3311 = _M0L1cS1389->$0;
            moonbit_decref(_M0L1cS1389);
            _M0L3valS3154 = _M0L8_2afieldS3311;
            _M0L6_2atmpS3153 = _M0L3valS3154;
            moonbit_incref(_M0L3resS1386);
            #line 231 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1386, _M0L6_2atmpS3153);
            _M0L3valS3156 = _M0L1iS1388->$0;
            _M0L6_2atmpS3155 = _M0L3valS3156 + 3;
            _M0L1iS1388->$0 = _M0L6_2atmpS3155;
          } else {
            int32_t _M0L3valS3158 = _M0L1iS1388->$0;
            int32_t _M0L6_2atmpS3157 = _M0L3valS3158 + 3;
            int32_t _M0L3valS3181;
            int32_t _M0L6_2atmpS3180;
            int32_t _M0L6_2atmpS3173;
            int32_t _M0L3valS3179;
            int32_t _M0L6_2atmpS3178;
            int32_t _M0L6_2atmpS3177;
            int32_t _M0L6_2atmpS3176;
            int32_t _M0L6_2atmpS3175;
            int32_t _M0L6_2atmpS3174;
            int32_t _M0L6_2atmpS3166;
            int32_t _M0L3valS3172;
            int32_t _M0L6_2atmpS3171;
            int32_t _M0L6_2atmpS3170;
            int32_t _M0L6_2atmpS3169;
            int32_t _M0L6_2atmpS3168;
            int32_t _M0L6_2atmpS3167;
            int32_t _M0L6_2atmpS3160;
            int32_t _M0L3valS3165;
            int32_t _M0L6_2atmpS3164;
            int32_t _M0L6_2atmpS3163;
            int32_t _M0L6_2atmpS3162;
            int32_t _M0L6_2atmpS3161;
            int32_t _M0L6_2atmpS3159;
            int32_t _M0L3valS3183;
            int32_t _M0L6_2atmpS3182;
            int32_t _M0L3valS3187;
            int32_t _M0L6_2atmpS3186;
            int32_t _M0L6_2atmpS3185;
            int32_t _M0L6_2atmpS3184;
            int32_t _M0L8_2afieldS3312;
            int32_t _M0L3valS3191;
            int32_t _M0L6_2atmpS3190;
            int32_t _M0L6_2atmpS3189;
            int32_t _M0L6_2atmpS3188;
            int32_t _M0L3valS3193;
            int32_t _M0L6_2atmpS3192;
            if (_M0L6_2atmpS3157 >= _M0L3lenS1387) {
              moonbit_decref(_M0L1cS1389);
              moonbit_decref(_M0L1iS1388);
              moonbit_decref(_M0L5bytesS1385);
              break;
            }
            _M0L3valS3181 = _M0L1cS1389->$0;
            _M0L6_2atmpS3180 = _M0L3valS3181 & 7;
            _M0L6_2atmpS3173 = _M0L6_2atmpS3180 << 18;
            _M0L3valS3179 = _M0L1iS1388->$0;
            _M0L6_2atmpS3178 = _M0L3valS3179 + 1;
            if (
              _M0L6_2atmpS3178 < 0
              || _M0L6_2atmpS3178 >= Moonbit_array_length(_M0L5bytesS1385)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3177 = _M0L5bytesS1385[_M0L6_2atmpS3178];
            _M0L6_2atmpS3176 = (int32_t)_M0L6_2atmpS3177;
            _M0L6_2atmpS3175 = _M0L6_2atmpS3176 & 63;
            _M0L6_2atmpS3174 = _M0L6_2atmpS3175 << 12;
            _M0L6_2atmpS3166 = _M0L6_2atmpS3173 | _M0L6_2atmpS3174;
            _M0L3valS3172 = _M0L1iS1388->$0;
            _M0L6_2atmpS3171 = _M0L3valS3172 + 2;
            if (
              _M0L6_2atmpS3171 < 0
              || _M0L6_2atmpS3171 >= Moonbit_array_length(_M0L5bytesS1385)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3170 = _M0L5bytesS1385[_M0L6_2atmpS3171];
            _M0L6_2atmpS3169 = (int32_t)_M0L6_2atmpS3170;
            _M0L6_2atmpS3168 = _M0L6_2atmpS3169 & 63;
            _M0L6_2atmpS3167 = _M0L6_2atmpS3168 << 6;
            _M0L6_2atmpS3160 = _M0L6_2atmpS3166 | _M0L6_2atmpS3167;
            _M0L3valS3165 = _M0L1iS1388->$0;
            _M0L6_2atmpS3164 = _M0L3valS3165 + 3;
            if (
              _M0L6_2atmpS3164 < 0
              || _M0L6_2atmpS3164 >= Moonbit_array_length(_M0L5bytesS1385)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3163 = _M0L5bytesS1385[_M0L6_2atmpS3164];
            _M0L6_2atmpS3162 = (int32_t)_M0L6_2atmpS3163;
            _M0L6_2atmpS3161 = _M0L6_2atmpS3162 & 63;
            _M0L6_2atmpS3159 = _M0L6_2atmpS3160 | _M0L6_2atmpS3161;
            _M0L1cS1389->$0 = _M0L6_2atmpS3159;
            _M0L3valS3183 = _M0L1cS1389->$0;
            _M0L6_2atmpS3182 = _M0L3valS3183 - 65536;
            _M0L1cS1389->$0 = _M0L6_2atmpS3182;
            _M0L3valS3187 = _M0L1cS1389->$0;
            _M0L6_2atmpS3186 = _M0L3valS3187 >> 10;
            _M0L6_2atmpS3185 = _M0L6_2atmpS3186 + 55296;
            _M0L6_2atmpS3184 = _M0L6_2atmpS3185;
            moonbit_incref(_M0L3resS1386);
            #line 242 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1386, _M0L6_2atmpS3184);
            _M0L8_2afieldS3312 = _M0L1cS1389->$0;
            moonbit_decref(_M0L1cS1389);
            _M0L3valS3191 = _M0L8_2afieldS3312;
            _M0L6_2atmpS3190 = _M0L3valS3191 & 1023;
            _M0L6_2atmpS3189 = _M0L6_2atmpS3190 + 56320;
            _M0L6_2atmpS3188 = _M0L6_2atmpS3189;
            moonbit_incref(_M0L3resS1386);
            #line 243 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1386, _M0L6_2atmpS3188);
            _M0L3valS3193 = _M0L1iS1388->$0;
            _M0L6_2atmpS3192 = _M0L3valS3193 + 4;
            _M0L1iS1388->$0 = _M0L6_2atmpS3192;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1388);
      moonbit_decref(_M0L5bytesS1385);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1386);
}

int32_t _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1378(
  int32_t _M0L6_2aenvS3104,
  moonbit_string_t _M0L1sS1379
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1380;
  int32_t _M0L3lenS1381;
  int32_t _M0L1iS1382;
  int32_t _M0L8_2afieldS3313;
  #line 197 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1380
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1380)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1380->$0 = 0;
  _M0L3lenS1381 = Moonbit_array_length(_M0L1sS1379);
  _M0L1iS1382 = 0;
  while (1) {
    if (_M0L1iS1382 < _M0L3lenS1381) {
      int32_t _M0L3valS3109 = _M0L3resS1380->$0;
      int32_t _M0L6_2atmpS3106 = _M0L3valS3109 * 10;
      int32_t _M0L6_2atmpS3108;
      int32_t _M0L6_2atmpS3107;
      int32_t _M0L6_2atmpS3105;
      int32_t _M0L6_2atmpS3110;
      if (
        _M0L1iS1382 < 0 || _M0L1iS1382 >= Moonbit_array_length(_M0L1sS1379)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3108 = _M0L1sS1379[_M0L1iS1382];
      _M0L6_2atmpS3107 = _M0L6_2atmpS3108 - 48;
      _M0L6_2atmpS3105 = _M0L6_2atmpS3106 + _M0L6_2atmpS3107;
      _M0L3resS1380->$0 = _M0L6_2atmpS3105;
      _M0L6_2atmpS3110 = _M0L1iS1382 + 1;
      _M0L1iS1382 = _M0L6_2atmpS3110;
      continue;
    } else {
      moonbit_decref(_M0L1sS1379);
    }
    break;
  }
  _M0L8_2afieldS3313 = _M0L3resS1380->$0;
  moonbit_decref(_M0L3resS1380);
  return _M0L8_2afieldS3313;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1358,
  moonbit_string_t _M0L12_2adiscard__S1359,
  int32_t _M0L12_2adiscard__S1360,
  struct _M0TWssbEu* _M0L12_2adiscard__S1361,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1362
) {
  struct moonbit_result_0 _result_3792;
  #line 34 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1362);
  moonbit_decref(_M0L12_2adiscard__S1361);
  moonbit_decref(_M0L12_2adiscard__S1359);
  moonbit_decref(_M0L12_2adiscard__S1358);
  _result_3792.tag = 1;
  _result_3792.data.ok = 0;
  return _result_3792;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1363,
  moonbit_string_t _M0L12_2adiscard__S1364,
  int32_t _M0L12_2adiscard__S1365,
  struct _M0TWssbEu* _M0L12_2adiscard__S1366,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1367
) {
  struct moonbit_result_0 _result_3793;
  #line 34 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1367);
  moonbit_decref(_M0L12_2adiscard__S1366);
  moonbit_decref(_M0L12_2adiscard__S1364);
  moonbit_decref(_M0L12_2adiscard__S1363);
  _result_3793.tag = 1;
  _result_3793.data.ok = 0;
  return _result_3793;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1368,
  moonbit_string_t _M0L12_2adiscard__S1369,
  int32_t _M0L12_2adiscard__S1370,
  struct _M0TWssbEu* _M0L12_2adiscard__S1371,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1372
) {
  struct moonbit_result_0 _result_3794;
  #line 34 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1372);
  moonbit_decref(_M0L12_2adiscard__S1371);
  moonbit_decref(_M0L12_2adiscard__S1369);
  moonbit_decref(_M0L12_2adiscard__S1368);
  _result_3794.tag = 1;
  _result_3794.data.ok = 0;
  return _result_3794;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal12conversation21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal12conversation50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1373,
  moonbit_string_t _M0L12_2adiscard__S1374,
  int32_t _M0L12_2adiscard__S1375,
  struct _M0TWssbEu* _M0L12_2adiscard__S1376,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1377
) {
  struct moonbit_result_0 _result_3795;
  #line 34 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1377);
  moonbit_decref(_M0L12_2adiscard__S1376);
  moonbit_decref(_M0L12_2adiscard__S1374);
  moonbit_decref(_M0L12_2adiscard__S1373);
  _result_3795.tag = 1;
  _result_3795.data.ok = 0;
  return _result_3795;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal12conversation28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal12conversation34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1357
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1357);
  return 0;
}

void* _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal12conversation7Message* _M0L4selfS1351
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1350;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3103;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3102;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS1349;
  int64_t _M0L9timestampS3096;
  void* _M0L6_2atmpS3095;
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L5usageS1353;
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L8_2afieldS3315;
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L7_2abindS1354;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3098;
  void* _M0L6_2atmpS3097;
  void* _M0L8_2afieldS3314;
  int32_t _M0L6_2acntS3639;
  void* _M0L7messageS3101;
  void* _M0L6_2atmpS3100;
  void* _M0L6_2atmpS3099;
  #line 66 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L7_2abindS1350 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3103 = _M0L7_2abindS1350;
  _M0L6_2atmpS3102
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3103
  };
  #line 67 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6objectS1349 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3102);
  _M0L9timestampS3096 = _M0L4selfS1351->$0;
  #line 68 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3095
  = _M0IP38clawteam8clawteam5clock9TimestampPB6ToJson8to__json(_M0L9timestampS3096);
  moonbit_incref(_M0L6objectS1349);
  #line 68 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6objectS1349, (moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS3095);
  _M0L8_2afieldS3315 = _M0L4selfS1351->$1;
  _M0L7_2abindS1354 = _M0L8_2afieldS3315;
  if (_M0L7_2abindS1354 == 0) {
    
  } else {
    struct _M0TP38clawteam8clawteam2ai5Usage* _M0L7_2aSomeS1355 =
      _M0L7_2abindS1354;
    struct _M0TP38clawteam8clawteam2ai5Usage* _M0L8_2ausageS1356 =
      _M0L7_2aSomeS1355;
    moonbit_incref(_M0L8_2ausageS1356);
    _M0L5usageS1353 = _M0L8_2ausageS1356;
    goto join_1352;
  }
  goto joinlet_3796;
  join_1352:;
  #line 70 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3098
  = _M0MP38clawteam8clawteam2ai5Usage10to__openai(_M0L5usageS1353);
  #line 70 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3097
  = _M0IP48clawteam8clawteam8internal6openai15CompletionUsagePB6ToJson8to__json(_M0L6_2atmpS3098);
  moonbit_incref(_M0L6objectS1349);
  #line 70 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6objectS1349, (moonbit_string_t)moonbit_string_literal_10.data, _M0L6_2atmpS3097);
  joinlet_3796:;
  _M0L8_2afieldS3314 = _M0L4selfS1351->$2;
  _M0L6_2acntS3639 = Moonbit_object_header(_M0L4selfS1351)->rc;
  if (_M0L6_2acntS3639 > 1) {
    int32_t _M0L11_2anew__cntS3641 = _M0L6_2acntS3639 - 1;
    Moonbit_object_header(_M0L4selfS1351)->rc = _M0L11_2anew__cntS3641;
    moonbit_incref(_M0L8_2afieldS3314);
  } else if (_M0L6_2acntS3639 == 1) {
    struct _M0TP38clawteam8clawteam2ai5Usage* _M0L8_2afieldS3640 =
      _M0L4selfS1351->$1;
    if (_M0L8_2afieldS3640) {
      moonbit_decref(_M0L8_2afieldS3640);
    }
    #line 72 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
    moonbit_free(_M0L4selfS1351);
  }
  _M0L7messageS3101 = _M0L8_2afieldS3314;
  #line 72 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3100
  = _M0MP38clawteam8clawteam2ai7Message10to__openai(_M0L7messageS3101);
  #line 72 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3099
  = _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__json(_M0L6_2atmpS3100);
  moonbit_incref(_M0L6objectS1349);
  #line 72 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6objectS1349, (moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3099);
  #line 73 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  return _M0MPC14json4Json6object(_M0L6objectS1349);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal12conversation41____test__74797065735f6a736f6e2e6d6274__0(
  
) {
  int64_t _M0L6_2atmpS3092;
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L6_2atmpS3093;
  void* _M0L6_2atmpS3094;
  struct _M0TP48clawteam8clawteam8internal12conversation7Message* _M0L6_2atmpS3086;
  int64_t _M0L6_2atmpS3088;
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L6_2atmpS3091;
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L6_2atmpS3089;
  void* _M0L6_2atmpS3090;
  struct _M0TP48clawteam8clawteam8internal12conversation7Message* _M0L6_2atmpS3087;
  struct _M0TP48clawteam8clawteam8internal12conversation7Message** _M0L6_2atmpS3085;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE* _M0L8messagesS1343;
  struct _M0TPB6ToJson _M0L6_2atmpS3028;
  moonbit_string_t _M0L6_2atmpS3084;
  void* _M0L6_2atmpS3083;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3073;
  void* _M0L6_2atmpS3082;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3079;
  void* _M0L6_2atmpS3081;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3080;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1345;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3078;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3077;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3076;
  void* _M0L6_2atmpS3075;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3074;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1344;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3072;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3071;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3070;
  void* _M0L6_2atmpS3039;
  moonbit_string_t _M0L6_2atmpS3069;
  void* _M0L6_2atmpS3068;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3044;
  moonbit_string_t _M0L6_2atmpS3067;
  void* _M0L6_2atmpS3066;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3059;
  moonbit_string_t _M0L6_2atmpS3065;
  void* _M0L6_2atmpS3064;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3060;
  moonbit_string_t _M0L6_2atmpS3063;
  void* _M0L6_2atmpS3062;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3061;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1347;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3058;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3057;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3056;
  void* _M0L6_2atmpS3055;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3045;
  void* _M0L6_2atmpS3054;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3051;
  void* _M0L6_2atmpS3053;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3052;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1348;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3050;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3049;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3048;
  void* _M0L6_2atmpS3047;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3046;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1346;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3043;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3042;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3041;
  void* _M0L6_2atmpS3040;
  void** _M0L6_2atmpS3038;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3037;
  void* _M0L6_2atmpS3036;
  void* _M0L6_2atmpS3029;
  moonbit_string_t _M0L6_2atmpS3032;
  moonbit_string_t _M0L6_2atmpS3033;
  moonbit_string_t _M0L6_2atmpS3034;
  moonbit_string_t _M0L6_2atmpS3035;
  moonbit_string_t* _M0L6_2atmpS3031;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3030;
  #line 2 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  #line 5 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3092
  = _M0MP38clawteam8clawteam5clock9Timestamp8from__ms(1625079600ll);
  _M0L6_2atmpS3093 = 0;
  #line 7 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3094
  = _M0FP38clawteam8clawteam2ai13user__message((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L6_2atmpS3086
  = (struct _M0TP48clawteam8clawteam8internal12conversation7Message*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal12conversation7Message));
  Moonbit_object_header(_M0L6_2atmpS3086)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal12conversation7Message, $1) >> 2, 2, 0);
  _M0L6_2atmpS3086->$0 = _M0L6_2atmpS3092;
  _M0L6_2atmpS3086->$1 = _M0L6_2atmpS3093;
  _M0L6_2atmpS3086->$2 = _M0L6_2atmpS3094;
  #line 10 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3088
  = _M0MP38clawteam8clawteam5clock9Timestamp8from__ms(1625079600ll);
  #line 11 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3091
  = _M0FP38clawteam8clawteam2ai13usage_2einner(10, 10, 20, 4294967296ll);
  _M0L6_2atmpS3089 = _M0L6_2atmpS3091;
  #line 12 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3090
  = _M0FP38clawteam8clawteam2ai13user__message((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L6_2atmpS3087
  = (struct _M0TP48clawteam8clawteam8internal12conversation7Message*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal12conversation7Message));
  Moonbit_object_header(_M0L6_2atmpS3087)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal12conversation7Message, $1) >> 2, 2, 0);
  _M0L6_2atmpS3087->$0 = _M0L6_2atmpS3088;
  _M0L6_2atmpS3087->$1 = _M0L6_2atmpS3089;
  _M0L6_2atmpS3087->$2 = _M0L6_2atmpS3090;
  _M0L6_2atmpS3085
  = (struct _M0TP48clawteam8clawteam8internal12conversation7Message**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3085[0] = _M0L6_2atmpS3086;
  _M0L6_2atmpS3085[1] = _M0L6_2atmpS3087;
  _M0L8messagesS1343
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE));
  Moonbit_object_header(_M0L8messagesS1343)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE, $0) >> 2, 1, 0);
  _M0L8messagesS1343->$0 = _M0L6_2atmpS3085;
  _M0L8messagesS1343->$1 = 2;
  _M0L6_2atmpS3028
  = (struct _M0TPB6ToJson){
    _M0FP0172moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fconversation_2fMessage_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L8messagesS1343
  };
  _M0L6_2atmpS3084 = 0;
  #line 17 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3083
  = _M0MPC14json4Json6number(0x1.8372fccp+30, _M0L6_2atmpS3084);
  _M0L8_2atupleS3073
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3073)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3073->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3073->$1 = _M0L6_2atmpS3083;
  #line 18 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3082
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS3079
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3079)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3079->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS3079->$1 = _M0L6_2atmpS3082;
  #line 18 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3081
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS3080
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3080)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3080->$0 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L8_2atupleS3080->$1 = _M0L6_2atmpS3081;
  _M0L7_2abindS1345 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1345[0] = _M0L8_2atupleS3079;
  _M0L7_2abindS1345[1] = _M0L8_2atupleS3080;
  _M0L6_2atmpS3078 = _M0L7_2abindS1345;
  _M0L6_2atmpS3077
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3078
  };
  #line 18 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3076 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3077);
  #line 18 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3075 = _M0MPC14json4Json6object(_M0L6_2atmpS3076);
  _M0L8_2atupleS3074
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3074)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3074->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3074->$1 = _M0L6_2atmpS3075;
  _M0L7_2abindS1344 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1344[0] = _M0L8_2atupleS3073;
  _M0L7_2abindS1344[1] = _M0L8_2atupleS3074;
  _M0L6_2atmpS3072 = _M0L7_2abindS1344;
  _M0L6_2atmpS3071
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3072
  };
  #line 16 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3070 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3071);
  #line 16 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3039 = _M0MPC14json4Json6object(_M0L6_2atmpS3070);
  _M0L6_2atmpS3069 = 0;
  #line 21 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3068
  = _M0MPC14json4Json6number(0x1.8372fccp+30, _M0L6_2atmpS3069);
  _M0L8_2atupleS3044
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3044)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3044->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3044->$1 = _M0L6_2atmpS3068;
  _M0L6_2atmpS3067 = 0;
  #line 23 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3066 = _M0MPC14json4Json6number(0x1.4p+3, _M0L6_2atmpS3067);
  _M0L8_2atupleS3059
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3059)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3059->$0 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L8_2atupleS3059->$1 = _M0L6_2atmpS3066;
  _M0L6_2atmpS3065 = 0;
  #line 24 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3064 = _M0MPC14json4Json6number(0x1.4p+3, _M0L6_2atmpS3065);
  _M0L8_2atupleS3060
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3060)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3060->$0 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L8_2atupleS3060->$1 = _M0L6_2atmpS3064;
  _M0L6_2atmpS3063 = 0;
  #line 25 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3062 = _M0MPC14json4Json6number(0x1.4p+4, _M0L6_2atmpS3063);
  _M0L8_2atupleS3061
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3061)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3061->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3061->$1 = _M0L6_2atmpS3062;
  _M0L7_2abindS1347 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1347[0] = _M0L8_2atupleS3059;
  _M0L7_2abindS1347[1] = _M0L8_2atupleS3060;
  _M0L7_2abindS1347[2] = _M0L8_2atupleS3061;
  _M0L6_2atmpS3058 = _M0L7_2abindS1347;
  _M0L6_2atmpS3057
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3058
  };
  #line 22 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3056 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3057);
  #line 22 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3055 = _M0MPC14json4Json6object(_M0L6_2atmpS3056);
  _M0L8_2atupleS3045
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3045)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3045->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3045->$1 = _M0L6_2atmpS3055;
  #line 27 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3054
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS3051
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3051)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3051->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS3051->$1 = _M0L6_2atmpS3054;
  #line 27 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3053
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS3052
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3052)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3052->$0 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L8_2atupleS3052->$1 = _M0L6_2atmpS3053;
  _M0L7_2abindS1348 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1348[0] = _M0L8_2atupleS3051;
  _M0L7_2abindS1348[1] = _M0L8_2atupleS3052;
  _M0L6_2atmpS3050 = _M0L7_2abindS1348;
  _M0L6_2atmpS3049
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3050
  };
  #line 27 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3048 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3049);
  #line 27 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3047 = _M0MPC14json4Json6object(_M0L6_2atmpS3048);
  _M0L8_2atupleS3046
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3046)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3046->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3046->$1 = _M0L6_2atmpS3047;
  _M0L7_2abindS1346 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1346[0] = _M0L8_2atupleS3044;
  _M0L7_2abindS1346[1] = _M0L8_2atupleS3045;
  _M0L7_2abindS1346[2] = _M0L8_2atupleS3046;
  _M0L6_2atmpS3043 = _M0L7_2abindS1346;
  _M0L6_2atmpS3042
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3043
  };
  #line 20 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3041 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3042);
  #line 20 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3040 = _M0MPC14json4Json6object(_M0L6_2atmpS3041);
  _M0L6_2atmpS3038 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3038[0] = _M0L6_2atmpS3039;
  _M0L6_2atmpS3038[1] = _M0L6_2atmpS3040;
  _M0L6_2atmpS3037
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3037)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3037->$0 = _M0L6_2atmpS3038;
  _M0L6_2atmpS3037->$1 = 2;
  #line 15 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  _M0L6_2atmpS3036 = _M0MPC14json4Json5array(_M0L6_2atmpS3037);
  _M0L6_2atmpS3029 = _M0L6_2atmpS3036;
  _M0L6_2atmpS3032 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3033 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS3034 = 0;
  _M0L6_2atmpS3035 = 0;
  _M0L6_2atmpS3031 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3031[0] = _M0L6_2atmpS3032;
  _M0L6_2atmpS3031[1] = _M0L6_2atmpS3033;
  _M0L6_2atmpS3031[2] = _M0L6_2atmpS3034;
  _M0L6_2atmpS3031[3] = _M0L6_2atmpS3035;
  _M0L6_2atmpS3030
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3030)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3030->$0 = _M0L6_2atmpS3031;
  _M0L6_2atmpS3030->$1 = 4;
  #line 15 "E:\\moonbit\\clawteam\\internal\\conversation\\types_json.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3028, _M0L6_2atmpS3029, (moonbit_string_t)moonbit_string_literal_21.data, _M0L6_2atmpS3030);
}

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0MP38clawteam8clawteam2ai5Usage10to__openai(
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L4selfS1341
) {
  int64_t _M0L19cache__read__tokensS3023;
  struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L6_2atmpS3024;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L23prompt__tokens__detailsS1340;
  int32_t _M0L14output__tokensS3017;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L6_2atmpS3018;
  void* _M0L4NoneS3019;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L6_2atmpS3020;
  int32_t _M0L13input__tokensS3021;
  int32_t _M0L8_2afieldS3316;
  int32_t _M0L13total__tokensS3022;
  #line 94 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  _M0L19cache__read__tokensS3023 = _M0L4selfS1341->$3;
  _M0L6_2atmpS3024
  = (struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails*)&_M0MP38clawteam8clawteam2ai5Usage10to__openaiC3025l95$closure.data;
  #line 95 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  _M0L23prompt__tokens__detailsS1340
  = _M0MPC16option6Option3mapGiRP48clawteam8clawteam8internal6openai19PromptTokensDetailsE(_M0L19cache__read__tokensS3023, _M0L6_2atmpS3024);
  _M0L14output__tokensS3017 = _M0L4selfS1341->$1;
  _M0L6_2atmpS3018 = 0;
  _M0L4NoneS3019
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L6_2atmpS3020 = 0;
  _M0L13input__tokensS3021 = _M0L4selfS1341->$0;
  _M0L8_2afieldS3316 = _M0L4selfS1341->$2;
  moonbit_decref(_M0L4selfS1341);
  _M0L13total__tokensS3022 = _M0L8_2afieldS3316;
  #line 98 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  return _M0MP48clawteam8clawteam8internal6openai15CompletionUsage3new(_M0L14output__tokensS3017, _M0L6_2atmpS3018, _M0L4NoneS3019, _M0L6_2atmpS3020, _M0L13input__tokensS3021, _M0L23prompt__tokens__detailsS1340, _M0L13total__tokensS3022);
}

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0MP38clawteam8clawteam2ai5Usage10to__openaiC3025l95(
  struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L6_2aenvS3026,
  int32_t _M0L14cached__tokensS1342
) {
  int64_t _M0L6_2atmpS3027;
  #line 95 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  moonbit_decref(_M0L6_2aenvS3026);
  _M0L6_2atmpS3027 = (int64_t)_M0L14cached__tokensS1342;
  #line 96 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  return _M0MP48clawteam8clawteam8internal6openai19PromptTokensDetails3new(_M0L6_2atmpS3027, 4294967296ll);
}

void* _M0MP38clawteam8clawteam2ai7Message10to__openai(void* _M0L4selfS1329) {
  moonbit_string_t _M0L7contentS1320;
  moonbit_string_t _M0L14tool__call__idS1321;
  moonbit_string_t _M0L7contentS1323;
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* _M0L11tool__callsS1324;
  moonbit_string_t _M0L7contentS1326;
  moonbit_string_t _M0L7contentS1328;
  moonbit_string_t _M0L6_2atmpS3016;
  moonbit_string_t _M0L6_2atmpS3015;
  moonbit_string_t _M0L6_2atmpS3012;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3013;
  moonbit_string_t _M0L6_2atmpS3014;
  #line 3 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  switch (Moonbit_object_tag(_M0L4selfS1329)) {
    case 0: {
      struct _M0DTP38clawteam8clawteam2ai7Message4User* _M0L7_2aUserS1330 =
        (struct _M0DTP38clawteam8clawteam2ai7Message4User*)_M0L4selfS1329;
      moonbit_string_t _M0L8_2afieldS3317 = _M0L7_2aUserS1330->$0;
      int32_t _M0L6_2acntS3642 = Moonbit_object_header(_M0L7_2aUserS1330)->rc;
      moonbit_string_t _M0L10_2acontentS1331;
      if (_M0L6_2acntS3642 > 1) {
        int32_t _M0L11_2anew__cntS3643 = _M0L6_2acntS3642 - 1;
        Moonbit_object_header(_M0L7_2aUserS1330)->rc = _M0L11_2anew__cntS3643;
        moonbit_incref(_M0L8_2afieldS3317);
      } else if (_M0L6_2acntS3642 == 1) {
        #line 4 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
        moonbit_free(_M0L7_2aUserS1330);
      }
      _M0L10_2acontentS1331 = _M0L8_2afieldS3317;
      _M0L7contentS1328 = _M0L10_2acontentS1331;
      goto join_1327;
      break;
    }
    
    case 1: {
      struct _M0DTP38clawteam8clawteam2ai7Message6System* _M0L9_2aSystemS1332 =
        (struct _M0DTP38clawteam8clawteam2ai7Message6System*)_M0L4selfS1329;
      moonbit_string_t _M0L8_2afieldS3318 = _M0L9_2aSystemS1332->$0;
      int32_t _M0L6_2acntS3644 =
        Moonbit_object_header(_M0L9_2aSystemS1332)->rc;
      moonbit_string_t _M0L10_2acontentS1333;
      if (_M0L6_2acntS3644 > 1) {
        int32_t _M0L11_2anew__cntS3645 = _M0L6_2acntS3644 - 1;
        Moonbit_object_header(_M0L9_2aSystemS1332)->rc
        = _M0L11_2anew__cntS3645;
        moonbit_incref(_M0L8_2afieldS3318);
      } else if (_M0L6_2acntS3644 == 1) {
        #line 4 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
        moonbit_free(_M0L9_2aSystemS1332);
      }
      _M0L10_2acontentS1333 = _M0L8_2afieldS3318;
      _M0L7contentS1326 = _M0L10_2acontentS1333;
      goto join_1325;
      break;
    }
    
    case 2: {
      struct _M0DTP38clawteam8clawteam2ai7Message9Assistant* _M0L12_2aAssistantS1334 =
        (struct _M0DTP38clawteam8clawteam2ai7Message9Assistant*)_M0L4selfS1329;
      moonbit_string_t _M0L8_2afieldS3320 = _M0L12_2aAssistantS1334->$0;
      moonbit_string_t _M0L10_2acontentS1335 = _M0L8_2afieldS3320;
      struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* _M0L8_2afieldS3319 =
        _M0L12_2aAssistantS1334->$1;
      int32_t _M0L6_2acntS3646 =
        Moonbit_object_header(_M0L12_2aAssistantS1334)->rc;
      struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* _M0L14_2atool__callsS1336;
      if (_M0L6_2acntS3646 > 1) {
        int32_t _M0L11_2anew__cntS3647 = _M0L6_2acntS3646 - 1;
        Moonbit_object_header(_M0L12_2aAssistantS1334)->rc
        = _M0L11_2anew__cntS3647;
        moonbit_incref(_M0L8_2afieldS3319);
        moonbit_incref(_M0L10_2acontentS1335);
      } else if (_M0L6_2acntS3646 == 1) {
        #line 4 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
        moonbit_free(_M0L12_2aAssistantS1334);
      }
      _M0L14_2atool__callsS1336 = _M0L8_2afieldS3319;
      _M0L7contentS1323 = _M0L10_2acontentS1335;
      _M0L11tool__callsS1324 = _M0L14_2atool__callsS1336;
      goto join_1322;
      break;
    }
    default: {
      struct _M0DTP38clawteam8clawteam2ai7Message4Tool* _M0L7_2aToolS1337 =
        (struct _M0DTP38clawteam8clawteam2ai7Message4Tool*)_M0L4selfS1329;
      moonbit_string_t _M0L8_2afieldS3322 = _M0L7_2aToolS1337->$0;
      moonbit_string_t _M0L10_2acontentS1338 = _M0L8_2afieldS3322;
      moonbit_string_t _M0L8_2afieldS3321 = _M0L7_2aToolS1337->$1;
      int32_t _M0L6_2acntS3648 = Moonbit_object_header(_M0L7_2aToolS1337)->rc;
      moonbit_string_t _M0L17_2atool__call__idS1339;
      if (_M0L6_2acntS3648 > 1) {
        int32_t _M0L11_2anew__cntS3649 = _M0L6_2acntS3648 - 1;
        Moonbit_object_header(_M0L7_2aToolS1337)->rc = _M0L11_2anew__cntS3649;
        moonbit_incref(_M0L8_2afieldS3321);
        moonbit_incref(_M0L10_2acontentS1338);
      } else if (_M0L6_2acntS3648 == 1) {
        #line 4 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
        moonbit_free(_M0L7_2aToolS1337);
      }
      _M0L17_2atool__call__idS1339 = _M0L8_2afieldS3321;
      _M0L7contentS1320 = _M0L10_2acontentS1338;
      _M0L14tool__call__idS1321 = _M0L17_2atool__call__idS1339;
      goto join_1319;
      break;
    }
  }
  join_1327:;
  _M0L6_2atmpS3016 = 0;
  #line 5 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  return _M0FP48clawteam8clawteam8internal6openai13user__messageGsE(_M0L7contentS1328, _M0L6_2atmpS3016);
  join_1325:;
  _M0L6_2atmpS3015 = 0;
  #line 6 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  return _M0FP48clawteam8clawteam8internal6openai15system__messageGsE(_M0L7contentS1326, _M0L6_2atmpS3015);
  join_1322:;
  _M0L6_2atmpS3012 = _M0L7contentS1323;
  moonbit_incref(_M0MP38clawteam8clawteam2ai8ToolCall16to__openai_2eclo);
  #line 10 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  _M0L6_2atmpS3013
  = _M0MPC15array5Array3mapGRP38clawteam8clawteam2ai8ToolCallRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L11tool__callsS1324, _M0MP38clawteam8clawteam2ai8ToolCall16to__openai_2eclo);
  _M0L6_2atmpS3014 = 0;
  #line 8 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  return _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGsE(_M0L6_2atmpS3012, _M0L6_2atmpS3013, _M0L6_2atmpS3014);
  join_1319:;
  #line 13 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  return _M0FP48clawteam8clawteam8internal6openai13tool__messageGsE(_M0L7contentS1320, _M0L14tool__call__idS1321);
}

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0MP38clawteam8clawteam2ai8ToolCall10to__openai(
  struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0L4selfS1318
) {
  moonbit_string_t _M0L8_2afieldS3325;
  moonbit_string_t _M0L2idS3009;
  moonbit_string_t _M0L8_2afieldS3324;
  moonbit_string_t _M0L4nameS3010;
  moonbit_string_t _M0L8_2afieldS3323;
  int32_t _M0L6_2acntS3650;
  moonbit_string_t _M0L9argumentsS3011;
  #line 78 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  _M0L8_2afieldS3325 = _M0L4selfS1318->$0;
  _M0L2idS3009 = _M0L8_2afieldS3325;
  _M0L8_2afieldS3324 = _M0L4selfS1318->$1;
  _M0L4nameS3010 = _M0L8_2afieldS3324;
  _M0L8_2afieldS3323 = _M0L4selfS1318->$2;
  _M0L6_2acntS3650 = Moonbit_object_header(_M0L4selfS1318)->rc;
  if (_M0L6_2acntS3650 > 1) {
    int32_t _M0L11_2anew__cntS3651 = _M0L6_2acntS3650 - 1;
    Moonbit_object_header(_M0L4selfS1318)->rc = _M0L11_2anew__cntS3651;
    if (_M0L8_2afieldS3323) {
      moonbit_incref(_M0L8_2afieldS3323);
    }
    moonbit_incref(_M0L4nameS3010);
    moonbit_incref(_M0L2idS3009);
  } else if (_M0L6_2acntS3650 == 1) {
    #line 81 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
    moonbit_free(_M0L4selfS1318);
  }
  _M0L9argumentsS3011 = _M0L8_2afieldS3323;
  #line 81 "E:\\moonbit\\clawteam\\ai\\convert.mbt"
  return _M0FP48clawteam8clawteam8internal6openai10tool__call(_M0L2idS3009, _M0L4nameS3010, _M0L9argumentsS3011);
}

struct _M0TP38clawteam8clawteam2ai5Usage* _M0FP38clawteam8clawteam2ai13usage_2einner(
  int32_t _M0L13input__tokensS1314,
  int32_t _M0L14output__tokensS1315,
  int32_t _M0L13total__tokensS1316,
  int64_t _M0L19cache__read__tokensS1317
) {
  struct _M0TP38clawteam8clawteam2ai5Usage* _block_3801;
  #line 63 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3801
  = (struct _M0TP38clawteam8clawteam2ai5Usage*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam2ai5Usage));
  Moonbit_object_header(_block_3801)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP38clawteam8clawteam2ai5Usage) >> 2, 0, 0);
  _block_3801->$0 = _M0L13input__tokensS1314;
  _block_3801->$1 = _M0L14output__tokensS1315;
  _block_3801->$2 = _M0L13total__tokensS1316;
  _block_3801->$3 = _M0L19cache__read__tokensS1317;
  return _block_3801;
}

void* _M0FP38clawteam8clawteam2ai13user__message(
  moonbit_string_t _M0L7contentS1313
) {
  void* _block_3802;
  #line 10 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3802
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam2ai7Message4User));
  Moonbit_object_header(_block_3802)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam2ai7Message4User, $0) >> 2, 1, 0);
  ((struct _M0DTP38clawteam8clawteam2ai7Message4User*)_block_3802)->$0
  = _M0L7contentS1313;
  return _block_3802;
}

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(
  void* _M0L4selfS1310
) {
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L5paramS1302;
  struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text* _M0L7_2aTextS1311;
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L8_2afieldS3328;
  int32_t _M0L6_2acntS3652;
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L8_2aparamS1312;
  void* _M0L6_2atmpS3008;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3004;
  moonbit_string_t _M0L8_2afieldS3327;
  moonbit_string_t _M0L4textS3007;
  void* _M0L6_2atmpS3006;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3005;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1304;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3003;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3002;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1303;
  int32_t _M0L14cache__controlS1306;
  int64_t _M0L8_2afieldS3326;
  int64_t _M0L7_2abindS1307;
  void* _M0L6_2atmpS3001;
  #line 16 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L7_2aTextS1311
  = (struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text*)_M0L4selfS1310;
  _M0L8_2afieldS3328 = _M0L7_2aTextS1311->$0;
  _M0L6_2acntS3652 = Moonbit_object_header(_M0L7_2aTextS1311)->rc;
  if (_M0L6_2acntS3652 > 1) {
    int32_t _M0L11_2anew__cntS3653 = _M0L6_2acntS3652 - 1;
    Moonbit_object_header(_M0L7_2aTextS1311)->rc = _M0L11_2anew__cntS3653;
    moonbit_incref(_M0L8_2afieldS3328);
  } else if (_M0L6_2acntS3652 == 1) {
    #line 19 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
    moonbit_free(_M0L7_2aTextS1311);
  }
  _M0L8_2aparamS1312 = _M0L8_2afieldS3328;
  _M0L5paramS1302 = _M0L8_2aparamS1312;
  goto join_1301;
  join_1301:;
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L6_2atmpS3008
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3004
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3004)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3004->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3004->$1 = _M0L6_2atmpS3008;
  _M0L8_2afieldS3327 = _M0L5paramS1302->$0;
  _M0L4textS3007 = _M0L8_2afieldS3327;
  moonbit_incref(_M0L4textS3007);
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L6_2atmpS3006 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4textS3007);
  _M0L8_2atupleS3005
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3005)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3005->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3005->$1 = _M0L6_2atmpS3006;
  _M0L7_2abindS1304 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1304[0] = _M0L8_2atupleS3004;
  _M0L7_2abindS1304[1] = _M0L8_2atupleS3005;
  _M0L6_2atmpS3003 = _M0L7_2abindS1304;
  _M0L6_2atmpS3002
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3003
  };
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L4jsonS1303 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3002);
  _M0L8_2afieldS3326 = _M0L5paramS1302->$1;
  moonbit_decref(_M0L5paramS1302);
  _M0L7_2abindS1307 = _M0L8_2afieldS3326;
  if (_M0L7_2abindS1307 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1308 = _M0L7_2abindS1307;
    int32_t _M0L17_2acache__controlS1309 = (int32_t)_M0L7_2aSomeS1308;
    _M0L14cache__controlS1306 = _M0L17_2acache__controlS1309;
    goto join_1305;
  }
  goto joinlet_3804;
  join_1305:;
  #line 23 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L6_2atmpS3001
  = _M0IP48clawteam8clawteam8internal6openai12CacheControlPB6ToJson8to__json(_M0L14cache__controlS1306);
  moonbit_incref(_M0L4jsonS1303);
  #line 23 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1303, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS3001);
  joinlet_3804:;
  #line 25 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1303);
}

void* _M0IP48clawteam8clawteam8internal6openai12CacheControlPB6ToJson8to__json(
  int32_t _M0L4selfS1300
) {
  void* _M0L6_2atmpS3000;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2999;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1299;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2998;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2997;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2996;
  #line 8 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  _M0L6_2atmpS3000
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS2999
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2999)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2999->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS2999->$1 = _M0L6_2atmpS3000;
  _M0L7_2abindS1299 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1299[0] = _M0L8_2atupleS2999;
  _M0L6_2atmpS2998 = _M0L7_2abindS1299;
  _M0L6_2atmpS2997
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS2998
  };
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  _M0L6_2atmpS2996 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2997);
  #line 9 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS2996);
}

void* _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__json(
  void* _M0L4selfS1290
) {
  int32_t _M0L24content__parts__to__jsonS1247;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L5paramS1261;
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L5paramS1264;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L5paramS1273;
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L5paramS1282;
  void* _M0L6_2atmpS2995;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2994;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1284;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2993;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2992;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1283;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3338;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS2990;
  moonbit_string_t _M0L4nameS1286;
  moonbit_string_t _M0L8_2afieldS3337;
  int32_t _M0L6_2acntS3664;
  moonbit_string_t _M0L7_2abindS1287;
  void* _M0L6_2atmpS2991;
  void* _M0L6_2atmpS2989;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2988;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1275;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2987;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2986;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1274;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3336;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS2984;
  moonbit_string_t _M0L4nameS1277;
  moonbit_string_t _M0L8_2afieldS3335;
  int32_t _M0L6_2acntS3661;
  moonbit_string_t _M0L7_2abindS1278;
  void* _M0L6_2atmpS2985;
  void* _M0L6_2atmpS2983;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2982;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1266;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2981;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2980;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1265;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3334;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS2974;
  moonbit_string_t _M0L4nameS1268;
  moonbit_string_t _M0L8_2afieldS3333;
  moonbit_string_t _M0L7_2abindS1269;
  void* _M0L6_2atmpS2975;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L8_2afieldS3332;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS2977;
  int32_t _M0L6_2atmpS2976;
  void* _M0L6_2atmpS2973;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2966;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3330;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS2972;
  void* _M0L6_2atmpS2971;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2967;
  moonbit_string_t _M0L8_2afieldS3329;
  int32_t _M0L6_2acntS3654;
  moonbit_string_t _M0L14tool__call__idS2970;
  void* _M0L6_2atmpS2969;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2968;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1262;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2965;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2964;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2963;
  #line 11 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L24content__parts__to__jsonS1247 = 0;
  switch (Moonbit_object_tag(_M0L4selfS1290)) {
    case 0: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System* _M0L9_2aSystemS1291 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System*)_M0L4selfS1290;
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L8_2afieldS3339 =
        _M0L9_2aSystemS1291->$0;
      int32_t _M0L6_2acntS3667 =
        Moonbit_object_header(_M0L9_2aSystemS1291)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L8_2aparamS1292;
      if (_M0L6_2acntS3667 > 1) {
        int32_t _M0L11_2anew__cntS3668 = _M0L6_2acntS3667 - 1;
        Moonbit_object_header(_M0L9_2aSystemS1291)->rc
        = _M0L11_2anew__cntS3668;
        moonbit_incref(_M0L8_2afieldS3339);
      } else if (_M0L6_2acntS3667 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L9_2aSystemS1291);
      }
      _M0L8_2aparamS1292 = _M0L8_2afieldS3339;
      _M0L5paramS1282 = _M0L8_2aparamS1292;
      goto join_1281;
      break;
    }
    
    case 1: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User* _M0L7_2aUserS1293 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User*)_M0L4selfS1290;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L8_2afieldS3340 =
        _M0L7_2aUserS1293->$0;
      int32_t _M0L6_2acntS3669 = Moonbit_object_header(_M0L7_2aUserS1293)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L8_2aparamS1294;
      if (_M0L6_2acntS3669 > 1) {
        int32_t _M0L11_2anew__cntS3670 = _M0L6_2acntS3669 - 1;
        Moonbit_object_header(_M0L7_2aUserS1293)->rc = _M0L11_2anew__cntS3670;
        moonbit_incref(_M0L8_2afieldS3340);
      } else if (_M0L6_2acntS3669 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L7_2aUserS1293);
      }
      _M0L8_2aparamS1294 = _M0L8_2afieldS3340;
      _M0L5paramS1273 = _M0L8_2aparamS1294;
      goto join_1272;
      break;
    }
    
    case 2: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant* _M0L12_2aAssistantS1295 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant*)_M0L4selfS1290;
      struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L8_2afieldS3341 =
        _M0L12_2aAssistantS1295->$0;
      int32_t _M0L6_2acntS3671 =
        Moonbit_object_header(_M0L12_2aAssistantS1295)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L8_2aparamS1296;
      if (_M0L6_2acntS3671 > 1) {
        int32_t _M0L11_2anew__cntS3672 = _M0L6_2acntS3671 - 1;
        Moonbit_object_header(_M0L12_2aAssistantS1295)->rc
        = _M0L11_2anew__cntS3672;
        moonbit_incref(_M0L8_2afieldS3341);
      } else if (_M0L6_2acntS3671 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L12_2aAssistantS1295);
      }
      _M0L8_2aparamS1296 = _M0L8_2afieldS3341;
      _M0L5paramS1264 = _M0L8_2aparamS1296;
      goto join_1263;
      break;
    }
    default: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool* _M0L7_2aToolS1297 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool*)_M0L4selfS1290;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L8_2afieldS3342 =
        _M0L7_2aToolS1297->$0;
      int32_t _M0L6_2acntS3673 = Moonbit_object_header(_M0L7_2aToolS1297)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L8_2aparamS1298;
      if (_M0L6_2acntS3673 > 1) {
        int32_t _M0L11_2anew__cntS3674 = _M0L6_2acntS3673 - 1;
        Moonbit_object_header(_M0L7_2aToolS1297)->rc = _M0L11_2anew__cntS3674;
        moonbit_incref(_M0L8_2afieldS3342);
      } else if (_M0L6_2acntS3673 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L7_2aToolS1297);
      }
      _M0L8_2aparamS1298 = _M0L8_2afieldS3342;
      _M0L5paramS1261 = _M0L8_2aparamS1298;
      goto join_1260;
      break;
    }
  }
  join_1281:;
  #line 28 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2995
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_26.data);
  _M0L8_2atupleS2994
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2994)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2994->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS2994->$1 = _M0L6_2atmpS2995;
  _M0L7_2abindS1284 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1284[0] = _M0L8_2atupleS2994;
  _M0L6_2atmpS2993 = _M0L7_2abindS1284;
  _M0L6_2atmpS2992
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS2993
  };
  #line 28 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L4jsonS1283 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2992);
  _M0L8_2afieldS3338 = _M0L5paramS1282->$0;
  _M0L7contentS2990 = _M0L8_2afieldS3338;
  moonbit_incref(_M0L7contentS2990);
  moonbit_incref(_M0L4jsonS1283);
  #line 29 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1247(_M0L24content__parts__to__jsonS1247, _M0L7contentS2990, _M0L4jsonS1283);
  _M0L8_2afieldS3337 = _M0L5paramS1282->$1;
  _M0L6_2acntS3664 = Moonbit_object_header(_M0L5paramS1282)->rc;
  if (_M0L6_2acntS3664 > 1) {
    int32_t _M0L11_2anew__cntS3666 = _M0L6_2acntS3664 - 1;
    Moonbit_object_header(_M0L5paramS1282)->rc = _M0L11_2anew__cntS3666;
    if (_M0L8_2afieldS3337) {
      moonbit_incref(_M0L8_2afieldS3337);
    }
  } else if (_M0L6_2acntS3664 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3665 =
      _M0L5paramS1282->$0;
    moonbit_decref(_M0L8_2afieldS3665);
    #line 30 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L5paramS1282);
  }
  _M0L7_2abindS1287 = _M0L8_2afieldS3337;
  if (_M0L7_2abindS1287 == 0) {
    if (_M0L7_2abindS1287) {
      moonbit_decref(_M0L7_2abindS1287);
    }
  } else {
    moonbit_string_t _M0L7_2aSomeS1288 = _M0L7_2abindS1287;
    moonbit_string_t _M0L7_2anameS1289 = _M0L7_2aSomeS1288;
    _M0L4nameS1286 = _M0L7_2anameS1289;
    goto join_1285;
  }
  goto joinlet_3809;
  join_1285:;
  #line 31 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2991 = _M0MPC14json4Json6string(_M0L4nameS1286);
  moonbit_incref(_M0L4jsonS1283);
  #line 31 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1283, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS2991);
  joinlet_3809:;
  #line 33 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1283);
  join_1272:;
  #line 36 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2989
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS2988
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2988)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2988->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS2988->$1 = _M0L6_2atmpS2989;
  _M0L7_2abindS1275 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1275[0] = _M0L8_2atupleS2988;
  _M0L6_2atmpS2987 = _M0L7_2abindS1275;
  _M0L6_2atmpS2986
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS2987
  };
  #line 36 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L4jsonS1274 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2986);
  _M0L8_2afieldS3336 = _M0L5paramS1273->$0;
  _M0L7contentS2984 = _M0L8_2afieldS3336;
  moonbit_incref(_M0L7contentS2984);
  moonbit_incref(_M0L4jsonS1274);
  #line 37 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1247(_M0L24content__parts__to__jsonS1247, _M0L7contentS2984, _M0L4jsonS1274);
  _M0L8_2afieldS3335 = _M0L5paramS1273->$1;
  _M0L6_2acntS3661 = Moonbit_object_header(_M0L5paramS1273)->rc;
  if (_M0L6_2acntS3661 > 1) {
    int32_t _M0L11_2anew__cntS3663 = _M0L6_2acntS3661 - 1;
    Moonbit_object_header(_M0L5paramS1273)->rc = _M0L11_2anew__cntS3663;
    if (_M0L8_2afieldS3335) {
      moonbit_incref(_M0L8_2afieldS3335);
    }
  } else if (_M0L6_2acntS3661 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3662 =
      _M0L5paramS1273->$0;
    moonbit_decref(_M0L8_2afieldS3662);
    #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L5paramS1273);
  }
  _M0L7_2abindS1278 = _M0L8_2afieldS3335;
  if (_M0L7_2abindS1278 == 0) {
    if (_M0L7_2abindS1278) {
      moonbit_decref(_M0L7_2abindS1278);
    }
  } else {
    moonbit_string_t _M0L7_2aSomeS1279 = _M0L7_2abindS1278;
    moonbit_string_t _M0L7_2anameS1280 = _M0L7_2aSomeS1279;
    _M0L4nameS1277 = _M0L7_2anameS1280;
    goto join_1276;
  }
  goto joinlet_3810;
  join_1276:;
  #line 39 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2985 = _M0MPC14json4Json6string(_M0L4nameS1277);
  moonbit_incref(_M0L4jsonS1274);
  #line 39 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1274, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS2985);
  joinlet_3810:;
  #line 41 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1274);
  join_1263:;
  #line 44 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2983
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_28.data);
  _M0L8_2atupleS2982
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2982)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2982->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS2982->$1 = _M0L6_2atmpS2983;
  _M0L7_2abindS1266 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1266[0] = _M0L8_2atupleS2982;
  _M0L6_2atmpS2981 = _M0L7_2abindS1266;
  _M0L6_2atmpS2980
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS2981
  };
  #line 44 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L4jsonS1265 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2980);
  _M0L8_2afieldS3334 = _M0L5paramS1264->$0;
  _M0L7contentS2974 = _M0L8_2afieldS3334;
  moonbit_incref(_M0L7contentS2974);
  moonbit_incref(_M0L4jsonS1265);
  #line 45 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1247(_M0L24content__parts__to__jsonS1247, _M0L7contentS2974, _M0L4jsonS1265);
  _M0L8_2afieldS3333 = _M0L5paramS1264->$1;
  _M0L7_2abindS1269 = _M0L8_2afieldS3333;
  if (_M0L7_2abindS1269 == 0) {
    
  } else {
    moonbit_string_t _M0L7_2aSomeS1270 = _M0L7_2abindS1269;
    moonbit_string_t _M0L7_2anameS1271 = _M0L7_2aSomeS1270;
    moonbit_incref(_M0L7_2anameS1271);
    _M0L4nameS1268 = _M0L7_2anameS1271;
    goto join_1267;
  }
  goto joinlet_3811;
  join_1267:;
  #line 47 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2975 = _M0MPC14json4Json6string(_M0L4nameS1268);
  moonbit_incref(_M0L4jsonS1265);
  #line 47 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1265, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS2975);
  joinlet_3811:;
  _M0L8_2afieldS3332 = _M0L5paramS1264->$2;
  _M0L11tool__callsS2977 = _M0L8_2afieldS3332;
  moonbit_incref(_M0L11tool__callsS2977);
  #line 49 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2976
  = _M0MPC15array5Array9is__emptyGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L11tool__callsS2977);
  if (!_M0L6_2atmpS2976) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L8_2afieldS3331 =
      _M0L5paramS1264->$2;
    int32_t _M0L6_2acntS3657 = Moonbit_object_header(_M0L5paramS1264)->rc;
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS2979;
    void* _M0L6_2atmpS2978;
    if (_M0L6_2acntS3657 > 1) {
      int32_t _M0L11_2anew__cntS3660 = _M0L6_2acntS3657 - 1;
      Moonbit_object_header(_M0L5paramS1264)->rc = _M0L11_2anew__cntS3660;
      moonbit_incref(_M0L8_2afieldS3331);
    } else if (_M0L6_2acntS3657 == 1) {
      moonbit_string_t _M0L8_2afieldS3659 = _M0L5paramS1264->$1;
      struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3658;
      if (_M0L8_2afieldS3659) {
        moonbit_decref(_M0L8_2afieldS3659);
      }
      _M0L8_2afieldS3658 = _M0L5paramS1264->$0;
      moonbit_decref(_M0L8_2afieldS3658);
      #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
      moonbit_free(_M0L5paramS1264);
    }
    _M0L11tool__callsS2979 = _M0L8_2afieldS3331;
    #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    _M0L6_2atmpS2978
    = _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L11tool__callsS2979);
    moonbit_incref(_M0L4jsonS1265);
    #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1265, (moonbit_string_t)moonbit_string_literal_29.data, _M0L6_2atmpS2978);
  } else {
    moonbit_decref(_M0L5paramS1264);
  }
  #line 52 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1265);
  join_1260:;
  #line 56 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2973
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_30.data);
  _M0L8_2atupleS2966
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2966)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2966->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS2966->$1 = _M0L6_2atmpS2973;
  _M0L8_2afieldS3330 = _M0L5paramS1261->$0;
  _M0L7contentS2972 = _M0L8_2afieldS3330;
  moonbit_incref(_M0L7contentS2972);
  #line 57 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2971
  = _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L7contentS2972);
  _M0L8_2atupleS2967
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2967)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2967->$0 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L8_2atupleS2967->$1 = _M0L6_2atmpS2971;
  _M0L8_2afieldS3329 = _M0L5paramS1261->$1;
  _M0L6_2acntS3654 = Moonbit_object_header(_M0L5paramS1261)->rc;
  if (_M0L6_2acntS3654 > 1) {
    int32_t _M0L11_2anew__cntS3656 = _M0L6_2acntS3654 - 1;
    Moonbit_object_header(_M0L5paramS1261)->rc = _M0L11_2anew__cntS3656;
    moonbit_incref(_M0L8_2afieldS3329);
  } else if (_M0L6_2acntS3654 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS3655 =
      _M0L5paramS1261->$0;
    moonbit_decref(_M0L8_2afieldS3655);
    #line 58 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L5paramS1261);
  }
  _M0L14tool__call__idS2970 = _M0L8_2afieldS3329;
  #line 58 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2969
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L14tool__call__idS2970);
  _M0L8_2atupleS2968
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2968)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2968->$0 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L8_2atupleS2968->$1 = _M0L6_2atmpS2969;
  _M0L7_2abindS1262 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1262[0] = _M0L8_2atupleS2966;
  _M0L7_2abindS1262[1] = _M0L8_2atupleS2967;
  _M0L7_2abindS1262[2] = _M0L8_2atupleS2968;
  _M0L6_2atmpS2965 = _M0L7_2abindS1262;
  _M0L6_2atmpS2964
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS2965
  };
  #line 55 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2963 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2964);
  #line 55 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS2963);
}

int32_t _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1247(
  int32_t _M0L6_2aenvS2952,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L14content__partsS1248,
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1249
) {
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L5partsS1251;
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L4textS1253;
  int32_t _M0L3lenS2956;
  moonbit_string_t _M0L8_2afieldS3343;
  int32_t _M0L6_2acntS3675;
  moonbit_string_t _M0L4textS2955;
  void* _M0L6_2atmpS2954;
  void* _M0L6_2atmpS2953;
  #line 14 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L3lenS2956 = _M0L14content__partsS1248->$1;
  if (_M0L3lenS2956 == 0) {
    moonbit_decref(_M0L4jsonS1249);
    moonbit_decref(_M0L14content__partsS1248);
  } else {
    int32_t _M0L3lenS2957 = _M0L14content__partsS1248->$1;
    if (_M0L3lenS2957 == 1) {
      void** _M0L8_2afieldS3346 = _M0L14content__partsS1248->$0;
      void** _M0L3bufS2960 = _M0L8_2afieldS3346;
      void* _M0L6_2atmpS3345 = (void*)_M0L3bufS2960[0];
      void* _M0L4_2axS1254 = _M0L6_2atmpS3345;
      struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text* _M0L7_2aTextS1255 =
        (struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text*)_M0L4_2axS1254;
      struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L8_2afieldS3344 =
        _M0L7_2aTextS1255->$0;
      struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L7_2atextS1256 =
        _M0L8_2afieldS3344;
      int64_t _M0L7_2abindS1257 = _M0L7_2atextS1256->$1;
      if (_M0L7_2abindS1257 == 4294967296ll) {
        moonbit_incref(_M0L7_2atextS1256);
        moonbit_decref(_M0L14content__partsS1248);
        _M0L4textS1253 = _M0L7_2atextS1256;
        goto join_1252;
      } else {
        int32_t _M0L3lenS2959 = _M0L14content__partsS1248->$1;
        int64_t _M0L6_2atmpS2958 = (int64_t)_M0L3lenS2959;
        struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4_2axS1258;
        #line 20 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        _M0L4_2axS1258
        = _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L14content__partsS1248, 0, _M0L6_2atmpS2958);
        _M0L5partsS1251 = _M0L4_2axS1258;
        goto join_1250;
      }
    } else {
      int32_t _M0L3lenS2962 = _M0L14content__partsS1248->$1;
      int64_t _M0L6_2atmpS2961 = (int64_t)_M0L3lenS2962;
      struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4_2axS1259;
      #line 18 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
      _M0L4_2axS1259
      = _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L14content__partsS1248, 0, _M0L6_2atmpS2961);
      _M0L5partsS1251 = _M0L4_2axS1259;
      goto join_1250;
    }
  }
  goto joinlet_3813;
  join_1252:;
  _M0L8_2afieldS3343 = _M0L4textS1253->$0;
  _M0L6_2acntS3675 = Moonbit_object_header(_M0L4textS1253)->rc;
  if (_M0L6_2acntS3675 > 1) {
    int32_t _M0L11_2anew__cntS3676 = _M0L6_2acntS3675 - 1;
    Moonbit_object_header(_M0L4textS1253)->rc = _M0L11_2anew__cntS3676;
    moonbit_incref(_M0L8_2afieldS3343);
  } else if (_M0L6_2acntS3675 == 1) {
    #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L4textS1253);
  }
  _M0L4textS2955 = _M0L8_2afieldS3343;
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2954 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4textS2955);
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1249, (moonbit_string_t)moonbit_string_literal_15.data, _M0L6_2atmpS2954);
  joinlet_3813:;
  goto joinlet_3812;
  join_1250:;
  #line 22 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS2953
  = _M0IPC15array9ArrayViewPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1251);
  #line 22 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1249, (moonbit_string_t)moonbit_string_literal_15.data, _M0L6_2atmpS2953);
  joinlet_3812:;
  return 0;
}

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L4selfS1246
) {
  moonbit_string_t _M0L8_2afieldS3348;
  moonbit_string_t _M0L2idS2951;
  void* _M0L6_2atmpS2950;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2944;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L8_2afieldS3347;
  int32_t _M0L6_2acntS3677;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L8functionS2949;
  void* _M0L6_2atmpS2948;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2945;
  void* _M0L6_2atmpS2947;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2946;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1245;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2943;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2942;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2941;
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L8_2afieldS3348 = _M0L4selfS1246->$0;
  _M0L2idS2951 = _M0L8_2afieldS3348;
  moonbit_incref(_M0L2idS2951);
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS2950 = _M0IPC16string6StringPB6ToJson8to__json(_M0L2idS2951);
  _M0L8_2atupleS2944
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2944)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2944->$0 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L8_2atupleS2944->$1 = _M0L6_2atmpS2950;
  _M0L8_2afieldS3347 = _M0L4selfS1246->$1;
  _M0L6_2acntS3677 = Moonbit_object_header(_M0L4selfS1246)->rc;
  if (_M0L6_2acntS3677 > 1) {
    int32_t _M0L11_2anew__cntS3679 = _M0L6_2acntS3677 - 1;
    Moonbit_object_header(_M0L4selfS1246)->rc = _M0L11_2anew__cntS3679;
    moonbit_incref(_M0L8_2afieldS3347);
  } else if (_M0L6_2acntS3677 == 1) {
    moonbit_string_t _M0L8_2afieldS3678 = _M0L4selfS1246->$0;
    moonbit_decref(_M0L8_2afieldS3678);
    #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
    moonbit_free(_M0L4selfS1246);
  }
  _M0L8functionS2949 = _M0L8_2afieldS3347;
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS2948
  = _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(_M0L8functionS2949);
  _M0L8_2atupleS2945
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2945)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2945->$0 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L8_2atupleS2945->$1 = _M0L6_2atmpS2948;
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS2947
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_33.data);
  _M0L8_2atupleS2946
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2946)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2946->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS2946->$1 = _M0L6_2atmpS2947;
  _M0L7_2abindS1245 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1245[0] = _M0L8_2atupleS2944;
  _M0L7_2abindS1245[1] = _M0L8_2atupleS2945;
  _M0L7_2abindS1245[2] = _M0L8_2atupleS2946;
  _M0L6_2atmpS2943 = _M0L7_2abindS1245;
  _M0L6_2atmpS2942
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS2943
  };
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS2941 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2942);
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS2941);
}

void* _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L4selfS1244
) {
  moonbit_string_t _M0L8_2afieldS3350;
  moonbit_string_t _M0L4nameS2940;
  void* _M0L6_2atmpS2939;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2934;
  moonbit_string_t _M0L8_2afieldS3349;
  int32_t _M0L6_2acntS3680;
  moonbit_string_t _M0L9argumentsS2938;
  moonbit_string_t _M0L6_2atmpS2937;
  void* _M0L6_2atmpS2936;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2935;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1243;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2933;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2932;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2931;
  #line 9 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L8_2afieldS3350 = _M0L4selfS1244->$1;
  _M0L4nameS2940 = _M0L8_2afieldS3350;
  moonbit_incref(_M0L4nameS2940);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS2939 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4nameS2940);
  _M0L8_2atupleS2934
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2934)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2934->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS2934->$1 = _M0L6_2atmpS2939;
  _M0L8_2afieldS3349 = _M0L4selfS1244->$0;
  _M0L6_2acntS3680 = Moonbit_object_header(_M0L4selfS1244)->rc;
  if (_M0L6_2acntS3680 > 1) {
    int32_t _M0L11_2anew__cntS3682 = _M0L6_2acntS3680 - 1;
    Moonbit_object_header(_M0L4selfS1244)->rc = _M0L11_2anew__cntS3682;
    if (_M0L8_2afieldS3349) {
      moonbit_incref(_M0L8_2afieldS3349);
    }
  } else if (_M0L6_2acntS3680 == 1) {
    moonbit_string_t _M0L8_2afieldS3681 = _M0L4selfS1244->$1;
    moonbit_decref(_M0L8_2afieldS3681);
    #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
    moonbit_free(_M0L4selfS1244);
  }
  _M0L9argumentsS2938 = _M0L8_2afieldS3349;
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS2937
  = _M0MPC16option6Option10unwrap__orGsE(_M0L9argumentsS2938, (moonbit_string_t)moonbit_string_literal_0.data);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS2936
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L6_2atmpS2937);
  _M0L8_2atupleS2935
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2935)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2935->$0 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L8_2atupleS2935->$1 = _M0L6_2atmpS2936;
  _M0L7_2abindS1243 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1243[0] = _M0L8_2atupleS2934;
  _M0L7_2abindS1243[1] = _M0L8_2atupleS2935;
  _M0L6_2atmpS2933 = _M0L7_2abindS1243;
  _M0L6_2atmpS2932
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2933
  };
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS2931 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2932);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS2931);
}

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0MP48clawteam8clawteam8internal6openai15CompletionUsage3new(
  int32_t _M0L18completion__tokensS1236,
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L27completion__tokens__detailsS1237,
  void* _M0L4costS1238,
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L13cost__detailsS1239,
  int32_t _M0L14prompt__tokensS1240,
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L23prompt__tokens__detailsS1241,
  int32_t _M0L13total__tokensS1242
) {
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _block_3814;
  #line 14 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _block_3814
  = (struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage));
  Moonbit_object_header(_block_3814)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage, $1) >> 2, 4, 0);
  _block_3814->$0 = _M0L18completion__tokensS1236;
  _block_3814->$1 = _M0L27completion__tokens__detailsS1237;
  _block_3814->$2 = _M0L4costS1238;
  _block_3814->$3 = _M0L13cost__detailsS1239;
  _block_3814->$4 = _M0L14prompt__tokensS1240;
  _block_3814->$5 = _M0L23prompt__tokens__detailsS1241;
  _block_3814->$6 = _M0L13total__tokensS1242;
  return _block_3814;
}

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0FP48clawteam8clawteam8internal6openai10tool__call(
  moonbit_string_t _M0L2idS1233,
  moonbit_string_t _M0L4nameS1235,
  moonbit_string_t _M0L9argumentsS1234
) {
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L6_2atmpS2930;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _block_3815;
  #line 266 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS2930
  = (struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction));
  Moonbit_object_header(_M0L6_2atmpS2930)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction, $0) >> 2, 2, 0);
  _M0L6_2atmpS2930->$0 = _M0L9argumentsS1234;
  _M0L6_2atmpS2930->$1 = _M0L4nameS1235;
  _block_3815
  = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall));
  Moonbit_object_header(_block_3815)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall, $0) >> 2, 2, 0);
  _block_3815->$0 = _M0L2idS1233;
  _block_3815->$1 = _M0L6_2atmpS2930;
  return _block_3815;
}

void* _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGsE(
  moonbit_string_t _M0L7contentS1228,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1232,
  moonbit_string_t _M0L4nameS1231
) {
  moonbit_string_t _M0L7contentS1227;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1225;
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L6_2atmpS2928;
  void* _block_3817;
  #line 226 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  if (_M0L7contentS1228 == 0) {
    void** _M0L6_2atmpS2929;
    if (_M0L7contentS1228) {
      moonbit_decref(_M0L7contentS1228);
    }
    _M0L6_2atmpS2929 = (void**)moonbit_empty_ref_array;
    _M0L7contentS1225
    = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
    Moonbit_object_header(_M0L7contentS1225)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
    _M0L7contentS1225->$0 = _M0L6_2atmpS2929;
    _M0L7contentS1225->$1 = 0;
  } else {
    moonbit_string_t _M0L7_2aSomeS1229 = _M0L7contentS1228;
    moonbit_string_t _M0L10_2acontentS1230 = _M0L7_2aSomeS1229;
    _M0L7contentS1227 = _M0L10_2acontentS1230;
    goto join_1226;
  }
  goto joinlet_3816;
  join_1226:;
  #line 232 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L7contentS1225
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1227);
  joinlet_3816:;
  _M0L6_2atmpS2928
  = (struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam));
  Moonbit_object_header(_M0L6_2atmpS2928)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam, $0) >> 2, 3, 0);
  _M0L6_2atmpS2928->$0 = _M0L7contentS1225;
  _M0L6_2atmpS2928->$1 = _M0L4nameS1231;
  _M0L6_2atmpS2928->$2 = _M0L11tool__callsS1232;
  _block_3817
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant));
  Moonbit_object_header(_block_3817)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant, $0) >> 2, 1, 2);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant*)_block_3817)->$0
  = _M0L6_2atmpS2928;
  return _block_3817;
}

void* _M0FP48clawteam8clawteam8internal6openai13tool__messageGsE(
  moonbit_string_t _M0L7contentS1223,
  moonbit_string_t _M0L14tool__call__idS1224
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS2927;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L6_2atmpS2926;
  void* _block_3818;
  #line 179 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 184 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS2927
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1223);
  _M0L6_2atmpS2926
  = (struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam));
  Moonbit_object_header(_M0L6_2atmpS2926)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS2926->$0 = _M0L6_2atmpS2927;
  _M0L6_2atmpS2926->$1 = _M0L14tool__call__idS1224;
  _block_3818
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool));
  Moonbit_object_header(_block_3818)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool, $0) >> 2, 1, 3);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool*)_block_3818)->$0
  = _M0L6_2atmpS2926;
  return _block_3818;
}

void* _M0FP48clawteam8clawteam8internal6openai13user__messageGsE(
  moonbit_string_t _M0L7contentS1221,
  moonbit_string_t _M0L4nameS1222
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS2925;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L6_2atmpS2924;
  void* _block_3819;
  #line 160 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 164 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS2925
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1221);
  _M0L6_2atmpS2924
  = (struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam));
  Moonbit_object_header(_M0L6_2atmpS2924)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS2924->$0 = _M0L6_2atmpS2925;
  _M0L6_2atmpS2924->$1 = _M0L4nameS1222;
  _block_3819
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User));
  Moonbit_object_header(_block_3819)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User, $0) >> 2, 1, 1);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User*)_block_3819)->$0
  = _M0L6_2atmpS2924;
  return _block_3819;
}

void* _M0FP48clawteam8clawteam8internal6openai15system__messageGsE(
  moonbit_string_t _M0L7contentS1219,
  moonbit_string_t _M0L4nameS1220
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS2923;
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6_2atmpS2922;
  void* _block_3820;
  #line 142 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS2923
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1219);
  _M0L6_2atmpS2922
  = (struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam));
  Moonbit_object_header(_M0L6_2atmpS2922)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS2922->$0 = _M0L6_2atmpS2923;
  _M0L6_2atmpS2922->$1 = _M0L4nameS1220;
  _block_3820
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System));
  Moonbit_object_header(_block_3820)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System, $0) >> 2, 1, 0);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System*)_block_3820)->$0
  = _M0L6_2atmpS2922;
  return _block_3820;
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(
  moonbit_string_t _M0L4selfS1218
) {
  void* _M0L6_2atmpS2921;
  void** _M0L6_2atmpS2920;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _block_3821;
  #line 97 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 100 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS2921
  = _M0FP48clawteam8clawteam8internal6openai19text__content__part(_M0L4selfS1218, 4294967296ll);
  _M0L6_2atmpS2920 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2920[0] = _M0L6_2atmpS2921;
  _block_3821
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
  Moonbit_object_header(_block_3821)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
  _block_3821->$0 = _M0L6_2atmpS2920;
  _block_3821->$1 = 1;
  return _block_3821;
}

void* _M0FP48clawteam8clawteam8internal6openai19text__content__part(
  moonbit_string_t _M0L4textS1216,
  int64_t _M0L14cache__controlS1217
) {
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L6_2atmpS2919;
  void* _block_3822;
  #line 82 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS2919
  = (struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam));
  Moonbit_object_header(_M0L6_2atmpS2919)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam, $0) >> 2, 1, 0);
  _M0L6_2atmpS2919->$0 = _M0L4textS1216;
  _M0L6_2atmpS2919->$1 = _M0L14cache__controlS1217;
  _block_3822
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text));
  Moonbit_object_header(_block_3822)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text, $0) >> 2, 1, 0);
  ((struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text*)_block_3822)->$0
  = _M0L6_2atmpS2919;
  return _block_3822;
}

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0MP48clawteam8clawteam8internal6openai19PromptTokensDetails3new(
  int64_t _M0L14cached__tokensS1214,
  int64_t _M0L13audio__tokensS1215
) {
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _block_3823;
  #line 9 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _block_3823
  = (struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails));
  Moonbit_object_header(_block_3823)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails) >> 2, 0, 0);
  _block_3823->$0 = _M0L14cached__tokensS1214;
  _block_3823->$1 = _M0L13audio__tokensS1215;
  return _block_3823;
}

void* _M0IP48clawteam8clawteam8internal6openai23CompletionTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L9_2ax__637S1211
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1207;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2918;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2917;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1206;
  int32_t _M0L8_24innerS1209;
  int64_t _M0L8_2afieldS3351;
  int64_t _M0L7_2abindS1210;
  void* _M0L6_2atmpS2916;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0L7_2abindS1207 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2918 = _M0L7_2abindS1207;
  _M0L6_2atmpS2917
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS2918
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0L6_24mapS1206 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2917);
  _M0L8_2afieldS3351 = _M0L9_2ax__637S1211->$0;
  moonbit_decref(_M0L9_2ax__637S1211);
  _M0L7_2abindS1210 = _M0L8_2afieldS3351;
  if (_M0L7_2abindS1210 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1212 = _M0L7_2abindS1210;
    int32_t _M0L11_2a_24innerS1213 = (int32_t)_M0L7_2aSomeS1212;
    _M0L8_24innerS1209 = _M0L11_2a_24innerS1213;
    goto join_1208;
  }
  goto joinlet_3824;
  join_1208:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0L6_2atmpS2916 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1209);
  moonbit_incref(_M0L6_24mapS1206);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1206, (moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS2916);
  joinlet_3824:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1206);
}

void* _M0IP48clawteam8clawteam8internal6openai11CostDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L9_2ax__680S1203
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1199;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2915;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2914;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1198;
  double _M0L8_24innerS1201;
  void* _M0L8_2afieldS3353;
  int32_t _M0L6_2acntS3683;
  void* _M0L7_2abindS1202;
  void* _M0L6_2atmpS2913;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0L7_2abindS1199 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2915 = _M0L7_2abindS1199;
  _M0L6_2atmpS2914
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS2915
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0L6_24mapS1198 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2914);
  _M0L8_2afieldS3353 = _M0L9_2ax__680S1203->$0;
  _M0L6_2acntS3683 = Moonbit_object_header(_M0L9_2ax__680S1203)->rc;
  if (_M0L6_2acntS3683 > 1) {
    int32_t _M0L11_2anew__cntS3684 = _M0L6_2acntS3683 - 1;
    Moonbit_object_header(_M0L9_2ax__680S1203)->rc = _M0L11_2anew__cntS3684;
    moonbit_incref(_M0L8_2afieldS3353);
  } else if (_M0L6_2acntS3683 == 1) {
    #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
    moonbit_free(_M0L9_2ax__680S1203);
  }
  _M0L7_2abindS1202 = _M0L8_2afieldS3353;
  switch (Moonbit_object_tag(_M0L7_2abindS1202)) {
    case 0:
      break;
    default: {
      struct _M0DTPC16option6OptionGdE4Some* _M0L7_2aSomeS1204 =
        (struct _M0DTPC16option6OptionGdE4Some*)_M0L7_2abindS1202;
      double _M0L8_2afieldS3352 = _M0L7_2aSomeS1204->$0;
      double _M0L11_2a_24innerS1205;
      moonbit_decref(_M0L7_2aSomeS1204);
      _M0L11_2a_24innerS1205 = _M0L8_2afieldS3352;
      _M0L8_24innerS1201 = _M0L11_2a_24innerS1205;
      goto join_1200;
      break;
    }
  }
  goto joinlet_3825;
  join_1200:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0L6_2atmpS2913
  = _M0IPC16double6DoublePB6ToJson8to__json(_M0L8_24innerS1201);
  moonbit_incref(_M0L6_24mapS1198);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1198, (moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS2913);
  joinlet_3825:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1198);
}

void* _M0IP48clawteam8clawteam8internal6openai19PromptTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L9_2ax__741S1190
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1186;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2912;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2911;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1185;
  int32_t _M0L8_24innerS1188;
  int64_t _M0L7_2abindS1189;
  void* _M0L6_2atmpS2909;
  int32_t _M0L8_24innerS1194;
  int64_t _M0L8_2afieldS3354;
  int64_t _M0L7_2abindS1195;
  void* _M0L6_2atmpS2910;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L7_2abindS1186 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2912 = _M0L7_2abindS1186;
  _M0L6_2atmpS2911
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS2912
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L6_24mapS1185 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2911);
  _M0L7_2abindS1189 = _M0L9_2ax__741S1190->$0;
  if (_M0L7_2abindS1189 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1191 = _M0L7_2abindS1189;
    int32_t _M0L11_2a_24innerS1192 = (int32_t)_M0L7_2aSomeS1191;
    _M0L8_24innerS1188 = _M0L11_2a_24innerS1192;
    goto join_1187;
  }
  goto joinlet_3826;
  join_1187:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L6_2atmpS2909 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1188);
  moonbit_incref(_M0L6_24mapS1185);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1185, (moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS2909);
  joinlet_3826:;
  _M0L8_2afieldS3354 = _M0L9_2ax__741S1190->$1;
  moonbit_decref(_M0L9_2ax__741S1190);
  _M0L7_2abindS1195 = _M0L8_2afieldS3354;
  if (_M0L7_2abindS1195 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1196 = _M0L7_2abindS1195;
    int32_t _M0L11_2a_24innerS1197 = (int32_t)_M0L7_2aSomeS1196;
    _M0L8_24innerS1194 = _M0L11_2a_24innerS1197;
    goto join_1193;
  }
  goto joinlet_3827;
  join_1193:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L6_2atmpS2910 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1194);
  moonbit_incref(_M0L6_24mapS1185);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1185, (moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS2910);
  joinlet_3827:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1185);
}

void* _M0IP48clawteam8clawteam8internal6openai15CompletionUsagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L9_2ax__750S1164
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1163;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2908;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2907;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1162;
  int32_t _M0L18completion__tokensS2898;
  void* _M0L6_2atmpS2897;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L8_24innerS1166;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L8_2afieldS3360;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L7_2abindS1167;
  void* _M0L6_2atmpS2899;
  double _M0L8_24innerS1171;
  void* _M0L8_2afieldS3359;
  void* _M0L7_2abindS1172;
  void* _M0L6_2atmpS2900;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L8_24innerS1176;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L8_2afieldS3357;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L7_2abindS1177;
  void* _M0L6_2atmpS2901;
  int32_t _M0L14prompt__tokensS2903;
  void* _M0L6_2atmpS2902;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L8_24innerS1181;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L8_2afieldS3356;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L7_2abindS1182;
  void* _M0L6_2atmpS2904;
  int32_t _M0L8_2afieldS3355;
  int32_t _M0L13total__tokensS2906;
  void* _M0L6_2atmpS2905;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L7_2abindS1163 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2908 = _M0L7_2abindS1163;
  _M0L6_2atmpS2907
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS2908
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_24mapS1162 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2907);
  _M0L18completion__tokensS2898 = _M0L9_2ax__750S1164->$0;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS2897
  = _M0IPC13int3IntPB6ToJson8to__json(_M0L18completion__tokensS2898);
  moonbit_incref(_M0L6_24mapS1162);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1162, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS2897);
  _M0L8_2afieldS3360 = _M0L9_2ax__750S1164->$1;
  _M0L7_2abindS1167 = _M0L8_2afieldS3360;
  if (_M0L7_2abindS1167 == 0) {
    
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L7_2aSomeS1168 =
      _M0L7_2abindS1167;
    struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L11_2a_24innerS1169 =
      _M0L7_2aSomeS1168;
    moonbit_incref(_M0L11_2a_24innerS1169);
    _M0L8_24innerS1166 = _M0L11_2a_24innerS1169;
    goto join_1165;
  }
  goto joinlet_3828;
  join_1165:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS2899
  = _M0IP48clawteam8clawteam8internal6openai23CompletionTokensDetailsPB6ToJson8to__json(_M0L8_24innerS1166);
  moonbit_incref(_M0L6_24mapS1162);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1162, (moonbit_string_t)moonbit_string_literal_39.data, _M0L6_2atmpS2899);
  joinlet_3828:;
  _M0L8_2afieldS3359 = _M0L9_2ax__750S1164->$2;
  _M0L7_2abindS1172 = _M0L8_2afieldS3359;
  switch (Moonbit_object_tag(_M0L7_2abindS1172)) {
    case 0: {
      moonbit_incref(_M0L7_2abindS1172);
      break;
    }
    default: {
      struct _M0DTPC16option6OptionGdE4Some* _M0L7_2aSomeS1173 =
        (struct _M0DTPC16option6OptionGdE4Some*)_M0L7_2abindS1172;
      double _M0L8_2afieldS3358 = _M0L7_2aSomeS1173->$0;
      double _M0L11_2a_24innerS1174 = _M0L8_2afieldS3358;
      _M0L8_24innerS1171 = _M0L11_2a_24innerS1174;
      goto join_1170;
      break;
    }
  }
  goto joinlet_3829;
  join_1170:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS2900
  = _M0IPC16double6DoublePB6ToJson8to__json(_M0L8_24innerS1171);
  moonbit_incref(_M0L6_24mapS1162);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1162, (moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS2900);
  joinlet_3829:;
  _M0L8_2afieldS3357 = _M0L9_2ax__750S1164->$3;
  _M0L7_2abindS1177 = _M0L8_2afieldS3357;
  if (_M0L7_2abindS1177 == 0) {
    
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L7_2aSomeS1178 =
      _M0L7_2abindS1177;
    struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L11_2a_24innerS1179 =
      _M0L7_2aSomeS1178;
    moonbit_incref(_M0L11_2a_24innerS1179);
    _M0L8_24innerS1176 = _M0L11_2a_24innerS1179;
    goto join_1175;
  }
  goto joinlet_3830;
  join_1175:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS2901
  = _M0IP48clawteam8clawteam8internal6openai11CostDetailsPB6ToJson8to__json(_M0L8_24innerS1176);
  moonbit_incref(_M0L6_24mapS1162);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1162, (moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS2901);
  joinlet_3830:;
  _M0L14prompt__tokensS2903 = _M0L9_2ax__750S1164->$4;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS2902
  = _M0IPC13int3IntPB6ToJson8to__json(_M0L14prompt__tokensS2903);
  moonbit_incref(_M0L6_24mapS1162);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1162, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS2902);
  _M0L8_2afieldS3356 = _M0L9_2ax__750S1164->$5;
  _M0L7_2abindS1182 = _M0L8_2afieldS3356;
  if (_M0L7_2abindS1182 == 0) {
    
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L7_2aSomeS1183 =
      _M0L7_2abindS1182;
    struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L11_2a_24innerS1184 =
      _M0L7_2aSomeS1183;
    moonbit_incref(_M0L11_2a_24innerS1184);
    _M0L8_24innerS1181 = _M0L11_2a_24innerS1184;
    goto join_1180;
  }
  goto joinlet_3831;
  join_1180:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS2904
  = _M0IP48clawteam8clawteam8internal6openai19PromptTokensDetailsPB6ToJson8to__json(_M0L8_24innerS1181);
  moonbit_incref(_M0L6_24mapS1162);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1162, (moonbit_string_t)moonbit_string_literal_42.data, _M0L6_2atmpS2904);
  joinlet_3831:;
  _M0L8_2afieldS3355 = _M0L9_2ax__750S1164->$6;
  moonbit_decref(_M0L9_2ax__750S1164);
  _M0L13total__tokensS2906 = _M0L8_2afieldS3355;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS2905
  = _M0IPC13int3IntPB6ToJson8to__json(_M0L13total__tokensS2906);
  moonbit_incref(_M0L6_24mapS1162);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1162, (moonbit_string_t)moonbit_string_literal_18.data, _M0L6_2atmpS2905);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1162);
}

void* _M0IP38clawteam8clawteam5clock9TimestampPB6ToJson8to__json(
  int64_t _M0L4selfS1161
) {
  double _M0L6_2atmpS2894;
  moonbit_string_t _M0L6_2atmpS2896;
  moonbit_string_t _M0L6_2atmpS2895;
  #line 68 "E:\\moonbit\\clawteam\\clock\\timestamp.mbt"
  _M0L6_2atmpS2894 = (double)_M0L4selfS1161;
  #line 69 "E:\\moonbit\\clawteam\\clock\\timestamp.mbt"
  _M0L6_2atmpS2896
  = _M0MPC15int645Int6418to__string_2einner(_M0L4selfS1161, 10);
  _M0L6_2atmpS2895 = _M0L6_2atmpS2896;
  #line 69 "E:\\moonbit\\clawteam\\clock\\timestamp.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2894, _M0L6_2atmpS2895);
}

int64_t _M0MP38clawteam8clawteam5clock9Timestamp8from__ms(
  int64_t _M0L2msS1160
) {
  #line 20 "E:\\moonbit\\clawteam\\clock\\timestamp.mbt"
  return _M0L2msS1160;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1155,
  void* _M0L7contentS1157,
  moonbit_string_t _M0L3locS1151,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1153
) {
  moonbit_string_t _M0L3locS1150;
  moonbit_string_t _M0L9args__locS1152;
  void* _M0L6_2atmpS2892;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2893;
  moonbit_string_t _M0L6actualS1154;
  moonbit_string_t _M0L4wantS1156;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1150 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1151);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1152 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1153);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2892 = _M0L3objS1155.$0->$method_0(_M0L3objS1155.$1);
  _M0L6_2atmpS2893 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1154
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2892, 0, 0, _M0L6_2atmpS2893);
  if (_M0L7contentS1157 == 0) {
    void* _M0L6_2atmpS2889;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2890;
    if (_M0L7contentS1157) {
      moonbit_decref(_M0L7contentS1157);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2889
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2890 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1156
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2889, 0, 0, _M0L6_2atmpS2890);
  } else {
    void* _M0L7_2aSomeS1158 = _M0L7contentS1157;
    void* _M0L4_2axS1159 = _M0L7_2aSomeS1158;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2891 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1156
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1159, 0, 0, _M0L6_2atmpS2891);
  }
  moonbit_incref(_M0L4wantS1156);
  moonbit_incref(_M0L6actualS1154);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1154, _M0L4wantS1156)
  ) {
    moonbit_string_t _M0L6_2atmpS2887;
    moonbit_string_t _M0L6_2atmpS3368;
    moonbit_string_t _M0L6_2atmpS2886;
    moonbit_string_t _M0L6_2atmpS3367;
    moonbit_string_t _M0L6_2atmpS2884;
    moonbit_string_t _M0L6_2atmpS2885;
    moonbit_string_t _M0L6_2atmpS3366;
    moonbit_string_t _M0L6_2atmpS2883;
    moonbit_string_t _M0L6_2atmpS3365;
    moonbit_string_t _M0L6_2atmpS2880;
    moonbit_string_t _M0L6_2atmpS2882;
    moonbit_string_t _M0L6_2atmpS2881;
    moonbit_string_t _M0L6_2atmpS3364;
    moonbit_string_t _M0L6_2atmpS2879;
    moonbit_string_t _M0L6_2atmpS3363;
    moonbit_string_t _M0L6_2atmpS2876;
    moonbit_string_t _M0L6_2atmpS2878;
    moonbit_string_t _M0L6_2atmpS2877;
    moonbit_string_t _M0L6_2atmpS3362;
    moonbit_string_t _M0L6_2atmpS2875;
    moonbit_string_t _M0L6_2atmpS3361;
    moonbit_string_t _M0L6_2atmpS2874;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2873;
    struct moonbit_result_0 _result_3832;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2887
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1150);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3368
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_43.data, _M0L6_2atmpS2887);
    moonbit_decref(_M0L6_2atmpS2887);
    _M0L6_2atmpS2886 = _M0L6_2atmpS3368;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3367
    = moonbit_add_string(_M0L6_2atmpS2886, (moonbit_string_t)moonbit_string_literal_44.data);
    moonbit_decref(_M0L6_2atmpS2886);
    _M0L6_2atmpS2884 = _M0L6_2atmpS3367;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2885
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1152);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3366 = moonbit_add_string(_M0L6_2atmpS2884, _M0L6_2atmpS2885);
    moonbit_decref(_M0L6_2atmpS2884);
    moonbit_decref(_M0L6_2atmpS2885);
    _M0L6_2atmpS2883 = _M0L6_2atmpS3366;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3365
    = moonbit_add_string(_M0L6_2atmpS2883, (moonbit_string_t)moonbit_string_literal_45.data);
    moonbit_decref(_M0L6_2atmpS2883);
    _M0L6_2atmpS2880 = _M0L6_2atmpS3365;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2882 = _M0MPC16string6String6escape(_M0L4wantS1156);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2881
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2882);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3364 = moonbit_add_string(_M0L6_2atmpS2880, _M0L6_2atmpS2881);
    moonbit_decref(_M0L6_2atmpS2880);
    moonbit_decref(_M0L6_2atmpS2881);
    _M0L6_2atmpS2879 = _M0L6_2atmpS3364;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3363
    = moonbit_add_string(_M0L6_2atmpS2879, (moonbit_string_t)moonbit_string_literal_46.data);
    moonbit_decref(_M0L6_2atmpS2879);
    _M0L6_2atmpS2876 = _M0L6_2atmpS3363;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2878 = _M0MPC16string6String6escape(_M0L6actualS1154);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2877
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2878);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3362 = moonbit_add_string(_M0L6_2atmpS2876, _M0L6_2atmpS2877);
    moonbit_decref(_M0L6_2atmpS2876);
    moonbit_decref(_M0L6_2atmpS2877);
    _M0L6_2atmpS2875 = _M0L6_2atmpS3362;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3361
    = moonbit_add_string(_M0L6_2atmpS2875, (moonbit_string_t)moonbit_string_literal_47.data);
    moonbit_decref(_M0L6_2atmpS2875);
    _M0L6_2atmpS2874 = _M0L6_2atmpS3361;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2873
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2873)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2873)->$0
    = _M0L6_2atmpS2874;
    _result_3832.tag = 0;
    _result_3832.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2873;
    return _result_3832;
  } else {
    int32_t _M0L6_2atmpS2888;
    struct moonbit_result_0 _result_3833;
    moonbit_decref(_M0L4wantS1156);
    moonbit_decref(_M0L6actualS1154);
    moonbit_decref(_M0L9args__locS1152);
    moonbit_decref(_M0L3locS1150);
    _M0L6_2atmpS2888 = 0;
    _result_3833.tag = 1;
    _result_3833.data.ok = _M0L6_2atmpS2888;
    return _result_3833;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1149,
  int32_t _M0L13escape__slashS1121,
  int32_t _M0L6indentS1116,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1142
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1108;
  void** _M0L6_2atmpS2872;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1109;
  int32_t _M0Lm5depthS1110;
  void* _M0L6_2atmpS2871;
  void* _M0L8_2aparamS1111;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1108 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2872 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1109
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1109)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1109->$0 = _M0L6_2atmpS2872;
  _M0L5stackS1109->$1 = 0;
  _M0Lm5depthS1110 = 0;
  _M0L6_2atmpS2871 = _M0L4selfS1149;
  _M0L8_2aparamS1111 = _M0L6_2atmpS2871;
  _2aloop_1127:;
  while (1) {
    if (_M0L8_2aparamS1111 == 0) {
      int32_t _M0L3lenS2833;
      if (_M0L8_2aparamS1111) {
        moonbit_decref(_M0L8_2aparamS1111);
      }
      _M0L3lenS2833 = _M0L5stackS1109->$1;
      if (_M0L3lenS2833 == 0) {
        if (_M0L8replacerS1142) {
          moonbit_decref(_M0L8replacerS1142);
        }
        moonbit_decref(_M0L5stackS1109);
        break;
      } else {
        void** _M0L8_2afieldS3376 = _M0L5stackS1109->$0;
        void** _M0L3bufS2857 = _M0L8_2afieldS3376;
        int32_t _M0L3lenS2859 = _M0L5stackS1109->$1;
        int32_t _M0L6_2atmpS2858 = _M0L3lenS2859 - 1;
        void* _M0L6_2atmpS3375 = (void*)_M0L3bufS2857[_M0L6_2atmpS2858];
        void* _M0L4_2axS1128 = _M0L6_2atmpS3375;
        switch (Moonbit_object_tag(_M0L4_2axS1128)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1129 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1128;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3371 =
              _M0L8_2aArrayS1129->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1130 =
              _M0L8_2afieldS3371;
            int32_t _M0L4_2aiS1131 = _M0L8_2aArrayS1129->$1;
            int32_t _M0L3lenS2845 = _M0L6_2aarrS1130->$1;
            if (_M0L4_2aiS1131 < _M0L3lenS2845) {
              int32_t _if__result_3835;
              void** _M0L8_2afieldS3370;
              void** _M0L3bufS2851;
              void* _M0L6_2atmpS3369;
              void* _M0L7elementS1132;
              int32_t _M0L6_2atmpS2846;
              void* _M0L6_2atmpS2849;
              if (_M0L4_2aiS1131 < 0) {
                _if__result_3835 = 1;
              } else {
                int32_t _M0L3lenS2850 = _M0L6_2aarrS1130->$1;
                _if__result_3835 = _M0L4_2aiS1131 >= _M0L3lenS2850;
              }
              if (_if__result_3835) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3370 = _M0L6_2aarrS1130->$0;
              _M0L3bufS2851 = _M0L8_2afieldS3370;
              _M0L6_2atmpS3369 = (void*)_M0L3bufS2851[_M0L4_2aiS1131];
              _M0L7elementS1132 = _M0L6_2atmpS3369;
              _M0L6_2atmpS2846 = _M0L4_2aiS1131 + 1;
              _M0L8_2aArrayS1129->$1 = _M0L6_2atmpS2846;
              if (_M0L4_2aiS1131 > 0) {
                int32_t _M0L6_2atmpS2848;
                moonbit_string_t _M0L6_2atmpS2847;
                moonbit_incref(_M0L7elementS1132);
                moonbit_incref(_M0L3bufS1108);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 44);
                _M0L6_2atmpS2848 = _M0Lm5depthS1110;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2847
                = _M0FPC14json11indent__str(_M0L6_2atmpS2848, _M0L6indentS1116);
                moonbit_incref(_M0L3bufS1108);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2847);
              } else {
                moonbit_incref(_M0L7elementS1132);
              }
              _M0L6_2atmpS2849 = _M0L7elementS1132;
              _M0L8_2aparamS1111 = _M0L6_2atmpS2849;
              goto _2aloop_1127;
            } else {
              int32_t _M0L6_2atmpS2852 = _M0Lm5depthS1110;
              void* _M0L6_2atmpS2853;
              int32_t _M0L6_2atmpS2855;
              moonbit_string_t _M0L6_2atmpS2854;
              void* _M0L6_2atmpS2856;
              _M0Lm5depthS1110 = _M0L6_2atmpS2852 - 1;
              moonbit_incref(_M0L5stackS1109);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2853
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1109);
              if (_M0L6_2atmpS2853) {
                moonbit_decref(_M0L6_2atmpS2853);
              }
              _M0L6_2atmpS2855 = _M0Lm5depthS1110;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2854
              = _M0FPC14json11indent__str(_M0L6_2atmpS2855, _M0L6indentS1116);
              moonbit_incref(_M0L3bufS1108);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2854);
              moonbit_incref(_M0L3bufS1108);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 93);
              _M0L6_2atmpS2856 = 0;
              _M0L8_2aparamS1111 = _M0L6_2atmpS2856;
              goto _2aloop_1127;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1133 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1128;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3374 =
              _M0L9_2aObjectS1133->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1134 =
              _M0L8_2afieldS3374;
            int32_t _M0L8_2afirstS1135 = _M0L9_2aObjectS1133->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1136;
            moonbit_incref(_M0L11_2aiteratorS1134);
            moonbit_incref(_M0L9_2aObjectS1133);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1136
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1134);
            if (_M0L7_2abindS1136 == 0) {
              int32_t _M0L6_2atmpS2834;
              void* _M0L6_2atmpS2835;
              int32_t _M0L6_2atmpS2837;
              moonbit_string_t _M0L6_2atmpS2836;
              void* _M0L6_2atmpS2838;
              if (_M0L7_2abindS1136) {
                moonbit_decref(_M0L7_2abindS1136);
              }
              moonbit_decref(_M0L9_2aObjectS1133);
              _M0L6_2atmpS2834 = _M0Lm5depthS1110;
              _M0Lm5depthS1110 = _M0L6_2atmpS2834 - 1;
              moonbit_incref(_M0L5stackS1109);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2835
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1109);
              if (_M0L6_2atmpS2835) {
                moonbit_decref(_M0L6_2atmpS2835);
              }
              _M0L6_2atmpS2837 = _M0Lm5depthS1110;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2836
              = _M0FPC14json11indent__str(_M0L6_2atmpS2837, _M0L6indentS1116);
              moonbit_incref(_M0L3bufS1108);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2836);
              moonbit_incref(_M0L3bufS1108);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 125);
              _M0L6_2atmpS2838 = 0;
              _M0L8_2aparamS1111 = _M0L6_2atmpS2838;
              goto _2aloop_1127;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1137 = _M0L7_2abindS1136;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1138 = _M0L7_2aSomeS1137;
              moonbit_string_t _M0L8_2afieldS3373 = _M0L4_2axS1138->$0;
              moonbit_string_t _M0L4_2akS1139 = _M0L8_2afieldS3373;
              void* _M0L8_2afieldS3372 = _M0L4_2axS1138->$1;
              int32_t _M0L6_2acntS3685 =
                Moonbit_object_header(_M0L4_2axS1138)->rc;
              void* _M0L4_2avS1140;
              void* _M0Lm2v2S1141;
              moonbit_string_t _M0L6_2atmpS2842;
              void* _M0L6_2atmpS2844;
              void* _M0L6_2atmpS2843;
              if (_M0L6_2acntS3685 > 1) {
                int32_t _M0L11_2anew__cntS3686 = _M0L6_2acntS3685 - 1;
                Moonbit_object_header(_M0L4_2axS1138)->rc
                = _M0L11_2anew__cntS3686;
                moonbit_incref(_M0L8_2afieldS3372);
                moonbit_incref(_M0L4_2akS1139);
              } else if (_M0L6_2acntS3685 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1138);
              }
              _M0L4_2avS1140 = _M0L8_2afieldS3372;
              _M0Lm2v2S1141 = _M0L4_2avS1140;
              if (_M0L8replacerS1142 == 0) {
                moonbit_incref(_M0Lm2v2S1141);
                moonbit_decref(_M0L4_2avS1140);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1143 =
                  _M0L8replacerS1142;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1144 =
                  _M0L7_2aSomeS1143;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1145 =
                  _M0L11_2areplacerS1144;
                void* _M0L7_2abindS1146;
                moonbit_incref(_M0L7_2afuncS1145);
                moonbit_incref(_M0L4_2akS1139);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1146
                = _M0L7_2afuncS1145->code(_M0L7_2afuncS1145, _M0L4_2akS1139, _M0L4_2avS1140);
                if (_M0L7_2abindS1146 == 0) {
                  void* _M0L6_2atmpS2839;
                  if (_M0L7_2abindS1146) {
                    moonbit_decref(_M0L7_2abindS1146);
                  }
                  moonbit_decref(_M0L4_2akS1139);
                  moonbit_decref(_M0L9_2aObjectS1133);
                  _M0L6_2atmpS2839 = 0;
                  _M0L8_2aparamS1111 = _M0L6_2atmpS2839;
                  goto _2aloop_1127;
                } else {
                  void* _M0L7_2aSomeS1147 = _M0L7_2abindS1146;
                  void* _M0L4_2avS1148 = _M0L7_2aSomeS1147;
                  _M0Lm2v2S1141 = _M0L4_2avS1148;
                }
              }
              if (!_M0L8_2afirstS1135) {
                int32_t _M0L6_2atmpS2841;
                moonbit_string_t _M0L6_2atmpS2840;
                moonbit_incref(_M0L3bufS1108);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 44);
                _M0L6_2atmpS2841 = _M0Lm5depthS1110;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2840
                = _M0FPC14json11indent__str(_M0L6_2atmpS2841, _M0L6indentS1116);
                moonbit_incref(_M0L3bufS1108);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2840);
              }
              moonbit_incref(_M0L3bufS1108);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2842
              = _M0FPC14json6escape(_M0L4_2akS1139, _M0L13escape__slashS1121);
              moonbit_incref(_M0L3bufS1108);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2842);
              moonbit_incref(_M0L3bufS1108);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 34);
              moonbit_incref(_M0L3bufS1108);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 58);
              if (_M0L6indentS1116 > 0) {
                moonbit_incref(_M0L3bufS1108);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 32);
              }
              _M0L9_2aObjectS1133->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1133);
              _M0L6_2atmpS2844 = _M0Lm2v2S1141;
              _M0L6_2atmpS2843 = _M0L6_2atmpS2844;
              _M0L8_2aparamS1111 = _M0L6_2atmpS2843;
              goto _2aloop_1127;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1112 = _M0L8_2aparamS1111;
      void* _M0L8_2avalueS1113 = _M0L7_2aSomeS1112;
      void* _M0L6_2atmpS2870;
      switch (Moonbit_object_tag(_M0L8_2avalueS1113)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1114 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1113;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3377 =
            _M0L9_2aObjectS1114->$0;
          int32_t _M0L6_2acntS3687 =
            Moonbit_object_header(_M0L9_2aObjectS1114)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1115;
          if (_M0L6_2acntS3687 > 1) {
            int32_t _M0L11_2anew__cntS3688 = _M0L6_2acntS3687 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1114)->rc
            = _M0L11_2anew__cntS3688;
            moonbit_incref(_M0L8_2afieldS3377);
          } else if (_M0L6_2acntS3687 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1114);
          }
          _M0L10_2amembersS1115 = _M0L8_2afieldS3377;
          moonbit_incref(_M0L10_2amembersS1115);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1115)) {
            moonbit_decref(_M0L10_2amembersS1115);
            moonbit_incref(_M0L3bufS1108);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, (moonbit_string_t)moonbit_string_literal_48.data);
          } else {
            int32_t _M0L6_2atmpS2865 = _M0Lm5depthS1110;
            int32_t _M0L6_2atmpS2867;
            moonbit_string_t _M0L6_2atmpS2866;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2869;
            void* _M0L6ObjectS2868;
            _M0Lm5depthS1110 = _M0L6_2atmpS2865 + 1;
            moonbit_incref(_M0L3bufS1108);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 123);
            _M0L6_2atmpS2867 = _M0Lm5depthS1110;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2866
            = _M0FPC14json11indent__str(_M0L6_2atmpS2867, _M0L6indentS1116);
            moonbit_incref(_M0L3bufS1108);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2866);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2869
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1115);
            _M0L6ObjectS2868
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2868)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2868)->$0
            = _M0L6_2atmpS2869;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2868)->$1
            = 1;
            moonbit_incref(_M0L5stackS1109);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1109, _M0L6ObjectS2868);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1117 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1113;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3378 =
            _M0L8_2aArrayS1117->$0;
          int32_t _M0L6_2acntS3689 =
            Moonbit_object_header(_M0L8_2aArrayS1117)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1118;
          if (_M0L6_2acntS3689 > 1) {
            int32_t _M0L11_2anew__cntS3690 = _M0L6_2acntS3689 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1117)->rc
            = _M0L11_2anew__cntS3690;
            moonbit_incref(_M0L8_2afieldS3378);
          } else if (_M0L6_2acntS3689 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1117);
          }
          _M0L6_2aarrS1118 = _M0L8_2afieldS3378;
          moonbit_incref(_M0L6_2aarrS1118);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1118)) {
            moonbit_decref(_M0L6_2aarrS1118);
            moonbit_incref(_M0L3bufS1108);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, (moonbit_string_t)moonbit_string_literal_49.data);
          } else {
            int32_t _M0L6_2atmpS2861 = _M0Lm5depthS1110;
            int32_t _M0L6_2atmpS2863;
            moonbit_string_t _M0L6_2atmpS2862;
            void* _M0L5ArrayS2864;
            _M0Lm5depthS1110 = _M0L6_2atmpS2861 + 1;
            moonbit_incref(_M0L3bufS1108);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 91);
            _M0L6_2atmpS2863 = _M0Lm5depthS1110;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2862
            = _M0FPC14json11indent__str(_M0L6_2atmpS2863, _M0L6indentS1116);
            moonbit_incref(_M0L3bufS1108);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2862);
            _M0L5ArrayS2864
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2864)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2864)->$0
            = _M0L6_2aarrS1118;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2864)->$1
            = 0;
            moonbit_incref(_M0L5stackS1109);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1109, _M0L5ArrayS2864);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1119 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1113;
          moonbit_string_t _M0L8_2afieldS3379 = _M0L9_2aStringS1119->$0;
          int32_t _M0L6_2acntS3691 =
            Moonbit_object_header(_M0L9_2aStringS1119)->rc;
          moonbit_string_t _M0L4_2asS1120;
          moonbit_string_t _M0L6_2atmpS2860;
          if (_M0L6_2acntS3691 > 1) {
            int32_t _M0L11_2anew__cntS3692 = _M0L6_2acntS3691 - 1;
            Moonbit_object_header(_M0L9_2aStringS1119)->rc
            = _M0L11_2anew__cntS3692;
            moonbit_incref(_M0L8_2afieldS3379);
          } else if (_M0L6_2acntS3691 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1119);
          }
          _M0L4_2asS1120 = _M0L8_2afieldS3379;
          moonbit_incref(_M0L3bufS1108);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2860
          = _M0FPC14json6escape(_M0L4_2asS1120, _M0L13escape__slashS1121);
          moonbit_incref(_M0L3bufS1108);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L6_2atmpS2860);
          moonbit_incref(_M0L3bufS1108);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1108, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1122 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1113;
          double _M0L4_2anS1123 = _M0L9_2aNumberS1122->$0;
          moonbit_string_t _M0L8_2afieldS3380 = _M0L9_2aNumberS1122->$1;
          int32_t _M0L6_2acntS3693 =
            Moonbit_object_header(_M0L9_2aNumberS1122)->rc;
          moonbit_string_t _M0L7_2areprS1124;
          if (_M0L6_2acntS3693 > 1) {
            int32_t _M0L11_2anew__cntS3694 = _M0L6_2acntS3693 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1122)->rc
            = _M0L11_2anew__cntS3694;
            if (_M0L8_2afieldS3380) {
              moonbit_incref(_M0L8_2afieldS3380);
            }
          } else if (_M0L6_2acntS3693 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1122);
          }
          _M0L7_2areprS1124 = _M0L8_2afieldS3380;
          if (_M0L7_2areprS1124 == 0) {
            if (_M0L7_2areprS1124) {
              moonbit_decref(_M0L7_2areprS1124);
            }
            moonbit_incref(_M0L3bufS1108);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1108, _M0L4_2anS1123);
          } else {
            moonbit_string_t _M0L7_2aSomeS1125 = _M0L7_2areprS1124;
            moonbit_string_t _M0L4_2arS1126 = _M0L7_2aSomeS1125;
            moonbit_incref(_M0L3bufS1108);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, _M0L4_2arS1126);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1108);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, (moonbit_string_t)moonbit_string_literal_50.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1108);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, (moonbit_string_t)moonbit_string_literal_51.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1113);
          moonbit_incref(_M0L3bufS1108);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1108, (moonbit_string_t)moonbit_string_literal_52.data);
          break;
        }
      }
      _M0L6_2atmpS2870 = 0;
      _M0L8_2aparamS1111 = _M0L6_2atmpS2870;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1108);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1107,
  int32_t _M0L6indentS1105
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1105 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1106 = _M0L6indentS1105 * _M0L5levelS1107;
    switch (_M0L6spacesS1106) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_53.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_54.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_55.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_56.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_57.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_58.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_59.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_60.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_61.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2832;
        moonbit_string_t _M0L6_2atmpS3381;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2832
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_62.data, _M0L6spacesS1106);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3381
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_53.data, _M0L6_2atmpS2832);
        moonbit_decref(_M0L6_2atmpS2832);
        return _M0L6_2atmpS3381;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1097,
  int32_t _M0L13escape__slashS1102
) {
  int32_t _M0L6_2atmpS2831;
  struct _M0TPB13StringBuilder* _M0L3bufS1096;
  struct _M0TWEOc* _M0L5_2aitS1098;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2831 = Moonbit_array_length(_M0L3strS1097);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1096 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2831);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1098 = _M0MPC16string6String4iter(_M0L3strS1097);
  while (1) {
    int32_t _M0L7_2abindS1099;
    moonbit_incref(_M0L5_2aitS1098);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1099 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1098);
    if (_M0L7_2abindS1099 == -1) {
      moonbit_decref(_M0L5_2aitS1098);
    } else {
      int32_t _M0L7_2aSomeS1100 = _M0L7_2abindS1099;
      int32_t _M0L4_2acS1101 = _M0L7_2aSomeS1100;
      if (_M0L4_2acS1101 == 34) {
        moonbit_incref(_M0L3bufS1096);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_63.data);
      } else if (_M0L4_2acS1101 == 92) {
        moonbit_incref(_M0L3bufS1096);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_64.data);
      } else if (_M0L4_2acS1101 == 47) {
        if (_M0L13escape__slashS1102) {
          moonbit_incref(_M0L3bufS1096);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_65.data);
        } else {
          moonbit_incref(_M0L3bufS1096);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1096, _M0L4_2acS1101);
        }
      } else if (_M0L4_2acS1101 == 10) {
        moonbit_incref(_M0L3bufS1096);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_66.data);
      } else if (_M0L4_2acS1101 == 13) {
        moonbit_incref(_M0L3bufS1096);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_67.data);
      } else if (_M0L4_2acS1101 == 8) {
        moonbit_incref(_M0L3bufS1096);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_68.data);
      } else if (_M0L4_2acS1101 == 9) {
        moonbit_incref(_M0L3bufS1096);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_69.data);
      } else {
        int32_t _M0L4codeS1103 = _M0L4_2acS1101;
        if (_M0L4codeS1103 == 12) {
          moonbit_incref(_M0L3bufS1096);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_70.data);
        } else if (_M0L4codeS1103 < 32) {
          int32_t _M0L6_2atmpS2830;
          moonbit_string_t _M0L6_2atmpS2829;
          moonbit_incref(_M0L3bufS1096);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, (moonbit_string_t)moonbit_string_literal_71.data);
          _M0L6_2atmpS2830 = _M0L4codeS1103 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2829 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2830);
          moonbit_incref(_M0L3bufS1096);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1096, _M0L6_2atmpS2829);
        } else {
          moonbit_incref(_M0L3bufS1096);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1096, _M0L4_2acS1101);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1096);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1094
) {
  int32_t _M0L8_2afieldS3382;
  int32_t _M0L3lenS2827;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3382 = _M0L4selfS1094->$1;
  moonbit_decref(_M0L4selfS1094);
  _M0L3lenS2827 = _M0L8_2afieldS3382;
  return _M0L3lenS2827 == 0;
}

int32_t _M0MPC15array5Array9is__emptyGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS1095
) {
  int32_t _M0L8_2afieldS3383;
  int32_t _M0L3lenS2828;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3383 = _M0L4selfS1095->$1;
  moonbit_decref(_M0L4selfS1095);
  _M0L3lenS2828 = _M0L8_2afieldS3383;
  return _M0L3lenS2828 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1091
) {
  int32_t _M0L3lenS1090;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1090 = _M0L4selfS1091->$1;
  if (_M0L3lenS1090 == 0) {
    moonbit_decref(_M0L4selfS1091);
    return 0;
  } else {
    int32_t _M0L5indexS1092 = _M0L3lenS1090 - 1;
    void** _M0L8_2afieldS3387 = _M0L4selfS1091->$0;
    void** _M0L3bufS2826 = _M0L8_2afieldS3387;
    void* _M0L6_2atmpS3386 = (void*)_M0L3bufS2826[_M0L5indexS1092];
    void* _M0L1vS1093 = _M0L6_2atmpS3386;
    void** _M0L8_2afieldS3385 = _M0L4selfS1091->$0;
    void** _M0L3bufS2825 = _M0L8_2afieldS3385;
    void* _M0L6_2aoldS3384;
    if (
      _M0L5indexS1092 < 0
      || _M0L5indexS1092 >= Moonbit_array_length(_M0L3bufS2825)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3384 = (void*)_M0L3bufS2825[_M0L5indexS1092];
    moonbit_incref(_M0L1vS1093);
    moonbit_decref(_M0L6_2aoldS3384);
    if (
      _M0L5indexS1092 < 0
      || _M0L5indexS1092 >= Moonbit_array_length(_M0L3bufS2825)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2825[_M0L5indexS1092]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1091->$1 = _M0L5indexS1092;
    moonbit_decref(_M0L4selfS1091);
    return _M0L1vS1093;
  }
}

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS1081,
  int32_t _M0L5startS1087,
  int64_t _M0L3endS1083
) {
  int32_t _M0L3lenS1080;
  int32_t _M0L3endS1082;
  int32_t _M0L5startS1086;
  int32_t _if__result_3837;
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3lenS1080 = _M0L4selfS1081->$1;
  if (_M0L3endS1083 == 4294967296ll) {
    _M0L3endS1082 = _M0L3lenS1080;
  } else {
    int64_t _M0L7_2aSomeS1084 = _M0L3endS1083;
    int32_t _M0L6_2aendS1085 = (int32_t)_M0L7_2aSomeS1084;
    if (_M0L6_2aendS1085 < 0) {
      _M0L3endS1082 = _M0L3lenS1080 + _M0L6_2aendS1085;
    } else {
      _M0L3endS1082 = _M0L6_2aendS1085;
    }
  }
  if (_M0L5startS1087 < 0) {
    _M0L5startS1086 = _M0L3lenS1080 + _M0L5startS1087;
  } else {
    _M0L5startS1086 = _M0L5startS1087;
  }
  if (_M0L5startS1086 >= 0) {
    if (_M0L5startS1086 <= _M0L3endS1082) {
      _if__result_3837 = _M0L3endS1082 <= _M0L3lenS1080;
    } else {
      _if__result_3837 = 0;
    }
  } else {
    _if__result_3837 = 0;
  }
  if (_if__result_3837) {
    void** _M0L7_2abindS1088;
    int32_t _M0L7_2abindS1089;
    int32_t _M0L6_2atmpS2824;
    #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L7_2abindS1088
    = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS1081);
    _M0L7_2abindS1089 = _M0L3endS1082 - _M0L5startS1086;
    _M0L6_2atmpS2824 = _M0L5startS1086 + _M0L7_2abindS1089;
    return (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE){_M0L5startS1086,
                                                                    _M0L6_2atmpS2824,
                                                                    _M0L7_2abindS1088};
  } else {
    moonbit_decref(_M0L4selfS1081);
    #line 263 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE((moonbit_string_t)moonbit_string_literal_72.data, (moonbit_string_t)moonbit_string_literal_73.data);
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1078,
  struct _M0TPB6Logger _M0L6loggerS1079
) {
  moonbit_string_t _M0L6_2atmpS2823;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2822;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2823 = _M0L4selfS1078;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2822 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2823);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2822, _M0L6loggerS1079);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1055,
  struct _M0TPB6Logger _M0L6loggerS1077
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3396;
  struct _M0TPC16string10StringView _M0L3pkgS1054;
  moonbit_string_t _M0L7_2adataS1056;
  int32_t _M0L8_2astartS1057;
  int32_t _M0L6_2atmpS2821;
  int32_t _M0L6_2aendS1058;
  int32_t _M0Lm9_2acursorS1059;
  int32_t _M0Lm13accept__stateS1060;
  int32_t _M0Lm10match__endS1061;
  int32_t _M0Lm20match__tag__saver__0S1062;
  int32_t _M0Lm6tag__0S1063;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1064;
  struct _M0TPC16string10StringView _M0L8_2afieldS3395;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1073;
  void* _M0L8_2afieldS3394;
  int32_t _M0L6_2acntS3695;
  void* _M0L16_2apackage__nameS1074;
  struct _M0TPC16string10StringView _M0L8_2afieldS3392;
  struct _M0TPC16string10StringView _M0L8filenameS2798;
  struct _M0TPC16string10StringView _M0L8_2afieldS3391;
  struct _M0TPC16string10StringView _M0L11start__lineS2799;
  struct _M0TPC16string10StringView _M0L8_2afieldS3390;
  struct _M0TPC16string10StringView _M0L13start__columnS2800;
  struct _M0TPC16string10StringView _M0L8_2afieldS3389;
  struct _M0TPC16string10StringView _M0L9end__lineS2801;
  struct _M0TPC16string10StringView _M0L8_2afieldS3388;
  int32_t _M0L6_2acntS3699;
  struct _M0TPC16string10StringView _M0L11end__columnS2802;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3396
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1055->$0_1, _M0L4selfS1055->$0_2, _M0L4selfS1055->$0_0
  };
  _M0L3pkgS1054 = _M0L8_2afieldS3396;
  moonbit_incref(_M0L3pkgS1054.$0);
  moonbit_incref(_M0L3pkgS1054.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1056 = _M0MPC16string10StringView4data(_M0L3pkgS1054);
  moonbit_incref(_M0L3pkgS1054.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1057
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1054);
  moonbit_incref(_M0L3pkgS1054.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2821 = _M0MPC16string10StringView6length(_M0L3pkgS1054);
  _M0L6_2aendS1058 = _M0L8_2astartS1057 + _M0L6_2atmpS2821;
  _M0Lm9_2acursorS1059 = _M0L8_2astartS1057;
  _M0Lm13accept__stateS1060 = -1;
  _M0Lm10match__endS1061 = -1;
  _M0Lm20match__tag__saver__0S1062 = -1;
  _M0Lm6tag__0S1063 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2813 = _M0Lm9_2acursorS1059;
    if (_M0L6_2atmpS2813 < _M0L6_2aendS1058) {
      int32_t _M0L6_2atmpS2820 = _M0Lm9_2acursorS1059;
      int32_t _M0L10next__charS1068;
      int32_t _M0L6_2atmpS2814;
      moonbit_incref(_M0L7_2adataS1056);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1068
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1056, _M0L6_2atmpS2820);
      _M0L6_2atmpS2814 = _M0Lm9_2acursorS1059;
      _M0Lm9_2acursorS1059 = _M0L6_2atmpS2814 + 1;
      if (_M0L10next__charS1068 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2815;
          _M0Lm6tag__0S1063 = _M0Lm9_2acursorS1059;
          _M0L6_2atmpS2815 = _M0Lm9_2acursorS1059;
          if (_M0L6_2atmpS2815 < _M0L6_2aendS1058) {
            int32_t _M0L6_2atmpS2819 = _M0Lm9_2acursorS1059;
            int32_t _M0L10next__charS1069;
            int32_t _M0L6_2atmpS2816;
            moonbit_incref(_M0L7_2adataS1056);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1069
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1056, _M0L6_2atmpS2819);
            _M0L6_2atmpS2816 = _M0Lm9_2acursorS1059;
            _M0Lm9_2acursorS1059 = _M0L6_2atmpS2816 + 1;
            if (_M0L10next__charS1069 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2817 = _M0Lm9_2acursorS1059;
                if (_M0L6_2atmpS2817 < _M0L6_2aendS1058) {
                  int32_t _M0L6_2atmpS2818 = _M0Lm9_2acursorS1059;
                  _M0Lm9_2acursorS1059 = _M0L6_2atmpS2818 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1062 = _M0Lm6tag__0S1063;
                  _M0Lm13accept__stateS1060 = 0;
                  _M0Lm10match__endS1061 = _M0Lm9_2acursorS1059;
                  goto join_1065;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1065;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1065;
    }
    break;
  }
  goto joinlet_3838;
  join_1065:;
  switch (_M0Lm13accept__stateS1060) {
    case 0: {
      int32_t _M0L6_2atmpS2811;
      int32_t _M0L6_2atmpS2810;
      int64_t _M0L6_2atmpS2807;
      int32_t _M0L6_2atmpS2809;
      int64_t _M0L6_2atmpS2808;
      struct _M0TPC16string10StringView _M0L13package__nameS1066;
      int64_t _M0L6_2atmpS2804;
      int32_t _M0L6_2atmpS2806;
      int64_t _M0L6_2atmpS2805;
      struct _M0TPC16string10StringView _M0L12module__nameS1067;
      void* _M0L4SomeS2803;
      moonbit_decref(_M0L3pkgS1054.$0);
      _M0L6_2atmpS2811 = _M0Lm20match__tag__saver__0S1062;
      _M0L6_2atmpS2810 = _M0L6_2atmpS2811 + 1;
      _M0L6_2atmpS2807 = (int64_t)_M0L6_2atmpS2810;
      _M0L6_2atmpS2809 = _M0Lm10match__endS1061;
      _M0L6_2atmpS2808 = (int64_t)_M0L6_2atmpS2809;
      moonbit_incref(_M0L7_2adataS1056);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1066
      = _M0MPC16string6String4view(_M0L7_2adataS1056, _M0L6_2atmpS2807, _M0L6_2atmpS2808);
      _M0L6_2atmpS2804 = (int64_t)_M0L8_2astartS1057;
      _M0L6_2atmpS2806 = _M0Lm20match__tag__saver__0S1062;
      _M0L6_2atmpS2805 = (int64_t)_M0L6_2atmpS2806;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1067
      = _M0MPC16string6String4view(_M0L7_2adataS1056, _M0L6_2atmpS2804, _M0L6_2atmpS2805);
      _M0L4SomeS2803
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2803)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2803)->$0_0
      = _M0L13package__nameS1066.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2803)->$0_1
      = _M0L13package__nameS1066.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2803)->$0_2
      = _M0L13package__nameS1066.$2;
      _M0L7_2abindS1064
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1064)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1064->$0_0 = _M0L12module__nameS1067.$0;
      _M0L7_2abindS1064->$0_1 = _M0L12module__nameS1067.$1;
      _M0L7_2abindS1064->$0_2 = _M0L12module__nameS1067.$2;
      _M0L7_2abindS1064->$1 = _M0L4SomeS2803;
      break;
    }
    default: {
      void* _M0L4NoneS2812;
      moonbit_decref(_M0L7_2adataS1056);
      _M0L4NoneS2812
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1064
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1064)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1064->$0_0 = _M0L3pkgS1054.$0;
      _M0L7_2abindS1064->$0_1 = _M0L3pkgS1054.$1;
      _M0L7_2abindS1064->$0_2 = _M0L3pkgS1054.$2;
      _M0L7_2abindS1064->$1 = _M0L4NoneS2812;
      break;
    }
  }
  joinlet_3838:;
  _M0L8_2afieldS3395
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1064->$0_1, _M0L7_2abindS1064->$0_2, _M0L7_2abindS1064->$0_0
  };
  _M0L15_2amodule__nameS1073 = _M0L8_2afieldS3395;
  _M0L8_2afieldS3394 = _M0L7_2abindS1064->$1;
  _M0L6_2acntS3695 = Moonbit_object_header(_M0L7_2abindS1064)->rc;
  if (_M0L6_2acntS3695 > 1) {
    int32_t _M0L11_2anew__cntS3696 = _M0L6_2acntS3695 - 1;
    Moonbit_object_header(_M0L7_2abindS1064)->rc = _M0L11_2anew__cntS3696;
    moonbit_incref(_M0L8_2afieldS3394);
    moonbit_incref(_M0L15_2amodule__nameS1073.$0);
  } else if (_M0L6_2acntS3695 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1064);
  }
  _M0L16_2apackage__nameS1074 = _M0L8_2afieldS3394;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1074)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1075 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1074;
      struct _M0TPC16string10StringView _M0L8_2afieldS3393 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1075->$0_1,
                                              _M0L7_2aSomeS1075->$0_2,
                                              _M0L7_2aSomeS1075->$0_0};
      int32_t _M0L6_2acntS3697 = Moonbit_object_header(_M0L7_2aSomeS1075)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1076;
      if (_M0L6_2acntS3697 > 1) {
        int32_t _M0L11_2anew__cntS3698 = _M0L6_2acntS3697 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1075)->rc = _M0L11_2anew__cntS3698;
        moonbit_incref(_M0L8_2afieldS3393.$0);
      } else if (_M0L6_2acntS3697 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1075);
      }
      _M0L12_2apkg__nameS1076 = _M0L8_2afieldS3393;
      if (_M0L6loggerS1077.$1) {
        moonbit_incref(_M0L6loggerS1077.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1077.$0->$method_2(_M0L6loggerS1077.$1, _M0L12_2apkg__nameS1076);
      if (_M0L6loggerS1077.$1) {
        moonbit_incref(_M0L6loggerS1077.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1077.$0->$method_3(_M0L6loggerS1077.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1074);
      break;
    }
  }
  _M0L8_2afieldS3392
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1055->$1_1, _M0L4selfS1055->$1_2, _M0L4selfS1055->$1_0
  };
  _M0L8filenameS2798 = _M0L8_2afieldS3392;
  moonbit_incref(_M0L8filenameS2798.$0);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_2(_M0L6loggerS1077.$1, _M0L8filenameS2798);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_3(_M0L6loggerS1077.$1, 58);
  _M0L8_2afieldS3391
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1055->$2_1, _M0L4selfS1055->$2_2, _M0L4selfS1055->$2_0
  };
  _M0L11start__lineS2799 = _M0L8_2afieldS3391;
  moonbit_incref(_M0L11start__lineS2799.$0);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_2(_M0L6loggerS1077.$1, _M0L11start__lineS2799);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_3(_M0L6loggerS1077.$1, 58);
  _M0L8_2afieldS3390
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1055->$3_1, _M0L4selfS1055->$3_2, _M0L4selfS1055->$3_0
  };
  _M0L13start__columnS2800 = _M0L8_2afieldS3390;
  moonbit_incref(_M0L13start__columnS2800.$0);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_2(_M0L6loggerS1077.$1, _M0L13start__columnS2800);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_3(_M0L6loggerS1077.$1, 45);
  _M0L8_2afieldS3389
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1055->$4_1, _M0L4selfS1055->$4_2, _M0L4selfS1055->$4_0
  };
  _M0L9end__lineS2801 = _M0L8_2afieldS3389;
  moonbit_incref(_M0L9end__lineS2801.$0);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_2(_M0L6loggerS1077.$1, _M0L9end__lineS2801);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_3(_M0L6loggerS1077.$1, 58);
  _M0L8_2afieldS3388
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1055->$5_1, _M0L4selfS1055->$5_2, _M0L4selfS1055->$5_0
  };
  _M0L6_2acntS3699 = Moonbit_object_header(_M0L4selfS1055)->rc;
  if (_M0L6_2acntS3699 > 1) {
    int32_t _M0L11_2anew__cntS3705 = _M0L6_2acntS3699 - 1;
    Moonbit_object_header(_M0L4selfS1055)->rc = _M0L11_2anew__cntS3705;
    moonbit_incref(_M0L8_2afieldS3388.$0);
  } else if (_M0L6_2acntS3699 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3704 =
      (struct _M0TPC16string10StringView){_M0L4selfS1055->$4_1,
                                            _M0L4selfS1055->$4_2,
                                            _M0L4selfS1055->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3703;
    struct _M0TPC16string10StringView _M0L8_2afieldS3702;
    struct _M0TPC16string10StringView _M0L8_2afieldS3701;
    struct _M0TPC16string10StringView _M0L8_2afieldS3700;
    moonbit_decref(_M0L8_2afieldS3704.$0);
    _M0L8_2afieldS3703
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1055->$3_1, _M0L4selfS1055->$3_2, _M0L4selfS1055->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3703.$0);
    _M0L8_2afieldS3702
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1055->$2_1, _M0L4selfS1055->$2_2, _M0L4selfS1055->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3702.$0);
    _M0L8_2afieldS3701
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1055->$1_1, _M0L4selfS1055->$1_2, _M0L4selfS1055->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3701.$0);
    _M0L8_2afieldS3700
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1055->$0_1, _M0L4selfS1055->$0_2, _M0L4selfS1055->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3700.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1055);
  }
  _M0L11end__columnS2802 = _M0L8_2afieldS3388;
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_2(_M0L6loggerS1077.$1, _M0L11end__columnS2802);
  if (_M0L6loggerS1077.$1) {
    moonbit_incref(_M0L6loggerS1077.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_3(_M0L6loggerS1077.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1077.$0->$method_2(_M0L6loggerS1077.$1, _M0L15_2amodule__nameS1073);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1053) {
  moonbit_string_t _M0L6_2atmpS2797;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2797
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1053);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2797);
  moonbit_decref(_M0L6_2atmpS2797);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1052,
  struct _M0TPB6Logger _M0L6loggerS1051
) {
  moonbit_string_t _M0L6_2atmpS2796;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2796 = _M0MPC16double6Double10to__string(_M0L4selfS1052);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1051.$0->$method_0(_M0L6loggerS1051.$1, _M0L6_2atmpS2796);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1050) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1050);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1037) {
  uint64_t _M0L4bitsS1038;
  uint64_t _M0L6_2atmpS2795;
  uint64_t _M0L6_2atmpS2794;
  int32_t _M0L8ieeeSignS1039;
  uint64_t _M0L12ieeeMantissaS1040;
  uint64_t _M0L6_2atmpS2793;
  uint64_t _M0L6_2atmpS2792;
  int32_t _M0L12ieeeExponentS1041;
  int32_t _if__result_3842;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1042;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1043;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2791;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1037 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_74.data;
  }
  _M0L4bitsS1038 = *(int64_t*)&_M0L3valS1037;
  _M0L6_2atmpS2795 = _M0L4bitsS1038 >> 63;
  _M0L6_2atmpS2794 = _M0L6_2atmpS2795 & 1ull;
  _M0L8ieeeSignS1039 = _M0L6_2atmpS2794 != 0ull;
  _M0L12ieeeMantissaS1040 = _M0L4bitsS1038 & 4503599627370495ull;
  _M0L6_2atmpS2793 = _M0L4bitsS1038 >> 52;
  _M0L6_2atmpS2792 = _M0L6_2atmpS2793 & 2047ull;
  _M0L12ieeeExponentS1041 = (int32_t)_M0L6_2atmpS2792;
  if (_M0L12ieeeExponentS1041 == 2047) {
    _if__result_3842 = 1;
  } else if (_M0L12ieeeExponentS1041 == 0) {
    _if__result_3842 = _M0L12ieeeMantissaS1040 == 0ull;
  } else {
    _if__result_3842 = 0;
  }
  if (_if__result_3842) {
    int32_t _M0L6_2atmpS2780 = _M0L12ieeeExponentS1041 != 0;
    int32_t _M0L6_2atmpS2781 = _M0L12ieeeMantissaS1040 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1039, _M0L6_2atmpS2780, _M0L6_2atmpS2781);
  }
  _M0Lm1vS1042 = _M0FPB31ryu__to__string_2erecord_2f1036;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1043
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1040, _M0L12ieeeExponentS1041);
  if (_M0L5smallS1043 == 0) {
    uint32_t _M0L6_2atmpS2782;
    if (_M0L5smallS1043) {
      moonbit_decref(_M0L5smallS1043);
    }
    _M0L6_2atmpS2782 = *(uint32_t*)&_M0L12ieeeExponentS1041;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1042 = _M0FPB3d2d(_M0L12ieeeMantissaS1040, _M0L6_2atmpS2782);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1044 = _M0L5smallS1043;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1045 = _M0L7_2aSomeS1044;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1046 = _M0L4_2afS1045;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2790 = _M0Lm1xS1046;
      uint64_t _M0L8_2afieldS3399 = _M0L6_2atmpS2790->$0;
      uint64_t _M0L8mantissaS2789 = _M0L8_2afieldS3399;
      uint64_t _M0L1qS1047 = _M0L8mantissaS2789 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2788 = _M0Lm1xS1046;
      uint64_t _M0L8_2afieldS3398 = _M0L6_2atmpS2788->$0;
      uint64_t _M0L8mantissaS2786 = _M0L8_2afieldS3398;
      uint64_t _M0L6_2atmpS2787 = 10ull * _M0L1qS1047;
      uint64_t _M0L1rS1048 = _M0L8mantissaS2786 - _M0L6_2atmpS2787;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2785;
      int32_t _M0L8_2afieldS3397;
      int32_t _M0L8exponentS2784;
      int32_t _M0L6_2atmpS2783;
      if (_M0L1rS1048 != 0ull) {
        break;
      }
      _M0L6_2atmpS2785 = _M0Lm1xS1046;
      _M0L8_2afieldS3397 = _M0L6_2atmpS2785->$1;
      moonbit_decref(_M0L6_2atmpS2785);
      _M0L8exponentS2784 = _M0L8_2afieldS3397;
      _M0L6_2atmpS2783 = _M0L8exponentS2784 + 1;
      _M0Lm1xS1046
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1046)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1046->$0 = _M0L1qS1047;
      _M0Lm1xS1046->$1 = _M0L6_2atmpS2783;
      continue;
      break;
    }
    _M0Lm1vS1042 = _M0Lm1xS1046;
  }
  _M0L6_2atmpS2791 = _M0Lm1vS1042;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2791, _M0L8ieeeSignS1039);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1031,
  int32_t _M0L12ieeeExponentS1033
) {
  uint64_t _M0L2m2S1030;
  int32_t _M0L6_2atmpS2779;
  int32_t _M0L2e2S1032;
  int32_t _M0L6_2atmpS2778;
  uint64_t _M0L6_2atmpS2777;
  uint64_t _M0L4maskS1034;
  uint64_t _M0L8fractionS1035;
  int32_t _M0L6_2atmpS2776;
  uint64_t _M0L6_2atmpS2775;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2774;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1030 = 4503599627370496ull | _M0L12ieeeMantissaS1031;
  _M0L6_2atmpS2779 = _M0L12ieeeExponentS1033 - 1023;
  _M0L2e2S1032 = _M0L6_2atmpS2779 - 52;
  if (_M0L2e2S1032 > 0) {
    return 0;
  }
  if (_M0L2e2S1032 < -52) {
    return 0;
  }
  _M0L6_2atmpS2778 = -_M0L2e2S1032;
  _M0L6_2atmpS2777 = 1ull << (_M0L6_2atmpS2778 & 63);
  _M0L4maskS1034 = _M0L6_2atmpS2777 - 1ull;
  _M0L8fractionS1035 = _M0L2m2S1030 & _M0L4maskS1034;
  if (_M0L8fractionS1035 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2776 = -_M0L2e2S1032;
  _M0L6_2atmpS2775 = _M0L2m2S1030 >> (_M0L6_2atmpS2776 & 63);
  _M0L6_2atmpS2774
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2774)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2774->$0 = _M0L6_2atmpS2775;
  _M0L6_2atmpS2774->$1 = 0;
  return _M0L6_2atmpS2774;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1004,
  int32_t _M0L4signS1002
) {
  int32_t _M0L6_2atmpS2773;
  moonbit_bytes_t _M0L6resultS1000;
  int32_t _M0Lm5indexS1001;
  uint64_t _M0Lm6outputS1003;
  uint64_t _M0L6_2atmpS2772;
  int32_t _M0L7olengthS1005;
  int32_t _M0L8_2afieldS3400;
  int32_t _M0L8exponentS2771;
  int32_t _M0L6_2atmpS2770;
  int32_t _M0Lm3expS1006;
  int32_t _M0L6_2atmpS2769;
  int32_t _M0L6_2atmpS2767;
  int32_t _M0L18scientificNotationS1007;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2773 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1000
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2773);
  _M0Lm5indexS1001 = 0;
  if (_M0L4signS1002) {
    int32_t _M0L6_2atmpS2642 = _M0Lm5indexS1001;
    int32_t _M0L6_2atmpS2643;
    if (
      _M0L6_2atmpS2642 < 0
      || _M0L6_2atmpS2642 >= Moonbit_array_length(_M0L6resultS1000)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1000[_M0L6_2atmpS2642] = 45;
    _M0L6_2atmpS2643 = _M0Lm5indexS1001;
    _M0Lm5indexS1001 = _M0L6_2atmpS2643 + 1;
  }
  _M0Lm6outputS1003 = _M0L1vS1004->$0;
  _M0L6_2atmpS2772 = _M0Lm6outputS1003;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1005 = _M0FPB17decimal__length17(_M0L6_2atmpS2772);
  _M0L8_2afieldS3400 = _M0L1vS1004->$1;
  moonbit_decref(_M0L1vS1004);
  _M0L8exponentS2771 = _M0L8_2afieldS3400;
  _M0L6_2atmpS2770 = _M0L8exponentS2771 + _M0L7olengthS1005;
  _M0Lm3expS1006 = _M0L6_2atmpS2770 - 1;
  _M0L6_2atmpS2769 = _M0Lm3expS1006;
  if (_M0L6_2atmpS2769 >= -6) {
    int32_t _M0L6_2atmpS2768 = _M0Lm3expS1006;
    _M0L6_2atmpS2767 = _M0L6_2atmpS2768 < 21;
  } else {
    _M0L6_2atmpS2767 = 0;
  }
  _M0L18scientificNotationS1007 = !_M0L6_2atmpS2767;
  if (_M0L18scientificNotationS1007) {
    int32_t _M0L7_2abindS1008 = _M0L7olengthS1005 - 1;
    int32_t _M0L1iS1009 = 0;
    int32_t _M0L6_2atmpS2653;
    uint64_t _M0L6_2atmpS2658;
    int32_t _M0L6_2atmpS2657;
    int32_t _M0L6_2atmpS2656;
    int32_t _M0L6_2atmpS2655;
    int32_t _M0L6_2atmpS2654;
    int32_t _M0L6_2atmpS2662;
    int32_t _M0L6_2atmpS2663;
    int32_t _M0L6_2atmpS2664;
    int32_t _M0L6_2atmpS2665;
    int32_t _M0L6_2atmpS2666;
    int32_t _M0L6_2atmpS2672;
    int32_t _M0L6_2atmpS2705;
    while (1) {
      if (_M0L1iS1009 < _M0L7_2abindS1008) {
        uint64_t _M0L6_2atmpS2651 = _M0Lm6outputS1003;
        uint64_t _M0L1cS1010 = _M0L6_2atmpS2651 % 10ull;
        uint64_t _M0L6_2atmpS2644 = _M0Lm6outputS1003;
        int32_t _M0L6_2atmpS2650;
        int32_t _M0L6_2atmpS2649;
        int32_t _M0L6_2atmpS2645;
        int32_t _M0L6_2atmpS2648;
        int32_t _M0L6_2atmpS2647;
        int32_t _M0L6_2atmpS2646;
        int32_t _M0L6_2atmpS2652;
        _M0Lm6outputS1003 = _M0L6_2atmpS2644 / 10ull;
        _M0L6_2atmpS2650 = _M0Lm5indexS1001;
        _M0L6_2atmpS2649 = _M0L6_2atmpS2650 + _M0L7olengthS1005;
        _M0L6_2atmpS2645 = _M0L6_2atmpS2649 - _M0L1iS1009;
        _M0L6_2atmpS2648 = (int32_t)_M0L1cS1010;
        _M0L6_2atmpS2647 = 48 + _M0L6_2atmpS2648;
        _M0L6_2atmpS2646 = _M0L6_2atmpS2647 & 0xff;
        if (
          _M0L6_2atmpS2645 < 0
          || _M0L6_2atmpS2645 >= Moonbit_array_length(_M0L6resultS1000)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1000[_M0L6_2atmpS2645] = _M0L6_2atmpS2646;
        _M0L6_2atmpS2652 = _M0L1iS1009 + 1;
        _M0L1iS1009 = _M0L6_2atmpS2652;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2653 = _M0Lm5indexS1001;
    _M0L6_2atmpS2658 = _M0Lm6outputS1003;
    _M0L6_2atmpS2657 = (int32_t)_M0L6_2atmpS2658;
    _M0L6_2atmpS2656 = _M0L6_2atmpS2657 % 10;
    _M0L6_2atmpS2655 = 48 + _M0L6_2atmpS2656;
    _M0L6_2atmpS2654 = _M0L6_2atmpS2655 & 0xff;
    if (
      _M0L6_2atmpS2653 < 0
      || _M0L6_2atmpS2653 >= Moonbit_array_length(_M0L6resultS1000)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1000[_M0L6_2atmpS2653] = _M0L6_2atmpS2654;
    if (_M0L7olengthS1005 > 1) {
      int32_t _M0L6_2atmpS2660 = _M0Lm5indexS1001;
      int32_t _M0L6_2atmpS2659 = _M0L6_2atmpS2660 + 1;
      if (
        _M0L6_2atmpS2659 < 0
        || _M0L6_2atmpS2659 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2659] = 46;
    } else {
      int32_t _M0L6_2atmpS2661 = _M0Lm5indexS1001;
      _M0Lm5indexS1001 = _M0L6_2atmpS2661 - 1;
    }
    _M0L6_2atmpS2662 = _M0Lm5indexS1001;
    _M0L6_2atmpS2663 = _M0L7olengthS1005 + 1;
    _M0Lm5indexS1001 = _M0L6_2atmpS2662 + _M0L6_2atmpS2663;
    _M0L6_2atmpS2664 = _M0Lm5indexS1001;
    if (
      _M0L6_2atmpS2664 < 0
      || _M0L6_2atmpS2664 >= Moonbit_array_length(_M0L6resultS1000)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1000[_M0L6_2atmpS2664] = 101;
    _M0L6_2atmpS2665 = _M0Lm5indexS1001;
    _M0Lm5indexS1001 = _M0L6_2atmpS2665 + 1;
    _M0L6_2atmpS2666 = _M0Lm3expS1006;
    if (_M0L6_2atmpS2666 < 0) {
      int32_t _M0L6_2atmpS2667 = _M0Lm5indexS1001;
      int32_t _M0L6_2atmpS2668;
      int32_t _M0L6_2atmpS2669;
      if (
        _M0L6_2atmpS2667 < 0
        || _M0L6_2atmpS2667 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2667] = 45;
      _M0L6_2atmpS2668 = _M0Lm5indexS1001;
      _M0Lm5indexS1001 = _M0L6_2atmpS2668 + 1;
      _M0L6_2atmpS2669 = _M0Lm3expS1006;
      _M0Lm3expS1006 = -_M0L6_2atmpS2669;
    } else {
      int32_t _M0L6_2atmpS2670 = _M0Lm5indexS1001;
      int32_t _M0L6_2atmpS2671;
      if (
        _M0L6_2atmpS2670 < 0
        || _M0L6_2atmpS2670 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2670] = 43;
      _M0L6_2atmpS2671 = _M0Lm5indexS1001;
      _M0Lm5indexS1001 = _M0L6_2atmpS2671 + 1;
    }
    _M0L6_2atmpS2672 = _M0Lm3expS1006;
    if (_M0L6_2atmpS2672 >= 100) {
      int32_t _M0L6_2atmpS2688 = _M0Lm3expS1006;
      int32_t _M0L1aS1012 = _M0L6_2atmpS2688 / 100;
      int32_t _M0L6_2atmpS2687 = _M0Lm3expS1006;
      int32_t _M0L6_2atmpS2686 = _M0L6_2atmpS2687 / 10;
      int32_t _M0L1bS1013 = _M0L6_2atmpS2686 % 10;
      int32_t _M0L6_2atmpS2685 = _M0Lm3expS1006;
      int32_t _M0L1cS1014 = _M0L6_2atmpS2685 % 10;
      int32_t _M0L6_2atmpS2673 = _M0Lm5indexS1001;
      int32_t _M0L6_2atmpS2675 = 48 + _M0L1aS1012;
      int32_t _M0L6_2atmpS2674 = _M0L6_2atmpS2675 & 0xff;
      int32_t _M0L6_2atmpS2679;
      int32_t _M0L6_2atmpS2676;
      int32_t _M0L6_2atmpS2678;
      int32_t _M0L6_2atmpS2677;
      int32_t _M0L6_2atmpS2683;
      int32_t _M0L6_2atmpS2680;
      int32_t _M0L6_2atmpS2682;
      int32_t _M0L6_2atmpS2681;
      int32_t _M0L6_2atmpS2684;
      if (
        _M0L6_2atmpS2673 < 0
        || _M0L6_2atmpS2673 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2673] = _M0L6_2atmpS2674;
      _M0L6_2atmpS2679 = _M0Lm5indexS1001;
      _M0L6_2atmpS2676 = _M0L6_2atmpS2679 + 1;
      _M0L6_2atmpS2678 = 48 + _M0L1bS1013;
      _M0L6_2atmpS2677 = _M0L6_2atmpS2678 & 0xff;
      if (
        _M0L6_2atmpS2676 < 0
        || _M0L6_2atmpS2676 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2676] = _M0L6_2atmpS2677;
      _M0L6_2atmpS2683 = _M0Lm5indexS1001;
      _M0L6_2atmpS2680 = _M0L6_2atmpS2683 + 2;
      _M0L6_2atmpS2682 = 48 + _M0L1cS1014;
      _M0L6_2atmpS2681 = _M0L6_2atmpS2682 & 0xff;
      if (
        _M0L6_2atmpS2680 < 0
        || _M0L6_2atmpS2680 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2680] = _M0L6_2atmpS2681;
      _M0L6_2atmpS2684 = _M0Lm5indexS1001;
      _M0Lm5indexS1001 = _M0L6_2atmpS2684 + 3;
    } else {
      int32_t _M0L6_2atmpS2689 = _M0Lm3expS1006;
      if (_M0L6_2atmpS2689 >= 10) {
        int32_t _M0L6_2atmpS2699 = _M0Lm3expS1006;
        int32_t _M0L1aS1015 = _M0L6_2atmpS2699 / 10;
        int32_t _M0L6_2atmpS2698 = _M0Lm3expS1006;
        int32_t _M0L1bS1016 = _M0L6_2atmpS2698 % 10;
        int32_t _M0L6_2atmpS2690 = _M0Lm5indexS1001;
        int32_t _M0L6_2atmpS2692 = 48 + _M0L1aS1015;
        int32_t _M0L6_2atmpS2691 = _M0L6_2atmpS2692 & 0xff;
        int32_t _M0L6_2atmpS2696;
        int32_t _M0L6_2atmpS2693;
        int32_t _M0L6_2atmpS2695;
        int32_t _M0L6_2atmpS2694;
        int32_t _M0L6_2atmpS2697;
        if (
          _M0L6_2atmpS2690 < 0
          || _M0L6_2atmpS2690 >= Moonbit_array_length(_M0L6resultS1000)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1000[_M0L6_2atmpS2690] = _M0L6_2atmpS2691;
        _M0L6_2atmpS2696 = _M0Lm5indexS1001;
        _M0L6_2atmpS2693 = _M0L6_2atmpS2696 + 1;
        _M0L6_2atmpS2695 = 48 + _M0L1bS1016;
        _M0L6_2atmpS2694 = _M0L6_2atmpS2695 & 0xff;
        if (
          _M0L6_2atmpS2693 < 0
          || _M0L6_2atmpS2693 >= Moonbit_array_length(_M0L6resultS1000)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1000[_M0L6_2atmpS2693] = _M0L6_2atmpS2694;
        _M0L6_2atmpS2697 = _M0Lm5indexS1001;
        _M0Lm5indexS1001 = _M0L6_2atmpS2697 + 2;
      } else {
        int32_t _M0L6_2atmpS2700 = _M0Lm5indexS1001;
        int32_t _M0L6_2atmpS2703 = _M0Lm3expS1006;
        int32_t _M0L6_2atmpS2702 = 48 + _M0L6_2atmpS2703;
        int32_t _M0L6_2atmpS2701 = _M0L6_2atmpS2702 & 0xff;
        int32_t _M0L6_2atmpS2704;
        if (
          _M0L6_2atmpS2700 < 0
          || _M0L6_2atmpS2700 >= Moonbit_array_length(_M0L6resultS1000)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1000[_M0L6_2atmpS2700] = _M0L6_2atmpS2701;
        _M0L6_2atmpS2704 = _M0Lm5indexS1001;
        _M0Lm5indexS1001 = _M0L6_2atmpS2704 + 1;
      }
    }
    _M0L6_2atmpS2705 = _M0Lm5indexS1001;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1000, 0, _M0L6_2atmpS2705);
  } else {
    int32_t _M0L6_2atmpS2706 = _M0Lm3expS1006;
    int32_t _M0L6_2atmpS2766;
    if (_M0L6_2atmpS2706 < 0) {
      int32_t _M0L6_2atmpS2707 = _M0Lm5indexS1001;
      int32_t _M0L6_2atmpS2708;
      int32_t _M0L6_2atmpS2709;
      int32_t _M0L6_2atmpS2710;
      int32_t _M0L1iS1017;
      int32_t _M0L7currentS1019;
      int32_t _M0L1iS1020;
      if (
        _M0L6_2atmpS2707 < 0
        || _M0L6_2atmpS2707 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2707] = 48;
      _M0L6_2atmpS2708 = _M0Lm5indexS1001;
      _M0Lm5indexS1001 = _M0L6_2atmpS2708 + 1;
      _M0L6_2atmpS2709 = _M0Lm5indexS1001;
      if (
        _M0L6_2atmpS2709 < 0
        || _M0L6_2atmpS2709 >= Moonbit_array_length(_M0L6resultS1000)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1000[_M0L6_2atmpS2709] = 46;
      _M0L6_2atmpS2710 = _M0Lm5indexS1001;
      _M0Lm5indexS1001 = _M0L6_2atmpS2710 + 1;
      _M0L1iS1017 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2711 = _M0Lm3expS1006;
        if (_M0L1iS1017 > _M0L6_2atmpS2711) {
          int32_t _M0L6_2atmpS2712 = _M0Lm5indexS1001;
          int32_t _M0L6_2atmpS2713;
          int32_t _M0L6_2atmpS2714;
          if (
            _M0L6_2atmpS2712 < 0
            || _M0L6_2atmpS2712 >= Moonbit_array_length(_M0L6resultS1000)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1000[_M0L6_2atmpS2712] = 48;
          _M0L6_2atmpS2713 = _M0Lm5indexS1001;
          _M0Lm5indexS1001 = _M0L6_2atmpS2713 + 1;
          _M0L6_2atmpS2714 = _M0L1iS1017 - 1;
          _M0L1iS1017 = _M0L6_2atmpS2714;
          continue;
        }
        break;
      }
      _M0L7currentS1019 = _M0Lm5indexS1001;
      _M0L1iS1020 = 0;
      while (1) {
        if (_M0L1iS1020 < _M0L7olengthS1005) {
          int32_t _M0L6_2atmpS2722 = _M0L7currentS1019 + _M0L7olengthS1005;
          int32_t _M0L6_2atmpS2721 = _M0L6_2atmpS2722 - _M0L1iS1020;
          int32_t _M0L6_2atmpS2715 = _M0L6_2atmpS2721 - 1;
          uint64_t _M0L6_2atmpS2720 = _M0Lm6outputS1003;
          uint64_t _M0L6_2atmpS2719 = _M0L6_2atmpS2720 % 10ull;
          int32_t _M0L6_2atmpS2718 = (int32_t)_M0L6_2atmpS2719;
          int32_t _M0L6_2atmpS2717 = 48 + _M0L6_2atmpS2718;
          int32_t _M0L6_2atmpS2716 = _M0L6_2atmpS2717 & 0xff;
          uint64_t _M0L6_2atmpS2723;
          int32_t _M0L6_2atmpS2724;
          int32_t _M0L6_2atmpS2725;
          if (
            _M0L6_2atmpS2715 < 0
            || _M0L6_2atmpS2715 >= Moonbit_array_length(_M0L6resultS1000)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1000[_M0L6_2atmpS2715] = _M0L6_2atmpS2716;
          _M0L6_2atmpS2723 = _M0Lm6outputS1003;
          _M0Lm6outputS1003 = _M0L6_2atmpS2723 / 10ull;
          _M0L6_2atmpS2724 = _M0Lm5indexS1001;
          _M0Lm5indexS1001 = _M0L6_2atmpS2724 + 1;
          _M0L6_2atmpS2725 = _M0L1iS1020 + 1;
          _M0L1iS1020 = _M0L6_2atmpS2725;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2727 = _M0Lm3expS1006;
      int32_t _M0L6_2atmpS2726 = _M0L6_2atmpS2727 + 1;
      if (_M0L6_2atmpS2726 >= _M0L7olengthS1005) {
        int32_t _M0L1iS1022 = 0;
        int32_t _M0L6_2atmpS2739;
        int32_t _M0L6_2atmpS2743;
        int32_t _M0L7_2abindS1024;
        int32_t _M0L2__S1025;
        while (1) {
          if (_M0L1iS1022 < _M0L7olengthS1005) {
            int32_t _M0L6_2atmpS2736 = _M0Lm5indexS1001;
            int32_t _M0L6_2atmpS2735 = _M0L6_2atmpS2736 + _M0L7olengthS1005;
            int32_t _M0L6_2atmpS2734 = _M0L6_2atmpS2735 - _M0L1iS1022;
            int32_t _M0L6_2atmpS2728 = _M0L6_2atmpS2734 - 1;
            uint64_t _M0L6_2atmpS2733 = _M0Lm6outputS1003;
            uint64_t _M0L6_2atmpS2732 = _M0L6_2atmpS2733 % 10ull;
            int32_t _M0L6_2atmpS2731 = (int32_t)_M0L6_2atmpS2732;
            int32_t _M0L6_2atmpS2730 = 48 + _M0L6_2atmpS2731;
            int32_t _M0L6_2atmpS2729 = _M0L6_2atmpS2730 & 0xff;
            uint64_t _M0L6_2atmpS2737;
            int32_t _M0L6_2atmpS2738;
            if (
              _M0L6_2atmpS2728 < 0
              || _M0L6_2atmpS2728 >= Moonbit_array_length(_M0L6resultS1000)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1000[_M0L6_2atmpS2728] = _M0L6_2atmpS2729;
            _M0L6_2atmpS2737 = _M0Lm6outputS1003;
            _M0Lm6outputS1003 = _M0L6_2atmpS2737 / 10ull;
            _M0L6_2atmpS2738 = _M0L1iS1022 + 1;
            _M0L1iS1022 = _M0L6_2atmpS2738;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2739 = _M0Lm5indexS1001;
        _M0Lm5indexS1001 = _M0L6_2atmpS2739 + _M0L7olengthS1005;
        _M0L6_2atmpS2743 = _M0Lm3expS1006;
        _M0L7_2abindS1024 = _M0L6_2atmpS2743 + 1;
        _M0L2__S1025 = _M0L7olengthS1005;
        while (1) {
          if (_M0L2__S1025 < _M0L7_2abindS1024) {
            int32_t _M0L6_2atmpS2740 = _M0Lm5indexS1001;
            int32_t _M0L6_2atmpS2741;
            int32_t _M0L6_2atmpS2742;
            if (
              _M0L6_2atmpS2740 < 0
              || _M0L6_2atmpS2740 >= Moonbit_array_length(_M0L6resultS1000)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1000[_M0L6_2atmpS2740] = 48;
            _M0L6_2atmpS2741 = _M0Lm5indexS1001;
            _M0Lm5indexS1001 = _M0L6_2atmpS2741 + 1;
            _M0L6_2atmpS2742 = _M0L2__S1025 + 1;
            _M0L2__S1025 = _M0L6_2atmpS2742;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2765 = _M0Lm5indexS1001;
        int32_t _M0Lm7currentS1027 = _M0L6_2atmpS2765 + 1;
        int32_t _M0L1iS1028 = 0;
        int32_t _M0L6_2atmpS2763;
        int32_t _M0L6_2atmpS2764;
        while (1) {
          if (_M0L1iS1028 < _M0L7olengthS1005) {
            int32_t _M0L6_2atmpS2746 = _M0L7olengthS1005 - _M0L1iS1028;
            int32_t _M0L6_2atmpS2744 = _M0L6_2atmpS2746 - 1;
            int32_t _M0L6_2atmpS2745 = _M0Lm3expS1006;
            int32_t _M0L6_2atmpS2760;
            int32_t _M0L6_2atmpS2759;
            int32_t _M0L6_2atmpS2758;
            int32_t _M0L6_2atmpS2752;
            uint64_t _M0L6_2atmpS2757;
            uint64_t _M0L6_2atmpS2756;
            int32_t _M0L6_2atmpS2755;
            int32_t _M0L6_2atmpS2754;
            int32_t _M0L6_2atmpS2753;
            uint64_t _M0L6_2atmpS2761;
            int32_t _M0L6_2atmpS2762;
            if (_M0L6_2atmpS2744 == _M0L6_2atmpS2745) {
              int32_t _M0L6_2atmpS2750 = _M0Lm7currentS1027;
              int32_t _M0L6_2atmpS2749 = _M0L6_2atmpS2750 + _M0L7olengthS1005;
              int32_t _M0L6_2atmpS2748 = _M0L6_2atmpS2749 - _M0L1iS1028;
              int32_t _M0L6_2atmpS2747 = _M0L6_2atmpS2748 - 1;
              int32_t _M0L6_2atmpS2751;
              if (
                _M0L6_2atmpS2747 < 0
                || _M0L6_2atmpS2747 >= Moonbit_array_length(_M0L6resultS1000)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1000[_M0L6_2atmpS2747] = 46;
              _M0L6_2atmpS2751 = _M0Lm7currentS1027;
              _M0Lm7currentS1027 = _M0L6_2atmpS2751 - 1;
            }
            _M0L6_2atmpS2760 = _M0Lm7currentS1027;
            _M0L6_2atmpS2759 = _M0L6_2atmpS2760 + _M0L7olengthS1005;
            _M0L6_2atmpS2758 = _M0L6_2atmpS2759 - _M0L1iS1028;
            _M0L6_2atmpS2752 = _M0L6_2atmpS2758 - 1;
            _M0L6_2atmpS2757 = _M0Lm6outputS1003;
            _M0L6_2atmpS2756 = _M0L6_2atmpS2757 % 10ull;
            _M0L6_2atmpS2755 = (int32_t)_M0L6_2atmpS2756;
            _M0L6_2atmpS2754 = 48 + _M0L6_2atmpS2755;
            _M0L6_2atmpS2753 = _M0L6_2atmpS2754 & 0xff;
            if (
              _M0L6_2atmpS2752 < 0
              || _M0L6_2atmpS2752 >= Moonbit_array_length(_M0L6resultS1000)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1000[_M0L6_2atmpS2752] = _M0L6_2atmpS2753;
            _M0L6_2atmpS2761 = _M0Lm6outputS1003;
            _M0Lm6outputS1003 = _M0L6_2atmpS2761 / 10ull;
            _M0L6_2atmpS2762 = _M0L1iS1028 + 1;
            _M0L1iS1028 = _M0L6_2atmpS2762;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2763 = _M0Lm5indexS1001;
        _M0L6_2atmpS2764 = _M0L7olengthS1005 + 1;
        _M0Lm5indexS1001 = _M0L6_2atmpS2763 + _M0L6_2atmpS2764;
      }
    }
    _M0L6_2atmpS2766 = _M0Lm5indexS1001;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1000, 0, _M0L6_2atmpS2766);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS946,
  uint32_t _M0L12ieeeExponentS945
) {
  int32_t _M0Lm2e2S943;
  uint64_t _M0Lm2m2S944;
  uint64_t _M0L6_2atmpS2641;
  uint64_t _M0L6_2atmpS2640;
  int32_t _M0L4evenS947;
  uint64_t _M0L6_2atmpS2639;
  uint64_t _M0L2mvS948;
  int32_t _M0L7mmShiftS949;
  uint64_t _M0Lm2vrS950;
  uint64_t _M0Lm2vpS951;
  uint64_t _M0Lm2vmS952;
  int32_t _M0Lm3e10S953;
  int32_t _M0Lm17vmIsTrailingZerosS954;
  int32_t _M0Lm17vrIsTrailingZerosS955;
  int32_t _M0L6_2atmpS2541;
  int32_t _M0Lm7removedS974;
  int32_t _M0Lm16lastRemovedDigitS975;
  uint64_t _M0Lm6outputS976;
  int32_t _M0L6_2atmpS2637;
  int32_t _M0L6_2atmpS2638;
  int32_t _M0L3expS999;
  uint64_t _M0L6_2atmpS2636;
  struct _M0TPB17FloatingDecimal64* _block_3855;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S943 = 0;
  _M0Lm2m2S944 = 0ull;
  if (_M0L12ieeeExponentS945 == 0u) {
    _M0Lm2e2S943 = -1076;
    _M0Lm2m2S944 = _M0L12ieeeMantissaS946;
  } else {
    int32_t _M0L6_2atmpS2540 = *(int32_t*)&_M0L12ieeeExponentS945;
    int32_t _M0L6_2atmpS2539 = _M0L6_2atmpS2540 - 1023;
    int32_t _M0L6_2atmpS2538 = _M0L6_2atmpS2539 - 52;
    _M0Lm2e2S943 = _M0L6_2atmpS2538 - 2;
    _M0Lm2m2S944 = 4503599627370496ull | _M0L12ieeeMantissaS946;
  }
  _M0L6_2atmpS2641 = _M0Lm2m2S944;
  _M0L6_2atmpS2640 = _M0L6_2atmpS2641 & 1ull;
  _M0L4evenS947 = _M0L6_2atmpS2640 == 0ull;
  _M0L6_2atmpS2639 = _M0Lm2m2S944;
  _M0L2mvS948 = 4ull * _M0L6_2atmpS2639;
  if (_M0L12ieeeMantissaS946 != 0ull) {
    _M0L7mmShiftS949 = 1;
  } else {
    _M0L7mmShiftS949 = _M0L12ieeeExponentS945 <= 1u;
  }
  _M0Lm2vrS950 = 0ull;
  _M0Lm2vpS951 = 0ull;
  _M0Lm2vmS952 = 0ull;
  _M0Lm3e10S953 = 0;
  _M0Lm17vmIsTrailingZerosS954 = 0;
  _M0Lm17vrIsTrailingZerosS955 = 0;
  _M0L6_2atmpS2541 = _M0Lm2e2S943;
  if (_M0L6_2atmpS2541 >= 0) {
    int32_t _M0L6_2atmpS2563 = _M0Lm2e2S943;
    int32_t _M0L6_2atmpS2559;
    int32_t _M0L6_2atmpS2562;
    int32_t _M0L6_2atmpS2561;
    int32_t _M0L6_2atmpS2560;
    int32_t _M0L1qS956;
    int32_t _M0L6_2atmpS2558;
    int32_t _M0L6_2atmpS2557;
    int32_t _M0L1kS957;
    int32_t _M0L6_2atmpS2556;
    int32_t _M0L6_2atmpS2555;
    int32_t _M0L6_2atmpS2554;
    int32_t _M0L1iS958;
    struct _M0TPB8Pow5Pair _M0L4pow5S959;
    uint64_t _M0L6_2atmpS2553;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS960;
    uint64_t _M0L8_2avrOutS961;
    uint64_t _M0L8_2avpOutS962;
    uint64_t _M0L8_2avmOutS963;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2559 = _M0FPB9log10Pow2(_M0L6_2atmpS2563);
    _M0L6_2atmpS2562 = _M0Lm2e2S943;
    _M0L6_2atmpS2561 = _M0L6_2atmpS2562 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2560 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2561);
    _M0L1qS956 = _M0L6_2atmpS2559 - _M0L6_2atmpS2560;
    _M0Lm3e10S953 = _M0L1qS956;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2558 = _M0FPB8pow5bits(_M0L1qS956);
    _M0L6_2atmpS2557 = 125 + _M0L6_2atmpS2558;
    _M0L1kS957 = _M0L6_2atmpS2557 - 1;
    _M0L6_2atmpS2556 = _M0Lm2e2S943;
    _M0L6_2atmpS2555 = -_M0L6_2atmpS2556;
    _M0L6_2atmpS2554 = _M0L6_2atmpS2555 + _M0L1qS956;
    _M0L1iS958 = _M0L6_2atmpS2554 + _M0L1kS957;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S959 = _M0FPB22double__computeInvPow5(_M0L1qS956);
    _M0L6_2atmpS2553 = _M0Lm2m2S944;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS960
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2553, _M0L4pow5S959, _M0L1iS958, _M0L7mmShiftS949);
    _M0L8_2avrOutS961 = _M0L7_2abindS960.$0;
    _M0L8_2avpOutS962 = _M0L7_2abindS960.$1;
    _M0L8_2avmOutS963 = _M0L7_2abindS960.$2;
    _M0Lm2vrS950 = _M0L8_2avrOutS961;
    _M0Lm2vpS951 = _M0L8_2avpOutS962;
    _M0Lm2vmS952 = _M0L8_2avmOutS963;
    if (_M0L1qS956 <= 21) {
      int32_t _M0L6_2atmpS2549 = (int32_t)_M0L2mvS948;
      uint64_t _M0L6_2atmpS2552 = _M0L2mvS948 / 5ull;
      int32_t _M0L6_2atmpS2551 = (int32_t)_M0L6_2atmpS2552;
      int32_t _M0L6_2atmpS2550 = 5 * _M0L6_2atmpS2551;
      int32_t _M0L6mvMod5S964 = _M0L6_2atmpS2549 - _M0L6_2atmpS2550;
      if (_M0L6mvMod5S964 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS955
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS948, _M0L1qS956);
      } else if (_M0L4evenS947) {
        uint64_t _M0L6_2atmpS2543 = _M0L2mvS948 - 1ull;
        uint64_t _M0L6_2atmpS2544;
        uint64_t _M0L6_2atmpS2542;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2544 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS949);
        _M0L6_2atmpS2542 = _M0L6_2atmpS2543 - _M0L6_2atmpS2544;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS954
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2542, _M0L1qS956);
      } else {
        uint64_t _M0L6_2atmpS2545 = _M0Lm2vpS951;
        uint64_t _M0L6_2atmpS2548 = _M0L2mvS948 + 2ull;
        int32_t _M0L6_2atmpS2547;
        uint64_t _M0L6_2atmpS2546;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2547
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2548, _M0L1qS956);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2546 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2547);
        _M0Lm2vpS951 = _M0L6_2atmpS2545 - _M0L6_2atmpS2546;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2577 = _M0Lm2e2S943;
    int32_t _M0L6_2atmpS2576 = -_M0L6_2atmpS2577;
    int32_t _M0L6_2atmpS2571;
    int32_t _M0L6_2atmpS2575;
    int32_t _M0L6_2atmpS2574;
    int32_t _M0L6_2atmpS2573;
    int32_t _M0L6_2atmpS2572;
    int32_t _M0L1qS965;
    int32_t _M0L6_2atmpS2564;
    int32_t _M0L6_2atmpS2570;
    int32_t _M0L6_2atmpS2569;
    int32_t _M0L1iS966;
    int32_t _M0L6_2atmpS2568;
    int32_t _M0L1kS967;
    int32_t _M0L1jS968;
    struct _M0TPB8Pow5Pair _M0L4pow5S969;
    uint64_t _M0L6_2atmpS2567;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS970;
    uint64_t _M0L8_2avrOutS971;
    uint64_t _M0L8_2avpOutS972;
    uint64_t _M0L8_2avmOutS973;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2571 = _M0FPB9log10Pow5(_M0L6_2atmpS2576);
    _M0L6_2atmpS2575 = _M0Lm2e2S943;
    _M0L6_2atmpS2574 = -_M0L6_2atmpS2575;
    _M0L6_2atmpS2573 = _M0L6_2atmpS2574 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2572 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2573);
    _M0L1qS965 = _M0L6_2atmpS2571 - _M0L6_2atmpS2572;
    _M0L6_2atmpS2564 = _M0Lm2e2S943;
    _M0Lm3e10S953 = _M0L1qS965 + _M0L6_2atmpS2564;
    _M0L6_2atmpS2570 = _M0Lm2e2S943;
    _M0L6_2atmpS2569 = -_M0L6_2atmpS2570;
    _M0L1iS966 = _M0L6_2atmpS2569 - _M0L1qS965;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2568 = _M0FPB8pow5bits(_M0L1iS966);
    _M0L1kS967 = _M0L6_2atmpS2568 - 125;
    _M0L1jS968 = _M0L1qS965 - _M0L1kS967;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S969 = _M0FPB19double__computePow5(_M0L1iS966);
    _M0L6_2atmpS2567 = _M0Lm2m2S944;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS970
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2567, _M0L4pow5S969, _M0L1jS968, _M0L7mmShiftS949);
    _M0L8_2avrOutS971 = _M0L7_2abindS970.$0;
    _M0L8_2avpOutS972 = _M0L7_2abindS970.$1;
    _M0L8_2avmOutS973 = _M0L7_2abindS970.$2;
    _M0Lm2vrS950 = _M0L8_2avrOutS971;
    _M0Lm2vpS951 = _M0L8_2avpOutS972;
    _M0Lm2vmS952 = _M0L8_2avmOutS973;
    if (_M0L1qS965 <= 1) {
      _M0Lm17vrIsTrailingZerosS955 = 1;
      if (_M0L4evenS947) {
        int32_t _M0L6_2atmpS2565;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2565 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS949);
        _M0Lm17vmIsTrailingZerosS954 = _M0L6_2atmpS2565 == 1;
      } else {
        uint64_t _M0L6_2atmpS2566 = _M0Lm2vpS951;
        _M0Lm2vpS951 = _M0L6_2atmpS2566 - 1ull;
      }
    } else if (_M0L1qS965 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS955
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS948, _M0L1qS965);
    }
  }
  _M0Lm7removedS974 = 0;
  _M0Lm16lastRemovedDigitS975 = 0;
  _M0Lm6outputS976 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS954 || _M0Lm17vrIsTrailingZerosS955) {
    int32_t _if__result_3852;
    uint64_t _M0L6_2atmpS2607;
    uint64_t _M0L6_2atmpS2613;
    uint64_t _M0L6_2atmpS2614;
    int32_t _if__result_3853;
    int32_t _M0L6_2atmpS2610;
    int64_t _M0L6_2atmpS2609;
    uint64_t _M0L6_2atmpS2608;
    while (1) {
      uint64_t _M0L6_2atmpS2590 = _M0Lm2vpS951;
      uint64_t _M0L7vpDiv10S977 = _M0L6_2atmpS2590 / 10ull;
      uint64_t _M0L6_2atmpS2589 = _M0Lm2vmS952;
      uint64_t _M0L7vmDiv10S978 = _M0L6_2atmpS2589 / 10ull;
      uint64_t _M0L6_2atmpS2588;
      int32_t _M0L6_2atmpS2585;
      int32_t _M0L6_2atmpS2587;
      int32_t _M0L6_2atmpS2586;
      int32_t _M0L7vmMod10S980;
      uint64_t _M0L6_2atmpS2584;
      uint64_t _M0L7vrDiv10S981;
      uint64_t _M0L6_2atmpS2583;
      int32_t _M0L6_2atmpS2580;
      int32_t _M0L6_2atmpS2582;
      int32_t _M0L6_2atmpS2581;
      int32_t _M0L7vrMod10S982;
      int32_t _M0L6_2atmpS2579;
      if (_M0L7vpDiv10S977 <= _M0L7vmDiv10S978) {
        break;
      }
      _M0L6_2atmpS2588 = _M0Lm2vmS952;
      _M0L6_2atmpS2585 = (int32_t)_M0L6_2atmpS2588;
      _M0L6_2atmpS2587 = (int32_t)_M0L7vmDiv10S978;
      _M0L6_2atmpS2586 = 10 * _M0L6_2atmpS2587;
      _M0L7vmMod10S980 = _M0L6_2atmpS2585 - _M0L6_2atmpS2586;
      _M0L6_2atmpS2584 = _M0Lm2vrS950;
      _M0L7vrDiv10S981 = _M0L6_2atmpS2584 / 10ull;
      _M0L6_2atmpS2583 = _M0Lm2vrS950;
      _M0L6_2atmpS2580 = (int32_t)_M0L6_2atmpS2583;
      _M0L6_2atmpS2582 = (int32_t)_M0L7vrDiv10S981;
      _M0L6_2atmpS2581 = 10 * _M0L6_2atmpS2582;
      _M0L7vrMod10S982 = _M0L6_2atmpS2580 - _M0L6_2atmpS2581;
      if (_M0Lm17vmIsTrailingZerosS954) {
        _M0Lm17vmIsTrailingZerosS954 = _M0L7vmMod10S980 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS954 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS955) {
        int32_t _M0L6_2atmpS2578 = _M0Lm16lastRemovedDigitS975;
        _M0Lm17vrIsTrailingZerosS955 = _M0L6_2atmpS2578 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS955 = 0;
      }
      _M0Lm16lastRemovedDigitS975 = _M0L7vrMod10S982;
      _M0Lm2vrS950 = _M0L7vrDiv10S981;
      _M0Lm2vpS951 = _M0L7vpDiv10S977;
      _M0Lm2vmS952 = _M0L7vmDiv10S978;
      _M0L6_2atmpS2579 = _M0Lm7removedS974;
      _M0Lm7removedS974 = _M0L6_2atmpS2579 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS954) {
      while (1) {
        uint64_t _M0L6_2atmpS2603 = _M0Lm2vmS952;
        uint64_t _M0L7vmDiv10S983 = _M0L6_2atmpS2603 / 10ull;
        uint64_t _M0L6_2atmpS2602 = _M0Lm2vmS952;
        int32_t _M0L6_2atmpS2599 = (int32_t)_M0L6_2atmpS2602;
        int32_t _M0L6_2atmpS2601 = (int32_t)_M0L7vmDiv10S983;
        int32_t _M0L6_2atmpS2600 = 10 * _M0L6_2atmpS2601;
        int32_t _M0L7vmMod10S984 = _M0L6_2atmpS2599 - _M0L6_2atmpS2600;
        uint64_t _M0L6_2atmpS2598;
        uint64_t _M0L7vpDiv10S986;
        uint64_t _M0L6_2atmpS2597;
        uint64_t _M0L7vrDiv10S987;
        uint64_t _M0L6_2atmpS2596;
        int32_t _M0L6_2atmpS2593;
        int32_t _M0L6_2atmpS2595;
        int32_t _M0L6_2atmpS2594;
        int32_t _M0L7vrMod10S988;
        int32_t _M0L6_2atmpS2592;
        if (_M0L7vmMod10S984 != 0) {
          break;
        }
        _M0L6_2atmpS2598 = _M0Lm2vpS951;
        _M0L7vpDiv10S986 = _M0L6_2atmpS2598 / 10ull;
        _M0L6_2atmpS2597 = _M0Lm2vrS950;
        _M0L7vrDiv10S987 = _M0L6_2atmpS2597 / 10ull;
        _M0L6_2atmpS2596 = _M0Lm2vrS950;
        _M0L6_2atmpS2593 = (int32_t)_M0L6_2atmpS2596;
        _M0L6_2atmpS2595 = (int32_t)_M0L7vrDiv10S987;
        _M0L6_2atmpS2594 = 10 * _M0L6_2atmpS2595;
        _M0L7vrMod10S988 = _M0L6_2atmpS2593 - _M0L6_2atmpS2594;
        if (_M0Lm17vrIsTrailingZerosS955) {
          int32_t _M0L6_2atmpS2591 = _M0Lm16lastRemovedDigitS975;
          _M0Lm17vrIsTrailingZerosS955 = _M0L6_2atmpS2591 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS955 = 0;
        }
        _M0Lm16lastRemovedDigitS975 = _M0L7vrMod10S988;
        _M0Lm2vrS950 = _M0L7vrDiv10S987;
        _M0Lm2vpS951 = _M0L7vpDiv10S986;
        _M0Lm2vmS952 = _M0L7vmDiv10S983;
        _M0L6_2atmpS2592 = _M0Lm7removedS974;
        _M0Lm7removedS974 = _M0L6_2atmpS2592 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS955) {
      int32_t _M0L6_2atmpS2606 = _M0Lm16lastRemovedDigitS975;
      if (_M0L6_2atmpS2606 == 5) {
        uint64_t _M0L6_2atmpS2605 = _M0Lm2vrS950;
        uint64_t _M0L6_2atmpS2604 = _M0L6_2atmpS2605 % 2ull;
        _if__result_3852 = _M0L6_2atmpS2604 == 0ull;
      } else {
        _if__result_3852 = 0;
      }
    } else {
      _if__result_3852 = 0;
    }
    if (_if__result_3852) {
      _M0Lm16lastRemovedDigitS975 = 4;
    }
    _M0L6_2atmpS2607 = _M0Lm2vrS950;
    _M0L6_2atmpS2613 = _M0Lm2vrS950;
    _M0L6_2atmpS2614 = _M0Lm2vmS952;
    if (_M0L6_2atmpS2613 == _M0L6_2atmpS2614) {
      if (!_M0L4evenS947) {
        _if__result_3853 = 1;
      } else {
        int32_t _M0L6_2atmpS2612 = _M0Lm17vmIsTrailingZerosS954;
        _if__result_3853 = !_M0L6_2atmpS2612;
      }
    } else {
      _if__result_3853 = 0;
    }
    if (_if__result_3853) {
      _M0L6_2atmpS2610 = 1;
    } else {
      int32_t _M0L6_2atmpS2611 = _M0Lm16lastRemovedDigitS975;
      _M0L6_2atmpS2610 = _M0L6_2atmpS2611 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2609 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2610);
    _M0L6_2atmpS2608 = *(uint64_t*)&_M0L6_2atmpS2609;
    _M0Lm6outputS976 = _M0L6_2atmpS2607 + _M0L6_2atmpS2608;
  } else {
    int32_t _M0Lm7roundUpS989 = 0;
    uint64_t _M0L6_2atmpS2635 = _M0Lm2vpS951;
    uint64_t _M0L8vpDiv100S990 = _M0L6_2atmpS2635 / 100ull;
    uint64_t _M0L6_2atmpS2634 = _M0Lm2vmS952;
    uint64_t _M0L8vmDiv100S991 = _M0L6_2atmpS2634 / 100ull;
    uint64_t _M0L6_2atmpS2629;
    uint64_t _M0L6_2atmpS2632;
    uint64_t _M0L6_2atmpS2633;
    int32_t _M0L6_2atmpS2631;
    uint64_t _M0L6_2atmpS2630;
    if (_M0L8vpDiv100S990 > _M0L8vmDiv100S991) {
      uint64_t _M0L6_2atmpS2620 = _M0Lm2vrS950;
      uint64_t _M0L8vrDiv100S992 = _M0L6_2atmpS2620 / 100ull;
      uint64_t _M0L6_2atmpS2619 = _M0Lm2vrS950;
      int32_t _M0L6_2atmpS2616 = (int32_t)_M0L6_2atmpS2619;
      int32_t _M0L6_2atmpS2618 = (int32_t)_M0L8vrDiv100S992;
      int32_t _M0L6_2atmpS2617 = 100 * _M0L6_2atmpS2618;
      int32_t _M0L8vrMod100S993 = _M0L6_2atmpS2616 - _M0L6_2atmpS2617;
      int32_t _M0L6_2atmpS2615;
      _M0Lm7roundUpS989 = _M0L8vrMod100S993 >= 50;
      _M0Lm2vrS950 = _M0L8vrDiv100S992;
      _M0Lm2vpS951 = _M0L8vpDiv100S990;
      _M0Lm2vmS952 = _M0L8vmDiv100S991;
      _M0L6_2atmpS2615 = _M0Lm7removedS974;
      _M0Lm7removedS974 = _M0L6_2atmpS2615 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2628 = _M0Lm2vpS951;
      uint64_t _M0L7vpDiv10S994 = _M0L6_2atmpS2628 / 10ull;
      uint64_t _M0L6_2atmpS2627 = _M0Lm2vmS952;
      uint64_t _M0L7vmDiv10S995 = _M0L6_2atmpS2627 / 10ull;
      uint64_t _M0L6_2atmpS2626;
      uint64_t _M0L7vrDiv10S997;
      uint64_t _M0L6_2atmpS2625;
      int32_t _M0L6_2atmpS2622;
      int32_t _M0L6_2atmpS2624;
      int32_t _M0L6_2atmpS2623;
      int32_t _M0L7vrMod10S998;
      int32_t _M0L6_2atmpS2621;
      if (_M0L7vpDiv10S994 <= _M0L7vmDiv10S995) {
        break;
      }
      _M0L6_2atmpS2626 = _M0Lm2vrS950;
      _M0L7vrDiv10S997 = _M0L6_2atmpS2626 / 10ull;
      _M0L6_2atmpS2625 = _M0Lm2vrS950;
      _M0L6_2atmpS2622 = (int32_t)_M0L6_2atmpS2625;
      _M0L6_2atmpS2624 = (int32_t)_M0L7vrDiv10S997;
      _M0L6_2atmpS2623 = 10 * _M0L6_2atmpS2624;
      _M0L7vrMod10S998 = _M0L6_2atmpS2622 - _M0L6_2atmpS2623;
      _M0Lm7roundUpS989 = _M0L7vrMod10S998 >= 5;
      _M0Lm2vrS950 = _M0L7vrDiv10S997;
      _M0Lm2vpS951 = _M0L7vpDiv10S994;
      _M0Lm2vmS952 = _M0L7vmDiv10S995;
      _M0L6_2atmpS2621 = _M0Lm7removedS974;
      _M0Lm7removedS974 = _M0L6_2atmpS2621 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2629 = _M0Lm2vrS950;
    _M0L6_2atmpS2632 = _M0Lm2vrS950;
    _M0L6_2atmpS2633 = _M0Lm2vmS952;
    _M0L6_2atmpS2631
    = _M0L6_2atmpS2632 == _M0L6_2atmpS2633 || _M0Lm7roundUpS989;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2630 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2631);
    _M0Lm6outputS976 = _M0L6_2atmpS2629 + _M0L6_2atmpS2630;
  }
  _M0L6_2atmpS2637 = _M0Lm3e10S953;
  _M0L6_2atmpS2638 = _M0Lm7removedS974;
  _M0L3expS999 = _M0L6_2atmpS2637 + _M0L6_2atmpS2638;
  _M0L6_2atmpS2636 = _M0Lm6outputS976;
  _block_3855
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3855)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3855->$0 = _M0L6_2atmpS2636;
  _block_3855->$1 = _M0L3expS999;
  return _block_3855;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS942) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS942) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS941) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS941) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS940) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS940) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS939) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS939 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS939 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS939 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS939 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS939 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS939 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS939 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS939 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS939 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS939 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS939 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS939 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS939 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS939 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS939 >= 100ull) {
    return 3;
  }
  if (_M0L1vS939 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS922) {
  int32_t _M0L6_2atmpS2537;
  int32_t _M0L6_2atmpS2536;
  int32_t _M0L4baseS921;
  int32_t _M0L5base2S923;
  int32_t _M0L6offsetS924;
  int32_t _M0L6_2atmpS2535;
  uint64_t _M0L4mul0S925;
  int32_t _M0L6_2atmpS2534;
  int32_t _M0L6_2atmpS2533;
  uint64_t _M0L4mul1S926;
  uint64_t _M0L1mS927;
  struct _M0TPB7Umul128 _M0L7_2abindS928;
  uint64_t _M0L7_2alow1S929;
  uint64_t _M0L8_2ahigh1S930;
  struct _M0TPB7Umul128 _M0L7_2abindS931;
  uint64_t _M0L7_2alow0S932;
  uint64_t _M0L8_2ahigh0S933;
  uint64_t _M0L3sumS934;
  uint64_t _M0Lm5high1S935;
  int32_t _M0L6_2atmpS2531;
  int32_t _M0L6_2atmpS2532;
  int32_t _M0L5deltaS936;
  uint64_t _M0L6_2atmpS2530;
  uint64_t _M0L6_2atmpS2522;
  int32_t _M0L6_2atmpS2529;
  uint32_t _M0L6_2atmpS2526;
  int32_t _M0L6_2atmpS2528;
  int32_t _M0L6_2atmpS2527;
  uint32_t _M0L6_2atmpS2525;
  uint32_t _M0L6_2atmpS2524;
  uint64_t _M0L6_2atmpS2523;
  uint64_t _M0L1aS937;
  uint64_t _M0L6_2atmpS2521;
  uint64_t _M0L1bS938;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2537 = _M0L1iS922 + 26;
  _M0L6_2atmpS2536 = _M0L6_2atmpS2537 - 1;
  _M0L4baseS921 = _M0L6_2atmpS2536 / 26;
  _M0L5base2S923 = _M0L4baseS921 * 26;
  _M0L6offsetS924 = _M0L5base2S923 - _M0L1iS922;
  _M0L6_2atmpS2535 = _M0L4baseS921 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S925
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2535);
  _M0L6_2atmpS2534 = _M0L4baseS921 * 2;
  _M0L6_2atmpS2533 = _M0L6_2atmpS2534 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S926
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2533);
  if (_M0L6offsetS924 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S925, _M0L4mul1S926};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS927
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS924);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS928 = _M0FPB7umul128(_M0L1mS927, _M0L4mul1S926);
  _M0L7_2alow1S929 = _M0L7_2abindS928.$0;
  _M0L8_2ahigh1S930 = _M0L7_2abindS928.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS931 = _M0FPB7umul128(_M0L1mS927, _M0L4mul0S925);
  _M0L7_2alow0S932 = _M0L7_2abindS931.$0;
  _M0L8_2ahigh0S933 = _M0L7_2abindS931.$1;
  _M0L3sumS934 = _M0L8_2ahigh0S933 + _M0L7_2alow1S929;
  _M0Lm5high1S935 = _M0L8_2ahigh1S930;
  if (_M0L3sumS934 < _M0L8_2ahigh0S933) {
    uint64_t _M0L6_2atmpS2520 = _M0Lm5high1S935;
    _M0Lm5high1S935 = _M0L6_2atmpS2520 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2531 = _M0FPB8pow5bits(_M0L5base2S923);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2532 = _M0FPB8pow5bits(_M0L1iS922);
  _M0L5deltaS936 = _M0L6_2atmpS2531 - _M0L6_2atmpS2532;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2530
  = _M0FPB13shiftright128(_M0L7_2alow0S932, _M0L3sumS934, _M0L5deltaS936);
  _M0L6_2atmpS2522 = _M0L6_2atmpS2530 + 1ull;
  _M0L6_2atmpS2529 = _M0L1iS922 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2526
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2529);
  _M0L6_2atmpS2528 = _M0L1iS922 % 16;
  _M0L6_2atmpS2527 = _M0L6_2atmpS2528 << 1;
  _M0L6_2atmpS2525 = _M0L6_2atmpS2526 >> (_M0L6_2atmpS2527 & 31);
  _M0L6_2atmpS2524 = _M0L6_2atmpS2525 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2523 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2524);
  _M0L1aS937 = _M0L6_2atmpS2522 + _M0L6_2atmpS2523;
  _M0L6_2atmpS2521 = _M0Lm5high1S935;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS938
  = _M0FPB13shiftright128(_M0L3sumS934, _M0L6_2atmpS2521, _M0L5deltaS936);
  return (struct _M0TPB8Pow5Pair){_M0L1aS937, _M0L1bS938};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS904) {
  int32_t _M0L4baseS903;
  int32_t _M0L5base2S905;
  int32_t _M0L6offsetS906;
  int32_t _M0L6_2atmpS2519;
  uint64_t _M0L4mul0S907;
  int32_t _M0L6_2atmpS2518;
  int32_t _M0L6_2atmpS2517;
  uint64_t _M0L4mul1S908;
  uint64_t _M0L1mS909;
  struct _M0TPB7Umul128 _M0L7_2abindS910;
  uint64_t _M0L7_2alow1S911;
  uint64_t _M0L8_2ahigh1S912;
  struct _M0TPB7Umul128 _M0L7_2abindS913;
  uint64_t _M0L7_2alow0S914;
  uint64_t _M0L8_2ahigh0S915;
  uint64_t _M0L3sumS916;
  uint64_t _M0Lm5high1S917;
  int32_t _M0L6_2atmpS2515;
  int32_t _M0L6_2atmpS2516;
  int32_t _M0L5deltaS918;
  uint64_t _M0L6_2atmpS2507;
  int32_t _M0L6_2atmpS2514;
  uint32_t _M0L6_2atmpS2511;
  int32_t _M0L6_2atmpS2513;
  int32_t _M0L6_2atmpS2512;
  uint32_t _M0L6_2atmpS2510;
  uint32_t _M0L6_2atmpS2509;
  uint64_t _M0L6_2atmpS2508;
  uint64_t _M0L1aS919;
  uint64_t _M0L6_2atmpS2506;
  uint64_t _M0L1bS920;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS903 = _M0L1iS904 / 26;
  _M0L5base2S905 = _M0L4baseS903 * 26;
  _M0L6offsetS906 = _M0L1iS904 - _M0L5base2S905;
  _M0L6_2atmpS2519 = _M0L4baseS903 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S907
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2519);
  _M0L6_2atmpS2518 = _M0L4baseS903 * 2;
  _M0L6_2atmpS2517 = _M0L6_2atmpS2518 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S908
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2517);
  if (_M0L6offsetS906 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S907, _M0L4mul1S908};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS909
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS906);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS910 = _M0FPB7umul128(_M0L1mS909, _M0L4mul1S908);
  _M0L7_2alow1S911 = _M0L7_2abindS910.$0;
  _M0L8_2ahigh1S912 = _M0L7_2abindS910.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS913 = _M0FPB7umul128(_M0L1mS909, _M0L4mul0S907);
  _M0L7_2alow0S914 = _M0L7_2abindS913.$0;
  _M0L8_2ahigh0S915 = _M0L7_2abindS913.$1;
  _M0L3sumS916 = _M0L8_2ahigh0S915 + _M0L7_2alow1S911;
  _M0Lm5high1S917 = _M0L8_2ahigh1S912;
  if (_M0L3sumS916 < _M0L8_2ahigh0S915) {
    uint64_t _M0L6_2atmpS2505 = _M0Lm5high1S917;
    _M0Lm5high1S917 = _M0L6_2atmpS2505 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2515 = _M0FPB8pow5bits(_M0L1iS904);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2516 = _M0FPB8pow5bits(_M0L5base2S905);
  _M0L5deltaS918 = _M0L6_2atmpS2515 - _M0L6_2atmpS2516;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2507
  = _M0FPB13shiftright128(_M0L7_2alow0S914, _M0L3sumS916, _M0L5deltaS918);
  _M0L6_2atmpS2514 = _M0L1iS904 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2511
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2514);
  _M0L6_2atmpS2513 = _M0L1iS904 % 16;
  _M0L6_2atmpS2512 = _M0L6_2atmpS2513 << 1;
  _M0L6_2atmpS2510 = _M0L6_2atmpS2511 >> (_M0L6_2atmpS2512 & 31);
  _M0L6_2atmpS2509 = _M0L6_2atmpS2510 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2508 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2509);
  _M0L1aS919 = _M0L6_2atmpS2507 + _M0L6_2atmpS2508;
  _M0L6_2atmpS2506 = _M0Lm5high1S917;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS920
  = _M0FPB13shiftright128(_M0L3sumS916, _M0L6_2atmpS2506, _M0L5deltaS918);
  return (struct _M0TPB8Pow5Pair){_M0L1aS919, _M0L1bS920};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS877,
  struct _M0TPB8Pow5Pair _M0L3mulS874,
  int32_t _M0L1jS890,
  int32_t _M0L7mmShiftS892
) {
  uint64_t _M0L7_2amul0S873;
  uint64_t _M0L7_2amul1S875;
  uint64_t _M0L1mS876;
  struct _M0TPB7Umul128 _M0L7_2abindS878;
  uint64_t _M0L5_2aloS879;
  uint64_t _M0L6_2atmpS880;
  struct _M0TPB7Umul128 _M0L7_2abindS881;
  uint64_t _M0L6_2alo2S882;
  uint64_t _M0L6_2ahi2S883;
  uint64_t _M0L3midS884;
  uint64_t _M0L6_2atmpS2504;
  uint64_t _M0L2hiS885;
  uint64_t _M0L3lo2S886;
  uint64_t _M0L6_2atmpS2502;
  uint64_t _M0L6_2atmpS2503;
  uint64_t _M0L4mid2S887;
  uint64_t _M0L6_2atmpS2501;
  uint64_t _M0L3hi2S888;
  int32_t _M0L6_2atmpS2500;
  int32_t _M0L6_2atmpS2499;
  uint64_t _M0L2vpS889;
  uint64_t _M0Lm2vmS891;
  int32_t _M0L6_2atmpS2498;
  int32_t _M0L6_2atmpS2497;
  uint64_t _M0L2vrS902;
  uint64_t _M0L6_2atmpS2496;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S873 = _M0L3mulS874.$0;
  _M0L7_2amul1S875 = _M0L3mulS874.$1;
  _M0L1mS876 = _M0L1mS877 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS878 = _M0FPB7umul128(_M0L1mS876, _M0L7_2amul0S873);
  _M0L5_2aloS879 = _M0L7_2abindS878.$0;
  _M0L6_2atmpS880 = _M0L7_2abindS878.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS881 = _M0FPB7umul128(_M0L1mS876, _M0L7_2amul1S875);
  _M0L6_2alo2S882 = _M0L7_2abindS881.$0;
  _M0L6_2ahi2S883 = _M0L7_2abindS881.$1;
  _M0L3midS884 = _M0L6_2atmpS880 + _M0L6_2alo2S882;
  if (_M0L3midS884 < _M0L6_2atmpS880) {
    _M0L6_2atmpS2504 = 1ull;
  } else {
    _M0L6_2atmpS2504 = 0ull;
  }
  _M0L2hiS885 = _M0L6_2ahi2S883 + _M0L6_2atmpS2504;
  _M0L3lo2S886 = _M0L5_2aloS879 + _M0L7_2amul0S873;
  _M0L6_2atmpS2502 = _M0L3midS884 + _M0L7_2amul1S875;
  if (_M0L3lo2S886 < _M0L5_2aloS879) {
    _M0L6_2atmpS2503 = 1ull;
  } else {
    _M0L6_2atmpS2503 = 0ull;
  }
  _M0L4mid2S887 = _M0L6_2atmpS2502 + _M0L6_2atmpS2503;
  if (_M0L4mid2S887 < _M0L3midS884) {
    _M0L6_2atmpS2501 = 1ull;
  } else {
    _M0L6_2atmpS2501 = 0ull;
  }
  _M0L3hi2S888 = _M0L2hiS885 + _M0L6_2atmpS2501;
  _M0L6_2atmpS2500 = _M0L1jS890 - 64;
  _M0L6_2atmpS2499 = _M0L6_2atmpS2500 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS889
  = _M0FPB13shiftright128(_M0L4mid2S887, _M0L3hi2S888, _M0L6_2atmpS2499);
  _M0Lm2vmS891 = 0ull;
  if (_M0L7mmShiftS892) {
    uint64_t _M0L3lo3S893 = _M0L5_2aloS879 - _M0L7_2amul0S873;
    uint64_t _M0L6_2atmpS2486 = _M0L3midS884 - _M0L7_2amul1S875;
    uint64_t _M0L6_2atmpS2487;
    uint64_t _M0L4mid3S894;
    uint64_t _M0L6_2atmpS2485;
    uint64_t _M0L3hi3S895;
    int32_t _M0L6_2atmpS2484;
    int32_t _M0L6_2atmpS2483;
    if (_M0L5_2aloS879 < _M0L3lo3S893) {
      _M0L6_2atmpS2487 = 1ull;
    } else {
      _M0L6_2atmpS2487 = 0ull;
    }
    _M0L4mid3S894 = _M0L6_2atmpS2486 - _M0L6_2atmpS2487;
    if (_M0L3midS884 < _M0L4mid3S894) {
      _M0L6_2atmpS2485 = 1ull;
    } else {
      _M0L6_2atmpS2485 = 0ull;
    }
    _M0L3hi3S895 = _M0L2hiS885 - _M0L6_2atmpS2485;
    _M0L6_2atmpS2484 = _M0L1jS890 - 64;
    _M0L6_2atmpS2483 = _M0L6_2atmpS2484 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS891
    = _M0FPB13shiftright128(_M0L4mid3S894, _M0L3hi3S895, _M0L6_2atmpS2483);
  } else {
    uint64_t _M0L3lo3S896 = _M0L5_2aloS879 + _M0L5_2aloS879;
    uint64_t _M0L6_2atmpS2494 = _M0L3midS884 + _M0L3midS884;
    uint64_t _M0L6_2atmpS2495;
    uint64_t _M0L4mid3S897;
    uint64_t _M0L6_2atmpS2492;
    uint64_t _M0L6_2atmpS2493;
    uint64_t _M0L3hi3S898;
    uint64_t _M0L3lo4S899;
    uint64_t _M0L6_2atmpS2490;
    uint64_t _M0L6_2atmpS2491;
    uint64_t _M0L4mid4S900;
    uint64_t _M0L6_2atmpS2489;
    uint64_t _M0L3hi4S901;
    int32_t _M0L6_2atmpS2488;
    if (_M0L3lo3S896 < _M0L5_2aloS879) {
      _M0L6_2atmpS2495 = 1ull;
    } else {
      _M0L6_2atmpS2495 = 0ull;
    }
    _M0L4mid3S897 = _M0L6_2atmpS2494 + _M0L6_2atmpS2495;
    _M0L6_2atmpS2492 = _M0L2hiS885 + _M0L2hiS885;
    if (_M0L4mid3S897 < _M0L3midS884) {
      _M0L6_2atmpS2493 = 1ull;
    } else {
      _M0L6_2atmpS2493 = 0ull;
    }
    _M0L3hi3S898 = _M0L6_2atmpS2492 + _M0L6_2atmpS2493;
    _M0L3lo4S899 = _M0L3lo3S896 - _M0L7_2amul0S873;
    _M0L6_2atmpS2490 = _M0L4mid3S897 - _M0L7_2amul1S875;
    if (_M0L3lo3S896 < _M0L3lo4S899) {
      _M0L6_2atmpS2491 = 1ull;
    } else {
      _M0L6_2atmpS2491 = 0ull;
    }
    _M0L4mid4S900 = _M0L6_2atmpS2490 - _M0L6_2atmpS2491;
    if (_M0L4mid3S897 < _M0L4mid4S900) {
      _M0L6_2atmpS2489 = 1ull;
    } else {
      _M0L6_2atmpS2489 = 0ull;
    }
    _M0L3hi4S901 = _M0L3hi3S898 - _M0L6_2atmpS2489;
    _M0L6_2atmpS2488 = _M0L1jS890 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS891
    = _M0FPB13shiftright128(_M0L4mid4S900, _M0L3hi4S901, _M0L6_2atmpS2488);
  }
  _M0L6_2atmpS2498 = _M0L1jS890 - 64;
  _M0L6_2atmpS2497 = _M0L6_2atmpS2498 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS902
  = _M0FPB13shiftright128(_M0L3midS884, _M0L2hiS885, _M0L6_2atmpS2497);
  _M0L6_2atmpS2496 = _M0Lm2vmS891;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS902,
                                                _M0L2vpS889,
                                                _M0L6_2atmpS2496};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS871,
  int32_t _M0L1pS872
) {
  uint64_t _M0L6_2atmpS2482;
  uint64_t _M0L6_2atmpS2481;
  uint64_t _M0L6_2atmpS2480;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2482 = 1ull << (_M0L1pS872 & 63);
  _M0L6_2atmpS2481 = _M0L6_2atmpS2482 - 1ull;
  _M0L6_2atmpS2480 = _M0L5valueS871 & _M0L6_2atmpS2481;
  return _M0L6_2atmpS2480 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS869,
  int32_t _M0L1pS870
) {
  int32_t _M0L6_2atmpS2479;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2479 = _M0FPB10pow5Factor(_M0L5valueS869);
  return _M0L6_2atmpS2479 >= _M0L1pS870;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS865) {
  uint64_t _M0L6_2atmpS2467;
  uint64_t _M0L6_2atmpS2468;
  uint64_t _M0L6_2atmpS2469;
  uint64_t _M0L6_2atmpS2470;
  int32_t _M0Lm5countS866;
  uint64_t _M0Lm5valueS867;
  uint64_t _M0L6_2atmpS2478;
  moonbit_string_t _M0L6_2atmpS2477;
  moonbit_string_t _M0L6_2atmpS3401;
  moonbit_string_t _M0L6_2atmpS2476;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2467 = _M0L5valueS865 % 5ull;
  if (_M0L6_2atmpS2467 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2468 = _M0L5valueS865 % 25ull;
  if (_M0L6_2atmpS2468 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2469 = _M0L5valueS865 % 125ull;
  if (_M0L6_2atmpS2469 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2470 = _M0L5valueS865 % 625ull;
  if (_M0L6_2atmpS2470 != 0ull) {
    return 3;
  }
  _M0Lm5countS866 = 4;
  _M0Lm5valueS867 = _M0L5valueS865 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2471 = _M0Lm5valueS867;
    if (_M0L6_2atmpS2471 > 0ull) {
      uint64_t _M0L6_2atmpS2473 = _M0Lm5valueS867;
      uint64_t _M0L6_2atmpS2472 = _M0L6_2atmpS2473 % 5ull;
      uint64_t _M0L6_2atmpS2474;
      int32_t _M0L6_2atmpS2475;
      if (_M0L6_2atmpS2472 != 0ull) {
        return _M0Lm5countS866;
      }
      _M0L6_2atmpS2474 = _M0Lm5valueS867;
      _M0Lm5valueS867 = _M0L6_2atmpS2474 / 5ull;
      _M0L6_2atmpS2475 = _M0Lm5countS866;
      _M0Lm5countS866 = _M0L6_2atmpS2475 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2478 = _M0Lm5valueS867;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2477
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2478);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3401
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_75.data, _M0L6_2atmpS2477);
  moonbit_decref(_M0L6_2atmpS2477);
  _M0L6_2atmpS2476 = _M0L6_2atmpS3401;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2476, (moonbit_string_t)moonbit_string_literal_76.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS864,
  uint64_t _M0L2hiS862,
  int32_t _M0L4distS863
) {
  int32_t _M0L6_2atmpS2466;
  uint64_t _M0L6_2atmpS2464;
  uint64_t _M0L6_2atmpS2465;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2466 = 64 - _M0L4distS863;
  _M0L6_2atmpS2464 = _M0L2hiS862 << (_M0L6_2atmpS2466 & 63);
  _M0L6_2atmpS2465 = _M0L2loS864 >> (_M0L4distS863 & 63);
  return _M0L6_2atmpS2464 | _M0L6_2atmpS2465;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS852,
  uint64_t _M0L1bS855
) {
  uint64_t _M0L3aLoS851;
  uint64_t _M0L3aHiS853;
  uint64_t _M0L3bLoS854;
  uint64_t _M0L3bHiS856;
  uint64_t _M0L1xS857;
  uint64_t _M0L6_2atmpS2462;
  uint64_t _M0L6_2atmpS2463;
  uint64_t _M0L1yS858;
  uint64_t _M0L6_2atmpS2460;
  uint64_t _M0L6_2atmpS2461;
  uint64_t _M0L1zS859;
  uint64_t _M0L6_2atmpS2458;
  uint64_t _M0L6_2atmpS2459;
  uint64_t _M0L6_2atmpS2456;
  uint64_t _M0L6_2atmpS2457;
  uint64_t _M0L1wS860;
  uint64_t _M0L2loS861;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS851 = _M0L1aS852 & 4294967295ull;
  _M0L3aHiS853 = _M0L1aS852 >> 32;
  _M0L3bLoS854 = _M0L1bS855 & 4294967295ull;
  _M0L3bHiS856 = _M0L1bS855 >> 32;
  _M0L1xS857 = _M0L3aLoS851 * _M0L3bLoS854;
  _M0L6_2atmpS2462 = _M0L3aHiS853 * _M0L3bLoS854;
  _M0L6_2atmpS2463 = _M0L1xS857 >> 32;
  _M0L1yS858 = _M0L6_2atmpS2462 + _M0L6_2atmpS2463;
  _M0L6_2atmpS2460 = _M0L3aLoS851 * _M0L3bHiS856;
  _M0L6_2atmpS2461 = _M0L1yS858 & 4294967295ull;
  _M0L1zS859 = _M0L6_2atmpS2460 + _M0L6_2atmpS2461;
  _M0L6_2atmpS2458 = _M0L3aHiS853 * _M0L3bHiS856;
  _M0L6_2atmpS2459 = _M0L1yS858 >> 32;
  _M0L6_2atmpS2456 = _M0L6_2atmpS2458 + _M0L6_2atmpS2459;
  _M0L6_2atmpS2457 = _M0L1zS859 >> 32;
  _M0L1wS860 = _M0L6_2atmpS2456 + _M0L6_2atmpS2457;
  _M0L2loS861 = _M0L1aS852 * _M0L1bS855;
  return (struct _M0TPB7Umul128){_M0L2loS861, _M0L1wS860};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS846,
  int32_t _M0L4fromS850,
  int32_t _M0L2toS848
) {
  int32_t _M0L6_2atmpS2455;
  struct _M0TPB13StringBuilder* _M0L3bufS845;
  int32_t _M0L1iS847;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2455 = Moonbit_array_length(_M0L5bytesS846);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS845 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2455);
  _M0L1iS847 = _M0L4fromS850;
  while (1) {
    if (_M0L1iS847 < _M0L2toS848) {
      int32_t _M0L6_2atmpS2453;
      int32_t _M0L6_2atmpS2452;
      int32_t _M0L6_2atmpS2454;
      if (
        _M0L1iS847 < 0 || _M0L1iS847 >= Moonbit_array_length(_M0L5bytesS846)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2453 = (int32_t)_M0L5bytesS846[_M0L1iS847];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2452 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2453);
      moonbit_incref(_M0L3bufS845);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS845, _M0L6_2atmpS2452);
      _M0L6_2atmpS2454 = _M0L1iS847 + 1;
      _M0L1iS847 = _M0L6_2atmpS2454;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS846);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS845);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS844) {
  int32_t _M0L6_2atmpS2451;
  uint32_t _M0L6_2atmpS2450;
  uint32_t _M0L6_2atmpS2449;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2451 = _M0L1eS844 * 78913;
  _M0L6_2atmpS2450 = *(uint32_t*)&_M0L6_2atmpS2451;
  _M0L6_2atmpS2449 = _M0L6_2atmpS2450 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2449;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS843) {
  int32_t _M0L6_2atmpS2448;
  uint32_t _M0L6_2atmpS2447;
  uint32_t _M0L6_2atmpS2446;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2448 = _M0L1eS843 * 732923;
  _M0L6_2atmpS2447 = *(uint32_t*)&_M0L6_2atmpS2448;
  _M0L6_2atmpS2446 = _M0L6_2atmpS2447 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2446;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS841,
  int32_t _M0L8exponentS842,
  int32_t _M0L8mantissaS839
) {
  moonbit_string_t _M0L1sS840;
  moonbit_string_t _M0L6_2atmpS3402;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS839) {
    return (moonbit_string_t)moonbit_string_literal_77.data;
  }
  if (_M0L4signS841) {
    _M0L1sS840 = (moonbit_string_t)moonbit_string_literal_78.data;
  } else {
    _M0L1sS840 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS842) {
    moonbit_string_t _M0L6_2atmpS3403;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3403
    = moonbit_add_string(_M0L1sS840, (moonbit_string_t)moonbit_string_literal_79.data);
    moonbit_decref(_M0L1sS840);
    return _M0L6_2atmpS3403;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3402
  = moonbit_add_string(_M0L1sS840, (moonbit_string_t)moonbit_string_literal_80.data);
  moonbit_decref(_M0L1sS840);
  return _M0L6_2atmpS3402;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS838) {
  int32_t _M0L6_2atmpS2445;
  uint32_t _M0L6_2atmpS2444;
  uint32_t _M0L6_2atmpS2443;
  int32_t _M0L6_2atmpS2442;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2445 = _M0L1eS838 * 1217359;
  _M0L6_2atmpS2444 = *(uint32_t*)&_M0L6_2atmpS2445;
  _M0L6_2atmpS2443 = _M0L6_2atmpS2444 >> 19;
  _M0L6_2atmpS2442 = *(int32_t*)&_M0L6_2atmpS2443;
  return _M0L6_2atmpS2442 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS837,
  struct _M0TPB6Hasher* _M0L6hasherS836
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS836, _M0L4selfS837);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS835,
  struct _M0TPB6Hasher* _M0L6hasherS834
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS834, _M0L4selfS835);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS832,
  moonbit_string_t _M0L5valueS830
) {
  int32_t _M0L7_2abindS829;
  int32_t _M0L1iS831;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS829 = Moonbit_array_length(_M0L5valueS830);
  _M0L1iS831 = 0;
  while (1) {
    if (_M0L1iS831 < _M0L7_2abindS829) {
      int32_t _M0L6_2atmpS2440 = _M0L5valueS830[_M0L1iS831];
      int32_t _M0L6_2atmpS2439 = (int32_t)_M0L6_2atmpS2440;
      uint32_t _M0L6_2atmpS2438 = *(uint32_t*)&_M0L6_2atmpS2439;
      int32_t _M0L6_2atmpS2441;
      moonbit_incref(_M0L4selfS832);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS832, _M0L6_2atmpS2438);
      _M0L6_2atmpS2441 = _M0L1iS831 + 1;
      _M0L1iS831 = _M0L6_2atmpS2441;
      continue;
    } else {
      moonbit_decref(_M0L4selfS832);
      moonbit_decref(_M0L5valueS830);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS827,
  int32_t _M0L3idxS828
) {
  int32_t _M0L6_2atmpS3404;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3404 = _M0L4selfS827[_M0L3idxS828];
  moonbit_decref(_M0L4selfS827);
  return _M0L6_2atmpS3404;
}

void* _M0IPC15array9ArrayViewPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4selfS821
) {
  int32_t _M0L3lenS820;
  int32_t _M0L6_2atmpS2437;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3resS822;
  int32_t _M0L3endS2435;
  int32_t _M0L5startS2436;
  int32_t _M0L7_2abindS823;
  int32_t _M0L1iS824;
  void* _block_3860;
  #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0L4selfS821.$0);
  #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L3lenS820
  = _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS821);
  if (_M0L3lenS820 == 0) {
    void** _M0L6_2atmpS2428;
    struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2427;
    moonbit_decref(_M0L4selfS821.$0);
    _M0L6_2atmpS2428 = (void**)moonbit_empty_ref_array;
    _M0L6_2atmpS2427
    = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
    Moonbit_object_header(_M0L6_2atmpS2427)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2427->$0 = _M0L6_2atmpS2428;
    _M0L6_2atmpS2427->$1 = 0;
    #line 268 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json5array(_M0L6_2atmpS2427);
  }
  moonbit_incref(_M0L4selfS821.$0);
  #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2437
  = _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS821);
  #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L3resS822
  = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L6_2atmpS2437);
  _M0L3endS2435 = _M0L4selfS821.$2;
  _M0L5startS2436 = _M0L4selfS821.$1;
  _M0L7_2abindS823 = _M0L3endS2435 - _M0L5startS2436;
  _M0L1iS824 = 0;
  while (1) {
    if (_M0L1iS824 < _M0L7_2abindS823) {
      void** _M0L8_2afieldS3408 = _M0L4selfS821.$0;
      void** _M0L3bufS2432 = _M0L8_2afieldS3408;
      int32_t _M0L5startS2434 = _M0L4selfS821.$1;
      int32_t _M0L6_2atmpS2433 = _M0L5startS2434 + _M0L1iS824;
      void* _M0L6_2atmpS3407 = (void*)_M0L3bufS2432[_M0L6_2atmpS2433];
      void* _M0L1xS825 = _M0L6_2atmpS3407;
      void** _M0L8_2afieldS3406 = _M0L3resS822->$0;
      void** _M0L3bufS2429 = _M0L8_2afieldS3406;
      void* _M0L6_2atmpS2430;
      void* _M0L6_2aoldS3405;
      int32_t _M0L6_2atmpS2431;
      moonbit_incref(_M0L3bufS2429);
      moonbit_incref(_M0L1xS825);
      #line 272 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2430
      = _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(_M0L1xS825);
      _M0L6_2aoldS3405 = (void*)_M0L3bufS2429[_M0L1iS824];
      moonbit_decref(_M0L6_2aoldS3405);
      _M0L3bufS2429[_M0L1iS824] = _M0L6_2atmpS2430;
      moonbit_decref(_M0L3bufS2429);
      _M0L6_2atmpS2431 = _M0L1iS824 + 1;
      _M0L1iS824 = _M0L6_2atmpS2431;
      continue;
    } else {
      moonbit_decref(_M0L4selfS821.$0);
    }
    break;
  }
  _block_3860 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3860)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3860)->$0 = _M0L3resS822;
  return _block_3860;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal12conversation7MessageE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE* _M0L4selfS817
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2424;
  void* _block_3861;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2424
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal12conversation7MessageRPB4JsonE(_M0L4selfS817, _M0IP48clawteam8clawteam8internal12conversation7MessagePB6ToJson14to__json_2eclo);
  _block_3861 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3861)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3861)->$0 = _M0L6_2atmpS2424;
  return _block_3861;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS818
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2425;
  void* _block_3862;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2425
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamRPB4JsonE(_M0L4selfS818, _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson14to__json_2eclo);
  _block_3862 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3862)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3862)->$0 = _M0L6_2atmpS2425;
  return _block_3862;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS819
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2426;
  void* _block_3863;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2426
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(_M0L4selfS819, _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo);
  _block_3863 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3863)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3863)->$0 = _M0L6_2atmpS2426;
  return _block_3863;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal12conversation7MessageRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE* _M0L4selfS790,
  struct _M0TWRP48clawteam8clawteam8internal12conversation7MessageERPB4Json* _M0L1fS794
) {
  int32_t _M0L3lenS2408;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS789;
  int32_t _M0L7_2abindS791;
  int32_t _M0L1iS792;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2408 = _M0L4selfS790->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS789 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2408);
  _M0L7_2abindS791 = _M0L4selfS790->$1;
  _M0L1iS792 = 0;
  while (1) {
    if (_M0L1iS792 < _M0L7_2abindS791) {
      struct _M0TP48clawteam8clawteam8internal12conversation7Message** _M0L8_2afieldS3412 =
        _M0L4selfS790->$0;
      struct _M0TP48clawteam8clawteam8internal12conversation7Message** _M0L3bufS2407 =
        _M0L8_2afieldS3412;
      struct _M0TP48clawteam8clawteam8internal12conversation7Message* _M0L6_2atmpS3411 =
        (struct _M0TP48clawteam8clawteam8internal12conversation7Message*)_M0L3bufS2407[
          _M0L1iS792
        ];
      struct _M0TP48clawteam8clawteam8internal12conversation7Message* _M0L1vS793 =
        _M0L6_2atmpS3411;
      void** _M0L8_2afieldS3410 = _M0L3arrS789->$0;
      void** _M0L3bufS2404 = _M0L8_2afieldS3410;
      void* _M0L6_2atmpS2405;
      void* _M0L6_2aoldS3409;
      int32_t _M0L6_2atmpS2406;
      moonbit_incref(_M0L3bufS2404);
      moonbit_incref(_M0L1fS794);
      moonbit_incref(_M0L1vS793);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2405 = _M0L1fS794->code(_M0L1fS794, _M0L1vS793);
      _M0L6_2aoldS3409 = (void*)_M0L3bufS2404[_M0L1iS792];
      moonbit_decref(_M0L6_2aoldS3409);
      _M0L3bufS2404[_M0L1iS792] = _M0L6_2atmpS2405;
      moonbit_decref(_M0L3bufS2404);
      _M0L6_2atmpS2406 = _M0L1iS792 + 1;
      _M0L1iS792 = _M0L6_2atmpS2406;
      continue;
    } else {
      moonbit_decref(_M0L1fS794);
      moonbit_decref(_M0L4selfS790);
    }
    break;
  }
  return _M0L3arrS789;
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0MPC15array5Array3mapGRP38clawteam8clawteam2ai8ToolCallRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* _M0L4selfS797,
  struct _M0TWRP38clawteam8clawteam2ai8ToolCallERP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L1fS801
) {
  int32_t _M0L3lenS2413;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L3arrS796;
  int32_t _M0L7_2abindS798;
  int32_t _M0L1iS799;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2413 = _M0L4selfS797->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS796
  = _M0MPC15array5Array12make__uninitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L3lenS2413);
  _M0L7_2abindS798 = _M0L4selfS797->$1;
  _M0L1iS799 = 0;
  while (1) {
    if (_M0L1iS799 < _M0L7_2abindS798) {
      struct _M0TP38clawteam8clawteam2ai8ToolCall** _M0L8_2afieldS3416 =
        _M0L4selfS797->$0;
      struct _M0TP38clawteam8clawteam2ai8ToolCall** _M0L3bufS2412 =
        _M0L8_2afieldS3416;
      struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0L6_2atmpS3415 =
        (struct _M0TP38clawteam8clawteam2ai8ToolCall*)_M0L3bufS2412[
          _M0L1iS799
        ];
      struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0L1vS800 =
        _M0L6_2atmpS3415;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8_2afieldS3414 =
        _M0L3arrS796->$0;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3bufS2409 =
        _M0L8_2afieldS3414;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS2410;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2aoldS3413;
      int32_t _M0L6_2atmpS2411;
      moonbit_incref(_M0L3bufS2409);
      moonbit_incref(_M0L1fS801);
      moonbit_incref(_M0L1vS800);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2410 = _M0L1fS801->code(_M0L1fS801, _M0L1vS800);
      _M0L6_2aoldS3413
      = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3bufS2409[
          _M0L1iS799
        ];
      if (_M0L6_2aoldS3413) {
        moonbit_decref(_M0L6_2aoldS3413);
      }
      _M0L3bufS2409[_M0L1iS799] = _M0L6_2atmpS2410;
      moonbit_decref(_M0L3bufS2409);
      _M0L6_2atmpS2411 = _M0L1iS799 + 1;
      _M0L1iS799 = _M0L6_2atmpS2411;
      continue;
    } else {
      moonbit_decref(_M0L1fS801);
      moonbit_decref(_M0L4selfS797);
    }
    break;
  }
  return _M0L3arrS796;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS804,
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json* _M0L1fS808
) {
  int32_t _M0L3lenS2418;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS803;
  int32_t _M0L7_2abindS805;
  int32_t _M0L1iS806;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2418 = _M0L4selfS804->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS803 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2418);
  _M0L7_2abindS805 = _M0L4selfS804->$1;
  _M0L1iS806 = 0;
  while (1) {
    if (_M0L1iS806 < _M0L7_2abindS805) {
      void** _M0L8_2afieldS3420 = _M0L4selfS804->$0;
      void** _M0L3bufS2417 = _M0L8_2afieldS3420;
      void* _M0L6_2atmpS3419 = (void*)_M0L3bufS2417[_M0L1iS806];
      void* _M0L1vS807 = _M0L6_2atmpS3419;
      void** _M0L8_2afieldS3418 = _M0L3arrS803->$0;
      void** _M0L3bufS2414 = _M0L8_2afieldS3418;
      void* _M0L6_2atmpS2415;
      void* _M0L6_2aoldS3417;
      int32_t _M0L6_2atmpS2416;
      moonbit_incref(_M0L3bufS2414);
      moonbit_incref(_M0L1fS808);
      moonbit_incref(_M0L1vS807);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2415 = _M0L1fS808->code(_M0L1fS808, _M0L1vS807);
      _M0L6_2aoldS3417 = (void*)_M0L3bufS2414[_M0L1iS806];
      moonbit_decref(_M0L6_2aoldS3417);
      _M0L3bufS2414[_M0L1iS806] = _M0L6_2atmpS2415;
      moonbit_decref(_M0L3bufS2414);
      _M0L6_2atmpS2416 = _M0L1iS806 + 1;
      _M0L1iS806 = _M0L6_2atmpS2416;
      continue;
    } else {
      moonbit_decref(_M0L1fS808);
      moonbit_decref(_M0L4selfS804);
    }
    break;
  }
  return _M0L3arrS803;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS811,
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0L1fS815
) {
  int32_t _M0L3lenS2423;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS810;
  int32_t _M0L7_2abindS812;
  int32_t _M0L1iS813;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2423 = _M0L4selfS811->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS810 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2423);
  _M0L7_2abindS812 = _M0L4selfS811->$1;
  _M0L1iS813 = 0;
  while (1) {
    if (_M0L1iS813 < _M0L7_2abindS812) {
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8_2afieldS3424 =
        _M0L4selfS811->$0;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3bufS2422 =
        _M0L8_2afieldS3424;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS3423 =
        (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3bufS2422[
          _M0L1iS813
        ];
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L1vS814 =
        _M0L6_2atmpS3423;
      void** _M0L8_2afieldS3422 = _M0L3arrS810->$0;
      void** _M0L3bufS2419 = _M0L8_2afieldS3422;
      void* _M0L6_2atmpS2420;
      void* _M0L6_2aoldS3421;
      int32_t _M0L6_2atmpS2421;
      moonbit_incref(_M0L3bufS2419);
      moonbit_incref(_M0L1fS815);
      moonbit_incref(_M0L1vS814);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2420 = _M0L1fS815->code(_M0L1fS815, _M0L1vS814);
      _M0L6_2aoldS3421 = (void*)_M0L3bufS2419[_M0L1iS813];
      moonbit_decref(_M0L6_2aoldS3421);
      _M0L3bufS2419[_M0L1iS813] = _M0L6_2atmpS2420;
      moonbit_decref(_M0L3bufS2419);
      _M0L6_2atmpS2421 = _M0L1iS813 + 1;
      _M0L1iS813 = _M0L6_2atmpS2421;
      continue;
    } else {
      moonbit_decref(_M0L1fS815);
      moonbit_decref(_M0L4selfS811);
    }
    break;
  }
  return _M0L3arrS810;
}

void* _M0IPC16double6DoublePB6ToJson8to__json(double _M0L4selfS788) {
  #line 229 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS788 != _M0L4selfS788) {
    #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_77.data);
  } else {
    int64_t _tmp_3868 = 9218868437227405311ll;
    double _M0L6_2atmpS2401 = *(double*)&_tmp_3868;
    if (_M0L4selfS788 > _M0L6_2atmpS2401) {
      #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_79.data);
    } else {
      int64_t _tmp_3869 = -4503599627370497ll;
      double _M0L6_2atmpS2402 = *(double*)&_tmp_3869;
      if (_M0L4selfS788 < _M0L6_2atmpS2402) {
        #line 235 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_81.data);
      } else {
        moonbit_string_t _M0L6_2atmpS2403 = 0;
        #line 237 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        return _M0MPC14json4Json6number(_M0L4selfS788, _M0L6_2atmpS2403);
      }
    }
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS787) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS787;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS786) {
  double _M0L6_2atmpS2399;
  moonbit_string_t _M0L6_2atmpS2400;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2399 = (double)_M0L4selfS786;
  _M0L6_2atmpS2400 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2399, _M0L6_2atmpS2400);
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS785) {
  void* _block_3870;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3870 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3870)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3870)->$0 = _M0L6objectS785;
  return _block_3870;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS784) {
  void* _block_3871;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3871 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3871)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3871)->$0 = _M0L6stringS784;
  return _block_3871;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS777
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3425;
  int32_t _M0L6_2acntS3706;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2398;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS776;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__* _closure_3872;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2393;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3425 = _M0L4selfS777->$5;
  _M0L6_2acntS3706 = Moonbit_object_header(_M0L4selfS777)->rc;
  if (_M0L6_2acntS3706 > 1) {
    int32_t _M0L11_2anew__cntS3708 = _M0L6_2acntS3706 - 1;
    Moonbit_object_header(_M0L4selfS777)->rc = _M0L11_2anew__cntS3708;
    if (_M0L8_2afieldS3425) {
      moonbit_incref(_M0L8_2afieldS3425);
    }
  } else if (_M0L6_2acntS3706 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3707 = _M0L4selfS777->$0;
    moonbit_decref(_M0L8_2afieldS3707);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS777);
  }
  _M0L4headS2398 = _M0L8_2afieldS3425;
  _M0L11curr__entryS776
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS776)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS776->$0 = _M0L4headS2398;
  _closure_3872
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__));
  Moonbit_object_header(_closure_3872)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__, $0) >> 2, 1, 0);
  _closure_3872->code = &_M0MPB3Map4iterGsRPB4JsonEC2394l591;
  _closure_3872->$0 = _M0L11curr__entryS776;
  _M0L6_2atmpS2393 = (struct _M0TWEOUsRPB4JsonE*)_closure_3872;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2393);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2394l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2395
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__* _M0L14_2acasted__envS2396;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3431;
  int32_t _M0L6_2acntS3709;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS776;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3430;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS778;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2396
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2394__l591__*)_M0L6_2aenvS2395;
  _M0L8_2afieldS3431 = _M0L14_2acasted__envS2396->$0;
  _M0L6_2acntS3709 = Moonbit_object_header(_M0L14_2acasted__envS2396)->rc;
  if (_M0L6_2acntS3709 > 1) {
    int32_t _M0L11_2anew__cntS3710 = _M0L6_2acntS3709 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2396)->rc
    = _M0L11_2anew__cntS3710;
    moonbit_incref(_M0L8_2afieldS3431);
  } else if (_M0L6_2acntS3709 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2396);
  }
  _M0L11curr__entryS776 = _M0L8_2afieldS3431;
  _M0L8_2afieldS3430 = _M0L11curr__entryS776->$0;
  _M0L7_2abindS778 = _M0L8_2afieldS3430;
  if (_M0L7_2abindS778 == 0) {
    moonbit_decref(_M0L11curr__entryS776);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS779 = _M0L7_2abindS778;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS780 = _M0L7_2aSomeS779;
    moonbit_string_t _M0L8_2afieldS3429 = _M0L4_2axS780->$4;
    moonbit_string_t _M0L6_2akeyS781 = _M0L8_2afieldS3429;
    void* _M0L8_2afieldS3428 = _M0L4_2axS780->$5;
    void* _M0L8_2avalueS782 = _M0L8_2afieldS3428;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3427 = _M0L4_2axS780->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS783 = _M0L8_2afieldS3427;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3426 =
      _M0L11curr__entryS776->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2397;
    if (_M0L7_2anextS783) {
      moonbit_incref(_M0L7_2anextS783);
    }
    moonbit_incref(_M0L8_2avalueS782);
    moonbit_incref(_M0L6_2akeyS781);
    if (_M0L6_2aoldS3426) {
      moonbit_decref(_M0L6_2aoldS3426);
    }
    _M0L11curr__entryS776->$0 = _M0L7_2anextS783;
    moonbit_decref(_M0L11curr__entryS776);
    _M0L8_2atupleS2397
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2397)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2397->$0 = _M0L6_2akeyS781;
    _M0L8_2atupleS2397->$1 = _M0L8_2avalueS782;
    return _M0L8_2atupleS2397;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS775
) {
  int32_t _M0L8_2afieldS3432;
  int32_t _M0L4sizeS2392;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3432 = _M0L4selfS775->$1;
  moonbit_decref(_M0L4selfS775);
  _M0L4sizeS2392 = _M0L8_2afieldS3432;
  return _M0L4sizeS2392 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS762,
  int32_t _M0L3keyS758
) {
  int32_t _M0L4hashS757;
  int32_t _M0L14capacity__maskS2377;
  int32_t _M0L6_2atmpS2376;
  int32_t _M0L1iS759;
  int32_t _M0L3idxS760;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS757 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS758);
  _M0L14capacity__maskS2377 = _M0L4selfS762->$3;
  _M0L6_2atmpS2376 = _M0L4hashS757 & _M0L14capacity__maskS2377;
  _M0L1iS759 = 0;
  _M0L3idxS760 = _M0L6_2atmpS2376;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3436 =
      _M0L4selfS762->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2375 =
      _M0L8_2afieldS3436;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3435;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS761;
    if (
      _M0L3idxS760 < 0
      || _M0L3idxS760 >= Moonbit_array_length(_M0L7entriesS2375)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3435
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2375[
        _M0L3idxS760
      ];
    _M0L7_2abindS761 = _M0L6_2atmpS3435;
    if (_M0L7_2abindS761 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2364;
      if (_M0L7_2abindS761) {
        moonbit_incref(_M0L7_2abindS761);
      }
      moonbit_decref(_M0L4selfS762);
      if (_M0L7_2abindS761) {
        moonbit_decref(_M0L7_2abindS761);
      }
      _M0L6_2atmpS2364 = 0;
      return _M0L6_2atmpS2364;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS763 =
        _M0L7_2abindS761;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS764 =
        _M0L7_2aSomeS763;
      int32_t _M0L4hashS2366 = _M0L8_2aentryS764->$3;
      int32_t _if__result_3874;
      int32_t _M0L8_2afieldS3433;
      int32_t _M0L3pslS2369;
      int32_t _M0L6_2atmpS2371;
      int32_t _M0L6_2atmpS2373;
      int32_t _M0L14capacity__maskS2374;
      int32_t _M0L6_2atmpS2372;
      if (_M0L4hashS2366 == _M0L4hashS757) {
        int32_t _M0L3keyS2365 = _M0L8_2aentryS764->$4;
        _if__result_3874 = _M0L3keyS2365 == _M0L3keyS758;
      } else {
        _if__result_3874 = 0;
      }
      if (_if__result_3874) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3434;
        int32_t _M0L6_2acntS3711;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2368;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2367;
        moonbit_incref(_M0L8_2aentryS764);
        moonbit_decref(_M0L4selfS762);
        _M0L8_2afieldS3434 = _M0L8_2aentryS764->$5;
        _M0L6_2acntS3711 = Moonbit_object_header(_M0L8_2aentryS764)->rc;
        if (_M0L6_2acntS3711 > 1) {
          int32_t _M0L11_2anew__cntS3713 = _M0L6_2acntS3711 - 1;
          Moonbit_object_header(_M0L8_2aentryS764)->rc
          = _M0L11_2anew__cntS3713;
          moonbit_incref(_M0L8_2afieldS3434);
        } else if (_M0L6_2acntS3711 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3712 =
            _M0L8_2aentryS764->$1;
          if (_M0L8_2afieldS3712) {
            moonbit_decref(_M0L8_2afieldS3712);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS764);
        }
        _M0L5valueS2368 = _M0L8_2afieldS3434;
        _M0L6_2atmpS2367 = _M0L5valueS2368;
        return _M0L6_2atmpS2367;
      } else {
        moonbit_incref(_M0L8_2aentryS764);
      }
      _M0L8_2afieldS3433 = _M0L8_2aentryS764->$2;
      moonbit_decref(_M0L8_2aentryS764);
      _M0L3pslS2369 = _M0L8_2afieldS3433;
      if (_M0L1iS759 > _M0L3pslS2369) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2370;
        moonbit_decref(_M0L4selfS762);
        _M0L6_2atmpS2370 = 0;
        return _M0L6_2atmpS2370;
      }
      _M0L6_2atmpS2371 = _M0L1iS759 + 1;
      _M0L6_2atmpS2373 = _M0L3idxS760 + 1;
      _M0L14capacity__maskS2374 = _M0L4selfS762->$3;
      _M0L6_2atmpS2372 = _M0L6_2atmpS2373 & _M0L14capacity__maskS2374;
      _M0L1iS759 = _M0L6_2atmpS2371;
      _M0L3idxS760 = _M0L6_2atmpS2372;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS771,
  moonbit_string_t _M0L3keyS767
) {
  int32_t _M0L4hashS766;
  int32_t _M0L14capacity__maskS2391;
  int32_t _M0L6_2atmpS2390;
  int32_t _M0L1iS768;
  int32_t _M0L3idxS769;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS767);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS766 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS767);
  _M0L14capacity__maskS2391 = _M0L4selfS771->$3;
  _M0L6_2atmpS2390 = _M0L4hashS766 & _M0L14capacity__maskS2391;
  _M0L1iS768 = 0;
  _M0L3idxS769 = _M0L6_2atmpS2390;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3442 =
      _M0L4selfS771->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2389 =
      _M0L8_2afieldS3442;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3441;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS770;
    if (
      _M0L3idxS769 < 0
      || _M0L3idxS769 >= Moonbit_array_length(_M0L7entriesS2389)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3441
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2389[
        _M0L3idxS769
      ];
    _M0L7_2abindS770 = _M0L6_2atmpS3441;
    if (_M0L7_2abindS770 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2378;
      if (_M0L7_2abindS770) {
        moonbit_incref(_M0L7_2abindS770);
      }
      moonbit_decref(_M0L4selfS771);
      if (_M0L7_2abindS770) {
        moonbit_decref(_M0L7_2abindS770);
      }
      moonbit_decref(_M0L3keyS767);
      _M0L6_2atmpS2378 = 0;
      return _M0L6_2atmpS2378;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS772 =
        _M0L7_2abindS770;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS773 =
        _M0L7_2aSomeS772;
      int32_t _M0L4hashS2380 = _M0L8_2aentryS773->$3;
      int32_t _if__result_3876;
      int32_t _M0L8_2afieldS3437;
      int32_t _M0L3pslS2383;
      int32_t _M0L6_2atmpS2385;
      int32_t _M0L6_2atmpS2387;
      int32_t _M0L14capacity__maskS2388;
      int32_t _M0L6_2atmpS2386;
      if (_M0L4hashS2380 == _M0L4hashS766) {
        moonbit_string_t _M0L8_2afieldS3440 = _M0L8_2aentryS773->$4;
        moonbit_string_t _M0L3keyS2379 = _M0L8_2afieldS3440;
        int32_t _M0L6_2atmpS3439;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3439
        = moonbit_val_array_equal(_M0L3keyS2379, _M0L3keyS767);
        _if__result_3876 = _M0L6_2atmpS3439;
      } else {
        _if__result_3876 = 0;
      }
      if (_if__result_3876) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3438;
        int32_t _M0L6_2acntS3714;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2382;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2381;
        moonbit_incref(_M0L8_2aentryS773);
        moonbit_decref(_M0L4selfS771);
        moonbit_decref(_M0L3keyS767);
        _M0L8_2afieldS3438 = _M0L8_2aentryS773->$5;
        _M0L6_2acntS3714 = Moonbit_object_header(_M0L8_2aentryS773)->rc;
        if (_M0L6_2acntS3714 > 1) {
          int32_t _M0L11_2anew__cntS3717 = _M0L6_2acntS3714 - 1;
          Moonbit_object_header(_M0L8_2aentryS773)->rc
          = _M0L11_2anew__cntS3717;
          moonbit_incref(_M0L8_2afieldS3438);
        } else if (_M0L6_2acntS3714 == 1) {
          moonbit_string_t _M0L8_2afieldS3716 = _M0L8_2aentryS773->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3715;
          moonbit_decref(_M0L8_2afieldS3716);
          _M0L8_2afieldS3715 = _M0L8_2aentryS773->$1;
          if (_M0L8_2afieldS3715) {
            moonbit_decref(_M0L8_2afieldS3715);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS773);
        }
        _M0L5valueS2382 = _M0L8_2afieldS3438;
        _M0L6_2atmpS2381 = _M0L5valueS2382;
        return _M0L6_2atmpS2381;
      } else {
        moonbit_incref(_M0L8_2aentryS773);
      }
      _M0L8_2afieldS3437 = _M0L8_2aentryS773->$2;
      moonbit_decref(_M0L8_2aentryS773);
      _M0L3pslS2383 = _M0L8_2afieldS3437;
      if (_M0L1iS768 > _M0L3pslS2383) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2384;
        moonbit_decref(_M0L4selfS771);
        moonbit_decref(_M0L3keyS767);
        _M0L6_2atmpS2384 = 0;
        return _M0L6_2atmpS2384;
      }
      _M0L6_2atmpS2385 = _M0L1iS768 + 1;
      _M0L6_2atmpS2387 = _M0L3idxS769 + 1;
      _M0L14capacity__maskS2388 = _M0L4selfS771->$3;
      _M0L6_2atmpS2386 = _M0L6_2atmpS2387 & _M0L14capacity__maskS2388;
      _M0L1iS768 = _M0L6_2atmpS2385;
      _M0L3idxS769 = _M0L6_2atmpS2386;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS734
) {
  int32_t _M0L6lengthS733;
  int32_t _M0Lm8capacityS735;
  int32_t _M0L6_2atmpS2329;
  int32_t _M0L6_2atmpS2328;
  int32_t _M0L6_2atmpS2339;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS736;
  int32_t _M0L3endS2337;
  int32_t _M0L5startS2338;
  int32_t _M0L7_2abindS737;
  int32_t _M0L2__S738;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS734.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS733
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS734);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS735 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS733);
  _M0L6_2atmpS2329 = _M0Lm8capacityS735;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2328 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2329);
  if (_M0L6lengthS733 > _M0L6_2atmpS2328) {
    int32_t _M0L6_2atmpS2330 = _M0Lm8capacityS735;
    _M0Lm8capacityS735 = _M0L6_2atmpS2330 * 2;
  }
  _M0L6_2atmpS2339 = _M0Lm8capacityS735;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS736
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2339);
  _M0L3endS2337 = _M0L3arrS734.$2;
  _M0L5startS2338 = _M0L3arrS734.$1;
  _M0L7_2abindS737 = _M0L3endS2337 - _M0L5startS2338;
  _M0L2__S738 = 0;
  while (1) {
    if (_M0L2__S738 < _M0L7_2abindS737) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3446 =
        _M0L3arrS734.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2334 =
        _M0L8_2afieldS3446;
      int32_t _M0L5startS2336 = _M0L3arrS734.$1;
      int32_t _M0L6_2atmpS2335 = _M0L5startS2336 + _M0L2__S738;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3445 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2334[
          _M0L6_2atmpS2335
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS739 =
        _M0L6_2atmpS3445;
      moonbit_string_t _M0L8_2afieldS3444 = _M0L1eS739->$0;
      moonbit_string_t _M0L6_2atmpS2331 = _M0L8_2afieldS3444;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3443 =
        _M0L1eS739->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2332 =
        _M0L8_2afieldS3443;
      int32_t _M0L6_2atmpS2333;
      moonbit_incref(_M0L6_2atmpS2332);
      moonbit_incref(_M0L6_2atmpS2331);
      moonbit_incref(_M0L1mS736);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS736, _M0L6_2atmpS2331, _M0L6_2atmpS2332);
      _M0L6_2atmpS2333 = _M0L2__S738 + 1;
      _M0L2__S738 = _M0L6_2atmpS2333;
      continue;
    } else {
      moonbit_decref(_M0L3arrS734.$0);
    }
    break;
  }
  return _M0L1mS736;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS742
) {
  int32_t _M0L6lengthS741;
  int32_t _M0Lm8capacityS743;
  int32_t _M0L6_2atmpS2341;
  int32_t _M0L6_2atmpS2340;
  int32_t _M0L6_2atmpS2351;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS744;
  int32_t _M0L3endS2349;
  int32_t _M0L5startS2350;
  int32_t _M0L7_2abindS745;
  int32_t _M0L2__S746;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS742.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS741
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS742);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS743 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS741);
  _M0L6_2atmpS2341 = _M0Lm8capacityS743;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2340 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2341);
  if (_M0L6lengthS741 > _M0L6_2atmpS2340) {
    int32_t _M0L6_2atmpS2342 = _M0Lm8capacityS743;
    _M0Lm8capacityS743 = _M0L6_2atmpS2342 * 2;
  }
  _M0L6_2atmpS2351 = _M0Lm8capacityS743;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS744
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2351);
  _M0L3endS2349 = _M0L3arrS742.$2;
  _M0L5startS2350 = _M0L3arrS742.$1;
  _M0L7_2abindS745 = _M0L3endS2349 - _M0L5startS2350;
  _M0L2__S746 = 0;
  while (1) {
    if (_M0L2__S746 < _M0L7_2abindS745) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3449 =
        _M0L3arrS742.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2346 =
        _M0L8_2afieldS3449;
      int32_t _M0L5startS2348 = _M0L3arrS742.$1;
      int32_t _M0L6_2atmpS2347 = _M0L5startS2348 + _M0L2__S746;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3448 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2346[
          _M0L6_2atmpS2347
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS747 = _M0L6_2atmpS3448;
      int32_t _M0L6_2atmpS2343 = _M0L1eS747->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3447 =
        _M0L1eS747->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2344 =
        _M0L8_2afieldS3447;
      int32_t _M0L6_2atmpS2345;
      moonbit_incref(_M0L6_2atmpS2344);
      moonbit_incref(_M0L1mS744);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS744, _M0L6_2atmpS2343, _M0L6_2atmpS2344);
      _M0L6_2atmpS2345 = _M0L2__S746 + 1;
      _M0L2__S746 = _M0L6_2atmpS2345;
      continue;
    } else {
      moonbit_decref(_M0L3arrS742.$0);
    }
    break;
  }
  return _M0L1mS744;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS750
) {
  int32_t _M0L6lengthS749;
  int32_t _M0Lm8capacityS751;
  int32_t _M0L6_2atmpS2353;
  int32_t _M0L6_2atmpS2352;
  int32_t _M0L6_2atmpS2363;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS752;
  int32_t _M0L3endS2361;
  int32_t _M0L5startS2362;
  int32_t _M0L7_2abindS753;
  int32_t _M0L2__S754;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS750.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS749 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS750);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS751 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS749);
  _M0L6_2atmpS2353 = _M0Lm8capacityS751;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2352 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2353);
  if (_M0L6lengthS749 > _M0L6_2atmpS2352) {
    int32_t _M0L6_2atmpS2354 = _M0Lm8capacityS751;
    _M0Lm8capacityS751 = _M0L6_2atmpS2354 * 2;
  }
  _M0L6_2atmpS2363 = _M0Lm8capacityS751;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS752 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2363);
  _M0L3endS2361 = _M0L3arrS750.$2;
  _M0L5startS2362 = _M0L3arrS750.$1;
  _M0L7_2abindS753 = _M0L3endS2361 - _M0L5startS2362;
  _M0L2__S754 = 0;
  while (1) {
    if (_M0L2__S754 < _M0L7_2abindS753) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS3453 = _M0L3arrS750.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2358 = _M0L8_2afieldS3453;
      int32_t _M0L5startS2360 = _M0L3arrS750.$1;
      int32_t _M0L6_2atmpS2359 = _M0L5startS2360 + _M0L2__S754;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS3452 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2358[_M0L6_2atmpS2359];
      struct _M0TUsRPB4JsonE* _M0L1eS755 = _M0L6_2atmpS3452;
      moonbit_string_t _M0L8_2afieldS3451 = _M0L1eS755->$0;
      moonbit_string_t _M0L6_2atmpS2355 = _M0L8_2afieldS3451;
      void* _M0L8_2afieldS3450 = _M0L1eS755->$1;
      void* _M0L6_2atmpS2356 = _M0L8_2afieldS3450;
      int32_t _M0L6_2atmpS2357;
      moonbit_incref(_M0L6_2atmpS2356);
      moonbit_incref(_M0L6_2atmpS2355);
      moonbit_incref(_M0L1mS752);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS752, _M0L6_2atmpS2355, _M0L6_2atmpS2356);
      _M0L6_2atmpS2357 = _M0L2__S754 + 1;
      _M0L2__S754 = _M0L6_2atmpS2357;
      continue;
    } else {
      moonbit_decref(_M0L3arrS750.$0);
    }
    break;
  }
  return _M0L1mS752;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS724,
  moonbit_string_t _M0L3keyS725,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS726
) {
  int32_t _M0L6_2atmpS2325;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS725);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2325 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS725);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS724, _M0L3keyS725, _M0L5valueS726, _M0L6_2atmpS2325);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS727,
  int32_t _M0L3keyS728,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS729
) {
  int32_t _M0L6_2atmpS2326;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2326 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS728);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS727, _M0L3keyS728, _M0L5valueS729, _M0L6_2atmpS2326);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS730,
  moonbit_string_t _M0L3keyS731,
  void* _M0L5valueS732
) {
  int32_t _M0L6_2atmpS2327;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS731);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2327 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS731);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS730, _M0L3keyS731, _M0L5valueS732, _M0L6_2atmpS2327);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS692
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3460;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS691;
  int32_t _M0L8capacityS2310;
  int32_t _M0L13new__capacityS693;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2305;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2304;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3459;
  int32_t _M0L6_2atmpS2306;
  int32_t _M0L8capacityS2308;
  int32_t _M0L6_2atmpS2307;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2309;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3458;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS694;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3460 = _M0L4selfS692->$5;
  _M0L9old__headS691 = _M0L8_2afieldS3460;
  _M0L8capacityS2310 = _M0L4selfS692->$2;
  _M0L13new__capacityS693 = _M0L8capacityS2310 << 1;
  _M0L6_2atmpS2305 = 0;
  _M0L6_2atmpS2304
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS693, _M0L6_2atmpS2305);
  _M0L6_2aoldS3459 = _M0L4selfS692->$0;
  if (_M0L9old__headS691) {
    moonbit_incref(_M0L9old__headS691);
  }
  moonbit_decref(_M0L6_2aoldS3459);
  _M0L4selfS692->$0 = _M0L6_2atmpS2304;
  _M0L4selfS692->$2 = _M0L13new__capacityS693;
  _M0L6_2atmpS2306 = _M0L13new__capacityS693 - 1;
  _M0L4selfS692->$3 = _M0L6_2atmpS2306;
  _M0L8capacityS2308 = _M0L4selfS692->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2307 = _M0FPB21calc__grow__threshold(_M0L8capacityS2308);
  _M0L4selfS692->$4 = _M0L6_2atmpS2307;
  _M0L4selfS692->$1 = 0;
  _M0L6_2atmpS2309 = 0;
  _M0L6_2aoldS3458 = _M0L4selfS692->$5;
  if (_M0L6_2aoldS3458) {
    moonbit_decref(_M0L6_2aoldS3458);
  }
  _M0L4selfS692->$5 = _M0L6_2atmpS2309;
  _M0L4selfS692->$6 = -1;
  _M0L8_2aparamS694 = _M0L9old__headS691;
  while (1) {
    if (_M0L8_2aparamS694 == 0) {
      if (_M0L8_2aparamS694) {
        moonbit_decref(_M0L8_2aparamS694);
      }
      moonbit_decref(_M0L4selfS692);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS695 =
        _M0L8_2aparamS694;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS696 =
        _M0L7_2aSomeS695;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3457 =
        _M0L4_2axS696->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS697 =
        _M0L8_2afieldS3457;
      moonbit_string_t _M0L8_2afieldS3456 = _M0L4_2axS696->$4;
      moonbit_string_t _M0L6_2akeyS698 = _M0L8_2afieldS3456;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3455 =
        _M0L4_2axS696->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS699 =
        _M0L8_2afieldS3455;
      int32_t _M0L8_2afieldS3454 = _M0L4_2axS696->$3;
      int32_t _M0L6_2acntS3718 = Moonbit_object_header(_M0L4_2axS696)->rc;
      int32_t _M0L7_2ahashS700;
      if (_M0L6_2acntS3718 > 1) {
        int32_t _M0L11_2anew__cntS3719 = _M0L6_2acntS3718 - 1;
        Moonbit_object_header(_M0L4_2axS696)->rc = _M0L11_2anew__cntS3719;
        moonbit_incref(_M0L8_2avalueS699);
        moonbit_incref(_M0L6_2akeyS698);
        if (_M0L7_2anextS697) {
          moonbit_incref(_M0L7_2anextS697);
        }
      } else if (_M0L6_2acntS3718 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS696);
      }
      _M0L7_2ahashS700 = _M0L8_2afieldS3454;
      moonbit_incref(_M0L4selfS692);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS692, _M0L6_2akeyS698, _M0L8_2avalueS699, _M0L7_2ahashS700);
      _M0L8_2aparamS694 = _M0L7_2anextS697;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS703
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3466;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS702;
  int32_t _M0L8capacityS2317;
  int32_t _M0L13new__capacityS704;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2312;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2311;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3465;
  int32_t _M0L6_2atmpS2313;
  int32_t _M0L8capacityS2315;
  int32_t _M0L6_2atmpS2314;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2316;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3464;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS705;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3466 = _M0L4selfS703->$5;
  _M0L9old__headS702 = _M0L8_2afieldS3466;
  _M0L8capacityS2317 = _M0L4selfS703->$2;
  _M0L13new__capacityS704 = _M0L8capacityS2317 << 1;
  _M0L6_2atmpS2312 = 0;
  _M0L6_2atmpS2311
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS704, _M0L6_2atmpS2312);
  _M0L6_2aoldS3465 = _M0L4selfS703->$0;
  if (_M0L9old__headS702) {
    moonbit_incref(_M0L9old__headS702);
  }
  moonbit_decref(_M0L6_2aoldS3465);
  _M0L4selfS703->$0 = _M0L6_2atmpS2311;
  _M0L4selfS703->$2 = _M0L13new__capacityS704;
  _M0L6_2atmpS2313 = _M0L13new__capacityS704 - 1;
  _M0L4selfS703->$3 = _M0L6_2atmpS2313;
  _M0L8capacityS2315 = _M0L4selfS703->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2314 = _M0FPB21calc__grow__threshold(_M0L8capacityS2315);
  _M0L4selfS703->$4 = _M0L6_2atmpS2314;
  _M0L4selfS703->$1 = 0;
  _M0L6_2atmpS2316 = 0;
  _M0L6_2aoldS3464 = _M0L4selfS703->$5;
  if (_M0L6_2aoldS3464) {
    moonbit_decref(_M0L6_2aoldS3464);
  }
  _M0L4selfS703->$5 = _M0L6_2atmpS2316;
  _M0L4selfS703->$6 = -1;
  _M0L8_2aparamS705 = _M0L9old__headS702;
  while (1) {
    if (_M0L8_2aparamS705 == 0) {
      if (_M0L8_2aparamS705) {
        moonbit_decref(_M0L8_2aparamS705);
      }
      moonbit_decref(_M0L4selfS703);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS706 =
        _M0L8_2aparamS705;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS707 =
        _M0L7_2aSomeS706;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3463 =
        _M0L4_2axS707->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS708 =
        _M0L8_2afieldS3463;
      int32_t _M0L6_2akeyS709 = _M0L4_2axS707->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3462 =
        _M0L4_2axS707->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS710 =
        _M0L8_2afieldS3462;
      int32_t _M0L8_2afieldS3461 = _M0L4_2axS707->$3;
      int32_t _M0L6_2acntS3720 = Moonbit_object_header(_M0L4_2axS707)->rc;
      int32_t _M0L7_2ahashS711;
      if (_M0L6_2acntS3720 > 1) {
        int32_t _M0L11_2anew__cntS3721 = _M0L6_2acntS3720 - 1;
        Moonbit_object_header(_M0L4_2axS707)->rc = _M0L11_2anew__cntS3721;
        moonbit_incref(_M0L8_2avalueS710);
        if (_M0L7_2anextS708) {
          moonbit_incref(_M0L7_2anextS708);
        }
      } else if (_M0L6_2acntS3720 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS707);
      }
      _M0L7_2ahashS711 = _M0L8_2afieldS3461;
      moonbit_incref(_M0L4selfS703);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS703, _M0L6_2akeyS709, _M0L8_2avalueS710, _M0L7_2ahashS711);
      _M0L8_2aparamS705 = _M0L7_2anextS708;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS714
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3473;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS713;
  int32_t _M0L8capacityS2324;
  int32_t _M0L13new__capacityS715;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2319;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2318;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS3472;
  int32_t _M0L6_2atmpS2320;
  int32_t _M0L8capacityS2322;
  int32_t _M0L6_2atmpS2321;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2323;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3471;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS716;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3473 = _M0L4selfS714->$5;
  _M0L9old__headS713 = _M0L8_2afieldS3473;
  _M0L8capacityS2324 = _M0L4selfS714->$2;
  _M0L13new__capacityS715 = _M0L8capacityS2324 << 1;
  _M0L6_2atmpS2319 = 0;
  _M0L6_2atmpS2318
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS715, _M0L6_2atmpS2319);
  _M0L6_2aoldS3472 = _M0L4selfS714->$0;
  if (_M0L9old__headS713) {
    moonbit_incref(_M0L9old__headS713);
  }
  moonbit_decref(_M0L6_2aoldS3472);
  _M0L4selfS714->$0 = _M0L6_2atmpS2318;
  _M0L4selfS714->$2 = _M0L13new__capacityS715;
  _M0L6_2atmpS2320 = _M0L13new__capacityS715 - 1;
  _M0L4selfS714->$3 = _M0L6_2atmpS2320;
  _M0L8capacityS2322 = _M0L4selfS714->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2321 = _M0FPB21calc__grow__threshold(_M0L8capacityS2322);
  _M0L4selfS714->$4 = _M0L6_2atmpS2321;
  _M0L4selfS714->$1 = 0;
  _M0L6_2atmpS2323 = 0;
  _M0L6_2aoldS3471 = _M0L4selfS714->$5;
  if (_M0L6_2aoldS3471) {
    moonbit_decref(_M0L6_2aoldS3471);
  }
  _M0L4selfS714->$5 = _M0L6_2atmpS2323;
  _M0L4selfS714->$6 = -1;
  _M0L8_2aparamS716 = _M0L9old__headS713;
  while (1) {
    if (_M0L8_2aparamS716 == 0) {
      if (_M0L8_2aparamS716) {
        moonbit_decref(_M0L8_2aparamS716);
      }
      moonbit_decref(_M0L4selfS714);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS717 = _M0L8_2aparamS716;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS718 = _M0L7_2aSomeS717;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3470 = _M0L4_2axS718->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS719 = _M0L8_2afieldS3470;
      moonbit_string_t _M0L8_2afieldS3469 = _M0L4_2axS718->$4;
      moonbit_string_t _M0L6_2akeyS720 = _M0L8_2afieldS3469;
      void* _M0L8_2afieldS3468 = _M0L4_2axS718->$5;
      void* _M0L8_2avalueS721 = _M0L8_2afieldS3468;
      int32_t _M0L8_2afieldS3467 = _M0L4_2axS718->$3;
      int32_t _M0L6_2acntS3722 = Moonbit_object_header(_M0L4_2axS718)->rc;
      int32_t _M0L7_2ahashS722;
      if (_M0L6_2acntS3722 > 1) {
        int32_t _M0L11_2anew__cntS3723 = _M0L6_2acntS3722 - 1;
        Moonbit_object_header(_M0L4_2axS718)->rc = _M0L11_2anew__cntS3723;
        moonbit_incref(_M0L8_2avalueS721);
        moonbit_incref(_M0L6_2akeyS720);
        if (_M0L7_2anextS719) {
          moonbit_incref(_M0L7_2anextS719);
        }
      } else if (_M0L6_2acntS3722 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS718);
      }
      _M0L7_2ahashS722 = _M0L8_2afieldS3467;
      moonbit_incref(_M0L4selfS714);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS714, _M0L6_2akeyS720, _M0L8_2avalueS721, _M0L7_2ahashS722);
      _M0L8_2aparamS716 = _M0L7_2anextS719;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS646,
  moonbit_string_t _M0L3keyS652,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS653,
  int32_t _M0L4hashS648
) {
  int32_t _M0L14capacity__maskS2267;
  int32_t _M0L6_2atmpS2266;
  int32_t _M0L3pslS643;
  int32_t _M0L3idxS644;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2267 = _M0L4selfS646->$3;
  _M0L6_2atmpS2266 = _M0L4hashS648 & _M0L14capacity__maskS2267;
  _M0L3pslS643 = 0;
  _M0L3idxS644 = _M0L6_2atmpS2266;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3478 =
      _M0L4selfS646->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2265 =
      _M0L8_2afieldS3478;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3477;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS645;
    if (
      _M0L3idxS644 < 0
      || _M0L3idxS644 >= Moonbit_array_length(_M0L7entriesS2265)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3477
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2265[
        _M0L3idxS644
      ];
    _M0L7_2abindS645 = _M0L6_2atmpS3477;
    if (_M0L7_2abindS645 == 0) {
      int32_t _M0L4sizeS2250 = _M0L4selfS646->$1;
      int32_t _M0L8grow__atS2251 = _M0L4selfS646->$4;
      int32_t _M0L7_2abindS649;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS650;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS651;
      if (_M0L4sizeS2250 >= _M0L8grow__atS2251) {
        int32_t _M0L14capacity__maskS2253;
        int32_t _M0L6_2atmpS2252;
        moonbit_incref(_M0L4selfS646);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS646);
        _M0L14capacity__maskS2253 = _M0L4selfS646->$3;
        _M0L6_2atmpS2252 = _M0L4hashS648 & _M0L14capacity__maskS2253;
        _M0L3pslS643 = 0;
        _M0L3idxS644 = _M0L6_2atmpS2252;
        continue;
      }
      _M0L7_2abindS649 = _M0L4selfS646->$6;
      _M0L7_2abindS650 = 0;
      _M0L5entryS651
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS651)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS651->$0 = _M0L7_2abindS649;
      _M0L5entryS651->$1 = _M0L7_2abindS650;
      _M0L5entryS651->$2 = _M0L3pslS643;
      _M0L5entryS651->$3 = _M0L4hashS648;
      _M0L5entryS651->$4 = _M0L3keyS652;
      _M0L5entryS651->$5 = _M0L5valueS653;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS646, _M0L3idxS644, _M0L5entryS651);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS654 =
        _M0L7_2abindS645;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS655 =
        _M0L7_2aSomeS654;
      int32_t _M0L4hashS2255 = _M0L14_2acurr__entryS655->$3;
      int32_t _if__result_3884;
      int32_t _M0L3pslS2256;
      int32_t _M0L6_2atmpS2261;
      int32_t _M0L6_2atmpS2263;
      int32_t _M0L14capacity__maskS2264;
      int32_t _M0L6_2atmpS2262;
      if (_M0L4hashS2255 == _M0L4hashS648) {
        moonbit_string_t _M0L8_2afieldS3476 = _M0L14_2acurr__entryS655->$4;
        moonbit_string_t _M0L3keyS2254 = _M0L8_2afieldS3476;
        int32_t _M0L6_2atmpS3475;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3475
        = moonbit_val_array_equal(_M0L3keyS2254, _M0L3keyS652);
        _if__result_3884 = _M0L6_2atmpS3475;
      } else {
        _if__result_3884 = 0;
      }
      if (_if__result_3884) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3474;
        moonbit_incref(_M0L14_2acurr__entryS655);
        moonbit_decref(_M0L3keyS652);
        moonbit_decref(_M0L4selfS646);
        _M0L6_2aoldS3474 = _M0L14_2acurr__entryS655->$5;
        moonbit_decref(_M0L6_2aoldS3474);
        _M0L14_2acurr__entryS655->$5 = _M0L5valueS653;
        moonbit_decref(_M0L14_2acurr__entryS655);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS655);
      }
      _M0L3pslS2256 = _M0L14_2acurr__entryS655->$2;
      if (_M0L3pslS643 > _M0L3pslS2256) {
        int32_t _M0L4sizeS2257 = _M0L4selfS646->$1;
        int32_t _M0L8grow__atS2258 = _M0L4selfS646->$4;
        int32_t _M0L7_2abindS656;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS657;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS658;
        if (_M0L4sizeS2257 >= _M0L8grow__atS2258) {
          int32_t _M0L14capacity__maskS2260;
          int32_t _M0L6_2atmpS2259;
          moonbit_decref(_M0L14_2acurr__entryS655);
          moonbit_incref(_M0L4selfS646);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS646);
          _M0L14capacity__maskS2260 = _M0L4selfS646->$3;
          _M0L6_2atmpS2259 = _M0L4hashS648 & _M0L14capacity__maskS2260;
          _M0L3pslS643 = 0;
          _M0L3idxS644 = _M0L6_2atmpS2259;
          continue;
        }
        moonbit_incref(_M0L4selfS646);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS646, _M0L3idxS644, _M0L14_2acurr__entryS655);
        _M0L7_2abindS656 = _M0L4selfS646->$6;
        _M0L7_2abindS657 = 0;
        _M0L5entryS658
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS658)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS658->$0 = _M0L7_2abindS656;
        _M0L5entryS658->$1 = _M0L7_2abindS657;
        _M0L5entryS658->$2 = _M0L3pslS643;
        _M0L5entryS658->$3 = _M0L4hashS648;
        _M0L5entryS658->$4 = _M0L3keyS652;
        _M0L5entryS658->$5 = _M0L5valueS653;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS646, _M0L3idxS644, _M0L5entryS658);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS655);
      }
      _M0L6_2atmpS2261 = _M0L3pslS643 + 1;
      _M0L6_2atmpS2263 = _M0L3idxS644 + 1;
      _M0L14capacity__maskS2264 = _M0L4selfS646->$3;
      _M0L6_2atmpS2262 = _M0L6_2atmpS2263 & _M0L14capacity__maskS2264;
      _M0L3pslS643 = _M0L6_2atmpS2261;
      _M0L3idxS644 = _M0L6_2atmpS2262;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS662,
  int32_t _M0L3keyS668,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS669,
  int32_t _M0L4hashS664
) {
  int32_t _M0L14capacity__maskS2285;
  int32_t _M0L6_2atmpS2284;
  int32_t _M0L3pslS659;
  int32_t _M0L3idxS660;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2285 = _M0L4selfS662->$3;
  _M0L6_2atmpS2284 = _M0L4hashS664 & _M0L14capacity__maskS2285;
  _M0L3pslS659 = 0;
  _M0L3idxS660 = _M0L6_2atmpS2284;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3481 =
      _M0L4selfS662->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2283 =
      _M0L8_2afieldS3481;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3480;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS661;
    if (
      _M0L3idxS660 < 0
      || _M0L3idxS660 >= Moonbit_array_length(_M0L7entriesS2283)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3480
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2283[
        _M0L3idxS660
      ];
    _M0L7_2abindS661 = _M0L6_2atmpS3480;
    if (_M0L7_2abindS661 == 0) {
      int32_t _M0L4sizeS2268 = _M0L4selfS662->$1;
      int32_t _M0L8grow__atS2269 = _M0L4selfS662->$4;
      int32_t _M0L7_2abindS665;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS666;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS667;
      if (_M0L4sizeS2268 >= _M0L8grow__atS2269) {
        int32_t _M0L14capacity__maskS2271;
        int32_t _M0L6_2atmpS2270;
        moonbit_incref(_M0L4selfS662);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS662);
        _M0L14capacity__maskS2271 = _M0L4selfS662->$3;
        _M0L6_2atmpS2270 = _M0L4hashS664 & _M0L14capacity__maskS2271;
        _M0L3pslS659 = 0;
        _M0L3idxS660 = _M0L6_2atmpS2270;
        continue;
      }
      _M0L7_2abindS665 = _M0L4selfS662->$6;
      _M0L7_2abindS666 = 0;
      _M0L5entryS667
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS667)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS667->$0 = _M0L7_2abindS665;
      _M0L5entryS667->$1 = _M0L7_2abindS666;
      _M0L5entryS667->$2 = _M0L3pslS659;
      _M0L5entryS667->$3 = _M0L4hashS664;
      _M0L5entryS667->$4 = _M0L3keyS668;
      _M0L5entryS667->$5 = _M0L5valueS669;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS662, _M0L3idxS660, _M0L5entryS667);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS670 =
        _M0L7_2abindS661;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS671 =
        _M0L7_2aSomeS670;
      int32_t _M0L4hashS2273 = _M0L14_2acurr__entryS671->$3;
      int32_t _if__result_3886;
      int32_t _M0L3pslS2274;
      int32_t _M0L6_2atmpS2279;
      int32_t _M0L6_2atmpS2281;
      int32_t _M0L14capacity__maskS2282;
      int32_t _M0L6_2atmpS2280;
      if (_M0L4hashS2273 == _M0L4hashS664) {
        int32_t _M0L3keyS2272 = _M0L14_2acurr__entryS671->$4;
        _if__result_3886 = _M0L3keyS2272 == _M0L3keyS668;
      } else {
        _if__result_3886 = 0;
      }
      if (_if__result_3886) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3479;
        moonbit_incref(_M0L14_2acurr__entryS671);
        moonbit_decref(_M0L4selfS662);
        _M0L6_2aoldS3479 = _M0L14_2acurr__entryS671->$5;
        moonbit_decref(_M0L6_2aoldS3479);
        _M0L14_2acurr__entryS671->$5 = _M0L5valueS669;
        moonbit_decref(_M0L14_2acurr__entryS671);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS671);
      }
      _M0L3pslS2274 = _M0L14_2acurr__entryS671->$2;
      if (_M0L3pslS659 > _M0L3pslS2274) {
        int32_t _M0L4sizeS2275 = _M0L4selfS662->$1;
        int32_t _M0L8grow__atS2276 = _M0L4selfS662->$4;
        int32_t _M0L7_2abindS672;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS673;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS674;
        if (_M0L4sizeS2275 >= _M0L8grow__atS2276) {
          int32_t _M0L14capacity__maskS2278;
          int32_t _M0L6_2atmpS2277;
          moonbit_decref(_M0L14_2acurr__entryS671);
          moonbit_incref(_M0L4selfS662);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS662);
          _M0L14capacity__maskS2278 = _M0L4selfS662->$3;
          _M0L6_2atmpS2277 = _M0L4hashS664 & _M0L14capacity__maskS2278;
          _M0L3pslS659 = 0;
          _M0L3idxS660 = _M0L6_2atmpS2277;
          continue;
        }
        moonbit_incref(_M0L4selfS662);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS662, _M0L3idxS660, _M0L14_2acurr__entryS671);
        _M0L7_2abindS672 = _M0L4selfS662->$6;
        _M0L7_2abindS673 = 0;
        _M0L5entryS674
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS674)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS674->$0 = _M0L7_2abindS672;
        _M0L5entryS674->$1 = _M0L7_2abindS673;
        _M0L5entryS674->$2 = _M0L3pslS659;
        _M0L5entryS674->$3 = _M0L4hashS664;
        _M0L5entryS674->$4 = _M0L3keyS668;
        _M0L5entryS674->$5 = _M0L5valueS669;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS662, _M0L3idxS660, _M0L5entryS674);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS671);
      }
      _M0L6_2atmpS2279 = _M0L3pslS659 + 1;
      _M0L6_2atmpS2281 = _M0L3idxS660 + 1;
      _M0L14capacity__maskS2282 = _M0L4selfS662->$3;
      _M0L6_2atmpS2280 = _M0L6_2atmpS2281 & _M0L14capacity__maskS2282;
      _M0L3pslS659 = _M0L6_2atmpS2279;
      _M0L3idxS660 = _M0L6_2atmpS2280;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS678,
  moonbit_string_t _M0L3keyS684,
  void* _M0L5valueS685,
  int32_t _M0L4hashS680
) {
  int32_t _M0L14capacity__maskS2303;
  int32_t _M0L6_2atmpS2302;
  int32_t _M0L3pslS675;
  int32_t _M0L3idxS676;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2303 = _M0L4selfS678->$3;
  _M0L6_2atmpS2302 = _M0L4hashS680 & _M0L14capacity__maskS2303;
  _M0L3pslS675 = 0;
  _M0L3idxS676 = _M0L6_2atmpS2302;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3486 = _M0L4selfS678->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2301 = _M0L8_2afieldS3486;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3485;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS677;
    if (
      _M0L3idxS676 < 0
      || _M0L3idxS676 >= Moonbit_array_length(_M0L7entriesS2301)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3485
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2301[_M0L3idxS676];
    _M0L7_2abindS677 = _M0L6_2atmpS3485;
    if (_M0L7_2abindS677 == 0) {
      int32_t _M0L4sizeS2286 = _M0L4selfS678->$1;
      int32_t _M0L8grow__atS2287 = _M0L4selfS678->$4;
      int32_t _M0L7_2abindS681;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS682;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS683;
      if (_M0L4sizeS2286 >= _M0L8grow__atS2287) {
        int32_t _M0L14capacity__maskS2289;
        int32_t _M0L6_2atmpS2288;
        moonbit_incref(_M0L4selfS678);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS678);
        _M0L14capacity__maskS2289 = _M0L4selfS678->$3;
        _M0L6_2atmpS2288 = _M0L4hashS680 & _M0L14capacity__maskS2289;
        _M0L3pslS675 = 0;
        _M0L3idxS676 = _M0L6_2atmpS2288;
        continue;
      }
      _M0L7_2abindS681 = _M0L4selfS678->$6;
      _M0L7_2abindS682 = 0;
      _M0L5entryS683
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS683)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS683->$0 = _M0L7_2abindS681;
      _M0L5entryS683->$1 = _M0L7_2abindS682;
      _M0L5entryS683->$2 = _M0L3pslS675;
      _M0L5entryS683->$3 = _M0L4hashS680;
      _M0L5entryS683->$4 = _M0L3keyS684;
      _M0L5entryS683->$5 = _M0L5valueS685;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS678, _M0L3idxS676, _M0L5entryS683);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS686 = _M0L7_2abindS677;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS687 =
        _M0L7_2aSomeS686;
      int32_t _M0L4hashS2291 = _M0L14_2acurr__entryS687->$3;
      int32_t _if__result_3888;
      int32_t _M0L3pslS2292;
      int32_t _M0L6_2atmpS2297;
      int32_t _M0L6_2atmpS2299;
      int32_t _M0L14capacity__maskS2300;
      int32_t _M0L6_2atmpS2298;
      if (_M0L4hashS2291 == _M0L4hashS680) {
        moonbit_string_t _M0L8_2afieldS3484 = _M0L14_2acurr__entryS687->$4;
        moonbit_string_t _M0L3keyS2290 = _M0L8_2afieldS3484;
        int32_t _M0L6_2atmpS3483;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3483
        = moonbit_val_array_equal(_M0L3keyS2290, _M0L3keyS684);
        _if__result_3888 = _M0L6_2atmpS3483;
      } else {
        _if__result_3888 = 0;
      }
      if (_if__result_3888) {
        void* _M0L6_2aoldS3482;
        moonbit_incref(_M0L14_2acurr__entryS687);
        moonbit_decref(_M0L3keyS684);
        moonbit_decref(_M0L4selfS678);
        _M0L6_2aoldS3482 = _M0L14_2acurr__entryS687->$5;
        moonbit_decref(_M0L6_2aoldS3482);
        _M0L14_2acurr__entryS687->$5 = _M0L5valueS685;
        moonbit_decref(_M0L14_2acurr__entryS687);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS687);
      }
      _M0L3pslS2292 = _M0L14_2acurr__entryS687->$2;
      if (_M0L3pslS675 > _M0L3pslS2292) {
        int32_t _M0L4sizeS2293 = _M0L4selfS678->$1;
        int32_t _M0L8grow__atS2294 = _M0L4selfS678->$4;
        int32_t _M0L7_2abindS688;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS689;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS690;
        if (_M0L4sizeS2293 >= _M0L8grow__atS2294) {
          int32_t _M0L14capacity__maskS2296;
          int32_t _M0L6_2atmpS2295;
          moonbit_decref(_M0L14_2acurr__entryS687);
          moonbit_incref(_M0L4selfS678);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS678);
          _M0L14capacity__maskS2296 = _M0L4selfS678->$3;
          _M0L6_2atmpS2295 = _M0L4hashS680 & _M0L14capacity__maskS2296;
          _M0L3pslS675 = 0;
          _M0L3idxS676 = _M0L6_2atmpS2295;
          continue;
        }
        moonbit_incref(_M0L4selfS678);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS678, _M0L3idxS676, _M0L14_2acurr__entryS687);
        _M0L7_2abindS688 = _M0L4selfS678->$6;
        _M0L7_2abindS689 = 0;
        _M0L5entryS690
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS690)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS690->$0 = _M0L7_2abindS688;
        _M0L5entryS690->$1 = _M0L7_2abindS689;
        _M0L5entryS690->$2 = _M0L3pslS675;
        _M0L5entryS690->$3 = _M0L4hashS680;
        _M0L5entryS690->$4 = _M0L3keyS684;
        _M0L5entryS690->$5 = _M0L5valueS685;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS678, _M0L3idxS676, _M0L5entryS690);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS687);
      }
      _M0L6_2atmpS2297 = _M0L3pslS675 + 1;
      _M0L6_2atmpS2299 = _M0L3idxS676 + 1;
      _M0L14capacity__maskS2300 = _M0L4selfS678->$3;
      _M0L6_2atmpS2298 = _M0L6_2atmpS2299 & _M0L14capacity__maskS2300;
      _M0L3pslS675 = _M0L6_2atmpS2297;
      _M0L3idxS676 = _M0L6_2atmpS2298;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS617,
  int32_t _M0L3idxS622,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS621
) {
  int32_t _M0L3pslS2217;
  int32_t _M0L6_2atmpS2213;
  int32_t _M0L6_2atmpS2215;
  int32_t _M0L14capacity__maskS2216;
  int32_t _M0L6_2atmpS2214;
  int32_t _M0L3pslS613;
  int32_t _M0L3idxS614;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS615;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2217 = _M0L5entryS621->$2;
  _M0L6_2atmpS2213 = _M0L3pslS2217 + 1;
  _M0L6_2atmpS2215 = _M0L3idxS622 + 1;
  _M0L14capacity__maskS2216 = _M0L4selfS617->$3;
  _M0L6_2atmpS2214 = _M0L6_2atmpS2215 & _M0L14capacity__maskS2216;
  _M0L3pslS613 = _M0L6_2atmpS2213;
  _M0L3idxS614 = _M0L6_2atmpS2214;
  _M0L5entryS615 = _M0L5entryS621;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3488 =
      _M0L4selfS617->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2212 =
      _M0L8_2afieldS3488;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3487;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS616;
    if (
      _M0L3idxS614 < 0
      || _M0L3idxS614 >= Moonbit_array_length(_M0L7entriesS2212)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3487
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2212[
        _M0L3idxS614
      ];
    _M0L7_2abindS616 = _M0L6_2atmpS3487;
    if (_M0L7_2abindS616 == 0) {
      _M0L5entryS615->$2 = _M0L3pslS613;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS617, _M0L5entryS615, _M0L3idxS614);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS619 =
        _M0L7_2abindS616;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS620 =
        _M0L7_2aSomeS619;
      int32_t _M0L3pslS2202 = _M0L14_2acurr__entryS620->$2;
      if (_M0L3pslS613 > _M0L3pslS2202) {
        int32_t _M0L3pslS2207;
        int32_t _M0L6_2atmpS2203;
        int32_t _M0L6_2atmpS2205;
        int32_t _M0L14capacity__maskS2206;
        int32_t _M0L6_2atmpS2204;
        _M0L5entryS615->$2 = _M0L3pslS613;
        moonbit_incref(_M0L14_2acurr__entryS620);
        moonbit_incref(_M0L4selfS617);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS617, _M0L5entryS615, _M0L3idxS614);
        _M0L3pslS2207 = _M0L14_2acurr__entryS620->$2;
        _M0L6_2atmpS2203 = _M0L3pslS2207 + 1;
        _M0L6_2atmpS2205 = _M0L3idxS614 + 1;
        _M0L14capacity__maskS2206 = _M0L4selfS617->$3;
        _M0L6_2atmpS2204 = _M0L6_2atmpS2205 & _M0L14capacity__maskS2206;
        _M0L3pslS613 = _M0L6_2atmpS2203;
        _M0L3idxS614 = _M0L6_2atmpS2204;
        _M0L5entryS615 = _M0L14_2acurr__entryS620;
        continue;
      } else {
        int32_t _M0L6_2atmpS2208 = _M0L3pslS613 + 1;
        int32_t _M0L6_2atmpS2210 = _M0L3idxS614 + 1;
        int32_t _M0L14capacity__maskS2211 = _M0L4selfS617->$3;
        int32_t _M0L6_2atmpS2209 =
          _M0L6_2atmpS2210 & _M0L14capacity__maskS2211;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3890 =
          _M0L5entryS615;
        _M0L3pslS613 = _M0L6_2atmpS2208;
        _M0L3idxS614 = _M0L6_2atmpS2209;
        _M0L5entryS615 = _tmp_3890;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS627,
  int32_t _M0L3idxS632,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS631
) {
  int32_t _M0L3pslS2233;
  int32_t _M0L6_2atmpS2229;
  int32_t _M0L6_2atmpS2231;
  int32_t _M0L14capacity__maskS2232;
  int32_t _M0L6_2atmpS2230;
  int32_t _M0L3pslS623;
  int32_t _M0L3idxS624;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS625;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2233 = _M0L5entryS631->$2;
  _M0L6_2atmpS2229 = _M0L3pslS2233 + 1;
  _M0L6_2atmpS2231 = _M0L3idxS632 + 1;
  _M0L14capacity__maskS2232 = _M0L4selfS627->$3;
  _M0L6_2atmpS2230 = _M0L6_2atmpS2231 & _M0L14capacity__maskS2232;
  _M0L3pslS623 = _M0L6_2atmpS2229;
  _M0L3idxS624 = _M0L6_2atmpS2230;
  _M0L5entryS625 = _M0L5entryS631;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3490 =
      _M0L4selfS627->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2228 =
      _M0L8_2afieldS3490;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3489;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS626;
    if (
      _M0L3idxS624 < 0
      || _M0L3idxS624 >= Moonbit_array_length(_M0L7entriesS2228)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3489
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2228[
        _M0L3idxS624
      ];
    _M0L7_2abindS626 = _M0L6_2atmpS3489;
    if (_M0L7_2abindS626 == 0) {
      _M0L5entryS625->$2 = _M0L3pslS623;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L5entryS625, _M0L3idxS624);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS629 =
        _M0L7_2abindS626;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS630 =
        _M0L7_2aSomeS629;
      int32_t _M0L3pslS2218 = _M0L14_2acurr__entryS630->$2;
      if (_M0L3pslS623 > _M0L3pslS2218) {
        int32_t _M0L3pslS2223;
        int32_t _M0L6_2atmpS2219;
        int32_t _M0L6_2atmpS2221;
        int32_t _M0L14capacity__maskS2222;
        int32_t _M0L6_2atmpS2220;
        _M0L5entryS625->$2 = _M0L3pslS623;
        moonbit_incref(_M0L14_2acurr__entryS630);
        moonbit_incref(_M0L4selfS627);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L5entryS625, _M0L3idxS624);
        _M0L3pslS2223 = _M0L14_2acurr__entryS630->$2;
        _M0L6_2atmpS2219 = _M0L3pslS2223 + 1;
        _M0L6_2atmpS2221 = _M0L3idxS624 + 1;
        _M0L14capacity__maskS2222 = _M0L4selfS627->$3;
        _M0L6_2atmpS2220 = _M0L6_2atmpS2221 & _M0L14capacity__maskS2222;
        _M0L3pslS623 = _M0L6_2atmpS2219;
        _M0L3idxS624 = _M0L6_2atmpS2220;
        _M0L5entryS625 = _M0L14_2acurr__entryS630;
        continue;
      } else {
        int32_t _M0L6_2atmpS2224 = _M0L3pslS623 + 1;
        int32_t _M0L6_2atmpS2226 = _M0L3idxS624 + 1;
        int32_t _M0L14capacity__maskS2227 = _M0L4selfS627->$3;
        int32_t _M0L6_2atmpS2225 =
          _M0L6_2atmpS2226 & _M0L14capacity__maskS2227;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3892 =
          _M0L5entryS625;
        _M0L3pslS623 = _M0L6_2atmpS2224;
        _M0L3idxS624 = _M0L6_2atmpS2225;
        _M0L5entryS625 = _tmp_3892;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS637,
  int32_t _M0L3idxS642,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS641
) {
  int32_t _M0L3pslS2249;
  int32_t _M0L6_2atmpS2245;
  int32_t _M0L6_2atmpS2247;
  int32_t _M0L14capacity__maskS2248;
  int32_t _M0L6_2atmpS2246;
  int32_t _M0L3pslS633;
  int32_t _M0L3idxS634;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS635;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2249 = _M0L5entryS641->$2;
  _M0L6_2atmpS2245 = _M0L3pslS2249 + 1;
  _M0L6_2atmpS2247 = _M0L3idxS642 + 1;
  _M0L14capacity__maskS2248 = _M0L4selfS637->$3;
  _M0L6_2atmpS2246 = _M0L6_2atmpS2247 & _M0L14capacity__maskS2248;
  _M0L3pslS633 = _M0L6_2atmpS2245;
  _M0L3idxS634 = _M0L6_2atmpS2246;
  _M0L5entryS635 = _M0L5entryS641;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3492 = _M0L4selfS637->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2244 = _M0L8_2afieldS3492;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3491;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS636;
    if (
      _M0L3idxS634 < 0
      || _M0L3idxS634 >= Moonbit_array_length(_M0L7entriesS2244)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3491
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2244[_M0L3idxS634];
    _M0L7_2abindS636 = _M0L6_2atmpS3491;
    if (_M0L7_2abindS636 == 0) {
      _M0L5entryS635->$2 = _M0L3pslS633;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS637, _M0L5entryS635, _M0L3idxS634);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS639 = _M0L7_2abindS636;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS640 =
        _M0L7_2aSomeS639;
      int32_t _M0L3pslS2234 = _M0L14_2acurr__entryS640->$2;
      if (_M0L3pslS633 > _M0L3pslS2234) {
        int32_t _M0L3pslS2239;
        int32_t _M0L6_2atmpS2235;
        int32_t _M0L6_2atmpS2237;
        int32_t _M0L14capacity__maskS2238;
        int32_t _M0L6_2atmpS2236;
        _M0L5entryS635->$2 = _M0L3pslS633;
        moonbit_incref(_M0L14_2acurr__entryS640);
        moonbit_incref(_M0L4selfS637);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS637, _M0L5entryS635, _M0L3idxS634);
        _M0L3pslS2239 = _M0L14_2acurr__entryS640->$2;
        _M0L6_2atmpS2235 = _M0L3pslS2239 + 1;
        _M0L6_2atmpS2237 = _M0L3idxS634 + 1;
        _M0L14capacity__maskS2238 = _M0L4selfS637->$3;
        _M0L6_2atmpS2236 = _M0L6_2atmpS2237 & _M0L14capacity__maskS2238;
        _M0L3pslS633 = _M0L6_2atmpS2235;
        _M0L3idxS634 = _M0L6_2atmpS2236;
        _M0L5entryS635 = _M0L14_2acurr__entryS640;
        continue;
      } else {
        int32_t _M0L6_2atmpS2240 = _M0L3pslS633 + 1;
        int32_t _M0L6_2atmpS2242 = _M0L3idxS634 + 1;
        int32_t _M0L14capacity__maskS2243 = _M0L4selfS637->$3;
        int32_t _M0L6_2atmpS2241 =
          _M0L6_2atmpS2242 & _M0L14capacity__maskS2243;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_3894 = _M0L5entryS635;
        _M0L3pslS633 = _M0L6_2atmpS2240;
        _M0L3idxS634 = _M0L6_2atmpS2241;
        _M0L5entryS635 = _tmp_3894;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS595,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS597,
  int32_t _M0L8new__idxS596
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3495;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2196;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2197;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3494;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3493;
  int32_t _M0L6_2acntS3724;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS598;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3495 = _M0L4selfS595->$0;
  _M0L7entriesS2196 = _M0L8_2afieldS3495;
  moonbit_incref(_M0L5entryS597);
  _M0L6_2atmpS2197 = _M0L5entryS597;
  if (
    _M0L8new__idxS596 < 0
    || _M0L8new__idxS596 >= Moonbit_array_length(_M0L7entriesS2196)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3494
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2196[
      _M0L8new__idxS596
    ];
  if (_M0L6_2aoldS3494) {
    moonbit_decref(_M0L6_2aoldS3494);
  }
  _M0L7entriesS2196[_M0L8new__idxS596] = _M0L6_2atmpS2197;
  _M0L8_2afieldS3493 = _M0L5entryS597->$1;
  _M0L6_2acntS3724 = Moonbit_object_header(_M0L5entryS597)->rc;
  if (_M0L6_2acntS3724 > 1) {
    int32_t _M0L11_2anew__cntS3727 = _M0L6_2acntS3724 - 1;
    Moonbit_object_header(_M0L5entryS597)->rc = _M0L11_2anew__cntS3727;
    if (_M0L8_2afieldS3493) {
      moonbit_incref(_M0L8_2afieldS3493);
    }
  } else if (_M0L6_2acntS3724 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3726 =
      _M0L5entryS597->$5;
    moonbit_string_t _M0L8_2afieldS3725;
    moonbit_decref(_M0L8_2afieldS3726);
    _M0L8_2afieldS3725 = _M0L5entryS597->$4;
    moonbit_decref(_M0L8_2afieldS3725);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS597);
  }
  _M0L7_2abindS598 = _M0L8_2afieldS3493;
  if (_M0L7_2abindS598 == 0) {
    if (_M0L7_2abindS598) {
      moonbit_decref(_M0L7_2abindS598);
    }
    _M0L4selfS595->$6 = _M0L8new__idxS596;
    moonbit_decref(_M0L4selfS595);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS599;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS600;
    moonbit_decref(_M0L4selfS595);
    _M0L7_2aSomeS599 = _M0L7_2abindS598;
    _M0L7_2anextS600 = _M0L7_2aSomeS599;
    _M0L7_2anextS600->$0 = _M0L8new__idxS596;
    moonbit_decref(_M0L7_2anextS600);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS601,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS603,
  int32_t _M0L8new__idxS602
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3498;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2198;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2199;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3497;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3496;
  int32_t _M0L6_2acntS3728;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS604;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3498 = _M0L4selfS601->$0;
  _M0L7entriesS2198 = _M0L8_2afieldS3498;
  moonbit_incref(_M0L5entryS603);
  _M0L6_2atmpS2199 = _M0L5entryS603;
  if (
    _M0L8new__idxS602 < 0
    || _M0L8new__idxS602 >= Moonbit_array_length(_M0L7entriesS2198)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3497
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2198[
      _M0L8new__idxS602
    ];
  if (_M0L6_2aoldS3497) {
    moonbit_decref(_M0L6_2aoldS3497);
  }
  _M0L7entriesS2198[_M0L8new__idxS602] = _M0L6_2atmpS2199;
  _M0L8_2afieldS3496 = _M0L5entryS603->$1;
  _M0L6_2acntS3728 = Moonbit_object_header(_M0L5entryS603)->rc;
  if (_M0L6_2acntS3728 > 1) {
    int32_t _M0L11_2anew__cntS3730 = _M0L6_2acntS3728 - 1;
    Moonbit_object_header(_M0L5entryS603)->rc = _M0L11_2anew__cntS3730;
    if (_M0L8_2afieldS3496) {
      moonbit_incref(_M0L8_2afieldS3496);
    }
  } else if (_M0L6_2acntS3728 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3729 =
      _M0L5entryS603->$5;
    moonbit_decref(_M0L8_2afieldS3729);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS603);
  }
  _M0L7_2abindS604 = _M0L8_2afieldS3496;
  if (_M0L7_2abindS604 == 0) {
    if (_M0L7_2abindS604) {
      moonbit_decref(_M0L7_2abindS604);
    }
    _M0L4selfS601->$6 = _M0L8new__idxS602;
    moonbit_decref(_M0L4selfS601);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS605;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS606;
    moonbit_decref(_M0L4selfS601);
    _M0L7_2aSomeS605 = _M0L7_2abindS604;
    _M0L7_2anextS606 = _M0L7_2aSomeS605;
    _M0L7_2anextS606->$0 = _M0L8new__idxS602;
    moonbit_decref(_M0L7_2anextS606);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS607,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS609,
  int32_t _M0L8new__idxS608
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3501;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2200;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2201;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3500;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3499;
  int32_t _M0L6_2acntS3731;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS610;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3501 = _M0L4selfS607->$0;
  _M0L7entriesS2200 = _M0L8_2afieldS3501;
  moonbit_incref(_M0L5entryS609);
  _M0L6_2atmpS2201 = _M0L5entryS609;
  if (
    _M0L8new__idxS608 < 0
    || _M0L8new__idxS608 >= Moonbit_array_length(_M0L7entriesS2200)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3500
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2200[_M0L8new__idxS608];
  if (_M0L6_2aoldS3500) {
    moonbit_decref(_M0L6_2aoldS3500);
  }
  _M0L7entriesS2200[_M0L8new__idxS608] = _M0L6_2atmpS2201;
  _M0L8_2afieldS3499 = _M0L5entryS609->$1;
  _M0L6_2acntS3731 = Moonbit_object_header(_M0L5entryS609)->rc;
  if (_M0L6_2acntS3731 > 1) {
    int32_t _M0L11_2anew__cntS3734 = _M0L6_2acntS3731 - 1;
    Moonbit_object_header(_M0L5entryS609)->rc = _M0L11_2anew__cntS3734;
    if (_M0L8_2afieldS3499) {
      moonbit_incref(_M0L8_2afieldS3499);
    }
  } else if (_M0L6_2acntS3731 == 1) {
    void* _M0L8_2afieldS3733 = _M0L5entryS609->$5;
    moonbit_string_t _M0L8_2afieldS3732;
    moonbit_decref(_M0L8_2afieldS3733);
    _M0L8_2afieldS3732 = _M0L5entryS609->$4;
    moonbit_decref(_M0L8_2afieldS3732);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS609);
  }
  _M0L7_2abindS610 = _M0L8_2afieldS3499;
  if (_M0L7_2abindS610 == 0) {
    if (_M0L7_2abindS610) {
      moonbit_decref(_M0L7_2abindS610);
    }
    _M0L4selfS607->$6 = _M0L8new__idxS608;
    moonbit_decref(_M0L4selfS607);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS611;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS612;
    moonbit_decref(_M0L4selfS607);
    _M0L7_2aSomeS611 = _M0L7_2abindS610;
    _M0L7_2anextS612 = _M0L7_2aSomeS611;
    _M0L7_2anextS612->$0 = _M0L8new__idxS608;
    moonbit_decref(_M0L7_2anextS612);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS584,
  int32_t _M0L3idxS586,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS585
) {
  int32_t _M0L7_2abindS583;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3503;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2174;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2175;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3502;
  int32_t _M0L4sizeS2177;
  int32_t _M0L6_2atmpS2176;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS583 = _M0L4selfS584->$6;
  switch (_M0L7_2abindS583) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2169;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3504;
      moonbit_incref(_M0L5entryS585);
      _M0L6_2atmpS2169 = _M0L5entryS585;
      _M0L6_2aoldS3504 = _M0L4selfS584->$5;
      if (_M0L6_2aoldS3504) {
        moonbit_decref(_M0L6_2aoldS3504);
      }
      _M0L4selfS584->$5 = _M0L6_2atmpS2169;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3507 =
        _M0L4selfS584->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2173 =
        _M0L8_2afieldS3507;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3506;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2172;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2170;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2171;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3505;
      if (
        _M0L7_2abindS583 < 0
        || _M0L7_2abindS583 >= Moonbit_array_length(_M0L7entriesS2173)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3506
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2173[
          _M0L7_2abindS583
        ];
      _M0L6_2atmpS2172 = _M0L6_2atmpS3506;
      if (_M0L6_2atmpS2172) {
        moonbit_incref(_M0L6_2atmpS2172);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2170
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2172);
      moonbit_incref(_M0L5entryS585);
      _M0L6_2atmpS2171 = _M0L5entryS585;
      _M0L6_2aoldS3505 = _M0L6_2atmpS2170->$1;
      if (_M0L6_2aoldS3505) {
        moonbit_decref(_M0L6_2aoldS3505);
      }
      _M0L6_2atmpS2170->$1 = _M0L6_2atmpS2171;
      moonbit_decref(_M0L6_2atmpS2170);
      break;
    }
  }
  _M0L4selfS584->$6 = _M0L3idxS586;
  _M0L8_2afieldS3503 = _M0L4selfS584->$0;
  _M0L7entriesS2174 = _M0L8_2afieldS3503;
  _M0L6_2atmpS2175 = _M0L5entryS585;
  if (
    _M0L3idxS586 < 0
    || _M0L3idxS586 >= Moonbit_array_length(_M0L7entriesS2174)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3502
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2174[
      _M0L3idxS586
    ];
  if (_M0L6_2aoldS3502) {
    moonbit_decref(_M0L6_2aoldS3502);
  }
  _M0L7entriesS2174[_M0L3idxS586] = _M0L6_2atmpS2175;
  _M0L4sizeS2177 = _M0L4selfS584->$1;
  _M0L6_2atmpS2176 = _M0L4sizeS2177 + 1;
  _M0L4selfS584->$1 = _M0L6_2atmpS2176;
  moonbit_decref(_M0L4selfS584);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS588,
  int32_t _M0L3idxS590,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS589
) {
  int32_t _M0L7_2abindS587;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3509;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2183;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2184;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3508;
  int32_t _M0L4sizeS2186;
  int32_t _M0L6_2atmpS2185;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS587 = _M0L4selfS588->$6;
  switch (_M0L7_2abindS587) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2178;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3510;
      moonbit_incref(_M0L5entryS589);
      _M0L6_2atmpS2178 = _M0L5entryS589;
      _M0L6_2aoldS3510 = _M0L4selfS588->$5;
      if (_M0L6_2aoldS3510) {
        moonbit_decref(_M0L6_2aoldS3510);
      }
      _M0L4selfS588->$5 = _M0L6_2atmpS2178;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3513 =
        _M0L4selfS588->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2182 =
        _M0L8_2afieldS3513;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3512;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2181;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2179;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2180;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3511;
      if (
        _M0L7_2abindS587 < 0
        || _M0L7_2abindS587 >= Moonbit_array_length(_M0L7entriesS2182)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3512
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2182[
          _M0L7_2abindS587
        ];
      _M0L6_2atmpS2181 = _M0L6_2atmpS3512;
      if (_M0L6_2atmpS2181) {
        moonbit_incref(_M0L6_2atmpS2181);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2179
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2181);
      moonbit_incref(_M0L5entryS589);
      _M0L6_2atmpS2180 = _M0L5entryS589;
      _M0L6_2aoldS3511 = _M0L6_2atmpS2179->$1;
      if (_M0L6_2aoldS3511) {
        moonbit_decref(_M0L6_2aoldS3511);
      }
      _M0L6_2atmpS2179->$1 = _M0L6_2atmpS2180;
      moonbit_decref(_M0L6_2atmpS2179);
      break;
    }
  }
  _M0L4selfS588->$6 = _M0L3idxS590;
  _M0L8_2afieldS3509 = _M0L4selfS588->$0;
  _M0L7entriesS2183 = _M0L8_2afieldS3509;
  _M0L6_2atmpS2184 = _M0L5entryS589;
  if (
    _M0L3idxS590 < 0
    || _M0L3idxS590 >= Moonbit_array_length(_M0L7entriesS2183)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3508
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2183[
      _M0L3idxS590
    ];
  if (_M0L6_2aoldS3508) {
    moonbit_decref(_M0L6_2aoldS3508);
  }
  _M0L7entriesS2183[_M0L3idxS590] = _M0L6_2atmpS2184;
  _M0L4sizeS2186 = _M0L4selfS588->$1;
  _M0L6_2atmpS2185 = _M0L4sizeS2186 + 1;
  _M0L4selfS588->$1 = _M0L6_2atmpS2185;
  moonbit_decref(_M0L4selfS588);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS592,
  int32_t _M0L3idxS594,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS593
) {
  int32_t _M0L7_2abindS591;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3515;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2192;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2193;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3514;
  int32_t _M0L4sizeS2195;
  int32_t _M0L6_2atmpS2194;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS591 = _M0L4selfS592->$6;
  switch (_M0L7_2abindS591) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2187;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3516;
      moonbit_incref(_M0L5entryS593);
      _M0L6_2atmpS2187 = _M0L5entryS593;
      _M0L6_2aoldS3516 = _M0L4selfS592->$5;
      if (_M0L6_2aoldS3516) {
        moonbit_decref(_M0L6_2aoldS3516);
      }
      _M0L4selfS592->$5 = _M0L6_2atmpS2187;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3519 = _M0L4selfS592->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2191 = _M0L8_2afieldS3519;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3518;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2190;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2188;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2189;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3517;
      if (
        _M0L7_2abindS591 < 0
        || _M0L7_2abindS591 >= Moonbit_array_length(_M0L7entriesS2191)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3518
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2191[_M0L7_2abindS591];
      _M0L6_2atmpS2190 = _M0L6_2atmpS3518;
      if (_M0L6_2atmpS2190) {
        moonbit_incref(_M0L6_2atmpS2190);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2188
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2190);
      moonbit_incref(_M0L5entryS593);
      _M0L6_2atmpS2189 = _M0L5entryS593;
      _M0L6_2aoldS3517 = _M0L6_2atmpS2188->$1;
      if (_M0L6_2aoldS3517) {
        moonbit_decref(_M0L6_2aoldS3517);
      }
      _M0L6_2atmpS2188->$1 = _M0L6_2atmpS2189;
      moonbit_decref(_M0L6_2atmpS2188);
      break;
    }
  }
  _M0L4selfS592->$6 = _M0L3idxS594;
  _M0L8_2afieldS3515 = _M0L4selfS592->$0;
  _M0L7entriesS2192 = _M0L8_2afieldS3515;
  _M0L6_2atmpS2193 = _M0L5entryS593;
  if (
    _M0L3idxS594 < 0
    || _M0L3idxS594 >= Moonbit_array_length(_M0L7entriesS2192)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3514
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2192[_M0L3idxS594];
  if (_M0L6_2aoldS3514) {
    moonbit_decref(_M0L6_2aoldS3514);
  }
  _M0L7entriesS2192[_M0L3idxS594] = _M0L6_2atmpS2193;
  _M0L4sizeS2195 = _M0L4selfS592->$1;
  _M0L6_2atmpS2194 = _M0L4sizeS2195 + 1;
  _M0L4selfS592->$1 = _M0L6_2atmpS2194;
  moonbit_decref(_M0L4selfS592);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS566
) {
  int32_t _M0L8capacityS565;
  int32_t _M0L7_2abindS567;
  int32_t _M0L7_2abindS568;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2166;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS569;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS570;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3895;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS565
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS566);
  _M0L7_2abindS567 = _M0L8capacityS565 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS568 = _M0FPB21calc__grow__threshold(_M0L8capacityS565);
  _M0L6_2atmpS2166 = 0;
  _M0L7_2abindS569
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS565, _M0L6_2atmpS2166);
  _M0L7_2abindS570 = 0;
  _block_3895
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3895)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3895->$0 = _M0L7_2abindS569;
  _block_3895->$1 = 0;
  _block_3895->$2 = _M0L8capacityS565;
  _block_3895->$3 = _M0L7_2abindS567;
  _block_3895->$4 = _M0L7_2abindS568;
  _block_3895->$5 = _M0L7_2abindS570;
  _block_3895->$6 = -1;
  return _block_3895;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS572
) {
  int32_t _M0L8capacityS571;
  int32_t _M0L7_2abindS573;
  int32_t _M0L7_2abindS574;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2167;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS575;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS576;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3896;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS571
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS572);
  _M0L7_2abindS573 = _M0L8capacityS571 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS574 = _M0FPB21calc__grow__threshold(_M0L8capacityS571);
  _M0L6_2atmpS2167 = 0;
  _M0L7_2abindS575
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS571, _M0L6_2atmpS2167);
  _M0L7_2abindS576 = 0;
  _block_3896
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3896)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3896->$0 = _M0L7_2abindS575;
  _block_3896->$1 = 0;
  _block_3896->$2 = _M0L8capacityS571;
  _block_3896->$3 = _M0L7_2abindS573;
  _block_3896->$4 = _M0L7_2abindS574;
  _block_3896->$5 = _M0L7_2abindS576;
  _block_3896->$6 = -1;
  return _block_3896;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS578
) {
  int32_t _M0L8capacityS577;
  int32_t _M0L7_2abindS579;
  int32_t _M0L7_2abindS580;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2168;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS581;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS582;
  struct _M0TPB3MapGsRPB4JsonE* _block_3897;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS577
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS578);
  _M0L7_2abindS579 = _M0L8capacityS577 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS580 = _M0FPB21calc__grow__threshold(_M0L8capacityS577);
  _M0L6_2atmpS2168 = 0;
  _M0L7_2abindS581
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS577, _M0L6_2atmpS2168);
  _M0L7_2abindS582 = 0;
  _block_3897
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_3897)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_3897->$0 = _M0L7_2abindS581;
  _block_3897->$1 = 0;
  _block_3897->$2 = _M0L8capacityS577;
  _block_3897->$3 = _M0L7_2abindS579;
  _block_3897->$4 = _M0L7_2abindS580;
  _block_3897->$5 = _M0L7_2abindS582;
  _block_3897->$6 = -1;
  return _block_3897;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS564) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS564 >= 0) {
    int32_t _M0L6_2atmpS2165;
    int32_t _M0L6_2atmpS2164;
    int32_t _M0L6_2atmpS2163;
    int32_t _M0L6_2atmpS2162;
    if (_M0L4selfS564 <= 1) {
      return 1;
    }
    if (_M0L4selfS564 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2165 = _M0L4selfS564 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2164 = moonbit_clz32(_M0L6_2atmpS2165);
    _M0L6_2atmpS2163 = _M0L6_2atmpS2164 - 1;
    _M0L6_2atmpS2162 = 2147483647 >> (_M0L6_2atmpS2163 & 31);
    return _M0L6_2atmpS2162 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS563) {
  int32_t _M0L6_2atmpS2161;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2161 = _M0L8capacityS563 * 13;
  return _M0L6_2atmpS2161 / 16;
}

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0MPC16option6Option3mapGiRP48clawteam8clawteam8internal6openai19PromptTokensDetailsE(
  int64_t _M0L4selfS559,
  struct _M0TWiERP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L1fS562
) {
  #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS559 == 4294967296ll) {
    moonbit_decref(_M0L1fS562);
    return 0;
  } else {
    int64_t _M0L7_2aSomeS560 = _M0L4selfS559;
    int32_t _M0L4_2atS561 = (int32_t)_M0L7_2aSomeS560;
    struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L6_2atmpS2160;
    #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    _M0L6_2atmpS2160 = _M0L1fS562->code(_M0L1fS562, _M0L4_2atS561);
    return _M0L6_2atmpS2160;
  }
}

moonbit_string_t _M0MPC16option6Option10unwrap__orGsE(
  moonbit_string_t _M0L4selfS556,
  moonbit_string_t _M0L7defaultS557
) {
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS556 == 0) {
    if (_M0L4selfS556) {
      moonbit_decref(_M0L4selfS556);
    }
    return _M0L7defaultS557;
  } else {
    moonbit_string_t _M0L7_2aSomeS558;
    moonbit_decref(_M0L7defaultS557);
    _M0L7_2aSomeS558 = _M0L4selfS556;
    return _M0L7_2aSomeS558;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS550
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS550 == 0) {
    if (_M0L4selfS550) {
      moonbit_decref(_M0L4selfS550);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS551 =
      _M0L4selfS550;
    return _M0L7_2aSomeS551;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS552
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS552 == 0) {
    if (_M0L4selfS552) {
      moonbit_decref(_M0L4selfS552);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS553 =
      _M0L4selfS552;
    return _M0L7_2aSomeS553;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS554
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS554 == 0) {
    if (_M0L4selfS554) {
      moonbit_decref(_M0L4selfS554);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS555 = _M0L4selfS554;
    return _M0L7_2aSomeS555;
  }
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t _M0L3lenS548
) {
  void** _M0L6_2atmpS2158;
  struct _M0TPB5ArrayGRPB4JsonE* _block_3898;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2158
  = (void**)moonbit_make_ref_array(_M0L3lenS548, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _block_3898
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_block_3898)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _block_3898->$0 = _M0L6_2atmpS2158;
  _block_3898->$1 = _M0L3lenS548;
  return _block_3898;
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0MPC15array5Array12make__uninitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  int32_t _M0L3lenS549
) {
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L6_2atmpS2159;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _block_3899;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2159
  = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**)moonbit_make_ref_array(_M0L3lenS549, 0);
  _block_3899
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE));
  Moonbit_object_header(_block_3899)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE, $0) >> 2, 1, 0);
  _block_3899->$0 = _M0L6_2atmpS2159;
  _block_3899->$1 = _M0L3lenS549;
  return _block_3899;
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS547
) {
  moonbit_string_t* _M0L6_2atmpS2157;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2157 = _M0L4selfS547;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2157);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS543,
  int32_t _M0L5indexS544
) {
  uint64_t* _M0L6_2atmpS2155;
  uint64_t _M0L6_2atmpS3520;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2155 = _M0L4selfS543;
  if (
    _M0L5indexS544 < 0
    || _M0L5indexS544 >= Moonbit_array_length(_M0L6_2atmpS2155)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3520 = (uint64_t)_M0L6_2atmpS2155[_M0L5indexS544];
  moonbit_decref(_M0L6_2atmpS2155);
  return _M0L6_2atmpS3520;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS545,
  int32_t _M0L5indexS546
) {
  uint32_t* _M0L6_2atmpS2156;
  uint32_t _M0L6_2atmpS3521;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2156 = _M0L4selfS545;
  if (
    _M0L5indexS546 < 0
    || _M0L5indexS546 >= Moonbit_array_length(_M0L6_2atmpS2156)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3521 = (uint32_t)_M0L6_2atmpS2156[_M0L5indexS546];
  moonbit_decref(_M0L6_2atmpS2156);
  return _M0L6_2atmpS3521;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS542
) {
  moonbit_string_t* _M0L6_2atmpS2153;
  int32_t _M0L6_2atmpS3522;
  int32_t _M0L6_2atmpS2154;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2152;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS542);
  _M0L6_2atmpS2153 = _M0L4selfS542;
  _M0L6_2atmpS3522 = Moonbit_array_length(_M0L4selfS542);
  moonbit_decref(_M0L4selfS542);
  _M0L6_2atmpS2154 = _M0L6_2atmpS3522;
  _M0L6_2atmpS2152
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2154, _M0L6_2atmpS2153
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2152);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS540
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS539;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__* _closure_3900;
  struct _M0TWEOs* _M0L6_2atmpS2140;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS539
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS539)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS539->$0 = 0;
  _closure_3900
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__));
  Moonbit_object_header(_closure_3900)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__, $0_0) >> 2, 2, 0);
  _closure_3900->code = &_M0MPC15array9ArrayView4iterGsEC2141l570;
  _closure_3900->$0_0 = _M0L4selfS540.$0;
  _closure_3900->$0_1 = _M0L4selfS540.$1;
  _closure_3900->$0_2 = _M0L4selfS540.$2;
  _closure_3900->$1 = _M0L1iS539;
  _M0L6_2atmpS2140 = (struct _M0TWEOs*)_closure_3900;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2140);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2141l570(
  struct _M0TWEOs* _M0L6_2aenvS2142
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__* _M0L14_2acasted__envS2143;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3527;
  struct _M0TPC13ref3RefGiE* _M0L1iS539;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3526;
  int32_t _M0L6_2acntS3735;
  struct _M0TPB9ArrayViewGsE _M0L4selfS540;
  int32_t _M0L3valS2144;
  int32_t _M0L6_2atmpS2145;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2143
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2141__l570__*)_M0L6_2aenvS2142;
  _M0L8_2afieldS3527 = _M0L14_2acasted__envS2143->$1;
  _M0L1iS539 = _M0L8_2afieldS3527;
  _M0L8_2afieldS3526
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2143->$0_1,
      _M0L14_2acasted__envS2143->$0_2,
      _M0L14_2acasted__envS2143->$0_0
  };
  _M0L6_2acntS3735 = Moonbit_object_header(_M0L14_2acasted__envS2143)->rc;
  if (_M0L6_2acntS3735 > 1) {
    int32_t _M0L11_2anew__cntS3736 = _M0L6_2acntS3735 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2143)->rc
    = _M0L11_2anew__cntS3736;
    moonbit_incref(_M0L1iS539);
    moonbit_incref(_M0L8_2afieldS3526.$0);
  } else if (_M0L6_2acntS3735 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2143);
  }
  _M0L4selfS540 = _M0L8_2afieldS3526;
  _M0L3valS2144 = _M0L1iS539->$0;
  moonbit_incref(_M0L4selfS540.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2145 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS540);
  if (_M0L3valS2144 < _M0L6_2atmpS2145) {
    moonbit_string_t* _M0L8_2afieldS3525 = _M0L4selfS540.$0;
    moonbit_string_t* _M0L3bufS2148 = _M0L8_2afieldS3525;
    int32_t _M0L8_2afieldS3524 = _M0L4selfS540.$1;
    int32_t _M0L5startS2150 = _M0L8_2afieldS3524;
    int32_t _M0L3valS2151 = _M0L1iS539->$0;
    int32_t _M0L6_2atmpS2149 = _M0L5startS2150 + _M0L3valS2151;
    moonbit_string_t _M0L6_2atmpS3523 =
      (moonbit_string_t)_M0L3bufS2148[_M0L6_2atmpS2149];
    moonbit_string_t _M0L4elemS541;
    int32_t _M0L3valS2147;
    int32_t _M0L6_2atmpS2146;
    moonbit_incref(_M0L6_2atmpS3523);
    moonbit_decref(_M0L3bufS2148);
    _M0L4elemS541 = _M0L6_2atmpS3523;
    _M0L3valS2147 = _M0L1iS539->$0;
    _M0L6_2atmpS2146 = _M0L3valS2147 + 1;
    _M0L1iS539->$0 = _M0L6_2atmpS2146;
    moonbit_decref(_M0L1iS539);
    return _M0L4elemS541;
  } else {
    moonbit_decref(_M0L4selfS540.$0);
    moonbit_decref(_M0L1iS539);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS538
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS538;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS537,
  struct _M0TPB6Logger _M0L6loggerS536
) {
  moonbit_string_t _M0L6_2atmpS2139;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2139
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS537, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS536.$0->$method_0(_M0L6loggerS536.$1, _M0L6_2atmpS2139);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS535,
  struct _M0TPB6Logger _M0L6loggerS534
) {
  moonbit_string_t _M0L6_2atmpS2138;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2138 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS535, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS534.$0->$method_0(_M0L6loggerS534.$1, _M0L6_2atmpS2138);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS529) {
  int32_t _M0L3lenS528;
  struct _M0TPC13ref3RefGiE* _M0L5indexS530;
  struct _M0R38String_3a_3aiter_2eanon__u2122__l247__* _closure_3901;
  struct _M0TWEOc* _M0L6_2atmpS2121;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS528 = Moonbit_array_length(_M0L4selfS529);
  _M0L5indexS530
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS530)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS530->$0 = 0;
  _closure_3901
  = (struct _M0R38String_3a_3aiter_2eanon__u2122__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2122__l247__));
  Moonbit_object_header(_closure_3901)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2122__l247__, $0) >> 2, 2, 0);
  _closure_3901->code = &_M0MPC16string6String4iterC2122l247;
  _closure_3901->$0 = _M0L5indexS530;
  _closure_3901->$1 = _M0L4selfS529;
  _closure_3901->$2 = _M0L3lenS528;
  _M0L6_2atmpS2121 = (struct _M0TWEOc*)_closure_3901;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2121);
}

int32_t _M0MPC16string6String4iterC2122l247(
  struct _M0TWEOc* _M0L6_2aenvS2123
) {
  struct _M0R38String_3a_3aiter_2eanon__u2122__l247__* _M0L14_2acasted__envS2124;
  int32_t _M0L3lenS528;
  moonbit_string_t _M0L8_2afieldS3530;
  moonbit_string_t _M0L4selfS529;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3529;
  int32_t _M0L6_2acntS3737;
  struct _M0TPC13ref3RefGiE* _M0L5indexS530;
  int32_t _M0L3valS2125;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2124
  = (struct _M0R38String_3a_3aiter_2eanon__u2122__l247__*)_M0L6_2aenvS2123;
  _M0L3lenS528 = _M0L14_2acasted__envS2124->$2;
  _M0L8_2afieldS3530 = _M0L14_2acasted__envS2124->$1;
  _M0L4selfS529 = _M0L8_2afieldS3530;
  _M0L8_2afieldS3529 = _M0L14_2acasted__envS2124->$0;
  _M0L6_2acntS3737 = Moonbit_object_header(_M0L14_2acasted__envS2124)->rc;
  if (_M0L6_2acntS3737 > 1) {
    int32_t _M0L11_2anew__cntS3738 = _M0L6_2acntS3737 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2124)->rc
    = _M0L11_2anew__cntS3738;
    moonbit_incref(_M0L4selfS529);
    moonbit_incref(_M0L8_2afieldS3529);
  } else if (_M0L6_2acntS3737 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2124);
  }
  _M0L5indexS530 = _M0L8_2afieldS3529;
  _M0L3valS2125 = _M0L5indexS530->$0;
  if (_M0L3valS2125 < _M0L3lenS528) {
    int32_t _M0L3valS2137 = _M0L5indexS530->$0;
    int32_t _M0L2c1S531 = _M0L4selfS529[_M0L3valS2137];
    int32_t _if__result_3902;
    int32_t _M0L3valS2135;
    int32_t _M0L6_2atmpS2134;
    int32_t _M0L6_2atmpS2136;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S531)) {
      int32_t _M0L3valS2127 = _M0L5indexS530->$0;
      int32_t _M0L6_2atmpS2126 = _M0L3valS2127 + 1;
      _if__result_3902 = _M0L6_2atmpS2126 < _M0L3lenS528;
    } else {
      _if__result_3902 = 0;
    }
    if (_if__result_3902) {
      int32_t _M0L3valS2133 = _M0L5indexS530->$0;
      int32_t _M0L6_2atmpS2132 = _M0L3valS2133 + 1;
      int32_t _M0L6_2atmpS3528 = _M0L4selfS529[_M0L6_2atmpS2132];
      int32_t _M0L2c2S532;
      moonbit_decref(_M0L4selfS529);
      _M0L2c2S532 = _M0L6_2atmpS3528;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S532)) {
        int32_t _M0L6_2atmpS2130 = (int32_t)_M0L2c1S531;
        int32_t _M0L6_2atmpS2131 = (int32_t)_M0L2c2S532;
        int32_t _M0L1cS533;
        int32_t _M0L3valS2129;
        int32_t _M0L6_2atmpS2128;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS533
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2130, _M0L6_2atmpS2131);
        _M0L3valS2129 = _M0L5indexS530->$0;
        _M0L6_2atmpS2128 = _M0L3valS2129 + 2;
        _M0L5indexS530->$0 = _M0L6_2atmpS2128;
        moonbit_decref(_M0L5indexS530);
        return _M0L1cS533;
      }
    } else {
      moonbit_decref(_M0L4selfS529);
    }
    _M0L3valS2135 = _M0L5indexS530->$0;
    _M0L6_2atmpS2134 = _M0L3valS2135 + 1;
    _M0L5indexS530->$0 = _M0L6_2atmpS2134;
    moonbit_decref(_M0L5indexS530);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2136 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S531);
    return _M0L6_2atmpS2136;
  } else {
    moonbit_decref(_M0L5indexS530);
    moonbit_decref(_M0L4selfS529);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS519,
  moonbit_string_t _M0L5valueS521
) {
  int32_t _M0L3lenS2106;
  moonbit_string_t* _M0L6_2atmpS2108;
  int32_t _M0L6_2atmpS3533;
  int32_t _M0L6_2atmpS2107;
  int32_t _M0L6lengthS520;
  moonbit_string_t* _M0L8_2afieldS3532;
  moonbit_string_t* _M0L3bufS2109;
  moonbit_string_t _M0L6_2aoldS3531;
  int32_t _M0L6_2atmpS2110;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2106 = _M0L4selfS519->$1;
  moonbit_incref(_M0L4selfS519);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2108 = _M0MPC15array5Array6bufferGsE(_M0L4selfS519);
  _M0L6_2atmpS3533 = Moonbit_array_length(_M0L6_2atmpS2108);
  moonbit_decref(_M0L6_2atmpS2108);
  _M0L6_2atmpS2107 = _M0L6_2atmpS3533;
  if (_M0L3lenS2106 == _M0L6_2atmpS2107) {
    moonbit_incref(_M0L4selfS519);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS519);
  }
  _M0L6lengthS520 = _M0L4selfS519->$1;
  _M0L8_2afieldS3532 = _M0L4selfS519->$0;
  _M0L3bufS2109 = _M0L8_2afieldS3532;
  _M0L6_2aoldS3531 = (moonbit_string_t)_M0L3bufS2109[_M0L6lengthS520];
  moonbit_decref(_M0L6_2aoldS3531);
  _M0L3bufS2109[_M0L6lengthS520] = _M0L5valueS521;
  _M0L6_2atmpS2110 = _M0L6lengthS520 + 1;
  _M0L4selfS519->$1 = _M0L6_2atmpS2110;
  moonbit_decref(_M0L4selfS519);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS522,
  struct _M0TUsiE* _M0L5valueS524
) {
  int32_t _M0L3lenS2111;
  struct _M0TUsiE** _M0L6_2atmpS2113;
  int32_t _M0L6_2atmpS3536;
  int32_t _M0L6_2atmpS2112;
  int32_t _M0L6lengthS523;
  struct _M0TUsiE** _M0L8_2afieldS3535;
  struct _M0TUsiE** _M0L3bufS2114;
  struct _M0TUsiE* _M0L6_2aoldS3534;
  int32_t _M0L6_2atmpS2115;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2111 = _M0L4selfS522->$1;
  moonbit_incref(_M0L4selfS522);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2113 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS522);
  _M0L6_2atmpS3536 = Moonbit_array_length(_M0L6_2atmpS2113);
  moonbit_decref(_M0L6_2atmpS2113);
  _M0L6_2atmpS2112 = _M0L6_2atmpS3536;
  if (_M0L3lenS2111 == _M0L6_2atmpS2112) {
    moonbit_incref(_M0L4selfS522);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS522);
  }
  _M0L6lengthS523 = _M0L4selfS522->$1;
  _M0L8_2afieldS3535 = _M0L4selfS522->$0;
  _M0L3bufS2114 = _M0L8_2afieldS3535;
  _M0L6_2aoldS3534 = (struct _M0TUsiE*)_M0L3bufS2114[_M0L6lengthS523];
  if (_M0L6_2aoldS3534) {
    moonbit_decref(_M0L6_2aoldS3534);
  }
  _M0L3bufS2114[_M0L6lengthS523] = _M0L5valueS524;
  _M0L6_2atmpS2115 = _M0L6lengthS523 + 1;
  _M0L4selfS522->$1 = _M0L6_2atmpS2115;
  moonbit_decref(_M0L4selfS522);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS525,
  void* _M0L5valueS527
) {
  int32_t _M0L3lenS2116;
  void** _M0L6_2atmpS2118;
  int32_t _M0L6_2atmpS3539;
  int32_t _M0L6_2atmpS2117;
  int32_t _M0L6lengthS526;
  void** _M0L8_2afieldS3538;
  void** _M0L3bufS2119;
  void* _M0L6_2aoldS3537;
  int32_t _M0L6_2atmpS2120;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2116 = _M0L4selfS525->$1;
  moonbit_incref(_M0L4selfS525);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2118
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS525);
  _M0L6_2atmpS3539 = Moonbit_array_length(_M0L6_2atmpS2118);
  moonbit_decref(_M0L6_2atmpS2118);
  _M0L6_2atmpS2117 = _M0L6_2atmpS3539;
  if (_M0L3lenS2116 == _M0L6_2atmpS2117) {
    moonbit_incref(_M0L4selfS525);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS525);
  }
  _M0L6lengthS526 = _M0L4selfS525->$1;
  _M0L8_2afieldS3538 = _M0L4selfS525->$0;
  _M0L3bufS2119 = _M0L8_2afieldS3538;
  _M0L6_2aoldS3537 = (void*)_M0L3bufS2119[_M0L6lengthS526];
  moonbit_decref(_M0L6_2aoldS3537);
  _M0L3bufS2119[_M0L6lengthS526] = _M0L5valueS527;
  _M0L6_2atmpS2120 = _M0L6lengthS526 + 1;
  _M0L4selfS525->$1 = _M0L6_2atmpS2120;
  moonbit_decref(_M0L4selfS525);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS511) {
  int32_t _M0L8old__capS510;
  int32_t _M0L8new__capS512;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS510 = _M0L4selfS511->$1;
  if (_M0L8old__capS510 == 0) {
    _M0L8new__capS512 = 8;
  } else {
    _M0L8new__capS512 = _M0L8old__capS510 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS511, _M0L8new__capS512);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS514
) {
  int32_t _M0L8old__capS513;
  int32_t _M0L8new__capS515;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS513 = _M0L4selfS514->$1;
  if (_M0L8old__capS513 == 0) {
    _M0L8new__capS515 = 8;
  } else {
    _M0L8new__capS515 = _M0L8old__capS513 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS514, _M0L8new__capS515);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS517
) {
  int32_t _M0L8old__capS516;
  int32_t _M0L8new__capS518;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS516 = _M0L4selfS517->$1;
  if (_M0L8old__capS516 == 0) {
    _M0L8new__capS518 = 8;
  } else {
    _M0L8new__capS518 = _M0L8old__capS516 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS517, _M0L8new__capS518);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS495,
  int32_t _M0L13new__capacityS493
) {
  moonbit_string_t* _M0L8new__bufS492;
  moonbit_string_t* _M0L8_2afieldS3541;
  moonbit_string_t* _M0L8old__bufS494;
  int32_t _M0L8old__capS496;
  int32_t _M0L9copy__lenS497;
  moonbit_string_t* _M0L6_2aoldS3540;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS492
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS493, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3541 = _M0L4selfS495->$0;
  _M0L8old__bufS494 = _M0L8_2afieldS3541;
  _M0L8old__capS496 = Moonbit_array_length(_M0L8old__bufS494);
  if (_M0L8old__capS496 < _M0L13new__capacityS493) {
    _M0L9copy__lenS497 = _M0L8old__capS496;
  } else {
    _M0L9copy__lenS497 = _M0L13new__capacityS493;
  }
  moonbit_incref(_M0L8old__bufS494);
  moonbit_incref(_M0L8new__bufS492);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS492, 0, _M0L8old__bufS494, 0, _M0L9copy__lenS497);
  _M0L6_2aoldS3540 = _M0L4selfS495->$0;
  moonbit_decref(_M0L6_2aoldS3540);
  _M0L4selfS495->$0 = _M0L8new__bufS492;
  moonbit_decref(_M0L4selfS495);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS501,
  int32_t _M0L13new__capacityS499
) {
  struct _M0TUsiE** _M0L8new__bufS498;
  struct _M0TUsiE** _M0L8_2afieldS3543;
  struct _M0TUsiE** _M0L8old__bufS500;
  int32_t _M0L8old__capS502;
  int32_t _M0L9copy__lenS503;
  struct _M0TUsiE** _M0L6_2aoldS3542;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS498
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS499, 0);
  _M0L8_2afieldS3543 = _M0L4selfS501->$0;
  _M0L8old__bufS500 = _M0L8_2afieldS3543;
  _M0L8old__capS502 = Moonbit_array_length(_M0L8old__bufS500);
  if (_M0L8old__capS502 < _M0L13new__capacityS499) {
    _M0L9copy__lenS503 = _M0L8old__capS502;
  } else {
    _M0L9copy__lenS503 = _M0L13new__capacityS499;
  }
  moonbit_incref(_M0L8old__bufS500);
  moonbit_incref(_M0L8new__bufS498);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS498, 0, _M0L8old__bufS500, 0, _M0L9copy__lenS503);
  _M0L6_2aoldS3542 = _M0L4selfS501->$0;
  moonbit_decref(_M0L6_2aoldS3542);
  _M0L4selfS501->$0 = _M0L8new__bufS498;
  moonbit_decref(_M0L4selfS501);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS507,
  int32_t _M0L13new__capacityS505
) {
  void** _M0L8new__bufS504;
  void** _M0L8_2afieldS3545;
  void** _M0L8old__bufS506;
  int32_t _M0L8old__capS508;
  int32_t _M0L9copy__lenS509;
  void** _M0L6_2aoldS3544;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS504
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS505, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3545 = _M0L4selfS507->$0;
  _M0L8old__bufS506 = _M0L8_2afieldS3545;
  _M0L8old__capS508 = Moonbit_array_length(_M0L8old__bufS506);
  if (_M0L8old__capS508 < _M0L13new__capacityS505) {
    _M0L9copy__lenS509 = _M0L8old__capS508;
  } else {
    _M0L9copy__lenS509 = _M0L13new__capacityS505;
  }
  moonbit_incref(_M0L8old__bufS506);
  moonbit_incref(_M0L8new__bufS504);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS504, 0, _M0L8old__bufS506, 0, _M0L9copy__lenS509);
  _M0L6_2aoldS3544 = _M0L4selfS507->$0;
  moonbit_decref(_M0L6_2aoldS3544);
  _M0L4selfS507->$0 = _M0L8new__bufS504;
  moonbit_decref(_M0L4selfS507);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS491
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS491 == 0) {
    moonbit_string_t* _M0L6_2atmpS2104 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3903 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3903)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3903->$0 = _M0L6_2atmpS2104;
    _block_3903->$1 = 0;
    return _block_3903;
  } else {
    moonbit_string_t* _M0L6_2atmpS2105 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS491, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3904 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3904)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3904->$0 = _M0L6_2atmpS2105;
    _block_3904->$1 = 0;
    return _block_3904;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS485,
  int32_t _M0L1nS484
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS484 <= 0) {
    moonbit_decref(_M0L4selfS485);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS484 == 1) {
    return _M0L4selfS485;
  } else {
    int32_t _M0L3lenS486 = Moonbit_array_length(_M0L4selfS485);
    int32_t _M0L6_2atmpS2103 = _M0L3lenS486 * _M0L1nS484;
    struct _M0TPB13StringBuilder* _M0L3bufS487;
    moonbit_string_t _M0L3strS488;
    int32_t _M0L2__S489;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS487 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2103);
    _M0L3strS488 = _M0L4selfS485;
    _M0L2__S489 = 0;
    while (1) {
      if (_M0L2__S489 < _M0L1nS484) {
        int32_t _M0L6_2atmpS2102;
        moonbit_incref(_M0L3strS488);
        moonbit_incref(_M0L3bufS487);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS487, _M0L3strS488);
        _M0L6_2atmpS2102 = _M0L2__S489 + 1;
        _M0L2__S489 = _M0L6_2atmpS2102;
        continue;
      } else {
        moonbit_decref(_M0L3strS488);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS487);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS482,
  struct _M0TPC16string10StringView _M0L3strS483
) {
  int32_t _M0L3lenS2090;
  int32_t _M0L6_2atmpS2092;
  int32_t _M0L6_2atmpS2091;
  int32_t _M0L6_2atmpS2089;
  moonbit_bytes_t _M0L8_2afieldS3546;
  moonbit_bytes_t _M0L4dataS2093;
  int32_t _M0L3lenS2094;
  moonbit_string_t _M0L6_2atmpS2095;
  int32_t _M0L6_2atmpS2096;
  int32_t _M0L6_2atmpS2097;
  int32_t _M0L3lenS2099;
  int32_t _M0L6_2atmpS2101;
  int32_t _M0L6_2atmpS2100;
  int32_t _M0L6_2atmpS2098;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2090 = _M0L4selfS482->$1;
  moonbit_incref(_M0L3strS483.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2092 = _M0MPC16string10StringView6length(_M0L3strS483);
  _M0L6_2atmpS2091 = _M0L6_2atmpS2092 * 2;
  _M0L6_2atmpS2089 = _M0L3lenS2090 + _M0L6_2atmpS2091;
  moonbit_incref(_M0L4selfS482);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS482, _M0L6_2atmpS2089);
  _M0L8_2afieldS3546 = _M0L4selfS482->$0;
  _M0L4dataS2093 = _M0L8_2afieldS3546;
  _M0L3lenS2094 = _M0L4selfS482->$1;
  moonbit_incref(_M0L4dataS2093);
  moonbit_incref(_M0L3strS483.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2095 = _M0MPC16string10StringView4data(_M0L3strS483);
  moonbit_incref(_M0L3strS483.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2096 = _M0MPC16string10StringView13start__offset(_M0L3strS483);
  moonbit_incref(_M0L3strS483.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2097 = _M0MPC16string10StringView6length(_M0L3strS483);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2093, _M0L3lenS2094, _M0L6_2atmpS2095, _M0L6_2atmpS2096, _M0L6_2atmpS2097);
  _M0L3lenS2099 = _M0L4selfS482->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2101 = _M0MPC16string10StringView6length(_M0L3strS483);
  _M0L6_2atmpS2100 = _M0L6_2atmpS2101 * 2;
  _M0L6_2atmpS2098 = _M0L3lenS2099 + _M0L6_2atmpS2100;
  _M0L4selfS482->$1 = _M0L6_2atmpS2098;
  moonbit_decref(_M0L4selfS482);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS474,
  int32_t _M0L3lenS477,
  int32_t _M0L13start__offsetS481,
  int64_t _M0L11end__offsetS472
) {
  int32_t _M0L11end__offsetS471;
  int32_t _M0L5indexS475;
  int32_t _M0L5countS476;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS472 == 4294967296ll) {
    _M0L11end__offsetS471 = Moonbit_array_length(_M0L4selfS474);
  } else {
    int64_t _M0L7_2aSomeS473 = _M0L11end__offsetS472;
    _M0L11end__offsetS471 = (int32_t)_M0L7_2aSomeS473;
  }
  _M0L5indexS475 = _M0L13start__offsetS481;
  _M0L5countS476 = 0;
  while (1) {
    int32_t _if__result_3907;
    if (_M0L5indexS475 < _M0L11end__offsetS471) {
      _if__result_3907 = _M0L5countS476 < _M0L3lenS477;
    } else {
      _if__result_3907 = 0;
    }
    if (_if__result_3907) {
      int32_t _M0L2c1S478 = _M0L4selfS474[_M0L5indexS475];
      int32_t _if__result_3908;
      int32_t _M0L6_2atmpS2087;
      int32_t _M0L6_2atmpS2088;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S478)) {
        int32_t _M0L6_2atmpS2083 = _M0L5indexS475 + 1;
        _if__result_3908 = _M0L6_2atmpS2083 < _M0L11end__offsetS471;
      } else {
        _if__result_3908 = 0;
      }
      if (_if__result_3908) {
        int32_t _M0L6_2atmpS2086 = _M0L5indexS475 + 1;
        int32_t _M0L2c2S479 = _M0L4selfS474[_M0L6_2atmpS2086];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S479)) {
          int32_t _M0L6_2atmpS2084 = _M0L5indexS475 + 2;
          int32_t _M0L6_2atmpS2085 = _M0L5countS476 + 1;
          _M0L5indexS475 = _M0L6_2atmpS2084;
          _M0L5countS476 = _M0L6_2atmpS2085;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_82.data, (moonbit_string_t)moonbit_string_literal_83.data);
        }
      }
      _M0L6_2atmpS2087 = _M0L5indexS475 + 1;
      _M0L6_2atmpS2088 = _M0L5countS476 + 1;
      _M0L5indexS475 = _M0L6_2atmpS2087;
      _M0L5countS476 = _M0L6_2atmpS2088;
      continue;
    } else {
      moonbit_decref(_M0L4selfS474);
      return _M0L5countS476 >= _M0L3lenS477;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS466
) {
  int32_t _M0L3endS2073;
  int32_t _M0L8_2afieldS3547;
  int32_t _M0L5startS2074;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2073 = _M0L4selfS466.$2;
  _M0L8_2afieldS3547 = _M0L4selfS466.$1;
  moonbit_decref(_M0L4selfS466.$0);
  _M0L5startS2074 = _M0L8_2afieldS3547;
  return _M0L3endS2073 - _M0L5startS2074;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS467
) {
  int32_t _M0L3endS2075;
  int32_t _M0L8_2afieldS3548;
  int32_t _M0L5startS2076;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2075 = _M0L4selfS467.$2;
  _M0L8_2afieldS3548 = _M0L4selfS467.$1;
  moonbit_decref(_M0L4selfS467.$0);
  _M0L5startS2076 = _M0L8_2afieldS3548;
  return _M0L3endS2075 - _M0L5startS2076;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS468
) {
  int32_t _M0L3endS2077;
  int32_t _M0L8_2afieldS3549;
  int32_t _M0L5startS2078;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2077 = _M0L4selfS468.$2;
  _M0L8_2afieldS3549 = _M0L4selfS468.$1;
  moonbit_decref(_M0L4selfS468.$0);
  _M0L5startS2078 = _M0L8_2afieldS3549;
  return _M0L3endS2077 - _M0L5startS2078;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS469
) {
  int32_t _M0L3endS2079;
  int32_t _M0L8_2afieldS3550;
  int32_t _M0L5startS2080;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2079 = _M0L4selfS469.$2;
  _M0L8_2afieldS3550 = _M0L4selfS469.$1;
  moonbit_decref(_M0L4selfS469.$0);
  _M0L5startS2080 = _M0L8_2afieldS3550;
  return _M0L3endS2079 - _M0L5startS2080;
}

int32_t _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4selfS470
) {
  int32_t _M0L3endS2081;
  int32_t _M0L8_2afieldS3551;
  int32_t _M0L5startS2082;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2081 = _M0L4selfS470.$2;
  _M0L8_2afieldS3551 = _M0L4selfS470.$1;
  moonbit_decref(_M0L4selfS470.$0);
  _M0L5startS2082 = _M0L8_2afieldS3551;
  return _M0L3endS2081 - _M0L5startS2082;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS464,
  int64_t _M0L19start__offset_2eoptS462,
  int64_t _M0L11end__offsetS465
) {
  int32_t _M0L13start__offsetS461;
  if (_M0L19start__offset_2eoptS462 == 4294967296ll) {
    _M0L13start__offsetS461 = 0;
  } else {
    int64_t _M0L7_2aSomeS463 = _M0L19start__offset_2eoptS462;
    _M0L13start__offsetS461 = (int32_t)_M0L7_2aSomeS463;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS464, _M0L13start__offsetS461, _M0L11end__offsetS465);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS459,
  int32_t _M0L13start__offsetS460,
  int64_t _M0L11end__offsetS457
) {
  int32_t _M0L11end__offsetS456;
  int32_t _if__result_3909;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS457 == 4294967296ll) {
    _M0L11end__offsetS456 = Moonbit_array_length(_M0L4selfS459);
  } else {
    int64_t _M0L7_2aSomeS458 = _M0L11end__offsetS457;
    _M0L11end__offsetS456 = (int32_t)_M0L7_2aSomeS458;
  }
  if (_M0L13start__offsetS460 >= 0) {
    if (_M0L13start__offsetS460 <= _M0L11end__offsetS456) {
      int32_t _M0L6_2atmpS2072 = Moonbit_array_length(_M0L4selfS459);
      _if__result_3909 = _M0L11end__offsetS456 <= _M0L6_2atmpS2072;
    } else {
      _if__result_3909 = 0;
    }
  } else {
    _if__result_3909 = 0;
  }
  if (_if__result_3909) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS460,
                                                 _M0L11end__offsetS456,
                                                 _M0L4selfS459};
  } else {
    moonbit_decref(_M0L4selfS459);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_84.data, (moonbit_string_t)moonbit_string_literal_85.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS455
) {
  moonbit_string_t _M0L8_2afieldS3553;
  moonbit_string_t _M0L3strS2069;
  int32_t _M0L5startS2070;
  int32_t _M0L8_2afieldS3552;
  int32_t _M0L3endS2071;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3553 = _M0L4selfS455.$0;
  _M0L3strS2069 = _M0L8_2afieldS3553;
  _M0L5startS2070 = _M0L4selfS455.$1;
  _M0L8_2afieldS3552 = _M0L4selfS455.$2;
  _M0L3endS2071 = _M0L8_2afieldS3552;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2069, _M0L5startS2070, _M0L3endS2071);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS453,
  struct _M0TPB6Logger _M0L6loggerS454
) {
  moonbit_string_t _M0L8_2afieldS3555;
  moonbit_string_t _M0L3strS2066;
  int32_t _M0L5startS2067;
  int32_t _M0L8_2afieldS3554;
  int32_t _M0L3endS2068;
  moonbit_string_t _M0L6substrS452;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3555 = _M0L4selfS453.$0;
  _M0L3strS2066 = _M0L8_2afieldS3555;
  _M0L5startS2067 = _M0L4selfS453.$1;
  _M0L8_2afieldS3554 = _M0L4selfS453.$2;
  _M0L3endS2068 = _M0L8_2afieldS3554;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS452
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2066, _M0L5startS2067, _M0L3endS2068);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS452, _M0L6loggerS454);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS444,
  struct _M0TPB6Logger _M0L6loggerS442
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS443;
  int32_t _M0L3lenS445;
  int32_t _M0L1iS446;
  int32_t _M0L3segS447;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS442.$1) {
    moonbit_incref(_M0L6loggerS442.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS442.$0->$method_3(_M0L6loggerS442.$1, 34);
  moonbit_incref(_M0L4selfS444);
  if (_M0L6loggerS442.$1) {
    moonbit_incref(_M0L6loggerS442.$1);
  }
  _M0L6_2aenvS443
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS443)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS443->$0 = _M0L4selfS444;
  _M0L6_2aenvS443->$1_0 = _M0L6loggerS442.$0;
  _M0L6_2aenvS443->$1_1 = _M0L6loggerS442.$1;
  _M0L3lenS445 = Moonbit_array_length(_M0L4selfS444);
  _M0L1iS446 = 0;
  _M0L3segS447 = 0;
  _2afor_448:;
  while (1) {
    int32_t _M0L4codeS449;
    int32_t _M0L1cS451;
    int32_t _M0L6_2atmpS2050;
    int32_t _M0L6_2atmpS2051;
    int32_t _M0L6_2atmpS2052;
    int32_t _tmp_3913;
    int32_t _tmp_3914;
    if (_M0L1iS446 >= _M0L3lenS445) {
      moonbit_decref(_M0L4selfS444);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
      break;
    }
    _M0L4codeS449 = _M0L4selfS444[_M0L1iS446];
    switch (_M0L4codeS449) {
      case 34: {
        _M0L1cS451 = _M0L4codeS449;
        goto join_450;
        break;
      }
      
      case 92: {
        _M0L1cS451 = _M0L4codeS449;
        goto join_450;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2053;
        int32_t _M0L6_2atmpS2054;
        moonbit_incref(_M0L6_2aenvS443);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_66.data);
        _M0L6_2atmpS2053 = _M0L1iS446 + 1;
        _M0L6_2atmpS2054 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2053;
        _M0L3segS447 = _M0L6_2atmpS2054;
        goto _2afor_448;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2055;
        int32_t _M0L6_2atmpS2056;
        moonbit_incref(_M0L6_2aenvS443);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_67.data);
        _M0L6_2atmpS2055 = _M0L1iS446 + 1;
        _M0L6_2atmpS2056 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2055;
        _M0L3segS447 = _M0L6_2atmpS2056;
        goto _2afor_448;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2057;
        int32_t _M0L6_2atmpS2058;
        moonbit_incref(_M0L6_2aenvS443);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_68.data);
        _M0L6_2atmpS2057 = _M0L1iS446 + 1;
        _M0L6_2atmpS2058 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2057;
        _M0L3segS447 = _M0L6_2atmpS2058;
        goto _2afor_448;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2059;
        int32_t _M0L6_2atmpS2060;
        moonbit_incref(_M0L6_2aenvS443);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_69.data);
        _M0L6_2atmpS2059 = _M0L1iS446 + 1;
        _M0L6_2atmpS2060 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2059;
        _M0L3segS447 = _M0L6_2atmpS2060;
        goto _2afor_448;
        break;
      }
      default: {
        if (_M0L4codeS449 < 32) {
          int32_t _M0L6_2atmpS2062;
          moonbit_string_t _M0L6_2atmpS2061;
          int32_t _M0L6_2atmpS2063;
          int32_t _M0L6_2atmpS2064;
          moonbit_incref(_M0L6_2aenvS443);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
          if (_M0L6loggerS442.$1) {
            moonbit_incref(_M0L6loggerS442.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_86.data);
          _M0L6_2atmpS2062 = _M0L4codeS449 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2061 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2062);
          if (_M0L6loggerS442.$1) {
            moonbit_incref(_M0L6loggerS442.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, _M0L6_2atmpS2061);
          if (_M0L6loggerS442.$1) {
            moonbit_incref(_M0L6loggerS442.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS442.$0->$method_3(_M0L6loggerS442.$1, 125);
          _M0L6_2atmpS2063 = _M0L1iS446 + 1;
          _M0L6_2atmpS2064 = _M0L1iS446 + 1;
          _M0L1iS446 = _M0L6_2atmpS2063;
          _M0L3segS447 = _M0L6_2atmpS2064;
          goto _2afor_448;
        } else {
          int32_t _M0L6_2atmpS2065 = _M0L1iS446 + 1;
          int32_t _tmp_3912 = _M0L3segS447;
          _M0L1iS446 = _M0L6_2atmpS2065;
          _M0L3segS447 = _tmp_3912;
          goto _2afor_448;
        }
        break;
      }
    }
    goto joinlet_3911;
    join_450:;
    moonbit_incref(_M0L6_2aenvS443);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
    if (_M0L6loggerS442.$1) {
      moonbit_incref(_M0L6loggerS442.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS442.$0->$method_3(_M0L6loggerS442.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2050 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS451);
    if (_M0L6loggerS442.$1) {
      moonbit_incref(_M0L6loggerS442.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS442.$0->$method_3(_M0L6loggerS442.$1, _M0L6_2atmpS2050);
    _M0L6_2atmpS2051 = _M0L1iS446 + 1;
    _M0L6_2atmpS2052 = _M0L1iS446 + 1;
    _M0L1iS446 = _M0L6_2atmpS2051;
    _M0L3segS447 = _M0L6_2atmpS2052;
    continue;
    joinlet_3911:;
    _tmp_3913 = _M0L1iS446;
    _tmp_3914 = _M0L3segS447;
    _M0L1iS446 = _tmp_3913;
    _M0L3segS447 = _tmp_3914;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS442.$0->$method_3(_M0L6loggerS442.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS438,
  int32_t _M0L3segS441,
  int32_t _M0L1iS440
) {
  struct _M0TPB6Logger _M0L8_2afieldS3557;
  struct _M0TPB6Logger _M0L6loggerS437;
  moonbit_string_t _M0L8_2afieldS3556;
  int32_t _M0L6_2acntS3739;
  moonbit_string_t _M0L4selfS439;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3557
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS438->$1_0, _M0L6_2aenvS438->$1_1
  };
  _M0L6loggerS437 = _M0L8_2afieldS3557;
  _M0L8_2afieldS3556 = _M0L6_2aenvS438->$0;
  _M0L6_2acntS3739 = Moonbit_object_header(_M0L6_2aenvS438)->rc;
  if (_M0L6_2acntS3739 > 1) {
    int32_t _M0L11_2anew__cntS3740 = _M0L6_2acntS3739 - 1;
    Moonbit_object_header(_M0L6_2aenvS438)->rc = _M0L11_2anew__cntS3740;
    if (_M0L6loggerS437.$1) {
      moonbit_incref(_M0L6loggerS437.$1);
    }
    moonbit_incref(_M0L8_2afieldS3556);
  } else if (_M0L6_2acntS3739 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS438);
  }
  _M0L4selfS439 = _M0L8_2afieldS3556;
  if (_M0L1iS440 > _M0L3segS441) {
    int32_t _M0L6_2atmpS2049 = _M0L1iS440 - _M0L3segS441;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS437.$0->$method_1(_M0L6loggerS437.$1, _M0L4selfS439, _M0L3segS441, _M0L6_2atmpS2049);
  } else {
    moonbit_decref(_M0L4selfS439);
    if (_M0L6loggerS437.$1) {
      moonbit_decref(_M0L6loggerS437.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS436) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS435;
  int32_t _M0L6_2atmpS2046;
  int32_t _M0L6_2atmpS2045;
  int32_t _M0L6_2atmpS2048;
  int32_t _M0L6_2atmpS2047;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2044;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS435 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2046 = _M0IPC14byte4BytePB3Div3div(_M0L1bS436, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2045
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2046);
  moonbit_incref(_M0L7_2aselfS435);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS435, _M0L6_2atmpS2045);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2048 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS436, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2047
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2048);
  moonbit_incref(_M0L7_2aselfS435);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS435, _M0L6_2atmpS2047);
  _M0L6_2atmpS2044 = _M0L7_2aselfS435;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2044);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS434) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS434 < 10) {
    int32_t _M0L6_2atmpS2041;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2041 = _M0IPC14byte4BytePB3Add3add(_M0L1iS434, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2041);
  } else {
    int32_t _M0L6_2atmpS2043;
    int32_t _M0L6_2atmpS2042;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2043 = _M0IPC14byte4BytePB3Add3add(_M0L1iS434, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2042 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2043, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2042);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS432,
  int32_t _M0L4thatS433
) {
  int32_t _M0L6_2atmpS2039;
  int32_t _M0L6_2atmpS2040;
  int32_t _M0L6_2atmpS2038;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2039 = (int32_t)_M0L4selfS432;
  _M0L6_2atmpS2040 = (int32_t)_M0L4thatS433;
  _M0L6_2atmpS2038 = _M0L6_2atmpS2039 - _M0L6_2atmpS2040;
  return _M0L6_2atmpS2038 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS430,
  int32_t _M0L4thatS431
) {
  int32_t _M0L6_2atmpS2036;
  int32_t _M0L6_2atmpS2037;
  int32_t _M0L6_2atmpS2035;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2036 = (int32_t)_M0L4selfS430;
  _M0L6_2atmpS2037 = (int32_t)_M0L4thatS431;
  _M0L6_2atmpS2035 = _M0L6_2atmpS2036 % _M0L6_2atmpS2037;
  return _M0L6_2atmpS2035 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS428,
  int32_t _M0L4thatS429
) {
  int32_t _M0L6_2atmpS2033;
  int32_t _M0L6_2atmpS2034;
  int32_t _M0L6_2atmpS2032;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2033 = (int32_t)_M0L4selfS428;
  _M0L6_2atmpS2034 = (int32_t)_M0L4thatS429;
  _M0L6_2atmpS2032 = _M0L6_2atmpS2033 / _M0L6_2atmpS2034;
  return _M0L6_2atmpS2032 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS426,
  int32_t _M0L4thatS427
) {
  int32_t _M0L6_2atmpS2030;
  int32_t _M0L6_2atmpS2031;
  int32_t _M0L6_2atmpS2029;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2030 = (int32_t)_M0L4selfS426;
  _M0L6_2atmpS2031 = (int32_t)_M0L4thatS427;
  _M0L6_2atmpS2029 = _M0L6_2atmpS2030 + _M0L6_2atmpS2031;
  return _M0L6_2atmpS2029 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS423,
  int32_t _M0L5startS421,
  int32_t _M0L3endS422
) {
  int32_t _if__result_3915;
  int32_t _M0L3lenS424;
  int32_t _M0L6_2atmpS2027;
  int32_t _M0L6_2atmpS2028;
  moonbit_bytes_t _M0L5bytesS425;
  moonbit_bytes_t _M0L6_2atmpS2026;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS421 == 0) {
    int32_t _M0L6_2atmpS2025 = Moonbit_array_length(_M0L3strS423);
    _if__result_3915 = _M0L3endS422 == _M0L6_2atmpS2025;
  } else {
    _if__result_3915 = 0;
  }
  if (_if__result_3915) {
    return _M0L3strS423;
  }
  _M0L3lenS424 = _M0L3endS422 - _M0L5startS421;
  _M0L6_2atmpS2027 = _M0L3lenS424 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2028 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS425
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2027, _M0L6_2atmpS2028);
  moonbit_incref(_M0L5bytesS425);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS425, 0, _M0L3strS423, _M0L5startS421, _M0L3lenS424);
  _M0L6_2atmpS2026 = _M0L5bytesS425;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2026, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS418) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS418;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS419
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS419;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS420) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS420;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS410,
  int32_t _M0L5radixS409
) {
  int32_t _if__result_3916;
  uint16_t* _M0L6bufferS411;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS409 < 2) {
    _if__result_3916 = 1;
  } else {
    _if__result_3916 = _M0L5radixS409 > 36;
  }
  if (_if__result_3916) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_87.data, (moonbit_string_t)moonbit_string_literal_88.data);
  }
  if (_M0L4selfS410 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_74.data;
  }
  switch (_M0L5radixS409) {
    case 10: {
      int32_t _M0L3lenS412;
      uint16_t* _M0L6bufferS413;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS412 = _M0FPB12dec__count64(_M0L4selfS410);
      _M0L6bufferS413 = (uint16_t*)moonbit_make_string(_M0L3lenS412, 0);
      moonbit_incref(_M0L6bufferS413);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS413, _M0L4selfS410, 0, _M0L3lenS412);
      _M0L6bufferS411 = _M0L6bufferS413;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS414;
      uint16_t* _M0L6bufferS415;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS414 = _M0FPB12hex__count64(_M0L4selfS410);
      _M0L6bufferS415 = (uint16_t*)moonbit_make_string(_M0L3lenS414, 0);
      moonbit_incref(_M0L6bufferS415);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS415, _M0L4selfS410, 0, _M0L3lenS414);
      _M0L6bufferS411 = _M0L6bufferS415;
      break;
    }
    default: {
      int32_t _M0L3lenS416;
      uint16_t* _M0L6bufferS417;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS416 = _M0FPB14radix__count64(_M0L4selfS410, _M0L5radixS409);
      _M0L6bufferS417 = (uint16_t*)moonbit_make_string(_M0L3lenS416, 0);
      moonbit_incref(_M0L6bufferS417);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS417, _M0L4selfS410, 0, _M0L3lenS416, _M0L5radixS409);
      _M0L6bufferS411 = _M0L6bufferS417;
      break;
    }
  }
  return _M0L6bufferS411;
}

moonbit_string_t _M0MPC15int645Int6418to__string_2einner(
  int64_t _M0L4selfS393,
  int32_t _M0L5radixS392
) {
  int32_t _if__result_3917;
  int32_t _M0L12is__negativeS394;
  uint64_t _M0L3numS395;
  uint16_t* _M0L6bufferS396;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS392 < 2) {
    _if__result_3917 = 1;
  } else {
    _if__result_3917 = _M0L5radixS392 > 36;
  }
  if (_if__result_3917) {
    #line 574 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_87.data, (moonbit_string_t)moonbit_string_literal_89.data);
  }
  if (_M0L4selfS393 == 0ll) {
    return (moonbit_string_t)moonbit_string_literal_74.data;
  }
  _M0L12is__negativeS394 = _M0L4selfS393 < 0ll;
  if (_M0L12is__negativeS394) {
    int64_t _M0L6_2atmpS2024 = -_M0L4selfS393;
    _M0L3numS395 = *(uint64_t*)&_M0L6_2atmpS2024;
  } else {
    _M0L3numS395 = *(uint64_t*)&_M0L4selfS393;
  }
  switch (_M0L5radixS392) {
    case 10: {
      int32_t _M0L10digit__lenS397;
      int32_t _M0L6_2atmpS2021;
      int32_t _M0L10total__lenS398;
      uint16_t* _M0L6bufferS399;
      int32_t _M0L12digit__startS400;
      #line 595 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS397 = _M0FPB12dec__count64(_M0L3numS395);
      if (_M0L12is__negativeS394) {
        _M0L6_2atmpS2021 = 1;
      } else {
        _M0L6_2atmpS2021 = 0;
      }
      _M0L10total__lenS398 = _M0L10digit__lenS397 + _M0L6_2atmpS2021;
      _M0L6bufferS399
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS398, 0);
      if (_M0L12is__negativeS394) {
        _M0L12digit__startS400 = 1;
      } else {
        _M0L12digit__startS400 = 0;
      }
      moonbit_incref(_M0L6bufferS399);
      #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS399, _M0L3numS395, _M0L12digit__startS400, _M0L10total__lenS398);
      _M0L6bufferS396 = _M0L6bufferS399;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS401;
      int32_t _M0L6_2atmpS2022;
      int32_t _M0L10total__lenS402;
      uint16_t* _M0L6bufferS403;
      int32_t _M0L12digit__startS404;
      #line 603 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS401 = _M0FPB12hex__count64(_M0L3numS395);
      if (_M0L12is__negativeS394) {
        _M0L6_2atmpS2022 = 1;
      } else {
        _M0L6_2atmpS2022 = 0;
      }
      _M0L10total__lenS402 = _M0L10digit__lenS401 + _M0L6_2atmpS2022;
      _M0L6bufferS403
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS402, 0);
      if (_M0L12is__negativeS394) {
        _M0L12digit__startS404 = 1;
      } else {
        _M0L12digit__startS404 = 0;
      }
      moonbit_incref(_M0L6bufferS403);
      #line 607 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS403, _M0L3numS395, _M0L12digit__startS404, _M0L10total__lenS402);
      _M0L6bufferS396 = _M0L6bufferS403;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS405;
      int32_t _M0L6_2atmpS2023;
      int32_t _M0L10total__lenS406;
      uint16_t* _M0L6bufferS407;
      int32_t _M0L12digit__startS408;
      #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS405
      = _M0FPB14radix__count64(_M0L3numS395, _M0L5radixS392);
      if (_M0L12is__negativeS394) {
        _M0L6_2atmpS2023 = 1;
      } else {
        _M0L6_2atmpS2023 = 0;
      }
      _M0L10total__lenS406 = _M0L10digit__lenS405 + _M0L6_2atmpS2023;
      _M0L6bufferS407
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS406, 0);
      if (_M0L12is__negativeS394) {
        _M0L12digit__startS408 = 1;
      } else {
        _M0L12digit__startS408 = 0;
      }
      moonbit_incref(_M0L6bufferS407);
      #line 615 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS407, _M0L3numS395, _M0L12digit__startS408, _M0L10total__lenS406, _M0L5radixS392);
      _M0L6bufferS396 = _M0L6bufferS407;
      break;
    }
  }
  if (_M0L12is__negativeS394) {
    _M0L6bufferS396[0] = 45;
  }
  return _M0L6bufferS396;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS382,
  uint64_t _M0L3numS370,
  int32_t _M0L12digit__startS373,
  int32_t _M0L10total__lenS372
) {
  uint64_t _M0Lm3numS369;
  int32_t _M0Lm6offsetS371;
  uint64_t _M0L6_2atmpS2020;
  int32_t _M0Lm9remainingS384;
  int32_t _M0L6_2atmpS2001;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS369 = _M0L3numS370;
  _M0Lm6offsetS371 = _M0L10total__lenS372 - _M0L12digit__startS373;
  while (1) {
    uint64_t _M0L6_2atmpS1964 = _M0Lm3numS369;
    if (_M0L6_2atmpS1964 >= 10000ull) {
      uint64_t _M0L6_2atmpS1987 = _M0Lm3numS369;
      uint64_t _M0L1tS374 = _M0L6_2atmpS1987 / 10000ull;
      uint64_t _M0L6_2atmpS1986 = _M0Lm3numS369;
      uint64_t _M0L6_2atmpS1985 = _M0L6_2atmpS1986 % 10000ull;
      int32_t _M0L1rS375 = (int32_t)_M0L6_2atmpS1985;
      int32_t _M0L2d1S376;
      int32_t _M0L2d2S377;
      int32_t _M0L6_2atmpS1965;
      int32_t _M0L6_2atmpS1984;
      int32_t _M0L6_2atmpS1983;
      int32_t _M0L6d1__hiS378;
      int32_t _M0L6_2atmpS1982;
      int32_t _M0L6_2atmpS1981;
      int32_t _M0L6d1__loS379;
      int32_t _M0L6_2atmpS1980;
      int32_t _M0L6_2atmpS1979;
      int32_t _M0L6d2__hiS380;
      int32_t _M0L6_2atmpS1978;
      int32_t _M0L6_2atmpS1977;
      int32_t _M0L6d2__loS381;
      int32_t _M0L6_2atmpS1967;
      int32_t _M0L6_2atmpS1966;
      int32_t _M0L6_2atmpS1970;
      int32_t _M0L6_2atmpS1969;
      int32_t _M0L6_2atmpS1968;
      int32_t _M0L6_2atmpS1973;
      int32_t _M0L6_2atmpS1972;
      int32_t _M0L6_2atmpS1971;
      int32_t _M0L6_2atmpS1976;
      int32_t _M0L6_2atmpS1975;
      int32_t _M0L6_2atmpS1974;
      _M0Lm3numS369 = _M0L1tS374;
      _M0L2d1S376 = _M0L1rS375 / 100;
      _M0L2d2S377 = _M0L1rS375 % 100;
      _M0L6_2atmpS1965 = _M0Lm6offsetS371;
      _M0Lm6offsetS371 = _M0L6_2atmpS1965 - 4;
      _M0L6_2atmpS1984 = _M0L2d1S376 / 10;
      _M0L6_2atmpS1983 = 48 + _M0L6_2atmpS1984;
      _M0L6d1__hiS378 = (uint16_t)_M0L6_2atmpS1983;
      _M0L6_2atmpS1982 = _M0L2d1S376 % 10;
      _M0L6_2atmpS1981 = 48 + _M0L6_2atmpS1982;
      _M0L6d1__loS379 = (uint16_t)_M0L6_2atmpS1981;
      _M0L6_2atmpS1980 = _M0L2d2S377 / 10;
      _M0L6_2atmpS1979 = 48 + _M0L6_2atmpS1980;
      _M0L6d2__hiS380 = (uint16_t)_M0L6_2atmpS1979;
      _M0L6_2atmpS1978 = _M0L2d2S377 % 10;
      _M0L6_2atmpS1977 = 48 + _M0L6_2atmpS1978;
      _M0L6d2__loS381 = (uint16_t)_M0L6_2atmpS1977;
      _M0L6_2atmpS1967 = _M0Lm6offsetS371;
      _M0L6_2atmpS1966 = _M0L12digit__startS373 + _M0L6_2atmpS1967;
      _M0L6bufferS382[_M0L6_2atmpS1966] = _M0L6d1__hiS378;
      _M0L6_2atmpS1970 = _M0Lm6offsetS371;
      _M0L6_2atmpS1969 = _M0L12digit__startS373 + _M0L6_2atmpS1970;
      _M0L6_2atmpS1968 = _M0L6_2atmpS1969 + 1;
      _M0L6bufferS382[_M0L6_2atmpS1968] = _M0L6d1__loS379;
      _M0L6_2atmpS1973 = _M0Lm6offsetS371;
      _M0L6_2atmpS1972 = _M0L12digit__startS373 + _M0L6_2atmpS1973;
      _M0L6_2atmpS1971 = _M0L6_2atmpS1972 + 2;
      _M0L6bufferS382[_M0L6_2atmpS1971] = _M0L6d2__hiS380;
      _M0L6_2atmpS1976 = _M0Lm6offsetS371;
      _M0L6_2atmpS1975 = _M0L12digit__startS373 + _M0L6_2atmpS1976;
      _M0L6_2atmpS1974 = _M0L6_2atmpS1975 + 3;
      _M0L6bufferS382[_M0L6_2atmpS1974] = _M0L6d2__loS381;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2020 = _M0Lm3numS369;
  _M0Lm9remainingS384 = (int32_t)_M0L6_2atmpS2020;
  while (1) {
    int32_t _M0L6_2atmpS1988 = _M0Lm9remainingS384;
    if (_M0L6_2atmpS1988 >= 100) {
      int32_t _M0L6_2atmpS2000 = _M0Lm9remainingS384;
      int32_t _M0L1tS385 = _M0L6_2atmpS2000 / 100;
      int32_t _M0L6_2atmpS1999 = _M0Lm9remainingS384;
      int32_t _M0L1dS386 = _M0L6_2atmpS1999 % 100;
      int32_t _M0L6_2atmpS1989;
      int32_t _M0L6_2atmpS1998;
      int32_t _M0L6_2atmpS1997;
      int32_t _M0L5d__hiS387;
      int32_t _M0L6_2atmpS1996;
      int32_t _M0L6_2atmpS1995;
      int32_t _M0L5d__loS388;
      int32_t _M0L6_2atmpS1991;
      int32_t _M0L6_2atmpS1990;
      int32_t _M0L6_2atmpS1994;
      int32_t _M0L6_2atmpS1993;
      int32_t _M0L6_2atmpS1992;
      _M0Lm9remainingS384 = _M0L1tS385;
      _M0L6_2atmpS1989 = _M0Lm6offsetS371;
      _M0Lm6offsetS371 = _M0L6_2atmpS1989 - 2;
      _M0L6_2atmpS1998 = _M0L1dS386 / 10;
      _M0L6_2atmpS1997 = 48 + _M0L6_2atmpS1998;
      _M0L5d__hiS387 = (uint16_t)_M0L6_2atmpS1997;
      _M0L6_2atmpS1996 = _M0L1dS386 % 10;
      _M0L6_2atmpS1995 = 48 + _M0L6_2atmpS1996;
      _M0L5d__loS388 = (uint16_t)_M0L6_2atmpS1995;
      _M0L6_2atmpS1991 = _M0Lm6offsetS371;
      _M0L6_2atmpS1990 = _M0L12digit__startS373 + _M0L6_2atmpS1991;
      _M0L6bufferS382[_M0L6_2atmpS1990] = _M0L5d__hiS387;
      _M0L6_2atmpS1994 = _M0Lm6offsetS371;
      _M0L6_2atmpS1993 = _M0L12digit__startS373 + _M0L6_2atmpS1994;
      _M0L6_2atmpS1992 = _M0L6_2atmpS1993 + 1;
      _M0L6bufferS382[_M0L6_2atmpS1992] = _M0L5d__loS388;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2001 = _M0Lm9remainingS384;
  if (_M0L6_2atmpS2001 >= 10) {
    int32_t _M0L6_2atmpS2002 = _M0Lm6offsetS371;
    int32_t _M0L6_2atmpS2013;
    int32_t _M0L6_2atmpS2012;
    int32_t _M0L6_2atmpS2011;
    int32_t _M0L5d__hiS390;
    int32_t _M0L6_2atmpS2010;
    int32_t _M0L6_2atmpS2009;
    int32_t _M0L6_2atmpS2008;
    int32_t _M0L5d__loS391;
    int32_t _M0L6_2atmpS2004;
    int32_t _M0L6_2atmpS2003;
    int32_t _M0L6_2atmpS2007;
    int32_t _M0L6_2atmpS2006;
    int32_t _M0L6_2atmpS2005;
    _M0Lm6offsetS371 = _M0L6_2atmpS2002 - 2;
    _M0L6_2atmpS2013 = _M0Lm9remainingS384;
    _M0L6_2atmpS2012 = _M0L6_2atmpS2013 / 10;
    _M0L6_2atmpS2011 = 48 + _M0L6_2atmpS2012;
    _M0L5d__hiS390 = (uint16_t)_M0L6_2atmpS2011;
    _M0L6_2atmpS2010 = _M0Lm9remainingS384;
    _M0L6_2atmpS2009 = _M0L6_2atmpS2010 % 10;
    _M0L6_2atmpS2008 = 48 + _M0L6_2atmpS2009;
    _M0L5d__loS391 = (uint16_t)_M0L6_2atmpS2008;
    _M0L6_2atmpS2004 = _M0Lm6offsetS371;
    _M0L6_2atmpS2003 = _M0L12digit__startS373 + _M0L6_2atmpS2004;
    _M0L6bufferS382[_M0L6_2atmpS2003] = _M0L5d__hiS390;
    _M0L6_2atmpS2007 = _M0Lm6offsetS371;
    _M0L6_2atmpS2006 = _M0L12digit__startS373 + _M0L6_2atmpS2007;
    _M0L6_2atmpS2005 = _M0L6_2atmpS2006 + 1;
    _M0L6bufferS382[_M0L6_2atmpS2005] = _M0L5d__loS391;
    moonbit_decref(_M0L6bufferS382);
  } else {
    int32_t _M0L6_2atmpS2014 = _M0Lm6offsetS371;
    int32_t _M0L6_2atmpS2019;
    int32_t _M0L6_2atmpS2015;
    int32_t _M0L6_2atmpS2018;
    int32_t _M0L6_2atmpS2017;
    int32_t _M0L6_2atmpS2016;
    _M0Lm6offsetS371 = _M0L6_2atmpS2014 - 1;
    _M0L6_2atmpS2019 = _M0Lm6offsetS371;
    _M0L6_2atmpS2015 = _M0L12digit__startS373 + _M0L6_2atmpS2019;
    _M0L6_2atmpS2018 = _M0Lm9remainingS384;
    _M0L6_2atmpS2017 = 48 + _M0L6_2atmpS2018;
    _M0L6_2atmpS2016 = (uint16_t)_M0L6_2atmpS2017;
    _M0L6bufferS382[_M0L6_2atmpS2015] = _M0L6_2atmpS2016;
    moonbit_decref(_M0L6bufferS382);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS364,
  uint64_t _M0L3numS358,
  int32_t _M0L12digit__startS356,
  int32_t _M0L10total__lenS355,
  int32_t _M0L5radixS360
) {
  int32_t _M0Lm6offsetS354;
  uint64_t _M0Lm1nS357;
  uint64_t _M0L4baseS359;
  int32_t _M0L6_2atmpS1946;
  int32_t _M0L6_2atmpS1945;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS354 = _M0L10total__lenS355 - _M0L12digit__startS356;
  _M0Lm1nS357 = _M0L3numS358;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS359 = _M0MPC13int3Int10to__uint64(_M0L5radixS360);
  _M0L6_2atmpS1946 = _M0L5radixS360 - 1;
  _M0L6_2atmpS1945 = _M0L5radixS360 & _M0L6_2atmpS1946;
  if (_M0L6_2atmpS1945 == 0) {
    int32_t _M0L5shiftS361;
    uint64_t _M0L4maskS362;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS361 = moonbit_ctz32(_M0L5radixS360);
    _M0L4maskS362 = _M0L4baseS359 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1947 = _M0Lm1nS357;
      if (_M0L6_2atmpS1947 > 0ull) {
        int32_t _M0L6_2atmpS1948 = _M0Lm6offsetS354;
        uint64_t _M0L6_2atmpS1954;
        uint64_t _M0L6_2atmpS1953;
        int32_t _M0L5digitS363;
        int32_t _M0L6_2atmpS1951;
        int32_t _M0L6_2atmpS1949;
        int32_t _M0L6_2atmpS1950;
        uint64_t _M0L6_2atmpS1952;
        _M0Lm6offsetS354 = _M0L6_2atmpS1948 - 1;
        _M0L6_2atmpS1954 = _M0Lm1nS357;
        _M0L6_2atmpS1953 = _M0L6_2atmpS1954 & _M0L4maskS362;
        _M0L5digitS363 = (int32_t)_M0L6_2atmpS1953;
        _M0L6_2atmpS1951 = _M0Lm6offsetS354;
        _M0L6_2atmpS1949 = _M0L12digit__startS356 + _M0L6_2atmpS1951;
        _M0L6_2atmpS1950
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS363
        ];
        _M0L6bufferS364[_M0L6_2atmpS1949] = _M0L6_2atmpS1950;
        _M0L6_2atmpS1952 = _M0Lm1nS357;
        _M0Lm1nS357 = _M0L6_2atmpS1952 >> (_M0L5shiftS361 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS364);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1955 = _M0Lm1nS357;
      if (_M0L6_2atmpS1955 > 0ull) {
        int32_t _M0L6_2atmpS1956 = _M0Lm6offsetS354;
        uint64_t _M0L6_2atmpS1963;
        uint64_t _M0L1qS366;
        uint64_t _M0L6_2atmpS1961;
        uint64_t _M0L6_2atmpS1962;
        uint64_t _M0L6_2atmpS1960;
        int32_t _M0L5digitS367;
        int32_t _M0L6_2atmpS1959;
        int32_t _M0L6_2atmpS1957;
        int32_t _M0L6_2atmpS1958;
        _M0Lm6offsetS354 = _M0L6_2atmpS1956 - 1;
        _M0L6_2atmpS1963 = _M0Lm1nS357;
        _M0L1qS366 = _M0L6_2atmpS1963 / _M0L4baseS359;
        _M0L6_2atmpS1961 = _M0Lm1nS357;
        _M0L6_2atmpS1962 = _M0L1qS366 * _M0L4baseS359;
        _M0L6_2atmpS1960 = _M0L6_2atmpS1961 - _M0L6_2atmpS1962;
        _M0L5digitS367 = (int32_t)_M0L6_2atmpS1960;
        _M0L6_2atmpS1959 = _M0Lm6offsetS354;
        _M0L6_2atmpS1957 = _M0L12digit__startS356 + _M0L6_2atmpS1959;
        _M0L6_2atmpS1958
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS367
        ];
        _M0L6bufferS364[_M0L6_2atmpS1957] = _M0L6_2atmpS1958;
        _M0Lm1nS357 = _M0L1qS366;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS364);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS351,
  uint64_t _M0L3numS347,
  int32_t _M0L12digit__startS345,
  int32_t _M0L10total__lenS344
) {
  int32_t _M0Lm6offsetS343;
  uint64_t _M0Lm1nS346;
  int32_t _M0L6_2atmpS1941;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS343 = _M0L10total__lenS344 - _M0L12digit__startS345;
  _M0Lm1nS346 = _M0L3numS347;
  while (1) {
    int32_t _M0L6_2atmpS1929 = _M0Lm6offsetS343;
    if (_M0L6_2atmpS1929 >= 2) {
      int32_t _M0L6_2atmpS1930 = _M0Lm6offsetS343;
      uint64_t _M0L6_2atmpS1940;
      uint64_t _M0L6_2atmpS1939;
      int32_t _M0L9byte__valS348;
      int32_t _M0L2hiS349;
      int32_t _M0L2loS350;
      int32_t _M0L6_2atmpS1933;
      int32_t _M0L6_2atmpS1931;
      int32_t _M0L6_2atmpS1932;
      int32_t _M0L6_2atmpS1937;
      int32_t _M0L6_2atmpS1936;
      int32_t _M0L6_2atmpS1934;
      int32_t _M0L6_2atmpS1935;
      uint64_t _M0L6_2atmpS1938;
      _M0Lm6offsetS343 = _M0L6_2atmpS1930 - 2;
      _M0L6_2atmpS1940 = _M0Lm1nS346;
      _M0L6_2atmpS1939 = _M0L6_2atmpS1940 & 255ull;
      _M0L9byte__valS348 = (int32_t)_M0L6_2atmpS1939;
      _M0L2hiS349 = _M0L9byte__valS348 / 16;
      _M0L2loS350 = _M0L9byte__valS348 % 16;
      _M0L6_2atmpS1933 = _M0Lm6offsetS343;
      _M0L6_2atmpS1931 = _M0L12digit__startS345 + _M0L6_2atmpS1933;
      _M0L6_2atmpS1932
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2hiS349
      ];
      _M0L6bufferS351[_M0L6_2atmpS1931] = _M0L6_2atmpS1932;
      _M0L6_2atmpS1937 = _M0Lm6offsetS343;
      _M0L6_2atmpS1936 = _M0L12digit__startS345 + _M0L6_2atmpS1937;
      _M0L6_2atmpS1934 = _M0L6_2atmpS1936 + 1;
      _M0L6_2atmpS1935
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2loS350
      ];
      _M0L6bufferS351[_M0L6_2atmpS1934] = _M0L6_2atmpS1935;
      _M0L6_2atmpS1938 = _M0Lm1nS346;
      _M0Lm1nS346 = _M0L6_2atmpS1938 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1941 = _M0Lm6offsetS343;
  if (_M0L6_2atmpS1941 == 1) {
    uint64_t _M0L6_2atmpS1944 = _M0Lm1nS346;
    uint64_t _M0L6_2atmpS1943 = _M0L6_2atmpS1944 & 15ull;
    int32_t _M0L6nibbleS353 = (int32_t)_M0L6_2atmpS1943;
    int32_t _M0L6_2atmpS1942 =
      ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L6nibbleS353];
    _M0L6bufferS351[_M0L12digit__startS345] = _M0L6_2atmpS1942;
    moonbit_decref(_M0L6bufferS351);
  } else {
    moonbit_decref(_M0L6bufferS351);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS337,
  int32_t _M0L5radixS340
) {
  uint64_t _M0Lm3numS338;
  uint64_t _M0L4baseS339;
  int32_t _M0Lm5countS341;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS337 == 0ull) {
    return 1;
  }
  _M0Lm3numS338 = _M0L5valueS337;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS339 = _M0MPC13int3Int10to__uint64(_M0L5radixS340);
  _M0Lm5countS341 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1926 = _M0Lm3numS338;
    if (_M0L6_2atmpS1926 > 0ull) {
      int32_t _M0L6_2atmpS1927 = _M0Lm5countS341;
      uint64_t _M0L6_2atmpS1928;
      _M0Lm5countS341 = _M0L6_2atmpS1927 + 1;
      _M0L6_2atmpS1928 = _M0Lm3numS338;
      _M0Lm3numS338 = _M0L6_2atmpS1928 / _M0L4baseS339;
      continue;
    }
    break;
  }
  return _M0Lm5countS341;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS335) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS335 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS336;
    int32_t _M0L6_2atmpS1925;
    int32_t _M0L6_2atmpS1924;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS336 = moonbit_clz64(_M0L5valueS335);
    _M0L6_2atmpS1925 = 63 - _M0L14leading__zerosS336;
    _M0L6_2atmpS1924 = _M0L6_2atmpS1925 / 4;
    return _M0L6_2atmpS1924 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS334) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS334 >= 10000000000ull) {
    if (_M0L5valueS334 >= 100000000000000ull) {
      if (_M0L5valueS334 >= 10000000000000000ull) {
        if (_M0L5valueS334 >= 1000000000000000000ull) {
          if (_M0L5valueS334 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS334 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS334 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS334 >= 1000000000000ull) {
      if (_M0L5valueS334 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS334 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS334 >= 100000ull) {
    if (_M0L5valueS334 >= 10000000ull) {
      if (_M0L5valueS334 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS334 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS334 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS334 >= 1000ull) {
    if (_M0L5valueS334 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS334 >= 100ull) {
    return 3;
  } else if (_M0L5valueS334 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS318,
  int32_t _M0L5radixS317
) {
  int32_t _if__result_3924;
  int32_t _M0L12is__negativeS319;
  uint32_t _M0L3numS320;
  uint16_t* _M0L6bufferS321;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS317 < 2) {
    _if__result_3924 = 1;
  } else {
    _if__result_3924 = _M0L5radixS317 > 36;
  }
  if (_if__result_3924) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_87.data, (moonbit_string_t)moonbit_string_literal_91.data);
  }
  if (_M0L4selfS318 == 0) {
    return (moonbit_string_t)moonbit_string_literal_74.data;
  }
  _M0L12is__negativeS319 = _M0L4selfS318 < 0;
  if (_M0L12is__negativeS319) {
    int32_t _M0L6_2atmpS1923 = -_M0L4selfS318;
    _M0L3numS320 = *(uint32_t*)&_M0L6_2atmpS1923;
  } else {
    _M0L3numS320 = *(uint32_t*)&_M0L4selfS318;
  }
  switch (_M0L5radixS317) {
    case 10: {
      int32_t _M0L10digit__lenS322;
      int32_t _M0L6_2atmpS1920;
      int32_t _M0L10total__lenS323;
      uint16_t* _M0L6bufferS324;
      int32_t _M0L12digit__startS325;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS322 = _M0FPB12dec__count32(_M0L3numS320);
      if (_M0L12is__negativeS319) {
        _M0L6_2atmpS1920 = 1;
      } else {
        _M0L6_2atmpS1920 = 0;
      }
      _M0L10total__lenS323 = _M0L10digit__lenS322 + _M0L6_2atmpS1920;
      _M0L6bufferS324
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS323, 0);
      if (_M0L12is__negativeS319) {
        _M0L12digit__startS325 = 1;
      } else {
        _M0L12digit__startS325 = 0;
      }
      moonbit_incref(_M0L6bufferS324);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS324, _M0L3numS320, _M0L12digit__startS325, _M0L10total__lenS323);
      _M0L6bufferS321 = _M0L6bufferS324;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS326;
      int32_t _M0L6_2atmpS1921;
      int32_t _M0L10total__lenS327;
      uint16_t* _M0L6bufferS328;
      int32_t _M0L12digit__startS329;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS326 = _M0FPB12hex__count32(_M0L3numS320);
      if (_M0L12is__negativeS319) {
        _M0L6_2atmpS1921 = 1;
      } else {
        _M0L6_2atmpS1921 = 0;
      }
      _M0L10total__lenS327 = _M0L10digit__lenS326 + _M0L6_2atmpS1921;
      _M0L6bufferS328
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS327, 0);
      if (_M0L12is__negativeS319) {
        _M0L12digit__startS329 = 1;
      } else {
        _M0L12digit__startS329 = 0;
      }
      moonbit_incref(_M0L6bufferS328);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS328, _M0L3numS320, _M0L12digit__startS329, _M0L10total__lenS327);
      _M0L6bufferS321 = _M0L6bufferS328;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS330;
      int32_t _M0L6_2atmpS1922;
      int32_t _M0L10total__lenS331;
      uint16_t* _M0L6bufferS332;
      int32_t _M0L12digit__startS333;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS330
      = _M0FPB14radix__count32(_M0L3numS320, _M0L5radixS317);
      if (_M0L12is__negativeS319) {
        _M0L6_2atmpS1922 = 1;
      } else {
        _M0L6_2atmpS1922 = 0;
      }
      _M0L10total__lenS331 = _M0L10digit__lenS330 + _M0L6_2atmpS1922;
      _M0L6bufferS332
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS331, 0);
      if (_M0L12is__negativeS319) {
        _M0L12digit__startS333 = 1;
      } else {
        _M0L12digit__startS333 = 0;
      }
      moonbit_incref(_M0L6bufferS332);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS332, _M0L3numS320, _M0L12digit__startS333, _M0L10total__lenS331, _M0L5radixS317);
      _M0L6bufferS321 = _M0L6bufferS332;
      break;
    }
  }
  if (_M0L12is__negativeS319) {
    _M0L6bufferS321[0] = 45;
  }
  return _M0L6bufferS321;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS311,
  int32_t _M0L5radixS314
) {
  uint32_t _M0Lm3numS312;
  uint32_t _M0L4baseS313;
  int32_t _M0Lm5countS315;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS311 == 0u) {
    return 1;
  }
  _M0Lm3numS312 = _M0L5valueS311;
  _M0L4baseS313 = *(uint32_t*)&_M0L5radixS314;
  _M0Lm5countS315 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1917 = _M0Lm3numS312;
    if (_M0L6_2atmpS1917 > 0u) {
      int32_t _M0L6_2atmpS1918 = _M0Lm5countS315;
      uint32_t _M0L6_2atmpS1919;
      _M0Lm5countS315 = _M0L6_2atmpS1918 + 1;
      _M0L6_2atmpS1919 = _M0Lm3numS312;
      _M0Lm3numS312 = _M0L6_2atmpS1919 / _M0L4baseS313;
      continue;
    }
    break;
  }
  return _M0Lm5countS315;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS309) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS309 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS310;
    int32_t _M0L6_2atmpS1916;
    int32_t _M0L6_2atmpS1915;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS310 = moonbit_clz32(_M0L5valueS309);
    _M0L6_2atmpS1916 = 31 - _M0L14leading__zerosS310;
    _M0L6_2atmpS1915 = _M0L6_2atmpS1916 / 4;
    return _M0L6_2atmpS1915 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS308) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS308 >= 100000u) {
    if (_M0L5valueS308 >= 10000000u) {
      if (_M0L5valueS308 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS308 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS308 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS308 >= 1000u) {
    if (_M0L5valueS308 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS308 >= 100u) {
    return 3;
  } else if (_M0L5valueS308 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS298,
  uint32_t _M0L3numS286,
  int32_t _M0L12digit__startS289,
  int32_t _M0L10total__lenS288
) {
  uint32_t _M0Lm3numS285;
  int32_t _M0Lm6offsetS287;
  uint32_t _M0L6_2atmpS1914;
  int32_t _M0Lm9remainingS300;
  int32_t _M0L6_2atmpS1895;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS285 = _M0L3numS286;
  _M0Lm6offsetS287 = _M0L10total__lenS288 - _M0L12digit__startS289;
  while (1) {
    uint32_t _M0L6_2atmpS1858 = _M0Lm3numS285;
    if (_M0L6_2atmpS1858 >= 10000u) {
      uint32_t _M0L6_2atmpS1881 = _M0Lm3numS285;
      uint32_t _M0L1tS290 = _M0L6_2atmpS1881 / 10000u;
      uint32_t _M0L6_2atmpS1880 = _M0Lm3numS285;
      uint32_t _M0L6_2atmpS1879 = _M0L6_2atmpS1880 % 10000u;
      int32_t _M0L1rS291 = *(int32_t*)&_M0L6_2atmpS1879;
      int32_t _M0L2d1S292;
      int32_t _M0L2d2S293;
      int32_t _M0L6_2atmpS1859;
      int32_t _M0L6_2atmpS1878;
      int32_t _M0L6_2atmpS1877;
      int32_t _M0L6d1__hiS294;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6_2atmpS1875;
      int32_t _M0L6d1__loS295;
      int32_t _M0L6_2atmpS1874;
      int32_t _M0L6_2atmpS1873;
      int32_t _M0L6d2__hiS296;
      int32_t _M0L6_2atmpS1872;
      int32_t _M0L6_2atmpS1871;
      int32_t _M0L6d2__loS297;
      int32_t _M0L6_2atmpS1861;
      int32_t _M0L6_2atmpS1860;
      int32_t _M0L6_2atmpS1864;
      int32_t _M0L6_2atmpS1863;
      int32_t _M0L6_2atmpS1862;
      int32_t _M0L6_2atmpS1867;
      int32_t _M0L6_2atmpS1866;
      int32_t _M0L6_2atmpS1865;
      int32_t _M0L6_2atmpS1870;
      int32_t _M0L6_2atmpS1869;
      int32_t _M0L6_2atmpS1868;
      _M0Lm3numS285 = _M0L1tS290;
      _M0L2d1S292 = _M0L1rS291 / 100;
      _M0L2d2S293 = _M0L1rS291 % 100;
      _M0L6_2atmpS1859 = _M0Lm6offsetS287;
      _M0Lm6offsetS287 = _M0L6_2atmpS1859 - 4;
      _M0L6_2atmpS1878 = _M0L2d1S292 / 10;
      _M0L6_2atmpS1877 = 48 + _M0L6_2atmpS1878;
      _M0L6d1__hiS294 = (uint16_t)_M0L6_2atmpS1877;
      _M0L6_2atmpS1876 = _M0L2d1S292 % 10;
      _M0L6_2atmpS1875 = 48 + _M0L6_2atmpS1876;
      _M0L6d1__loS295 = (uint16_t)_M0L6_2atmpS1875;
      _M0L6_2atmpS1874 = _M0L2d2S293 / 10;
      _M0L6_2atmpS1873 = 48 + _M0L6_2atmpS1874;
      _M0L6d2__hiS296 = (uint16_t)_M0L6_2atmpS1873;
      _M0L6_2atmpS1872 = _M0L2d2S293 % 10;
      _M0L6_2atmpS1871 = 48 + _M0L6_2atmpS1872;
      _M0L6d2__loS297 = (uint16_t)_M0L6_2atmpS1871;
      _M0L6_2atmpS1861 = _M0Lm6offsetS287;
      _M0L6_2atmpS1860 = _M0L12digit__startS289 + _M0L6_2atmpS1861;
      _M0L6bufferS298[_M0L6_2atmpS1860] = _M0L6d1__hiS294;
      _M0L6_2atmpS1864 = _M0Lm6offsetS287;
      _M0L6_2atmpS1863 = _M0L12digit__startS289 + _M0L6_2atmpS1864;
      _M0L6_2atmpS1862 = _M0L6_2atmpS1863 + 1;
      _M0L6bufferS298[_M0L6_2atmpS1862] = _M0L6d1__loS295;
      _M0L6_2atmpS1867 = _M0Lm6offsetS287;
      _M0L6_2atmpS1866 = _M0L12digit__startS289 + _M0L6_2atmpS1867;
      _M0L6_2atmpS1865 = _M0L6_2atmpS1866 + 2;
      _M0L6bufferS298[_M0L6_2atmpS1865] = _M0L6d2__hiS296;
      _M0L6_2atmpS1870 = _M0Lm6offsetS287;
      _M0L6_2atmpS1869 = _M0L12digit__startS289 + _M0L6_2atmpS1870;
      _M0L6_2atmpS1868 = _M0L6_2atmpS1869 + 3;
      _M0L6bufferS298[_M0L6_2atmpS1868] = _M0L6d2__loS297;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1914 = _M0Lm3numS285;
  _M0Lm9remainingS300 = *(int32_t*)&_M0L6_2atmpS1914;
  while (1) {
    int32_t _M0L6_2atmpS1882 = _M0Lm9remainingS300;
    if (_M0L6_2atmpS1882 >= 100) {
      int32_t _M0L6_2atmpS1894 = _M0Lm9remainingS300;
      int32_t _M0L1tS301 = _M0L6_2atmpS1894 / 100;
      int32_t _M0L6_2atmpS1893 = _M0Lm9remainingS300;
      int32_t _M0L1dS302 = _M0L6_2atmpS1893 % 100;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6_2atmpS1892;
      int32_t _M0L6_2atmpS1891;
      int32_t _M0L5d__hiS303;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6_2atmpS1889;
      int32_t _M0L5d__loS304;
      int32_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1884;
      int32_t _M0L6_2atmpS1888;
      int32_t _M0L6_2atmpS1887;
      int32_t _M0L6_2atmpS1886;
      _M0Lm9remainingS300 = _M0L1tS301;
      _M0L6_2atmpS1883 = _M0Lm6offsetS287;
      _M0Lm6offsetS287 = _M0L6_2atmpS1883 - 2;
      _M0L6_2atmpS1892 = _M0L1dS302 / 10;
      _M0L6_2atmpS1891 = 48 + _M0L6_2atmpS1892;
      _M0L5d__hiS303 = (uint16_t)_M0L6_2atmpS1891;
      _M0L6_2atmpS1890 = _M0L1dS302 % 10;
      _M0L6_2atmpS1889 = 48 + _M0L6_2atmpS1890;
      _M0L5d__loS304 = (uint16_t)_M0L6_2atmpS1889;
      _M0L6_2atmpS1885 = _M0Lm6offsetS287;
      _M0L6_2atmpS1884 = _M0L12digit__startS289 + _M0L6_2atmpS1885;
      _M0L6bufferS298[_M0L6_2atmpS1884] = _M0L5d__hiS303;
      _M0L6_2atmpS1888 = _M0Lm6offsetS287;
      _M0L6_2atmpS1887 = _M0L12digit__startS289 + _M0L6_2atmpS1888;
      _M0L6_2atmpS1886 = _M0L6_2atmpS1887 + 1;
      _M0L6bufferS298[_M0L6_2atmpS1886] = _M0L5d__loS304;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1895 = _M0Lm9remainingS300;
  if (_M0L6_2atmpS1895 >= 10) {
    int32_t _M0L6_2atmpS1896 = _M0Lm6offsetS287;
    int32_t _M0L6_2atmpS1907;
    int32_t _M0L6_2atmpS1906;
    int32_t _M0L6_2atmpS1905;
    int32_t _M0L5d__hiS306;
    int32_t _M0L6_2atmpS1904;
    int32_t _M0L6_2atmpS1903;
    int32_t _M0L6_2atmpS1902;
    int32_t _M0L5d__loS307;
    int32_t _M0L6_2atmpS1898;
    int32_t _M0L6_2atmpS1897;
    int32_t _M0L6_2atmpS1901;
    int32_t _M0L6_2atmpS1900;
    int32_t _M0L6_2atmpS1899;
    _M0Lm6offsetS287 = _M0L6_2atmpS1896 - 2;
    _M0L6_2atmpS1907 = _M0Lm9remainingS300;
    _M0L6_2atmpS1906 = _M0L6_2atmpS1907 / 10;
    _M0L6_2atmpS1905 = 48 + _M0L6_2atmpS1906;
    _M0L5d__hiS306 = (uint16_t)_M0L6_2atmpS1905;
    _M0L6_2atmpS1904 = _M0Lm9remainingS300;
    _M0L6_2atmpS1903 = _M0L6_2atmpS1904 % 10;
    _M0L6_2atmpS1902 = 48 + _M0L6_2atmpS1903;
    _M0L5d__loS307 = (uint16_t)_M0L6_2atmpS1902;
    _M0L6_2atmpS1898 = _M0Lm6offsetS287;
    _M0L6_2atmpS1897 = _M0L12digit__startS289 + _M0L6_2atmpS1898;
    _M0L6bufferS298[_M0L6_2atmpS1897] = _M0L5d__hiS306;
    _M0L6_2atmpS1901 = _M0Lm6offsetS287;
    _M0L6_2atmpS1900 = _M0L12digit__startS289 + _M0L6_2atmpS1901;
    _M0L6_2atmpS1899 = _M0L6_2atmpS1900 + 1;
    _M0L6bufferS298[_M0L6_2atmpS1899] = _M0L5d__loS307;
    moonbit_decref(_M0L6bufferS298);
  } else {
    int32_t _M0L6_2atmpS1908 = _M0Lm6offsetS287;
    int32_t _M0L6_2atmpS1913;
    int32_t _M0L6_2atmpS1909;
    int32_t _M0L6_2atmpS1912;
    int32_t _M0L6_2atmpS1911;
    int32_t _M0L6_2atmpS1910;
    _M0Lm6offsetS287 = _M0L6_2atmpS1908 - 1;
    _M0L6_2atmpS1913 = _M0Lm6offsetS287;
    _M0L6_2atmpS1909 = _M0L12digit__startS289 + _M0L6_2atmpS1913;
    _M0L6_2atmpS1912 = _M0Lm9remainingS300;
    _M0L6_2atmpS1911 = 48 + _M0L6_2atmpS1912;
    _M0L6_2atmpS1910 = (uint16_t)_M0L6_2atmpS1911;
    _M0L6bufferS298[_M0L6_2atmpS1909] = _M0L6_2atmpS1910;
    moonbit_decref(_M0L6bufferS298);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS280,
  uint32_t _M0L3numS274,
  int32_t _M0L12digit__startS272,
  int32_t _M0L10total__lenS271,
  int32_t _M0L5radixS276
) {
  int32_t _M0Lm6offsetS270;
  uint32_t _M0Lm1nS273;
  uint32_t _M0L4baseS275;
  int32_t _M0L6_2atmpS1840;
  int32_t _M0L6_2atmpS1839;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS270 = _M0L10total__lenS271 - _M0L12digit__startS272;
  _M0Lm1nS273 = _M0L3numS274;
  _M0L4baseS275 = *(uint32_t*)&_M0L5radixS276;
  _M0L6_2atmpS1840 = _M0L5radixS276 - 1;
  _M0L6_2atmpS1839 = _M0L5radixS276 & _M0L6_2atmpS1840;
  if (_M0L6_2atmpS1839 == 0) {
    int32_t _M0L5shiftS277;
    uint32_t _M0L4maskS278;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS277 = moonbit_ctz32(_M0L5radixS276);
    _M0L4maskS278 = _M0L4baseS275 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1841 = _M0Lm1nS273;
      if (_M0L6_2atmpS1841 > 0u) {
        int32_t _M0L6_2atmpS1842 = _M0Lm6offsetS270;
        uint32_t _M0L6_2atmpS1848;
        uint32_t _M0L6_2atmpS1847;
        int32_t _M0L5digitS279;
        int32_t _M0L6_2atmpS1845;
        int32_t _M0L6_2atmpS1843;
        int32_t _M0L6_2atmpS1844;
        uint32_t _M0L6_2atmpS1846;
        _M0Lm6offsetS270 = _M0L6_2atmpS1842 - 1;
        _M0L6_2atmpS1848 = _M0Lm1nS273;
        _M0L6_2atmpS1847 = _M0L6_2atmpS1848 & _M0L4maskS278;
        _M0L5digitS279 = *(int32_t*)&_M0L6_2atmpS1847;
        _M0L6_2atmpS1845 = _M0Lm6offsetS270;
        _M0L6_2atmpS1843 = _M0L12digit__startS272 + _M0L6_2atmpS1845;
        _M0L6_2atmpS1844
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS279
        ];
        _M0L6bufferS280[_M0L6_2atmpS1843] = _M0L6_2atmpS1844;
        _M0L6_2atmpS1846 = _M0Lm1nS273;
        _M0Lm1nS273 = _M0L6_2atmpS1846 >> (_M0L5shiftS277 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS280);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1849 = _M0Lm1nS273;
      if (_M0L6_2atmpS1849 > 0u) {
        int32_t _M0L6_2atmpS1850 = _M0Lm6offsetS270;
        uint32_t _M0L6_2atmpS1857;
        uint32_t _M0L1qS282;
        uint32_t _M0L6_2atmpS1855;
        uint32_t _M0L6_2atmpS1856;
        uint32_t _M0L6_2atmpS1854;
        int32_t _M0L5digitS283;
        int32_t _M0L6_2atmpS1853;
        int32_t _M0L6_2atmpS1851;
        int32_t _M0L6_2atmpS1852;
        _M0Lm6offsetS270 = _M0L6_2atmpS1850 - 1;
        _M0L6_2atmpS1857 = _M0Lm1nS273;
        _M0L1qS282 = _M0L6_2atmpS1857 / _M0L4baseS275;
        _M0L6_2atmpS1855 = _M0Lm1nS273;
        _M0L6_2atmpS1856 = _M0L1qS282 * _M0L4baseS275;
        _M0L6_2atmpS1854 = _M0L6_2atmpS1855 - _M0L6_2atmpS1856;
        _M0L5digitS283 = *(int32_t*)&_M0L6_2atmpS1854;
        _M0L6_2atmpS1853 = _M0Lm6offsetS270;
        _M0L6_2atmpS1851 = _M0L12digit__startS272 + _M0L6_2atmpS1853;
        _M0L6_2atmpS1852
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS283
        ];
        _M0L6bufferS280[_M0L6_2atmpS1851] = _M0L6_2atmpS1852;
        _M0Lm1nS273 = _M0L1qS282;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS280);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS267,
  uint32_t _M0L3numS263,
  int32_t _M0L12digit__startS261,
  int32_t _M0L10total__lenS260
) {
  int32_t _M0Lm6offsetS259;
  uint32_t _M0Lm1nS262;
  int32_t _M0L6_2atmpS1835;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS259 = _M0L10total__lenS260 - _M0L12digit__startS261;
  _M0Lm1nS262 = _M0L3numS263;
  while (1) {
    int32_t _M0L6_2atmpS1823 = _M0Lm6offsetS259;
    if (_M0L6_2atmpS1823 >= 2) {
      int32_t _M0L6_2atmpS1824 = _M0Lm6offsetS259;
      uint32_t _M0L6_2atmpS1834;
      uint32_t _M0L6_2atmpS1833;
      int32_t _M0L9byte__valS264;
      int32_t _M0L2hiS265;
      int32_t _M0L2loS266;
      int32_t _M0L6_2atmpS1827;
      int32_t _M0L6_2atmpS1825;
      int32_t _M0L6_2atmpS1826;
      int32_t _M0L6_2atmpS1831;
      int32_t _M0L6_2atmpS1830;
      int32_t _M0L6_2atmpS1828;
      int32_t _M0L6_2atmpS1829;
      uint32_t _M0L6_2atmpS1832;
      _M0Lm6offsetS259 = _M0L6_2atmpS1824 - 2;
      _M0L6_2atmpS1834 = _M0Lm1nS262;
      _M0L6_2atmpS1833 = _M0L6_2atmpS1834 & 255u;
      _M0L9byte__valS264 = *(int32_t*)&_M0L6_2atmpS1833;
      _M0L2hiS265 = _M0L9byte__valS264 / 16;
      _M0L2loS266 = _M0L9byte__valS264 % 16;
      _M0L6_2atmpS1827 = _M0Lm6offsetS259;
      _M0L6_2atmpS1825 = _M0L12digit__startS261 + _M0L6_2atmpS1827;
      _M0L6_2atmpS1826
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2hiS265
      ];
      _M0L6bufferS267[_M0L6_2atmpS1825] = _M0L6_2atmpS1826;
      _M0L6_2atmpS1831 = _M0Lm6offsetS259;
      _M0L6_2atmpS1830 = _M0L12digit__startS261 + _M0L6_2atmpS1831;
      _M0L6_2atmpS1828 = _M0L6_2atmpS1830 + 1;
      _M0L6_2atmpS1829
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2loS266
      ];
      _M0L6bufferS267[_M0L6_2atmpS1828] = _M0L6_2atmpS1829;
      _M0L6_2atmpS1832 = _M0Lm1nS262;
      _M0Lm1nS262 = _M0L6_2atmpS1832 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1835 = _M0Lm6offsetS259;
  if (_M0L6_2atmpS1835 == 1) {
    uint32_t _M0L6_2atmpS1838 = _M0Lm1nS262;
    uint32_t _M0L6_2atmpS1837 = _M0L6_2atmpS1838 & 15u;
    int32_t _M0L6nibbleS269 = *(int32_t*)&_M0L6_2atmpS1837;
    int32_t _M0L6_2atmpS1836 =
      ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L6nibbleS269];
    _M0L6bufferS267[_M0L12digit__startS261] = _M0L6_2atmpS1836;
    moonbit_decref(_M0L6bufferS267);
  } else {
    moonbit_decref(_M0L6bufferS267);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS254) {
  struct _M0TWEOs* _M0L7_2afuncS253;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS253 = _M0L4selfS254;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS253->code(_M0L7_2afuncS253);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS256
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS255;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS255 = _M0L4selfS256;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS255->code(_M0L7_2afuncS255);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS258) {
  struct _M0TWEOc* _M0L7_2afuncS257;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS257 = _M0L4selfS258;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS257->code(_M0L7_2afuncS257);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS246
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS245;
  struct _M0TPB6Logger _M0L6_2atmpS1819;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS245 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS245);
  _M0L6_2atmpS1819
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS245
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS246, _M0L6_2atmpS1819);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS245);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS247;
  struct _M0TPB6Logger _M0L6_2atmpS1820;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS247);
  _M0L6_2atmpS1820
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS247
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS248, _M0L6_2atmpS1820);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS247);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS250
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS249;
  struct _M0TPB6Logger _M0L6_2atmpS1821;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS249 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS249);
  _M0L6_2atmpS1821
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS249
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS250, _M0L6_2atmpS1821);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS249);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS252
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS251;
  struct _M0TPB6Logger _M0L6_2atmpS1822;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS251 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS251);
  _M0L6_2atmpS1822
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS251
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS252, _M0L6_2atmpS1822);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS251);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS244
) {
  int32_t _M0L8_2afieldS3558;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3558 = _M0L4selfS244.$1;
  moonbit_decref(_M0L4selfS244.$0);
  return _M0L8_2afieldS3558;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS243
) {
  int32_t _M0L3endS1817;
  int32_t _M0L8_2afieldS3559;
  int32_t _M0L5startS1818;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1817 = _M0L4selfS243.$2;
  _M0L8_2afieldS3559 = _M0L4selfS243.$1;
  moonbit_decref(_M0L4selfS243.$0);
  _M0L5startS1818 = _M0L8_2afieldS3559;
  return _M0L3endS1817 - _M0L5startS1818;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS242
) {
  moonbit_string_t _M0L8_2afieldS3560;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3560 = _M0L4selfS242.$0;
  return _M0L8_2afieldS3560;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS238,
  moonbit_string_t _M0L5valueS239,
  int32_t _M0L5startS240,
  int32_t _M0L3lenS241
) {
  int32_t _M0L6_2atmpS1816;
  int64_t _M0L6_2atmpS1815;
  struct _M0TPC16string10StringView _M0L6_2atmpS1814;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1816 = _M0L5startS240 + _M0L3lenS241;
  _M0L6_2atmpS1815 = (int64_t)_M0L6_2atmpS1816;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1814
  = _M0MPC16string6String11sub_2einner(_M0L5valueS239, _M0L5startS240, _M0L6_2atmpS1815);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS238, _M0L6_2atmpS1814);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS231,
  int32_t _M0L5startS237,
  int64_t _M0L3endS233
) {
  int32_t _M0L3lenS230;
  int32_t _M0L3endS232;
  int32_t _M0L5startS236;
  int32_t _if__result_3931;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS230 = Moonbit_array_length(_M0L4selfS231);
  if (_M0L3endS233 == 4294967296ll) {
    _M0L3endS232 = _M0L3lenS230;
  } else {
    int64_t _M0L7_2aSomeS234 = _M0L3endS233;
    int32_t _M0L6_2aendS235 = (int32_t)_M0L7_2aSomeS234;
    if (_M0L6_2aendS235 < 0) {
      _M0L3endS232 = _M0L3lenS230 + _M0L6_2aendS235;
    } else {
      _M0L3endS232 = _M0L6_2aendS235;
    }
  }
  if (_M0L5startS237 < 0) {
    _M0L5startS236 = _M0L3lenS230 + _M0L5startS237;
  } else {
    _M0L5startS236 = _M0L5startS237;
  }
  if (_M0L5startS236 >= 0) {
    if (_M0L5startS236 <= _M0L3endS232) {
      _if__result_3931 = _M0L3endS232 <= _M0L3lenS230;
    } else {
      _if__result_3931 = 0;
    }
  } else {
    _if__result_3931 = 0;
  }
  if (_if__result_3931) {
    if (_M0L5startS236 < _M0L3lenS230) {
      int32_t _M0L6_2atmpS1811 = _M0L4selfS231[_M0L5startS236];
      int32_t _M0L6_2atmpS1810;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1810
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1811);
      if (!_M0L6_2atmpS1810) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS232 < _M0L3lenS230) {
      int32_t _M0L6_2atmpS1813 = _M0L4selfS231[_M0L3endS232];
      int32_t _M0L6_2atmpS1812;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1812
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1813);
      if (!_M0L6_2atmpS1812) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS236,
                                                 _M0L3endS232,
                                                 _M0L4selfS231};
  } else {
    moonbit_decref(_M0L4selfS231);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS227) {
  struct _M0TPB6Hasher* _M0L1hS226;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS226 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS226);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS226, _M0L4selfS227);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS226);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS229
) {
  struct _M0TPB6Hasher* _M0L1hS228;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS228 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS228);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS228, _M0L4selfS229);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS228);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS224) {
  int32_t _M0L4seedS223;
  if (_M0L10seed_2eoptS224 == 4294967296ll) {
    _M0L4seedS223 = 0;
  } else {
    int64_t _M0L7_2aSomeS225 = _M0L10seed_2eoptS224;
    _M0L4seedS223 = (int32_t)_M0L7_2aSomeS225;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS223);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS222) {
  uint32_t _M0L6_2atmpS1809;
  uint32_t _M0L6_2atmpS1808;
  struct _M0TPB6Hasher* _block_3932;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1809 = *(uint32_t*)&_M0L4seedS222;
  _M0L6_2atmpS1808 = _M0L6_2atmpS1809 + 374761393u;
  _block_3932
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3932)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3932->$0 = _M0L6_2atmpS1808;
  return _block_3932;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS221) {
  uint32_t _M0L6_2atmpS1807;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1807 = _M0MPB6Hasher9avalanche(_M0L4selfS221);
  return *(int32_t*)&_M0L6_2atmpS1807;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS220) {
  uint32_t _M0L8_2afieldS3561;
  uint32_t _M0Lm3accS219;
  uint32_t _M0L6_2atmpS1796;
  uint32_t _M0L6_2atmpS1798;
  uint32_t _M0L6_2atmpS1797;
  uint32_t _M0L6_2atmpS1799;
  uint32_t _M0L6_2atmpS1800;
  uint32_t _M0L6_2atmpS1802;
  uint32_t _M0L6_2atmpS1801;
  uint32_t _M0L6_2atmpS1803;
  uint32_t _M0L6_2atmpS1804;
  uint32_t _M0L6_2atmpS1806;
  uint32_t _M0L6_2atmpS1805;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3561 = _M0L4selfS220->$0;
  moonbit_decref(_M0L4selfS220);
  _M0Lm3accS219 = _M0L8_2afieldS3561;
  _M0L6_2atmpS1796 = _M0Lm3accS219;
  _M0L6_2atmpS1798 = _M0Lm3accS219;
  _M0L6_2atmpS1797 = _M0L6_2atmpS1798 >> 15;
  _M0Lm3accS219 = _M0L6_2atmpS1796 ^ _M0L6_2atmpS1797;
  _M0L6_2atmpS1799 = _M0Lm3accS219;
  _M0Lm3accS219 = _M0L6_2atmpS1799 * 2246822519u;
  _M0L6_2atmpS1800 = _M0Lm3accS219;
  _M0L6_2atmpS1802 = _M0Lm3accS219;
  _M0L6_2atmpS1801 = _M0L6_2atmpS1802 >> 13;
  _M0Lm3accS219 = _M0L6_2atmpS1800 ^ _M0L6_2atmpS1801;
  _M0L6_2atmpS1803 = _M0Lm3accS219;
  _M0Lm3accS219 = _M0L6_2atmpS1803 * 3266489917u;
  _M0L6_2atmpS1804 = _M0Lm3accS219;
  _M0L6_2atmpS1806 = _M0Lm3accS219;
  _M0L6_2atmpS1805 = _M0L6_2atmpS1806 >> 16;
  _M0Lm3accS219 = _M0L6_2atmpS1804 ^ _M0L6_2atmpS1805;
  return _M0Lm3accS219;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS217,
  moonbit_string_t _M0L1yS218
) {
  int32_t _M0L6_2atmpS3562;
  int32_t _M0L6_2atmpS1795;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3562 = moonbit_val_array_equal(_M0L1xS217, _M0L1yS218);
  moonbit_decref(_M0L1xS217);
  moonbit_decref(_M0L1yS218);
  _M0L6_2atmpS1795 = _M0L6_2atmpS3562;
  return !_M0L6_2atmpS1795;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS214,
  int32_t _M0L5valueS213
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS213, _M0L4selfS214);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS216,
  moonbit_string_t _M0L5valueS215
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS215, _M0L4selfS216);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS212) {
  int64_t _M0L6_2atmpS1794;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1794 = (int64_t)_M0L4selfS212;
  return *(uint64_t*)&_M0L6_2atmpS1794;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS210,
  moonbit_string_t _M0L4reprS211
) {
  void* _block_3933;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3933 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_3933)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_3933)->$0 = _M0L6numberS210;
  ((struct _M0DTPB4Json6Number*)_block_3933)->$1 = _M0L4reprS211;
  return _block_3933;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS208,
  int32_t _M0L5valueS209
) {
  uint32_t _M0L6_2atmpS1793;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1793 = *(uint32_t*)&_M0L5valueS209;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS208, _M0L6_2atmpS1793);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS201
) {
  struct _M0TPB13StringBuilder* _M0L3bufS199;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS200;
  int32_t _M0L7_2abindS202;
  int32_t _M0L1iS203;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS199 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS200 = _M0L4selfS201;
  moonbit_incref(_M0L3bufS199);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS199, 91);
  _M0L7_2abindS202 = _M0L7_2aselfS200->$1;
  _M0L1iS203 = 0;
  while (1) {
    if (_M0L1iS203 < _M0L7_2abindS202) {
      int32_t _if__result_3935;
      moonbit_string_t* _M0L8_2afieldS3564;
      moonbit_string_t* _M0L3bufS1791;
      moonbit_string_t _M0L6_2atmpS3563;
      moonbit_string_t _M0L4itemS204;
      int32_t _M0L6_2atmpS1792;
      if (_M0L1iS203 != 0) {
        moonbit_incref(_M0L3bufS199);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS199, (moonbit_string_t)moonbit_string_literal_92.data);
      }
      if (_M0L1iS203 < 0) {
        _if__result_3935 = 1;
      } else {
        int32_t _M0L3lenS1790 = _M0L7_2aselfS200->$1;
        _if__result_3935 = _M0L1iS203 >= _M0L3lenS1790;
      }
      if (_if__result_3935) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3564 = _M0L7_2aselfS200->$0;
      _M0L3bufS1791 = _M0L8_2afieldS3564;
      _M0L6_2atmpS3563 = (moonbit_string_t)_M0L3bufS1791[_M0L1iS203];
      _M0L4itemS204 = _M0L6_2atmpS3563;
      if (_M0L4itemS204 == 0) {
        moonbit_incref(_M0L3bufS199);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS199, (moonbit_string_t)moonbit_string_literal_52.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS205 = _M0L4itemS204;
        moonbit_string_t _M0L6_2alocS206 = _M0L7_2aSomeS205;
        moonbit_string_t _M0L6_2atmpS1789;
        moonbit_incref(_M0L6_2alocS206);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1789
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS206);
        moonbit_incref(_M0L3bufS199);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS199, _M0L6_2atmpS1789);
      }
      _M0L6_2atmpS1792 = _M0L1iS203 + 1;
      _M0L1iS203 = _M0L6_2atmpS1792;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS200);
    }
    break;
  }
  moonbit_incref(_M0L3bufS199);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS199, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS199);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS198
) {
  moonbit_string_t _M0L6_2atmpS1788;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1787;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1788 = _M0L4selfS198;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1787 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1788);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1787);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS197
) {
  struct _M0TPB13StringBuilder* _M0L2sbS196;
  struct _M0TPC16string10StringView _M0L8_2afieldS3577;
  struct _M0TPC16string10StringView _M0L3pkgS1772;
  moonbit_string_t _M0L6_2atmpS1771;
  moonbit_string_t _M0L6_2atmpS3576;
  moonbit_string_t _M0L6_2atmpS1770;
  moonbit_string_t _M0L6_2atmpS3575;
  moonbit_string_t _M0L6_2atmpS1769;
  struct _M0TPC16string10StringView _M0L8_2afieldS3574;
  struct _M0TPC16string10StringView _M0L8filenameS1773;
  struct _M0TPC16string10StringView _M0L8_2afieldS3573;
  struct _M0TPC16string10StringView _M0L11start__lineS1776;
  moonbit_string_t _M0L6_2atmpS1775;
  moonbit_string_t _M0L6_2atmpS3572;
  moonbit_string_t _M0L6_2atmpS1774;
  struct _M0TPC16string10StringView _M0L8_2afieldS3571;
  struct _M0TPC16string10StringView _M0L13start__columnS1779;
  moonbit_string_t _M0L6_2atmpS1778;
  moonbit_string_t _M0L6_2atmpS3570;
  moonbit_string_t _M0L6_2atmpS1777;
  struct _M0TPC16string10StringView _M0L8_2afieldS3569;
  struct _M0TPC16string10StringView _M0L9end__lineS1782;
  moonbit_string_t _M0L6_2atmpS1781;
  moonbit_string_t _M0L6_2atmpS3568;
  moonbit_string_t _M0L6_2atmpS1780;
  struct _M0TPC16string10StringView _M0L8_2afieldS3567;
  int32_t _M0L6_2acntS3741;
  struct _M0TPC16string10StringView _M0L11end__columnS1786;
  moonbit_string_t _M0L6_2atmpS1785;
  moonbit_string_t _M0L6_2atmpS3566;
  moonbit_string_t _M0L6_2atmpS1784;
  moonbit_string_t _M0L6_2atmpS3565;
  moonbit_string_t _M0L6_2atmpS1783;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS196 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3577
  = (struct _M0TPC16string10StringView){
    _M0L4selfS197->$0_1, _M0L4selfS197->$0_2, _M0L4selfS197->$0_0
  };
  _M0L3pkgS1772 = _M0L8_2afieldS3577;
  moonbit_incref(_M0L3pkgS1772.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1771
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1772);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3576
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_93.data, _M0L6_2atmpS1771);
  moonbit_decref(_M0L6_2atmpS1771);
  _M0L6_2atmpS1770 = _M0L6_2atmpS3576;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3575
  = moonbit_add_string(_M0L6_2atmpS1770, (moonbit_string_t)moonbit_string_literal_94.data);
  moonbit_decref(_M0L6_2atmpS1770);
  _M0L6_2atmpS1769 = _M0L6_2atmpS3575;
  moonbit_incref(_M0L2sbS196);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS196, _M0L6_2atmpS1769);
  moonbit_incref(_M0L2sbS196);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS196, (moonbit_string_t)moonbit_string_literal_95.data);
  _M0L8_2afieldS3574
  = (struct _M0TPC16string10StringView){
    _M0L4selfS197->$1_1, _M0L4selfS197->$1_2, _M0L4selfS197->$1_0
  };
  _M0L8filenameS1773 = _M0L8_2afieldS3574;
  moonbit_incref(_M0L8filenameS1773.$0);
  moonbit_incref(_M0L2sbS196);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS196, _M0L8filenameS1773);
  _M0L8_2afieldS3573
  = (struct _M0TPC16string10StringView){
    _M0L4selfS197->$2_1, _M0L4selfS197->$2_2, _M0L4selfS197->$2_0
  };
  _M0L11start__lineS1776 = _M0L8_2afieldS3573;
  moonbit_incref(_M0L11start__lineS1776.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1775
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1776);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3572
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_96.data, _M0L6_2atmpS1775);
  moonbit_decref(_M0L6_2atmpS1775);
  _M0L6_2atmpS1774 = _M0L6_2atmpS3572;
  moonbit_incref(_M0L2sbS196);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS196, _M0L6_2atmpS1774);
  _M0L8_2afieldS3571
  = (struct _M0TPC16string10StringView){
    _M0L4selfS197->$3_1, _M0L4selfS197->$3_2, _M0L4selfS197->$3_0
  };
  _M0L13start__columnS1779 = _M0L8_2afieldS3571;
  moonbit_incref(_M0L13start__columnS1779.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1778
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1779);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3570
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_97.data, _M0L6_2atmpS1778);
  moonbit_decref(_M0L6_2atmpS1778);
  _M0L6_2atmpS1777 = _M0L6_2atmpS3570;
  moonbit_incref(_M0L2sbS196);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS196, _M0L6_2atmpS1777);
  _M0L8_2afieldS3569
  = (struct _M0TPC16string10StringView){
    _M0L4selfS197->$4_1, _M0L4selfS197->$4_2, _M0L4selfS197->$4_0
  };
  _M0L9end__lineS1782 = _M0L8_2afieldS3569;
  moonbit_incref(_M0L9end__lineS1782.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1781
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1782);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3568
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_98.data, _M0L6_2atmpS1781);
  moonbit_decref(_M0L6_2atmpS1781);
  _M0L6_2atmpS1780 = _M0L6_2atmpS3568;
  moonbit_incref(_M0L2sbS196);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS196, _M0L6_2atmpS1780);
  _M0L8_2afieldS3567
  = (struct _M0TPC16string10StringView){
    _M0L4selfS197->$5_1, _M0L4selfS197->$5_2, _M0L4selfS197->$5_0
  };
  _M0L6_2acntS3741 = Moonbit_object_header(_M0L4selfS197)->rc;
  if (_M0L6_2acntS3741 > 1) {
    int32_t _M0L11_2anew__cntS3747 = _M0L6_2acntS3741 - 1;
    Moonbit_object_header(_M0L4selfS197)->rc = _M0L11_2anew__cntS3747;
    moonbit_incref(_M0L8_2afieldS3567.$0);
  } else if (_M0L6_2acntS3741 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3746 =
      (struct _M0TPC16string10StringView){_M0L4selfS197->$4_1,
                                            _M0L4selfS197->$4_2,
                                            _M0L4selfS197->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3745;
    struct _M0TPC16string10StringView _M0L8_2afieldS3744;
    struct _M0TPC16string10StringView _M0L8_2afieldS3743;
    struct _M0TPC16string10StringView _M0L8_2afieldS3742;
    moonbit_decref(_M0L8_2afieldS3746.$0);
    _M0L8_2afieldS3745
    = (struct _M0TPC16string10StringView){
      _M0L4selfS197->$3_1, _M0L4selfS197->$3_2, _M0L4selfS197->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3745.$0);
    _M0L8_2afieldS3744
    = (struct _M0TPC16string10StringView){
      _M0L4selfS197->$2_1, _M0L4selfS197->$2_2, _M0L4selfS197->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3744.$0);
    _M0L8_2afieldS3743
    = (struct _M0TPC16string10StringView){
      _M0L4selfS197->$1_1, _M0L4selfS197->$1_2, _M0L4selfS197->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3743.$0);
    _M0L8_2afieldS3742
    = (struct _M0TPC16string10StringView){
      _M0L4selfS197->$0_1, _M0L4selfS197->$0_2, _M0L4selfS197->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3742.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS197);
  }
  _M0L11end__columnS1786 = _M0L8_2afieldS3567;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1785
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1786);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3566
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_99.data, _M0L6_2atmpS1785);
  moonbit_decref(_M0L6_2atmpS1785);
  _M0L6_2atmpS1784 = _M0L6_2atmpS3566;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3565
  = moonbit_add_string(_M0L6_2atmpS1784, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1784);
  _M0L6_2atmpS1783 = _M0L6_2atmpS3565;
  moonbit_incref(_M0L2sbS196);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS196, _M0L6_2atmpS1783);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS196);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS194,
  moonbit_string_t _M0L3strS195
) {
  int32_t _M0L3lenS1759;
  int32_t _M0L6_2atmpS1761;
  int32_t _M0L6_2atmpS1760;
  int32_t _M0L6_2atmpS1758;
  moonbit_bytes_t _M0L8_2afieldS3579;
  moonbit_bytes_t _M0L4dataS1762;
  int32_t _M0L3lenS1763;
  int32_t _M0L6_2atmpS1764;
  int32_t _M0L3lenS1766;
  int32_t _M0L6_2atmpS3578;
  int32_t _M0L6_2atmpS1768;
  int32_t _M0L6_2atmpS1767;
  int32_t _M0L6_2atmpS1765;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1759 = _M0L4selfS194->$1;
  _M0L6_2atmpS1761 = Moonbit_array_length(_M0L3strS195);
  _M0L6_2atmpS1760 = _M0L6_2atmpS1761 * 2;
  _M0L6_2atmpS1758 = _M0L3lenS1759 + _M0L6_2atmpS1760;
  moonbit_incref(_M0L4selfS194);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS194, _M0L6_2atmpS1758);
  _M0L8_2afieldS3579 = _M0L4selfS194->$0;
  _M0L4dataS1762 = _M0L8_2afieldS3579;
  _M0L3lenS1763 = _M0L4selfS194->$1;
  _M0L6_2atmpS1764 = Moonbit_array_length(_M0L3strS195);
  moonbit_incref(_M0L4dataS1762);
  moonbit_incref(_M0L3strS195);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1762, _M0L3lenS1763, _M0L3strS195, 0, _M0L6_2atmpS1764);
  _M0L3lenS1766 = _M0L4selfS194->$1;
  _M0L6_2atmpS3578 = Moonbit_array_length(_M0L3strS195);
  moonbit_decref(_M0L3strS195);
  _M0L6_2atmpS1768 = _M0L6_2atmpS3578;
  _M0L6_2atmpS1767 = _M0L6_2atmpS1768 * 2;
  _M0L6_2atmpS1765 = _M0L3lenS1766 + _M0L6_2atmpS1767;
  _M0L4selfS194->$1 = _M0L6_2atmpS1765;
  moonbit_decref(_M0L4selfS194);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS186,
  int32_t _M0L13bytes__offsetS181,
  moonbit_string_t _M0L3strS188,
  int32_t _M0L11str__offsetS184,
  int32_t _M0L6lengthS182
) {
  int32_t _M0L6_2atmpS1757;
  int32_t _M0L6_2atmpS1756;
  int32_t _M0L2e1S180;
  int32_t _M0L6_2atmpS1755;
  int32_t _M0L2e2S183;
  int32_t _M0L4len1S185;
  int32_t _M0L4len2S187;
  int32_t _if__result_3936;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1757 = _M0L6lengthS182 * 2;
  _M0L6_2atmpS1756 = _M0L13bytes__offsetS181 + _M0L6_2atmpS1757;
  _M0L2e1S180 = _M0L6_2atmpS1756 - 1;
  _M0L6_2atmpS1755 = _M0L11str__offsetS184 + _M0L6lengthS182;
  _M0L2e2S183 = _M0L6_2atmpS1755 - 1;
  _M0L4len1S185 = Moonbit_array_length(_M0L4selfS186);
  _M0L4len2S187 = Moonbit_array_length(_M0L3strS188);
  if (_M0L6lengthS182 >= 0) {
    if (_M0L13bytes__offsetS181 >= 0) {
      if (_M0L2e1S180 < _M0L4len1S185) {
        if (_M0L11str__offsetS184 >= 0) {
          _if__result_3936 = _M0L2e2S183 < _M0L4len2S187;
        } else {
          _if__result_3936 = 0;
        }
      } else {
        _if__result_3936 = 0;
      }
    } else {
      _if__result_3936 = 0;
    }
  } else {
    _if__result_3936 = 0;
  }
  if (_if__result_3936) {
    int32_t _M0L16end__str__offsetS189 =
      _M0L11str__offsetS184 + _M0L6lengthS182;
    int32_t _M0L1iS190 = _M0L11str__offsetS184;
    int32_t _M0L1jS191 = _M0L13bytes__offsetS181;
    while (1) {
      if (_M0L1iS190 < _M0L16end__str__offsetS189) {
        int32_t _M0L6_2atmpS1752 = _M0L3strS188[_M0L1iS190];
        int32_t _M0L6_2atmpS1751 = (int32_t)_M0L6_2atmpS1752;
        uint32_t _M0L1cS192 = *(uint32_t*)&_M0L6_2atmpS1751;
        uint32_t _M0L6_2atmpS1747 = _M0L1cS192 & 255u;
        int32_t _M0L6_2atmpS1746;
        int32_t _M0L6_2atmpS1748;
        uint32_t _M0L6_2atmpS1750;
        int32_t _M0L6_2atmpS1749;
        int32_t _M0L6_2atmpS1753;
        int32_t _M0L6_2atmpS1754;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1746 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1747);
        if (
          _M0L1jS191 < 0 || _M0L1jS191 >= Moonbit_array_length(_M0L4selfS186)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS186[_M0L1jS191] = _M0L6_2atmpS1746;
        _M0L6_2atmpS1748 = _M0L1jS191 + 1;
        _M0L6_2atmpS1750 = _M0L1cS192 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1749 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1750);
        if (
          _M0L6_2atmpS1748 < 0
          || _M0L6_2atmpS1748 >= Moonbit_array_length(_M0L4selfS186)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS186[_M0L6_2atmpS1748] = _M0L6_2atmpS1749;
        _M0L6_2atmpS1753 = _M0L1iS190 + 1;
        _M0L6_2atmpS1754 = _M0L1jS191 + 2;
        _M0L1iS190 = _M0L6_2atmpS1753;
        _M0L1jS191 = _M0L6_2atmpS1754;
        continue;
      } else {
        moonbit_decref(_M0L3strS188);
        moonbit_decref(_M0L4selfS186);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS188);
    moonbit_decref(_M0L4selfS186);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS177,
  double _M0L3objS176
) {
  struct _M0TPB6Logger _M0L6_2atmpS1744;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1744
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS177
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS176, _M0L6_2atmpS1744);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS179,
  struct _M0TPC16string10StringView _M0L3objS178
) {
  struct _M0TPB6Logger _M0L6_2atmpS1745;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1745
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS179
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS178, _M0L6_2atmpS1745);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS122
) {
  int32_t _M0L6_2atmpS1743;
  struct _M0TPC16string10StringView _M0L7_2abindS121;
  moonbit_string_t _M0L7_2adataS123;
  int32_t _M0L8_2astartS124;
  int32_t _M0L6_2atmpS1742;
  int32_t _M0L6_2aendS125;
  int32_t _M0Lm9_2acursorS126;
  int32_t _M0Lm13accept__stateS127;
  int32_t _M0Lm10match__endS128;
  int32_t _M0Lm20match__tag__saver__0S129;
  int32_t _M0Lm20match__tag__saver__1S130;
  int32_t _M0Lm20match__tag__saver__2S131;
  int32_t _M0Lm20match__tag__saver__3S132;
  int32_t _M0Lm20match__tag__saver__4S133;
  int32_t _M0Lm6tag__0S134;
  int32_t _M0Lm6tag__1S135;
  int32_t _M0Lm9tag__1__1S136;
  int32_t _M0Lm9tag__1__2S137;
  int32_t _M0Lm6tag__3S138;
  int32_t _M0Lm6tag__2S139;
  int32_t _M0Lm9tag__2__1S140;
  int32_t _M0Lm6tag__4S141;
  int32_t _M0L6_2atmpS1700;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1743 = Moonbit_array_length(_M0L4reprS122);
  _M0L7_2abindS121
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1743, _M0L4reprS122
  };
  moonbit_incref(_M0L7_2abindS121.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS123 = _M0MPC16string10StringView4data(_M0L7_2abindS121);
  moonbit_incref(_M0L7_2abindS121.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS124
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS121);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1742 = _M0MPC16string10StringView6length(_M0L7_2abindS121);
  _M0L6_2aendS125 = _M0L8_2astartS124 + _M0L6_2atmpS1742;
  _M0Lm9_2acursorS126 = _M0L8_2astartS124;
  _M0Lm13accept__stateS127 = -1;
  _M0Lm10match__endS128 = -1;
  _M0Lm20match__tag__saver__0S129 = -1;
  _M0Lm20match__tag__saver__1S130 = -1;
  _M0Lm20match__tag__saver__2S131 = -1;
  _M0Lm20match__tag__saver__3S132 = -1;
  _M0Lm20match__tag__saver__4S133 = -1;
  _M0Lm6tag__0S134 = -1;
  _M0Lm6tag__1S135 = -1;
  _M0Lm9tag__1__1S136 = -1;
  _M0Lm9tag__1__2S137 = -1;
  _M0Lm6tag__3S138 = -1;
  _M0Lm6tag__2S139 = -1;
  _M0Lm9tag__2__1S140 = -1;
  _M0Lm6tag__4S141 = -1;
  _M0L6_2atmpS1700 = _M0Lm9_2acursorS126;
  if (_M0L6_2atmpS1700 < _M0L6_2aendS125) {
    int32_t _M0L6_2atmpS1702 = _M0Lm9_2acursorS126;
    int32_t _M0L6_2atmpS1701;
    moonbit_incref(_M0L7_2adataS123);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1701
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1702);
    if (_M0L6_2atmpS1701 == 64) {
      int32_t _M0L6_2atmpS1703 = _M0Lm9_2acursorS126;
      _M0Lm9_2acursorS126 = _M0L6_2atmpS1703 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1704;
        _M0Lm6tag__0S134 = _M0Lm9_2acursorS126;
        _M0L6_2atmpS1704 = _M0Lm9_2acursorS126;
        if (_M0L6_2atmpS1704 < _M0L6_2aendS125) {
          int32_t _M0L6_2atmpS1741 = _M0Lm9_2acursorS126;
          int32_t _M0L10next__charS149;
          int32_t _M0L6_2atmpS1705;
          moonbit_incref(_M0L7_2adataS123);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS149
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1741);
          _M0L6_2atmpS1705 = _M0Lm9_2acursorS126;
          _M0Lm9_2acursorS126 = _M0L6_2atmpS1705 + 1;
          if (_M0L10next__charS149 == 58) {
            int32_t _M0L6_2atmpS1706 = _M0Lm9_2acursorS126;
            if (_M0L6_2atmpS1706 < _M0L6_2aendS125) {
              int32_t _M0L6_2atmpS1707 = _M0Lm9_2acursorS126;
              int32_t _M0L12dispatch__15S150;
              _M0Lm9_2acursorS126 = _M0L6_2atmpS1707 + 1;
              _M0L12dispatch__15S150 = 0;
              loop__label__15_153:;
              while (1) {
                int32_t _M0L6_2atmpS1708;
                switch (_M0L12dispatch__15S150) {
                  case 3: {
                    int32_t _M0L6_2atmpS1711;
                    _M0Lm9tag__1__2S137 = _M0Lm9tag__1__1S136;
                    _M0Lm9tag__1__1S136 = _M0Lm6tag__1S135;
                    _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                    _M0L6_2atmpS1711 = _M0Lm9_2acursorS126;
                    if (_M0L6_2atmpS1711 < _M0L6_2aendS125) {
                      int32_t _M0L6_2atmpS1716 = _M0Lm9_2acursorS126;
                      int32_t _M0L10next__charS157;
                      int32_t _M0L6_2atmpS1712;
                      moonbit_incref(_M0L7_2adataS123);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS157
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1716);
                      _M0L6_2atmpS1712 = _M0Lm9_2acursorS126;
                      _M0Lm9_2acursorS126 = _M0L6_2atmpS1712 + 1;
                      if (_M0L10next__charS157 < 58) {
                        if (_M0L10next__charS157 < 48) {
                          goto join_156;
                        } else {
                          int32_t _M0L6_2atmpS1713;
                          _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                          _M0Lm9tag__2__1S140 = _M0Lm6tag__2S139;
                          _M0Lm6tag__2S139 = _M0Lm9_2acursorS126;
                          _M0Lm6tag__3S138 = _M0Lm9_2acursorS126;
                          _M0L6_2atmpS1713 = _M0Lm9_2acursorS126;
                          if (_M0L6_2atmpS1713 < _M0L6_2aendS125) {
                            int32_t _M0L6_2atmpS1715 = _M0Lm9_2acursorS126;
                            int32_t _M0L10next__charS159;
                            int32_t _M0L6_2atmpS1714;
                            moonbit_incref(_M0L7_2adataS123);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS159
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1715);
                            _M0L6_2atmpS1714 = _M0Lm9_2acursorS126;
                            _M0Lm9_2acursorS126 = _M0L6_2atmpS1714 + 1;
                            if (_M0L10next__charS159 < 48) {
                              if (_M0L10next__charS159 == 45) {
                                goto join_151;
                              } else {
                                goto join_158;
                              }
                            } else if (_M0L10next__charS159 > 57) {
                              if (_M0L10next__charS159 < 59) {
                                _M0L12dispatch__15S150 = 3;
                                goto loop__label__15_153;
                              } else {
                                goto join_158;
                              }
                            } else {
                              _M0L12dispatch__15S150 = 6;
                              goto loop__label__15_153;
                            }
                            join_158:;
                            _M0L12dispatch__15S150 = 0;
                            goto loop__label__15_153;
                          } else {
                            goto join_142;
                          }
                        }
                      } else if (_M0L10next__charS157 > 58) {
                        goto join_156;
                      } else {
                        _M0L12dispatch__15S150 = 1;
                        goto loop__label__15_153;
                      }
                      join_156:;
                      _M0L12dispatch__15S150 = 0;
                      goto loop__label__15_153;
                    } else {
                      goto join_142;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1717;
                    _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                    _M0Lm6tag__2S139 = _M0Lm9_2acursorS126;
                    _M0L6_2atmpS1717 = _M0Lm9_2acursorS126;
                    if (_M0L6_2atmpS1717 < _M0L6_2aendS125) {
                      int32_t _M0L6_2atmpS1719 = _M0Lm9_2acursorS126;
                      int32_t _M0L10next__charS161;
                      int32_t _M0L6_2atmpS1718;
                      moonbit_incref(_M0L7_2adataS123);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS161
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1719);
                      _M0L6_2atmpS1718 = _M0Lm9_2acursorS126;
                      _M0Lm9_2acursorS126 = _M0L6_2atmpS1718 + 1;
                      if (_M0L10next__charS161 < 58) {
                        if (_M0L10next__charS161 < 48) {
                          goto join_160;
                        } else {
                          _M0L12dispatch__15S150 = 2;
                          goto loop__label__15_153;
                        }
                      } else if (_M0L10next__charS161 > 58) {
                        goto join_160;
                      } else {
                        _M0L12dispatch__15S150 = 3;
                        goto loop__label__15_153;
                      }
                      join_160:;
                      _M0L12dispatch__15S150 = 0;
                      goto loop__label__15_153;
                    } else {
                      goto join_142;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1720;
                    _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                    _M0L6_2atmpS1720 = _M0Lm9_2acursorS126;
                    if (_M0L6_2atmpS1720 < _M0L6_2aendS125) {
                      int32_t _M0L6_2atmpS1722 = _M0Lm9_2acursorS126;
                      int32_t _M0L10next__charS162;
                      int32_t _M0L6_2atmpS1721;
                      moonbit_incref(_M0L7_2adataS123);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS162
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1722);
                      _M0L6_2atmpS1721 = _M0Lm9_2acursorS126;
                      _M0Lm9_2acursorS126 = _M0L6_2atmpS1721 + 1;
                      if (_M0L10next__charS162 == 58) {
                        _M0L12dispatch__15S150 = 1;
                        goto loop__label__15_153;
                      } else {
                        _M0L12dispatch__15S150 = 0;
                        goto loop__label__15_153;
                      }
                    } else {
                      goto join_142;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1723;
                    _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                    _M0Lm6tag__4S141 = _M0Lm9_2acursorS126;
                    _M0L6_2atmpS1723 = _M0Lm9_2acursorS126;
                    if (_M0L6_2atmpS1723 < _M0L6_2aendS125) {
                      int32_t _M0L6_2atmpS1731 = _M0Lm9_2acursorS126;
                      int32_t _M0L10next__charS164;
                      int32_t _M0L6_2atmpS1724;
                      moonbit_incref(_M0L7_2adataS123);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS164
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1731);
                      _M0L6_2atmpS1724 = _M0Lm9_2acursorS126;
                      _M0Lm9_2acursorS126 = _M0L6_2atmpS1724 + 1;
                      if (_M0L10next__charS164 < 58) {
                        if (_M0L10next__charS164 < 48) {
                          goto join_163;
                        } else {
                          _M0L12dispatch__15S150 = 4;
                          goto loop__label__15_153;
                        }
                      } else if (_M0L10next__charS164 > 58) {
                        goto join_163;
                      } else {
                        int32_t _M0L6_2atmpS1725;
                        _M0Lm9tag__1__2S137 = _M0Lm9tag__1__1S136;
                        _M0Lm9tag__1__1S136 = _M0Lm6tag__1S135;
                        _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                        _M0L6_2atmpS1725 = _M0Lm9_2acursorS126;
                        if (_M0L6_2atmpS1725 < _M0L6_2aendS125) {
                          int32_t _M0L6_2atmpS1730 = _M0Lm9_2acursorS126;
                          int32_t _M0L10next__charS166;
                          int32_t _M0L6_2atmpS1726;
                          moonbit_incref(_M0L7_2adataS123);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS166
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1730);
                          _M0L6_2atmpS1726 = _M0Lm9_2acursorS126;
                          _M0Lm9_2acursorS126 = _M0L6_2atmpS1726 + 1;
                          if (_M0L10next__charS166 < 58) {
                            if (_M0L10next__charS166 < 48) {
                              goto join_165;
                            } else {
                              int32_t _M0L6_2atmpS1727;
                              _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                              _M0Lm9tag__2__1S140 = _M0Lm6tag__2S139;
                              _M0Lm6tag__2S139 = _M0Lm9_2acursorS126;
                              _M0L6_2atmpS1727 = _M0Lm9_2acursorS126;
                              if (_M0L6_2atmpS1727 < _M0L6_2aendS125) {
                                int32_t _M0L6_2atmpS1729 =
                                  _M0Lm9_2acursorS126;
                                int32_t _M0L10next__charS168;
                                int32_t _M0L6_2atmpS1728;
                                moonbit_incref(_M0L7_2adataS123);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS168
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1729);
                                _M0L6_2atmpS1728 = _M0Lm9_2acursorS126;
                                _M0Lm9_2acursorS126 = _M0L6_2atmpS1728 + 1;
                                if (_M0L10next__charS168 < 58) {
                                  if (_M0L10next__charS168 < 48) {
                                    goto join_167;
                                  } else {
                                    _M0L12dispatch__15S150 = 5;
                                    goto loop__label__15_153;
                                  }
                                } else if (_M0L10next__charS168 > 58) {
                                  goto join_167;
                                } else {
                                  _M0L12dispatch__15S150 = 3;
                                  goto loop__label__15_153;
                                }
                                join_167:;
                                _M0L12dispatch__15S150 = 0;
                                goto loop__label__15_153;
                              } else {
                                goto join_155;
                              }
                            }
                          } else if (_M0L10next__charS166 > 58) {
                            goto join_165;
                          } else {
                            _M0L12dispatch__15S150 = 1;
                            goto loop__label__15_153;
                          }
                          join_165:;
                          _M0L12dispatch__15S150 = 0;
                          goto loop__label__15_153;
                        } else {
                          goto join_142;
                        }
                      }
                      join_163:;
                      _M0L12dispatch__15S150 = 0;
                      goto loop__label__15_153;
                    } else {
                      goto join_142;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1732;
                    _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                    _M0Lm6tag__2S139 = _M0Lm9_2acursorS126;
                    _M0L6_2atmpS1732 = _M0Lm9_2acursorS126;
                    if (_M0L6_2atmpS1732 < _M0L6_2aendS125) {
                      int32_t _M0L6_2atmpS1734 = _M0Lm9_2acursorS126;
                      int32_t _M0L10next__charS170;
                      int32_t _M0L6_2atmpS1733;
                      moonbit_incref(_M0L7_2adataS123);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS170
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1734);
                      _M0L6_2atmpS1733 = _M0Lm9_2acursorS126;
                      _M0Lm9_2acursorS126 = _M0L6_2atmpS1733 + 1;
                      if (_M0L10next__charS170 < 58) {
                        if (_M0L10next__charS170 < 48) {
                          goto join_169;
                        } else {
                          _M0L12dispatch__15S150 = 5;
                          goto loop__label__15_153;
                        }
                      } else if (_M0L10next__charS170 > 58) {
                        goto join_169;
                      } else {
                        _M0L12dispatch__15S150 = 3;
                        goto loop__label__15_153;
                      }
                      join_169:;
                      _M0L12dispatch__15S150 = 0;
                      goto loop__label__15_153;
                    } else {
                      goto join_155;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1735;
                    _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                    _M0Lm6tag__2S139 = _M0Lm9_2acursorS126;
                    _M0Lm6tag__3S138 = _M0Lm9_2acursorS126;
                    _M0L6_2atmpS1735 = _M0Lm9_2acursorS126;
                    if (_M0L6_2atmpS1735 < _M0L6_2aendS125) {
                      int32_t _M0L6_2atmpS1737 = _M0Lm9_2acursorS126;
                      int32_t _M0L10next__charS172;
                      int32_t _M0L6_2atmpS1736;
                      moonbit_incref(_M0L7_2adataS123);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS172
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1737);
                      _M0L6_2atmpS1736 = _M0Lm9_2acursorS126;
                      _M0Lm9_2acursorS126 = _M0L6_2atmpS1736 + 1;
                      if (_M0L10next__charS172 < 48) {
                        if (_M0L10next__charS172 == 45) {
                          goto join_151;
                        } else {
                          goto join_171;
                        }
                      } else if (_M0L10next__charS172 > 57) {
                        if (_M0L10next__charS172 < 59) {
                          _M0L12dispatch__15S150 = 3;
                          goto loop__label__15_153;
                        } else {
                          goto join_171;
                        }
                      } else {
                        _M0L12dispatch__15S150 = 6;
                        goto loop__label__15_153;
                      }
                      join_171:;
                      _M0L12dispatch__15S150 = 0;
                      goto loop__label__15_153;
                    } else {
                      goto join_142;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1738;
                    _M0Lm9tag__1__1S136 = _M0Lm6tag__1S135;
                    _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                    _M0L6_2atmpS1738 = _M0Lm9_2acursorS126;
                    if (_M0L6_2atmpS1738 < _M0L6_2aendS125) {
                      int32_t _M0L6_2atmpS1740 = _M0Lm9_2acursorS126;
                      int32_t _M0L10next__charS174;
                      int32_t _M0L6_2atmpS1739;
                      moonbit_incref(_M0L7_2adataS123);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS174
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1740);
                      _M0L6_2atmpS1739 = _M0Lm9_2acursorS126;
                      _M0Lm9_2acursorS126 = _M0L6_2atmpS1739 + 1;
                      if (_M0L10next__charS174 < 58) {
                        if (_M0L10next__charS174 < 48) {
                          goto join_173;
                        } else {
                          _M0L12dispatch__15S150 = 2;
                          goto loop__label__15_153;
                        }
                      } else if (_M0L10next__charS174 > 58) {
                        goto join_173;
                      } else {
                        _M0L12dispatch__15S150 = 1;
                        goto loop__label__15_153;
                      }
                      join_173:;
                      _M0L12dispatch__15S150 = 0;
                      goto loop__label__15_153;
                    } else {
                      goto join_142;
                    }
                    break;
                  }
                  default: {
                    goto join_142;
                    break;
                  }
                }
                join_155:;
                _M0Lm6tag__1S135 = _M0Lm9tag__1__2S137;
                _M0Lm6tag__2S139 = _M0Lm9tag__2__1S140;
                _M0Lm20match__tag__saver__0S129 = _M0Lm6tag__0S134;
                _M0Lm20match__tag__saver__1S130 = _M0Lm6tag__1S135;
                _M0Lm20match__tag__saver__2S131 = _M0Lm6tag__2S139;
                _M0Lm20match__tag__saver__3S132 = _M0Lm6tag__3S138;
                _M0Lm20match__tag__saver__4S133 = _M0Lm6tag__4S141;
                _M0Lm13accept__stateS127 = 0;
                _M0Lm10match__endS128 = _M0Lm9_2acursorS126;
                goto join_142;
                join_151:;
                _M0Lm9tag__1__1S136 = _M0Lm9tag__1__2S137;
                _M0Lm6tag__1S135 = _M0Lm9_2acursorS126;
                _M0Lm6tag__2S139 = _M0Lm9tag__2__1S140;
                _M0L6_2atmpS1708 = _M0Lm9_2acursorS126;
                if (_M0L6_2atmpS1708 < _M0L6_2aendS125) {
                  int32_t _M0L6_2atmpS1710 = _M0Lm9_2acursorS126;
                  int32_t _M0L10next__charS154;
                  int32_t _M0L6_2atmpS1709;
                  moonbit_incref(_M0L7_2adataS123);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS154
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS123, _M0L6_2atmpS1710);
                  _M0L6_2atmpS1709 = _M0Lm9_2acursorS126;
                  _M0Lm9_2acursorS126 = _M0L6_2atmpS1709 + 1;
                  if (_M0L10next__charS154 < 58) {
                    if (_M0L10next__charS154 < 48) {
                      goto join_152;
                    } else {
                      _M0L12dispatch__15S150 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS154 > 58) {
                    goto join_152;
                  } else {
                    _M0L12dispatch__15S150 = 1;
                    continue;
                  }
                  join_152:;
                  _M0L12dispatch__15S150 = 0;
                  continue;
                } else {
                  goto join_142;
                }
                break;
              }
            } else {
              goto join_142;
            }
          } else {
            continue;
          }
        } else {
          goto join_142;
        }
        break;
      }
    } else {
      goto join_142;
    }
  } else {
    goto join_142;
  }
  join_142:;
  switch (_M0Lm13accept__stateS127) {
    case 0: {
      int32_t _M0L6_2atmpS1699 = _M0Lm20match__tag__saver__1S130;
      int32_t _M0L6_2atmpS1698 = _M0L6_2atmpS1699 + 1;
      int64_t _M0L6_2atmpS1695 = (int64_t)_M0L6_2atmpS1698;
      int32_t _M0L6_2atmpS1697 = _M0Lm20match__tag__saver__2S131;
      int64_t _M0L6_2atmpS1696 = (int64_t)_M0L6_2atmpS1697;
      struct _M0TPC16string10StringView _M0L11start__lineS143;
      int32_t _M0L6_2atmpS1694;
      int32_t _M0L6_2atmpS1693;
      int64_t _M0L6_2atmpS1690;
      int32_t _M0L6_2atmpS1692;
      int64_t _M0L6_2atmpS1691;
      struct _M0TPC16string10StringView _M0L13start__columnS144;
      int32_t _M0L6_2atmpS1689;
      int64_t _M0L6_2atmpS1686;
      int32_t _M0L6_2atmpS1688;
      int64_t _M0L6_2atmpS1687;
      struct _M0TPC16string10StringView _M0L3pkgS145;
      int32_t _M0L6_2atmpS1685;
      int32_t _M0L6_2atmpS1684;
      int64_t _M0L6_2atmpS1681;
      int32_t _M0L6_2atmpS1683;
      int64_t _M0L6_2atmpS1682;
      struct _M0TPC16string10StringView _M0L8filenameS146;
      int32_t _M0L6_2atmpS1680;
      int32_t _M0L6_2atmpS1679;
      int64_t _M0L6_2atmpS1676;
      int32_t _M0L6_2atmpS1678;
      int64_t _M0L6_2atmpS1677;
      struct _M0TPC16string10StringView _M0L9end__lineS147;
      int32_t _M0L6_2atmpS1675;
      int32_t _M0L6_2atmpS1674;
      int64_t _M0L6_2atmpS1671;
      int32_t _M0L6_2atmpS1673;
      int64_t _M0L6_2atmpS1672;
      struct _M0TPC16string10StringView _M0L11end__columnS148;
      struct _M0TPB13SourceLocRepr* _block_3953;
      moonbit_incref(_M0L7_2adataS123);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS143
      = _M0MPC16string6String4view(_M0L7_2adataS123, _M0L6_2atmpS1695, _M0L6_2atmpS1696);
      _M0L6_2atmpS1694 = _M0Lm20match__tag__saver__2S131;
      _M0L6_2atmpS1693 = _M0L6_2atmpS1694 + 1;
      _M0L6_2atmpS1690 = (int64_t)_M0L6_2atmpS1693;
      _M0L6_2atmpS1692 = _M0Lm20match__tag__saver__3S132;
      _M0L6_2atmpS1691 = (int64_t)_M0L6_2atmpS1692;
      moonbit_incref(_M0L7_2adataS123);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS144
      = _M0MPC16string6String4view(_M0L7_2adataS123, _M0L6_2atmpS1690, _M0L6_2atmpS1691);
      _M0L6_2atmpS1689 = _M0L8_2astartS124 + 1;
      _M0L6_2atmpS1686 = (int64_t)_M0L6_2atmpS1689;
      _M0L6_2atmpS1688 = _M0Lm20match__tag__saver__0S129;
      _M0L6_2atmpS1687 = (int64_t)_M0L6_2atmpS1688;
      moonbit_incref(_M0L7_2adataS123);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS145
      = _M0MPC16string6String4view(_M0L7_2adataS123, _M0L6_2atmpS1686, _M0L6_2atmpS1687);
      _M0L6_2atmpS1685 = _M0Lm20match__tag__saver__0S129;
      _M0L6_2atmpS1684 = _M0L6_2atmpS1685 + 1;
      _M0L6_2atmpS1681 = (int64_t)_M0L6_2atmpS1684;
      _M0L6_2atmpS1683 = _M0Lm20match__tag__saver__1S130;
      _M0L6_2atmpS1682 = (int64_t)_M0L6_2atmpS1683;
      moonbit_incref(_M0L7_2adataS123);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS146
      = _M0MPC16string6String4view(_M0L7_2adataS123, _M0L6_2atmpS1681, _M0L6_2atmpS1682);
      _M0L6_2atmpS1680 = _M0Lm20match__tag__saver__3S132;
      _M0L6_2atmpS1679 = _M0L6_2atmpS1680 + 1;
      _M0L6_2atmpS1676 = (int64_t)_M0L6_2atmpS1679;
      _M0L6_2atmpS1678 = _M0Lm20match__tag__saver__4S133;
      _M0L6_2atmpS1677 = (int64_t)_M0L6_2atmpS1678;
      moonbit_incref(_M0L7_2adataS123);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS147
      = _M0MPC16string6String4view(_M0L7_2adataS123, _M0L6_2atmpS1676, _M0L6_2atmpS1677);
      _M0L6_2atmpS1675 = _M0Lm20match__tag__saver__4S133;
      _M0L6_2atmpS1674 = _M0L6_2atmpS1675 + 1;
      _M0L6_2atmpS1671 = (int64_t)_M0L6_2atmpS1674;
      _M0L6_2atmpS1673 = _M0Lm10match__endS128;
      _M0L6_2atmpS1672 = (int64_t)_M0L6_2atmpS1673;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS148
      = _M0MPC16string6String4view(_M0L7_2adataS123, _M0L6_2atmpS1671, _M0L6_2atmpS1672);
      _block_3953
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3953)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3953->$0_0 = _M0L3pkgS145.$0;
      _block_3953->$0_1 = _M0L3pkgS145.$1;
      _block_3953->$0_2 = _M0L3pkgS145.$2;
      _block_3953->$1_0 = _M0L8filenameS146.$0;
      _block_3953->$1_1 = _M0L8filenameS146.$1;
      _block_3953->$1_2 = _M0L8filenameS146.$2;
      _block_3953->$2_0 = _M0L11start__lineS143.$0;
      _block_3953->$2_1 = _M0L11start__lineS143.$1;
      _block_3953->$2_2 = _M0L11start__lineS143.$2;
      _block_3953->$3_0 = _M0L13start__columnS144.$0;
      _block_3953->$3_1 = _M0L13start__columnS144.$1;
      _block_3953->$3_2 = _M0L13start__columnS144.$2;
      _block_3953->$4_0 = _M0L9end__lineS147.$0;
      _block_3953->$4_1 = _M0L9end__lineS147.$1;
      _block_3953->$4_2 = _M0L9end__lineS147.$2;
      _block_3953->$5_0 = _M0L11end__columnS148.$0;
      _block_3953->$5_1 = _M0L11end__columnS148.$1;
      _block_3953->$5_2 = _M0L11end__columnS148.$2;
      return _block_3953;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS123);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS119,
  int32_t _M0L5indexS120
) {
  int32_t _M0L3lenS118;
  int32_t _if__result_3954;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS118 = _M0L4selfS119->$1;
  if (_M0L5indexS120 >= 0) {
    _if__result_3954 = _M0L5indexS120 < _M0L3lenS118;
  } else {
    _if__result_3954 = 0;
  }
  if (_if__result_3954) {
    moonbit_string_t* _M0L6_2atmpS1670;
    moonbit_string_t _M0L6_2atmpS3580;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1670 = _M0MPC15array5Array6bufferGsE(_M0L4selfS119);
    if (
      _M0L5indexS120 < 0
      || _M0L5indexS120 >= Moonbit_array_length(_M0L6_2atmpS1670)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3580 = (moonbit_string_t)_M0L6_2atmpS1670[_M0L5indexS120];
    moonbit_incref(_M0L6_2atmpS3580);
    moonbit_decref(_M0L6_2atmpS1670);
    return _M0L6_2atmpS3580;
  } else {
    moonbit_decref(_M0L4selfS119);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS114
) {
  moonbit_string_t* _M0L8_2afieldS3581;
  int32_t _M0L6_2acntS3748;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3581 = _M0L4selfS114->$0;
  _M0L6_2acntS3748 = Moonbit_object_header(_M0L4selfS114)->rc;
  if (_M0L6_2acntS3748 > 1) {
    int32_t _M0L11_2anew__cntS3749 = _M0L6_2acntS3748 - 1;
    Moonbit_object_header(_M0L4selfS114)->rc = _M0L11_2anew__cntS3749;
    moonbit_incref(_M0L8_2afieldS3581);
  } else if (_M0L6_2acntS3748 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS114);
  }
  return _M0L8_2afieldS3581;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS115
) {
  struct _M0TUsiE** _M0L8_2afieldS3582;
  int32_t _M0L6_2acntS3750;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3582 = _M0L4selfS115->$0;
  _M0L6_2acntS3750 = Moonbit_object_header(_M0L4selfS115)->rc;
  if (_M0L6_2acntS3750 > 1) {
    int32_t _M0L11_2anew__cntS3751 = _M0L6_2acntS3750 - 1;
    Moonbit_object_header(_M0L4selfS115)->rc = _M0L11_2anew__cntS3751;
    moonbit_incref(_M0L8_2afieldS3582);
  } else if (_M0L6_2acntS3750 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS115);
  }
  return _M0L8_2afieldS3582;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS116
) {
  void** _M0L8_2afieldS3583;
  int32_t _M0L6_2acntS3752;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3583 = _M0L4selfS116->$0;
  _M0L6_2acntS3752 = Moonbit_object_header(_M0L4selfS116)->rc;
  if (_M0L6_2acntS3752 > 1) {
    int32_t _M0L11_2anew__cntS3753 = _M0L6_2acntS3752 - 1;
    Moonbit_object_header(_M0L4selfS116)->rc = _M0L11_2anew__cntS3753;
    moonbit_incref(_M0L8_2afieldS3583);
  } else if (_M0L6_2acntS3752 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS116);
  }
  return _M0L8_2afieldS3583;
}

void** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS117
) {
  void** _M0L8_2afieldS3584;
  int32_t _M0L6_2acntS3754;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3584 = _M0L4selfS117->$0;
  _M0L6_2acntS3754 = Moonbit_object_header(_M0L4selfS117)->rc;
  if (_M0L6_2acntS3754 > 1) {
    int32_t _M0L11_2anew__cntS3755 = _M0L6_2acntS3754 - 1;
    Moonbit_object_header(_M0L4selfS117)->rc = _M0L11_2anew__cntS3755;
    moonbit_incref(_M0L8_2afieldS3584);
  } else if (_M0L6_2acntS3754 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS117);
  }
  return _M0L8_2afieldS3584;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS113) {
  struct _M0TPB13StringBuilder* _M0L3bufS112;
  struct _M0TPB6Logger _M0L6_2atmpS1669;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS112 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS112);
  _M0L6_2atmpS1669
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS112
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS113, _M0L6_2atmpS1669);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS112);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS111) {
  int32_t _M0L6_2atmpS1668;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1668 = (int32_t)_M0L4selfS111;
  return _M0L6_2atmpS1668;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS109,
  int32_t _M0L8trailingS110
) {
  int32_t _M0L6_2atmpS1667;
  int32_t _M0L6_2atmpS1666;
  int32_t _M0L6_2atmpS1665;
  int32_t _M0L6_2atmpS1664;
  int32_t _M0L6_2atmpS1663;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1667 = _M0L7leadingS109 - 55296;
  _M0L6_2atmpS1666 = _M0L6_2atmpS1667 * 1024;
  _M0L6_2atmpS1665 = _M0L6_2atmpS1666 + _M0L8trailingS110;
  _M0L6_2atmpS1664 = _M0L6_2atmpS1665 - 56320;
  _M0L6_2atmpS1663 = _M0L6_2atmpS1664 + 65536;
  return _M0L6_2atmpS1663;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS108) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS108 >= 56320) {
    return _M0L4selfS108 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS107) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS107 >= 55296) {
    return _M0L4selfS107 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS104,
  int32_t _M0L2chS106
) {
  int32_t _M0L3lenS1658;
  int32_t _M0L6_2atmpS1657;
  moonbit_bytes_t _M0L8_2afieldS3585;
  moonbit_bytes_t _M0L4dataS1661;
  int32_t _M0L3lenS1662;
  int32_t _M0L3incS105;
  int32_t _M0L3lenS1660;
  int32_t _M0L6_2atmpS1659;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1658 = _M0L4selfS104->$1;
  _M0L6_2atmpS1657 = _M0L3lenS1658 + 4;
  moonbit_incref(_M0L4selfS104);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS104, _M0L6_2atmpS1657);
  _M0L8_2afieldS3585 = _M0L4selfS104->$0;
  _M0L4dataS1661 = _M0L8_2afieldS3585;
  _M0L3lenS1662 = _M0L4selfS104->$1;
  moonbit_incref(_M0L4dataS1661);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS105
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1661, _M0L3lenS1662, _M0L2chS106);
  _M0L3lenS1660 = _M0L4selfS104->$1;
  _M0L6_2atmpS1659 = _M0L3lenS1660 + _M0L3incS105;
  _M0L4selfS104->$1 = _M0L6_2atmpS1659;
  moonbit_decref(_M0L4selfS104);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS99,
  int32_t _M0L8requiredS100
) {
  moonbit_bytes_t _M0L8_2afieldS3589;
  moonbit_bytes_t _M0L4dataS1656;
  int32_t _M0L6_2atmpS3588;
  int32_t _M0L12current__lenS98;
  int32_t _M0Lm13enough__spaceS101;
  int32_t _M0L6_2atmpS1654;
  int32_t _M0L6_2atmpS1655;
  moonbit_bytes_t _M0L9new__dataS103;
  moonbit_bytes_t _M0L8_2afieldS3587;
  moonbit_bytes_t _M0L4dataS1652;
  int32_t _M0L3lenS1653;
  moonbit_bytes_t _M0L6_2aoldS3586;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3589 = _M0L4selfS99->$0;
  _M0L4dataS1656 = _M0L8_2afieldS3589;
  _M0L6_2atmpS3588 = Moonbit_array_length(_M0L4dataS1656);
  _M0L12current__lenS98 = _M0L6_2atmpS3588;
  if (_M0L8requiredS100 <= _M0L12current__lenS98) {
    moonbit_decref(_M0L4selfS99);
    return 0;
  }
  _M0Lm13enough__spaceS101 = _M0L12current__lenS98;
  while (1) {
    int32_t _M0L6_2atmpS1650 = _M0Lm13enough__spaceS101;
    if (_M0L6_2atmpS1650 < _M0L8requiredS100) {
      int32_t _M0L6_2atmpS1651 = _M0Lm13enough__spaceS101;
      _M0Lm13enough__spaceS101 = _M0L6_2atmpS1651 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1654 = _M0Lm13enough__spaceS101;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1655 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS103
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1654, _M0L6_2atmpS1655);
  _M0L8_2afieldS3587 = _M0L4selfS99->$0;
  _M0L4dataS1652 = _M0L8_2afieldS3587;
  _M0L3lenS1653 = _M0L4selfS99->$1;
  moonbit_incref(_M0L4dataS1652);
  moonbit_incref(_M0L9new__dataS103);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS103, 0, _M0L4dataS1652, 0, _M0L3lenS1653);
  _M0L6_2aoldS3586 = _M0L4selfS99->$0;
  moonbit_decref(_M0L6_2aoldS3586);
  _M0L4selfS99->$0 = _M0L9new__dataS103;
  moonbit_decref(_M0L4selfS99);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS93,
  int32_t _M0L6offsetS94,
  int32_t _M0L5valueS92
) {
  uint32_t _M0L4codeS91;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS91 = _M0MPC14char4Char8to__uint(_M0L5valueS92);
  if (_M0L4codeS91 < 65536u) {
    uint32_t _M0L6_2atmpS1633 = _M0L4codeS91 & 255u;
    int32_t _M0L6_2atmpS1632;
    int32_t _M0L6_2atmpS1634;
    uint32_t _M0L6_2atmpS1636;
    int32_t _M0L6_2atmpS1635;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1632 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1633);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1632;
    _M0L6_2atmpS1634 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1636 = _M0L4codeS91 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1635 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1636);
    if (
      _M0L6_2atmpS1634 < 0
      || _M0L6_2atmpS1634 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1634] = _M0L6_2atmpS1635;
    moonbit_decref(_M0L4selfS93);
    return 2;
  } else if (_M0L4codeS91 < 1114112u) {
    uint32_t _M0L2hiS95 = _M0L4codeS91 - 65536u;
    uint32_t _M0L6_2atmpS1649 = _M0L2hiS95 >> 10;
    uint32_t _M0L2loS96 = _M0L6_2atmpS1649 | 55296u;
    uint32_t _M0L6_2atmpS1648 = _M0L2hiS95 & 1023u;
    uint32_t _M0L2hiS97 = _M0L6_2atmpS1648 | 56320u;
    uint32_t _M0L6_2atmpS1638 = _M0L2loS96 & 255u;
    int32_t _M0L6_2atmpS1637;
    int32_t _M0L6_2atmpS1639;
    uint32_t _M0L6_2atmpS1641;
    int32_t _M0L6_2atmpS1640;
    int32_t _M0L6_2atmpS1642;
    uint32_t _M0L6_2atmpS1644;
    int32_t _M0L6_2atmpS1643;
    int32_t _M0L6_2atmpS1645;
    uint32_t _M0L6_2atmpS1647;
    int32_t _M0L6_2atmpS1646;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1637 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1638);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1637;
    _M0L6_2atmpS1639 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1641 = _M0L2loS96 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1640 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1641);
    if (
      _M0L6_2atmpS1639 < 0
      || _M0L6_2atmpS1639 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1639] = _M0L6_2atmpS1640;
    _M0L6_2atmpS1642 = _M0L6offsetS94 + 2;
    _M0L6_2atmpS1644 = _M0L2hiS97 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1643 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1644);
    if (
      _M0L6_2atmpS1642 < 0
      || _M0L6_2atmpS1642 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1642] = _M0L6_2atmpS1643;
    _M0L6_2atmpS1645 = _M0L6offsetS94 + 3;
    _M0L6_2atmpS1647 = _M0L2hiS97 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1646 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1647);
    if (
      _M0L6_2atmpS1645 < 0
      || _M0L6_2atmpS1645 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1645] = _M0L6_2atmpS1646;
    moonbit_decref(_M0L4selfS93);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS93);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_100.data, (moonbit_string_t)moonbit_string_literal_101.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS90) {
  int32_t _M0L6_2atmpS1631;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1631 = *(int32_t*)&_M0L4selfS90;
  return _M0L6_2atmpS1631 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1630;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1630 = _M0L4selfS89;
  return *(uint32_t*)&_M0L6_2atmpS1630;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS88
) {
  moonbit_bytes_t _M0L8_2afieldS3591;
  moonbit_bytes_t _M0L4dataS1629;
  moonbit_bytes_t _M0L6_2atmpS1626;
  int32_t _M0L8_2afieldS3590;
  int32_t _M0L3lenS1628;
  int64_t _M0L6_2atmpS1627;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3591 = _M0L4selfS88->$0;
  _M0L4dataS1629 = _M0L8_2afieldS3591;
  moonbit_incref(_M0L4dataS1629);
  _M0L6_2atmpS1626 = _M0L4dataS1629;
  _M0L8_2afieldS3590 = _M0L4selfS88->$1;
  moonbit_decref(_M0L4selfS88);
  _M0L3lenS1628 = _M0L8_2afieldS3590;
  _M0L6_2atmpS1627 = (int64_t)_M0L3lenS1628;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1626, 0, _M0L6_2atmpS1627);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS83,
  int32_t _M0L6offsetS87,
  int64_t _M0L6lengthS85
) {
  int32_t _M0L3lenS82;
  int32_t _M0L6lengthS84;
  int32_t _if__result_3956;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS82 = Moonbit_array_length(_M0L4selfS83);
  if (_M0L6lengthS85 == 4294967296ll) {
    _M0L6lengthS84 = _M0L3lenS82 - _M0L6offsetS87;
  } else {
    int64_t _M0L7_2aSomeS86 = _M0L6lengthS85;
    _M0L6lengthS84 = (int32_t)_M0L7_2aSomeS86;
  }
  if (_M0L6offsetS87 >= 0) {
    if (_M0L6lengthS84 >= 0) {
      int32_t _M0L6_2atmpS1625 = _M0L6offsetS87 + _M0L6lengthS84;
      _if__result_3956 = _M0L6_2atmpS1625 <= _M0L3lenS82;
    } else {
      _if__result_3956 = 0;
    }
  } else {
    _if__result_3956 = 0;
  }
  if (_if__result_3956) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS83, _M0L6offsetS87, _M0L6lengthS84);
  } else {
    moonbit_decref(_M0L4selfS83);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS80
) {
  int32_t _M0L7initialS79;
  moonbit_bytes_t _M0L4dataS81;
  struct _M0TPB13StringBuilder* _block_3957;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS80 < 1) {
    _M0L7initialS79 = 1;
  } else {
    _M0L7initialS79 = _M0L10size__hintS80;
  }
  _M0L4dataS81 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS79, 0);
  _block_3957
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3957)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3957->$0 = _M0L4dataS81;
  _block_3957->$1 = 0;
  return _block_3957;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS78) {
  int32_t _M0L6_2atmpS1624;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1624 = (int32_t)_M0L4selfS78;
  return _M0L6_2atmpS1624;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS63,
  int32_t _M0L11dst__offsetS64,
  moonbit_string_t* _M0L3srcS65,
  int32_t _M0L11src__offsetS66,
  int32_t _M0L3lenS67
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS63, _M0L11dst__offsetS64, _M0L3srcS65, _M0L11src__offsetS66, _M0L3lenS67);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS68,
  int32_t _M0L11dst__offsetS69,
  struct _M0TUsiE** _M0L3srcS70,
  int32_t _M0L11src__offsetS71,
  int32_t _M0L3lenS72
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS68, _M0L11dst__offsetS69, _M0L3srcS70, _M0L11src__offsetS71, _M0L3lenS72);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS73,
  int32_t _M0L11dst__offsetS74,
  void** _M0L3srcS75,
  int32_t _M0L11src__offsetS76,
  int32_t _M0L3lenS77
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS73, _M0L11dst__offsetS74, _M0L3srcS75, _M0L11src__offsetS76, _M0L3lenS77);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS27,
  int32_t _M0L11dst__offsetS29,
  moonbit_bytes_t _M0L3srcS28,
  int32_t _M0L11src__offsetS30,
  int32_t _M0L3lenS32
) {
  int32_t _if__result_3958;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS27 == _M0L3srcS28) {
    _if__result_3958 = _M0L11dst__offsetS29 < _M0L11src__offsetS30;
  } else {
    _if__result_3958 = 0;
  }
  if (_if__result_3958) {
    int32_t _M0L1iS31 = 0;
    while (1) {
      if (_M0L1iS31 < _M0L3lenS32) {
        int32_t _M0L6_2atmpS1588 = _M0L11dst__offsetS29 + _M0L1iS31;
        int32_t _M0L6_2atmpS1590 = _M0L11src__offsetS30 + _M0L1iS31;
        int32_t _M0L6_2atmpS1589;
        int32_t _M0L6_2atmpS1591;
        if (
          _M0L6_2atmpS1590 < 0
          || _M0L6_2atmpS1590 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1589 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1590];
        if (
          _M0L6_2atmpS1588 < 0
          || _M0L6_2atmpS1588 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1588] = _M0L6_2atmpS1589;
        _M0L6_2atmpS1591 = _M0L1iS31 + 1;
        _M0L1iS31 = _M0L6_2atmpS1591;
        continue;
      } else {
        moonbit_decref(_M0L3srcS28);
        moonbit_decref(_M0L3dstS27);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1596 = _M0L3lenS32 - 1;
    int32_t _M0L1iS34 = _M0L6_2atmpS1596;
    while (1) {
      if (_M0L1iS34 >= 0) {
        int32_t _M0L6_2atmpS1592 = _M0L11dst__offsetS29 + _M0L1iS34;
        int32_t _M0L6_2atmpS1594 = _M0L11src__offsetS30 + _M0L1iS34;
        int32_t _M0L6_2atmpS1593;
        int32_t _M0L6_2atmpS1595;
        if (
          _M0L6_2atmpS1594 < 0
          || _M0L6_2atmpS1594 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1593 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1594];
        if (
          _M0L6_2atmpS1592 < 0
          || _M0L6_2atmpS1592 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1592] = _M0L6_2atmpS1593;
        _M0L6_2atmpS1595 = _M0L1iS34 - 1;
        _M0L1iS34 = _M0L6_2atmpS1595;
        continue;
      } else {
        moonbit_decref(_M0L3srcS28);
        moonbit_decref(_M0L3dstS27);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS36,
  int32_t _M0L11dst__offsetS38,
  moonbit_string_t* _M0L3srcS37,
  int32_t _M0L11src__offsetS39,
  int32_t _M0L3lenS41
) {
  int32_t _if__result_3961;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS36 == _M0L3srcS37) {
    _if__result_3961 = _M0L11dst__offsetS38 < _M0L11src__offsetS39;
  } else {
    _if__result_3961 = 0;
  }
  if (_if__result_3961) {
    int32_t _M0L1iS40 = 0;
    while (1) {
      if (_M0L1iS40 < _M0L3lenS41) {
        int32_t _M0L6_2atmpS1597 = _M0L11dst__offsetS38 + _M0L1iS40;
        int32_t _M0L6_2atmpS1599 = _M0L11src__offsetS39 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS3593;
        moonbit_string_t _M0L6_2atmpS1598;
        moonbit_string_t _M0L6_2aoldS3592;
        int32_t _M0L6_2atmpS1600;
        if (
          _M0L6_2atmpS1599 < 0
          || _M0L6_2atmpS1599 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3593 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1599];
        _M0L6_2atmpS1598 = _M0L6_2atmpS3593;
        if (
          _M0L6_2atmpS1597 < 0
          || _M0L6_2atmpS1597 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3592 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1597];
        moonbit_incref(_M0L6_2atmpS1598);
        moonbit_decref(_M0L6_2aoldS3592);
        _M0L3dstS36[_M0L6_2atmpS1597] = _M0L6_2atmpS1598;
        _M0L6_2atmpS1600 = _M0L1iS40 + 1;
        _M0L1iS40 = _M0L6_2atmpS1600;
        continue;
      } else {
        moonbit_decref(_M0L3srcS37);
        moonbit_decref(_M0L3dstS36);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1605 = _M0L3lenS41 - 1;
    int32_t _M0L1iS43 = _M0L6_2atmpS1605;
    while (1) {
      if (_M0L1iS43 >= 0) {
        int32_t _M0L6_2atmpS1601 = _M0L11dst__offsetS38 + _M0L1iS43;
        int32_t _M0L6_2atmpS1603 = _M0L11src__offsetS39 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS3595;
        moonbit_string_t _M0L6_2atmpS1602;
        moonbit_string_t _M0L6_2aoldS3594;
        int32_t _M0L6_2atmpS1604;
        if (
          _M0L6_2atmpS1603 < 0
          || _M0L6_2atmpS1603 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3595 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1603];
        _M0L6_2atmpS1602 = _M0L6_2atmpS3595;
        if (
          _M0L6_2atmpS1601 < 0
          || _M0L6_2atmpS1601 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3594 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1601];
        moonbit_incref(_M0L6_2atmpS1602);
        moonbit_decref(_M0L6_2aoldS3594);
        _M0L3dstS36[_M0L6_2atmpS1601] = _M0L6_2atmpS1602;
        _M0L6_2atmpS1604 = _M0L1iS43 - 1;
        _M0L1iS43 = _M0L6_2atmpS1604;
        continue;
      } else {
        moonbit_decref(_M0L3srcS37);
        moonbit_decref(_M0L3dstS36);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS45,
  int32_t _M0L11dst__offsetS47,
  struct _M0TUsiE** _M0L3srcS46,
  int32_t _M0L11src__offsetS48,
  int32_t _M0L3lenS50
) {
  int32_t _if__result_3964;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS45 == _M0L3srcS46) {
    _if__result_3964 = _M0L11dst__offsetS47 < _M0L11src__offsetS48;
  } else {
    _if__result_3964 = 0;
  }
  if (_if__result_3964) {
    int32_t _M0L1iS49 = 0;
    while (1) {
      if (_M0L1iS49 < _M0L3lenS50) {
        int32_t _M0L6_2atmpS1606 = _M0L11dst__offsetS47 + _M0L1iS49;
        int32_t _M0L6_2atmpS1608 = _M0L11src__offsetS48 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS3597;
        struct _M0TUsiE* _M0L6_2atmpS1607;
        struct _M0TUsiE* _M0L6_2aoldS3596;
        int32_t _M0L6_2atmpS1609;
        if (
          _M0L6_2atmpS1608 < 0
          || _M0L6_2atmpS1608 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3597 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1608];
        _M0L6_2atmpS1607 = _M0L6_2atmpS3597;
        if (
          _M0L6_2atmpS1606 < 0
          || _M0L6_2atmpS1606 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3596 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1606];
        if (_M0L6_2atmpS1607) {
          moonbit_incref(_M0L6_2atmpS1607);
        }
        if (_M0L6_2aoldS3596) {
          moonbit_decref(_M0L6_2aoldS3596);
        }
        _M0L3dstS45[_M0L6_2atmpS1606] = _M0L6_2atmpS1607;
        _M0L6_2atmpS1609 = _M0L1iS49 + 1;
        _M0L1iS49 = _M0L6_2atmpS1609;
        continue;
      } else {
        moonbit_decref(_M0L3srcS46);
        moonbit_decref(_M0L3dstS45);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1614 = _M0L3lenS50 - 1;
    int32_t _M0L1iS52 = _M0L6_2atmpS1614;
    while (1) {
      if (_M0L1iS52 >= 0) {
        int32_t _M0L6_2atmpS1610 = _M0L11dst__offsetS47 + _M0L1iS52;
        int32_t _M0L6_2atmpS1612 = _M0L11src__offsetS48 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS3599;
        struct _M0TUsiE* _M0L6_2atmpS1611;
        struct _M0TUsiE* _M0L6_2aoldS3598;
        int32_t _M0L6_2atmpS1613;
        if (
          _M0L6_2atmpS1612 < 0
          || _M0L6_2atmpS1612 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3599 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1612];
        _M0L6_2atmpS1611 = _M0L6_2atmpS3599;
        if (
          _M0L6_2atmpS1610 < 0
          || _M0L6_2atmpS1610 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3598 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1610];
        if (_M0L6_2atmpS1611) {
          moonbit_incref(_M0L6_2atmpS1611);
        }
        if (_M0L6_2aoldS3598) {
          moonbit_decref(_M0L6_2aoldS3598);
        }
        _M0L3dstS45[_M0L6_2atmpS1610] = _M0L6_2atmpS1611;
        _M0L6_2atmpS1613 = _M0L1iS52 - 1;
        _M0L1iS52 = _M0L6_2atmpS1613;
        continue;
      } else {
        moonbit_decref(_M0L3srcS46);
        moonbit_decref(_M0L3dstS45);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS54,
  int32_t _M0L11dst__offsetS56,
  void** _M0L3srcS55,
  int32_t _M0L11src__offsetS57,
  int32_t _M0L3lenS59
) {
  int32_t _if__result_3967;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS54 == _M0L3srcS55) {
    _if__result_3967 = _M0L11dst__offsetS56 < _M0L11src__offsetS57;
  } else {
    _if__result_3967 = 0;
  }
  if (_if__result_3967) {
    int32_t _M0L1iS58 = 0;
    while (1) {
      if (_M0L1iS58 < _M0L3lenS59) {
        int32_t _M0L6_2atmpS1615 = _M0L11dst__offsetS56 + _M0L1iS58;
        int32_t _M0L6_2atmpS1617 = _M0L11src__offsetS57 + _M0L1iS58;
        void* _M0L6_2atmpS3601;
        void* _M0L6_2atmpS1616;
        void* _M0L6_2aoldS3600;
        int32_t _M0L6_2atmpS1618;
        if (
          _M0L6_2atmpS1617 < 0
          || _M0L6_2atmpS1617 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3601 = (void*)_M0L3srcS55[_M0L6_2atmpS1617];
        _M0L6_2atmpS1616 = _M0L6_2atmpS3601;
        if (
          _M0L6_2atmpS1615 < 0
          || _M0L6_2atmpS1615 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3600 = (void*)_M0L3dstS54[_M0L6_2atmpS1615];
        moonbit_incref(_M0L6_2atmpS1616);
        moonbit_decref(_M0L6_2aoldS3600);
        _M0L3dstS54[_M0L6_2atmpS1615] = _M0L6_2atmpS1616;
        _M0L6_2atmpS1618 = _M0L1iS58 + 1;
        _M0L1iS58 = _M0L6_2atmpS1618;
        continue;
      } else {
        moonbit_decref(_M0L3srcS55);
        moonbit_decref(_M0L3dstS54);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1623 = _M0L3lenS59 - 1;
    int32_t _M0L1iS61 = _M0L6_2atmpS1623;
    while (1) {
      if (_M0L1iS61 >= 0) {
        int32_t _M0L6_2atmpS1619 = _M0L11dst__offsetS56 + _M0L1iS61;
        int32_t _M0L6_2atmpS1621 = _M0L11src__offsetS57 + _M0L1iS61;
        void* _M0L6_2atmpS3603;
        void* _M0L6_2atmpS1620;
        void* _M0L6_2aoldS3602;
        int32_t _M0L6_2atmpS1622;
        if (
          _M0L6_2atmpS1621 < 0
          || _M0L6_2atmpS1621 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3603 = (void*)_M0L3srcS55[_M0L6_2atmpS1621];
        _M0L6_2atmpS1620 = _M0L6_2atmpS3603;
        if (
          _M0L6_2atmpS1619 < 0
          || _M0L6_2atmpS1619 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3602 = (void*)_M0L3dstS54[_M0L6_2atmpS1619];
        moonbit_incref(_M0L6_2atmpS1620);
        moonbit_decref(_M0L6_2aoldS3602);
        _M0L3dstS54[_M0L6_2atmpS1619] = _M0L6_2atmpS1620;
        _M0L6_2atmpS1622 = _M0L1iS61 - 1;
        _M0L1iS61 = _M0L6_2atmpS1622;
        continue;
      } else {
        moonbit_decref(_M0L3srcS55);
        moonbit_decref(_M0L3dstS54);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS19,
  moonbit_string_t _M0L3locS20
) {
  moonbit_string_t _M0L6_2atmpS1572;
  moonbit_string_t _M0L6_2atmpS3606;
  moonbit_string_t _M0L6_2atmpS1570;
  moonbit_string_t _M0L6_2atmpS1571;
  moonbit_string_t _M0L6_2atmpS3605;
  moonbit_string_t _M0L6_2atmpS1569;
  moonbit_string_t _M0L6_2atmpS3604;
  moonbit_string_t _M0L6_2atmpS1568;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1572 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3606
  = moonbit_add_string(_M0L6_2atmpS1572, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1572);
  _M0L6_2atmpS1570 = _M0L6_2atmpS3606;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1571
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3605 = moonbit_add_string(_M0L6_2atmpS1570, _M0L6_2atmpS1571);
  moonbit_decref(_M0L6_2atmpS1570);
  moonbit_decref(_M0L6_2atmpS1571);
  _M0L6_2atmpS1569 = _M0L6_2atmpS3605;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3604
  = moonbit_add_string(_M0L6_2atmpS1569, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1569);
  _M0L6_2atmpS1568 = _M0L6_2atmpS3604;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1568);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1577;
  moonbit_string_t _M0L6_2atmpS3609;
  moonbit_string_t _M0L6_2atmpS1575;
  moonbit_string_t _M0L6_2atmpS1576;
  moonbit_string_t _M0L6_2atmpS3608;
  moonbit_string_t _M0L6_2atmpS1574;
  moonbit_string_t _M0L6_2atmpS3607;
  moonbit_string_t _M0L6_2atmpS1573;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1577 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3609
  = moonbit_add_string(_M0L6_2atmpS1577, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1577);
  _M0L6_2atmpS1575 = _M0L6_2atmpS3609;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1576
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3608 = moonbit_add_string(_M0L6_2atmpS1575, _M0L6_2atmpS1576);
  moonbit_decref(_M0L6_2atmpS1575);
  moonbit_decref(_M0L6_2atmpS1576);
  _M0L6_2atmpS1574 = _M0L6_2atmpS3608;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3607
  = moonbit_add_string(_M0L6_2atmpS1574, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1574);
  _M0L6_2atmpS1573 = _M0L6_2atmpS3607;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1573);
  return 0;
}

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPB5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS1582;
  moonbit_string_t _M0L6_2atmpS3612;
  moonbit_string_t _M0L6_2atmpS1580;
  moonbit_string_t _M0L6_2atmpS1581;
  moonbit_string_t _M0L6_2atmpS3611;
  moonbit_string_t _M0L6_2atmpS1579;
  moonbit_string_t _M0L6_2atmpS3610;
  moonbit_string_t _M0L6_2atmpS1578;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1582 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3612
  = moonbit_add_string(_M0L6_2atmpS1582, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1582);
  _M0L6_2atmpS1580 = _M0L6_2atmpS3612;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1581
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3611 = moonbit_add_string(_M0L6_2atmpS1580, _M0L6_2atmpS1581);
  moonbit_decref(_M0L6_2atmpS1580);
  moonbit_decref(_M0L6_2atmpS1581);
  _M0L6_2atmpS1579 = _M0L6_2atmpS3611;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3610
  = moonbit_add_string(_M0L6_2atmpS1579, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1579);
  _M0L6_2atmpS1578 = _M0L6_2atmpS3610;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L6_2atmpS1578);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS25,
  moonbit_string_t _M0L3locS26
) {
  moonbit_string_t _M0L6_2atmpS1587;
  moonbit_string_t _M0L6_2atmpS3615;
  moonbit_string_t _M0L6_2atmpS1585;
  moonbit_string_t _M0L6_2atmpS1586;
  moonbit_string_t _M0L6_2atmpS3614;
  moonbit_string_t _M0L6_2atmpS1584;
  moonbit_string_t _M0L6_2atmpS3613;
  moonbit_string_t _M0L6_2atmpS1583;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1587 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3615
  = moonbit_add_string(_M0L6_2atmpS1587, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1587);
  _M0L6_2atmpS1585 = _M0L6_2atmpS3615;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1586
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3614 = moonbit_add_string(_M0L6_2atmpS1585, _M0L6_2atmpS1586);
  moonbit_decref(_M0L6_2atmpS1585);
  moonbit_decref(_M0L6_2atmpS1586);
  _M0L6_2atmpS1584 = _M0L6_2atmpS3614;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3613
  = moonbit_add_string(_M0L6_2atmpS1584, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1584);
  _M0L6_2atmpS1583 = _M0L6_2atmpS3613;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1583);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS17,
  uint32_t _M0L5valueS18
) {
  uint32_t _M0L3accS1567;
  uint32_t _M0L6_2atmpS1566;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1567 = _M0L4selfS17->$0;
  _M0L6_2atmpS1566 = _M0L3accS1567 + 4u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1566;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS17, _M0L5valueS18);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5inputS16
) {
  uint32_t _M0L3accS1564;
  uint32_t _M0L6_2atmpS1565;
  uint32_t _M0L6_2atmpS1563;
  uint32_t _M0L6_2atmpS1562;
  uint32_t _M0L6_2atmpS1561;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1564 = _M0L4selfS15->$0;
  _M0L6_2atmpS1565 = _M0L5inputS16 * 3266489917u;
  _M0L6_2atmpS1563 = _M0L3accS1564 + _M0L6_2atmpS1565;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1562 = _M0FPB4rotl(_M0L6_2atmpS1563, 17);
  _M0L6_2atmpS1561 = _M0L6_2atmpS1562 * 668265263u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1561;
  moonbit_decref(_M0L4selfS15);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS13, int32_t _M0L1rS14) {
  uint32_t _M0L6_2atmpS1558;
  int32_t _M0L6_2atmpS1560;
  uint32_t _M0L6_2atmpS1559;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1558 = _M0L1xS13 << (_M0L1rS14 & 31);
  _M0L6_2atmpS1560 = 32 - _M0L1rS14;
  _M0L6_2atmpS1559 = _M0L1xS13 >> (_M0L6_2atmpS1560 & 31);
  return _M0L6_2atmpS1558 | _M0L6_2atmpS1559;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S9,
  struct _M0TPB6Logger _M0L10_2ax__4934S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS3616;
  int32_t _M0L6_2acntS3756;
  moonbit_string_t _M0L15_2a_2aarg__4935S11;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S9;
  _M0L8_2afieldS3616 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS3756 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS3756 > 1) {
    int32_t _M0L11_2anew__cntS3757 = _M0L6_2acntS3756 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS3757;
    moonbit_incref(_M0L8_2afieldS3616);
  } else if (_M0L6_2acntS3756 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__4935S11 = _M0L8_2afieldS3616;
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_103.data);
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S12, _M0L15_2a_2aarg__4935S11);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_104.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS8) {
  void* _block_3970;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3970 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3970)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3970)->$0 = _M0L4selfS8;
  return _block_3970;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS7) {
  void* _block_3971;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3971 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3971)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3971)->$0 = _M0L5arrayS7;
  return _block_3971;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS6,
  moonbit_string_t _M0L3objS5
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS5, _M0L4selfS6);
  return 0;
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS1) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS1);
  moonbit_decref(_M0L3msgS1);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGuE(moonbit_string_t _M0L3msgS2) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS2);
  moonbit_decref(_M0L3msgS2);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
  return 0;
}

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPC15abort5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1504) {
  switch (Moonbit_object_tag(_M0L4_2aeS1504)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1504);
      return (moonbit_string_t)moonbit_string_literal_105.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1504);
      return (moonbit_string_t)moonbit_string_literal_106.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1504);
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1504);
      return (moonbit_string_t)moonbit_string_literal_107.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1504);
      return (moonbit_string_t)moonbit_string_literal_108.data;
      break;
    }
  }
}

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal12conversation7MessageE(
  void* _M0L11_2aobj__ptrS1523
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE* _M0L7_2aselfS1522 =
    (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal12conversation7MessageE*)_M0L11_2aobj__ptrS1523;
  return _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal12conversation7MessageE(_M0L7_2aselfS1522);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1521,
  int32_t _M0L8_2aparamS1520
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1519 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1521;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1519, _M0L8_2aparamS1520);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1518,
  struct _M0TPC16string10StringView _M0L8_2aparamS1517
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1516 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1518;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1516, _M0L8_2aparamS1517);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1515,
  moonbit_string_t _M0L8_2aparamS1512,
  int32_t _M0L8_2aparamS1513,
  int32_t _M0L8_2aparamS1514
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1511 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1515;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1511, _M0L8_2aparamS1512, _M0L8_2aparamS1513, _M0L8_2aparamS1514);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1510,
  moonbit_string_t _M0L8_2aparamS1509
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1508 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1510;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1508, _M0L8_2aparamS1509);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1557 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1556;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1555;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1426;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1554;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1553;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1552;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1531;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1427;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1551;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1550;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1549;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1532;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1428;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1548;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1547;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1546;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1533;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1429;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1545;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1544;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1543;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1534;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1430;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1542;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1541;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1540;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1535;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1431;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1539;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1538;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1537;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1536;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1425;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1530;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1529;
  _M0L6_2atmpS1557[0] = (moonbit_string_t)moonbit_string_literal_109.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal12conversation47____test__74797065735f6a736f6e2e6d6274__0_2eclo);
  _M0L8_2atupleS1556
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1556)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1556->$0
  = _M0FP48clawteam8clawteam8internal12conversation47____test__74797065735f6a736f6e2e6d6274__0_2eclo;
  _M0L8_2atupleS1556->$1 = _M0L6_2atmpS1557;
  _M0L8_2atupleS1555
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1555)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1555->$0 = 0;
  _M0L8_2atupleS1555->$1 = _M0L8_2atupleS1556;
  _M0L7_2abindS1426
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1426[0] = _M0L8_2atupleS1555;
  _M0L6_2atmpS1554 = _M0L7_2abindS1426;
  _M0L6_2atmpS1553
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1554
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1552
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1553);
  _M0L8_2atupleS1531
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1531)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1531->$0 = (moonbit_string_t)moonbit_string_literal_110.data;
  _M0L8_2atupleS1531->$1 = _M0L6_2atmpS1552;
  _M0L7_2abindS1427
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1551 = _M0L7_2abindS1427;
  _M0L6_2atmpS1550
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1551
  };
  #line 401 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1549
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1550);
  _M0L8_2atupleS1532
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1532)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1532->$0 = (moonbit_string_t)moonbit_string_literal_111.data;
  _M0L8_2atupleS1532->$1 = _M0L6_2atmpS1549;
  _M0L7_2abindS1428
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1548 = _M0L7_2abindS1428;
  _M0L6_2atmpS1547
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1548
  };
  #line 403 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1546
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1547);
  _M0L8_2atupleS1533
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1533)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1533->$0 = (moonbit_string_t)moonbit_string_literal_112.data;
  _M0L8_2atupleS1533->$1 = _M0L6_2atmpS1546;
  _M0L7_2abindS1429
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1545 = _M0L7_2abindS1429;
  _M0L6_2atmpS1544
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1545
  };
  #line 405 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1543
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1544);
  _M0L8_2atupleS1534
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1534)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1534->$0 = (moonbit_string_t)moonbit_string_literal_113.data;
  _M0L8_2atupleS1534->$1 = _M0L6_2atmpS1543;
  _M0L7_2abindS1430
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1542 = _M0L7_2abindS1430;
  _M0L6_2atmpS1541
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1542
  };
  #line 407 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1540
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1541);
  _M0L8_2atupleS1535
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1535)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1535->$0 = (moonbit_string_t)moonbit_string_literal_114.data;
  _M0L8_2atupleS1535->$1 = _M0L6_2atmpS1540;
  _M0L7_2abindS1431
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1539 = _M0L7_2abindS1431;
  _M0L6_2atmpS1538
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1539
  };
  #line 409 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1537
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1538);
  _M0L8_2atupleS1536
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1536)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1536->$0 = (moonbit_string_t)moonbit_string_literal_115.data;
  _M0L8_2atupleS1536->$1 = _M0L6_2atmpS1537;
  _M0L7_2abindS1425
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(6);
  _M0L7_2abindS1425[0] = _M0L8_2atupleS1531;
  _M0L7_2abindS1425[1] = _M0L8_2atupleS1532;
  _M0L7_2abindS1425[2] = _M0L8_2atupleS1533;
  _M0L7_2abindS1425[3] = _M0L8_2atupleS1534;
  _M0L7_2abindS1425[4] = _M0L8_2atupleS1535;
  _M0L7_2abindS1425[5] = _M0L8_2atupleS1536;
  _M0L6_2atmpS1530 = _M0L7_2abindS1425;
  _M0L6_2atmpS1529
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 6, _M0L6_2atmpS1530
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal12conversation48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1529);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1528;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1498;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1499;
  int32_t _M0L7_2abindS1500;
  int32_t _M0L2__S1501;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1528
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1498
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1498)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1498->$0 = _M0L6_2atmpS1528;
  _M0L12async__testsS1498->$1 = 0;
  #line 448 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1499
  = _M0FP48clawteam8clawteam8internal12conversation52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1500 = _M0L7_2abindS1499->$1;
  _M0L2__S1501 = 0;
  while (1) {
    if (_M0L2__S1501 < _M0L7_2abindS1500) {
      struct _M0TUsiE** _M0L8_2afieldS3620 = _M0L7_2abindS1499->$0;
      struct _M0TUsiE** _M0L3bufS1527 = _M0L8_2afieldS3620;
      struct _M0TUsiE* _M0L6_2atmpS3619 =
        (struct _M0TUsiE*)_M0L3bufS1527[_M0L2__S1501];
      struct _M0TUsiE* _M0L3argS1502 = _M0L6_2atmpS3619;
      moonbit_string_t _M0L8_2afieldS3618 = _M0L3argS1502->$0;
      moonbit_string_t _M0L6_2atmpS1524 = _M0L8_2afieldS3618;
      int32_t _M0L8_2afieldS3617 = _M0L3argS1502->$1;
      int32_t _M0L6_2atmpS1525 = _M0L8_2afieldS3617;
      int32_t _M0L6_2atmpS1526;
      moonbit_incref(_M0L6_2atmpS1524);
      moonbit_incref(_M0L12async__testsS1498);
      #line 449 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam8internal12conversation44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1498, _M0L6_2atmpS1524, _M0L6_2atmpS1525);
      _M0L6_2atmpS1526 = _M0L2__S1501 + 1;
      _M0L2__S1501 = _M0L6_2atmpS1526;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1499);
    }
    break;
  }
  #line 451 "E:\\moonbit\\clawteam\\internal\\conversation\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal12conversation28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal12conversation34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1498);
  return 0;
}