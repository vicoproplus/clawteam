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
struct _M0Y3Int;

struct _M0KTPB6ToJsonTPC16option6OptionGiE;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__;

struct _M0DTPB4Json5Array;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__;

struct _M0R38String_3a_3aiter_2eanon__u1812__l247__;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0DTP38clawteam8clawteam2ai7Message6System;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB6Logger;

struct _M0Y5Int64;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam18ai__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWEuQRPC15error5Error;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__;

struct _M0TP38clawteam8clawteam2ai8ToolCall;

struct _M0KTPB6ToJsonS3Int;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTP38clawteam8clawteam2ai7Message9Assistant;

struct _M0DTP38clawteam8clawteam2ai7Message4User;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam18ai__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0KTPB6ToJsonS6String;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TP38clawteam8clawteam2ai5Usage;

struct _M0TWRPC15error5ErrorEu;

struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB6Hasher;

struct _M0DTP38clawteam8clawteam2ai7Message4Tool;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0KTPB6ToJsonTPC16option6OptionGsE;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0DTPC15error5Error110clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPB9ArrayViewGsE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0Y3Int {
  int32_t $0;
  
};

struct _M0KTPB6ToJsonTPC16option6OptionGiE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0DTPC14json10WriteFrame6Object {
  int32_t $1;
  struct _M0TWEOUsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0DTPB4Json5Array {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0TWssbEu {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  
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

struct _M0TUsRPB6LoggerE {
  moonbit_string_t $0;
  struct _M0BTPB6Logger* $1_0;
  void* $1_1;
  
};

struct _M0TWEOc {
  int32_t(* code)(struct _M0TWEOc*);
  
};

struct _M0TPB13StringBuilder {
  int32_t $1;
  moonbit_bytes_t $0;
  
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

struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0R38String_3a_3aiter_2eanon__u1812__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0DTP38clawteam8clawteam2ai7Message6System {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0Y5Int64 {
  int64_t $0;
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam18ai__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPB4Json6Number {
  double $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0TP38clawteam8clawteam2ai8ToolCall {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  
};

struct _M0KTPB6ToJsonS3Int {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TPB6ToJson {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPB4Json6String {
  moonbit_string_t $0;
  
};

struct _M0DTP38clawteam8clawteam2ai7Message9Assistant {
  moonbit_string_t $0;
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* $1;
  
};

struct _M0DTP38clawteam8clawteam2ai7Message4User {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam18ai__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0KTPB6ToJsonS6String {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
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

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTP38clawteam8clawteam2ai7Message4Tool {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
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

struct _M0KTPB6ToJsonTPC16option6OptionGsE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0DTPC15error5Error110clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
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

struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE {
  int32_t $1;
  struct _M0TP38clawteam8clawteam2ai8ToolCall** $0;
  
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

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
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

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1173(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1164(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testC2808l433(
  struct _M0TWEOc*
);

int32_t _M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testC2804l434(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1095(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1090(
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1077(
  int32_t,
  moonbit_string_t
);

#define _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam18ai__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__0(
  
);

struct _M0TP38clawteam8clawteam2ai5Usage* _M0FP38clawteam8clawteam2ai5usage(
  int32_t,
  int32_t,
  int64_t,
  int64_t
);

struct _M0TP38clawteam8clawteam2ai5Usage* _M0FP38clawteam8clawteam2ai13usage_2einner(
  int32_t,
  int32_t,
  int32_t,
  int64_t
);

struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0FP38clawteam8clawteam2ai10tool__call(
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MP38clawteam8clawteam2ai7Message7content(void*);

void* _M0FP38clawteam8clawteam2ai13tool__message(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP38clawteam8clawteam2ai26assistant__message_2einner(
  moonbit_string_t,
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE*
);

void* _M0FP38clawteam8clawteam2ai15system__message(moonbit_string_t);

void* _M0FP38clawteam8clawteam2ai13user__message(moonbit_string_t);

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

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
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

void* _M0IPC16option6OptionPB6ToJson8to__jsonGiE(int64_t);

void* _M0IPC16option6OptionPB6ToJson8to__jsonGsE(moonbit_string_t);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2015l591(
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

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

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

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1831l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC1812l247(struct _M0TWEOc*);

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

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FP15Error10to__string(void*);

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE(
  void*
);

void* _M0IPC16string6StringPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
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

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGiE(
  void*
);

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[63]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 62), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 49, 58, 51, 45, 50, 
    52, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 51, 58, 52, 54, 45, 
    53, 51, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 51, 58, 49, 54, 45, 
    53, 51, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 97, 105, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 
    34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[63]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 62), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 49, 58, 51, 45, 50, 
    48, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[63]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 62), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 58, 49, 51, 45, 53, 
    58, 50, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 49, 58, 51, 57, 45, 
    53, 49, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_92 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 57, 58, 49, 54, 45, 
    52, 57, 58, 50, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 54, 58, 51, 45, 51, 
    54, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 55, 58, 51, 49, 45, 
    52, 55, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_120 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_82 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 53, 58, 49, 54, 45, 
    51, 53, 58, 51, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 50, 58, 52, 49, 45, 
    53, 50, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    103, 101, 116, 95, 105, 110, 102, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    123, 34, 112, 97, 114, 97, 109, 34, 58, 32, 34, 118, 97, 108, 117, 
    101, 34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[103]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 102), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 
    116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 57, 58, 51, 45, 52, 
    57, 58, 55, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 52, 58, 52, 52, 45, 
    51, 52, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    117, 115, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_96 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 49, 58, 49, 54, 45, 
    53, 49, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    123, 34, 113, 117, 101, 114, 121, 34, 58, 32, 34, 108, 97, 116, 101, 
    115, 116, 32, 110, 101, 119, 115, 34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 56, 58, 51, 45, 52, 
    56, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 58, 51, 45, 54, 58, 
    52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    99, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    84, 111, 111, 108, 32, 111, 117, 116, 112, 117, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 55, 58, 51, 45, 51, 
    55, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 55, 58, 49, 54, 45, 
    52, 55, 58, 50, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 51, 58, 49, 51, 45, 
    50, 51, 58, 50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 53, 45, 52, 58, 
    53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 50, 58, 51, 45, 53, 
    50, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 49, 58, 51, 45, 53, 
    49, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 52, 58, 49, 54, 45, 
    51, 52, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 56, 58, 49, 54, 45, 
    52, 56, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    116, 99, 95, 52, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 58, 53, 45, 49, 
    56, 58, 49, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 53, 58, 52, 53, 45, 
    51, 53, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    109, 101, 115, 115, 97, 103, 101, 95, 116, 101, 115, 116, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 58, 51, 45, 49, 48, 
    58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 58, 53, 45, 56, 58, 
    53, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 99, 95, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[101]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 100), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 
    116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    99, 111, 110, 118, 101, 114, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 54, 58, 49, 54, 45, 
    51, 54, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 53, 58, 51, 45, 51, 
    53, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_73 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 50, 58, 53, 45, 50, 
    50, 58, 55, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    103, 101, 116, 95, 115, 116, 97, 116, 117, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 57, 58, 51, 56, 45, 
    52, 57, 58, 55, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    65, 115, 115, 105, 115, 116, 97, 110, 116, 32, 109, 101, 115, 115, 
    97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 55, 58, 52, 57, 45, 
    51, 55, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    109, 101, 115, 115, 97, 103, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    83, 121, 115, 116, 101, 109, 32, 109, 101, 115, 115, 97, 103, 101, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 54, 58, 52, 52, 45, 
    51, 54, 58, 52, 55, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 56, 58, 51, 51, 45, 
    52, 56, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 55, 58, 51, 45, 52, 
    55, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    116, 99, 95, 49, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 55, 58, 49, 54, 45, 
    51, 55, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 52, 58, 51, 45, 51, 
    52, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 51, 58, 51, 45, 53, 
    51, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    102, 101, 116, 99, 104, 95, 100, 97, 116, 97, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_110 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[63]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 62), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 58, 49, 51, 45, 57, 
    58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 50, 58, 49, 54, 45, 
    53, 50, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 97, 105, 95, 98, 108, 97, 99, 107, 98, 111, 120, 
    95, 116, 101, 115, 116, 58, 109, 101, 115, 115, 97, 103, 101, 95, 
    116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 57, 58, 49, 51, 45, 
    49, 57, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    72, 101, 108, 108, 111, 44, 32, 119, 111, 114, 108, 100, 33, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1173$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1173
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__2_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__2_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0120moonbitlang_2fcore_2foption_2fOption_5bInt_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGiE}
  };

struct _M0BTPB6ToJson* _M0FP0120moonbitlang_2fcore_2foption_2fOption_5bInt_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0120moonbitlang_2fcore_2foption_2fOption_5bInt_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC16string6StringPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE}
  };

struct _M0BTPB6ToJson* _M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

void* _M0FPB4null;

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
} _M0FPB30ryu__to__string_2erecord_2f904$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB30ryu__to__string_2erecord_2f904 =
  &_M0FPB30ryu__to__string_2erecord_2f904$object.data;

void* _M0FPC17prelude4null;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP38clawteam8clawteam18ai__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2841
) {
  return _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2840
) {
  return _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test55____test__6d6573736167655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2839
) {
  return _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__0();
}

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1194,
  moonbit_string_t _M0L8filenameS1169,
  int32_t _M0L5indexS1172
) {
  struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164* _closure_3195;
  struct _M0TWssbEu* _M0L14handle__resultS1164;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1173;
  void* _M0L11_2atry__errS1188;
  struct moonbit_result_0 _tmp_3197;
  int32_t _handle__error__result_3198;
  int32_t _M0L6_2atmpS2827;
  void* _M0L3errS1189;
  moonbit_string_t _M0L4nameS1191;
  struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1192;
  moonbit_string_t _M0L8_2afieldS2842;
  int32_t _M0L6_2acntS3094;
  moonbit_string_t _M0L7_2anameS1193;
  #line 532 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1169);
  _closure_3195
  = (struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164*)moonbit_malloc(sizeof(struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164));
  Moonbit_object_header(_closure_3195)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164, $1) >> 2, 1, 0);
  _closure_3195->code
  = &_M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1164;
  _closure_3195->$0 = _M0L5indexS1172;
  _closure_3195->$1 = _M0L8filenameS1169;
  _M0L14handle__resultS1164 = (struct _M0TWssbEu*)_closure_3195;
  _M0L17error__to__stringS1173
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1173$closure.data;
  moonbit_incref(_M0L12async__testsS1194);
  moonbit_incref(_M0L17error__to__stringS1173);
  moonbit_incref(_M0L8filenameS1169);
  moonbit_incref(_M0L14handle__resultS1164);
  #line 566 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3197
  = _M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1194, _M0L8filenameS1169, _M0L5indexS1172, _M0L14handle__resultS1164, _M0L17error__to__stringS1173);
  if (_tmp_3197.tag) {
    int32_t const _M0L5_2aokS2836 = _tmp_3197.data.ok;
    _handle__error__result_3198 = _M0L5_2aokS2836;
  } else {
    void* const _M0L6_2aerrS2837 = _tmp_3197.data.err;
    moonbit_decref(_M0L12async__testsS1194);
    moonbit_decref(_M0L17error__to__stringS1173);
    moonbit_decref(_M0L8filenameS1169);
    _M0L11_2atry__errS1188 = _M0L6_2aerrS2837;
    goto join_1187;
  }
  if (_handle__error__result_3198) {
    moonbit_decref(_M0L12async__testsS1194);
    moonbit_decref(_M0L17error__to__stringS1173);
    moonbit_decref(_M0L8filenameS1169);
    _M0L6_2atmpS2827 = 1;
  } else {
    struct moonbit_result_0 _tmp_3199;
    int32_t _handle__error__result_3200;
    moonbit_incref(_M0L12async__testsS1194);
    moonbit_incref(_M0L17error__to__stringS1173);
    moonbit_incref(_M0L8filenameS1169);
    moonbit_incref(_M0L14handle__resultS1164);
    #line 569 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    _tmp_3199
    = _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1194, _M0L8filenameS1169, _M0L5indexS1172, _M0L14handle__resultS1164, _M0L17error__to__stringS1173);
    if (_tmp_3199.tag) {
      int32_t const _M0L5_2aokS2834 = _tmp_3199.data.ok;
      _handle__error__result_3200 = _M0L5_2aokS2834;
    } else {
      void* const _M0L6_2aerrS2835 = _tmp_3199.data.err;
      moonbit_decref(_M0L12async__testsS1194);
      moonbit_decref(_M0L17error__to__stringS1173);
      moonbit_decref(_M0L8filenameS1169);
      _M0L11_2atry__errS1188 = _M0L6_2aerrS2835;
      goto join_1187;
    }
    if (_handle__error__result_3200) {
      moonbit_decref(_M0L12async__testsS1194);
      moonbit_decref(_M0L17error__to__stringS1173);
      moonbit_decref(_M0L8filenameS1169);
      _M0L6_2atmpS2827 = 1;
    } else {
      struct moonbit_result_0 _tmp_3201;
      int32_t _handle__error__result_3202;
      moonbit_incref(_M0L12async__testsS1194);
      moonbit_incref(_M0L17error__to__stringS1173);
      moonbit_incref(_M0L8filenameS1169);
      moonbit_incref(_M0L14handle__resultS1164);
      #line 572 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _tmp_3201
      = _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1194, _M0L8filenameS1169, _M0L5indexS1172, _M0L14handle__resultS1164, _M0L17error__to__stringS1173);
      if (_tmp_3201.tag) {
        int32_t const _M0L5_2aokS2832 = _tmp_3201.data.ok;
        _handle__error__result_3202 = _M0L5_2aokS2832;
      } else {
        void* const _M0L6_2aerrS2833 = _tmp_3201.data.err;
        moonbit_decref(_M0L12async__testsS1194);
        moonbit_decref(_M0L17error__to__stringS1173);
        moonbit_decref(_M0L8filenameS1169);
        _M0L11_2atry__errS1188 = _M0L6_2aerrS2833;
        goto join_1187;
      }
      if (_handle__error__result_3202) {
        moonbit_decref(_M0L12async__testsS1194);
        moonbit_decref(_M0L17error__to__stringS1173);
        moonbit_decref(_M0L8filenameS1169);
        _M0L6_2atmpS2827 = 1;
      } else {
        struct moonbit_result_0 _tmp_3203;
        int32_t _handle__error__result_3204;
        moonbit_incref(_M0L12async__testsS1194);
        moonbit_incref(_M0L17error__to__stringS1173);
        moonbit_incref(_M0L8filenameS1169);
        moonbit_incref(_M0L14handle__resultS1164);
        #line 575 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        _tmp_3203
        = _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1194, _M0L8filenameS1169, _M0L5indexS1172, _M0L14handle__resultS1164, _M0L17error__to__stringS1173);
        if (_tmp_3203.tag) {
          int32_t const _M0L5_2aokS2830 = _tmp_3203.data.ok;
          _handle__error__result_3204 = _M0L5_2aokS2830;
        } else {
          void* const _M0L6_2aerrS2831 = _tmp_3203.data.err;
          moonbit_decref(_M0L12async__testsS1194);
          moonbit_decref(_M0L17error__to__stringS1173);
          moonbit_decref(_M0L8filenameS1169);
          _M0L11_2atry__errS1188 = _M0L6_2aerrS2831;
          goto join_1187;
        }
        if (_handle__error__result_3204) {
          moonbit_decref(_M0L12async__testsS1194);
          moonbit_decref(_M0L17error__to__stringS1173);
          moonbit_decref(_M0L8filenameS1169);
          _M0L6_2atmpS2827 = 1;
        } else {
          struct moonbit_result_0 _tmp_3205;
          moonbit_incref(_M0L14handle__resultS1164);
          #line 578 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
          _tmp_3205
          = _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1194, _M0L8filenameS1169, _M0L5indexS1172, _M0L14handle__resultS1164, _M0L17error__to__stringS1173);
          if (_tmp_3205.tag) {
            int32_t const _M0L5_2aokS2828 = _tmp_3205.data.ok;
            _M0L6_2atmpS2827 = _M0L5_2aokS2828;
          } else {
            void* const _M0L6_2aerrS2829 = _tmp_3205.data.err;
            _M0L11_2atry__errS1188 = _M0L6_2aerrS2829;
            goto join_1187;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2827) {
    void* _M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2838 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2838)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2838)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1188
    = _M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2838;
    goto join_1187;
  } else {
    moonbit_decref(_M0L14handle__resultS1164);
  }
  goto joinlet_3196;
  join_1187:;
  _M0L3errS1189 = _M0L11_2atry__errS1188;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1192
  = (struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1189;
  _M0L8_2afieldS2842 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1192->$0;
  _M0L6_2acntS3094
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1192)->rc;
  if (_M0L6_2acntS3094 > 1) {
    int32_t _M0L11_2anew__cntS3095 = _M0L6_2acntS3094 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1192)->rc
    = _M0L11_2anew__cntS3095;
    moonbit_incref(_M0L8_2afieldS2842);
  } else if (_M0L6_2acntS3094 == 1) {
    #line 585 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1192);
  }
  _M0L7_2anameS1193 = _M0L8_2afieldS2842;
  _M0L4nameS1191 = _M0L7_2anameS1193;
  goto join_1190;
  goto joinlet_3206;
  join_1190:;
  #line 586 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1164(_M0L14handle__resultS1164, _M0L4nameS1191, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3206:;
  joinlet_3196:;
  return 0;
}

moonbit_string_t _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1173(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2826,
  void* _M0L3errS1174
) {
  void* _M0L1eS1176;
  moonbit_string_t _M0L1eS1178;
  #line 555 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2826);
  switch (Moonbit_object_tag(_M0L3errS1174)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1179 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1174;
      moonbit_string_t _M0L8_2afieldS2843 = _M0L10_2aFailureS1179->$0;
      int32_t _M0L6_2acntS3096 =
        Moonbit_object_header(_M0L10_2aFailureS1179)->rc;
      moonbit_string_t _M0L4_2aeS1180;
      if (_M0L6_2acntS3096 > 1) {
        int32_t _M0L11_2anew__cntS3097 = _M0L6_2acntS3096 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1179)->rc
        = _M0L11_2anew__cntS3097;
        moonbit_incref(_M0L8_2afieldS2843);
      } else if (_M0L6_2acntS3096 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1179);
      }
      _M0L4_2aeS1180 = _M0L8_2afieldS2843;
      _M0L1eS1178 = _M0L4_2aeS1180;
      goto join_1177;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1181 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1174;
      moonbit_string_t _M0L8_2afieldS2844 = _M0L15_2aInspectErrorS1181->$0;
      int32_t _M0L6_2acntS3098 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1181)->rc;
      moonbit_string_t _M0L4_2aeS1182;
      if (_M0L6_2acntS3098 > 1) {
        int32_t _M0L11_2anew__cntS3099 = _M0L6_2acntS3098 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1181)->rc
        = _M0L11_2anew__cntS3099;
        moonbit_incref(_M0L8_2afieldS2844);
      } else if (_M0L6_2acntS3098 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1181);
      }
      _M0L4_2aeS1182 = _M0L8_2afieldS2844;
      _M0L1eS1178 = _M0L4_2aeS1182;
      goto join_1177;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1183 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1174;
      moonbit_string_t _M0L8_2afieldS2845 = _M0L16_2aSnapshotErrorS1183->$0;
      int32_t _M0L6_2acntS3100 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1183)->rc;
      moonbit_string_t _M0L4_2aeS1184;
      if (_M0L6_2acntS3100 > 1) {
        int32_t _M0L11_2anew__cntS3101 = _M0L6_2acntS3100 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1183)->rc
        = _M0L11_2anew__cntS3101;
        moonbit_incref(_M0L8_2afieldS2845);
      } else if (_M0L6_2acntS3100 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1183);
      }
      _M0L4_2aeS1184 = _M0L8_2afieldS2845;
      _M0L1eS1178 = _M0L4_2aeS1184;
      goto join_1177;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error110clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1185 =
        (struct _M0DTPC15error5Error110clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1174;
      moonbit_string_t _M0L8_2afieldS2846 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1185->$0;
      int32_t _M0L6_2acntS3102 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1185)->rc;
      moonbit_string_t _M0L4_2aeS1186;
      if (_M0L6_2acntS3102 > 1) {
        int32_t _M0L11_2anew__cntS3103 = _M0L6_2acntS3102 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1185)->rc
        = _M0L11_2anew__cntS3103;
        moonbit_incref(_M0L8_2afieldS2846);
      } else if (_M0L6_2acntS3102 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1185);
      }
      _M0L4_2aeS1186 = _M0L8_2afieldS2846;
      _M0L1eS1178 = _M0L4_2aeS1186;
      goto join_1177;
      break;
    }
    default: {
      _M0L1eS1176 = _M0L3errS1174;
      goto join_1175;
      break;
    }
  }
  join_1177:;
  return _M0L1eS1178;
  join_1175:;
  #line 561 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1176);
}

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1164(
  struct _M0TWssbEu* _M0L6_2aenvS2812,
  moonbit_string_t _M0L8testnameS1165,
  moonbit_string_t _M0L7messageS1166,
  int32_t _M0L7skippedS1167
) {
  struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164* _M0L14_2acasted__envS2813;
  moonbit_string_t _M0L8_2afieldS2856;
  moonbit_string_t _M0L8filenameS1169;
  int32_t _M0L8_2afieldS2855;
  int32_t _M0L6_2acntS3104;
  int32_t _M0L5indexS1172;
  int32_t _if__result_3209;
  moonbit_string_t _M0L10file__nameS1168;
  moonbit_string_t _M0L10test__nameS1170;
  moonbit_string_t _M0L7messageS1171;
  moonbit_string_t _M0L6_2atmpS2825;
  moonbit_string_t _M0L6_2atmpS2854;
  moonbit_string_t _M0L6_2atmpS2824;
  moonbit_string_t _M0L6_2atmpS2853;
  moonbit_string_t _M0L6_2atmpS2822;
  moonbit_string_t _M0L6_2atmpS2823;
  moonbit_string_t _M0L6_2atmpS2852;
  moonbit_string_t _M0L6_2atmpS2821;
  moonbit_string_t _M0L6_2atmpS2851;
  moonbit_string_t _M0L6_2atmpS2819;
  moonbit_string_t _M0L6_2atmpS2820;
  moonbit_string_t _M0L6_2atmpS2850;
  moonbit_string_t _M0L6_2atmpS2818;
  moonbit_string_t _M0L6_2atmpS2849;
  moonbit_string_t _M0L6_2atmpS2816;
  moonbit_string_t _M0L6_2atmpS2817;
  moonbit_string_t _M0L6_2atmpS2848;
  moonbit_string_t _M0L6_2atmpS2815;
  moonbit_string_t _M0L6_2atmpS2847;
  moonbit_string_t _M0L6_2atmpS2814;
  #line 539 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2813
  = (struct _M0R114_24clawteam_2fclawteam_2fai__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1164*)_M0L6_2aenvS2812;
  _M0L8_2afieldS2856 = _M0L14_2acasted__envS2813->$1;
  _M0L8filenameS1169 = _M0L8_2afieldS2856;
  _M0L8_2afieldS2855 = _M0L14_2acasted__envS2813->$0;
  _M0L6_2acntS3104 = Moonbit_object_header(_M0L14_2acasted__envS2813)->rc;
  if (_M0L6_2acntS3104 > 1) {
    int32_t _M0L11_2anew__cntS3105 = _M0L6_2acntS3104 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2813)->rc
    = _M0L11_2anew__cntS3105;
    moonbit_incref(_M0L8filenameS1169);
  } else if (_M0L6_2acntS3104 == 1) {
    #line 539 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2813);
  }
  _M0L5indexS1172 = _M0L8_2afieldS2855;
  if (!_M0L7skippedS1167) {
    _if__result_3209 = 1;
  } else {
    _if__result_3209 = 0;
  }
  if (_if__result_3209) {
    
  }
  #line 545 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1168 = _M0MPC16string6String6escape(_M0L8filenameS1169);
  #line 546 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1170 = _M0MPC16string6String6escape(_M0L8testnameS1165);
  #line 547 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1171 = _M0MPC16string6String6escape(_M0L7messageS1166);
  #line 548 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 550 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2825
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1168);
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2854
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2825);
  moonbit_decref(_M0L6_2atmpS2825);
  _M0L6_2atmpS2824 = _M0L6_2atmpS2854;
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2853
  = moonbit_add_string(_M0L6_2atmpS2824, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2824);
  _M0L6_2atmpS2822 = _M0L6_2atmpS2853;
  #line 550 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2823
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1172);
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2852 = moonbit_add_string(_M0L6_2atmpS2822, _M0L6_2atmpS2823);
  moonbit_decref(_M0L6_2atmpS2822);
  moonbit_decref(_M0L6_2atmpS2823);
  _M0L6_2atmpS2821 = _M0L6_2atmpS2852;
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2851
  = moonbit_add_string(_M0L6_2atmpS2821, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2821);
  _M0L6_2atmpS2819 = _M0L6_2atmpS2851;
  #line 550 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2820
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1170);
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2850 = moonbit_add_string(_M0L6_2atmpS2819, _M0L6_2atmpS2820);
  moonbit_decref(_M0L6_2atmpS2819);
  moonbit_decref(_M0L6_2atmpS2820);
  _M0L6_2atmpS2818 = _M0L6_2atmpS2850;
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2849
  = moonbit_add_string(_M0L6_2atmpS2818, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2818);
  _M0L6_2atmpS2816 = _M0L6_2atmpS2849;
  #line 550 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2817
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1171);
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2848 = moonbit_add_string(_M0L6_2atmpS2816, _M0L6_2atmpS2817);
  moonbit_decref(_M0L6_2atmpS2816);
  moonbit_decref(_M0L6_2atmpS2817);
  _M0L6_2atmpS2815 = _M0L6_2atmpS2848;
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2847
  = moonbit_add_string(_M0L6_2atmpS2815, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2815);
  _M0L6_2atmpS2814 = _M0L6_2atmpS2847;
  #line 549 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2814);
  #line 552 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1163,
  moonbit_string_t _M0L8filenameS1160,
  int32_t _M0L5indexS1154,
  struct _M0TWssbEu* _M0L14handle__resultS1150,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1152
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1130;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1159;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1132;
  moonbit_string_t* _M0L5attrsS1133;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1153;
  moonbit_string_t _M0L4nameS1136;
  moonbit_string_t _M0L4nameS1134;
  int32_t _M0L6_2atmpS2811;
  struct _M0TWEOs* _M0L5_2aitS1138;
  struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__* _closure_3218;
  struct _M0TWEOc* _M0L6_2atmpS2802;
  struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__* _closure_3219;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2803;
  struct moonbit_result_0 _result_3220;
  #line 413 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1163);
  moonbit_incref(_M0FP38clawteam8clawteam18ai__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 420 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1159
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP38clawteam8clawteam18ai__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1160);
  if (_M0L7_2abindS1159 == 0) {
    struct moonbit_result_0 _result_3211;
    if (_M0L7_2abindS1159) {
      moonbit_decref(_M0L7_2abindS1159);
    }
    moonbit_decref(_M0L17error__to__stringS1152);
    moonbit_decref(_M0L14handle__resultS1150);
    _result_3211.tag = 1;
    _result_3211.data.ok = 0;
    return _result_3211;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1161 =
      _M0L7_2abindS1159;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1162 =
      _M0L7_2aSomeS1161;
    _M0L10index__mapS1130 = _M0L13_2aindex__mapS1162;
    goto join_1129;
  }
  join_1129:;
  #line 422 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1153
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1130, _M0L5indexS1154);
  if (_M0L7_2abindS1153 == 0) {
    struct moonbit_result_0 _result_3213;
    if (_M0L7_2abindS1153) {
      moonbit_decref(_M0L7_2abindS1153);
    }
    moonbit_decref(_M0L17error__to__stringS1152);
    moonbit_decref(_M0L14handle__resultS1150);
    _result_3213.tag = 1;
    _result_3213.data.ok = 0;
    return _result_3213;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1155 =
      _M0L7_2abindS1153;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1156 = _M0L7_2aSomeS1155;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS2860 = _M0L4_2axS1156->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1157 = _M0L8_2afieldS2860;
    moonbit_string_t* _M0L8_2afieldS2859 = _M0L4_2axS1156->$1;
    int32_t _M0L6_2acntS3106 = Moonbit_object_header(_M0L4_2axS1156)->rc;
    moonbit_string_t* _M0L8_2aattrsS1158;
    if (_M0L6_2acntS3106 > 1) {
      int32_t _M0L11_2anew__cntS3107 = _M0L6_2acntS3106 - 1;
      Moonbit_object_header(_M0L4_2axS1156)->rc = _M0L11_2anew__cntS3107;
      moonbit_incref(_M0L8_2afieldS2859);
      moonbit_incref(_M0L4_2afS1157);
    } else if (_M0L6_2acntS3106 == 1) {
      #line 420 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1156);
    }
    _M0L8_2aattrsS1158 = _M0L8_2afieldS2859;
    _M0L1fS1132 = _M0L4_2afS1157;
    _M0L5attrsS1133 = _M0L8_2aattrsS1158;
    goto join_1131;
  }
  join_1131:;
  _M0L6_2atmpS2811 = Moonbit_array_length(_M0L5attrsS1133);
  if (_M0L6_2atmpS2811 >= 1) {
    moonbit_string_t _M0L6_2atmpS2858 = (moonbit_string_t)_M0L5attrsS1133[0];
    moonbit_string_t _M0L7_2anameS1137 = _M0L6_2atmpS2858;
    moonbit_incref(_M0L7_2anameS1137);
    _M0L4nameS1136 = _M0L7_2anameS1137;
    goto join_1135;
  } else {
    _M0L4nameS1134 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3214;
  join_1135:;
  _M0L4nameS1134 = _M0L4nameS1136;
  joinlet_3214:;
  #line 423 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1138 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1133);
  while (1) {
    moonbit_string_t _M0L4attrS1140;
    moonbit_string_t _M0L7_2abindS1147;
    int32_t _M0L6_2atmpS2795;
    int64_t _M0L6_2atmpS2794;
    moonbit_incref(_M0L5_2aitS1138);
    #line 425 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1147 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1138);
    if (_M0L7_2abindS1147 == 0) {
      if (_M0L7_2abindS1147) {
        moonbit_decref(_M0L7_2abindS1147);
      }
      moonbit_decref(_M0L5_2aitS1138);
    } else {
      moonbit_string_t _M0L7_2aSomeS1148 = _M0L7_2abindS1147;
      moonbit_string_t _M0L7_2aattrS1149 = _M0L7_2aSomeS1148;
      _M0L4attrS1140 = _M0L7_2aattrS1149;
      goto join_1139;
    }
    goto joinlet_3216;
    join_1139:;
    _M0L6_2atmpS2795 = Moonbit_array_length(_M0L4attrS1140);
    _M0L6_2atmpS2794 = (int64_t)_M0L6_2atmpS2795;
    moonbit_incref(_M0L4attrS1140);
    #line 426 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1140, 5, 0, _M0L6_2atmpS2794)
    ) {
      int32_t _M0L6_2atmpS2801 = _M0L4attrS1140[0];
      int32_t _M0L4_2axS1141 = _M0L6_2atmpS2801;
      if (_M0L4_2axS1141 == 112) {
        int32_t _M0L6_2atmpS2800 = _M0L4attrS1140[1];
        int32_t _M0L4_2axS1142 = _M0L6_2atmpS2800;
        if (_M0L4_2axS1142 == 97) {
          int32_t _M0L6_2atmpS2799 = _M0L4attrS1140[2];
          int32_t _M0L4_2axS1143 = _M0L6_2atmpS2799;
          if (_M0L4_2axS1143 == 110) {
            int32_t _M0L6_2atmpS2798 = _M0L4attrS1140[3];
            int32_t _M0L4_2axS1144 = _M0L6_2atmpS2798;
            if (_M0L4_2axS1144 == 105) {
              int32_t _M0L6_2atmpS2857 = _M0L4attrS1140[4];
              int32_t _M0L6_2atmpS2797;
              int32_t _M0L4_2axS1145;
              moonbit_decref(_M0L4attrS1140);
              _M0L6_2atmpS2797 = _M0L6_2atmpS2857;
              _M0L4_2axS1145 = _M0L6_2atmpS2797;
              if (_M0L4_2axS1145 == 99) {
                void* _M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2796;
                struct moonbit_result_0 _result_3217;
                moonbit_decref(_M0L17error__to__stringS1152);
                moonbit_decref(_M0L14handle__resultS1150);
                moonbit_decref(_M0L5_2aitS1138);
                moonbit_decref(_M0L1fS1132);
                _M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2796
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2796)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2796)->$0
                = _M0L4nameS1134;
                _result_3217.tag = 0;
                _result_3217.data.err
                = _M0L112clawteam_2fclawteam_2fai__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2796;
                return _result_3217;
              }
            } else {
              moonbit_decref(_M0L4attrS1140);
            }
          } else {
            moonbit_decref(_M0L4attrS1140);
          }
        } else {
          moonbit_decref(_M0L4attrS1140);
        }
      } else {
        moonbit_decref(_M0L4attrS1140);
      }
    } else {
      moonbit_decref(_M0L4attrS1140);
    }
    continue;
    joinlet_3216:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1150);
  moonbit_incref(_M0L4nameS1134);
  _closure_3218
  = (struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__*)moonbit_malloc(sizeof(struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__));
  Moonbit_object_header(_closure_3218)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__, $0) >> 2, 2, 0);
  _closure_3218->code
  = &_M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testC2808l433;
  _closure_3218->$0 = _M0L14handle__resultS1150;
  _closure_3218->$1 = _M0L4nameS1134;
  _M0L6_2atmpS2802 = (struct _M0TWEOc*)_closure_3218;
  _closure_3219
  = (struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__*)moonbit_malloc(sizeof(struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__));
  Moonbit_object_header(_closure_3219)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__, $0) >> 2, 3, 0);
  _closure_3219->code
  = &_M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testC2804l434;
  _closure_3219->$0 = _M0L17error__to__stringS1152;
  _closure_3219->$1 = _M0L14handle__resultS1150;
  _closure_3219->$2 = _M0L4nameS1134;
  _M0L6_2atmpS2803 = (struct _M0TWRPC15error5ErrorEu*)_closure_3219;
  #line 431 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0FP38clawteam8clawteam18ai__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1132, _M0L6_2atmpS2802, _M0L6_2atmpS2803);
  _result_3220.tag = 1;
  _result_3220.data.ok = 1;
  return _result_3220;
}

int32_t _M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testC2808l433(
  struct _M0TWEOc* _M0L6_2aenvS2809
) {
  struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__* _M0L14_2acasted__envS2810;
  moonbit_string_t _M0L8_2afieldS2862;
  moonbit_string_t _M0L4nameS1134;
  struct _M0TWssbEu* _M0L8_2afieldS2861;
  int32_t _M0L6_2acntS3108;
  struct _M0TWssbEu* _M0L14handle__resultS1150;
  #line 433 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2810
  = (struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2808__l433__*)_M0L6_2aenvS2809;
  _M0L8_2afieldS2862 = _M0L14_2acasted__envS2810->$1;
  _M0L4nameS1134 = _M0L8_2afieldS2862;
  _M0L8_2afieldS2861 = _M0L14_2acasted__envS2810->$0;
  _M0L6_2acntS3108 = Moonbit_object_header(_M0L14_2acasted__envS2810)->rc;
  if (_M0L6_2acntS3108 > 1) {
    int32_t _M0L11_2anew__cntS3109 = _M0L6_2acntS3108 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2810)->rc
    = _M0L11_2anew__cntS3109;
    moonbit_incref(_M0L4nameS1134);
    moonbit_incref(_M0L8_2afieldS2861);
  } else if (_M0L6_2acntS3108 == 1) {
    #line 433 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2810);
  }
  _M0L14handle__resultS1150 = _M0L8_2afieldS2861;
  #line 433 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1150->code(_M0L14handle__resultS1150, _M0L4nameS1134, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP38clawteam8clawteam18ai__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testC2804l434(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2805,
  void* _M0L3errS1151
) {
  struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__* _M0L14_2acasted__envS2806;
  moonbit_string_t _M0L8_2afieldS2865;
  moonbit_string_t _M0L4nameS1134;
  struct _M0TWssbEu* _M0L8_2afieldS2864;
  struct _M0TWssbEu* _M0L14handle__resultS1150;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2863;
  int32_t _M0L6_2acntS3110;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1152;
  moonbit_string_t _M0L6_2atmpS2807;
  #line 434 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2806
  = (struct _M0R197_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fai__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2804__l434__*)_M0L6_2aenvS2805;
  _M0L8_2afieldS2865 = _M0L14_2acasted__envS2806->$2;
  _M0L4nameS1134 = _M0L8_2afieldS2865;
  _M0L8_2afieldS2864 = _M0L14_2acasted__envS2806->$1;
  _M0L14handle__resultS1150 = _M0L8_2afieldS2864;
  _M0L8_2afieldS2863 = _M0L14_2acasted__envS2806->$0;
  _M0L6_2acntS3110 = Moonbit_object_header(_M0L14_2acasted__envS2806)->rc;
  if (_M0L6_2acntS3110 > 1) {
    int32_t _M0L11_2anew__cntS3111 = _M0L6_2acntS3110 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2806)->rc
    = _M0L11_2anew__cntS3111;
    moonbit_incref(_M0L4nameS1134);
    moonbit_incref(_M0L14handle__resultS1150);
    moonbit_incref(_M0L8_2afieldS2863);
  } else if (_M0L6_2acntS3110 == 1) {
    #line 434 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2806);
  }
  _M0L17error__to__stringS1152 = _M0L8_2afieldS2863;
  #line 434 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2807
  = _M0L17error__to__stringS1152->code(_M0L17error__to__stringS1152, _M0L3errS1151);
  #line 434 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1150->code(_M0L14handle__resultS1150, _M0L4nameS1134, _M0L6_2atmpS2807, 0);
  return 0;
}

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1122,
  struct _M0TWEOc* _M0L6on__okS1123,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1120
) {
  void* _M0L11_2atry__errS1118;
  struct moonbit_result_0 _tmp_3222;
  void* _M0L3errS1119;
  #line 375 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3222 = _M0L1fS1122->code(_M0L1fS1122);
  if (_tmp_3222.tag) {
    int32_t const _M0L5_2aokS2792 = _tmp_3222.data.ok;
    moonbit_decref(_M0L7on__errS1120);
  } else {
    void* const _M0L6_2aerrS2793 = _tmp_3222.data.err;
    moonbit_decref(_M0L6on__okS1123);
    _M0L11_2atry__errS1118 = _M0L6_2aerrS2793;
    goto join_1117;
  }
  #line 382 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1123->code(_M0L6on__okS1123);
  goto joinlet_3221;
  join_1117:;
  _M0L3errS1119 = _M0L11_2atry__errS1118;
  #line 383 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1120->code(_M0L7on__errS1120, _M0L3errS1119);
  joinlet_3221:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1077;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1090;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1095;
  struct _M0TUsiE** _M0L6_2atmpS2791;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1102;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1103;
  moonbit_string_t _M0L6_2atmpS2790;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1104;
  int32_t _M0L7_2abindS1105;
  int32_t _M0L2__S1106;
  #line 193 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1077 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1090
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1095 = 0;
  _M0L6_2atmpS2791 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1102
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1102)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1102->$0 = _M0L6_2atmpS2791;
  _M0L16file__and__indexS1102->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1103
  = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1090(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1090);
  #line 284 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2790 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1103, 1);
  #line 283 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1104
  = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1095(_M0L51moonbit__test__driver__internal__split__mbt__stringS1095, _M0L6_2atmpS2790, 47);
  _M0L7_2abindS1105 = _M0L10test__argsS1104->$1;
  _M0L2__S1106 = 0;
  while (1) {
    if (_M0L2__S1106 < _M0L7_2abindS1105) {
      moonbit_string_t* _M0L8_2afieldS2867 = _M0L10test__argsS1104->$0;
      moonbit_string_t* _M0L3bufS2789 = _M0L8_2afieldS2867;
      moonbit_string_t _M0L6_2atmpS2866 =
        (moonbit_string_t)_M0L3bufS2789[_M0L2__S1106];
      moonbit_string_t _M0L3argS1107 = _M0L6_2atmpS2866;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1108;
      moonbit_string_t _M0L4fileS1109;
      moonbit_string_t _M0L5rangeS1110;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1111;
      moonbit_string_t _M0L6_2atmpS2787;
      int32_t _M0L5startS1112;
      moonbit_string_t _M0L6_2atmpS2786;
      int32_t _M0L3endS1113;
      int32_t _M0L1iS1114;
      int32_t _M0L6_2atmpS2788;
      moonbit_incref(_M0L3argS1107);
      #line 288 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1108
      = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1095(_M0L51moonbit__test__driver__internal__split__mbt__stringS1095, _M0L3argS1107, 58);
      moonbit_incref(_M0L16file__and__rangeS1108);
      #line 289 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1109
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1108, 0);
      #line 290 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1110
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1108, 1);
      #line 291 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1111
      = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1095(_M0L51moonbit__test__driver__internal__split__mbt__stringS1095, _M0L5rangeS1110, 45);
      moonbit_incref(_M0L15start__and__endS1111);
      #line 294 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2787
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1111, 0);
      #line 294 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1112
      = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1077(_M0L45moonbit__test__driver__internal__parse__int__S1077, _M0L6_2atmpS2787);
      #line 295 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2786
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1111, 1);
      #line 295 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1113
      = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1077(_M0L45moonbit__test__driver__internal__parse__int__S1077, _M0L6_2atmpS2786);
      _M0L1iS1114 = _M0L5startS1112;
      while (1) {
        if (_M0L1iS1114 < _M0L3endS1113) {
          struct _M0TUsiE* _M0L8_2atupleS2784;
          int32_t _M0L6_2atmpS2785;
          moonbit_incref(_M0L4fileS1109);
          _M0L8_2atupleS2784
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2784)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2784->$0 = _M0L4fileS1109;
          _M0L8_2atupleS2784->$1 = _M0L1iS1114;
          moonbit_incref(_M0L16file__and__indexS1102);
          #line 297 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1102, _M0L8_2atupleS2784);
          _M0L6_2atmpS2785 = _M0L1iS1114 + 1;
          _M0L1iS1114 = _M0L6_2atmpS2785;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1109);
        }
        break;
      }
      _M0L6_2atmpS2788 = _M0L2__S1106 + 1;
      _M0L2__S1106 = _M0L6_2atmpS2788;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1104);
    }
    break;
  }
  return _M0L16file__and__indexS1102;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1095(
  int32_t _M0L6_2aenvS2765,
  moonbit_string_t _M0L1sS1096,
  int32_t _M0L3sepS1097
) {
  moonbit_string_t* _M0L6_2atmpS2783;
  struct _M0TPB5ArrayGsE* _M0L3resS1098;
  struct _M0TPC13ref3RefGiE* _M0L1iS1099;
  struct _M0TPC13ref3RefGiE* _M0L5startS1100;
  int32_t _M0L3valS2778;
  int32_t _M0L6_2atmpS2779;
  #line 261 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2783 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1098
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1098)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1098->$0 = _M0L6_2atmpS2783;
  _M0L3resS1098->$1 = 0;
  _M0L1iS1099
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1099)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1099->$0 = 0;
  _M0L5startS1100
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1100)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1100->$0 = 0;
  while (1) {
    int32_t _M0L3valS2766 = _M0L1iS1099->$0;
    int32_t _M0L6_2atmpS2767 = Moonbit_array_length(_M0L1sS1096);
    if (_M0L3valS2766 < _M0L6_2atmpS2767) {
      int32_t _M0L3valS2770 = _M0L1iS1099->$0;
      int32_t _M0L6_2atmpS2769;
      int32_t _M0L6_2atmpS2768;
      int32_t _M0L3valS2777;
      int32_t _M0L6_2atmpS2776;
      if (
        _M0L3valS2770 < 0
        || _M0L3valS2770 >= Moonbit_array_length(_M0L1sS1096)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2769 = _M0L1sS1096[_M0L3valS2770];
      _M0L6_2atmpS2768 = _M0L6_2atmpS2769;
      if (_M0L6_2atmpS2768 == _M0L3sepS1097) {
        int32_t _M0L3valS2772 = _M0L5startS1100->$0;
        int32_t _M0L3valS2773 = _M0L1iS1099->$0;
        moonbit_string_t _M0L6_2atmpS2771;
        int32_t _M0L3valS2775;
        int32_t _M0L6_2atmpS2774;
        moonbit_incref(_M0L1sS1096);
        #line 270 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS2771
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1096, _M0L3valS2772, _M0L3valS2773);
        moonbit_incref(_M0L3resS1098);
        #line 270 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1098, _M0L6_2atmpS2771);
        _M0L3valS2775 = _M0L1iS1099->$0;
        _M0L6_2atmpS2774 = _M0L3valS2775 + 1;
        _M0L5startS1100->$0 = _M0L6_2atmpS2774;
      }
      _M0L3valS2777 = _M0L1iS1099->$0;
      _M0L6_2atmpS2776 = _M0L3valS2777 + 1;
      _M0L1iS1099->$0 = _M0L6_2atmpS2776;
      continue;
    } else {
      moonbit_decref(_M0L1iS1099);
    }
    break;
  }
  _M0L3valS2778 = _M0L5startS1100->$0;
  _M0L6_2atmpS2779 = Moonbit_array_length(_M0L1sS1096);
  if (_M0L3valS2778 < _M0L6_2atmpS2779) {
    int32_t _M0L8_2afieldS2868 = _M0L5startS1100->$0;
    int32_t _M0L3valS2781;
    int32_t _M0L6_2atmpS2782;
    moonbit_string_t _M0L6_2atmpS2780;
    moonbit_decref(_M0L5startS1100);
    _M0L3valS2781 = _M0L8_2afieldS2868;
    _M0L6_2atmpS2782 = Moonbit_array_length(_M0L1sS1096);
    #line 276 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS2780
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1096, _M0L3valS2781, _M0L6_2atmpS2782);
    moonbit_incref(_M0L3resS1098);
    #line 276 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1098, _M0L6_2atmpS2780);
  } else {
    moonbit_decref(_M0L5startS1100);
    moonbit_decref(_M0L1sS1096);
  }
  return _M0L3resS1098;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1090(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083
) {
  moonbit_bytes_t* _M0L3tmpS1091;
  int32_t _M0L6_2atmpS2764;
  struct _M0TPB5ArrayGsE* _M0L3resS1092;
  int32_t _M0L1iS1093;
  #line 250 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1091
  = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2764 = Moonbit_array_length(_M0L3tmpS1091);
  #line 254 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1092 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2764);
  _M0L1iS1093 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2760 = Moonbit_array_length(_M0L3tmpS1091);
    if (_M0L1iS1093 < _M0L6_2atmpS2760) {
      moonbit_bytes_t _M0L6_2atmpS2869;
      moonbit_bytes_t _M0L6_2atmpS2762;
      moonbit_string_t _M0L6_2atmpS2761;
      int32_t _M0L6_2atmpS2763;
      if (
        _M0L1iS1093 < 0 || _M0L1iS1093 >= Moonbit_array_length(_M0L3tmpS1091)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2869 = (moonbit_bytes_t)_M0L3tmpS1091[_M0L1iS1093];
      _M0L6_2atmpS2762 = _M0L6_2atmpS2869;
      moonbit_incref(_M0L6_2atmpS2762);
      #line 256 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2761
      = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083, _M0L6_2atmpS2762);
      moonbit_incref(_M0L3resS1092);
      #line 256 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1092, _M0L6_2atmpS2761);
      _M0L6_2atmpS2763 = _M0L1iS1093 + 1;
      _M0L1iS1093 = _M0L6_2atmpS2763;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1091);
    }
    break;
  }
  return _M0L3resS1092;
}

moonbit_string_t _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1083(
  int32_t _M0L6_2aenvS2674,
  moonbit_bytes_t _M0L5bytesS1084
) {
  struct _M0TPB13StringBuilder* _M0L3resS1085;
  int32_t _M0L3lenS1086;
  struct _M0TPC13ref3RefGiE* _M0L1iS1087;
  #line 206 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1085 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1086 = Moonbit_array_length(_M0L5bytesS1084);
  _M0L1iS1087
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1087)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1087->$0 = 0;
  while (1) {
    int32_t _M0L3valS2675 = _M0L1iS1087->$0;
    if (_M0L3valS2675 < _M0L3lenS1086) {
      int32_t _M0L3valS2759 = _M0L1iS1087->$0;
      int32_t _M0L6_2atmpS2758;
      int32_t _M0L6_2atmpS2757;
      struct _M0TPC13ref3RefGiE* _M0L1cS1088;
      int32_t _M0L3valS2676;
      if (
        _M0L3valS2759 < 0
        || _M0L3valS2759 >= Moonbit_array_length(_M0L5bytesS1084)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2758 = _M0L5bytesS1084[_M0L3valS2759];
      _M0L6_2atmpS2757 = (int32_t)_M0L6_2atmpS2758;
      _M0L1cS1088
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1088)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1088->$0 = _M0L6_2atmpS2757;
      _M0L3valS2676 = _M0L1cS1088->$0;
      if (_M0L3valS2676 < 128) {
        int32_t _M0L8_2afieldS2870 = _M0L1cS1088->$0;
        int32_t _M0L3valS2678;
        int32_t _M0L6_2atmpS2677;
        int32_t _M0L3valS2680;
        int32_t _M0L6_2atmpS2679;
        moonbit_decref(_M0L1cS1088);
        _M0L3valS2678 = _M0L8_2afieldS2870;
        _M0L6_2atmpS2677 = _M0L3valS2678;
        moonbit_incref(_M0L3resS1085);
        #line 215 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1085, _M0L6_2atmpS2677);
        _M0L3valS2680 = _M0L1iS1087->$0;
        _M0L6_2atmpS2679 = _M0L3valS2680 + 1;
        _M0L1iS1087->$0 = _M0L6_2atmpS2679;
      } else {
        int32_t _M0L3valS2681 = _M0L1cS1088->$0;
        if (_M0L3valS2681 < 224) {
          int32_t _M0L3valS2683 = _M0L1iS1087->$0;
          int32_t _M0L6_2atmpS2682 = _M0L3valS2683 + 1;
          int32_t _M0L3valS2692;
          int32_t _M0L6_2atmpS2691;
          int32_t _M0L6_2atmpS2685;
          int32_t _M0L3valS2690;
          int32_t _M0L6_2atmpS2689;
          int32_t _M0L6_2atmpS2688;
          int32_t _M0L6_2atmpS2687;
          int32_t _M0L6_2atmpS2686;
          int32_t _M0L6_2atmpS2684;
          int32_t _M0L8_2afieldS2871;
          int32_t _M0L3valS2694;
          int32_t _M0L6_2atmpS2693;
          int32_t _M0L3valS2696;
          int32_t _M0L6_2atmpS2695;
          if (_M0L6_2atmpS2682 >= _M0L3lenS1086) {
            moonbit_decref(_M0L1cS1088);
            moonbit_decref(_M0L1iS1087);
            moonbit_decref(_M0L5bytesS1084);
            break;
          }
          _M0L3valS2692 = _M0L1cS1088->$0;
          _M0L6_2atmpS2691 = _M0L3valS2692 & 31;
          _M0L6_2atmpS2685 = _M0L6_2atmpS2691 << 6;
          _M0L3valS2690 = _M0L1iS1087->$0;
          _M0L6_2atmpS2689 = _M0L3valS2690 + 1;
          if (
            _M0L6_2atmpS2689 < 0
            || _M0L6_2atmpS2689 >= Moonbit_array_length(_M0L5bytesS1084)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2688 = _M0L5bytesS1084[_M0L6_2atmpS2689];
          _M0L6_2atmpS2687 = (int32_t)_M0L6_2atmpS2688;
          _M0L6_2atmpS2686 = _M0L6_2atmpS2687 & 63;
          _M0L6_2atmpS2684 = _M0L6_2atmpS2685 | _M0L6_2atmpS2686;
          _M0L1cS1088->$0 = _M0L6_2atmpS2684;
          _M0L8_2afieldS2871 = _M0L1cS1088->$0;
          moonbit_decref(_M0L1cS1088);
          _M0L3valS2694 = _M0L8_2afieldS2871;
          _M0L6_2atmpS2693 = _M0L3valS2694;
          moonbit_incref(_M0L3resS1085);
          #line 222 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1085, _M0L6_2atmpS2693);
          _M0L3valS2696 = _M0L1iS1087->$0;
          _M0L6_2atmpS2695 = _M0L3valS2696 + 2;
          _M0L1iS1087->$0 = _M0L6_2atmpS2695;
        } else {
          int32_t _M0L3valS2697 = _M0L1cS1088->$0;
          if (_M0L3valS2697 < 240) {
            int32_t _M0L3valS2699 = _M0L1iS1087->$0;
            int32_t _M0L6_2atmpS2698 = _M0L3valS2699 + 2;
            int32_t _M0L3valS2715;
            int32_t _M0L6_2atmpS2714;
            int32_t _M0L6_2atmpS2707;
            int32_t _M0L3valS2713;
            int32_t _M0L6_2atmpS2712;
            int32_t _M0L6_2atmpS2711;
            int32_t _M0L6_2atmpS2710;
            int32_t _M0L6_2atmpS2709;
            int32_t _M0L6_2atmpS2708;
            int32_t _M0L6_2atmpS2701;
            int32_t _M0L3valS2706;
            int32_t _M0L6_2atmpS2705;
            int32_t _M0L6_2atmpS2704;
            int32_t _M0L6_2atmpS2703;
            int32_t _M0L6_2atmpS2702;
            int32_t _M0L6_2atmpS2700;
            int32_t _M0L8_2afieldS2872;
            int32_t _M0L3valS2717;
            int32_t _M0L6_2atmpS2716;
            int32_t _M0L3valS2719;
            int32_t _M0L6_2atmpS2718;
            if (_M0L6_2atmpS2698 >= _M0L3lenS1086) {
              moonbit_decref(_M0L1cS1088);
              moonbit_decref(_M0L1iS1087);
              moonbit_decref(_M0L5bytesS1084);
              break;
            }
            _M0L3valS2715 = _M0L1cS1088->$0;
            _M0L6_2atmpS2714 = _M0L3valS2715 & 15;
            _M0L6_2atmpS2707 = _M0L6_2atmpS2714 << 12;
            _M0L3valS2713 = _M0L1iS1087->$0;
            _M0L6_2atmpS2712 = _M0L3valS2713 + 1;
            if (
              _M0L6_2atmpS2712 < 0
              || _M0L6_2atmpS2712 >= Moonbit_array_length(_M0L5bytesS1084)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2711 = _M0L5bytesS1084[_M0L6_2atmpS2712];
            _M0L6_2atmpS2710 = (int32_t)_M0L6_2atmpS2711;
            _M0L6_2atmpS2709 = _M0L6_2atmpS2710 & 63;
            _M0L6_2atmpS2708 = _M0L6_2atmpS2709 << 6;
            _M0L6_2atmpS2701 = _M0L6_2atmpS2707 | _M0L6_2atmpS2708;
            _M0L3valS2706 = _M0L1iS1087->$0;
            _M0L6_2atmpS2705 = _M0L3valS2706 + 2;
            if (
              _M0L6_2atmpS2705 < 0
              || _M0L6_2atmpS2705 >= Moonbit_array_length(_M0L5bytesS1084)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2704 = _M0L5bytesS1084[_M0L6_2atmpS2705];
            _M0L6_2atmpS2703 = (int32_t)_M0L6_2atmpS2704;
            _M0L6_2atmpS2702 = _M0L6_2atmpS2703 & 63;
            _M0L6_2atmpS2700 = _M0L6_2atmpS2701 | _M0L6_2atmpS2702;
            _M0L1cS1088->$0 = _M0L6_2atmpS2700;
            _M0L8_2afieldS2872 = _M0L1cS1088->$0;
            moonbit_decref(_M0L1cS1088);
            _M0L3valS2717 = _M0L8_2afieldS2872;
            _M0L6_2atmpS2716 = _M0L3valS2717;
            moonbit_incref(_M0L3resS1085);
            #line 231 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1085, _M0L6_2atmpS2716);
            _M0L3valS2719 = _M0L1iS1087->$0;
            _M0L6_2atmpS2718 = _M0L3valS2719 + 3;
            _M0L1iS1087->$0 = _M0L6_2atmpS2718;
          } else {
            int32_t _M0L3valS2721 = _M0L1iS1087->$0;
            int32_t _M0L6_2atmpS2720 = _M0L3valS2721 + 3;
            int32_t _M0L3valS2744;
            int32_t _M0L6_2atmpS2743;
            int32_t _M0L6_2atmpS2736;
            int32_t _M0L3valS2742;
            int32_t _M0L6_2atmpS2741;
            int32_t _M0L6_2atmpS2740;
            int32_t _M0L6_2atmpS2739;
            int32_t _M0L6_2atmpS2738;
            int32_t _M0L6_2atmpS2737;
            int32_t _M0L6_2atmpS2729;
            int32_t _M0L3valS2735;
            int32_t _M0L6_2atmpS2734;
            int32_t _M0L6_2atmpS2733;
            int32_t _M0L6_2atmpS2732;
            int32_t _M0L6_2atmpS2731;
            int32_t _M0L6_2atmpS2730;
            int32_t _M0L6_2atmpS2723;
            int32_t _M0L3valS2728;
            int32_t _M0L6_2atmpS2727;
            int32_t _M0L6_2atmpS2726;
            int32_t _M0L6_2atmpS2725;
            int32_t _M0L6_2atmpS2724;
            int32_t _M0L6_2atmpS2722;
            int32_t _M0L3valS2746;
            int32_t _M0L6_2atmpS2745;
            int32_t _M0L3valS2750;
            int32_t _M0L6_2atmpS2749;
            int32_t _M0L6_2atmpS2748;
            int32_t _M0L6_2atmpS2747;
            int32_t _M0L8_2afieldS2873;
            int32_t _M0L3valS2754;
            int32_t _M0L6_2atmpS2753;
            int32_t _M0L6_2atmpS2752;
            int32_t _M0L6_2atmpS2751;
            int32_t _M0L3valS2756;
            int32_t _M0L6_2atmpS2755;
            if (_M0L6_2atmpS2720 >= _M0L3lenS1086) {
              moonbit_decref(_M0L1cS1088);
              moonbit_decref(_M0L1iS1087);
              moonbit_decref(_M0L5bytesS1084);
              break;
            }
            _M0L3valS2744 = _M0L1cS1088->$0;
            _M0L6_2atmpS2743 = _M0L3valS2744 & 7;
            _M0L6_2atmpS2736 = _M0L6_2atmpS2743 << 18;
            _M0L3valS2742 = _M0L1iS1087->$0;
            _M0L6_2atmpS2741 = _M0L3valS2742 + 1;
            if (
              _M0L6_2atmpS2741 < 0
              || _M0L6_2atmpS2741 >= Moonbit_array_length(_M0L5bytesS1084)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2740 = _M0L5bytesS1084[_M0L6_2atmpS2741];
            _M0L6_2atmpS2739 = (int32_t)_M0L6_2atmpS2740;
            _M0L6_2atmpS2738 = _M0L6_2atmpS2739 & 63;
            _M0L6_2atmpS2737 = _M0L6_2atmpS2738 << 12;
            _M0L6_2atmpS2729 = _M0L6_2atmpS2736 | _M0L6_2atmpS2737;
            _M0L3valS2735 = _M0L1iS1087->$0;
            _M0L6_2atmpS2734 = _M0L3valS2735 + 2;
            if (
              _M0L6_2atmpS2734 < 0
              || _M0L6_2atmpS2734 >= Moonbit_array_length(_M0L5bytesS1084)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2733 = _M0L5bytesS1084[_M0L6_2atmpS2734];
            _M0L6_2atmpS2732 = (int32_t)_M0L6_2atmpS2733;
            _M0L6_2atmpS2731 = _M0L6_2atmpS2732 & 63;
            _M0L6_2atmpS2730 = _M0L6_2atmpS2731 << 6;
            _M0L6_2atmpS2723 = _M0L6_2atmpS2729 | _M0L6_2atmpS2730;
            _M0L3valS2728 = _M0L1iS1087->$0;
            _M0L6_2atmpS2727 = _M0L3valS2728 + 3;
            if (
              _M0L6_2atmpS2727 < 0
              || _M0L6_2atmpS2727 >= Moonbit_array_length(_M0L5bytesS1084)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2726 = _M0L5bytesS1084[_M0L6_2atmpS2727];
            _M0L6_2atmpS2725 = (int32_t)_M0L6_2atmpS2726;
            _M0L6_2atmpS2724 = _M0L6_2atmpS2725 & 63;
            _M0L6_2atmpS2722 = _M0L6_2atmpS2723 | _M0L6_2atmpS2724;
            _M0L1cS1088->$0 = _M0L6_2atmpS2722;
            _M0L3valS2746 = _M0L1cS1088->$0;
            _M0L6_2atmpS2745 = _M0L3valS2746 - 65536;
            _M0L1cS1088->$0 = _M0L6_2atmpS2745;
            _M0L3valS2750 = _M0L1cS1088->$0;
            _M0L6_2atmpS2749 = _M0L3valS2750 >> 10;
            _M0L6_2atmpS2748 = _M0L6_2atmpS2749 + 55296;
            _M0L6_2atmpS2747 = _M0L6_2atmpS2748;
            moonbit_incref(_M0L3resS1085);
            #line 242 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1085, _M0L6_2atmpS2747);
            _M0L8_2afieldS2873 = _M0L1cS1088->$0;
            moonbit_decref(_M0L1cS1088);
            _M0L3valS2754 = _M0L8_2afieldS2873;
            _M0L6_2atmpS2753 = _M0L3valS2754 & 1023;
            _M0L6_2atmpS2752 = _M0L6_2atmpS2753 + 56320;
            _M0L6_2atmpS2751 = _M0L6_2atmpS2752;
            moonbit_incref(_M0L3resS1085);
            #line 243 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1085, _M0L6_2atmpS2751);
            _M0L3valS2756 = _M0L1iS1087->$0;
            _M0L6_2atmpS2755 = _M0L3valS2756 + 4;
            _M0L1iS1087->$0 = _M0L6_2atmpS2755;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1087);
      moonbit_decref(_M0L5bytesS1084);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1085);
}

int32_t _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1077(
  int32_t _M0L6_2aenvS2667,
  moonbit_string_t _M0L1sS1078
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1079;
  int32_t _M0L3lenS1080;
  int32_t _M0L1iS1081;
  int32_t _M0L8_2afieldS2874;
  #line 197 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1079
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1079)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1079->$0 = 0;
  _M0L3lenS1080 = Moonbit_array_length(_M0L1sS1078);
  _M0L1iS1081 = 0;
  while (1) {
    if (_M0L1iS1081 < _M0L3lenS1080) {
      int32_t _M0L3valS2672 = _M0L3resS1079->$0;
      int32_t _M0L6_2atmpS2669 = _M0L3valS2672 * 10;
      int32_t _M0L6_2atmpS2671;
      int32_t _M0L6_2atmpS2670;
      int32_t _M0L6_2atmpS2668;
      int32_t _M0L6_2atmpS2673;
      if (
        _M0L1iS1081 < 0 || _M0L1iS1081 >= Moonbit_array_length(_M0L1sS1078)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2671 = _M0L1sS1078[_M0L1iS1081];
      _M0L6_2atmpS2670 = _M0L6_2atmpS2671 - 48;
      _M0L6_2atmpS2668 = _M0L6_2atmpS2669 + _M0L6_2atmpS2670;
      _M0L3resS1079->$0 = _M0L6_2atmpS2668;
      _M0L6_2atmpS2673 = _M0L1iS1081 + 1;
      _M0L1iS1081 = _M0L6_2atmpS2673;
      continue;
    } else {
      moonbit_decref(_M0L1sS1078);
    }
    break;
  }
  _M0L8_2afieldS2874 = _M0L3resS1079->$0;
  moonbit_decref(_M0L3resS1079);
  return _M0L8_2afieldS2874;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1057,
  moonbit_string_t _M0L12_2adiscard__S1058,
  int32_t _M0L12_2adiscard__S1059,
  struct _M0TWssbEu* _M0L12_2adiscard__S1060,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1061
) {
  struct moonbit_result_0 _result_3229;
  #line 34 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1061);
  moonbit_decref(_M0L12_2adiscard__S1060);
  moonbit_decref(_M0L12_2adiscard__S1058);
  moonbit_decref(_M0L12_2adiscard__S1057);
  _result_3229.tag = 1;
  _result_3229.data.ok = 0;
  return _result_3229;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1062,
  moonbit_string_t _M0L12_2adiscard__S1063,
  int32_t _M0L12_2adiscard__S1064,
  struct _M0TWssbEu* _M0L12_2adiscard__S1065,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1066
) {
  struct moonbit_result_0 _result_3230;
  #line 34 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1066);
  moonbit_decref(_M0L12_2adiscard__S1065);
  moonbit_decref(_M0L12_2adiscard__S1063);
  moonbit_decref(_M0L12_2adiscard__S1062);
  _result_3230.tag = 1;
  _result_3230.data.ok = 0;
  return _result_3230;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1067,
  moonbit_string_t _M0L12_2adiscard__S1068,
  int32_t _M0L12_2adiscard__S1069,
  struct _M0TWssbEu* _M0L12_2adiscard__S1070,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1071
) {
  struct moonbit_result_0 _result_3231;
  #line 34 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1071);
  moonbit_decref(_M0L12_2adiscard__S1070);
  moonbit_decref(_M0L12_2adiscard__S1068);
  moonbit_decref(_M0L12_2adiscard__S1067);
  _result_3231.tag = 1;
  _result_3231.data.ok = 0;
  return _result_3231;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam18ai__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1072,
  moonbit_string_t _M0L12_2adiscard__S1073,
  int32_t _M0L12_2adiscard__S1074,
  struct _M0TWssbEu* _M0L12_2adiscard__S1075,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1076
) {
  struct moonbit_result_0 _result_3232;
  #line 34 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1076);
  moonbit_decref(_M0L12_2adiscard__S1075);
  moonbit_decref(_M0L12_2adiscard__S1073);
  moonbit_decref(_M0L12_2adiscard__S1072);
  _result_3232.tag = 1;
  _result_3232.data.ok = 0;
  return _result_3232;
}

int32_t _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam18ai__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1056
) {
  #line 12 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1056);
  return 0;
}

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__2(
  
) {
  moonbit_string_t _M0L6_2atmpS2666;
  struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0L2tcS1054;
  moonbit_string_t _M0L8_2afieldS2880;
  moonbit_string_t _M0L2idS2602;
  struct _M0TPB6ToJson _M0L6_2atmpS2593;
  void* _M0L6_2atmpS2601;
  void* _M0L6_2atmpS2594;
  moonbit_string_t _M0L6_2atmpS2597;
  moonbit_string_t _M0L6_2atmpS2598;
  moonbit_string_t _M0L6_2atmpS2599;
  moonbit_string_t _M0L6_2atmpS2600;
  moonbit_string_t* _M0L6_2atmpS2596;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2595;
  struct moonbit_result_0 _tmp_3233;
  moonbit_string_t _M0L8_2afieldS2879;
  moonbit_string_t _M0L4nameS2614;
  struct _M0TPB6ToJson _M0L6_2atmpS2605;
  void* _M0L6_2atmpS2613;
  void* _M0L6_2atmpS2606;
  moonbit_string_t _M0L6_2atmpS2609;
  moonbit_string_t _M0L6_2atmpS2610;
  moonbit_string_t _M0L6_2atmpS2611;
  moonbit_string_t _M0L6_2atmpS2612;
  moonbit_string_t* _M0L6_2atmpS2608;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2607;
  struct moonbit_result_0 _tmp_3235;
  moonbit_string_t _M0L8_2afieldS2878;
  int32_t _M0L6_2acntS3112;
  moonbit_string_t _M0L9argumentsS2629;
  struct _M0TPB6ToJson _M0L6_2atmpS2617;
  void* _M0L6_2atmpS2628;
  void** _M0L6_2atmpS2627;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2626;
  void* _M0L6_2atmpS2625;
  void* _M0L6_2atmpS2618;
  moonbit_string_t _M0L6_2atmpS2621;
  moonbit_string_t _M0L6_2atmpS2622;
  moonbit_string_t _M0L6_2atmpS2623;
  moonbit_string_t _M0L6_2atmpS2624;
  moonbit_string_t* _M0L6_2atmpS2620;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2619;
  struct moonbit_result_0 _tmp_3237;
  moonbit_string_t _M0L6_2atmpS2665;
  struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0L12tc__wo__argsS1055;
  moonbit_string_t _M0L8_2afieldS2877;
  moonbit_string_t _M0L2idS2641;
  struct _M0TPB6ToJson _M0L6_2atmpS2632;
  void* _M0L6_2atmpS2640;
  void* _M0L6_2atmpS2633;
  moonbit_string_t _M0L6_2atmpS2636;
  moonbit_string_t _M0L6_2atmpS2637;
  moonbit_string_t _M0L6_2atmpS2638;
  moonbit_string_t _M0L6_2atmpS2639;
  moonbit_string_t* _M0L6_2atmpS2635;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2634;
  struct moonbit_result_0 _tmp_3239;
  moonbit_string_t _M0L8_2afieldS2876;
  moonbit_string_t _M0L4nameS2653;
  struct _M0TPB6ToJson _M0L6_2atmpS2644;
  void* _M0L6_2atmpS2652;
  void* _M0L6_2atmpS2645;
  moonbit_string_t _M0L6_2atmpS2648;
  moonbit_string_t _M0L6_2atmpS2649;
  moonbit_string_t _M0L6_2atmpS2650;
  moonbit_string_t _M0L6_2atmpS2651;
  moonbit_string_t* _M0L6_2atmpS2647;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2646;
  struct moonbit_result_0 _tmp_3241;
  moonbit_string_t _M0L8_2afieldS2875;
  int32_t _M0L6_2acntS3116;
  moonbit_string_t _M0L9argumentsS2664;
  struct _M0TPB6ToJson _M0L6_2atmpS2656;
  void* _M0L6_2atmpS2657;
  moonbit_string_t _M0L6_2atmpS2660;
  moonbit_string_t _M0L6_2atmpS2661;
  moonbit_string_t _M0L6_2atmpS2662;
  moonbit_string_t _M0L6_2atmpS2663;
  moonbit_string_t* _M0L6_2atmpS2659;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2658;
  #line 41 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2666 = (moonbit_string_t)moonbit_string_literal_9.data;
  #line 42 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L2tcS1054
  = _M0FP38clawteam8clawteam2ai10tool__call((moonbit_string_t)moonbit_string_literal_10.data, (moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS2666);
  _M0L8_2afieldS2880 = _M0L2tcS1054->$0;
  _M0L2idS2602 = _M0L8_2afieldS2880;
  moonbit_incref(_M0L2idS2602);
  _M0L6_2atmpS2593
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L2idS2602
  };
  #line 47 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2601
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L6_2atmpS2594 = _M0L6_2atmpS2601;
  _M0L6_2atmpS2597 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS2598 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS2599 = 0;
  _M0L6_2atmpS2600 = 0;
  _M0L6_2atmpS2596 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2596[0] = _M0L6_2atmpS2597;
  _M0L6_2atmpS2596[1] = _M0L6_2atmpS2598;
  _M0L6_2atmpS2596[2] = _M0L6_2atmpS2599;
  _M0L6_2atmpS2596[3] = _M0L6_2atmpS2600;
  _M0L6_2atmpS2595
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2595)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2595->$0 = _M0L6_2atmpS2596;
  _M0L6_2atmpS2595->$1 = 4;
  #line 47 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3233
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2593, _M0L6_2atmpS2594, (moonbit_string_t)moonbit_string_literal_14.data, _M0L6_2atmpS2595);
  if (_tmp_3233.tag) {
    int32_t const _M0L5_2aokS2603 = _tmp_3233.data.ok;
  } else {
    void* const _M0L6_2aerrS2604 = _tmp_3233.data.err;
    struct moonbit_result_0 _result_3234;
    moonbit_decref(_M0L2tcS1054);
    _result_3234.tag = 0;
    _result_3234.data.err = _M0L6_2aerrS2604;
    return _result_3234;
  }
  _M0L8_2afieldS2879 = _M0L2tcS1054->$1;
  _M0L4nameS2614 = _M0L8_2afieldS2879;
  moonbit_incref(_M0L4nameS2614);
  _M0L6_2atmpS2605
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L4nameS2614
  };
  #line 48 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2613
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L6_2atmpS2606 = _M0L6_2atmpS2613;
  _M0L6_2atmpS2609 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS2610 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS2611 = 0;
  _M0L6_2atmpS2612 = 0;
  _M0L6_2atmpS2608 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2608[0] = _M0L6_2atmpS2609;
  _M0L6_2atmpS2608[1] = _M0L6_2atmpS2610;
  _M0L6_2atmpS2608[2] = _M0L6_2atmpS2611;
  _M0L6_2atmpS2608[3] = _M0L6_2atmpS2612;
  _M0L6_2atmpS2607
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2607)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2607->$0 = _M0L6_2atmpS2608;
  _M0L6_2atmpS2607->$1 = 4;
  #line 48 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3235
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2605, _M0L6_2atmpS2606, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS2607);
  if (_tmp_3235.tag) {
    int32_t const _M0L5_2aokS2615 = _tmp_3235.data.ok;
  } else {
    void* const _M0L6_2aerrS2616 = _tmp_3235.data.err;
    struct moonbit_result_0 _result_3236;
    moonbit_decref(_M0L2tcS1054);
    _result_3236.tag = 0;
    _result_3236.data.err = _M0L6_2aerrS2616;
    return _result_3236;
  }
  _M0L8_2afieldS2878 = _M0L2tcS1054->$2;
  _M0L6_2acntS3112 = Moonbit_object_header(_M0L2tcS1054)->rc;
  if (_M0L6_2acntS3112 > 1) {
    int32_t _M0L11_2anew__cntS3115 = _M0L6_2acntS3112 - 1;
    Moonbit_object_header(_M0L2tcS1054)->rc = _M0L11_2anew__cntS3115;
    if (_M0L8_2afieldS2878) {
      moonbit_incref(_M0L8_2afieldS2878);
    }
  } else if (_M0L6_2acntS3112 == 1) {
    moonbit_string_t _M0L8_2afieldS3114 = _M0L2tcS1054->$1;
    moonbit_string_t _M0L8_2afieldS3113;
    moonbit_decref(_M0L8_2afieldS3114);
    _M0L8_2afieldS3113 = _M0L2tcS1054->$0;
    moonbit_decref(_M0L8_2afieldS3113);
    #line 49 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
    moonbit_free(_M0L2tcS1054);
  }
  _M0L9argumentsS2629 = _M0L8_2afieldS2878;
  _M0L6_2atmpS2617
  = (struct _M0TPB6ToJson){
    _M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L9argumentsS2629
  };
  #line 49 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2628
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L6_2atmpS2627 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2627[0] = _M0L6_2atmpS2628;
  _M0L6_2atmpS2626
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS2626)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2626->$0 = _M0L6_2atmpS2627;
  _M0L6_2atmpS2626->$1 = 1;
  #line 49 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2625 = _M0MPC14json4Json5array(_M0L6_2atmpS2626);
  _M0L6_2atmpS2618 = _M0L6_2atmpS2625;
  _M0L6_2atmpS2621 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS2622 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS2623 = 0;
  _M0L6_2atmpS2624 = 0;
  _M0L6_2atmpS2620 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2620[0] = _M0L6_2atmpS2621;
  _M0L6_2atmpS2620[1] = _M0L6_2atmpS2622;
  _M0L6_2atmpS2620[2] = _M0L6_2atmpS2623;
  _M0L6_2atmpS2620[3] = _M0L6_2atmpS2624;
  _M0L6_2atmpS2619
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2619)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2619->$0 = _M0L6_2atmpS2620;
  _M0L6_2atmpS2619->$1 = 4;
  #line 49 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3237
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2617, _M0L6_2atmpS2618, (moonbit_string_t)moonbit_string_literal_20.data, _M0L6_2atmpS2619);
  if (_tmp_3237.tag) {
    int32_t const _M0L5_2aokS2630 = _tmp_3237.data.ok;
  } else {
    void* const _M0L6_2aerrS2631 = _tmp_3237.data.err;
    struct moonbit_result_0 _result_3238;
    _result_3238.tag = 0;
    _result_3238.data.err = _M0L6_2aerrS2631;
    return _result_3238;
  }
  _M0L6_2atmpS2665 = 0;
  #line 50 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L12tc__wo__argsS1055
  = _M0FP38clawteam8clawteam2ai10tool__call((moonbit_string_t)moonbit_string_literal_21.data, (moonbit_string_t)moonbit_string_literal_22.data, _M0L6_2atmpS2665);
  _M0L8_2afieldS2877 = _M0L12tc__wo__argsS1055->$0;
  _M0L2idS2641 = _M0L8_2afieldS2877;
  moonbit_incref(_M0L2idS2641);
  _M0L6_2atmpS2632
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L2idS2641
  };
  #line 51 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2640
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_21.data);
  _M0L6_2atmpS2633 = _M0L6_2atmpS2640;
  _M0L6_2atmpS2636 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS2637 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS2638 = 0;
  _M0L6_2atmpS2639 = 0;
  _M0L6_2atmpS2635 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2635[0] = _M0L6_2atmpS2636;
  _M0L6_2atmpS2635[1] = _M0L6_2atmpS2637;
  _M0L6_2atmpS2635[2] = _M0L6_2atmpS2638;
  _M0L6_2atmpS2635[3] = _M0L6_2atmpS2639;
  _M0L6_2atmpS2634
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2634)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2634->$0 = _M0L6_2atmpS2635;
  _M0L6_2atmpS2634->$1 = 4;
  #line 51 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3239
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2632, _M0L6_2atmpS2633, (moonbit_string_t)moonbit_string_literal_25.data, _M0L6_2atmpS2634);
  if (_tmp_3239.tag) {
    int32_t const _M0L5_2aokS2642 = _tmp_3239.data.ok;
  } else {
    void* const _M0L6_2aerrS2643 = _tmp_3239.data.err;
    struct moonbit_result_0 _result_3240;
    moonbit_decref(_M0L12tc__wo__argsS1055);
    _result_3240.tag = 0;
    _result_3240.data.err = _M0L6_2aerrS2643;
    return _result_3240;
  }
  _M0L8_2afieldS2876 = _M0L12tc__wo__argsS1055->$1;
  _M0L4nameS2653 = _M0L8_2afieldS2876;
  moonbit_incref(_M0L4nameS2653);
  _M0L6_2atmpS2644
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L4nameS2653
  };
  #line 52 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2652
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L6_2atmpS2645 = _M0L6_2atmpS2652;
  _M0L6_2atmpS2648 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS2649 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS2650 = 0;
  _M0L6_2atmpS2651 = 0;
  _M0L6_2atmpS2647 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2647[0] = _M0L6_2atmpS2648;
  _M0L6_2atmpS2647[1] = _M0L6_2atmpS2649;
  _M0L6_2atmpS2647[2] = _M0L6_2atmpS2650;
  _M0L6_2atmpS2647[3] = _M0L6_2atmpS2651;
  _M0L6_2atmpS2646
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2646)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2646->$0 = _M0L6_2atmpS2647;
  _M0L6_2atmpS2646->$1 = 4;
  #line 52 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3241
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2644, _M0L6_2atmpS2645, (moonbit_string_t)moonbit_string_literal_28.data, _M0L6_2atmpS2646);
  if (_tmp_3241.tag) {
    int32_t const _M0L5_2aokS2654 = _tmp_3241.data.ok;
  } else {
    void* const _M0L6_2aerrS2655 = _tmp_3241.data.err;
    struct moonbit_result_0 _result_3242;
    moonbit_decref(_M0L12tc__wo__argsS1055);
    _result_3242.tag = 0;
    _result_3242.data.err = _M0L6_2aerrS2655;
    return _result_3242;
  }
  _M0L8_2afieldS2875 = _M0L12tc__wo__argsS1055->$2;
  _M0L6_2acntS3116 = Moonbit_object_header(_M0L12tc__wo__argsS1055)->rc;
  if (_M0L6_2acntS3116 > 1) {
    int32_t _M0L11_2anew__cntS3119 = _M0L6_2acntS3116 - 1;
    Moonbit_object_header(_M0L12tc__wo__argsS1055)->rc
    = _M0L11_2anew__cntS3119;
    if (_M0L8_2afieldS2875) {
      moonbit_incref(_M0L8_2afieldS2875);
    }
  } else if (_M0L6_2acntS3116 == 1) {
    moonbit_string_t _M0L8_2afieldS3118 = _M0L12tc__wo__argsS1055->$1;
    moonbit_string_t _M0L8_2afieldS3117;
    moonbit_decref(_M0L8_2afieldS3118);
    _M0L8_2afieldS3117 = _M0L12tc__wo__argsS1055->$0;
    moonbit_decref(_M0L8_2afieldS3117);
    #line 53 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
    moonbit_free(_M0L12tc__wo__argsS1055);
  }
  _M0L9argumentsS2664 = _M0L8_2afieldS2875;
  _M0L6_2atmpS2656
  = (struct _M0TPB6ToJson){
    _M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L9argumentsS2664
  };
  moonbit_incref(_M0FPC17prelude4null);
  _M0L6_2atmpS2657 = _M0FPC17prelude4null;
  _M0L6_2atmpS2660 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS2661 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS2662 = 0;
  _M0L6_2atmpS2663 = 0;
  _M0L6_2atmpS2659 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2659[0] = _M0L6_2atmpS2660;
  _M0L6_2atmpS2659[1] = _M0L6_2atmpS2661;
  _M0L6_2atmpS2659[2] = _M0L6_2atmpS2662;
  _M0L6_2atmpS2659[3] = _M0L6_2atmpS2663;
  _M0L6_2atmpS2658
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2658)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2658->$0 = _M0L6_2atmpS2659;
  _M0L6_2atmpS2658->$1 = 4;
  #line 53 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2656, _M0L6_2atmpS2657, (moonbit_string_t)moonbit_string_literal_31.data, _M0L6_2atmpS2658);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__1(
  
) {
  struct _M0TP38clawteam8clawteam2ai5Usage* _M0L5usageS1053;
  int32_t _M0L13input__tokensS2546;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2547;
  struct _M0TPB6ToJson _M0L6_2atmpS2536;
  moonbit_string_t _M0L6_2atmpS2545;
  void* _M0L6_2atmpS2544;
  void* _M0L6_2atmpS2537;
  moonbit_string_t _M0L6_2atmpS2540;
  moonbit_string_t _M0L6_2atmpS2541;
  moonbit_string_t _M0L6_2atmpS2542;
  moonbit_string_t _M0L6_2atmpS2543;
  moonbit_string_t* _M0L6_2atmpS2539;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2538;
  struct moonbit_result_0 _tmp_3243;
  int32_t _M0L14output__tokensS2560;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2561;
  struct _M0TPB6ToJson _M0L6_2atmpS2550;
  moonbit_string_t _M0L6_2atmpS2559;
  void* _M0L6_2atmpS2558;
  void* _M0L6_2atmpS2551;
  moonbit_string_t _M0L6_2atmpS2554;
  moonbit_string_t _M0L6_2atmpS2555;
  moonbit_string_t _M0L6_2atmpS2556;
  moonbit_string_t _M0L6_2atmpS2557;
  moonbit_string_t* _M0L6_2atmpS2553;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2552;
  struct moonbit_result_0 _tmp_3245;
  int32_t _M0L13total__tokensS2574;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2575;
  struct _M0TPB6ToJson _M0L6_2atmpS2564;
  moonbit_string_t _M0L6_2atmpS2573;
  void* _M0L6_2atmpS2572;
  void* _M0L6_2atmpS2565;
  moonbit_string_t _M0L6_2atmpS2568;
  moonbit_string_t _M0L6_2atmpS2569;
  moonbit_string_t _M0L6_2atmpS2570;
  moonbit_string_t _M0L6_2atmpS2571;
  moonbit_string_t* _M0L6_2atmpS2567;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2566;
  struct moonbit_result_0 _tmp_3247;
  int64_t _M0L8_2afieldS2881;
  int64_t _M0L19cache__read__tokensS2591;
  struct _M0Y5Int64* _M0L14_2aboxed__selfS2592;
  struct _M0TPB6ToJson _M0L6_2atmpS2578;
  moonbit_string_t _M0L6_2atmpS2590;
  void* _M0L6_2atmpS2589;
  void** _M0L6_2atmpS2588;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2587;
  void* _M0L6_2atmpS2586;
  void* _M0L6_2atmpS2579;
  moonbit_string_t _M0L6_2atmpS2582;
  moonbit_string_t _M0L6_2atmpS2583;
  moonbit_string_t _M0L6_2atmpS2584;
  moonbit_string_t _M0L6_2atmpS2585;
  moonbit_string_t* _M0L6_2atmpS2581;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2580;
  #line 28 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  #line 29 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L5usageS1053
  = _M0FP38clawteam8clawteam2ai5usage(200, 400, 4294967296ll, 150ll);
  _M0L13input__tokensS2546 = _M0L5usageS1053->$0;
  _M0L14_2aboxed__selfS2547
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2547)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2547->$0 = _M0L13input__tokensS2546;
  _M0L6_2atmpS2536
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2547
  };
  _M0L6_2atmpS2545 = 0;
  #line 34 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2544 = _M0MPC14json4Json6number(0x1.9p+7, _M0L6_2atmpS2545);
  _M0L6_2atmpS2537 = _M0L6_2atmpS2544;
  _M0L6_2atmpS2540 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS2541 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS2542 = 0;
  _M0L6_2atmpS2543 = 0;
  _M0L6_2atmpS2539 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2539[0] = _M0L6_2atmpS2540;
  _M0L6_2atmpS2539[1] = _M0L6_2atmpS2541;
  _M0L6_2atmpS2539[2] = _M0L6_2atmpS2542;
  _M0L6_2atmpS2539[3] = _M0L6_2atmpS2543;
  _M0L6_2atmpS2538
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2538)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2538->$0 = _M0L6_2atmpS2539;
  _M0L6_2atmpS2538->$1 = 4;
  #line 34 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3243
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2536, _M0L6_2atmpS2537, (moonbit_string_t)moonbit_string_literal_34.data, _M0L6_2atmpS2538);
  if (_tmp_3243.tag) {
    int32_t const _M0L5_2aokS2548 = _tmp_3243.data.ok;
  } else {
    void* const _M0L6_2aerrS2549 = _tmp_3243.data.err;
    struct moonbit_result_0 _result_3244;
    moonbit_decref(_M0L5usageS1053);
    _result_3244.tag = 0;
    _result_3244.data.err = _M0L6_2aerrS2549;
    return _result_3244;
  }
  _M0L14output__tokensS2560 = _M0L5usageS1053->$1;
  _M0L14_2aboxed__selfS2561
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2561)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2561->$0 = _M0L14output__tokensS2560;
  _M0L6_2atmpS2550
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2561
  };
  _M0L6_2atmpS2559 = 0;
  #line 35 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2558 = _M0MPC14json4Json6number(0x1.9p+8, _M0L6_2atmpS2559);
  _M0L6_2atmpS2551 = _M0L6_2atmpS2558;
  _M0L6_2atmpS2554 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS2555 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS2556 = 0;
  _M0L6_2atmpS2557 = 0;
  _M0L6_2atmpS2553 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2553[0] = _M0L6_2atmpS2554;
  _M0L6_2atmpS2553[1] = _M0L6_2atmpS2555;
  _M0L6_2atmpS2553[2] = _M0L6_2atmpS2556;
  _M0L6_2atmpS2553[3] = _M0L6_2atmpS2557;
  _M0L6_2atmpS2552
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2552)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2552->$0 = _M0L6_2atmpS2553;
  _M0L6_2atmpS2552->$1 = 4;
  #line 35 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3245
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2550, _M0L6_2atmpS2551, (moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS2552);
  if (_tmp_3245.tag) {
    int32_t const _M0L5_2aokS2562 = _tmp_3245.data.ok;
  } else {
    void* const _M0L6_2aerrS2563 = _tmp_3245.data.err;
    struct moonbit_result_0 _result_3246;
    moonbit_decref(_M0L5usageS1053);
    _result_3246.tag = 0;
    _result_3246.data.err = _M0L6_2aerrS2563;
    return _result_3246;
  }
  _M0L13total__tokensS2574 = _M0L5usageS1053->$2;
  _M0L14_2aboxed__selfS2575
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2575)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2575->$0 = _M0L13total__tokensS2574;
  _M0L6_2atmpS2564
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2575
  };
  _M0L6_2atmpS2573 = 0;
  #line 36 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2572 = _M0MPC14json4Json6number(0x1.2cp+9, _M0L6_2atmpS2573);
  _M0L6_2atmpS2565 = _M0L6_2atmpS2572;
  _M0L6_2atmpS2568 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS2569 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS2570 = 0;
  _M0L6_2atmpS2571 = 0;
  _M0L6_2atmpS2567 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2567[0] = _M0L6_2atmpS2568;
  _M0L6_2atmpS2567[1] = _M0L6_2atmpS2569;
  _M0L6_2atmpS2567[2] = _M0L6_2atmpS2570;
  _M0L6_2atmpS2567[3] = _M0L6_2atmpS2571;
  _M0L6_2atmpS2566
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2566)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2566->$0 = _M0L6_2atmpS2567;
  _M0L6_2atmpS2566->$1 = 4;
  #line 36 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3247
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2564, _M0L6_2atmpS2565, (moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS2566);
  if (_tmp_3247.tag) {
    int32_t const _M0L5_2aokS2576 = _tmp_3247.data.ok;
  } else {
    void* const _M0L6_2aerrS2577 = _tmp_3247.data.err;
    struct moonbit_result_0 _result_3248;
    moonbit_decref(_M0L5usageS1053);
    _result_3248.tag = 0;
    _result_3248.data.err = _M0L6_2aerrS2577;
    return _result_3248;
  }
  _M0L8_2afieldS2881 = _M0L5usageS1053->$3;
  moonbit_decref(_M0L5usageS1053);
  _M0L19cache__read__tokensS2591 = _M0L8_2afieldS2881;
  _M0L14_2aboxed__selfS2592
  = (struct _M0Y5Int64*)moonbit_malloc(sizeof(struct _M0Y5Int64));
  Moonbit_object_header(_M0L14_2aboxed__selfS2592)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y5Int64) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2592->$0 = _M0L19cache__read__tokensS2591;
  _M0L6_2atmpS2578
  = (struct _M0TPB6ToJson){
    _M0FP0120moonbitlang_2fcore_2foption_2fOption_5bInt_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2592
  };
  _M0L6_2atmpS2590 = 0;
  #line 37 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2589 = _M0MPC14json4Json6number(0x1.2cp+7, _M0L6_2atmpS2590);
  _M0L6_2atmpS2588 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2588[0] = _M0L6_2atmpS2589;
  _M0L6_2atmpS2587
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS2587)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2587->$0 = _M0L6_2atmpS2588;
  _M0L6_2atmpS2587->$1 = 1;
  #line 37 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2586 = _M0MPC14json4Json5array(_M0L6_2atmpS2587);
  _M0L6_2atmpS2579 = _M0L6_2atmpS2586;
  _M0L6_2atmpS2582 = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS2583 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS2584 = 0;
  _M0L6_2atmpS2585 = 0;
  _M0L6_2atmpS2581 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2581[0] = _M0L6_2atmpS2582;
  _M0L6_2atmpS2581[1] = _M0L6_2atmpS2583;
  _M0L6_2atmpS2581[2] = _M0L6_2atmpS2584;
  _M0L6_2atmpS2581[3] = _M0L6_2atmpS2585;
  _M0L6_2atmpS2580
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2580)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2580->$0 = _M0L6_2atmpS2581;
  _M0L6_2atmpS2580->$1 = 4;
  #line 37 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2578, _M0L6_2atmpS2579, (moonbit_string_t)moonbit_string_literal_43.data, _M0L6_2atmpS2580);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam18ai__blackbox__test45____test__6d6573736167655f746573742e6d6274__0(
  
) {
  void* _M0L6_2atmpS2492;
  moonbit_string_t _M0L6_2atmpS2491;
  struct _M0TPB6ToJson _M0L6_2atmpS2482;
  void* _M0L6_2atmpS2490;
  void* _M0L6_2atmpS2483;
  moonbit_string_t _M0L6_2atmpS2486;
  moonbit_string_t _M0L6_2atmpS2487;
  moonbit_string_t _M0L6_2atmpS2488;
  moonbit_string_t _M0L6_2atmpS2489;
  moonbit_string_t* _M0L6_2atmpS2485;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2484;
  struct moonbit_result_0 _tmp_3249;
  void* _M0L6_2atmpS2505;
  moonbit_string_t _M0L6_2atmpS2504;
  struct _M0TPB6ToJson _M0L6_2atmpS2495;
  void* _M0L6_2atmpS2503;
  void* _M0L6_2atmpS2496;
  moonbit_string_t _M0L6_2atmpS2499;
  moonbit_string_t _M0L6_2atmpS2500;
  moonbit_string_t _M0L6_2atmpS2501;
  moonbit_string_t _M0L6_2atmpS2502;
  moonbit_string_t* _M0L6_2atmpS2498;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2497;
  struct moonbit_result_0 _tmp_3251;
  moonbit_string_t _M0L6_2atmpS2522;
  struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0L6_2atmpS2521;
  struct _M0TP38clawteam8clawteam2ai8ToolCall** _M0L6_2atmpS2520;
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* _M0L6_2atmpS2519;
  void* _M0L6_2atmpS2518;
  moonbit_string_t _M0L6_2atmpS2517;
  struct _M0TPB6ToJson _M0L6_2atmpS2508;
  void* _M0L6_2atmpS2516;
  void* _M0L6_2atmpS2509;
  moonbit_string_t _M0L6_2atmpS2512;
  moonbit_string_t _M0L6_2atmpS2513;
  moonbit_string_t _M0L6_2atmpS2514;
  moonbit_string_t _M0L6_2atmpS2515;
  moonbit_string_t* _M0L6_2atmpS2511;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2510;
  struct moonbit_result_0 _tmp_3253;
  void* _M0L6_2atmpS2535;
  moonbit_string_t _M0L6_2atmpS2534;
  struct _M0TPB6ToJson _M0L6_2atmpS2525;
  void* _M0L6_2atmpS2533;
  void* _M0L6_2atmpS2526;
  moonbit_string_t _M0L6_2atmpS2529;
  moonbit_string_t _M0L6_2atmpS2530;
  moonbit_string_t _M0L6_2atmpS2531;
  moonbit_string_t _M0L6_2atmpS2532;
  moonbit_string_t* _M0L6_2atmpS2528;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2527;
  #line 2 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  #line 4 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2492
  = _M0FP38clawteam8clawteam2ai13user__message((moonbit_string_t)moonbit_string_literal_44.data);
  #line 4 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2491
  = _M0MP38clawteam8clawteam2ai7Message7content(_M0L6_2atmpS2492);
  _M0L6_2atmpS2482
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS2491
  };
  #line 5 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2490
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_44.data);
  _M0L6_2atmpS2483 = _M0L6_2atmpS2490;
  _M0L6_2atmpS2486 = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS2487 = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS2488 = 0;
  _M0L6_2atmpS2489 = 0;
  _M0L6_2atmpS2485 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2485[0] = _M0L6_2atmpS2486;
  _M0L6_2atmpS2485[1] = _M0L6_2atmpS2487;
  _M0L6_2atmpS2485[2] = _M0L6_2atmpS2488;
  _M0L6_2atmpS2485[3] = _M0L6_2atmpS2489;
  _M0L6_2atmpS2484
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2484)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2484->$0 = _M0L6_2atmpS2485;
  _M0L6_2atmpS2484->$1 = 4;
  #line 3 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3249
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2482, _M0L6_2atmpS2483, (moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS2484);
  if (_tmp_3249.tag) {
    int32_t const _M0L5_2aokS2493 = _tmp_3249.data.ok;
  } else {
    void* const _M0L6_2aerrS2494 = _tmp_3249.data.err;
    struct moonbit_result_0 _result_3250;
    _result_3250.tag = 0;
    _result_3250.data.err = _M0L6_2aerrS2494;
    return _result_3250;
  }
  #line 8 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2505
  = _M0FP38clawteam8clawteam2ai15system__message((moonbit_string_t)moonbit_string_literal_48.data);
  #line 8 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2504
  = _M0MP38clawteam8clawteam2ai7Message7content(_M0L6_2atmpS2505);
  _M0L6_2atmpS2495
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS2504
  };
  #line 9 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2503
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_48.data);
  _M0L6_2atmpS2496 = _M0L6_2atmpS2503;
  _M0L6_2atmpS2499 = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS2500 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS2501 = 0;
  _M0L6_2atmpS2502 = 0;
  _M0L6_2atmpS2498 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2498[0] = _M0L6_2atmpS2499;
  _M0L6_2atmpS2498[1] = _M0L6_2atmpS2500;
  _M0L6_2atmpS2498[2] = _M0L6_2atmpS2501;
  _M0L6_2atmpS2498[3] = _M0L6_2atmpS2502;
  _M0L6_2atmpS2497
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2497)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2497->$0 = _M0L6_2atmpS2498;
  _M0L6_2atmpS2497->$1 = 4;
  #line 7 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3251
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2495, _M0L6_2atmpS2496, (moonbit_string_t)moonbit_string_literal_51.data, _M0L6_2atmpS2497);
  if (_tmp_3251.tag) {
    int32_t const _M0L5_2aokS2506 = _tmp_3251.data.ok;
  } else {
    void* const _M0L6_2aerrS2507 = _tmp_3251.data.err;
    struct moonbit_result_0 _result_3252;
    _result_3252.tag = 0;
    _result_3252.data.err = _M0L6_2aerrS2507;
    return _result_3252;
  }
  _M0L6_2atmpS2522 = (moonbit_string_t)moonbit_string_literal_52.data;
  #line 13 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2521
  = _M0FP38clawteam8clawteam2ai10tool__call((moonbit_string_t)moonbit_string_literal_53.data, (moonbit_string_t)moonbit_string_literal_54.data, _M0L6_2atmpS2522);
  _M0L6_2atmpS2520
  = (struct _M0TP38clawteam8clawteam2ai8ToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2520[0] = _M0L6_2atmpS2521;
  _M0L6_2atmpS2519
  = (struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE));
  Moonbit_object_header(_M0L6_2atmpS2519)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2519->$0 = _M0L6_2atmpS2520;
  _M0L6_2atmpS2519->$1 = 1;
  #line 12 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2518
  = _M0FP38clawteam8clawteam2ai26assistant__message_2einner((moonbit_string_t)moonbit_string_literal_55.data, _M0L6_2atmpS2519);
  #line 12 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2517
  = _M0MP38clawteam8clawteam2ai7Message7content(_M0L6_2atmpS2518);
  _M0L6_2atmpS2508
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS2517
  };
  #line 19 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2516
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_55.data);
  _M0L6_2atmpS2509 = _M0L6_2atmpS2516;
  _M0L6_2atmpS2512 = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS2513 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS2514 = 0;
  _M0L6_2atmpS2515 = 0;
  _M0L6_2atmpS2511 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2511[0] = _M0L6_2atmpS2512;
  _M0L6_2atmpS2511[1] = _M0L6_2atmpS2513;
  _M0L6_2atmpS2511[2] = _M0L6_2atmpS2514;
  _M0L6_2atmpS2511[3] = _M0L6_2atmpS2515;
  _M0L6_2atmpS2510
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2510)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2510->$0 = _M0L6_2atmpS2511;
  _M0L6_2atmpS2510->$1 = 4;
  #line 11 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _tmp_3253
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2508, _M0L6_2atmpS2509, (moonbit_string_t)moonbit_string_literal_58.data, _M0L6_2atmpS2510);
  if (_tmp_3253.tag) {
    int32_t const _M0L5_2aokS2523 = _tmp_3253.data.ok;
  } else {
    void* const _M0L6_2aerrS2524 = _tmp_3253.data.err;
    struct moonbit_result_0 _result_3254;
    _result_3254.tag = 0;
    _result_3254.data.err = _M0L6_2aerrS2524;
    return _result_3254;
  }
  #line 22 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2535
  = _M0FP38clawteam8clawteam2ai13tool__message((moonbit_string_t)moonbit_string_literal_59.data, (moonbit_string_t)moonbit_string_literal_53.data);
  #line 22 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2534
  = _M0MP38clawteam8clawteam2ai7Message7content(_M0L6_2atmpS2535);
  _M0L6_2atmpS2525
  = (struct _M0TPB6ToJson){
    _M0FP081String_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS2534
  };
  #line 23 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  _M0L6_2atmpS2533
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_59.data);
  _M0L6_2atmpS2526 = _M0L6_2atmpS2533;
  _M0L6_2atmpS2529 = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS2530 = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS2531 = 0;
  _M0L6_2atmpS2532 = 0;
  _M0L6_2atmpS2528 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2528[0] = _M0L6_2atmpS2529;
  _M0L6_2atmpS2528[1] = _M0L6_2atmpS2530;
  _M0L6_2atmpS2528[2] = _M0L6_2atmpS2531;
  _M0L6_2atmpS2528[3] = _M0L6_2atmpS2532;
  _M0L6_2atmpS2527
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2527)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2527->$0 = _M0L6_2atmpS2528;
  _M0L6_2atmpS2527->$1 = 4;
  #line 21 "E:\\moonbit\\clawteam\\ai\\message_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2525, _M0L6_2atmpS2526, (moonbit_string_t)moonbit_string_literal_62.data, _M0L6_2atmpS2527);
}

struct _M0TP38clawteam8clawteam2ai5Usage* _M0FP38clawteam8clawteam2ai5usage(
  int32_t _M0L13input__tokensS1050,
  int32_t _M0L14output__tokensS1051,
  int64_t _M0L19total__tokens_2eoptS1048,
  int64_t _M0L19cache__read__tokensS1052
) {
  int32_t _M0L13total__tokensS1047;
  if (_M0L19total__tokens_2eoptS1048 == 4294967296ll) {
    _M0L13total__tokensS1047
    = _M0L13input__tokensS1050 + _M0L14output__tokensS1051;
  } else {
    int64_t _M0L7_2aSomeS1049 = _M0L19total__tokens_2eoptS1048;
    _M0L13total__tokensS1047 = (int32_t)_M0L7_2aSomeS1049;
  }
  return _M0FP38clawteam8clawteam2ai13usage_2einner(_M0L13input__tokensS1050, _M0L14output__tokensS1051, _M0L13total__tokensS1047, _M0L19cache__read__tokensS1052);
}

struct _M0TP38clawteam8clawteam2ai5Usage* _M0FP38clawteam8clawteam2ai13usage_2einner(
  int32_t _M0L13input__tokensS1043,
  int32_t _M0L14output__tokensS1044,
  int32_t _M0L13total__tokensS1045,
  int64_t _M0L19cache__read__tokensS1046
) {
  struct _M0TP38clawteam8clawteam2ai5Usage* _block_3255;
  #line 63 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3255
  = (struct _M0TP38clawteam8clawteam2ai5Usage*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam2ai5Usage));
  Moonbit_object_header(_block_3255)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP38clawteam8clawteam2ai5Usage) >> 2, 0, 0);
  _block_3255->$0 = _M0L13input__tokensS1043;
  _block_3255->$1 = _M0L14output__tokensS1044;
  _block_3255->$2 = _M0L13total__tokensS1045;
  _block_3255->$3 = _M0L19cache__read__tokensS1046;
  return _block_3255;
}

struct _M0TP38clawteam8clawteam2ai8ToolCall* _M0FP38clawteam8clawteam2ai10tool__call(
  moonbit_string_t _M0L2idS1040,
  moonbit_string_t _M0L4nameS1041,
  moonbit_string_t _M0L9argumentsS1042
) {
  struct _M0TP38clawteam8clawteam2ai8ToolCall* _block_3256;
  #line 50 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3256
  = (struct _M0TP38clawteam8clawteam2ai8ToolCall*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam2ai8ToolCall));
  Moonbit_object_header(_block_3256)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam2ai8ToolCall, $0) >> 2, 3, 0);
  _block_3256->$0 = _M0L2idS1040;
  _block_3256->$1 = _M0L4nameS1041;
  _block_3256->$2 = _M0L9argumentsS1042;
  return _block_3256;
}

moonbit_string_t _M0MP38clawteam8clawteam2ai7Message7content(
  void* _M0L4selfS1031
) {
  moonbit_string_t _M0L7contentS1024;
  moonbit_string_t _M0L7contentS1026;
  moonbit_string_t _M0L7contentS1028;
  moonbit_string_t _M0L7contentS1030;
  #line 33 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  switch (Moonbit_object_tag(_M0L4selfS1031)) {
    case 0: {
      struct _M0DTP38clawteam8clawteam2ai7Message4User* _M0L7_2aUserS1032 =
        (struct _M0DTP38clawteam8clawteam2ai7Message4User*)_M0L4selfS1031;
      moonbit_string_t _M0L8_2afieldS2882 = _M0L7_2aUserS1032->$0;
      int32_t _M0L6_2acntS3120 = Moonbit_object_header(_M0L7_2aUserS1032)->rc;
      moonbit_string_t _M0L10_2acontentS1033;
      if (_M0L6_2acntS3120 > 1) {
        int32_t _M0L11_2anew__cntS3121 = _M0L6_2acntS3120 - 1;
        Moonbit_object_header(_M0L7_2aUserS1032)->rc = _M0L11_2anew__cntS3121;
        moonbit_incref(_M0L8_2afieldS2882);
      } else if (_M0L6_2acntS3120 == 1) {
        #line 34 "E:\\moonbit\\clawteam\\ai\\message.mbt"
        moonbit_free(_M0L7_2aUserS1032);
      }
      _M0L10_2acontentS1033 = _M0L8_2afieldS2882;
      _M0L7contentS1030 = _M0L10_2acontentS1033;
      goto join_1029;
      break;
    }
    
    case 1: {
      struct _M0DTP38clawteam8clawteam2ai7Message6System* _M0L9_2aSystemS1034 =
        (struct _M0DTP38clawteam8clawteam2ai7Message6System*)_M0L4selfS1031;
      moonbit_string_t _M0L8_2afieldS2883 = _M0L9_2aSystemS1034->$0;
      int32_t _M0L6_2acntS3122 =
        Moonbit_object_header(_M0L9_2aSystemS1034)->rc;
      moonbit_string_t _M0L10_2acontentS1035;
      if (_M0L6_2acntS3122 > 1) {
        int32_t _M0L11_2anew__cntS3123 = _M0L6_2acntS3122 - 1;
        Moonbit_object_header(_M0L9_2aSystemS1034)->rc
        = _M0L11_2anew__cntS3123;
        moonbit_incref(_M0L8_2afieldS2883);
      } else if (_M0L6_2acntS3122 == 1) {
        #line 34 "E:\\moonbit\\clawteam\\ai\\message.mbt"
        moonbit_free(_M0L9_2aSystemS1034);
      }
      _M0L10_2acontentS1035 = _M0L8_2afieldS2883;
      _M0L7contentS1028 = _M0L10_2acontentS1035;
      goto join_1027;
      break;
    }
    
    case 2: {
      struct _M0DTP38clawteam8clawteam2ai7Message9Assistant* _M0L12_2aAssistantS1036 =
        (struct _M0DTP38clawteam8clawteam2ai7Message9Assistant*)_M0L4selfS1031;
      moonbit_string_t _M0L8_2afieldS2884 = _M0L12_2aAssistantS1036->$0;
      int32_t _M0L6_2acntS3124 =
        Moonbit_object_header(_M0L12_2aAssistantS1036)->rc;
      moonbit_string_t _M0L10_2acontentS1037;
      if (_M0L6_2acntS3124 > 1) {
        int32_t _M0L11_2anew__cntS3126 = _M0L6_2acntS3124 - 1;
        Moonbit_object_header(_M0L12_2aAssistantS1036)->rc
        = _M0L11_2anew__cntS3126;
        moonbit_incref(_M0L8_2afieldS2884);
      } else if (_M0L6_2acntS3124 == 1) {
        struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* _M0L8_2afieldS3125 =
          _M0L12_2aAssistantS1036->$1;
        moonbit_decref(_M0L8_2afieldS3125);
        #line 34 "E:\\moonbit\\clawteam\\ai\\message.mbt"
        moonbit_free(_M0L12_2aAssistantS1036);
      }
      _M0L10_2acontentS1037 = _M0L8_2afieldS2884;
      _M0L7contentS1026 = _M0L10_2acontentS1037;
      goto join_1025;
      break;
    }
    default: {
      struct _M0DTP38clawteam8clawteam2ai7Message4Tool* _M0L7_2aToolS1038 =
        (struct _M0DTP38clawteam8clawteam2ai7Message4Tool*)_M0L4selfS1031;
      moonbit_string_t _M0L8_2afieldS2885 = _M0L7_2aToolS1038->$0;
      int32_t _M0L6_2acntS3127 = Moonbit_object_header(_M0L7_2aToolS1038)->rc;
      moonbit_string_t _M0L10_2acontentS1039;
      if (_M0L6_2acntS3127 > 1) {
        int32_t _M0L11_2anew__cntS3129 = _M0L6_2acntS3127 - 1;
        Moonbit_object_header(_M0L7_2aToolS1038)->rc = _M0L11_2anew__cntS3129;
        moonbit_incref(_M0L8_2afieldS2885);
      } else if (_M0L6_2acntS3127 == 1) {
        moonbit_string_t _M0L8_2afieldS3128 = _M0L7_2aToolS1038->$1;
        moonbit_decref(_M0L8_2afieldS3128);
        #line 34 "E:\\moonbit\\clawteam\\ai\\message.mbt"
        moonbit_free(_M0L7_2aToolS1038);
      }
      _M0L10_2acontentS1039 = _M0L8_2afieldS2885;
      _M0L7contentS1024 = _M0L10_2acontentS1039;
      goto join_1023;
      break;
    }
  }
  join_1029:;
  return _M0L7contentS1030;
  join_1027:;
  return _M0L7contentS1028;
  join_1025:;
  return _M0L7contentS1026;
  join_1023:;
  return _M0L7contentS1024;
}

void* _M0FP38clawteam8clawteam2ai13tool__message(
  moonbit_string_t _M0L7contentS1021,
  moonbit_string_t _M0L14tool__call__idS1022
) {
  void* _block_3261;
  #line 28 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3261
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam2ai7Message4Tool));
  Moonbit_object_header(_block_3261)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam2ai7Message4Tool, $0) >> 2, 2, 3);
  ((struct _M0DTP38clawteam8clawteam2ai7Message4Tool*)_block_3261)->$0
  = _M0L7contentS1021;
  ((struct _M0DTP38clawteam8clawteam2ai7Message4Tool*)_block_3261)->$1
  = _M0L14tool__call__idS1022;
  return _block_3261;
}

void* _M0FP38clawteam8clawteam2ai26assistant__message_2einner(
  moonbit_string_t _M0L7contentS1019,
  struct _M0TPB5ArrayGRP38clawteam8clawteam2ai8ToolCallE* _M0L11tool__callsS1020
) {
  void* _block_3262;
  #line 20 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3262
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam2ai7Message9Assistant));
  Moonbit_object_header(_block_3262)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam2ai7Message9Assistant, $0) >> 2, 2, 2);
  ((struct _M0DTP38clawteam8clawteam2ai7Message9Assistant*)_block_3262)->$0
  = _M0L7contentS1019;
  ((struct _M0DTP38clawteam8clawteam2ai7Message9Assistant*)_block_3262)->$1
  = _M0L11tool__callsS1020;
  return _block_3262;
}

void* _M0FP38clawteam8clawteam2ai15system__message(
  moonbit_string_t _M0L7contentS1018
) {
  void* _block_3263;
  #line 15 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3263
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam2ai7Message6System));
  Moonbit_object_header(_block_3263)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam2ai7Message6System, $0) >> 2, 1, 1);
  ((struct _M0DTP38clawteam8clawteam2ai7Message6System*)_block_3263)->$0
  = _M0L7contentS1018;
  return _block_3263;
}

void* _M0FP38clawteam8clawteam2ai13user__message(
  moonbit_string_t _M0L7contentS1017
) {
  void* _block_3264;
  #line 10 "E:\\moonbit\\clawteam\\ai\\message.mbt"
  _block_3264
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam2ai7Message4User));
  Moonbit_object_header(_block_3264)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam2ai7Message4User, $0) >> 2, 1, 0);
  ((struct _M0DTP38clawteam8clawteam2ai7Message4User*)_block_3264)->$0
  = _M0L7contentS1017;
  return _block_3264;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1012,
  void* _M0L7contentS1014,
  moonbit_string_t _M0L3locS1008,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1010
) {
  moonbit_string_t _M0L3locS1007;
  moonbit_string_t _M0L9args__locS1009;
  void* _M0L6_2atmpS2480;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2481;
  moonbit_string_t _M0L6actualS1011;
  moonbit_string_t _M0L4wantS1013;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1007 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1008);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1009 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1010);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2480 = _M0L3objS1012.$0->$method_0(_M0L3objS1012.$1);
  _M0L6_2atmpS2481 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1011
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2480, 0, 0, _M0L6_2atmpS2481);
  if (_M0L7contentS1014 == 0) {
    void* _M0L6_2atmpS2477;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2478;
    if (_M0L7contentS1014) {
      moonbit_decref(_M0L7contentS1014);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2477
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2478 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1013
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2477, 0, 0, _M0L6_2atmpS2478);
  } else {
    void* _M0L7_2aSomeS1015 = _M0L7contentS1014;
    void* _M0L4_2axS1016 = _M0L7_2aSomeS1015;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2479 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1013
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1016, 0, 0, _M0L6_2atmpS2479);
  }
  moonbit_incref(_M0L4wantS1013);
  moonbit_incref(_M0L6actualS1011);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1011, _M0L4wantS1013)
  ) {
    moonbit_string_t _M0L6_2atmpS2475;
    moonbit_string_t _M0L6_2atmpS2893;
    moonbit_string_t _M0L6_2atmpS2474;
    moonbit_string_t _M0L6_2atmpS2892;
    moonbit_string_t _M0L6_2atmpS2472;
    moonbit_string_t _M0L6_2atmpS2473;
    moonbit_string_t _M0L6_2atmpS2891;
    moonbit_string_t _M0L6_2atmpS2471;
    moonbit_string_t _M0L6_2atmpS2890;
    moonbit_string_t _M0L6_2atmpS2468;
    moonbit_string_t _M0L6_2atmpS2470;
    moonbit_string_t _M0L6_2atmpS2469;
    moonbit_string_t _M0L6_2atmpS2889;
    moonbit_string_t _M0L6_2atmpS2467;
    moonbit_string_t _M0L6_2atmpS2888;
    moonbit_string_t _M0L6_2atmpS2464;
    moonbit_string_t _M0L6_2atmpS2466;
    moonbit_string_t _M0L6_2atmpS2465;
    moonbit_string_t _M0L6_2atmpS2887;
    moonbit_string_t _M0L6_2atmpS2463;
    moonbit_string_t _M0L6_2atmpS2886;
    moonbit_string_t _M0L6_2atmpS2462;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2461;
    struct moonbit_result_0 _result_3265;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2475
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1007);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2893
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_63.data, _M0L6_2atmpS2475);
    moonbit_decref(_M0L6_2atmpS2475);
    _M0L6_2atmpS2474 = _M0L6_2atmpS2893;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2892
    = moonbit_add_string(_M0L6_2atmpS2474, (moonbit_string_t)moonbit_string_literal_64.data);
    moonbit_decref(_M0L6_2atmpS2474);
    _M0L6_2atmpS2472 = _M0L6_2atmpS2892;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2473
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1009);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2891 = moonbit_add_string(_M0L6_2atmpS2472, _M0L6_2atmpS2473);
    moonbit_decref(_M0L6_2atmpS2472);
    moonbit_decref(_M0L6_2atmpS2473);
    _M0L6_2atmpS2471 = _M0L6_2atmpS2891;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2890
    = moonbit_add_string(_M0L6_2atmpS2471, (moonbit_string_t)moonbit_string_literal_65.data);
    moonbit_decref(_M0L6_2atmpS2471);
    _M0L6_2atmpS2468 = _M0L6_2atmpS2890;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2470 = _M0MPC16string6String6escape(_M0L4wantS1013);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2469
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2470);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2889 = moonbit_add_string(_M0L6_2atmpS2468, _M0L6_2atmpS2469);
    moonbit_decref(_M0L6_2atmpS2468);
    moonbit_decref(_M0L6_2atmpS2469);
    _M0L6_2atmpS2467 = _M0L6_2atmpS2889;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2888
    = moonbit_add_string(_M0L6_2atmpS2467, (moonbit_string_t)moonbit_string_literal_66.data);
    moonbit_decref(_M0L6_2atmpS2467);
    _M0L6_2atmpS2464 = _M0L6_2atmpS2888;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2466 = _M0MPC16string6String6escape(_M0L6actualS1011);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2465
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2466);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2887 = moonbit_add_string(_M0L6_2atmpS2464, _M0L6_2atmpS2465);
    moonbit_decref(_M0L6_2atmpS2464);
    moonbit_decref(_M0L6_2atmpS2465);
    _M0L6_2atmpS2463 = _M0L6_2atmpS2887;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2886
    = moonbit_add_string(_M0L6_2atmpS2463, (moonbit_string_t)moonbit_string_literal_67.data);
    moonbit_decref(_M0L6_2atmpS2463);
    _M0L6_2atmpS2462 = _M0L6_2atmpS2886;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2461
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2461)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2461)->$0
    = _M0L6_2atmpS2462;
    _result_3265.tag = 0;
    _result_3265.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2461;
    return _result_3265;
  } else {
    int32_t _M0L6_2atmpS2476;
    struct moonbit_result_0 _result_3266;
    moonbit_decref(_M0L4wantS1013);
    moonbit_decref(_M0L6actualS1011);
    moonbit_decref(_M0L9args__locS1009);
    moonbit_decref(_M0L3locS1007);
    _M0L6_2atmpS2476 = 0;
    _result_3266.tag = 1;
    _result_3266.data.ok = _M0L6_2atmpS2476;
    return _result_3266;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1006,
  int32_t _M0L13escape__slashS978,
  int32_t _M0L6indentS973,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS999
) {
  struct _M0TPB13StringBuilder* _M0L3bufS965;
  void** _M0L6_2atmpS2460;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS966;
  int32_t _M0Lm5depthS967;
  void* _M0L6_2atmpS2459;
  void* _M0L8_2aparamS968;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS965 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2460 = (void**)moonbit_empty_ref_array;
  _M0L5stackS966
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS966)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS966->$0 = _M0L6_2atmpS2460;
  _M0L5stackS966->$1 = 0;
  _M0Lm5depthS967 = 0;
  _M0L6_2atmpS2459 = _M0L4selfS1006;
  _M0L8_2aparamS968 = _M0L6_2atmpS2459;
  _2aloop_984:;
  while (1) {
    if (_M0L8_2aparamS968 == 0) {
      int32_t _M0L3lenS2421;
      if (_M0L8_2aparamS968) {
        moonbit_decref(_M0L8_2aparamS968);
      }
      _M0L3lenS2421 = _M0L5stackS966->$1;
      if (_M0L3lenS2421 == 0) {
        if (_M0L8replacerS999) {
          moonbit_decref(_M0L8replacerS999);
        }
        moonbit_decref(_M0L5stackS966);
        break;
      } else {
        void** _M0L8_2afieldS2901 = _M0L5stackS966->$0;
        void** _M0L3bufS2445 = _M0L8_2afieldS2901;
        int32_t _M0L3lenS2447 = _M0L5stackS966->$1;
        int32_t _M0L6_2atmpS2446 = _M0L3lenS2447 - 1;
        void* _M0L6_2atmpS2900 = (void*)_M0L3bufS2445[_M0L6_2atmpS2446];
        void* _M0L4_2axS985 = _M0L6_2atmpS2900;
        switch (Moonbit_object_tag(_M0L4_2axS985)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS986 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS985;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2896 =
              _M0L8_2aArrayS986->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS987 =
              _M0L8_2afieldS2896;
            int32_t _M0L4_2aiS988 = _M0L8_2aArrayS986->$1;
            int32_t _M0L3lenS2433 = _M0L6_2aarrS987->$1;
            if (_M0L4_2aiS988 < _M0L3lenS2433) {
              int32_t _if__result_3268;
              void** _M0L8_2afieldS2895;
              void** _M0L3bufS2439;
              void* _M0L6_2atmpS2894;
              void* _M0L7elementS989;
              int32_t _M0L6_2atmpS2434;
              void* _M0L6_2atmpS2437;
              if (_M0L4_2aiS988 < 0) {
                _if__result_3268 = 1;
              } else {
                int32_t _M0L3lenS2438 = _M0L6_2aarrS987->$1;
                _if__result_3268 = _M0L4_2aiS988 >= _M0L3lenS2438;
              }
              if (_if__result_3268) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS2895 = _M0L6_2aarrS987->$0;
              _M0L3bufS2439 = _M0L8_2afieldS2895;
              _M0L6_2atmpS2894 = (void*)_M0L3bufS2439[_M0L4_2aiS988];
              _M0L7elementS989 = _M0L6_2atmpS2894;
              _M0L6_2atmpS2434 = _M0L4_2aiS988 + 1;
              _M0L8_2aArrayS986->$1 = _M0L6_2atmpS2434;
              if (_M0L4_2aiS988 > 0) {
                int32_t _M0L6_2atmpS2436;
                moonbit_string_t _M0L6_2atmpS2435;
                moonbit_incref(_M0L7elementS989);
                moonbit_incref(_M0L3bufS965);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 44);
                _M0L6_2atmpS2436 = _M0Lm5depthS967;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2435
                = _M0FPC14json11indent__str(_M0L6_2atmpS2436, _M0L6indentS973);
                moonbit_incref(_M0L3bufS965);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2435);
              } else {
                moonbit_incref(_M0L7elementS989);
              }
              _M0L6_2atmpS2437 = _M0L7elementS989;
              _M0L8_2aparamS968 = _M0L6_2atmpS2437;
              goto _2aloop_984;
            } else {
              int32_t _M0L6_2atmpS2440 = _M0Lm5depthS967;
              void* _M0L6_2atmpS2441;
              int32_t _M0L6_2atmpS2443;
              moonbit_string_t _M0L6_2atmpS2442;
              void* _M0L6_2atmpS2444;
              _M0Lm5depthS967 = _M0L6_2atmpS2440 - 1;
              moonbit_incref(_M0L5stackS966);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2441
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS966);
              if (_M0L6_2atmpS2441) {
                moonbit_decref(_M0L6_2atmpS2441);
              }
              _M0L6_2atmpS2443 = _M0Lm5depthS967;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2442
              = _M0FPC14json11indent__str(_M0L6_2atmpS2443, _M0L6indentS973);
              moonbit_incref(_M0L3bufS965);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2442);
              moonbit_incref(_M0L3bufS965);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 93);
              _M0L6_2atmpS2444 = 0;
              _M0L8_2aparamS968 = _M0L6_2atmpS2444;
              goto _2aloop_984;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS990 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS985;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS2899 =
              _M0L9_2aObjectS990->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS991 =
              _M0L8_2afieldS2899;
            int32_t _M0L8_2afirstS992 = _M0L9_2aObjectS990->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS993;
            moonbit_incref(_M0L11_2aiteratorS991);
            moonbit_incref(_M0L9_2aObjectS990);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS993
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS991);
            if (_M0L7_2abindS993 == 0) {
              int32_t _M0L6_2atmpS2422;
              void* _M0L6_2atmpS2423;
              int32_t _M0L6_2atmpS2425;
              moonbit_string_t _M0L6_2atmpS2424;
              void* _M0L6_2atmpS2426;
              if (_M0L7_2abindS993) {
                moonbit_decref(_M0L7_2abindS993);
              }
              moonbit_decref(_M0L9_2aObjectS990);
              _M0L6_2atmpS2422 = _M0Lm5depthS967;
              _M0Lm5depthS967 = _M0L6_2atmpS2422 - 1;
              moonbit_incref(_M0L5stackS966);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2423
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS966);
              if (_M0L6_2atmpS2423) {
                moonbit_decref(_M0L6_2atmpS2423);
              }
              _M0L6_2atmpS2425 = _M0Lm5depthS967;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2424
              = _M0FPC14json11indent__str(_M0L6_2atmpS2425, _M0L6indentS973);
              moonbit_incref(_M0L3bufS965);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2424);
              moonbit_incref(_M0L3bufS965);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 125);
              _M0L6_2atmpS2426 = 0;
              _M0L8_2aparamS968 = _M0L6_2atmpS2426;
              goto _2aloop_984;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS994 = _M0L7_2abindS993;
              struct _M0TUsRPB4JsonE* _M0L4_2axS995 = _M0L7_2aSomeS994;
              moonbit_string_t _M0L8_2afieldS2898 = _M0L4_2axS995->$0;
              moonbit_string_t _M0L4_2akS996 = _M0L8_2afieldS2898;
              void* _M0L8_2afieldS2897 = _M0L4_2axS995->$1;
              int32_t _M0L6_2acntS3130 =
                Moonbit_object_header(_M0L4_2axS995)->rc;
              void* _M0L4_2avS997;
              void* _M0Lm2v2S998;
              moonbit_string_t _M0L6_2atmpS2430;
              void* _M0L6_2atmpS2432;
              void* _M0L6_2atmpS2431;
              if (_M0L6_2acntS3130 > 1) {
                int32_t _M0L11_2anew__cntS3131 = _M0L6_2acntS3130 - 1;
                Moonbit_object_header(_M0L4_2axS995)->rc
                = _M0L11_2anew__cntS3131;
                moonbit_incref(_M0L8_2afieldS2897);
                moonbit_incref(_M0L4_2akS996);
              } else if (_M0L6_2acntS3130 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS995);
              }
              _M0L4_2avS997 = _M0L8_2afieldS2897;
              _M0Lm2v2S998 = _M0L4_2avS997;
              if (_M0L8replacerS999 == 0) {
                moonbit_incref(_M0Lm2v2S998);
                moonbit_decref(_M0L4_2avS997);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1000 =
                  _M0L8replacerS999;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1001 =
                  _M0L7_2aSomeS1000;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1002 =
                  _M0L11_2areplacerS1001;
                void* _M0L7_2abindS1003;
                moonbit_incref(_M0L7_2afuncS1002);
                moonbit_incref(_M0L4_2akS996);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1003
                = _M0L7_2afuncS1002->code(_M0L7_2afuncS1002, _M0L4_2akS996, _M0L4_2avS997);
                if (_M0L7_2abindS1003 == 0) {
                  void* _M0L6_2atmpS2427;
                  if (_M0L7_2abindS1003) {
                    moonbit_decref(_M0L7_2abindS1003);
                  }
                  moonbit_decref(_M0L4_2akS996);
                  moonbit_decref(_M0L9_2aObjectS990);
                  _M0L6_2atmpS2427 = 0;
                  _M0L8_2aparamS968 = _M0L6_2atmpS2427;
                  goto _2aloop_984;
                } else {
                  void* _M0L7_2aSomeS1004 = _M0L7_2abindS1003;
                  void* _M0L4_2avS1005 = _M0L7_2aSomeS1004;
                  _M0Lm2v2S998 = _M0L4_2avS1005;
                }
              }
              if (!_M0L8_2afirstS992) {
                int32_t _M0L6_2atmpS2429;
                moonbit_string_t _M0L6_2atmpS2428;
                moonbit_incref(_M0L3bufS965);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 44);
                _M0L6_2atmpS2429 = _M0Lm5depthS967;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2428
                = _M0FPC14json11indent__str(_M0L6_2atmpS2429, _M0L6indentS973);
                moonbit_incref(_M0L3bufS965);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2428);
              }
              moonbit_incref(_M0L3bufS965);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2430
              = _M0FPC14json6escape(_M0L4_2akS996, _M0L13escape__slashS978);
              moonbit_incref(_M0L3bufS965);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2430);
              moonbit_incref(_M0L3bufS965);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 34);
              moonbit_incref(_M0L3bufS965);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 58);
              if (_M0L6indentS973 > 0) {
                moonbit_incref(_M0L3bufS965);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 32);
              }
              _M0L9_2aObjectS990->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS990);
              _M0L6_2atmpS2432 = _M0Lm2v2S998;
              _M0L6_2atmpS2431 = _M0L6_2atmpS2432;
              _M0L8_2aparamS968 = _M0L6_2atmpS2431;
              goto _2aloop_984;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS969 = _M0L8_2aparamS968;
      void* _M0L8_2avalueS970 = _M0L7_2aSomeS969;
      void* _M0L6_2atmpS2458;
      switch (Moonbit_object_tag(_M0L8_2avalueS970)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS971 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS970;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS2902 =
            _M0L9_2aObjectS971->$0;
          int32_t _M0L6_2acntS3132 =
            Moonbit_object_header(_M0L9_2aObjectS971)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS972;
          if (_M0L6_2acntS3132 > 1) {
            int32_t _M0L11_2anew__cntS3133 = _M0L6_2acntS3132 - 1;
            Moonbit_object_header(_M0L9_2aObjectS971)->rc
            = _M0L11_2anew__cntS3133;
            moonbit_incref(_M0L8_2afieldS2902);
          } else if (_M0L6_2acntS3132 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS971);
          }
          _M0L10_2amembersS972 = _M0L8_2afieldS2902;
          moonbit_incref(_M0L10_2amembersS972);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS972)) {
            moonbit_decref(_M0L10_2amembersS972);
            moonbit_incref(_M0L3bufS965);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, (moonbit_string_t)moonbit_string_literal_68.data);
          } else {
            int32_t _M0L6_2atmpS2453 = _M0Lm5depthS967;
            int32_t _M0L6_2atmpS2455;
            moonbit_string_t _M0L6_2atmpS2454;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2457;
            void* _M0L6ObjectS2456;
            _M0Lm5depthS967 = _M0L6_2atmpS2453 + 1;
            moonbit_incref(_M0L3bufS965);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 123);
            _M0L6_2atmpS2455 = _M0Lm5depthS967;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2454
            = _M0FPC14json11indent__str(_M0L6_2atmpS2455, _M0L6indentS973);
            moonbit_incref(_M0L3bufS965);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2454);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2457
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS972);
            _M0L6ObjectS2456
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2456)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2456)->$0
            = _M0L6_2atmpS2457;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2456)->$1
            = 1;
            moonbit_incref(_M0L5stackS966);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS966, _M0L6ObjectS2456);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS974 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS970;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2903 =
            _M0L8_2aArrayS974->$0;
          int32_t _M0L6_2acntS3134 =
            Moonbit_object_header(_M0L8_2aArrayS974)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS975;
          if (_M0L6_2acntS3134 > 1) {
            int32_t _M0L11_2anew__cntS3135 = _M0L6_2acntS3134 - 1;
            Moonbit_object_header(_M0L8_2aArrayS974)->rc
            = _M0L11_2anew__cntS3135;
            moonbit_incref(_M0L8_2afieldS2903);
          } else if (_M0L6_2acntS3134 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS974);
          }
          _M0L6_2aarrS975 = _M0L8_2afieldS2903;
          moonbit_incref(_M0L6_2aarrS975);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS975)) {
            moonbit_decref(_M0L6_2aarrS975);
            moonbit_incref(_M0L3bufS965);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, (moonbit_string_t)moonbit_string_literal_69.data);
          } else {
            int32_t _M0L6_2atmpS2449 = _M0Lm5depthS967;
            int32_t _M0L6_2atmpS2451;
            moonbit_string_t _M0L6_2atmpS2450;
            void* _M0L5ArrayS2452;
            _M0Lm5depthS967 = _M0L6_2atmpS2449 + 1;
            moonbit_incref(_M0L3bufS965);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 91);
            _M0L6_2atmpS2451 = _M0Lm5depthS967;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2450
            = _M0FPC14json11indent__str(_M0L6_2atmpS2451, _M0L6indentS973);
            moonbit_incref(_M0L3bufS965);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2450);
            _M0L5ArrayS2452
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2452)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2452)->$0
            = _M0L6_2aarrS975;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2452)->$1
            = 0;
            moonbit_incref(_M0L5stackS966);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS966, _M0L5ArrayS2452);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS976 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS970;
          moonbit_string_t _M0L8_2afieldS2904 = _M0L9_2aStringS976->$0;
          int32_t _M0L6_2acntS3136 =
            Moonbit_object_header(_M0L9_2aStringS976)->rc;
          moonbit_string_t _M0L4_2asS977;
          moonbit_string_t _M0L6_2atmpS2448;
          if (_M0L6_2acntS3136 > 1) {
            int32_t _M0L11_2anew__cntS3137 = _M0L6_2acntS3136 - 1;
            Moonbit_object_header(_M0L9_2aStringS976)->rc
            = _M0L11_2anew__cntS3137;
            moonbit_incref(_M0L8_2afieldS2904);
          } else if (_M0L6_2acntS3136 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS976);
          }
          _M0L4_2asS977 = _M0L8_2afieldS2904;
          moonbit_incref(_M0L3bufS965);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2448
          = _M0FPC14json6escape(_M0L4_2asS977, _M0L13escape__slashS978);
          moonbit_incref(_M0L3bufS965);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L6_2atmpS2448);
          moonbit_incref(_M0L3bufS965);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS965, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS979 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS970;
          double _M0L4_2anS980 = _M0L9_2aNumberS979->$0;
          moonbit_string_t _M0L8_2afieldS2905 = _M0L9_2aNumberS979->$1;
          int32_t _M0L6_2acntS3138 =
            Moonbit_object_header(_M0L9_2aNumberS979)->rc;
          moonbit_string_t _M0L7_2areprS981;
          if (_M0L6_2acntS3138 > 1) {
            int32_t _M0L11_2anew__cntS3139 = _M0L6_2acntS3138 - 1;
            Moonbit_object_header(_M0L9_2aNumberS979)->rc
            = _M0L11_2anew__cntS3139;
            if (_M0L8_2afieldS2905) {
              moonbit_incref(_M0L8_2afieldS2905);
            }
          } else if (_M0L6_2acntS3138 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS979);
          }
          _M0L7_2areprS981 = _M0L8_2afieldS2905;
          if (_M0L7_2areprS981 == 0) {
            if (_M0L7_2areprS981) {
              moonbit_decref(_M0L7_2areprS981);
            }
            moonbit_incref(_M0L3bufS965);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS965, _M0L4_2anS980);
          } else {
            moonbit_string_t _M0L7_2aSomeS982 = _M0L7_2areprS981;
            moonbit_string_t _M0L4_2arS983 = _M0L7_2aSomeS982;
            moonbit_incref(_M0L3bufS965);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, _M0L4_2arS983);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS965);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, (moonbit_string_t)moonbit_string_literal_70.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS965);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, (moonbit_string_t)moonbit_string_literal_71.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS970);
          moonbit_incref(_M0L3bufS965);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS965, (moonbit_string_t)moonbit_string_literal_72.data);
          break;
        }
      }
      _M0L6_2atmpS2458 = 0;
      _M0L8_2aparamS968 = _M0L6_2atmpS2458;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS965);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS964,
  int32_t _M0L6indentS962
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS962 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS963 = _M0L6indentS962 * _M0L5levelS964;
    switch (_M0L6spacesS963) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_73.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_74.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_75.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_76.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_77.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_78.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_79.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_80.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_81.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2420;
        moonbit_string_t _M0L6_2atmpS2906;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2420
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_82.data, _M0L6spacesS963);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2906
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_73.data, _M0L6_2atmpS2420);
        moonbit_decref(_M0L6_2atmpS2420);
        return _M0L6_2atmpS2906;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS954,
  int32_t _M0L13escape__slashS959
) {
  int32_t _M0L6_2atmpS2419;
  struct _M0TPB13StringBuilder* _M0L3bufS953;
  struct _M0TWEOc* _M0L5_2aitS955;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2419 = Moonbit_array_length(_M0L3strS954);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS953 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2419);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS955 = _M0MPC16string6String4iter(_M0L3strS954);
  while (1) {
    int32_t _M0L7_2abindS956;
    moonbit_incref(_M0L5_2aitS955);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS956 = _M0MPB4Iter4nextGcE(_M0L5_2aitS955);
    if (_M0L7_2abindS956 == -1) {
      moonbit_decref(_M0L5_2aitS955);
    } else {
      int32_t _M0L7_2aSomeS957 = _M0L7_2abindS956;
      int32_t _M0L4_2acS958 = _M0L7_2aSomeS957;
      if (_M0L4_2acS958 == 34) {
        moonbit_incref(_M0L3bufS953);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_83.data);
      } else if (_M0L4_2acS958 == 92) {
        moonbit_incref(_M0L3bufS953);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_84.data);
      } else if (_M0L4_2acS958 == 47) {
        if (_M0L13escape__slashS959) {
          moonbit_incref(_M0L3bufS953);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_85.data);
        } else {
          moonbit_incref(_M0L3bufS953);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS953, _M0L4_2acS958);
        }
      } else if (_M0L4_2acS958 == 10) {
        moonbit_incref(_M0L3bufS953);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_86.data);
      } else if (_M0L4_2acS958 == 13) {
        moonbit_incref(_M0L3bufS953);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_87.data);
      } else if (_M0L4_2acS958 == 8) {
        moonbit_incref(_M0L3bufS953);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_88.data);
      } else if (_M0L4_2acS958 == 9) {
        moonbit_incref(_M0L3bufS953);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_89.data);
      } else {
        int32_t _M0L4codeS960 = _M0L4_2acS958;
        if (_M0L4codeS960 == 12) {
          moonbit_incref(_M0L3bufS953);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_90.data);
        } else if (_M0L4codeS960 < 32) {
          int32_t _M0L6_2atmpS2418;
          moonbit_string_t _M0L6_2atmpS2417;
          moonbit_incref(_M0L3bufS953);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, (moonbit_string_t)moonbit_string_literal_91.data);
          _M0L6_2atmpS2418 = _M0L4codeS960 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2417 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2418);
          moonbit_incref(_M0L3bufS953);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS953, _M0L6_2atmpS2417);
        } else {
          moonbit_incref(_M0L3bufS953);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS953, _M0L4_2acS958);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS953);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS952
) {
  int32_t _M0L8_2afieldS2907;
  int32_t _M0L3lenS2416;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS2907 = _M0L4selfS952->$1;
  moonbit_decref(_M0L4selfS952);
  _M0L3lenS2416 = _M0L8_2afieldS2907;
  return _M0L3lenS2416 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS949
) {
  int32_t _M0L3lenS948;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS948 = _M0L4selfS949->$1;
  if (_M0L3lenS948 == 0) {
    moonbit_decref(_M0L4selfS949);
    return 0;
  } else {
    int32_t _M0L5indexS950 = _M0L3lenS948 - 1;
    void** _M0L8_2afieldS2911 = _M0L4selfS949->$0;
    void** _M0L3bufS2415 = _M0L8_2afieldS2911;
    void* _M0L6_2atmpS2910 = (void*)_M0L3bufS2415[_M0L5indexS950];
    void* _M0L1vS951 = _M0L6_2atmpS2910;
    void** _M0L8_2afieldS2909 = _M0L4selfS949->$0;
    void** _M0L3bufS2414 = _M0L8_2afieldS2909;
    void* _M0L6_2aoldS2908;
    if (
      _M0L5indexS950 < 0
      || _M0L5indexS950 >= Moonbit_array_length(_M0L3bufS2414)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS2908 = (void*)_M0L3bufS2414[_M0L5indexS950];
    moonbit_incref(_M0L1vS951);
    moonbit_decref(_M0L6_2aoldS2908);
    if (
      _M0L5indexS950 < 0
      || _M0L5indexS950 >= Moonbit_array_length(_M0L3bufS2414)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2414[_M0L5indexS950]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS949->$1 = _M0L5indexS950;
    moonbit_decref(_M0L4selfS949);
    return _M0L1vS951;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS946,
  struct _M0TPB6Logger _M0L6loggerS947
) {
  moonbit_string_t _M0L6_2atmpS2413;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2412;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2413 = _M0L4selfS946;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2412 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2413);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2412, _M0L6loggerS947);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS923,
  struct _M0TPB6Logger _M0L6loggerS945
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2920;
  struct _M0TPC16string10StringView _M0L3pkgS922;
  moonbit_string_t _M0L7_2adataS924;
  int32_t _M0L8_2astartS925;
  int32_t _M0L6_2atmpS2411;
  int32_t _M0L6_2aendS926;
  int32_t _M0Lm9_2acursorS927;
  int32_t _M0Lm13accept__stateS928;
  int32_t _M0Lm10match__endS929;
  int32_t _M0Lm20match__tag__saver__0S930;
  int32_t _M0Lm6tag__0S931;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS932;
  struct _M0TPC16string10StringView _M0L8_2afieldS2919;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS941;
  void* _M0L8_2afieldS2918;
  int32_t _M0L6_2acntS3140;
  void* _M0L16_2apackage__nameS942;
  struct _M0TPC16string10StringView _M0L8_2afieldS2916;
  struct _M0TPC16string10StringView _M0L8filenameS2388;
  struct _M0TPC16string10StringView _M0L8_2afieldS2915;
  struct _M0TPC16string10StringView _M0L11start__lineS2389;
  struct _M0TPC16string10StringView _M0L8_2afieldS2914;
  struct _M0TPC16string10StringView _M0L13start__columnS2390;
  struct _M0TPC16string10StringView _M0L8_2afieldS2913;
  struct _M0TPC16string10StringView _M0L9end__lineS2391;
  struct _M0TPC16string10StringView _M0L8_2afieldS2912;
  int32_t _M0L6_2acntS3144;
  struct _M0TPC16string10StringView _M0L11end__columnS2392;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS2920
  = (struct _M0TPC16string10StringView){
    _M0L4selfS923->$0_1, _M0L4selfS923->$0_2, _M0L4selfS923->$0_0
  };
  _M0L3pkgS922 = _M0L8_2afieldS2920;
  moonbit_incref(_M0L3pkgS922.$0);
  moonbit_incref(_M0L3pkgS922.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS924 = _M0MPC16string10StringView4data(_M0L3pkgS922);
  moonbit_incref(_M0L3pkgS922.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS925 = _M0MPC16string10StringView13start__offset(_M0L3pkgS922);
  moonbit_incref(_M0L3pkgS922.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2411 = _M0MPC16string10StringView6length(_M0L3pkgS922);
  _M0L6_2aendS926 = _M0L8_2astartS925 + _M0L6_2atmpS2411;
  _M0Lm9_2acursorS927 = _M0L8_2astartS925;
  _M0Lm13accept__stateS928 = -1;
  _M0Lm10match__endS929 = -1;
  _M0Lm20match__tag__saver__0S930 = -1;
  _M0Lm6tag__0S931 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2403 = _M0Lm9_2acursorS927;
    if (_M0L6_2atmpS2403 < _M0L6_2aendS926) {
      int32_t _M0L6_2atmpS2410 = _M0Lm9_2acursorS927;
      int32_t _M0L10next__charS936;
      int32_t _M0L6_2atmpS2404;
      moonbit_incref(_M0L7_2adataS924);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS936
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS924, _M0L6_2atmpS2410);
      _M0L6_2atmpS2404 = _M0Lm9_2acursorS927;
      _M0Lm9_2acursorS927 = _M0L6_2atmpS2404 + 1;
      if (_M0L10next__charS936 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2405;
          _M0Lm6tag__0S931 = _M0Lm9_2acursorS927;
          _M0L6_2atmpS2405 = _M0Lm9_2acursorS927;
          if (_M0L6_2atmpS2405 < _M0L6_2aendS926) {
            int32_t _M0L6_2atmpS2409 = _M0Lm9_2acursorS927;
            int32_t _M0L10next__charS937;
            int32_t _M0L6_2atmpS2406;
            moonbit_incref(_M0L7_2adataS924);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS937
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS924, _M0L6_2atmpS2409);
            _M0L6_2atmpS2406 = _M0Lm9_2acursorS927;
            _M0Lm9_2acursorS927 = _M0L6_2atmpS2406 + 1;
            if (_M0L10next__charS937 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2407 = _M0Lm9_2acursorS927;
                if (_M0L6_2atmpS2407 < _M0L6_2aendS926) {
                  int32_t _M0L6_2atmpS2408 = _M0Lm9_2acursorS927;
                  _M0Lm9_2acursorS927 = _M0L6_2atmpS2408 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S930 = _M0Lm6tag__0S931;
                  _M0Lm13accept__stateS928 = 0;
                  _M0Lm10match__endS929 = _M0Lm9_2acursorS927;
                  goto join_933;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_933;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_933;
    }
    break;
  }
  goto joinlet_3270;
  join_933:;
  switch (_M0Lm13accept__stateS928) {
    case 0: {
      int32_t _M0L6_2atmpS2401;
      int32_t _M0L6_2atmpS2400;
      int64_t _M0L6_2atmpS2397;
      int32_t _M0L6_2atmpS2399;
      int64_t _M0L6_2atmpS2398;
      struct _M0TPC16string10StringView _M0L13package__nameS934;
      int64_t _M0L6_2atmpS2394;
      int32_t _M0L6_2atmpS2396;
      int64_t _M0L6_2atmpS2395;
      struct _M0TPC16string10StringView _M0L12module__nameS935;
      void* _M0L4SomeS2393;
      moonbit_decref(_M0L3pkgS922.$0);
      _M0L6_2atmpS2401 = _M0Lm20match__tag__saver__0S930;
      _M0L6_2atmpS2400 = _M0L6_2atmpS2401 + 1;
      _M0L6_2atmpS2397 = (int64_t)_M0L6_2atmpS2400;
      _M0L6_2atmpS2399 = _M0Lm10match__endS929;
      _M0L6_2atmpS2398 = (int64_t)_M0L6_2atmpS2399;
      moonbit_incref(_M0L7_2adataS924);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS934
      = _M0MPC16string6String4view(_M0L7_2adataS924, _M0L6_2atmpS2397, _M0L6_2atmpS2398);
      _M0L6_2atmpS2394 = (int64_t)_M0L8_2astartS925;
      _M0L6_2atmpS2396 = _M0Lm20match__tag__saver__0S930;
      _M0L6_2atmpS2395 = (int64_t)_M0L6_2atmpS2396;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS935
      = _M0MPC16string6String4view(_M0L7_2adataS924, _M0L6_2atmpS2394, _M0L6_2atmpS2395);
      _M0L4SomeS2393
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2393)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2393)->$0_0
      = _M0L13package__nameS934.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2393)->$0_1
      = _M0L13package__nameS934.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2393)->$0_2
      = _M0L13package__nameS934.$2;
      _M0L7_2abindS932
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS932)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS932->$0_0 = _M0L12module__nameS935.$0;
      _M0L7_2abindS932->$0_1 = _M0L12module__nameS935.$1;
      _M0L7_2abindS932->$0_2 = _M0L12module__nameS935.$2;
      _M0L7_2abindS932->$1 = _M0L4SomeS2393;
      break;
    }
    default: {
      void* _M0L4NoneS2402;
      moonbit_decref(_M0L7_2adataS924);
      _M0L4NoneS2402
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS932
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS932)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS932->$0_0 = _M0L3pkgS922.$0;
      _M0L7_2abindS932->$0_1 = _M0L3pkgS922.$1;
      _M0L7_2abindS932->$0_2 = _M0L3pkgS922.$2;
      _M0L7_2abindS932->$1 = _M0L4NoneS2402;
      break;
    }
  }
  joinlet_3270:;
  _M0L8_2afieldS2919
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS932->$0_1, _M0L7_2abindS932->$0_2, _M0L7_2abindS932->$0_0
  };
  _M0L15_2amodule__nameS941 = _M0L8_2afieldS2919;
  _M0L8_2afieldS2918 = _M0L7_2abindS932->$1;
  _M0L6_2acntS3140 = Moonbit_object_header(_M0L7_2abindS932)->rc;
  if (_M0L6_2acntS3140 > 1) {
    int32_t _M0L11_2anew__cntS3141 = _M0L6_2acntS3140 - 1;
    Moonbit_object_header(_M0L7_2abindS932)->rc = _M0L11_2anew__cntS3141;
    moonbit_incref(_M0L8_2afieldS2918);
    moonbit_incref(_M0L15_2amodule__nameS941.$0);
  } else if (_M0L6_2acntS3140 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS932);
  }
  _M0L16_2apackage__nameS942 = _M0L8_2afieldS2918;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS942)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS943 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS942;
      struct _M0TPC16string10StringView _M0L8_2afieldS2917 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS943->$0_1,
                                              _M0L7_2aSomeS943->$0_2,
                                              _M0L7_2aSomeS943->$0_0};
      int32_t _M0L6_2acntS3142 = Moonbit_object_header(_M0L7_2aSomeS943)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS944;
      if (_M0L6_2acntS3142 > 1) {
        int32_t _M0L11_2anew__cntS3143 = _M0L6_2acntS3142 - 1;
        Moonbit_object_header(_M0L7_2aSomeS943)->rc = _M0L11_2anew__cntS3143;
        moonbit_incref(_M0L8_2afieldS2917.$0);
      } else if (_M0L6_2acntS3142 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS943);
      }
      _M0L12_2apkg__nameS944 = _M0L8_2afieldS2917;
      if (_M0L6loggerS945.$1) {
        moonbit_incref(_M0L6loggerS945.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS945.$0->$method_2(_M0L6loggerS945.$1, _M0L12_2apkg__nameS944);
      if (_M0L6loggerS945.$1) {
        moonbit_incref(_M0L6loggerS945.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS945.$0->$method_3(_M0L6loggerS945.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS942);
      break;
    }
  }
  _M0L8_2afieldS2916
  = (struct _M0TPC16string10StringView){
    _M0L4selfS923->$1_1, _M0L4selfS923->$1_2, _M0L4selfS923->$1_0
  };
  _M0L8filenameS2388 = _M0L8_2afieldS2916;
  moonbit_incref(_M0L8filenameS2388.$0);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_2(_M0L6loggerS945.$1, _M0L8filenameS2388);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_3(_M0L6loggerS945.$1, 58);
  _M0L8_2afieldS2915
  = (struct _M0TPC16string10StringView){
    _M0L4selfS923->$2_1, _M0L4selfS923->$2_2, _M0L4selfS923->$2_0
  };
  _M0L11start__lineS2389 = _M0L8_2afieldS2915;
  moonbit_incref(_M0L11start__lineS2389.$0);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_2(_M0L6loggerS945.$1, _M0L11start__lineS2389);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_3(_M0L6loggerS945.$1, 58);
  _M0L8_2afieldS2914
  = (struct _M0TPC16string10StringView){
    _M0L4selfS923->$3_1, _M0L4selfS923->$3_2, _M0L4selfS923->$3_0
  };
  _M0L13start__columnS2390 = _M0L8_2afieldS2914;
  moonbit_incref(_M0L13start__columnS2390.$0);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_2(_M0L6loggerS945.$1, _M0L13start__columnS2390);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_3(_M0L6loggerS945.$1, 45);
  _M0L8_2afieldS2913
  = (struct _M0TPC16string10StringView){
    _M0L4selfS923->$4_1, _M0L4selfS923->$4_2, _M0L4selfS923->$4_0
  };
  _M0L9end__lineS2391 = _M0L8_2afieldS2913;
  moonbit_incref(_M0L9end__lineS2391.$0);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_2(_M0L6loggerS945.$1, _M0L9end__lineS2391);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_3(_M0L6loggerS945.$1, 58);
  _M0L8_2afieldS2912
  = (struct _M0TPC16string10StringView){
    _M0L4selfS923->$5_1, _M0L4selfS923->$5_2, _M0L4selfS923->$5_0
  };
  _M0L6_2acntS3144 = Moonbit_object_header(_M0L4selfS923)->rc;
  if (_M0L6_2acntS3144 > 1) {
    int32_t _M0L11_2anew__cntS3150 = _M0L6_2acntS3144 - 1;
    Moonbit_object_header(_M0L4selfS923)->rc = _M0L11_2anew__cntS3150;
    moonbit_incref(_M0L8_2afieldS2912.$0);
  } else if (_M0L6_2acntS3144 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3149 =
      (struct _M0TPC16string10StringView){_M0L4selfS923->$4_1,
                                            _M0L4selfS923->$4_2,
                                            _M0L4selfS923->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3148;
    struct _M0TPC16string10StringView _M0L8_2afieldS3147;
    struct _M0TPC16string10StringView _M0L8_2afieldS3146;
    struct _M0TPC16string10StringView _M0L8_2afieldS3145;
    moonbit_decref(_M0L8_2afieldS3149.$0);
    _M0L8_2afieldS3148
    = (struct _M0TPC16string10StringView){
      _M0L4selfS923->$3_1, _M0L4selfS923->$3_2, _M0L4selfS923->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3148.$0);
    _M0L8_2afieldS3147
    = (struct _M0TPC16string10StringView){
      _M0L4selfS923->$2_1, _M0L4selfS923->$2_2, _M0L4selfS923->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3147.$0);
    _M0L8_2afieldS3146
    = (struct _M0TPC16string10StringView){
      _M0L4selfS923->$1_1, _M0L4selfS923->$1_2, _M0L4selfS923->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3146.$0);
    _M0L8_2afieldS3145
    = (struct _M0TPC16string10StringView){
      _M0L4selfS923->$0_1, _M0L4selfS923->$0_2, _M0L4selfS923->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3145.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS923);
  }
  _M0L11end__columnS2392 = _M0L8_2afieldS2912;
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_2(_M0L6loggerS945.$1, _M0L11end__columnS2392);
  if (_M0L6loggerS945.$1) {
    moonbit_incref(_M0L6loggerS945.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_3(_M0L6loggerS945.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS945.$0->$method_2(_M0L6loggerS945.$1, _M0L15_2amodule__nameS941);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS921) {
  moonbit_string_t _M0L6_2atmpS2387;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2387 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS921);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2387);
  moonbit_decref(_M0L6_2atmpS2387);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS920,
  struct _M0TPB6Logger _M0L6loggerS919
) {
  moonbit_string_t _M0L6_2atmpS2386;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2386 = _M0MPC16double6Double10to__string(_M0L4selfS920);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS919.$0->$method_0(_M0L6loggerS919.$1, _M0L6_2atmpS2386);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS918) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS918);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS905) {
  uint64_t _M0L4bitsS906;
  uint64_t _M0L6_2atmpS2385;
  uint64_t _M0L6_2atmpS2384;
  int32_t _M0L8ieeeSignS907;
  uint64_t _M0L12ieeeMantissaS908;
  uint64_t _M0L6_2atmpS2383;
  uint64_t _M0L6_2atmpS2382;
  int32_t _M0L12ieeeExponentS909;
  int32_t _if__result_3274;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS910;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS911;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2381;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS905 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_92.data;
  }
  _M0L4bitsS906 = *(int64_t*)&_M0L3valS905;
  _M0L6_2atmpS2385 = _M0L4bitsS906 >> 63;
  _M0L6_2atmpS2384 = _M0L6_2atmpS2385 & 1ull;
  _M0L8ieeeSignS907 = _M0L6_2atmpS2384 != 0ull;
  _M0L12ieeeMantissaS908 = _M0L4bitsS906 & 4503599627370495ull;
  _M0L6_2atmpS2383 = _M0L4bitsS906 >> 52;
  _M0L6_2atmpS2382 = _M0L6_2atmpS2383 & 2047ull;
  _M0L12ieeeExponentS909 = (int32_t)_M0L6_2atmpS2382;
  if (_M0L12ieeeExponentS909 == 2047) {
    _if__result_3274 = 1;
  } else if (_M0L12ieeeExponentS909 == 0) {
    _if__result_3274 = _M0L12ieeeMantissaS908 == 0ull;
  } else {
    _if__result_3274 = 0;
  }
  if (_if__result_3274) {
    int32_t _M0L6_2atmpS2370 = _M0L12ieeeExponentS909 != 0;
    int32_t _M0L6_2atmpS2371 = _M0L12ieeeMantissaS908 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS907, _M0L6_2atmpS2370, _M0L6_2atmpS2371);
  }
  _M0Lm1vS910 = _M0FPB30ryu__to__string_2erecord_2f904;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS911
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS908, _M0L12ieeeExponentS909);
  if (_M0L5smallS911 == 0) {
    uint32_t _M0L6_2atmpS2372;
    if (_M0L5smallS911) {
      moonbit_decref(_M0L5smallS911);
    }
    _M0L6_2atmpS2372 = *(uint32_t*)&_M0L12ieeeExponentS909;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS910 = _M0FPB3d2d(_M0L12ieeeMantissaS908, _M0L6_2atmpS2372);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS912 = _M0L5smallS911;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS913 = _M0L7_2aSomeS912;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS914 = _M0L4_2afS913;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2380 = _M0Lm1xS914;
      uint64_t _M0L8_2afieldS2923 = _M0L6_2atmpS2380->$0;
      uint64_t _M0L8mantissaS2379 = _M0L8_2afieldS2923;
      uint64_t _M0L1qS915 = _M0L8mantissaS2379 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2378 = _M0Lm1xS914;
      uint64_t _M0L8_2afieldS2922 = _M0L6_2atmpS2378->$0;
      uint64_t _M0L8mantissaS2376 = _M0L8_2afieldS2922;
      uint64_t _M0L6_2atmpS2377 = 10ull * _M0L1qS915;
      uint64_t _M0L1rS916 = _M0L8mantissaS2376 - _M0L6_2atmpS2377;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2375;
      int32_t _M0L8_2afieldS2921;
      int32_t _M0L8exponentS2374;
      int32_t _M0L6_2atmpS2373;
      if (_M0L1rS916 != 0ull) {
        break;
      }
      _M0L6_2atmpS2375 = _M0Lm1xS914;
      _M0L8_2afieldS2921 = _M0L6_2atmpS2375->$1;
      moonbit_decref(_M0L6_2atmpS2375);
      _M0L8exponentS2374 = _M0L8_2afieldS2921;
      _M0L6_2atmpS2373 = _M0L8exponentS2374 + 1;
      _M0Lm1xS914
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS914)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS914->$0 = _M0L1qS915;
      _M0Lm1xS914->$1 = _M0L6_2atmpS2373;
      continue;
      break;
    }
    _M0Lm1vS910 = _M0Lm1xS914;
  }
  _M0L6_2atmpS2381 = _M0Lm1vS910;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2381, _M0L8ieeeSignS907);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS899,
  int32_t _M0L12ieeeExponentS901
) {
  uint64_t _M0L2m2S898;
  int32_t _M0L6_2atmpS2369;
  int32_t _M0L2e2S900;
  int32_t _M0L6_2atmpS2368;
  uint64_t _M0L6_2atmpS2367;
  uint64_t _M0L4maskS902;
  uint64_t _M0L8fractionS903;
  int32_t _M0L6_2atmpS2366;
  uint64_t _M0L6_2atmpS2365;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2364;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S898 = 4503599627370496ull | _M0L12ieeeMantissaS899;
  _M0L6_2atmpS2369 = _M0L12ieeeExponentS901 - 1023;
  _M0L2e2S900 = _M0L6_2atmpS2369 - 52;
  if (_M0L2e2S900 > 0) {
    return 0;
  }
  if (_M0L2e2S900 < -52) {
    return 0;
  }
  _M0L6_2atmpS2368 = -_M0L2e2S900;
  _M0L6_2atmpS2367 = 1ull << (_M0L6_2atmpS2368 & 63);
  _M0L4maskS902 = _M0L6_2atmpS2367 - 1ull;
  _M0L8fractionS903 = _M0L2m2S898 & _M0L4maskS902;
  if (_M0L8fractionS903 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2366 = -_M0L2e2S900;
  _M0L6_2atmpS2365 = _M0L2m2S898 >> (_M0L6_2atmpS2366 & 63);
  _M0L6_2atmpS2364
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2364)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2364->$0 = _M0L6_2atmpS2365;
  _M0L6_2atmpS2364->$1 = 0;
  return _M0L6_2atmpS2364;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS872,
  int32_t _M0L4signS870
) {
  int32_t _M0L6_2atmpS2363;
  moonbit_bytes_t _M0L6resultS868;
  int32_t _M0Lm5indexS869;
  uint64_t _M0Lm6outputS871;
  uint64_t _M0L6_2atmpS2362;
  int32_t _M0L7olengthS873;
  int32_t _M0L8_2afieldS2924;
  int32_t _M0L8exponentS2361;
  int32_t _M0L6_2atmpS2360;
  int32_t _M0Lm3expS874;
  int32_t _M0L6_2atmpS2359;
  int32_t _M0L6_2atmpS2357;
  int32_t _M0L18scientificNotationS875;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2363 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS868 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2363);
  _M0Lm5indexS869 = 0;
  if (_M0L4signS870) {
    int32_t _M0L6_2atmpS2232 = _M0Lm5indexS869;
    int32_t _M0L6_2atmpS2233;
    if (
      _M0L6_2atmpS2232 < 0
      || _M0L6_2atmpS2232 >= Moonbit_array_length(_M0L6resultS868)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS868[_M0L6_2atmpS2232] = 45;
    _M0L6_2atmpS2233 = _M0Lm5indexS869;
    _M0Lm5indexS869 = _M0L6_2atmpS2233 + 1;
  }
  _M0Lm6outputS871 = _M0L1vS872->$0;
  _M0L6_2atmpS2362 = _M0Lm6outputS871;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS873 = _M0FPB17decimal__length17(_M0L6_2atmpS2362);
  _M0L8_2afieldS2924 = _M0L1vS872->$1;
  moonbit_decref(_M0L1vS872);
  _M0L8exponentS2361 = _M0L8_2afieldS2924;
  _M0L6_2atmpS2360 = _M0L8exponentS2361 + _M0L7olengthS873;
  _M0Lm3expS874 = _M0L6_2atmpS2360 - 1;
  _M0L6_2atmpS2359 = _M0Lm3expS874;
  if (_M0L6_2atmpS2359 >= -6) {
    int32_t _M0L6_2atmpS2358 = _M0Lm3expS874;
    _M0L6_2atmpS2357 = _M0L6_2atmpS2358 < 21;
  } else {
    _M0L6_2atmpS2357 = 0;
  }
  _M0L18scientificNotationS875 = !_M0L6_2atmpS2357;
  if (_M0L18scientificNotationS875) {
    int32_t _M0L7_2abindS876 = _M0L7olengthS873 - 1;
    int32_t _M0L1iS877 = 0;
    int32_t _M0L6_2atmpS2243;
    uint64_t _M0L6_2atmpS2248;
    int32_t _M0L6_2atmpS2247;
    int32_t _M0L6_2atmpS2246;
    int32_t _M0L6_2atmpS2245;
    int32_t _M0L6_2atmpS2244;
    int32_t _M0L6_2atmpS2252;
    int32_t _M0L6_2atmpS2253;
    int32_t _M0L6_2atmpS2254;
    int32_t _M0L6_2atmpS2255;
    int32_t _M0L6_2atmpS2256;
    int32_t _M0L6_2atmpS2262;
    int32_t _M0L6_2atmpS2295;
    while (1) {
      if (_M0L1iS877 < _M0L7_2abindS876) {
        uint64_t _M0L6_2atmpS2241 = _M0Lm6outputS871;
        uint64_t _M0L1cS878 = _M0L6_2atmpS2241 % 10ull;
        uint64_t _M0L6_2atmpS2234 = _M0Lm6outputS871;
        int32_t _M0L6_2atmpS2240;
        int32_t _M0L6_2atmpS2239;
        int32_t _M0L6_2atmpS2235;
        int32_t _M0L6_2atmpS2238;
        int32_t _M0L6_2atmpS2237;
        int32_t _M0L6_2atmpS2236;
        int32_t _M0L6_2atmpS2242;
        _M0Lm6outputS871 = _M0L6_2atmpS2234 / 10ull;
        _M0L6_2atmpS2240 = _M0Lm5indexS869;
        _M0L6_2atmpS2239 = _M0L6_2atmpS2240 + _M0L7olengthS873;
        _M0L6_2atmpS2235 = _M0L6_2atmpS2239 - _M0L1iS877;
        _M0L6_2atmpS2238 = (int32_t)_M0L1cS878;
        _M0L6_2atmpS2237 = 48 + _M0L6_2atmpS2238;
        _M0L6_2atmpS2236 = _M0L6_2atmpS2237 & 0xff;
        if (
          _M0L6_2atmpS2235 < 0
          || _M0L6_2atmpS2235 >= Moonbit_array_length(_M0L6resultS868)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS868[_M0L6_2atmpS2235] = _M0L6_2atmpS2236;
        _M0L6_2atmpS2242 = _M0L1iS877 + 1;
        _M0L1iS877 = _M0L6_2atmpS2242;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2243 = _M0Lm5indexS869;
    _M0L6_2atmpS2248 = _M0Lm6outputS871;
    _M0L6_2atmpS2247 = (int32_t)_M0L6_2atmpS2248;
    _M0L6_2atmpS2246 = _M0L6_2atmpS2247 % 10;
    _M0L6_2atmpS2245 = 48 + _M0L6_2atmpS2246;
    _M0L6_2atmpS2244 = _M0L6_2atmpS2245 & 0xff;
    if (
      _M0L6_2atmpS2243 < 0
      || _M0L6_2atmpS2243 >= Moonbit_array_length(_M0L6resultS868)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS868[_M0L6_2atmpS2243] = _M0L6_2atmpS2244;
    if (_M0L7olengthS873 > 1) {
      int32_t _M0L6_2atmpS2250 = _M0Lm5indexS869;
      int32_t _M0L6_2atmpS2249 = _M0L6_2atmpS2250 + 1;
      if (
        _M0L6_2atmpS2249 < 0
        || _M0L6_2atmpS2249 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2249] = 46;
    } else {
      int32_t _M0L6_2atmpS2251 = _M0Lm5indexS869;
      _M0Lm5indexS869 = _M0L6_2atmpS2251 - 1;
    }
    _M0L6_2atmpS2252 = _M0Lm5indexS869;
    _M0L6_2atmpS2253 = _M0L7olengthS873 + 1;
    _M0Lm5indexS869 = _M0L6_2atmpS2252 + _M0L6_2atmpS2253;
    _M0L6_2atmpS2254 = _M0Lm5indexS869;
    if (
      _M0L6_2atmpS2254 < 0
      || _M0L6_2atmpS2254 >= Moonbit_array_length(_M0L6resultS868)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS868[_M0L6_2atmpS2254] = 101;
    _M0L6_2atmpS2255 = _M0Lm5indexS869;
    _M0Lm5indexS869 = _M0L6_2atmpS2255 + 1;
    _M0L6_2atmpS2256 = _M0Lm3expS874;
    if (_M0L6_2atmpS2256 < 0) {
      int32_t _M0L6_2atmpS2257 = _M0Lm5indexS869;
      int32_t _M0L6_2atmpS2258;
      int32_t _M0L6_2atmpS2259;
      if (
        _M0L6_2atmpS2257 < 0
        || _M0L6_2atmpS2257 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2257] = 45;
      _M0L6_2atmpS2258 = _M0Lm5indexS869;
      _M0Lm5indexS869 = _M0L6_2atmpS2258 + 1;
      _M0L6_2atmpS2259 = _M0Lm3expS874;
      _M0Lm3expS874 = -_M0L6_2atmpS2259;
    } else {
      int32_t _M0L6_2atmpS2260 = _M0Lm5indexS869;
      int32_t _M0L6_2atmpS2261;
      if (
        _M0L6_2atmpS2260 < 0
        || _M0L6_2atmpS2260 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2260] = 43;
      _M0L6_2atmpS2261 = _M0Lm5indexS869;
      _M0Lm5indexS869 = _M0L6_2atmpS2261 + 1;
    }
    _M0L6_2atmpS2262 = _M0Lm3expS874;
    if (_M0L6_2atmpS2262 >= 100) {
      int32_t _M0L6_2atmpS2278 = _M0Lm3expS874;
      int32_t _M0L1aS880 = _M0L6_2atmpS2278 / 100;
      int32_t _M0L6_2atmpS2277 = _M0Lm3expS874;
      int32_t _M0L6_2atmpS2276 = _M0L6_2atmpS2277 / 10;
      int32_t _M0L1bS881 = _M0L6_2atmpS2276 % 10;
      int32_t _M0L6_2atmpS2275 = _M0Lm3expS874;
      int32_t _M0L1cS882 = _M0L6_2atmpS2275 % 10;
      int32_t _M0L6_2atmpS2263 = _M0Lm5indexS869;
      int32_t _M0L6_2atmpS2265 = 48 + _M0L1aS880;
      int32_t _M0L6_2atmpS2264 = _M0L6_2atmpS2265 & 0xff;
      int32_t _M0L6_2atmpS2269;
      int32_t _M0L6_2atmpS2266;
      int32_t _M0L6_2atmpS2268;
      int32_t _M0L6_2atmpS2267;
      int32_t _M0L6_2atmpS2273;
      int32_t _M0L6_2atmpS2270;
      int32_t _M0L6_2atmpS2272;
      int32_t _M0L6_2atmpS2271;
      int32_t _M0L6_2atmpS2274;
      if (
        _M0L6_2atmpS2263 < 0
        || _M0L6_2atmpS2263 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2263] = _M0L6_2atmpS2264;
      _M0L6_2atmpS2269 = _M0Lm5indexS869;
      _M0L6_2atmpS2266 = _M0L6_2atmpS2269 + 1;
      _M0L6_2atmpS2268 = 48 + _M0L1bS881;
      _M0L6_2atmpS2267 = _M0L6_2atmpS2268 & 0xff;
      if (
        _M0L6_2atmpS2266 < 0
        || _M0L6_2atmpS2266 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2266] = _M0L6_2atmpS2267;
      _M0L6_2atmpS2273 = _M0Lm5indexS869;
      _M0L6_2atmpS2270 = _M0L6_2atmpS2273 + 2;
      _M0L6_2atmpS2272 = 48 + _M0L1cS882;
      _M0L6_2atmpS2271 = _M0L6_2atmpS2272 & 0xff;
      if (
        _M0L6_2atmpS2270 < 0
        || _M0L6_2atmpS2270 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2270] = _M0L6_2atmpS2271;
      _M0L6_2atmpS2274 = _M0Lm5indexS869;
      _M0Lm5indexS869 = _M0L6_2atmpS2274 + 3;
    } else {
      int32_t _M0L6_2atmpS2279 = _M0Lm3expS874;
      if (_M0L6_2atmpS2279 >= 10) {
        int32_t _M0L6_2atmpS2289 = _M0Lm3expS874;
        int32_t _M0L1aS883 = _M0L6_2atmpS2289 / 10;
        int32_t _M0L6_2atmpS2288 = _M0Lm3expS874;
        int32_t _M0L1bS884 = _M0L6_2atmpS2288 % 10;
        int32_t _M0L6_2atmpS2280 = _M0Lm5indexS869;
        int32_t _M0L6_2atmpS2282 = 48 + _M0L1aS883;
        int32_t _M0L6_2atmpS2281 = _M0L6_2atmpS2282 & 0xff;
        int32_t _M0L6_2atmpS2286;
        int32_t _M0L6_2atmpS2283;
        int32_t _M0L6_2atmpS2285;
        int32_t _M0L6_2atmpS2284;
        int32_t _M0L6_2atmpS2287;
        if (
          _M0L6_2atmpS2280 < 0
          || _M0L6_2atmpS2280 >= Moonbit_array_length(_M0L6resultS868)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS868[_M0L6_2atmpS2280] = _M0L6_2atmpS2281;
        _M0L6_2atmpS2286 = _M0Lm5indexS869;
        _M0L6_2atmpS2283 = _M0L6_2atmpS2286 + 1;
        _M0L6_2atmpS2285 = 48 + _M0L1bS884;
        _M0L6_2atmpS2284 = _M0L6_2atmpS2285 & 0xff;
        if (
          _M0L6_2atmpS2283 < 0
          || _M0L6_2atmpS2283 >= Moonbit_array_length(_M0L6resultS868)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS868[_M0L6_2atmpS2283] = _M0L6_2atmpS2284;
        _M0L6_2atmpS2287 = _M0Lm5indexS869;
        _M0Lm5indexS869 = _M0L6_2atmpS2287 + 2;
      } else {
        int32_t _M0L6_2atmpS2290 = _M0Lm5indexS869;
        int32_t _M0L6_2atmpS2293 = _M0Lm3expS874;
        int32_t _M0L6_2atmpS2292 = 48 + _M0L6_2atmpS2293;
        int32_t _M0L6_2atmpS2291 = _M0L6_2atmpS2292 & 0xff;
        int32_t _M0L6_2atmpS2294;
        if (
          _M0L6_2atmpS2290 < 0
          || _M0L6_2atmpS2290 >= Moonbit_array_length(_M0L6resultS868)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS868[_M0L6_2atmpS2290] = _M0L6_2atmpS2291;
        _M0L6_2atmpS2294 = _M0Lm5indexS869;
        _M0Lm5indexS869 = _M0L6_2atmpS2294 + 1;
      }
    }
    _M0L6_2atmpS2295 = _M0Lm5indexS869;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS868, 0, _M0L6_2atmpS2295);
  } else {
    int32_t _M0L6_2atmpS2296 = _M0Lm3expS874;
    int32_t _M0L6_2atmpS2356;
    if (_M0L6_2atmpS2296 < 0) {
      int32_t _M0L6_2atmpS2297 = _M0Lm5indexS869;
      int32_t _M0L6_2atmpS2298;
      int32_t _M0L6_2atmpS2299;
      int32_t _M0L6_2atmpS2300;
      int32_t _M0L1iS885;
      int32_t _M0L7currentS887;
      int32_t _M0L1iS888;
      if (
        _M0L6_2atmpS2297 < 0
        || _M0L6_2atmpS2297 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2297] = 48;
      _M0L6_2atmpS2298 = _M0Lm5indexS869;
      _M0Lm5indexS869 = _M0L6_2atmpS2298 + 1;
      _M0L6_2atmpS2299 = _M0Lm5indexS869;
      if (
        _M0L6_2atmpS2299 < 0
        || _M0L6_2atmpS2299 >= Moonbit_array_length(_M0L6resultS868)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS868[_M0L6_2atmpS2299] = 46;
      _M0L6_2atmpS2300 = _M0Lm5indexS869;
      _M0Lm5indexS869 = _M0L6_2atmpS2300 + 1;
      _M0L1iS885 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2301 = _M0Lm3expS874;
        if (_M0L1iS885 > _M0L6_2atmpS2301) {
          int32_t _M0L6_2atmpS2302 = _M0Lm5indexS869;
          int32_t _M0L6_2atmpS2303;
          int32_t _M0L6_2atmpS2304;
          if (
            _M0L6_2atmpS2302 < 0
            || _M0L6_2atmpS2302 >= Moonbit_array_length(_M0L6resultS868)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS868[_M0L6_2atmpS2302] = 48;
          _M0L6_2atmpS2303 = _M0Lm5indexS869;
          _M0Lm5indexS869 = _M0L6_2atmpS2303 + 1;
          _M0L6_2atmpS2304 = _M0L1iS885 - 1;
          _M0L1iS885 = _M0L6_2atmpS2304;
          continue;
        }
        break;
      }
      _M0L7currentS887 = _M0Lm5indexS869;
      _M0L1iS888 = 0;
      while (1) {
        if (_M0L1iS888 < _M0L7olengthS873) {
          int32_t _M0L6_2atmpS2312 = _M0L7currentS887 + _M0L7olengthS873;
          int32_t _M0L6_2atmpS2311 = _M0L6_2atmpS2312 - _M0L1iS888;
          int32_t _M0L6_2atmpS2305 = _M0L6_2atmpS2311 - 1;
          uint64_t _M0L6_2atmpS2310 = _M0Lm6outputS871;
          uint64_t _M0L6_2atmpS2309 = _M0L6_2atmpS2310 % 10ull;
          int32_t _M0L6_2atmpS2308 = (int32_t)_M0L6_2atmpS2309;
          int32_t _M0L6_2atmpS2307 = 48 + _M0L6_2atmpS2308;
          int32_t _M0L6_2atmpS2306 = _M0L6_2atmpS2307 & 0xff;
          uint64_t _M0L6_2atmpS2313;
          int32_t _M0L6_2atmpS2314;
          int32_t _M0L6_2atmpS2315;
          if (
            _M0L6_2atmpS2305 < 0
            || _M0L6_2atmpS2305 >= Moonbit_array_length(_M0L6resultS868)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS868[_M0L6_2atmpS2305] = _M0L6_2atmpS2306;
          _M0L6_2atmpS2313 = _M0Lm6outputS871;
          _M0Lm6outputS871 = _M0L6_2atmpS2313 / 10ull;
          _M0L6_2atmpS2314 = _M0Lm5indexS869;
          _M0Lm5indexS869 = _M0L6_2atmpS2314 + 1;
          _M0L6_2atmpS2315 = _M0L1iS888 + 1;
          _M0L1iS888 = _M0L6_2atmpS2315;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2317 = _M0Lm3expS874;
      int32_t _M0L6_2atmpS2316 = _M0L6_2atmpS2317 + 1;
      if (_M0L6_2atmpS2316 >= _M0L7olengthS873) {
        int32_t _M0L1iS890 = 0;
        int32_t _M0L6_2atmpS2329;
        int32_t _M0L6_2atmpS2333;
        int32_t _M0L7_2abindS892;
        int32_t _M0L2__S893;
        while (1) {
          if (_M0L1iS890 < _M0L7olengthS873) {
            int32_t _M0L6_2atmpS2326 = _M0Lm5indexS869;
            int32_t _M0L6_2atmpS2325 = _M0L6_2atmpS2326 + _M0L7olengthS873;
            int32_t _M0L6_2atmpS2324 = _M0L6_2atmpS2325 - _M0L1iS890;
            int32_t _M0L6_2atmpS2318 = _M0L6_2atmpS2324 - 1;
            uint64_t _M0L6_2atmpS2323 = _M0Lm6outputS871;
            uint64_t _M0L6_2atmpS2322 = _M0L6_2atmpS2323 % 10ull;
            int32_t _M0L6_2atmpS2321 = (int32_t)_M0L6_2atmpS2322;
            int32_t _M0L6_2atmpS2320 = 48 + _M0L6_2atmpS2321;
            int32_t _M0L6_2atmpS2319 = _M0L6_2atmpS2320 & 0xff;
            uint64_t _M0L6_2atmpS2327;
            int32_t _M0L6_2atmpS2328;
            if (
              _M0L6_2atmpS2318 < 0
              || _M0L6_2atmpS2318 >= Moonbit_array_length(_M0L6resultS868)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS868[_M0L6_2atmpS2318] = _M0L6_2atmpS2319;
            _M0L6_2atmpS2327 = _M0Lm6outputS871;
            _M0Lm6outputS871 = _M0L6_2atmpS2327 / 10ull;
            _M0L6_2atmpS2328 = _M0L1iS890 + 1;
            _M0L1iS890 = _M0L6_2atmpS2328;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2329 = _M0Lm5indexS869;
        _M0Lm5indexS869 = _M0L6_2atmpS2329 + _M0L7olengthS873;
        _M0L6_2atmpS2333 = _M0Lm3expS874;
        _M0L7_2abindS892 = _M0L6_2atmpS2333 + 1;
        _M0L2__S893 = _M0L7olengthS873;
        while (1) {
          if (_M0L2__S893 < _M0L7_2abindS892) {
            int32_t _M0L6_2atmpS2330 = _M0Lm5indexS869;
            int32_t _M0L6_2atmpS2331;
            int32_t _M0L6_2atmpS2332;
            if (
              _M0L6_2atmpS2330 < 0
              || _M0L6_2atmpS2330 >= Moonbit_array_length(_M0L6resultS868)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS868[_M0L6_2atmpS2330] = 48;
            _M0L6_2atmpS2331 = _M0Lm5indexS869;
            _M0Lm5indexS869 = _M0L6_2atmpS2331 + 1;
            _M0L6_2atmpS2332 = _M0L2__S893 + 1;
            _M0L2__S893 = _M0L6_2atmpS2332;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2355 = _M0Lm5indexS869;
        int32_t _M0Lm7currentS895 = _M0L6_2atmpS2355 + 1;
        int32_t _M0L1iS896 = 0;
        int32_t _M0L6_2atmpS2353;
        int32_t _M0L6_2atmpS2354;
        while (1) {
          if (_M0L1iS896 < _M0L7olengthS873) {
            int32_t _M0L6_2atmpS2336 = _M0L7olengthS873 - _M0L1iS896;
            int32_t _M0L6_2atmpS2334 = _M0L6_2atmpS2336 - 1;
            int32_t _M0L6_2atmpS2335 = _M0Lm3expS874;
            int32_t _M0L6_2atmpS2350;
            int32_t _M0L6_2atmpS2349;
            int32_t _M0L6_2atmpS2348;
            int32_t _M0L6_2atmpS2342;
            uint64_t _M0L6_2atmpS2347;
            uint64_t _M0L6_2atmpS2346;
            int32_t _M0L6_2atmpS2345;
            int32_t _M0L6_2atmpS2344;
            int32_t _M0L6_2atmpS2343;
            uint64_t _M0L6_2atmpS2351;
            int32_t _M0L6_2atmpS2352;
            if (_M0L6_2atmpS2334 == _M0L6_2atmpS2335) {
              int32_t _M0L6_2atmpS2340 = _M0Lm7currentS895;
              int32_t _M0L6_2atmpS2339 = _M0L6_2atmpS2340 + _M0L7olengthS873;
              int32_t _M0L6_2atmpS2338 = _M0L6_2atmpS2339 - _M0L1iS896;
              int32_t _M0L6_2atmpS2337 = _M0L6_2atmpS2338 - 1;
              int32_t _M0L6_2atmpS2341;
              if (
                _M0L6_2atmpS2337 < 0
                || _M0L6_2atmpS2337 >= Moonbit_array_length(_M0L6resultS868)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS868[_M0L6_2atmpS2337] = 46;
              _M0L6_2atmpS2341 = _M0Lm7currentS895;
              _M0Lm7currentS895 = _M0L6_2atmpS2341 - 1;
            }
            _M0L6_2atmpS2350 = _M0Lm7currentS895;
            _M0L6_2atmpS2349 = _M0L6_2atmpS2350 + _M0L7olengthS873;
            _M0L6_2atmpS2348 = _M0L6_2atmpS2349 - _M0L1iS896;
            _M0L6_2atmpS2342 = _M0L6_2atmpS2348 - 1;
            _M0L6_2atmpS2347 = _M0Lm6outputS871;
            _M0L6_2atmpS2346 = _M0L6_2atmpS2347 % 10ull;
            _M0L6_2atmpS2345 = (int32_t)_M0L6_2atmpS2346;
            _M0L6_2atmpS2344 = 48 + _M0L6_2atmpS2345;
            _M0L6_2atmpS2343 = _M0L6_2atmpS2344 & 0xff;
            if (
              _M0L6_2atmpS2342 < 0
              || _M0L6_2atmpS2342 >= Moonbit_array_length(_M0L6resultS868)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS868[_M0L6_2atmpS2342] = _M0L6_2atmpS2343;
            _M0L6_2atmpS2351 = _M0Lm6outputS871;
            _M0Lm6outputS871 = _M0L6_2atmpS2351 / 10ull;
            _M0L6_2atmpS2352 = _M0L1iS896 + 1;
            _M0L1iS896 = _M0L6_2atmpS2352;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2353 = _M0Lm5indexS869;
        _M0L6_2atmpS2354 = _M0L7olengthS873 + 1;
        _M0Lm5indexS869 = _M0L6_2atmpS2353 + _M0L6_2atmpS2354;
      }
    }
    _M0L6_2atmpS2356 = _M0Lm5indexS869;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS868, 0, _M0L6_2atmpS2356);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS814,
  uint32_t _M0L12ieeeExponentS813
) {
  int32_t _M0Lm2e2S811;
  uint64_t _M0Lm2m2S812;
  uint64_t _M0L6_2atmpS2231;
  uint64_t _M0L6_2atmpS2230;
  int32_t _M0L4evenS815;
  uint64_t _M0L6_2atmpS2229;
  uint64_t _M0L2mvS816;
  int32_t _M0L7mmShiftS817;
  uint64_t _M0Lm2vrS818;
  uint64_t _M0Lm2vpS819;
  uint64_t _M0Lm2vmS820;
  int32_t _M0Lm3e10S821;
  int32_t _M0Lm17vmIsTrailingZerosS822;
  int32_t _M0Lm17vrIsTrailingZerosS823;
  int32_t _M0L6_2atmpS2131;
  int32_t _M0Lm7removedS842;
  int32_t _M0Lm16lastRemovedDigitS843;
  uint64_t _M0Lm6outputS844;
  int32_t _M0L6_2atmpS2227;
  int32_t _M0L6_2atmpS2228;
  int32_t _M0L3expS867;
  uint64_t _M0L6_2atmpS2226;
  struct _M0TPB17FloatingDecimal64* _block_3287;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S811 = 0;
  _M0Lm2m2S812 = 0ull;
  if (_M0L12ieeeExponentS813 == 0u) {
    _M0Lm2e2S811 = -1076;
    _M0Lm2m2S812 = _M0L12ieeeMantissaS814;
  } else {
    int32_t _M0L6_2atmpS2130 = *(int32_t*)&_M0L12ieeeExponentS813;
    int32_t _M0L6_2atmpS2129 = _M0L6_2atmpS2130 - 1023;
    int32_t _M0L6_2atmpS2128 = _M0L6_2atmpS2129 - 52;
    _M0Lm2e2S811 = _M0L6_2atmpS2128 - 2;
    _M0Lm2m2S812 = 4503599627370496ull | _M0L12ieeeMantissaS814;
  }
  _M0L6_2atmpS2231 = _M0Lm2m2S812;
  _M0L6_2atmpS2230 = _M0L6_2atmpS2231 & 1ull;
  _M0L4evenS815 = _M0L6_2atmpS2230 == 0ull;
  _M0L6_2atmpS2229 = _M0Lm2m2S812;
  _M0L2mvS816 = 4ull * _M0L6_2atmpS2229;
  if (_M0L12ieeeMantissaS814 != 0ull) {
    _M0L7mmShiftS817 = 1;
  } else {
    _M0L7mmShiftS817 = _M0L12ieeeExponentS813 <= 1u;
  }
  _M0Lm2vrS818 = 0ull;
  _M0Lm2vpS819 = 0ull;
  _M0Lm2vmS820 = 0ull;
  _M0Lm3e10S821 = 0;
  _M0Lm17vmIsTrailingZerosS822 = 0;
  _M0Lm17vrIsTrailingZerosS823 = 0;
  _M0L6_2atmpS2131 = _M0Lm2e2S811;
  if (_M0L6_2atmpS2131 >= 0) {
    int32_t _M0L6_2atmpS2153 = _M0Lm2e2S811;
    int32_t _M0L6_2atmpS2149;
    int32_t _M0L6_2atmpS2152;
    int32_t _M0L6_2atmpS2151;
    int32_t _M0L6_2atmpS2150;
    int32_t _M0L1qS824;
    int32_t _M0L6_2atmpS2148;
    int32_t _M0L6_2atmpS2147;
    int32_t _M0L1kS825;
    int32_t _M0L6_2atmpS2146;
    int32_t _M0L6_2atmpS2145;
    int32_t _M0L6_2atmpS2144;
    int32_t _M0L1iS826;
    struct _M0TPB8Pow5Pair _M0L4pow5S827;
    uint64_t _M0L6_2atmpS2143;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS828;
    uint64_t _M0L8_2avrOutS829;
    uint64_t _M0L8_2avpOutS830;
    uint64_t _M0L8_2avmOutS831;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2149 = _M0FPB9log10Pow2(_M0L6_2atmpS2153);
    _M0L6_2atmpS2152 = _M0Lm2e2S811;
    _M0L6_2atmpS2151 = _M0L6_2atmpS2152 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2150 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2151);
    _M0L1qS824 = _M0L6_2atmpS2149 - _M0L6_2atmpS2150;
    _M0Lm3e10S821 = _M0L1qS824;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2148 = _M0FPB8pow5bits(_M0L1qS824);
    _M0L6_2atmpS2147 = 125 + _M0L6_2atmpS2148;
    _M0L1kS825 = _M0L6_2atmpS2147 - 1;
    _M0L6_2atmpS2146 = _M0Lm2e2S811;
    _M0L6_2atmpS2145 = -_M0L6_2atmpS2146;
    _M0L6_2atmpS2144 = _M0L6_2atmpS2145 + _M0L1qS824;
    _M0L1iS826 = _M0L6_2atmpS2144 + _M0L1kS825;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S827 = _M0FPB22double__computeInvPow5(_M0L1qS824);
    _M0L6_2atmpS2143 = _M0Lm2m2S812;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS828
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2143, _M0L4pow5S827, _M0L1iS826, _M0L7mmShiftS817);
    _M0L8_2avrOutS829 = _M0L7_2abindS828.$0;
    _M0L8_2avpOutS830 = _M0L7_2abindS828.$1;
    _M0L8_2avmOutS831 = _M0L7_2abindS828.$2;
    _M0Lm2vrS818 = _M0L8_2avrOutS829;
    _M0Lm2vpS819 = _M0L8_2avpOutS830;
    _M0Lm2vmS820 = _M0L8_2avmOutS831;
    if (_M0L1qS824 <= 21) {
      int32_t _M0L6_2atmpS2139 = (int32_t)_M0L2mvS816;
      uint64_t _M0L6_2atmpS2142 = _M0L2mvS816 / 5ull;
      int32_t _M0L6_2atmpS2141 = (int32_t)_M0L6_2atmpS2142;
      int32_t _M0L6_2atmpS2140 = 5 * _M0L6_2atmpS2141;
      int32_t _M0L6mvMod5S832 = _M0L6_2atmpS2139 - _M0L6_2atmpS2140;
      if (_M0L6mvMod5S832 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS823
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS816, _M0L1qS824);
      } else if (_M0L4evenS815) {
        uint64_t _M0L6_2atmpS2133 = _M0L2mvS816 - 1ull;
        uint64_t _M0L6_2atmpS2134;
        uint64_t _M0L6_2atmpS2132;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2134 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS817);
        _M0L6_2atmpS2132 = _M0L6_2atmpS2133 - _M0L6_2atmpS2134;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS822
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2132, _M0L1qS824);
      } else {
        uint64_t _M0L6_2atmpS2135 = _M0Lm2vpS819;
        uint64_t _M0L6_2atmpS2138 = _M0L2mvS816 + 2ull;
        int32_t _M0L6_2atmpS2137;
        uint64_t _M0L6_2atmpS2136;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2137
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2138, _M0L1qS824);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2136 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2137);
        _M0Lm2vpS819 = _M0L6_2atmpS2135 - _M0L6_2atmpS2136;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2167 = _M0Lm2e2S811;
    int32_t _M0L6_2atmpS2166 = -_M0L6_2atmpS2167;
    int32_t _M0L6_2atmpS2161;
    int32_t _M0L6_2atmpS2165;
    int32_t _M0L6_2atmpS2164;
    int32_t _M0L6_2atmpS2163;
    int32_t _M0L6_2atmpS2162;
    int32_t _M0L1qS833;
    int32_t _M0L6_2atmpS2154;
    int32_t _M0L6_2atmpS2160;
    int32_t _M0L6_2atmpS2159;
    int32_t _M0L1iS834;
    int32_t _M0L6_2atmpS2158;
    int32_t _M0L1kS835;
    int32_t _M0L1jS836;
    struct _M0TPB8Pow5Pair _M0L4pow5S837;
    uint64_t _M0L6_2atmpS2157;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS838;
    uint64_t _M0L8_2avrOutS839;
    uint64_t _M0L8_2avpOutS840;
    uint64_t _M0L8_2avmOutS841;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2161 = _M0FPB9log10Pow5(_M0L6_2atmpS2166);
    _M0L6_2atmpS2165 = _M0Lm2e2S811;
    _M0L6_2atmpS2164 = -_M0L6_2atmpS2165;
    _M0L6_2atmpS2163 = _M0L6_2atmpS2164 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2162 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2163);
    _M0L1qS833 = _M0L6_2atmpS2161 - _M0L6_2atmpS2162;
    _M0L6_2atmpS2154 = _M0Lm2e2S811;
    _M0Lm3e10S821 = _M0L1qS833 + _M0L6_2atmpS2154;
    _M0L6_2atmpS2160 = _M0Lm2e2S811;
    _M0L6_2atmpS2159 = -_M0L6_2atmpS2160;
    _M0L1iS834 = _M0L6_2atmpS2159 - _M0L1qS833;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2158 = _M0FPB8pow5bits(_M0L1iS834);
    _M0L1kS835 = _M0L6_2atmpS2158 - 125;
    _M0L1jS836 = _M0L1qS833 - _M0L1kS835;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S837 = _M0FPB19double__computePow5(_M0L1iS834);
    _M0L6_2atmpS2157 = _M0Lm2m2S812;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS838
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2157, _M0L4pow5S837, _M0L1jS836, _M0L7mmShiftS817);
    _M0L8_2avrOutS839 = _M0L7_2abindS838.$0;
    _M0L8_2avpOutS840 = _M0L7_2abindS838.$1;
    _M0L8_2avmOutS841 = _M0L7_2abindS838.$2;
    _M0Lm2vrS818 = _M0L8_2avrOutS839;
    _M0Lm2vpS819 = _M0L8_2avpOutS840;
    _M0Lm2vmS820 = _M0L8_2avmOutS841;
    if (_M0L1qS833 <= 1) {
      _M0Lm17vrIsTrailingZerosS823 = 1;
      if (_M0L4evenS815) {
        int32_t _M0L6_2atmpS2155;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2155 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS817);
        _M0Lm17vmIsTrailingZerosS822 = _M0L6_2atmpS2155 == 1;
      } else {
        uint64_t _M0L6_2atmpS2156 = _M0Lm2vpS819;
        _M0Lm2vpS819 = _M0L6_2atmpS2156 - 1ull;
      }
    } else if (_M0L1qS833 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS823
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS816, _M0L1qS833);
    }
  }
  _M0Lm7removedS842 = 0;
  _M0Lm16lastRemovedDigitS843 = 0;
  _M0Lm6outputS844 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS822 || _M0Lm17vrIsTrailingZerosS823) {
    int32_t _if__result_3284;
    uint64_t _M0L6_2atmpS2197;
    uint64_t _M0L6_2atmpS2203;
    uint64_t _M0L6_2atmpS2204;
    int32_t _if__result_3285;
    int32_t _M0L6_2atmpS2200;
    int64_t _M0L6_2atmpS2199;
    uint64_t _M0L6_2atmpS2198;
    while (1) {
      uint64_t _M0L6_2atmpS2180 = _M0Lm2vpS819;
      uint64_t _M0L7vpDiv10S845 = _M0L6_2atmpS2180 / 10ull;
      uint64_t _M0L6_2atmpS2179 = _M0Lm2vmS820;
      uint64_t _M0L7vmDiv10S846 = _M0L6_2atmpS2179 / 10ull;
      uint64_t _M0L6_2atmpS2178;
      int32_t _M0L6_2atmpS2175;
      int32_t _M0L6_2atmpS2177;
      int32_t _M0L6_2atmpS2176;
      int32_t _M0L7vmMod10S848;
      uint64_t _M0L6_2atmpS2174;
      uint64_t _M0L7vrDiv10S849;
      uint64_t _M0L6_2atmpS2173;
      int32_t _M0L6_2atmpS2170;
      int32_t _M0L6_2atmpS2172;
      int32_t _M0L6_2atmpS2171;
      int32_t _M0L7vrMod10S850;
      int32_t _M0L6_2atmpS2169;
      if (_M0L7vpDiv10S845 <= _M0L7vmDiv10S846) {
        break;
      }
      _M0L6_2atmpS2178 = _M0Lm2vmS820;
      _M0L6_2atmpS2175 = (int32_t)_M0L6_2atmpS2178;
      _M0L6_2atmpS2177 = (int32_t)_M0L7vmDiv10S846;
      _M0L6_2atmpS2176 = 10 * _M0L6_2atmpS2177;
      _M0L7vmMod10S848 = _M0L6_2atmpS2175 - _M0L6_2atmpS2176;
      _M0L6_2atmpS2174 = _M0Lm2vrS818;
      _M0L7vrDiv10S849 = _M0L6_2atmpS2174 / 10ull;
      _M0L6_2atmpS2173 = _M0Lm2vrS818;
      _M0L6_2atmpS2170 = (int32_t)_M0L6_2atmpS2173;
      _M0L6_2atmpS2172 = (int32_t)_M0L7vrDiv10S849;
      _M0L6_2atmpS2171 = 10 * _M0L6_2atmpS2172;
      _M0L7vrMod10S850 = _M0L6_2atmpS2170 - _M0L6_2atmpS2171;
      if (_M0Lm17vmIsTrailingZerosS822) {
        _M0Lm17vmIsTrailingZerosS822 = _M0L7vmMod10S848 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS822 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS823) {
        int32_t _M0L6_2atmpS2168 = _M0Lm16lastRemovedDigitS843;
        _M0Lm17vrIsTrailingZerosS823 = _M0L6_2atmpS2168 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS823 = 0;
      }
      _M0Lm16lastRemovedDigitS843 = _M0L7vrMod10S850;
      _M0Lm2vrS818 = _M0L7vrDiv10S849;
      _M0Lm2vpS819 = _M0L7vpDiv10S845;
      _M0Lm2vmS820 = _M0L7vmDiv10S846;
      _M0L6_2atmpS2169 = _M0Lm7removedS842;
      _M0Lm7removedS842 = _M0L6_2atmpS2169 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS822) {
      while (1) {
        uint64_t _M0L6_2atmpS2193 = _M0Lm2vmS820;
        uint64_t _M0L7vmDiv10S851 = _M0L6_2atmpS2193 / 10ull;
        uint64_t _M0L6_2atmpS2192 = _M0Lm2vmS820;
        int32_t _M0L6_2atmpS2189 = (int32_t)_M0L6_2atmpS2192;
        int32_t _M0L6_2atmpS2191 = (int32_t)_M0L7vmDiv10S851;
        int32_t _M0L6_2atmpS2190 = 10 * _M0L6_2atmpS2191;
        int32_t _M0L7vmMod10S852 = _M0L6_2atmpS2189 - _M0L6_2atmpS2190;
        uint64_t _M0L6_2atmpS2188;
        uint64_t _M0L7vpDiv10S854;
        uint64_t _M0L6_2atmpS2187;
        uint64_t _M0L7vrDiv10S855;
        uint64_t _M0L6_2atmpS2186;
        int32_t _M0L6_2atmpS2183;
        int32_t _M0L6_2atmpS2185;
        int32_t _M0L6_2atmpS2184;
        int32_t _M0L7vrMod10S856;
        int32_t _M0L6_2atmpS2182;
        if (_M0L7vmMod10S852 != 0) {
          break;
        }
        _M0L6_2atmpS2188 = _M0Lm2vpS819;
        _M0L7vpDiv10S854 = _M0L6_2atmpS2188 / 10ull;
        _M0L6_2atmpS2187 = _M0Lm2vrS818;
        _M0L7vrDiv10S855 = _M0L6_2atmpS2187 / 10ull;
        _M0L6_2atmpS2186 = _M0Lm2vrS818;
        _M0L6_2atmpS2183 = (int32_t)_M0L6_2atmpS2186;
        _M0L6_2atmpS2185 = (int32_t)_M0L7vrDiv10S855;
        _M0L6_2atmpS2184 = 10 * _M0L6_2atmpS2185;
        _M0L7vrMod10S856 = _M0L6_2atmpS2183 - _M0L6_2atmpS2184;
        if (_M0Lm17vrIsTrailingZerosS823) {
          int32_t _M0L6_2atmpS2181 = _M0Lm16lastRemovedDigitS843;
          _M0Lm17vrIsTrailingZerosS823 = _M0L6_2atmpS2181 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS823 = 0;
        }
        _M0Lm16lastRemovedDigitS843 = _M0L7vrMod10S856;
        _M0Lm2vrS818 = _M0L7vrDiv10S855;
        _M0Lm2vpS819 = _M0L7vpDiv10S854;
        _M0Lm2vmS820 = _M0L7vmDiv10S851;
        _M0L6_2atmpS2182 = _M0Lm7removedS842;
        _M0Lm7removedS842 = _M0L6_2atmpS2182 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS823) {
      int32_t _M0L6_2atmpS2196 = _M0Lm16lastRemovedDigitS843;
      if (_M0L6_2atmpS2196 == 5) {
        uint64_t _M0L6_2atmpS2195 = _M0Lm2vrS818;
        uint64_t _M0L6_2atmpS2194 = _M0L6_2atmpS2195 % 2ull;
        _if__result_3284 = _M0L6_2atmpS2194 == 0ull;
      } else {
        _if__result_3284 = 0;
      }
    } else {
      _if__result_3284 = 0;
    }
    if (_if__result_3284) {
      _M0Lm16lastRemovedDigitS843 = 4;
    }
    _M0L6_2atmpS2197 = _M0Lm2vrS818;
    _M0L6_2atmpS2203 = _M0Lm2vrS818;
    _M0L6_2atmpS2204 = _M0Lm2vmS820;
    if (_M0L6_2atmpS2203 == _M0L6_2atmpS2204) {
      if (!_M0L4evenS815) {
        _if__result_3285 = 1;
      } else {
        int32_t _M0L6_2atmpS2202 = _M0Lm17vmIsTrailingZerosS822;
        _if__result_3285 = !_M0L6_2atmpS2202;
      }
    } else {
      _if__result_3285 = 0;
    }
    if (_if__result_3285) {
      _M0L6_2atmpS2200 = 1;
    } else {
      int32_t _M0L6_2atmpS2201 = _M0Lm16lastRemovedDigitS843;
      _M0L6_2atmpS2200 = _M0L6_2atmpS2201 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2199 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2200);
    _M0L6_2atmpS2198 = *(uint64_t*)&_M0L6_2atmpS2199;
    _M0Lm6outputS844 = _M0L6_2atmpS2197 + _M0L6_2atmpS2198;
  } else {
    int32_t _M0Lm7roundUpS857 = 0;
    uint64_t _M0L6_2atmpS2225 = _M0Lm2vpS819;
    uint64_t _M0L8vpDiv100S858 = _M0L6_2atmpS2225 / 100ull;
    uint64_t _M0L6_2atmpS2224 = _M0Lm2vmS820;
    uint64_t _M0L8vmDiv100S859 = _M0L6_2atmpS2224 / 100ull;
    uint64_t _M0L6_2atmpS2219;
    uint64_t _M0L6_2atmpS2222;
    uint64_t _M0L6_2atmpS2223;
    int32_t _M0L6_2atmpS2221;
    uint64_t _M0L6_2atmpS2220;
    if (_M0L8vpDiv100S858 > _M0L8vmDiv100S859) {
      uint64_t _M0L6_2atmpS2210 = _M0Lm2vrS818;
      uint64_t _M0L8vrDiv100S860 = _M0L6_2atmpS2210 / 100ull;
      uint64_t _M0L6_2atmpS2209 = _M0Lm2vrS818;
      int32_t _M0L6_2atmpS2206 = (int32_t)_M0L6_2atmpS2209;
      int32_t _M0L6_2atmpS2208 = (int32_t)_M0L8vrDiv100S860;
      int32_t _M0L6_2atmpS2207 = 100 * _M0L6_2atmpS2208;
      int32_t _M0L8vrMod100S861 = _M0L6_2atmpS2206 - _M0L6_2atmpS2207;
      int32_t _M0L6_2atmpS2205;
      _M0Lm7roundUpS857 = _M0L8vrMod100S861 >= 50;
      _M0Lm2vrS818 = _M0L8vrDiv100S860;
      _M0Lm2vpS819 = _M0L8vpDiv100S858;
      _M0Lm2vmS820 = _M0L8vmDiv100S859;
      _M0L6_2atmpS2205 = _M0Lm7removedS842;
      _M0Lm7removedS842 = _M0L6_2atmpS2205 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2218 = _M0Lm2vpS819;
      uint64_t _M0L7vpDiv10S862 = _M0L6_2atmpS2218 / 10ull;
      uint64_t _M0L6_2atmpS2217 = _M0Lm2vmS820;
      uint64_t _M0L7vmDiv10S863 = _M0L6_2atmpS2217 / 10ull;
      uint64_t _M0L6_2atmpS2216;
      uint64_t _M0L7vrDiv10S865;
      uint64_t _M0L6_2atmpS2215;
      int32_t _M0L6_2atmpS2212;
      int32_t _M0L6_2atmpS2214;
      int32_t _M0L6_2atmpS2213;
      int32_t _M0L7vrMod10S866;
      int32_t _M0L6_2atmpS2211;
      if (_M0L7vpDiv10S862 <= _M0L7vmDiv10S863) {
        break;
      }
      _M0L6_2atmpS2216 = _M0Lm2vrS818;
      _M0L7vrDiv10S865 = _M0L6_2atmpS2216 / 10ull;
      _M0L6_2atmpS2215 = _M0Lm2vrS818;
      _M0L6_2atmpS2212 = (int32_t)_M0L6_2atmpS2215;
      _M0L6_2atmpS2214 = (int32_t)_M0L7vrDiv10S865;
      _M0L6_2atmpS2213 = 10 * _M0L6_2atmpS2214;
      _M0L7vrMod10S866 = _M0L6_2atmpS2212 - _M0L6_2atmpS2213;
      _M0Lm7roundUpS857 = _M0L7vrMod10S866 >= 5;
      _M0Lm2vrS818 = _M0L7vrDiv10S865;
      _M0Lm2vpS819 = _M0L7vpDiv10S862;
      _M0Lm2vmS820 = _M0L7vmDiv10S863;
      _M0L6_2atmpS2211 = _M0Lm7removedS842;
      _M0Lm7removedS842 = _M0L6_2atmpS2211 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2219 = _M0Lm2vrS818;
    _M0L6_2atmpS2222 = _M0Lm2vrS818;
    _M0L6_2atmpS2223 = _M0Lm2vmS820;
    _M0L6_2atmpS2221
    = _M0L6_2atmpS2222 == _M0L6_2atmpS2223 || _M0Lm7roundUpS857;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2220 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2221);
    _M0Lm6outputS844 = _M0L6_2atmpS2219 + _M0L6_2atmpS2220;
  }
  _M0L6_2atmpS2227 = _M0Lm3e10S821;
  _M0L6_2atmpS2228 = _M0Lm7removedS842;
  _M0L3expS867 = _M0L6_2atmpS2227 + _M0L6_2atmpS2228;
  _M0L6_2atmpS2226 = _M0Lm6outputS844;
  _block_3287
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3287)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3287->$0 = _M0L6_2atmpS2226;
  _block_3287->$1 = _M0L3expS867;
  return _block_3287;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS810) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS810) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS809) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS809) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS808) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS808) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS807) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS807 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS807 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS807 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS807 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS807 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS807 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS807 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS807 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS807 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS807 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS807 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS807 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS807 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS807 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS807 >= 100ull) {
    return 3;
  }
  if (_M0L1vS807 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS790) {
  int32_t _M0L6_2atmpS2127;
  int32_t _M0L6_2atmpS2126;
  int32_t _M0L4baseS789;
  int32_t _M0L5base2S791;
  int32_t _M0L6offsetS792;
  int32_t _M0L6_2atmpS2125;
  uint64_t _M0L4mul0S793;
  int32_t _M0L6_2atmpS2124;
  int32_t _M0L6_2atmpS2123;
  uint64_t _M0L4mul1S794;
  uint64_t _M0L1mS795;
  struct _M0TPB7Umul128 _M0L7_2abindS796;
  uint64_t _M0L7_2alow1S797;
  uint64_t _M0L8_2ahigh1S798;
  struct _M0TPB7Umul128 _M0L7_2abindS799;
  uint64_t _M0L7_2alow0S800;
  uint64_t _M0L8_2ahigh0S801;
  uint64_t _M0L3sumS802;
  uint64_t _M0Lm5high1S803;
  int32_t _M0L6_2atmpS2121;
  int32_t _M0L6_2atmpS2122;
  int32_t _M0L5deltaS804;
  uint64_t _M0L6_2atmpS2120;
  uint64_t _M0L6_2atmpS2112;
  int32_t _M0L6_2atmpS2119;
  uint32_t _M0L6_2atmpS2116;
  int32_t _M0L6_2atmpS2118;
  int32_t _M0L6_2atmpS2117;
  uint32_t _M0L6_2atmpS2115;
  uint32_t _M0L6_2atmpS2114;
  uint64_t _M0L6_2atmpS2113;
  uint64_t _M0L1aS805;
  uint64_t _M0L6_2atmpS2111;
  uint64_t _M0L1bS806;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2127 = _M0L1iS790 + 26;
  _M0L6_2atmpS2126 = _M0L6_2atmpS2127 - 1;
  _M0L4baseS789 = _M0L6_2atmpS2126 / 26;
  _M0L5base2S791 = _M0L4baseS789 * 26;
  _M0L6offsetS792 = _M0L5base2S791 - _M0L1iS790;
  _M0L6_2atmpS2125 = _M0L4baseS789 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S793
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2125);
  _M0L6_2atmpS2124 = _M0L4baseS789 * 2;
  _M0L6_2atmpS2123 = _M0L6_2atmpS2124 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S794
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2123);
  if (_M0L6offsetS792 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S793, _M0L4mul1S794};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS795
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS792);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS796 = _M0FPB7umul128(_M0L1mS795, _M0L4mul1S794);
  _M0L7_2alow1S797 = _M0L7_2abindS796.$0;
  _M0L8_2ahigh1S798 = _M0L7_2abindS796.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS799 = _M0FPB7umul128(_M0L1mS795, _M0L4mul0S793);
  _M0L7_2alow0S800 = _M0L7_2abindS799.$0;
  _M0L8_2ahigh0S801 = _M0L7_2abindS799.$1;
  _M0L3sumS802 = _M0L8_2ahigh0S801 + _M0L7_2alow1S797;
  _M0Lm5high1S803 = _M0L8_2ahigh1S798;
  if (_M0L3sumS802 < _M0L8_2ahigh0S801) {
    uint64_t _M0L6_2atmpS2110 = _M0Lm5high1S803;
    _M0Lm5high1S803 = _M0L6_2atmpS2110 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2121 = _M0FPB8pow5bits(_M0L5base2S791);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2122 = _M0FPB8pow5bits(_M0L1iS790);
  _M0L5deltaS804 = _M0L6_2atmpS2121 - _M0L6_2atmpS2122;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2120
  = _M0FPB13shiftright128(_M0L7_2alow0S800, _M0L3sumS802, _M0L5deltaS804);
  _M0L6_2atmpS2112 = _M0L6_2atmpS2120 + 1ull;
  _M0L6_2atmpS2119 = _M0L1iS790 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2116
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2119);
  _M0L6_2atmpS2118 = _M0L1iS790 % 16;
  _M0L6_2atmpS2117 = _M0L6_2atmpS2118 << 1;
  _M0L6_2atmpS2115 = _M0L6_2atmpS2116 >> (_M0L6_2atmpS2117 & 31);
  _M0L6_2atmpS2114 = _M0L6_2atmpS2115 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2113 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2114);
  _M0L1aS805 = _M0L6_2atmpS2112 + _M0L6_2atmpS2113;
  _M0L6_2atmpS2111 = _M0Lm5high1S803;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS806
  = _M0FPB13shiftright128(_M0L3sumS802, _M0L6_2atmpS2111, _M0L5deltaS804);
  return (struct _M0TPB8Pow5Pair){_M0L1aS805, _M0L1bS806};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS772) {
  int32_t _M0L4baseS771;
  int32_t _M0L5base2S773;
  int32_t _M0L6offsetS774;
  int32_t _M0L6_2atmpS2109;
  uint64_t _M0L4mul0S775;
  int32_t _M0L6_2atmpS2108;
  int32_t _M0L6_2atmpS2107;
  uint64_t _M0L4mul1S776;
  uint64_t _M0L1mS777;
  struct _M0TPB7Umul128 _M0L7_2abindS778;
  uint64_t _M0L7_2alow1S779;
  uint64_t _M0L8_2ahigh1S780;
  struct _M0TPB7Umul128 _M0L7_2abindS781;
  uint64_t _M0L7_2alow0S782;
  uint64_t _M0L8_2ahigh0S783;
  uint64_t _M0L3sumS784;
  uint64_t _M0Lm5high1S785;
  int32_t _M0L6_2atmpS2105;
  int32_t _M0L6_2atmpS2106;
  int32_t _M0L5deltaS786;
  uint64_t _M0L6_2atmpS2097;
  int32_t _M0L6_2atmpS2104;
  uint32_t _M0L6_2atmpS2101;
  int32_t _M0L6_2atmpS2103;
  int32_t _M0L6_2atmpS2102;
  uint32_t _M0L6_2atmpS2100;
  uint32_t _M0L6_2atmpS2099;
  uint64_t _M0L6_2atmpS2098;
  uint64_t _M0L1aS787;
  uint64_t _M0L6_2atmpS2096;
  uint64_t _M0L1bS788;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS771 = _M0L1iS772 / 26;
  _M0L5base2S773 = _M0L4baseS771 * 26;
  _M0L6offsetS774 = _M0L1iS772 - _M0L5base2S773;
  _M0L6_2atmpS2109 = _M0L4baseS771 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S775
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2109);
  _M0L6_2atmpS2108 = _M0L4baseS771 * 2;
  _M0L6_2atmpS2107 = _M0L6_2atmpS2108 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S776
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2107);
  if (_M0L6offsetS774 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S775, _M0L4mul1S776};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS777
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS774);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS778 = _M0FPB7umul128(_M0L1mS777, _M0L4mul1S776);
  _M0L7_2alow1S779 = _M0L7_2abindS778.$0;
  _M0L8_2ahigh1S780 = _M0L7_2abindS778.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS781 = _M0FPB7umul128(_M0L1mS777, _M0L4mul0S775);
  _M0L7_2alow0S782 = _M0L7_2abindS781.$0;
  _M0L8_2ahigh0S783 = _M0L7_2abindS781.$1;
  _M0L3sumS784 = _M0L8_2ahigh0S783 + _M0L7_2alow1S779;
  _M0Lm5high1S785 = _M0L8_2ahigh1S780;
  if (_M0L3sumS784 < _M0L8_2ahigh0S783) {
    uint64_t _M0L6_2atmpS2095 = _M0Lm5high1S785;
    _M0Lm5high1S785 = _M0L6_2atmpS2095 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2105 = _M0FPB8pow5bits(_M0L1iS772);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2106 = _M0FPB8pow5bits(_M0L5base2S773);
  _M0L5deltaS786 = _M0L6_2atmpS2105 - _M0L6_2atmpS2106;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2097
  = _M0FPB13shiftright128(_M0L7_2alow0S782, _M0L3sumS784, _M0L5deltaS786);
  _M0L6_2atmpS2104 = _M0L1iS772 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2101
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2104);
  _M0L6_2atmpS2103 = _M0L1iS772 % 16;
  _M0L6_2atmpS2102 = _M0L6_2atmpS2103 << 1;
  _M0L6_2atmpS2100 = _M0L6_2atmpS2101 >> (_M0L6_2atmpS2102 & 31);
  _M0L6_2atmpS2099 = _M0L6_2atmpS2100 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2098 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2099);
  _M0L1aS787 = _M0L6_2atmpS2097 + _M0L6_2atmpS2098;
  _M0L6_2atmpS2096 = _M0Lm5high1S785;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS788
  = _M0FPB13shiftright128(_M0L3sumS784, _M0L6_2atmpS2096, _M0L5deltaS786);
  return (struct _M0TPB8Pow5Pair){_M0L1aS787, _M0L1bS788};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS745,
  struct _M0TPB8Pow5Pair _M0L3mulS742,
  int32_t _M0L1jS758,
  int32_t _M0L7mmShiftS760
) {
  uint64_t _M0L7_2amul0S741;
  uint64_t _M0L7_2amul1S743;
  uint64_t _M0L1mS744;
  struct _M0TPB7Umul128 _M0L7_2abindS746;
  uint64_t _M0L5_2aloS747;
  uint64_t _M0L6_2atmpS748;
  struct _M0TPB7Umul128 _M0L7_2abindS749;
  uint64_t _M0L6_2alo2S750;
  uint64_t _M0L6_2ahi2S751;
  uint64_t _M0L3midS752;
  uint64_t _M0L6_2atmpS2094;
  uint64_t _M0L2hiS753;
  uint64_t _M0L3lo2S754;
  uint64_t _M0L6_2atmpS2092;
  uint64_t _M0L6_2atmpS2093;
  uint64_t _M0L4mid2S755;
  uint64_t _M0L6_2atmpS2091;
  uint64_t _M0L3hi2S756;
  int32_t _M0L6_2atmpS2090;
  int32_t _M0L6_2atmpS2089;
  uint64_t _M0L2vpS757;
  uint64_t _M0Lm2vmS759;
  int32_t _M0L6_2atmpS2088;
  int32_t _M0L6_2atmpS2087;
  uint64_t _M0L2vrS770;
  uint64_t _M0L6_2atmpS2086;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S741 = _M0L3mulS742.$0;
  _M0L7_2amul1S743 = _M0L3mulS742.$1;
  _M0L1mS744 = _M0L1mS745 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS746 = _M0FPB7umul128(_M0L1mS744, _M0L7_2amul0S741);
  _M0L5_2aloS747 = _M0L7_2abindS746.$0;
  _M0L6_2atmpS748 = _M0L7_2abindS746.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS749 = _M0FPB7umul128(_M0L1mS744, _M0L7_2amul1S743);
  _M0L6_2alo2S750 = _M0L7_2abindS749.$0;
  _M0L6_2ahi2S751 = _M0L7_2abindS749.$1;
  _M0L3midS752 = _M0L6_2atmpS748 + _M0L6_2alo2S750;
  if (_M0L3midS752 < _M0L6_2atmpS748) {
    _M0L6_2atmpS2094 = 1ull;
  } else {
    _M0L6_2atmpS2094 = 0ull;
  }
  _M0L2hiS753 = _M0L6_2ahi2S751 + _M0L6_2atmpS2094;
  _M0L3lo2S754 = _M0L5_2aloS747 + _M0L7_2amul0S741;
  _M0L6_2atmpS2092 = _M0L3midS752 + _M0L7_2amul1S743;
  if (_M0L3lo2S754 < _M0L5_2aloS747) {
    _M0L6_2atmpS2093 = 1ull;
  } else {
    _M0L6_2atmpS2093 = 0ull;
  }
  _M0L4mid2S755 = _M0L6_2atmpS2092 + _M0L6_2atmpS2093;
  if (_M0L4mid2S755 < _M0L3midS752) {
    _M0L6_2atmpS2091 = 1ull;
  } else {
    _M0L6_2atmpS2091 = 0ull;
  }
  _M0L3hi2S756 = _M0L2hiS753 + _M0L6_2atmpS2091;
  _M0L6_2atmpS2090 = _M0L1jS758 - 64;
  _M0L6_2atmpS2089 = _M0L6_2atmpS2090 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS757
  = _M0FPB13shiftright128(_M0L4mid2S755, _M0L3hi2S756, _M0L6_2atmpS2089);
  _M0Lm2vmS759 = 0ull;
  if (_M0L7mmShiftS760) {
    uint64_t _M0L3lo3S761 = _M0L5_2aloS747 - _M0L7_2amul0S741;
    uint64_t _M0L6_2atmpS2076 = _M0L3midS752 - _M0L7_2amul1S743;
    uint64_t _M0L6_2atmpS2077;
    uint64_t _M0L4mid3S762;
    uint64_t _M0L6_2atmpS2075;
    uint64_t _M0L3hi3S763;
    int32_t _M0L6_2atmpS2074;
    int32_t _M0L6_2atmpS2073;
    if (_M0L5_2aloS747 < _M0L3lo3S761) {
      _M0L6_2atmpS2077 = 1ull;
    } else {
      _M0L6_2atmpS2077 = 0ull;
    }
    _M0L4mid3S762 = _M0L6_2atmpS2076 - _M0L6_2atmpS2077;
    if (_M0L3midS752 < _M0L4mid3S762) {
      _M0L6_2atmpS2075 = 1ull;
    } else {
      _M0L6_2atmpS2075 = 0ull;
    }
    _M0L3hi3S763 = _M0L2hiS753 - _M0L6_2atmpS2075;
    _M0L6_2atmpS2074 = _M0L1jS758 - 64;
    _M0L6_2atmpS2073 = _M0L6_2atmpS2074 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS759
    = _M0FPB13shiftright128(_M0L4mid3S762, _M0L3hi3S763, _M0L6_2atmpS2073);
  } else {
    uint64_t _M0L3lo3S764 = _M0L5_2aloS747 + _M0L5_2aloS747;
    uint64_t _M0L6_2atmpS2084 = _M0L3midS752 + _M0L3midS752;
    uint64_t _M0L6_2atmpS2085;
    uint64_t _M0L4mid3S765;
    uint64_t _M0L6_2atmpS2082;
    uint64_t _M0L6_2atmpS2083;
    uint64_t _M0L3hi3S766;
    uint64_t _M0L3lo4S767;
    uint64_t _M0L6_2atmpS2080;
    uint64_t _M0L6_2atmpS2081;
    uint64_t _M0L4mid4S768;
    uint64_t _M0L6_2atmpS2079;
    uint64_t _M0L3hi4S769;
    int32_t _M0L6_2atmpS2078;
    if (_M0L3lo3S764 < _M0L5_2aloS747) {
      _M0L6_2atmpS2085 = 1ull;
    } else {
      _M0L6_2atmpS2085 = 0ull;
    }
    _M0L4mid3S765 = _M0L6_2atmpS2084 + _M0L6_2atmpS2085;
    _M0L6_2atmpS2082 = _M0L2hiS753 + _M0L2hiS753;
    if (_M0L4mid3S765 < _M0L3midS752) {
      _M0L6_2atmpS2083 = 1ull;
    } else {
      _M0L6_2atmpS2083 = 0ull;
    }
    _M0L3hi3S766 = _M0L6_2atmpS2082 + _M0L6_2atmpS2083;
    _M0L3lo4S767 = _M0L3lo3S764 - _M0L7_2amul0S741;
    _M0L6_2atmpS2080 = _M0L4mid3S765 - _M0L7_2amul1S743;
    if (_M0L3lo3S764 < _M0L3lo4S767) {
      _M0L6_2atmpS2081 = 1ull;
    } else {
      _M0L6_2atmpS2081 = 0ull;
    }
    _M0L4mid4S768 = _M0L6_2atmpS2080 - _M0L6_2atmpS2081;
    if (_M0L4mid3S765 < _M0L4mid4S768) {
      _M0L6_2atmpS2079 = 1ull;
    } else {
      _M0L6_2atmpS2079 = 0ull;
    }
    _M0L3hi4S769 = _M0L3hi3S766 - _M0L6_2atmpS2079;
    _M0L6_2atmpS2078 = _M0L1jS758 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS759
    = _M0FPB13shiftright128(_M0L4mid4S768, _M0L3hi4S769, _M0L6_2atmpS2078);
  }
  _M0L6_2atmpS2088 = _M0L1jS758 - 64;
  _M0L6_2atmpS2087 = _M0L6_2atmpS2088 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS770
  = _M0FPB13shiftright128(_M0L3midS752, _M0L2hiS753, _M0L6_2atmpS2087);
  _M0L6_2atmpS2086 = _M0Lm2vmS759;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS770,
                                                _M0L2vpS757,
                                                _M0L6_2atmpS2086};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS739,
  int32_t _M0L1pS740
) {
  uint64_t _M0L6_2atmpS2072;
  uint64_t _M0L6_2atmpS2071;
  uint64_t _M0L6_2atmpS2070;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2072 = 1ull << (_M0L1pS740 & 63);
  _M0L6_2atmpS2071 = _M0L6_2atmpS2072 - 1ull;
  _M0L6_2atmpS2070 = _M0L5valueS739 & _M0L6_2atmpS2071;
  return _M0L6_2atmpS2070 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS737,
  int32_t _M0L1pS738
) {
  int32_t _M0L6_2atmpS2069;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2069 = _M0FPB10pow5Factor(_M0L5valueS737);
  return _M0L6_2atmpS2069 >= _M0L1pS738;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS733) {
  uint64_t _M0L6_2atmpS2057;
  uint64_t _M0L6_2atmpS2058;
  uint64_t _M0L6_2atmpS2059;
  uint64_t _M0L6_2atmpS2060;
  int32_t _M0Lm5countS734;
  uint64_t _M0Lm5valueS735;
  uint64_t _M0L6_2atmpS2068;
  moonbit_string_t _M0L6_2atmpS2067;
  moonbit_string_t _M0L6_2atmpS2925;
  moonbit_string_t _M0L6_2atmpS2066;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2057 = _M0L5valueS733 % 5ull;
  if (_M0L6_2atmpS2057 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2058 = _M0L5valueS733 % 25ull;
  if (_M0L6_2atmpS2058 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2059 = _M0L5valueS733 % 125ull;
  if (_M0L6_2atmpS2059 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2060 = _M0L5valueS733 % 625ull;
  if (_M0L6_2atmpS2060 != 0ull) {
    return 3;
  }
  _M0Lm5countS734 = 4;
  _M0Lm5valueS735 = _M0L5valueS733 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2061 = _M0Lm5valueS735;
    if (_M0L6_2atmpS2061 > 0ull) {
      uint64_t _M0L6_2atmpS2063 = _M0Lm5valueS735;
      uint64_t _M0L6_2atmpS2062 = _M0L6_2atmpS2063 % 5ull;
      uint64_t _M0L6_2atmpS2064;
      int32_t _M0L6_2atmpS2065;
      if (_M0L6_2atmpS2062 != 0ull) {
        return _M0Lm5countS734;
      }
      _M0L6_2atmpS2064 = _M0Lm5valueS735;
      _M0Lm5valueS735 = _M0L6_2atmpS2064 / 5ull;
      _M0L6_2atmpS2065 = _M0Lm5countS734;
      _M0Lm5countS734 = _M0L6_2atmpS2065 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2068 = _M0Lm5valueS735;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2067
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2068);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2925
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_93.data, _M0L6_2atmpS2067);
  moonbit_decref(_M0L6_2atmpS2067);
  _M0L6_2atmpS2066 = _M0L6_2atmpS2925;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2066, (moonbit_string_t)moonbit_string_literal_94.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS732,
  uint64_t _M0L2hiS730,
  int32_t _M0L4distS731
) {
  int32_t _M0L6_2atmpS2056;
  uint64_t _M0L6_2atmpS2054;
  uint64_t _M0L6_2atmpS2055;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2056 = 64 - _M0L4distS731;
  _M0L6_2atmpS2054 = _M0L2hiS730 << (_M0L6_2atmpS2056 & 63);
  _M0L6_2atmpS2055 = _M0L2loS732 >> (_M0L4distS731 & 63);
  return _M0L6_2atmpS2054 | _M0L6_2atmpS2055;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS720,
  uint64_t _M0L1bS723
) {
  uint64_t _M0L3aLoS719;
  uint64_t _M0L3aHiS721;
  uint64_t _M0L3bLoS722;
  uint64_t _M0L3bHiS724;
  uint64_t _M0L1xS725;
  uint64_t _M0L6_2atmpS2052;
  uint64_t _M0L6_2atmpS2053;
  uint64_t _M0L1yS726;
  uint64_t _M0L6_2atmpS2050;
  uint64_t _M0L6_2atmpS2051;
  uint64_t _M0L1zS727;
  uint64_t _M0L6_2atmpS2048;
  uint64_t _M0L6_2atmpS2049;
  uint64_t _M0L6_2atmpS2046;
  uint64_t _M0L6_2atmpS2047;
  uint64_t _M0L1wS728;
  uint64_t _M0L2loS729;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS719 = _M0L1aS720 & 4294967295ull;
  _M0L3aHiS721 = _M0L1aS720 >> 32;
  _M0L3bLoS722 = _M0L1bS723 & 4294967295ull;
  _M0L3bHiS724 = _M0L1bS723 >> 32;
  _M0L1xS725 = _M0L3aLoS719 * _M0L3bLoS722;
  _M0L6_2atmpS2052 = _M0L3aHiS721 * _M0L3bLoS722;
  _M0L6_2atmpS2053 = _M0L1xS725 >> 32;
  _M0L1yS726 = _M0L6_2atmpS2052 + _M0L6_2atmpS2053;
  _M0L6_2atmpS2050 = _M0L3aLoS719 * _M0L3bHiS724;
  _M0L6_2atmpS2051 = _M0L1yS726 & 4294967295ull;
  _M0L1zS727 = _M0L6_2atmpS2050 + _M0L6_2atmpS2051;
  _M0L6_2atmpS2048 = _M0L3aHiS721 * _M0L3bHiS724;
  _M0L6_2atmpS2049 = _M0L1yS726 >> 32;
  _M0L6_2atmpS2046 = _M0L6_2atmpS2048 + _M0L6_2atmpS2049;
  _M0L6_2atmpS2047 = _M0L1zS727 >> 32;
  _M0L1wS728 = _M0L6_2atmpS2046 + _M0L6_2atmpS2047;
  _M0L2loS729 = _M0L1aS720 * _M0L1bS723;
  return (struct _M0TPB7Umul128){_M0L2loS729, _M0L1wS728};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS714,
  int32_t _M0L4fromS718,
  int32_t _M0L2toS716
) {
  int32_t _M0L6_2atmpS2045;
  struct _M0TPB13StringBuilder* _M0L3bufS713;
  int32_t _M0L1iS715;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2045 = Moonbit_array_length(_M0L5bytesS714);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS713 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2045);
  _M0L1iS715 = _M0L4fromS718;
  while (1) {
    if (_M0L1iS715 < _M0L2toS716) {
      int32_t _M0L6_2atmpS2043;
      int32_t _M0L6_2atmpS2042;
      int32_t _M0L6_2atmpS2044;
      if (
        _M0L1iS715 < 0 || _M0L1iS715 >= Moonbit_array_length(_M0L5bytesS714)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2043 = (int32_t)_M0L5bytesS714[_M0L1iS715];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2042 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2043);
      moonbit_incref(_M0L3bufS713);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS713, _M0L6_2atmpS2042);
      _M0L6_2atmpS2044 = _M0L1iS715 + 1;
      _M0L1iS715 = _M0L6_2atmpS2044;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS714);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS713);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS712) {
  int32_t _M0L6_2atmpS2041;
  uint32_t _M0L6_2atmpS2040;
  uint32_t _M0L6_2atmpS2039;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2041 = _M0L1eS712 * 78913;
  _M0L6_2atmpS2040 = *(uint32_t*)&_M0L6_2atmpS2041;
  _M0L6_2atmpS2039 = _M0L6_2atmpS2040 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2039;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS711) {
  int32_t _M0L6_2atmpS2038;
  uint32_t _M0L6_2atmpS2037;
  uint32_t _M0L6_2atmpS2036;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2038 = _M0L1eS711 * 732923;
  _M0L6_2atmpS2037 = *(uint32_t*)&_M0L6_2atmpS2038;
  _M0L6_2atmpS2036 = _M0L6_2atmpS2037 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2036;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS709,
  int32_t _M0L8exponentS710,
  int32_t _M0L8mantissaS707
) {
  moonbit_string_t _M0L1sS708;
  moonbit_string_t _M0L6_2atmpS2926;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS707) {
    return (moonbit_string_t)moonbit_string_literal_95.data;
  }
  if (_M0L4signS709) {
    _M0L1sS708 = (moonbit_string_t)moonbit_string_literal_96.data;
  } else {
    _M0L1sS708 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS710) {
    moonbit_string_t _M0L6_2atmpS2927;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2927
    = moonbit_add_string(_M0L1sS708, (moonbit_string_t)moonbit_string_literal_97.data);
    moonbit_decref(_M0L1sS708);
    return _M0L6_2atmpS2927;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2926
  = moonbit_add_string(_M0L1sS708, (moonbit_string_t)moonbit_string_literal_98.data);
  moonbit_decref(_M0L1sS708);
  return _M0L6_2atmpS2926;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS706) {
  int32_t _M0L6_2atmpS2035;
  uint32_t _M0L6_2atmpS2034;
  uint32_t _M0L6_2atmpS2033;
  int32_t _M0L6_2atmpS2032;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2035 = _M0L1eS706 * 1217359;
  _M0L6_2atmpS2034 = *(uint32_t*)&_M0L6_2atmpS2035;
  _M0L6_2atmpS2033 = _M0L6_2atmpS2034 >> 19;
  _M0L6_2atmpS2032 = *(int32_t*)&_M0L6_2atmpS2033;
  return _M0L6_2atmpS2032 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS705,
  struct _M0TPB6Hasher* _M0L6hasherS704
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS704, _M0L4selfS705);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS703,
  struct _M0TPB6Hasher* _M0L6hasherS702
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS702, _M0L4selfS703);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS700,
  moonbit_string_t _M0L5valueS698
) {
  int32_t _M0L7_2abindS697;
  int32_t _M0L1iS699;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS697 = Moonbit_array_length(_M0L5valueS698);
  _M0L1iS699 = 0;
  while (1) {
    if (_M0L1iS699 < _M0L7_2abindS697) {
      int32_t _M0L6_2atmpS2030 = _M0L5valueS698[_M0L1iS699];
      int32_t _M0L6_2atmpS2029 = (int32_t)_M0L6_2atmpS2030;
      uint32_t _M0L6_2atmpS2028 = *(uint32_t*)&_M0L6_2atmpS2029;
      int32_t _M0L6_2atmpS2031;
      moonbit_incref(_M0L4selfS700);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS700, _M0L6_2atmpS2028);
      _M0L6_2atmpS2031 = _M0L1iS699 + 1;
      _M0L1iS699 = _M0L6_2atmpS2031;
      continue;
    } else {
      moonbit_decref(_M0L4selfS700);
      moonbit_decref(_M0L5valueS698);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS695,
  int32_t _M0L3idxS696
) {
  int32_t _M0L6_2atmpS2928;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2928 = _M0L4selfS695[_M0L3idxS696];
  moonbit_decref(_M0L4selfS695);
  return _M0L6_2atmpS2928;
}

void* _M0IPC16option6OptionPB6ToJson8to__jsonGiE(int64_t _M0L4selfS689) {
  #line 287 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS689 == 4294967296ll) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    int64_t _M0L7_2aSomeS690 = _M0L4selfS689;
    int32_t _M0L8_2avalueS691 = (int32_t)_M0L7_2aSomeS690;
    void* _M0L6_2atmpS2024;
    void** _M0L6_2atmpS2023;
    struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2022;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L6_2atmpS2024 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_2avalueS691);
    _M0L6_2atmpS2023 = (void**)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS2023[0] = _M0L6_2atmpS2024;
    _M0L6_2atmpS2022
    = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
    Moonbit_object_header(_M0L6_2atmpS2022)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2022->$0 = _M0L6_2atmpS2023;
    _M0L6_2atmpS2022->$1 = 1;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json5array(_M0L6_2atmpS2022);
  }
}

void* _M0IPC16option6OptionPB6ToJson8to__jsonGsE(
  moonbit_string_t _M0L4selfS692
) {
  #line 287 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS692 == 0) {
    if (_M0L4selfS692) {
      moonbit_decref(_M0L4selfS692);
    }
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    moonbit_string_t _M0L7_2aSomeS693 = _M0L4selfS692;
    moonbit_string_t _M0L8_2avalueS694 = _M0L7_2aSomeS693;
    void* _M0L6_2atmpS2027;
    void** _M0L6_2atmpS2026;
    struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2025;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L6_2atmpS2027
    = _M0IPC16string6StringPB6ToJson8to__json(_M0L8_2avalueS694);
    _M0L6_2atmpS2026 = (void**)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS2026[0] = _M0L6_2atmpS2027;
    _M0L6_2atmpS2025
    = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
    Moonbit_object_header(_M0L6_2atmpS2025)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2025->$0 = _M0L6_2atmpS2026;
    _M0L6_2atmpS2025->$1 = 1;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json5array(_M0L6_2atmpS2025);
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS688) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS688;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS687) {
  double _M0L6_2atmpS2020;
  moonbit_string_t _M0L6_2atmpS2021;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2020 = (double)_M0L4selfS687;
  _M0L6_2atmpS2021 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2020, _M0L6_2atmpS2021);
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS686) {
  void* _block_3291;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3291 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3291)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3291)->$0 = _M0L6stringS686;
  return _block_3291;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS679
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2929;
  int32_t _M0L6_2acntS3151;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2019;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS678;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__* _closure_3292;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2014;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2929 = _M0L4selfS679->$5;
  _M0L6_2acntS3151 = Moonbit_object_header(_M0L4selfS679)->rc;
  if (_M0L6_2acntS3151 > 1) {
    int32_t _M0L11_2anew__cntS3153 = _M0L6_2acntS3151 - 1;
    Moonbit_object_header(_M0L4selfS679)->rc = _M0L11_2anew__cntS3153;
    if (_M0L8_2afieldS2929) {
      moonbit_incref(_M0L8_2afieldS2929);
    }
  } else if (_M0L6_2acntS3151 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3152 = _M0L4selfS679->$0;
    moonbit_decref(_M0L8_2afieldS3152);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS679);
  }
  _M0L4headS2019 = _M0L8_2afieldS2929;
  _M0L11curr__entryS678
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS678)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS678->$0 = _M0L4headS2019;
  _closure_3292
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__));
  Moonbit_object_header(_closure_3292)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__, $0) >> 2, 1, 0);
  _closure_3292->code = &_M0MPB3Map4iterGsRPB4JsonEC2015l591;
  _closure_3292->$0 = _M0L11curr__entryS678;
  _M0L6_2atmpS2014 = (struct _M0TWEOUsRPB4JsonE*)_closure_3292;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2014);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2015l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2016
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__* _M0L14_2acasted__envS2017;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS2935;
  int32_t _M0L6_2acntS3154;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS678;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2934;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS680;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2017
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2015__l591__*)_M0L6_2aenvS2016;
  _M0L8_2afieldS2935 = _M0L14_2acasted__envS2017->$0;
  _M0L6_2acntS3154 = Moonbit_object_header(_M0L14_2acasted__envS2017)->rc;
  if (_M0L6_2acntS3154 > 1) {
    int32_t _M0L11_2anew__cntS3155 = _M0L6_2acntS3154 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2017)->rc
    = _M0L11_2anew__cntS3155;
    moonbit_incref(_M0L8_2afieldS2935);
  } else if (_M0L6_2acntS3154 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2017);
  }
  _M0L11curr__entryS678 = _M0L8_2afieldS2935;
  _M0L8_2afieldS2934 = _M0L11curr__entryS678->$0;
  _M0L7_2abindS680 = _M0L8_2afieldS2934;
  if (_M0L7_2abindS680 == 0) {
    moonbit_decref(_M0L11curr__entryS678);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS681 = _M0L7_2abindS680;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS682 = _M0L7_2aSomeS681;
    moonbit_string_t _M0L8_2afieldS2933 = _M0L4_2axS682->$4;
    moonbit_string_t _M0L6_2akeyS683 = _M0L8_2afieldS2933;
    void* _M0L8_2afieldS2932 = _M0L4_2axS682->$5;
    void* _M0L8_2avalueS684 = _M0L8_2afieldS2932;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2931 = _M0L4_2axS682->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS685 = _M0L8_2afieldS2931;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2930 =
      _M0L11curr__entryS678->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2018;
    if (_M0L7_2anextS685) {
      moonbit_incref(_M0L7_2anextS685);
    }
    moonbit_incref(_M0L8_2avalueS684);
    moonbit_incref(_M0L6_2akeyS683);
    if (_M0L6_2aoldS2930) {
      moonbit_decref(_M0L6_2aoldS2930);
    }
    _M0L11curr__entryS678->$0 = _M0L7_2anextS685;
    moonbit_decref(_M0L11curr__entryS678);
    _M0L8_2atupleS2018
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2018)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2018->$0 = _M0L6_2akeyS683;
    _M0L8_2atupleS2018->$1 = _M0L8_2avalueS684;
    return _M0L8_2atupleS2018;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS677
) {
  int32_t _M0L8_2afieldS2936;
  int32_t _M0L4sizeS2013;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2936 = _M0L4selfS677->$1;
  moonbit_decref(_M0L4selfS677);
  _M0L4sizeS2013 = _M0L8_2afieldS2936;
  return _M0L4sizeS2013 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS664,
  int32_t _M0L3keyS660
) {
  int32_t _M0L4hashS659;
  int32_t _M0L14capacity__maskS1998;
  int32_t _M0L6_2atmpS1997;
  int32_t _M0L1iS661;
  int32_t _M0L3idxS662;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS659 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS660);
  _M0L14capacity__maskS1998 = _M0L4selfS664->$3;
  _M0L6_2atmpS1997 = _M0L4hashS659 & _M0L14capacity__maskS1998;
  _M0L1iS661 = 0;
  _M0L3idxS662 = _M0L6_2atmpS1997;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2940 =
      _M0L4selfS664->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1996 =
      _M0L8_2afieldS2940;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2939;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS663;
    if (
      _M0L3idxS662 < 0
      || _M0L3idxS662 >= Moonbit_array_length(_M0L7entriesS1996)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2939
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1996[
        _M0L3idxS662
      ];
    _M0L7_2abindS663 = _M0L6_2atmpS2939;
    if (_M0L7_2abindS663 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1985;
      if (_M0L7_2abindS663) {
        moonbit_incref(_M0L7_2abindS663);
      }
      moonbit_decref(_M0L4selfS664);
      if (_M0L7_2abindS663) {
        moonbit_decref(_M0L7_2abindS663);
      }
      _M0L6_2atmpS1985 = 0;
      return _M0L6_2atmpS1985;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS665 =
        _M0L7_2abindS663;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS666 =
        _M0L7_2aSomeS665;
      int32_t _M0L4hashS1987 = _M0L8_2aentryS666->$3;
      int32_t _if__result_3294;
      int32_t _M0L8_2afieldS2937;
      int32_t _M0L3pslS1990;
      int32_t _M0L6_2atmpS1992;
      int32_t _M0L6_2atmpS1994;
      int32_t _M0L14capacity__maskS1995;
      int32_t _M0L6_2atmpS1993;
      if (_M0L4hashS1987 == _M0L4hashS659) {
        int32_t _M0L3keyS1986 = _M0L8_2aentryS666->$4;
        _if__result_3294 = _M0L3keyS1986 == _M0L3keyS660;
      } else {
        _if__result_3294 = 0;
      }
      if (_if__result_3294) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2938;
        int32_t _M0L6_2acntS3156;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1989;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1988;
        moonbit_incref(_M0L8_2aentryS666);
        moonbit_decref(_M0L4selfS664);
        _M0L8_2afieldS2938 = _M0L8_2aentryS666->$5;
        _M0L6_2acntS3156 = Moonbit_object_header(_M0L8_2aentryS666)->rc;
        if (_M0L6_2acntS3156 > 1) {
          int32_t _M0L11_2anew__cntS3158 = _M0L6_2acntS3156 - 1;
          Moonbit_object_header(_M0L8_2aentryS666)->rc
          = _M0L11_2anew__cntS3158;
          moonbit_incref(_M0L8_2afieldS2938);
        } else if (_M0L6_2acntS3156 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3157 =
            _M0L8_2aentryS666->$1;
          if (_M0L8_2afieldS3157) {
            moonbit_decref(_M0L8_2afieldS3157);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS666);
        }
        _M0L5valueS1989 = _M0L8_2afieldS2938;
        _M0L6_2atmpS1988 = _M0L5valueS1989;
        return _M0L6_2atmpS1988;
      } else {
        moonbit_incref(_M0L8_2aentryS666);
      }
      _M0L8_2afieldS2937 = _M0L8_2aentryS666->$2;
      moonbit_decref(_M0L8_2aentryS666);
      _M0L3pslS1990 = _M0L8_2afieldS2937;
      if (_M0L1iS661 > _M0L3pslS1990) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1991;
        moonbit_decref(_M0L4selfS664);
        _M0L6_2atmpS1991 = 0;
        return _M0L6_2atmpS1991;
      }
      _M0L6_2atmpS1992 = _M0L1iS661 + 1;
      _M0L6_2atmpS1994 = _M0L3idxS662 + 1;
      _M0L14capacity__maskS1995 = _M0L4selfS664->$3;
      _M0L6_2atmpS1993 = _M0L6_2atmpS1994 & _M0L14capacity__maskS1995;
      _M0L1iS661 = _M0L6_2atmpS1992;
      _M0L3idxS662 = _M0L6_2atmpS1993;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS673,
  moonbit_string_t _M0L3keyS669
) {
  int32_t _M0L4hashS668;
  int32_t _M0L14capacity__maskS2012;
  int32_t _M0L6_2atmpS2011;
  int32_t _M0L1iS670;
  int32_t _M0L3idxS671;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS669);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS668 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS669);
  _M0L14capacity__maskS2012 = _M0L4selfS673->$3;
  _M0L6_2atmpS2011 = _M0L4hashS668 & _M0L14capacity__maskS2012;
  _M0L1iS670 = 0;
  _M0L3idxS671 = _M0L6_2atmpS2011;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2946 =
      _M0L4selfS673->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2010 =
      _M0L8_2afieldS2946;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2945;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS672;
    if (
      _M0L3idxS671 < 0
      || _M0L3idxS671 >= Moonbit_array_length(_M0L7entriesS2010)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2945
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2010[
        _M0L3idxS671
      ];
    _M0L7_2abindS672 = _M0L6_2atmpS2945;
    if (_M0L7_2abindS672 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1999;
      if (_M0L7_2abindS672) {
        moonbit_incref(_M0L7_2abindS672);
      }
      moonbit_decref(_M0L4selfS673);
      if (_M0L7_2abindS672) {
        moonbit_decref(_M0L7_2abindS672);
      }
      moonbit_decref(_M0L3keyS669);
      _M0L6_2atmpS1999 = 0;
      return _M0L6_2atmpS1999;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS674 =
        _M0L7_2abindS672;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS675 =
        _M0L7_2aSomeS674;
      int32_t _M0L4hashS2001 = _M0L8_2aentryS675->$3;
      int32_t _if__result_3296;
      int32_t _M0L8_2afieldS2941;
      int32_t _M0L3pslS2004;
      int32_t _M0L6_2atmpS2006;
      int32_t _M0L6_2atmpS2008;
      int32_t _M0L14capacity__maskS2009;
      int32_t _M0L6_2atmpS2007;
      if (_M0L4hashS2001 == _M0L4hashS668) {
        moonbit_string_t _M0L8_2afieldS2944 = _M0L8_2aentryS675->$4;
        moonbit_string_t _M0L3keyS2000 = _M0L8_2afieldS2944;
        int32_t _M0L6_2atmpS2943;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2943
        = moonbit_val_array_equal(_M0L3keyS2000, _M0L3keyS669);
        _if__result_3296 = _M0L6_2atmpS2943;
      } else {
        _if__result_3296 = 0;
      }
      if (_if__result_3296) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2942;
        int32_t _M0L6_2acntS3159;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2003;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2002;
        moonbit_incref(_M0L8_2aentryS675);
        moonbit_decref(_M0L4selfS673);
        moonbit_decref(_M0L3keyS669);
        _M0L8_2afieldS2942 = _M0L8_2aentryS675->$5;
        _M0L6_2acntS3159 = Moonbit_object_header(_M0L8_2aentryS675)->rc;
        if (_M0L6_2acntS3159 > 1) {
          int32_t _M0L11_2anew__cntS3162 = _M0L6_2acntS3159 - 1;
          Moonbit_object_header(_M0L8_2aentryS675)->rc
          = _M0L11_2anew__cntS3162;
          moonbit_incref(_M0L8_2afieldS2942);
        } else if (_M0L6_2acntS3159 == 1) {
          moonbit_string_t _M0L8_2afieldS3161 = _M0L8_2aentryS675->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3160;
          moonbit_decref(_M0L8_2afieldS3161);
          _M0L8_2afieldS3160 = _M0L8_2aentryS675->$1;
          if (_M0L8_2afieldS3160) {
            moonbit_decref(_M0L8_2afieldS3160);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS675);
        }
        _M0L5valueS2003 = _M0L8_2afieldS2942;
        _M0L6_2atmpS2002 = _M0L5valueS2003;
        return _M0L6_2atmpS2002;
      } else {
        moonbit_incref(_M0L8_2aentryS675);
      }
      _M0L8_2afieldS2941 = _M0L8_2aentryS675->$2;
      moonbit_decref(_M0L8_2aentryS675);
      _M0L3pslS2004 = _M0L8_2afieldS2941;
      if (_M0L1iS670 > _M0L3pslS2004) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2005;
        moonbit_decref(_M0L4selfS673);
        moonbit_decref(_M0L3keyS669);
        _M0L6_2atmpS2005 = 0;
        return _M0L6_2atmpS2005;
      }
      _M0L6_2atmpS2006 = _M0L1iS670 + 1;
      _M0L6_2atmpS2008 = _M0L3idxS671 + 1;
      _M0L14capacity__maskS2009 = _M0L4selfS673->$3;
      _M0L6_2atmpS2007 = _M0L6_2atmpS2008 & _M0L14capacity__maskS2009;
      _M0L1iS670 = _M0L6_2atmpS2006;
      _M0L3idxS671 = _M0L6_2atmpS2007;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS644
) {
  int32_t _M0L6lengthS643;
  int32_t _M0Lm8capacityS645;
  int32_t _M0L6_2atmpS1962;
  int32_t _M0L6_2atmpS1961;
  int32_t _M0L6_2atmpS1972;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS646;
  int32_t _M0L3endS1970;
  int32_t _M0L5startS1971;
  int32_t _M0L7_2abindS647;
  int32_t _M0L2__S648;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS644.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS643
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS644);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS645 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS643);
  _M0L6_2atmpS1962 = _M0Lm8capacityS645;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1961 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1962);
  if (_M0L6lengthS643 > _M0L6_2atmpS1961) {
    int32_t _M0L6_2atmpS1963 = _M0Lm8capacityS645;
    _M0Lm8capacityS645 = _M0L6_2atmpS1963 * 2;
  }
  _M0L6_2atmpS1972 = _M0Lm8capacityS645;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS646
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1972);
  _M0L3endS1970 = _M0L3arrS644.$2;
  _M0L5startS1971 = _M0L3arrS644.$1;
  _M0L7_2abindS647 = _M0L3endS1970 - _M0L5startS1971;
  _M0L2__S648 = 0;
  while (1) {
    if (_M0L2__S648 < _M0L7_2abindS647) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2950 =
        _M0L3arrS644.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1967 =
        _M0L8_2afieldS2950;
      int32_t _M0L5startS1969 = _M0L3arrS644.$1;
      int32_t _M0L6_2atmpS1968 = _M0L5startS1969 + _M0L2__S648;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2949 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1967[
          _M0L6_2atmpS1968
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS649 =
        _M0L6_2atmpS2949;
      moonbit_string_t _M0L8_2afieldS2948 = _M0L1eS649->$0;
      moonbit_string_t _M0L6_2atmpS1964 = _M0L8_2afieldS2948;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2947 =
        _M0L1eS649->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1965 =
        _M0L8_2afieldS2947;
      int32_t _M0L6_2atmpS1966;
      moonbit_incref(_M0L6_2atmpS1965);
      moonbit_incref(_M0L6_2atmpS1964);
      moonbit_incref(_M0L1mS646);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS646, _M0L6_2atmpS1964, _M0L6_2atmpS1965);
      _M0L6_2atmpS1966 = _M0L2__S648 + 1;
      _M0L2__S648 = _M0L6_2atmpS1966;
      continue;
    } else {
      moonbit_decref(_M0L3arrS644.$0);
    }
    break;
  }
  return _M0L1mS646;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS652
) {
  int32_t _M0L6lengthS651;
  int32_t _M0Lm8capacityS653;
  int32_t _M0L6_2atmpS1974;
  int32_t _M0L6_2atmpS1973;
  int32_t _M0L6_2atmpS1984;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS654;
  int32_t _M0L3endS1982;
  int32_t _M0L5startS1983;
  int32_t _M0L7_2abindS655;
  int32_t _M0L2__S656;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS652.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS651
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS652);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS653 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS651);
  _M0L6_2atmpS1974 = _M0Lm8capacityS653;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1973 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1974);
  if (_M0L6lengthS651 > _M0L6_2atmpS1973) {
    int32_t _M0L6_2atmpS1975 = _M0Lm8capacityS653;
    _M0Lm8capacityS653 = _M0L6_2atmpS1975 * 2;
  }
  _M0L6_2atmpS1984 = _M0Lm8capacityS653;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS654
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1984);
  _M0L3endS1982 = _M0L3arrS652.$2;
  _M0L5startS1983 = _M0L3arrS652.$1;
  _M0L7_2abindS655 = _M0L3endS1982 - _M0L5startS1983;
  _M0L2__S656 = 0;
  while (1) {
    if (_M0L2__S656 < _M0L7_2abindS655) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2953 =
        _M0L3arrS652.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1979 =
        _M0L8_2afieldS2953;
      int32_t _M0L5startS1981 = _M0L3arrS652.$1;
      int32_t _M0L6_2atmpS1980 = _M0L5startS1981 + _M0L2__S656;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2952 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1979[
          _M0L6_2atmpS1980
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS657 = _M0L6_2atmpS2952;
      int32_t _M0L6_2atmpS1976 = _M0L1eS657->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2951 =
        _M0L1eS657->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1977 =
        _M0L8_2afieldS2951;
      int32_t _M0L6_2atmpS1978;
      moonbit_incref(_M0L6_2atmpS1977);
      moonbit_incref(_M0L1mS654);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS654, _M0L6_2atmpS1976, _M0L6_2atmpS1977);
      _M0L6_2atmpS1978 = _M0L2__S656 + 1;
      _M0L2__S656 = _M0L6_2atmpS1978;
      continue;
    } else {
      moonbit_decref(_M0L3arrS652.$0);
    }
    break;
  }
  return _M0L1mS654;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS637,
  moonbit_string_t _M0L3keyS638,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS639
) {
  int32_t _M0L6_2atmpS1959;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS638);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1959 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS638);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS637, _M0L3keyS638, _M0L5valueS639, _M0L6_2atmpS1959);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS640,
  int32_t _M0L3keyS641,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS642
) {
  int32_t _M0L6_2atmpS1960;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1960 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS641);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS640, _M0L3keyS641, _M0L5valueS642, _M0L6_2atmpS1960);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS616
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2960;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS615;
  int32_t _M0L8capacityS1951;
  int32_t _M0L13new__capacityS617;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1946;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1945;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2959;
  int32_t _M0L6_2atmpS1947;
  int32_t _M0L8capacityS1949;
  int32_t _M0L6_2atmpS1948;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1950;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2958;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS618;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2960 = _M0L4selfS616->$5;
  _M0L9old__headS615 = _M0L8_2afieldS2960;
  _M0L8capacityS1951 = _M0L4selfS616->$2;
  _M0L13new__capacityS617 = _M0L8capacityS1951 << 1;
  _M0L6_2atmpS1946 = 0;
  _M0L6_2atmpS1945
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS617, _M0L6_2atmpS1946);
  _M0L6_2aoldS2959 = _M0L4selfS616->$0;
  if (_M0L9old__headS615) {
    moonbit_incref(_M0L9old__headS615);
  }
  moonbit_decref(_M0L6_2aoldS2959);
  _M0L4selfS616->$0 = _M0L6_2atmpS1945;
  _M0L4selfS616->$2 = _M0L13new__capacityS617;
  _M0L6_2atmpS1947 = _M0L13new__capacityS617 - 1;
  _M0L4selfS616->$3 = _M0L6_2atmpS1947;
  _M0L8capacityS1949 = _M0L4selfS616->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1948 = _M0FPB21calc__grow__threshold(_M0L8capacityS1949);
  _M0L4selfS616->$4 = _M0L6_2atmpS1948;
  _M0L4selfS616->$1 = 0;
  _M0L6_2atmpS1950 = 0;
  _M0L6_2aoldS2958 = _M0L4selfS616->$5;
  if (_M0L6_2aoldS2958) {
    moonbit_decref(_M0L6_2aoldS2958);
  }
  _M0L4selfS616->$5 = _M0L6_2atmpS1950;
  _M0L4selfS616->$6 = -1;
  _M0L8_2aparamS618 = _M0L9old__headS615;
  while (1) {
    if (_M0L8_2aparamS618 == 0) {
      if (_M0L8_2aparamS618) {
        moonbit_decref(_M0L8_2aparamS618);
      }
      moonbit_decref(_M0L4selfS616);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS619 =
        _M0L8_2aparamS618;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS620 =
        _M0L7_2aSomeS619;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2957 =
        _M0L4_2axS620->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS621 =
        _M0L8_2afieldS2957;
      moonbit_string_t _M0L8_2afieldS2956 = _M0L4_2axS620->$4;
      moonbit_string_t _M0L6_2akeyS622 = _M0L8_2afieldS2956;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2955 =
        _M0L4_2axS620->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS623 =
        _M0L8_2afieldS2955;
      int32_t _M0L8_2afieldS2954 = _M0L4_2axS620->$3;
      int32_t _M0L6_2acntS3163 = Moonbit_object_header(_M0L4_2axS620)->rc;
      int32_t _M0L7_2ahashS624;
      if (_M0L6_2acntS3163 > 1) {
        int32_t _M0L11_2anew__cntS3164 = _M0L6_2acntS3163 - 1;
        Moonbit_object_header(_M0L4_2axS620)->rc = _M0L11_2anew__cntS3164;
        moonbit_incref(_M0L8_2avalueS623);
        moonbit_incref(_M0L6_2akeyS622);
        if (_M0L7_2anextS621) {
          moonbit_incref(_M0L7_2anextS621);
        }
      } else if (_M0L6_2acntS3163 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS620);
      }
      _M0L7_2ahashS624 = _M0L8_2afieldS2954;
      moonbit_incref(_M0L4selfS616);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS616, _M0L6_2akeyS622, _M0L8_2avalueS623, _M0L7_2ahashS624);
      _M0L8_2aparamS618 = _M0L7_2anextS621;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS627
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2966;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS626;
  int32_t _M0L8capacityS1958;
  int32_t _M0L13new__capacityS628;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1953;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1952;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2965;
  int32_t _M0L6_2atmpS1954;
  int32_t _M0L8capacityS1956;
  int32_t _M0L6_2atmpS1955;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1957;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2964;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS629;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2966 = _M0L4selfS627->$5;
  _M0L9old__headS626 = _M0L8_2afieldS2966;
  _M0L8capacityS1958 = _M0L4selfS627->$2;
  _M0L13new__capacityS628 = _M0L8capacityS1958 << 1;
  _M0L6_2atmpS1953 = 0;
  _M0L6_2atmpS1952
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS628, _M0L6_2atmpS1953);
  _M0L6_2aoldS2965 = _M0L4selfS627->$0;
  if (_M0L9old__headS626) {
    moonbit_incref(_M0L9old__headS626);
  }
  moonbit_decref(_M0L6_2aoldS2965);
  _M0L4selfS627->$0 = _M0L6_2atmpS1952;
  _M0L4selfS627->$2 = _M0L13new__capacityS628;
  _M0L6_2atmpS1954 = _M0L13new__capacityS628 - 1;
  _M0L4selfS627->$3 = _M0L6_2atmpS1954;
  _M0L8capacityS1956 = _M0L4selfS627->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1955 = _M0FPB21calc__grow__threshold(_M0L8capacityS1956);
  _M0L4selfS627->$4 = _M0L6_2atmpS1955;
  _M0L4selfS627->$1 = 0;
  _M0L6_2atmpS1957 = 0;
  _M0L6_2aoldS2964 = _M0L4selfS627->$5;
  if (_M0L6_2aoldS2964) {
    moonbit_decref(_M0L6_2aoldS2964);
  }
  _M0L4selfS627->$5 = _M0L6_2atmpS1957;
  _M0L4selfS627->$6 = -1;
  _M0L8_2aparamS629 = _M0L9old__headS626;
  while (1) {
    if (_M0L8_2aparamS629 == 0) {
      if (_M0L8_2aparamS629) {
        moonbit_decref(_M0L8_2aparamS629);
      }
      moonbit_decref(_M0L4selfS627);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS630 =
        _M0L8_2aparamS629;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS631 =
        _M0L7_2aSomeS630;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2963 =
        _M0L4_2axS631->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS632 =
        _M0L8_2afieldS2963;
      int32_t _M0L6_2akeyS633 = _M0L4_2axS631->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2962 =
        _M0L4_2axS631->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS634 =
        _M0L8_2afieldS2962;
      int32_t _M0L8_2afieldS2961 = _M0L4_2axS631->$3;
      int32_t _M0L6_2acntS3165 = Moonbit_object_header(_M0L4_2axS631)->rc;
      int32_t _M0L7_2ahashS635;
      if (_M0L6_2acntS3165 > 1) {
        int32_t _M0L11_2anew__cntS3166 = _M0L6_2acntS3165 - 1;
        Moonbit_object_header(_M0L4_2axS631)->rc = _M0L11_2anew__cntS3166;
        moonbit_incref(_M0L8_2avalueS634);
        if (_M0L7_2anextS632) {
          moonbit_incref(_M0L7_2anextS632);
        }
      } else if (_M0L6_2acntS3165 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS631);
      }
      _M0L7_2ahashS635 = _M0L8_2afieldS2961;
      moonbit_incref(_M0L4selfS627);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L6_2akeyS633, _M0L8_2avalueS634, _M0L7_2ahashS635);
      _M0L8_2aparamS629 = _M0L7_2anextS632;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS586,
  moonbit_string_t _M0L3keyS592,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS593,
  int32_t _M0L4hashS588
) {
  int32_t _M0L14capacity__maskS1926;
  int32_t _M0L6_2atmpS1925;
  int32_t _M0L3pslS583;
  int32_t _M0L3idxS584;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1926 = _M0L4selfS586->$3;
  _M0L6_2atmpS1925 = _M0L4hashS588 & _M0L14capacity__maskS1926;
  _M0L3pslS583 = 0;
  _M0L3idxS584 = _M0L6_2atmpS1925;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2971 =
      _M0L4selfS586->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1924 =
      _M0L8_2afieldS2971;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2970;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS585;
    if (
      _M0L3idxS584 < 0
      || _M0L3idxS584 >= Moonbit_array_length(_M0L7entriesS1924)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2970
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1924[
        _M0L3idxS584
      ];
    _M0L7_2abindS585 = _M0L6_2atmpS2970;
    if (_M0L7_2abindS585 == 0) {
      int32_t _M0L4sizeS1909 = _M0L4selfS586->$1;
      int32_t _M0L8grow__atS1910 = _M0L4selfS586->$4;
      int32_t _M0L7_2abindS589;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS590;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS591;
      if (_M0L4sizeS1909 >= _M0L8grow__atS1910) {
        int32_t _M0L14capacity__maskS1912;
        int32_t _M0L6_2atmpS1911;
        moonbit_incref(_M0L4selfS586);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS586);
        _M0L14capacity__maskS1912 = _M0L4selfS586->$3;
        _M0L6_2atmpS1911 = _M0L4hashS588 & _M0L14capacity__maskS1912;
        _M0L3pslS583 = 0;
        _M0L3idxS584 = _M0L6_2atmpS1911;
        continue;
      }
      _M0L7_2abindS589 = _M0L4selfS586->$6;
      _M0L7_2abindS590 = 0;
      _M0L5entryS591
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS591)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS591->$0 = _M0L7_2abindS589;
      _M0L5entryS591->$1 = _M0L7_2abindS590;
      _M0L5entryS591->$2 = _M0L3pslS583;
      _M0L5entryS591->$3 = _M0L4hashS588;
      _M0L5entryS591->$4 = _M0L3keyS592;
      _M0L5entryS591->$5 = _M0L5valueS593;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS586, _M0L3idxS584, _M0L5entryS591);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS594 =
        _M0L7_2abindS585;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS595 =
        _M0L7_2aSomeS594;
      int32_t _M0L4hashS1914 = _M0L14_2acurr__entryS595->$3;
      int32_t _if__result_3302;
      int32_t _M0L3pslS1915;
      int32_t _M0L6_2atmpS1920;
      int32_t _M0L6_2atmpS1922;
      int32_t _M0L14capacity__maskS1923;
      int32_t _M0L6_2atmpS1921;
      if (_M0L4hashS1914 == _M0L4hashS588) {
        moonbit_string_t _M0L8_2afieldS2969 = _M0L14_2acurr__entryS595->$4;
        moonbit_string_t _M0L3keyS1913 = _M0L8_2afieldS2969;
        int32_t _M0L6_2atmpS2968;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2968
        = moonbit_val_array_equal(_M0L3keyS1913, _M0L3keyS592);
        _if__result_3302 = _M0L6_2atmpS2968;
      } else {
        _if__result_3302 = 0;
      }
      if (_if__result_3302) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2967;
        moonbit_incref(_M0L14_2acurr__entryS595);
        moonbit_decref(_M0L3keyS592);
        moonbit_decref(_M0L4selfS586);
        _M0L6_2aoldS2967 = _M0L14_2acurr__entryS595->$5;
        moonbit_decref(_M0L6_2aoldS2967);
        _M0L14_2acurr__entryS595->$5 = _M0L5valueS593;
        moonbit_decref(_M0L14_2acurr__entryS595);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS595);
      }
      _M0L3pslS1915 = _M0L14_2acurr__entryS595->$2;
      if (_M0L3pslS583 > _M0L3pslS1915) {
        int32_t _M0L4sizeS1916 = _M0L4selfS586->$1;
        int32_t _M0L8grow__atS1917 = _M0L4selfS586->$4;
        int32_t _M0L7_2abindS596;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS597;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS598;
        if (_M0L4sizeS1916 >= _M0L8grow__atS1917) {
          int32_t _M0L14capacity__maskS1919;
          int32_t _M0L6_2atmpS1918;
          moonbit_decref(_M0L14_2acurr__entryS595);
          moonbit_incref(_M0L4selfS586);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS586);
          _M0L14capacity__maskS1919 = _M0L4selfS586->$3;
          _M0L6_2atmpS1918 = _M0L4hashS588 & _M0L14capacity__maskS1919;
          _M0L3pslS583 = 0;
          _M0L3idxS584 = _M0L6_2atmpS1918;
          continue;
        }
        moonbit_incref(_M0L4selfS586);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS586, _M0L3idxS584, _M0L14_2acurr__entryS595);
        _M0L7_2abindS596 = _M0L4selfS586->$6;
        _M0L7_2abindS597 = 0;
        _M0L5entryS598
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS598)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS598->$0 = _M0L7_2abindS596;
        _M0L5entryS598->$1 = _M0L7_2abindS597;
        _M0L5entryS598->$2 = _M0L3pslS583;
        _M0L5entryS598->$3 = _M0L4hashS588;
        _M0L5entryS598->$4 = _M0L3keyS592;
        _M0L5entryS598->$5 = _M0L5valueS593;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS586, _M0L3idxS584, _M0L5entryS598);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS595);
      }
      _M0L6_2atmpS1920 = _M0L3pslS583 + 1;
      _M0L6_2atmpS1922 = _M0L3idxS584 + 1;
      _M0L14capacity__maskS1923 = _M0L4selfS586->$3;
      _M0L6_2atmpS1921 = _M0L6_2atmpS1922 & _M0L14capacity__maskS1923;
      _M0L3pslS583 = _M0L6_2atmpS1920;
      _M0L3idxS584 = _M0L6_2atmpS1921;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS602,
  int32_t _M0L3keyS608,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS609,
  int32_t _M0L4hashS604
) {
  int32_t _M0L14capacity__maskS1944;
  int32_t _M0L6_2atmpS1943;
  int32_t _M0L3pslS599;
  int32_t _M0L3idxS600;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1944 = _M0L4selfS602->$3;
  _M0L6_2atmpS1943 = _M0L4hashS604 & _M0L14capacity__maskS1944;
  _M0L3pslS599 = 0;
  _M0L3idxS600 = _M0L6_2atmpS1943;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2974 =
      _M0L4selfS602->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1942 =
      _M0L8_2afieldS2974;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2973;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS601;
    if (
      _M0L3idxS600 < 0
      || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS1942)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2973
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1942[
        _M0L3idxS600
      ];
    _M0L7_2abindS601 = _M0L6_2atmpS2973;
    if (_M0L7_2abindS601 == 0) {
      int32_t _M0L4sizeS1927 = _M0L4selfS602->$1;
      int32_t _M0L8grow__atS1928 = _M0L4selfS602->$4;
      int32_t _M0L7_2abindS605;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS606;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS607;
      if (_M0L4sizeS1927 >= _M0L8grow__atS1928) {
        int32_t _M0L14capacity__maskS1930;
        int32_t _M0L6_2atmpS1929;
        moonbit_incref(_M0L4selfS602);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS602);
        _M0L14capacity__maskS1930 = _M0L4selfS602->$3;
        _M0L6_2atmpS1929 = _M0L4hashS604 & _M0L14capacity__maskS1930;
        _M0L3pslS599 = 0;
        _M0L3idxS600 = _M0L6_2atmpS1929;
        continue;
      }
      _M0L7_2abindS605 = _M0L4selfS602->$6;
      _M0L7_2abindS606 = 0;
      _M0L5entryS607
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS607)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS607->$0 = _M0L7_2abindS605;
      _M0L5entryS607->$1 = _M0L7_2abindS606;
      _M0L5entryS607->$2 = _M0L3pslS599;
      _M0L5entryS607->$3 = _M0L4hashS604;
      _M0L5entryS607->$4 = _M0L3keyS608;
      _M0L5entryS607->$5 = _M0L5valueS609;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS602, _M0L3idxS600, _M0L5entryS607);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS610 =
        _M0L7_2abindS601;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS611 =
        _M0L7_2aSomeS610;
      int32_t _M0L4hashS1932 = _M0L14_2acurr__entryS611->$3;
      int32_t _if__result_3304;
      int32_t _M0L3pslS1933;
      int32_t _M0L6_2atmpS1938;
      int32_t _M0L6_2atmpS1940;
      int32_t _M0L14capacity__maskS1941;
      int32_t _M0L6_2atmpS1939;
      if (_M0L4hashS1932 == _M0L4hashS604) {
        int32_t _M0L3keyS1931 = _M0L14_2acurr__entryS611->$4;
        _if__result_3304 = _M0L3keyS1931 == _M0L3keyS608;
      } else {
        _if__result_3304 = 0;
      }
      if (_if__result_3304) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2972;
        moonbit_incref(_M0L14_2acurr__entryS611);
        moonbit_decref(_M0L4selfS602);
        _M0L6_2aoldS2972 = _M0L14_2acurr__entryS611->$5;
        moonbit_decref(_M0L6_2aoldS2972);
        _M0L14_2acurr__entryS611->$5 = _M0L5valueS609;
        moonbit_decref(_M0L14_2acurr__entryS611);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS611);
      }
      _M0L3pslS1933 = _M0L14_2acurr__entryS611->$2;
      if (_M0L3pslS599 > _M0L3pslS1933) {
        int32_t _M0L4sizeS1934 = _M0L4selfS602->$1;
        int32_t _M0L8grow__atS1935 = _M0L4selfS602->$4;
        int32_t _M0L7_2abindS612;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS613;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS614;
        if (_M0L4sizeS1934 >= _M0L8grow__atS1935) {
          int32_t _M0L14capacity__maskS1937;
          int32_t _M0L6_2atmpS1936;
          moonbit_decref(_M0L14_2acurr__entryS611);
          moonbit_incref(_M0L4selfS602);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS602);
          _M0L14capacity__maskS1937 = _M0L4selfS602->$3;
          _M0L6_2atmpS1936 = _M0L4hashS604 & _M0L14capacity__maskS1937;
          _M0L3pslS599 = 0;
          _M0L3idxS600 = _M0L6_2atmpS1936;
          continue;
        }
        moonbit_incref(_M0L4selfS602);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS602, _M0L3idxS600, _M0L14_2acurr__entryS611);
        _M0L7_2abindS612 = _M0L4selfS602->$6;
        _M0L7_2abindS613 = 0;
        _M0L5entryS614
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS614)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS614->$0 = _M0L7_2abindS612;
        _M0L5entryS614->$1 = _M0L7_2abindS613;
        _M0L5entryS614->$2 = _M0L3pslS599;
        _M0L5entryS614->$3 = _M0L4hashS604;
        _M0L5entryS614->$4 = _M0L3keyS608;
        _M0L5entryS614->$5 = _M0L5valueS609;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS602, _M0L3idxS600, _M0L5entryS614);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS611);
      }
      _M0L6_2atmpS1938 = _M0L3pslS599 + 1;
      _M0L6_2atmpS1940 = _M0L3idxS600 + 1;
      _M0L14capacity__maskS1941 = _M0L4selfS602->$3;
      _M0L6_2atmpS1939 = _M0L6_2atmpS1940 & _M0L14capacity__maskS1941;
      _M0L3pslS599 = _M0L6_2atmpS1938;
      _M0L3idxS600 = _M0L6_2atmpS1939;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS567,
  int32_t _M0L3idxS572,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS571
) {
  int32_t _M0L3pslS1892;
  int32_t _M0L6_2atmpS1888;
  int32_t _M0L6_2atmpS1890;
  int32_t _M0L14capacity__maskS1891;
  int32_t _M0L6_2atmpS1889;
  int32_t _M0L3pslS563;
  int32_t _M0L3idxS564;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS565;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1892 = _M0L5entryS571->$2;
  _M0L6_2atmpS1888 = _M0L3pslS1892 + 1;
  _M0L6_2atmpS1890 = _M0L3idxS572 + 1;
  _M0L14capacity__maskS1891 = _M0L4selfS567->$3;
  _M0L6_2atmpS1889 = _M0L6_2atmpS1890 & _M0L14capacity__maskS1891;
  _M0L3pslS563 = _M0L6_2atmpS1888;
  _M0L3idxS564 = _M0L6_2atmpS1889;
  _M0L5entryS565 = _M0L5entryS571;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2976 =
      _M0L4selfS567->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1887 =
      _M0L8_2afieldS2976;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2975;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS566;
    if (
      _M0L3idxS564 < 0
      || _M0L3idxS564 >= Moonbit_array_length(_M0L7entriesS1887)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2975
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1887[
        _M0L3idxS564
      ];
    _M0L7_2abindS566 = _M0L6_2atmpS2975;
    if (_M0L7_2abindS566 == 0) {
      _M0L5entryS565->$2 = _M0L3pslS563;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS567, _M0L5entryS565, _M0L3idxS564);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS569 =
        _M0L7_2abindS566;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS570 =
        _M0L7_2aSomeS569;
      int32_t _M0L3pslS1877 = _M0L14_2acurr__entryS570->$2;
      if (_M0L3pslS563 > _M0L3pslS1877) {
        int32_t _M0L3pslS1882;
        int32_t _M0L6_2atmpS1878;
        int32_t _M0L6_2atmpS1880;
        int32_t _M0L14capacity__maskS1881;
        int32_t _M0L6_2atmpS1879;
        _M0L5entryS565->$2 = _M0L3pslS563;
        moonbit_incref(_M0L14_2acurr__entryS570);
        moonbit_incref(_M0L4selfS567);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS567, _M0L5entryS565, _M0L3idxS564);
        _M0L3pslS1882 = _M0L14_2acurr__entryS570->$2;
        _M0L6_2atmpS1878 = _M0L3pslS1882 + 1;
        _M0L6_2atmpS1880 = _M0L3idxS564 + 1;
        _M0L14capacity__maskS1881 = _M0L4selfS567->$3;
        _M0L6_2atmpS1879 = _M0L6_2atmpS1880 & _M0L14capacity__maskS1881;
        _M0L3pslS563 = _M0L6_2atmpS1878;
        _M0L3idxS564 = _M0L6_2atmpS1879;
        _M0L5entryS565 = _M0L14_2acurr__entryS570;
        continue;
      } else {
        int32_t _M0L6_2atmpS1883 = _M0L3pslS563 + 1;
        int32_t _M0L6_2atmpS1885 = _M0L3idxS564 + 1;
        int32_t _M0L14capacity__maskS1886 = _M0L4selfS567->$3;
        int32_t _M0L6_2atmpS1884 =
          _M0L6_2atmpS1885 & _M0L14capacity__maskS1886;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3306 =
          _M0L5entryS565;
        _M0L3pslS563 = _M0L6_2atmpS1883;
        _M0L3idxS564 = _M0L6_2atmpS1884;
        _M0L5entryS565 = _tmp_3306;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS577,
  int32_t _M0L3idxS582,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS581
) {
  int32_t _M0L3pslS1908;
  int32_t _M0L6_2atmpS1904;
  int32_t _M0L6_2atmpS1906;
  int32_t _M0L14capacity__maskS1907;
  int32_t _M0L6_2atmpS1905;
  int32_t _M0L3pslS573;
  int32_t _M0L3idxS574;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS575;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1908 = _M0L5entryS581->$2;
  _M0L6_2atmpS1904 = _M0L3pslS1908 + 1;
  _M0L6_2atmpS1906 = _M0L3idxS582 + 1;
  _M0L14capacity__maskS1907 = _M0L4selfS577->$3;
  _M0L6_2atmpS1905 = _M0L6_2atmpS1906 & _M0L14capacity__maskS1907;
  _M0L3pslS573 = _M0L6_2atmpS1904;
  _M0L3idxS574 = _M0L6_2atmpS1905;
  _M0L5entryS575 = _M0L5entryS581;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2978 =
      _M0L4selfS577->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1903 =
      _M0L8_2afieldS2978;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2977;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS576;
    if (
      _M0L3idxS574 < 0
      || _M0L3idxS574 >= Moonbit_array_length(_M0L7entriesS1903)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2977
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1903[
        _M0L3idxS574
      ];
    _M0L7_2abindS576 = _M0L6_2atmpS2977;
    if (_M0L7_2abindS576 == 0) {
      _M0L5entryS575->$2 = _M0L3pslS573;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS577, _M0L5entryS575, _M0L3idxS574);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS579 =
        _M0L7_2abindS576;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS580 =
        _M0L7_2aSomeS579;
      int32_t _M0L3pslS1893 = _M0L14_2acurr__entryS580->$2;
      if (_M0L3pslS573 > _M0L3pslS1893) {
        int32_t _M0L3pslS1898;
        int32_t _M0L6_2atmpS1894;
        int32_t _M0L6_2atmpS1896;
        int32_t _M0L14capacity__maskS1897;
        int32_t _M0L6_2atmpS1895;
        _M0L5entryS575->$2 = _M0L3pslS573;
        moonbit_incref(_M0L14_2acurr__entryS580);
        moonbit_incref(_M0L4selfS577);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS577, _M0L5entryS575, _M0L3idxS574);
        _M0L3pslS1898 = _M0L14_2acurr__entryS580->$2;
        _M0L6_2atmpS1894 = _M0L3pslS1898 + 1;
        _M0L6_2atmpS1896 = _M0L3idxS574 + 1;
        _M0L14capacity__maskS1897 = _M0L4selfS577->$3;
        _M0L6_2atmpS1895 = _M0L6_2atmpS1896 & _M0L14capacity__maskS1897;
        _M0L3pslS573 = _M0L6_2atmpS1894;
        _M0L3idxS574 = _M0L6_2atmpS1895;
        _M0L5entryS575 = _M0L14_2acurr__entryS580;
        continue;
      } else {
        int32_t _M0L6_2atmpS1899 = _M0L3pslS573 + 1;
        int32_t _M0L6_2atmpS1901 = _M0L3idxS574 + 1;
        int32_t _M0L14capacity__maskS1902 = _M0L4selfS577->$3;
        int32_t _M0L6_2atmpS1900 =
          _M0L6_2atmpS1901 & _M0L14capacity__maskS1902;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3308 =
          _M0L5entryS575;
        _M0L3pslS573 = _M0L6_2atmpS1899;
        _M0L3idxS574 = _M0L6_2atmpS1900;
        _M0L5entryS575 = _tmp_3308;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS551,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS553,
  int32_t _M0L8new__idxS552
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2981;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1873;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1874;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2980;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2979;
  int32_t _M0L6_2acntS3167;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS554;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2981 = _M0L4selfS551->$0;
  _M0L7entriesS1873 = _M0L8_2afieldS2981;
  moonbit_incref(_M0L5entryS553);
  _M0L6_2atmpS1874 = _M0L5entryS553;
  if (
    _M0L8new__idxS552 < 0
    || _M0L8new__idxS552 >= Moonbit_array_length(_M0L7entriesS1873)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2980
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1873[
      _M0L8new__idxS552
    ];
  if (_M0L6_2aoldS2980) {
    moonbit_decref(_M0L6_2aoldS2980);
  }
  _M0L7entriesS1873[_M0L8new__idxS552] = _M0L6_2atmpS1874;
  _M0L8_2afieldS2979 = _M0L5entryS553->$1;
  _M0L6_2acntS3167 = Moonbit_object_header(_M0L5entryS553)->rc;
  if (_M0L6_2acntS3167 > 1) {
    int32_t _M0L11_2anew__cntS3170 = _M0L6_2acntS3167 - 1;
    Moonbit_object_header(_M0L5entryS553)->rc = _M0L11_2anew__cntS3170;
    if (_M0L8_2afieldS2979) {
      moonbit_incref(_M0L8_2afieldS2979);
    }
  } else if (_M0L6_2acntS3167 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3169 =
      _M0L5entryS553->$5;
    moonbit_string_t _M0L8_2afieldS3168;
    moonbit_decref(_M0L8_2afieldS3169);
    _M0L8_2afieldS3168 = _M0L5entryS553->$4;
    moonbit_decref(_M0L8_2afieldS3168);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS553);
  }
  _M0L7_2abindS554 = _M0L8_2afieldS2979;
  if (_M0L7_2abindS554 == 0) {
    if (_M0L7_2abindS554) {
      moonbit_decref(_M0L7_2abindS554);
    }
    _M0L4selfS551->$6 = _M0L8new__idxS552;
    moonbit_decref(_M0L4selfS551);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS555;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS556;
    moonbit_decref(_M0L4selfS551);
    _M0L7_2aSomeS555 = _M0L7_2abindS554;
    _M0L7_2anextS556 = _M0L7_2aSomeS555;
    _M0L7_2anextS556->$0 = _M0L8new__idxS552;
    moonbit_decref(_M0L7_2anextS556);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS557,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS559,
  int32_t _M0L8new__idxS558
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2984;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1875;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1876;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2983;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2982;
  int32_t _M0L6_2acntS3171;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS560;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2984 = _M0L4selfS557->$0;
  _M0L7entriesS1875 = _M0L8_2afieldS2984;
  moonbit_incref(_M0L5entryS559);
  _M0L6_2atmpS1876 = _M0L5entryS559;
  if (
    _M0L8new__idxS558 < 0
    || _M0L8new__idxS558 >= Moonbit_array_length(_M0L7entriesS1875)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2983
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1875[
      _M0L8new__idxS558
    ];
  if (_M0L6_2aoldS2983) {
    moonbit_decref(_M0L6_2aoldS2983);
  }
  _M0L7entriesS1875[_M0L8new__idxS558] = _M0L6_2atmpS1876;
  _M0L8_2afieldS2982 = _M0L5entryS559->$1;
  _M0L6_2acntS3171 = Moonbit_object_header(_M0L5entryS559)->rc;
  if (_M0L6_2acntS3171 > 1) {
    int32_t _M0L11_2anew__cntS3173 = _M0L6_2acntS3171 - 1;
    Moonbit_object_header(_M0L5entryS559)->rc = _M0L11_2anew__cntS3173;
    if (_M0L8_2afieldS2982) {
      moonbit_incref(_M0L8_2afieldS2982);
    }
  } else if (_M0L6_2acntS3171 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3172 =
      _M0L5entryS559->$5;
    moonbit_decref(_M0L8_2afieldS3172);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS559);
  }
  _M0L7_2abindS560 = _M0L8_2afieldS2982;
  if (_M0L7_2abindS560 == 0) {
    if (_M0L7_2abindS560) {
      moonbit_decref(_M0L7_2abindS560);
    }
    _M0L4selfS557->$6 = _M0L8new__idxS558;
    moonbit_decref(_M0L4selfS557);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS561;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS562;
    moonbit_decref(_M0L4selfS557);
    _M0L7_2aSomeS561 = _M0L7_2abindS560;
    _M0L7_2anextS562 = _M0L7_2aSomeS561;
    _M0L7_2anextS562->$0 = _M0L8new__idxS558;
    moonbit_decref(_M0L7_2anextS562);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS544,
  int32_t _M0L3idxS546,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS545
) {
  int32_t _M0L7_2abindS543;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2986;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1860;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1861;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2985;
  int32_t _M0L4sizeS1863;
  int32_t _M0L6_2atmpS1862;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS543 = _M0L4selfS544->$6;
  switch (_M0L7_2abindS543) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1855;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2987;
      moonbit_incref(_M0L5entryS545);
      _M0L6_2atmpS1855 = _M0L5entryS545;
      _M0L6_2aoldS2987 = _M0L4selfS544->$5;
      if (_M0L6_2aoldS2987) {
        moonbit_decref(_M0L6_2aoldS2987);
      }
      _M0L4selfS544->$5 = _M0L6_2atmpS1855;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2990 =
        _M0L4selfS544->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1859 =
        _M0L8_2afieldS2990;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2989;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1858;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1856;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1857;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2988;
      if (
        _M0L7_2abindS543 < 0
        || _M0L7_2abindS543 >= Moonbit_array_length(_M0L7entriesS1859)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2989
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1859[
          _M0L7_2abindS543
        ];
      _M0L6_2atmpS1858 = _M0L6_2atmpS2989;
      if (_M0L6_2atmpS1858) {
        moonbit_incref(_M0L6_2atmpS1858);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1856
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1858);
      moonbit_incref(_M0L5entryS545);
      _M0L6_2atmpS1857 = _M0L5entryS545;
      _M0L6_2aoldS2988 = _M0L6_2atmpS1856->$1;
      if (_M0L6_2aoldS2988) {
        moonbit_decref(_M0L6_2aoldS2988);
      }
      _M0L6_2atmpS1856->$1 = _M0L6_2atmpS1857;
      moonbit_decref(_M0L6_2atmpS1856);
      break;
    }
  }
  _M0L4selfS544->$6 = _M0L3idxS546;
  _M0L8_2afieldS2986 = _M0L4selfS544->$0;
  _M0L7entriesS1860 = _M0L8_2afieldS2986;
  _M0L6_2atmpS1861 = _M0L5entryS545;
  if (
    _M0L3idxS546 < 0
    || _M0L3idxS546 >= Moonbit_array_length(_M0L7entriesS1860)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2985
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1860[
      _M0L3idxS546
    ];
  if (_M0L6_2aoldS2985) {
    moonbit_decref(_M0L6_2aoldS2985);
  }
  _M0L7entriesS1860[_M0L3idxS546] = _M0L6_2atmpS1861;
  _M0L4sizeS1863 = _M0L4selfS544->$1;
  _M0L6_2atmpS1862 = _M0L4sizeS1863 + 1;
  _M0L4selfS544->$1 = _M0L6_2atmpS1862;
  moonbit_decref(_M0L4selfS544);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS548,
  int32_t _M0L3idxS550,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS549
) {
  int32_t _M0L7_2abindS547;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2992;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1869;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1870;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2991;
  int32_t _M0L4sizeS1872;
  int32_t _M0L6_2atmpS1871;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS547 = _M0L4selfS548->$6;
  switch (_M0L7_2abindS547) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1864;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2993;
      moonbit_incref(_M0L5entryS549);
      _M0L6_2atmpS1864 = _M0L5entryS549;
      _M0L6_2aoldS2993 = _M0L4selfS548->$5;
      if (_M0L6_2aoldS2993) {
        moonbit_decref(_M0L6_2aoldS2993);
      }
      _M0L4selfS548->$5 = _M0L6_2atmpS1864;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2996 =
        _M0L4selfS548->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1868 =
        _M0L8_2afieldS2996;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2995;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1867;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1865;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1866;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2994;
      if (
        _M0L7_2abindS547 < 0
        || _M0L7_2abindS547 >= Moonbit_array_length(_M0L7entriesS1868)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2995
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1868[
          _M0L7_2abindS547
        ];
      _M0L6_2atmpS1867 = _M0L6_2atmpS2995;
      if (_M0L6_2atmpS1867) {
        moonbit_incref(_M0L6_2atmpS1867);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1865
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1867);
      moonbit_incref(_M0L5entryS549);
      _M0L6_2atmpS1866 = _M0L5entryS549;
      _M0L6_2aoldS2994 = _M0L6_2atmpS1865->$1;
      if (_M0L6_2aoldS2994) {
        moonbit_decref(_M0L6_2aoldS2994);
      }
      _M0L6_2atmpS1865->$1 = _M0L6_2atmpS1866;
      moonbit_decref(_M0L6_2atmpS1865);
      break;
    }
  }
  _M0L4selfS548->$6 = _M0L3idxS550;
  _M0L8_2afieldS2992 = _M0L4selfS548->$0;
  _M0L7entriesS1869 = _M0L8_2afieldS2992;
  _M0L6_2atmpS1870 = _M0L5entryS549;
  if (
    _M0L3idxS550 < 0
    || _M0L3idxS550 >= Moonbit_array_length(_M0L7entriesS1869)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2991
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1869[
      _M0L3idxS550
    ];
  if (_M0L6_2aoldS2991) {
    moonbit_decref(_M0L6_2aoldS2991);
  }
  _M0L7entriesS1869[_M0L3idxS550] = _M0L6_2atmpS1870;
  _M0L4sizeS1872 = _M0L4selfS548->$1;
  _M0L6_2atmpS1871 = _M0L4sizeS1872 + 1;
  _M0L4selfS548->$1 = _M0L6_2atmpS1871;
  moonbit_decref(_M0L4selfS548);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS532
) {
  int32_t _M0L8capacityS531;
  int32_t _M0L7_2abindS533;
  int32_t _M0L7_2abindS534;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1853;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS535;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS536;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3309;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS531
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS532);
  _M0L7_2abindS533 = _M0L8capacityS531 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS534 = _M0FPB21calc__grow__threshold(_M0L8capacityS531);
  _M0L6_2atmpS1853 = 0;
  _M0L7_2abindS535
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS531, _M0L6_2atmpS1853);
  _M0L7_2abindS536 = 0;
  _block_3309
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3309)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3309->$0 = _M0L7_2abindS535;
  _block_3309->$1 = 0;
  _block_3309->$2 = _M0L8capacityS531;
  _block_3309->$3 = _M0L7_2abindS533;
  _block_3309->$4 = _M0L7_2abindS534;
  _block_3309->$5 = _M0L7_2abindS536;
  _block_3309->$6 = -1;
  return _block_3309;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS538
) {
  int32_t _M0L8capacityS537;
  int32_t _M0L7_2abindS539;
  int32_t _M0L7_2abindS540;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1854;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS541;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS542;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3310;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS537
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS538);
  _M0L7_2abindS539 = _M0L8capacityS537 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS540 = _M0FPB21calc__grow__threshold(_M0L8capacityS537);
  _M0L6_2atmpS1854 = 0;
  _M0L7_2abindS541
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS537, _M0L6_2atmpS1854);
  _M0L7_2abindS542 = 0;
  _block_3310
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3310)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3310->$0 = _M0L7_2abindS541;
  _block_3310->$1 = 0;
  _block_3310->$2 = _M0L8capacityS537;
  _block_3310->$3 = _M0L7_2abindS539;
  _block_3310->$4 = _M0L7_2abindS540;
  _block_3310->$5 = _M0L7_2abindS542;
  _block_3310->$6 = -1;
  return _block_3310;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS530) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS530 >= 0) {
    int32_t _M0L6_2atmpS1852;
    int32_t _M0L6_2atmpS1851;
    int32_t _M0L6_2atmpS1850;
    int32_t _M0L6_2atmpS1849;
    if (_M0L4selfS530 <= 1) {
      return 1;
    }
    if (_M0L4selfS530 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1852 = _M0L4selfS530 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1851 = moonbit_clz32(_M0L6_2atmpS1852);
    _M0L6_2atmpS1850 = _M0L6_2atmpS1851 - 1;
    _M0L6_2atmpS1849 = 2147483647 >> (_M0L6_2atmpS1850 & 31);
    return _M0L6_2atmpS1849 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS529) {
  int32_t _M0L6_2atmpS1848;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1848 = _M0L8capacityS529 * 13;
  return _M0L6_2atmpS1848 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS525
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS525 == 0) {
    if (_M0L4selfS525) {
      moonbit_decref(_M0L4selfS525);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS526 =
      _M0L4selfS525;
    return _M0L7_2aSomeS526;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS527
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS527 == 0) {
    if (_M0L4selfS527) {
      moonbit_decref(_M0L4selfS527);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS528 =
      _M0L4selfS527;
    return _M0L7_2aSomeS528;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS524
) {
  moonbit_string_t* _M0L6_2atmpS1847;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1847 = _M0L4selfS524;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1847);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS520,
  int32_t _M0L5indexS521
) {
  uint64_t* _M0L6_2atmpS1845;
  uint64_t _M0L6_2atmpS2997;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1845 = _M0L4selfS520;
  if (
    _M0L5indexS521 < 0
    || _M0L5indexS521 >= Moonbit_array_length(_M0L6_2atmpS1845)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2997 = (uint64_t)_M0L6_2atmpS1845[_M0L5indexS521];
  moonbit_decref(_M0L6_2atmpS1845);
  return _M0L6_2atmpS2997;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS522,
  int32_t _M0L5indexS523
) {
  uint32_t* _M0L6_2atmpS1846;
  uint32_t _M0L6_2atmpS2998;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1846 = _M0L4selfS522;
  if (
    _M0L5indexS523 < 0
    || _M0L5indexS523 >= Moonbit_array_length(_M0L6_2atmpS1846)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2998 = (uint32_t)_M0L6_2atmpS1846[_M0L5indexS523];
  moonbit_decref(_M0L6_2atmpS1846);
  return _M0L6_2atmpS2998;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS519
) {
  moonbit_string_t* _M0L6_2atmpS1843;
  int32_t _M0L6_2atmpS2999;
  int32_t _M0L6_2atmpS1844;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1842;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS519);
  _M0L6_2atmpS1843 = _M0L4selfS519;
  _M0L6_2atmpS2999 = Moonbit_array_length(_M0L4selfS519);
  moonbit_decref(_M0L4selfS519);
  _M0L6_2atmpS1844 = _M0L6_2atmpS2999;
  _M0L6_2atmpS1842
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1844, _M0L6_2atmpS1843
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1842);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS517
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS516;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__* _closure_3311;
  struct _M0TWEOs* _M0L6_2atmpS1830;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS516
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS516)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS516->$0 = 0;
  _closure_3311
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__));
  Moonbit_object_header(_closure_3311)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__, $0_0) >> 2, 2, 0);
  _closure_3311->code = &_M0MPC15array9ArrayView4iterGsEC1831l570;
  _closure_3311->$0_0 = _M0L4selfS517.$0;
  _closure_3311->$0_1 = _M0L4selfS517.$1;
  _closure_3311->$0_2 = _M0L4selfS517.$2;
  _closure_3311->$1 = _M0L1iS516;
  _M0L6_2atmpS1830 = (struct _M0TWEOs*)_closure_3311;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1830);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1831l570(
  struct _M0TWEOs* _M0L6_2aenvS1832
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__* _M0L14_2acasted__envS1833;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3004;
  struct _M0TPC13ref3RefGiE* _M0L1iS516;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3003;
  int32_t _M0L6_2acntS3174;
  struct _M0TPB9ArrayViewGsE _M0L4selfS517;
  int32_t _M0L3valS1834;
  int32_t _M0L6_2atmpS1835;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1833
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1831__l570__*)_M0L6_2aenvS1832;
  _M0L8_2afieldS3004 = _M0L14_2acasted__envS1833->$1;
  _M0L1iS516 = _M0L8_2afieldS3004;
  _M0L8_2afieldS3003
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1833->$0_1,
      _M0L14_2acasted__envS1833->$0_2,
      _M0L14_2acasted__envS1833->$0_0
  };
  _M0L6_2acntS3174 = Moonbit_object_header(_M0L14_2acasted__envS1833)->rc;
  if (_M0L6_2acntS3174 > 1) {
    int32_t _M0L11_2anew__cntS3175 = _M0L6_2acntS3174 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1833)->rc
    = _M0L11_2anew__cntS3175;
    moonbit_incref(_M0L1iS516);
    moonbit_incref(_M0L8_2afieldS3003.$0);
  } else if (_M0L6_2acntS3174 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1833);
  }
  _M0L4selfS517 = _M0L8_2afieldS3003;
  _M0L3valS1834 = _M0L1iS516->$0;
  moonbit_incref(_M0L4selfS517.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1835 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS517);
  if (_M0L3valS1834 < _M0L6_2atmpS1835) {
    moonbit_string_t* _M0L8_2afieldS3002 = _M0L4selfS517.$0;
    moonbit_string_t* _M0L3bufS1838 = _M0L8_2afieldS3002;
    int32_t _M0L8_2afieldS3001 = _M0L4selfS517.$1;
    int32_t _M0L5startS1840 = _M0L8_2afieldS3001;
    int32_t _M0L3valS1841 = _M0L1iS516->$0;
    int32_t _M0L6_2atmpS1839 = _M0L5startS1840 + _M0L3valS1841;
    moonbit_string_t _M0L6_2atmpS3000 =
      (moonbit_string_t)_M0L3bufS1838[_M0L6_2atmpS1839];
    moonbit_string_t _M0L4elemS518;
    int32_t _M0L3valS1837;
    int32_t _M0L6_2atmpS1836;
    moonbit_incref(_M0L6_2atmpS3000);
    moonbit_decref(_M0L3bufS1838);
    _M0L4elemS518 = _M0L6_2atmpS3000;
    _M0L3valS1837 = _M0L1iS516->$0;
    _M0L6_2atmpS1836 = _M0L3valS1837 + 1;
    _M0L1iS516->$0 = _M0L6_2atmpS1836;
    moonbit_decref(_M0L1iS516);
    return _M0L4elemS518;
  } else {
    moonbit_decref(_M0L4selfS517.$0);
    moonbit_decref(_M0L1iS516);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS515
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS515;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS514,
  struct _M0TPB6Logger _M0L6loggerS513
) {
  moonbit_string_t _M0L6_2atmpS1829;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1829
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS514, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS513.$0->$method_0(_M0L6loggerS513.$1, _M0L6_2atmpS1829);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS512,
  struct _M0TPB6Logger _M0L6loggerS511
) {
  moonbit_string_t _M0L6_2atmpS1828;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1828 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS512, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS511.$0->$method_0(_M0L6loggerS511.$1, _M0L6_2atmpS1828);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS506) {
  int32_t _M0L3lenS505;
  struct _M0TPC13ref3RefGiE* _M0L5indexS507;
  struct _M0R38String_3a_3aiter_2eanon__u1812__l247__* _closure_3312;
  struct _M0TWEOc* _M0L6_2atmpS1811;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS505 = Moonbit_array_length(_M0L4selfS506);
  _M0L5indexS507
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS507)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS507->$0 = 0;
  _closure_3312
  = (struct _M0R38String_3a_3aiter_2eanon__u1812__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u1812__l247__));
  Moonbit_object_header(_closure_3312)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u1812__l247__, $0) >> 2, 2, 0);
  _closure_3312->code = &_M0MPC16string6String4iterC1812l247;
  _closure_3312->$0 = _M0L5indexS507;
  _closure_3312->$1 = _M0L4selfS506;
  _closure_3312->$2 = _M0L3lenS505;
  _M0L6_2atmpS1811 = (struct _M0TWEOc*)_closure_3312;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1811);
}

int32_t _M0MPC16string6String4iterC1812l247(
  struct _M0TWEOc* _M0L6_2aenvS1813
) {
  struct _M0R38String_3a_3aiter_2eanon__u1812__l247__* _M0L14_2acasted__envS1814;
  int32_t _M0L3lenS505;
  moonbit_string_t _M0L8_2afieldS3007;
  moonbit_string_t _M0L4selfS506;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3006;
  int32_t _M0L6_2acntS3176;
  struct _M0TPC13ref3RefGiE* _M0L5indexS507;
  int32_t _M0L3valS1815;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS1814
  = (struct _M0R38String_3a_3aiter_2eanon__u1812__l247__*)_M0L6_2aenvS1813;
  _M0L3lenS505 = _M0L14_2acasted__envS1814->$2;
  _M0L8_2afieldS3007 = _M0L14_2acasted__envS1814->$1;
  _M0L4selfS506 = _M0L8_2afieldS3007;
  _M0L8_2afieldS3006 = _M0L14_2acasted__envS1814->$0;
  _M0L6_2acntS3176 = Moonbit_object_header(_M0L14_2acasted__envS1814)->rc;
  if (_M0L6_2acntS3176 > 1) {
    int32_t _M0L11_2anew__cntS3177 = _M0L6_2acntS3176 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1814)->rc
    = _M0L11_2anew__cntS3177;
    moonbit_incref(_M0L4selfS506);
    moonbit_incref(_M0L8_2afieldS3006);
  } else if (_M0L6_2acntS3176 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS1814);
  }
  _M0L5indexS507 = _M0L8_2afieldS3006;
  _M0L3valS1815 = _M0L5indexS507->$0;
  if (_M0L3valS1815 < _M0L3lenS505) {
    int32_t _M0L3valS1827 = _M0L5indexS507->$0;
    int32_t _M0L2c1S508 = _M0L4selfS506[_M0L3valS1827];
    int32_t _if__result_3313;
    int32_t _M0L3valS1825;
    int32_t _M0L6_2atmpS1824;
    int32_t _M0L6_2atmpS1826;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S508)) {
      int32_t _M0L3valS1817 = _M0L5indexS507->$0;
      int32_t _M0L6_2atmpS1816 = _M0L3valS1817 + 1;
      _if__result_3313 = _M0L6_2atmpS1816 < _M0L3lenS505;
    } else {
      _if__result_3313 = 0;
    }
    if (_if__result_3313) {
      int32_t _M0L3valS1823 = _M0L5indexS507->$0;
      int32_t _M0L6_2atmpS1822 = _M0L3valS1823 + 1;
      int32_t _M0L6_2atmpS3005 = _M0L4selfS506[_M0L6_2atmpS1822];
      int32_t _M0L2c2S509;
      moonbit_decref(_M0L4selfS506);
      _M0L2c2S509 = _M0L6_2atmpS3005;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S509)) {
        int32_t _M0L6_2atmpS1820 = (int32_t)_M0L2c1S508;
        int32_t _M0L6_2atmpS1821 = (int32_t)_M0L2c2S509;
        int32_t _M0L1cS510;
        int32_t _M0L3valS1819;
        int32_t _M0L6_2atmpS1818;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS510
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1820, _M0L6_2atmpS1821);
        _M0L3valS1819 = _M0L5indexS507->$0;
        _M0L6_2atmpS1818 = _M0L3valS1819 + 2;
        _M0L5indexS507->$0 = _M0L6_2atmpS1818;
        moonbit_decref(_M0L5indexS507);
        return _M0L1cS510;
      }
    } else {
      moonbit_decref(_M0L4selfS506);
    }
    _M0L3valS1825 = _M0L5indexS507->$0;
    _M0L6_2atmpS1824 = _M0L3valS1825 + 1;
    _M0L5indexS507->$0 = _M0L6_2atmpS1824;
    moonbit_decref(_M0L5indexS507);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS1826 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S508);
    return _M0L6_2atmpS1826;
  } else {
    moonbit_decref(_M0L5indexS507);
    moonbit_decref(_M0L4selfS506);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS496,
  moonbit_string_t _M0L5valueS498
) {
  int32_t _M0L3lenS1796;
  moonbit_string_t* _M0L6_2atmpS1798;
  int32_t _M0L6_2atmpS3010;
  int32_t _M0L6_2atmpS1797;
  int32_t _M0L6lengthS497;
  moonbit_string_t* _M0L8_2afieldS3009;
  moonbit_string_t* _M0L3bufS1799;
  moonbit_string_t _M0L6_2aoldS3008;
  int32_t _M0L6_2atmpS1800;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1796 = _M0L4selfS496->$1;
  moonbit_incref(_M0L4selfS496);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1798 = _M0MPC15array5Array6bufferGsE(_M0L4selfS496);
  _M0L6_2atmpS3010 = Moonbit_array_length(_M0L6_2atmpS1798);
  moonbit_decref(_M0L6_2atmpS1798);
  _M0L6_2atmpS1797 = _M0L6_2atmpS3010;
  if (_M0L3lenS1796 == _M0L6_2atmpS1797) {
    moonbit_incref(_M0L4selfS496);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS496);
  }
  _M0L6lengthS497 = _M0L4selfS496->$1;
  _M0L8_2afieldS3009 = _M0L4selfS496->$0;
  _M0L3bufS1799 = _M0L8_2afieldS3009;
  _M0L6_2aoldS3008 = (moonbit_string_t)_M0L3bufS1799[_M0L6lengthS497];
  moonbit_decref(_M0L6_2aoldS3008);
  _M0L3bufS1799[_M0L6lengthS497] = _M0L5valueS498;
  _M0L6_2atmpS1800 = _M0L6lengthS497 + 1;
  _M0L4selfS496->$1 = _M0L6_2atmpS1800;
  moonbit_decref(_M0L4selfS496);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS499,
  struct _M0TUsiE* _M0L5valueS501
) {
  int32_t _M0L3lenS1801;
  struct _M0TUsiE** _M0L6_2atmpS1803;
  int32_t _M0L6_2atmpS3013;
  int32_t _M0L6_2atmpS1802;
  int32_t _M0L6lengthS500;
  struct _M0TUsiE** _M0L8_2afieldS3012;
  struct _M0TUsiE** _M0L3bufS1804;
  struct _M0TUsiE* _M0L6_2aoldS3011;
  int32_t _M0L6_2atmpS1805;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1801 = _M0L4selfS499->$1;
  moonbit_incref(_M0L4selfS499);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1803 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS499);
  _M0L6_2atmpS3013 = Moonbit_array_length(_M0L6_2atmpS1803);
  moonbit_decref(_M0L6_2atmpS1803);
  _M0L6_2atmpS1802 = _M0L6_2atmpS3013;
  if (_M0L3lenS1801 == _M0L6_2atmpS1802) {
    moonbit_incref(_M0L4selfS499);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS499);
  }
  _M0L6lengthS500 = _M0L4selfS499->$1;
  _M0L8_2afieldS3012 = _M0L4selfS499->$0;
  _M0L3bufS1804 = _M0L8_2afieldS3012;
  _M0L6_2aoldS3011 = (struct _M0TUsiE*)_M0L3bufS1804[_M0L6lengthS500];
  if (_M0L6_2aoldS3011) {
    moonbit_decref(_M0L6_2aoldS3011);
  }
  _M0L3bufS1804[_M0L6lengthS500] = _M0L5valueS501;
  _M0L6_2atmpS1805 = _M0L6lengthS500 + 1;
  _M0L4selfS499->$1 = _M0L6_2atmpS1805;
  moonbit_decref(_M0L4selfS499);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS502,
  void* _M0L5valueS504
) {
  int32_t _M0L3lenS1806;
  void** _M0L6_2atmpS1808;
  int32_t _M0L6_2atmpS3016;
  int32_t _M0L6_2atmpS1807;
  int32_t _M0L6lengthS503;
  void** _M0L8_2afieldS3015;
  void** _M0L3bufS1809;
  void* _M0L6_2aoldS3014;
  int32_t _M0L6_2atmpS1810;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1806 = _M0L4selfS502->$1;
  moonbit_incref(_M0L4selfS502);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1808
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS502);
  _M0L6_2atmpS3016 = Moonbit_array_length(_M0L6_2atmpS1808);
  moonbit_decref(_M0L6_2atmpS1808);
  _M0L6_2atmpS1807 = _M0L6_2atmpS3016;
  if (_M0L3lenS1806 == _M0L6_2atmpS1807) {
    moonbit_incref(_M0L4selfS502);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS502);
  }
  _M0L6lengthS503 = _M0L4selfS502->$1;
  _M0L8_2afieldS3015 = _M0L4selfS502->$0;
  _M0L3bufS1809 = _M0L8_2afieldS3015;
  _M0L6_2aoldS3014 = (void*)_M0L3bufS1809[_M0L6lengthS503];
  moonbit_decref(_M0L6_2aoldS3014);
  _M0L3bufS1809[_M0L6lengthS503] = _M0L5valueS504;
  _M0L6_2atmpS1810 = _M0L6lengthS503 + 1;
  _M0L4selfS502->$1 = _M0L6_2atmpS1810;
  moonbit_decref(_M0L4selfS502);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS488) {
  int32_t _M0L8old__capS487;
  int32_t _M0L8new__capS489;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS487 = _M0L4selfS488->$1;
  if (_M0L8old__capS487 == 0) {
    _M0L8new__capS489 = 8;
  } else {
    _M0L8new__capS489 = _M0L8old__capS487 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS488, _M0L8new__capS489);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS491
) {
  int32_t _M0L8old__capS490;
  int32_t _M0L8new__capS492;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS490 = _M0L4selfS491->$1;
  if (_M0L8old__capS490 == 0) {
    _M0L8new__capS492 = 8;
  } else {
    _M0L8new__capS492 = _M0L8old__capS490 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS491, _M0L8new__capS492);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS494
) {
  int32_t _M0L8old__capS493;
  int32_t _M0L8new__capS495;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS493 = _M0L4selfS494->$1;
  if (_M0L8old__capS493 == 0) {
    _M0L8new__capS495 = 8;
  } else {
    _M0L8new__capS495 = _M0L8old__capS493 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS494, _M0L8new__capS495);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS472,
  int32_t _M0L13new__capacityS470
) {
  moonbit_string_t* _M0L8new__bufS469;
  moonbit_string_t* _M0L8_2afieldS3018;
  moonbit_string_t* _M0L8old__bufS471;
  int32_t _M0L8old__capS473;
  int32_t _M0L9copy__lenS474;
  moonbit_string_t* _M0L6_2aoldS3017;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS469
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS470, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3018 = _M0L4selfS472->$0;
  _M0L8old__bufS471 = _M0L8_2afieldS3018;
  _M0L8old__capS473 = Moonbit_array_length(_M0L8old__bufS471);
  if (_M0L8old__capS473 < _M0L13new__capacityS470) {
    _M0L9copy__lenS474 = _M0L8old__capS473;
  } else {
    _M0L9copy__lenS474 = _M0L13new__capacityS470;
  }
  moonbit_incref(_M0L8old__bufS471);
  moonbit_incref(_M0L8new__bufS469);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS469, 0, _M0L8old__bufS471, 0, _M0L9copy__lenS474);
  _M0L6_2aoldS3017 = _M0L4selfS472->$0;
  moonbit_decref(_M0L6_2aoldS3017);
  _M0L4selfS472->$0 = _M0L8new__bufS469;
  moonbit_decref(_M0L4selfS472);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS478,
  int32_t _M0L13new__capacityS476
) {
  struct _M0TUsiE** _M0L8new__bufS475;
  struct _M0TUsiE** _M0L8_2afieldS3020;
  struct _M0TUsiE** _M0L8old__bufS477;
  int32_t _M0L8old__capS479;
  int32_t _M0L9copy__lenS480;
  struct _M0TUsiE** _M0L6_2aoldS3019;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS475
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS476, 0);
  _M0L8_2afieldS3020 = _M0L4selfS478->$0;
  _M0L8old__bufS477 = _M0L8_2afieldS3020;
  _M0L8old__capS479 = Moonbit_array_length(_M0L8old__bufS477);
  if (_M0L8old__capS479 < _M0L13new__capacityS476) {
    _M0L9copy__lenS480 = _M0L8old__capS479;
  } else {
    _M0L9copy__lenS480 = _M0L13new__capacityS476;
  }
  moonbit_incref(_M0L8old__bufS477);
  moonbit_incref(_M0L8new__bufS475);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS475, 0, _M0L8old__bufS477, 0, _M0L9copy__lenS480);
  _M0L6_2aoldS3019 = _M0L4selfS478->$0;
  moonbit_decref(_M0L6_2aoldS3019);
  _M0L4selfS478->$0 = _M0L8new__bufS475;
  moonbit_decref(_M0L4selfS478);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS484,
  int32_t _M0L13new__capacityS482
) {
  void** _M0L8new__bufS481;
  void** _M0L8_2afieldS3022;
  void** _M0L8old__bufS483;
  int32_t _M0L8old__capS485;
  int32_t _M0L9copy__lenS486;
  void** _M0L6_2aoldS3021;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS481
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS482, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3022 = _M0L4selfS484->$0;
  _M0L8old__bufS483 = _M0L8_2afieldS3022;
  _M0L8old__capS485 = Moonbit_array_length(_M0L8old__bufS483);
  if (_M0L8old__capS485 < _M0L13new__capacityS482) {
    _M0L9copy__lenS486 = _M0L8old__capS485;
  } else {
    _M0L9copy__lenS486 = _M0L13new__capacityS482;
  }
  moonbit_incref(_M0L8old__bufS483);
  moonbit_incref(_M0L8new__bufS481);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS481, 0, _M0L8old__bufS483, 0, _M0L9copy__lenS486);
  _M0L6_2aoldS3021 = _M0L4selfS484->$0;
  moonbit_decref(_M0L6_2aoldS3021);
  _M0L4selfS484->$0 = _M0L8new__bufS481;
  moonbit_decref(_M0L4selfS484);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS468
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS468 == 0) {
    moonbit_string_t* _M0L6_2atmpS1794 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3314 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3314)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3314->$0 = _M0L6_2atmpS1794;
    _block_3314->$1 = 0;
    return _block_3314;
  } else {
    moonbit_string_t* _M0L6_2atmpS1795 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS468, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3315 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3315)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3315->$0 = _M0L6_2atmpS1795;
    _block_3315->$1 = 0;
    return _block_3315;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS462,
  int32_t _M0L1nS461
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS461 <= 0) {
    moonbit_decref(_M0L4selfS462);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS461 == 1) {
    return _M0L4selfS462;
  } else {
    int32_t _M0L3lenS463 = Moonbit_array_length(_M0L4selfS462);
    int32_t _M0L6_2atmpS1793 = _M0L3lenS463 * _M0L1nS461;
    struct _M0TPB13StringBuilder* _M0L3bufS464;
    moonbit_string_t _M0L3strS465;
    int32_t _M0L2__S466;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS464 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1793);
    _M0L3strS465 = _M0L4selfS462;
    _M0L2__S466 = 0;
    while (1) {
      if (_M0L2__S466 < _M0L1nS461) {
        int32_t _M0L6_2atmpS1792;
        moonbit_incref(_M0L3strS465);
        moonbit_incref(_M0L3bufS464);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS464, _M0L3strS465);
        _M0L6_2atmpS1792 = _M0L2__S466 + 1;
        _M0L2__S466 = _M0L6_2atmpS1792;
        continue;
      } else {
        moonbit_decref(_M0L3strS465);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS464);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS459,
  struct _M0TPC16string10StringView _M0L3strS460
) {
  int32_t _M0L3lenS1780;
  int32_t _M0L6_2atmpS1782;
  int32_t _M0L6_2atmpS1781;
  int32_t _M0L6_2atmpS1779;
  moonbit_bytes_t _M0L8_2afieldS3023;
  moonbit_bytes_t _M0L4dataS1783;
  int32_t _M0L3lenS1784;
  moonbit_string_t _M0L6_2atmpS1785;
  int32_t _M0L6_2atmpS1786;
  int32_t _M0L6_2atmpS1787;
  int32_t _M0L3lenS1789;
  int32_t _M0L6_2atmpS1791;
  int32_t _M0L6_2atmpS1790;
  int32_t _M0L6_2atmpS1788;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1780 = _M0L4selfS459->$1;
  moonbit_incref(_M0L3strS460.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1782 = _M0MPC16string10StringView6length(_M0L3strS460);
  _M0L6_2atmpS1781 = _M0L6_2atmpS1782 * 2;
  _M0L6_2atmpS1779 = _M0L3lenS1780 + _M0L6_2atmpS1781;
  moonbit_incref(_M0L4selfS459);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS459, _M0L6_2atmpS1779);
  _M0L8_2afieldS3023 = _M0L4selfS459->$0;
  _M0L4dataS1783 = _M0L8_2afieldS3023;
  _M0L3lenS1784 = _M0L4selfS459->$1;
  moonbit_incref(_M0L4dataS1783);
  moonbit_incref(_M0L3strS460.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1785 = _M0MPC16string10StringView4data(_M0L3strS460);
  moonbit_incref(_M0L3strS460.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1786 = _M0MPC16string10StringView13start__offset(_M0L3strS460);
  moonbit_incref(_M0L3strS460.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1787 = _M0MPC16string10StringView6length(_M0L3strS460);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1783, _M0L3lenS1784, _M0L6_2atmpS1785, _M0L6_2atmpS1786, _M0L6_2atmpS1787);
  _M0L3lenS1789 = _M0L4selfS459->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1791 = _M0MPC16string10StringView6length(_M0L3strS460);
  _M0L6_2atmpS1790 = _M0L6_2atmpS1791 * 2;
  _M0L6_2atmpS1788 = _M0L3lenS1789 + _M0L6_2atmpS1790;
  _M0L4selfS459->$1 = _M0L6_2atmpS1788;
  moonbit_decref(_M0L4selfS459);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS451,
  int32_t _M0L3lenS454,
  int32_t _M0L13start__offsetS458,
  int64_t _M0L11end__offsetS449
) {
  int32_t _M0L11end__offsetS448;
  int32_t _M0L5indexS452;
  int32_t _M0L5countS453;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS449 == 4294967296ll) {
    _M0L11end__offsetS448 = Moonbit_array_length(_M0L4selfS451);
  } else {
    int64_t _M0L7_2aSomeS450 = _M0L11end__offsetS449;
    _M0L11end__offsetS448 = (int32_t)_M0L7_2aSomeS450;
  }
  _M0L5indexS452 = _M0L13start__offsetS458;
  _M0L5countS453 = 0;
  while (1) {
    int32_t _if__result_3318;
    if (_M0L5indexS452 < _M0L11end__offsetS448) {
      _if__result_3318 = _M0L5countS453 < _M0L3lenS454;
    } else {
      _if__result_3318 = 0;
    }
    if (_if__result_3318) {
      int32_t _M0L2c1S455 = _M0L4selfS451[_M0L5indexS452];
      int32_t _if__result_3319;
      int32_t _M0L6_2atmpS1777;
      int32_t _M0L6_2atmpS1778;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S455)) {
        int32_t _M0L6_2atmpS1773 = _M0L5indexS452 + 1;
        _if__result_3319 = _M0L6_2atmpS1773 < _M0L11end__offsetS448;
      } else {
        _if__result_3319 = 0;
      }
      if (_if__result_3319) {
        int32_t _M0L6_2atmpS1776 = _M0L5indexS452 + 1;
        int32_t _M0L2c2S456 = _M0L4selfS451[_M0L6_2atmpS1776];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S456)) {
          int32_t _M0L6_2atmpS1774 = _M0L5indexS452 + 2;
          int32_t _M0L6_2atmpS1775 = _M0L5countS453 + 1;
          _M0L5indexS452 = _M0L6_2atmpS1774;
          _M0L5countS453 = _M0L6_2atmpS1775;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_99.data, (moonbit_string_t)moonbit_string_literal_100.data);
        }
      }
      _M0L6_2atmpS1777 = _M0L5indexS452 + 1;
      _M0L6_2atmpS1778 = _M0L5countS453 + 1;
      _M0L5indexS452 = _M0L6_2atmpS1777;
      _M0L5countS453 = _M0L6_2atmpS1778;
      continue;
    } else {
      moonbit_decref(_M0L4selfS451);
      return _M0L5countS453 >= _M0L3lenS454;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS445
) {
  int32_t _M0L3endS1767;
  int32_t _M0L8_2afieldS3024;
  int32_t _M0L5startS1768;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1767 = _M0L4selfS445.$2;
  _M0L8_2afieldS3024 = _M0L4selfS445.$1;
  moonbit_decref(_M0L4selfS445.$0);
  _M0L5startS1768 = _M0L8_2afieldS3024;
  return _M0L3endS1767 - _M0L5startS1768;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS446
) {
  int32_t _M0L3endS1769;
  int32_t _M0L8_2afieldS3025;
  int32_t _M0L5startS1770;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1769 = _M0L4selfS446.$2;
  _M0L8_2afieldS3025 = _M0L4selfS446.$1;
  moonbit_decref(_M0L4selfS446.$0);
  _M0L5startS1770 = _M0L8_2afieldS3025;
  return _M0L3endS1769 - _M0L5startS1770;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS447
) {
  int32_t _M0L3endS1771;
  int32_t _M0L8_2afieldS3026;
  int32_t _M0L5startS1772;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1771 = _M0L4selfS447.$2;
  _M0L8_2afieldS3026 = _M0L4selfS447.$1;
  moonbit_decref(_M0L4selfS447.$0);
  _M0L5startS1772 = _M0L8_2afieldS3026;
  return _M0L3endS1771 - _M0L5startS1772;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS443,
  int64_t _M0L19start__offset_2eoptS441,
  int64_t _M0L11end__offsetS444
) {
  int32_t _M0L13start__offsetS440;
  if (_M0L19start__offset_2eoptS441 == 4294967296ll) {
    _M0L13start__offsetS440 = 0;
  } else {
    int64_t _M0L7_2aSomeS442 = _M0L19start__offset_2eoptS441;
    _M0L13start__offsetS440 = (int32_t)_M0L7_2aSomeS442;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS443, _M0L13start__offsetS440, _M0L11end__offsetS444);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS438,
  int32_t _M0L13start__offsetS439,
  int64_t _M0L11end__offsetS436
) {
  int32_t _M0L11end__offsetS435;
  int32_t _if__result_3320;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS436 == 4294967296ll) {
    _M0L11end__offsetS435 = Moonbit_array_length(_M0L4selfS438);
  } else {
    int64_t _M0L7_2aSomeS437 = _M0L11end__offsetS436;
    _M0L11end__offsetS435 = (int32_t)_M0L7_2aSomeS437;
  }
  if (_M0L13start__offsetS439 >= 0) {
    if (_M0L13start__offsetS439 <= _M0L11end__offsetS435) {
      int32_t _M0L6_2atmpS1766 = Moonbit_array_length(_M0L4selfS438);
      _if__result_3320 = _M0L11end__offsetS435 <= _M0L6_2atmpS1766;
    } else {
      _if__result_3320 = 0;
    }
  } else {
    _if__result_3320 = 0;
  }
  if (_if__result_3320) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS439,
                                                 _M0L11end__offsetS435,
                                                 _M0L4selfS438};
  } else {
    moonbit_decref(_M0L4selfS438);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_101.data, (moonbit_string_t)moonbit_string_literal_102.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS434
) {
  moonbit_string_t _M0L8_2afieldS3028;
  moonbit_string_t _M0L3strS1763;
  int32_t _M0L5startS1764;
  int32_t _M0L8_2afieldS3027;
  int32_t _M0L3endS1765;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3028 = _M0L4selfS434.$0;
  _M0L3strS1763 = _M0L8_2afieldS3028;
  _M0L5startS1764 = _M0L4selfS434.$1;
  _M0L8_2afieldS3027 = _M0L4selfS434.$2;
  _M0L3endS1765 = _M0L8_2afieldS3027;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1763, _M0L5startS1764, _M0L3endS1765);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS432,
  struct _M0TPB6Logger _M0L6loggerS433
) {
  moonbit_string_t _M0L8_2afieldS3030;
  moonbit_string_t _M0L3strS1760;
  int32_t _M0L5startS1761;
  int32_t _M0L8_2afieldS3029;
  int32_t _M0L3endS1762;
  moonbit_string_t _M0L6substrS431;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3030 = _M0L4selfS432.$0;
  _M0L3strS1760 = _M0L8_2afieldS3030;
  _M0L5startS1761 = _M0L4selfS432.$1;
  _M0L8_2afieldS3029 = _M0L4selfS432.$2;
  _M0L3endS1762 = _M0L8_2afieldS3029;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS431
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1760, _M0L5startS1761, _M0L3endS1762);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS431, _M0L6loggerS433);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS423,
  struct _M0TPB6Logger _M0L6loggerS421
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS422;
  int32_t _M0L3lenS424;
  int32_t _M0L1iS425;
  int32_t _M0L3segS426;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS421.$1) {
    moonbit_incref(_M0L6loggerS421.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS421.$0->$method_3(_M0L6loggerS421.$1, 34);
  moonbit_incref(_M0L4selfS423);
  if (_M0L6loggerS421.$1) {
    moonbit_incref(_M0L6loggerS421.$1);
  }
  _M0L6_2aenvS422
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS422)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS422->$0 = _M0L4selfS423;
  _M0L6_2aenvS422->$1_0 = _M0L6loggerS421.$0;
  _M0L6_2aenvS422->$1_1 = _M0L6loggerS421.$1;
  _M0L3lenS424 = Moonbit_array_length(_M0L4selfS423);
  _M0L1iS425 = 0;
  _M0L3segS426 = 0;
  _2afor_427:;
  while (1) {
    int32_t _M0L4codeS428;
    int32_t _M0L1cS430;
    int32_t _M0L6_2atmpS1744;
    int32_t _M0L6_2atmpS1745;
    int32_t _M0L6_2atmpS1746;
    int32_t _tmp_3324;
    int32_t _tmp_3325;
    if (_M0L1iS425 >= _M0L3lenS424) {
      moonbit_decref(_M0L4selfS423);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS422, _M0L3segS426, _M0L1iS425);
      break;
    }
    _M0L4codeS428 = _M0L4selfS423[_M0L1iS425];
    switch (_M0L4codeS428) {
      case 34: {
        _M0L1cS430 = _M0L4codeS428;
        goto join_429;
        break;
      }
      
      case 92: {
        _M0L1cS430 = _M0L4codeS428;
        goto join_429;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1747;
        int32_t _M0L6_2atmpS1748;
        moonbit_incref(_M0L6_2aenvS422);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS422, _M0L3segS426, _M0L1iS425);
        if (_M0L6loggerS421.$1) {
          moonbit_incref(_M0L6loggerS421.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS421.$0->$method_0(_M0L6loggerS421.$1, (moonbit_string_t)moonbit_string_literal_86.data);
        _M0L6_2atmpS1747 = _M0L1iS425 + 1;
        _M0L6_2atmpS1748 = _M0L1iS425 + 1;
        _M0L1iS425 = _M0L6_2atmpS1747;
        _M0L3segS426 = _M0L6_2atmpS1748;
        goto _2afor_427;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1749;
        int32_t _M0L6_2atmpS1750;
        moonbit_incref(_M0L6_2aenvS422);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS422, _M0L3segS426, _M0L1iS425);
        if (_M0L6loggerS421.$1) {
          moonbit_incref(_M0L6loggerS421.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS421.$0->$method_0(_M0L6loggerS421.$1, (moonbit_string_t)moonbit_string_literal_87.data);
        _M0L6_2atmpS1749 = _M0L1iS425 + 1;
        _M0L6_2atmpS1750 = _M0L1iS425 + 1;
        _M0L1iS425 = _M0L6_2atmpS1749;
        _M0L3segS426 = _M0L6_2atmpS1750;
        goto _2afor_427;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1751;
        int32_t _M0L6_2atmpS1752;
        moonbit_incref(_M0L6_2aenvS422);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS422, _M0L3segS426, _M0L1iS425);
        if (_M0L6loggerS421.$1) {
          moonbit_incref(_M0L6loggerS421.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS421.$0->$method_0(_M0L6loggerS421.$1, (moonbit_string_t)moonbit_string_literal_88.data);
        _M0L6_2atmpS1751 = _M0L1iS425 + 1;
        _M0L6_2atmpS1752 = _M0L1iS425 + 1;
        _M0L1iS425 = _M0L6_2atmpS1751;
        _M0L3segS426 = _M0L6_2atmpS1752;
        goto _2afor_427;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1753;
        int32_t _M0L6_2atmpS1754;
        moonbit_incref(_M0L6_2aenvS422);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS422, _M0L3segS426, _M0L1iS425);
        if (_M0L6loggerS421.$1) {
          moonbit_incref(_M0L6loggerS421.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS421.$0->$method_0(_M0L6loggerS421.$1, (moonbit_string_t)moonbit_string_literal_89.data);
        _M0L6_2atmpS1753 = _M0L1iS425 + 1;
        _M0L6_2atmpS1754 = _M0L1iS425 + 1;
        _M0L1iS425 = _M0L6_2atmpS1753;
        _M0L3segS426 = _M0L6_2atmpS1754;
        goto _2afor_427;
        break;
      }
      default: {
        if (_M0L4codeS428 < 32) {
          int32_t _M0L6_2atmpS1756;
          moonbit_string_t _M0L6_2atmpS1755;
          int32_t _M0L6_2atmpS1757;
          int32_t _M0L6_2atmpS1758;
          moonbit_incref(_M0L6_2aenvS422);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS422, _M0L3segS426, _M0L1iS425);
          if (_M0L6loggerS421.$1) {
            moonbit_incref(_M0L6loggerS421.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS421.$0->$method_0(_M0L6loggerS421.$1, (moonbit_string_t)moonbit_string_literal_103.data);
          _M0L6_2atmpS1756 = _M0L4codeS428 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1755 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1756);
          if (_M0L6loggerS421.$1) {
            moonbit_incref(_M0L6loggerS421.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS421.$0->$method_0(_M0L6loggerS421.$1, _M0L6_2atmpS1755);
          if (_M0L6loggerS421.$1) {
            moonbit_incref(_M0L6loggerS421.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS421.$0->$method_3(_M0L6loggerS421.$1, 125);
          _M0L6_2atmpS1757 = _M0L1iS425 + 1;
          _M0L6_2atmpS1758 = _M0L1iS425 + 1;
          _M0L1iS425 = _M0L6_2atmpS1757;
          _M0L3segS426 = _M0L6_2atmpS1758;
          goto _2afor_427;
        } else {
          int32_t _M0L6_2atmpS1759 = _M0L1iS425 + 1;
          int32_t _tmp_3323 = _M0L3segS426;
          _M0L1iS425 = _M0L6_2atmpS1759;
          _M0L3segS426 = _tmp_3323;
          goto _2afor_427;
        }
        break;
      }
    }
    goto joinlet_3322;
    join_429:;
    moonbit_incref(_M0L6_2aenvS422);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS422, _M0L3segS426, _M0L1iS425);
    if (_M0L6loggerS421.$1) {
      moonbit_incref(_M0L6loggerS421.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS421.$0->$method_3(_M0L6loggerS421.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1744 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS430);
    if (_M0L6loggerS421.$1) {
      moonbit_incref(_M0L6loggerS421.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS421.$0->$method_3(_M0L6loggerS421.$1, _M0L6_2atmpS1744);
    _M0L6_2atmpS1745 = _M0L1iS425 + 1;
    _M0L6_2atmpS1746 = _M0L1iS425 + 1;
    _M0L1iS425 = _M0L6_2atmpS1745;
    _M0L3segS426 = _M0L6_2atmpS1746;
    continue;
    joinlet_3322:;
    _tmp_3324 = _M0L1iS425;
    _tmp_3325 = _M0L3segS426;
    _M0L1iS425 = _tmp_3324;
    _M0L3segS426 = _tmp_3325;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS421.$0->$method_3(_M0L6loggerS421.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS417,
  int32_t _M0L3segS420,
  int32_t _M0L1iS419
) {
  struct _M0TPB6Logger _M0L8_2afieldS3032;
  struct _M0TPB6Logger _M0L6loggerS416;
  moonbit_string_t _M0L8_2afieldS3031;
  int32_t _M0L6_2acntS3178;
  moonbit_string_t _M0L4selfS418;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3032
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS417->$1_0, _M0L6_2aenvS417->$1_1
  };
  _M0L6loggerS416 = _M0L8_2afieldS3032;
  _M0L8_2afieldS3031 = _M0L6_2aenvS417->$0;
  _M0L6_2acntS3178 = Moonbit_object_header(_M0L6_2aenvS417)->rc;
  if (_M0L6_2acntS3178 > 1) {
    int32_t _M0L11_2anew__cntS3179 = _M0L6_2acntS3178 - 1;
    Moonbit_object_header(_M0L6_2aenvS417)->rc = _M0L11_2anew__cntS3179;
    if (_M0L6loggerS416.$1) {
      moonbit_incref(_M0L6loggerS416.$1);
    }
    moonbit_incref(_M0L8_2afieldS3031);
  } else if (_M0L6_2acntS3178 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS417);
  }
  _M0L4selfS418 = _M0L8_2afieldS3031;
  if (_M0L1iS419 > _M0L3segS420) {
    int32_t _M0L6_2atmpS1743 = _M0L1iS419 - _M0L3segS420;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS416.$0->$method_1(_M0L6loggerS416.$1, _M0L4selfS418, _M0L3segS420, _M0L6_2atmpS1743);
  } else {
    moonbit_decref(_M0L4selfS418);
    if (_M0L6loggerS416.$1) {
      moonbit_decref(_M0L6loggerS416.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS415) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS414;
  int32_t _M0L6_2atmpS1740;
  int32_t _M0L6_2atmpS1739;
  int32_t _M0L6_2atmpS1742;
  int32_t _M0L6_2atmpS1741;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1738;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS414 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1740 = _M0IPC14byte4BytePB3Div3div(_M0L1bS415, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1739
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1740);
  moonbit_incref(_M0L7_2aselfS414);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS414, _M0L6_2atmpS1739);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1742 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS415, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1741
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1742);
  moonbit_incref(_M0L7_2aselfS414);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS414, _M0L6_2atmpS1741);
  _M0L6_2atmpS1738 = _M0L7_2aselfS414;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1738);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS413) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS413 < 10) {
    int32_t _M0L6_2atmpS1735;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1735 = _M0IPC14byte4BytePB3Add3add(_M0L1iS413, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1735);
  } else {
    int32_t _M0L6_2atmpS1737;
    int32_t _M0L6_2atmpS1736;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1737 = _M0IPC14byte4BytePB3Add3add(_M0L1iS413, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1736 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1737, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1736);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS411,
  int32_t _M0L4thatS412
) {
  int32_t _M0L6_2atmpS1733;
  int32_t _M0L6_2atmpS1734;
  int32_t _M0L6_2atmpS1732;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1733 = (int32_t)_M0L4selfS411;
  _M0L6_2atmpS1734 = (int32_t)_M0L4thatS412;
  _M0L6_2atmpS1732 = _M0L6_2atmpS1733 - _M0L6_2atmpS1734;
  return _M0L6_2atmpS1732 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS409,
  int32_t _M0L4thatS410
) {
  int32_t _M0L6_2atmpS1730;
  int32_t _M0L6_2atmpS1731;
  int32_t _M0L6_2atmpS1729;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1730 = (int32_t)_M0L4selfS409;
  _M0L6_2atmpS1731 = (int32_t)_M0L4thatS410;
  _M0L6_2atmpS1729 = _M0L6_2atmpS1730 % _M0L6_2atmpS1731;
  return _M0L6_2atmpS1729 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS407,
  int32_t _M0L4thatS408
) {
  int32_t _M0L6_2atmpS1727;
  int32_t _M0L6_2atmpS1728;
  int32_t _M0L6_2atmpS1726;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1727 = (int32_t)_M0L4selfS407;
  _M0L6_2atmpS1728 = (int32_t)_M0L4thatS408;
  _M0L6_2atmpS1726 = _M0L6_2atmpS1727 / _M0L6_2atmpS1728;
  return _M0L6_2atmpS1726 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS405,
  int32_t _M0L4thatS406
) {
  int32_t _M0L6_2atmpS1724;
  int32_t _M0L6_2atmpS1725;
  int32_t _M0L6_2atmpS1723;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1724 = (int32_t)_M0L4selfS405;
  _M0L6_2atmpS1725 = (int32_t)_M0L4thatS406;
  _M0L6_2atmpS1723 = _M0L6_2atmpS1724 + _M0L6_2atmpS1725;
  return _M0L6_2atmpS1723 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS402,
  int32_t _M0L5startS400,
  int32_t _M0L3endS401
) {
  int32_t _if__result_3326;
  int32_t _M0L3lenS403;
  int32_t _M0L6_2atmpS1721;
  int32_t _M0L6_2atmpS1722;
  moonbit_bytes_t _M0L5bytesS404;
  moonbit_bytes_t _M0L6_2atmpS1720;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS400 == 0) {
    int32_t _M0L6_2atmpS1719 = Moonbit_array_length(_M0L3strS402);
    _if__result_3326 = _M0L3endS401 == _M0L6_2atmpS1719;
  } else {
    _if__result_3326 = 0;
  }
  if (_if__result_3326) {
    return _M0L3strS402;
  }
  _M0L3lenS403 = _M0L3endS401 - _M0L5startS400;
  _M0L6_2atmpS1721 = _M0L3lenS403 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1722 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS404
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1721, _M0L6_2atmpS1722);
  moonbit_incref(_M0L5bytesS404);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS404, 0, _M0L3strS402, _M0L5startS400, _M0L3lenS403);
  _M0L6_2atmpS1720 = _M0L5bytesS404;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1720, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS397) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS397;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS398
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS398;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS399) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS399;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS389,
  int32_t _M0L5radixS388
) {
  int32_t _if__result_3327;
  uint16_t* _M0L6bufferS390;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS388 < 2) {
    _if__result_3327 = 1;
  } else {
    _if__result_3327 = _M0L5radixS388 > 36;
  }
  if (_if__result_3327) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_104.data, (moonbit_string_t)moonbit_string_literal_105.data);
  }
  if (_M0L4selfS389 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_92.data;
  }
  switch (_M0L5radixS388) {
    case 10: {
      int32_t _M0L3lenS391;
      uint16_t* _M0L6bufferS392;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS391 = _M0FPB12dec__count64(_M0L4selfS389);
      _M0L6bufferS392 = (uint16_t*)moonbit_make_string(_M0L3lenS391, 0);
      moonbit_incref(_M0L6bufferS392);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS392, _M0L4selfS389, 0, _M0L3lenS391);
      _M0L6bufferS390 = _M0L6bufferS392;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS393;
      uint16_t* _M0L6bufferS394;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS393 = _M0FPB12hex__count64(_M0L4selfS389);
      _M0L6bufferS394 = (uint16_t*)moonbit_make_string(_M0L3lenS393, 0);
      moonbit_incref(_M0L6bufferS394);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS394, _M0L4selfS389, 0, _M0L3lenS393);
      _M0L6bufferS390 = _M0L6bufferS394;
      break;
    }
    default: {
      int32_t _M0L3lenS395;
      uint16_t* _M0L6bufferS396;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS395 = _M0FPB14radix__count64(_M0L4selfS389, _M0L5radixS388);
      _M0L6bufferS396 = (uint16_t*)moonbit_make_string(_M0L3lenS395, 0);
      moonbit_incref(_M0L6bufferS396);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS396, _M0L4selfS389, 0, _M0L3lenS395, _M0L5radixS388);
      _M0L6bufferS390 = _M0L6bufferS396;
      break;
    }
  }
  return _M0L6bufferS390;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS378,
  uint64_t _M0L3numS366,
  int32_t _M0L12digit__startS369,
  int32_t _M0L10total__lenS368
) {
  uint64_t _M0Lm3numS365;
  int32_t _M0Lm6offsetS367;
  uint64_t _M0L6_2atmpS1718;
  int32_t _M0Lm9remainingS380;
  int32_t _M0L6_2atmpS1699;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS365 = _M0L3numS366;
  _M0Lm6offsetS367 = _M0L10total__lenS368 - _M0L12digit__startS369;
  while (1) {
    uint64_t _M0L6_2atmpS1662 = _M0Lm3numS365;
    if (_M0L6_2atmpS1662 >= 10000ull) {
      uint64_t _M0L6_2atmpS1685 = _M0Lm3numS365;
      uint64_t _M0L1tS370 = _M0L6_2atmpS1685 / 10000ull;
      uint64_t _M0L6_2atmpS1684 = _M0Lm3numS365;
      uint64_t _M0L6_2atmpS1683 = _M0L6_2atmpS1684 % 10000ull;
      int32_t _M0L1rS371 = (int32_t)_M0L6_2atmpS1683;
      int32_t _M0L2d1S372;
      int32_t _M0L2d2S373;
      int32_t _M0L6_2atmpS1663;
      int32_t _M0L6_2atmpS1682;
      int32_t _M0L6_2atmpS1681;
      int32_t _M0L6d1__hiS374;
      int32_t _M0L6_2atmpS1680;
      int32_t _M0L6_2atmpS1679;
      int32_t _M0L6d1__loS375;
      int32_t _M0L6_2atmpS1678;
      int32_t _M0L6_2atmpS1677;
      int32_t _M0L6d2__hiS376;
      int32_t _M0L6_2atmpS1676;
      int32_t _M0L6_2atmpS1675;
      int32_t _M0L6d2__loS377;
      int32_t _M0L6_2atmpS1665;
      int32_t _M0L6_2atmpS1664;
      int32_t _M0L6_2atmpS1668;
      int32_t _M0L6_2atmpS1667;
      int32_t _M0L6_2atmpS1666;
      int32_t _M0L6_2atmpS1671;
      int32_t _M0L6_2atmpS1670;
      int32_t _M0L6_2atmpS1669;
      int32_t _M0L6_2atmpS1674;
      int32_t _M0L6_2atmpS1673;
      int32_t _M0L6_2atmpS1672;
      _M0Lm3numS365 = _M0L1tS370;
      _M0L2d1S372 = _M0L1rS371 / 100;
      _M0L2d2S373 = _M0L1rS371 % 100;
      _M0L6_2atmpS1663 = _M0Lm6offsetS367;
      _M0Lm6offsetS367 = _M0L6_2atmpS1663 - 4;
      _M0L6_2atmpS1682 = _M0L2d1S372 / 10;
      _M0L6_2atmpS1681 = 48 + _M0L6_2atmpS1682;
      _M0L6d1__hiS374 = (uint16_t)_M0L6_2atmpS1681;
      _M0L6_2atmpS1680 = _M0L2d1S372 % 10;
      _M0L6_2atmpS1679 = 48 + _M0L6_2atmpS1680;
      _M0L6d1__loS375 = (uint16_t)_M0L6_2atmpS1679;
      _M0L6_2atmpS1678 = _M0L2d2S373 / 10;
      _M0L6_2atmpS1677 = 48 + _M0L6_2atmpS1678;
      _M0L6d2__hiS376 = (uint16_t)_M0L6_2atmpS1677;
      _M0L6_2atmpS1676 = _M0L2d2S373 % 10;
      _M0L6_2atmpS1675 = 48 + _M0L6_2atmpS1676;
      _M0L6d2__loS377 = (uint16_t)_M0L6_2atmpS1675;
      _M0L6_2atmpS1665 = _M0Lm6offsetS367;
      _M0L6_2atmpS1664 = _M0L12digit__startS369 + _M0L6_2atmpS1665;
      _M0L6bufferS378[_M0L6_2atmpS1664] = _M0L6d1__hiS374;
      _M0L6_2atmpS1668 = _M0Lm6offsetS367;
      _M0L6_2atmpS1667 = _M0L12digit__startS369 + _M0L6_2atmpS1668;
      _M0L6_2atmpS1666 = _M0L6_2atmpS1667 + 1;
      _M0L6bufferS378[_M0L6_2atmpS1666] = _M0L6d1__loS375;
      _M0L6_2atmpS1671 = _M0Lm6offsetS367;
      _M0L6_2atmpS1670 = _M0L12digit__startS369 + _M0L6_2atmpS1671;
      _M0L6_2atmpS1669 = _M0L6_2atmpS1670 + 2;
      _M0L6bufferS378[_M0L6_2atmpS1669] = _M0L6d2__hiS376;
      _M0L6_2atmpS1674 = _M0Lm6offsetS367;
      _M0L6_2atmpS1673 = _M0L12digit__startS369 + _M0L6_2atmpS1674;
      _M0L6_2atmpS1672 = _M0L6_2atmpS1673 + 3;
      _M0L6bufferS378[_M0L6_2atmpS1672] = _M0L6d2__loS377;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1718 = _M0Lm3numS365;
  _M0Lm9remainingS380 = (int32_t)_M0L6_2atmpS1718;
  while (1) {
    int32_t _M0L6_2atmpS1686 = _M0Lm9remainingS380;
    if (_M0L6_2atmpS1686 >= 100) {
      int32_t _M0L6_2atmpS1698 = _M0Lm9remainingS380;
      int32_t _M0L1tS381 = _M0L6_2atmpS1698 / 100;
      int32_t _M0L6_2atmpS1697 = _M0Lm9remainingS380;
      int32_t _M0L1dS382 = _M0L6_2atmpS1697 % 100;
      int32_t _M0L6_2atmpS1687;
      int32_t _M0L6_2atmpS1696;
      int32_t _M0L6_2atmpS1695;
      int32_t _M0L5d__hiS383;
      int32_t _M0L6_2atmpS1694;
      int32_t _M0L6_2atmpS1693;
      int32_t _M0L5d__loS384;
      int32_t _M0L6_2atmpS1689;
      int32_t _M0L6_2atmpS1688;
      int32_t _M0L6_2atmpS1692;
      int32_t _M0L6_2atmpS1691;
      int32_t _M0L6_2atmpS1690;
      _M0Lm9remainingS380 = _M0L1tS381;
      _M0L6_2atmpS1687 = _M0Lm6offsetS367;
      _M0Lm6offsetS367 = _M0L6_2atmpS1687 - 2;
      _M0L6_2atmpS1696 = _M0L1dS382 / 10;
      _M0L6_2atmpS1695 = 48 + _M0L6_2atmpS1696;
      _M0L5d__hiS383 = (uint16_t)_M0L6_2atmpS1695;
      _M0L6_2atmpS1694 = _M0L1dS382 % 10;
      _M0L6_2atmpS1693 = 48 + _M0L6_2atmpS1694;
      _M0L5d__loS384 = (uint16_t)_M0L6_2atmpS1693;
      _M0L6_2atmpS1689 = _M0Lm6offsetS367;
      _M0L6_2atmpS1688 = _M0L12digit__startS369 + _M0L6_2atmpS1689;
      _M0L6bufferS378[_M0L6_2atmpS1688] = _M0L5d__hiS383;
      _M0L6_2atmpS1692 = _M0Lm6offsetS367;
      _M0L6_2atmpS1691 = _M0L12digit__startS369 + _M0L6_2atmpS1692;
      _M0L6_2atmpS1690 = _M0L6_2atmpS1691 + 1;
      _M0L6bufferS378[_M0L6_2atmpS1690] = _M0L5d__loS384;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1699 = _M0Lm9remainingS380;
  if (_M0L6_2atmpS1699 >= 10) {
    int32_t _M0L6_2atmpS1700 = _M0Lm6offsetS367;
    int32_t _M0L6_2atmpS1711;
    int32_t _M0L6_2atmpS1710;
    int32_t _M0L6_2atmpS1709;
    int32_t _M0L5d__hiS386;
    int32_t _M0L6_2atmpS1708;
    int32_t _M0L6_2atmpS1707;
    int32_t _M0L6_2atmpS1706;
    int32_t _M0L5d__loS387;
    int32_t _M0L6_2atmpS1702;
    int32_t _M0L6_2atmpS1701;
    int32_t _M0L6_2atmpS1705;
    int32_t _M0L6_2atmpS1704;
    int32_t _M0L6_2atmpS1703;
    _M0Lm6offsetS367 = _M0L6_2atmpS1700 - 2;
    _M0L6_2atmpS1711 = _M0Lm9remainingS380;
    _M0L6_2atmpS1710 = _M0L6_2atmpS1711 / 10;
    _M0L6_2atmpS1709 = 48 + _M0L6_2atmpS1710;
    _M0L5d__hiS386 = (uint16_t)_M0L6_2atmpS1709;
    _M0L6_2atmpS1708 = _M0Lm9remainingS380;
    _M0L6_2atmpS1707 = _M0L6_2atmpS1708 % 10;
    _M0L6_2atmpS1706 = 48 + _M0L6_2atmpS1707;
    _M0L5d__loS387 = (uint16_t)_M0L6_2atmpS1706;
    _M0L6_2atmpS1702 = _M0Lm6offsetS367;
    _M0L6_2atmpS1701 = _M0L12digit__startS369 + _M0L6_2atmpS1702;
    _M0L6bufferS378[_M0L6_2atmpS1701] = _M0L5d__hiS386;
    _M0L6_2atmpS1705 = _M0Lm6offsetS367;
    _M0L6_2atmpS1704 = _M0L12digit__startS369 + _M0L6_2atmpS1705;
    _M0L6_2atmpS1703 = _M0L6_2atmpS1704 + 1;
    _M0L6bufferS378[_M0L6_2atmpS1703] = _M0L5d__loS387;
    moonbit_decref(_M0L6bufferS378);
  } else {
    int32_t _M0L6_2atmpS1712 = _M0Lm6offsetS367;
    int32_t _M0L6_2atmpS1717;
    int32_t _M0L6_2atmpS1713;
    int32_t _M0L6_2atmpS1716;
    int32_t _M0L6_2atmpS1715;
    int32_t _M0L6_2atmpS1714;
    _M0Lm6offsetS367 = _M0L6_2atmpS1712 - 1;
    _M0L6_2atmpS1717 = _M0Lm6offsetS367;
    _M0L6_2atmpS1713 = _M0L12digit__startS369 + _M0L6_2atmpS1717;
    _M0L6_2atmpS1716 = _M0Lm9remainingS380;
    _M0L6_2atmpS1715 = 48 + _M0L6_2atmpS1716;
    _M0L6_2atmpS1714 = (uint16_t)_M0L6_2atmpS1715;
    _M0L6bufferS378[_M0L6_2atmpS1713] = _M0L6_2atmpS1714;
    moonbit_decref(_M0L6bufferS378);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS360,
  uint64_t _M0L3numS354,
  int32_t _M0L12digit__startS352,
  int32_t _M0L10total__lenS351,
  int32_t _M0L5radixS356
) {
  int32_t _M0Lm6offsetS350;
  uint64_t _M0Lm1nS353;
  uint64_t _M0L4baseS355;
  int32_t _M0L6_2atmpS1644;
  int32_t _M0L6_2atmpS1643;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS350 = _M0L10total__lenS351 - _M0L12digit__startS352;
  _M0Lm1nS353 = _M0L3numS354;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS355 = _M0MPC13int3Int10to__uint64(_M0L5radixS356);
  _M0L6_2atmpS1644 = _M0L5radixS356 - 1;
  _M0L6_2atmpS1643 = _M0L5radixS356 & _M0L6_2atmpS1644;
  if (_M0L6_2atmpS1643 == 0) {
    int32_t _M0L5shiftS357;
    uint64_t _M0L4maskS358;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS357 = moonbit_ctz32(_M0L5radixS356);
    _M0L4maskS358 = _M0L4baseS355 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1645 = _M0Lm1nS353;
      if (_M0L6_2atmpS1645 > 0ull) {
        int32_t _M0L6_2atmpS1646 = _M0Lm6offsetS350;
        uint64_t _M0L6_2atmpS1652;
        uint64_t _M0L6_2atmpS1651;
        int32_t _M0L5digitS359;
        int32_t _M0L6_2atmpS1649;
        int32_t _M0L6_2atmpS1647;
        int32_t _M0L6_2atmpS1648;
        uint64_t _M0L6_2atmpS1650;
        _M0Lm6offsetS350 = _M0L6_2atmpS1646 - 1;
        _M0L6_2atmpS1652 = _M0Lm1nS353;
        _M0L6_2atmpS1651 = _M0L6_2atmpS1652 & _M0L4maskS358;
        _M0L5digitS359 = (int32_t)_M0L6_2atmpS1651;
        _M0L6_2atmpS1649 = _M0Lm6offsetS350;
        _M0L6_2atmpS1647 = _M0L12digit__startS352 + _M0L6_2atmpS1649;
        _M0L6_2atmpS1648
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS359
        ];
        _M0L6bufferS360[_M0L6_2atmpS1647] = _M0L6_2atmpS1648;
        _M0L6_2atmpS1650 = _M0Lm1nS353;
        _M0Lm1nS353 = _M0L6_2atmpS1650 >> (_M0L5shiftS357 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS360);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1653 = _M0Lm1nS353;
      if (_M0L6_2atmpS1653 > 0ull) {
        int32_t _M0L6_2atmpS1654 = _M0Lm6offsetS350;
        uint64_t _M0L6_2atmpS1661;
        uint64_t _M0L1qS362;
        uint64_t _M0L6_2atmpS1659;
        uint64_t _M0L6_2atmpS1660;
        uint64_t _M0L6_2atmpS1658;
        int32_t _M0L5digitS363;
        int32_t _M0L6_2atmpS1657;
        int32_t _M0L6_2atmpS1655;
        int32_t _M0L6_2atmpS1656;
        _M0Lm6offsetS350 = _M0L6_2atmpS1654 - 1;
        _M0L6_2atmpS1661 = _M0Lm1nS353;
        _M0L1qS362 = _M0L6_2atmpS1661 / _M0L4baseS355;
        _M0L6_2atmpS1659 = _M0Lm1nS353;
        _M0L6_2atmpS1660 = _M0L1qS362 * _M0L4baseS355;
        _M0L6_2atmpS1658 = _M0L6_2atmpS1659 - _M0L6_2atmpS1660;
        _M0L5digitS363 = (int32_t)_M0L6_2atmpS1658;
        _M0L6_2atmpS1657 = _M0Lm6offsetS350;
        _M0L6_2atmpS1655 = _M0L12digit__startS352 + _M0L6_2atmpS1657;
        _M0L6_2atmpS1656
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS363
        ];
        _M0L6bufferS360[_M0L6_2atmpS1655] = _M0L6_2atmpS1656;
        _M0Lm1nS353 = _M0L1qS362;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS360);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS347,
  uint64_t _M0L3numS343,
  int32_t _M0L12digit__startS341,
  int32_t _M0L10total__lenS340
) {
  int32_t _M0Lm6offsetS339;
  uint64_t _M0Lm1nS342;
  int32_t _M0L6_2atmpS1639;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS339 = _M0L10total__lenS340 - _M0L12digit__startS341;
  _M0Lm1nS342 = _M0L3numS343;
  while (1) {
    int32_t _M0L6_2atmpS1627 = _M0Lm6offsetS339;
    if (_M0L6_2atmpS1627 >= 2) {
      int32_t _M0L6_2atmpS1628 = _M0Lm6offsetS339;
      uint64_t _M0L6_2atmpS1638;
      uint64_t _M0L6_2atmpS1637;
      int32_t _M0L9byte__valS344;
      int32_t _M0L2hiS345;
      int32_t _M0L2loS346;
      int32_t _M0L6_2atmpS1631;
      int32_t _M0L6_2atmpS1629;
      int32_t _M0L6_2atmpS1630;
      int32_t _M0L6_2atmpS1635;
      int32_t _M0L6_2atmpS1634;
      int32_t _M0L6_2atmpS1632;
      int32_t _M0L6_2atmpS1633;
      uint64_t _M0L6_2atmpS1636;
      _M0Lm6offsetS339 = _M0L6_2atmpS1628 - 2;
      _M0L6_2atmpS1638 = _M0Lm1nS342;
      _M0L6_2atmpS1637 = _M0L6_2atmpS1638 & 255ull;
      _M0L9byte__valS344 = (int32_t)_M0L6_2atmpS1637;
      _M0L2hiS345 = _M0L9byte__valS344 / 16;
      _M0L2loS346 = _M0L9byte__valS344 % 16;
      _M0L6_2atmpS1631 = _M0Lm6offsetS339;
      _M0L6_2atmpS1629 = _M0L12digit__startS341 + _M0L6_2atmpS1631;
      _M0L6_2atmpS1630
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2hiS345
      ];
      _M0L6bufferS347[_M0L6_2atmpS1629] = _M0L6_2atmpS1630;
      _M0L6_2atmpS1635 = _M0Lm6offsetS339;
      _M0L6_2atmpS1634 = _M0L12digit__startS341 + _M0L6_2atmpS1635;
      _M0L6_2atmpS1632 = _M0L6_2atmpS1634 + 1;
      _M0L6_2atmpS1633
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2loS346
      ];
      _M0L6bufferS347[_M0L6_2atmpS1632] = _M0L6_2atmpS1633;
      _M0L6_2atmpS1636 = _M0Lm1nS342;
      _M0Lm1nS342 = _M0L6_2atmpS1636 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1639 = _M0Lm6offsetS339;
  if (_M0L6_2atmpS1639 == 1) {
    uint64_t _M0L6_2atmpS1642 = _M0Lm1nS342;
    uint64_t _M0L6_2atmpS1641 = _M0L6_2atmpS1642 & 15ull;
    int32_t _M0L6nibbleS349 = (int32_t)_M0L6_2atmpS1641;
    int32_t _M0L6_2atmpS1640 =
      ((moonbit_string_t)moonbit_string_literal_106.data)[_M0L6nibbleS349];
    _M0L6bufferS347[_M0L12digit__startS341] = _M0L6_2atmpS1640;
    moonbit_decref(_M0L6bufferS347);
  } else {
    moonbit_decref(_M0L6bufferS347);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS333,
  int32_t _M0L5radixS336
) {
  uint64_t _M0Lm3numS334;
  uint64_t _M0L4baseS335;
  int32_t _M0Lm5countS337;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS333 == 0ull) {
    return 1;
  }
  _M0Lm3numS334 = _M0L5valueS333;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS335 = _M0MPC13int3Int10to__uint64(_M0L5radixS336);
  _M0Lm5countS337 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1624 = _M0Lm3numS334;
    if (_M0L6_2atmpS1624 > 0ull) {
      int32_t _M0L6_2atmpS1625 = _M0Lm5countS337;
      uint64_t _M0L6_2atmpS1626;
      _M0Lm5countS337 = _M0L6_2atmpS1625 + 1;
      _M0L6_2atmpS1626 = _M0Lm3numS334;
      _M0Lm3numS334 = _M0L6_2atmpS1626 / _M0L4baseS335;
      continue;
    }
    break;
  }
  return _M0Lm5countS337;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS331) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS331 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS332;
    int32_t _M0L6_2atmpS1623;
    int32_t _M0L6_2atmpS1622;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS332 = moonbit_clz64(_M0L5valueS331);
    _M0L6_2atmpS1623 = 63 - _M0L14leading__zerosS332;
    _M0L6_2atmpS1622 = _M0L6_2atmpS1623 / 4;
    return _M0L6_2atmpS1622 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS330) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS330 >= 10000000000ull) {
    if (_M0L5valueS330 >= 100000000000000ull) {
      if (_M0L5valueS330 >= 10000000000000000ull) {
        if (_M0L5valueS330 >= 1000000000000000000ull) {
          if (_M0L5valueS330 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS330 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS330 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS330 >= 1000000000000ull) {
      if (_M0L5valueS330 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS330 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS330 >= 100000ull) {
    if (_M0L5valueS330 >= 10000000ull) {
      if (_M0L5valueS330 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS330 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS330 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS330 >= 1000ull) {
    if (_M0L5valueS330 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS330 >= 100ull) {
    return 3;
  } else if (_M0L5valueS330 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS314,
  int32_t _M0L5radixS313
) {
  int32_t _if__result_3334;
  int32_t _M0L12is__negativeS315;
  uint32_t _M0L3numS316;
  uint16_t* _M0L6bufferS317;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS313 < 2) {
    _if__result_3334 = 1;
  } else {
    _if__result_3334 = _M0L5radixS313 > 36;
  }
  if (_if__result_3334) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_104.data, (moonbit_string_t)moonbit_string_literal_107.data);
  }
  if (_M0L4selfS314 == 0) {
    return (moonbit_string_t)moonbit_string_literal_92.data;
  }
  _M0L12is__negativeS315 = _M0L4selfS314 < 0;
  if (_M0L12is__negativeS315) {
    int32_t _M0L6_2atmpS1621 = -_M0L4selfS314;
    _M0L3numS316 = *(uint32_t*)&_M0L6_2atmpS1621;
  } else {
    _M0L3numS316 = *(uint32_t*)&_M0L4selfS314;
  }
  switch (_M0L5radixS313) {
    case 10: {
      int32_t _M0L10digit__lenS318;
      int32_t _M0L6_2atmpS1618;
      int32_t _M0L10total__lenS319;
      uint16_t* _M0L6bufferS320;
      int32_t _M0L12digit__startS321;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS318 = _M0FPB12dec__count32(_M0L3numS316);
      if (_M0L12is__negativeS315) {
        _M0L6_2atmpS1618 = 1;
      } else {
        _M0L6_2atmpS1618 = 0;
      }
      _M0L10total__lenS319 = _M0L10digit__lenS318 + _M0L6_2atmpS1618;
      _M0L6bufferS320
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS319, 0);
      if (_M0L12is__negativeS315) {
        _M0L12digit__startS321 = 1;
      } else {
        _M0L12digit__startS321 = 0;
      }
      moonbit_incref(_M0L6bufferS320);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS320, _M0L3numS316, _M0L12digit__startS321, _M0L10total__lenS319);
      _M0L6bufferS317 = _M0L6bufferS320;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS322;
      int32_t _M0L6_2atmpS1619;
      int32_t _M0L10total__lenS323;
      uint16_t* _M0L6bufferS324;
      int32_t _M0L12digit__startS325;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS322 = _M0FPB12hex__count32(_M0L3numS316);
      if (_M0L12is__negativeS315) {
        _M0L6_2atmpS1619 = 1;
      } else {
        _M0L6_2atmpS1619 = 0;
      }
      _M0L10total__lenS323 = _M0L10digit__lenS322 + _M0L6_2atmpS1619;
      _M0L6bufferS324
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS323, 0);
      if (_M0L12is__negativeS315) {
        _M0L12digit__startS325 = 1;
      } else {
        _M0L12digit__startS325 = 0;
      }
      moonbit_incref(_M0L6bufferS324);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS324, _M0L3numS316, _M0L12digit__startS325, _M0L10total__lenS323);
      _M0L6bufferS317 = _M0L6bufferS324;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS326;
      int32_t _M0L6_2atmpS1620;
      int32_t _M0L10total__lenS327;
      uint16_t* _M0L6bufferS328;
      int32_t _M0L12digit__startS329;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS326
      = _M0FPB14radix__count32(_M0L3numS316, _M0L5radixS313);
      if (_M0L12is__negativeS315) {
        _M0L6_2atmpS1620 = 1;
      } else {
        _M0L6_2atmpS1620 = 0;
      }
      _M0L10total__lenS327 = _M0L10digit__lenS326 + _M0L6_2atmpS1620;
      _M0L6bufferS328
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS327, 0);
      if (_M0L12is__negativeS315) {
        _M0L12digit__startS329 = 1;
      } else {
        _M0L12digit__startS329 = 0;
      }
      moonbit_incref(_M0L6bufferS328);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS328, _M0L3numS316, _M0L12digit__startS329, _M0L10total__lenS327, _M0L5radixS313);
      _M0L6bufferS317 = _M0L6bufferS328;
      break;
    }
  }
  if (_M0L12is__negativeS315) {
    _M0L6bufferS317[0] = 45;
  }
  return _M0L6bufferS317;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS307,
  int32_t _M0L5radixS310
) {
  uint32_t _M0Lm3numS308;
  uint32_t _M0L4baseS309;
  int32_t _M0Lm5countS311;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS307 == 0u) {
    return 1;
  }
  _M0Lm3numS308 = _M0L5valueS307;
  _M0L4baseS309 = *(uint32_t*)&_M0L5radixS310;
  _M0Lm5countS311 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1615 = _M0Lm3numS308;
    if (_M0L6_2atmpS1615 > 0u) {
      int32_t _M0L6_2atmpS1616 = _M0Lm5countS311;
      uint32_t _M0L6_2atmpS1617;
      _M0Lm5countS311 = _M0L6_2atmpS1616 + 1;
      _M0L6_2atmpS1617 = _M0Lm3numS308;
      _M0Lm3numS308 = _M0L6_2atmpS1617 / _M0L4baseS309;
      continue;
    }
    break;
  }
  return _M0Lm5countS311;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS305) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS305 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS306;
    int32_t _M0L6_2atmpS1614;
    int32_t _M0L6_2atmpS1613;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS306 = moonbit_clz32(_M0L5valueS305);
    _M0L6_2atmpS1614 = 31 - _M0L14leading__zerosS306;
    _M0L6_2atmpS1613 = _M0L6_2atmpS1614 / 4;
    return _M0L6_2atmpS1613 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS304) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS304 >= 100000u) {
    if (_M0L5valueS304 >= 10000000u) {
      if (_M0L5valueS304 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS304 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS304 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS304 >= 1000u) {
    if (_M0L5valueS304 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS304 >= 100u) {
    return 3;
  } else if (_M0L5valueS304 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS294,
  uint32_t _M0L3numS282,
  int32_t _M0L12digit__startS285,
  int32_t _M0L10total__lenS284
) {
  uint32_t _M0Lm3numS281;
  int32_t _M0Lm6offsetS283;
  uint32_t _M0L6_2atmpS1612;
  int32_t _M0Lm9remainingS296;
  int32_t _M0L6_2atmpS1593;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS281 = _M0L3numS282;
  _M0Lm6offsetS283 = _M0L10total__lenS284 - _M0L12digit__startS285;
  while (1) {
    uint32_t _M0L6_2atmpS1556 = _M0Lm3numS281;
    if (_M0L6_2atmpS1556 >= 10000u) {
      uint32_t _M0L6_2atmpS1579 = _M0Lm3numS281;
      uint32_t _M0L1tS286 = _M0L6_2atmpS1579 / 10000u;
      uint32_t _M0L6_2atmpS1578 = _M0Lm3numS281;
      uint32_t _M0L6_2atmpS1577 = _M0L6_2atmpS1578 % 10000u;
      int32_t _M0L1rS287 = *(int32_t*)&_M0L6_2atmpS1577;
      int32_t _M0L2d1S288;
      int32_t _M0L2d2S289;
      int32_t _M0L6_2atmpS1557;
      int32_t _M0L6_2atmpS1576;
      int32_t _M0L6_2atmpS1575;
      int32_t _M0L6d1__hiS290;
      int32_t _M0L6_2atmpS1574;
      int32_t _M0L6_2atmpS1573;
      int32_t _M0L6d1__loS291;
      int32_t _M0L6_2atmpS1572;
      int32_t _M0L6_2atmpS1571;
      int32_t _M0L6d2__hiS292;
      int32_t _M0L6_2atmpS1570;
      int32_t _M0L6_2atmpS1569;
      int32_t _M0L6d2__loS293;
      int32_t _M0L6_2atmpS1559;
      int32_t _M0L6_2atmpS1558;
      int32_t _M0L6_2atmpS1562;
      int32_t _M0L6_2atmpS1561;
      int32_t _M0L6_2atmpS1560;
      int32_t _M0L6_2atmpS1565;
      int32_t _M0L6_2atmpS1564;
      int32_t _M0L6_2atmpS1563;
      int32_t _M0L6_2atmpS1568;
      int32_t _M0L6_2atmpS1567;
      int32_t _M0L6_2atmpS1566;
      _M0Lm3numS281 = _M0L1tS286;
      _M0L2d1S288 = _M0L1rS287 / 100;
      _M0L2d2S289 = _M0L1rS287 % 100;
      _M0L6_2atmpS1557 = _M0Lm6offsetS283;
      _M0Lm6offsetS283 = _M0L6_2atmpS1557 - 4;
      _M0L6_2atmpS1576 = _M0L2d1S288 / 10;
      _M0L6_2atmpS1575 = 48 + _M0L6_2atmpS1576;
      _M0L6d1__hiS290 = (uint16_t)_M0L6_2atmpS1575;
      _M0L6_2atmpS1574 = _M0L2d1S288 % 10;
      _M0L6_2atmpS1573 = 48 + _M0L6_2atmpS1574;
      _M0L6d1__loS291 = (uint16_t)_M0L6_2atmpS1573;
      _M0L6_2atmpS1572 = _M0L2d2S289 / 10;
      _M0L6_2atmpS1571 = 48 + _M0L6_2atmpS1572;
      _M0L6d2__hiS292 = (uint16_t)_M0L6_2atmpS1571;
      _M0L6_2atmpS1570 = _M0L2d2S289 % 10;
      _M0L6_2atmpS1569 = 48 + _M0L6_2atmpS1570;
      _M0L6d2__loS293 = (uint16_t)_M0L6_2atmpS1569;
      _M0L6_2atmpS1559 = _M0Lm6offsetS283;
      _M0L6_2atmpS1558 = _M0L12digit__startS285 + _M0L6_2atmpS1559;
      _M0L6bufferS294[_M0L6_2atmpS1558] = _M0L6d1__hiS290;
      _M0L6_2atmpS1562 = _M0Lm6offsetS283;
      _M0L6_2atmpS1561 = _M0L12digit__startS285 + _M0L6_2atmpS1562;
      _M0L6_2atmpS1560 = _M0L6_2atmpS1561 + 1;
      _M0L6bufferS294[_M0L6_2atmpS1560] = _M0L6d1__loS291;
      _M0L6_2atmpS1565 = _M0Lm6offsetS283;
      _M0L6_2atmpS1564 = _M0L12digit__startS285 + _M0L6_2atmpS1565;
      _M0L6_2atmpS1563 = _M0L6_2atmpS1564 + 2;
      _M0L6bufferS294[_M0L6_2atmpS1563] = _M0L6d2__hiS292;
      _M0L6_2atmpS1568 = _M0Lm6offsetS283;
      _M0L6_2atmpS1567 = _M0L12digit__startS285 + _M0L6_2atmpS1568;
      _M0L6_2atmpS1566 = _M0L6_2atmpS1567 + 3;
      _M0L6bufferS294[_M0L6_2atmpS1566] = _M0L6d2__loS293;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1612 = _M0Lm3numS281;
  _M0Lm9remainingS296 = *(int32_t*)&_M0L6_2atmpS1612;
  while (1) {
    int32_t _M0L6_2atmpS1580 = _M0Lm9remainingS296;
    if (_M0L6_2atmpS1580 >= 100) {
      int32_t _M0L6_2atmpS1592 = _M0Lm9remainingS296;
      int32_t _M0L1tS297 = _M0L6_2atmpS1592 / 100;
      int32_t _M0L6_2atmpS1591 = _M0Lm9remainingS296;
      int32_t _M0L1dS298 = _M0L6_2atmpS1591 % 100;
      int32_t _M0L6_2atmpS1581;
      int32_t _M0L6_2atmpS1590;
      int32_t _M0L6_2atmpS1589;
      int32_t _M0L5d__hiS299;
      int32_t _M0L6_2atmpS1588;
      int32_t _M0L6_2atmpS1587;
      int32_t _M0L5d__loS300;
      int32_t _M0L6_2atmpS1583;
      int32_t _M0L6_2atmpS1582;
      int32_t _M0L6_2atmpS1586;
      int32_t _M0L6_2atmpS1585;
      int32_t _M0L6_2atmpS1584;
      _M0Lm9remainingS296 = _M0L1tS297;
      _M0L6_2atmpS1581 = _M0Lm6offsetS283;
      _M0Lm6offsetS283 = _M0L6_2atmpS1581 - 2;
      _M0L6_2atmpS1590 = _M0L1dS298 / 10;
      _M0L6_2atmpS1589 = 48 + _M0L6_2atmpS1590;
      _M0L5d__hiS299 = (uint16_t)_M0L6_2atmpS1589;
      _M0L6_2atmpS1588 = _M0L1dS298 % 10;
      _M0L6_2atmpS1587 = 48 + _M0L6_2atmpS1588;
      _M0L5d__loS300 = (uint16_t)_M0L6_2atmpS1587;
      _M0L6_2atmpS1583 = _M0Lm6offsetS283;
      _M0L6_2atmpS1582 = _M0L12digit__startS285 + _M0L6_2atmpS1583;
      _M0L6bufferS294[_M0L6_2atmpS1582] = _M0L5d__hiS299;
      _M0L6_2atmpS1586 = _M0Lm6offsetS283;
      _M0L6_2atmpS1585 = _M0L12digit__startS285 + _M0L6_2atmpS1586;
      _M0L6_2atmpS1584 = _M0L6_2atmpS1585 + 1;
      _M0L6bufferS294[_M0L6_2atmpS1584] = _M0L5d__loS300;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1593 = _M0Lm9remainingS296;
  if (_M0L6_2atmpS1593 >= 10) {
    int32_t _M0L6_2atmpS1594 = _M0Lm6offsetS283;
    int32_t _M0L6_2atmpS1605;
    int32_t _M0L6_2atmpS1604;
    int32_t _M0L6_2atmpS1603;
    int32_t _M0L5d__hiS302;
    int32_t _M0L6_2atmpS1602;
    int32_t _M0L6_2atmpS1601;
    int32_t _M0L6_2atmpS1600;
    int32_t _M0L5d__loS303;
    int32_t _M0L6_2atmpS1596;
    int32_t _M0L6_2atmpS1595;
    int32_t _M0L6_2atmpS1599;
    int32_t _M0L6_2atmpS1598;
    int32_t _M0L6_2atmpS1597;
    _M0Lm6offsetS283 = _M0L6_2atmpS1594 - 2;
    _M0L6_2atmpS1605 = _M0Lm9remainingS296;
    _M0L6_2atmpS1604 = _M0L6_2atmpS1605 / 10;
    _M0L6_2atmpS1603 = 48 + _M0L6_2atmpS1604;
    _M0L5d__hiS302 = (uint16_t)_M0L6_2atmpS1603;
    _M0L6_2atmpS1602 = _M0Lm9remainingS296;
    _M0L6_2atmpS1601 = _M0L6_2atmpS1602 % 10;
    _M0L6_2atmpS1600 = 48 + _M0L6_2atmpS1601;
    _M0L5d__loS303 = (uint16_t)_M0L6_2atmpS1600;
    _M0L6_2atmpS1596 = _M0Lm6offsetS283;
    _M0L6_2atmpS1595 = _M0L12digit__startS285 + _M0L6_2atmpS1596;
    _M0L6bufferS294[_M0L6_2atmpS1595] = _M0L5d__hiS302;
    _M0L6_2atmpS1599 = _M0Lm6offsetS283;
    _M0L6_2atmpS1598 = _M0L12digit__startS285 + _M0L6_2atmpS1599;
    _M0L6_2atmpS1597 = _M0L6_2atmpS1598 + 1;
    _M0L6bufferS294[_M0L6_2atmpS1597] = _M0L5d__loS303;
    moonbit_decref(_M0L6bufferS294);
  } else {
    int32_t _M0L6_2atmpS1606 = _M0Lm6offsetS283;
    int32_t _M0L6_2atmpS1611;
    int32_t _M0L6_2atmpS1607;
    int32_t _M0L6_2atmpS1610;
    int32_t _M0L6_2atmpS1609;
    int32_t _M0L6_2atmpS1608;
    _M0Lm6offsetS283 = _M0L6_2atmpS1606 - 1;
    _M0L6_2atmpS1611 = _M0Lm6offsetS283;
    _M0L6_2atmpS1607 = _M0L12digit__startS285 + _M0L6_2atmpS1611;
    _M0L6_2atmpS1610 = _M0Lm9remainingS296;
    _M0L6_2atmpS1609 = 48 + _M0L6_2atmpS1610;
    _M0L6_2atmpS1608 = (uint16_t)_M0L6_2atmpS1609;
    _M0L6bufferS294[_M0L6_2atmpS1607] = _M0L6_2atmpS1608;
    moonbit_decref(_M0L6bufferS294);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS276,
  uint32_t _M0L3numS270,
  int32_t _M0L12digit__startS268,
  int32_t _M0L10total__lenS267,
  int32_t _M0L5radixS272
) {
  int32_t _M0Lm6offsetS266;
  uint32_t _M0Lm1nS269;
  uint32_t _M0L4baseS271;
  int32_t _M0L6_2atmpS1538;
  int32_t _M0L6_2atmpS1537;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS266 = _M0L10total__lenS267 - _M0L12digit__startS268;
  _M0Lm1nS269 = _M0L3numS270;
  _M0L4baseS271 = *(uint32_t*)&_M0L5radixS272;
  _M0L6_2atmpS1538 = _M0L5radixS272 - 1;
  _M0L6_2atmpS1537 = _M0L5radixS272 & _M0L6_2atmpS1538;
  if (_M0L6_2atmpS1537 == 0) {
    int32_t _M0L5shiftS273;
    uint32_t _M0L4maskS274;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS273 = moonbit_ctz32(_M0L5radixS272);
    _M0L4maskS274 = _M0L4baseS271 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1539 = _M0Lm1nS269;
      if (_M0L6_2atmpS1539 > 0u) {
        int32_t _M0L6_2atmpS1540 = _M0Lm6offsetS266;
        uint32_t _M0L6_2atmpS1546;
        uint32_t _M0L6_2atmpS1545;
        int32_t _M0L5digitS275;
        int32_t _M0L6_2atmpS1543;
        int32_t _M0L6_2atmpS1541;
        int32_t _M0L6_2atmpS1542;
        uint32_t _M0L6_2atmpS1544;
        _M0Lm6offsetS266 = _M0L6_2atmpS1540 - 1;
        _M0L6_2atmpS1546 = _M0Lm1nS269;
        _M0L6_2atmpS1545 = _M0L6_2atmpS1546 & _M0L4maskS274;
        _M0L5digitS275 = *(int32_t*)&_M0L6_2atmpS1545;
        _M0L6_2atmpS1543 = _M0Lm6offsetS266;
        _M0L6_2atmpS1541 = _M0L12digit__startS268 + _M0L6_2atmpS1543;
        _M0L6_2atmpS1542
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS275
        ];
        _M0L6bufferS276[_M0L6_2atmpS1541] = _M0L6_2atmpS1542;
        _M0L6_2atmpS1544 = _M0Lm1nS269;
        _M0Lm1nS269 = _M0L6_2atmpS1544 >> (_M0L5shiftS273 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS276);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1547 = _M0Lm1nS269;
      if (_M0L6_2atmpS1547 > 0u) {
        int32_t _M0L6_2atmpS1548 = _M0Lm6offsetS266;
        uint32_t _M0L6_2atmpS1555;
        uint32_t _M0L1qS278;
        uint32_t _M0L6_2atmpS1553;
        uint32_t _M0L6_2atmpS1554;
        uint32_t _M0L6_2atmpS1552;
        int32_t _M0L5digitS279;
        int32_t _M0L6_2atmpS1551;
        int32_t _M0L6_2atmpS1549;
        int32_t _M0L6_2atmpS1550;
        _M0Lm6offsetS266 = _M0L6_2atmpS1548 - 1;
        _M0L6_2atmpS1555 = _M0Lm1nS269;
        _M0L1qS278 = _M0L6_2atmpS1555 / _M0L4baseS271;
        _M0L6_2atmpS1553 = _M0Lm1nS269;
        _M0L6_2atmpS1554 = _M0L1qS278 * _M0L4baseS271;
        _M0L6_2atmpS1552 = _M0L6_2atmpS1553 - _M0L6_2atmpS1554;
        _M0L5digitS279 = *(int32_t*)&_M0L6_2atmpS1552;
        _M0L6_2atmpS1551 = _M0Lm6offsetS266;
        _M0L6_2atmpS1549 = _M0L12digit__startS268 + _M0L6_2atmpS1551;
        _M0L6_2atmpS1550
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS279
        ];
        _M0L6bufferS276[_M0L6_2atmpS1549] = _M0L6_2atmpS1550;
        _M0Lm1nS269 = _M0L1qS278;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS276);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS263,
  uint32_t _M0L3numS259,
  int32_t _M0L12digit__startS257,
  int32_t _M0L10total__lenS256
) {
  int32_t _M0Lm6offsetS255;
  uint32_t _M0Lm1nS258;
  int32_t _M0L6_2atmpS1533;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS255 = _M0L10total__lenS256 - _M0L12digit__startS257;
  _M0Lm1nS258 = _M0L3numS259;
  while (1) {
    int32_t _M0L6_2atmpS1521 = _M0Lm6offsetS255;
    if (_M0L6_2atmpS1521 >= 2) {
      int32_t _M0L6_2atmpS1522 = _M0Lm6offsetS255;
      uint32_t _M0L6_2atmpS1532;
      uint32_t _M0L6_2atmpS1531;
      int32_t _M0L9byte__valS260;
      int32_t _M0L2hiS261;
      int32_t _M0L2loS262;
      int32_t _M0L6_2atmpS1525;
      int32_t _M0L6_2atmpS1523;
      int32_t _M0L6_2atmpS1524;
      int32_t _M0L6_2atmpS1529;
      int32_t _M0L6_2atmpS1528;
      int32_t _M0L6_2atmpS1526;
      int32_t _M0L6_2atmpS1527;
      uint32_t _M0L6_2atmpS1530;
      _M0Lm6offsetS255 = _M0L6_2atmpS1522 - 2;
      _M0L6_2atmpS1532 = _M0Lm1nS258;
      _M0L6_2atmpS1531 = _M0L6_2atmpS1532 & 255u;
      _M0L9byte__valS260 = *(int32_t*)&_M0L6_2atmpS1531;
      _M0L2hiS261 = _M0L9byte__valS260 / 16;
      _M0L2loS262 = _M0L9byte__valS260 % 16;
      _M0L6_2atmpS1525 = _M0Lm6offsetS255;
      _M0L6_2atmpS1523 = _M0L12digit__startS257 + _M0L6_2atmpS1525;
      _M0L6_2atmpS1524
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2hiS261
      ];
      _M0L6bufferS263[_M0L6_2atmpS1523] = _M0L6_2atmpS1524;
      _M0L6_2atmpS1529 = _M0Lm6offsetS255;
      _M0L6_2atmpS1528 = _M0L12digit__startS257 + _M0L6_2atmpS1529;
      _M0L6_2atmpS1526 = _M0L6_2atmpS1528 + 1;
      _M0L6_2atmpS1527
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2loS262
      ];
      _M0L6bufferS263[_M0L6_2atmpS1526] = _M0L6_2atmpS1527;
      _M0L6_2atmpS1530 = _M0Lm1nS258;
      _M0Lm1nS258 = _M0L6_2atmpS1530 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1533 = _M0Lm6offsetS255;
  if (_M0L6_2atmpS1533 == 1) {
    uint32_t _M0L6_2atmpS1536 = _M0Lm1nS258;
    uint32_t _M0L6_2atmpS1535 = _M0L6_2atmpS1536 & 15u;
    int32_t _M0L6nibbleS265 = *(int32_t*)&_M0L6_2atmpS1535;
    int32_t _M0L6_2atmpS1534 =
      ((moonbit_string_t)moonbit_string_literal_106.data)[_M0L6nibbleS265];
    _M0L6bufferS263[_M0L12digit__startS257] = _M0L6_2atmpS1534;
    moonbit_decref(_M0L6bufferS263);
  } else {
    moonbit_decref(_M0L6bufferS263);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS250) {
  struct _M0TWEOs* _M0L7_2afuncS249;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS249 = _M0L4selfS250;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS249->code(_M0L7_2afuncS249);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS252
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS251;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS251 = _M0L4selfS252;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS251->code(_M0L7_2afuncS251);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS254) {
  struct _M0TWEOc* _M0L7_2afuncS253;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS253 = _M0L4selfS254;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS253->code(_M0L7_2afuncS253);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS242
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS241;
  struct _M0TPB6Logger _M0L6_2atmpS1517;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS241 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS241);
  _M0L6_2atmpS1517
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS241
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS242, _M0L6_2atmpS1517);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS241);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS244
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS243;
  struct _M0TPB6Logger _M0L6_2atmpS1518;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS243 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS243);
  _M0L6_2atmpS1518
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS243
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS244, _M0L6_2atmpS1518);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS243);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS246
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS245;
  struct _M0TPB6Logger _M0L6_2atmpS1519;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS245 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS245);
  _M0L6_2atmpS1519
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS245
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS246, _M0L6_2atmpS1519);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS245);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS247;
  struct _M0TPB6Logger _M0L6_2atmpS1520;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS247);
  _M0L6_2atmpS1520
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS247
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS248, _M0L6_2atmpS1520);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS247);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS240
) {
  int32_t _M0L8_2afieldS3033;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3033 = _M0L4selfS240.$1;
  moonbit_decref(_M0L4selfS240.$0);
  return _M0L8_2afieldS3033;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS239
) {
  int32_t _M0L3endS1515;
  int32_t _M0L8_2afieldS3034;
  int32_t _M0L5startS1516;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1515 = _M0L4selfS239.$2;
  _M0L8_2afieldS3034 = _M0L4selfS239.$1;
  moonbit_decref(_M0L4selfS239.$0);
  _M0L5startS1516 = _M0L8_2afieldS3034;
  return _M0L3endS1515 - _M0L5startS1516;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS238
) {
  moonbit_string_t _M0L8_2afieldS3035;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3035 = _M0L4selfS238.$0;
  return _M0L8_2afieldS3035;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS234,
  moonbit_string_t _M0L5valueS235,
  int32_t _M0L5startS236,
  int32_t _M0L3lenS237
) {
  int32_t _M0L6_2atmpS1514;
  int64_t _M0L6_2atmpS1513;
  struct _M0TPC16string10StringView _M0L6_2atmpS1512;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1514 = _M0L5startS236 + _M0L3lenS237;
  _M0L6_2atmpS1513 = (int64_t)_M0L6_2atmpS1514;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1512
  = _M0MPC16string6String11sub_2einner(_M0L5valueS235, _M0L5startS236, _M0L6_2atmpS1513);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS234, _M0L6_2atmpS1512);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS227,
  int32_t _M0L5startS233,
  int64_t _M0L3endS229
) {
  int32_t _M0L3lenS226;
  int32_t _M0L3endS228;
  int32_t _M0L5startS232;
  int32_t _if__result_3341;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS226 = Moonbit_array_length(_M0L4selfS227);
  if (_M0L3endS229 == 4294967296ll) {
    _M0L3endS228 = _M0L3lenS226;
  } else {
    int64_t _M0L7_2aSomeS230 = _M0L3endS229;
    int32_t _M0L6_2aendS231 = (int32_t)_M0L7_2aSomeS230;
    if (_M0L6_2aendS231 < 0) {
      _M0L3endS228 = _M0L3lenS226 + _M0L6_2aendS231;
    } else {
      _M0L3endS228 = _M0L6_2aendS231;
    }
  }
  if (_M0L5startS233 < 0) {
    _M0L5startS232 = _M0L3lenS226 + _M0L5startS233;
  } else {
    _M0L5startS232 = _M0L5startS233;
  }
  if (_M0L5startS232 >= 0) {
    if (_M0L5startS232 <= _M0L3endS228) {
      _if__result_3341 = _M0L3endS228 <= _M0L3lenS226;
    } else {
      _if__result_3341 = 0;
    }
  } else {
    _if__result_3341 = 0;
  }
  if (_if__result_3341) {
    if (_M0L5startS232 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1509 = _M0L4selfS227[_M0L5startS232];
      int32_t _M0L6_2atmpS1508;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1508
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1509);
      if (!_M0L6_2atmpS1508) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS228 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1511 = _M0L4selfS227[_M0L3endS228];
      int32_t _M0L6_2atmpS1510;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1510
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1511);
      if (!_M0L6_2atmpS1510) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS232,
                                                 _M0L3endS228,
                                                 _M0L4selfS227};
  } else {
    moonbit_decref(_M0L4selfS227);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS223) {
  struct _M0TPB6Hasher* _M0L1hS222;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS222 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS222);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS222, _M0L4selfS223);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS222);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS225
) {
  struct _M0TPB6Hasher* _M0L1hS224;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS224 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS224);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS224, _M0L4selfS225);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS224);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS220) {
  int32_t _M0L4seedS219;
  if (_M0L10seed_2eoptS220 == 4294967296ll) {
    _M0L4seedS219 = 0;
  } else {
    int64_t _M0L7_2aSomeS221 = _M0L10seed_2eoptS220;
    _M0L4seedS219 = (int32_t)_M0L7_2aSomeS221;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS219);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS218) {
  uint32_t _M0L6_2atmpS1507;
  uint32_t _M0L6_2atmpS1506;
  struct _M0TPB6Hasher* _block_3342;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1507 = *(uint32_t*)&_M0L4seedS218;
  _M0L6_2atmpS1506 = _M0L6_2atmpS1507 + 374761393u;
  _block_3342
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3342)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3342->$0 = _M0L6_2atmpS1506;
  return _block_3342;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS217) {
  uint32_t _M0L6_2atmpS1505;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1505 = _M0MPB6Hasher9avalanche(_M0L4selfS217);
  return *(int32_t*)&_M0L6_2atmpS1505;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS216) {
  uint32_t _M0L8_2afieldS3036;
  uint32_t _M0Lm3accS215;
  uint32_t _M0L6_2atmpS1494;
  uint32_t _M0L6_2atmpS1496;
  uint32_t _M0L6_2atmpS1495;
  uint32_t _M0L6_2atmpS1497;
  uint32_t _M0L6_2atmpS1498;
  uint32_t _M0L6_2atmpS1500;
  uint32_t _M0L6_2atmpS1499;
  uint32_t _M0L6_2atmpS1501;
  uint32_t _M0L6_2atmpS1502;
  uint32_t _M0L6_2atmpS1504;
  uint32_t _M0L6_2atmpS1503;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3036 = _M0L4selfS216->$0;
  moonbit_decref(_M0L4selfS216);
  _M0Lm3accS215 = _M0L8_2afieldS3036;
  _M0L6_2atmpS1494 = _M0Lm3accS215;
  _M0L6_2atmpS1496 = _M0Lm3accS215;
  _M0L6_2atmpS1495 = _M0L6_2atmpS1496 >> 15;
  _M0Lm3accS215 = _M0L6_2atmpS1494 ^ _M0L6_2atmpS1495;
  _M0L6_2atmpS1497 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1497 * 2246822519u;
  _M0L6_2atmpS1498 = _M0Lm3accS215;
  _M0L6_2atmpS1500 = _M0Lm3accS215;
  _M0L6_2atmpS1499 = _M0L6_2atmpS1500 >> 13;
  _M0Lm3accS215 = _M0L6_2atmpS1498 ^ _M0L6_2atmpS1499;
  _M0L6_2atmpS1501 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1501 * 3266489917u;
  _M0L6_2atmpS1502 = _M0Lm3accS215;
  _M0L6_2atmpS1504 = _M0Lm3accS215;
  _M0L6_2atmpS1503 = _M0L6_2atmpS1504 >> 16;
  _M0Lm3accS215 = _M0L6_2atmpS1502 ^ _M0L6_2atmpS1503;
  return _M0Lm3accS215;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS213,
  moonbit_string_t _M0L1yS214
) {
  int32_t _M0L6_2atmpS3037;
  int32_t _M0L6_2atmpS1493;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3037 = moonbit_val_array_equal(_M0L1xS213, _M0L1yS214);
  moonbit_decref(_M0L1xS213);
  moonbit_decref(_M0L1yS214);
  _M0L6_2atmpS1493 = _M0L6_2atmpS3037;
  return !_M0L6_2atmpS1493;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS210,
  int32_t _M0L5valueS209
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS209, _M0L4selfS210);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS212,
  moonbit_string_t _M0L5valueS211
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS211, _M0L4selfS212);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS208) {
  int64_t _M0L6_2atmpS1492;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1492 = (int64_t)_M0L4selfS208;
  return *(uint64_t*)&_M0L6_2atmpS1492;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS206,
  moonbit_string_t _M0L4reprS207
) {
  void* _block_3343;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3343 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_3343)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_3343)->$0 = _M0L6numberS206;
  ((struct _M0DTPB4Json6Number*)_block_3343)->$1 = _M0L4reprS207;
  return _block_3343;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS204,
  int32_t _M0L5valueS205
) {
  uint32_t _M0L6_2atmpS1491;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1491 = *(uint32_t*)&_M0L5valueS205;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS204, _M0L6_2atmpS1491);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS197
) {
  struct _M0TPB13StringBuilder* _M0L3bufS195;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS196;
  int32_t _M0L7_2abindS198;
  int32_t _M0L1iS199;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS195 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS196 = _M0L4selfS197;
  moonbit_incref(_M0L3bufS195);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS195, 91);
  _M0L7_2abindS198 = _M0L7_2aselfS196->$1;
  _M0L1iS199 = 0;
  while (1) {
    if (_M0L1iS199 < _M0L7_2abindS198) {
      int32_t _if__result_3345;
      moonbit_string_t* _M0L8_2afieldS3039;
      moonbit_string_t* _M0L3bufS1489;
      moonbit_string_t _M0L6_2atmpS3038;
      moonbit_string_t _M0L4itemS200;
      int32_t _M0L6_2atmpS1490;
      if (_M0L1iS199 != 0) {
        moonbit_incref(_M0L3bufS195);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS195, (moonbit_string_t)moonbit_string_literal_108.data);
      }
      if (_M0L1iS199 < 0) {
        _if__result_3345 = 1;
      } else {
        int32_t _M0L3lenS1488 = _M0L7_2aselfS196->$1;
        _if__result_3345 = _M0L1iS199 >= _M0L3lenS1488;
      }
      if (_if__result_3345) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3039 = _M0L7_2aselfS196->$0;
      _M0L3bufS1489 = _M0L8_2afieldS3039;
      _M0L6_2atmpS3038 = (moonbit_string_t)_M0L3bufS1489[_M0L1iS199];
      _M0L4itemS200 = _M0L6_2atmpS3038;
      if (_M0L4itemS200 == 0) {
        moonbit_incref(_M0L3bufS195);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS195, (moonbit_string_t)moonbit_string_literal_72.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS201 = _M0L4itemS200;
        moonbit_string_t _M0L6_2alocS202 = _M0L7_2aSomeS201;
        moonbit_string_t _M0L6_2atmpS1487;
        moonbit_incref(_M0L6_2alocS202);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1487
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS202);
        moonbit_incref(_M0L3bufS195);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS195, _M0L6_2atmpS1487);
      }
      _M0L6_2atmpS1490 = _M0L1iS199 + 1;
      _M0L1iS199 = _M0L6_2atmpS1490;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS196);
    }
    break;
  }
  moonbit_incref(_M0L3bufS195);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS195, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS195);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS194
) {
  moonbit_string_t _M0L6_2atmpS1486;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1485;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1486 = _M0L4selfS194;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1485 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1486);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1485);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS193
) {
  struct _M0TPB13StringBuilder* _M0L2sbS192;
  struct _M0TPC16string10StringView _M0L8_2afieldS3052;
  struct _M0TPC16string10StringView _M0L3pkgS1470;
  moonbit_string_t _M0L6_2atmpS1469;
  moonbit_string_t _M0L6_2atmpS3051;
  moonbit_string_t _M0L6_2atmpS1468;
  moonbit_string_t _M0L6_2atmpS3050;
  moonbit_string_t _M0L6_2atmpS1467;
  struct _M0TPC16string10StringView _M0L8_2afieldS3049;
  struct _M0TPC16string10StringView _M0L8filenameS1471;
  struct _M0TPC16string10StringView _M0L8_2afieldS3048;
  struct _M0TPC16string10StringView _M0L11start__lineS1474;
  moonbit_string_t _M0L6_2atmpS1473;
  moonbit_string_t _M0L6_2atmpS3047;
  moonbit_string_t _M0L6_2atmpS1472;
  struct _M0TPC16string10StringView _M0L8_2afieldS3046;
  struct _M0TPC16string10StringView _M0L13start__columnS1477;
  moonbit_string_t _M0L6_2atmpS1476;
  moonbit_string_t _M0L6_2atmpS3045;
  moonbit_string_t _M0L6_2atmpS1475;
  struct _M0TPC16string10StringView _M0L8_2afieldS3044;
  struct _M0TPC16string10StringView _M0L9end__lineS1480;
  moonbit_string_t _M0L6_2atmpS1479;
  moonbit_string_t _M0L6_2atmpS3043;
  moonbit_string_t _M0L6_2atmpS1478;
  struct _M0TPC16string10StringView _M0L8_2afieldS3042;
  int32_t _M0L6_2acntS3180;
  struct _M0TPC16string10StringView _M0L11end__columnS1484;
  moonbit_string_t _M0L6_2atmpS1483;
  moonbit_string_t _M0L6_2atmpS3041;
  moonbit_string_t _M0L6_2atmpS1482;
  moonbit_string_t _M0L6_2atmpS3040;
  moonbit_string_t _M0L6_2atmpS1481;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS192 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3052
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$0_1, _M0L4selfS193->$0_2, _M0L4selfS193->$0_0
  };
  _M0L3pkgS1470 = _M0L8_2afieldS3052;
  moonbit_incref(_M0L3pkgS1470.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1469
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1470);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3051
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_109.data, _M0L6_2atmpS1469);
  moonbit_decref(_M0L6_2atmpS1469);
  _M0L6_2atmpS1468 = _M0L6_2atmpS3051;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3050
  = moonbit_add_string(_M0L6_2atmpS1468, (moonbit_string_t)moonbit_string_literal_110.data);
  moonbit_decref(_M0L6_2atmpS1468);
  _M0L6_2atmpS1467 = _M0L6_2atmpS3050;
  moonbit_incref(_M0L2sbS192);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1467);
  moonbit_incref(_M0L2sbS192);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, (moonbit_string_t)moonbit_string_literal_111.data);
  _M0L8_2afieldS3049
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$1_1, _M0L4selfS193->$1_2, _M0L4selfS193->$1_0
  };
  _M0L8filenameS1471 = _M0L8_2afieldS3049;
  moonbit_incref(_M0L8filenameS1471.$0);
  moonbit_incref(_M0L2sbS192);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS192, _M0L8filenameS1471);
  _M0L8_2afieldS3048
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$2_1, _M0L4selfS193->$2_2, _M0L4selfS193->$2_0
  };
  _M0L11start__lineS1474 = _M0L8_2afieldS3048;
  moonbit_incref(_M0L11start__lineS1474.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1473
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1474);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3047
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_112.data, _M0L6_2atmpS1473);
  moonbit_decref(_M0L6_2atmpS1473);
  _M0L6_2atmpS1472 = _M0L6_2atmpS3047;
  moonbit_incref(_M0L2sbS192);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1472);
  _M0L8_2afieldS3046
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$3_1, _M0L4selfS193->$3_2, _M0L4selfS193->$3_0
  };
  _M0L13start__columnS1477 = _M0L8_2afieldS3046;
  moonbit_incref(_M0L13start__columnS1477.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1476
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1477);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3045
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_113.data, _M0L6_2atmpS1476);
  moonbit_decref(_M0L6_2atmpS1476);
  _M0L6_2atmpS1475 = _M0L6_2atmpS3045;
  moonbit_incref(_M0L2sbS192);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1475);
  _M0L8_2afieldS3044
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$4_1, _M0L4selfS193->$4_2, _M0L4selfS193->$4_0
  };
  _M0L9end__lineS1480 = _M0L8_2afieldS3044;
  moonbit_incref(_M0L9end__lineS1480.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1479
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1480);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3043
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_114.data, _M0L6_2atmpS1479);
  moonbit_decref(_M0L6_2atmpS1479);
  _M0L6_2atmpS1478 = _M0L6_2atmpS3043;
  moonbit_incref(_M0L2sbS192);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1478);
  _M0L8_2afieldS3042
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$5_1, _M0L4selfS193->$5_2, _M0L4selfS193->$5_0
  };
  _M0L6_2acntS3180 = Moonbit_object_header(_M0L4selfS193)->rc;
  if (_M0L6_2acntS3180 > 1) {
    int32_t _M0L11_2anew__cntS3186 = _M0L6_2acntS3180 - 1;
    Moonbit_object_header(_M0L4selfS193)->rc = _M0L11_2anew__cntS3186;
    moonbit_incref(_M0L8_2afieldS3042.$0);
  } else if (_M0L6_2acntS3180 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3185 =
      (struct _M0TPC16string10StringView){_M0L4selfS193->$4_1,
                                            _M0L4selfS193->$4_2,
                                            _M0L4selfS193->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3184;
    struct _M0TPC16string10StringView _M0L8_2afieldS3183;
    struct _M0TPC16string10StringView _M0L8_2afieldS3182;
    struct _M0TPC16string10StringView _M0L8_2afieldS3181;
    moonbit_decref(_M0L8_2afieldS3185.$0);
    _M0L8_2afieldS3184
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$3_1, _M0L4selfS193->$3_2, _M0L4selfS193->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3184.$0);
    _M0L8_2afieldS3183
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$2_1, _M0L4selfS193->$2_2, _M0L4selfS193->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3183.$0);
    _M0L8_2afieldS3182
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$1_1, _M0L4selfS193->$1_2, _M0L4selfS193->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3182.$0);
    _M0L8_2afieldS3181
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$0_1, _M0L4selfS193->$0_2, _M0L4selfS193->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3181.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS193);
  }
  _M0L11end__columnS1484 = _M0L8_2afieldS3042;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1483
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1484);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3041
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_115.data, _M0L6_2atmpS1483);
  moonbit_decref(_M0L6_2atmpS1483);
  _M0L6_2atmpS1482 = _M0L6_2atmpS3041;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3040
  = moonbit_add_string(_M0L6_2atmpS1482, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1482);
  _M0L6_2atmpS1481 = _M0L6_2atmpS3040;
  moonbit_incref(_M0L2sbS192);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1481);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS192);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS190,
  moonbit_string_t _M0L3strS191
) {
  int32_t _M0L3lenS1457;
  int32_t _M0L6_2atmpS1459;
  int32_t _M0L6_2atmpS1458;
  int32_t _M0L6_2atmpS1456;
  moonbit_bytes_t _M0L8_2afieldS3054;
  moonbit_bytes_t _M0L4dataS1460;
  int32_t _M0L3lenS1461;
  int32_t _M0L6_2atmpS1462;
  int32_t _M0L3lenS1464;
  int32_t _M0L6_2atmpS3053;
  int32_t _M0L6_2atmpS1466;
  int32_t _M0L6_2atmpS1465;
  int32_t _M0L6_2atmpS1463;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1457 = _M0L4selfS190->$1;
  _M0L6_2atmpS1459 = Moonbit_array_length(_M0L3strS191);
  _M0L6_2atmpS1458 = _M0L6_2atmpS1459 * 2;
  _M0L6_2atmpS1456 = _M0L3lenS1457 + _M0L6_2atmpS1458;
  moonbit_incref(_M0L4selfS190);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS190, _M0L6_2atmpS1456);
  _M0L8_2afieldS3054 = _M0L4selfS190->$0;
  _M0L4dataS1460 = _M0L8_2afieldS3054;
  _M0L3lenS1461 = _M0L4selfS190->$1;
  _M0L6_2atmpS1462 = Moonbit_array_length(_M0L3strS191);
  moonbit_incref(_M0L4dataS1460);
  moonbit_incref(_M0L3strS191);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1460, _M0L3lenS1461, _M0L3strS191, 0, _M0L6_2atmpS1462);
  _M0L3lenS1464 = _M0L4selfS190->$1;
  _M0L6_2atmpS3053 = Moonbit_array_length(_M0L3strS191);
  moonbit_decref(_M0L3strS191);
  _M0L6_2atmpS1466 = _M0L6_2atmpS3053;
  _M0L6_2atmpS1465 = _M0L6_2atmpS1466 * 2;
  _M0L6_2atmpS1463 = _M0L3lenS1464 + _M0L6_2atmpS1465;
  _M0L4selfS190->$1 = _M0L6_2atmpS1463;
  moonbit_decref(_M0L4selfS190);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS182,
  int32_t _M0L13bytes__offsetS177,
  moonbit_string_t _M0L3strS184,
  int32_t _M0L11str__offsetS180,
  int32_t _M0L6lengthS178
) {
  int32_t _M0L6_2atmpS1455;
  int32_t _M0L6_2atmpS1454;
  int32_t _M0L2e1S176;
  int32_t _M0L6_2atmpS1453;
  int32_t _M0L2e2S179;
  int32_t _M0L4len1S181;
  int32_t _M0L4len2S183;
  int32_t _if__result_3346;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1455 = _M0L6lengthS178 * 2;
  _M0L6_2atmpS1454 = _M0L13bytes__offsetS177 + _M0L6_2atmpS1455;
  _M0L2e1S176 = _M0L6_2atmpS1454 - 1;
  _M0L6_2atmpS1453 = _M0L11str__offsetS180 + _M0L6lengthS178;
  _M0L2e2S179 = _M0L6_2atmpS1453 - 1;
  _M0L4len1S181 = Moonbit_array_length(_M0L4selfS182);
  _M0L4len2S183 = Moonbit_array_length(_M0L3strS184);
  if (_M0L6lengthS178 >= 0) {
    if (_M0L13bytes__offsetS177 >= 0) {
      if (_M0L2e1S176 < _M0L4len1S181) {
        if (_M0L11str__offsetS180 >= 0) {
          _if__result_3346 = _M0L2e2S179 < _M0L4len2S183;
        } else {
          _if__result_3346 = 0;
        }
      } else {
        _if__result_3346 = 0;
      }
    } else {
      _if__result_3346 = 0;
    }
  } else {
    _if__result_3346 = 0;
  }
  if (_if__result_3346) {
    int32_t _M0L16end__str__offsetS185 =
      _M0L11str__offsetS180 + _M0L6lengthS178;
    int32_t _M0L1iS186 = _M0L11str__offsetS180;
    int32_t _M0L1jS187 = _M0L13bytes__offsetS177;
    while (1) {
      if (_M0L1iS186 < _M0L16end__str__offsetS185) {
        int32_t _M0L6_2atmpS1450 = _M0L3strS184[_M0L1iS186];
        int32_t _M0L6_2atmpS1449 = (int32_t)_M0L6_2atmpS1450;
        uint32_t _M0L1cS188 = *(uint32_t*)&_M0L6_2atmpS1449;
        uint32_t _M0L6_2atmpS1445 = _M0L1cS188 & 255u;
        int32_t _M0L6_2atmpS1444;
        int32_t _M0L6_2atmpS1446;
        uint32_t _M0L6_2atmpS1448;
        int32_t _M0L6_2atmpS1447;
        int32_t _M0L6_2atmpS1451;
        int32_t _M0L6_2atmpS1452;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1444 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1445);
        if (
          _M0L1jS187 < 0 || _M0L1jS187 >= Moonbit_array_length(_M0L4selfS182)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS182[_M0L1jS187] = _M0L6_2atmpS1444;
        _M0L6_2atmpS1446 = _M0L1jS187 + 1;
        _M0L6_2atmpS1448 = _M0L1cS188 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1447 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1448);
        if (
          _M0L6_2atmpS1446 < 0
          || _M0L6_2atmpS1446 >= Moonbit_array_length(_M0L4selfS182)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS182[_M0L6_2atmpS1446] = _M0L6_2atmpS1447;
        _M0L6_2atmpS1451 = _M0L1iS186 + 1;
        _M0L6_2atmpS1452 = _M0L1jS187 + 2;
        _M0L1iS186 = _M0L6_2atmpS1451;
        _M0L1jS187 = _M0L6_2atmpS1452;
        continue;
      } else {
        moonbit_decref(_M0L3strS184);
        moonbit_decref(_M0L4selfS182);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS184);
    moonbit_decref(_M0L4selfS182);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS173,
  double _M0L3objS172
) {
  struct _M0TPB6Logger _M0L6_2atmpS1442;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1442
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS173
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS172, _M0L6_2atmpS1442);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS175,
  struct _M0TPC16string10StringView _M0L3objS174
) {
  struct _M0TPB6Logger _M0L6_2atmpS1443;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1443
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS175
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS174, _M0L6_2atmpS1443);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS118
) {
  int32_t _M0L6_2atmpS1441;
  struct _M0TPC16string10StringView _M0L7_2abindS117;
  moonbit_string_t _M0L7_2adataS119;
  int32_t _M0L8_2astartS120;
  int32_t _M0L6_2atmpS1440;
  int32_t _M0L6_2aendS121;
  int32_t _M0Lm9_2acursorS122;
  int32_t _M0Lm13accept__stateS123;
  int32_t _M0Lm10match__endS124;
  int32_t _M0Lm20match__tag__saver__0S125;
  int32_t _M0Lm20match__tag__saver__1S126;
  int32_t _M0Lm20match__tag__saver__2S127;
  int32_t _M0Lm20match__tag__saver__3S128;
  int32_t _M0Lm20match__tag__saver__4S129;
  int32_t _M0Lm6tag__0S130;
  int32_t _M0Lm6tag__1S131;
  int32_t _M0Lm9tag__1__1S132;
  int32_t _M0Lm9tag__1__2S133;
  int32_t _M0Lm6tag__3S134;
  int32_t _M0Lm6tag__2S135;
  int32_t _M0Lm9tag__2__1S136;
  int32_t _M0Lm6tag__4S137;
  int32_t _M0L6_2atmpS1398;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1441 = Moonbit_array_length(_M0L4reprS118);
  _M0L7_2abindS117
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1441, _M0L4reprS118
  };
  moonbit_incref(_M0L7_2abindS117.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS119 = _M0MPC16string10StringView4data(_M0L7_2abindS117);
  moonbit_incref(_M0L7_2abindS117.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS120
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS117);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1440 = _M0MPC16string10StringView6length(_M0L7_2abindS117);
  _M0L6_2aendS121 = _M0L8_2astartS120 + _M0L6_2atmpS1440;
  _M0Lm9_2acursorS122 = _M0L8_2astartS120;
  _M0Lm13accept__stateS123 = -1;
  _M0Lm10match__endS124 = -1;
  _M0Lm20match__tag__saver__0S125 = -1;
  _M0Lm20match__tag__saver__1S126 = -1;
  _M0Lm20match__tag__saver__2S127 = -1;
  _M0Lm20match__tag__saver__3S128 = -1;
  _M0Lm20match__tag__saver__4S129 = -1;
  _M0Lm6tag__0S130 = -1;
  _M0Lm6tag__1S131 = -1;
  _M0Lm9tag__1__1S132 = -1;
  _M0Lm9tag__1__2S133 = -1;
  _M0Lm6tag__3S134 = -1;
  _M0Lm6tag__2S135 = -1;
  _M0Lm9tag__2__1S136 = -1;
  _M0Lm6tag__4S137 = -1;
  _M0L6_2atmpS1398 = _M0Lm9_2acursorS122;
  if (_M0L6_2atmpS1398 < _M0L6_2aendS121) {
    int32_t _M0L6_2atmpS1400 = _M0Lm9_2acursorS122;
    int32_t _M0L6_2atmpS1399;
    moonbit_incref(_M0L7_2adataS119);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1399
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1400);
    if (_M0L6_2atmpS1399 == 64) {
      int32_t _M0L6_2atmpS1401 = _M0Lm9_2acursorS122;
      _M0Lm9_2acursorS122 = _M0L6_2atmpS1401 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1402;
        _M0Lm6tag__0S130 = _M0Lm9_2acursorS122;
        _M0L6_2atmpS1402 = _M0Lm9_2acursorS122;
        if (_M0L6_2atmpS1402 < _M0L6_2aendS121) {
          int32_t _M0L6_2atmpS1439 = _M0Lm9_2acursorS122;
          int32_t _M0L10next__charS145;
          int32_t _M0L6_2atmpS1403;
          moonbit_incref(_M0L7_2adataS119);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS145
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1439);
          _M0L6_2atmpS1403 = _M0Lm9_2acursorS122;
          _M0Lm9_2acursorS122 = _M0L6_2atmpS1403 + 1;
          if (_M0L10next__charS145 == 58) {
            int32_t _M0L6_2atmpS1404 = _M0Lm9_2acursorS122;
            if (_M0L6_2atmpS1404 < _M0L6_2aendS121) {
              int32_t _M0L6_2atmpS1405 = _M0Lm9_2acursorS122;
              int32_t _M0L12dispatch__15S146;
              _M0Lm9_2acursorS122 = _M0L6_2atmpS1405 + 1;
              _M0L12dispatch__15S146 = 0;
              loop__label__15_149:;
              while (1) {
                int32_t _M0L6_2atmpS1406;
                switch (_M0L12dispatch__15S146) {
                  case 3: {
                    int32_t _M0L6_2atmpS1409;
                    _M0Lm9tag__1__2S133 = _M0Lm9tag__1__1S132;
                    _M0Lm9tag__1__1S132 = _M0Lm6tag__1S131;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1409 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1409 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1414 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS153;
                      int32_t _M0L6_2atmpS1410;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS153
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1414);
                      _M0L6_2atmpS1410 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1410 + 1;
                      if (_M0L10next__charS153 < 58) {
                        if (_M0L10next__charS153 < 48) {
                          goto join_152;
                        } else {
                          int32_t _M0L6_2atmpS1411;
                          _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                          _M0Lm9tag__2__1S136 = _M0Lm6tag__2S135;
                          _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                          _M0Lm6tag__3S134 = _M0Lm9_2acursorS122;
                          _M0L6_2atmpS1411 = _M0Lm9_2acursorS122;
                          if (_M0L6_2atmpS1411 < _M0L6_2aendS121) {
                            int32_t _M0L6_2atmpS1413 = _M0Lm9_2acursorS122;
                            int32_t _M0L10next__charS155;
                            int32_t _M0L6_2atmpS1412;
                            moonbit_incref(_M0L7_2adataS119);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS155
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1413);
                            _M0L6_2atmpS1412 = _M0Lm9_2acursorS122;
                            _M0Lm9_2acursorS122 = _M0L6_2atmpS1412 + 1;
                            if (_M0L10next__charS155 < 48) {
                              if (_M0L10next__charS155 == 45) {
                                goto join_147;
                              } else {
                                goto join_154;
                              }
                            } else if (_M0L10next__charS155 > 57) {
                              if (_M0L10next__charS155 < 59) {
                                _M0L12dispatch__15S146 = 3;
                                goto loop__label__15_149;
                              } else {
                                goto join_154;
                              }
                            } else {
                              _M0L12dispatch__15S146 = 6;
                              goto loop__label__15_149;
                            }
                            join_154:;
                            _M0L12dispatch__15S146 = 0;
                            goto loop__label__15_149;
                          } else {
                            goto join_138;
                          }
                        }
                      } else if (_M0L10next__charS153 > 58) {
                        goto join_152;
                      } else {
                        _M0L12dispatch__15S146 = 1;
                        goto loop__label__15_149;
                      }
                      join_152:;
                      _M0L12dispatch__15S146 = 0;
                      goto loop__label__15_149;
                    } else {
                      goto join_138;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1415;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1415 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1415 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1417 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS157;
                      int32_t _M0L6_2atmpS1416;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS157
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1417);
                      _M0L6_2atmpS1416 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1416 + 1;
                      if (_M0L10next__charS157 < 58) {
                        if (_M0L10next__charS157 < 48) {
                          goto join_156;
                        } else {
                          _M0L12dispatch__15S146 = 2;
                          goto loop__label__15_149;
                        }
                      } else if (_M0L10next__charS157 > 58) {
                        goto join_156;
                      } else {
                        _M0L12dispatch__15S146 = 3;
                        goto loop__label__15_149;
                      }
                      join_156:;
                      _M0L12dispatch__15S146 = 0;
                      goto loop__label__15_149;
                    } else {
                      goto join_138;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1418;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1418 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1418 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1420 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS158;
                      int32_t _M0L6_2atmpS1419;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS158
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1420);
                      _M0L6_2atmpS1419 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1419 + 1;
                      if (_M0L10next__charS158 == 58) {
                        _M0L12dispatch__15S146 = 1;
                        goto loop__label__15_149;
                      } else {
                        _M0L12dispatch__15S146 = 0;
                        goto loop__label__15_149;
                      }
                    } else {
                      goto join_138;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1421;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__4S137 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1421 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1421 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1429 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1422;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1429);
                      _M0L6_2atmpS1422 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1422 + 1;
                      if (_M0L10next__charS160 < 58) {
                        if (_M0L10next__charS160 < 48) {
                          goto join_159;
                        } else {
                          _M0L12dispatch__15S146 = 4;
                          goto loop__label__15_149;
                        }
                      } else if (_M0L10next__charS160 > 58) {
                        goto join_159;
                      } else {
                        int32_t _M0L6_2atmpS1423;
                        _M0Lm9tag__1__2S133 = _M0Lm9tag__1__1S132;
                        _M0Lm9tag__1__1S132 = _M0Lm6tag__1S131;
                        _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                        _M0L6_2atmpS1423 = _M0Lm9_2acursorS122;
                        if (_M0L6_2atmpS1423 < _M0L6_2aendS121) {
                          int32_t _M0L6_2atmpS1428 = _M0Lm9_2acursorS122;
                          int32_t _M0L10next__charS162;
                          int32_t _M0L6_2atmpS1424;
                          moonbit_incref(_M0L7_2adataS119);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS162
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1428);
                          _M0L6_2atmpS1424 = _M0Lm9_2acursorS122;
                          _M0Lm9_2acursorS122 = _M0L6_2atmpS1424 + 1;
                          if (_M0L10next__charS162 < 58) {
                            if (_M0L10next__charS162 < 48) {
                              goto join_161;
                            } else {
                              int32_t _M0L6_2atmpS1425;
                              _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                              _M0Lm9tag__2__1S136 = _M0Lm6tag__2S135;
                              _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                              _M0L6_2atmpS1425 = _M0Lm9_2acursorS122;
                              if (_M0L6_2atmpS1425 < _M0L6_2aendS121) {
                                int32_t _M0L6_2atmpS1427 =
                                  _M0Lm9_2acursorS122;
                                int32_t _M0L10next__charS164;
                                int32_t _M0L6_2atmpS1426;
                                moonbit_incref(_M0L7_2adataS119);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS164
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1427);
                                _M0L6_2atmpS1426 = _M0Lm9_2acursorS122;
                                _M0Lm9_2acursorS122 = _M0L6_2atmpS1426 + 1;
                                if (_M0L10next__charS164 < 58) {
                                  if (_M0L10next__charS164 < 48) {
                                    goto join_163;
                                  } else {
                                    _M0L12dispatch__15S146 = 5;
                                    goto loop__label__15_149;
                                  }
                                } else if (_M0L10next__charS164 > 58) {
                                  goto join_163;
                                } else {
                                  _M0L12dispatch__15S146 = 3;
                                  goto loop__label__15_149;
                                }
                                join_163:;
                                _M0L12dispatch__15S146 = 0;
                                goto loop__label__15_149;
                              } else {
                                goto join_151;
                              }
                            }
                          } else if (_M0L10next__charS162 > 58) {
                            goto join_161;
                          } else {
                            _M0L12dispatch__15S146 = 1;
                            goto loop__label__15_149;
                          }
                          join_161:;
                          _M0L12dispatch__15S146 = 0;
                          goto loop__label__15_149;
                        } else {
                          goto join_138;
                        }
                      }
                      join_159:;
                      _M0L12dispatch__15S146 = 0;
                      goto loop__label__15_149;
                    } else {
                      goto join_138;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1430;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1430 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1430 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1432 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS166;
                      int32_t _M0L6_2atmpS1431;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS166
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1432);
                      _M0L6_2atmpS1431 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1431 + 1;
                      if (_M0L10next__charS166 < 58) {
                        if (_M0L10next__charS166 < 48) {
                          goto join_165;
                        } else {
                          _M0L12dispatch__15S146 = 5;
                          goto loop__label__15_149;
                        }
                      } else if (_M0L10next__charS166 > 58) {
                        goto join_165;
                      } else {
                        _M0L12dispatch__15S146 = 3;
                        goto loop__label__15_149;
                      }
                      join_165:;
                      _M0L12dispatch__15S146 = 0;
                      goto loop__label__15_149;
                    } else {
                      goto join_151;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1433;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__3S134 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1433 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1433 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1435 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS168;
                      int32_t _M0L6_2atmpS1434;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS168
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1435);
                      _M0L6_2atmpS1434 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1434 + 1;
                      if (_M0L10next__charS168 < 48) {
                        if (_M0L10next__charS168 == 45) {
                          goto join_147;
                        } else {
                          goto join_167;
                        }
                      } else if (_M0L10next__charS168 > 57) {
                        if (_M0L10next__charS168 < 59) {
                          _M0L12dispatch__15S146 = 3;
                          goto loop__label__15_149;
                        } else {
                          goto join_167;
                        }
                      } else {
                        _M0L12dispatch__15S146 = 6;
                        goto loop__label__15_149;
                      }
                      join_167:;
                      _M0L12dispatch__15S146 = 0;
                      goto loop__label__15_149;
                    } else {
                      goto join_138;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1436;
                    _M0Lm9tag__1__1S132 = _M0Lm6tag__1S131;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1436 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1436 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1438 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS170;
                      int32_t _M0L6_2atmpS1437;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS170
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1438);
                      _M0L6_2atmpS1437 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1437 + 1;
                      if (_M0L10next__charS170 < 58) {
                        if (_M0L10next__charS170 < 48) {
                          goto join_169;
                        } else {
                          _M0L12dispatch__15S146 = 2;
                          goto loop__label__15_149;
                        }
                      } else if (_M0L10next__charS170 > 58) {
                        goto join_169;
                      } else {
                        _M0L12dispatch__15S146 = 1;
                        goto loop__label__15_149;
                      }
                      join_169:;
                      _M0L12dispatch__15S146 = 0;
                      goto loop__label__15_149;
                    } else {
                      goto join_138;
                    }
                    break;
                  }
                  default: {
                    goto join_138;
                    break;
                  }
                }
                join_151:;
                _M0Lm6tag__1S131 = _M0Lm9tag__1__2S133;
                _M0Lm6tag__2S135 = _M0Lm9tag__2__1S136;
                _M0Lm20match__tag__saver__0S125 = _M0Lm6tag__0S130;
                _M0Lm20match__tag__saver__1S126 = _M0Lm6tag__1S131;
                _M0Lm20match__tag__saver__2S127 = _M0Lm6tag__2S135;
                _M0Lm20match__tag__saver__3S128 = _M0Lm6tag__3S134;
                _M0Lm20match__tag__saver__4S129 = _M0Lm6tag__4S137;
                _M0Lm13accept__stateS123 = 0;
                _M0Lm10match__endS124 = _M0Lm9_2acursorS122;
                goto join_138;
                join_147:;
                _M0Lm9tag__1__1S132 = _M0Lm9tag__1__2S133;
                _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                _M0Lm6tag__2S135 = _M0Lm9tag__2__1S136;
                _M0L6_2atmpS1406 = _M0Lm9_2acursorS122;
                if (_M0L6_2atmpS1406 < _M0L6_2aendS121) {
                  int32_t _M0L6_2atmpS1408 = _M0Lm9_2acursorS122;
                  int32_t _M0L10next__charS150;
                  int32_t _M0L6_2atmpS1407;
                  moonbit_incref(_M0L7_2adataS119);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS150
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1408);
                  _M0L6_2atmpS1407 = _M0Lm9_2acursorS122;
                  _M0Lm9_2acursorS122 = _M0L6_2atmpS1407 + 1;
                  if (_M0L10next__charS150 < 58) {
                    if (_M0L10next__charS150 < 48) {
                      goto join_148;
                    } else {
                      _M0L12dispatch__15S146 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS150 > 58) {
                    goto join_148;
                  } else {
                    _M0L12dispatch__15S146 = 1;
                    continue;
                  }
                  join_148:;
                  _M0L12dispatch__15S146 = 0;
                  continue;
                } else {
                  goto join_138;
                }
                break;
              }
            } else {
              goto join_138;
            }
          } else {
            continue;
          }
        } else {
          goto join_138;
        }
        break;
      }
    } else {
      goto join_138;
    }
  } else {
    goto join_138;
  }
  join_138:;
  switch (_M0Lm13accept__stateS123) {
    case 0: {
      int32_t _M0L6_2atmpS1397 = _M0Lm20match__tag__saver__1S126;
      int32_t _M0L6_2atmpS1396 = _M0L6_2atmpS1397 + 1;
      int64_t _M0L6_2atmpS1393 = (int64_t)_M0L6_2atmpS1396;
      int32_t _M0L6_2atmpS1395 = _M0Lm20match__tag__saver__2S127;
      int64_t _M0L6_2atmpS1394 = (int64_t)_M0L6_2atmpS1395;
      struct _M0TPC16string10StringView _M0L11start__lineS139;
      int32_t _M0L6_2atmpS1392;
      int32_t _M0L6_2atmpS1391;
      int64_t _M0L6_2atmpS1388;
      int32_t _M0L6_2atmpS1390;
      int64_t _M0L6_2atmpS1389;
      struct _M0TPC16string10StringView _M0L13start__columnS140;
      int32_t _M0L6_2atmpS1387;
      int64_t _M0L6_2atmpS1384;
      int32_t _M0L6_2atmpS1386;
      int64_t _M0L6_2atmpS1385;
      struct _M0TPC16string10StringView _M0L3pkgS141;
      int32_t _M0L6_2atmpS1383;
      int32_t _M0L6_2atmpS1382;
      int64_t _M0L6_2atmpS1379;
      int32_t _M0L6_2atmpS1381;
      int64_t _M0L6_2atmpS1380;
      struct _M0TPC16string10StringView _M0L8filenameS142;
      int32_t _M0L6_2atmpS1378;
      int32_t _M0L6_2atmpS1377;
      int64_t _M0L6_2atmpS1374;
      int32_t _M0L6_2atmpS1376;
      int64_t _M0L6_2atmpS1375;
      struct _M0TPC16string10StringView _M0L9end__lineS143;
      int32_t _M0L6_2atmpS1373;
      int32_t _M0L6_2atmpS1372;
      int64_t _M0L6_2atmpS1369;
      int32_t _M0L6_2atmpS1371;
      int64_t _M0L6_2atmpS1370;
      struct _M0TPC16string10StringView _M0L11end__columnS144;
      struct _M0TPB13SourceLocRepr* _block_3363;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS139
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1393, _M0L6_2atmpS1394);
      _M0L6_2atmpS1392 = _M0Lm20match__tag__saver__2S127;
      _M0L6_2atmpS1391 = _M0L6_2atmpS1392 + 1;
      _M0L6_2atmpS1388 = (int64_t)_M0L6_2atmpS1391;
      _M0L6_2atmpS1390 = _M0Lm20match__tag__saver__3S128;
      _M0L6_2atmpS1389 = (int64_t)_M0L6_2atmpS1390;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS140
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1388, _M0L6_2atmpS1389);
      _M0L6_2atmpS1387 = _M0L8_2astartS120 + 1;
      _M0L6_2atmpS1384 = (int64_t)_M0L6_2atmpS1387;
      _M0L6_2atmpS1386 = _M0Lm20match__tag__saver__0S125;
      _M0L6_2atmpS1385 = (int64_t)_M0L6_2atmpS1386;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS141
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1384, _M0L6_2atmpS1385);
      _M0L6_2atmpS1383 = _M0Lm20match__tag__saver__0S125;
      _M0L6_2atmpS1382 = _M0L6_2atmpS1383 + 1;
      _M0L6_2atmpS1379 = (int64_t)_M0L6_2atmpS1382;
      _M0L6_2atmpS1381 = _M0Lm20match__tag__saver__1S126;
      _M0L6_2atmpS1380 = (int64_t)_M0L6_2atmpS1381;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS142
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1379, _M0L6_2atmpS1380);
      _M0L6_2atmpS1378 = _M0Lm20match__tag__saver__3S128;
      _M0L6_2atmpS1377 = _M0L6_2atmpS1378 + 1;
      _M0L6_2atmpS1374 = (int64_t)_M0L6_2atmpS1377;
      _M0L6_2atmpS1376 = _M0Lm20match__tag__saver__4S129;
      _M0L6_2atmpS1375 = (int64_t)_M0L6_2atmpS1376;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS143
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1374, _M0L6_2atmpS1375);
      _M0L6_2atmpS1373 = _M0Lm20match__tag__saver__4S129;
      _M0L6_2atmpS1372 = _M0L6_2atmpS1373 + 1;
      _M0L6_2atmpS1369 = (int64_t)_M0L6_2atmpS1372;
      _M0L6_2atmpS1371 = _M0Lm10match__endS124;
      _M0L6_2atmpS1370 = (int64_t)_M0L6_2atmpS1371;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS144
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1369, _M0L6_2atmpS1370);
      _block_3363
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3363)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3363->$0_0 = _M0L3pkgS141.$0;
      _block_3363->$0_1 = _M0L3pkgS141.$1;
      _block_3363->$0_2 = _M0L3pkgS141.$2;
      _block_3363->$1_0 = _M0L8filenameS142.$0;
      _block_3363->$1_1 = _M0L8filenameS142.$1;
      _block_3363->$1_2 = _M0L8filenameS142.$2;
      _block_3363->$2_0 = _M0L11start__lineS139.$0;
      _block_3363->$2_1 = _M0L11start__lineS139.$1;
      _block_3363->$2_2 = _M0L11start__lineS139.$2;
      _block_3363->$3_0 = _M0L13start__columnS140.$0;
      _block_3363->$3_1 = _M0L13start__columnS140.$1;
      _block_3363->$3_2 = _M0L13start__columnS140.$2;
      _block_3363->$4_0 = _M0L9end__lineS143.$0;
      _block_3363->$4_1 = _M0L9end__lineS143.$1;
      _block_3363->$4_2 = _M0L9end__lineS143.$2;
      _block_3363->$5_0 = _M0L11end__columnS144.$0;
      _block_3363->$5_1 = _M0L11end__columnS144.$1;
      _block_3363->$5_2 = _M0L11end__columnS144.$2;
      return _block_3363;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS119);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS115,
  int32_t _M0L5indexS116
) {
  int32_t _M0L3lenS114;
  int32_t _if__result_3364;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS114 = _M0L4selfS115->$1;
  if (_M0L5indexS116 >= 0) {
    _if__result_3364 = _M0L5indexS116 < _M0L3lenS114;
  } else {
    _if__result_3364 = 0;
  }
  if (_if__result_3364) {
    moonbit_string_t* _M0L6_2atmpS1368;
    moonbit_string_t _M0L6_2atmpS3055;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1368 = _M0MPC15array5Array6bufferGsE(_M0L4selfS115);
    if (
      _M0L5indexS116 < 0
      || _M0L5indexS116 >= Moonbit_array_length(_M0L6_2atmpS1368)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3055 = (moonbit_string_t)_M0L6_2atmpS1368[_M0L5indexS116];
    moonbit_incref(_M0L6_2atmpS3055);
    moonbit_decref(_M0L6_2atmpS1368);
    return _M0L6_2atmpS3055;
  } else {
    moonbit_decref(_M0L4selfS115);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS111
) {
  moonbit_string_t* _M0L8_2afieldS3056;
  int32_t _M0L6_2acntS3187;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3056 = _M0L4selfS111->$0;
  _M0L6_2acntS3187 = Moonbit_object_header(_M0L4selfS111)->rc;
  if (_M0L6_2acntS3187 > 1) {
    int32_t _M0L11_2anew__cntS3188 = _M0L6_2acntS3187 - 1;
    Moonbit_object_header(_M0L4selfS111)->rc = _M0L11_2anew__cntS3188;
    moonbit_incref(_M0L8_2afieldS3056);
  } else if (_M0L6_2acntS3187 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS111);
  }
  return _M0L8_2afieldS3056;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS112
) {
  struct _M0TUsiE** _M0L8_2afieldS3057;
  int32_t _M0L6_2acntS3189;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3057 = _M0L4selfS112->$0;
  _M0L6_2acntS3189 = Moonbit_object_header(_M0L4selfS112)->rc;
  if (_M0L6_2acntS3189 > 1) {
    int32_t _M0L11_2anew__cntS3190 = _M0L6_2acntS3189 - 1;
    Moonbit_object_header(_M0L4selfS112)->rc = _M0L11_2anew__cntS3190;
    moonbit_incref(_M0L8_2afieldS3057);
  } else if (_M0L6_2acntS3189 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS112);
  }
  return _M0L8_2afieldS3057;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS113
) {
  void** _M0L8_2afieldS3058;
  int32_t _M0L6_2acntS3191;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3058 = _M0L4selfS113->$0;
  _M0L6_2acntS3191 = Moonbit_object_header(_M0L4selfS113)->rc;
  if (_M0L6_2acntS3191 > 1) {
    int32_t _M0L11_2anew__cntS3192 = _M0L6_2acntS3191 - 1;
    Moonbit_object_header(_M0L4selfS113)->rc = _M0L11_2anew__cntS3192;
    moonbit_incref(_M0L8_2afieldS3058);
  } else if (_M0L6_2acntS3191 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS113);
  }
  return _M0L8_2afieldS3058;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS110) {
  struct _M0TPB13StringBuilder* _M0L3bufS109;
  struct _M0TPB6Logger _M0L6_2atmpS1367;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS109 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS109);
  _M0L6_2atmpS1367
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS109
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS110, _M0L6_2atmpS1367);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS109);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS108) {
  int32_t _M0L6_2atmpS1366;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1366 = (int32_t)_M0L4selfS108;
  return _M0L6_2atmpS1366;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS106,
  int32_t _M0L8trailingS107
) {
  int32_t _M0L6_2atmpS1365;
  int32_t _M0L6_2atmpS1364;
  int32_t _M0L6_2atmpS1363;
  int32_t _M0L6_2atmpS1362;
  int32_t _M0L6_2atmpS1361;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1365 = _M0L7leadingS106 - 55296;
  _M0L6_2atmpS1364 = _M0L6_2atmpS1365 * 1024;
  _M0L6_2atmpS1363 = _M0L6_2atmpS1364 + _M0L8trailingS107;
  _M0L6_2atmpS1362 = _M0L6_2atmpS1363 - 56320;
  _M0L6_2atmpS1361 = _M0L6_2atmpS1362 + 65536;
  return _M0L6_2atmpS1361;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS105) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS105 >= 56320) {
    return _M0L4selfS105 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS104) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS104 >= 55296) {
    return _M0L4selfS104 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS101,
  int32_t _M0L2chS103
) {
  int32_t _M0L3lenS1356;
  int32_t _M0L6_2atmpS1355;
  moonbit_bytes_t _M0L8_2afieldS3059;
  moonbit_bytes_t _M0L4dataS1359;
  int32_t _M0L3lenS1360;
  int32_t _M0L3incS102;
  int32_t _M0L3lenS1358;
  int32_t _M0L6_2atmpS1357;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1356 = _M0L4selfS101->$1;
  _M0L6_2atmpS1355 = _M0L3lenS1356 + 4;
  moonbit_incref(_M0L4selfS101);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS101, _M0L6_2atmpS1355);
  _M0L8_2afieldS3059 = _M0L4selfS101->$0;
  _M0L4dataS1359 = _M0L8_2afieldS3059;
  _M0L3lenS1360 = _M0L4selfS101->$1;
  moonbit_incref(_M0L4dataS1359);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS102
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1359, _M0L3lenS1360, _M0L2chS103);
  _M0L3lenS1358 = _M0L4selfS101->$1;
  _M0L6_2atmpS1357 = _M0L3lenS1358 + _M0L3incS102;
  _M0L4selfS101->$1 = _M0L6_2atmpS1357;
  moonbit_decref(_M0L4selfS101);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS96,
  int32_t _M0L8requiredS97
) {
  moonbit_bytes_t _M0L8_2afieldS3063;
  moonbit_bytes_t _M0L4dataS1354;
  int32_t _M0L6_2atmpS3062;
  int32_t _M0L12current__lenS95;
  int32_t _M0Lm13enough__spaceS98;
  int32_t _M0L6_2atmpS1352;
  int32_t _M0L6_2atmpS1353;
  moonbit_bytes_t _M0L9new__dataS100;
  moonbit_bytes_t _M0L8_2afieldS3061;
  moonbit_bytes_t _M0L4dataS1350;
  int32_t _M0L3lenS1351;
  moonbit_bytes_t _M0L6_2aoldS3060;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3063 = _M0L4selfS96->$0;
  _M0L4dataS1354 = _M0L8_2afieldS3063;
  _M0L6_2atmpS3062 = Moonbit_array_length(_M0L4dataS1354);
  _M0L12current__lenS95 = _M0L6_2atmpS3062;
  if (_M0L8requiredS97 <= _M0L12current__lenS95) {
    moonbit_decref(_M0L4selfS96);
    return 0;
  }
  _M0Lm13enough__spaceS98 = _M0L12current__lenS95;
  while (1) {
    int32_t _M0L6_2atmpS1348 = _M0Lm13enough__spaceS98;
    if (_M0L6_2atmpS1348 < _M0L8requiredS97) {
      int32_t _M0L6_2atmpS1349 = _M0Lm13enough__spaceS98;
      _M0Lm13enough__spaceS98 = _M0L6_2atmpS1349 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1352 = _M0Lm13enough__spaceS98;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1353 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS100
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1352, _M0L6_2atmpS1353);
  _M0L8_2afieldS3061 = _M0L4selfS96->$0;
  _M0L4dataS1350 = _M0L8_2afieldS3061;
  _M0L3lenS1351 = _M0L4selfS96->$1;
  moonbit_incref(_M0L4dataS1350);
  moonbit_incref(_M0L9new__dataS100);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS100, 0, _M0L4dataS1350, 0, _M0L3lenS1351);
  _M0L6_2aoldS3060 = _M0L4selfS96->$0;
  moonbit_decref(_M0L6_2aoldS3060);
  _M0L4selfS96->$0 = _M0L9new__dataS100;
  moonbit_decref(_M0L4selfS96);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS90,
  int32_t _M0L6offsetS91,
  int32_t _M0L5valueS89
) {
  uint32_t _M0L4codeS88;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS88 = _M0MPC14char4Char8to__uint(_M0L5valueS89);
  if (_M0L4codeS88 < 65536u) {
    uint32_t _M0L6_2atmpS1331 = _M0L4codeS88 & 255u;
    int32_t _M0L6_2atmpS1330;
    int32_t _M0L6_2atmpS1332;
    uint32_t _M0L6_2atmpS1334;
    int32_t _M0L6_2atmpS1333;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1330 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1331);
    if (
      _M0L6offsetS91 < 0
      || _M0L6offsetS91 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6offsetS91] = _M0L6_2atmpS1330;
    _M0L6_2atmpS1332 = _M0L6offsetS91 + 1;
    _M0L6_2atmpS1334 = _M0L4codeS88 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1333 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1334);
    if (
      _M0L6_2atmpS1332 < 0
      || _M0L6_2atmpS1332 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1332] = _M0L6_2atmpS1333;
    moonbit_decref(_M0L4selfS90);
    return 2;
  } else if (_M0L4codeS88 < 1114112u) {
    uint32_t _M0L2hiS92 = _M0L4codeS88 - 65536u;
    uint32_t _M0L6_2atmpS1347 = _M0L2hiS92 >> 10;
    uint32_t _M0L2loS93 = _M0L6_2atmpS1347 | 55296u;
    uint32_t _M0L6_2atmpS1346 = _M0L2hiS92 & 1023u;
    uint32_t _M0L2hiS94 = _M0L6_2atmpS1346 | 56320u;
    uint32_t _M0L6_2atmpS1336 = _M0L2loS93 & 255u;
    int32_t _M0L6_2atmpS1335;
    int32_t _M0L6_2atmpS1337;
    uint32_t _M0L6_2atmpS1339;
    int32_t _M0L6_2atmpS1338;
    int32_t _M0L6_2atmpS1340;
    uint32_t _M0L6_2atmpS1342;
    int32_t _M0L6_2atmpS1341;
    int32_t _M0L6_2atmpS1343;
    uint32_t _M0L6_2atmpS1345;
    int32_t _M0L6_2atmpS1344;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1335 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1336);
    if (
      _M0L6offsetS91 < 0
      || _M0L6offsetS91 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6offsetS91] = _M0L6_2atmpS1335;
    _M0L6_2atmpS1337 = _M0L6offsetS91 + 1;
    _M0L6_2atmpS1339 = _M0L2loS93 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1338 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1339);
    if (
      _M0L6_2atmpS1337 < 0
      || _M0L6_2atmpS1337 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1337] = _M0L6_2atmpS1338;
    _M0L6_2atmpS1340 = _M0L6offsetS91 + 2;
    _M0L6_2atmpS1342 = _M0L2hiS94 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1341 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1342);
    if (
      _M0L6_2atmpS1340 < 0
      || _M0L6_2atmpS1340 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1340] = _M0L6_2atmpS1341;
    _M0L6_2atmpS1343 = _M0L6offsetS91 + 3;
    _M0L6_2atmpS1345 = _M0L2hiS94 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1344 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1345);
    if (
      _M0L6_2atmpS1343 < 0
      || _M0L6_2atmpS1343 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1343] = _M0L6_2atmpS1344;
    moonbit_decref(_M0L4selfS90);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS90);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_116.data, (moonbit_string_t)moonbit_string_literal_117.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS87) {
  int32_t _M0L6_2atmpS1329;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1329 = *(int32_t*)&_M0L4selfS87;
  return _M0L6_2atmpS1329 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS86) {
  int32_t _M0L6_2atmpS1328;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1328 = _M0L4selfS86;
  return *(uint32_t*)&_M0L6_2atmpS1328;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS85
) {
  moonbit_bytes_t _M0L8_2afieldS3065;
  moonbit_bytes_t _M0L4dataS1327;
  moonbit_bytes_t _M0L6_2atmpS1324;
  int32_t _M0L8_2afieldS3064;
  int32_t _M0L3lenS1326;
  int64_t _M0L6_2atmpS1325;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3065 = _M0L4selfS85->$0;
  _M0L4dataS1327 = _M0L8_2afieldS3065;
  moonbit_incref(_M0L4dataS1327);
  _M0L6_2atmpS1324 = _M0L4dataS1327;
  _M0L8_2afieldS3064 = _M0L4selfS85->$1;
  moonbit_decref(_M0L4selfS85);
  _M0L3lenS1326 = _M0L8_2afieldS3064;
  _M0L6_2atmpS1325 = (int64_t)_M0L3lenS1326;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1324, 0, _M0L6_2atmpS1325);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS80,
  int32_t _M0L6offsetS84,
  int64_t _M0L6lengthS82
) {
  int32_t _M0L3lenS79;
  int32_t _M0L6lengthS81;
  int32_t _if__result_3366;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS79 = Moonbit_array_length(_M0L4selfS80);
  if (_M0L6lengthS82 == 4294967296ll) {
    _M0L6lengthS81 = _M0L3lenS79 - _M0L6offsetS84;
  } else {
    int64_t _M0L7_2aSomeS83 = _M0L6lengthS82;
    _M0L6lengthS81 = (int32_t)_M0L7_2aSomeS83;
  }
  if (_M0L6offsetS84 >= 0) {
    if (_M0L6lengthS81 >= 0) {
      int32_t _M0L6_2atmpS1323 = _M0L6offsetS84 + _M0L6lengthS81;
      _if__result_3366 = _M0L6_2atmpS1323 <= _M0L3lenS79;
    } else {
      _if__result_3366 = 0;
    }
  } else {
    _if__result_3366 = 0;
  }
  if (_if__result_3366) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS80, _M0L6offsetS84, _M0L6lengthS81);
  } else {
    moonbit_decref(_M0L4selfS80);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS77
) {
  int32_t _M0L7initialS76;
  moonbit_bytes_t _M0L4dataS78;
  struct _M0TPB13StringBuilder* _block_3367;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS77 < 1) {
    _M0L7initialS76 = 1;
  } else {
    _M0L7initialS76 = _M0L10size__hintS77;
  }
  _M0L4dataS78 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS76, 0);
  _block_3367
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3367)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3367->$0 = _M0L4dataS78;
  _block_3367->$1 = 0;
  return _block_3367;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS75) {
  int32_t _M0L6_2atmpS1322;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1322 = (int32_t)_M0L4selfS75;
  return _M0L6_2atmpS1322;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS60,
  int32_t _M0L11dst__offsetS61,
  moonbit_string_t* _M0L3srcS62,
  int32_t _M0L11src__offsetS63,
  int32_t _M0L3lenS64
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS60, _M0L11dst__offsetS61, _M0L3srcS62, _M0L11src__offsetS63, _M0L3lenS64);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS65,
  int32_t _M0L11dst__offsetS66,
  struct _M0TUsiE** _M0L3srcS67,
  int32_t _M0L11src__offsetS68,
  int32_t _M0L3lenS69
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS65, _M0L11dst__offsetS66, _M0L3srcS67, _M0L11src__offsetS68, _M0L3lenS69);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS70,
  int32_t _M0L11dst__offsetS71,
  void** _M0L3srcS72,
  int32_t _M0L11src__offsetS73,
  int32_t _M0L3lenS74
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS70, _M0L11dst__offsetS71, _M0L3srcS72, _M0L11src__offsetS73, _M0L3lenS74);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS24,
  int32_t _M0L11dst__offsetS26,
  moonbit_bytes_t _M0L3srcS25,
  int32_t _M0L11src__offsetS27,
  int32_t _M0L3lenS29
) {
  int32_t _if__result_3368;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS24 == _M0L3srcS25) {
    _if__result_3368 = _M0L11dst__offsetS26 < _M0L11src__offsetS27;
  } else {
    _if__result_3368 = 0;
  }
  if (_if__result_3368) {
    int32_t _M0L1iS28 = 0;
    while (1) {
      if (_M0L1iS28 < _M0L3lenS29) {
        int32_t _M0L6_2atmpS1286 = _M0L11dst__offsetS26 + _M0L1iS28;
        int32_t _M0L6_2atmpS1288 = _M0L11src__offsetS27 + _M0L1iS28;
        int32_t _M0L6_2atmpS1287;
        int32_t _M0L6_2atmpS1289;
        if (
          _M0L6_2atmpS1288 < 0
          || _M0L6_2atmpS1288 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1287 = (int32_t)_M0L3srcS25[_M0L6_2atmpS1288];
        if (
          _M0L6_2atmpS1286 < 0
          || _M0L6_2atmpS1286 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS24[_M0L6_2atmpS1286] = _M0L6_2atmpS1287;
        _M0L6_2atmpS1289 = _M0L1iS28 + 1;
        _M0L1iS28 = _M0L6_2atmpS1289;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1294 = _M0L3lenS29 - 1;
    int32_t _M0L1iS31 = _M0L6_2atmpS1294;
    while (1) {
      if (_M0L1iS31 >= 0) {
        int32_t _M0L6_2atmpS1290 = _M0L11dst__offsetS26 + _M0L1iS31;
        int32_t _M0L6_2atmpS1292 = _M0L11src__offsetS27 + _M0L1iS31;
        int32_t _M0L6_2atmpS1291;
        int32_t _M0L6_2atmpS1293;
        if (
          _M0L6_2atmpS1292 < 0
          || _M0L6_2atmpS1292 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1291 = (int32_t)_M0L3srcS25[_M0L6_2atmpS1292];
        if (
          _M0L6_2atmpS1290 < 0
          || _M0L6_2atmpS1290 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS24[_M0L6_2atmpS1290] = _M0L6_2atmpS1291;
        _M0L6_2atmpS1293 = _M0L1iS31 - 1;
        _M0L1iS31 = _M0L6_2atmpS1293;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS33,
  int32_t _M0L11dst__offsetS35,
  moonbit_string_t* _M0L3srcS34,
  int32_t _M0L11src__offsetS36,
  int32_t _M0L3lenS38
) {
  int32_t _if__result_3371;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS33 == _M0L3srcS34) {
    _if__result_3371 = _M0L11dst__offsetS35 < _M0L11src__offsetS36;
  } else {
    _if__result_3371 = 0;
  }
  if (_if__result_3371) {
    int32_t _M0L1iS37 = 0;
    while (1) {
      if (_M0L1iS37 < _M0L3lenS38) {
        int32_t _M0L6_2atmpS1295 = _M0L11dst__offsetS35 + _M0L1iS37;
        int32_t _M0L6_2atmpS1297 = _M0L11src__offsetS36 + _M0L1iS37;
        moonbit_string_t _M0L6_2atmpS3067;
        moonbit_string_t _M0L6_2atmpS1296;
        moonbit_string_t _M0L6_2aoldS3066;
        int32_t _M0L6_2atmpS1298;
        if (
          _M0L6_2atmpS1297 < 0
          || _M0L6_2atmpS1297 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3067 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS1297];
        _M0L6_2atmpS1296 = _M0L6_2atmpS3067;
        if (
          _M0L6_2atmpS1295 < 0
          || _M0L6_2atmpS1295 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3066 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS1295];
        moonbit_incref(_M0L6_2atmpS1296);
        moonbit_decref(_M0L6_2aoldS3066);
        _M0L3dstS33[_M0L6_2atmpS1295] = _M0L6_2atmpS1296;
        _M0L6_2atmpS1298 = _M0L1iS37 + 1;
        _M0L1iS37 = _M0L6_2atmpS1298;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1303 = _M0L3lenS38 - 1;
    int32_t _M0L1iS40 = _M0L6_2atmpS1303;
    while (1) {
      if (_M0L1iS40 >= 0) {
        int32_t _M0L6_2atmpS1299 = _M0L11dst__offsetS35 + _M0L1iS40;
        int32_t _M0L6_2atmpS1301 = _M0L11src__offsetS36 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS3069;
        moonbit_string_t _M0L6_2atmpS1300;
        moonbit_string_t _M0L6_2aoldS3068;
        int32_t _M0L6_2atmpS1302;
        if (
          _M0L6_2atmpS1301 < 0
          || _M0L6_2atmpS1301 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3069 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS1301];
        _M0L6_2atmpS1300 = _M0L6_2atmpS3069;
        if (
          _M0L6_2atmpS1299 < 0
          || _M0L6_2atmpS1299 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3068 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS1299];
        moonbit_incref(_M0L6_2atmpS1300);
        moonbit_decref(_M0L6_2aoldS3068);
        _M0L3dstS33[_M0L6_2atmpS1299] = _M0L6_2atmpS1300;
        _M0L6_2atmpS1302 = _M0L1iS40 - 1;
        _M0L1iS40 = _M0L6_2atmpS1302;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS42,
  int32_t _M0L11dst__offsetS44,
  struct _M0TUsiE** _M0L3srcS43,
  int32_t _M0L11src__offsetS45,
  int32_t _M0L3lenS47
) {
  int32_t _if__result_3374;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS42 == _M0L3srcS43) {
    _if__result_3374 = _M0L11dst__offsetS44 < _M0L11src__offsetS45;
  } else {
    _if__result_3374 = 0;
  }
  if (_if__result_3374) {
    int32_t _M0L1iS46 = 0;
    while (1) {
      if (_M0L1iS46 < _M0L3lenS47) {
        int32_t _M0L6_2atmpS1304 = _M0L11dst__offsetS44 + _M0L1iS46;
        int32_t _M0L6_2atmpS1306 = _M0L11src__offsetS45 + _M0L1iS46;
        struct _M0TUsiE* _M0L6_2atmpS3071;
        struct _M0TUsiE* _M0L6_2atmpS1305;
        struct _M0TUsiE* _M0L6_2aoldS3070;
        int32_t _M0L6_2atmpS1307;
        if (
          _M0L6_2atmpS1306 < 0
          || _M0L6_2atmpS1306 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3071 = (struct _M0TUsiE*)_M0L3srcS43[_M0L6_2atmpS1306];
        _M0L6_2atmpS1305 = _M0L6_2atmpS3071;
        if (
          _M0L6_2atmpS1304 < 0
          || _M0L6_2atmpS1304 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3070 = (struct _M0TUsiE*)_M0L3dstS42[_M0L6_2atmpS1304];
        if (_M0L6_2atmpS1305) {
          moonbit_incref(_M0L6_2atmpS1305);
        }
        if (_M0L6_2aoldS3070) {
          moonbit_decref(_M0L6_2aoldS3070);
        }
        _M0L3dstS42[_M0L6_2atmpS1304] = _M0L6_2atmpS1305;
        _M0L6_2atmpS1307 = _M0L1iS46 + 1;
        _M0L1iS46 = _M0L6_2atmpS1307;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1312 = _M0L3lenS47 - 1;
    int32_t _M0L1iS49 = _M0L6_2atmpS1312;
    while (1) {
      if (_M0L1iS49 >= 0) {
        int32_t _M0L6_2atmpS1308 = _M0L11dst__offsetS44 + _M0L1iS49;
        int32_t _M0L6_2atmpS1310 = _M0L11src__offsetS45 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS3073;
        struct _M0TUsiE* _M0L6_2atmpS1309;
        struct _M0TUsiE* _M0L6_2aoldS3072;
        int32_t _M0L6_2atmpS1311;
        if (
          _M0L6_2atmpS1310 < 0
          || _M0L6_2atmpS1310 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3073 = (struct _M0TUsiE*)_M0L3srcS43[_M0L6_2atmpS1310];
        _M0L6_2atmpS1309 = _M0L6_2atmpS3073;
        if (
          _M0L6_2atmpS1308 < 0
          || _M0L6_2atmpS1308 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3072 = (struct _M0TUsiE*)_M0L3dstS42[_M0L6_2atmpS1308];
        if (_M0L6_2atmpS1309) {
          moonbit_incref(_M0L6_2atmpS1309);
        }
        if (_M0L6_2aoldS3072) {
          moonbit_decref(_M0L6_2aoldS3072);
        }
        _M0L3dstS42[_M0L6_2atmpS1308] = _M0L6_2atmpS1309;
        _M0L6_2atmpS1311 = _M0L1iS49 - 1;
        _M0L1iS49 = _M0L6_2atmpS1311;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS51,
  int32_t _M0L11dst__offsetS53,
  void** _M0L3srcS52,
  int32_t _M0L11src__offsetS54,
  int32_t _M0L3lenS56
) {
  int32_t _if__result_3377;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS51 == _M0L3srcS52) {
    _if__result_3377 = _M0L11dst__offsetS53 < _M0L11src__offsetS54;
  } else {
    _if__result_3377 = 0;
  }
  if (_if__result_3377) {
    int32_t _M0L1iS55 = 0;
    while (1) {
      if (_M0L1iS55 < _M0L3lenS56) {
        int32_t _M0L6_2atmpS1313 = _M0L11dst__offsetS53 + _M0L1iS55;
        int32_t _M0L6_2atmpS1315 = _M0L11src__offsetS54 + _M0L1iS55;
        void* _M0L6_2atmpS3075;
        void* _M0L6_2atmpS1314;
        void* _M0L6_2aoldS3074;
        int32_t _M0L6_2atmpS1316;
        if (
          _M0L6_2atmpS1315 < 0
          || _M0L6_2atmpS1315 >= Moonbit_array_length(_M0L3srcS52)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3075 = (void*)_M0L3srcS52[_M0L6_2atmpS1315];
        _M0L6_2atmpS1314 = _M0L6_2atmpS3075;
        if (
          _M0L6_2atmpS1313 < 0
          || _M0L6_2atmpS1313 >= Moonbit_array_length(_M0L3dstS51)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3074 = (void*)_M0L3dstS51[_M0L6_2atmpS1313];
        moonbit_incref(_M0L6_2atmpS1314);
        moonbit_decref(_M0L6_2aoldS3074);
        _M0L3dstS51[_M0L6_2atmpS1313] = _M0L6_2atmpS1314;
        _M0L6_2atmpS1316 = _M0L1iS55 + 1;
        _M0L1iS55 = _M0L6_2atmpS1316;
        continue;
      } else {
        moonbit_decref(_M0L3srcS52);
        moonbit_decref(_M0L3dstS51);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1321 = _M0L3lenS56 - 1;
    int32_t _M0L1iS58 = _M0L6_2atmpS1321;
    while (1) {
      if (_M0L1iS58 >= 0) {
        int32_t _M0L6_2atmpS1317 = _M0L11dst__offsetS53 + _M0L1iS58;
        int32_t _M0L6_2atmpS1319 = _M0L11src__offsetS54 + _M0L1iS58;
        void* _M0L6_2atmpS3077;
        void* _M0L6_2atmpS1318;
        void* _M0L6_2aoldS3076;
        int32_t _M0L6_2atmpS1320;
        if (
          _M0L6_2atmpS1319 < 0
          || _M0L6_2atmpS1319 >= Moonbit_array_length(_M0L3srcS52)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3077 = (void*)_M0L3srcS52[_M0L6_2atmpS1319];
        _M0L6_2atmpS1318 = _M0L6_2atmpS3077;
        if (
          _M0L6_2atmpS1317 < 0
          || _M0L6_2atmpS1317 >= Moonbit_array_length(_M0L3dstS51)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3076 = (void*)_M0L3dstS51[_M0L6_2atmpS1317];
        moonbit_incref(_M0L6_2atmpS1318);
        moonbit_decref(_M0L6_2aoldS3076);
        _M0L3dstS51[_M0L6_2atmpS1317] = _M0L6_2atmpS1318;
        _M0L6_2atmpS1320 = _M0L1iS58 - 1;
        _M0L1iS58 = _M0L6_2atmpS1320;
        continue;
      } else {
        moonbit_decref(_M0L3srcS52);
        moonbit_decref(_M0L3dstS51);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS18,
  moonbit_string_t _M0L3locS19
) {
  moonbit_string_t _M0L6_2atmpS1275;
  moonbit_string_t _M0L6_2atmpS3080;
  moonbit_string_t _M0L6_2atmpS1273;
  moonbit_string_t _M0L6_2atmpS1274;
  moonbit_string_t _M0L6_2atmpS3079;
  moonbit_string_t _M0L6_2atmpS1272;
  moonbit_string_t _M0L6_2atmpS3078;
  moonbit_string_t _M0L6_2atmpS1271;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1275 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3080
  = moonbit_add_string(_M0L6_2atmpS1275, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1275);
  _M0L6_2atmpS1273 = _M0L6_2atmpS3080;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1274
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3079 = moonbit_add_string(_M0L6_2atmpS1273, _M0L6_2atmpS1274);
  moonbit_decref(_M0L6_2atmpS1273);
  moonbit_decref(_M0L6_2atmpS1274);
  _M0L6_2atmpS1272 = _M0L6_2atmpS3079;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3078
  = moonbit_add_string(_M0L6_2atmpS1272, (moonbit_string_t)moonbit_string_literal_73.data);
  moonbit_decref(_M0L6_2atmpS1272);
  _M0L6_2atmpS1271 = _M0L6_2atmpS3078;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1271);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1280;
  moonbit_string_t _M0L6_2atmpS3083;
  moonbit_string_t _M0L6_2atmpS1278;
  moonbit_string_t _M0L6_2atmpS1279;
  moonbit_string_t _M0L6_2atmpS3082;
  moonbit_string_t _M0L6_2atmpS1277;
  moonbit_string_t _M0L6_2atmpS3081;
  moonbit_string_t _M0L6_2atmpS1276;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1280 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3083
  = moonbit_add_string(_M0L6_2atmpS1280, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1280);
  _M0L6_2atmpS1278 = _M0L6_2atmpS3083;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1279
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3082 = moonbit_add_string(_M0L6_2atmpS1278, _M0L6_2atmpS1279);
  moonbit_decref(_M0L6_2atmpS1278);
  moonbit_decref(_M0L6_2atmpS1279);
  _M0L6_2atmpS1277 = _M0L6_2atmpS3082;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3081
  = moonbit_add_string(_M0L6_2atmpS1277, (moonbit_string_t)moonbit_string_literal_73.data);
  moonbit_decref(_M0L6_2atmpS1277);
  _M0L6_2atmpS1276 = _M0L6_2atmpS3081;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1276);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1285;
  moonbit_string_t _M0L6_2atmpS3086;
  moonbit_string_t _M0L6_2atmpS1283;
  moonbit_string_t _M0L6_2atmpS1284;
  moonbit_string_t _M0L6_2atmpS3085;
  moonbit_string_t _M0L6_2atmpS1282;
  moonbit_string_t _M0L6_2atmpS3084;
  moonbit_string_t _M0L6_2atmpS1281;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1285 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3086
  = moonbit_add_string(_M0L6_2atmpS1285, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1285);
  _M0L6_2atmpS1283 = _M0L6_2atmpS3086;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1284
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3085 = moonbit_add_string(_M0L6_2atmpS1283, _M0L6_2atmpS1284);
  moonbit_decref(_M0L6_2atmpS1283);
  moonbit_decref(_M0L6_2atmpS1284);
  _M0L6_2atmpS1282 = _M0L6_2atmpS3085;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3084
  = moonbit_add_string(_M0L6_2atmpS1282, (moonbit_string_t)moonbit_string_literal_73.data);
  moonbit_decref(_M0L6_2atmpS1282);
  _M0L6_2atmpS1281 = _M0L6_2atmpS3084;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1281);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5valueS17
) {
  uint32_t _M0L3accS1270;
  uint32_t _M0L6_2atmpS1269;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1270 = _M0L4selfS16->$0;
  _M0L6_2atmpS1269 = _M0L3accS1270 + 4u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1269;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS16, _M0L5valueS17);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5inputS15
) {
  uint32_t _M0L3accS1267;
  uint32_t _M0L6_2atmpS1268;
  uint32_t _M0L6_2atmpS1266;
  uint32_t _M0L6_2atmpS1265;
  uint32_t _M0L6_2atmpS1264;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1267 = _M0L4selfS14->$0;
  _M0L6_2atmpS1268 = _M0L5inputS15 * 3266489917u;
  _M0L6_2atmpS1266 = _M0L3accS1267 + _M0L6_2atmpS1268;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1265 = _M0FPB4rotl(_M0L6_2atmpS1266, 17);
  _M0L6_2atmpS1264 = _M0L6_2atmpS1265 * 668265263u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1264;
  moonbit_decref(_M0L4selfS14);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS12, int32_t _M0L1rS13) {
  uint32_t _M0L6_2atmpS1261;
  int32_t _M0L6_2atmpS1263;
  uint32_t _M0L6_2atmpS1262;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1261 = _M0L1xS12 << (_M0L1rS13 & 31);
  _M0L6_2atmpS1263 = 32 - _M0L1rS13;
  _M0L6_2atmpS1262 = _M0L1xS12 >> (_M0L6_2atmpS1263 & 31);
  return _M0L6_2atmpS1261 | _M0L6_2atmpS1262;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S8,
  struct _M0TPB6Logger _M0L10_2ax__4934S11
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS9;
  moonbit_string_t _M0L8_2afieldS3087;
  int32_t _M0L6_2acntS3193;
  moonbit_string_t _M0L15_2a_2aarg__4935S10;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS9
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S8;
  _M0L8_2afieldS3087 = _M0L10_2aFailureS9->$0;
  _M0L6_2acntS3193 = Moonbit_object_header(_M0L10_2aFailureS9)->rc;
  if (_M0L6_2acntS3193 > 1) {
    int32_t _M0L11_2anew__cntS3194 = _M0L6_2acntS3193 - 1;
    Moonbit_object_header(_M0L10_2aFailureS9)->rc = _M0L11_2anew__cntS3194;
    moonbit_incref(_M0L8_2afieldS3087);
  } else if (_M0L6_2acntS3193 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS9);
  }
  _M0L15_2a_2aarg__4935S10 = _M0L8_2afieldS3087;
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_119.data);
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S11, _M0L15_2a_2aarg__4935S10);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_120.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS7) {
  void* _block_3380;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3380 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3380)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3380)->$0 = _M0L4selfS7;
  return _block_3380;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS6) {
  void* _block_3381;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3381 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3381)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3381)->$0 = _M0L5arrayS6;
  return _block_3381;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS5,
  moonbit_string_t _M0L3objS4
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS4, _M0L4selfS5);
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

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1201) {
  switch (Moonbit_object_tag(_M0L4_2aeS1201)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1201);
      return (moonbit_string_t)moonbit_string_literal_121.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1201);
      return (moonbit_string_t)moonbit_string_literal_122.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1201);
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS1201);
      return (moonbit_string_t)moonbit_string_literal_123.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1201);
      return (moonbit_string_t)moonbit_string_literal_124.data;
      break;
    }
  }
}

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE(
  void* _M0L11_2aobj__ptrS1228
) {
  moonbit_string_t _M0L7_2aselfS1227 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1228;
  return _M0IPC16option6OptionPB6ToJson8to__jsonGsE(_M0L7_2aselfS1227);
}

void* _M0IPC16string6StringPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1226
) {
  moonbit_string_t _M0L7_2aselfS1225 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1226;
  return _M0IPC16string6StringPB6ToJson8to__json(_M0L7_2aselfS1225);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1224,
  int32_t _M0L8_2aparamS1223
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1222 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1224;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1222, _M0L8_2aparamS1223);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1221,
  struct _M0TPC16string10StringView _M0L8_2aparamS1220
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1219 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1221;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1219, _M0L8_2aparamS1220);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1218,
  moonbit_string_t _M0L8_2aparamS1215,
  int32_t _M0L8_2aparamS1216,
  int32_t _M0L8_2aparamS1217
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1214 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1218;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1214, _M0L8_2aparamS1215, _M0L8_2aparamS1216, _M0L8_2aparamS1217);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1213,
  moonbit_string_t _M0L8_2aparamS1212
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1211 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1213;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1211, _M0L8_2aparamS1212);
  return 0;
}

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGiE(
  void* _M0L11_2aobj__ptrS1209
) {
  struct _M0Y5Int64* _M0L14_2aboxed__selfS1210 =
    (struct _M0Y5Int64*)_M0L11_2aobj__ptrS1209;
  int64_t _M0L8_2afieldS3088 = _M0L14_2aboxed__selfS1210->$0;
  int64_t _M0L7_2aselfS1208;
  moonbit_decref(_M0L14_2aboxed__selfS1210);
  _M0L7_2aselfS1208 = _M0L8_2afieldS3088;
  return _M0IPC16option6OptionPB6ToJson8to__jsonGiE(_M0L7_2aselfS1208);
}

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1206
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1207 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1206;
  int32_t _M0L8_2afieldS3089 = _M0L14_2aboxed__selfS1207->$0;
  int32_t _M0L7_2aselfS1205;
  moonbit_decref(_M0L14_2aboxed__selfS1207);
  _M0L7_2aselfS1205 = _M0L8_2afieldS3089;
  return _M0IPC13int3IntPB6ToJson8to__json(_M0L7_2aselfS1205);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1260;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1259;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1252;
  moonbit_string_t* _M0L6_2atmpS1258;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1257;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1253;
  moonbit_string_t* _M0L6_2atmpS1256;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1255;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1254;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1125;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1251;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1250;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1249;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1236;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1126;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1248;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1247;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1246;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1237;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1127;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1245;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1244;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1243;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1238;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1128;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1242;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1241;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1240;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1239;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1124;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1235;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1234;
  _M0FPB4null = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  moonbit_incref(_M0FPB4null);
  _M0FPC17prelude4null = _M0FPB4null;
  _M0L6_2atmpS1260 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1260[0] = (moonbit_string_t)moonbit_string_literal_125.data;
  moonbit_incref(_M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1259
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1259)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1259->$0
  = _M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1259->$1 = _M0L6_2atmpS1260;
  _M0L8_2atupleS1252
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1252)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1252->$0 = 0;
  _M0L8_2atupleS1252->$1 = _M0L8_2atupleS1259;
  _M0L6_2atmpS1258 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1258[0] = (moonbit_string_t)moonbit_string_literal_126.data;
  moonbit_incref(_M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1257
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1257)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1257->$0
  = _M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1257->$1 = _M0L6_2atmpS1258;
  _M0L8_2atupleS1253
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1253)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1253->$0 = 1;
  _M0L8_2atupleS1253->$1 = _M0L8_2atupleS1257;
  _M0L6_2atmpS1256 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1256[0] = (moonbit_string_t)moonbit_string_literal_127.data;
  moonbit_incref(_M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1255
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1255)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1255->$0
  = _M0FP38clawteam8clawteam18ai__blackbox__test51____test__6d6573736167655f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1255->$1 = _M0L6_2atmpS1256;
  _M0L8_2atupleS1254
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1254)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1254->$0 = 2;
  _M0L8_2atupleS1254->$1 = _M0L8_2atupleS1255;
  _M0L7_2abindS1125
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1125[0] = _M0L8_2atupleS1252;
  _M0L7_2abindS1125[1] = _M0L8_2atupleS1253;
  _M0L7_2abindS1125[2] = _M0L8_2atupleS1254;
  _M0L6_2atmpS1251 = _M0L7_2abindS1125;
  _M0L6_2atmpS1250
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 3, _M0L6_2atmpS1251
  };
  #line 398 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1249
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1250);
  _M0L8_2atupleS1236
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1236)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1236->$0 = (moonbit_string_t)moonbit_string_literal_128.data;
  _M0L8_2atupleS1236->$1 = _M0L6_2atmpS1249;
  _M0L7_2abindS1126
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1248 = _M0L7_2abindS1126;
  _M0L6_2atmpS1247
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1248
  };
  #line 403 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1246
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1247);
  _M0L8_2atupleS1237
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1237)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1237->$0 = (moonbit_string_t)moonbit_string_literal_129.data;
  _M0L8_2atupleS1237->$1 = _M0L6_2atmpS1246;
  _M0L7_2abindS1127
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1245 = _M0L7_2abindS1127;
  _M0L6_2atmpS1244
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1245
  };
  #line 405 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1243
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1244);
  _M0L8_2atupleS1238
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1238)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1238->$0 = (moonbit_string_t)moonbit_string_literal_130.data;
  _M0L8_2atupleS1238->$1 = _M0L6_2atmpS1243;
  _M0L7_2abindS1128
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1242 = _M0L7_2abindS1128;
  _M0L6_2atmpS1241
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1242
  };
  #line 407 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1240
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1241);
  _M0L8_2atupleS1239
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1239)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1239->$0 = (moonbit_string_t)moonbit_string_literal_131.data;
  _M0L8_2atupleS1239->$1 = _M0L6_2atmpS1240;
  _M0L7_2abindS1124
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1124[0] = _M0L8_2atupleS1236;
  _M0L7_2abindS1124[1] = _M0L8_2atupleS1237;
  _M0L7_2abindS1124[2] = _M0L8_2atupleS1238;
  _M0L7_2abindS1124[3] = _M0L8_2atupleS1239;
  _M0L6_2atmpS1235 = _M0L7_2abindS1124;
  _M0L6_2atmpS1234
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 4, _M0L6_2atmpS1235
  };
  #line 397 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0FP38clawteam8clawteam18ai__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1234);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1233;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1195;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1196;
  int32_t _M0L7_2abindS1197;
  int32_t _M0L2__S1198;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1233
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1195
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1195)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1195->$0 = _M0L6_2atmpS1233;
  _M0L12async__testsS1195->$1 = 0;
  #line 446 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1196
  = _M0FP38clawteam8clawteam18ai__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1197 = _M0L7_2abindS1196->$1;
  _M0L2__S1198 = 0;
  while (1) {
    if (_M0L2__S1198 < _M0L7_2abindS1197) {
      struct _M0TUsiE** _M0L8_2afieldS3093 = _M0L7_2abindS1196->$0;
      struct _M0TUsiE** _M0L3bufS1232 = _M0L8_2afieldS3093;
      struct _M0TUsiE* _M0L6_2atmpS3092 =
        (struct _M0TUsiE*)_M0L3bufS1232[_M0L2__S1198];
      struct _M0TUsiE* _M0L3argS1199 = _M0L6_2atmpS3092;
      moonbit_string_t _M0L8_2afieldS3091 = _M0L3argS1199->$0;
      moonbit_string_t _M0L6_2atmpS1229 = _M0L8_2afieldS3091;
      int32_t _M0L8_2afieldS3090 = _M0L3argS1199->$1;
      int32_t _M0L6_2atmpS1230 = _M0L8_2afieldS3090;
      int32_t _M0L6_2atmpS1231;
      moonbit_incref(_M0L6_2atmpS1229);
      moonbit_incref(_M0L12async__testsS1195);
      #line 447 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
      _M0FP38clawteam8clawteam18ai__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1195, _M0L6_2atmpS1229, _M0L6_2atmpS1230);
      _M0L6_2atmpS1231 = _M0L2__S1198 + 1;
      _M0L2__S1198 = _M0L6_2atmpS1231;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1196);
    }
    break;
  }
  #line 449 "E:\\moonbit\\clawteam\\ai\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP38clawteam8clawteam18ai__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam18ai__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1195);
  return 0;
}