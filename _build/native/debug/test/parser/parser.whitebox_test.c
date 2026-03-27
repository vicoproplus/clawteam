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

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0TP38clawteam8clawteam6parser11RegexParser;

struct _M0TP38clawteam8clawteam6parser11ParseResult;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0KTPB4ShowS4Bool;

struct _M0DTPB4Json5Array;

struct _M0R38String_3a_3aiter_2eanon__u1942__l247__;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0Y4Bool;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam6parser33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam6parser33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0KTPB4ShowS3Int;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0BTPB4Show;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0DTPC15error5Error98clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB4ShowS6String;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPB9ArrayViewGsE;

struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0Y3Int {
  int32_t $0;
  
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

struct _M0TP38clawteam8clawteam6parser11RegexParser {
  struct _M0TPB5ArrayGsE* $0;
  struct _M0TPB5ArrayGsE* $1;
  
};

struct _M0TP38clawteam8clawteam6parser11ParseResult {
  int32_t $0;
  int32_t $1;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowS4Bool {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0DTPB4Json5Array {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u1942__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0Y4Bool {
  int32_t $0;
  
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

struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam6parser33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
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

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam6parser33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0KTPB4ShowS3Int {
  struct _M0BTPB4Show* $0;
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

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0DTPC15error5Error98clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowS6String {
  struct _M0BTPB4Show* $0;
  void* $1;
  
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

struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN17error__to__stringS1179(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN14handle__resultS1170(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testC2786l429(
  struct _M0TWEOc*
);

int32_t _M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testC2782l430(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP38clawteam8clawteam6parser45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1104(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1099(
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1086(
  int32_t,
  moonbit_string_t
);

#define _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP38clawteam8clawteam6parser28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam6parser34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__4(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__3(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__0(
  
);

struct _M0TP38clawteam8clawteam6parser11RegexParser* _M0MP38clawteam8clawteam6parser11RegexParser3new(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB5ArrayGsE*
);

void* _M0IP38clawteam8clawteam6parser11PatternTypePB6ToJson8to__json(int32_t);

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

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2145l591(
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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1961l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC1942l247(struct _M0TWEOc*);

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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(int32_t);

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

int32_t _M0MPB6Hasher12combine__int(struct _M0TPB6Hasher*, int32_t);

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show,
  moonbit_string_t,
  moonbit_string_t,
  struct _M0TPB5ArrayGORPB9SourceLocE*
);

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

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t);

moonbit_string_t _M0FPB33base64__encode__string__codepoint(moonbit_string_t);

int32_t _M0MPC16string6String16unsafe__char__at(moonbit_string_t, int32_t);

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t);

int32_t _M0FPB32code__point__of__surrogate__pair(int32_t, int32_t);

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t);

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t);

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

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGbE(
  void*
);

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
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

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGiE(
  void*
);

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
);

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    80, 97, 116, 116, 101, 114, 110, 84, 121, 112, 101, 32, 85, 110, 
    107, 110, 111, 119, 110, 32, 116, 111, 95, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    83, 117, 99, 99, 101, 115, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    34, 87, 111, 114, 107, 105, 110, 103, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 52, 58, 51, 45, 51, 52, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 55, 58, 51, 45, 55, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_30 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 112, 97, 114, 115, 101, 114, 34, 44, 32, 34, 102, 105, 108, 101, 
    110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[91]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 90), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 112, 97, 114, 115, 101, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 
    69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_65 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_34 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    34, 82, 101, 97, 100, 121, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 54, 58, 49, 49, 45, 50, 54, 58, 50, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    80, 97, 116, 116, 101, 114, 110, 84, 121, 112, 101, 32, 82, 101, 
    97, 100, 121, 32, 116, 111, 95, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_45 =
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
} const moonbit_string_literal_102 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 48, 58, 51, 55, 45, 50, 48, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 54, 58, 51, 45, 50, 54, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_55 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 49, 52, 58, 51, 55, 45, 49, 52, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_46 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    82, 101, 97, 100, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[93]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 92), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 112, 97, 114, 115, 101, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 
    116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 
    107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 49, 52, 58, 49, 49, 45, 49, 52, 58, 50, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 52, 58, 51, 53, 45, 51, 52, 58, 52, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 55, 58, 53, 49, 45, 55, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_25 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 62, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 55, 48, 58, 53, 45, 55, 
    48, 58, 54, 57, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 56, 58, 49, 49, 45, 56, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 49, 52, 58, 51, 45, 49, 52, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 54, 58, 51, 55, 45, 50, 54, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_83 =
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
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_69 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 56, 58, 51, 45, 56, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    85, 110, 107, 110, 111, 119, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 48, 58, 49, 49, 45, 50, 48, 58, 50, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 52, 58, 49, 49, 45, 51, 52, 58, 50, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    80, 97, 116, 116, 101, 114, 110, 84, 121, 112, 101, 32, 87, 111, 
    114, 107, 105, 110, 103, 32, 116, 111, 95, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 56, 58, 53, 51, 45, 56, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    112, 97, 114, 115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 
    109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 55, 58, 49, 49, 45, 55, 58, 52, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 112, 97, 114, 115, 101, 114, 58, 112, 97, 114, 
    115, 101, 114, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 48, 58, 51, 45, 50, 48, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 29), 
    80, 97, 114, 115, 101, 82, 101, 115, 117, 108, 116, 32, 105, 110, 
    116, 101, 114, 110, 97, 108, 32, 99, 114, 101, 97, 116, 105, 111, 
    110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_81 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    87, 111, 114, 107, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 56, 49, 58, 57, 45, 56, 
    49, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    34, 85, 110, 107, 110, 111, 119, 110, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    9581, 9472, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    80, 114, 111, 103, 114, 101, 115, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 29), 
    82, 101, 103, 101, 120, 80, 97, 114, 115, 101, 114, 32, 105, 110, 
    116, 101, 114, 110, 97, 108, 32, 99, 114, 101, 97, 116, 105, 111, 
    110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    84, 104, 105, 110, 107, 105, 110, 103, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint8_t const data[65]; 
} const moonbit_bytes_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 64), 
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 
    82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98, 99, 100, 101, 102, 103, 
    104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 
    57, 43, 47, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__4_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__4_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN17error__to__stringS1179$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN17error__to__stringS1179
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__3_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__4_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__4_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__1_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGiE}
  };

struct _M0BTPB4Show* _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

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

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGbE}
  };

struct _M0BTPB4Show* _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow}
  };

struct _M0BTPB4Show* _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

moonbit_bytes_t _M0FPB14base64__encodeN6base64S1657 =
  (moonbit_bytes_t)moonbit_bytes_literal_0.data;

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
} _M0FPB30ryu__to__string_2erecord_2f951$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB30ryu__to__string_2erecord_2f951 =
  &_M0FPB30ryu__to__string_2erecord_2f951$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP38clawteam8clawteam6parser48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2821
) {
  return _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2820
) {
  return _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__3();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2819
) {
  return _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2818
) {
  return _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser57____test__7061727365725f7762746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2817
) {
  return _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__4();
}

int32_t _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1200,
  moonbit_string_t _M0L8filenameS1175,
  int32_t _M0L5indexS1178
) {
  struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170* _closure_3166;
  struct _M0TWssbEu* _M0L14handle__resultS1170;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1179;
  void* _M0L11_2atry__errS1194;
  struct moonbit_result_0 _tmp_3168;
  int32_t _handle__error__result_3169;
  int32_t _M0L6_2atmpS2805;
  void* _M0L3errS1195;
  moonbit_string_t _M0L4nameS1197;
  struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1198;
  moonbit_string_t _M0L8_2afieldS2822;
  int32_t _M0L6_2acntS3080;
  moonbit_string_t _M0L7_2anameS1199;
  #line 528 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_incref(_M0L8filenameS1175);
  _closure_3166
  = (struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170*)moonbit_malloc(sizeof(struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170));
  Moonbit_object_header(_closure_3166)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170, $1) >> 2, 1, 0);
  _closure_3166->code
  = &_M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN14handle__resultS1170;
  _closure_3166->$0 = _M0L5indexS1178;
  _closure_3166->$1 = _M0L8filenameS1175;
  _M0L14handle__resultS1170 = (struct _M0TWssbEu*)_closure_3166;
  _M0L17error__to__stringS1179
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN17error__to__stringS1179$closure.data;
  moonbit_incref(_M0L12async__testsS1200);
  moonbit_incref(_M0L17error__to__stringS1179);
  moonbit_incref(_M0L8filenameS1175);
  moonbit_incref(_M0L14handle__resultS1170);
  #line 562 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _tmp_3168
  = _M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__test(_M0L12async__testsS1200, _M0L8filenameS1175, _M0L5indexS1178, _M0L14handle__resultS1170, _M0L17error__to__stringS1179);
  if (_tmp_3168.tag) {
    int32_t const _M0L5_2aokS2814 = _tmp_3168.data.ok;
    _handle__error__result_3169 = _M0L5_2aokS2814;
  } else {
    void* const _M0L6_2aerrS2815 = _tmp_3168.data.err;
    moonbit_decref(_M0L12async__testsS1200);
    moonbit_decref(_M0L17error__to__stringS1179);
    moonbit_decref(_M0L8filenameS1175);
    _M0L11_2atry__errS1194 = _M0L6_2aerrS2815;
    goto join_1193;
  }
  if (_handle__error__result_3169) {
    moonbit_decref(_M0L12async__testsS1200);
    moonbit_decref(_M0L17error__to__stringS1179);
    moonbit_decref(_M0L8filenameS1175);
    _M0L6_2atmpS2805 = 1;
  } else {
    struct moonbit_result_0 _tmp_3170;
    int32_t _handle__error__result_3171;
    moonbit_incref(_M0L12async__testsS1200);
    moonbit_incref(_M0L17error__to__stringS1179);
    moonbit_incref(_M0L8filenameS1175);
    moonbit_incref(_M0L14handle__resultS1170);
    #line 565 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    _tmp_3170
    = _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1200, _M0L8filenameS1175, _M0L5indexS1178, _M0L14handle__resultS1170, _M0L17error__to__stringS1179);
    if (_tmp_3170.tag) {
      int32_t const _M0L5_2aokS2812 = _tmp_3170.data.ok;
      _handle__error__result_3171 = _M0L5_2aokS2812;
    } else {
      void* const _M0L6_2aerrS2813 = _tmp_3170.data.err;
      moonbit_decref(_M0L12async__testsS1200);
      moonbit_decref(_M0L17error__to__stringS1179);
      moonbit_decref(_M0L8filenameS1175);
      _M0L11_2atry__errS1194 = _M0L6_2aerrS2813;
      goto join_1193;
    }
    if (_handle__error__result_3171) {
      moonbit_decref(_M0L12async__testsS1200);
      moonbit_decref(_M0L17error__to__stringS1179);
      moonbit_decref(_M0L8filenameS1175);
      _M0L6_2atmpS2805 = 1;
    } else {
      struct moonbit_result_0 _tmp_3172;
      int32_t _handle__error__result_3173;
      moonbit_incref(_M0L12async__testsS1200);
      moonbit_incref(_M0L17error__to__stringS1179);
      moonbit_incref(_M0L8filenameS1175);
      moonbit_incref(_M0L14handle__resultS1170);
      #line 568 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _tmp_3172
      = _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1200, _M0L8filenameS1175, _M0L5indexS1178, _M0L14handle__resultS1170, _M0L17error__to__stringS1179);
      if (_tmp_3172.tag) {
        int32_t const _M0L5_2aokS2810 = _tmp_3172.data.ok;
        _handle__error__result_3173 = _M0L5_2aokS2810;
      } else {
        void* const _M0L6_2aerrS2811 = _tmp_3172.data.err;
        moonbit_decref(_M0L12async__testsS1200);
        moonbit_decref(_M0L17error__to__stringS1179);
        moonbit_decref(_M0L8filenameS1175);
        _M0L11_2atry__errS1194 = _M0L6_2aerrS2811;
        goto join_1193;
      }
      if (_handle__error__result_3173) {
        moonbit_decref(_M0L12async__testsS1200);
        moonbit_decref(_M0L17error__to__stringS1179);
        moonbit_decref(_M0L8filenameS1175);
        _M0L6_2atmpS2805 = 1;
      } else {
        struct moonbit_result_0 _tmp_3174;
        int32_t _handle__error__result_3175;
        moonbit_incref(_M0L12async__testsS1200);
        moonbit_incref(_M0L17error__to__stringS1179);
        moonbit_incref(_M0L8filenameS1175);
        moonbit_incref(_M0L14handle__resultS1170);
        #line 571 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        _tmp_3174
        = _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1200, _M0L8filenameS1175, _M0L5indexS1178, _M0L14handle__resultS1170, _M0L17error__to__stringS1179);
        if (_tmp_3174.tag) {
          int32_t const _M0L5_2aokS2808 = _tmp_3174.data.ok;
          _handle__error__result_3175 = _M0L5_2aokS2808;
        } else {
          void* const _M0L6_2aerrS2809 = _tmp_3174.data.err;
          moonbit_decref(_M0L12async__testsS1200);
          moonbit_decref(_M0L17error__to__stringS1179);
          moonbit_decref(_M0L8filenameS1175);
          _M0L11_2atry__errS1194 = _M0L6_2aerrS2809;
          goto join_1193;
        }
        if (_handle__error__result_3175) {
          moonbit_decref(_M0L12async__testsS1200);
          moonbit_decref(_M0L17error__to__stringS1179);
          moonbit_decref(_M0L8filenameS1175);
          _M0L6_2atmpS2805 = 1;
        } else {
          struct moonbit_result_0 _tmp_3176;
          moonbit_incref(_M0L14handle__resultS1170);
          #line 574 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
          _tmp_3176
          = _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1200, _M0L8filenameS1175, _M0L5indexS1178, _M0L14handle__resultS1170, _M0L17error__to__stringS1179);
          if (_tmp_3176.tag) {
            int32_t const _M0L5_2aokS2806 = _tmp_3176.data.ok;
            _M0L6_2atmpS2805 = _M0L5_2aokS2806;
          } else {
            void* const _M0L6_2aerrS2807 = _tmp_3176.data.err;
            _M0L11_2atry__errS1194 = _M0L6_2aerrS2807;
            goto join_1193;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2805) {
    void* _M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2816 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2816)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2816)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1194
    = _M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2816;
    goto join_1193;
  } else {
    moonbit_decref(_M0L14handle__resultS1170);
  }
  goto joinlet_3167;
  join_1193:;
  _M0L3errS1195 = _M0L11_2atry__errS1194;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1198
  = (struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1195;
  _M0L8_2afieldS2822 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1198->$0;
  _M0L6_2acntS3080
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1198)->rc;
  if (_M0L6_2acntS3080 > 1) {
    int32_t _M0L11_2anew__cntS3081 = _M0L6_2acntS3080 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1198)->rc
    = _M0L11_2anew__cntS3081;
    moonbit_incref(_M0L8_2afieldS2822);
  } else if (_M0L6_2acntS3080 == 1) {
    #line 581 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1198);
  }
  _M0L7_2anameS1199 = _M0L8_2afieldS2822;
  _M0L4nameS1197 = _M0L7_2anameS1199;
  goto join_1196;
  goto joinlet_3177;
  join_1196:;
  #line 582 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN14handle__resultS1170(_M0L14handle__resultS1170, _M0L4nameS1197, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3177:;
  joinlet_3167:;
  return 0;
}

moonbit_string_t _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN17error__to__stringS1179(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2804,
  void* _M0L3errS1180
) {
  void* _M0L1eS1182;
  moonbit_string_t _M0L1eS1184;
  #line 551 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2804);
  switch (Moonbit_object_tag(_M0L3errS1180)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1185 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1180;
      moonbit_string_t _M0L8_2afieldS2823 = _M0L10_2aFailureS1185->$0;
      int32_t _M0L6_2acntS3082 =
        Moonbit_object_header(_M0L10_2aFailureS1185)->rc;
      moonbit_string_t _M0L4_2aeS1186;
      if (_M0L6_2acntS3082 > 1) {
        int32_t _M0L11_2anew__cntS3083 = _M0L6_2acntS3082 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1185)->rc
        = _M0L11_2anew__cntS3083;
        moonbit_incref(_M0L8_2afieldS2823);
      } else if (_M0L6_2acntS3082 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1185);
      }
      _M0L4_2aeS1186 = _M0L8_2afieldS2823;
      _M0L1eS1184 = _M0L4_2aeS1186;
      goto join_1183;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1187 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1180;
      moonbit_string_t _M0L8_2afieldS2824 = _M0L15_2aInspectErrorS1187->$0;
      int32_t _M0L6_2acntS3084 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1187)->rc;
      moonbit_string_t _M0L4_2aeS1188;
      if (_M0L6_2acntS3084 > 1) {
        int32_t _M0L11_2anew__cntS3085 = _M0L6_2acntS3084 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1187)->rc
        = _M0L11_2anew__cntS3085;
        moonbit_incref(_M0L8_2afieldS2824);
      } else if (_M0L6_2acntS3084 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1187);
      }
      _M0L4_2aeS1188 = _M0L8_2afieldS2824;
      _M0L1eS1184 = _M0L4_2aeS1188;
      goto join_1183;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1189 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1180;
      moonbit_string_t _M0L8_2afieldS2825 = _M0L16_2aSnapshotErrorS1189->$0;
      int32_t _M0L6_2acntS3086 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1189)->rc;
      moonbit_string_t _M0L4_2aeS1190;
      if (_M0L6_2acntS3086 > 1) {
        int32_t _M0L11_2anew__cntS3087 = _M0L6_2acntS3086 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1189)->rc
        = _M0L11_2anew__cntS3087;
        moonbit_incref(_M0L8_2afieldS2825);
      } else if (_M0L6_2acntS3086 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1189);
      }
      _M0L4_2aeS1190 = _M0L8_2afieldS2825;
      _M0L1eS1184 = _M0L4_2aeS1190;
      goto join_1183;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error98clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1191 =
        (struct _M0DTPC15error5Error98clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1180;
      moonbit_string_t _M0L8_2afieldS2826 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1191->$0;
      int32_t _M0L6_2acntS3088 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1191)->rc;
      moonbit_string_t _M0L4_2aeS1192;
      if (_M0L6_2acntS3088 > 1) {
        int32_t _M0L11_2anew__cntS3089 = _M0L6_2acntS3088 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1191)->rc
        = _M0L11_2anew__cntS3089;
        moonbit_incref(_M0L8_2afieldS2826);
      } else if (_M0L6_2acntS3088 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1191);
      }
      _M0L4_2aeS1192 = _M0L8_2afieldS2826;
      _M0L1eS1184 = _M0L4_2aeS1192;
      goto join_1183;
      break;
    }
    default: {
      _M0L1eS1182 = _M0L3errS1180;
      goto join_1181;
      break;
    }
  }
  join_1183:;
  return _M0L1eS1184;
  join_1181:;
  #line 557 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1182);
}

int32_t _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__executeN14handle__resultS1170(
  struct _M0TWssbEu* _M0L6_2aenvS2790,
  moonbit_string_t _M0L8testnameS1171,
  moonbit_string_t _M0L7messageS1172,
  int32_t _M0L7skippedS1173
) {
  struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170* _M0L14_2acasted__envS2791;
  moonbit_string_t _M0L8_2afieldS2836;
  moonbit_string_t _M0L8filenameS1175;
  int32_t _M0L8_2afieldS2835;
  int32_t _M0L6_2acntS3090;
  int32_t _M0L5indexS1178;
  int32_t _if__result_3180;
  moonbit_string_t _M0L10file__nameS1174;
  moonbit_string_t _M0L10test__nameS1176;
  moonbit_string_t _M0L7messageS1177;
  moonbit_string_t _M0L6_2atmpS2803;
  moonbit_string_t _M0L6_2atmpS2834;
  moonbit_string_t _M0L6_2atmpS2802;
  moonbit_string_t _M0L6_2atmpS2833;
  moonbit_string_t _M0L6_2atmpS2800;
  moonbit_string_t _M0L6_2atmpS2801;
  moonbit_string_t _M0L6_2atmpS2832;
  moonbit_string_t _M0L6_2atmpS2799;
  moonbit_string_t _M0L6_2atmpS2831;
  moonbit_string_t _M0L6_2atmpS2797;
  moonbit_string_t _M0L6_2atmpS2798;
  moonbit_string_t _M0L6_2atmpS2830;
  moonbit_string_t _M0L6_2atmpS2796;
  moonbit_string_t _M0L6_2atmpS2829;
  moonbit_string_t _M0L6_2atmpS2794;
  moonbit_string_t _M0L6_2atmpS2795;
  moonbit_string_t _M0L6_2atmpS2828;
  moonbit_string_t _M0L6_2atmpS2793;
  moonbit_string_t _M0L6_2atmpS2827;
  moonbit_string_t _M0L6_2atmpS2792;
  #line 535 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS2791
  = (struct _M0R102_24clawteam_2fclawteam_2fparser_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1170*)_M0L6_2aenvS2790;
  _M0L8_2afieldS2836 = _M0L14_2acasted__envS2791->$1;
  _M0L8filenameS1175 = _M0L8_2afieldS2836;
  _M0L8_2afieldS2835 = _M0L14_2acasted__envS2791->$0;
  _M0L6_2acntS3090 = Moonbit_object_header(_M0L14_2acasted__envS2791)->rc;
  if (_M0L6_2acntS3090 > 1) {
    int32_t _M0L11_2anew__cntS3091 = _M0L6_2acntS3090 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2791)->rc
    = _M0L11_2anew__cntS3091;
    moonbit_incref(_M0L8filenameS1175);
  } else if (_M0L6_2acntS3090 == 1) {
    #line 535 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2791);
  }
  _M0L5indexS1178 = _M0L8_2afieldS2835;
  if (!_M0L7skippedS1173) {
    _if__result_3180 = 1;
  } else {
    _if__result_3180 = 0;
  }
  if (_if__result_3180) {
    
  }
  #line 541 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L10file__nameS1174 = _M0MPC16string6String6escape(_M0L8filenameS1175);
  #line 542 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L10test__nameS1176 = _M0MPC16string6String6escape(_M0L8testnameS1171);
  #line 543 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L7messageS1177 = _M0MPC16string6String6escape(_M0L7messageS1172);
  #line 544 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 546 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2803
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1174);
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2834
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2803);
  moonbit_decref(_M0L6_2atmpS2803);
  _M0L6_2atmpS2802 = _M0L6_2atmpS2834;
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2833
  = moonbit_add_string(_M0L6_2atmpS2802, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2802);
  _M0L6_2atmpS2800 = _M0L6_2atmpS2833;
  #line 546 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2801
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1178);
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2832 = moonbit_add_string(_M0L6_2atmpS2800, _M0L6_2atmpS2801);
  moonbit_decref(_M0L6_2atmpS2800);
  moonbit_decref(_M0L6_2atmpS2801);
  _M0L6_2atmpS2799 = _M0L6_2atmpS2832;
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2831
  = moonbit_add_string(_M0L6_2atmpS2799, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2799);
  _M0L6_2atmpS2797 = _M0L6_2atmpS2831;
  #line 546 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2798
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1176);
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2830 = moonbit_add_string(_M0L6_2atmpS2797, _M0L6_2atmpS2798);
  moonbit_decref(_M0L6_2atmpS2797);
  moonbit_decref(_M0L6_2atmpS2798);
  _M0L6_2atmpS2796 = _M0L6_2atmpS2830;
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2829
  = moonbit_add_string(_M0L6_2atmpS2796, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2796);
  _M0L6_2atmpS2794 = _M0L6_2atmpS2829;
  #line 546 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2795
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1177);
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2828 = moonbit_add_string(_M0L6_2atmpS2794, _M0L6_2atmpS2795);
  moonbit_decref(_M0L6_2atmpS2794);
  moonbit_decref(_M0L6_2atmpS2795);
  _M0L6_2atmpS2793 = _M0L6_2atmpS2828;
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2827
  = moonbit_add_string(_M0L6_2atmpS2793, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2793);
  _M0L6_2atmpS2792 = _M0L6_2atmpS2827;
  #line 545 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2792);
  #line 548 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1169,
  moonbit_string_t _M0L8filenameS1166,
  int32_t _M0L5indexS1160,
  struct _M0TWssbEu* _M0L14handle__resultS1156,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1158
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1136;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1165;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1138;
  moonbit_string_t* _M0L5attrsS1139;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1159;
  moonbit_string_t _M0L4nameS1142;
  moonbit_string_t _M0L4nameS1140;
  int32_t _M0L6_2atmpS2789;
  struct _M0TWEOs* _M0L5_2aitS1144;
  struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__* _closure_3189;
  struct _M0TWEOc* _M0L6_2atmpS2780;
  struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__* _closure_3190;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2781;
  struct moonbit_result_0 _result_3191;
  #line 409 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1169);
  moonbit_incref(_M0FP38clawteam8clawteam6parser48moonbit__test__driver__internal__no__args__tests);
  #line 416 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1165
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP38clawteam8clawteam6parser48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1166);
  if (_M0L7_2abindS1165 == 0) {
    struct moonbit_result_0 _result_3182;
    if (_M0L7_2abindS1165) {
      moonbit_decref(_M0L7_2abindS1165);
    }
    moonbit_decref(_M0L17error__to__stringS1158);
    moonbit_decref(_M0L14handle__resultS1156);
    _result_3182.tag = 1;
    _result_3182.data.ok = 0;
    return _result_3182;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1167 =
      _M0L7_2abindS1165;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1168 =
      _M0L7_2aSomeS1167;
    _M0L10index__mapS1136 = _M0L13_2aindex__mapS1168;
    goto join_1135;
  }
  join_1135:;
  #line 418 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1159
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1136, _M0L5indexS1160);
  if (_M0L7_2abindS1159 == 0) {
    struct moonbit_result_0 _result_3184;
    if (_M0L7_2abindS1159) {
      moonbit_decref(_M0L7_2abindS1159);
    }
    moonbit_decref(_M0L17error__to__stringS1158);
    moonbit_decref(_M0L14handle__resultS1156);
    _result_3184.tag = 1;
    _result_3184.data.ok = 0;
    return _result_3184;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1161 =
      _M0L7_2abindS1159;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1162 = _M0L7_2aSomeS1161;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS2840 = _M0L4_2axS1162->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1163 = _M0L8_2afieldS2840;
    moonbit_string_t* _M0L8_2afieldS2839 = _M0L4_2axS1162->$1;
    int32_t _M0L6_2acntS3092 = Moonbit_object_header(_M0L4_2axS1162)->rc;
    moonbit_string_t* _M0L8_2aattrsS1164;
    if (_M0L6_2acntS3092 > 1) {
      int32_t _M0L11_2anew__cntS3093 = _M0L6_2acntS3092 - 1;
      Moonbit_object_header(_M0L4_2axS1162)->rc = _M0L11_2anew__cntS3093;
      moonbit_incref(_M0L8_2afieldS2839);
      moonbit_incref(_M0L4_2afS1163);
    } else if (_M0L6_2acntS3092 == 1) {
      #line 416 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      moonbit_free(_M0L4_2axS1162);
    }
    _M0L8_2aattrsS1164 = _M0L8_2afieldS2839;
    _M0L1fS1138 = _M0L4_2afS1163;
    _M0L5attrsS1139 = _M0L8_2aattrsS1164;
    goto join_1137;
  }
  join_1137:;
  _M0L6_2atmpS2789 = Moonbit_array_length(_M0L5attrsS1139);
  if (_M0L6_2atmpS2789 >= 1) {
    moonbit_string_t _M0L6_2atmpS2838 = (moonbit_string_t)_M0L5attrsS1139[0];
    moonbit_string_t _M0L7_2anameS1143 = _M0L6_2atmpS2838;
    moonbit_incref(_M0L7_2anameS1143);
    _M0L4nameS1142 = _M0L7_2anameS1143;
    goto join_1141;
  } else {
    _M0L4nameS1140 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3185;
  join_1141:;
  _M0L4nameS1140 = _M0L4nameS1142;
  joinlet_3185:;
  #line 419 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L5_2aitS1144 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1139);
  while (1) {
    moonbit_string_t _M0L4attrS1146;
    moonbit_string_t _M0L7_2abindS1153;
    int32_t _M0L6_2atmpS2773;
    int64_t _M0L6_2atmpS2772;
    moonbit_incref(_M0L5_2aitS1144);
    #line 421 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    _M0L7_2abindS1153 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1144);
    if (_M0L7_2abindS1153 == 0) {
      if (_M0L7_2abindS1153) {
        moonbit_decref(_M0L7_2abindS1153);
      }
      moonbit_decref(_M0L5_2aitS1144);
    } else {
      moonbit_string_t _M0L7_2aSomeS1154 = _M0L7_2abindS1153;
      moonbit_string_t _M0L7_2aattrS1155 = _M0L7_2aSomeS1154;
      _M0L4attrS1146 = _M0L7_2aattrS1155;
      goto join_1145;
    }
    goto joinlet_3187;
    join_1145:;
    _M0L6_2atmpS2773 = Moonbit_array_length(_M0L4attrS1146);
    _M0L6_2atmpS2772 = (int64_t)_M0L6_2atmpS2773;
    moonbit_incref(_M0L4attrS1146);
    #line 422 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1146, 5, 0, _M0L6_2atmpS2772)
    ) {
      int32_t _M0L6_2atmpS2779 = _M0L4attrS1146[0];
      int32_t _M0L4_2axS1147 = _M0L6_2atmpS2779;
      if (_M0L4_2axS1147 == 112) {
        int32_t _M0L6_2atmpS2778 = _M0L4attrS1146[1];
        int32_t _M0L4_2axS1148 = _M0L6_2atmpS2778;
        if (_M0L4_2axS1148 == 97) {
          int32_t _M0L6_2atmpS2777 = _M0L4attrS1146[2];
          int32_t _M0L4_2axS1149 = _M0L6_2atmpS2777;
          if (_M0L4_2axS1149 == 110) {
            int32_t _M0L6_2atmpS2776 = _M0L4attrS1146[3];
            int32_t _M0L4_2axS1150 = _M0L6_2atmpS2776;
            if (_M0L4_2axS1150 == 105) {
              int32_t _M0L6_2atmpS2837 = _M0L4attrS1146[4];
              int32_t _M0L6_2atmpS2775;
              int32_t _M0L4_2axS1151;
              moonbit_decref(_M0L4attrS1146);
              _M0L6_2atmpS2775 = _M0L6_2atmpS2837;
              _M0L4_2axS1151 = _M0L6_2atmpS2775;
              if (_M0L4_2axS1151 == 99) {
                void* _M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2774;
                struct moonbit_result_0 _result_3188;
                moonbit_decref(_M0L17error__to__stringS1158);
                moonbit_decref(_M0L14handle__resultS1156);
                moonbit_decref(_M0L5_2aitS1144);
                moonbit_decref(_M0L1fS1138);
                _M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2774
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2774)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2774)->$0
                = _M0L4nameS1140;
                _result_3188.tag = 0;
                _result_3188.data.err
                = _M0L100clawteam_2fclawteam_2fparser_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2774;
                return _result_3188;
              }
            } else {
              moonbit_decref(_M0L4attrS1146);
            }
          } else {
            moonbit_decref(_M0L4attrS1146);
          }
        } else {
          moonbit_decref(_M0L4attrS1146);
        }
      } else {
        moonbit_decref(_M0L4attrS1146);
      }
    } else {
      moonbit_decref(_M0L4attrS1146);
    }
    continue;
    joinlet_3187:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1156);
  moonbit_incref(_M0L4nameS1140);
  _closure_3189
  = (struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__*)moonbit_malloc(sizeof(struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__));
  Moonbit_object_header(_closure_3189)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__, $0) >> 2, 2, 0);
  _closure_3189->code
  = &_M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testC2786l429;
  _closure_3189->$0 = _M0L14handle__resultS1156;
  _closure_3189->$1 = _M0L4nameS1140;
  _M0L6_2atmpS2780 = (struct _M0TWEOc*)_closure_3189;
  _closure_3190
  = (struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__*)moonbit_malloc(sizeof(struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__));
  Moonbit_object_header(_closure_3190)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__, $0) >> 2, 3, 0);
  _closure_3190->code
  = &_M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testC2782l430;
  _closure_3190->$0 = _M0L17error__to__stringS1158;
  _closure_3190->$1 = _M0L14handle__resultS1156;
  _closure_3190->$2 = _M0L4nameS1140;
  _M0L6_2atmpS2781 = (struct _M0TWRPC15error5ErrorEu*)_closure_3190;
  #line 427 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0FP38clawteam8clawteam6parser45moonbit__test__driver__internal__catch__error(_M0L1fS1138, _M0L6_2atmpS2780, _M0L6_2atmpS2781);
  _result_3191.tag = 1;
  _result_3191.data.ok = 1;
  return _result_3191;
}

int32_t _M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testC2786l429(
  struct _M0TWEOc* _M0L6_2aenvS2787
) {
  struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__* _M0L14_2acasted__envS2788;
  moonbit_string_t _M0L8_2afieldS2842;
  moonbit_string_t _M0L4nameS1140;
  struct _M0TWssbEu* _M0L8_2afieldS2841;
  int32_t _M0L6_2acntS3094;
  struct _M0TWssbEu* _M0L14handle__resultS1156;
  #line 429 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS2788
  = (struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2786__l429__*)_M0L6_2aenvS2787;
  _M0L8_2afieldS2842 = _M0L14_2acasted__envS2788->$1;
  _M0L4nameS1140 = _M0L8_2afieldS2842;
  _M0L8_2afieldS2841 = _M0L14_2acasted__envS2788->$0;
  _M0L6_2acntS3094 = Moonbit_object_header(_M0L14_2acasted__envS2788)->rc;
  if (_M0L6_2acntS3094 > 1) {
    int32_t _M0L11_2anew__cntS3095 = _M0L6_2acntS3094 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2788)->rc
    = _M0L11_2anew__cntS3095;
    moonbit_incref(_M0L4nameS1140);
    moonbit_incref(_M0L8_2afieldS2841);
  } else if (_M0L6_2acntS3094 == 1) {
    #line 429 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2788);
  }
  _M0L14handle__resultS1156 = _M0L8_2afieldS2841;
  #line 429 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS1156->code(_M0L14handle__resultS1156, _M0L4nameS1140, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP38clawteam8clawteam6parser41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testC2782l430(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2783,
  void* _M0L3errS1157
) {
  struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__* _M0L14_2acasted__envS2784;
  moonbit_string_t _M0L8_2afieldS2845;
  moonbit_string_t _M0L4nameS1140;
  struct _M0TWssbEu* _M0L8_2afieldS2844;
  struct _M0TWssbEu* _M0L14handle__resultS1156;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2843;
  int32_t _M0L6_2acntS3096;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1158;
  moonbit_string_t _M0L6_2atmpS2785;
  #line 430 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS2784
  = (struct _M0R173_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fparser_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2782__l430__*)_M0L6_2aenvS2783;
  _M0L8_2afieldS2845 = _M0L14_2acasted__envS2784->$2;
  _M0L4nameS1140 = _M0L8_2afieldS2845;
  _M0L8_2afieldS2844 = _M0L14_2acasted__envS2784->$1;
  _M0L14handle__resultS1156 = _M0L8_2afieldS2844;
  _M0L8_2afieldS2843 = _M0L14_2acasted__envS2784->$0;
  _M0L6_2acntS3096 = Moonbit_object_header(_M0L14_2acasted__envS2784)->rc;
  if (_M0L6_2acntS3096 > 1) {
    int32_t _M0L11_2anew__cntS3097 = _M0L6_2acntS3096 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2784)->rc
    = _M0L11_2anew__cntS3097;
    moonbit_incref(_M0L4nameS1140);
    moonbit_incref(_M0L14handle__resultS1156);
    moonbit_incref(_M0L8_2afieldS2843);
  } else if (_M0L6_2acntS3096 == 1) {
    #line 430 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2784);
  }
  _M0L17error__to__stringS1158 = _M0L8_2afieldS2843;
  #line 430 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2785
  = _M0L17error__to__stringS1158->code(_M0L17error__to__stringS1158, _M0L3errS1157);
  #line 430 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS1156->code(_M0L14handle__resultS1156, _M0L4nameS1140, _M0L6_2atmpS2785, 0);
  return 0;
}

int32_t _M0FP38clawteam8clawteam6parser45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1131,
  struct _M0TWEOc* _M0L6on__okS1132,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1129
) {
  void* _M0L11_2atry__errS1127;
  struct moonbit_result_0 _tmp_3193;
  void* _M0L3errS1128;
  #line 375 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _tmp_3193 = _M0L1fS1131->code(_M0L1fS1131);
  if (_tmp_3193.tag) {
    int32_t const _M0L5_2aokS2770 = _tmp_3193.data.ok;
    moonbit_decref(_M0L7on__errS1129);
  } else {
    void* const _M0L6_2aerrS2771 = _tmp_3193.data.err;
    moonbit_decref(_M0L6on__okS1132);
    _M0L11_2atry__errS1127 = _M0L6_2aerrS2771;
    goto join_1126;
  }
  #line 382 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6on__okS1132->code(_M0L6on__okS1132);
  goto joinlet_3192;
  join_1126:;
  _M0L3errS1128 = _M0L11_2atry__errS1127;
  #line 383 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L7on__errS1129->code(_M0L7on__errS1129, _M0L3errS1128);
  joinlet_3192:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1086;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1099;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1104;
  struct _M0TUsiE** _M0L6_2atmpS2769;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1111;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1112;
  moonbit_string_t _M0L6_2atmpS2768;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1113;
  int32_t _M0L7_2abindS1114;
  int32_t _M0L2__S1115;
  #line 193 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1086 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1099
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1104 = 0;
  _M0L6_2atmpS2769 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1111
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1111)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1111->$0 = _M0L6_2atmpS2769;
  _M0L16file__and__indexS1111->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L9cli__argsS1112
  = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1099(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1099);
  #line 284 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2768 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1112, 1);
  #line 283 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L10test__argsS1113
  = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1104(_M0L51moonbit__test__driver__internal__split__mbt__stringS1104, _M0L6_2atmpS2768, 47);
  _M0L7_2abindS1114 = _M0L10test__argsS1113->$1;
  _M0L2__S1115 = 0;
  while (1) {
    if (_M0L2__S1115 < _M0L7_2abindS1114) {
      moonbit_string_t* _M0L8_2afieldS2847 = _M0L10test__argsS1113->$0;
      moonbit_string_t* _M0L3bufS2767 = _M0L8_2afieldS2847;
      moonbit_string_t _M0L6_2atmpS2846 =
        (moonbit_string_t)_M0L3bufS2767[_M0L2__S1115];
      moonbit_string_t _M0L3argS1116 = _M0L6_2atmpS2846;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1117;
      moonbit_string_t _M0L4fileS1118;
      moonbit_string_t _M0L5rangeS1119;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1120;
      moonbit_string_t _M0L6_2atmpS2765;
      int32_t _M0L5startS1121;
      moonbit_string_t _M0L6_2atmpS2764;
      int32_t _M0L3endS1122;
      int32_t _M0L1iS1123;
      int32_t _M0L6_2atmpS2766;
      moonbit_incref(_M0L3argS1116);
      #line 288 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L16file__and__rangeS1117
      = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1104(_M0L51moonbit__test__driver__internal__split__mbt__stringS1104, _M0L3argS1116, 58);
      moonbit_incref(_M0L16file__and__rangeS1117);
      #line 289 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L4fileS1118
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1117, 0);
      #line 290 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L5rangeS1119
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1117, 1);
      #line 291 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L15start__and__endS1120
      = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1104(_M0L51moonbit__test__driver__internal__split__mbt__stringS1104, _M0L5rangeS1119, 45);
      moonbit_incref(_M0L15start__and__endS1120);
      #line 294 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS2765
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1120, 0);
      #line 294 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L5startS1121
      = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1086(_M0L45moonbit__test__driver__internal__parse__int__S1086, _M0L6_2atmpS2765);
      #line 295 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS2764
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1120, 1);
      #line 295 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L3endS1122
      = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1086(_M0L45moonbit__test__driver__internal__parse__int__S1086, _M0L6_2atmpS2764);
      _M0L1iS1123 = _M0L5startS1121;
      while (1) {
        if (_M0L1iS1123 < _M0L3endS1122) {
          struct _M0TUsiE* _M0L8_2atupleS2762;
          int32_t _M0L6_2atmpS2763;
          moonbit_incref(_M0L4fileS1118);
          _M0L8_2atupleS2762
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2762)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2762->$0 = _M0L4fileS1118;
          _M0L8_2atupleS2762->$1 = _M0L1iS1123;
          moonbit_incref(_M0L16file__and__indexS1111);
          #line 297 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1111, _M0L8_2atupleS2762);
          _M0L6_2atmpS2763 = _M0L1iS1123 + 1;
          _M0L1iS1123 = _M0L6_2atmpS2763;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1118);
        }
        break;
      }
      _M0L6_2atmpS2766 = _M0L2__S1115 + 1;
      _M0L2__S1115 = _M0L6_2atmpS2766;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1113);
    }
    break;
  }
  return _M0L16file__and__indexS1111;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1104(
  int32_t _M0L6_2aenvS2743,
  moonbit_string_t _M0L1sS1105,
  int32_t _M0L3sepS1106
) {
  moonbit_string_t* _M0L6_2atmpS2761;
  struct _M0TPB5ArrayGsE* _M0L3resS1107;
  struct _M0TPC13ref3RefGiE* _M0L1iS1108;
  struct _M0TPC13ref3RefGiE* _M0L5startS1109;
  int32_t _M0L3valS2756;
  int32_t _M0L6_2atmpS2757;
  #line 261 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS2761 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1107
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1107)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1107->$0 = _M0L6_2atmpS2761;
  _M0L3resS1107->$1 = 0;
  _M0L1iS1108
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1108)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1108->$0 = 0;
  _M0L5startS1109
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1109)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1109->$0 = 0;
  while (1) {
    int32_t _M0L3valS2744 = _M0L1iS1108->$0;
    int32_t _M0L6_2atmpS2745 = Moonbit_array_length(_M0L1sS1105);
    if (_M0L3valS2744 < _M0L6_2atmpS2745) {
      int32_t _M0L3valS2748 = _M0L1iS1108->$0;
      int32_t _M0L6_2atmpS2747;
      int32_t _M0L6_2atmpS2746;
      int32_t _M0L3valS2755;
      int32_t _M0L6_2atmpS2754;
      if (
        _M0L3valS2748 < 0
        || _M0L3valS2748 >= Moonbit_array_length(_M0L1sS1105)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2747 = _M0L1sS1105[_M0L3valS2748];
      _M0L6_2atmpS2746 = _M0L6_2atmpS2747;
      if (_M0L6_2atmpS2746 == _M0L3sepS1106) {
        int32_t _M0L3valS2750 = _M0L5startS1109->$0;
        int32_t _M0L3valS2751 = _M0L1iS1108->$0;
        moonbit_string_t _M0L6_2atmpS2749;
        int32_t _M0L3valS2753;
        int32_t _M0L6_2atmpS2752;
        moonbit_incref(_M0L1sS1105);
        #line 270 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        _M0L6_2atmpS2749
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1105, _M0L3valS2750, _M0L3valS2751);
        moonbit_incref(_M0L3resS1107);
        #line 270 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1107, _M0L6_2atmpS2749);
        _M0L3valS2753 = _M0L1iS1108->$0;
        _M0L6_2atmpS2752 = _M0L3valS2753 + 1;
        _M0L5startS1109->$0 = _M0L6_2atmpS2752;
      }
      _M0L3valS2755 = _M0L1iS1108->$0;
      _M0L6_2atmpS2754 = _M0L3valS2755 + 1;
      _M0L1iS1108->$0 = _M0L6_2atmpS2754;
      continue;
    } else {
      moonbit_decref(_M0L1iS1108);
    }
    break;
  }
  _M0L3valS2756 = _M0L5startS1109->$0;
  _M0L6_2atmpS2757 = Moonbit_array_length(_M0L1sS1105);
  if (_M0L3valS2756 < _M0L6_2atmpS2757) {
    int32_t _M0L8_2afieldS2848 = _M0L5startS1109->$0;
    int32_t _M0L3valS2759;
    int32_t _M0L6_2atmpS2760;
    moonbit_string_t _M0L6_2atmpS2758;
    moonbit_decref(_M0L5startS1109);
    _M0L3valS2759 = _M0L8_2afieldS2848;
    _M0L6_2atmpS2760 = Moonbit_array_length(_M0L1sS1105);
    #line 276 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    _M0L6_2atmpS2758
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1105, _M0L3valS2759, _M0L6_2atmpS2760);
    moonbit_incref(_M0L3resS1107);
    #line 276 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1107, _M0L6_2atmpS2758);
  } else {
    moonbit_decref(_M0L5startS1109);
    moonbit_decref(_M0L1sS1105);
  }
  return _M0L3resS1107;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1099(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092
) {
  moonbit_bytes_t* _M0L3tmpS1100;
  int32_t _M0L6_2atmpS2742;
  struct _M0TPB5ArrayGsE* _M0L3resS1101;
  int32_t _M0L1iS1102;
  #line 250 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L3tmpS1100
  = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2742 = Moonbit_array_length(_M0L3tmpS1100);
  #line 254 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1101 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2742);
  _M0L1iS1102 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2738 = Moonbit_array_length(_M0L3tmpS1100);
    if (_M0L1iS1102 < _M0L6_2atmpS2738) {
      moonbit_bytes_t _M0L6_2atmpS2849;
      moonbit_bytes_t _M0L6_2atmpS2740;
      moonbit_string_t _M0L6_2atmpS2739;
      int32_t _M0L6_2atmpS2741;
      if (
        _M0L1iS1102 < 0 || _M0L1iS1102 >= Moonbit_array_length(_M0L3tmpS1100)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2849 = (moonbit_bytes_t)_M0L3tmpS1100[_M0L1iS1102];
      _M0L6_2atmpS2740 = _M0L6_2atmpS2849;
      moonbit_incref(_M0L6_2atmpS2740);
      #line 256 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS2739
      = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092, _M0L6_2atmpS2740);
      moonbit_incref(_M0L3resS1101);
      #line 256 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1101, _M0L6_2atmpS2739);
      _M0L6_2atmpS2741 = _M0L1iS1102 + 1;
      _M0L1iS1102 = _M0L6_2atmpS2741;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1100);
    }
    break;
  }
  return _M0L3resS1101;
}

moonbit_string_t _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1092(
  int32_t _M0L6_2aenvS2652,
  moonbit_bytes_t _M0L5bytesS1093
) {
  struct _M0TPB13StringBuilder* _M0L3resS1094;
  int32_t _M0L3lenS1095;
  struct _M0TPC13ref3RefGiE* _M0L1iS1096;
  #line 206 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1094 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1095 = Moonbit_array_length(_M0L5bytesS1093);
  _M0L1iS1096
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1096)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1096->$0 = 0;
  while (1) {
    int32_t _M0L3valS2653 = _M0L1iS1096->$0;
    if (_M0L3valS2653 < _M0L3lenS1095) {
      int32_t _M0L3valS2737 = _M0L1iS1096->$0;
      int32_t _M0L6_2atmpS2736;
      int32_t _M0L6_2atmpS2735;
      struct _M0TPC13ref3RefGiE* _M0L1cS1097;
      int32_t _M0L3valS2654;
      if (
        _M0L3valS2737 < 0
        || _M0L3valS2737 >= Moonbit_array_length(_M0L5bytesS1093)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2736 = _M0L5bytesS1093[_M0L3valS2737];
      _M0L6_2atmpS2735 = (int32_t)_M0L6_2atmpS2736;
      _M0L1cS1097
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1097)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1097->$0 = _M0L6_2atmpS2735;
      _M0L3valS2654 = _M0L1cS1097->$0;
      if (_M0L3valS2654 < 128) {
        int32_t _M0L8_2afieldS2850 = _M0L1cS1097->$0;
        int32_t _M0L3valS2656;
        int32_t _M0L6_2atmpS2655;
        int32_t _M0L3valS2658;
        int32_t _M0L6_2atmpS2657;
        moonbit_decref(_M0L1cS1097);
        _M0L3valS2656 = _M0L8_2afieldS2850;
        _M0L6_2atmpS2655 = _M0L3valS2656;
        moonbit_incref(_M0L3resS1094);
        #line 215 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1094, _M0L6_2atmpS2655);
        _M0L3valS2658 = _M0L1iS1096->$0;
        _M0L6_2atmpS2657 = _M0L3valS2658 + 1;
        _M0L1iS1096->$0 = _M0L6_2atmpS2657;
      } else {
        int32_t _M0L3valS2659 = _M0L1cS1097->$0;
        if (_M0L3valS2659 < 224) {
          int32_t _M0L3valS2661 = _M0L1iS1096->$0;
          int32_t _M0L6_2atmpS2660 = _M0L3valS2661 + 1;
          int32_t _M0L3valS2670;
          int32_t _M0L6_2atmpS2669;
          int32_t _M0L6_2atmpS2663;
          int32_t _M0L3valS2668;
          int32_t _M0L6_2atmpS2667;
          int32_t _M0L6_2atmpS2666;
          int32_t _M0L6_2atmpS2665;
          int32_t _M0L6_2atmpS2664;
          int32_t _M0L6_2atmpS2662;
          int32_t _M0L8_2afieldS2851;
          int32_t _M0L3valS2672;
          int32_t _M0L6_2atmpS2671;
          int32_t _M0L3valS2674;
          int32_t _M0L6_2atmpS2673;
          if (_M0L6_2atmpS2660 >= _M0L3lenS1095) {
            moonbit_decref(_M0L1cS1097);
            moonbit_decref(_M0L1iS1096);
            moonbit_decref(_M0L5bytesS1093);
            break;
          }
          _M0L3valS2670 = _M0L1cS1097->$0;
          _M0L6_2atmpS2669 = _M0L3valS2670 & 31;
          _M0L6_2atmpS2663 = _M0L6_2atmpS2669 << 6;
          _M0L3valS2668 = _M0L1iS1096->$0;
          _M0L6_2atmpS2667 = _M0L3valS2668 + 1;
          if (
            _M0L6_2atmpS2667 < 0
            || _M0L6_2atmpS2667 >= Moonbit_array_length(_M0L5bytesS1093)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2666 = _M0L5bytesS1093[_M0L6_2atmpS2667];
          _M0L6_2atmpS2665 = (int32_t)_M0L6_2atmpS2666;
          _M0L6_2atmpS2664 = _M0L6_2atmpS2665 & 63;
          _M0L6_2atmpS2662 = _M0L6_2atmpS2663 | _M0L6_2atmpS2664;
          _M0L1cS1097->$0 = _M0L6_2atmpS2662;
          _M0L8_2afieldS2851 = _M0L1cS1097->$0;
          moonbit_decref(_M0L1cS1097);
          _M0L3valS2672 = _M0L8_2afieldS2851;
          _M0L6_2atmpS2671 = _M0L3valS2672;
          moonbit_incref(_M0L3resS1094);
          #line 222 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1094, _M0L6_2atmpS2671);
          _M0L3valS2674 = _M0L1iS1096->$0;
          _M0L6_2atmpS2673 = _M0L3valS2674 + 2;
          _M0L1iS1096->$0 = _M0L6_2atmpS2673;
        } else {
          int32_t _M0L3valS2675 = _M0L1cS1097->$0;
          if (_M0L3valS2675 < 240) {
            int32_t _M0L3valS2677 = _M0L1iS1096->$0;
            int32_t _M0L6_2atmpS2676 = _M0L3valS2677 + 2;
            int32_t _M0L3valS2693;
            int32_t _M0L6_2atmpS2692;
            int32_t _M0L6_2atmpS2685;
            int32_t _M0L3valS2691;
            int32_t _M0L6_2atmpS2690;
            int32_t _M0L6_2atmpS2689;
            int32_t _M0L6_2atmpS2688;
            int32_t _M0L6_2atmpS2687;
            int32_t _M0L6_2atmpS2686;
            int32_t _M0L6_2atmpS2679;
            int32_t _M0L3valS2684;
            int32_t _M0L6_2atmpS2683;
            int32_t _M0L6_2atmpS2682;
            int32_t _M0L6_2atmpS2681;
            int32_t _M0L6_2atmpS2680;
            int32_t _M0L6_2atmpS2678;
            int32_t _M0L8_2afieldS2852;
            int32_t _M0L3valS2695;
            int32_t _M0L6_2atmpS2694;
            int32_t _M0L3valS2697;
            int32_t _M0L6_2atmpS2696;
            if (_M0L6_2atmpS2676 >= _M0L3lenS1095) {
              moonbit_decref(_M0L1cS1097);
              moonbit_decref(_M0L1iS1096);
              moonbit_decref(_M0L5bytesS1093);
              break;
            }
            _M0L3valS2693 = _M0L1cS1097->$0;
            _M0L6_2atmpS2692 = _M0L3valS2693 & 15;
            _M0L6_2atmpS2685 = _M0L6_2atmpS2692 << 12;
            _M0L3valS2691 = _M0L1iS1096->$0;
            _M0L6_2atmpS2690 = _M0L3valS2691 + 1;
            if (
              _M0L6_2atmpS2690 < 0
              || _M0L6_2atmpS2690 >= Moonbit_array_length(_M0L5bytesS1093)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2689 = _M0L5bytesS1093[_M0L6_2atmpS2690];
            _M0L6_2atmpS2688 = (int32_t)_M0L6_2atmpS2689;
            _M0L6_2atmpS2687 = _M0L6_2atmpS2688 & 63;
            _M0L6_2atmpS2686 = _M0L6_2atmpS2687 << 6;
            _M0L6_2atmpS2679 = _M0L6_2atmpS2685 | _M0L6_2atmpS2686;
            _M0L3valS2684 = _M0L1iS1096->$0;
            _M0L6_2atmpS2683 = _M0L3valS2684 + 2;
            if (
              _M0L6_2atmpS2683 < 0
              || _M0L6_2atmpS2683 >= Moonbit_array_length(_M0L5bytesS1093)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2682 = _M0L5bytesS1093[_M0L6_2atmpS2683];
            _M0L6_2atmpS2681 = (int32_t)_M0L6_2atmpS2682;
            _M0L6_2atmpS2680 = _M0L6_2atmpS2681 & 63;
            _M0L6_2atmpS2678 = _M0L6_2atmpS2679 | _M0L6_2atmpS2680;
            _M0L1cS1097->$0 = _M0L6_2atmpS2678;
            _M0L8_2afieldS2852 = _M0L1cS1097->$0;
            moonbit_decref(_M0L1cS1097);
            _M0L3valS2695 = _M0L8_2afieldS2852;
            _M0L6_2atmpS2694 = _M0L3valS2695;
            moonbit_incref(_M0L3resS1094);
            #line 231 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1094, _M0L6_2atmpS2694);
            _M0L3valS2697 = _M0L1iS1096->$0;
            _M0L6_2atmpS2696 = _M0L3valS2697 + 3;
            _M0L1iS1096->$0 = _M0L6_2atmpS2696;
          } else {
            int32_t _M0L3valS2699 = _M0L1iS1096->$0;
            int32_t _M0L6_2atmpS2698 = _M0L3valS2699 + 3;
            int32_t _M0L3valS2722;
            int32_t _M0L6_2atmpS2721;
            int32_t _M0L6_2atmpS2714;
            int32_t _M0L3valS2720;
            int32_t _M0L6_2atmpS2719;
            int32_t _M0L6_2atmpS2718;
            int32_t _M0L6_2atmpS2717;
            int32_t _M0L6_2atmpS2716;
            int32_t _M0L6_2atmpS2715;
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
            int32_t _M0L3valS2724;
            int32_t _M0L6_2atmpS2723;
            int32_t _M0L3valS2728;
            int32_t _M0L6_2atmpS2727;
            int32_t _M0L6_2atmpS2726;
            int32_t _M0L6_2atmpS2725;
            int32_t _M0L8_2afieldS2853;
            int32_t _M0L3valS2732;
            int32_t _M0L6_2atmpS2731;
            int32_t _M0L6_2atmpS2730;
            int32_t _M0L6_2atmpS2729;
            int32_t _M0L3valS2734;
            int32_t _M0L6_2atmpS2733;
            if (_M0L6_2atmpS2698 >= _M0L3lenS1095) {
              moonbit_decref(_M0L1cS1097);
              moonbit_decref(_M0L1iS1096);
              moonbit_decref(_M0L5bytesS1093);
              break;
            }
            _M0L3valS2722 = _M0L1cS1097->$0;
            _M0L6_2atmpS2721 = _M0L3valS2722 & 7;
            _M0L6_2atmpS2714 = _M0L6_2atmpS2721 << 18;
            _M0L3valS2720 = _M0L1iS1096->$0;
            _M0L6_2atmpS2719 = _M0L3valS2720 + 1;
            if (
              _M0L6_2atmpS2719 < 0
              || _M0L6_2atmpS2719 >= Moonbit_array_length(_M0L5bytesS1093)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2718 = _M0L5bytesS1093[_M0L6_2atmpS2719];
            _M0L6_2atmpS2717 = (int32_t)_M0L6_2atmpS2718;
            _M0L6_2atmpS2716 = _M0L6_2atmpS2717 & 63;
            _M0L6_2atmpS2715 = _M0L6_2atmpS2716 << 12;
            _M0L6_2atmpS2707 = _M0L6_2atmpS2714 | _M0L6_2atmpS2715;
            _M0L3valS2713 = _M0L1iS1096->$0;
            _M0L6_2atmpS2712 = _M0L3valS2713 + 2;
            if (
              _M0L6_2atmpS2712 < 0
              || _M0L6_2atmpS2712 >= Moonbit_array_length(_M0L5bytesS1093)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2711 = _M0L5bytesS1093[_M0L6_2atmpS2712];
            _M0L6_2atmpS2710 = (int32_t)_M0L6_2atmpS2711;
            _M0L6_2atmpS2709 = _M0L6_2atmpS2710 & 63;
            _M0L6_2atmpS2708 = _M0L6_2atmpS2709 << 6;
            _M0L6_2atmpS2701 = _M0L6_2atmpS2707 | _M0L6_2atmpS2708;
            _M0L3valS2706 = _M0L1iS1096->$0;
            _M0L6_2atmpS2705 = _M0L3valS2706 + 3;
            if (
              _M0L6_2atmpS2705 < 0
              || _M0L6_2atmpS2705 >= Moonbit_array_length(_M0L5bytesS1093)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2704 = _M0L5bytesS1093[_M0L6_2atmpS2705];
            _M0L6_2atmpS2703 = (int32_t)_M0L6_2atmpS2704;
            _M0L6_2atmpS2702 = _M0L6_2atmpS2703 & 63;
            _M0L6_2atmpS2700 = _M0L6_2atmpS2701 | _M0L6_2atmpS2702;
            _M0L1cS1097->$0 = _M0L6_2atmpS2700;
            _M0L3valS2724 = _M0L1cS1097->$0;
            _M0L6_2atmpS2723 = _M0L3valS2724 - 65536;
            _M0L1cS1097->$0 = _M0L6_2atmpS2723;
            _M0L3valS2728 = _M0L1cS1097->$0;
            _M0L6_2atmpS2727 = _M0L3valS2728 >> 10;
            _M0L6_2atmpS2726 = _M0L6_2atmpS2727 + 55296;
            _M0L6_2atmpS2725 = _M0L6_2atmpS2726;
            moonbit_incref(_M0L3resS1094);
            #line 242 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1094, _M0L6_2atmpS2725);
            _M0L8_2afieldS2853 = _M0L1cS1097->$0;
            moonbit_decref(_M0L1cS1097);
            _M0L3valS2732 = _M0L8_2afieldS2853;
            _M0L6_2atmpS2731 = _M0L3valS2732 & 1023;
            _M0L6_2atmpS2730 = _M0L6_2atmpS2731 + 56320;
            _M0L6_2atmpS2729 = _M0L6_2atmpS2730;
            moonbit_incref(_M0L3resS1094);
            #line 243 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1094, _M0L6_2atmpS2729);
            _M0L3valS2734 = _M0L1iS1096->$0;
            _M0L6_2atmpS2733 = _M0L3valS2734 + 4;
            _M0L1iS1096->$0 = _M0L6_2atmpS2733;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1096);
      moonbit_decref(_M0L5bytesS1093);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1094);
}

int32_t _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1086(
  int32_t _M0L6_2aenvS2645,
  moonbit_string_t _M0L1sS1087
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1088;
  int32_t _M0L3lenS1089;
  int32_t _M0L1iS1090;
  int32_t _M0L8_2afieldS2854;
  #line 197 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1088
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1088)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1088->$0 = 0;
  _M0L3lenS1089 = Moonbit_array_length(_M0L1sS1087);
  _M0L1iS1090 = 0;
  while (1) {
    if (_M0L1iS1090 < _M0L3lenS1089) {
      int32_t _M0L3valS2650 = _M0L3resS1088->$0;
      int32_t _M0L6_2atmpS2647 = _M0L3valS2650 * 10;
      int32_t _M0L6_2atmpS2649;
      int32_t _M0L6_2atmpS2648;
      int32_t _M0L6_2atmpS2646;
      int32_t _M0L6_2atmpS2651;
      if (
        _M0L1iS1090 < 0 || _M0L1iS1090 >= Moonbit_array_length(_M0L1sS1087)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2649 = _M0L1sS1087[_M0L1iS1090];
      _M0L6_2atmpS2648 = _M0L6_2atmpS2649 - 48;
      _M0L6_2atmpS2646 = _M0L6_2atmpS2647 + _M0L6_2atmpS2648;
      _M0L3resS1088->$0 = _M0L6_2atmpS2646;
      _M0L6_2atmpS2651 = _M0L1iS1090 + 1;
      _M0L1iS1090 = _M0L6_2atmpS2651;
      continue;
    } else {
      moonbit_decref(_M0L1sS1087);
    }
    break;
  }
  _M0L8_2afieldS2854 = _M0L3resS1088->$0;
  moonbit_decref(_M0L3resS1088);
  return _M0L8_2afieldS2854;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1066,
  moonbit_string_t _M0L12_2adiscard__S1067,
  int32_t _M0L12_2adiscard__S1068,
  struct _M0TWssbEu* _M0L12_2adiscard__S1069,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1070
) {
  struct moonbit_result_0 _result_3200;
  #line 34 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1070);
  moonbit_decref(_M0L12_2adiscard__S1069);
  moonbit_decref(_M0L12_2adiscard__S1067);
  moonbit_decref(_M0L12_2adiscard__S1066);
  _result_3200.tag = 1;
  _result_3200.data.ok = 0;
  return _result_3200;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1071,
  moonbit_string_t _M0L12_2adiscard__S1072,
  int32_t _M0L12_2adiscard__S1073,
  struct _M0TWssbEu* _M0L12_2adiscard__S1074,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1075
) {
  struct moonbit_result_0 _result_3201;
  #line 34 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1075);
  moonbit_decref(_M0L12_2adiscard__S1074);
  moonbit_decref(_M0L12_2adiscard__S1072);
  moonbit_decref(_M0L12_2adiscard__S1071);
  _result_3201.tag = 1;
  _result_3201.data.ok = 0;
  return _result_3201;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1076,
  moonbit_string_t _M0L12_2adiscard__S1077,
  int32_t _M0L12_2adiscard__S1078,
  struct _M0TWssbEu* _M0L12_2adiscard__S1079,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1080
) {
  struct moonbit_result_0 _result_3202;
  #line 34 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1080);
  moonbit_decref(_M0L12_2adiscard__S1079);
  moonbit_decref(_M0L12_2adiscard__S1077);
  moonbit_decref(_M0L12_2adiscard__S1076);
  _result_3202.tag = 1;
  _result_3202.data.ok = 0;
  return _result_3202;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam6parser21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam6parser50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1081,
  moonbit_string_t _M0L12_2adiscard__S1082,
  int32_t _M0L12_2adiscard__S1083,
  struct _M0TWssbEu* _M0L12_2adiscard__S1084,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1085
) {
  struct moonbit_result_0 _result_3203;
  #line 34 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1085);
  moonbit_decref(_M0L12_2adiscard__S1084);
  moonbit_decref(_M0L12_2adiscard__S1082);
  moonbit_decref(_M0L12_2adiscard__S1081);
  _result_3203.tag = 1;
  _result_3203.data.ok = 0;
  return _result_3203;
}

int32_t _M0IP016_24default__implP38clawteam8clawteam6parser28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam6parser34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1065
) {
  #line 12 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1065);
  return 0;
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__4(
  
) {
  struct _M0TP38clawteam8clawteam6parser11ParseResult* _M0L6resultS1064;
  int32_t _M0L8_2afieldS2855;
  int32_t _M0L7matchedS2643;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2644;
  struct _M0TPB4Show _M0L6_2atmpS2636;
  moonbit_string_t _M0L6_2atmpS2639;
  moonbit_string_t _M0L6_2atmpS2640;
  moonbit_string_t _M0L6_2atmpS2641;
  moonbit_string_t _M0L6_2atmpS2642;
  moonbit_string_t* _M0L6_2atmpS2638;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2637;
  #line 29 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6resultS1064
  = (struct _M0TP38clawteam8clawteam6parser11ParseResult*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6parser11ParseResult));
  Moonbit_object_header(_M0L6resultS1064)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP38clawteam8clawteam6parser11ParseResult) >> 2, 0, 0);
  _M0L6resultS1064->$0 = 1;
  _M0L6resultS1064->$1 = 3;
  _M0L8_2afieldS2855 = _M0L6resultS1064->$0;
  moonbit_decref(_M0L6resultS1064);
  _M0L7matchedS2643 = _M0L8_2afieldS2855;
  _M0L14_2aboxed__selfS2644
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2644)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2644->$0 = _M0L7matchedS2643;
  _M0L6_2atmpS2636
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2644
  };
  _M0L6_2atmpS2639 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS2640 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS2641 = 0;
  _M0L6_2atmpS2642 = 0;
  _M0L6_2atmpS2638 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2638[0] = _M0L6_2atmpS2639;
  _M0L6_2atmpS2638[1] = _M0L6_2atmpS2640;
  _M0L6_2atmpS2638[2] = _M0L6_2atmpS2641;
  _M0L6_2atmpS2638[3] = _M0L6_2atmpS2642;
  _M0L6_2atmpS2637
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2637)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2637->$0 = _M0L6_2atmpS2638;
  _M0L6_2atmpS2637->$1 = 4;
  #line 34 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2636, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS2637);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__3(
  
) {
  int32_t _M0L2ptS1062;
  void* _M0L4jsonS1063;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2635;
  moonbit_string_t _M0L6_2atmpS2634;
  struct _M0TPB4Show _M0L6_2atmpS2627;
  moonbit_string_t _M0L6_2atmpS2630;
  moonbit_string_t _M0L6_2atmpS2631;
  moonbit_string_t _M0L6_2atmpS2632;
  moonbit_string_t _M0L6_2atmpS2633;
  moonbit_string_t* _M0L6_2atmpS2629;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2628;
  #line 23 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L2ptS1062 = 5;
  #line 25 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L4jsonS1063
  = _M0IP38clawteam8clawteam6parser11PatternTypePB6ToJson8to__json(_M0L2ptS1062);
  _M0L6_2atmpS2635 = 0;
  #line 26 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6_2atmpS2634
  = _M0MPC14json4Json17stringify_2einner(_M0L4jsonS1063, 0, 0, _M0L6_2atmpS2635);
  _M0L6_2atmpS2627
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2634
  };
  _M0L6_2atmpS2630 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS2631 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS2632 = 0;
  _M0L6_2atmpS2633 = 0;
  _M0L6_2atmpS2629 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2629[0] = _M0L6_2atmpS2630;
  _M0L6_2atmpS2629[1] = _M0L6_2atmpS2631;
  _M0L6_2atmpS2629[2] = _M0L6_2atmpS2632;
  _M0L6_2atmpS2629[3] = _M0L6_2atmpS2633;
  _M0L6_2atmpS2628
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2628)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2628->$0 = _M0L6_2atmpS2629;
  _M0L6_2atmpS2628->$1 = 4;
  #line 26 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2627, (moonbit_string_t)moonbit_string_literal_15.data, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS2628);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__2(
  
) {
  int32_t _M0L2ptS1060;
  void* _M0L4jsonS1061;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2626;
  moonbit_string_t _M0L6_2atmpS2625;
  struct _M0TPB4Show _M0L6_2atmpS2618;
  moonbit_string_t _M0L6_2atmpS2621;
  moonbit_string_t _M0L6_2atmpS2622;
  moonbit_string_t _M0L6_2atmpS2623;
  moonbit_string_t _M0L6_2atmpS2624;
  moonbit_string_t* _M0L6_2atmpS2620;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2619;
  #line 17 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L2ptS1060 = 4;
  #line 19 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L4jsonS1061
  = _M0IP38clawteam8clawteam6parser11PatternTypePB6ToJson8to__json(_M0L2ptS1060);
  _M0L6_2atmpS2626 = 0;
  #line 20 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6_2atmpS2625
  = _M0MPC14json4Json17stringify_2einner(_M0L4jsonS1061, 0, 0, _M0L6_2atmpS2626);
  _M0L6_2atmpS2618
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2625
  };
  _M0L6_2atmpS2621 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS2622 = (moonbit_string_t)moonbit_string_literal_18.data;
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
  #line 20 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2618, (moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, _M0L6_2atmpS2619);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__1(
  
) {
  int32_t _M0L2ptS1058;
  void* _M0L4jsonS1059;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2617;
  moonbit_string_t _M0L6_2atmpS2616;
  struct _M0TPB4Show _M0L6_2atmpS2609;
  moonbit_string_t _M0L6_2atmpS2612;
  moonbit_string_t _M0L6_2atmpS2613;
  moonbit_string_t _M0L6_2atmpS2614;
  moonbit_string_t _M0L6_2atmpS2615;
  moonbit_string_t* _M0L6_2atmpS2611;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2610;
  #line 11 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L2ptS1058 = 3;
  #line 13 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L4jsonS1059
  = _M0IP38clawteam8clawteam6parser11PatternTypePB6ToJson8to__json(_M0L2ptS1058);
  _M0L6_2atmpS2617 = 0;
  #line 14 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6_2atmpS2616
  = _M0MPC14json4Json17stringify_2einner(_M0L4jsonS1059, 0, 0, _M0L6_2atmpS2617);
  _M0L6_2atmpS2609
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2616
  };
  _M0L6_2atmpS2612 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS2613 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS2614 = 0;
  _M0L6_2atmpS2615 = 0;
  _M0L6_2atmpS2611 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2611[0] = _M0L6_2atmpS2612;
  _M0L6_2atmpS2611[1] = _M0L6_2atmpS2613;
  _M0L6_2atmpS2611[2] = _M0L6_2atmpS2614;
  _M0L6_2atmpS2611[3] = _M0L6_2atmpS2615;
  _M0L6_2atmpS2610
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2610)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2610->$0 = _M0L6_2atmpS2611;
  _M0L6_2atmpS2610->$1 = 4;
  #line 14 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2609, (moonbit_string_t)moonbit_string_literal_23.data, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS2610);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam6parser47____test__7061727365725f7762746573742e6d6274__0(
  
) {
  moonbit_string_t* _M0L6_2atmpS2608;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2605;
  moonbit_string_t* _M0L6_2atmpS2607;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2606;
  struct _M0TP38clawteam8clawteam6parser11RegexParser* _M0L6parserS1057;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS2857;
  struct _M0TPB5ArrayGsE* _M0L15ready__patternsS2592;
  int32_t _M0L6_2atmpS2590;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2591;
  struct _M0TPB4Show _M0L6_2atmpS2583;
  moonbit_string_t _M0L6_2atmpS2586;
  moonbit_string_t _M0L6_2atmpS2587;
  moonbit_string_t _M0L6_2atmpS2588;
  moonbit_string_t _M0L6_2atmpS2589;
  moonbit_string_t* _M0L6_2atmpS2585;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2584;
  struct moonbit_result_0 _tmp_3204;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS2856;
  int32_t _M0L6_2acntS3098;
  struct _M0TPB5ArrayGsE* _M0L17working__patternsS2604;
  int32_t _M0L6_2atmpS2602;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2603;
  struct _M0TPB4Show _M0L6_2atmpS2595;
  moonbit_string_t _M0L6_2atmpS2598;
  moonbit_string_t _M0L6_2atmpS2599;
  moonbit_string_t _M0L6_2atmpS2600;
  moonbit_string_t _M0L6_2atmpS2601;
  moonbit_string_t* _M0L6_2atmpS2597;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2596;
  #line 5 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6_2atmpS2608 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2608[0] = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS2608[1] = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS2605
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2605)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2605->$0 = _M0L6_2atmpS2608;
  _M0L6_2atmpS2605->$1 = 2;
  _M0L6_2atmpS2607 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2607[0] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS2606
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2606)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2606->$0 = _M0L6_2atmpS2607;
  _M0L6_2atmpS2606->$1 = 1;
  #line 6 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6parserS1057
  = _M0MP38clawteam8clawteam6parser11RegexParser3new(_M0L6_2atmpS2605, _M0L6_2atmpS2606);
  _M0L8_2afieldS2857 = _M0L6parserS1057->$0;
  _M0L15ready__patternsS2592 = _M0L8_2afieldS2857;
  moonbit_incref(_M0L15ready__patternsS2592);
  #line 7 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6_2atmpS2590
  = _M0MPC15array5Array6lengthGsE(_M0L15ready__patternsS2592);
  _M0L14_2aboxed__selfS2591
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2591)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2591->$0 = _M0L6_2atmpS2590;
  _M0L6_2atmpS2583
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2591
  };
  _M0L6_2atmpS2586 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS2587 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS2588 = 0;
  _M0L6_2atmpS2589 = 0;
  _M0L6_2atmpS2585 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2585[0] = _M0L6_2atmpS2586;
  _M0L6_2atmpS2585[1] = _M0L6_2atmpS2587;
  _M0L6_2atmpS2585[2] = _M0L6_2atmpS2588;
  _M0L6_2atmpS2585[3] = _M0L6_2atmpS2589;
  _M0L6_2atmpS2584
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2584)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2584->$0 = _M0L6_2atmpS2585;
  _M0L6_2atmpS2584->$1 = 4;
  #line 7 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _tmp_3204
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2583, (moonbit_string_t)moonbit_string_literal_30.data, (moonbit_string_t)moonbit_string_literal_31.data, _M0L6_2atmpS2584);
  if (_tmp_3204.tag) {
    int32_t const _M0L5_2aokS2593 = _tmp_3204.data.ok;
  } else {
    void* const _M0L6_2aerrS2594 = _tmp_3204.data.err;
    struct moonbit_result_0 _result_3205;
    moonbit_decref(_M0L6parserS1057);
    _result_3205.tag = 0;
    _result_3205.data.err = _M0L6_2aerrS2594;
    return _result_3205;
  }
  _M0L8_2afieldS2856 = _M0L6parserS1057->$1;
  _M0L6_2acntS3098 = Moonbit_object_header(_M0L6parserS1057)->rc;
  if (_M0L6_2acntS3098 > 1) {
    int32_t _M0L11_2anew__cntS3100 = _M0L6_2acntS3098 - 1;
    Moonbit_object_header(_M0L6parserS1057)->rc = _M0L11_2anew__cntS3100;
    moonbit_incref(_M0L8_2afieldS2856);
  } else if (_M0L6_2acntS3098 == 1) {
    struct _M0TPB5ArrayGsE* _M0L8_2afieldS3099 = _M0L6parserS1057->$0;
    moonbit_decref(_M0L8_2afieldS3099);
    #line 8 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
    moonbit_free(_M0L6parserS1057);
  }
  _M0L17working__patternsS2604 = _M0L8_2afieldS2856;
  #line 8 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  _M0L6_2atmpS2602
  = _M0MPC15array5Array6lengthGsE(_M0L17working__patternsS2604);
  _M0L14_2aboxed__selfS2603
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2603)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2603->$0 = _M0L6_2atmpS2602;
  _M0L6_2atmpS2595
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2603
  };
  _M0L6_2atmpS2598 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS2599 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS2600 = 0;
  _M0L6_2atmpS2601 = 0;
  _M0L6_2atmpS2597 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2597[0] = _M0L6_2atmpS2598;
  _M0L6_2atmpS2597[1] = _M0L6_2atmpS2599;
  _M0L6_2atmpS2597[2] = _M0L6_2atmpS2600;
  _M0L6_2atmpS2597[3] = _M0L6_2atmpS2601;
  _M0L6_2atmpS2596
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2596)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2596->$0 = _M0L6_2atmpS2597;
  _M0L6_2atmpS2596->$1 = 4;
  #line 8 "E:\\moonbit\\clawteam\\parser\\parser_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2595, (moonbit_string_t)moonbit_string_literal_34.data, (moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS2596);
}

struct _M0TP38clawteam8clawteam6parser11RegexParser* _M0MP38clawteam8clawteam6parser11RegexParser3new(
  struct _M0TPB5ArrayGsE* _M0L15ready__patternsS1055,
  struct _M0TPB5ArrayGsE* _M0L17working__patternsS1056
) {
  struct _M0TP38clawteam8clawteam6parser11RegexParser* _block_3206;
  #line 31 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
  _block_3206
  = (struct _M0TP38clawteam8clawteam6parser11RegexParser*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6parser11RegexParser));
  Moonbit_object_header(_block_3206)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6parser11RegexParser, $0) >> 2, 2, 0);
  _block_3206->$0 = _M0L15ready__patternsS1055;
  _block_3206->$1 = _M0L17working__patternsS1056;
  return _block_3206;
}

void* _M0IP38clawteam8clawteam6parser11PatternTypePB6ToJson8to__json(
  int32_t _M0L9_2ax__118S1054
) {
  #line 7 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
  switch (_M0L9_2ax__118S1054) {
    case 0: {
      #line 7 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_36.data);
      break;
    }
    
    case 1: {
      #line 7 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_37.data);
      break;
    }
    
    case 2: {
      #line 7 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_38.data);
      break;
    }
    
    case 3: {
      #line 7 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_39.data);
      break;
    }
    
    case 4: {
      #line 7 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_40.data);
      break;
    }
    default: {
      #line 7 "E:\\moonbit\\clawteam\\parser\\regex_parser.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_41.data);
      break;
    }
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1053,
  int32_t _M0L13escape__slashS1025,
  int32_t _M0L6indentS1020,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1046
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1012;
  void** _M0L6_2atmpS2582;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1013;
  int32_t _M0Lm5depthS1014;
  void* _M0L6_2atmpS2581;
  void* _M0L8_2aparamS1015;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1012 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2582 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1013
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1013)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1013->$0 = _M0L6_2atmpS2582;
  _M0L5stackS1013->$1 = 0;
  _M0Lm5depthS1014 = 0;
  _M0L6_2atmpS2581 = _M0L4selfS1053;
  _M0L8_2aparamS1015 = _M0L6_2atmpS2581;
  _2aloop_1031:;
  while (1) {
    if (_M0L8_2aparamS1015 == 0) {
      int32_t _M0L3lenS2543;
      if (_M0L8_2aparamS1015) {
        moonbit_decref(_M0L8_2aparamS1015);
      }
      _M0L3lenS2543 = _M0L5stackS1013->$1;
      if (_M0L3lenS2543 == 0) {
        if (_M0L8replacerS1046) {
          moonbit_decref(_M0L8replacerS1046);
        }
        moonbit_decref(_M0L5stackS1013);
        break;
      } else {
        void** _M0L8_2afieldS2865 = _M0L5stackS1013->$0;
        void** _M0L3bufS2567 = _M0L8_2afieldS2865;
        int32_t _M0L3lenS2569 = _M0L5stackS1013->$1;
        int32_t _M0L6_2atmpS2568 = _M0L3lenS2569 - 1;
        void* _M0L6_2atmpS2864 = (void*)_M0L3bufS2567[_M0L6_2atmpS2568];
        void* _M0L4_2axS1032 = _M0L6_2atmpS2864;
        switch (Moonbit_object_tag(_M0L4_2axS1032)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1033 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1032;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2860 =
              _M0L8_2aArrayS1033->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1034 =
              _M0L8_2afieldS2860;
            int32_t _M0L4_2aiS1035 = _M0L8_2aArrayS1033->$1;
            int32_t _M0L3lenS2555 = _M0L6_2aarrS1034->$1;
            if (_M0L4_2aiS1035 < _M0L3lenS2555) {
              int32_t _if__result_3208;
              void** _M0L8_2afieldS2859;
              void** _M0L3bufS2561;
              void* _M0L6_2atmpS2858;
              void* _M0L7elementS1036;
              int32_t _M0L6_2atmpS2556;
              void* _M0L6_2atmpS2559;
              if (_M0L4_2aiS1035 < 0) {
                _if__result_3208 = 1;
              } else {
                int32_t _M0L3lenS2560 = _M0L6_2aarrS1034->$1;
                _if__result_3208 = _M0L4_2aiS1035 >= _M0L3lenS2560;
              }
              if (_if__result_3208) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS2859 = _M0L6_2aarrS1034->$0;
              _M0L3bufS2561 = _M0L8_2afieldS2859;
              _M0L6_2atmpS2858 = (void*)_M0L3bufS2561[_M0L4_2aiS1035];
              _M0L7elementS1036 = _M0L6_2atmpS2858;
              _M0L6_2atmpS2556 = _M0L4_2aiS1035 + 1;
              _M0L8_2aArrayS1033->$1 = _M0L6_2atmpS2556;
              if (_M0L4_2aiS1035 > 0) {
                int32_t _M0L6_2atmpS2558;
                moonbit_string_t _M0L6_2atmpS2557;
                moonbit_incref(_M0L7elementS1036);
                moonbit_incref(_M0L3bufS1012);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 44);
                _M0L6_2atmpS2558 = _M0Lm5depthS1014;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2557
                = _M0FPC14json11indent__str(_M0L6_2atmpS2558, _M0L6indentS1020);
                moonbit_incref(_M0L3bufS1012);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2557);
              } else {
                moonbit_incref(_M0L7elementS1036);
              }
              _M0L6_2atmpS2559 = _M0L7elementS1036;
              _M0L8_2aparamS1015 = _M0L6_2atmpS2559;
              goto _2aloop_1031;
            } else {
              int32_t _M0L6_2atmpS2562 = _M0Lm5depthS1014;
              void* _M0L6_2atmpS2563;
              int32_t _M0L6_2atmpS2565;
              moonbit_string_t _M0L6_2atmpS2564;
              void* _M0L6_2atmpS2566;
              _M0Lm5depthS1014 = _M0L6_2atmpS2562 - 1;
              moonbit_incref(_M0L5stackS1013);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2563
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1013);
              if (_M0L6_2atmpS2563) {
                moonbit_decref(_M0L6_2atmpS2563);
              }
              _M0L6_2atmpS2565 = _M0Lm5depthS1014;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2564
              = _M0FPC14json11indent__str(_M0L6_2atmpS2565, _M0L6indentS1020);
              moonbit_incref(_M0L3bufS1012);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2564);
              moonbit_incref(_M0L3bufS1012);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 93);
              _M0L6_2atmpS2566 = 0;
              _M0L8_2aparamS1015 = _M0L6_2atmpS2566;
              goto _2aloop_1031;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1037 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1032;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS2863 =
              _M0L9_2aObjectS1037->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1038 =
              _M0L8_2afieldS2863;
            int32_t _M0L8_2afirstS1039 = _M0L9_2aObjectS1037->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1040;
            moonbit_incref(_M0L11_2aiteratorS1038);
            moonbit_incref(_M0L9_2aObjectS1037);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1040
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1038);
            if (_M0L7_2abindS1040 == 0) {
              int32_t _M0L6_2atmpS2544;
              void* _M0L6_2atmpS2545;
              int32_t _M0L6_2atmpS2547;
              moonbit_string_t _M0L6_2atmpS2546;
              void* _M0L6_2atmpS2548;
              if (_M0L7_2abindS1040) {
                moonbit_decref(_M0L7_2abindS1040);
              }
              moonbit_decref(_M0L9_2aObjectS1037);
              _M0L6_2atmpS2544 = _M0Lm5depthS1014;
              _M0Lm5depthS1014 = _M0L6_2atmpS2544 - 1;
              moonbit_incref(_M0L5stackS1013);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2545
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1013);
              if (_M0L6_2atmpS2545) {
                moonbit_decref(_M0L6_2atmpS2545);
              }
              _M0L6_2atmpS2547 = _M0Lm5depthS1014;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2546
              = _M0FPC14json11indent__str(_M0L6_2atmpS2547, _M0L6indentS1020);
              moonbit_incref(_M0L3bufS1012);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2546);
              moonbit_incref(_M0L3bufS1012);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 125);
              _M0L6_2atmpS2548 = 0;
              _M0L8_2aparamS1015 = _M0L6_2atmpS2548;
              goto _2aloop_1031;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1041 = _M0L7_2abindS1040;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1042 = _M0L7_2aSomeS1041;
              moonbit_string_t _M0L8_2afieldS2862 = _M0L4_2axS1042->$0;
              moonbit_string_t _M0L4_2akS1043 = _M0L8_2afieldS2862;
              void* _M0L8_2afieldS2861 = _M0L4_2axS1042->$1;
              int32_t _M0L6_2acntS3101 =
                Moonbit_object_header(_M0L4_2axS1042)->rc;
              void* _M0L4_2avS1044;
              void* _M0Lm2v2S1045;
              moonbit_string_t _M0L6_2atmpS2552;
              void* _M0L6_2atmpS2554;
              void* _M0L6_2atmpS2553;
              if (_M0L6_2acntS3101 > 1) {
                int32_t _M0L11_2anew__cntS3102 = _M0L6_2acntS3101 - 1;
                Moonbit_object_header(_M0L4_2axS1042)->rc
                = _M0L11_2anew__cntS3102;
                moonbit_incref(_M0L8_2afieldS2861);
                moonbit_incref(_M0L4_2akS1043);
              } else if (_M0L6_2acntS3101 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1042);
              }
              _M0L4_2avS1044 = _M0L8_2afieldS2861;
              _M0Lm2v2S1045 = _M0L4_2avS1044;
              if (_M0L8replacerS1046 == 0) {
                moonbit_incref(_M0Lm2v2S1045);
                moonbit_decref(_M0L4_2avS1044);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1047 =
                  _M0L8replacerS1046;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1048 =
                  _M0L7_2aSomeS1047;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1049 =
                  _M0L11_2areplacerS1048;
                void* _M0L7_2abindS1050;
                moonbit_incref(_M0L7_2afuncS1049);
                moonbit_incref(_M0L4_2akS1043);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1050
                = _M0L7_2afuncS1049->code(_M0L7_2afuncS1049, _M0L4_2akS1043, _M0L4_2avS1044);
                if (_M0L7_2abindS1050 == 0) {
                  void* _M0L6_2atmpS2549;
                  if (_M0L7_2abindS1050) {
                    moonbit_decref(_M0L7_2abindS1050);
                  }
                  moonbit_decref(_M0L4_2akS1043);
                  moonbit_decref(_M0L9_2aObjectS1037);
                  _M0L6_2atmpS2549 = 0;
                  _M0L8_2aparamS1015 = _M0L6_2atmpS2549;
                  goto _2aloop_1031;
                } else {
                  void* _M0L7_2aSomeS1051 = _M0L7_2abindS1050;
                  void* _M0L4_2avS1052 = _M0L7_2aSomeS1051;
                  _M0Lm2v2S1045 = _M0L4_2avS1052;
                }
              }
              if (!_M0L8_2afirstS1039) {
                int32_t _M0L6_2atmpS2551;
                moonbit_string_t _M0L6_2atmpS2550;
                moonbit_incref(_M0L3bufS1012);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 44);
                _M0L6_2atmpS2551 = _M0Lm5depthS1014;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2550
                = _M0FPC14json11indent__str(_M0L6_2atmpS2551, _M0L6indentS1020);
                moonbit_incref(_M0L3bufS1012);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2550);
              }
              moonbit_incref(_M0L3bufS1012);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2552
              = _M0FPC14json6escape(_M0L4_2akS1043, _M0L13escape__slashS1025);
              moonbit_incref(_M0L3bufS1012);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2552);
              moonbit_incref(_M0L3bufS1012);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 34);
              moonbit_incref(_M0L3bufS1012);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 58);
              if (_M0L6indentS1020 > 0) {
                moonbit_incref(_M0L3bufS1012);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 32);
              }
              _M0L9_2aObjectS1037->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1037);
              _M0L6_2atmpS2554 = _M0Lm2v2S1045;
              _M0L6_2atmpS2553 = _M0L6_2atmpS2554;
              _M0L8_2aparamS1015 = _M0L6_2atmpS2553;
              goto _2aloop_1031;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1016 = _M0L8_2aparamS1015;
      void* _M0L8_2avalueS1017 = _M0L7_2aSomeS1016;
      void* _M0L6_2atmpS2580;
      switch (Moonbit_object_tag(_M0L8_2avalueS1017)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1018 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1017;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS2866 =
            _M0L9_2aObjectS1018->$0;
          int32_t _M0L6_2acntS3103 =
            Moonbit_object_header(_M0L9_2aObjectS1018)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1019;
          if (_M0L6_2acntS3103 > 1) {
            int32_t _M0L11_2anew__cntS3104 = _M0L6_2acntS3103 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1018)->rc
            = _M0L11_2anew__cntS3104;
            moonbit_incref(_M0L8_2afieldS2866);
          } else if (_M0L6_2acntS3103 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1018);
          }
          _M0L10_2amembersS1019 = _M0L8_2afieldS2866;
          moonbit_incref(_M0L10_2amembersS1019);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1019)) {
            moonbit_decref(_M0L10_2amembersS1019);
            moonbit_incref(_M0L3bufS1012);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, (moonbit_string_t)moonbit_string_literal_42.data);
          } else {
            int32_t _M0L6_2atmpS2575 = _M0Lm5depthS1014;
            int32_t _M0L6_2atmpS2577;
            moonbit_string_t _M0L6_2atmpS2576;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2579;
            void* _M0L6ObjectS2578;
            _M0Lm5depthS1014 = _M0L6_2atmpS2575 + 1;
            moonbit_incref(_M0L3bufS1012);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 123);
            _M0L6_2atmpS2577 = _M0Lm5depthS1014;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2576
            = _M0FPC14json11indent__str(_M0L6_2atmpS2577, _M0L6indentS1020);
            moonbit_incref(_M0L3bufS1012);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2576);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2579
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1019);
            _M0L6ObjectS2578
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2578)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2578)->$0
            = _M0L6_2atmpS2579;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2578)->$1
            = 1;
            moonbit_incref(_M0L5stackS1013);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1013, _M0L6ObjectS2578);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1021 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1017;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2867 =
            _M0L8_2aArrayS1021->$0;
          int32_t _M0L6_2acntS3105 =
            Moonbit_object_header(_M0L8_2aArrayS1021)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1022;
          if (_M0L6_2acntS3105 > 1) {
            int32_t _M0L11_2anew__cntS3106 = _M0L6_2acntS3105 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1021)->rc
            = _M0L11_2anew__cntS3106;
            moonbit_incref(_M0L8_2afieldS2867);
          } else if (_M0L6_2acntS3105 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1021);
          }
          _M0L6_2aarrS1022 = _M0L8_2afieldS2867;
          moonbit_incref(_M0L6_2aarrS1022);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1022)) {
            moonbit_decref(_M0L6_2aarrS1022);
            moonbit_incref(_M0L3bufS1012);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, (moonbit_string_t)moonbit_string_literal_43.data);
          } else {
            int32_t _M0L6_2atmpS2571 = _M0Lm5depthS1014;
            int32_t _M0L6_2atmpS2573;
            moonbit_string_t _M0L6_2atmpS2572;
            void* _M0L5ArrayS2574;
            _M0Lm5depthS1014 = _M0L6_2atmpS2571 + 1;
            moonbit_incref(_M0L3bufS1012);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 91);
            _M0L6_2atmpS2573 = _M0Lm5depthS1014;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2572
            = _M0FPC14json11indent__str(_M0L6_2atmpS2573, _M0L6indentS1020);
            moonbit_incref(_M0L3bufS1012);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2572);
            _M0L5ArrayS2574
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2574)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2574)->$0
            = _M0L6_2aarrS1022;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2574)->$1
            = 0;
            moonbit_incref(_M0L5stackS1013);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1013, _M0L5ArrayS2574);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1023 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1017;
          moonbit_string_t _M0L8_2afieldS2868 = _M0L9_2aStringS1023->$0;
          int32_t _M0L6_2acntS3107 =
            Moonbit_object_header(_M0L9_2aStringS1023)->rc;
          moonbit_string_t _M0L4_2asS1024;
          moonbit_string_t _M0L6_2atmpS2570;
          if (_M0L6_2acntS3107 > 1) {
            int32_t _M0L11_2anew__cntS3108 = _M0L6_2acntS3107 - 1;
            Moonbit_object_header(_M0L9_2aStringS1023)->rc
            = _M0L11_2anew__cntS3108;
            moonbit_incref(_M0L8_2afieldS2868);
          } else if (_M0L6_2acntS3107 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1023);
          }
          _M0L4_2asS1024 = _M0L8_2afieldS2868;
          moonbit_incref(_M0L3bufS1012);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2570
          = _M0FPC14json6escape(_M0L4_2asS1024, _M0L13escape__slashS1025);
          moonbit_incref(_M0L3bufS1012);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L6_2atmpS2570);
          moonbit_incref(_M0L3bufS1012);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1012, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1026 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1017;
          double _M0L4_2anS1027 = _M0L9_2aNumberS1026->$0;
          moonbit_string_t _M0L8_2afieldS2869 = _M0L9_2aNumberS1026->$1;
          int32_t _M0L6_2acntS3109 =
            Moonbit_object_header(_M0L9_2aNumberS1026)->rc;
          moonbit_string_t _M0L7_2areprS1028;
          if (_M0L6_2acntS3109 > 1) {
            int32_t _M0L11_2anew__cntS3110 = _M0L6_2acntS3109 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1026)->rc
            = _M0L11_2anew__cntS3110;
            if (_M0L8_2afieldS2869) {
              moonbit_incref(_M0L8_2afieldS2869);
            }
          } else if (_M0L6_2acntS3109 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1026);
          }
          _M0L7_2areprS1028 = _M0L8_2afieldS2869;
          if (_M0L7_2areprS1028 == 0) {
            if (_M0L7_2areprS1028) {
              moonbit_decref(_M0L7_2areprS1028);
            }
            moonbit_incref(_M0L3bufS1012);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1012, _M0L4_2anS1027);
          } else {
            moonbit_string_t _M0L7_2aSomeS1029 = _M0L7_2areprS1028;
            moonbit_string_t _M0L4_2arS1030 = _M0L7_2aSomeS1029;
            moonbit_incref(_M0L3bufS1012);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, _M0L4_2arS1030);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1012);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, (moonbit_string_t)moonbit_string_literal_11.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1012);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, (moonbit_string_t)moonbit_string_literal_44.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1017);
          moonbit_incref(_M0L3bufS1012);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1012, (moonbit_string_t)moonbit_string_literal_45.data);
          break;
        }
      }
      _M0L6_2atmpS2580 = 0;
      _M0L8_2aparamS1015 = _M0L6_2atmpS2580;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1012);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1011,
  int32_t _M0L6indentS1009
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1009 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1010 = _M0L6indentS1009 * _M0L5levelS1011;
    switch (_M0L6spacesS1010) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_46.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_47.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_48.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_49.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_50.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_51.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_52.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_53.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_54.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2542;
        moonbit_string_t _M0L6_2atmpS2870;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2542
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_55.data, _M0L6spacesS1010);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2870
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_46.data, _M0L6_2atmpS2542);
        moonbit_decref(_M0L6_2atmpS2542);
        return _M0L6_2atmpS2870;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1001,
  int32_t _M0L13escape__slashS1006
) {
  int32_t _M0L6_2atmpS2541;
  struct _M0TPB13StringBuilder* _M0L3bufS1000;
  struct _M0TWEOc* _M0L5_2aitS1002;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2541 = Moonbit_array_length(_M0L3strS1001);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1000 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2541);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1002 = _M0MPC16string6String4iter(_M0L3strS1001);
  while (1) {
    int32_t _M0L7_2abindS1003;
    moonbit_incref(_M0L5_2aitS1002);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1003 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1002);
    if (_M0L7_2abindS1003 == -1) {
      moonbit_decref(_M0L5_2aitS1002);
    } else {
      int32_t _M0L7_2aSomeS1004 = _M0L7_2abindS1003;
      int32_t _M0L4_2acS1005 = _M0L7_2aSomeS1004;
      if (_M0L4_2acS1005 == 34) {
        moonbit_incref(_M0L3bufS1000);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_56.data);
      } else if (_M0L4_2acS1005 == 92) {
        moonbit_incref(_M0L3bufS1000);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_57.data);
      } else if (_M0L4_2acS1005 == 47) {
        if (_M0L13escape__slashS1006) {
          moonbit_incref(_M0L3bufS1000);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_58.data);
        } else {
          moonbit_incref(_M0L3bufS1000);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1000, _M0L4_2acS1005);
        }
      } else if (_M0L4_2acS1005 == 10) {
        moonbit_incref(_M0L3bufS1000);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_59.data);
      } else if (_M0L4_2acS1005 == 13) {
        moonbit_incref(_M0L3bufS1000);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_60.data);
      } else if (_M0L4_2acS1005 == 8) {
        moonbit_incref(_M0L3bufS1000);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_61.data);
      } else if (_M0L4_2acS1005 == 9) {
        moonbit_incref(_M0L3bufS1000);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_62.data);
      } else {
        int32_t _M0L4codeS1007 = _M0L4_2acS1005;
        if (_M0L4codeS1007 == 12) {
          moonbit_incref(_M0L3bufS1000);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_63.data);
        } else if (_M0L4codeS1007 < 32) {
          int32_t _M0L6_2atmpS2540;
          moonbit_string_t _M0L6_2atmpS2539;
          moonbit_incref(_M0L3bufS1000);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, (moonbit_string_t)moonbit_string_literal_64.data);
          _M0L6_2atmpS2540 = _M0L4codeS1007 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2539 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2540);
          moonbit_incref(_M0L3bufS1000);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1000, _M0L6_2atmpS2539);
        } else {
          moonbit_incref(_M0L3bufS1000);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1000, _M0L4_2acS1005);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1000);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS999
) {
  int32_t _M0L8_2afieldS2871;
  int32_t _M0L3lenS2538;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS2871 = _M0L4selfS999->$1;
  moonbit_decref(_M0L4selfS999);
  _M0L3lenS2538 = _M0L8_2afieldS2871;
  return _M0L3lenS2538 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS996
) {
  int32_t _M0L3lenS995;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS995 = _M0L4selfS996->$1;
  if (_M0L3lenS995 == 0) {
    moonbit_decref(_M0L4selfS996);
    return 0;
  } else {
    int32_t _M0L5indexS997 = _M0L3lenS995 - 1;
    void** _M0L8_2afieldS2875 = _M0L4selfS996->$0;
    void** _M0L3bufS2537 = _M0L8_2afieldS2875;
    void* _M0L6_2atmpS2874 = (void*)_M0L3bufS2537[_M0L5indexS997];
    void* _M0L1vS998 = _M0L6_2atmpS2874;
    void** _M0L8_2afieldS2873 = _M0L4selfS996->$0;
    void** _M0L3bufS2536 = _M0L8_2afieldS2873;
    void* _M0L6_2aoldS2872;
    if (
      _M0L5indexS997 < 0
      || _M0L5indexS997 >= Moonbit_array_length(_M0L3bufS2536)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS2872 = (void*)_M0L3bufS2536[_M0L5indexS997];
    moonbit_incref(_M0L1vS998);
    moonbit_decref(_M0L6_2aoldS2872);
    if (
      _M0L5indexS997 < 0
      || _M0L5indexS997 >= Moonbit_array_length(_M0L3bufS2536)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2536[_M0L5indexS997]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS996->$1 = _M0L5indexS997;
    moonbit_decref(_M0L4selfS996);
    return _M0L1vS998;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS993,
  struct _M0TPB6Logger _M0L6loggerS994
) {
  moonbit_string_t _M0L6_2atmpS2535;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2534;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2535 = _M0L4selfS993;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2534 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2535);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2534, _M0L6loggerS994);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS970,
  struct _M0TPB6Logger _M0L6loggerS992
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2884;
  struct _M0TPC16string10StringView _M0L3pkgS969;
  moonbit_string_t _M0L7_2adataS971;
  int32_t _M0L8_2astartS972;
  int32_t _M0L6_2atmpS2533;
  int32_t _M0L6_2aendS973;
  int32_t _M0Lm9_2acursorS974;
  int32_t _M0Lm13accept__stateS975;
  int32_t _M0Lm10match__endS976;
  int32_t _M0Lm20match__tag__saver__0S977;
  int32_t _M0Lm6tag__0S978;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS979;
  struct _M0TPC16string10StringView _M0L8_2afieldS2883;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS988;
  void* _M0L8_2afieldS2882;
  int32_t _M0L6_2acntS3111;
  void* _M0L16_2apackage__nameS989;
  struct _M0TPC16string10StringView _M0L8_2afieldS2880;
  struct _M0TPC16string10StringView _M0L8filenameS2510;
  struct _M0TPC16string10StringView _M0L8_2afieldS2879;
  struct _M0TPC16string10StringView _M0L11start__lineS2511;
  struct _M0TPC16string10StringView _M0L8_2afieldS2878;
  struct _M0TPC16string10StringView _M0L13start__columnS2512;
  struct _M0TPC16string10StringView _M0L8_2afieldS2877;
  struct _M0TPC16string10StringView _M0L9end__lineS2513;
  struct _M0TPC16string10StringView _M0L8_2afieldS2876;
  int32_t _M0L6_2acntS3115;
  struct _M0TPC16string10StringView _M0L11end__columnS2514;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS2884
  = (struct _M0TPC16string10StringView){
    _M0L4selfS970->$0_1, _M0L4selfS970->$0_2, _M0L4selfS970->$0_0
  };
  _M0L3pkgS969 = _M0L8_2afieldS2884;
  moonbit_incref(_M0L3pkgS969.$0);
  moonbit_incref(_M0L3pkgS969.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS971 = _M0MPC16string10StringView4data(_M0L3pkgS969);
  moonbit_incref(_M0L3pkgS969.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS972 = _M0MPC16string10StringView13start__offset(_M0L3pkgS969);
  moonbit_incref(_M0L3pkgS969.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2533 = _M0MPC16string10StringView6length(_M0L3pkgS969);
  _M0L6_2aendS973 = _M0L8_2astartS972 + _M0L6_2atmpS2533;
  _M0Lm9_2acursorS974 = _M0L8_2astartS972;
  _M0Lm13accept__stateS975 = -1;
  _M0Lm10match__endS976 = -1;
  _M0Lm20match__tag__saver__0S977 = -1;
  _M0Lm6tag__0S978 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2525 = _M0Lm9_2acursorS974;
    if (_M0L6_2atmpS2525 < _M0L6_2aendS973) {
      int32_t _M0L6_2atmpS2532 = _M0Lm9_2acursorS974;
      int32_t _M0L10next__charS983;
      int32_t _M0L6_2atmpS2526;
      moonbit_incref(_M0L7_2adataS971);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS983
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS971, _M0L6_2atmpS2532);
      _M0L6_2atmpS2526 = _M0Lm9_2acursorS974;
      _M0Lm9_2acursorS974 = _M0L6_2atmpS2526 + 1;
      if (_M0L10next__charS983 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2527;
          _M0Lm6tag__0S978 = _M0Lm9_2acursorS974;
          _M0L6_2atmpS2527 = _M0Lm9_2acursorS974;
          if (_M0L6_2atmpS2527 < _M0L6_2aendS973) {
            int32_t _M0L6_2atmpS2531 = _M0Lm9_2acursorS974;
            int32_t _M0L10next__charS984;
            int32_t _M0L6_2atmpS2528;
            moonbit_incref(_M0L7_2adataS971);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS984
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS971, _M0L6_2atmpS2531);
            _M0L6_2atmpS2528 = _M0Lm9_2acursorS974;
            _M0Lm9_2acursorS974 = _M0L6_2atmpS2528 + 1;
            if (_M0L10next__charS984 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2529 = _M0Lm9_2acursorS974;
                if (_M0L6_2atmpS2529 < _M0L6_2aendS973) {
                  int32_t _M0L6_2atmpS2530 = _M0Lm9_2acursorS974;
                  _M0Lm9_2acursorS974 = _M0L6_2atmpS2530 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S977 = _M0Lm6tag__0S978;
                  _M0Lm13accept__stateS975 = 0;
                  _M0Lm10match__endS976 = _M0Lm9_2acursorS974;
                  goto join_980;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_980;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_980;
    }
    break;
  }
  goto joinlet_3210;
  join_980:;
  switch (_M0Lm13accept__stateS975) {
    case 0: {
      int32_t _M0L6_2atmpS2523;
      int32_t _M0L6_2atmpS2522;
      int64_t _M0L6_2atmpS2519;
      int32_t _M0L6_2atmpS2521;
      int64_t _M0L6_2atmpS2520;
      struct _M0TPC16string10StringView _M0L13package__nameS981;
      int64_t _M0L6_2atmpS2516;
      int32_t _M0L6_2atmpS2518;
      int64_t _M0L6_2atmpS2517;
      struct _M0TPC16string10StringView _M0L12module__nameS982;
      void* _M0L4SomeS2515;
      moonbit_decref(_M0L3pkgS969.$0);
      _M0L6_2atmpS2523 = _M0Lm20match__tag__saver__0S977;
      _M0L6_2atmpS2522 = _M0L6_2atmpS2523 + 1;
      _M0L6_2atmpS2519 = (int64_t)_M0L6_2atmpS2522;
      _M0L6_2atmpS2521 = _M0Lm10match__endS976;
      _M0L6_2atmpS2520 = (int64_t)_M0L6_2atmpS2521;
      moonbit_incref(_M0L7_2adataS971);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS981
      = _M0MPC16string6String4view(_M0L7_2adataS971, _M0L6_2atmpS2519, _M0L6_2atmpS2520);
      _M0L6_2atmpS2516 = (int64_t)_M0L8_2astartS972;
      _M0L6_2atmpS2518 = _M0Lm20match__tag__saver__0S977;
      _M0L6_2atmpS2517 = (int64_t)_M0L6_2atmpS2518;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS982
      = _M0MPC16string6String4view(_M0L7_2adataS971, _M0L6_2atmpS2516, _M0L6_2atmpS2517);
      _M0L4SomeS2515
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2515)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2515)->$0_0
      = _M0L13package__nameS981.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2515)->$0_1
      = _M0L13package__nameS981.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2515)->$0_2
      = _M0L13package__nameS981.$2;
      _M0L7_2abindS979
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS979)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS979->$0_0 = _M0L12module__nameS982.$0;
      _M0L7_2abindS979->$0_1 = _M0L12module__nameS982.$1;
      _M0L7_2abindS979->$0_2 = _M0L12module__nameS982.$2;
      _M0L7_2abindS979->$1 = _M0L4SomeS2515;
      break;
    }
    default: {
      void* _M0L4NoneS2524;
      moonbit_decref(_M0L7_2adataS971);
      _M0L4NoneS2524
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS979
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS979)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS979->$0_0 = _M0L3pkgS969.$0;
      _M0L7_2abindS979->$0_1 = _M0L3pkgS969.$1;
      _M0L7_2abindS979->$0_2 = _M0L3pkgS969.$2;
      _M0L7_2abindS979->$1 = _M0L4NoneS2524;
      break;
    }
  }
  joinlet_3210:;
  _M0L8_2afieldS2883
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS979->$0_1, _M0L7_2abindS979->$0_2, _M0L7_2abindS979->$0_0
  };
  _M0L15_2amodule__nameS988 = _M0L8_2afieldS2883;
  _M0L8_2afieldS2882 = _M0L7_2abindS979->$1;
  _M0L6_2acntS3111 = Moonbit_object_header(_M0L7_2abindS979)->rc;
  if (_M0L6_2acntS3111 > 1) {
    int32_t _M0L11_2anew__cntS3112 = _M0L6_2acntS3111 - 1;
    Moonbit_object_header(_M0L7_2abindS979)->rc = _M0L11_2anew__cntS3112;
    moonbit_incref(_M0L8_2afieldS2882);
    moonbit_incref(_M0L15_2amodule__nameS988.$0);
  } else if (_M0L6_2acntS3111 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS979);
  }
  _M0L16_2apackage__nameS989 = _M0L8_2afieldS2882;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS989)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS990 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS989;
      struct _M0TPC16string10StringView _M0L8_2afieldS2881 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS990->$0_1,
                                              _M0L7_2aSomeS990->$0_2,
                                              _M0L7_2aSomeS990->$0_0};
      int32_t _M0L6_2acntS3113 = Moonbit_object_header(_M0L7_2aSomeS990)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS991;
      if (_M0L6_2acntS3113 > 1) {
        int32_t _M0L11_2anew__cntS3114 = _M0L6_2acntS3113 - 1;
        Moonbit_object_header(_M0L7_2aSomeS990)->rc = _M0L11_2anew__cntS3114;
        moonbit_incref(_M0L8_2afieldS2881.$0);
      } else if (_M0L6_2acntS3113 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS990);
      }
      _M0L12_2apkg__nameS991 = _M0L8_2afieldS2881;
      if (_M0L6loggerS992.$1) {
        moonbit_incref(_M0L6loggerS992.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS992.$0->$method_2(_M0L6loggerS992.$1, _M0L12_2apkg__nameS991);
      if (_M0L6loggerS992.$1) {
        moonbit_incref(_M0L6loggerS992.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS992.$0->$method_3(_M0L6loggerS992.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS989);
      break;
    }
  }
  _M0L8_2afieldS2880
  = (struct _M0TPC16string10StringView){
    _M0L4selfS970->$1_1, _M0L4selfS970->$1_2, _M0L4selfS970->$1_0
  };
  _M0L8filenameS2510 = _M0L8_2afieldS2880;
  moonbit_incref(_M0L8filenameS2510.$0);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_2(_M0L6loggerS992.$1, _M0L8filenameS2510);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_3(_M0L6loggerS992.$1, 58);
  _M0L8_2afieldS2879
  = (struct _M0TPC16string10StringView){
    _M0L4selfS970->$2_1, _M0L4selfS970->$2_2, _M0L4selfS970->$2_0
  };
  _M0L11start__lineS2511 = _M0L8_2afieldS2879;
  moonbit_incref(_M0L11start__lineS2511.$0);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_2(_M0L6loggerS992.$1, _M0L11start__lineS2511);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_3(_M0L6loggerS992.$1, 58);
  _M0L8_2afieldS2878
  = (struct _M0TPC16string10StringView){
    _M0L4selfS970->$3_1, _M0L4selfS970->$3_2, _M0L4selfS970->$3_0
  };
  _M0L13start__columnS2512 = _M0L8_2afieldS2878;
  moonbit_incref(_M0L13start__columnS2512.$0);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_2(_M0L6loggerS992.$1, _M0L13start__columnS2512);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_3(_M0L6loggerS992.$1, 45);
  _M0L8_2afieldS2877
  = (struct _M0TPC16string10StringView){
    _M0L4selfS970->$4_1, _M0L4selfS970->$4_2, _M0L4selfS970->$4_0
  };
  _M0L9end__lineS2513 = _M0L8_2afieldS2877;
  moonbit_incref(_M0L9end__lineS2513.$0);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_2(_M0L6loggerS992.$1, _M0L9end__lineS2513);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_3(_M0L6loggerS992.$1, 58);
  _M0L8_2afieldS2876
  = (struct _M0TPC16string10StringView){
    _M0L4selfS970->$5_1, _M0L4selfS970->$5_2, _M0L4selfS970->$5_0
  };
  _M0L6_2acntS3115 = Moonbit_object_header(_M0L4selfS970)->rc;
  if (_M0L6_2acntS3115 > 1) {
    int32_t _M0L11_2anew__cntS3121 = _M0L6_2acntS3115 - 1;
    Moonbit_object_header(_M0L4selfS970)->rc = _M0L11_2anew__cntS3121;
    moonbit_incref(_M0L8_2afieldS2876.$0);
  } else if (_M0L6_2acntS3115 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3120 =
      (struct _M0TPC16string10StringView){_M0L4selfS970->$4_1,
                                            _M0L4selfS970->$4_2,
                                            _M0L4selfS970->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3119;
    struct _M0TPC16string10StringView _M0L8_2afieldS3118;
    struct _M0TPC16string10StringView _M0L8_2afieldS3117;
    struct _M0TPC16string10StringView _M0L8_2afieldS3116;
    moonbit_decref(_M0L8_2afieldS3120.$0);
    _M0L8_2afieldS3119
    = (struct _M0TPC16string10StringView){
      _M0L4selfS970->$3_1, _M0L4selfS970->$3_2, _M0L4selfS970->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3119.$0);
    _M0L8_2afieldS3118
    = (struct _M0TPC16string10StringView){
      _M0L4selfS970->$2_1, _M0L4selfS970->$2_2, _M0L4selfS970->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3118.$0);
    _M0L8_2afieldS3117
    = (struct _M0TPC16string10StringView){
      _M0L4selfS970->$1_1, _M0L4selfS970->$1_2, _M0L4selfS970->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3117.$0);
    _M0L8_2afieldS3116
    = (struct _M0TPC16string10StringView){
      _M0L4selfS970->$0_1, _M0L4selfS970->$0_2, _M0L4selfS970->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3116.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS970);
  }
  _M0L11end__columnS2514 = _M0L8_2afieldS2876;
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_2(_M0L6loggerS992.$1, _M0L11end__columnS2514);
  if (_M0L6loggerS992.$1) {
    moonbit_incref(_M0L6loggerS992.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_3(_M0L6loggerS992.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS992.$0->$method_2(_M0L6loggerS992.$1, _M0L15_2amodule__nameS988);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS968) {
  moonbit_string_t _M0L6_2atmpS2509;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2509 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS968);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2509);
  moonbit_decref(_M0L6_2atmpS2509);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS967,
  struct _M0TPB6Logger _M0L6loggerS966
) {
  moonbit_string_t _M0L6_2atmpS2508;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2508 = _M0MPC16double6Double10to__string(_M0L4selfS967);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS966.$0->$method_0(_M0L6loggerS966.$1, _M0L6_2atmpS2508);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS965) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS965);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS952) {
  uint64_t _M0L4bitsS953;
  uint64_t _M0L6_2atmpS2507;
  uint64_t _M0L6_2atmpS2506;
  int32_t _M0L8ieeeSignS954;
  uint64_t _M0L12ieeeMantissaS955;
  uint64_t _M0L6_2atmpS2505;
  uint64_t _M0L6_2atmpS2504;
  int32_t _M0L12ieeeExponentS956;
  int32_t _if__result_3214;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS957;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS958;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2503;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS952 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_65.data;
  }
  _M0L4bitsS953 = *(int64_t*)&_M0L3valS952;
  _M0L6_2atmpS2507 = _M0L4bitsS953 >> 63;
  _M0L6_2atmpS2506 = _M0L6_2atmpS2507 & 1ull;
  _M0L8ieeeSignS954 = _M0L6_2atmpS2506 != 0ull;
  _M0L12ieeeMantissaS955 = _M0L4bitsS953 & 4503599627370495ull;
  _M0L6_2atmpS2505 = _M0L4bitsS953 >> 52;
  _M0L6_2atmpS2504 = _M0L6_2atmpS2505 & 2047ull;
  _M0L12ieeeExponentS956 = (int32_t)_M0L6_2atmpS2504;
  if (_M0L12ieeeExponentS956 == 2047) {
    _if__result_3214 = 1;
  } else if (_M0L12ieeeExponentS956 == 0) {
    _if__result_3214 = _M0L12ieeeMantissaS955 == 0ull;
  } else {
    _if__result_3214 = 0;
  }
  if (_if__result_3214) {
    int32_t _M0L6_2atmpS2492 = _M0L12ieeeExponentS956 != 0;
    int32_t _M0L6_2atmpS2493 = _M0L12ieeeMantissaS955 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS954, _M0L6_2atmpS2492, _M0L6_2atmpS2493);
  }
  _M0Lm1vS957 = _M0FPB30ryu__to__string_2erecord_2f951;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS958
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS955, _M0L12ieeeExponentS956);
  if (_M0L5smallS958 == 0) {
    uint32_t _M0L6_2atmpS2494;
    if (_M0L5smallS958) {
      moonbit_decref(_M0L5smallS958);
    }
    _M0L6_2atmpS2494 = *(uint32_t*)&_M0L12ieeeExponentS956;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS957 = _M0FPB3d2d(_M0L12ieeeMantissaS955, _M0L6_2atmpS2494);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS959 = _M0L5smallS958;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS960 = _M0L7_2aSomeS959;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS961 = _M0L4_2afS960;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2502 = _M0Lm1xS961;
      uint64_t _M0L8_2afieldS2887 = _M0L6_2atmpS2502->$0;
      uint64_t _M0L8mantissaS2501 = _M0L8_2afieldS2887;
      uint64_t _M0L1qS962 = _M0L8mantissaS2501 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2500 = _M0Lm1xS961;
      uint64_t _M0L8_2afieldS2886 = _M0L6_2atmpS2500->$0;
      uint64_t _M0L8mantissaS2498 = _M0L8_2afieldS2886;
      uint64_t _M0L6_2atmpS2499 = 10ull * _M0L1qS962;
      uint64_t _M0L1rS963 = _M0L8mantissaS2498 - _M0L6_2atmpS2499;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2497;
      int32_t _M0L8_2afieldS2885;
      int32_t _M0L8exponentS2496;
      int32_t _M0L6_2atmpS2495;
      if (_M0L1rS963 != 0ull) {
        break;
      }
      _M0L6_2atmpS2497 = _M0Lm1xS961;
      _M0L8_2afieldS2885 = _M0L6_2atmpS2497->$1;
      moonbit_decref(_M0L6_2atmpS2497);
      _M0L8exponentS2496 = _M0L8_2afieldS2885;
      _M0L6_2atmpS2495 = _M0L8exponentS2496 + 1;
      _M0Lm1xS961
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS961)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS961->$0 = _M0L1qS962;
      _M0Lm1xS961->$1 = _M0L6_2atmpS2495;
      continue;
      break;
    }
    _M0Lm1vS957 = _M0Lm1xS961;
  }
  _M0L6_2atmpS2503 = _M0Lm1vS957;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2503, _M0L8ieeeSignS954);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS946,
  int32_t _M0L12ieeeExponentS948
) {
  uint64_t _M0L2m2S945;
  int32_t _M0L6_2atmpS2491;
  int32_t _M0L2e2S947;
  int32_t _M0L6_2atmpS2490;
  uint64_t _M0L6_2atmpS2489;
  uint64_t _M0L4maskS949;
  uint64_t _M0L8fractionS950;
  int32_t _M0L6_2atmpS2488;
  uint64_t _M0L6_2atmpS2487;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2486;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S945 = 4503599627370496ull | _M0L12ieeeMantissaS946;
  _M0L6_2atmpS2491 = _M0L12ieeeExponentS948 - 1023;
  _M0L2e2S947 = _M0L6_2atmpS2491 - 52;
  if (_M0L2e2S947 > 0) {
    return 0;
  }
  if (_M0L2e2S947 < -52) {
    return 0;
  }
  _M0L6_2atmpS2490 = -_M0L2e2S947;
  _M0L6_2atmpS2489 = 1ull << (_M0L6_2atmpS2490 & 63);
  _M0L4maskS949 = _M0L6_2atmpS2489 - 1ull;
  _M0L8fractionS950 = _M0L2m2S945 & _M0L4maskS949;
  if (_M0L8fractionS950 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2488 = -_M0L2e2S947;
  _M0L6_2atmpS2487 = _M0L2m2S945 >> (_M0L6_2atmpS2488 & 63);
  _M0L6_2atmpS2486
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2486)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2486->$0 = _M0L6_2atmpS2487;
  _M0L6_2atmpS2486->$1 = 0;
  return _M0L6_2atmpS2486;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS919,
  int32_t _M0L4signS917
) {
  int32_t _M0L6_2atmpS2485;
  moonbit_bytes_t _M0L6resultS915;
  int32_t _M0Lm5indexS916;
  uint64_t _M0Lm6outputS918;
  uint64_t _M0L6_2atmpS2484;
  int32_t _M0L7olengthS920;
  int32_t _M0L8_2afieldS2888;
  int32_t _M0L8exponentS2483;
  int32_t _M0L6_2atmpS2482;
  int32_t _M0Lm3expS921;
  int32_t _M0L6_2atmpS2481;
  int32_t _M0L6_2atmpS2479;
  int32_t _M0L18scientificNotationS922;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2485 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS915 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2485);
  _M0Lm5indexS916 = 0;
  if (_M0L4signS917) {
    int32_t _M0L6_2atmpS2354 = _M0Lm5indexS916;
    int32_t _M0L6_2atmpS2355;
    if (
      _M0L6_2atmpS2354 < 0
      || _M0L6_2atmpS2354 >= Moonbit_array_length(_M0L6resultS915)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS915[_M0L6_2atmpS2354] = 45;
    _M0L6_2atmpS2355 = _M0Lm5indexS916;
    _M0Lm5indexS916 = _M0L6_2atmpS2355 + 1;
  }
  _M0Lm6outputS918 = _M0L1vS919->$0;
  _M0L6_2atmpS2484 = _M0Lm6outputS918;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS920 = _M0FPB17decimal__length17(_M0L6_2atmpS2484);
  _M0L8_2afieldS2888 = _M0L1vS919->$1;
  moonbit_decref(_M0L1vS919);
  _M0L8exponentS2483 = _M0L8_2afieldS2888;
  _M0L6_2atmpS2482 = _M0L8exponentS2483 + _M0L7olengthS920;
  _M0Lm3expS921 = _M0L6_2atmpS2482 - 1;
  _M0L6_2atmpS2481 = _M0Lm3expS921;
  if (_M0L6_2atmpS2481 >= -6) {
    int32_t _M0L6_2atmpS2480 = _M0Lm3expS921;
    _M0L6_2atmpS2479 = _M0L6_2atmpS2480 < 21;
  } else {
    _M0L6_2atmpS2479 = 0;
  }
  _M0L18scientificNotationS922 = !_M0L6_2atmpS2479;
  if (_M0L18scientificNotationS922) {
    int32_t _M0L7_2abindS923 = _M0L7olengthS920 - 1;
    int32_t _M0L1iS924 = 0;
    int32_t _M0L6_2atmpS2365;
    uint64_t _M0L6_2atmpS2370;
    int32_t _M0L6_2atmpS2369;
    int32_t _M0L6_2atmpS2368;
    int32_t _M0L6_2atmpS2367;
    int32_t _M0L6_2atmpS2366;
    int32_t _M0L6_2atmpS2374;
    int32_t _M0L6_2atmpS2375;
    int32_t _M0L6_2atmpS2376;
    int32_t _M0L6_2atmpS2377;
    int32_t _M0L6_2atmpS2378;
    int32_t _M0L6_2atmpS2384;
    int32_t _M0L6_2atmpS2417;
    while (1) {
      if (_M0L1iS924 < _M0L7_2abindS923) {
        uint64_t _M0L6_2atmpS2363 = _M0Lm6outputS918;
        uint64_t _M0L1cS925 = _M0L6_2atmpS2363 % 10ull;
        uint64_t _M0L6_2atmpS2356 = _M0Lm6outputS918;
        int32_t _M0L6_2atmpS2362;
        int32_t _M0L6_2atmpS2361;
        int32_t _M0L6_2atmpS2357;
        int32_t _M0L6_2atmpS2360;
        int32_t _M0L6_2atmpS2359;
        int32_t _M0L6_2atmpS2358;
        int32_t _M0L6_2atmpS2364;
        _M0Lm6outputS918 = _M0L6_2atmpS2356 / 10ull;
        _M0L6_2atmpS2362 = _M0Lm5indexS916;
        _M0L6_2atmpS2361 = _M0L6_2atmpS2362 + _M0L7olengthS920;
        _M0L6_2atmpS2357 = _M0L6_2atmpS2361 - _M0L1iS924;
        _M0L6_2atmpS2360 = (int32_t)_M0L1cS925;
        _M0L6_2atmpS2359 = 48 + _M0L6_2atmpS2360;
        _M0L6_2atmpS2358 = _M0L6_2atmpS2359 & 0xff;
        if (
          _M0L6_2atmpS2357 < 0
          || _M0L6_2atmpS2357 >= Moonbit_array_length(_M0L6resultS915)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS915[_M0L6_2atmpS2357] = _M0L6_2atmpS2358;
        _M0L6_2atmpS2364 = _M0L1iS924 + 1;
        _M0L1iS924 = _M0L6_2atmpS2364;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2365 = _M0Lm5indexS916;
    _M0L6_2atmpS2370 = _M0Lm6outputS918;
    _M0L6_2atmpS2369 = (int32_t)_M0L6_2atmpS2370;
    _M0L6_2atmpS2368 = _M0L6_2atmpS2369 % 10;
    _M0L6_2atmpS2367 = 48 + _M0L6_2atmpS2368;
    _M0L6_2atmpS2366 = _M0L6_2atmpS2367 & 0xff;
    if (
      _M0L6_2atmpS2365 < 0
      || _M0L6_2atmpS2365 >= Moonbit_array_length(_M0L6resultS915)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS915[_M0L6_2atmpS2365] = _M0L6_2atmpS2366;
    if (_M0L7olengthS920 > 1) {
      int32_t _M0L6_2atmpS2372 = _M0Lm5indexS916;
      int32_t _M0L6_2atmpS2371 = _M0L6_2atmpS2372 + 1;
      if (
        _M0L6_2atmpS2371 < 0
        || _M0L6_2atmpS2371 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2371] = 46;
    } else {
      int32_t _M0L6_2atmpS2373 = _M0Lm5indexS916;
      _M0Lm5indexS916 = _M0L6_2atmpS2373 - 1;
    }
    _M0L6_2atmpS2374 = _M0Lm5indexS916;
    _M0L6_2atmpS2375 = _M0L7olengthS920 + 1;
    _M0Lm5indexS916 = _M0L6_2atmpS2374 + _M0L6_2atmpS2375;
    _M0L6_2atmpS2376 = _M0Lm5indexS916;
    if (
      _M0L6_2atmpS2376 < 0
      || _M0L6_2atmpS2376 >= Moonbit_array_length(_M0L6resultS915)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS915[_M0L6_2atmpS2376] = 101;
    _M0L6_2atmpS2377 = _M0Lm5indexS916;
    _M0Lm5indexS916 = _M0L6_2atmpS2377 + 1;
    _M0L6_2atmpS2378 = _M0Lm3expS921;
    if (_M0L6_2atmpS2378 < 0) {
      int32_t _M0L6_2atmpS2379 = _M0Lm5indexS916;
      int32_t _M0L6_2atmpS2380;
      int32_t _M0L6_2atmpS2381;
      if (
        _M0L6_2atmpS2379 < 0
        || _M0L6_2atmpS2379 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2379] = 45;
      _M0L6_2atmpS2380 = _M0Lm5indexS916;
      _M0Lm5indexS916 = _M0L6_2atmpS2380 + 1;
      _M0L6_2atmpS2381 = _M0Lm3expS921;
      _M0Lm3expS921 = -_M0L6_2atmpS2381;
    } else {
      int32_t _M0L6_2atmpS2382 = _M0Lm5indexS916;
      int32_t _M0L6_2atmpS2383;
      if (
        _M0L6_2atmpS2382 < 0
        || _M0L6_2atmpS2382 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2382] = 43;
      _M0L6_2atmpS2383 = _M0Lm5indexS916;
      _M0Lm5indexS916 = _M0L6_2atmpS2383 + 1;
    }
    _M0L6_2atmpS2384 = _M0Lm3expS921;
    if (_M0L6_2atmpS2384 >= 100) {
      int32_t _M0L6_2atmpS2400 = _M0Lm3expS921;
      int32_t _M0L1aS927 = _M0L6_2atmpS2400 / 100;
      int32_t _M0L6_2atmpS2399 = _M0Lm3expS921;
      int32_t _M0L6_2atmpS2398 = _M0L6_2atmpS2399 / 10;
      int32_t _M0L1bS928 = _M0L6_2atmpS2398 % 10;
      int32_t _M0L6_2atmpS2397 = _M0Lm3expS921;
      int32_t _M0L1cS929 = _M0L6_2atmpS2397 % 10;
      int32_t _M0L6_2atmpS2385 = _M0Lm5indexS916;
      int32_t _M0L6_2atmpS2387 = 48 + _M0L1aS927;
      int32_t _M0L6_2atmpS2386 = _M0L6_2atmpS2387 & 0xff;
      int32_t _M0L6_2atmpS2391;
      int32_t _M0L6_2atmpS2388;
      int32_t _M0L6_2atmpS2390;
      int32_t _M0L6_2atmpS2389;
      int32_t _M0L6_2atmpS2395;
      int32_t _M0L6_2atmpS2392;
      int32_t _M0L6_2atmpS2394;
      int32_t _M0L6_2atmpS2393;
      int32_t _M0L6_2atmpS2396;
      if (
        _M0L6_2atmpS2385 < 0
        || _M0L6_2atmpS2385 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2385] = _M0L6_2atmpS2386;
      _M0L6_2atmpS2391 = _M0Lm5indexS916;
      _M0L6_2atmpS2388 = _M0L6_2atmpS2391 + 1;
      _M0L6_2atmpS2390 = 48 + _M0L1bS928;
      _M0L6_2atmpS2389 = _M0L6_2atmpS2390 & 0xff;
      if (
        _M0L6_2atmpS2388 < 0
        || _M0L6_2atmpS2388 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2388] = _M0L6_2atmpS2389;
      _M0L6_2atmpS2395 = _M0Lm5indexS916;
      _M0L6_2atmpS2392 = _M0L6_2atmpS2395 + 2;
      _M0L6_2atmpS2394 = 48 + _M0L1cS929;
      _M0L6_2atmpS2393 = _M0L6_2atmpS2394 & 0xff;
      if (
        _M0L6_2atmpS2392 < 0
        || _M0L6_2atmpS2392 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2392] = _M0L6_2atmpS2393;
      _M0L6_2atmpS2396 = _M0Lm5indexS916;
      _M0Lm5indexS916 = _M0L6_2atmpS2396 + 3;
    } else {
      int32_t _M0L6_2atmpS2401 = _M0Lm3expS921;
      if (_M0L6_2atmpS2401 >= 10) {
        int32_t _M0L6_2atmpS2411 = _M0Lm3expS921;
        int32_t _M0L1aS930 = _M0L6_2atmpS2411 / 10;
        int32_t _M0L6_2atmpS2410 = _M0Lm3expS921;
        int32_t _M0L1bS931 = _M0L6_2atmpS2410 % 10;
        int32_t _M0L6_2atmpS2402 = _M0Lm5indexS916;
        int32_t _M0L6_2atmpS2404 = 48 + _M0L1aS930;
        int32_t _M0L6_2atmpS2403 = _M0L6_2atmpS2404 & 0xff;
        int32_t _M0L6_2atmpS2408;
        int32_t _M0L6_2atmpS2405;
        int32_t _M0L6_2atmpS2407;
        int32_t _M0L6_2atmpS2406;
        int32_t _M0L6_2atmpS2409;
        if (
          _M0L6_2atmpS2402 < 0
          || _M0L6_2atmpS2402 >= Moonbit_array_length(_M0L6resultS915)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS915[_M0L6_2atmpS2402] = _M0L6_2atmpS2403;
        _M0L6_2atmpS2408 = _M0Lm5indexS916;
        _M0L6_2atmpS2405 = _M0L6_2atmpS2408 + 1;
        _M0L6_2atmpS2407 = 48 + _M0L1bS931;
        _M0L6_2atmpS2406 = _M0L6_2atmpS2407 & 0xff;
        if (
          _M0L6_2atmpS2405 < 0
          || _M0L6_2atmpS2405 >= Moonbit_array_length(_M0L6resultS915)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS915[_M0L6_2atmpS2405] = _M0L6_2atmpS2406;
        _M0L6_2atmpS2409 = _M0Lm5indexS916;
        _M0Lm5indexS916 = _M0L6_2atmpS2409 + 2;
      } else {
        int32_t _M0L6_2atmpS2412 = _M0Lm5indexS916;
        int32_t _M0L6_2atmpS2415 = _M0Lm3expS921;
        int32_t _M0L6_2atmpS2414 = 48 + _M0L6_2atmpS2415;
        int32_t _M0L6_2atmpS2413 = _M0L6_2atmpS2414 & 0xff;
        int32_t _M0L6_2atmpS2416;
        if (
          _M0L6_2atmpS2412 < 0
          || _M0L6_2atmpS2412 >= Moonbit_array_length(_M0L6resultS915)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS915[_M0L6_2atmpS2412] = _M0L6_2atmpS2413;
        _M0L6_2atmpS2416 = _M0Lm5indexS916;
        _M0Lm5indexS916 = _M0L6_2atmpS2416 + 1;
      }
    }
    _M0L6_2atmpS2417 = _M0Lm5indexS916;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS915, 0, _M0L6_2atmpS2417);
  } else {
    int32_t _M0L6_2atmpS2418 = _M0Lm3expS921;
    int32_t _M0L6_2atmpS2478;
    if (_M0L6_2atmpS2418 < 0) {
      int32_t _M0L6_2atmpS2419 = _M0Lm5indexS916;
      int32_t _M0L6_2atmpS2420;
      int32_t _M0L6_2atmpS2421;
      int32_t _M0L6_2atmpS2422;
      int32_t _M0L1iS932;
      int32_t _M0L7currentS934;
      int32_t _M0L1iS935;
      if (
        _M0L6_2atmpS2419 < 0
        || _M0L6_2atmpS2419 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2419] = 48;
      _M0L6_2atmpS2420 = _M0Lm5indexS916;
      _M0Lm5indexS916 = _M0L6_2atmpS2420 + 1;
      _M0L6_2atmpS2421 = _M0Lm5indexS916;
      if (
        _M0L6_2atmpS2421 < 0
        || _M0L6_2atmpS2421 >= Moonbit_array_length(_M0L6resultS915)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS915[_M0L6_2atmpS2421] = 46;
      _M0L6_2atmpS2422 = _M0Lm5indexS916;
      _M0Lm5indexS916 = _M0L6_2atmpS2422 + 1;
      _M0L1iS932 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2423 = _M0Lm3expS921;
        if (_M0L1iS932 > _M0L6_2atmpS2423) {
          int32_t _M0L6_2atmpS2424 = _M0Lm5indexS916;
          int32_t _M0L6_2atmpS2425;
          int32_t _M0L6_2atmpS2426;
          if (
            _M0L6_2atmpS2424 < 0
            || _M0L6_2atmpS2424 >= Moonbit_array_length(_M0L6resultS915)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS915[_M0L6_2atmpS2424] = 48;
          _M0L6_2atmpS2425 = _M0Lm5indexS916;
          _M0Lm5indexS916 = _M0L6_2atmpS2425 + 1;
          _M0L6_2atmpS2426 = _M0L1iS932 - 1;
          _M0L1iS932 = _M0L6_2atmpS2426;
          continue;
        }
        break;
      }
      _M0L7currentS934 = _M0Lm5indexS916;
      _M0L1iS935 = 0;
      while (1) {
        if (_M0L1iS935 < _M0L7olengthS920) {
          int32_t _M0L6_2atmpS2434 = _M0L7currentS934 + _M0L7olengthS920;
          int32_t _M0L6_2atmpS2433 = _M0L6_2atmpS2434 - _M0L1iS935;
          int32_t _M0L6_2atmpS2427 = _M0L6_2atmpS2433 - 1;
          uint64_t _M0L6_2atmpS2432 = _M0Lm6outputS918;
          uint64_t _M0L6_2atmpS2431 = _M0L6_2atmpS2432 % 10ull;
          int32_t _M0L6_2atmpS2430 = (int32_t)_M0L6_2atmpS2431;
          int32_t _M0L6_2atmpS2429 = 48 + _M0L6_2atmpS2430;
          int32_t _M0L6_2atmpS2428 = _M0L6_2atmpS2429 & 0xff;
          uint64_t _M0L6_2atmpS2435;
          int32_t _M0L6_2atmpS2436;
          int32_t _M0L6_2atmpS2437;
          if (
            _M0L6_2atmpS2427 < 0
            || _M0L6_2atmpS2427 >= Moonbit_array_length(_M0L6resultS915)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS915[_M0L6_2atmpS2427] = _M0L6_2atmpS2428;
          _M0L6_2atmpS2435 = _M0Lm6outputS918;
          _M0Lm6outputS918 = _M0L6_2atmpS2435 / 10ull;
          _M0L6_2atmpS2436 = _M0Lm5indexS916;
          _M0Lm5indexS916 = _M0L6_2atmpS2436 + 1;
          _M0L6_2atmpS2437 = _M0L1iS935 + 1;
          _M0L1iS935 = _M0L6_2atmpS2437;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2439 = _M0Lm3expS921;
      int32_t _M0L6_2atmpS2438 = _M0L6_2atmpS2439 + 1;
      if (_M0L6_2atmpS2438 >= _M0L7olengthS920) {
        int32_t _M0L1iS937 = 0;
        int32_t _M0L6_2atmpS2451;
        int32_t _M0L6_2atmpS2455;
        int32_t _M0L7_2abindS939;
        int32_t _M0L2__S940;
        while (1) {
          if (_M0L1iS937 < _M0L7olengthS920) {
            int32_t _M0L6_2atmpS2448 = _M0Lm5indexS916;
            int32_t _M0L6_2atmpS2447 = _M0L6_2atmpS2448 + _M0L7olengthS920;
            int32_t _M0L6_2atmpS2446 = _M0L6_2atmpS2447 - _M0L1iS937;
            int32_t _M0L6_2atmpS2440 = _M0L6_2atmpS2446 - 1;
            uint64_t _M0L6_2atmpS2445 = _M0Lm6outputS918;
            uint64_t _M0L6_2atmpS2444 = _M0L6_2atmpS2445 % 10ull;
            int32_t _M0L6_2atmpS2443 = (int32_t)_M0L6_2atmpS2444;
            int32_t _M0L6_2atmpS2442 = 48 + _M0L6_2atmpS2443;
            int32_t _M0L6_2atmpS2441 = _M0L6_2atmpS2442 & 0xff;
            uint64_t _M0L6_2atmpS2449;
            int32_t _M0L6_2atmpS2450;
            if (
              _M0L6_2atmpS2440 < 0
              || _M0L6_2atmpS2440 >= Moonbit_array_length(_M0L6resultS915)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS915[_M0L6_2atmpS2440] = _M0L6_2atmpS2441;
            _M0L6_2atmpS2449 = _M0Lm6outputS918;
            _M0Lm6outputS918 = _M0L6_2atmpS2449 / 10ull;
            _M0L6_2atmpS2450 = _M0L1iS937 + 1;
            _M0L1iS937 = _M0L6_2atmpS2450;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2451 = _M0Lm5indexS916;
        _M0Lm5indexS916 = _M0L6_2atmpS2451 + _M0L7olengthS920;
        _M0L6_2atmpS2455 = _M0Lm3expS921;
        _M0L7_2abindS939 = _M0L6_2atmpS2455 + 1;
        _M0L2__S940 = _M0L7olengthS920;
        while (1) {
          if (_M0L2__S940 < _M0L7_2abindS939) {
            int32_t _M0L6_2atmpS2452 = _M0Lm5indexS916;
            int32_t _M0L6_2atmpS2453;
            int32_t _M0L6_2atmpS2454;
            if (
              _M0L6_2atmpS2452 < 0
              || _M0L6_2atmpS2452 >= Moonbit_array_length(_M0L6resultS915)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS915[_M0L6_2atmpS2452] = 48;
            _M0L6_2atmpS2453 = _M0Lm5indexS916;
            _M0Lm5indexS916 = _M0L6_2atmpS2453 + 1;
            _M0L6_2atmpS2454 = _M0L2__S940 + 1;
            _M0L2__S940 = _M0L6_2atmpS2454;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2477 = _M0Lm5indexS916;
        int32_t _M0Lm7currentS942 = _M0L6_2atmpS2477 + 1;
        int32_t _M0L1iS943 = 0;
        int32_t _M0L6_2atmpS2475;
        int32_t _M0L6_2atmpS2476;
        while (1) {
          if (_M0L1iS943 < _M0L7olengthS920) {
            int32_t _M0L6_2atmpS2458 = _M0L7olengthS920 - _M0L1iS943;
            int32_t _M0L6_2atmpS2456 = _M0L6_2atmpS2458 - 1;
            int32_t _M0L6_2atmpS2457 = _M0Lm3expS921;
            int32_t _M0L6_2atmpS2472;
            int32_t _M0L6_2atmpS2471;
            int32_t _M0L6_2atmpS2470;
            int32_t _M0L6_2atmpS2464;
            uint64_t _M0L6_2atmpS2469;
            uint64_t _M0L6_2atmpS2468;
            int32_t _M0L6_2atmpS2467;
            int32_t _M0L6_2atmpS2466;
            int32_t _M0L6_2atmpS2465;
            uint64_t _M0L6_2atmpS2473;
            int32_t _M0L6_2atmpS2474;
            if (_M0L6_2atmpS2456 == _M0L6_2atmpS2457) {
              int32_t _M0L6_2atmpS2462 = _M0Lm7currentS942;
              int32_t _M0L6_2atmpS2461 = _M0L6_2atmpS2462 + _M0L7olengthS920;
              int32_t _M0L6_2atmpS2460 = _M0L6_2atmpS2461 - _M0L1iS943;
              int32_t _M0L6_2atmpS2459 = _M0L6_2atmpS2460 - 1;
              int32_t _M0L6_2atmpS2463;
              if (
                _M0L6_2atmpS2459 < 0
                || _M0L6_2atmpS2459 >= Moonbit_array_length(_M0L6resultS915)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS915[_M0L6_2atmpS2459] = 46;
              _M0L6_2atmpS2463 = _M0Lm7currentS942;
              _M0Lm7currentS942 = _M0L6_2atmpS2463 - 1;
            }
            _M0L6_2atmpS2472 = _M0Lm7currentS942;
            _M0L6_2atmpS2471 = _M0L6_2atmpS2472 + _M0L7olengthS920;
            _M0L6_2atmpS2470 = _M0L6_2atmpS2471 - _M0L1iS943;
            _M0L6_2atmpS2464 = _M0L6_2atmpS2470 - 1;
            _M0L6_2atmpS2469 = _M0Lm6outputS918;
            _M0L6_2atmpS2468 = _M0L6_2atmpS2469 % 10ull;
            _M0L6_2atmpS2467 = (int32_t)_M0L6_2atmpS2468;
            _M0L6_2atmpS2466 = 48 + _M0L6_2atmpS2467;
            _M0L6_2atmpS2465 = _M0L6_2atmpS2466 & 0xff;
            if (
              _M0L6_2atmpS2464 < 0
              || _M0L6_2atmpS2464 >= Moonbit_array_length(_M0L6resultS915)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS915[_M0L6_2atmpS2464] = _M0L6_2atmpS2465;
            _M0L6_2atmpS2473 = _M0Lm6outputS918;
            _M0Lm6outputS918 = _M0L6_2atmpS2473 / 10ull;
            _M0L6_2atmpS2474 = _M0L1iS943 + 1;
            _M0L1iS943 = _M0L6_2atmpS2474;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2475 = _M0Lm5indexS916;
        _M0L6_2atmpS2476 = _M0L7olengthS920 + 1;
        _M0Lm5indexS916 = _M0L6_2atmpS2475 + _M0L6_2atmpS2476;
      }
    }
    _M0L6_2atmpS2478 = _M0Lm5indexS916;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS915, 0, _M0L6_2atmpS2478);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS861,
  uint32_t _M0L12ieeeExponentS860
) {
  int32_t _M0Lm2e2S858;
  uint64_t _M0Lm2m2S859;
  uint64_t _M0L6_2atmpS2353;
  uint64_t _M0L6_2atmpS2352;
  int32_t _M0L4evenS862;
  uint64_t _M0L6_2atmpS2351;
  uint64_t _M0L2mvS863;
  int32_t _M0L7mmShiftS864;
  uint64_t _M0Lm2vrS865;
  uint64_t _M0Lm2vpS866;
  uint64_t _M0Lm2vmS867;
  int32_t _M0Lm3e10S868;
  int32_t _M0Lm17vmIsTrailingZerosS869;
  int32_t _M0Lm17vrIsTrailingZerosS870;
  int32_t _M0L6_2atmpS2253;
  int32_t _M0Lm7removedS889;
  int32_t _M0Lm16lastRemovedDigitS890;
  uint64_t _M0Lm6outputS891;
  int32_t _M0L6_2atmpS2349;
  int32_t _M0L6_2atmpS2350;
  int32_t _M0L3expS914;
  uint64_t _M0L6_2atmpS2348;
  struct _M0TPB17FloatingDecimal64* _block_3227;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S858 = 0;
  _M0Lm2m2S859 = 0ull;
  if (_M0L12ieeeExponentS860 == 0u) {
    _M0Lm2e2S858 = -1076;
    _M0Lm2m2S859 = _M0L12ieeeMantissaS861;
  } else {
    int32_t _M0L6_2atmpS2252 = *(int32_t*)&_M0L12ieeeExponentS860;
    int32_t _M0L6_2atmpS2251 = _M0L6_2atmpS2252 - 1023;
    int32_t _M0L6_2atmpS2250 = _M0L6_2atmpS2251 - 52;
    _M0Lm2e2S858 = _M0L6_2atmpS2250 - 2;
    _M0Lm2m2S859 = 4503599627370496ull | _M0L12ieeeMantissaS861;
  }
  _M0L6_2atmpS2353 = _M0Lm2m2S859;
  _M0L6_2atmpS2352 = _M0L6_2atmpS2353 & 1ull;
  _M0L4evenS862 = _M0L6_2atmpS2352 == 0ull;
  _M0L6_2atmpS2351 = _M0Lm2m2S859;
  _M0L2mvS863 = 4ull * _M0L6_2atmpS2351;
  if (_M0L12ieeeMantissaS861 != 0ull) {
    _M0L7mmShiftS864 = 1;
  } else {
    _M0L7mmShiftS864 = _M0L12ieeeExponentS860 <= 1u;
  }
  _M0Lm2vrS865 = 0ull;
  _M0Lm2vpS866 = 0ull;
  _M0Lm2vmS867 = 0ull;
  _M0Lm3e10S868 = 0;
  _M0Lm17vmIsTrailingZerosS869 = 0;
  _M0Lm17vrIsTrailingZerosS870 = 0;
  _M0L6_2atmpS2253 = _M0Lm2e2S858;
  if (_M0L6_2atmpS2253 >= 0) {
    int32_t _M0L6_2atmpS2275 = _M0Lm2e2S858;
    int32_t _M0L6_2atmpS2271;
    int32_t _M0L6_2atmpS2274;
    int32_t _M0L6_2atmpS2273;
    int32_t _M0L6_2atmpS2272;
    int32_t _M0L1qS871;
    int32_t _M0L6_2atmpS2270;
    int32_t _M0L6_2atmpS2269;
    int32_t _M0L1kS872;
    int32_t _M0L6_2atmpS2268;
    int32_t _M0L6_2atmpS2267;
    int32_t _M0L6_2atmpS2266;
    int32_t _M0L1iS873;
    struct _M0TPB8Pow5Pair _M0L4pow5S874;
    uint64_t _M0L6_2atmpS2265;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS875;
    uint64_t _M0L8_2avrOutS876;
    uint64_t _M0L8_2avpOutS877;
    uint64_t _M0L8_2avmOutS878;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2271 = _M0FPB9log10Pow2(_M0L6_2atmpS2275);
    _M0L6_2atmpS2274 = _M0Lm2e2S858;
    _M0L6_2atmpS2273 = _M0L6_2atmpS2274 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2272 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2273);
    _M0L1qS871 = _M0L6_2atmpS2271 - _M0L6_2atmpS2272;
    _M0Lm3e10S868 = _M0L1qS871;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2270 = _M0FPB8pow5bits(_M0L1qS871);
    _M0L6_2atmpS2269 = 125 + _M0L6_2atmpS2270;
    _M0L1kS872 = _M0L6_2atmpS2269 - 1;
    _M0L6_2atmpS2268 = _M0Lm2e2S858;
    _M0L6_2atmpS2267 = -_M0L6_2atmpS2268;
    _M0L6_2atmpS2266 = _M0L6_2atmpS2267 + _M0L1qS871;
    _M0L1iS873 = _M0L6_2atmpS2266 + _M0L1kS872;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S874 = _M0FPB22double__computeInvPow5(_M0L1qS871);
    _M0L6_2atmpS2265 = _M0Lm2m2S859;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS875
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2265, _M0L4pow5S874, _M0L1iS873, _M0L7mmShiftS864);
    _M0L8_2avrOutS876 = _M0L7_2abindS875.$0;
    _M0L8_2avpOutS877 = _M0L7_2abindS875.$1;
    _M0L8_2avmOutS878 = _M0L7_2abindS875.$2;
    _M0Lm2vrS865 = _M0L8_2avrOutS876;
    _M0Lm2vpS866 = _M0L8_2avpOutS877;
    _M0Lm2vmS867 = _M0L8_2avmOutS878;
    if (_M0L1qS871 <= 21) {
      int32_t _M0L6_2atmpS2261 = (int32_t)_M0L2mvS863;
      uint64_t _M0L6_2atmpS2264 = _M0L2mvS863 / 5ull;
      int32_t _M0L6_2atmpS2263 = (int32_t)_M0L6_2atmpS2264;
      int32_t _M0L6_2atmpS2262 = 5 * _M0L6_2atmpS2263;
      int32_t _M0L6mvMod5S879 = _M0L6_2atmpS2261 - _M0L6_2atmpS2262;
      if (_M0L6mvMod5S879 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS870
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS863, _M0L1qS871);
      } else if (_M0L4evenS862) {
        uint64_t _M0L6_2atmpS2255 = _M0L2mvS863 - 1ull;
        uint64_t _M0L6_2atmpS2256;
        uint64_t _M0L6_2atmpS2254;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2256 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS864);
        _M0L6_2atmpS2254 = _M0L6_2atmpS2255 - _M0L6_2atmpS2256;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS869
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2254, _M0L1qS871);
      } else {
        uint64_t _M0L6_2atmpS2257 = _M0Lm2vpS866;
        uint64_t _M0L6_2atmpS2260 = _M0L2mvS863 + 2ull;
        int32_t _M0L6_2atmpS2259;
        uint64_t _M0L6_2atmpS2258;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2259
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2260, _M0L1qS871);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2258 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2259);
        _M0Lm2vpS866 = _M0L6_2atmpS2257 - _M0L6_2atmpS2258;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2289 = _M0Lm2e2S858;
    int32_t _M0L6_2atmpS2288 = -_M0L6_2atmpS2289;
    int32_t _M0L6_2atmpS2283;
    int32_t _M0L6_2atmpS2287;
    int32_t _M0L6_2atmpS2286;
    int32_t _M0L6_2atmpS2285;
    int32_t _M0L6_2atmpS2284;
    int32_t _M0L1qS880;
    int32_t _M0L6_2atmpS2276;
    int32_t _M0L6_2atmpS2282;
    int32_t _M0L6_2atmpS2281;
    int32_t _M0L1iS881;
    int32_t _M0L6_2atmpS2280;
    int32_t _M0L1kS882;
    int32_t _M0L1jS883;
    struct _M0TPB8Pow5Pair _M0L4pow5S884;
    uint64_t _M0L6_2atmpS2279;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS885;
    uint64_t _M0L8_2avrOutS886;
    uint64_t _M0L8_2avpOutS887;
    uint64_t _M0L8_2avmOutS888;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2283 = _M0FPB9log10Pow5(_M0L6_2atmpS2288);
    _M0L6_2atmpS2287 = _M0Lm2e2S858;
    _M0L6_2atmpS2286 = -_M0L6_2atmpS2287;
    _M0L6_2atmpS2285 = _M0L6_2atmpS2286 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2284 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2285);
    _M0L1qS880 = _M0L6_2atmpS2283 - _M0L6_2atmpS2284;
    _M0L6_2atmpS2276 = _M0Lm2e2S858;
    _M0Lm3e10S868 = _M0L1qS880 + _M0L6_2atmpS2276;
    _M0L6_2atmpS2282 = _M0Lm2e2S858;
    _M0L6_2atmpS2281 = -_M0L6_2atmpS2282;
    _M0L1iS881 = _M0L6_2atmpS2281 - _M0L1qS880;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2280 = _M0FPB8pow5bits(_M0L1iS881);
    _M0L1kS882 = _M0L6_2atmpS2280 - 125;
    _M0L1jS883 = _M0L1qS880 - _M0L1kS882;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S884 = _M0FPB19double__computePow5(_M0L1iS881);
    _M0L6_2atmpS2279 = _M0Lm2m2S859;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS885
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2279, _M0L4pow5S884, _M0L1jS883, _M0L7mmShiftS864);
    _M0L8_2avrOutS886 = _M0L7_2abindS885.$0;
    _M0L8_2avpOutS887 = _M0L7_2abindS885.$1;
    _M0L8_2avmOutS888 = _M0L7_2abindS885.$2;
    _M0Lm2vrS865 = _M0L8_2avrOutS886;
    _M0Lm2vpS866 = _M0L8_2avpOutS887;
    _M0Lm2vmS867 = _M0L8_2avmOutS888;
    if (_M0L1qS880 <= 1) {
      _M0Lm17vrIsTrailingZerosS870 = 1;
      if (_M0L4evenS862) {
        int32_t _M0L6_2atmpS2277;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2277 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS864);
        _M0Lm17vmIsTrailingZerosS869 = _M0L6_2atmpS2277 == 1;
      } else {
        uint64_t _M0L6_2atmpS2278 = _M0Lm2vpS866;
        _M0Lm2vpS866 = _M0L6_2atmpS2278 - 1ull;
      }
    } else if (_M0L1qS880 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS870
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS863, _M0L1qS880);
    }
  }
  _M0Lm7removedS889 = 0;
  _M0Lm16lastRemovedDigitS890 = 0;
  _M0Lm6outputS891 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS869 || _M0Lm17vrIsTrailingZerosS870) {
    int32_t _if__result_3224;
    uint64_t _M0L6_2atmpS2319;
    uint64_t _M0L6_2atmpS2325;
    uint64_t _M0L6_2atmpS2326;
    int32_t _if__result_3225;
    int32_t _M0L6_2atmpS2322;
    int64_t _M0L6_2atmpS2321;
    uint64_t _M0L6_2atmpS2320;
    while (1) {
      uint64_t _M0L6_2atmpS2302 = _M0Lm2vpS866;
      uint64_t _M0L7vpDiv10S892 = _M0L6_2atmpS2302 / 10ull;
      uint64_t _M0L6_2atmpS2301 = _M0Lm2vmS867;
      uint64_t _M0L7vmDiv10S893 = _M0L6_2atmpS2301 / 10ull;
      uint64_t _M0L6_2atmpS2300;
      int32_t _M0L6_2atmpS2297;
      int32_t _M0L6_2atmpS2299;
      int32_t _M0L6_2atmpS2298;
      int32_t _M0L7vmMod10S895;
      uint64_t _M0L6_2atmpS2296;
      uint64_t _M0L7vrDiv10S896;
      uint64_t _M0L6_2atmpS2295;
      int32_t _M0L6_2atmpS2292;
      int32_t _M0L6_2atmpS2294;
      int32_t _M0L6_2atmpS2293;
      int32_t _M0L7vrMod10S897;
      int32_t _M0L6_2atmpS2291;
      if (_M0L7vpDiv10S892 <= _M0L7vmDiv10S893) {
        break;
      }
      _M0L6_2atmpS2300 = _M0Lm2vmS867;
      _M0L6_2atmpS2297 = (int32_t)_M0L6_2atmpS2300;
      _M0L6_2atmpS2299 = (int32_t)_M0L7vmDiv10S893;
      _M0L6_2atmpS2298 = 10 * _M0L6_2atmpS2299;
      _M0L7vmMod10S895 = _M0L6_2atmpS2297 - _M0L6_2atmpS2298;
      _M0L6_2atmpS2296 = _M0Lm2vrS865;
      _M0L7vrDiv10S896 = _M0L6_2atmpS2296 / 10ull;
      _M0L6_2atmpS2295 = _M0Lm2vrS865;
      _M0L6_2atmpS2292 = (int32_t)_M0L6_2atmpS2295;
      _M0L6_2atmpS2294 = (int32_t)_M0L7vrDiv10S896;
      _M0L6_2atmpS2293 = 10 * _M0L6_2atmpS2294;
      _M0L7vrMod10S897 = _M0L6_2atmpS2292 - _M0L6_2atmpS2293;
      if (_M0Lm17vmIsTrailingZerosS869) {
        _M0Lm17vmIsTrailingZerosS869 = _M0L7vmMod10S895 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS869 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS870) {
        int32_t _M0L6_2atmpS2290 = _M0Lm16lastRemovedDigitS890;
        _M0Lm17vrIsTrailingZerosS870 = _M0L6_2atmpS2290 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS870 = 0;
      }
      _M0Lm16lastRemovedDigitS890 = _M0L7vrMod10S897;
      _M0Lm2vrS865 = _M0L7vrDiv10S896;
      _M0Lm2vpS866 = _M0L7vpDiv10S892;
      _M0Lm2vmS867 = _M0L7vmDiv10S893;
      _M0L6_2atmpS2291 = _M0Lm7removedS889;
      _M0Lm7removedS889 = _M0L6_2atmpS2291 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS869) {
      while (1) {
        uint64_t _M0L6_2atmpS2315 = _M0Lm2vmS867;
        uint64_t _M0L7vmDiv10S898 = _M0L6_2atmpS2315 / 10ull;
        uint64_t _M0L6_2atmpS2314 = _M0Lm2vmS867;
        int32_t _M0L6_2atmpS2311 = (int32_t)_M0L6_2atmpS2314;
        int32_t _M0L6_2atmpS2313 = (int32_t)_M0L7vmDiv10S898;
        int32_t _M0L6_2atmpS2312 = 10 * _M0L6_2atmpS2313;
        int32_t _M0L7vmMod10S899 = _M0L6_2atmpS2311 - _M0L6_2atmpS2312;
        uint64_t _M0L6_2atmpS2310;
        uint64_t _M0L7vpDiv10S901;
        uint64_t _M0L6_2atmpS2309;
        uint64_t _M0L7vrDiv10S902;
        uint64_t _M0L6_2atmpS2308;
        int32_t _M0L6_2atmpS2305;
        int32_t _M0L6_2atmpS2307;
        int32_t _M0L6_2atmpS2306;
        int32_t _M0L7vrMod10S903;
        int32_t _M0L6_2atmpS2304;
        if (_M0L7vmMod10S899 != 0) {
          break;
        }
        _M0L6_2atmpS2310 = _M0Lm2vpS866;
        _M0L7vpDiv10S901 = _M0L6_2atmpS2310 / 10ull;
        _M0L6_2atmpS2309 = _M0Lm2vrS865;
        _M0L7vrDiv10S902 = _M0L6_2atmpS2309 / 10ull;
        _M0L6_2atmpS2308 = _M0Lm2vrS865;
        _M0L6_2atmpS2305 = (int32_t)_M0L6_2atmpS2308;
        _M0L6_2atmpS2307 = (int32_t)_M0L7vrDiv10S902;
        _M0L6_2atmpS2306 = 10 * _M0L6_2atmpS2307;
        _M0L7vrMod10S903 = _M0L6_2atmpS2305 - _M0L6_2atmpS2306;
        if (_M0Lm17vrIsTrailingZerosS870) {
          int32_t _M0L6_2atmpS2303 = _M0Lm16lastRemovedDigitS890;
          _M0Lm17vrIsTrailingZerosS870 = _M0L6_2atmpS2303 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS870 = 0;
        }
        _M0Lm16lastRemovedDigitS890 = _M0L7vrMod10S903;
        _M0Lm2vrS865 = _M0L7vrDiv10S902;
        _M0Lm2vpS866 = _M0L7vpDiv10S901;
        _M0Lm2vmS867 = _M0L7vmDiv10S898;
        _M0L6_2atmpS2304 = _M0Lm7removedS889;
        _M0Lm7removedS889 = _M0L6_2atmpS2304 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS870) {
      int32_t _M0L6_2atmpS2318 = _M0Lm16lastRemovedDigitS890;
      if (_M0L6_2atmpS2318 == 5) {
        uint64_t _M0L6_2atmpS2317 = _M0Lm2vrS865;
        uint64_t _M0L6_2atmpS2316 = _M0L6_2atmpS2317 % 2ull;
        _if__result_3224 = _M0L6_2atmpS2316 == 0ull;
      } else {
        _if__result_3224 = 0;
      }
    } else {
      _if__result_3224 = 0;
    }
    if (_if__result_3224) {
      _M0Lm16lastRemovedDigitS890 = 4;
    }
    _M0L6_2atmpS2319 = _M0Lm2vrS865;
    _M0L6_2atmpS2325 = _M0Lm2vrS865;
    _M0L6_2atmpS2326 = _M0Lm2vmS867;
    if (_M0L6_2atmpS2325 == _M0L6_2atmpS2326) {
      if (!_M0L4evenS862) {
        _if__result_3225 = 1;
      } else {
        int32_t _M0L6_2atmpS2324 = _M0Lm17vmIsTrailingZerosS869;
        _if__result_3225 = !_M0L6_2atmpS2324;
      }
    } else {
      _if__result_3225 = 0;
    }
    if (_if__result_3225) {
      _M0L6_2atmpS2322 = 1;
    } else {
      int32_t _M0L6_2atmpS2323 = _M0Lm16lastRemovedDigitS890;
      _M0L6_2atmpS2322 = _M0L6_2atmpS2323 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2321 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2322);
    _M0L6_2atmpS2320 = *(uint64_t*)&_M0L6_2atmpS2321;
    _M0Lm6outputS891 = _M0L6_2atmpS2319 + _M0L6_2atmpS2320;
  } else {
    int32_t _M0Lm7roundUpS904 = 0;
    uint64_t _M0L6_2atmpS2347 = _M0Lm2vpS866;
    uint64_t _M0L8vpDiv100S905 = _M0L6_2atmpS2347 / 100ull;
    uint64_t _M0L6_2atmpS2346 = _M0Lm2vmS867;
    uint64_t _M0L8vmDiv100S906 = _M0L6_2atmpS2346 / 100ull;
    uint64_t _M0L6_2atmpS2341;
    uint64_t _M0L6_2atmpS2344;
    uint64_t _M0L6_2atmpS2345;
    int32_t _M0L6_2atmpS2343;
    uint64_t _M0L6_2atmpS2342;
    if (_M0L8vpDiv100S905 > _M0L8vmDiv100S906) {
      uint64_t _M0L6_2atmpS2332 = _M0Lm2vrS865;
      uint64_t _M0L8vrDiv100S907 = _M0L6_2atmpS2332 / 100ull;
      uint64_t _M0L6_2atmpS2331 = _M0Lm2vrS865;
      int32_t _M0L6_2atmpS2328 = (int32_t)_M0L6_2atmpS2331;
      int32_t _M0L6_2atmpS2330 = (int32_t)_M0L8vrDiv100S907;
      int32_t _M0L6_2atmpS2329 = 100 * _M0L6_2atmpS2330;
      int32_t _M0L8vrMod100S908 = _M0L6_2atmpS2328 - _M0L6_2atmpS2329;
      int32_t _M0L6_2atmpS2327;
      _M0Lm7roundUpS904 = _M0L8vrMod100S908 >= 50;
      _M0Lm2vrS865 = _M0L8vrDiv100S907;
      _M0Lm2vpS866 = _M0L8vpDiv100S905;
      _M0Lm2vmS867 = _M0L8vmDiv100S906;
      _M0L6_2atmpS2327 = _M0Lm7removedS889;
      _M0Lm7removedS889 = _M0L6_2atmpS2327 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2340 = _M0Lm2vpS866;
      uint64_t _M0L7vpDiv10S909 = _M0L6_2atmpS2340 / 10ull;
      uint64_t _M0L6_2atmpS2339 = _M0Lm2vmS867;
      uint64_t _M0L7vmDiv10S910 = _M0L6_2atmpS2339 / 10ull;
      uint64_t _M0L6_2atmpS2338;
      uint64_t _M0L7vrDiv10S912;
      uint64_t _M0L6_2atmpS2337;
      int32_t _M0L6_2atmpS2334;
      int32_t _M0L6_2atmpS2336;
      int32_t _M0L6_2atmpS2335;
      int32_t _M0L7vrMod10S913;
      int32_t _M0L6_2atmpS2333;
      if (_M0L7vpDiv10S909 <= _M0L7vmDiv10S910) {
        break;
      }
      _M0L6_2atmpS2338 = _M0Lm2vrS865;
      _M0L7vrDiv10S912 = _M0L6_2atmpS2338 / 10ull;
      _M0L6_2atmpS2337 = _M0Lm2vrS865;
      _M0L6_2atmpS2334 = (int32_t)_M0L6_2atmpS2337;
      _M0L6_2atmpS2336 = (int32_t)_M0L7vrDiv10S912;
      _M0L6_2atmpS2335 = 10 * _M0L6_2atmpS2336;
      _M0L7vrMod10S913 = _M0L6_2atmpS2334 - _M0L6_2atmpS2335;
      _M0Lm7roundUpS904 = _M0L7vrMod10S913 >= 5;
      _M0Lm2vrS865 = _M0L7vrDiv10S912;
      _M0Lm2vpS866 = _M0L7vpDiv10S909;
      _M0Lm2vmS867 = _M0L7vmDiv10S910;
      _M0L6_2atmpS2333 = _M0Lm7removedS889;
      _M0Lm7removedS889 = _M0L6_2atmpS2333 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2341 = _M0Lm2vrS865;
    _M0L6_2atmpS2344 = _M0Lm2vrS865;
    _M0L6_2atmpS2345 = _M0Lm2vmS867;
    _M0L6_2atmpS2343
    = _M0L6_2atmpS2344 == _M0L6_2atmpS2345 || _M0Lm7roundUpS904;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2342 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2343);
    _M0Lm6outputS891 = _M0L6_2atmpS2341 + _M0L6_2atmpS2342;
  }
  _M0L6_2atmpS2349 = _M0Lm3e10S868;
  _M0L6_2atmpS2350 = _M0Lm7removedS889;
  _M0L3expS914 = _M0L6_2atmpS2349 + _M0L6_2atmpS2350;
  _M0L6_2atmpS2348 = _M0Lm6outputS891;
  _block_3227
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3227)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3227->$0 = _M0L6_2atmpS2348;
  _block_3227->$1 = _M0L3expS914;
  return _block_3227;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS857) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS857) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS856) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS856) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS855) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS855) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS854) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS854 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS854 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS854 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS854 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS854 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS854 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS854 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS854 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS854 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS854 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS854 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS854 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS854 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS854 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS854 >= 100ull) {
    return 3;
  }
  if (_M0L1vS854 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS837) {
  int32_t _M0L6_2atmpS2249;
  int32_t _M0L6_2atmpS2248;
  int32_t _M0L4baseS836;
  int32_t _M0L5base2S838;
  int32_t _M0L6offsetS839;
  int32_t _M0L6_2atmpS2247;
  uint64_t _M0L4mul0S840;
  int32_t _M0L6_2atmpS2246;
  int32_t _M0L6_2atmpS2245;
  uint64_t _M0L4mul1S841;
  uint64_t _M0L1mS842;
  struct _M0TPB7Umul128 _M0L7_2abindS843;
  uint64_t _M0L7_2alow1S844;
  uint64_t _M0L8_2ahigh1S845;
  struct _M0TPB7Umul128 _M0L7_2abindS846;
  uint64_t _M0L7_2alow0S847;
  uint64_t _M0L8_2ahigh0S848;
  uint64_t _M0L3sumS849;
  uint64_t _M0Lm5high1S850;
  int32_t _M0L6_2atmpS2243;
  int32_t _M0L6_2atmpS2244;
  int32_t _M0L5deltaS851;
  uint64_t _M0L6_2atmpS2242;
  uint64_t _M0L6_2atmpS2234;
  int32_t _M0L6_2atmpS2241;
  uint32_t _M0L6_2atmpS2238;
  int32_t _M0L6_2atmpS2240;
  int32_t _M0L6_2atmpS2239;
  uint32_t _M0L6_2atmpS2237;
  uint32_t _M0L6_2atmpS2236;
  uint64_t _M0L6_2atmpS2235;
  uint64_t _M0L1aS852;
  uint64_t _M0L6_2atmpS2233;
  uint64_t _M0L1bS853;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2249 = _M0L1iS837 + 26;
  _M0L6_2atmpS2248 = _M0L6_2atmpS2249 - 1;
  _M0L4baseS836 = _M0L6_2atmpS2248 / 26;
  _M0L5base2S838 = _M0L4baseS836 * 26;
  _M0L6offsetS839 = _M0L5base2S838 - _M0L1iS837;
  _M0L6_2atmpS2247 = _M0L4baseS836 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S840
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2247);
  _M0L6_2atmpS2246 = _M0L4baseS836 * 2;
  _M0L6_2atmpS2245 = _M0L6_2atmpS2246 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S841
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2245);
  if (_M0L6offsetS839 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S840, _M0L4mul1S841};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS842
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS839);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS843 = _M0FPB7umul128(_M0L1mS842, _M0L4mul1S841);
  _M0L7_2alow1S844 = _M0L7_2abindS843.$0;
  _M0L8_2ahigh1S845 = _M0L7_2abindS843.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS846 = _M0FPB7umul128(_M0L1mS842, _M0L4mul0S840);
  _M0L7_2alow0S847 = _M0L7_2abindS846.$0;
  _M0L8_2ahigh0S848 = _M0L7_2abindS846.$1;
  _M0L3sumS849 = _M0L8_2ahigh0S848 + _M0L7_2alow1S844;
  _M0Lm5high1S850 = _M0L8_2ahigh1S845;
  if (_M0L3sumS849 < _M0L8_2ahigh0S848) {
    uint64_t _M0L6_2atmpS2232 = _M0Lm5high1S850;
    _M0Lm5high1S850 = _M0L6_2atmpS2232 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2243 = _M0FPB8pow5bits(_M0L5base2S838);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2244 = _M0FPB8pow5bits(_M0L1iS837);
  _M0L5deltaS851 = _M0L6_2atmpS2243 - _M0L6_2atmpS2244;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2242
  = _M0FPB13shiftright128(_M0L7_2alow0S847, _M0L3sumS849, _M0L5deltaS851);
  _M0L6_2atmpS2234 = _M0L6_2atmpS2242 + 1ull;
  _M0L6_2atmpS2241 = _M0L1iS837 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2238
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2241);
  _M0L6_2atmpS2240 = _M0L1iS837 % 16;
  _M0L6_2atmpS2239 = _M0L6_2atmpS2240 << 1;
  _M0L6_2atmpS2237 = _M0L6_2atmpS2238 >> (_M0L6_2atmpS2239 & 31);
  _M0L6_2atmpS2236 = _M0L6_2atmpS2237 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2235 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2236);
  _M0L1aS852 = _M0L6_2atmpS2234 + _M0L6_2atmpS2235;
  _M0L6_2atmpS2233 = _M0Lm5high1S850;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS853
  = _M0FPB13shiftright128(_M0L3sumS849, _M0L6_2atmpS2233, _M0L5deltaS851);
  return (struct _M0TPB8Pow5Pair){_M0L1aS852, _M0L1bS853};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS819) {
  int32_t _M0L4baseS818;
  int32_t _M0L5base2S820;
  int32_t _M0L6offsetS821;
  int32_t _M0L6_2atmpS2231;
  uint64_t _M0L4mul0S822;
  int32_t _M0L6_2atmpS2230;
  int32_t _M0L6_2atmpS2229;
  uint64_t _M0L4mul1S823;
  uint64_t _M0L1mS824;
  struct _M0TPB7Umul128 _M0L7_2abindS825;
  uint64_t _M0L7_2alow1S826;
  uint64_t _M0L8_2ahigh1S827;
  struct _M0TPB7Umul128 _M0L7_2abindS828;
  uint64_t _M0L7_2alow0S829;
  uint64_t _M0L8_2ahigh0S830;
  uint64_t _M0L3sumS831;
  uint64_t _M0Lm5high1S832;
  int32_t _M0L6_2atmpS2227;
  int32_t _M0L6_2atmpS2228;
  int32_t _M0L5deltaS833;
  uint64_t _M0L6_2atmpS2219;
  int32_t _M0L6_2atmpS2226;
  uint32_t _M0L6_2atmpS2223;
  int32_t _M0L6_2atmpS2225;
  int32_t _M0L6_2atmpS2224;
  uint32_t _M0L6_2atmpS2222;
  uint32_t _M0L6_2atmpS2221;
  uint64_t _M0L6_2atmpS2220;
  uint64_t _M0L1aS834;
  uint64_t _M0L6_2atmpS2218;
  uint64_t _M0L1bS835;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS818 = _M0L1iS819 / 26;
  _M0L5base2S820 = _M0L4baseS818 * 26;
  _M0L6offsetS821 = _M0L1iS819 - _M0L5base2S820;
  _M0L6_2atmpS2231 = _M0L4baseS818 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S822
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2231);
  _M0L6_2atmpS2230 = _M0L4baseS818 * 2;
  _M0L6_2atmpS2229 = _M0L6_2atmpS2230 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S823
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2229);
  if (_M0L6offsetS821 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S822, _M0L4mul1S823};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS824
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS821);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS825 = _M0FPB7umul128(_M0L1mS824, _M0L4mul1S823);
  _M0L7_2alow1S826 = _M0L7_2abindS825.$0;
  _M0L8_2ahigh1S827 = _M0L7_2abindS825.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS828 = _M0FPB7umul128(_M0L1mS824, _M0L4mul0S822);
  _M0L7_2alow0S829 = _M0L7_2abindS828.$0;
  _M0L8_2ahigh0S830 = _M0L7_2abindS828.$1;
  _M0L3sumS831 = _M0L8_2ahigh0S830 + _M0L7_2alow1S826;
  _M0Lm5high1S832 = _M0L8_2ahigh1S827;
  if (_M0L3sumS831 < _M0L8_2ahigh0S830) {
    uint64_t _M0L6_2atmpS2217 = _M0Lm5high1S832;
    _M0Lm5high1S832 = _M0L6_2atmpS2217 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2227 = _M0FPB8pow5bits(_M0L1iS819);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2228 = _M0FPB8pow5bits(_M0L5base2S820);
  _M0L5deltaS833 = _M0L6_2atmpS2227 - _M0L6_2atmpS2228;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2219
  = _M0FPB13shiftright128(_M0L7_2alow0S829, _M0L3sumS831, _M0L5deltaS833);
  _M0L6_2atmpS2226 = _M0L1iS819 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2223
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2226);
  _M0L6_2atmpS2225 = _M0L1iS819 % 16;
  _M0L6_2atmpS2224 = _M0L6_2atmpS2225 << 1;
  _M0L6_2atmpS2222 = _M0L6_2atmpS2223 >> (_M0L6_2atmpS2224 & 31);
  _M0L6_2atmpS2221 = _M0L6_2atmpS2222 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2220 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2221);
  _M0L1aS834 = _M0L6_2atmpS2219 + _M0L6_2atmpS2220;
  _M0L6_2atmpS2218 = _M0Lm5high1S832;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS835
  = _M0FPB13shiftright128(_M0L3sumS831, _M0L6_2atmpS2218, _M0L5deltaS833);
  return (struct _M0TPB8Pow5Pair){_M0L1aS834, _M0L1bS835};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS792,
  struct _M0TPB8Pow5Pair _M0L3mulS789,
  int32_t _M0L1jS805,
  int32_t _M0L7mmShiftS807
) {
  uint64_t _M0L7_2amul0S788;
  uint64_t _M0L7_2amul1S790;
  uint64_t _M0L1mS791;
  struct _M0TPB7Umul128 _M0L7_2abindS793;
  uint64_t _M0L5_2aloS794;
  uint64_t _M0L6_2atmpS795;
  struct _M0TPB7Umul128 _M0L7_2abindS796;
  uint64_t _M0L6_2alo2S797;
  uint64_t _M0L6_2ahi2S798;
  uint64_t _M0L3midS799;
  uint64_t _M0L6_2atmpS2216;
  uint64_t _M0L2hiS800;
  uint64_t _M0L3lo2S801;
  uint64_t _M0L6_2atmpS2214;
  uint64_t _M0L6_2atmpS2215;
  uint64_t _M0L4mid2S802;
  uint64_t _M0L6_2atmpS2213;
  uint64_t _M0L3hi2S803;
  int32_t _M0L6_2atmpS2212;
  int32_t _M0L6_2atmpS2211;
  uint64_t _M0L2vpS804;
  uint64_t _M0Lm2vmS806;
  int32_t _M0L6_2atmpS2210;
  int32_t _M0L6_2atmpS2209;
  uint64_t _M0L2vrS817;
  uint64_t _M0L6_2atmpS2208;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S788 = _M0L3mulS789.$0;
  _M0L7_2amul1S790 = _M0L3mulS789.$1;
  _M0L1mS791 = _M0L1mS792 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS793 = _M0FPB7umul128(_M0L1mS791, _M0L7_2amul0S788);
  _M0L5_2aloS794 = _M0L7_2abindS793.$0;
  _M0L6_2atmpS795 = _M0L7_2abindS793.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS796 = _M0FPB7umul128(_M0L1mS791, _M0L7_2amul1S790);
  _M0L6_2alo2S797 = _M0L7_2abindS796.$0;
  _M0L6_2ahi2S798 = _M0L7_2abindS796.$1;
  _M0L3midS799 = _M0L6_2atmpS795 + _M0L6_2alo2S797;
  if (_M0L3midS799 < _M0L6_2atmpS795) {
    _M0L6_2atmpS2216 = 1ull;
  } else {
    _M0L6_2atmpS2216 = 0ull;
  }
  _M0L2hiS800 = _M0L6_2ahi2S798 + _M0L6_2atmpS2216;
  _M0L3lo2S801 = _M0L5_2aloS794 + _M0L7_2amul0S788;
  _M0L6_2atmpS2214 = _M0L3midS799 + _M0L7_2amul1S790;
  if (_M0L3lo2S801 < _M0L5_2aloS794) {
    _M0L6_2atmpS2215 = 1ull;
  } else {
    _M0L6_2atmpS2215 = 0ull;
  }
  _M0L4mid2S802 = _M0L6_2atmpS2214 + _M0L6_2atmpS2215;
  if (_M0L4mid2S802 < _M0L3midS799) {
    _M0L6_2atmpS2213 = 1ull;
  } else {
    _M0L6_2atmpS2213 = 0ull;
  }
  _M0L3hi2S803 = _M0L2hiS800 + _M0L6_2atmpS2213;
  _M0L6_2atmpS2212 = _M0L1jS805 - 64;
  _M0L6_2atmpS2211 = _M0L6_2atmpS2212 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS804
  = _M0FPB13shiftright128(_M0L4mid2S802, _M0L3hi2S803, _M0L6_2atmpS2211);
  _M0Lm2vmS806 = 0ull;
  if (_M0L7mmShiftS807) {
    uint64_t _M0L3lo3S808 = _M0L5_2aloS794 - _M0L7_2amul0S788;
    uint64_t _M0L6_2atmpS2198 = _M0L3midS799 - _M0L7_2amul1S790;
    uint64_t _M0L6_2atmpS2199;
    uint64_t _M0L4mid3S809;
    uint64_t _M0L6_2atmpS2197;
    uint64_t _M0L3hi3S810;
    int32_t _M0L6_2atmpS2196;
    int32_t _M0L6_2atmpS2195;
    if (_M0L5_2aloS794 < _M0L3lo3S808) {
      _M0L6_2atmpS2199 = 1ull;
    } else {
      _M0L6_2atmpS2199 = 0ull;
    }
    _M0L4mid3S809 = _M0L6_2atmpS2198 - _M0L6_2atmpS2199;
    if (_M0L3midS799 < _M0L4mid3S809) {
      _M0L6_2atmpS2197 = 1ull;
    } else {
      _M0L6_2atmpS2197 = 0ull;
    }
    _M0L3hi3S810 = _M0L2hiS800 - _M0L6_2atmpS2197;
    _M0L6_2atmpS2196 = _M0L1jS805 - 64;
    _M0L6_2atmpS2195 = _M0L6_2atmpS2196 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS806
    = _M0FPB13shiftright128(_M0L4mid3S809, _M0L3hi3S810, _M0L6_2atmpS2195);
  } else {
    uint64_t _M0L3lo3S811 = _M0L5_2aloS794 + _M0L5_2aloS794;
    uint64_t _M0L6_2atmpS2206 = _M0L3midS799 + _M0L3midS799;
    uint64_t _M0L6_2atmpS2207;
    uint64_t _M0L4mid3S812;
    uint64_t _M0L6_2atmpS2204;
    uint64_t _M0L6_2atmpS2205;
    uint64_t _M0L3hi3S813;
    uint64_t _M0L3lo4S814;
    uint64_t _M0L6_2atmpS2202;
    uint64_t _M0L6_2atmpS2203;
    uint64_t _M0L4mid4S815;
    uint64_t _M0L6_2atmpS2201;
    uint64_t _M0L3hi4S816;
    int32_t _M0L6_2atmpS2200;
    if (_M0L3lo3S811 < _M0L5_2aloS794) {
      _M0L6_2atmpS2207 = 1ull;
    } else {
      _M0L6_2atmpS2207 = 0ull;
    }
    _M0L4mid3S812 = _M0L6_2atmpS2206 + _M0L6_2atmpS2207;
    _M0L6_2atmpS2204 = _M0L2hiS800 + _M0L2hiS800;
    if (_M0L4mid3S812 < _M0L3midS799) {
      _M0L6_2atmpS2205 = 1ull;
    } else {
      _M0L6_2atmpS2205 = 0ull;
    }
    _M0L3hi3S813 = _M0L6_2atmpS2204 + _M0L6_2atmpS2205;
    _M0L3lo4S814 = _M0L3lo3S811 - _M0L7_2amul0S788;
    _M0L6_2atmpS2202 = _M0L4mid3S812 - _M0L7_2amul1S790;
    if (_M0L3lo3S811 < _M0L3lo4S814) {
      _M0L6_2atmpS2203 = 1ull;
    } else {
      _M0L6_2atmpS2203 = 0ull;
    }
    _M0L4mid4S815 = _M0L6_2atmpS2202 - _M0L6_2atmpS2203;
    if (_M0L4mid3S812 < _M0L4mid4S815) {
      _M0L6_2atmpS2201 = 1ull;
    } else {
      _M0L6_2atmpS2201 = 0ull;
    }
    _M0L3hi4S816 = _M0L3hi3S813 - _M0L6_2atmpS2201;
    _M0L6_2atmpS2200 = _M0L1jS805 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS806
    = _M0FPB13shiftright128(_M0L4mid4S815, _M0L3hi4S816, _M0L6_2atmpS2200);
  }
  _M0L6_2atmpS2210 = _M0L1jS805 - 64;
  _M0L6_2atmpS2209 = _M0L6_2atmpS2210 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS817
  = _M0FPB13shiftright128(_M0L3midS799, _M0L2hiS800, _M0L6_2atmpS2209);
  _M0L6_2atmpS2208 = _M0Lm2vmS806;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS817,
                                                _M0L2vpS804,
                                                _M0L6_2atmpS2208};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS786,
  int32_t _M0L1pS787
) {
  uint64_t _M0L6_2atmpS2194;
  uint64_t _M0L6_2atmpS2193;
  uint64_t _M0L6_2atmpS2192;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2194 = 1ull << (_M0L1pS787 & 63);
  _M0L6_2atmpS2193 = _M0L6_2atmpS2194 - 1ull;
  _M0L6_2atmpS2192 = _M0L5valueS786 & _M0L6_2atmpS2193;
  return _M0L6_2atmpS2192 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS784,
  int32_t _M0L1pS785
) {
  int32_t _M0L6_2atmpS2191;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2191 = _M0FPB10pow5Factor(_M0L5valueS784);
  return _M0L6_2atmpS2191 >= _M0L1pS785;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS780) {
  uint64_t _M0L6_2atmpS2179;
  uint64_t _M0L6_2atmpS2180;
  uint64_t _M0L6_2atmpS2181;
  uint64_t _M0L6_2atmpS2182;
  int32_t _M0Lm5countS781;
  uint64_t _M0Lm5valueS782;
  uint64_t _M0L6_2atmpS2190;
  moonbit_string_t _M0L6_2atmpS2189;
  moonbit_string_t _M0L6_2atmpS2889;
  moonbit_string_t _M0L6_2atmpS2188;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2179 = _M0L5valueS780 % 5ull;
  if (_M0L6_2atmpS2179 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2180 = _M0L5valueS780 % 25ull;
  if (_M0L6_2atmpS2180 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2181 = _M0L5valueS780 % 125ull;
  if (_M0L6_2atmpS2181 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2182 = _M0L5valueS780 % 625ull;
  if (_M0L6_2atmpS2182 != 0ull) {
    return 3;
  }
  _M0Lm5countS781 = 4;
  _M0Lm5valueS782 = _M0L5valueS780 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2183 = _M0Lm5valueS782;
    if (_M0L6_2atmpS2183 > 0ull) {
      uint64_t _M0L6_2atmpS2185 = _M0Lm5valueS782;
      uint64_t _M0L6_2atmpS2184 = _M0L6_2atmpS2185 % 5ull;
      uint64_t _M0L6_2atmpS2186;
      int32_t _M0L6_2atmpS2187;
      if (_M0L6_2atmpS2184 != 0ull) {
        return _M0Lm5countS781;
      }
      _M0L6_2atmpS2186 = _M0Lm5valueS782;
      _M0Lm5valueS782 = _M0L6_2atmpS2186 / 5ull;
      _M0L6_2atmpS2187 = _M0Lm5countS781;
      _M0Lm5countS781 = _M0L6_2atmpS2187 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2190 = _M0Lm5valueS782;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2189
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2190);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2889
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS2189);
  moonbit_decref(_M0L6_2atmpS2189);
  _M0L6_2atmpS2188 = _M0L6_2atmpS2889;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2188, (moonbit_string_t)moonbit_string_literal_67.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS779,
  uint64_t _M0L2hiS777,
  int32_t _M0L4distS778
) {
  int32_t _M0L6_2atmpS2178;
  uint64_t _M0L6_2atmpS2176;
  uint64_t _M0L6_2atmpS2177;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2178 = 64 - _M0L4distS778;
  _M0L6_2atmpS2176 = _M0L2hiS777 << (_M0L6_2atmpS2178 & 63);
  _M0L6_2atmpS2177 = _M0L2loS779 >> (_M0L4distS778 & 63);
  return _M0L6_2atmpS2176 | _M0L6_2atmpS2177;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS767,
  uint64_t _M0L1bS770
) {
  uint64_t _M0L3aLoS766;
  uint64_t _M0L3aHiS768;
  uint64_t _M0L3bLoS769;
  uint64_t _M0L3bHiS771;
  uint64_t _M0L1xS772;
  uint64_t _M0L6_2atmpS2174;
  uint64_t _M0L6_2atmpS2175;
  uint64_t _M0L1yS773;
  uint64_t _M0L6_2atmpS2172;
  uint64_t _M0L6_2atmpS2173;
  uint64_t _M0L1zS774;
  uint64_t _M0L6_2atmpS2170;
  uint64_t _M0L6_2atmpS2171;
  uint64_t _M0L6_2atmpS2168;
  uint64_t _M0L6_2atmpS2169;
  uint64_t _M0L1wS775;
  uint64_t _M0L2loS776;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS766 = _M0L1aS767 & 4294967295ull;
  _M0L3aHiS768 = _M0L1aS767 >> 32;
  _M0L3bLoS769 = _M0L1bS770 & 4294967295ull;
  _M0L3bHiS771 = _M0L1bS770 >> 32;
  _M0L1xS772 = _M0L3aLoS766 * _M0L3bLoS769;
  _M0L6_2atmpS2174 = _M0L3aHiS768 * _M0L3bLoS769;
  _M0L6_2atmpS2175 = _M0L1xS772 >> 32;
  _M0L1yS773 = _M0L6_2atmpS2174 + _M0L6_2atmpS2175;
  _M0L6_2atmpS2172 = _M0L3aLoS766 * _M0L3bHiS771;
  _M0L6_2atmpS2173 = _M0L1yS773 & 4294967295ull;
  _M0L1zS774 = _M0L6_2atmpS2172 + _M0L6_2atmpS2173;
  _M0L6_2atmpS2170 = _M0L3aHiS768 * _M0L3bHiS771;
  _M0L6_2atmpS2171 = _M0L1yS773 >> 32;
  _M0L6_2atmpS2168 = _M0L6_2atmpS2170 + _M0L6_2atmpS2171;
  _M0L6_2atmpS2169 = _M0L1zS774 >> 32;
  _M0L1wS775 = _M0L6_2atmpS2168 + _M0L6_2atmpS2169;
  _M0L2loS776 = _M0L1aS767 * _M0L1bS770;
  return (struct _M0TPB7Umul128){_M0L2loS776, _M0L1wS775};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS761,
  int32_t _M0L4fromS765,
  int32_t _M0L2toS763
) {
  int32_t _M0L6_2atmpS2167;
  struct _M0TPB13StringBuilder* _M0L3bufS760;
  int32_t _M0L1iS762;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2167 = Moonbit_array_length(_M0L5bytesS761);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS760 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2167);
  _M0L1iS762 = _M0L4fromS765;
  while (1) {
    if (_M0L1iS762 < _M0L2toS763) {
      int32_t _M0L6_2atmpS2165;
      int32_t _M0L6_2atmpS2164;
      int32_t _M0L6_2atmpS2166;
      if (
        _M0L1iS762 < 0 || _M0L1iS762 >= Moonbit_array_length(_M0L5bytesS761)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2165 = (int32_t)_M0L5bytesS761[_M0L1iS762];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2164 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2165);
      moonbit_incref(_M0L3bufS760);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS760, _M0L6_2atmpS2164);
      _M0L6_2atmpS2166 = _M0L1iS762 + 1;
      _M0L1iS762 = _M0L6_2atmpS2166;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS761);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS760);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS759) {
  int32_t _M0L6_2atmpS2163;
  uint32_t _M0L6_2atmpS2162;
  uint32_t _M0L6_2atmpS2161;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2163 = _M0L1eS759 * 78913;
  _M0L6_2atmpS2162 = *(uint32_t*)&_M0L6_2atmpS2163;
  _M0L6_2atmpS2161 = _M0L6_2atmpS2162 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2161;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS758) {
  int32_t _M0L6_2atmpS2160;
  uint32_t _M0L6_2atmpS2159;
  uint32_t _M0L6_2atmpS2158;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2160 = _M0L1eS758 * 732923;
  _M0L6_2atmpS2159 = *(uint32_t*)&_M0L6_2atmpS2160;
  _M0L6_2atmpS2158 = _M0L6_2atmpS2159 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2158;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS756,
  int32_t _M0L8exponentS757,
  int32_t _M0L8mantissaS754
) {
  moonbit_string_t _M0L1sS755;
  moonbit_string_t _M0L6_2atmpS2890;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS754) {
    return (moonbit_string_t)moonbit_string_literal_68.data;
  }
  if (_M0L4signS756) {
    _M0L1sS755 = (moonbit_string_t)moonbit_string_literal_69.data;
  } else {
    _M0L1sS755 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS757) {
    moonbit_string_t _M0L6_2atmpS2891;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2891
    = moonbit_add_string(_M0L1sS755, (moonbit_string_t)moonbit_string_literal_70.data);
    moonbit_decref(_M0L1sS755);
    return _M0L6_2atmpS2891;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2890
  = moonbit_add_string(_M0L1sS755, (moonbit_string_t)moonbit_string_literal_71.data);
  moonbit_decref(_M0L1sS755);
  return _M0L6_2atmpS2890;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS753) {
  int32_t _M0L6_2atmpS2157;
  uint32_t _M0L6_2atmpS2156;
  uint32_t _M0L6_2atmpS2155;
  int32_t _M0L6_2atmpS2154;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2157 = _M0L1eS753 * 1217359;
  _M0L6_2atmpS2156 = *(uint32_t*)&_M0L6_2atmpS2157;
  _M0L6_2atmpS2155 = _M0L6_2atmpS2156 >> 19;
  _M0L6_2atmpS2154 = *(int32_t*)&_M0L6_2atmpS2155;
  return _M0L6_2atmpS2154 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS752,
  struct _M0TPB6Hasher* _M0L6hasherS751
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS751, _M0L4selfS752);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS750,
  struct _M0TPB6Hasher* _M0L6hasherS749
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS749, _M0L4selfS750);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS747,
  moonbit_string_t _M0L5valueS745
) {
  int32_t _M0L7_2abindS744;
  int32_t _M0L1iS746;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS744 = Moonbit_array_length(_M0L5valueS745);
  _M0L1iS746 = 0;
  while (1) {
    if (_M0L1iS746 < _M0L7_2abindS744) {
      int32_t _M0L6_2atmpS2152 = _M0L5valueS745[_M0L1iS746];
      int32_t _M0L6_2atmpS2151 = (int32_t)_M0L6_2atmpS2152;
      uint32_t _M0L6_2atmpS2150 = *(uint32_t*)&_M0L6_2atmpS2151;
      int32_t _M0L6_2atmpS2153;
      moonbit_incref(_M0L4selfS747);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS747, _M0L6_2atmpS2150);
      _M0L6_2atmpS2153 = _M0L1iS746 + 1;
      _M0L1iS746 = _M0L6_2atmpS2153;
      continue;
    } else {
      moonbit_decref(_M0L4selfS747);
      moonbit_decref(_M0L5valueS745);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS742,
  int32_t _M0L3idxS743
) {
  int32_t _M0L6_2atmpS2892;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2892 = _M0L4selfS742[_M0L3idxS743];
  moonbit_decref(_M0L4selfS742);
  return _M0L6_2atmpS2892;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS741) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS741;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS734
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2893;
  int32_t _M0L6_2acntS3122;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2149;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS733;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__* _closure_3231;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2144;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2893 = _M0L4selfS734->$5;
  _M0L6_2acntS3122 = Moonbit_object_header(_M0L4selfS734)->rc;
  if (_M0L6_2acntS3122 > 1) {
    int32_t _M0L11_2anew__cntS3124 = _M0L6_2acntS3122 - 1;
    Moonbit_object_header(_M0L4selfS734)->rc = _M0L11_2anew__cntS3124;
    if (_M0L8_2afieldS2893) {
      moonbit_incref(_M0L8_2afieldS2893);
    }
  } else if (_M0L6_2acntS3122 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3123 = _M0L4selfS734->$0;
    moonbit_decref(_M0L8_2afieldS3123);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS734);
  }
  _M0L4headS2149 = _M0L8_2afieldS2893;
  _M0L11curr__entryS733
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS733)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS733->$0 = _M0L4headS2149;
  _closure_3231
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__));
  Moonbit_object_header(_closure_3231)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__, $0) >> 2, 1, 0);
  _closure_3231->code = &_M0MPB3Map4iterGsRPB4JsonEC2145l591;
  _closure_3231->$0 = _M0L11curr__entryS733;
  _M0L6_2atmpS2144 = (struct _M0TWEOUsRPB4JsonE*)_closure_3231;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2144);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2145l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2146
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__* _M0L14_2acasted__envS2147;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS2899;
  int32_t _M0L6_2acntS3125;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS733;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2898;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS735;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2147
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2145__l591__*)_M0L6_2aenvS2146;
  _M0L8_2afieldS2899 = _M0L14_2acasted__envS2147->$0;
  _M0L6_2acntS3125 = Moonbit_object_header(_M0L14_2acasted__envS2147)->rc;
  if (_M0L6_2acntS3125 > 1) {
    int32_t _M0L11_2anew__cntS3126 = _M0L6_2acntS3125 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2147)->rc
    = _M0L11_2anew__cntS3126;
    moonbit_incref(_M0L8_2afieldS2899);
  } else if (_M0L6_2acntS3125 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2147);
  }
  _M0L11curr__entryS733 = _M0L8_2afieldS2899;
  _M0L8_2afieldS2898 = _M0L11curr__entryS733->$0;
  _M0L7_2abindS735 = _M0L8_2afieldS2898;
  if (_M0L7_2abindS735 == 0) {
    moonbit_decref(_M0L11curr__entryS733);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS736 = _M0L7_2abindS735;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS737 = _M0L7_2aSomeS736;
    moonbit_string_t _M0L8_2afieldS2897 = _M0L4_2axS737->$4;
    moonbit_string_t _M0L6_2akeyS738 = _M0L8_2afieldS2897;
    void* _M0L8_2afieldS2896 = _M0L4_2axS737->$5;
    void* _M0L8_2avalueS739 = _M0L8_2afieldS2896;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2895 = _M0L4_2axS737->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS740 = _M0L8_2afieldS2895;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2894 =
      _M0L11curr__entryS733->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2148;
    if (_M0L7_2anextS740) {
      moonbit_incref(_M0L7_2anextS740);
    }
    moonbit_incref(_M0L8_2avalueS739);
    moonbit_incref(_M0L6_2akeyS738);
    if (_M0L6_2aoldS2894) {
      moonbit_decref(_M0L6_2aoldS2894);
    }
    _M0L11curr__entryS733->$0 = _M0L7_2anextS740;
    moonbit_decref(_M0L11curr__entryS733);
    _M0L8_2atupleS2148
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2148)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2148->$0 = _M0L6_2akeyS738;
    _M0L8_2atupleS2148->$1 = _M0L8_2avalueS739;
    return _M0L8_2atupleS2148;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS732
) {
  int32_t _M0L8_2afieldS2900;
  int32_t _M0L4sizeS2143;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2900 = _M0L4selfS732->$1;
  moonbit_decref(_M0L4selfS732);
  _M0L4sizeS2143 = _M0L8_2afieldS2900;
  return _M0L4sizeS2143 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS719,
  int32_t _M0L3keyS715
) {
  int32_t _M0L4hashS714;
  int32_t _M0L14capacity__maskS2128;
  int32_t _M0L6_2atmpS2127;
  int32_t _M0L1iS716;
  int32_t _M0L3idxS717;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS714 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS715);
  _M0L14capacity__maskS2128 = _M0L4selfS719->$3;
  _M0L6_2atmpS2127 = _M0L4hashS714 & _M0L14capacity__maskS2128;
  _M0L1iS716 = 0;
  _M0L3idxS717 = _M0L6_2atmpS2127;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2904 =
      _M0L4selfS719->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2126 =
      _M0L8_2afieldS2904;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2903;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS718;
    if (
      _M0L3idxS717 < 0
      || _M0L3idxS717 >= Moonbit_array_length(_M0L7entriesS2126)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2903
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2126[
        _M0L3idxS717
      ];
    _M0L7_2abindS718 = _M0L6_2atmpS2903;
    if (_M0L7_2abindS718 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2115;
      if (_M0L7_2abindS718) {
        moonbit_incref(_M0L7_2abindS718);
      }
      moonbit_decref(_M0L4selfS719);
      if (_M0L7_2abindS718) {
        moonbit_decref(_M0L7_2abindS718);
      }
      _M0L6_2atmpS2115 = 0;
      return _M0L6_2atmpS2115;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS720 =
        _M0L7_2abindS718;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS721 =
        _M0L7_2aSomeS720;
      int32_t _M0L4hashS2117 = _M0L8_2aentryS721->$3;
      int32_t _if__result_3233;
      int32_t _M0L8_2afieldS2901;
      int32_t _M0L3pslS2120;
      int32_t _M0L6_2atmpS2122;
      int32_t _M0L6_2atmpS2124;
      int32_t _M0L14capacity__maskS2125;
      int32_t _M0L6_2atmpS2123;
      if (_M0L4hashS2117 == _M0L4hashS714) {
        int32_t _M0L3keyS2116 = _M0L8_2aentryS721->$4;
        _if__result_3233 = _M0L3keyS2116 == _M0L3keyS715;
      } else {
        _if__result_3233 = 0;
      }
      if (_if__result_3233) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2902;
        int32_t _M0L6_2acntS3127;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2119;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2118;
        moonbit_incref(_M0L8_2aentryS721);
        moonbit_decref(_M0L4selfS719);
        _M0L8_2afieldS2902 = _M0L8_2aentryS721->$5;
        _M0L6_2acntS3127 = Moonbit_object_header(_M0L8_2aentryS721)->rc;
        if (_M0L6_2acntS3127 > 1) {
          int32_t _M0L11_2anew__cntS3129 = _M0L6_2acntS3127 - 1;
          Moonbit_object_header(_M0L8_2aentryS721)->rc
          = _M0L11_2anew__cntS3129;
          moonbit_incref(_M0L8_2afieldS2902);
        } else if (_M0L6_2acntS3127 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3128 =
            _M0L8_2aentryS721->$1;
          if (_M0L8_2afieldS3128) {
            moonbit_decref(_M0L8_2afieldS3128);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS721);
        }
        _M0L5valueS2119 = _M0L8_2afieldS2902;
        _M0L6_2atmpS2118 = _M0L5valueS2119;
        return _M0L6_2atmpS2118;
      } else {
        moonbit_incref(_M0L8_2aentryS721);
      }
      _M0L8_2afieldS2901 = _M0L8_2aentryS721->$2;
      moonbit_decref(_M0L8_2aentryS721);
      _M0L3pslS2120 = _M0L8_2afieldS2901;
      if (_M0L1iS716 > _M0L3pslS2120) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2121;
        moonbit_decref(_M0L4selfS719);
        _M0L6_2atmpS2121 = 0;
        return _M0L6_2atmpS2121;
      }
      _M0L6_2atmpS2122 = _M0L1iS716 + 1;
      _M0L6_2atmpS2124 = _M0L3idxS717 + 1;
      _M0L14capacity__maskS2125 = _M0L4selfS719->$3;
      _M0L6_2atmpS2123 = _M0L6_2atmpS2124 & _M0L14capacity__maskS2125;
      _M0L1iS716 = _M0L6_2atmpS2122;
      _M0L3idxS717 = _M0L6_2atmpS2123;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS728,
  moonbit_string_t _M0L3keyS724
) {
  int32_t _M0L4hashS723;
  int32_t _M0L14capacity__maskS2142;
  int32_t _M0L6_2atmpS2141;
  int32_t _M0L1iS725;
  int32_t _M0L3idxS726;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS724);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS723 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS724);
  _M0L14capacity__maskS2142 = _M0L4selfS728->$3;
  _M0L6_2atmpS2141 = _M0L4hashS723 & _M0L14capacity__maskS2142;
  _M0L1iS725 = 0;
  _M0L3idxS726 = _M0L6_2atmpS2141;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2910 =
      _M0L4selfS728->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2140 =
      _M0L8_2afieldS2910;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2909;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS727;
    if (
      _M0L3idxS726 < 0
      || _M0L3idxS726 >= Moonbit_array_length(_M0L7entriesS2140)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2909
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2140[
        _M0L3idxS726
      ];
    _M0L7_2abindS727 = _M0L6_2atmpS2909;
    if (_M0L7_2abindS727 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2129;
      if (_M0L7_2abindS727) {
        moonbit_incref(_M0L7_2abindS727);
      }
      moonbit_decref(_M0L4selfS728);
      if (_M0L7_2abindS727) {
        moonbit_decref(_M0L7_2abindS727);
      }
      moonbit_decref(_M0L3keyS724);
      _M0L6_2atmpS2129 = 0;
      return _M0L6_2atmpS2129;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS729 =
        _M0L7_2abindS727;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS730 =
        _M0L7_2aSomeS729;
      int32_t _M0L4hashS2131 = _M0L8_2aentryS730->$3;
      int32_t _if__result_3235;
      int32_t _M0L8_2afieldS2905;
      int32_t _M0L3pslS2134;
      int32_t _M0L6_2atmpS2136;
      int32_t _M0L6_2atmpS2138;
      int32_t _M0L14capacity__maskS2139;
      int32_t _M0L6_2atmpS2137;
      if (_M0L4hashS2131 == _M0L4hashS723) {
        moonbit_string_t _M0L8_2afieldS2908 = _M0L8_2aentryS730->$4;
        moonbit_string_t _M0L3keyS2130 = _M0L8_2afieldS2908;
        int32_t _M0L6_2atmpS2907;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2907
        = moonbit_val_array_equal(_M0L3keyS2130, _M0L3keyS724);
        _if__result_3235 = _M0L6_2atmpS2907;
      } else {
        _if__result_3235 = 0;
      }
      if (_if__result_3235) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2906;
        int32_t _M0L6_2acntS3130;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2133;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2132;
        moonbit_incref(_M0L8_2aentryS730);
        moonbit_decref(_M0L4selfS728);
        moonbit_decref(_M0L3keyS724);
        _M0L8_2afieldS2906 = _M0L8_2aentryS730->$5;
        _M0L6_2acntS3130 = Moonbit_object_header(_M0L8_2aentryS730)->rc;
        if (_M0L6_2acntS3130 > 1) {
          int32_t _M0L11_2anew__cntS3133 = _M0L6_2acntS3130 - 1;
          Moonbit_object_header(_M0L8_2aentryS730)->rc
          = _M0L11_2anew__cntS3133;
          moonbit_incref(_M0L8_2afieldS2906);
        } else if (_M0L6_2acntS3130 == 1) {
          moonbit_string_t _M0L8_2afieldS3132 = _M0L8_2aentryS730->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3131;
          moonbit_decref(_M0L8_2afieldS3132);
          _M0L8_2afieldS3131 = _M0L8_2aentryS730->$1;
          if (_M0L8_2afieldS3131) {
            moonbit_decref(_M0L8_2afieldS3131);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS730);
        }
        _M0L5valueS2133 = _M0L8_2afieldS2906;
        _M0L6_2atmpS2132 = _M0L5valueS2133;
        return _M0L6_2atmpS2132;
      } else {
        moonbit_incref(_M0L8_2aentryS730);
      }
      _M0L8_2afieldS2905 = _M0L8_2aentryS730->$2;
      moonbit_decref(_M0L8_2aentryS730);
      _M0L3pslS2134 = _M0L8_2afieldS2905;
      if (_M0L1iS725 > _M0L3pslS2134) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2135;
        moonbit_decref(_M0L4selfS728);
        moonbit_decref(_M0L3keyS724);
        _M0L6_2atmpS2135 = 0;
        return _M0L6_2atmpS2135;
      }
      _M0L6_2atmpS2136 = _M0L1iS725 + 1;
      _M0L6_2atmpS2138 = _M0L3idxS726 + 1;
      _M0L14capacity__maskS2139 = _M0L4selfS728->$3;
      _M0L6_2atmpS2137 = _M0L6_2atmpS2138 & _M0L14capacity__maskS2139;
      _M0L1iS725 = _M0L6_2atmpS2136;
      _M0L3idxS726 = _M0L6_2atmpS2137;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS699
) {
  int32_t _M0L6lengthS698;
  int32_t _M0Lm8capacityS700;
  int32_t _M0L6_2atmpS2092;
  int32_t _M0L6_2atmpS2091;
  int32_t _M0L6_2atmpS2102;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS701;
  int32_t _M0L3endS2100;
  int32_t _M0L5startS2101;
  int32_t _M0L7_2abindS702;
  int32_t _M0L2__S703;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS699.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS698
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS699);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS700 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS698);
  _M0L6_2atmpS2092 = _M0Lm8capacityS700;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2091 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2092);
  if (_M0L6lengthS698 > _M0L6_2atmpS2091) {
    int32_t _M0L6_2atmpS2093 = _M0Lm8capacityS700;
    _M0Lm8capacityS700 = _M0L6_2atmpS2093 * 2;
  }
  _M0L6_2atmpS2102 = _M0Lm8capacityS700;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS701
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2102);
  _M0L3endS2100 = _M0L3arrS699.$2;
  _M0L5startS2101 = _M0L3arrS699.$1;
  _M0L7_2abindS702 = _M0L3endS2100 - _M0L5startS2101;
  _M0L2__S703 = 0;
  while (1) {
    if (_M0L2__S703 < _M0L7_2abindS702) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2914 =
        _M0L3arrS699.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2097 =
        _M0L8_2afieldS2914;
      int32_t _M0L5startS2099 = _M0L3arrS699.$1;
      int32_t _M0L6_2atmpS2098 = _M0L5startS2099 + _M0L2__S703;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2913 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2097[
          _M0L6_2atmpS2098
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS704 =
        _M0L6_2atmpS2913;
      moonbit_string_t _M0L8_2afieldS2912 = _M0L1eS704->$0;
      moonbit_string_t _M0L6_2atmpS2094 = _M0L8_2afieldS2912;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2911 =
        _M0L1eS704->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2095 =
        _M0L8_2afieldS2911;
      int32_t _M0L6_2atmpS2096;
      moonbit_incref(_M0L6_2atmpS2095);
      moonbit_incref(_M0L6_2atmpS2094);
      moonbit_incref(_M0L1mS701);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS701, _M0L6_2atmpS2094, _M0L6_2atmpS2095);
      _M0L6_2atmpS2096 = _M0L2__S703 + 1;
      _M0L2__S703 = _M0L6_2atmpS2096;
      continue;
    } else {
      moonbit_decref(_M0L3arrS699.$0);
    }
    break;
  }
  return _M0L1mS701;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS707
) {
  int32_t _M0L6lengthS706;
  int32_t _M0Lm8capacityS708;
  int32_t _M0L6_2atmpS2104;
  int32_t _M0L6_2atmpS2103;
  int32_t _M0L6_2atmpS2114;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS709;
  int32_t _M0L3endS2112;
  int32_t _M0L5startS2113;
  int32_t _M0L7_2abindS710;
  int32_t _M0L2__S711;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS707.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS706
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS707);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS708 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS706);
  _M0L6_2atmpS2104 = _M0Lm8capacityS708;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2103 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2104);
  if (_M0L6lengthS706 > _M0L6_2atmpS2103) {
    int32_t _M0L6_2atmpS2105 = _M0Lm8capacityS708;
    _M0Lm8capacityS708 = _M0L6_2atmpS2105 * 2;
  }
  _M0L6_2atmpS2114 = _M0Lm8capacityS708;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS709
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2114);
  _M0L3endS2112 = _M0L3arrS707.$2;
  _M0L5startS2113 = _M0L3arrS707.$1;
  _M0L7_2abindS710 = _M0L3endS2112 - _M0L5startS2113;
  _M0L2__S711 = 0;
  while (1) {
    if (_M0L2__S711 < _M0L7_2abindS710) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2917 =
        _M0L3arrS707.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2109 =
        _M0L8_2afieldS2917;
      int32_t _M0L5startS2111 = _M0L3arrS707.$1;
      int32_t _M0L6_2atmpS2110 = _M0L5startS2111 + _M0L2__S711;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2916 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2109[
          _M0L6_2atmpS2110
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS712 = _M0L6_2atmpS2916;
      int32_t _M0L6_2atmpS2106 = _M0L1eS712->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2915 =
        _M0L1eS712->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2107 =
        _M0L8_2afieldS2915;
      int32_t _M0L6_2atmpS2108;
      moonbit_incref(_M0L6_2atmpS2107);
      moonbit_incref(_M0L1mS709);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS709, _M0L6_2atmpS2106, _M0L6_2atmpS2107);
      _M0L6_2atmpS2108 = _M0L2__S711 + 1;
      _M0L2__S711 = _M0L6_2atmpS2108;
      continue;
    } else {
      moonbit_decref(_M0L3arrS707.$0);
    }
    break;
  }
  return _M0L1mS709;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS692,
  moonbit_string_t _M0L3keyS693,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS694
) {
  int32_t _M0L6_2atmpS2089;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS693);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2089 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS693);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS692, _M0L3keyS693, _M0L5valueS694, _M0L6_2atmpS2089);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS695,
  int32_t _M0L3keyS696,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS697
) {
  int32_t _M0L6_2atmpS2090;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2090 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS696);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS695, _M0L3keyS696, _M0L5valueS697, _M0L6_2atmpS2090);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS671
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2924;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS670;
  int32_t _M0L8capacityS2081;
  int32_t _M0L13new__capacityS672;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2076;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2075;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2923;
  int32_t _M0L6_2atmpS2077;
  int32_t _M0L8capacityS2079;
  int32_t _M0L6_2atmpS2078;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2080;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2922;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS673;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2924 = _M0L4selfS671->$5;
  _M0L9old__headS670 = _M0L8_2afieldS2924;
  _M0L8capacityS2081 = _M0L4selfS671->$2;
  _M0L13new__capacityS672 = _M0L8capacityS2081 << 1;
  _M0L6_2atmpS2076 = 0;
  _M0L6_2atmpS2075
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS672, _M0L6_2atmpS2076);
  _M0L6_2aoldS2923 = _M0L4selfS671->$0;
  if (_M0L9old__headS670) {
    moonbit_incref(_M0L9old__headS670);
  }
  moonbit_decref(_M0L6_2aoldS2923);
  _M0L4selfS671->$0 = _M0L6_2atmpS2075;
  _M0L4selfS671->$2 = _M0L13new__capacityS672;
  _M0L6_2atmpS2077 = _M0L13new__capacityS672 - 1;
  _M0L4selfS671->$3 = _M0L6_2atmpS2077;
  _M0L8capacityS2079 = _M0L4selfS671->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2078 = _M0FPB21calc__grow__threshold(_M0L8capacityS2079);
  _M0L4selfS671->$4 = _M0L6_2atmpS2078;
  _M0L4selfS671->$1 = 0;
  _M0L6_2atmpS2080 = 0;
  _M0L6_2aoldS2922 = _M0L4selfS671->$5;
  if (_M0L6_2aoldS2922) {
    moonbit_decref(_M0L6_2aoldS2922);
  }
  _M0L4selfS671->$5 = _M0L6_2atmpS2080;
  _M0L4selfS671->$6 = -1;
  _M0L8_2aparamS673 = _M0L9old__headS670;
  while (1) {
    if (_M0L8_2aparamS673 == 0) {
      if (_M0L8_2aparamS673) {
        moonbit_decref(_M0L8_2aparamS673);
      }
      moonbit_decref(_M0L4selfS671);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS674 =
        _M0L8_2aparamS673;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS675 =
        _M0L7_2aSomeS674;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2921 =
        _M0L4_2axS675->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS676 =
        _M0L8_2afieldS2921;
      moonbit_string_t _M0L8_2afieldS2920 = _M0L4_2axS675->$4;
      moonbit_string_t _M0L6_2akeyS677 = _M0L8_2afieldS2920;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2919 =
        _M0L4_2axS675->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS678 =
        _M0L8_2afieldS2919;
      int32_t _M0L8_2afieldS2918 = _M0L4_2axS675->$3;
      int32_t _M0L6_2acntS3134 = Moonbit_object_header(_M0L4_2axS675)->rc;
      int32_t _M0L7_2ahashS679;
      if (_M0L6_2acntS3134 > 1) {
        int32_t _M0L11_2anew__cntS3135 = _M0L6_2acntS3134 - 1;
        Moonbit_object_header(_M0L4_2axS675)->rc = _M0L11_2anew__cntS3135;
        moonbit_incref(_M0L8_2avalueS678);
        moonbit_incref(_M0L6_2akeyS677);
        if (_M0L7_2anextS676) {
          moonbit_incref(_M0L7_2anextS676);
        }
      } else if (_M0L6_2acntS3134 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS675);
      }
      _M0L7_2ahashS679 = _M0L8_2afieldS2918;
      moonbit_incref(_M0L4selfS671);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS671, _M0L6_2akeyS677, _M0L8_2avalueS678, _M0L7_2ahashS679);
      _M0L8_2aparamS673 = _M0L7_2anextS676;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS682
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2930;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS681;
  int32_t _M0L8capacityS2088;
  int32_t _M0L13new__capacityS683;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2083;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2082;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2929;
  int32_t _M0L6_2atmpS2084;
  int32_t _M0L8capacityS2086;
  int32_t _M0L6_2atmpS2085;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2087;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2928;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS684;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2930 = _M0L4selfS682->$5;
  _M0L9old__headS681 = _M0L8_2afieldS2930;
  _M0L8capacityS2088 = _M0L4selfS682->$2;
  _M0L13new__capacityS683 = _M0L8capacityS2088 << 1;
  _M0L6_2atmpS2083 = 0;
  _M0L6_2atmpS2082
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS683, _M0L6_2atmpS2083);
  _M0L6_2aoldS2929 = _M0L4selfS682->$0;
  if (_M0L9old__headS681) {
    moonbit_incref(_M0L9old__headS681);
  }
  moonbit_decref(_M0L6_2aoldS2929);
  _M0L4selfS682->$0 = _M0L6_2atmpS2082;
  _M0L4selfS682->$2 = _M0L13new__capacityS683;
  _M0L6_2atmpS2084 = _M0L13new__capacityS683 - 1;
  _M0L4selfS682->$3 = _M0L6_2atmpS2084;
  _M0L8capacityS2086 = _M0L4selfS682->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2085 = _M0FPB21calc__grow__threshold(_M0L8capacityS2086);
  _M0L4selfS682->$4 = _M0L6_2atmpS2085;
  _M0L4selfS682->$1 = 0;
  _M0L6_2atmpS2087 = 0;
  _M0L6_2aoldS2928 = _M0L4selfS682->$5;
  if (_M0L6_2aoldS2928) {
    moonbit_decref(_M0L6_2aoldS2928);
  }
  _M0L4selfS682->$5 = _M0L6_2atmpS2087;
  _M0L4selfS682->$6 = -1;
  _M0L8_2aparamS684 = _M0L9old__headS681;
  while (1) {
    if (_M0L8_2aparamS684 == 0) {
      if (_M0L8_2aparamS684) {
        moonbit_decref(_M0L8_2aparamS684);
      }
      moonbit_decref(_M0L4selfS682);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS685 =
        _M0L8_2aparamS684;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS686 =
        _M0L7_2aSomeS685;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2927 =
        _M0L4_2axS686->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS687 =
        _M0L8_2afieldS2927;
      int32_t _M0L6_2akeyS688 = _M0L4_2axS686->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2926 =
        _M0L4_2axS686->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS689 =
        _M0L8_2afieldS2926;
      int32_t _M0L8_2afieldS2925 = _M0L4_2axS686->$3;
      int32_t _M0L6_2acntS3136 = Moonbit_object_header(_M0L4_2axS686)->rc;
      int32_t _M0L7_2ahashS690;
      if (_M0L6_2acntS3136 > 1) {
        int32_t _M0L11_2anew__cntS3137 = _M0L6_2acntS3136 - 1;
        Moonbit_object_header(_M0L4_2axS686)->rc = _M0L11_2anew__cntS3137;
        moonbit_incref(_M0L8_2avalueS689);
        if (_M0L7_2anextS687) {
          moonbit_incref(_M0L7_2anextS687);
        }
      } else if (_M0L6_2acntS3136 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS686);
      }
      _M0L7_2ahashS690 = _M0L8_2afieldS2925;
      moonbit_incref(_M0L4selfS682);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS682, _M0L6_2akeyS688, _M0L8_2avalueS689, _M0L7_2ahashS690);
      _M0L8_2aparamS684 = _M0L7_2anextS687;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS641,
  moonbit_string_t _M0L3keyS647,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS648,
  int32_t _M0L4hashS643
) {
  int32_t _M0L14capacity__maskS2056;
  int32_t _M0L6_2atmpS2055;
  int32_t _M0L3pslS638;
  int32_t _M0L3idxS639;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2056 = _M0L4selfS641->$3;
  _M0L6_2atmpS2055 = _M0L4hashS643 & _M0L14capacity__maskS2056;
  _M0L3pslS638 = 0;
  _M0L3idxS639 = _M0L6_2atmpS2055;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2935 =
      _M0L4selfS641->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2054 =
      _M0L8_2afieldS2935;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2934;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS640;
    if (
      _M0L3idxS639 < 0
      || _M0L3idxS639 >= Moonbit_array_length(_M0L7entriesS2054)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2934
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2054[
        _M0L3idxS639
      ];
    _M0L7_2abindS640 = _M0L6_2atmpS2934;
    if (_M0L7_2abindS640 == 0) {
      int32_t _M0L4sizeS2039 = _M0L4selfS641->$1;
      int32_t _M0L8grow__atS2040 = _M0L4selfS641->$4;
      int32_t _M0L7_2abindS644;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS645;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS646;
      if (_M0L4sizeS2039 >= _M0L8grow__atS2040) {
        int32_t _M0L14capacity__maskS2042;
        int32_t _M0L6_2atmpS2041;
        moonbit_incref(_M0L4selfS641);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS641);
        _M0L14capacity__maskS2042 = _M0L4selfS641->$3;
        _M0L6_2atmpS2041 = _M0L4hashS643 & _M0L14capacity__maskS2042;
        _M0L3pslS638 = 0;
        _M0L3idxS639 = _M0L6_2atmpS2041;
        continue;
      }
      _M0L7_2abindS644 = _M0L4selfS641->$6;
      _M0L7_2abindS645 = 0;
      _M0L5entryS646
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS646)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS646->$0 = _M0L7_2abindS644;
      _M0L5entryS646->$1 = _M0L7_2abindS645;
      _M0L5entryS646->$2 = _M0L3pslS638;
      _M0L5entryS646->$3 = _M0L4hashS643;
      _M0L5entryS646->$4 = _M0L3keyS647;
      _M0L5entryS646->$5 = _M0L5valueS648;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS641, _M0L3idxS639, _M0L5entryS646);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS649 =
        _M0L7_2abindS640;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS650 =
        _M0L7_2aSomeS649;
      int32_t _M0L4hashS2044 = _M0L14_2acurr__entryS650->$3;
      int32_t _if__result_3241;
      int32_t _M0L3pslS2045;
      int32_t _M0L6_2atmpS2050;
      int32_t _M0L6_2atmpS2052;
      int32_t _M0L14capacity__maskS2053;
      int32_t _M0L6_2atmpS2051;
      if (_M0L4hashS2044 == _M0L4hashS643) {
        moonbit_string_t _M0L8_2afieldS2933 = _M0L14_2acurr__entryS650->$4;
        moonbit_string_t _M0L3keyS2043 = _M0L8_2afieldS2933;
        int32_t _M0L6_2atmpS2932;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2932
        = moonbit_val_array_equal(_M0L3keyS2043, _M0L3keyS647);
        _if__result_3241 = _M0L6_2atmpS2932;
      } else {
        _if__result_3241 = 0;
      }
      if (_if__result_3241) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2931;
        moonbit_incref(_M0L14_2acurr__entryS650);
        moonbit_decref(_M0L3keyS647);
        moonbit_decref(_M0L4selfS641);
        _M0L6_2aoldS2931 = _M0L14_2acurr__entryS650->$5;
        moonbit_decref(_M0L6_2aoldS2931);
        _M0L14_2acurr__entryS650->$5 = _M0L5valueS648;
        moonbit_decref(_M0L14_2acurr__entryS650);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS650);
      }
      _M0L3pslS2045 = _M0L14_2acurr__entryS650->$2;
      if (_M0L3pslS638 > _M0L3pslS2045) {
        int32_t _M0L4sizeS2046 = _M0L4selfS641->$1;
        int32_t _M0L8grow__atS2047 = _M0L4selfS641->$4;
        int32_t _M0L7_2abindS651;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS652;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS653;
        if (_M0L4sizeS2046 >= _M0L8grow__atS2047) {
          int32_t _M0L14capacity__maskS2049;
          int32_t _M0L6_2atmpS2048;
          moonbit_decref(_M0L14_2acurr__entryS650);
          moonbit_incref(_M0L4selfS641);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS641);
          _M0L14capacity__maskS2049 = _M0L4selfS641->$3;
          _M0L6_2atmpS2048 = _M0L4hashS643 & _M0L14capacity__maskS2049;
          _M0L3pslS638 = 0;
          _M0L3idxS639 = _M0L6_2atmpS2048;
          continue;
        }
        moonbit_incref(_M0L4selfS641);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS641, _M0L3idxS639, _M0L14_2acurr__entryS650);
        _M0L7_2abindS651 = _M0L4selfS641->$6;
        _M0L7_2abindS652 = 0;
        _M0L5entryS653
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS653)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS653->$0 = _M0L7_2abindS651;
        _M0L5entryS653->$1 = _M0L7_2abindS652;
        _M0L5entryS653->$2 = _M0L3pslS638;
        _M0L5entryS653->$3 = _M0L4hashS643;
        _M0L5entryS653->$4 = _M0L3keyS647;
        _M0L5entryS653->$5 = _M0L5valueS648;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS641, _M0L3idxS639, _M0L5entryS653);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS650);
      }
      _M0L6_2atmpS2050 = _M0L3pslS638 + 1;
      _M0L6_2atmpS2052 = _M0L3idxS639 + 1;
      _M0L14capacity__maskS2053 = _M0L4selfS641->$3;
      _M0L6_2atmpS2051 = _M0L6_2atmpS2052 & _M0L14capacity__maskS2053;
      _M0L3pslS638 = _M0L6_2atmpS2050;
      _M0L3idxS639 = _M0L6_2atmpS2051;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS657,
  int32_t _M0L3keyS663,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS664,
  int32_t _M0L4hashS659
) {
  int32_t _M0L14capacity__maskS2074;
  int32_t _M0L6_2atmpS2073;
  int32_t _M0L3pslS654;
  int32_t _M0L3idxS655;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2074 = _M0L4selfS657->$3;
  _M0L6_2atmpS2073 = _M0L4hashS659 & _M0L14capacity__maskS2074;
  _M0L3pslS654 = 0;
  _M0L3idxS655 = _M0L6_2atmpS2073;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2938 =
      _M0L4selfS657->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2072 =
      _M0L8_2afieldS2938;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2937;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS656;
    if (
      _M0L3idxS655 < 0
      || _M0L3idxS655 >= Moonbit_array_length(_M0L7entriesS2072)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2937
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2072[
        _M0L3idxS655
      ];
    _M0L7_2abindS656 = _M0L6_2atmpS2937;
    if (_M0L7_2abindS656 == 0) {
      int32_t _M0L4sizeS2057 = _M0L4selfS657->$1;
      int32_t _M0L8grow__atS2058 = _M0L4selfS657->$4;
      int32_t _M0L7_2abindS660;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS661;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS662;
      if (_M0L4sizeS2057 >= _M0L8grow__atS2058) {
        int32_t _M0L14capacity__maskS2060;
        int32_t _M0L6_2atmpS2059;
        moonbit_incref(_M0L4selfS657);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS657);
        _M0L14capacity__maskS2060 = _M0L4selfS657->$3;
        _M0L6_2atmpS2059 = _M0L4hashS659 & _M0L14capacity__maskS2060;
        _M0L3pslS654 = 0;
        _M0L3idxS655 = _M0L6_2atmpS2059;
        continue;
      }
      _M0L7_2abindS660 = _M0L4selfS657->$6;
      _M0L7_2abindS661 = 0;
      _M0L5entryS662
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS662)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS662->$0 = _M0L7_2abindS660;
      _M0L5entryS662->$1 = _M0L7_2abindS661;
      _M0L5entryS662->$2 = _M0L3pslS654;
      _M0L5entryS662->$3 = _M0L4hashS659;
      _M0L5entryS662->$4 = _M0L3keyS663;
      _M0L5entryS662->$5 = _M0L5valueS664;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS657, _M0L3idxS655, _M0L5entryS662);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS665 =
        _M0L7_2abindS656;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS666 =
        _M0L7_2aSomeS665;
      int32_t _M0L4hashS2062 = _M0L14_2acurr__entryS666->$3;
      int32_t _if__result_3243;
      int32_t _M0L3pslS2063;
      int32_t _M0L6_2atmpS2068;
      int32_t _M0L6_2atmpS2070;
      int32_t _M0L14capacity__maskS2071;
      int32_t _M0L6_2atmpS2069;
      if (_M0L4hashS2062 == _M0L4hashS659) {
        int32_t _M0L3keyS2061 = _M0L14_2acurr__entryS666->$4;
        _if__result_3243 = _M0L3keyS2061 == _M0L3keyS663;
      } else {
        _if__result_3243 = 0;
      }
      if (_if__result_3243) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2936;
        moonbit_incref(_M0L14_2acurr__entryS666);
        moonbit_decref(_M0L4selfS657);
        _M0L6_2aoldS2936 = _M0L14_2acurr__entryS666->$5;
        moonbit_decref(_M0L6_2aoldS2936);
        _M0L14_2acurr__entryS666->$5 = _M0L5valueS664;
        moonbit_decref(_M0L14_2acurr__entryS666);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS666);
      }
      _M0L3pslS2063 = _M0L14_2acurr__entryS666->$2;
      if (_M0L3pslS654 > _M0L3pslS2063) {
        int32_t _M0L4sizeS2064 = _M0L4selfS657->$1;
        int32_t _M0L8grow__atS2065 = _M0L4selfS657->$4;
        int32_t _M0L7_2abindS667;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS668;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS669;
        if (_M0L4sizeS2064 >= _M0L8grow__atS2065) {
          int32_t _M0L14capacity__maskS2067;
          int32_t _M0L6_2atmpS2066;
          moonbit_decref(_M0L14_2acurr__entryS666);
          moonbit_incref(_M0L4selfS657);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS657);
          _M0L14capacity__maskS2067 = _M0L4selfS657->$3;
          _M0L6_2atmpS2066 = _M0L4hashS659 & _M0L14capacity__maskS2067;
          _M0L3pslS654 = 0;
          _M0L3idxS655 = _M0L6_2atmpS2066;
          continue;
        }
        moonbit_incref(_M0L4selfS657);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS657, _M0L3idxS655, _M0L14_2acurr__entryS666);
        _M0L7_2abindS667 = _M0L4selfS657->$6;
        _M0L7_2abindS668 = 0;
        _M0L5entryS669
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS669)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS669->$0 = _M0L7_2abindS667;
        _M0L5entryS669->$1 = _M0L7_2abindS668;
        _M0L5entryS669->$2 = _M0L3pslS654;
        _M0L5entryS669->$3 = _M0L4hashS659;
        _M0L5entryS669->$4 = _M0L3keyS663;
        _M0L5entryS669->$5 = _M0L5valueS664;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS657, _M0L3idxS655, _M0L5entryS669);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS666);
      }
      _M0L6_2atmpS2068 = _M0L3pslS654 + 1;
      _M0L6_2atmpS2070 = _M0L3idxS655 + 1;
      _M0L14capacity__maskS2071 = _M0L4selfS657->$3;
      _M0L6_2atmpS2069 = _M0L6_2atmpS2070 & _M0L14capacity__maskS2071;
      _M0L3pslS654 = _M0L6_2atmpS2068;
      _M0L3idxS655 = _M0L6_2atmpS2069;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS622,
  int32_t _M0L3idxS627,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS626
) {
  int32_t _M0L3pslS2022;
  int32_t _M0L6_2atmpS2018;
  int32_t _M0L6_2atmpS2020;
  int32_t _M0L14capacity__maskS2021;
  int32_t _M0L6_2atmpS2019;
  int32_t _M0L3pslS618;
  int32_t _M0L3idxS619;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS620;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2022 = _M0L5entryS626->$2;
  _M0L6_2atmpS2018 = _M0L3pslS2022 + 1;
  _M0L6_2atmpS2020 = _M0L3idxS627 + 1;
  _M0L14capacity__maskS2021 = _M0L4selfS622->$3;
  _M0L6_2atmpS2019 = _M0L6_2atmpS2020 & _M0L14capacity__maskS2021;
  _M0L3pslS618 = _M0L6_2atmpS2018;
  _M0L3idxS619 = _M0L6_2atmpS2019;
  _M0L5entryS620 = _M0L5entryS626;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2940 =
      _M0L4selfS622->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2017 =
      _M0L8_2afieldS2940;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2939;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS621;
    if (
      _M0L3idxS619 < 0
      || _M0L3idxS619 >= Moonbit_array_length(_M0L7entriesS2017)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2939
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2017[
        _M0L3idxS619
      ];
    _M0L7_2abindS621 = _M0L6_2atmpS2939;
    if (_M0L7_2abindS621 == 0) {
      _M0L5entryS620->$2 = _M0L3pslS618;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS622, _M0L5entryS620, _M0L3idxS619);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS624 =
        _M0L7_2abindS621;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS625 =
        _M0L7_2aSomeS624;
      int32_t _M0L3pslS2007 = _M0L14_2acurr__entryS625->$2;
      if (_M0L3pslS618 > _M0L3pslS2007) {
        int32_t _M0L3pslS2012;
        int32_t _M0L6_2atmpS2008;
        int32_t _M0L6_2atmpS2010;
        int32_t _M0L14capacity__maskS2011;
        int32_t _M0L6_2atmpS2009;
        _M0L5entryS620->$2 = _M0L3pslS618;
        moonbit_incref(_M0L14_2acurr__entryS625);
        moonbit_incref(_M0L4selfS622);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS622, _M0L5entryS620, _M0L3idxS619);
        _M0L3pslS2012 = _M0L14_2acurr__entryS625->$2;
        _M0L6_2atmpS2008 = _M0L3pslS2012 + 1;
        _M0L6_2atmpS2010 = _M0L3idxS619 + 1;
        _M0L14capacity__maskS2011 = _M0L4selfS622->$3;
        _M0L6_2atmpS2009 = _M0L6_2atmpS2010 & _M0L14capacity__maskS2011;
        _M0L3pslS618 = _M0L6_2atmpS2008;
        _M0L3idxS619 = _M0L6_2atmpS2009;
        _M0L5entryS620 = _M0L14_2acurr__entryS625;
        continue;
      } else {
        int32_t _M0L6_2atmpS2013 = _M0L3pslS618 + 1;
        int32_t _M0L6_2atmpS2015 = _M0L3idxS619 + 1;
        int32_t _M0L14capacity__maskS2016 = _M0L4selfS622->$3;
        int32_t _M0L6_2atmpS2014 =
          _M0L6_2atmpS2015 & _M0L14capacity__maskS2016;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3245 =
          _M0L5entryS620;
        _M0L3pslS618 = _M0L6_2atmpS2013;
        _M0L3idxS619 = _M0L6_2atmpS2014;
        _M0L5entryS620 = _tmp_3245;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS632,
  int32_t _M0L3idxS637,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS636
) {
  int32_t _M0L3pslS2038;
  int32_t _M0L6_2atmpS2034;
  int32_t _M0L6_2atmpS2036;
  int32_t _M0L14capacity__maskS2037;
  int32_t _M0L6_2atmpS2035;
  int32_t _M0L3pslS628;
  int32_t _M0L3idxS629;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS630;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2038 = _M0L5entryS636->$2;
  _M0L6_2atmpS2034 = _M0L3pslS2038 + 1;
  _M0L6_2atmpS2036 = _M0L3idxS637 + 1;
  _M0L14capacity__maskS2037 = _M0L4selfS632->$3;
  _M0L6_2atmpS2035 = _M0L6_2atmpS2036 & _M0L14capacity__maskS2037;
  _M0L3pslS628 = _M0L6_2atmpS2034;
  _M0L3idxS629 = _M0L6_2atmpS2035;
  _M0L5entryS630 = _M0L5entryS636;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2942 =
      _M0L4selfS632->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2033 =
      _M0L8_2afieldS2942;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2941;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS631;
    if (
      _M0L3idxS629 < 0
      || _M0L3idxS629 >= Moonbit_array_length(_M0L7entriesS2033)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2941
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2033[
        _M0L3idxS629
      ];
    _M0L7_2abindS631 = _M0L6_2atmpS2941;
    if (_M0L7_2abindS631 == 0) {
      _M0L5entryS630->$2 = _M0L3pslS628;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS632, _M0L5entryS630, _M0L3idxS629);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS634 =
        _M0L7_2abindS631;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS635 =
        _M0L7_2aSomeS634;
      int32_t _M0L3pslS2023 = _M0L14_2acurr__entryS635->$2;
      if (_M0L3pslS628 > _M0L3pslS2023) {
        int32_t _M0L3pslS2028;
        int32_t _M0L6_2atmpS2024;
        int32_t _M0L6_2atmpS2026;
        int32_t _M0L14capacity__maskS2027;
        int32_t _M0L6_2atmpS2025;
        _M0L5entryS630->$2 = _M0L3pslS628;
        moonbit_incref(_M0L14_2acurr__entryS635);
        moonbit_incref(_M0L4selfS632);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS632, _M0L5entryS630, _M0L3idxS629);
        _M0L3pslS2028 = _M0L14_2acurr__entryS635->$2;
        _M0L6_2atmpS2024 = _M0L3pslS2028 + 1;
        _M0L6_2atmpS2026 = _M0L3idxS629 + 1;
        _M0L14capacity__maskS2027 = _M0L4selfS632->$3;
        _M0L6_2atmpS2025 = _M0L6_2atmpS2026 & _M0L14capacity__maskS2027;
        _M0L3pslS628 = _M0L6_2atmpS2024;
        _M0L3idxS629 = _M0L6_2atmpS2025;
        _M0L5entryS630 = _M0L14_2acurr__entryS635;
        continue;
      } else {
        int32_t _M0L6_2atmpS2029 = _M0L3pslS628 + 1;
        int32_t _M0L6_2atmpS2031 = _M0L3idxS629 + 1;
        int32_t _M0L14capacity__maskS2032 = _M0L4selfS632->$3;
        int32_t _M0L6_2atmpS2030 =
          _M0L6_2atmpS2031 & _M0L14capacity__maskS2032;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3247 =
          _M0L5entryS630;
        _M0L3pslS628 = _M0L6_2atmpS2029;
        _M0L3idxS629 = _M0L6_2atmpS2030;
        _M0L5entryS630 = _tmp_3247;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS606,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS608,
  int32_t _M0L8new__idxS607
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2945;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2003;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2004;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2944;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2943;
  int32_t _M0L6_2acntS3138;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS609;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2945 = _M0L4selfS606->$0;
  _M0L7entriesS2003 = _M0L8_2afieldS2945;
  moonbit_incref(_M0L5entryS608);
  _M0L6_2atmpS2004 = _M0L5entryS608;
  if (
    _M0L8new__idxS607 < 0
    || _M0L8new__idxS607 >= Moonbit_array_length(_M0L7entriesS2003)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2944
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2003[
      _M0L8new__idxS607
    ];
  if (_M0L6_2aoldS2944) {
    moonbit_decref(_M0L6_2aoldS2944);
  }
  _M0L7entriesS2003[_M0L8new__idxS607] = _M0L6_2atmpS2004;
  _M0L8_2afieldS2943 = _M0L5entryS608->$1;
  _M0L6_2acntS3138 = Moonbit_object_header(_M0L5entryS608)->rc;
  if (_M0L6_2acntS3138 > 1) {
    int32_t _M0L11_2anew__cntS3141 = _M0L6_2acntS3138 - 1;
    Moonbit_object_header(_M0L5entryS608)->rc = _M0L11_2anew__cntS3141;
    if (_M0L8_2afieldS2943) {
      moonbit_incref(_M0L8_2afieldS2943);
    }
  } else if (_M0L6_2acntS3138 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3140 =
      _M0L5entryS608->$5;
    moonbit_string_t _M0L8_2afieldS3139;
    moonbit_decref(_M0L8_2afieldS3140);
    _M0L8_2afieldS3139 = _M0L5entryS608->$4;
    moonbit_decref(_M0L8_2afieldS3139);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS608);
  }
  _M0L7_2abindS609 = _M0L8_2afieldS2943;
  if (_M0L7_2abindS609 == 0) {
    if (_M0L7_2abindS609) {
      moonbit_decref(_M0L7_2abindS609);
    }
    _M0L4selfS606->$6 = _M0L8new__idxS607;
    moonbit_decref(_M0L4selfS606);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS610;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS611;
    moonbit_decref(_M0L4selfS606);
    _M0L7_2aSomeS610 = _M0L7_2abindS609;
    _M0L7_2anextS611 = _M0L7_2aSomeS610;
    _M0L7_2anextS611->$0 = _M0L8new__idxS607;
    moonbit_decref(_M0L7_2anextS611);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS612,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS614,
  int32_t _M0L8new__idxS613
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2948;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2005;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2006;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2947;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2946;
  int32_t _M0L6_2acntS3142;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS615;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2948 = _M0L4selfS612->$0;
  _M0L7entriesS2005 = _M0L8_2afieldS2948;
  moonbit_incref(_M0L5entryS614);
  _M0L6_2atmpS2006 = _M0L5entryS614;
  if (
    _M0L8new__idxS613 < 0
    || _M0L8new__idxS613 >= Moonbit_array_length(_M0L7entriesS2005)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2947
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2005[
      _M0L8new__idxS613
    ];
  if (_M0L6_2aoldS2947) {
    moonbit_decref(_M0L6_2aoldS2947);
  }
  _M0L7entriesS2005[_M0L8new__idxS613] = _M0L6_2atmpS2006;
  _M0L8_2afieldS2946 = _M0L5entryS614->$1;
  _M0L6_2acntS3142 = Moonbit_object_header(_M0L5entryS614)->rc;
  if (_M0L6_2acntS3142 > 1) {
    int32_t _M0L11_2anew__cntS3144 = _M0L6_2acntS3142 - 1;
    Moonbit_object_header(_M0L5entryS614)->rc = _M0L11_2anew__cntS3144;
    if (_M0L8_2afieldS2946) {
      moonbit_incref(_M0L8_2afieldS2946);
    }
  } else if (_M0L6_2acntS3142 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3143 =
      _M0L5entryS614->$5;
    moonbit_decref(_M0L8_2afieldS3143);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS614);
  }
  _M0L7_2abindS615 = _M0L8_2afieldS2946;
  if (_M0L7_2abindS615 == 0) {
    if (_M0L7_2abindS615) {
      moonbit_decref(_M0L7_2abindS615);
    }
    _M0L4selfS612->$6 = _M0L8new__idxS613;
    moonbit_decref(_M0L4selfS612);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS616;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS617;
    moonbit_decref(_M0L4selfS612);
    _M0L7_2aSomeS616 = _M0L7_2abindS615;
    _M0L7_2anextS617 = _M0L7_2aSomeS616;
    _M0L7_2anextS617->$0 = _M0L8new__idxS613;
    moonbit_decref(_M0L7_2anextS617);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS599,
  int32_t _M0L3idxS601,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS600
) {
  int32_t _M0L7_2abindS598;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2950;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1990;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1991;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2949;
  int32_t _M0L4sizeS1993;
  int32_t _M0L6_2atmpS1992;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS598 = _M0L4selfS599->$6;
  switch (_M0L7_2abindS598) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1985;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2951;
      moonbit_incref(_M0L5entryS600);
      _M0L6_2atmpS1985 = _M0L5entryS600;
      _M0L6_2aoldS2951 = _M0L4selfS599->$5;
      if (_M0L6_2aoldS2951) {
        moonbit_decref(_M0L6_2aoldS2951);
      }
      _M0L4selfS599->$5 = _M0L6_2atmpS1985;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2954 =
        _M0L4selfS599->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1989 =
        _M0L8_2afieldS2954;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2953;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1988;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1986;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1987;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2952;
      if (
        _M0L7_2abindS598 < 0
        || _M0L7_2abindS598 >= Moonbit_array_length(_M0L7entriesS1989)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2953
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1989[
          _M0L7_2abindS598
        ];
      _M0L6_2atmpS1988 = _M0L6_2atmpS2953;
      if (_M0L6_2atmpS1988) {
        moonbit_incref(_M0L6_2atmpS1988);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1986
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1988);
      moonbit_incref(_M0L5entryS600);
      _M0L6_2atmpS1987 = _M0L5entryS600;
      _M0L6_2aoldS2952 = _M0L6_2atmpS1986->$1;
      if (_M0L6_2aoldS2952) {
        moonbit_decref(_M0L6_2aoldS2952);
      }
      _M0L6_2atmpS1986->$1 = _M0L6_2atmpS1987;
      moonbit_decref(_M0L6_2atmpS1986);
      break;
    }
  }
  _M0L4selfS599->$6 = _M0L3idxS601;
  _M0L8_2afieldS2950 = _M0L4selfS599->$0;
  _M0L7entriesS1990 = _M0L8_2afieldS2950;
  _M0L6_2atmpS1991 = _M0L5entryS600;
  if (
    _M0L3idxS601 < 0
    || _M0L3idxS601 >= Moonbit_array_length(_M0L7entriesS1990)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2949
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1990[
      _M0L3idxS601
    ];
  if (_M0L6_2aoldS2949) {
    moonbit_decref(_M0L6_2aoldS2949);
  }
  _M0L7entriesS1990[_M0L3idxS601] = _M0L6_2atmpS1991;
  _M0L4sizeS1993 = _M0L4selfS599->$1;
  _M0L6_2atmpS1992 = _M0L4sizeS1993 + 1;
  _M0L4selfS599->$1 = _M0L6_2atmpS1992;
  moonbit_decref(_M0L4selfS599);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS603,
  int32_t _M0L3idxS605,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS604
) {
  int32_t _M0L7_2abindS602;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2956;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1999;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2000;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2955;
  int32_t _M0L4sizeS2002;
  int32_t _M0L6_2atmpS2001;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS602 = _M0L4selfS603->$6;
  switch (_M0L7_2abindS602) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1994;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2957;
      moonbit_incref(_M0L5entryS604);
      _M0L6_2atmpS1994 = _M0L5entryS604;
      _M0L6_2aoldS2957 = _M0L4selfS603->$5;
      if (_M0L6_2aoldS2957) {
        moonbit_decref(_M0L6_2aoldS2957);
      }
      _M0L4selfS603->$5 = _M0L6_2atmpS1994;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2960 =
        _M0L4selfS603->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1998 =
        _M0L8_2afieldS2960;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2959;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1997;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1995;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1996;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2958;
      if (
        _M0L7_2abindS602 < 0
        || _M0L7_2abindS602 >= Moonbit_array_length(_M0L7entriesS1998)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2959
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1998[
          _M0L7_2abindS602
        ];
      _M0L6_2atmpS1997 = _M0L6_2atmpS2959;
      if (_M0L6_2atmpS1997) {
        moonbit_incref(_M0L6_2atmpS1997);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1995
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1997);
      moonbit_incref(_M0L5entryS604);
      _M0L6_2atmpS1996 = _M0L5entryS604;
      _M0L6_2aoldS2958 = _M0L6_2atmpS1995->$1;
      if (_M0L6_2aoldS2958) {
        moonbit_decref(_M0L6_2aoldS2958);
      }
      _M0L6_2atmpS1995->$1 = _M0L6_2atmpS1996;
      moonbit_decref(_M0L6_2atmpS1995);
      break;
    }
  }
  _M0L4selfS603->$6 = _M0L3idxS605;
  _M0L8_2afieldS2956 = _M0L4selfS603->$0;
  _M0L7entriesS1999 = _M0L8_2afieldS2956;
  _M0L6_2atmpS2000 = _M0L5entryS604;
  if (
    _M0L3idxS605 < 0
    || _M0L3idxS605 >= Moonbit_array_length(_M0L7entriesS1999)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2955
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1999[
      _M0L3idxS605
    ];
  if (_M0L6_2aoldS2955) {
    moonbit_decref(_M0L6_2aoldS2955);
  }
  _M0L7entriesS1999[_M0L3idxS605] = _M0L6_2atmpS2000;
  _M0L4sizeS2002 = _M0L4selfS603->$1;
  _M0L6_2atmpS2001 = _M0L4sizeS2002 + 1;
  _M0L4selfS603->$1 = _M0L6_2atmpS2001;
  moonbit_decref(_M0L4selfS603);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS587
) {
  int32_t _M0L8capacityS586;
  int32_t _M0L7_2abindS588;
  int32_t _M0L7_2abindS589;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1983;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS590;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS591;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3248;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS586
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS587);
  _M0L7_2abindS588 = _M0L8capacityS586 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS589 = _M0FPB21calc__grow__threshold(_M0L8capacityS586);
  _M0L6_2atmpS1983 = 0;
  _M0L7_2abindS590
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS586, _M0L6_2atmpS1983);
  _M0L7_2abindS591 = 0;
  _block_3248
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3248)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3248->$0 = _M0L7_2abindS590;
  _block_3248->$1 = 0;
  _block_3248->$2 = _M0L8capacityS586;
  _block_3248->$3 = _M0L7_2abindS588;
  _block_3248->$4 = _M0L7_2abindS589;
  _block_3248->$5 = _M0L7_2abindS591;
  _block_3248->$6 = -1;
  return _block_3248;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS593
) {
  int32_t _M0L8capacityS592;
  int32_t _M0L7_2abindS594;
  int32_t _M0L7_2abindS595;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1984;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS596;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS597;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3249;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS592
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS593);
  _M0L7_2abindS594 = _M0L8capacityS592 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS595 = _M0FPB21calc__grow__threshold(_M0L8capacityS592);
  _M0L6_2atmpS1984 = 0;
  _M0L7_2abindS596
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS592, _M0L6_2atmpS1984);
  _M0L7_2abindS597 = 0;
  _block_3249
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3249)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3249->$0 = _M0L7_2abindS596;
  _block_3249->$1 = 0;
  _block_3249->$2 = _M0L8capacityS592;
  _block_3249->$3 = _M0L7_2abindS594;
  _block_3249->$4 = _M0L7_2abindS595;
  _block_3249->$5 = _M0L7_2abindS597;
  _block_3249->$6 = -1;
  return _block_3249;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS585) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS585 >= 0) {
    int32_t _M0L6_2atmpS1982;
    int32_t _M0L6_2atmpS1981;
    int32_t _M0L6_2atmpS1980;
    int32_t _M0L6_2atmpS1979;
    if (_M0L4selfS585 <= 1) {
      return 1;
    }
    if (_M0L4selfS585 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1982 = _M0L4selfS585 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1981 = moonbit_clz32(_M0L6_2atmpS1982);
    _M0L6_2atmpS1980 = _M0L6_2atmpS1981 - 1;
    _M0L6_2atmpS1979 = 2147483647 >> (_M0L6_2atmpS1980 & 31);
    return _M0L6_2atmpS1979 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS584) {
  int32_t _M0L6_2atmpS1978;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1978 = _M0L8capacityS584 * 13;
  return _M0L6_2atmpS1978 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS580
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS580 == 0) {
    if (_M0L4selfS580) {
      moonbit_decref(_M0L4selfS580);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS581 =
      _M0L4selfS580;
    return _M0L7_2aSomeS581;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS582
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS582 == 0) {
    if (_M0L4selfS582) {
      moonbit_decref(_M0L4selfS582);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS583 =
      _M0L4selfS582;
    return _M0L7_2aSomeS583;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS579
) {
  moonbit_string_t* _M0L6_2atmpS1977;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1977 = _M0L4selfS579;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1977);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS575,
  int32_t _M0L5indexS576
) {
  uint64_t* _M0L6_2atmpS1975;
  uint64_t _M0L6_2atmpS2961;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1975 = _M0L4selfS575;
  if (
    _M0L5indexS576 < 0
    || _M0L5indexS576 >= Moonbit_array_length(_M0L6_2atmpS1975)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2961 = (uint64_t)_M0L6_2atmpS1975[_M0L5indexS576];
  moonbit_decref(_M0L6_2atmpS1975);
  return _M0L6_2atmpS2961;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS577,
  int32_t _M0L5indexS578
) {
  uint32_t* _M0L6_2atmpS1976;
  uint32_t _M0L6_2atmpS2962;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1976 = _M0L4selfS577;
  if (
    _M0L5indexS578 < 0
    || _M0L5indexS578 >= Moonbit_array_length(_M0L6_2atmpS1976)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2962 = (uint32_t)_M0L6_2atmpS1976[_M0L5indexS578];
  moonbit_decref(_M0L6_2atmpS1976);
  return _M0L6_2atmpS2962;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS574
) {
  moonbit_string_t* _M0L6_2atmpS1973;
  int32_t _M0L6_2atmpS2963;
  int32_t _M0L6_2atmpS1974;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1972;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS574);
  _M0L6_2atmpS1973 = _M0L4selfS574;
  _M0L6_2atmpS2963 = Moonbit_array_length(_M0L4selfS574);
  moonbit_decref(_M0L4selfS574);
  _M0L6_2atmpS1974 = _M0L6_2atmpS2963;
  _M0L6_2atmpS1972
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1974, _M0L6_2atmpS1973
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1972);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS572
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS571;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__* _closure_3250;
  struct _M0TWEOs* _M0L6_2atmpS1960;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS571
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS571)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS571->$0 = 0;
  _closure_3250
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__));
  Moonbit_object_header(_closure_3250)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__, $0_0) >> 2, 2, 0);
  _closure_3250->code = &_M0MPC15array9ArrayView4iterGsEC1961l570;
  _closure_3250->$0_0 = _M0L4selfS572.$0;
  _closure_3250->$0_1 = _M0L4selfS572.$1;
  _closure_3250->$0_2 = _M0L4selfS572.$2;
  _closure_3250->$1 = _M0L1iS571;
  _M0L6_2atmpS1960 = (struct _M0TWEOs*)_closure_3250;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1960);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1961l570(
  struct _M0TWEOs* _M0L6_2aenvS1962
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__* _M0L14_2acasted__envS1963;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2968;
  struct _M0TPC13ref3RefGiE* _M0L1iS571;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2967;
  int32_t _M0L6_2acntS3145;
  struct _M0TPB9ArrayViewGsE _M0L4selfS572;
  int32_t _M0L3valS1964;
  int32_t _M0L6_2atmpS1965;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1963
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1961__l570__*)_M0L6_2aenvS1962;
  _M0L8_2afieldS2968 = _M0L14_2acasted__envS1963->$1;
  _M0L1iS571 = _M0L8_2afieldS2968;
  _M0L8_2afieldS2967
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1963->$0_1,
      _M0L14_2acasted__envS1963->$0_2,
      _M0L14_2acasted__envS1963->$0_0
  };
  _M0L6_2acntS3145 = Moonbit_object_header(_M0L14_2acasted__envS1963)->rc;
  if (_M0L6_2acntS3145 > 1) {
    int32_t _M0L11_2anew__cntS3146 = _M0L6_2acntS3145 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1963)->rc
    = _M0L11_2anew__cntS3146;
    moonbit_incref(_M0L1iS571);
    moonbit_incref(_M0L8_2afieldS2967.$0);
  } else if (_M0L6_2acntS3145 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1963);
  }
  _M0L4selfS572 = _M0L8_2afieldS2967;
  _M0L3valS1964 = _M0L1iS571->$0;
  moonbit_incref(_M0L4selfS572.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1965 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS572);
  if (_M0L3valS1964 < _M0L6_2atmpS1965) {
    moonbit_string_t* _M0L8_2afieldS2966 = _M0L4selfS572.$0;
    moonbit_string_t* _M0L3bufS1968 = _M0L8_2afieldS2966;
    int32_t _M0L8_2afieldS2965 = _M0L4selfS572.$1;
    int32_t _M0L5startS1970 = _M0L8_2afieldS2965;
    int32_t _M0L3valS1971 = _M0L1iS571->$0;
    int32_t _M0L6_2atmpS1969 = _M0L5startS1970 + _M0L3valS1971;
    moonbit_string_t _M0L6_2atmpS2964 =
      (moonbit_string_t)_M0L3bufS1968[_M0L6_2atmpS1969];
    moonbit_string_t _M0L4elemS573;
    int32_t _M0L3valS1967;
    int32_t _M0L6_2atmpS1966;
    moonbit_incref(_M0L6_2atmpS2964);
    moonbit_decref(_M0L3bufS1968);
    _M0L4elemS573 = _M0L6_2atmpS2964;
    _M0L3valS1967 = _M0L1iS571->$0;
    _M0L6_2atmpS1966 = _M0L3valS1967 + 1;
    _M0L1iS571->$0 = _M0L6_2atmpS1966;
    moonbit_decref(_M0L1iS571);
    return _M0L4elemS573;
  } else {
    moonbit_decref(_M0L4selfS572.$0);
    moonbit_decref(_M0L1iS571);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS570
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS570;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS569,
  struct _M0TPB6Logger _M0L6loggerS568
) {
  moonbit_string_t _M0L6_2atmpS1959;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1959
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS569, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS568.$0->$method_0(_M0L6loggerS568.$1, _M0L6_2atmpS1959);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS567,
  struct _M0TPB6Logger _M0L6loggerS566
) {
  moonbit_string_t _M0L6_2atmpS1958;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1958 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS567, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS566.$0->$method_0(_M0L6loggerS566.$1, _M0L6_2atmpS1958);
  return 0;
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS564,
  struct _M0TPB6Logger _M0L6loggerS565
) {
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L4selfS564) {
    #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS565.$0->$method_0(_M0L6loggerS565.$1, (moonbit_string_t)moonbit_string_literal_11.data);
  } else {
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS565.$0->$method_0(_M0L6loggerS565.$1, (moonbit_string_t)moonbit_string_literal_44.data);
  }
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS559) {
  int32_t _M0L3lenS558;
  struct _M0TPC13ref3RefGiE* _M0L5indexS560;
  struct _M0R38String_3a_3aiter_2eanon__u1942__l247__* _closure_3251;
  struct _M0TWEOc* _M0L6_2atmpS1941;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS558 = Moonbit_array_length(_M0L4selfS559);
  _M0L5indexS560
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS560)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS560->$0 = 0;
  _closure_3251
  = (struct _M0R38String_3a_3aiter_2eanon__u1942__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u1942__l247__));
  Moonbit_object_header(_closure_3251)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u1942__l247__, $0) >> 2, 2, 0);
  _closure_3251->code = &_M0MPC16string6String4iterC1942l247;
  _closure_3251->$0 = _M0L5indexS560;
  _closure_3251->$1 = _M0L4selfS559;
  _closure_3251->$2 = _M0L3lenS558;
  _M0L6_2atmpS1941 = (struct _M0TWEOc*)_closure_3251;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1941);
}

int32_t _M0MPC16string6String4iterC1942l247(
  struct _M0TWEOc* _M0L6_2aenvS1943
) {
  struct _M0R38String_3a_3aiter_2eanon__u1942__l247__* _M0L14_2acasted__envS1944;
  int32_t _M0L3lenS558;
  moonbit_string_t _M0L8_2afieldS2971;
  moonbit_string_t _M0L4selfS559;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2970;
  int32_t _M0L6_2acntS3147;
  struct _M0TPC13ref3RefGiE* _M0L5indexS560;
  int32_t _M0L3valS1945;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS1944
  = (struct _M0R38String_3a_3aiter_2eanon__u1942__l247__*)_M0L6_2aenvS1943;
  _M0L3lenS558 = _M0L14_2acasted__envS1944->$2;
  _M0L8_2afieldS2971 = _M0L14_2acasted__envS1944->$1;
  _M0L4selfS559 = _M0L8_2afieldS2971;
  _M0L8_2afieldS2970 = _M0L14_2acasted__envS1944->$0;
  _M0L6_2acntS3147 = Moonbit_object_header(_M0L14_2acasted__envS1944)->rc;
  if (_M0L6_2acntS3147 > 1) {
    int32_t _M0L11_2anew__cntS3148 = _M0L6_2acntS3147 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1944)->rc
    = _M0L11_2anew__cntS3148;
    moonbit_incref(_M0L4selfS559);
    moonbit_incref(_M0L8_2afieldS2970);
  } else if (_M0L6_2acntS3147 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS1944);
  }
  _M0L5indexS560 = _M0L8_2afieldS2970;
  _M0L3valS1945 = _M0L5indexS560->$0;
  if (_M0L3valS1945 < _M0L3lenS558) {
    int32_t _M0L3valS1957 = _M0L5indexS560->$0;
    int32_t _M0L2c1S561 = _M0L4selfS559[_M0L3valS1957];
    int32_t _if__result_3252;
    int32_t _M0L3valS1955;
    int32_t _M0L6_2atmpS1954;
    int32_t _M0L6_2atmpS1956;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S561)) {
      int32_t _M0L3valS1947 = _M0L5indexS560->$0;
      int32_t _M0L6_2atmpS1946 = _M0L3valS1947 + 1;
      _if__result_3252 = _M0L6_2atmpS1946 < _M0L3lenS558;
    } else {
      _if__result_3252 = 0;
    }
    if (_if__result_3252) {
      int32_t _M0L3valS1953 = _M0L5indexS560->$0;
      int32_t _M0L6_2atmpS1952 = _M0L3valS1953 + 1;
      int32_t _M0L6_2atmpS2969 = _M0L4selfS559[_M0L6_2atmpS1952];
      int32_t _M0L2c2S562;
      moonbit_decref(_M0L4selfS559);
      _M0L2c2S562 = _M0L6_2atmpS2969;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S562)) {
        int32_t _M0L6_2atmpS1950 = (int32_t)_M0L2c1S561;
        int32_t _M0L6_2atmpS1951 = (int32_t)_M0L2c2S562;
        int32_t _M0L1cS563;
        int32_t _M0L3valS1949;
        int32_t _M0L6_2atmpS1948;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS563
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1950, _M0L6_2atmpS1951);
        _M0L3valS1949 = _M0L5indexS560->$0;
        _M0L6_2atmpS1948 = _M0L3valS1949 + 2;
        _M0L5indexS560->$0 = _M0L6_2atmpS1948;
        moonbit_decref(_M0L5indexS560);
        return _M0L1cS563;
      }
    } else {
      moonbit_decref(_M0L4selfS559);
    }
    _M0L3valS1955 = _M0L5indexS560->$0;
    _M0L6_2atmpS1954 = _M0L3valS1955 + 1;
    _M0L5indexS560->$0 = _M0L6_2atmpS1954;
    moonbit_decref(_M0L5indexS560);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS1956 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S561);
    return _M0L6_2atmpS1956;
  } else {
    moonbit_decref(_M0L5indexS560);
    moonbit_decref(_M0L4selfS559);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS549,
  moonbit_string_t _M0L5valueS551
) {
  int32_t _M0L3lenS1926;
  moonbit_string_t* _M0L6_2atmpS1928;
  int32_t _M0L6_2atmpS2974;
  int32_t _M0L6_2atmpS1927;
  int32_t _M0L6lengthS550;
  moonbit_string_t* _M0L8_2afieldS2973;
  moonbit_string_t* _M0L3bufS1929;
  moonbit_string_t _M0L6_2aoldS2972;
  int32_t _M0L6_2atmpS1930;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1926 = _M0L4selfS549->$1;
  moonbit_incref(_M0L4selfS549);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1928 = _M0MPC15array5Array6bufferGsE(_M0L4selfS549);
  _M0L6_2atmpS2974 = Moonbit_array_length(_M0L6_2atmpS1928);
  moonbit_decref(_M0L6_2atmpS1928);
  _M0L6_2atmpS1927 = _M0L6_2atmpS2974;
  if (_M0L3lenS1926 == _M0L6_2atmpS1927) {
    moonbit_incref(_M0L4selfS549);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS549);
  }
  _M0L6lengthS550 = _M0L4selfS549->$1;
  _M0L8_2afieldS2973 = _M0L4selfS549->$0;
  _M0L3bufS1929 = _M0L8_2afieldS2973;
  _M0L6_2aoldS2972 = (moonbit_string_t)_M0L3bufS1929[_M0L6lengthS550];
  moonbit_decref(_M0L6_2aoldS2972);
  _M0L3bufS1929[_M0L6lengthS550] = _M0L5valueS551;
  _M0L6_2atmpS1930 = _M0L6lengthS550 + 1;
  _M0L4selfS549->$1 = _M0L6_2atmpS1930;
  moonbit_decref(_M0L4selfS549);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS552,
  struct _M0TUsiE* _M0L5valueS554
) {
  int32_t _M0L3lenS1931;
  struct _M0TUsiE** _M0L6_2atmpS1933;
  int32_t _M0L6_2atmpS2977;
  int32_t _M0L6_2atmpS1932;
  int32_t _M0L6lengthS553;
  struct _M0TUsiE** _M0L8_2afieldS2976;
  struct _M0TUsiE** _M0L3bufS1934;
  struct _M0TUsiE* _M0L6_2aoldS2975;
  int32_t _M0L6_2atmpS1935;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1931 = _M0L4selfS552->$1;
  moonbit_incref(_M0L4selfS552);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1933 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS552);
  _M0L6_2atmpS2977 = Moonbit_array_length(_M0L6_2atmpS1933);
  moonbit_decref(_M0L6_2atmpS1933);
  _M0L6_2atmpS1932 = _M0L6_2atmpS2977;
  if (_M0L3lenS1931 == _M0L6_2atmpS1932) {
    moonbit_incref(_M0L4selfS552);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS552);
  }
  _M0L6lengthS553 = _M0L4selfS552->$1;
  _M0L8_2afieldS2976 = _M0L4selfS552->$0;
  _M0L3bufS1934 = _M0L8_2afieldS2976;
  _M0L6_2aoldS2975 = (struct _M0TUsiE*)_M0L3bufS1934[_M0L6lengthS553];
  if (_M0L6_2aoldS2975) {
    moonbit_decref(_M0L6_2aoldS2975);
  }
  _M0L3bufS1934[_M0L6lengthS553] = _M0L5valueS554;
  _M0L6_2atmpS1935 = _M0L6lengthS553 + 1;
  _M0L4selfS552->$1 = _M0L6_2atmpS1935;
  moonbit_decref(_M0L4selfS552);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS555,
  void* _M0L5valueS557
) {
  int32_t _M0L3lenS1936;
  void** _M0L6_2atmpS1938;
  int32_t _M0L6_2atmpS2980;
  int32_t _M0L6_2atmpS1937;
  int32_t _M0L6lengthS556;
  void** _M0L8_2afieldS2979;
  void** _M0L3bufS1939;
  void* _M0L6_2aoldS2978;
  int32_t _M0L6_2atmpS1940;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1936 = _M0L4selfS555->$1;
  moonbit_incref(_M0L4selfS555);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1938
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS555);
  _M0L6_2atmpS2980 = Moonbit_array_length(_M0L6_2atmpS1938);
  moonbit_decref(_M0L6_2atmpS1938);
  _M0L6_2atmpS1937 = _M0L6_2atmpS2980;
  if (_M0L3lenS1936 == _M0L6_2atmpS1937) {
    moonbit_incref(_M0L4selfS555);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS555);
  }
  _M0L6lengthS556 = _M0L4selfS555->$1;
  _M0L8_2afieldS2979 = _M0L4selfS555->$0;
  _M0L3bufS1939 = _M0L8_2afieldS2979;
  _M0L6_2aoldS2978 = (void*)_M0L3bufS1939[_M0L6lengthS556];
  moonbit_decref(_M0L6_2aoldS2978);
  _M0L3bufS1939[_M0L6lengthS556] = _M0L5valueS557;
  _M0L6_2atmpS1940 = _M0L6lengthS556 + 1;
  _M0L4selfS555->$1 = _M0L6_2atmpS1940;
  moonbit_decref(_M0L4selfS555);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS541) {
  int32_t _M0L8old__capS540;
  int32_t _M0L8new__capS542;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS540 = _M0L4selfS541->$1;
  if (_M0L8old__capS540 == 0) {
    _M0L8new__capS542 = 8;
  } else {
    _M0L8new__capS542 = _M0L8old__capS540 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS541, _M0L8new__capS542);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS544
) {
  int32_t _M0L8old__capS543;
  int32_t _M0L8new__capS545;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS543 = _M0L4selfS544->$1;
  if (_M0L8old__capS543 == 0) {
    _M0L8new__capS545 = 8;
  } else {
    _M0L8new__capS545 = _M0L8old__capS543 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS544, _M0L8new__capS545);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS547
) {
  int32_t _M0L8old__capS546;
  int32_t _M0L8new__capS548;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS546 = _M0L4selfS547->$1;
  if (_M0L8old__capS546 == 0) {
    _M0L8new__capS548 = 8;
  } else {
    _M0L8new__capS548 = _M0L8old__capS546 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS547, _M0L8new__capS548);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS525,
  int32_t _M0L13new__capacityS523
) {
  moonbit_string_t* _M0L8new__bufS522;
  moonbit_string_t* _M0L8_2afieldS2982;
  moonbit_string_t* _M0L8old__bufS524;
  int32_t _M0L8old__capS526;
  int32_t _M0L9copy__lenS527;
  moonbit_string_t* _M0L6_2aoldS2981;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS522
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS523, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS2982 = _M0L4selfS525->$0;
  _M0L8old__bufS524 = _M0L8_2afieldS2982;
  _M0L8old__capS526 = Moonbit_array_length(_M0L8old__bufS524);
  if (_M0L8old__capS526 < _M0L13new__capacityS523) {
    _M0L9copy__lenS527 = _M0L8old__capS526;
  } else {
    _M0L9copy__lenS527 = _M0L13new__capacityS523;
  }
  moonbit_incref(_M0L8old__bufS524);
  moonbit_incref(_M0L8new__bufS522);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS522, 0, _M0L8old__bufS524, 0, _M0L9copy__lenS527);
  _M0L6_2aoldS2981 = _M0L4selfS525->$0;
  moonbit_decref(_M0L6_2aoldS2981);
  _M0L4selfS525->$0 = _M0L8new__bufS522;
  moonbit_decref(_M0L4selfS525);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS531,
  int32_t _M0L13new__capacityS529
) {
  struct _M0TUsiE** _M0L8new__bufS528;
  struct _M0TUsiE** _M0L8_2afieldS2984;
  struct _M0TUsiE** _M0L8old__bufS530;
  int32_t _M0L8old__capS532;
  int32_t _M0L9copy__lenS533;
  struct _M0TUsiE** _M0L6_2aoldS2983;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS528
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS529, 0);
  _M0L8_2afieldS2984 = _M0L4selfS531->$0;
  _M0L8old__bufS530 = _M0L8_2afieldS2984;
  _M0L8old__capS532 = Moonbit_array_length(_M0L8old__bufS530);
  if (_M0L8old__capS532 < _M0L13new__capacityS529) {
    _M0L9copy__lenS533 = _M0L8old__capS532;
  } else {
    _M0L9copy__lenS533 = _M0L13new__capacityS529;
  }
  moonbit_incref(_M0L8old__bufS530);
  moonbit_incref(_M0L8new__bufS528);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS528, 0, _M0L8old__bufS530, 0, _M0L9copy__lenS533);
  _M0L6_2aoldS2983 = _M0L4selfS531->$0;
  moonbit_decref(_M0L6_2aoldS2983);
  _M0L4selfS531->$0 = _M0L8new__bufS528;
  moonbit_decref(_M0L4selfS531);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS537,
  int32_t _M0L13new__capacityS535
) {
  void** _M0L8new__bufS534;
  void** _M0L8_2afieldS2986;
  void** _M0L8old__bufS536;
  int32_t _M0L8old__capS538;
  int32_t _M0L9copy__lenS539;
  void** _M0L6_2aoldS2985;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS534
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS535, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS2986 = _M0L4selfS537->$0;
  _M0L8old__bufS536 = _M0L8_2afieldS2986;
  _M0L8old__capS538 = Moonbit_array_length(_M0L8old__bufS536);
  if (_M0L8old__capS538 < _M0L13new__capacityS535) {
    _M0L9copy__lenS539 = _M0L8old__capS538;
  } else {
    _M0L9copy__lenS539 = _M0L13new__capacityS535;
  }
  moonbit_incref(_M0L8old__bufS536);
  moonbit_incref(_M0L8new__bufS534);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS534, 0, _M0L8old__bufS536, 0, _M0L9copy__lenS539);
  _M0L6_2aoldS2985 = _M0L4selfS537->$0;
  moonbit_decref(_M0L6_2aoldS2985);
  _M0L4selfS537->$0 = _M0L8new__bufS534;
  moonbit_decref(_M0L4selfS537);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS521
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS521 == 0) {
    moonbit_string_t* _M0L6_2atmpS1924 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3253 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3253)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3253->$0 = _M0L6_2atmpS1924;
    _block_3253->$1 = 0;
    return _block_3253;
  } else {
    moonbit_string_t* _M0L6_2atmpS1925 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS521, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3254 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3254)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3254->$0 = _M0L6_2atmpS1925;
    _block_3254->$1 = 0;
    return _block_3254;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS515,
  int32_t _M0L1nS514
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS514 <= 0) {
    moonbit_decref(_M0L4selfS515);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS514 == 1) {
    return _M0L4selfS515;
  } else {
    int32_t _M0L3lenS516 = Moonbit_array_length(_M0L4selfS515);
    int32_t _M0L6_2atmpS1923 = _M0L3lenS516 * _M0L1nS514;
    struct _M0TPB13StringBuilder* _M0L3bufS517;
    moonbit_string_t _M0L3strS518;
    int32_t _M0L2__S519;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS517 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1923);
    _M0L3strS518 = _M0L4selfS515;
    _M0L2__S519 = 0;
    while (1) {
      if (_M0L2__S519 < _M0L1nS514) {
        int32_t _M0L6_2atmpS1922;
        moonbit_incref(_M0L3strS518);
        moonbit_incref(_M0L3bufS517);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS517, _M0L3strS518);
        _M0L6_2atmpS1922 = _M0L2__S519 + 1;
        _M0L2__S519 = _M0L6_2atmpS1922;
        continue;
      } else {
        moonbit_decref(_M0L3strS518);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS517);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS512,
  struct _M0TPC16string10StringView _M0L3strS513
) {
  int32_t _M0L3lenS1910;
  int32_t _M0L6_2atmpS1912;
  int32_t _M0L6_2atmpS1911;
  int32_t _M0L6_2atmpS1909;
  moonbit_bytes_t _M0L8_2afieldS2987;
  moonbit_bytes_t _M0L4dataS1913;
  int32_t _M0L3lenS1914;
  moonbit_string_t _M0L6_2atmpS1915;
  int32_t _M0L6_2atmpS1916;
  int32_t _M0L6_2atmpS1917;
  int32_t _M0L3lenS1919;
  int32_t _M0L6_2atmpS1921;
  int32_t _M0L6_2atmpS1920;
  int32_t _M0L6_2atmpS1918;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1910 = _M0L4selfS512->$1;
  moonbit_incref(_M0L3strS513.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1912 = _M0MPC16string10StringView6length(_M0L3strS513);
  _M0L6_2atmpS1911 = _M0L6_2atmpS1912 * 2;
  _M0L6_2atmpS1909 = _M0L3lenS1910 + _M0L6_2atmpS1911;
  moonbit_incref(_M0L4selfS512);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS512, _M0L6_2atmpS1909);
  _M0L8_2afieldS2987 = _M0L4selfS512->$0;
  _M0L4dataS1913 = _M0L8_2afieldS2987;
  _M0L3lenS1914 = _M0L4selfS512->$1;
  moonbit_incref(_M0L4dataS1913);
  moonbit_incref(_M0L3strS513.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1915 = _M0MPC16string10StringView4data(_M0L3strS513);
  moonbit_incref(_M0L3strS513.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1916 = _M0MPC16string10StringView13start__offset(_M0L3strS513);
  moonbit_incref(_M0L3strS513.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1917 = _M0MPC16string10StringView6length(_M0L3strS513);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1913, _M0L3lenS1914, _M0L6_2atmpS1915, _M0L6_2atmpS1916, _M0L6_2atmpS1917);
  _M0L3lenS1919 = _M0L4selfS512->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1921 = _M0MPC16string10StringView6length(_M0L3strS513);
  _M0L6_2atmpS1920 = _M0L6_2atmpS1921 * 2;
  _M0L6_2atmpS1918 = _M0L3lenS1919 + _M0L6_2atmpS1920;
  _M0L4selfS512->$1 = _M0L6_2atmpS1918;
  moonbit_decref(_M0L4selfS512);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS504,
  int32_t _M0L3lenS507,
  int32_t _M0L13start__offsetS511,
  int64_t _M0L11end__offsetS502
) {
  int32_t _M0L11end__offsetS501;
  int32_t _M0L5indexS505;
  int32_t _M0L5countS506;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS502 == 4294967296ll) {
    _M0L11end__offsetS501 = Moonbit_array_length(_M0L4selfS504);
  } else {
    int64_t _M0L7_2aSomeS503 = _M0L11end__offsetS502;
    _M0L11end__offsetS501 = (int32_t)_M0L7_2aSomeS503;
  }
  _M0L5indexS505 = _M0L13start__offsetS511;
  _M0L5countS506 = 0;
  while (1) {
    int32_t _if__result_3257;
    if (_M0L5indexS505 < _M0L11end__offsetS501) {
      _if__result_3257 = _M0L5countS506 < _M0L3lenS507;
    } else {
      _if__result_3257 = 0;
    }
    if (_if__result_3257) {
      int32_t _M0L2c1S508 = _M0L4selfS504[_M0L5indexS505];
      int32_t _if__result_3258;
      int32_t _M0L6_2atmpS1907;
      int32_t _M0L6_2atmpS1908;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S508)) {
        int32_t _M0L6_2atmpS1903 = _M0L5indexS505 + 1;
        _if__result_3258 = _M0L6_2atmpS1903 < _M0L11end__offsetS501;
      } else {
        _if__result_3258 = 0;
      }
      if (_if__result_3258) {
        int32_t _M0L6_2atmpS1906 = _M0L5indexS505 + 1;
        int32_t _M0L2c2S509 = _M0L4selfS504[_M0L6_2atmpS1906];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S509)) {
          int32_t _M0L6_2atmpS1904 = _M0L5indexS505 + 2;
          int32_t _M0L6_2atmpS1905 = _M0L5countS506 + 1;
          _M0L5indexS505 = _M0L6_2atmpS1904;
          _M0L5countS506 = _M0L6_2atmpS1905;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_72.data, (moonbit_string_t)moonbit_string_literal_73.data);
        }
      }
      _M0L6_2atmpS1907 = _M0L5indexS505 + 1;
      _M0L6_2atmpS1908 = _M0L5countS506 + 1;
      _M0L5indexS505 = _M0L6_2atmpS1907;
      _M0L5countS506 = _M0L6_2atmpS1908;
      continue;
    } else {
      moonbit_decref(_M0L4selfS504);
      return _M0L5countS506 >= _M0L3lenS507;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS498
) {
  int32_t _M0L3endS1897;
  int32_t _M0L8_2afieldS2988;
  int32_t _M0L5startS1898;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1897 = _M0L4selfS498.$2;
  _M0L8_2afieldS2988 = _M0L4selfS498.$1;
  moonbit_decref(_M0L4selfS498.$0);
  _M0L5startS1898 = _M0L8_2afieldS2988;
  return _M0L3endS1897 - _M0L5startS1898;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS499
) {
  int32_t _M0L3endS1899;
  int32_t _M0L8_2afieldS2989;
  int32_t _M0L5startS1900;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1899 = _M0L4selfS499.$2;
  _M0L8_2afieldS2989 = _M0L4selfS499.$1;
  moonbit_decref(_M0L4selfS499.$0);
  _M0L5startS1900 = _M0L8_2afieldS2989;
  return _M0L3endS1899 - _M0L5startS1900;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS500
) {
  int32_t _M0L3endS1901;
  int32_t _M0L8_2afieldS2990;
  int32_t _M0L5startS1902;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1901 = _M0L4selfS500.$2;
  _M0L8_2afieldS2990 = _M0L4selfS500.$1;
  moonbit_decref(_M0L4selfS500.$0);
  _M0L5startS1902 = _M0L8_2afieldS2990;
  return _M0L3endS1901 - _M0L5startS1902;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS496,
  int64_t _M0L19start__offset_2eoptS494,
  int64_t _M0L11end__offsetS497
) {
  int32_t _M0L13start__offsetS493;
  if (_M0L19start__offset_2eoptS494 == 4294967296ll) {
    _M0L13start__offsetS493 = 0;
  } else {
    int64_t _M0L7_2aSomeS495 = _M0L19start__offset_2eoptS494;
    _M0L13start__offsetS493 = (int32_t)_M0L7_2aSomeS495;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS496, _M0L13start__offsetS493, _M0L11end__offsetS497);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS491,
  int32_t _M0L13start__offsetS492,
  int64_t _M0L11end__offsetS489
) {
  int32_t _M0L11end__offsetS488;
  int32_t _if__result_3259;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS489 == 4294967296ll) {
    _M0L11end__offsetS488 = Moonbit_array_length(_M0L4selfS491);
  } else {
    int64_t _M0L7_2aSomeS490 = _M0L11end__offsetS489;
    _M0L11end__offsetS488 = (int32_t)_M0L7_2aSomeS490;
  }
  if (_M0L13start__offsetS492 >= 0) {
    if (_M0L13start__offsetS492 <= _M0L11end__offsetS488) {
      int32_t _M0L6_2atmpS1896 = Moonbit_array_length(_M0L4selfS491);
      _if__result_3259 = _M0L11end__offsetS488 <= _M0L6_2atmpS1896;
    } else {
      _if__result_3259 = 0;
    }
  } else {
    _if__result_3259 = 0;
  }
  if (_if__result_3259) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS492,
                                                 _M0L11end__offsetS488,
                                                 _M0L4selfS491};
  } else {
    moonbit_decref(_M0L4selfS491);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_74.data, (moonbit_string_t)moonbit_string_literal_75.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS487
) {
  moonbit_string_t _M0L8_2afieldS2992;
  moonbit_string_t _M0L3strS1893;
  int32_t _M0L5startS1894;
  int32_t _M0L8_2afieldS2991;
  int32_t _M0L3endS1895;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2992 = _M0L4selfS487.$0;
  _M0L3strS1893 = _M0L8_2afieldS2992;
  _M0L5startS1894 = _M0L4selfS487.$1;
  _M0L8_2afieldS2991 = _M0L4selfS487.$2;
  _M0L3endS1895 = _M0L8_2afieldS2991;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1893, _M0L5startS1894, _M0L3endS1895);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS485,
  struct _M0TPB6Logger _M0L6loggerS486
) {
  moonbit_string_t _M0L8_2afieldS2994;
  moonbit_string_t _M0L3strS1890;
  int32_t _M0L5startS1891;
  int32_t _M0L8_2afieldS2993;
  int32_t _M0L3endS1892;
  moonbit_string_t _M0L6substrS484;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2994 = _M0L4selfS485.$0;
  _M0L3strS1890 = _M0L8_2afieldS2994;
  _M0L5startS1891 = _M0L4selfS485.$1;
  _M0L8_2afieldS2993 = _M0L4selfS485.$2;
  _M0L3endS1892 = _M0L8_2afieldS2993;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS484
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1890, _M0L5startS1891, _M0L3endS1892);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS484, _M0L6loggerS486);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS476,
  struct _M0TPB6Logger _M0L6loggerS474
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS475;
  int32_t _M0L3lenS477;
  int32_t _M0L1iS478;
  int32_t _M0L3segS479;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS474.$1) {
    moonbit_incref(_M0L6loggerS474.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 34);
  moonbit_incref(_M0L4selfS476);
  if (_M0L6loggerS474.$1) {
    moonbit_incref(_M0L6loggerS474.$1);
  }
  _M0L6_2aenvS475
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS475)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS475->$0 = _M0L4selfS476;
  _M0L6_2aenvS475->$1_0 = _M0L6loggerS474.$0;
  _M0L6_2aenvS475->$1_1 = _M0L6loggerS474.$1;
  _M0L3lenS477 = Moonbit_array_length(_M0L4selfS476);
  _M0L1iS478 = 0;
  _M0L3segS479 = 0;
  _2afor_480:;
  while (1) {
    int32_t _M0L4codeS481;
    int32_t _M0L1cS483;
    int32_t _M0L6_2atmpS1874;
    int32_t _M0L6_2atmpS1875;
    int32_t _M0L6_2atmpS1876;
    int32_t _tmp_3263;
    int32_t _tmp_3264;
    if (_M0L1iS478 >= _M0L3lenS477) {
      moonbit_decref(_M0L4selfS476);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
      break;
    }
    _M0L4codeS481 = _M0L4selfS476[_M0L1iS478];
    switch (_M0L4codeS481) {
      case 34: {
        _M0L1cS483 = _M0L4codeS481;
        goto join_482;
        break;
      }
      
      case 92: {
        _M0L1cS483 = _M0L4codeS481;
        goto join_482;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1877;
        int32_t _M0L6_2atmpS1878;
        moonbit_incref(_M0L6_2aenvS475);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_59.data);
        _M0L6_2atmpS1877 = _M0L1iS478 + 1;
        _M0L6_2atmpS1878 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS1877;
        _M0L3segS479 = _M0L6_2atmpS1878;
        goto _2afor_480;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1879;
        int32_t _M0L6_2atmpS1880;
        moonbit_incref(_M0L6_2aenvS475);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_60.data);
        _M0L6_2atmpS1879 = _M0L1iS478 + 1;
        _M0L6_2atmpS1880 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS1879;
        _M0L3segS479 = _M0L6_2atmpS1880;
        goto _2afor_480;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1881;
        int32_t _M0L6_2atmpS1882;
        moonbit_incref(_M0L6_2aenvS475);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_61.data);
        _M0L6_2atmpS1881 = _M0L1iS478 + 1;
        _M0L6_2atmpS1882 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS1881;
        _M0L3segS479 = _M0L6_2atmpS1882;
        goto _2afor_480;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1883;
        int32_t _M0L6_2atmpS1884;
        moonbit_incref(_M0L6_2aenvS475);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_62.data);
        _M0L6_2atmpS1883 = _M0L1iS478 + 1;
        _M0L6_2atmpS1884 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS1883;
        _M0L3segS479 = _M0L6_2atmpS1884;
        goto _2afor_480;
        break;
      }
      default: {
        if (_M0L4codeS481 < 32) {
          int32_t _M0L6_2atmpS1886;
          moonbit_string_t _M0L6_2atmpS1885;
          int32_t _M0L6_2atmpS1887;
          int32_t _M0L6_2atmpS1888;
          moonbit_incref(_M0L6_2aenvS475);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
          if (_M0L6loggerS474.$1) {
            moonbit_incref(_M0L6loggerS474.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_76.data);
          _M0L6_2atmpS1886 = _M0L4codeS481 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1885 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1886);
          if (_M0L6loggerS474.$1) {
            moonbit_incref(_M0L6loggerS474.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, _M0L6_2atmpS1885);
          if (_M0L6loggerS474.$1) {
            moonbit_incref(_M0L6loggerS474.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 125);
          _M0L6_2atmpS1887 = _M0L1iS478 + 1;
          _M0L6_2atmpS1888 = _M0L1iS478 + 1;
          _M0L1iS478 = _M0L6_2atmpS1887;
          _M0L3segS479 = _M0L6_2atmpS1888;
          goto _2afor_480;
        } else {
          int32_t _M0L6_2atmpS1889 = _M0L1iS478 + 1;
          int32_t _tmp_3262 = _M0L3segS479;
          _M0L1iS478 = _M0L6_2atmpS1889;
          _M0L3segS479 = _tmp_3262;
          goto _2afor_480;
        }
        break;
      }
    }
    goto joinlet_3261;
    join_482:;
    moonbit_incref(_M0L6_2aenvS475);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
    if (_M0L6loggerS474.$1) {
      moonbit_incref(_M0L6loggerS474.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1874 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS483);
    if (_M0L6loggerS474.$1) {
      moonbit_incref(_M0L6loggerS474.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, _M0L6_2atmpS1874);
    _M0L6_2atmpS1875 = _M0L1iS478 + 1;
    _M0L6_2atmpS1876 = _M0L1iS478 + 1;
    _M0L1iS478 = _M0L6_2atmpS1875;
    _M0L3segS479 = _M0L6_2atmpS1876;
    continue;
    joinlet_3261:;
    _tmp_3263 = _M0L1iS478;
    _tmp_3264 = _M0L3segS479;
    _M0L1iS478 = _tmp_3263;
    _M0L3segS479 = _tmp_3264;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS470,
  int32_t _M0L3segS473,
  int32_t _M0L1iS472
) {
  struct _M0TPB6Logger _M0L8_2afieldS2996;
  struct _M0TPB6Logger _M0L6loggerS469;
  moonbit_string_t _M0L8_2afieldS2995;
  int32_t _M0L6_2acntS3149;
  moonbit_string_t _M0L4selfS471;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS2996
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS470->$1_0, _M0L6_2aenvS470->$1_1
  };
  _M0L6loggerS469 = _M0L8_2afieldS2996;
  _M0L8_2afieldS2995 = _M0L6_2aenvS470->$0;
  _M0L6_2acntS3149 = Moonbit_object_header(_M0L6_2aenvS470)->rc;
  if (_M0L6_2acntS3149 > 1) {
    int32_t _M0L11_2anew__cntS3150 = _M0L6_2acntS3149 - 1;
    Moonbit_object_header(_M0L6_2aenvS470)->rc = _M0L11_2anew__cntS3150;
    if (_M0L6loggerS469.$1) {
      moonbit_incref(_M0L6loggerS469.$1);
    }
    moonbit_incref(_M0L8_2afieldS2995);
  } else if (_M0L6_2acntS3149 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS470);
  }
  _M0L4selfS471 = _M0L8_2afieldS2995;
  if (_M0L1iS472 > _M0L3segS473) {
    int32_t _M0L6_2atmpS1873 = _M0L1iS472 - _M0L3segS473;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS469.$0->$method_1(_M0L6loggerS469.$1, _M0L4selfS471, _M0L3segS473, _M0L6_2atmpS1873);
  } else {
    moonbit_decref(_M0L4selfS471);
    if (_M0L6loggerS469.$1) {
      moonbit_decref(_M0L6loggerS469.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS468) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS467;
  int32_t _M0L6_2atmpS1870;
  int32_t _M0L6_2atmpS1869;
  int32_t _M0L6_2atmpS1872;
  int32_t _M0L6_2atmpS1871;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1868;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS467 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1870 = _M0IPC14byte4BytePB3Div3div(_M0L1bS468, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1869
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1870);
  moonbit_incref(_M0L7_2aselfS467);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS467, _M0L6_2atmpS1869);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1872 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS468, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1871
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1872);
  moonbit_incref(_M0L7_2aselfS467);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS467, _M0L6_2atmpS1871);
  _M0L6_2atmpS1868 = _M0L7_2aselfS467;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1868);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS466) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS466 < 10) {
    int32_t _M0L6_2atmpS1865;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1865 = _M0IPC14byte4BytePB3Add3add(_M0L1iS466, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1865);
  } else {
    int32_t _M0L6_2atmpS1867;
    int32_t _M0L6_2atmpS1866;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1867 = _M0IPC14byte4BytePB3Add3add(_M0L1iS466, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1866 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1867, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1866);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS464,
  int32_t _M0L4thatS465
) {
  int32_t _M0L6_2atmpS1863;
  int32_t _M0L6_2atmpS1864;
  int32_t _M0L6_2atmpS1862;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1863 = (int32_t)_M0L4selfS464;
  _M0L6_2atmpS1864 = (int32_t)_M0L4thatS465;
  _M0L6_2atmpS1862 = _M0L6_2atmpS1863 - _M0L6_2atmpS1864;
  return _M0L6_2atmpS1862 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS462,
  int32_t _M0L4thatS463
) {
  int32_t _M0L6_2atmpS1860;
  int32_t _M0L6_2atmpS1861;
  int32_t _M0L6_2atmpS1859;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1860 = (int32_t)_M0L4selfS462;
  _M0L6_2atmpS1861 = (int32_t)_M0L4thatS463;
  _M0L6_2atmpS1859 = _M0L6_2atmpS1860 % _M0L6_2atmpS1861;
  return _M0L6_2atmpS1859 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS460,
  int32_t _M0L4thatS461
) {
  int32_t _M0L6_2atmpS1857;
  int32_t _M0L6_2atmpS1858;
  int32_t _M0L6_2atmpS1856;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1857 = (int32_t)_M0L4selfS460;
  _M0L6_2atmpS1858 = (int32_t)_M0L4thatS461;
  _M0L6_2atmpS1856 = _M0L6_2atmpS1857 / _M0L6_2atmpS1858;
  return _M0L6_2atmpS1856 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS458,
  int32_t _M0L4thatS459
) {
  int32_t _M0L6_2atmpS1854;
  int32_t _M0L6_2atmpS1855;
  int32_t _M0L6_2atmpS1853;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1854 = (int32_t)_M0L4selfS458;
  _M0L6_2atmpS1855 = (int32_t)_M0L4thatS459;
  _M0L6_2atmpS1853 = _M0L6_2atmpS1854 + _M0L6_2atmpS1855;
  return _M0L6_2atmpS1853 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS455,
  int32_t _M0L5startS453,
  int32_t _M0L3endS454
) {
  int32_t _if__result_3265;
  int32_t _M0L3lenS456;
  int32_t _M0L6_2atmpS1851;
  int32_t _M0L6_2atmpS1852;
  moonbit_bytes_t _M0L5bytesS457;
  moonbit_bytes_t _M0L6_2atmpS1850;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS453 == 0) {
    int32_t _M0L6_2atmpS1849 = Moonbit_array_length(_M0L3strS455);
    _if__result_3265 = _M0L3endS454 == _M0L6_2atmpS1849;
  } else {
    _if__result_3265 = 0;
  }
  if (_if__result_3265) {
    return _M0L3strS455;
  }
  _M0L3lenS456 = _M0L3endS454 - _M0L5startS453;
  _M0L6_2atmpS1851 = _M0L3lenS456 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1852 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS457
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1851, _M0L6_2atmpS1852);
  moonbit_incref(_M0L5bytesS457);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS457, 0, _M0L3strS455, _M0L5startS453, _M0L3lenS456);
  _M0L6_2atmpS1850 = _M0L5bytesS457;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1850, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS450) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS450;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS451
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS451;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS452) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS452;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS442,
  int32_t _M0L5radixS441
) {
  int32_t _if__result_3266;
  uint16_t* _M0L6bufferS443;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS441 < 2) {
    _if__result_3266 = 1;
  } else {
    _if__result_3266 = _M0L5radixS441 > 36;
  }
  if (_if__result_3266) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_77.data, (moonbit_string_t)moonbit_string_literal_78.data);
  }
  if (_M0L4selfS442 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_65.data;
  }
  switch (_M0L5radixS441) {
    case 10: {
      int32_t _M0L3lenS444;
      uint16_t* _M0L6bufferS445;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS444 = _M0FPB12dec__count64(_M0L4selfS442);
      _M0L6bufferS445 = (uint16_t*)moonbit_make_string(_M0L3lenS444, 0);
      moonbit_incref(_M0L6bufferS445);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS445, _M0L4selfS442, 0, _M0L3lenS444);
      _M0L6bufferS443 = _M0L6bufferS445;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS446;
      uint16_t* _M0L6bufferS447;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS446 = _M0FPB12hex__count64(_M0L4selfS442);
      _M0L6bufferS447 = (uint16_t*)moonbit_make_string(_M0L3lenS446, 0);
      moonbit_incref(_M0L6bufferS447);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS447, _M0L4selfS442, 0, _M0L3lenS446);
      _M0L6bufferS443 = _M0L6bufferS447;
      break;
    }
    default: {
      int32_t _M0L3lenS448;
      uint16_t* _M0L6bufferS449;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS448 = _M0FPB14radix__count64(_M0L4selfS442, _M0L5radixS441);
      _M0L6bufferS449 = (uint16_t*)moonbit_make_string(_M0L3lenS448, 0);
      moonbit_incref(_M0L6bufferS449);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS449, _M0L4selfS442, 0, _M0L3lenS448, _M0L5radixS441);
      _M0L6bufferS443 = _M0L6bufferS449;
      break;
    }
  }
  return _M0L6bufferS443;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS431,
  uint64_t _M0L3numS419,
  int32_t _M0L12digit__startS422,
  int32_t _M0L10total__lenS421
) {
  uint64_t _M0Lm3numS418;
  int32_t _M0Lm6offsetS420;
  uint64_t _M0L6_2atmpS1848;
  int32_t _M0Lm9remainingS433;
  int32_t _M0L6_2atmpS1829;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS418 = _M0L3numS419;
  _M0Lm6offsetS420 = _M0L10total__lenS421 - _M0L12digit__startS422;
  while (1) {
    uint64_t _M0L6_2atmpS1792 = _M0Lm3numS418;
    if (_M0L6_2atmpS1792 >= 10000ull) {
      uint64_t _M0L6_2atmpS1815 = _M0Lm3numS418;
      uint64_t _M0L1tS423 = _M0L6_2atmpS1815 / 10000ull;
      uint64_t _M0L6_2atmpS1814 = _M0Lm3numS418;
      uint64_t _M0L6_2atmpS1813 = _M0L6_2atmpS1814 % 10000ull;
      int32_t _M0L1rS424 = (int32_t)_M0L6_2atmpS1813;
      int32_t _M0L2d1S425;
      int32_t _M0L2d2S426;
      int32_t _M0L6_2atmpS1793;
      int32_t _M0L6_2atmpS1812;
      int32_t _M0L6_2atmpS1811;
      int32_t _M0L6d1__hiS427;
      int32_t _M0L6_2atmpS1810;
      int32_t _M0L6_2atmpS1809;
      int32_t _M0L6d1__loS428;
      int32_t _M0L6_2atmpS1808;
      int32_t _M0L6_2atmpS1807;
      int32_t _M0L6d2__hiS429;
      int32_t _M0L6_2atmpS1806;
      int32_t _M0L6_2atmpS1805;
      int32_t _M0L6d2__loS430;
      int32_t _M0L6_2atmpS1795;
      int32_t _M0L6_2atmpS1794;
      int32_t _M0L6_2atmpS1798;
      int32_t _M0L6_2atmpS1797;
      int32_t _M0L6_2atmpS1796;
      int32_t _M0L6_2atmpS1801;
      int32_t _M0L6_2atmpS1800;
      int32_t _M0L6_2atmpS1799;
      int32_t _M0L6_2atmpS1804;
      int32_t _M0L6_2atmpS1803;
      int32_t _M0L6_2atmpS1802;
      _M0Lm3numS418 = _M0L1tS423;
      _M0L2d1S425 = _M0L1rS424 / 100;
      _M0L2d2S426 = _M0L1rS424 % 100;
      _M0L6_2atmpS1793 = _M0Lm6offsetS420;
      _M0Lm6offsetS420 = _M0L6_2atmpS1793 - 4;
      _M0L6_2atmpS1812 = _M0L2d1S425 / 10;
      _M0L6_2atmpS1811 = 48 + _M0L6_2atmpS1812;
      _M0L6d1__hiS427 = (uint16_t)_M0L6_2atmpS1811;
      _M0L6_2atmpS1810 = _M0L2d1S425 % 10;
      _M0L6_2atmpS1809 = 48 + _M0L6_2atmpS1810;
      _M0L6d1__loS428 = (uint16_t)_M0L6_2atmpS1809;
      _M0L6_2atmpS1808 = _M0L2d2S426 / 10;
      _M0L6_2atmpS1807 = 48 + _M0L6_2atmpS1808;
      _M0L6d2__hiS429 = (uint16_t)_M0L6_2atmpS1807;
      _M0L6_2atmpS1806 = _M0L2d2S426 % 10;
      _M0L6_2atmpS1805 = 48 + _M0L6_2atmpS1806;
      _M0L6d2__loS430 = (uint16_t)_M0L6_2atmpS1805;
      _M0L6_2atmpS1795 = _M0Lm6offsetS420;
      _M0L6_2atmpS1794 = _M0L12digit__startS422 + _M0L6_2atmpS1795;
      _M0L6bufferS431[_M0L6_2atmpS1794] = _M0L6d1__hiS427;
      _M0L6_2atmpS1798 = _M0Lm6offsetS420;
      _M0L6_2atmpS1797 = _M0L12digit__startS422 + _M0L6_2atmpS1798;
      _M0L6_2atmpS1796 = _M0L6_2atmpS1797 + 1;
      _M0L6bufferS431[_M0L6_2atmpS1796] = _M0L6d1__loS428;
      _M0L6_2atmpS1801 = _M0Lm6offsetS420;
      _M0L6_2atmpS1800 = _M0L12digit__startS422 + _M0L6_2atmpS1801;
      _M0L6_2atmpS1799 = _M0L6_2atmpS1800 + 2;
      _M0L6bufferS431[_M0L6_2atmpS1799] = _M0L6d2__hiS429;
      _M0L6_2atmpS1804 = _M0Lm6offsetS420;
      _M0L6_2atmpS1803 = _M0L12digit__startS422 + _M0L6_2atmpS1804;
      _M0L6_2atmpS1802 = _M0L6_2atmpS1803 + 3;
      _M0L6bufferS431[_M0L6_2atmpS1802] = _M0L6d2__loS430;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1848 = _M0Lm3numS418;
  _M0Lm9remainingS433 = (int32_t)_M0L6_2atmpS1848;
  while (1) {
    int32_t _M0L6_2atmpS1816 = _M0Lm9remainingS433;
    if (_M0L6_2atmpS1816 >= 100) {
      int32_t _M0L6_2atmpS1828 = _M0Lm9remainingS433;
      int32_t _M0L1tS434 = _M0L6_2atmpS1828 / 100;
      int32_t _M0L6_2atmpS1827 = _M0Lm9remainingS433;
      int32_t _M0L1dS435 = _M0L6_2atmpS1827 % 100;
      int32_t _M0L6_2atmpS1817;
      int32_t _M0L6_2atmpS1826;
      int32_t _M0L6_2atmpS1825;
      int32_t _M0L5d__hiS436;
      int32_t _M0L6_2atmpS1824;
      int32_t _M0L6_2atmpS1823;
      int32_t _M0L5d__loS437;
      int32_t _M0L6_2atmpS1819;
      int32_t _M0L6_2atmpS1818;
      int32_t _M0L6_2atmpS1822;
      int32_t _M0L6_2atmpS1821;
      int32_t _M0L6_2atmpS1820;
      _M0Lm9remainingS433 = _M0L1tS434;
      _M0L6_2atmpS1817 = _M0Lm6offsetS420;
      _M0Lm6offsetS420 = _M0L6_2atmpS1817 - 2;
      _M0L6_2atmpS1826 = _M0L1dS435 / 10;
      _M0L6_2atmpS1825 = 48 + _M0L6_2atmpS1826;
      _M0L5d__hiS436 = (uint16_t)_M0L6_2atmpS1825;
      _M0L6_2atmpS1824 = _M0L1dS435 % 10;
      _M0L6_2atmpS1823 = 48 + _M0L6_2atmpS1824;
      _M0L5d__loS437 = (uint16_t)_M0L6_2atmpS1823;
      _M0L6_2atmpS1819 = _M0Lm6offsetS420;
      _M0L6_2atmpS1818 = _M0L12digit__startS422 + _M0L6_2atmpS1819;
      _M0L6bufferS431[_M0L6_2atmpS1818] = _M0L5d__hiS436;
      _M0L6_2atmpS1822 = _M0Lm6offsetS420;
      _M0L6_2atmpS1821 = _M0L12digit__startS422 + _M0L6_2atmpS1822;
      _M0L6_2atmpS1820 = _M0L6_2atmpS1821 + 1;
      _M0L6bufferS431[_M0L6_2atmpS1820] = _M0L5d__loS437;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1829 = _M0Lm9remainingS433;
  if (_M0L6_2atmpS1829 >= 10) {
    int32_t _M0L6_2atmpS1830 = _M0Lm6offsetS420;
    int32_t _M0L6_2atmpS1841;
    int32_t _M0L6_2atmpS1840;
    int32_t _M0L6_2atmpS1839;
    int32_t _M0L5d__hiS439;
    int32_t _M0L6_2atmpS1838;
    int32_t _M0L6_2atmpS1837;
    int32_t _M0L6_2atmpS1836;
    int32_t _M0L5d__loS440;
    int32_t _M0L6_2atmpS1832;
    int32_t _M0L6_2atmpS1831;
    int32_t _M0L6_2atmpS1835;
    int32_t _M0L6_2atmpS1834;
    int32_t _M0L6_2atmpS1833;
    _M0Lm6offsetS420 = _M0L6_2atmpS1830 - 2;
    _M0L6_2atmpS1841 = _M0Lm9remainingS433;
    _M0L6_2atmpS1840 = _M0L6_2atmpS1841 / 10;
    _M0L6_2atmpS1839 = 48 + _M0L6_2atmpS1840;
    _M0L5d__hiS439 = (uint16_t)_M0L6_2atmpS1839;
    _M0L6_2atmpS1838 = _M0Lm9remainingS433;
    _M0L6_2atmpS1837 = _M0L6_2atmpS1838 % 10;
    _M0L6_2atmpS1836 = 48 + _M0L6_2atmpS1837;
    _M0L5d__loS440 = (uint16_t)_M0L6_2atmpS1836;
    _M0L6_2atmpS1832 = _M0Lm6offsetS420;
    _M0L6_2atmpS1831 = _M0L12digit__startS422 + _M0L6_2atmpS1832;
    _M0L6bufferS431[_M0L6_2atmpS1831] = _M0L5d__hiS439;
    _M0L6_2atmpS1835 = _M0Lm6offsetS420;
    _M0L6_2atmpS1834 = _M0L12digit__startS422 + _M0L6_2atmpS1835;
    _M0L6_2atmpS1833 = _M0L6_2atmpS1834 + 1;
    _M0L6bufferS431[_M0L6_2atmpS1833] = _M0L5d__loS440;
    moonbit_decref(_M0L6bufferS431);
  } else {
    int32_t _M0L6_2atmpS1842 = _M0Lm6offsetS420;
    int32_t _M0L6_2atmpS1847;
    int32_t _M0L6_2atmpS1843;
    int32_t _M0L6_2atmpS1846;
    int32_t _M0L6_2atmpS1845;
    int32_t _M0L6_2atmpS1844;
    _M0Lm6offsetS420 = _M0L6_2atmpS1842 - 1;
    _M0L6_2atmpS1847 = _M0Lm6offsetS420;
    _M0L6_2atmpS1843 = _M0L12digit__startS422 + _M0L6_2atmpS1847;
    _M0L6_2atmpS1846 = _M0Lm9remainingS433;
    _M0L6_2atmpS1845 = 48 + _M0L6_2atmpS1846;
    _M0L6_2atmpS1844 = (uint16_t)_M0L6_2atmpS1845;
    _M0L6bufferS431[_M0L6_2atmpS1843] = _M0L6_2atmpS1844;
    moonbit_decref(_M0L6bufferS431);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS413,
  uint64_t _M0L3numS407,
  int32_t _M0L12digit__startS405,
  int32_t _M0L10total__lenS404,
  int32_t _M0L5radixS409
) {
  int32_t _M0Lm6offsetS403;
  uint64_t _M0Lm1nS406;
  uint64_t _M0L4baseS408;
  int32_t _M0L6_2atmpS1774;
  int32_t _M0L6_2atmpS1773;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS403 = _M0L10total__lenS404 - _M0L12digit__startS405;
  _M0Lm1nS406 = _M0L3numS407;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS408 = _M0MPC13int3Int10to__uint64(_M0L5radixS409);
  _M0L6_2atmpS1774 = _M0L5radixS409 - 1;
  _M0L6_2atmpS1773 = _M0L5radixS409 & _M0L6_2atmpS1774;
  if (_M0L6_2atmpS1773 == 0) {
    int32_t _M0L5shiftS410;
    uint64_t _M0L4maskS411;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS410 = moonbit_ctz32(_M0L5radixS409);
    _M0L4maskS411 = _M0L4baseS408 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1775 = _M0Lm1nS406;
      if (_M0L6_2atmpS1775 > 0ull) {
        int32_t _M0L6_2atmpS1776 = _M0Lm6offsetS403;
        uint64_t _M0L6_2atmpS1782;
        uint64_t _M0L6_2atmpS1781;
        int32_t _M0L5digitS412;
        int32_t _M0L6_2atmpS1779;
        int32_t _M0L6_2atmpS1777;
        int32_t _M0L6_2atmpS1778;
        uint64_t _M0L6_2atmpS1780;
        _M0Lm6offsetS403 = _M0L6_2atmpS1776 - 1;
        _M0L6_2atmpS1782 = _M0Lm1nS406;
        _M0L6_2atmpS1781 = _M0L6_2atmpS1782 & _M0L4maskS411;
        _M0L5digitS412 = (int32_t)_M0L6_2atmpS1781;
        _M0L6_2atmpS1779 = _M0Lm6offsetS403;
        _M0L6_2atmpS1777 = _M0L12digit__startS405 + _M0L6_2atmpS1779;
        _M0L6_2atmpS1778
        = ((moonbit_string_t)moonbit_string_literal_79.data)[
          _M0L5digitS412
        ];
        _M0L6bufferS413[_M0L6_2atmpS1777] = _M0L6_2atmpS1778;
        _M0L6_2atmpS1780 = _M0Lm1nS406;
        _M0Lm1nS406 = _M0L6_2atmpS1780 >> (_M0L5shiftS410 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS413);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1783 = _M0Lm1nS406;
      if (_M0L6_2atmpS1783 > 0ull) {
        int32_t _M0L6_2atmpS1784 = _M0Lm6offsetS403;
        uint64_t _M0L6_2atmpS1791;
        uint64_t _M0L1qS415;
        uint64_t _M0L6_2atmpS1789;
        uint64_t _M0L6_2atmpS1790;
        uint64_t _M0L6_2atmpS1788;
        int32_t _M0L5digitS416;
        int32_t _M0L6_2atmpS1787;
        int32_t _M0L6_2atmpS1785;
        int32_t _M0L6_2atmpS1786;
        _M0Lm6offsetS403 = _M0L6_2atmpS1784 - 1;
        _M0L6_2atmpS1791 = _M0Lm1nS406;
        _M0L1qS415 = _M0L6_2atmpS1791 / _M0L4baseS408;
        _M0L6_2atmpS1789 = _M0Lm1nS406;
        _M0L6_2atmpS1790 = _M0L1qS415 * _M0L4baseS408;
        _M0L6_2atmpS1788 = _M0L6_2atmpS1789 - _M0L6_2atmpS1790;
        _M0L5digitS416 = (int32_t)_M0L6_2atmpS1788;
        _M0L6_2atmpS1787 = _M0Lm6offsetS403;
        _M0L6_2atmpS1785 = _M0L12digit__startS405 + _M0L6_2atmpS1787;
        _M0L6_2atmpS1786
        = ((moonbit_string_t)moonbit_string_literal_79.data)[
          _M0L5digitS416
        ];
        _M0L6bufferS413[_M0L6_2atmpS1785] = _M0L6_2atmpS1786;
        _M0Lm1nS406 = _M0L1qS415;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS413);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS400,
  uint64_t _M0L3numS396,
  int32_t _M0L12digit__startS394,
  int32_t _M0L10total__lenS393
) {
  int32_t _M0Lm6offsetS392;
  uint64_t _M0Lm1nS395;
  int32_t _M0L6_2atmpS1769;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS392 = _M0L10total__lenS393 - _M0L12digit__startS394;
  _M0Lm1nS395 = _M0L3numS396;
  while (1) {
    int32_t _M0L6_2atmpS1757 = _M0Lm6offsetS392;
    if (_M0L6_2atmpS1757 >= 2) {
      int32_t _M0L6_2atmpS1758 = _M0Lm6offsetS392;
      uint64_t _M0L6_2atmpS1768;
      uint64_t _M0L6_2atmpS1767;
      int32_t _M0L9byte__valS397;
      int32_t _M0L2hiS398;
      int32_t _M0L2loS399;
      int32_t _M0L6_2atmpS1761;
      int32_t _M0L6_2atmpS1759;
      int32_t _M0L6_2atmpS1760;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L6_2atmpS1764;
      int32_t _M0L6_2atmpS1762;
      int32_t _M0L6_2atmpS1763;
      uint64_t _M0L6_2atmpS1766;
      _M0Lm6offsetS392 = _M0L6_2atmpS1758 - 2;
      _M0L6_2atmpS1768 = _M0Lm1nS395;
      _M0L6_2atmpS1767 = _M0L6_2atmpS1768 & 255ull;
      _M0L9byte__valS397 = (int32_t)_M0L6_2atmpS1767;
      _M0L2hiS398 = _M0L9byte__valS397 / 16;
      _M0L2loS399 = _M0L9byte__valS397 % 16;
      _M0L6_2atmpS1761 = _M0Lm6offsetS392;
      _M0L6_2atmpS1759 = _M0L12digit__startS394 + _M0L6_2atmpS1761;
      _M0L6_2atmpS1760
      = ((moonbit_string_t)moonbit_string_literal_79.data)[
        _M0L2hiS398
      ];
      _M0L6bufferS400[_M0L6_2atmpS1759] = _M0L6_2atmpS1760;
      _M0L6_2atmpS1765 = _M0Lm6offsetS392;
      _M0L6_2atmpS1764 = _M0L12digit__startS394 + _M0L6_2atmpS1765;
      _M0L6_2atmpS1762 = _M0L6_2atmpS1764 + 1;
      _M0L6_2atmpS1763
      = ((moonbit_string_t)moonbit_string_literal_79.data)[
        _M0L2loS399
      ];
      _M0L6bufferS400[_M0L6_2atmpS1762] = _M0L6_2atmpS1763;
      _M0L6_2atmpS1766 = _M0Lm1nS395;
      _M0Lm1nS395 = _M0L6_2atmpS1766 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1769 = _M0Lm6offsetS392;
  if (_M0L6_2atmpS1769 == 1) {
    uint64_t _M0L6_2atmpS1772 = _M0Lm1nS395;
    uint64_t _M0L6_2atmpS1771 = _M0L6_2atmpS1772 & 15ull;
    int32_t _M0L6nibbleS402 = (int32_t)_M0L6_2atmpS1771;
    int32_t _M0L6_2atmpS1770 =
      ((moonbit_string_t)moonbit_string_literal_79.data)[_M0L6nibbleS402];
    _M0L6bufferS400[_M0L12digit__startS394] = _M0L6_2atmpS1770;
    moonbit_decref(_M0L6bufferS400);
  } else {
    moonbit_decref(_M0L6bufferS400);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS386,
  int32_t _M0L5radixS389
) {
  uint64_t _M0Lm3numS387;
  uint64_t _M0L4baseS388;
  int32_t _M0Lm5countS390;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS386 == 0ull) {
    return 1;
  }
  _M0Lm3numS387 = _M0L5valueS386;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS388 = _M0MPC13int3Int10to__uint64(_M0L5radixS389);
  _M0Lm5countS390 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1754 = _M0Lm3numS387;
    if (_M0L6_2atmpS1754 > 0ull) {
      int32_t _M0L6_2atmpS1755 = _M0Lm5countS390;
      uint64_t _M0L6_2atmpS1756;
      _M0Lm5countS390 = _M0L6_2atmpS1755 + 1;
      _M0L6_2atmpS1756 = _M0Lm3numS387;
      _M0Lm3numS387 = _M0L6_2atmpS1756 / _M0L4baseS388;
      continue;
    }
    break;
  }
  return _M0Lm5countS390;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS384) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS384 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS385;
    int32_t _M0L6_2atmpS1753;
    int32_t _M0L6_2atmpS1752;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS385 = moonbit_clz64(_M0L5valueS384);
    _M0L6_2atmpS1753 = 63 - _M0L14leading__zerosS385;
    _M0L6_2atmpS1752 = _M0L6_2atmpS1753 / 4;
    return _M0L6_2atmpS1752 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS383) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS383 >= 10000000000ull) {
    if (_M0L5valueS383 >= 100000000000000ull) {
      if (_M0L5valueS383 >= 10000000000000000ull) {
        if (_M0L5valueS383 >= 1000000000000000000ull) {
          if (_M0L5valueS383 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS383 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS383 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS383 >= 1000000000000ull) {
      if (_M0L5valueS383 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS383 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS383 >= 100000ull) {
    if (_M0L5valueS383 >= 10000000ull) {
      if (_M0L5valueS383 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS383 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS383 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS383 >= 1000ull) {
    if (_M0L5valueS383 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS383 >= 100ull) {
    return 3;
  } else if (_M0L5valueS383 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS367,
  int32_t _M0L5radixS366
) {
  int32_t _if__result_3273;
  int32_t _M0L12is__negativeS368;
  uint32_t _M0L3numS369;
  uint16_t* _M0L6bufferS370;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS366 < 2) {
    _if__result_3273 = 1;
  } else {
    _if__result_3273 = _M0L5radixS366 > 36;
  }
  if (_if__result_3273) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_77.data, (moonbit_string_t)moonbit_string_literal_80.data);
  }
  if (_M0L4selfS367 == 0) {
    return (moonbit_string_t)moonbit_string_literal_65.data;
  }
  _M0L12is__negativeS368 = _M0L4selfS367 < 0;
  if (_M0L12is__negativeS368) {
    int32_t _M0L6_2atmpS1751 = -_M0L4selfS367;
    _M0L3numS369 = *(uint32_t*)&_M0L6_2atmpS1751;
  } else {
    _M0L3numS369 = *(uint32_t*)&_M0L4selfS367;
  }
  switch (_M0L5radixS366) {
    case 10: {
      int32_t _M0L10digit__lenS371;
      int32_t _M0L6_2atmpS1748;
      int32_t _M0L10total__lenS372;
      uint16_t* _M0L6bufferS373;
      int32_t _M0L12digit__startS374;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS371 = _M0FPB12dec__count32(_M0L3numS369);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS1748 = 1;
      } else {
        _M0L6_2atmpS1748 = 0;
      }
      _M0L10total__lenS372 = _M0L10digit__lenS371 + _M0L6_2atmpS1748;
      _M0L6bufferS373
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS372, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS374 = 1;
      } else {
        _M0L12digit__startS374 = 0;
      }
      moonbit_incref(_M0L6bufferS373);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS373, _M0L3numS369, _M0L12digit__startS374, _M0L10total__lenS372);
      _M0L6bufferS370 = _M0L6bufferS373;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS375;
      int32_t _M0L6_2atmpS1749;
      int32_t _M0L10total__lenS376;
      uint16_t* _M0L6bufferS377;
      int32_t _M0L12digit__startS378;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS375 = _M0FPB12hex__count32(_M0L3numS369);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS1749 = 1;
      } else {
        _M0L6_2atmpS1749 = 0;
      }
      _M0L10total__lenS376 = _M0L10digit__lenS375 + _M0L6_2atmpS1749;
      _M0L6bufferS377
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS376, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS378 = 1;
      } else {
        _M0L12digit__startS378 = 0;
      }
      moonbit_incref(_M0L6bufferS377);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS377, _M0L3numS369, _M0L12digit__startS378, _M0L10total__lenS376);
      _M0L6bufferS370 = _M0L6bufferS377;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS379;
      int32_t _M0L6_2atmpS1750;
      int32_t _M0L10total__lenS380;
      uint16_t* _M0L6bufferS381;
      int32_t _M0L12digit__startS382;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS379
      = _M0FPB14radix__count32(_M0L3numS369, _M0L5radixS366);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS1750 = 1;
      } else {
        _M0L6_2atmpS1750 = 0;
      }
      _M0L10total__lenS380 = _M0L10digit__lenS379 + _M0L6_2atmpS1750;
      _M0L6bufferS381
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS380, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS382 = 1;
      } else {
        _M0L12digit__startS382 = 0;
      }
      moonbit_incref(_M0L6bufferS381);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS381, _M0L3numS369, _M0L12digit__startS382, _M0L10total__lenS380, _M0L5radixS366);
      _M0L6bufferS370 = _M0L6bufferS381;
      break;
    }
  }
  if (_M0L12is__negativeS368) {
    _M0L6bufferS370[0] = 45;
  }
  return _M0L6bufferS370;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS360,
  int32_t _M0L5radixS363
) {
  uint32_t _M0Lm3numS361;
  uint32_t _M0L4baseS362;
  int32_t _M0Lm5countS364;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS360 == 0u) {
    return 1;
  }
  _M0Lm3numS361 = _M0L5valueS360;
  _M0L4baseS362 = *(uint32_t*)&_M0L5radixS363;
  _M0Lm5countS364 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1745 = _M0Lm3numS361;
    if (_M0L6_2atmpS1745 > 0u) {
      int32_t _M0L6_2atmpS1746 = _M0Lm5countS364;
      uint32_t _M0L6_2atmpS1747;
      _M0Lm5countS364 = _M0L6_2atmpS1746 + 1;
      _M0L6_2atmpS1747 = _M0Lm3numS361;
      _M0Lm3numS361 = _M0L6_2atmpS1747 / _M0L4baseS362;
      continue;
    }
    break;
  }
  return _M0Lm5countS364;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS358) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS358 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS359;
    int32_t _M0L6_2atmpS1744;
    int32_t _M0L6_2atmpS1743;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS359 = moonbit_clz32(_M0L5valueS358);
    _M0L6_2atmpS1744 = 31 - _M0L14leading__zerosS359;
    _M0L6_2atmpS1743 = _M0L6_2atmpS1744 / 4;
    return _M0L6_2atmpS1743 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS357) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS357 >= 100000u) {
    if (_M0L5valueS357 >= 10000000u) {
      if (_M0L5valueS357 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS357 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS357 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS357 >= 1000u) {
    if (_M0L5valueS357 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS357 >= 100u) {
    return 3;
  } else if (_M0L5valueS357 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS347,
  uint32_t _M0L3numS335,
  int32_t _M0L12digit__startS338,
  int32_t _M0L10total__lenS337
) {
  uint32_t _M0Lm3numS334;
  int32_t _M0Lm6offsetS336;
  uint32_t _M0L6_2atmpS1742;
  int32_t _M0Lm9remainingS349;
  int32_t _M0L6_2atmpS1723;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS334 = _M0L3numS335;
  _M0Lm6offsetS336 = _M0L10total__lenS337 - _M0L12digit__startS338;
  while (1) {
    uint32_t _M0L6_2atmpS1686 = _M0Lm3numS334;
    if (_M0L6_2atmpS1686 >= 10000u) {
      uint32_t _M0L6_2atmpS1709 = _M0Lm3numS334;
      uint32_t _M0L1tS339 = _M0L6_2atmpS1709 / 10000u;
      uint32_t _M0L6_2atmpS1708 = _M0Lm3numS334;
      uint32_t _M0L6_2atmpS1707 = _M0L6_2atmpS1708 % 10000u;
      int32_t _M0L1rS340 = *(int32_t*)&_M0L6_2atmpS1707;
      int32_t _M0L2d1S341;
      int32_t _M0L2d2S342;
      int32_t _M0L6_2atmpS1687;
      int32_t _M0L6_2atmpS1706;
      int32_t _M0L6_2atmpS1705;
      int32_t _M0L6d1__hiS343;
      int32_t _M0L6_2atmpS1704;
      int32_t _M0L6_2atmpS1703;
      int32_t _M0L6d1__loS344;
      int32_t _M0L6_2atmpS1702;
      int32_t _M0L6_2atmpS1701;
      int32_t _M0L6d2__hiS345;
      int32_t _M0L6_2atmpS1700;
      int32_t _M0L6_2atmpS1699;
      int32_t _M0L6d2__loS346;
      int32_t _M0L6_2atmpS1689;
      int32_t _M0L6_2atmpS1688;
      int32_t _M0L6_2atmpS1692;
      int32_t _M0L6_2atmpS1691;
      int32_t _M0L6_2atmpS1690;
      int32_t _M0L6_2atmpS1695;
      int32_t _M0L6_2atmpS1694;
      int32_t _M0L6_2atmpS1693;
      int32_t _M0L6_2atmpS1698;
      int32_t _M0L6_2atmpS1697;
      int32_t _M0L6_2atmpS1696;
      _M0Lm3numS334 = _M0L1tS339;
      _M0L2d1S341 = _M0L1rS340 / 100;
      _M0L2d2S342 = _M0L1rS340 % 100;
      _M0L6_2atmpS1687 = _M0Lm6offsetS336;
      _M0Lm6offsetS336 = _M0L6_2atmpS1687 - 4;
      _M0L6_2atmpS1706 = _M0L2d1S341 / 10;
      _M0L6_2atmpS1705 = 48 + _M0L6_2atmpS1706;
      _M0L6d1__hiS343 = (uint16_t)_M0L6_2atmpS1705;
      _M0L6_2atmpS1704 = _M0L2d1S341 % 10;
      _M0L6_2atmpS1703 = 48 + _M0L6_2atmpS1704;
      _M0L6d1__loS344 = (uint16_t)_M0L6_2atmpS1703;
      _M0L6_2atmpS1702 = _M0L2d2S342 / 10;
      _M0L6_2atmpS1701 = 48 + _M0L6_2atmpS1702;
      _M0L6d2__hiS345 = (uint16_t)_M0L6_2atmpS1701;
      _M0L6_2atmpS1700 = _M0L2d2S342 % 10;
      _M0L6_2atmpS1699 = 48 + _M0L6_2atmpS1700;
      _M0L6d2__loS346 = (uint16_t)_M0L6_2atmpS1699;
      _M0L6_2atmpS1689 = _M0Lm6offsetS336;
      _M0L6_2atmpS1688 = _M0L12digit__startS338 + _M0L6_2atmpS1689;
      _M0L6bufferS347[_M0L6_2atmpS1688] = _M0L6d1__hiS343;
      _M0L6_2atmpS1692 = _M0Lm6offsetS336;
      _M0L6_2atmpS1691 = _M0L12digit__startS338 + _M0L6_2atmpS1692;
      _M0L6_2atmpS1690 = _M0L6_2atmpS1691 + 1;
      _M0L6bufferS347[_M0L6_2atmpS1690] = _M0L6d1__loS344;
      _M0L6_2atmpS1695 = _M0Lm6offsetS336;
      _M0L6_2atmpS1694 = _M0L12digit__startS338 + _M0L6_2atmpS1695;
      _M0L6_2atmpS1693 = _M0L6_2atmpS1694 + 2;
      _M0L6bufferS347[_M0L6_2atmpS1693] = _M0L6d2__hiS345;
      _M0L6_2atmpS1698 = _M0Lm6offsetS336;
      _M0L6_2atmpS1697 = _M0L12digit__startS338 + _M0L6_2atmpS1698;
      _M0L6_2atmpS1696 = _M0L6_2atmpS1697 + 3;
      _M0L6bufferS347[_M0L6_2atmpS1696] = _M0L6d2__loS346;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1742 = _M0Lm3numS334;
  _M0Lm9remainingS349 = *(int32_t*)&_M0L6_2atmpS1742;
  while (1) {
    int32_t _M0L6_2atmpS1710 = _M0Lm9remainingS349;
    if (_M0L6_2atmpS1710 >= 100) {
      int32_t _M0L6_2atmpS1722 = _M0Lm9remainingS349;
      int32_t _M0L1tS350 = _M0L6_2atmpS1722 / 100;
      int32_t _M0L6_2atmpS1721 = _M0Lm9remainingS349;
      int32_t _M0L1dS351 = _M0L6_2atmpS1721 % 100;
      int32_t _M0L6_2atmpS1711;
      int32_t _M0L6_2atmpS1720;
      int32_t _M0L6_2atmpS1719;
      int32_t _M0L5d__hiS352;
      int32_t _M0L6_2atmpS1718;
      int32_t _M0L6_2atmpS1717;
      int32_t _M0L5d__loS353;
      int32_t _M0L6_2atmpS1713;
      int32_t _M0L6_2atmpS1712;
      int32_t _M0L6_2atmpS1716;
      int32_t _M0L6_2atmpS1715;
      int32_t _M0L6_2atmpS1714;
      _M0Lm9remainingS349 = _M0L1tS350;
      _M0L6_2atmpS1711 = _M0Lm6offsetS336;
      _M0Lm6offsetS336 = _M0L6_2atmpS1711 - 2;
      _M0L6_2atmpS1720 = _M0L1dS351 / 10;
      _M0L6_2atmpS1719 = 48 + _M0L6_2atmpS1720;
      _M0L5d__hiS352 = (uint16_t)_M0L6_2atmpS1719;
      _M0L6_2atmpS1718 = _M0L1dS351 % 10;
      _M0L6_2atmpS1717 = 48 + _M0L6_2atmpS1718;
      _M0L5d__loS353 = (uint16_t)_M0L6_2atmpS1717;
      _M0L6_2atmpS1713 = _M0Lm6offsetS336;
      _M0L6_2atmpS1712 = _M0L12digit__startS338 + _M0L6_2atmpS1713;
      _M0L6bufferS347[_M0L6_2atmpS1712] = _M0L5d__hiS352;
      _M0L6_2atmpS1716 = _M0Lm6offsetS336;
      _M0L6_2atmpS1715 = _M0L12digit__startS338 + _M0L6_2atmpS1716;
      _M0L6_2atmpS1714 = _M0L6_2atmpS1715 + 1;
      _M0L6bufferS347[_M0L6_2atmpS1714] = _M0L5d__loS353;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1723 = _M0Lm9remainingS349;
  if (_M0L6_2atmpS1723 >= 10) {
    int32_t _M0L6_2atmpS1724 = _M0Lm6offsetS336;
    int32_t _M0L6_2atmpS1735;
    int32_t _M0L6_2atmpS1734;
    int32_t _M0L6_2atmpS1733;
    int32_t _M0L5d__hiS355;
    int32_t _M0L6_2atmpS1732;
    int32_t _M0L6_2atmpS1731;
    int32_t _M0L6_2atmpS1730;
    int32_t _M0L5d__loS356;
    int32_t _M0L6_2atmpS1726;
    int32_t _M0L6_2atmpS1725;
    int32_t _M0L6_2atmpS1729;
    int32_t _M0L6_2atmpS1728;
    int32_t _M0L6_2atmpS1727;
    _M0Lm6offsetS336 = _M0L6_2atmpS1724 - 2;
    _M0L6_2atmpS1735 = _M0Lm9remainingS349;
    _M0L6_2atmpS1734 = _M0L6_2atmpS1735 / 10;
    _M0L6_2atmpS1733 = 48 + _M0L6_2atmpS1734;
    _M0L5d__hiS355 = (uint16_t)_M0L6_2atmpS1733;
    _M0L6_2atmpS1732 = _M0Lm9remainingS349;
    _M0L6_2atmpS1731 = _M0L6_2atmpS1732 % 10;
    _M0L6_2atmpS1730 = 48 + _M0L6_2atmpS1731;
    _M0L5d__loS356 = (uint16_t)_M0L6_2atmpS1730;
    _M0L6_2atmpS1726 = _M0Lm6offsetS336;
    _M0L6_2atmpS1725 = _M0L12digit__startS338 + _M0L6_2atmpS1726;
    _M0L6bufferS347[_M0L6_2atmpS1725] = _M0L5d__hiS355;
    _M0L6_2atmpS1729 = _M0Lm6offsetS336;
    _M0L6_2atmpS1728 = _M0L12digit__startS338 + _M0L6_2atmpS1729;
    _M0L6_2atmpS1727 = _M0L6_2atmpS1728 + 1;
    _M0L6bufferS347[_M0L6_2atmpS1727] = _M0L5d__loS356;
    moonbit_decref(_M0L6bufferS347);
  } else {
    int32_t _M0L6_2atmpS1736 = _M0Lm6offsetS336;
    int32_t _M0L6_2atmpS1741;
    int32_t _M0L6_2atmpS1737;
    int32_t _M0L6_2atmpS1740;
    int32_t _M0L6_2atmpS1739;
    int32_t _M0L6_2atmpS1738;
    _M0Lm6offsetS336 = _M0L6_2atmpS1736 - 1;
    _M0L6_2atmpS1741 = _M0Lm6offsetS336;
    _M0L6_2atmpS1737 = _M0L12digit__startS338 + _M0L6_2atmpS1741;
    _M0L6_2atmpS1740 = _M0Lm9remainingS349;
    _M0L6_2atmpS1739 = 48 + _M0L6_2atmpS1740;
    _M0L6_2atmpS1738 = (uint16_t)_M0L6_2atmpS1739;
    _M0L6bufferS347[_M0L6_2atmpS1737] = _M0L6_2atmpS1738;
    moonbit_decref(_M0L6bufferS347);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS329,
  uint32_t _M0L3numS323,
  int32_t _M0L12digit__startS321,
  int32_t _M0L10total__lenS320,
  int32_t _M0L5radixS325
) {
  int32_t _M0Lm6offsetS319;
  uint32_t _M0Lm1nS322;
  uint32_t _M0L4baseS324;
  int32_t _M0L6_2atmpS1668;
  int32_t _M0L6_2atmpS1667;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS319 = _M0L10total__lenS320 - _M0L12digit__startS321;
  _M0Lm1nS322 = _M0L3numS323;
  _M0L4baseS324 = *(uint32_t*)&_M0L5radixS325;
  _M0L6_2atmpS1668 = _M0L5radixS325 - 1;
  _M0L6_2atmpS1667 = _M0L5radixS325 & _M0L6_2atmpS1668;
  if (_M0L6_2atmpS1667 == 0) {
    int32_t _M0L5shiftS326;
    uint32_t _M0L4maskS327;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS326 = moonbit_ctz32(_M0L5radixS325);
    _M0L4maskS327 = _M0L4baseS324 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1669 = _M0Lm1nS322;
      if (_M0L6_2atmpS1669 > 0u) {
        int32_t _M0L6_2atmpS1670 = _M0Lm6offsetS319;
        uint32_t _M0L6_2atmpS1676;
        uint32_t _M0L6_2atmpS1675;
        int32_t _M0L5digitS328;
        int32_t _M0L6_2atmpS1673;
        int32_t _M0L6_2atmpS1671;
        int32_t _M0L6_2atmpS1672;
        uint32_t _M0L6_2atmpS1674;
        _M0Lm6offsetS319 = _M0L6_2atmpS1670 - 1;
        _M0L6_2atmpS1676 = _M0Lm1nS322;
        _M0L6_2atmpS1675 = _M0L6_2atmpS1676 & _M0L4maskS327;
        _M0L5digitS328 = *(int32_t*)&_M0L6_2atmpS1675;
        _M0L6_2atmpS1673 = _M0Lm6offsetS319;
        _M0L6_2atmpS1671 = _M0L12digit__startS321 + _M0L6_2atmpS1673;
        _M0L6_2atmpS1672
        = ((moonbit_string_t)moonbit_string_literal_79.data)[
          _M0L5digitS328
        ];
        _M0L6bufferS329[_M0L6_2atmpS1671] = _M0L6_2atmpS1672;
        _M0L6_2atmpS1674 = _M0Lm1nS322;
        _M0Lm1nS322 = _M0L6_2atmpS1674 >> (_M0L5shiftS326 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS329);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1677 = _M0Lm1nS322;
      if (_M0L6_2atmpS1677 > 0u) {
        int32_t _M0L6_2atmpS1678 = _M0Lm6offsetS319;
        uint32_t _M0L6_2atmpS1685;
        uint32_t _M0L1qS331;
        uint32_t _M0L6_2atmpS1683;
        uint32_t _M0L6_2atmpS1684;
        uint32_t _M0L6_2atmpS1682;
        int32_t _M0L5digitS332;
        int32_t _M0L6_2atmpS1681;
        int32_t _M0L6_2atmpS1679;
        int32_t _M0L6_2atmpS1680;
        _M0Lm6offsetS319 = _M0L6_2atmpS1678 - 1;
        _M0L6_2atmpS1685 = _M0Lm1nS322;
        _M0L1qS331 = _M0L6_2atmpS1685 / _M0L4baseS324;
        _M0L6_2atmpS1683 = _M0Lm1nS322;
        _M0L6_2atmpS1684 = _M0L1qS331 * _M0L4baseS324;
        _M0L6_2atmpS1682 = _M0L6_2atmpS1683 - _M0L6_2atmpS1684;
        _M0L5digitS332 = *(int32_t*)&_M0L6_2atmpS1682;
        _M0L6_2atmpS1681 = _M0Lm6offsetS319;
        _M0L6_2atmpS1679 = _M0L12digit__startS321 + _M0L6_2atmpS1681;
        _M0L6_2atmpS1680
        = ((moonbit_string_t)moonbit_string_literal_79.data)[
          _M0L5digitS332
        ];
        _M0L6bufferS329[_M0L6_2atmpS1679] = _M0L6_2atmpS1680;
        _M0Lm1nS322 = _M0L1qS331;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS329);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS316,
  uint32_t _M0L3numS312,
  int32_t _M0L12digit__startS310,
  int32_t _M0L10total__lenS309
) {
  int32_t _M0Lm6offsetS308;
  uint32_t _M0Lm1nS311;
  int32_t _M0L6_2atmpS1663;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS308 = _M0L10total__lenS309 - _M0L12digit__startS310;
  _M0Lm1nS311 = _M0L3numS312;
  while (1) {
    int32_t _M0L6_2atmpS1651 = _M0Lm6offsetS308;
    if (_M0L6_2atmpS1651 >= 2) {
      int32_t _M0L6_2atmpS1652 = _M0Lm6offsetS308;
      uint32_t _M0L6_2atmpS1662;
      uint32_t _M0L6_2atmpS1661;
      int32_t _M0L9byte__valS313;
      int32_t _M0L2hiS314;
      int32_t _M0L2loS315;
      int32_t _M0L6_2atmpS1655;
      int32_t _M0L6_2atmpS1653;
      int32_t _M0L6_2atmpS1654;
      int32_t _M0L6_2atmpS1659;
      int32_t _M0L6_2atmpS1658;
      int32_t _M0L6_2atmpS1656;
      int32_t _M0L6_2atmpS1657;
      uint32_t _M0L6_2atmpS1660;
      _M0Lm6offsetS308 = _M0L6_2atmpS1652 - 2;
      _M0L6_2atmpS1662 = _M0Lm1nS311;
      _M0L6_2atmpS1661 = _M0L6_2atmpS1662 & 255u;
      _M0L9byte__valS313 = *(int32_t*)&_M0L6_2atmpS1661;
      _M0L2hiS314 = _M0L9byte__valS313 / 16;
      _M0L2loS315 = _M0L9byte__valS313 % 16;
      _M0L6_2atmpS1655 = _M0Lm6offsetS308;
      _M0L6_2atmpS1653 = _M0L12digit__startS310 + _M0L6_2atmpS1655;
      _M0L6_2atmpS1654
      = ((moonbit_string_t)moonbit_string_literal_79.data)[
        _M0L2hiS314
      ];
      _M0L6bufferS316[_M0L6_2atmpS1653] = _M0L6_2atmpS1654;
      _M0L6_2atmpS1659 = _M0Lm6offsetS308;
      _M0L6_2atmpS1658 = _M0L12digit__startS310 + _M0L6_2atmpS1659;
      _M0L6_2atmpS1656 = _M0L6_2atmpS1658 + 1;
      _M0L6_2atmpS1657
      = ((moonbit_string_t)moonbit_string_literal_79.data)[
        _M0L2loS315
      ];
      _M0L6bufferS316[_M0L6_2atmpS1656] = _M0L6_2atmpS1657;
      _M0L6_2atmpS1660 = _M0Lm1nS311;
      _M0Lm1nS311 = _M0L6_2atmpS1660 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1663 = _M0Lm6offsetS308;
  if (_M0L6_2atmpS1663 == 1) {
    uint32_t _M0L6_2atmpS1666 = _M0Lm1nS311;
    uint32_t _M0L6_2atmpS1665 = _M0L6_2atmpS1666 & 15u;
    int32_t _M0L6nibbleS318 = *(int32_t*)&_M0L6_2atmpS1665;
    int32_t _M0L6_2atmpS1664 =
      ((moonbit_string_t)moonbit_string_literal_79.data)[_M0L6nibbleS318];
    _M0L6bufferS316[_M0L12digit__startS310] = _M0L6_2atmpS1664;
    moonbit_decref(_M0L6bufferS316);
  } else {
    moonbit_decref(_M0L6bufferS316);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS303) {
  struct _M0TWEOs* _M0L7_2afuncS302;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS302 = _M0L4selfS303;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS302->code(_M0L7_2afuncS302);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS305
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS304;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS304 = _M0L4selfS305;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS304->code(_M0L7_2afuncS304);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS307) {
  struct _M0TWEOc* _M0L7_2afuncS306;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS306 = _M0L4selfS307;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS306->code(_M0L7_2afuncS306);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS293
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS292;
  struct _M0TPB6Logger _M0L6_2atmpS1646;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS292 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS292);
  _M0L6_2atmpS1646
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS292
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS293, _M0L6_2atmpS1646);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS292);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS295
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS294;
  struct _M0TPB6Logger _M0L6_2atmpS1647;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS294 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS294);
  _M0L6_2atmpS1647
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS294
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS295, _M0L6_2atmpS1647);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS294);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(
  int32_t _M0L4selfS297
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS296;
  struct _M0TPB6Logger _M0L6_2atmpS1648;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS296 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS296);
  _M0L6_2atmpS1648
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS296
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14bool4BoolPB4Show6output(_M0L4selfS297, _M0L6_2atmpS1648);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS296);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS299
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS298;
  struct _M0TPB6Logger _M0L6_2atmpS1649;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS298 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS298);
  _M0L6_2atmpS1649
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS298
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS299, _M0L6_2atmpS1649);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS298);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS301
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS300;
  struct _M0TPB6Logger _M0L6_2atmpS1650;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS300 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS300);
  _M0L6_2atmpS1650
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS300
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS301, _M0L6_2atmpS1650);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS300);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS291
) {
  int32_t _M0L8_2afieldS2997;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2997 = _M0L4selfS291.$1;
  moonbit_decref(_M0L4selfS291.$0);
  return _M0L8_2afieldS2997;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS290
) {
  int32_t _M0L3endS1644;
  int32_t _M0L8_2afieldS2998;
  int32_t _M0L5startS1645;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1644 = _M0L4selfS290.$2;
  _M0L8_2afieldS2998 = _M0L4selfS290.$1;
  moonbit_decref(_M0L4selfS290.$0);
  _M0L5startS1645 = _M0L8_2afieldS2998;
  return _M0L3endS1644 - _M0L5startS1645;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS289
) {
  moonbit_string_t _M0L8_2afieldS2999;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2999 = _M0L4selfS289.$0;
  return _M0L8_2afieldS2999;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS285,
  moonbit_string_t _M0L5valueS286,
  int32_t _M0L5startS287,
  int32_t _M0L3lenS288
) {
  int32_t _M0L6_2atmpS1643;
  int64_t _M0L6_2atmpS1642;
  struct _M0TPC16string10StringView _M0L6_2atmpS1641;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1643 = _M0L5startS287 + _M0L3lenS288;
  _M0L6_2atmpS1642 = (int64_t)_M0L6_2atmpS1643;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1641
  = _M0MPC16string6String11sub_2einner(_M0L5valueS286, _M0L5startS287, _M0L6_2atmpS1642);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS285, _M0L6_2atmpS1641);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS278,
  int32_t _M0L5startS284,
  int64_t _M0L3endS280
) {
  int32_t _M0L3lenS277;
  int32_t _M0L3endS279;
  int32_t _M0L5startS283;
  int32_t _if__result_3280;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS277 = Moonbit_array_length(_M0L4selfS278);
  if (_M0L3endS280 == 4294967296ll) {
    _M0L3endS279 = _M0L3lenS277;
  } else {
    int64_t _M0L7_2aSomeS281 = _M0L3endS280;
    int32_t _M0L6_2aendS282 = (int32_t)_M0L7_2aSomeS281;
    if (_M0L6_2aendS282 < 0) {
      _M0L3endS279 = _M0L3lenS277 + _M0L6_2aendS282;
    } else {
      _M0L3endS279 = _M0L6_2aendS282;
    }
  }
  if (_M0L5startS284 < 0) {
    _M0L5startS283 = _M0L3lenS277 + _M0L5startS284;
  } else {
    _M0L5startS283 = _M0L5startS284;
  }
  if (_M0L5startS283 >= 0) {
    if (_M0L5startS283 <= _M0L3endS279) {
      _if__result_3280 = _M0L3endS279 <= _M0L3lenS277;
    } else {
      _if__result_3280 = 0;
    }
  } else {
    _if__result_3280 = 0;
  }
  if (_if__result_3280) {
    if (_M0L5startS283 < _M0L3lenS277) {
      int32_t _M0L6_2atmpS1638 = _M0L4selfS278[_M0L5startS283];
      int32_t _M0L6_2atmpS1637;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1637
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1638);
      if (!_M0L6_2atmpS1637) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS279 < _M0L3lenS277) {
      int32_t _M0L6_2atmpS1640 = _M0L4selfS278[_M0L3endS279];
      int32_t _M0L6_2atmpS1639;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1639
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1640);
      if (!_M0L6_2atmpS1639) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS283,
                                                 _M0L3endS279,
                                                 _M0L4selfS278};
  } else {
    moonbit_decref(_M0L4selfS278);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS274) {
  struct _M0TPB6Hasher* _M0L1hS273;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS273 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS273);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS273, _M0L4selfS274);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS273);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS276
) {
  struct _M0TPB6Hasher* _M0L1hS275;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS275 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS275);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS275, _M0L4selfS276);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS275);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS271) {
  int32_t _M0L4seedS270;
  if (_M0L10seed_2eoptS271 == 4294967296ll) {
    _M0L4seedS270 = 0;
  } else {
    int64_t _M0L7_2aSomeS272 = _M0L10seed_2eoptS271;
    _M0L4seedS270 = (int32_t)_M0L7_2aSomeS272;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS270);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS269) {
  uint32_t _M0L6_2atmpS1636;
  uint32_t _M0L6_2atmpS1635;
  struct _M0TPB6Hasher* _block_3281;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1636 = *(uint32_t*)&_M0L4seedS269;
  _M0L6_2atmpS1635 = _M0L6_2atmpS1636 + 374761393u;
  _block_3281
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3281)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3281->$0 = _M0L6_2atmpS1635;
  return _block_3281;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS268) {
  uint32_t _M0L6_2atmpS1634;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1634 = _M0MPB6Hasher9avalanche(_M0L4selfS268);
  return *(int32_t*)&_M0L6_2atmpS1634;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS267) {
  uint32_t _M0L8_2afieldS3000;
  uint32_t _M0Lm3accS266;
  uint32_t _M0L6_2atmpS1623;
  uint32_t _M0L6_2atmpS1625;
  uint32_t _M0L6_2atmpS1624;
  uint32_t _M0L6_2atmpS1626;
  uint32_t _M0L6_2atmpS1627;
  uint32_t _M0L6_2atmpS1629;
  uint32_t _M0L6_2atmpS1628;
  uint32_t _M0L6_2atmpS1630;
  uint32_t _M0L6_2atmpS1631;
  uint32_t _M0L6_2atmpS1633;
  uint32_t _M0L6_2atmpS1632;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3000 = _M0L4selfS267->$0;
  moonbit_decref(_M0L4selfS267);
  _M0Lm3accS266 = _M0L8_2afieldS3000;
  _M0L6_2atmpS1623 = _M0Lm3accS266;
  _M0L6_2atmpS1625 = _M0Lm3accS266;
  _M0L6_2atmpS1624 = _M0L6_2atmpS1625 >> 15;
  _M0Lm3accS266 = _M0L6_2atmpS1623 ^ _M0L6_2atmpS1624;
  _M0L6_2atmpS1626 = _M0Lm3accS266;
  _M0Lm3accS266 = _M0L6_2atmpS1626 * 2246822519u;
  _M0L6_2atmpS1627 = _M0Lm3accS266;
  _M0L6_2atmpS1629 = _M0Lm3accS266;
  _M0L6_2atmpS1628 = _M0L6_2atmpS1629 >> 13;
  _M0Lm3accS266 = _M0L6_2atmpS1627 ^ _M0L6_2atmpS1628;
  _M0L6_2atmpS1630 = _M0Lm3accS266;
  _M0Lm3accS266 = _M0L6_2atmpS1630 * 3266489917u;
  _M0L6_2atmpS1631 = _M0Lm3accS266;
  _M0L6_2atmpS1633 = _M0Lm3accS266;
  _M0L6_2atmpS1632 = _M0L6_2atmpS1633 >> 16;
  _M0Lm3accS266 = _M0L6_2atmpS1631 ^ _M0L6_2atmpS1632;
  return _M0Lm3accS266;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS264,
  moonbit_string_t _M0L1yS265
) {
  int32_t _M0L6_2atmpS3001;
  int32_t _M0L6_2atmpS1622;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3001 = moonbit_val_array_equal(_M0L1xS264, _M0L1yS265);
  moonbit_decref(_M0L1xS264);
  moonbit_decref(_M0L1yS265);
  _M0L6_2atmpS1622 = _M0L6_2atmpS3001;
  return !_M0L6_2atmpS1622;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS261,
  int32_t _M0L5valueS260
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS260, _M0L4selfS261);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS263,
  moonbit_string_t _M0L5valueS262
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS262, _M0L4selfS263);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS259) {
  int64_t _M0L6_2atmpS1621;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1621 = (int64_t)_M0L4selfS259;
  return *(uint64_t*)&_M0L6_2atmpS1621;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS257,
  int32_t _M0L5valueS258
) {
  uint32_t _M0L6_2atmpS1620;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1620 = *(uint32_t*)&_M0L5valueS258;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS257, _M0L6_2atmpS1620);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS247,
  moonbit_string_t _M0L7contentS248,
  moonbit_string_t _M0L3locS250,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS252
) {
  moonbit_string_t _M0L6actualS246;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6actualS246 = _M0L3objS247.$0->$method_1(_M0L3objS247.$1);
  moonbit_incref(_M0L7contentS248);
  moonbit_incref(_M0L6actualS246);
  #line 192 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS246, _M0L7contentS248)
  ) {
    moonbit_string_t _M0L3locS249;
    moonbit_string_t _M0L9args__locS251;
    moonbit_string_t _M0L15expect__escapedS253;
    moonbit_string_t _M0L15actual__escapedS254;
    moonbit_string_t _M0L6_2atmpS1618;
    moonbit_string_t _M0L6_2atmpS1617;
    moonbit_string_t _M0L6_2atmpS3017;
    moonbit_string_t _M0L6_2atmpS1616;
    moonbit_string_t _M0L6_2atmpS3016;
    moonbit_string_t _M0L14expect__base64S255;
    moonbit_string_t _M0L6_2atmpS1615;
    moonbit_string_t _M0L6_2atmpS1614;
    moonbit_string_t _M0L6_2atmpS3015;
    moonbit_string_t _M0L6_2atmpS1613;
    moonbit_string_t _M0L6_2atmpS3014;
    moonbit_string_t _M0L14actual__base64S256;
    moonbit_string_t _M0L6_2atmpS1612;
    moonbit_string_t _M0L6_2atmpS3013;
    moonbit_string_t _M0L6_2atmpS1611;
    moonbit_string_t _M0L6_2atmpS3012;
    moonbit_string_t _M0L6_2atmpS1609;
    moonbit_string_t _M0L6_2atmpS1610;
    moonbit_string_t _M0L6_2atmpS3011;
    moonbit_string_t _M0L6_2atmpS1608;
    moonbit_string_t _M0L6_2atmpS3010;
    moonbit_string_t _M0L6_2atmpS1606;
    moonbit_string_t _M0L6_2atmpS1607;
    moonbit_string_t _M0L6_2atmpS3009;
    moonbit_string_t _M0L6_2atmpS1605;
    moonbit_string_t _M0L6_2atmpS3008;
    moonbit_string_t _M0L6_2atmpS1603;
    moonbit_string_t _M0L6_2atmpS1604;
    moonbit_string_t _M0L6_2atmpS3007;
    moonbit_string_t _M0L6_2atmpS1602;
    moonbit_string_t _M0L6_2atmpS3006;
    moonbit_string_t _M0L6_2atmpS1600;
    moonbit_string_t _M0L6_2atmpS1601;
    moonbit_string_t _M0L6_2atmpS3005;
    moonbit_string_t _M0L6_2atmpS1599;
    moonbit_string_t _M0L6_2atmpS3004;
    moonbit_string_t _M0L6_2atmpS1597;
    moonbit_string_t _M0L6_2atmpS1598;
    moonbit_string_t _M0L6_2atmpS3003;
    moonbit_string_t _M0L6_2atmpS1596;
    moonbit_string_t _M0L6_2atmpS3002;
    moonbit_string_t _M0L6_2atmpS1595;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1594;
    struct moonbit_result_0 _result_3282;
    #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L3locS249 = _M0MPB9SourceLoc16to__json__string(_M0L3locS250);
    #line 194 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L9args__locS251 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS252);
    moonbit_incref(_M0L7contentS248);
    #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15expect__escapedS253
    = _M0MPC16string6String6escape(_M0L7contentS248);
    moonbit_incref(_M0L6actualS246);
    #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15actual__escapedS254 = _M0MPC16string6String6escape(_M0L6actualS246);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1618
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS248);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1617
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1618);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3017
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_81.data, _M0L6_2atmpS1617);
    moonbit_decref(_M0L6_2atmpS1617);
    _M0L6_2atmpS1616 = _M0L6_2atmpS3017;
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3016
    = moonbit_add_string(_M0L6_2atmpS1616, (moonbit_string_t)moonbit_string_literal_81.data);
    moonbit_decref(_M0L6_2atmpS1616);
    _M0L14expect__base64S255 = _M0L6_2atmpS3016;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1615
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS246);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1614
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1615);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3015
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_81.data, _M0L6_2atmpS1614);
    moonbit_decref(_M0L6_2atmpS1614);
    _M0L6_2atmpS1613 = _M0L6_2atmpS3015;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3014
    = moonbit_add_string(_M0L6_2atmpS1613, (moonbit_string_t)moonbit_string_literal_81.data);
    moonbit_decref(_M0L6_2atmpS1613);
    _M0L14actual__base64S256 = _M0L6_2atmpS3014;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1612 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS249);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3013
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_82.data, _M0L6_2atmpS1612);
    moonbit_decref(_M0L6_2atmpS1612);
    _M0L6_2atmpS1611 = _M0L6_2atmpS3013;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3012
    = moonbit_add_string(_M0L6_2atmpS1611, (moonbit_string_t)moonbit_string_literal_83.data);
    moonbit_decref(_M0L6_2atmpS1611);
    _M0L6_2atmpS1609 = _M0L6_2atmpS3012;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1610
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS251);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3011 = moonbit_add_string(_M0L6_2atmpS1609, _M0L6_2atmpS1610);
    moonbit_decref(_M0L6_2atmpS1609);
    moonbit_decref(_M0L6_2atmpS1610);
    _M0L6_2atmpS1608 = _M0L6_2atmpS3011;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3010
    = moonbit_add_string(_M0L6_2atmpS1608, (moonbit_string_t)moonbit_string_literal_84.data);
    moonbit_decref(_M0L6_2atmpS1608);
    _M0L6_2atmpS1606 = _M0L6_2atmpS3010;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1607
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS253);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3009 = moonbit_add_string(_M0L6_2atmpS1606, _M0L6_2atmpS1607);
    moonbit_decref(_M0L6_2atmpS1606);
    moonbit_decref(_M0L6_2atmpS1607);
    _M0L6_2atmpS1605 = _M0L6_2atmpS3009;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3008
    = moonbit_add_string(_M0L6_2atmpS1605, (moonbit_string_t)moonbit_string_literal_85.data);
    moonbit_decref(_M0L6_2atmpS1605);
    _M0L6_2atmpS1603 = _M0L6_2atmpS3008;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1604
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS254);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3007 = moonbit_add_string(_M0L6_2atmpS1603, _M0L6_2atmpS1604);
    moonbit_decref(_M0L6_2atmpS1603);
    moonbit_decref(_M0L6_2atmpS1604);
    _M0L6_2atmpS1602 = _M0L6_2atmpS3007;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3006
    = moonbit_add_string(_M0L6_2atmpS1602, (moonbit_string_t)moonbit_string_literal_86.data);
    moonbit_decref(_M0L6_2atmpS1602);
    _M0L6_2atmpS1600 = _M0L6_2atmpS3006;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1601
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S255);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3005 = moonbit_add_string(_M0L6_2atmpS1600, _M0L6_2atmpS1601);
    moonbit_decref(_M0L6_2atmpS1600);
    moonbit_decref(_M0L6_2atmpS1601);
    _M0L6_2atmpS1599 = _M0L6_2atmpS3005;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3004
    = moonbit_add_string(_M0L6_2atmpS1599, (moonbit_string_t)moonbit_string_literal_87.data);
    moonbit_decref(_M0L6_2atmpS1599);
    _M0L6_2atmpS1597 = _M0L6_2atmpS3004;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1598
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S256);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3003 = moonbit_add_string(_M0L6_2atmpS1597, _M0L6_2atmpS1598);
    moonbit_decref(_M0L6_2atmpS1597);
    moonbit_decref(_M0L6_2atmpS1598);
    _M0L6_2atmpS1596 = _M0L6_2atmpS3003;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3002
    = moonbit_add_string(_M0L6_2atmpS1596, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1596);
    _M0L6_2atmpS1595 = _M0L6_2atmpS3002;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1594
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1594)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1594)->$0
    = _M0L6_2atmpS1595;
    _result_3282.tag = 0;
    _result_3282.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1594;
    return _result_3282;
  } else {
    int32_t _M0L6_2atmpS1619;
    struct moonbit_result_0 _result_3283;
    moonbit_decref(_M0L9args__locS252);
    moonbit_decref(_M0L3locS250);
    moonbit_decref(_M0L7contentS248);
    moonbit_decref(_M0L6actualS246);
    _M0L6_2atmpS1619 = 0;
    _result_3283.tag = 1;
    _result_3283.data.ok = _M0L6_2atmpS1619;
    return _result_3283;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS239
) {
  struct _M0TPB13StringBuilder* _M0L3bufS237;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS238;
  int32_t _M0L7_2abindS240;
  int32_t _M0L1iS241;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS237 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS238 = _M0L4selfS239;
  moonbit_incref(_M0L3bufS237);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS237, 91);
  _M0L7_2abindS240 = _M0L7_2aselfS238->$1;
  _M0L1iS241 = 0;
  while (1) {
    if (_M0L1iS241 < _M0L7_2abindS240) {
      int32_t _if__result_3285;
      moonbit_string_t* _M0L8_2afieldS3019;
      moonbit_string_t* _M0L3bufS1592;
      moonbit_string_t _M0L6_2atmpS3018;
      moonbit_string_t _M0L4itemS242;
      int32_t _M0L6_2atmpS1593;
      if (_M0L1iS241 != 0) {
        moonbit_incref(_M0L3bufS237);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS237, (moonbit_string_t)moonbit_string_literal_88.data);
      }
      if (_M0L1iS241 < 0) {
        _if__result_3285 = 1;
      } else {
        int32_t _M0L3lenS1591 = _M0L7_2aselfS238->$1;
        _if__result_3285 = _M0L1iS241 >= _M0L3lenS1591;
      }
      if (_if__result_3285) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3019 = _M0L7_2aselfS238->$0;
      _M0L3bufS1592 = _M0L8_2afieldS3019;
      _M0L6_2atmpS3018 = (moonbit_string_t)_M0L3bufS1592[_M0L1iS241];
      _M0L4itemS242 = _M0L6_2atmpS3018;
      if (_M0L4itemS242 == 0) {
        moonbit_incref(_M0L3bufS237);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS237, (moonbit_string_t)moonbit_string_literal_45.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS243 = _M0L4itemS242;
        moonbit_string_t _M0L6_2alocS244 = _M0L7_2aSomeS243;
        moonbit_string_t _M0L6_2atmpS1590;
        moonbit_incref(_M0L6_2alocS244);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1590
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS244);
        moonbit_incref(_M0L3bufS237);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS237, _M0L6_2atmpS1590);
      }
      _M0L6_2atmpS1593 = _M0L1iS241 + 1;
      _M0L1iS241 = _M0L6_2atmpS1593;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS238);
    }
    break;
  }
  moonbit_incref(_M0L3bufS237);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS237, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS237);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS236
) {
  moonbit_string_t _M0L6_2atmpS1589;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1588;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1589 = _M0L4selfS236;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1588 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1589);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1588);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS235
) {
  struct _M0TPB13StringBuilder* _M0L2sbS234;
  struct _M0TPC16string10StringView _M0L8_2afieldS3032;
  struct _M0TPC16string10StringView _M0L3pkgS1573;
  moonbit_string_t _M0L6_2atmpS1572;
  moonbit_string_t _M0L6_2atmpS3031;
  moonbit_string_t _M0L6_2atmpS1571;
  moonbit_string_t _M0L6_2atmpS3030;
  moonbit_string_t _M0L6_2atmpS1570;
  struct _M0TPC16string10StringView _M0L8_2afieldS3029;
  struct _M0TPC16string10StringView _M0L8filenameS1574;
  struct _M0TPC16string10StringView _M0L8_2afieldS3028;
  struct _M0TPC16string10StringView _M0L11start__lineS1577;
  moonbit_string_t _M0L6_2atmpS1576;
  moonbit_string_t _M0L6_2atmpS3027;
  moonbit_string_t _M0L6_2atmpS1575;
  struct _M0TPC16string10StringView _M0L8_2afieldS3026;
  struct _M0TPC16string10StringView _M0L13start__columnS1580;
  moonbit_string_t _M0L6_2atmpS1579;
  moonbit_string_t _M0L6_2atmpS3025;
  moonbit_string_t _M0L6_2atmpS1578;
  struct _M0TPC16string10StringView _M0L8_2afieldS3024;
  struct _M0TPC16string10StringView _M0L9end__lineS1583;
  moonbit_string_t _M0L6_2atmpS1582;
  moonbit_string_t _M0L6_2atmpS3023;
  moonbit_string_t _M0L6_2atmpS1581;
  struct _M0TPC16string10StringView _M0L8_2afieldS3022;
  int32_t _M0L6_2acntS3151;
  struct _M0TPC16string10StringView _M0L11end__columnS1587;
  moonbit_string_t _M0L6_2atmpS1586;
  moonbit_string_t _M0L6_2atmpS3021;
  moonbit_string_t _M0L6_2atmpS1585;
  moonbit_string_t _M0L6_2atmpS3020;
  moonbit_string_t _M0L6_2atmpS1584;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS234 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3032
  = (struct _M0TPC16string10StringView){
    _M0L4selfS235->$0_1, _M0L4selfS235->$0_2, _M0L4selfS235->$0_0
  };
  _M0L3pkgS1573 = _M0L8_2afieldS3032;
  moonbit_incref(_M0L3pkgS1573.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1572
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1573);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3031
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_89.data, _M0L6_2atmpS1572);
  moonbit_decref(_M0L6_2atmpS1572);
  _M0L6_2atmpS1571 = _M0L6_2atmpS3031;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3030
  = moonbit_add_string(_M0L6_2atmpS1571, (moonbit_string_t)moonbit_string_literal_81.data);
  moonbit_decref(_M0L6_2atmpS1571);
  _M0L6_2atmpS1570 = _M0L6_2atmpS3030;
  moonbit_incref(_M0L2sbS234);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS234, _M0L6_2atmpS1570);
  moonbit_incref(_M0L2sbS234);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS234, (moonbit_string_t)moonbit_string_literal_90.data);
  _M0L8_2afieldS3029
  = (struct _M0TPC16string10StringView){
    _M0L4selfS235->$1_1, _M0L4selfS235->$1_2, _M0L4selfS235->$1_0
  };
  _M0L8filenameS1574 = _M0L8_2afieldS3029;
  moonbit_incref(_M0L8filenameS1574.$0);
  moonbit_incref(_M0L2sbS234);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS234, _M0L8filenameS1574);
  _M0L8_2afieldS3028
  = (struct _M0TPC16string10StringView){
    _M0L4selfS235->$2_1, _M0L4selfS235->$2_2, _M0L4selfS235->$2_0
  };
  _M0L11start__lineS1577 = _M0L8_2afieldS3028;
  moonbit_incref(_M0L11start__lineS1577.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1576
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1577);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3027
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_91.data, _M0L6_2atmpS1576);
  moonbit_decref(_M0L6_2atmpS1576);
  _M0L6_2atmpS1575 = _M0L6_2atmpS3027;
  moonbit_incref(_M0L2sbS234);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS234, _M0L6_2atmpS1575);
  _M0L8_2afieldS3026
  = (struct _M0TPC16string10StringView){
    _M0L4selfS235->$3_1, _M0L4selfS235->$3_2, _M0L4selfS235->$3_0
  };
  _M0L13start__columnS1580 = _M0L8_2afieldS3026;
  moonbit_incref(_M0L13start__columnS1580.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1579
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1580);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3025
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_92.data, _M0L6_2atmpS1579);
  moonbit_decref(_M0L6_2atmpS1579);
  _M0L6_2atmpS1578 = _M0L6_2atmpS3025;
  moonbit_incref(_M0L2sbS234);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS234, _M0L6_2atmpS1578);
  _M0L8_2afieldS3024
  = (struct _M0TPC16string10StringView){
    _M0L4selfS235->$4_1, _M0L4selfS235->$4_2, _M0L4selfS235->$4_0
  };
  _M0L9end__lineS1583 = _M0L8_2afieldS3024;
  moonbit_incref(_M0L9end__lineS1583.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1582
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1583);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3023
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_93.data, _M0L6_2atmpS1582);
  moonbit_decref(_M0L6_2atmpS1582);
  _M0L6_2atmpS1581 = _M0L6_2atmpS3023;
  moonbit_incref(_M0L2sbS234);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS234, _M0L6_2atmpS1581);
  _M0L8_2afieldS3022
  = (struct _M0TPC16string10StringView){
    _M0L4selfS235->$5_1, _M0L4selfS235->$5_2, _M0L4selfS235->$5_0
  };
  _M0L6_2acntS3151 = Moonbit_object_header(_M0L4selfS235)->rc;
  if (_M0L6_2acntS3151 > 1) {
    int32_t _M0L11_2anew__cntS3157 = _M0L6_2acntS3151 - 1;
    Moonbit_object_header(_M0L4selfS235)->rc = _M0L11_2anew__cntS3157;
    moonbit_incref(_M0L8_2afieldS3022.$0);
  } else if (_M0L6_2acntS3151 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3156 =
      (struct _M0TPC16string10StringView){_M0L4selfS235->$4_1,
                                            _M0L4selfS235->$4_2,
                                            _M0L4selfS235->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3155;
    struct _M0TPC16string10StringView _M0L8_2afieldS3154;
    struct _M0TPC16string10StringView _M0L8_2afieldS3153;
    struct _M0TPC16string10StringView _M0L8_2afieldS3152;
    moonbit_decref(_M0L8_2afieldS3156.$0);
    _M0L8_2afieldS3155
    = (struct _M0TPC16string10StringView){
      _M0L4selfS235->$3_1, _M0L4selfS235->$3_2, _M0L4selfS235->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3155.$0);
    _M0L8_2afieldS3154
    = (struct _M0TPC16string10StringView){
      _M0L4selfS235->$2_1, _M0L4selfS235->$2_2, _M0L4selfS235->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3154.$0);
    _M0L8_2afieldS3153
    = (struct _M0TPC16string10StringView){
      _M0L4selfS235->$1_1, _M0L4selfS235->$1_2, _M0L4selfS235->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3153.$0);
    _M0L8_2afieldS3152
    = (struct _M0TPC16string10StringView){
      _M0L4selfS235->$0_1, _M0L4selfS235->$0_2, _M0L4selfS235->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3152.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS235);
  }
  _M0L11end__columnS1587 = _M0L8_2afieldS3022;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1586
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1587);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3021
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_94.data, _M0L6_2atmpS1586);
  moonbit_decref(_M0L6_2atmpS1586);
  _M0L6_2atmpS1585 = _M0L6_2atmpS3021;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3020
  = moonbit_add_string(_M0L6_2atmpS1585, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1585);
  _M0L6_2atmpS1584 = _M0L6_2atmpS3020;
  moonbit_incref(_M0L2sbS234);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS234, _M0L6_2atmpS1584);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS234);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS232,
  moonbit_string_t _M0L3strS233
) {
  int32_t _M0L3lenS1560;
  int32_t _M0L6_2atmpS1562;
  int32_t _M0L6_2atmpS1561;
  int32_t _M0L6_2atmpS1559;
  moonbit_bytes_t _M0L8_2afieldS3034;
  moonbit_bytes_t _M0L4dataS1563;
  int32_t _M0L3lenS1564;
  int32_t _M0L6_2atmpS1565;
  int32_t _M0L3lenS1567;
  int32_t _M0L6_2atmpS3033;
  int32_t _M0L6_2atmpS1569;
  int32_t _M0L6_2atmpS1568;
  int32_t _M0L6_2atmpS1566;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1560 = _M0L4selfS232->$1;
  _M0L6_2atmpS1562 = Moonbit_array_length(_M0L3strS233);
  _M0L6_2atmpS1561 = _M0L6_2atmpS1562 * 2;
  _M0L6_2atmpS1559 = _M0L3lenS1560 + _M0L6_2atmpS1561;
  moonbit_incref(_M0L4selfS232);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS232, _M0L6_2atmpS1559);
  _M0L8_2afieldS3034 = _M0L4selfS232->$0;
  _M0L4dataS1563 = _M0L8_2afieldS3034;
  _M0L3lenS1564 = _M0L4selfS232->$1;
  _M0L6_2atmpS1565 = Moonbit_array_length(_M0L3strS233);
  moonbit_incref(_M0L4dataS1563);
  moonbit_incref(_M0L3strS233);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1563, _M0L3lenS1564, _M0L3strS233, 0, _M0L6_2atmpS1565);
  _M0L3lenS1567 = _M0L4selfS232->$1;
  _M0L6_2atmpS3033 = Moonbit_array_length(_M0L3strS233);
  moonbit_decref(_M0L3strS233);
  _M0L6_2atmpS1569 = _M0L6_2atmpS3033;
  _M0L6_2atmpS1568 = _M0L6_2atmpS1569 * 2;
  _M0L6_2atmpS1566 = _M0L3lenS1567 + _M0L6_2atmpS1568;
  _M0L4selfS232->$1 = _M0L6_2atmpS1566;
  moonbit_decref(_M0L4selfS232);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS224,
  int32_t _M0L13bytes__offsetS219,
  moonbit_string_t _M0L3strS226,
  int32_t _M0L11str__offsetS222,
  int32_t _M0L6lengthS220
) {
  int32_t _M0L6_2atmpS1558;
  int32_t _M0L6_2atmpS1557;
  int32_t _M0L2e1S218;
  int32_t _M0L6_2atmpS1556;
  int32_t _M0L2e2S221;
  int32_t _M0L4len1S223;
  int32_t _M0L4len2S225;
  int32_t _if__result_3286;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1558 = _M0L6lengthS220 * 2;
  _M0L6_2atmpS1557 = _M0L13bytes__offsetS219 + _M0L6_2atmpS1558;
  _M0L2e1S218 = _M0L6_2atmpS1557 - 1;
  _M0L6_2atmpS1556 = _M0L11str__offsetS222 + _M0L6lengthS220;
  _M0L2e2S221 = _M0L6_2atmpS1556 - 1;
  _M0L4len1S223 = Moonbit_array_length(_M0L4selfS224);
  _M0L4len2S225 = Moonbit_array_length(_M0L3strS226);
  if (_M0L6lengthS220 >= 0) {
    if (_M0L13bytes__offsetS219 >= 0) {
      if (_M0L2e1S218 < _M0L4len1S223) {
        if (_M0L11str__offsetS222 >= 0) {
          _if__result_3286 = _M0L2e2S221 < _M0L4len2S225;
        } else {
          _if__result_3286 = 0;
        }
      } else {
        _if__result_3286 = 0;
      }
    } else {
      _if__result_3286 = 0;
    }
  } else {
    _if__result_3286 = 0;
  }
  if (_if__result_3286) {
    int32_t _M0L16end__str__offsetS227 =
      _M0L11str__offsetS222 + _M0L6lengthS220;
    int32_t _M0L1iS228 = _M0L11str__offsetS222;
    int32_t _M0L1jS229 = _M0L13bytes__offsetS219;
    while (1) {
      if (_M0L1iS228 < _M0L16end__str__offsetS227) {
        int32_t _M0L6_2atmpS1553 = _M0L3strS226[_M0L1iS228];
        int32_t _M0L6_2atmpS1552 = (int32_t)_M0L6_2atmpS1553;
        uint32_t _M0L1cS230 = *(uint32_t*)&_M0L6_2atmpS1552;
        uint32_t _M0L6_2atmpS1548 = _M0L1cS230 & 255u;
        int32_t _M0L6_2atmpS1547;
        int32_t _M0L6_2atmpS1549;
        uint32_t _M0L6_2atmpS1551;
        int32_t _M0L6_2atmpS1550;
        int32_t _M0L6_2atmpS1554;
        int32_t _M0L6_2atmpS1555;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1547 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1548);
        if (
          _M0L1jS229 < 0 || _M0L1jS229 >= Moonbit_array_length(_M0L4selfS224)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS224[_M0L1jS229] = _M0L6_2atmpS1547;
        _M0L6_2atmpS1549 = _M0L1jS229 + 1;
        _M0L6_2atmpS1551 = _M0L1cS230 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1550 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1551);
        if (
          _M0L6_2atmpS1549 < 0
          || _M0L6_2atmpS1549 >= Moonbit_array_length(_M0L4selfS224)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS224[_M0L6_2atmpS1549] = _M0L6_2atmpS1550;
        _M0L6_2atmpS1554 = _M0L1iS228 + 1;
        _M0L6_2atmpS1555 = _M0L1jS229 + 2;
        _M0L1iS228 = _M0L6_2atmpS1554;
        _M0L1jS229 = _M0L6_2atmpS1555;
        continue;
      } else {
        moonbit_decref(_M0L3strS226);
        moonbit_decref(_M0L4selfS224);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS226);
    moonbit_decref(_M0L4selfS224);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS215,
  double _M0L3objS214
) {
  struct _M0TPB6Logger _M0L6_2atmpS1545;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1545
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS215
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS214, _M0L6_2atmpS1545);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS217,
  struct _M0TPC16string10StringView _M0L3objS216
) {
  struct _M0TPB6Logger _M0L6_2atmpS1546;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1546
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS217
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS216, _M0L6_2atmpS1546);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS160
) {
  int32_t _M0L6_2atmpS1544;
  struct _M0TPC16string10StringView _M0L7_2abindS159;
  moonbit_string_t _M0L7_2adataS161;
  int32_t _M0L8_2astartS162;
  int32_t _M0L6_2atmpS1543;
  int32_t _M0L6_2aendS163;
  int32_t _M0Lm9_2acursorS164;
  int32_t _M0Lm13accept__stateS165;
  int32_t _M0Lm10match__endS166;
  int32_t _M0Lm20match__tag__saver__0S167;
  int32_t _M0Lm20match__tag__saver__1S168;
  int32_t _M0Lm20match__tag__saver__2S169;
  int32_t _M0Lm20match__tag__saver__3S170;
  int32_t _M0Lm20match__tag__saver__4S171;
  int32_t _M0Lm6tag__0S172;
  int32_t _M0Lm6tag__1S173;
  int32_t _M0Lm9tag__1__1S174;
  int32_t _M0Lm9tag__1__2S175;
  int32_t _M0Lm6tag__3S176;
  int32_t _M0Lm6tag__2S177;
  int32_t _M0Lm9tag__2__1S178;
  int32_t _M0Lm6tag__4S179;
  int32_t _M0L6_2atmpS1501;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1544 = Moonbit_array_length(_M0L4reprS160);
  _M0L7_2abindS159
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1544, _M0L4reprS160
  };
  moonbit_incref(_M0L7_2abindS159.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS161 = _M0MPC16string10StringView4data(_M0L7_2abindS159);
  moonbit_incref(_M0L7_2abindS159.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS162
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS159);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1543 = _M0MPC16string10StringView6length(_M0L7_2abindS159);
  _M0L6_2aendS163 = _M0L8_2astartS162 + _M0L6_2atmpS1543;
  _M0Lm9_2acursorS164 = _M0L8_2astartS162;
  _M0Lm13accept__stateS165 = -1;
  _M0Lm10match__endS166 = -1;
  _M0Lm20match__tag__saver__0S167 = -1;
  _M0Lm20match__tag__saver__1S168 = -1;
  _M0Lm20match__tag__saver__2S169 = -1;
  _M0Lm20match__tag__saver__3S170 = -1;
  _M0Lm20match__tag__saver__4S171 = -1;
  _M0Lm6tag__0S172 = -1;
  _M0Lm6tag__1S173 = -1;
  _M0Lm9tag__1__1S174 = -1;
  _M0Lm9tag__1__2S175 = -1;
  _M0Lm6tag__3S176 = -1;
  _M0Lm6tag__2S177 = -1;
  _M0Lm9tag__2__1S178 = -1;
  _M0Lm6tag__4S179 = -1;
  _M0L6_2atmpS1501 = _M0Lm9_2acursorS164;
  if (_M0L6_2atmpS1501 < _M0L6_2aendS163) {
    int32_t _M0L6_2atmpS1503 = _M0Lm9_2acursorS164;
    int32_t _M0L6_2atmpS1502;
    moonbit_incref(_M0L7_2adataS161);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1502
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1503);
    if (_M0L6_2atmpS1502 == 64) {
      int32_t _M0L6_2atmpS1504 = _M0Lm9_2acursorS164;
      _M0Lm9_2acursorS164 = _M0L6_2atmpS1504 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1505;
        _M0Lm6tag__0S172 = _M0Lm9_2acursorS164;
        _M0L6_2atmpS1505 = _M0Lm9_2acursorS164;
        if (_M0L6_2atmpS1505 < _M0L6_2aendS163) {
          int32_t _M0L6_2atmpS1542 = _M0Lm9_2acursorS164;
          int32_t _M0L10next__charS187;
          int32_t _M0L6_2atmpS1506;
          moonbit_incref(_M0L7_2adataS161);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS187
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1542);
          _M0L6_2atmpS1506 = _M0Lm9_2acursorS164;
          _M0Lm9_2acursorS164 = _M0L6_2atmpS1506 + 1;
          if (_M0L10next__charS187 == 58) {
            int32_t _M0L6_2atmpS1507 = _M0Lm9_2acursorS164;
            if (_M0L6_2atmpS1507 < _M0L6_2aendS163) {
              int32_t _M0L6_2atmpS1508 = _M0Lm9_2acursorS164;
              int32_t _M0L12dispatch__15S188;
              _M0Lm9_2acursorS164 = _M0L6_2atmpS1508 + 1;
              _M0L12dispatch__15S188 = 0;
              loop__label__15_191:;
              while (1) {
                int32_t _M0L6_2atmpS1509;
                switch (_M0L12dispatch__15S188) {
                  case 3: {
                    int32_t _M0L6_2atmpS1512;
                    _M0Lm9tag__1__2S175 = _M0Lm9tag__1__1S174;
                    _M0Lm9tag__1__1S174 = _M0Lm6tag__1S173;
                    _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                    _M0L6_2atmpS1512 = _M0Lm9_2acursorS164;
                    if (_M0L6_2atmpS1512 < _M0L6_2aendS163) {
                      int32_t _M0L6_2atmpS1517 = _M0Lm9_2acursorS164;
                      int32_t _M0L10next__charS195;
                      int32_t _M0L6_2atmpS1513;
                      moonbit_incref(_M0L7_2adataS161);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS195
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1517);
                      _M0L6_2atmpS1513 = _M0Lm9_2acursorS164;
                      _M0Lm9_2acursorS164 = _M0L6_2atmpS1513 + 1;
                      if (_M0L10next__charS195 < 58) {
                        if (_M0L10next__charS195 < 48) {
                          goto join_194;
                        } else {
                          int32_t _M0L6_2atmpS1514;
                          _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                          _M0Lm9tag__2__1S178 = _M0Lm6tag__2S177;
                          _M0Lm6tag__2S177 = _M0Lm9_2acursorS164;
                          _M0Lm6tag__3S176 = _M0Lm9_2acursorS164;
                          _M0L6_2atmpS1514 = _M0Lm9_2acursorS164;
                          if (_M0L6_2atmpS1514 < _M0L6_2aendS163) {
                            int32_t _M0L6_2atmpS1516 = _M0Lm9_2acursorS164;
                            int32_t _M0L10next__charS197;
                            int32_t _M0L6_2atmpS1515;
                            moonbit_incref(_M0L7_2adataS161);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS197
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1516);
                            _M0L6_2atmpS1515 = _M0Lm9_2acursorS164;
                            _M0Lm9_2acursorS164 = _M0L6_2atmpS1515 + 1;
                            if (_M0L10next__charS197 < 48) {
                              if (_M0L10next__charS197 == 45) {
                                goto join_189;
                              } else {
                                goto join_196;
                              }
                            } else if (_M0L10next__charS197 > 57) {
                              if (_M0L10next__charS197 < 59) {
                                _M0L12dispatch__15S188 = 3;
                                goto loop__label__15_191;
                              } else {
                                goto join_196;
                              }
                            } else {
                              _M0L12dispatch__15S188 = 6;
                              goto loop__label__15_191;
                            }
                            join_196:;
                            _M0L12dispatch__15S188 = 0;
                            goto loop__label__15_191;
                          } else {
                            goto join_180;
                          }
                        }
                      } else if (_M0L10next__charS195 > 58) {
                        goto join_194;
                      } else {
                        _M0L12dispatch__15S188 = 1;
                        goto loop__label__15_191;
                      }
                      join_194:;
                      _M0L12dispatch__15S188 = 0;
                      goto loop__label__15_191;
                    } else {
                      goto join_180;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1518;
                    _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                    _M0Lm6tag__2S177 = _M0Lm9_2acursorS164;
                    _M0L6_2atmpS1518 = _M0Lm9_2acursorS164;
                    if (_M0L6_2atmpS1518 < _M0L6_2aendS163) {
                      int32_t _M0L6_2atmpS1520 = _M0Lm9_2acursorS164;
                      int32_t _M0L10next__charS199;
                      int32_t _M0L6_2atmpS1519;
                      moonbit_incref(_M0L7_2adataS161);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS199
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1520);
                      _M0L6_2atmpS1519 = _M0Lm9_2acursorS164;
                      _M0Lm9_2acursorS164 = _M0L6_2atmpS1519 + 1;
                      if (_M0L10next__charS199 < 58) {
                        if (_M0L10next__charS199 < 48) {
                          goto join_198;
                        } else {
                          _M0L12dispatch__15S188 = 2;
                          goto loop__label__15_191;
                        }
                      } else if (_M0L10next__charS199 > 58) {
                        goto join_198;
                      } else {
                        _M0L12dispatch__15S188 = 3;
                        goto loop__label__15_191;
                      }
                      join_198:;
                      _M0L12dispatch__15S188 = 0;
                      goto loop__label__15_191;
                    } else {
                      goto join_180;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1521;
                    _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                    _M0L6_2atmpS1521 = _M0Lm9_2acursorS164;
                    if (_M0L6_2atmpS1521 < _M0L6_2aendS163) {
                      int32_t _M0L6_2atmpS1523 = _M0Lm9_2acursorS164;
                      int32_t _M0L10next__charS200;
                      int32_t _M0L6_2atmpS1522;
                      moonbit_incref(_M0L7_2adataS161);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS200
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1523);
                      _M0L6_2atmpS1522 = _M0Lm9_2acursorS164;
                      _M0Lm9_2acursorS164 = _M0L6_2atmpS1522 + 1;
                      if (_M0L10next__charS200 == 58) {
                        _M0L12dispatch__15S188 = 1;
                        goto loop__label__15_191;
                      } else {
                        _M0L12dispatch__15S188 = 0;
                        goto loop__label__15_191;
                      }
                    } else {
                      goto join_180;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1524;
                    _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                    _M0Lm6tag__4S179 = _M0Lm9_2acursorS164;
                    _M0L6_2atmpS1524 = _M0Lm9_2acursorS164;
                    if (_M0L6_2atmpS1524 < _M0L6_2aendS163) {
                      int32_t _M0L6_2atmpS1532 = _M0Lm9_2acursorS164;
                      int32_t _M0L10next__charS202;
                      int32_t _M0L6_2atmpS1525;
                      moonbit_incref(_M0L7_2adataS161);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS202
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1532);
                      _M0L6_2atmpS1525 = _M0Lm9_2acursorS164;
                      _M0Lm9_2acursorS164 = _M0L6_2atmpS1525 + 1;
                      if (_M0L10next__charS202 < 58) {
                        if (_M0L10next__charS202 < 48) {
                          goto join_201;
                        } else {
                          _M0L12dispatch__15S188 = 4;
                          goto loop__label__15_191;
                        }
                      } else if (_M0L10next__charS202 > 58) {
                        goto join_201;
                      } else {
                        int32_t _M0L6_2atmpS1526;
                        _M0Lm9tag__1__2S175 = _M0Lm9tag__1__1S174;
                        _M0Lm9tag__1__1S174 = _M0Lm6tag__1S173;
                        _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                        _M0L6_2atmpS1526 = _M0Lm9_2acursorS164;
                        if (_M0L6_2atmpS1526 < _M0L6_2aendS163) {
                          int32_t _M0L6_2atmpS1531 = _M0Lm9_2acursorS164;
                          int32_t _M0L10next__charS204;
                          int32_t _M0L6_2atmpS1527;
                          moonbit_incref(_M0L7_2adataS161);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS204
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1531);
                          _M0L6_2atmpS1527 = _M0Lm9_2acursorS164;
                          _M0Lm9_2acursorS164 = _M0L6_2atmpS1527 + 1;
                          if (_M0L10next__charS204 < 58) {
                            if (_M0L10next__charS204 < 48) {
                              goto join_203;
                            } else {
                              int32_t _M0L6_2atmpS1528;
                              _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                              _M0Lm9tag__2__1S178 = _M0Lm6tag__2S177;
                              _M0Lm6tag__2S177 = _M0Lm9_2acursorS164;
                              _M0L6_2atmpS1528 = _M0Lm9_2acursorS164;
                              if (_M0L6_2atmpS1528 < _M0L6_2aendS163) {
                                int32_t _M0L6_2atmpS1530 =
                                  _M0Lm9_2acursorS164;
                                int32_t _M0L10next__charS206;
                                int32_t _M0L6_2atmpS1529;
                                moonbit_incref(_M0L7_2adataS161);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS206
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1530);
                                _M0L6_2atmpS1529 = _M0Lm9_2acursorS164;
                                _M0Lm9_2acursorS164 = _M0L6_2atmpS1529 + 1;
                                if (_M0L10next__charS206 < 58) {
                                  if (_M0L10next__charS206 < 48) {
                                    goto join_205;
                                  } else {
                                    _M0L12dispatch__15S188 = 5;
                                    goto loop__label__15_191;
                                  }
                                } else if (_M0L10next__charS206 > 58) {
                                  goto join_205;
                                } else {
                                  _M0L12dispatch__15S188 = 3;
                                  goto loop__label__15_191;
                                }
                                join_205:;
                                _M0L12dispatch__15S188 = 0;
                                goto loop__label__15_191;
                              } else {
                                goto join_193;
                              }
                            }
                          } else if (_M0L10next__charS204 > 58) {
                            goto join_203;
                          } else {
                            _M0L12dispatch__15S188 = 1;
                            goto loop__label__15_191;
                          }
                          join_203:;
                          _M0L12dispatch__15S188 = 0;
                          goto loop__label__15_191;
                        } else {
                          goto join_180;
                        }
                      }
                      join_201:;
                      _M0L12dispatch__15S188 = 0;
                      goto loop__label__15_191;
                    } else {
                      goto join_180;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1533;
                    _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                    _M0Lm6tag__2S177 = _M0Lm9_2acursorS164;
                    _M0L6_2atmpS1533 = _M0Lm9_2acursorS164;
                    if (_M0L6_2atmpS1533 < _M0L6_2aendS163) {
                      int32_t _M0L6_2atmpS1535 = _M0Lm9_2acursorS164;
                      int32_t _M0L10next__charS208;
                      int32_t _M0L6_2atmpS1534;
                      moonbit_incref(_M0L7_2adataS161);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS208
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1535);
                      _M0L6_2atmpS1534 = _M0Lm9_2acursorS164;
                      _M0Lm9_2acursorS164 = _M0L6_2atmpS1534 + 1;
                      if (_M0L10next__charS208 < 58) {
                        if (_M0L10next__charS208 < 48) {
                          goto join_207;
                        } else {
                          _M0L12dispatch__15S188 = 5;
                          goto loop__label__15_191;
                        }
                      } else if (_M0L10next__charS208 > 58) {
                        goto join_207;
                      } else {
                        _M0L12dispatch__15S188 = 3;
                        goto loop__label__15_191;
                      }
                      join_207:;
                      _M0L12dispatch__15S188 = 0;
                      goto loop__label__15_191;
                    } else {
                      goto join_193;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1536;
                    _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                    _M0Lm6tag__2S177 = _M0Lm9_2acursorS164;
                    _M0Lm6tag__3S176 = _M0Lm9_2acursorS164;
                    _M0L6_2atmpS1536 = _M0Lm9_2acursorS164;
                    if (_M0L6_2atmpS1536 < _M0L6_2aendS163) {
                      int32_t _M0L6_2atmpS1538 = _M0Lm9_2acursorS164;
                      int32_t _M0L10next__charS210;
                      int32_t _M0L6_2atmpS1537;
                      moonbit_incref(_M0L7_2adataS161);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS210
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1538);
                      _M0L6_2atmpS1537 = _M0Lm9_2acursorS164;
                      _M0Lm9_2acursorS164 = _M0L6_2atmpS1537 + 1;
                      if (_M0L10next__charS210 < 48) {
                        if (_M0L10next__charS210 == 45) {
                          goto join_189;
                        } else {
                          goto join_209;
                        }
                      } else if (_M0L10next__charS210 > 57) {
                        if (_M0L10next__charS210 < 59) {
                          _M0L12dispatch__15S188 = 3;
                          goto loop__label__15_191;
                        } else {
                          goto join_209;
                        }
                      } else {
                        _M0L12dispatch__15S188 = 6;
                        goto loop__label__15_191;
                      }
                      join_209:;
                      _M0L12dispatch__15S188 = 0;
                      goto loop__label__15_191;
                    } else {
                      goto join_180;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1539;
                    _M0Lm9tag__1__1S174 = _M0Lm6tag__1S173;
                    _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                    _M0L6_2atmpS1539 = _M0Lm9_2acursorS164;
                    if (_M0L6_2atmpS1539 < _M0L6_2aendS163) {
                      int32_t _M0L6_2atmpS1541 = _M0Lm9_2acursorS164;
                      int32_t _M0L10next__charS212;
                      int32_t _M0L6_2atmpS1540;
                      moonbit_incref(_M0L7_2adataS161);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS212
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1541);
                      _M0L6_2atmpS1540 = _M0Lm9_2acursorS164;
                      _M0Lm9_2acursorS164 = _M0L6_2atmpS1540 + 1;
                      if (_M0L10next__charS212 < 58) {
                        if (_M0L10next__charS212 < 48) {
                          goto join_211;
                        } else {
                          _M0L12dispatch__15S188 = 2;
                          goto loop__label__15_191;
                        }
                      } else if (_M0L10next__charS212 > 58) {
                        goto join_211;
                      } else {
                        _M0L12dispatch__15S188 = 1;
                        goto loop__label__15_191;
                      }
                      join_211:;
                      _M0L12dispatch__15S188 = 0;
                      goto loop__label__15_191;
                    } else {
                      goto join_180;
                    }
                    break;
                  }
                  default: {
                    goto join_180;
                    break;
                  }
                }
                join_193:;
                _M0Lm6tag__1S173 = _M0Lm9tag__1__2S175;
                _M0Lm6tag__2S177 = _M0Lm9tag__2__1S178;
                _M0Lm20match__tag__saver__0S167 = _M0Lm6tag__0S172;
                _M0Lm20match__tag__saver__1S168 = _M0Lm6tag__1S173;
                _M0Lm20match__tag__saver__2S169 = _M0Lm6tag__2S177;
                _M0Lm20match__tag__saver__3S170 = _M0Lm6tag__3S176;
                _M0Lm20match__tag__saver__4S171 = _M0Lm6tag__4S179;
                _M0Lm13accept__stateS165 = 0;
                _M0Lm10match__endS166 = _M0Lm9_2acursorS164;
                goto join_180;
                join_189:;
                _M0Lm9tag__1__1S174 = _M0Lm9tag__1__2S175;
                _M0Lm6tag__1S173 = _M0Lm9_2acursorS164;
                _M0Lm6tag__2S177 = _M0Lm9tag__2__1S178;
                _M0L6_2atmpS1509 = _M0Lm9_2acursorS164;
                if (_M0L6_2atmpS1509 < _M0L6_2aendS163) {
                  int32_t _M0L6_2atmpS1511 = _M0Lm9_2acursorS164;
                  int32_t _M0L10next__charS192;
                  int32_t _M0L6_2atmpS1510;
                  moonbit_incref(_M0L7_2adataS161);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS192
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS161, _M0L6_2atmpS1511);
                  _M0L6_2atmpS1510 = _M0Lm9_2acursorS164;
                  _M0Lm9_2acursorS164 = _M0L6_2atmpS1510 + 1;
                  if (_M0L10next__charS192 < 58) {
                    if (_M0L10next__charS192 < 48) {
                      goto join_190;
                    } else {
                      _M0L12dispatch__15S188 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS192 > 58) {
                    goto join_190;
                  } else {
                    _M0L12dispatch__15S188 = 1;
                    continue;
                  }
                  join_190:;
                  _M0L12dispatch__15S188 = 0;
                  continue;
                } else {
                  goto join_180;
                }
                break;
              }
            } else {
              goto join_180;
            }
          } else {
            continue;
          }
        } else {
          goto join_180;
        }
        break;
      }
    } else {
      goto join_180;
    }
  } else {
    goto join_180;
  }
  join_180:;
  switch (_M0Lm13accept__stateS165) {
    case 0: {
      int32_t _M0L6_2atmpS1500 = _M0Lm20match__tag__saver__1S168;
      int32_t _M0L6_2atmpS1499 = _M0L6_2atmpS1500 + 1;
      int64_t _M0L6_2atmpS1496 = (int64_t)_M0L6_2atmpS1499;
      int32_t _M0L6_2atmpS1498 = _M0Lm20match__tag__saver__2S169;
      int64_t _M0L6_2atmpS1497 = (int64_t)_M0L6_2atmpS1498;
      struct _M0TPC16string10StringView _M0L11start__lineS181;
      int32_t _M0L6_2atmpS1495;
      int32_t _M0L6_2atmpS1494;
      int64_t _M0L6_2atmpS1491;
      int32_t _M0L6_2atmpS1493;
      int64_t _M0L6_2atmpS1492;
      struct _M0TPC16string10StringView _M0L13start__columnS182;
      int32_t _M0L6_2atmpS1490;
      int64_t _M0L6_2atmpS1487;
      int32_t _M0L6_2atmpS1489;
      int64_t _M0L6_2atmpS1488;
      struct _M0TPC16string10StringView _M0L3pkgS183;
      int32_t _M0L6_2atmpS1486;
      int32_t _M0L6_2atmpS1485;
      int64_t _M0L6_2atmpS1482;
      int32_t _M0L6_2atmpS1484;
      int64_t _M0L6_2atmpS1483;
      struct _M0TPC16string10StringView _M0L8filenameS184;
      int32_t _M0L6_2atmpS1481;
      int32_t _M0L6_2atmpS1480;
      int64_t _M0L6_2atmpS1477;
      int32_t _M0L6_2atmpS1479;
      int64_t _M0L6_2atmpS1478;
      struct _M0TPC16string10StringView _M0L9end__lineS185;
      int32_t _M0L6_2atmpS1476;
      int32_t _M0L6_2atmpS1475;
      int64_t _M0L6_2atmpS1472;
      int32_t _M0L6_2atmpS1474;
      int64_t _M0L6_2atmpS1473;
      struct _M0TPC16string10StringView _M0L11end__columnS186;
      struct _M0TPB13SourceLocRepr* _block_3303;
      moonbit_incref(_M0L7_2adataS161);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS181
      = _M0MPC16string6String4view(_M0L7_2adataS161, _M0L6_2atmpS1496, _M0L6_2atmpS1497);
      _M0L6_2atmpS1495 = _M0Lm20match__tag__saver__2S169;
      _M0L6_2atmpS1494 = _M0L6_2atmpS1495 + 1;
      _M0L6_2atmpS1491 = (int64_t)_M0L6_2atmpS1494;
      _M0L6_2atmpS1493 = _M0Lm20match__tag__saver__3S170;
      _M0L6_2atmpS1492 = (int64_t)_M0L6_2atmpS1493;
      moonbit_incref(_M0L7_2adataS161);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS182
      = _M0MPC16string6String4view(_M0L7_2adataS161, _M0L6_2atmpS1491, _M0L6_2atmpS1492);
      _M0L6_2atmpS1490 = _M0L8_2astartS162 + 1;
      _M0L6_2atmpS1487 = (int64_t)_M0L6_2atmpS1490;
      _M0L6_2atmpS1489 = _M0Lm20match__tag__saver__0S167;
      _M0L6_2atmpS1488 = (int64_t)_M0L6_2atmpS1489;
      moonbit_incref(_M0L7_2adataS161);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS183
      = _M0MPC16string6String4view(_M0L7_2adataS161, _M0L6_2atmpS1487, _M0L6_2atmpS1488);
      _M0L6_2atmpS1486 = _M0Lm20match__tag__saver__0S167;
      _M0L6_2atmpS1485 = _M0L6_2atmpS1486 + 1;
      _M0L6_2atmpS1482 = (int64_t)_M0L6_2atmpS1485;
      _M0L6_2atmpS1484 = _M0Lm20match__tag__saver__1S168;
      _M0L6_2atmpS1483 = (int64_t)_M0L6_2atmpS1484;
      moonbit_incref(_M0L7_2adataS161);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS184
      = _M0MPC16string6String4view(_M0L7_2adataS161, _M0L6_2atmpS1482, _M0L6_2atmpS1483);
      _M0L6_2atmpS1481 = _M0Lm20match__tag__saver__3S170;
      _M0L6_2atmpS1480 = _M0L6_2atmpS1481 + 1;
      _M0L6_2atmpS1477 = (int64_t)_M0L6_2atmpS1480;
      _M0L6_2atmpS1479 = _M0Lm20match__tag__saver__4S171;
      _M0L6_2atmpS1478 = (int64_t)_M0L6_2atmpS1479;
      moonbit_incref(_M0L7_2adataS161);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS185
      = _M0MPC16string6String4view(_M0L7_2adataS161, _M0L6_2atmpS1477, _M0L6_2atmpS1478);
      _M0L6_2atmpS1476 = _M0Lm20match__tag__saver__4S171;
      _M0L6_2atmpS1475 = _M0L6_2atmpS1476 + 1;
      _M0L6_2atmpS1472 = (int64_t)_M0L6_2atmpS1475;
      _M0L6_2atmpS1474 = _M0Lm10match__endS166;
      _M0L6_2atmpS1473 = (int64_t)_M0L6_2atmpS1474;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS186
      = _M0MPC16string6String4view(_M0L7_2adataS161, _M0L6_2atmpS1472, _M0L6_2atmpS1473);
      _block_3303
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3303)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3303->$0_0 = _M0L3pkgS183.$0;
      _block_3303->$0_1 = _M0L3pkgS183.$1;
      _block_3303->$0_2 = _M0L3pkgS183.$2;
      _block_3303->$1_0 = _M0L8filenameS184.$0;
      _block_3303->$1_1 = _M0L8filenameS184.$1;
      _block_3303->$1_2 = _M0L8filenameS184.$2;
      _block_3303->$2_0 = _M0L11start__lineS181.$0;
      _block_3303->$2_1 = _M0L11start__lineS181.$1;
      _block_3303->$2_2 = _M0L11start__lineS181.$2;
      _block_3303->$3_0 = _M0L13start__columnS182.$0;
      _block_3303->$3_1 = _M0L13start__columnS182.$1;
      _block_3303->$3_2 = _M0L13start__columnS182.$2;
      _block_3303->$4_0 = _M0L9end__lineS185.$0;
      _block_3303->$4_1 = _M0L9end__lineS185.$1;
      _block_3303->$4_2 = _M0L9end__lineS185.$2;
      _block_3303->$5_0 = _M0L11end__columnS186.$0;
      _block_3303->$5_1 = _M0L11end__columnS186.$1;
      _block_3303->$5_2 = _M0L11end__columnS186.$2;
      return _block_3303;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS161);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS157,
  int32_t _M0L5indexS158
) {
  int32_t _M0L3lenS156;
  int32_t _if__result_3304;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS156 = _M0L4selfS157->$1;
  if (_M0L5indexS158 >= 0) {
    _if__result_3304 = _M0L5indexS158 < _M0L3lenS156;
  } else {
    _if__result_3304 = 0;
  }
  if (_if__result_3304) {
    moonbit_string_t* _M0L6_2atmpS1471;
    moonbit_string_t _M0L6_2atmpS3035;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1471 = _M0MPC15array5Array6bufferGsE(_M0L4selfS157);
    if (
      _M0L5indexS158 < 0
      || _M0L5indexS158 >= Moonbit_array_length(_M0L6_2atmpS1471)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3035 = (moonbit_string_t)_M0L6_2atmpS1471[_M0L5indexS158];
    moonbit_incref(_M0L6_2atmpS3035);
    moonbit_decref(_M0L6_2atmpS1471);
    return _M0L6_2atmpS3035;
  } else {
    moonbit_decref(_M0L4selfS157);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS155) {
  int32_t _M0L8_2afieldS3036;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3036 = _M0L4selfS155->$1;
  moonbit_decref(_M0L4selfS155);
  return _M0L8_2afieldS3036;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS152
) {
  moonbit_string_t* _M0L8_2afieldS3037;
  int32_t _M0L6_2acntS3158;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3037 = _M0L4selfS152->$0;
  _M0L6_2acntS3158 = Moonbit_object_header(_M0L4selfS152)->rc;
  if (_M0L6_2acntS3158 > 1) {
    int32_t _M0L11_2anew__cntS3159 = _M0L6_2acntS3158 - 1;
    Moonbit_object_header(_M0L4selfS152)->rc = _M0L11_2anew__cntS3159;
    moonbit_incref(_M0L8_2afieldS3037);
  } else if (_M0L6_2acntS3158 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS152);
  }
  return _M0L8_2afieldS3037;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS153
) {
  struct _M0TUsiE** _M0L8_2afieldS3038;
  int32_t _M0L6_2acntS3160;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3038 = _M0L4selfS153->$0;
  _M0L6_2acntS3160 = Moonbit_object_header(_M0L4selfS153)->rc;
  if (_M0L6_2acntS3160 > 1) {
    int32_t _M0L11_2anew__cntS3161 = _M0L6_2acntS3160 - 1;
    Moonbit_object_header(_M0L4selfS153)->rc = _M0L11_2anew__cntS3161;
    moonbit_incref(_M0L8_2afieldS3038);
  } else if (_M0L6_2acntS3160 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS153);
  }
  return _M0L8_2afieldS3038;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS154
) {
  void** _M0L8_2afieldS3039;
  int32_t _M0L6_2acntS3162;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3039 = _M0L4selfS154->$0;
  _M0L6_2acntS3162 = Moonbit_object_header(_M0L4selfS154)->rc;
  if (_M0L6_2acntS3162 > 1) {
    int32_t _M0L11_2anew__cntS3163 = _M0L6_2acntS3162 - 1;
    Moonbit_object_header(_M0L4selfS154)->rc = _M0L11_2anew__cntS3163;
    moonbit_incref(_M0L8_2afieldS3039);
  } else if (_M0L6_2acntS3162 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS154);
  }
  return _M0L8_2afieldS3039;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS151) {
  struct _M0TPB13StringBuilder* _M0L3bufS150;
  struct _M0TPB6Logger _M0L6_2atmpS1470;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS150 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS150);
  _M0L6_2atmpS1470
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS150
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS151, _M0L6_2atmpS1470);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS150);
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS144
) {
  int32_t _M0L17codepoint__lengthS143;
  int32_t _M0L6_2atmpS1469;
  moonbit_bytes_t _M0L4dataS145;
  int32_t _M0L1iS146;
  int32_t _M0L12utf16__indexS147;
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_incref(_M0L1sS144);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L17codepoint__lengthS143
  = _M0MPC16string6String20char__length_2einner(_M0L1sS144, 0, 4294967296ll);
  _M0L6_2atmpS1469 = _M0L17codepoint__lengthS143 * 4;
  _M0L4dataS145 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1469, 0);
  _M0L1iS146 = 0;
  _M0L12utf16__indexS147 = 0;
  while (1) {
    if (_M0L1iS146 < _M0L17codepoint__lengthS143) {
      int32_t _M0L6_2atmpS1466;
      int32_t _M0L1cS148;
      int32_t _M0L6_2atmpS1467;
      int32_t _M0L6_2atmpS1468;
      moonbit_incref(_M0L1sS144);
      #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1466
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS144, _M0L12utf16__indexS147);
      _M0L1cS148 = _M0L6_2atmpS1466;
      if (_M0L1cS148 > 65535) {
        int32_t _M0L6_2atmpS1434 = _M0L1iS146 * 4;
        int32_t _M0L6_2atmpS1436 = _M0L1cS148 & 255;
        int32_t _M0L6_2atmpS1435 = _M0L6_2atmpS1436 & 0xff;
        int32_t _M0L6_2atmpS1441;
        int32_t _M0L6_2atmpS1437;
        int32_t _M0L6_2atmpS1440;
        int32_t _M0L6_2atmpS1439;
        int32_t _M0L6_2atmpS1438;
        int32_t _M0L6_2atmpS1446;
        int32_t _M0L6_2atmpS1442;
        int32_t _M0L6_2atmpS1445;
        int32_t _M0L6_2atmpS1444;
        int32_t _M0L6_2atmpS1443;
        int32_t _M0L6_2atmpS1451;
        int32_t _M0L6_2atmpS1447;
        int32_t _M0L6_2atmpS1450;
        int32_t _M0L6_2atmpS1449;
        int32_t _M0L6_2atmpS1448;
        int32_t _M0L6_2atmpS1452;
        int32_t _M0L6_2atmpS1453;
        if (
          _M0L6_2atmpS1434 < 0
          || _M0L6_2atmpS1434 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1434] = _M0L6_2atmpS1435;
        _M0L6_2atmpS1441 = _M0L1iS146 * 4;
        _M0L6_2atmpS1437 = _M0L6_2atmpS1441 + 1;
        _M0L6_2atmpS1440 = _M0L1cS148 >> 8;
        _M0L6_2atmpS1439 = _M0L6_2atmpS1440 & 255;
        _M0L6_2atmpS1438 = _M0L6_2atmpS1439 & 0xff;
        if (
          _M0L6_2atmpS1437 < 0
          || _M0L6_2atmpS1437 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1437] = _M0L6_2atmpS1438;
        _M0L6_2atmpS1446 = _M0L1iS146 * 4;
        _M0L6_2atmpS1442 = _M0L6_2atmpS1446 + 2;
        _M0L6_2atmpS1445 = _M0L1cS148 >> 16;
        _M0L6_2atmpS1444 = _M0L6_2atmpS1445 & 255;
        _M0L6_2atmpS1443 = _M0L6_2atmpS1444 & 0xff;
        if (
          _M0L6_2atmpS1442 < 0
          || _M0L6_2atmpS1442 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1442] = _M0L6_2atmpS1443;
        _M0L6_2atmpS1451 = _M0L1iS146 * 4;
        _M0L6_2atmpS1447 = _M0L6_2atmpS1451 + 3;
        _M0L6_2atmpS1450 = _M0L1cS148 >> 24;
        _M0L6_2atmpS1449 = _M0L6_2atmpS1450 & 255;
        _M0L6_2atmpS1448 = _M0L6_2atmpS1449 & 0xff;
        if (
          _M0L6_2atmpS1447 < 0
          || _M0L6_2atmpS1447 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 114 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1447] = _M0L6_2atmpS1448;
        _M0L6_2atmpS1452 = _M0L1iS146 + 1;
        _M0L6_2atmpS1453 = _M0L12utf16__indexS147 + 2;
        _M0L1iS146 = _M0L6_2atmpS1452;
        _M0L12utf16__indexS147 = _M0L6_2atmpS1453;
        continue;
      } else {
        int32_t _M0L6_2atmpS1454 = _M0L1iS146 * 4;
        int32_t _M0L6_2atmpS1456 = _M0L1cS148 & 255;
        int32_t _M0L6_2atmpS1455 = _M0L6_2atmpS1456 & 0xff;
        int32_t _M0L6_2atmpS1461;
        int32_t _M0L6_2atmpS1457;
        int32_t _M0L6_2atmpS1460;
        int32_t _M0L6_2atmpS1459;
        int32_t _M0L6_2atmpS1458;
        int32_t _M0L6_2atmpS1463;
        int32_t _M0L6_2atmpS1462;
        int32_t _M0L6_2atmpS1465;
        int32_t _M0L6_2atmpS1464;
        if (
          _M0L6_2atmpS1454 < 0
          || _M0L6_2atmpS1454 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1454] = _M0L6_2atmpS1455;
        _M0L6_2atmpS1461 = _M0L1iS146 * 4;
        _M0L6_2atmpS1457 = _M0L6_2atmpS1461 + 1;
        _M0L6_2atmpS1460 = _M0L1cS148 >> 8;
        _M0L6_2atmpS1459 = _M0L6_2atmpS1460 & 255;
        _M0L6_2atmpS1458 = _M0L6_2atmpS1459 & 0xff;
        if (
          _M0L6_2atmpS1457 < 0
          || _M0L6_2atmpS1457 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1457] = _M0L6_2atmpS1458;
        _M0L6_2atmpS1463 = _M0L1iS146 * 4;
        _M0L6_2atmpS1462 = _M0L6_2atmpS1463 + 2;
        if (
          _M0L6_2atmpS1462 < 0
          || _M0L6_2atmpS1462 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1462] = 0;
        _M0L6_2atmpS1465 = _M0L1iS146 * 4;
        _M0L6_2atmpS1464 = _M0L6_2atmpS1465 + 3;
        if (
          _M0L6_2atmpS1464 < 0
          || _M0L6_2atmpS1464 >= Moonbit_array_length(_M0L4dataS145)
        ) {
          #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS145[_M0L6_2atmpS1464] = 0;
      }
      _M0L6_2atmpS1467 = _M0L1iS146 + 1;
      _M0L6_2atmpS1468 = _M0L12utf16__indexS147 + 1;
      _M0L1iS146 = _M0L6_2atmpS1467;
      _M0L12utf16__indexS147 = _M0L6_2atmpS1468;
      continue;
    } else {
      moonbit_decref(_M0L1sS144);
    }
    break;
  }
  #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS145);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS140,
  int32_t _M0L5indexS141
) {
  int32_t _M0L2c1S139;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S139 = _M0L4selfS140[_M0L5indexS141];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S139)) {
    int32_t _M0L6_2atmpS1433 = _M0L5indexS141 + 1;
    int32_t _M0L6_2atmpS3040 = _M0L4selfS140[_M0L6_2atmpS1433];
    int32_t _M0L2c2S142;
    int32_t _M0L6_2atmpS1431;
    int32_t _M0L6_2atmpS1432;
    moonbit_decref(_M0L4selfS140);
    _M0L2c2S142 = _M0L6_2atmpS3040;
    _M0L6_2atmpS1431 = (int32_t)_M0L2c1S139;
    _M0L6_2atmpS1432 = (int32_t)_M0L2c2S142;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1431, _M0L6_2atmpS1432);
  } else {
    moonbit_decref(_M0L4selfS140);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S139);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS138) {
  int32_t _M0L6_2atmpS1430;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1430 = (int32_t)_M0L4selfS138;
  return _M0L6_2atmpS1430;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS136,
  int32_t _M0L8trailingS137
) {
  int32_t _M0L6_2atmpS1429;
  int32_t _M0L6_2atmpS1428;
  int32_t _M0L6_2atmpS1427;
  int32_t _M0L6_2atmpS1426;
  int32_t _M0L6_2atmpS1425;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1429 = _M0L7leadingS136 - 55296;
  _M0L6_2atmpS1428 = _M0L6_2atmpS1429 * 1024;
  _M0L6_2atmpS1427 = _M0L6_2atmpS1428 + _M0L8trailingS137;
  _M0L6_2atmpS1426 = _M0L6_2atmpS1427 - 56320;
  _M0L6_2atmpS1425 = _M0L6_2atmpS1426 + 65536;
  return _M0L6_2atmpS1425;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS129,
  int32_t _M0L13start__offsetS130,
  int64_t _M0L11end__offsetS127
) {
  int32_t _M0L11end__offsetS126;
  int32_t _if__result_3306;
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS127 == 4294967296ll) {
    _M0L11end__offsetS126 = Moonbit_array_length(_M0L4selfS129);
  } else {
    int64_t _M0L7_2aSomeS128 = _M0L11end__offsetS127;
    _M0L11end__offsetS126 = (int32_t)_M0L7_2aSomeS128;
  }
  if (_M0L13start__offsetS130 >= 0) {
    if (_M0L13start__offsetS130 <= _M0L11end__offsetS126) {
      int32_t _M0L6_2atmpS1418 = Moonbit_array_length(_M0L4selfS129);
      _if__result_3306 = _M0L11end__offsetS126 <= _M0L6_2atmpS1418;
    } else {
      _if__result_3306 = 0;
    }
  } else {
    _if__result_3306 = 0;
  }
  if (_if__result_3306) {
    int32_t _M0L12utf16__indexS131 = _M0L13start__offsetS130;
    int32_t _M0L11char__countS132 = 0;
    while (1) {
      if (_M0L12utf16__indexS131 < _M0L11end__offsetS126) {
        int32_t _M0L2c1S133 = _M0L4selfS129[_M0L12utf16__indexS131];
        int32_t _if__result_3308;
        int32_t _M0L6_2atmpS1423;
        int32_t _M0L6_2atmpS1424;
        #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S133)) {
          int32_t _M0L6_2atmpS1419 = _M0L12utf16__indexS131 + 1;
          _if__result_3308 = _M0L6_2atmpS1419 < _M0L11end__offsetS126;
        } else {
          _if__result_3308 = 0;
        }
        if (_if__result_3308) {
          int32_t _M0L6_2atmpS1422 = _M0L12utf16__indexS131 + 1;
          int32_t _M0L2c2S134 = _M0L4selfS129[_M0L6_2atmpS1422];
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S134)) {
            int32_t _M0L6_2atmpS1420 = _M0L12utf16__indexS131 + 2;
            int32_t _M0L6_2atmpS1421 = _M0L11char__countS132 + 1;
            _M0L12utf16__indexS131 = _M0L6_2atmpS1420;
            _M0L11char__countS132 = _M0L6_2atmpS1421;
            continue;
          } else {
            #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
            _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_72.data, (moonbit_string_t)moonbit_string_literal_95.data);
          }
        }
        _M0L6_2atmpS1423 = _M0L12utf16__indexS131 + 1;
        _M0L6_2atmpS1424 = _M0L11char__countS132 + 1;
        _M0L12utf16__indexS131 = _M0L6_2atmpS1423;
        _M0L11char__countS132 = _M0L6_2atmpS1424;
        continue;
      } else {
        moonbit_decref(_M0L4selfS129);
        return _M0L11char__countS132;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS129);
    #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_96.data, (moonbit_string_t)moonbit_string_literal_97.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS125) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS125 >= 56320) {
    return _M0L4selfS125 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS124) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS124 >= 55296) {
    return _M0L4selfS124 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS105) {
  struct _M0TPB13StringBuilder* _M0L3bufS103;
  int32_t _M0L3lenS104;
  int32_t _M0L3remS106;
  int32_t _M0L1iS107;
  #line 61 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L3bufS103 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS104 = Moonbit_array_length(_M0L4dataS105);
  _M0L3remS106 = _M0L3lenS104 % 3;
  _M0L1iS107 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1370 = _M0L3lenS104 - _M0L3remS106;
    if (_M0L1iS107 < _M0L6_2atmpS1370) {
      int32_t _M0L6_2atmpS1392;
      int32_t _M0L2b0S108;
      int32_t _M0L6_2atmpS1391;
      int32_t _M0L6_2atmpS1390;
      int32_t _M0L2b1S109;
      int32_t _M0L6_2atmpS1389;
      int32_t _M0L6_2atmpS1388;
      int32_t _M0L2b2S110;
      int32_t _M0L6_2atmpS1387;
      int32_t _M0L6_2atmpS1386;
      int32_t _M0L2x0S111;
      int32_t _M0L6_2atmpS1385;
      int32_t _M0L6_2atmpS1382;
      int32_t _M0L6_2atmpS1384;
      int32_t _M0L6_2atmpS1383;
      int32_t _M0L6_2atmpS1381;
      int32_t _M0L2x1S112;
      int32_t _M0L6_2atmpS1380;
      int32_t _M0L6_2atmpS1377;
      int32_t _M0L6_2atmpS1379;
      int32_t _M0L6_2atmpS1378;
      int32_t _M0L6_2atmpS1376;
      int32_t _M0L2x2S113;
      int32_t _M0L6_2atmpS1375;
      int32_t _M0L2x3S114;
      int32_t _M0L6_2atmpS1371;
      int32_t _M0L6_2atmpS1372;
      int32_t _M0L6_2atmpS1373;
      int32_t _M0L6_2atmpS1374;
      int32_t _M0L6_2atmpS1393;
      if (
        _M0L1iS107 < 0 || _M0L1iS107 >= Moonbit_array_length(_M0L4dataS105)
      ) {
        #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1392 = (int32_t)_M0L4dataS105[_M0L1iS107];
      _M0L2b0S108 = (int32_t)_M0L6_2atmpS1392;
      _M0L6_2atmpS1391 = _M0L1iS107 + 1;
      if (
        _M0L6_2atmpS1391 < 0
        || _M0L6_2atmpS1391 >= Moonbit_array_length(_M0L4dataS105)
      ) {
        #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1390 = (int32_t)_M0L4dataS105[_M0L6_2atmpS1391];
      _M0L2b1S109 = (int32_t)_M0L6_2atmpS1390;
      _M0L6_2atmpS1389 = _M0L1iS107 + 2;
      if (
        _M0L6_2atmpS1389 < 0
        || _M0L6_2atmpS1389 >= Moonbit_array_length(_M0L4dataS105)
      ) {
        #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1388 = (int32_t)_M0L4dataS105[_M0L6_2atmpS1389];
      _M0L2b2S110 = (int32_t)_M0L6_2atmpS1388;
      _M0L6_2atmpS1387 = _M0L2b0S108 & 252;
      _M0L6_2atmpS1386 = _M0L6_2atmpS1387 >> 2;
      if (
        _M0L6_2atmpS1386 < 0
        || _M0L6_2atmpS1386
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x0S111 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1386];
      _M0L6_2atmpS1385 = _M0L2b0S108 & 3;
      _M0L6_2atmpS1382 = _M0L6_2atmpS1385 << 4;
      _M0L6_2atmpS1384 = _M0L2b1S109 & 240;
      _M0L6_2atmpS1383 = _M0L6_2atmpS1384 >> 4;
      _M0L6_2atmpS1381 = _M0L6_2atmpS1382 | _M0L6_2atmpS1383;
      if (
        _M0L6_2atmpS1381 < 0
        || _M0L6_2atmpS1381
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x1S112 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1381];
      _M0L6_2atmpS1380 = _M0L2b1S109 & 15;
      _M0L6_2atmpS1377 = _M0L6_2atmpS1380 << 2;
      _M0L6_2atmpS1379 = _M0L2b2S110 & 192;
      _M0L6_2atmpS1378 = _M0L6_2atmpS1379 >> 6;
      _M0L6_2atmpS1376 = _M0L6_2atmpS1377 | _M0L6_2atmpS1378;
      if (
        _M0L6_2atmpS1376 < 0
        || _M0L6_2atmpS1376
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x2S113 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1376];
      _M0L6_2atmpS1375 = _M0L2b2S110 & 63;
      if (
        _M0L6_2atmpS1375 < 0
        || _M0L6_2atmpS1375
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x3S114 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1375];
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1371 = _M0MPC14byte4Byte8to__char(_M0L2x0S111);
      moonbit_incref(_M0L3bufS103);
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1371);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1372 = _M0MPC14byte4Byte8to__char(_M0L2x1S112);
      moonbit_incref(_M0L3bufS103);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1372);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1373 = _M0MPC14byte4Byte8to__char(_M0L2x2S113);
      moonbit_incref(_M0L3bufS103);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1373);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1374 = _M0MPC14byte4Byte8to__char(_M0L2x3S114);
      moonbit_incref(_M0L3bufS103);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1374);
      _M0L6_2atmpS1393 = _M0L1iS107 + 3;
      _M0L1iS107 = _M0L6_2atmpS1393;
      continue;
    }
    break;
  }
  if (_M0L3remS106 == 1) {
    int32_t _M0L6_2atmpS1401 = _M0L3lenS104 - 1;
    int32_t _M0L6_2atmpS3041;
    int32_t _M0L6_2atmpS1400;
    int32_t _M0L2b0S116;
    int32_t _M0L6_2atmpS1399;
    int32_t _M0L6_2atmpS1398;
    int32_t _M0L2x0S117;
    int32_t _M0L6_2atmpS1397;
    int32_t _M0L6_2atmpS1396;
    int32_t _M0L2x1S118;
    int32_t _M0L6_2atmpS1394;
    int32_t _M0L6_2atmpS1395;
    if (
      _M0L6_2atmpS1401 < 0
      || _M0L6_2atmpS1401 >= Moonbit_array_length(_M0L4dataS105)
    ) {
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3041 = (int32_t)_M0L4dataS105[_M0L6_2atmpS1401];
    moonbit_decref(_M0L4dataS105);
    _M0L6_2atmpS1400 = _M0L6_2atmpS3041;
    _M0L2b0S116 = (int32_t)_M0L6_2atmpS1400;
    _M0L6_2atmpS1399 = _M0L2b0S116 & 252;
    _M0L6_2atmpS1398 = _M0L6_2atmpS1399 >> 2;
    if (
      _M0L6_2atmpS1398 < 0
      || _M0L6_2atmpS1398
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S117 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1398];
    _M0L6_2atmpS1397 = _M0L2b0S116 & 3;
    _M0L6_2atmpS1396 = _M0L6_2atmpS1397 << 4;
    if (
      _M0L6_2atmpS1396 < 0
      || _M0L6_2atmpS1396
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S118 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1396];
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1394 = _M0MPC14byte4Byte8to__char(_M0L2x0S117);
    moonbit_incref(_M0L3bufS103);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1394);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1395 = _M0MPC14byte4Byte8to__char(_M0L2x1S118);
    moonbit_incref(_M0L3bufS103);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1395);
    moonbit_incref(_M0L3bufS103);
    #line 85 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, 61);
    moonbit_incref(_M0L3bufS103);
    #line 86 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, 61);
  } else if (_M0L3remS106 == 2) {
    int32_t _M0L6_2atmpS1417 = _M0L3lenS104 - 2;
    int32_t _M0L6_2atmpS1416;
    int32_t _M0L2b0S119;
    int32_t _M0L6_2atmpS1415;
    int32_t _M0L6_2atmpS3042;
    int32_t _M0L6_2atmpS1414;
    int32_t _M0L2b1S120;
    int32_t _M0L6_2atmpS1413;
    int32_t _M0L6_2atmpS1412;
    int32_t _M0L2x0S121;
    int32_t _M0L6_2atmpS1411;
    int32_t _M0L6_2atmpS1408;
    int32_t _M0L6_2atmpS1410;
    int32_t _M0L6_2atmpS1409;
    int32_t _M0L6_2atmpS1407;
    int32_t _M0L2x1S122;
    int32_t _M0L6_2atmpS1406;
    int32_t _M0L6_2atmpS1405;
    int32_t _M0L2x2S123;
    int32_t _M0L6_2atmpS1402;
    int32_t _M0L6_2atmpS1403;
    int32_t _M0L6_2atmpS1404;
    if (
      _M0L6_2atmpS1417 < 0
      || _M0L6_2atmpS1417 >= Moonbit_array_length(_M0L4dataS105)
    ) {
      #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1416 = (int32_t)_M0L4dataS105[_M0L6_2atmpS1417];
    _M0L2b0S119 = (int32_t)_M0L6_2atmpS1416;
    _M0L6_2atmpS1415 = _M0L3lenS104 - 1;
    if (
      _M0L6_2atmpS1415 < 0
      || _M0L6_2atmpS1415 >= Moonbit_array_length(_M0L4dataS105)
    ) {
      #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3042 = (int32_t)_M0L4dataS105[_M0L6_2atmpS1415];
    moonbit_decref(_M0L4dataS105);
    _M0L6_2atmpS1414 = _M0L6_2atmpS3042;
    _M0L2b1S120 = (int32_t)_M0L6_2atmpS1414;
    _M0L6_2atmpS1413 = _M0L2b0S119 & 252;
    _M0L6_2atmpS1412 = _M0L6_2atmpS1413 >> 2;
    if (
      _M0L6_2atmpS1412 < 0
      || _M0L6_2atmpS1412
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S121 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1412];
    _M0L6_2atmpS1411 = _M0L2b0S119 & 3;
    _M0L6_2atmpS1408 = _M0L6_2atmpS1411 << 4;
    _M0L6_2atmpS1410 = _M0L2b1S120 & 240;
    _M0L6_2atmpS1409 = _M0L6_2atmpS1410 >> 4;
    _M0L6_2atmpS1407 = _M0L6_2atmpS1408 | _M0L6_2atmpS1409;
    if (
      _M0L6_2atmpS1407 < 0
      || _M0L6_2atmpS1407
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S122 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1407];
    _M0L6_2atmpS1406 = _M0L2b1S120 & 15;
    _M0L6_2atmpS1405 = _M0L6_2atmpS1406 << 2;
    if (
      _M0L6_2atmpS1405 < 0
      || _M0L6_2atmpS1405
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x2S123 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1405];
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1402 = _M0MPC14byte4Byte8to__char(_M0L2x0S121);
    moonbit_incref(_M0L3bufS103);
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1402);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1403 = _M0MPC14byte4Byte8to__char(_M0L2x1S122);
    moonbit_incref(_M0L3bufS103);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1403);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1404 = _M0MPC14byte4Byte8to__char(_M0L2x2S123);
    moonbit_incref(_M0L3bufS103);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, _M0L6_2atmpS1404);
    moonbit_incref(_M0L3bufS103);
    #line 96 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS103, 61);
  } else {
    moonbit_decref(_M0L4dataS105);
  }
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS103);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS100,
  int32_t _M0L2chS102
) {
  int32_t _M0L3lenS1365;
  int32_t _M0L6_2atmpS1364;
  moonbit_bytes_t _M0L8_2afieldS3043;
  moonbit_bytes_t _M0L4dataS1368;
  int32_t _M0L3lenS1369;
  int32_t _M0L3incS101;
  int32_t _M0L3lenS1367;
  int32_t _M0L6_2atmpS1366;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1365 = _M0L4selfS100->$1;
  _M0L6_2atmpS1364 = _M0L3lenS1365 + 4;
  moonbit_incref(_M0L4selfS100);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS100, _M0L6_2atmpS1364);
  _M0L8_2afieldS3043 = _M0L4selfS100->$0;
  _M0L4dataS1368 = _M0L8_2afieldS3043;
  _M0L3lenS1369 = _M0L4selfS100->$1;
  moonbit_incref(_M0L4dataS1368);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS101
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1368, _M0L3lenS1369, _M0L2chS102);
  _M0L3lenS1367 = _M0L4selfS100->$1;
  _M0L6_2atmpS1366 = _M0L3lenS1367 + _M0L3incS101;
  _M0L4selfS100->$1 = _M0L6_2atmpS1366;
  moonbit_decref(_M0L4selfS100);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS95,
  int32_t _M0L8requiredS96
) {
  moonbit_bytes_t _M0L8_2afieldS3047;
  moonbit_bytes_t _M0L4dataS1363;
  int32_t _M0L6_2atmpS3046;
  int32_t _M0L12current__lenS94;
  int32_t _M0Lm13enough__spaceS97;
  int32_t _M0L6_2atmpS1361;
  int32_t _M0L6_2atmpS1362;
  moonbit_bytes_t _M0L9new__dataS99;
  moonbit_bytes_t _M0L8_2afieldS3045;
  moonbit_bytes_t _M0L4dataS1359;
  int32_t _M0L3lenS1360;
  moonbit_bytes_t _M0L6_2aoldS3044;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3047 = _M0L4selfS95->$0;
  _M0L4dataS1363 = _M0L8_2afieldS3047;
  _M0L6_2atmpS3046 = Moonbit_array_length(_M0L4dataS1363);
  _M0L12current__lenS94 = _M0L6_2atmpS3046;
  if (_M0L8requiredS96 <= _M0L12current__lenS94) {
    moonbit_decref(_M0L4selfS95);
    return 0;
  }
  _M0Lm13enough__spaceS97 = _M0L12current__lenS94;
  while (1) {
    int32_t _M0L6_2atmpS1357 = _M0Lm13enough__spaceS97;
    if (_M0L6_2atmpS1357 < _M0L8requiredS96) {
      int32_t _M0L6_2atmpS1358 = _M0Lm13enough__spaceS97;
      _M0Lm13enough__spaceS97 = _M0L6_2atmpS1358 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1361 = _M0Lm13enough__spaceS97;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1362 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS99
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1361, _M0L6_2atmpS1362);
  _M0L8_2afieldS3045 = _M0L4selfS95->$0;
  _M0L4dataS1359 = _M0L8_2afieldS3045;
  _M0L3lenS1360 = _M0L4selfS95->$1;
  moonbit_incref(_M0L4dataS1359);
  moonbit_incref(_M0L9new__dataS99);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS99, 0, _M0L4dataS1359, 0, _M0L3lenS1360);
  _M0L6_2aoldS3044 = _M0L4selfS95->$0;
  moonbit_decref(_M0L6_2aoldS3044);
  _M0L4selfS95->$0 = _M0L9new__dataS99;
  moonbit_decref(_M0L4selfS95);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS89,
  int32_t _M0L6offsetS90,
  int32_t _M0L5valueS88
) {
  uint32_t _M0L4codeS87;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS87 = _M0MPC14char4Char8to__uint(_M0L5valueS88);
  if (_M0L4codeS87 < 65536u) {
    uint32_t _M0L6_2atmpS1340 = _M0L4codeS87 & 255u;
    int32_t _M0L6_2atmpS1339;
    int32_t _M0L6_2atmpS1341;
    uint32_t _M0L6_2atmpS1343;
    int32_t _M0L6_2atmpS1342;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1339 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1340);
    if (
      _M0L6offsetS90 < 0
      || _M0L6offsetS90 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6offsetS90] = _M0L6_2atmpS1339;
    _M0L6_2atmpS1341 = _M0L6offsetS90 + 1;
    _M0L6_2atmpS1343 = _M0L4codeS87 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1342 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1343);
    if (
      _M0L6_2atmpS1341 < 0
      || _M0L6_2atmpS1341 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1341] = _M0L6_2atmpS1342;
    moonbit_decref(_M0L4selfS89);
    return 2;
  } else if (_M0L4codeS87 < 1114112u) {
    uint32_t _M0L2hiS91 = _M0L4codeS87 - 65536u;
    uint32_t _M0L6_2atmpS1356 = _M0L2hiS91 >> 10;
    uint32_t _M0L2loS92 = _M0L6_2atmpS1356 | 55296u;
    uint32_t _M0L6_2atmpS1355 = _M0L2hiS91 & 1023u;
    uint32_t _M0L2hiS93 = _M0L6_2atmpS1355 | 56320u;
    uint32_t _M0L6_2atmpS1345 = _M0L2loS92 & 255u;
    int32_t _M0L6_2atmpS1344;
    int32_t _M0L6_2atmpS1346;
    uint32_t _M0L6_2atmpS1348;
    int32_t _M0L6_2atmpS1347;
    int32_t _M0L6_2atmpS1349;
    uint32_t _M0L6_2atmpS1351;
    int32_t _M0L6_2atmpS1350;
    int32_t _M0L6_2atmpS1352;
    uint32_t _M0L6_2atmpS1354;
    int32_t _M0L6_2atmpS1353;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1344 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1345);
    if (
      _M0L6offsetS90 < 0
      || _M0L6offsetS90 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6offsetS90] = _M0L6_2atmpS1344;
    _M0L6_2atmpS1346 = _M0L6offsetS90 + 1;
    _M0L6_2atmpS1348 = _M0L2loS92 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1347 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1348);
    if (
      _M0L6_2atmpS1346 < 0
      || _M0L6_2atmpS1346 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1346] = _M0L6_2atmpS1347;
    _M0L6_2atmpS1349 = _M0L6offsetS90 + 2;
    _M0L6_2atmpS1351 = _M0L2hiS93 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1350 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1351);
    if (
      _M0L6_2atmpS1349 < 0
      || _M0L6_2atmpS1349 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1349] = _M0L6_2atmpS1350;
    _M0L6_2atmpS1352 = _M0L6offsetS90 + 3;
    _M0L6_2atmpS1354 = _M0L2hiS93 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1353 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1354);
    if (
      _M0L6_2atmpS1352 < 0
      || _M0L6_2atmpS1352 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1352] = _M0L6_2atmpS1353;
    moonbit_decref(_M0L4selfS89);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS89);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_98.data, (moonbit_string_t)moonbit_string_literal_99.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS86) {
  int32_t _M0L6_2atmpS1338;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1338 = *(int32_t*)&_M0L4selfS86;
  return _M0L6_2atmpS1338 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS85) {
  int32_t _M0L6_2atmpS1337;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1337 = _M0L4selfS85;
  return *(uint32_t*)&_M0L6_2atmpS1337;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS84
) {
  moonbit_bytes_t _M0L8_2afieldS3049;
  moonbit_bytes_t _M0L4dataS1336;
  moonbit_bytes_t _M0L6_2atmpS1333;
  int32_t _M0L8_2afieldS3048;
  int32_t _M0L3lenS1335;
  int64_t _M0L6_2atmpS1334;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3049 = _M0L4selfS84->$0;
  _M0L4dataS1336 = _M0L8_2afieldS3049;
  moonbit_incref(_M0L4dataS1336);
  _M0L6_2atmpS1333 = _M0L4dataS1336;
  _M0L8_2afieldS3048 = _M0L4selfS84->$1;
  moonbit_decref(_M0L4selfS84);
  _M0L3lenS1335 = _M0L8_2afieldS3048;
  _M0L6_2atmpS1334 = (int64_t)_M0L3lenS1335;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1333, 0, _M0L6_2atmpS1334);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS79,
  int32_t _M0L6offsetS83,
  int64_t _M0L6lengthS81
) {
  int32_t _M0L3lenS78;
  int32_t _M0L6lengthS80;
  int32_t _if__result_3311;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS78 = Moonbit_array_length(_M0L4selfS79);
  if (_M0L6lengthS81 == 4294967296ll) {
    _M0L6lengthS80 = _M0L3lenS78 - _M0L6offsetS83;
  } else {
    int64_t _M0L7_2aSomeS82 = _M0L6lengthS81;
    _M0L6lengthS80 = (int32_t)_M0L7_2aSomeS82;
  }
  if (_M0L6offsetS83 >= 0) {
    if (_M0L6lengthS80 >= 0) {
      int32_t _M0L6_2atmpS1332 = _M0L6offsetS83 + _M0L6lengthS80;
      _if__result_3311 = _M0L6_2atmpS1332 <= _M0L3lenS78;
    } else {
      _if__result_3311 = 0;
    }
  } else {
    _if__result_3311 = 0;
  }
  if (_if__result_3311) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS79, _M0L6offsetS83, _M0L6lengthS80);
  } else {
    moonbit_decref(_M0L4selfS79);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS76
) {
  int32_t _M0L7initialS75;
  moonbit_bytes_t _M0L4dataS77;
  struct _M0TPB13StringBuilder* _block_3312;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS76 < 1) {
    _M0L7initialS75 = 1;
  } else {
    _M0L7initialS75 = _M0L10size__hintS76;
  }
  _M0L4dataS77 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS75, 0);
  _block_3312
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3312)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3312->$0 = _M0L4dataS77;
  _block_3312->$1 = 0;
  return _block_3312;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS74) {
  int32_t _M0L6_2atmpS1331;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1331 = (int32_t)_M0L4selfS74;
  return _M0L6_2atmpS1331;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS59,
  int32_t _M0L11dst__offsetS60,
  moonbit_string_t* _M0L3srcS61,
  int32_t _M0L11src__offsetS62,
  int32_t _M0L3lenS63
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS59, _M0L11dst__offsetS60, _M0L3srcS61, _M0L11src__offsetS62, _M0L3lenS63);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS64,
  int32_t _M0L11dst__offsetS65,
  struct _M0TUsiE** _M0L3srcS66,
  int32_t _M0L11src__offsetS67,
  int32_t _M0L3lenS68
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS64, _M0L11dst__offsetS65, _M0L3srcS66, _M0L11src__offsetS67, _M0L3lenS68);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS69,
  int32_t _M0L11dst__offsetS70,
  void** _M0L3srcS71,
  int32_t _M0L11src__offsetS72,
  int32_t _M0L3lenS73
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS69, _M0L11dst__offsetS70, _M0L3srcS71, _M0L11src__offsetS72, _M0L3lenS73);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS23,
  int32_t _M0L11dst__offsetS25,
  moonbit_bytes_t _M0L3srcS24,
  int32_t _M0L11src__offsetS26,
  int32_t _M0L3lenS28
) {
  int32_t _if__result_3313;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_3313 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_3313 = 0;
  }
  if (_if__result_3313) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS1295 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS1297 = _M0L11src__offsetS26 + _M0L1iS27;
        int32_t _M0L6_2atmpS1296;
        int32_t _M0L6_2atmpS1298;
        if (
          _M0L6_2atmpS1297 < 0
          || _M0L6_2atmpS1297 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1296 = (int32_t)_M0L3srcS24[_M0L6_2atmpS1297];
        if (
          _M0L6_2atmpS1295 < 0
          || _M0L6_2atmpS1295 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS23[_M0L6_2atmpS1295] = _M0L6_2atmpS1296;
        _M0L6_2atmpS1298 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS1298;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1303 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS1303;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS1299 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS1301 = _M0L11src__offsetS26 + _M0L1iS30;
        int32_t _M0L6_2atmpS1300;
        int32_t _M0L6_2atmpS1302;
        if (
          _M0L6_2atmpS1301 < 0
          || _M0L6_2atmpS1301 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1300 = (int32_t)_M0L3srcS24[_M0L6_2atmpS1301];
        if (
          _M0L6_2atmpS1299 < 0
          || _M0L6_2atmpS1299 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS23[_M0L6_2atmpS1299] = _M0L6_2atmpS1300;
        _M0L6_2atmpS1302 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS1302;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS32,
  int32_t _M0L11dst__offsetS34,
  moonbit_string_t* _M0L3srcS33,
  int32_t _M0L11src__offsetS35,
  int32_t _M0L3lenS37
) {
  int32_t _if__result_3316;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_3316 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_3316 = 0;
  }
  if (_if__result_3316) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS1304 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS1306 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS3051;
        moonbit_string_t _M0L6_2atmpS1305;
        moonbit_string_t _M0L6_2aoldS3050;
        int32_t _M0L6_2atmpS1307;
        if (
          _M0L6_2atmpS1306 < 0
          || _M0L6_2atmpS1306 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3051 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1306];
        _M0L6_2atmpS1305 = _M0L6_2atmpS3051;
        if (
          _M0L6_2atmpS1304 < 0
          || _M0L6_2atmpS1304 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3050 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1304];
        moonbit_incref(_M0L6_2atmpS1305);
        moonbit_decref(_M0L6_2aoldS3050);
        _M0L3dstS32[_M0L6_2atmpS1304] = _M0L6_2atmpS1305;
        _M0L6_2atmpS1307 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS1307;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1312 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS1312;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS1308 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS1310 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS3053;
        moonbit_string_t _M0L6_2atmpS1309;
        moonbit_string_t _M0L6_2aoldS3052;
        int32_t _M0L6_2atmpS1311;
        if (
          _M0L6_2atmpS1310 < 0
          || _M0L6_2atmpS1310 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3053 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1310];
        _M0L6_2atmpS1309 = _M0L6_2atmpS3053;
        if (
          _M0L6_2atmpS1308 < 0
          || _M0L6_2atmpS1308 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3052 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1308];
        moonbit_incref(_M0L6_2atmpS1309);
        moonbit_decref(_M0L6_2aoldS3052);
        _M0L3dstS32[_M0L6_2atmpS1308] = _M0L6_2atmpS1309;
        _M0L6_2atmpS1311 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS1311;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS41,
  int32_t _M0L11dst__offsetS43,
  struct _M0TUsiE** _M0L3srcS42,
  int32_t _M0L11src__offsetS44,
  int32_t _M0L3lenS46
) {
  int32_t _if__result_3319;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_3319 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_3319 = 0;
  }
  if (_if__result_3319) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS1313 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS1315 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsiE* _M0L6_2atmpS3055;
        struct _M0TUsiE* _M0L6_2atmpS1314;
        struct _M0TUsiE* _M0L6_2aoldS3054;
        int32_t _M0L6_2atmpS1316;
        if (
          _M0L6_2atmpS1315 < 0
          || _M0L6_2atmpS1315 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3055 = (struct _M0TUsiE*)_M0L3srcS42[_M0L6_2atmpS1315];
        _M0L6_2atmpS1314 = _M0L6_2atmpS3055;
        if (
          _M0L6_2atmpS1313 < 0
          || _M0L6_2atmpS1313 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3054 = (struct _M0TUsiE*)_M0L3dstS41[_M0L6_2atmpS1313];
        if (_M0L6_2atmpS1314) {
          moonbit_incref(_M0L6_2atmpS1314);
        }
        if (_M0L6_2aoldS3054) {
          moonbit_decref(_M0L6_2aoldS3054);
        }
        _M0L3dstS41[_M0L6_2atmpS1313] = _M0L6_2atmpS1314;
        _M0L6_2atmpS1316 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS1316;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1321 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS1321;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS1317 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS1319 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsiE* _M0L6_2atmpS3057;
        struct _M0TUsiE* _M0L6_2atmpS1318;
        struct _M0TUsiE* _M0L6_2aoldS3056;
        int32_t _M0L6_2atmpS1320;
        if (
          _M0L6_2atmpS1319 < 0
          || _M0L6_2atmpS1319 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3057 = (struct _M0TUsiE*)_M0L3srcS42[_M0L6_2atmpS1319];
        _M0L6_2atmpS1318 = _M0L6_2atmpS3057;
        if (
          _M0L6_2atmpS1317 < 0
          || _M0L6_2atmpS1317 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3056 = (struct _M0TUsiE*)_M0L3dstS41[_M0L6_2atmpS1317];
        if (_M0L6_2atmpS1318) {
          moonbit_incref(_M0L6_2atmpS1318);
        }
        if (_M0L6_2aoldS3056) {
          moonbit_decref(_M0L6_2aoldS3056);
        }
        _M0L3dstS41[_M0L6_2atmpS1317] = _M0L6_2atmpS1318;
        _M0L6_2atmpS1320 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS1320;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS50,
  int32_t _M0L11dst__offsetS52,
  void** _M0L3srcS51,
  int32_t _M0L11src__offsetS53,
  int32_t _M0L3lenS55
) {
  int32_t _if__result_3322;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS50 == _M0L3srcS51) {
    _if__result_3322 = _M0L11dst__offsetS52 < _M0L11src__offsetS53;
  } else {
    _if__result_3322 = 0;
  }
  if (_if__result_3322) {
    int32_t _M0L1iS54 = 0;
    while (1) {
      if (_M0L1iS54 < _M0L3lenS55) {
        int32_t _M0L6_2atmpS1322 = _M0L11dst__offsetS52 + _M0L1iS54;
        int32_t _M0L6_2atmpS1324 = _M0L11src__offsetS53 + _M0L1iS54;
        void* _M0L6_2atmpS3059;
        void* _M0L6_2atmpS1323;
        void* _M0L6_2aoldS3058;
        int32_t _M0L6_2atmpS1325;
        if (
          _M0L6_2atmpS1324 < 0
          || _M0L6_2atmpS1324 >= Moonbit_array_length(_M0L3srcS51)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3059 = (void*)_M0L3srcS51[_M0L6_2atmpS1324];
        _M0L6_2atmpS1323 = _M0L6_2atmpS3059;
        if (
          _M0L6_2atmpS1322 < 0
          || _M0L6_2atmpS1322 >= Moonbit_array_length(_M0L3dstS50)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3058 = (void*)_M0L3dstS50[_M0L6_2atmpS1322];
        moonbit_incref(_M0L6_2atmpS1323);
        moonbit_decref(_M0L6_2aoldS3058);
        _M0L3dstS50[_M0L6_2atmpS1322] = _M0L6_2atmpS1323;
        _M0L6_2atmpS1325 = _M0L1iS54 + 1;
        _M0L1iS54 = _M0L6_2atmpS1325;
        continue;
      } else {
        moonbit_decref(_M0L3srcS51);
        moonbit_decref(_M0L3dstS50);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1330 = _M0L3lenS55 - 1;
    int32_t _M0L1iS57 = _M0L6_2atmpS1330;
    while (1) {
      if (_M0L1iS57 >= 0) {
        int32_t _M0L6_2atmpS1326 = _M0L11dst__offsetS52 + _M0L1iS57;
        int32_t _M0L6_2atmpS1328 = _M0L11src__offsetS53 + _M0L1iS57;
        void* _M0L6_2atmpS3061;
        void* _M0L6_2atmpS1327;
        void* _M0L6_2aoldS3060;
        int32_t _M0L6_2atmpS1329;
        if (
          _M0L6_2atmpS1328 < 0
          || _M0L6_2atmpS1328 >= Moonbit_array_length(_M0L3srcS51)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3061 = (void*)_M0L3srcS51[_M0L6_2atmpS1328];
        _M0L6_2atmpS1327 = _M0L6_2atmpS3061;
        if (
          _M0L6_2atmpS1326 < 0
          || _M0L6_2atmpS1326 >= Moonbit_array_length(_M0L3dstS50)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3060 = (void*)_M0L3dstS50[_M0L6_2atmpS1326];
        moonbit_incref(_M0L6_2atmpS1327);
        moonbit_decref(_M0L6_2aoldS3060);
        _M0L3dstS50[_M0L6_2atmpS1326] = _M0L6_2atmpS1327;
        _M0L6_2atmpS1329 = _M0L1iS57 - 1;
        _M0L1iS57 = _M0L6_2atmpS1329;
        continue;
      } else {
        moonbit_decref(_M0L3srcS51);
        moonbit_decref(_M0L3dstS50);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS17,
  moonbit_string_t _M0L3locS18
) {
  moonbit_string_t _M0L6_2atmpS1284;
  moonbit_string_t _M0L6_2atmpS3064;
  moonbit_string_t _M0L6_2atmpS1282;
  moonbit_string_t _M0L6_2atmpS1283;
  moonbit_string_t _M0L6_2atmpS3063;
  moonbit_string_t _M0L6_2atmpS1281;
  moonbit_string_t _M0L6_2atmpS3062;
  moonbit_string_t _M0L6_2atmpS1280;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1284 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3064
  = moonbit_add_string(_M0L6_2atmpS1284, (moonbit_string_t)moonbit_string_literal_100.data);
  moonbit_decref(_M0L6_2atmpS1284);
  _M0L6_2atmpS1282 = _M0L6_2atmpS3064;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1283
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3063 = moonbit_add_string(_M0L6_2atmpS1282, _M0L6_2atmpS1283);
  moonbit_decref(_M0L6_2atmpS1282);
  moonbit_decref(_M0L6_2atmpS1283);
  _M0L6_2atmpS1281 = _M0L6_2atmpS3063;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3062
  = moonbit_add_string(_M0L6_2atmpS1281, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS1281);
  _M0L6_2atmpS1280 = _M0L6_2atmpS3062;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1280);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS19,
  moonbit_string_t _M0L3locS20
) {
  moonbit_string_t _M0L6_2atmpS1289;
  moonbit_string_t _M0L6_2atmpS3067;
  moonbit_string_t _M0L6_2atmpS1287;
  moonbit_string_t _M0L6_2atmpS1288;
  moonbit_string_t _M0L6_2atmpS3066;
  moonbit_string_t _M0L6_2atmpS1286;
  moonbit_string_t _M0L6_2atmpS3065;
  moonbit_string_t _M0L6_2atmpS1285;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1289 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3067
  = moonbit_add_string(_M0L6_2atmpS1289, (moonbit_string_t)moonbit_string_literal_100.data);
  moonbit_decref(_M0L6_2atmpS1289);
  _M0L6_2atmpS1287 = _M0L6_2atmpS3067;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1288
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3066 = moonbit_add_string(_M0L6_2atmpS1287, _M0L6_2atmpS1288);
  moonbit_decref(_M0L6_2atmpS1287);
  moonbit_decref(_M0L6_2atmpS1288);
  _M0L6_2atmpS1286 = _M0L6_2atmpS3066;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3065
  = moonbit_add_string(_M0L6_2atmpS1286, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS1286);
  _M0L6_2atmpS1285 = _M0L6_2atmpS3065;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1285);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1294;
  moonbit_string_t _M0L6_2atmpS3070;
  moonbit_string_t _M0L6_2atmpS1292;
  moonbit_string_t _M0L6_2atmpS1293;
  moonbit_string_t _M0L6_2atmpS3069;
  moonbit_string_t _M0L6_2atmpS1291;
  moonbit_string_t _M0L6_2atmpS3068;
  moonbit_string_t _M0L6_2atmpS1290;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1294 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3070
  = moonbit_add_string(_M0L6_2atmpS1294, (moonbit_string_t)moonbit_string_literal_100.data);
  moonbit_decref(_M0L6_2atmpS1294);
  _M0L6_2atmpS1292 = _M0L6_2atmpS3070;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1293
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3069 = moonbit_add_string(_M0L6_2atmpS1292, _M0L6_2atmpS1293);
  moonbit_decref(_M0L6_2atmpS1292);
  moonbit_decref(_M0L6_2atmpS1293);
  _M0L6_2atmpS1291 = _M0L6_2atmpS3069;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3068
  = moonbit_add_string(_M0L6_2atmpS1291, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS1291);
  _M0L6_2atmpS1290 = _M0L6_2atmpS3068;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1290);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5valueS16
) {
  uint32_t _M0L3accS1279;
  uint32_t _M0L6_2atmpS1278;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1279 = _M0L4selfS15->$0;
  _M0L6_2atmpS1278 = _M0L3accS1279 + 4u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1278;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS15, _M0L5valueS16);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS13,
  uint32_t _M0L5inputS14
) {
  uint32_t _M0L3accS1276;
  uint32_t _M0L6_2atmpS1277;
  uint32_t _M0L6_2atmpS1275;
  uint32_t _M0L6_2atmpS1274;
  uint32_t _M0L6_2atmpS1273;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1276 = _M0L4selfS13->$0;
  _M0L6_2atmpS1277 = _M0L5inputS14 * 3266489917u;
  _M0L6_2atmpS1275 = _M0L3accS1276 + _M0L6_2atmpS1277;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1274 = _M0FPB4rotl(_M0L6_2atmpS1275, 17);
  _M0L6_2atmpS1273 = _M0L6_2atmpS1274 * 668265263u;
  _M0L4selfS13->$0 = _M0L6_2atmpS1273;
  moonbit_decref(_M0L4selfS13);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS11, int32_t _M0L1rS12) {
  uint32_t _M0L6_2atmpS1270;
  int32_t _M0L6_2atmpS1272;
  uint32_t _M0L6_2atmpS1271;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1270 = _M0L1xS11 << (_M0L1rS12 & 31);
  _M0L6_2atmpS1272 = 32 - _M0L1rS12;
  _M0L6_2atmpS1271 = _M0L1xS11 >> (_M0L6_2atmpS1272 & 31);
  return _M0L6_2atmpS1270 | _M0L6_2atmpS1271;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S7,
  struct _M0TPB6Logger _M0L10_2ax__4934S10
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS8;
  moonbit_string_t _M0L8_2afieldS3071;
  int32_t _M0L6_2acntS3164;
  moonbit_string_t _M0L15_2a_2aarg__4935S9;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS8
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S7;
  _M0L8_2afieldS3071 = _M0L10_2aFailureS8->$0;
  _M0L6_2acntS3164 = Moonbit_object_header(_M0L10_2aFailureS8)->rc;
  if (_M0L6_2acntS3164 > 1) {
    int32_t _M0L11_2anew__cntS3165 = _M0L6_2acntS3164 - 1;
    Moonbit_object_header(_M0L10_2aFailureS8)->rc = _M0L11_2anew__cntS3165;
    moonbit_incref(_M0L8_2afieldS3071);
  } else if (_M0L6_2acntS3164 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS8);
  }
  _M0L15_2a_2aarg__4935S9 = _M0L8_2afieldS3071;
  if (_M0L10_2ax__4934S10.$1) {
    moonbit_incref(_M0L10_2ax__4934S10.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S10.$0->$method_0(_M0L10_2ax__4934S10.$1, (moonbit_string_t)moonbit_string_literal_101.data);
  if (_M0L10_2ax__4934S10.$1) {
    moonbit_incref(_M0L10_2ax__4934S10.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S10, _M0L15_2a_2aarg__4935S9);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S10.$0->$method_0(_M0L10_2ax__4934S10.$1, (moonbit_string_t)moonbit_string_literal_102.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS6) {
  void* _block_3325;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3325 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3325)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3325)->$0 = _M0L4selfS6;
  return _block_3325;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1207) {
  switch (Moonbit_object_tag(_M0L4_2aeS1207)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1207);
      return (moonbit_string_t)moonbit_string_literal_103.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1207);
      return (moonbit_string_t)moonbit_string_literal_104.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1207);
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS1207);
      return (moonbit_string_t)moonbit_string_literal_105.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1207);
      return (moonbit_string_t)moonbit_string_literal_106.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1243
) {
  moonbit_string_t _M0L7_2aselfS1242 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1243;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS1242);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1241,
  struct _M0TPB6Logger _M0L8_2aparamS1240
) {
  moonbit_string_t _M0L7_2aselfS1239 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1241;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS1239, _M0L8_2aparamS1240);
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGbE(
  void* _M0L11_2aobj__ptrS1237
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1238 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1237;
  int32_t _M0L8_2afieldS3072 = _M0L14_2aboxed__selfS1238->$0;
  int32_t _M0L7_2aselfS1236;
  moonbit_decref(_M0L14_2aboxed__selfS1238);
  _M0L7_2aselfS1236 = _M0L8_2afieldS3072;
  return _M0IP016_24default__implPB4Show10to__stringGbE(_M0L7_2aselfS1236);
}

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1234,
  struct _M0TPB6Logger _M0L8_2aparamS1233
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1235 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1234;
  int32_t _M0L8_2afieldS3073 = _M0L14_2aboxed__selfS1235->$0;
  int32_t _M0L7_2aselfS1232;
  moonbit_decref(_M0L14_2aboxed__selfS1235);
  _M0L7_2aselfS1232 = _M0L8_2afieldS3073;
  _M0IPC14bool4BoolPB4Show6output(_M0L7_2aselfS1232, _M0L8_2aparamS1233);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1231,
  int32_t _M0L8_2aparamS1230
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1229 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1231;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1229, _M0L8_2aparamS1230);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1228,
  struct _M0TPC16string10StringView _M0L8_2aparamS1227
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1226 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1228;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1226, _M0L8_2aparamS1227);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1225,
  moonbit_string_t _M0L8_2aparamS1222,
  int32_t _M0L8_2aparamS1223,
  int32_t _M0L8_2aparamS1224
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1221 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1225;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1221, _M0L8_2aparamS1222, _M0L8_2aparamS1223, _M0L8_2aparamS1224);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1220,
  moonbit_string_t _M0L8_2aparamS1219
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1218 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1220;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1218, _M0L8_2aparamS1219);
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGiE(
  void* _M0L11_2aobj__ptrS1216
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1217 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1216;
  int32_t _M0L8_2afieldS3074 = _M0L14_2aboxed__selfS1217->$0;
  int32_t _M0L7_2aselfS1215;
  moonbit_decref(_M0L14_2aboxed__selfS1217);
  _M0L7_2aselfS1215 = _M0L8_2afieldS3074;
  return _M0IP016_24default__implPB4Show10to__stringGiE(_M0L7_2aselfS1215);
}

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1213,
  struct _M0TPB6Logger _M0L8_2aparamS1212
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1214 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1213;
  int32_t _M0L8_2afieldS3075 = _M0L14_2aboxed__selfS1214->$0;
  int32_t _M0L7_2aselfS1211;
  moonbit_decref(_M0L14_2aboxed__selfS1214);
  _M0L7_2aselfS1211 = _M0L8_2afieldS3075;
  _M0IPC13int3IntPB4Show6output(_M0L7_2aselfS1211, _M0L8_2aparamS1212);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1269 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1268;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1255;
  moonbit_string_t* _M0L6_2atmpS1267;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1266;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1256;
  moonbit_string_t* _M0L6_2atmpS1265;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1264;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1257;
  moonbit_string_t* _M0L6_2atmpS1263;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1262;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1258;
  moonbit_string_t* _M0L6_2atmpS1261;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1260;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1259;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1134;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1254;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1253;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1252;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1251;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1133;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1250;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1249;
  _M0L6_2atmpS1269[0] = (moonbit_string_t)moonbit_string_literal_107.data;
  moonbit_incref(_M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1268
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1268)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1268->$0
  = _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1268->$1 = _M0L6_2atmpS1269;
  _M0L8_2atupleS1255
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1255)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1255->$0 = 0;
  _M0L8_2atupleS1255->$1 = _M0L8_2atupleS1268;
  _M0L6_2atmpS1267 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1267[0] = (moonbit_string_t)moonbit_string_literal_108.data;
  moonbit_incref(_M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1266
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1266)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1266->$0
  = _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1266->$1 = _M0L6_2atmpS1267;
  _M0L8_2atupleS1256
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1256)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1256->$0 = 1;
  _M0L8_2atupleS1256->$1 = _M0L8_2atupleS1266;
  _M0L6_2atmpS1265 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1265[0] = (moonbit_string_t)moonbit_string_literal_109.data;
  moonbit_incref(_M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1264
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1264)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1264->$0
  = _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1264->$1 = _M0L6_2atmpS1265;
  _M0L8_2atupleS1257
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1257)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1257->$0 = 2;
  _M0L8_2atupleS1257->$1 = _M0L8_2atupleS1264;
  _M0L6_2atmpS1263 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1263[0] = (moonbit_string_t)moonbit_string_literal_110.data;
  moonbit_incref(_M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__3_2eclo);
  _M0L8_2atupleS1262
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1262)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1262->$0
  = _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__3_2eclo;
  _M0L8_2atupleS1262->$1 = _M0L6_2atmpS1263;
  _M0L8_2atupleS1258
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1258)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1258->$0 = 3;
  _M0L8_2atupleS1258->$1 = _M0L8_2atupleS1262;
  _M0L6_2atmpS1261 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1261[0] = (moonbit_string_t)moonbit_string_literal_111.data;
  moonbit_incref(_M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__4_2eclo);
  _M0L8_2atupleS1260
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1260)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1260->$0
  = _M0FP38clawteam8clawteam6parser53____test__7061727365725f7762746573742e6d6274__4_2eclo;
  _M0L8_2atupleS1260->$1 = _M0L6_2atmpS1261;
  _M0L8_2atupleS1259
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1259)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1259->$0 = 4;
  _M0L8_2atupleS1259->$1 = _M0L8_2atupleS1260;
  _M0L7_2abindS1134
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(5);
  _M0L7_2abindS1134[0] = _M0L8_2atupleS1255;
  _M0L7_2abindS1134[1] = _M0L8_2atupleS1256;
  _M0L7_2abindS1134[2] = _M0L8_2atupleS1257;
  _M0L7_2abindS1134[3] = _M0L8_2atupleS1258;
  _M0L7_2abindS1134[4] = _M0L8_2atupleS1259;
  _M0L6_2atmpS1254 = _M0L7_2abindS1134;
  _M0L6_2atmpS1253
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 5, _M0L6_2atmpS1254
  };
  #line 398 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1252
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1253);
  _M0L8_2atupleS1251
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1251)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1251->$0 = (moonbit_string_t)moonbit_string_literal_112.data;
  _M0L8_2atupleS1251->$1 = _M0L6_2atmpS1252;
  _M0L7_2abindS1133
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1133[0] = _M0L8_2atupleS1251;
  _M0L6_2atmpS1250 = _M0L7_2abindS1133;
  _M0L6_2atmpS1249
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS1250
  };
  #line 397 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0FP38clawteam8clawteam6parser48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1249);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1248;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1201;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1202;
  int32_t _M0L7_2abindS1203;
  int32_t _M0L2__S1204;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1248
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1201
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1201)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1201->$0 = _M0L6_2atmpS1248;
  _M0L12async__testsS1201->$1 = 0;
  #line 442 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1202
  = _M0FP38clawteam8clawteam6parser52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1203 = _M0L7_2abindS1202->$1;
  _M0L2__S1204 = 0;
  while (1) {
    if (_M0L2__S1204 < _M0L7_2abindS1203) {
      struct _M0TUsiE** _M0L8_2afieldS3079 = _M0L7_2abindS1202->$0;
      struct _M0TUsiE** _M0L3bufS1247 = _M0L8_2afieldS3079;
      struct _M0TUsiE* _M0L6_2atmpS3078 =
        (struct _M0TUsiE*)_M0L3bufS1247[_M0L2__S1204];
      struct _M0TUsiE* _M0L3argS1205 = _M0L6_2atmpS3078;
      moonbit_string_t _M0L8_2afieldS3077 = _M0L3argS1205->$0;
      moonbit_string_t _M0L6_2atmpS1244 = _M0L8_2afieldS3077;
      int32_t _M0L8_2afieldS3076 = _M0L3argS1205->$1;
      int32_t _M0L6_2atmpS1245 = _M0L8_2afieldS3076;
      int32_t _M0L6_2atmpS1246;
      moonbit_incref(_M0L6_2atmpS1244);
      moonbit_incref(_M0L12async__testsS1201);
      #line 443 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
      _M0FP38clawteam8clawteam6parser44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1201, _M0L6_2atmpS1244, _M0L6_2atmpS1245);
      _M0L6_2atmpS1246 = _M0L2__S1204 + 1;
      _M0L2__S1204 = _M0L6_2atmpS1246;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1202);
    }
    break;
  }
  #line 445 "E:\\moonbit\\clawteam\\parser\\__generated_driver_for_whitebox_test.mbt"
  _M0IP016_24default__implP38clawteam8clawteam6parser28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam6parser34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1201);
  return 0;
}