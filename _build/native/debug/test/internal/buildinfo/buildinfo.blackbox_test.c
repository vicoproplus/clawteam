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
struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPC15error5ErrorE3Err;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0DTPC16result6ResultGUiRPC16string10StringViewbERPC17strconv12StrConvErrorE2Ok;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6Logger;

struct _M0TP48clawteam8clawteam8internal9buildinfo5Build;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0TWEOs;

struct _M0R38String_3a_3aiter_2eanon__u2159__l247__;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal25buildinfo__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0DTPC15error5Error128clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC17strconv12StrConvErrorE2Ok;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPB7FailureE3Err;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC16result6ResultGlRPC17strconv12StrConvErrorE2Ok;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC16result6ResultGlRPC17strconv12StrConvErrorE3Err;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__;

struct _M0TUbRPC16string10StringViewE;

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal9buildinfo7Version;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal25buildinfo__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0Y4Bool;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0TP48clawteam8clawteam8internal9buildinfo7Version;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0DTPC16result6ResultGiRPC17strconv12StrConvErrorE2Ok;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGiRPC17strconv12StrConvErrorE3Err;

struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError;

struct _M0DTPB4Json6Object;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPB7FailureE2Ok;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__;

struct _M0KTPB6ToJsonS4Bool;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TUiRPC16string10StringViewbE;

struct _M0DTPC16result6ResultGUiRPC16string10StringViewbERPC17strconv12StrConvErrorE3Err;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0DTPC16result6ResultGuRPC17strconv12StrConvErrorE3Err;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC14json10WriteFrame6Object {
  int32_t $1;
  struct _M0TWEOUsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
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

struct _M0DTPC16result6ResultGUiRPC16string10StringViewbERPC17strconv12StrConvErrorE2Ok {
  struct _M0TUiRPC16string10StringViewbE* $0;
  
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

struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TP48clawteam8clawteam8internal9buildinfo5Build {
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TPB6ToJson {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0R38String_3a_3aiter_2eanon__u2159__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal25buildinfo__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error128clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPC15error5ErrorE2Ok {
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* $0;
  
};

struct _M0DTPC16result6ResultGuRPC17strconv12StrConvErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPB7FailureE3Err {
  void* $0;
  
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

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGlRPC17strconv12StrConvErrorE2Ok {
  int64_t $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGlRPC17strconv12StrConvErrorE3Err {
  void* $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0TUbRPC16string10StringViewE {
  int32_t $0;
  int32_t $1_1;
  int32_t $1_2;
  moonbit_string_t $1_0;
  
};

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal9buildinfo7Version {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal25buildinfo__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
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

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0Y4Bool {
  int32_t $0;
  
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

struct _M0TP48clawteam8clawteam8internal9buildinfo7Version {
  int32_t $0;
  int32_t $1;
  int32_t $2;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* $3;
  
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

struct _M0DTPC16result6ResultGiRPC17strconv12StrConvErrorE2Ok {
  int32_t $0;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0DTPC16result6ResultGiRPC17strconv12StrConvErrorE3Err {
  void* $0;
  
};

struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** $0;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError {
  moonbit_string_t $0;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal9buildinfo7VersionRPB7FailureE2Ok {
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0KTPB6ToJsonS4Bool {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
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

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0TUiRPC16string10StringViewbE {
  int32_t $0;
  int32_t $1_1;
  int32_t $1_2;
  int32_t $2;
  moonbit_string_t $1_0;
  
};

struct _M0DTPC16result6ResultGUiRPC16string10StringViewbERPC17strconv12StrConvErrorE3Err {
  void* $0;
  
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

struct _M0DTPC16result6ResultGuRPC17strconv12StrConvErrorE3Err {
  void* $0;
  
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

struct moonbit_result_3 {
  int tag;
  union { struct _M0TUiRPC16string10StringViewbE* ok; void* err;  } data;
  
};

struct moonbit_result_2 {
  int tag;
  union { int64_t ok; void* err;  } data;
  
};

struct moonbit_result_1 {
  int tag;
  union {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* ok;
    void* err;
    
  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1449(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1440(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testC3582l430(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testC3578l431(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1372(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1367(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1354(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal25buildinfo__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45____test__76657273696f6e5f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45____test__76657273696f6e5f746573742e6d6274__0(
  
);

int32_t _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB2Eq5equal(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*
);

int32_t _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB7Compare7compare(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*
);

struct moonbit_result_1 _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse(
  moonbit_string_t
);

void* _M0IP48clawteam8clawteam8internal9buildinfo5BuildPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build*
);

void* _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*
);

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

int32_t _M0IPC17strconv12StrConvErrorPB4Show6output(
  void*,
  struct _M0TPB6Logger
);

struct moonbit_result_0 _M0FPC17strconv18parse__int_2einner(
  struct _M0TPC16string10StringView,
  int32_t
);

struct moonbit_result_2 _M0FPC17strconv20parse__int64_2einner(
  struct _M0TPC16string10StringView,
  int32_t
);

int64_t _M0FPC17strconv19overflow__threshold(int32_t, int32_t);

struct moonbit_result_0 _M0FPC17strconv11syntax__errGiE();

struct moonbit_result_2 _M0FPC17strconv11syntax__errGlE();

struct moonbit_result_0 _M0FPC17strconv10range__errGuE();

struct moonbit_result_2 _M0FPC17strconv10range__errGlE();

struct moonbit_result_3 _M0FPC17strconv25check__and__consume__base(
  struct _M0TPC16string10StringView,
  int32_t
);

struct moonbit_result_3 _M0FPC17strconv9base__errGUiRPC16string10StringViewbEE(
  
);

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

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t);

void* _M0IPC14bool4BoolPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json7boolean(int32_t);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2428l591(
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

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2178l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2159l247(struct _M0TWEOc*);

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

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t,
  int32_t,
  int32_t,
  int64_t
);

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t,
  int32_t,
  int32_t,
  int32_t
);

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t,
  int32_t,
  int32_t,
  int64_t
);

int32_t _M0MPC16string6String24char__length__eq_2einner(
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

int32_t _M0IPC16string10StringViewPB2Eq5equal(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
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

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView,
  int32_t,
  int64_t
);

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs*);

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE*
);

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc*);

struct moonbit_result_1 _M0FPB4failGRP48clawteam8clawteam8internal9buildinfo7VersionE(
  moonbit_string_t,
  moonbit_string_t
);

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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPC17strconv12StrConvErrorE(
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

int32_t _M0IP016_24default__implPB7Compare6op__gtGRP48clawteam8clawteam8internal9buildinfo7VersionE(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*
);

int32_t _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal9buildinfo7VersionE(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*
);

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0IP016_24default__implPB2Eq10not__equalGRPC16string10StringViewE(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
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

int32_t _M0MPC16string6String16unsafe__char__at(moonbit_string_t, int32_t);

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

int64_t _M0FPB5abortGOiE(moonbit_string_t, moonbit_string_t);

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

int64_t _M0FPC15abort5abortGOiE(moonbit_string_t);

moonbit_string_t _M0FP15Error10to__string(void*);

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

void* _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

void* _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    73, 110, 118, 97, 108, 105, 100, 32, 118, 101, 114, 115, 105, 111, 
    110, 32, 115, 116, 114, 105, 110, 103, 32, 102, 111, 114, 109, 97, 
    116, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_3 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    86, 101, 114, 115, 105, 111, 110, 58, 58, 99, 111, 109, 112, 97, 
    114, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    109, 105, 110, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 53, 58, 
    51, 45, 50, 53, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    49, 46, 48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    109, 97, 106, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 54, 58, 
    51, 45, 50, 54, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 55, 58, 
    51, 51, 45, 50, 55, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 54, 58, 
    51, 51, 45, 50, 54, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 53, 58, 
    51, 51, 45, 50, 53, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 
    49, 51, 58, 53, 45, 49, 49, 51, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    49, 46, 50, 46, 51, 43, 52, 53, 46, 97, 98, 99, 100, 49, 50, 51, 
    52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 52, 58, 
    51, 45, 50, 52, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[79]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 78), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 50, 
    56, 45, 52, 58, 54, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    86, 101, 114, 115, 105, 111, 110, 58, 58, 112, 97, 114, 115, 101, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[79]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 78), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 58, 50, 
    56, 45, 49, 49, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 50, 58, 
    51, 52, 45, 50, 50, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    49, 46, 48, 46, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 56, 58, 
    51, 45, 50, 56, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 53, 58, 
    49, 54, 45, 50, 53, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 50, 54, 58, 57, 45, 
    52, 50, 54, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[78]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 77), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 58, 51, 
    45, 49, 49, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 52, 58, 
    49, 54, 45, 50, 52, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 51, 58, 
    51, 45, 50, 51, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 50, 58, 
    49, 54, 45, 50, 50, 58, 50, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    49, 46, 48, 46, 48, 43, 49, 46, 97, 98, 99, 100, 101, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    49, 46, 49, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 54, 58, 
    49, 54, 45, 50, 54, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_118 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_75 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    49, 46, 48, 46, 48, 43, 50, 46, 49, 50, 51, 52, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_66 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    98, 117, 105, 108, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    97, 98, 99, 100, 49, 50, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    44, 32, 34, 109, 101, 115, 115, 97, 103, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    50, 46, 48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    118, 101, 114, 115, 105, 111, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[78]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 77), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 51, 
    45, 52, 58, 54, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_89 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    118, 101, 114, 115, 105, 111, 110, 95, 116, 101, 115, 116, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    118, 101, 114, 115, 105, 111, 110, 95, 115, 116, 114, 105, 110, 103, 
    46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    48, 46, 48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[119]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 118), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 117, 
    105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 98, 
    111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 
    112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 51, 58, 
    49, 54, 45, 50, 51, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 50, 58, 
    51, 45, 50, 50, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 56, 58, 
    49, 54, 45, 50, 56, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[117]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 116), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 117, 
    105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 98, 
    111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 
    114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 121, 110, 116, 97, 120, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    118, 97, 108, 117, 101, 32, 111, 117, 116, 32, 111, 102, 32, 114, 
    97, 110, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[79]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 78), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 58, 49, 
    54, 45, 54, 58, 49, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 58, 118, 101, 114, 115, 105, 
    111, 110, 46, 109, 98, 116, 58, 54, 53, 58, 49, 48, 45, 54, 53, 58, 
    53, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_108 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    99, 111, 109, 109, 105, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[79]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 78), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 49, 
    54, 45, 52, 58, 49, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 56, 58, 
    51, 51, 45, 50, 56, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 55, 58, 
    51, 45, 50, 55, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 51, 58, 
    51, 51, 45, 50, 51, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    112, 97, 116, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 52, 58, 
    51, 51, 45, 50, 52, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    110, 117, 109, 98, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 117, 105, 108, 
    100, 105, 110, 102, 111, 34, 44, 32, 34, 102, 105, 108, 101, 110, 
    97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 98, 
    117, 105, 108, 100, 105, 110, 102, 111, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 58, 118, 101, 114, 115, 105, 
    111, 110, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 55, 58, 
    49, 54, 45, 50, 55, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    105, 110, 118, 97, 108, 105, 100, 32, 98, 97, 115, 101, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct moonbit_object const moonbit_constant_constructor_1 =
  { -1, Moonbit_make_regular_object_header(2, 0, 1)};

struct moonbit_object const moonbit_constant_constructor_2 =
  { -1, Moonbit_make_regular_object_header(2, 0, 2)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1449$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1449
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test51____test__76657273696f6e5f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test51____test__76657273696f6e5f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__0_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0127clawteam_2fclawteam_2finternal_2fbuildinfo_2fVersion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0127clawteam_2fclawteam_2finternal_2fbuildinfo_2fVersion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0127clawteam_2fclawteam_2finternal_2fbuildinfo_2fVersion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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

moonbit_string_t _M0FPC17strconv14base__err__str =
  (moonbit_string_t)moonbit_string_literal_0.data;

moonbit_string_t _M0FPC17strconv15range__err__str =
  (moonbit_string_t)moonbit_string_literal_1.data;

moonbit_string_t _M0FPC17strconv16syntax__err__str =
  (moonbit_string_t)moonbit_string_literal_2.data;

moonbit_string_t _M0FPC17strconv20parse__int64_2einnerN7_2abindS543 =
  (moonbit_string_t)moonbit_string_literal_3.data;

struct { int32_t rc; uint32_t meta; struct _M0TPB17FloatingDecimal64 data; 
} _M0FPB31ryu__to__string_2erecord_2f1027$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1027 =
  &_M0FPB31ryu__to__string_2erecord_2f1027$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3614
) {
  return _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45____test__76657273696f6e5f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test55____test__76657273696f6e5f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3613
) {
  return _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45____test__76657273696f6e5f746573742e6d6274__1();
}

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1470,
  moonbit_string_t _M0L8filenameS1445,
  int32_t _M0L5indexS1448
) {
  struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440* _closure_4098;
  struct _M0TWssbEu* _M0L14handle__resultS1440;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1449;
  void* _M0L11_2atry__errS1464;
  struct moonbit_result_0 _tmp_4100;
  int32_t _handle__error__result_4101;
  int32_t _M0L6_2atmpS3601;
  void* _M0L3errS1465;
  moonbit_string_t _M0L4nameS1467;
  struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1468;
  moonbit_string_t _M0L8_2afieldS3615;
  int32_t _M0L6_2acntS3991;
  moonbit_string_t _M0L7_2anameS1469;
  #line 529 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1445);
  _closure_4098
  = (struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440*)moonbit_malloc(sizeof(struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440));
  Moonbit_object_header(_closure_4098)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440, $1) >> 2, 1, 0);
  _closure_4098->code
  = &_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1440;
  _closure_4098->$0 = _M0L5indexS1448;
  _closure_4098->$1 = _M0L8filenameS1445;
  _M0L14handle__resultS1440 = (struct _M0TWssbEu*)_closure_4098;
  _M0L17error__to__stringS1449
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1449$closure.data;
  moonbit_incref(_M0L12async__testsS1470);
  moonbit_incref(_M0L17error__to__stringS1449);
  moonbit_incref(_M0L8filenameS1445);
  moonbit_incref(_M0L14handle__resultS1440);
  #line 563 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4100
  = _M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1470, _M0L8filenameS1445, _M0L5indexS1448, _M0L14handle__resultS1440, _M0L17error__to__stringS1449);
  if (_tmp_4100.tag) {
    int32_t const _M0L5_2aokS3610 = _tmp_4100.data.ok;
    _handle__error__result_4101 = _M0L5_2aokS3610;
  } else {
    void* const _M0L6_2aerrS3611 = _tmp_4100.data.err;
    moonbit_decref(_M0L12async__testsS1470);
    moonbit_decref(_M0L17error__to__stringS1449);
    moonbit_decref(_M0L8filenameS1445);
    _M0L11_2atry__errS1464 = _M0L6_2aerrS3611;
    goto join_1463;
  }
  if (_handle__error__result_4101) {
    moonbit_decref(_M0L12async__testsS1470);
    moonbit_decref(_M0L17error__to__stringS1449);
    moonbit_decref(_M0L8filenameS1445);
    _M0L6_2atmpS3601 = 1;
  } else {
    struct moonbit_result_0 _tmp_4102;
    int32_t _handle__error__result_4103;
    moonbit_incref(_M0L12async__testsS1470);
    moonbit_incref(_M0L17error__to__stringS1449);
    moonbit_incref(_M0L8filenameS1445);
    moonbit_incref(_M0L14handle__resultS1440);
    #line 566 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    _tmp_4102
    = _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1470, _M0L8filenameS1445, _M0L5indexS1448, _M0L14handle__resultS1440, _M0L17error__to__stringS1449);
    if (_tmp_4102.tag) {
      int32_t const _M0L5_2aokS3608 = _tmp_4102.data.ok;
      _handle__error__result_4103 = _M0L5_2aokS3608;
    } else {
      void* const _M0L6_2aerrS3609 = _tmp_4102.data.err;
      moonbit_decref(_M0L12async__testsS1470);
      moonbit_decref(_M0L17error__to__stringS1449);
      moonbit_decref(_M0L8filenameS1445);
      _M0L11_2atry__errS1464 = _M0L6_2aerrS3609;
      goto join_1463;
    }
    if (_handle__error__result_4103) {
      moonbit_decref(_M0L12async__testsS1470);
      moonbit_decref(_M0L17error__to__stringS1449);
      moonbit_decref(_M0L8filenameS1445);
      _M0L6_2atmpS3601 = 1;
    } else {
      struct moonbit_result_0 _tmp_4104;
      int32_t _handle__error__result_4105;
      moonbit_incref(_M0L12async__testsS1470);
      moonbit_incref(_M0L17error__to__stringS1449);
      moonbit_incref(_M0L8filenameS1445);
      moonbit_incref(_M0L14handle__resultS1440);
      #line 569 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _tmp_4104
      = _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1470, _M0L8filenameS1445, _M0L5indexS1448, _M0L14handle__resultS1440, _M0L17error__to__stringS1449);
      if (_tmp_4104.tag) {
        int32_t const _M0L5_2aokS3606 = _tmp_4104.data.ok;
        _handle__error__result_4105 = _M0L5_2aokS3606;
      } else {
        void* const _M0L6_2aerrS3607 = _tmp_4104.data.err;
        moonbit_decref(_M0L12async__testsS1470);
        moonbit_decref(_M0L17error__to__stringS1449);
        moonbit_decref(_M0L8filenameS1445);
        _M0L11_2atry__errS1464 = _M0L6_2aerrS3607;
        goto join_1463;
      }
      if (_handle__error__result_4105) {
        moonbit_decref(_M0L12async__testsS1470);
        moonbit_decref(_M0L17error__to__stringS1449);
        moonbit_decref(_M0L8filenameS1445);
        _M0L6_2atmpS3601 = 1;
      } else {
        struct moonbit_result_0 _tmp_4106;
        int32_t _handle__error__result_4107;
        moonbit_incref(_M0L12async__testsS1470);
        moonbit_incref(_M0L17error__to__stringS1449);
        moonbit_incref(_M0L8filenameS1445);
        moonbit_incref(_M0L14handle__resultS1440);
        #line 572 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        _tmp_4106
        = _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1470, _M0L8filenameS1445, _M0L5indexS1448, _M0L14handle__resultS1440, _M0L17error__to__stringS1449);
        if (_tmp_4106.tag) {
          int32_t const _M0L5_2aokS3604 = _tmp_4106.data.ok;
          _handle__error__result_4107 = _M0L5_2aokS3604;
        } else {
          void* const _M0L6_2aerrS3605 = _tmp_4106.data.err;
          moonbit_decref(_M0L12async__testsS1470);
          moonbit_decref(_M0L17error__to__stringS1449);
          moonbit_decref(_M0L8filenameS1445);
          _M0L11_2atry__errS1464 = _M0L6_2aerrS3605;
          goto join_1463;
        }
        if (_handle__error__result_4107) {
          moonbit_decref(_M0L12async__testsS1470);
          moonbit_decref(_M0L17error__to__stringS1449);
          moonbit_decref(_M0L8filenameS1445);
          _M0L6_2atmpS3601 = 1;
        } else {
          struct moonbit_result_0 _tmp_4108;
          moonbit_incref(_M0L14handle__resultS1440);
          #line 575 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
          _tmp_4108
          = _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1470, _M0L8filenameS1445, _M0L5indexS1448, _M0L14handle__resultS1440, _M0L17error__to__stringS1449);
          if (_tmp_4108.tag) {
            int32_t const _M0L5_2aokS3602 = _tmp_4108.data.ok;
            _M0L6_2atmpS3601 = _M0L5_2aokS3602;
          } else {
            void* const _M0L6_2aerrS3603 = _tmp_4108.data.err;
            _M0L11_2atry__errS1464 = _M0L6_2aerrS3603;
            goto join_1463;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3601) {
    void* _M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3612 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3612)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
    ((struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3612)->$0
    = (moonbit_string_t)moonbit_string_literal_3.data;
    _M0L11_2atry__errS1464
    = _M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3612;
    goto join_1463;
  } else {
    moonbit_decref(_M0L14handle__resultS1440);
  }
  goto joinlet_4099;
  join_1463:;
  _M0L3errS1465 = _M0L11_2atry__errS1464;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1468
  = (struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1465;
  _M0L8_2afieldS3615 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1468->$0;
  _M0L6_2acntS3991
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1468)->rc;
  if (_M0L6_2acntS3991 > 1) {
    int32_t _M0L11_2anew__cntS3992 = _M0L6_2acntS3991 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1468)->rc
    = _M0L11_2anew__cntS3992;
    moonbit_incref(_M0L8_2afieldS3615);
  } else if (_M0L6_2acntS3991 == 1) {
    #line 582 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1468);
  }
  _M0L7_2anameS1469 = _M0L8_2afieldS3615;
  _M0L4nameS1467 = _M0L7_2anameS1469;
  goto join_1466;
  goto joinlet_4109;
  join_1466:;
  #line 583 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1440(_M0L14handle__resultS1440, _M0L4nameS1467, (moonbit_string_t)moonbit_string_literal_4.data, 1);
  joinlet_4109:;
  joinlet_4099:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1449(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3600,
  void* _M0L3errS1450
) {
  void* _M0L1eS1452;
  moonbit_string_t _M0L1eS1454;
  #line 552 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3600);
  switch (Moonbit_object_tag(_M0L3errS1450)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1455 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1450;
      moonbit_string_t _M0L8_2afieldS3616 = _M0L10_2aFailureS1455->$0;
      int32_t _M0L6_2acntS3993 =
        Moonbit_object_header(_M0L10_2aFailureS1455)->rc;
      moonbit_string_t _M0L4_2aeS1456;
      if (_M0L6_2acntS3993 > 1) {
        int32_t _M0L11_2anew__cntS3994 = _M0L6_2acntS3993 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1455)->rc
        = _M0L11_2anew__cntS3994;
        moonbit_incref(_M0L8_2afieldS3616);
      } else if (_M0L6_2acntS3993 == 1) {
        #line 553 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1455);
      }
      _M0L4_2aeS1456 = _M0L8_2afieldS3616;
      _M0L1eS1454 = _M0L4_2aeS1456;
      goto join_1453;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1457 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1450;
      moonbit_string_t _M0L8_2afieldS3617 = _M0L15_2aInspectErrorS1457->$0;
      int32_t _M0L6_2acntS3995 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1457)->rc;
      moonbit_string_t _M0L4_2aeS1458;
      if (_M0L6_2acntS3995 > 1) {
        int32_t _M0L11_2anew__cntS3996 = _M0L6_2acntS3995 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1457)->rc
        = _M0L11_2anew__cntS3996;
        moonbit_incref(_M0L8_2afieldS3617);
      } else if (_M0L6_2acntS3995 == 1) {
        #line 553 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1457);
      }
      _M0L4_2aeS1458 = _M0L8_2afieldS3617;
      _M0L1eS1454 = _M0L4_2aeS1458;
      goto join_1453;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1459 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1450;
      moonbit_string_t _M0L8_2afieldS3618 = _M0L16_2aSnapshotErrorS1459->$0;
      int32_t _M0L6_2acntS3997 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1459)->rc;
      moonbit_string_t _M0L4_2aeS1460;
      if (_M0L6_2acntS3997 > 1) {
        int32_t _M0L11_2anew__cntS3998 = _M0L6_2acntS3997 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1459)->rc
        = _M0L11_2anew__cntS3998;
        moonbit_incref(_M0L8_2afieldS3618);
      } else if (_M0L6_2acntS3997 == 1) {
        #line 553 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1459);
      }
      _M0L4_2aeS1460 = _M0L8_2afieldS3618;
      _M0L1eS1454 = _M0L4_2aeS1460;
      goto join_1453;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error128clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1461 =
        (struct _M0DTPC15error5Error128clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1450;
      moonbit_string_t _M0L8_2afieldS3619 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1461->$0;
      int32_t _M0L6_2acntS3999 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1461)->rc;
      moonbit_string_t _M0L4_2aeS1462;
      if (_M0L6_2acntS3999 > 1) {
        int32_t _M0L11_2anew__cntS4000 = _M0L6_2acntS3999 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1461)->rc
        = _M0L11_2anew__cntS4000;
        moonbit_incref(_M0L8_2afieldS3619);
      } else if (_M0L6_2acntS3999 == 1) {
        #line 553 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1461);
      }
      _M0L4_2aeS1462 = _M0L8_2afieldS3619;
      _M0L1eS1454 = _M0L4_2aeS1462;
      goto join_1453;
      break;
    }
    default: {
      _M0L1eS1452 = _M0L3errS1450;
      goto join_1451;
      break;
    }
  }
  join_1453:;
  return _M0L1eS1454;
  join_1451:;
  #line 558 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1452);
}

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1440(
  struct _M0TWssbEu* _M0L6_2aenvS3586,
  moonbit_string_t _M0L8testnameS1441,
  moonbit_string_t _M0L7messageS1442,
  int32_t _M0L7skippedS1443
) {
  struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440* _M0L14_2acasted__envS3587;
  moonbit_string_t _M0L8_2afieldS3629;
  moonbit_string_t _M0L8filenameS1445;
  int32_t _M0L8_2afieldS3628;
  int32_t _M0L6_2acntS4001;
  int32_t _M0L5indexS1448;
  int32_t _if__result_4112;
  moonbit_string_t _M0L10file__nameS1444;
  moonbit_string_t _M0L10test__nameS1446;
  moonbit_string_t _M0L7messageS1447;
  moonbit_string_t _M0L6_2atmpS3599;
  moonbit_string_t _M0L6_2atmpS3627;
  moonbit_string_t _M0L6_2atmpS3598;
  moonbit_string_t _M0L6_2atmpS3626;
  moonbit_string_t _M0L6_2atmpS3596;
  moonbit_string_t _M0L6_2atmpS3597;
  moonbit_string_t _M0L6_2atmpS3625;
  moonbit_string_t _M0L6_2atmpS3595;
  moonbit_string_t _M0L6_2atmpS3624;
  moonbit_string_t _M0L6_2atmpS3593;
  moonbit_string_t _M0L6_2atmpS3594;
  moonbit_string_t _M0L6_2atmpS3623;
  moonbit_string_t _M0L6_2atmpS3592;
  moonbit_string_t _M0L6_2atmpS3622;
  moonbit_string_t _M0L6_2atmpS3590;
  moonbit_string_t _M0L6_2atmpS3591;
  moonbit_string_t _M0L6_2atmpS3621;
  moonbit_string_t _M0L6_2atmpS3589;
  moonbit_string_t _M0L6_2atmpS3620;
  moonbit_string_t _M0L6_2atmpS3588;
  #line 536 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3587
  = (struct _M0R132_24clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1440*)_M0L6_2aenvS3586;
  _M0L8_2afieldS3629 = _M0L14_2acasted__envS3587->$1;
  _M0L8filenameS1445 = _M0L8_2afieldS3629;
  _M0L8_2afieldS3628 = _M0L14_2acasted__envS3587->$0;
  _M0L6_2acntS4001 = Moonbit_object_header(_M0L14_2acasted__envS3587)->rc;
  if (_M0L6_2acntS4001 > 1) {
    int32_t _M0L11_2anew__cntS4002 = _M0L6_2acntS4001 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3587)->rc
    = _M0L11_2anew__cntS4002;
    moonbit_incref(_M0L8filenameS1445);
  } else if (_M0L6_2acntS4001 == 1) {
    #line 536 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3587);
  }
  _M0L5indexS1448 = _M0L8_2afieldS3628;
  if (!_M0L7skippedS1443) {
    _if__result_4112 = 1;
  } else {
    _if__result_4112 = 0;
  }
  if (_if__result_4112) {
    
  }
  #line 542 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1444 = _M0MPC16string6String6escape(_M0L8filenameS1445);
  #line 543 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1446 = _M0MPC16string6String6escape(_M0L8testnameS1441);
  #line 544 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1447 = _M0MPC16string6String6escape(_M0L7messageS1442);
  #line 545 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_5.data);
  #line 547 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3599
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1444);
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3627
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_6.data, _M0L6_2atmpS3599);
  moonbit_decref(_M0L6_2atmpS3599);
  _M0L6_2atmpS3598 = _M0L6_2atmpS3627;
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3626
  = moonbit_add_string(_M0L6_2atmpS3598, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3598);
  _M0L6_2atmpS3596 = _M0L6_2atmpS3626;
  #line 547 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3597
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1448);
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3625 = moonbit_add_string(_M0L6_2atmpS3596, _M0L6_2atmpS3597);
  moonbit_decref(_M0L6_2atmpS3596);
  moonbit_decref(_M0L6_2atmpS3597);
  _M0L6_2atmpS3595 = _M0L6_2atmpS3625;
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3624
  = moonbit_add_string(_M0L6_2atmpS3595, (moonbit_string_t)moonbit_string_literal_8.data);
  moonbit_decref(_M0L6_2atmpS3595);
  _M0L6_2atmpS3593 = _M0L6_2atmpS3624;
  #line 547 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3594
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1446);
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3623 = moonbit_add_string(_M0L6_2atmpS3593, _M0L6_2atmpS3594);
  moonbit_decref(_M0L6_2atmpS3593);
  moonbit_decref(_M0L6_2atmpS3594);
  _M0L6_2atmpS3592 = _M0L6_2atmpS3623;
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3622
  = moonbit_add_string(_M0L6_2atmpS3592, (moonbit_string_t)moonbit_string_literal_9.data);
  moonbit_decref(_M0L6_2atmpS3592);
  _M0L6_2atmpS3590 = _M0L6_2atmpS3622;
  #line 547 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3591
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1447);
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3621 = moonbit_add_string(_M0L6_2atmpS3590, _M0L6_2atmpS3591);
  moonbit_decref(_M0L6_2atmpS3590);
  moonbit_decref(_M0L6_2atmpS3591);
  _M0L6_2atmpS3589 = _M0L6_2atmpS3621;
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3620
  = moonbit_add_string(_M0L6_2atmpS3589, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS3589);
  _M0L6_2atmpS3588 = _M0L6_2atmpS3620;
  #line 546 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3588);
  #line 549 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_11.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1439,
  moonbit_string_t _M0L8filenameS1436,
  int32_t _M0L5indexS1430,
  struct _M0TWssbEu* _M0L14handle__resultS1426,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1428
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1406;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1435;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1408;
  moonbit_string_t* _M0L5attrsS1409;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1429;
  moonbit_string_t _M0L4nameS1412;
  moonbit_string_t _M0L4nameS1410;
  int32_t _M0L6_2atmpS3585;
  struct _M0TWEOs* _M0L5_2aitS1414;
  struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__* _closure_4121;
  struct _M0TWEOc* _M0L6_2atmpS3576;
  struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__* _closure_4122;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3577;
  struct moonbit_result_0 _result_4123;
  #line 410 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1439);
  moonbit_incref(_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 417 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1435
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1436);
  if (_M0L7_2abindS1435 == 0) {
    struct moonbit_result_0 _result_4114;
    if (_M0L7_2abindS1435) {
      moonbit_decref(_M0L7_2abindS1435);
    }
    moonbit_decref(_M0L17error__to__stringS1428);
    moonbit_decref(_M0L14handle__resultS1426);
    _result_4114.tag = 1;
    _result_4114.data.ok = 0;
    return _result_4114;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1437 =
      _M0L7_2abindS1435;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1438 =
      _M0L7_2aSomeS1437;
    _M0L10index__mapS1406 = _M0L13_2aindex__mapS1438;
    goto join_1405;
  }
  join_1405:;
  #line 419 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1429
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1406, _M0L5indexS1430);
  if (_M0L7_2abindS1429 == 0) {
    struct moonbit_result_0 _result_4116;
    if (_M0L7_2abindS1429) {
      moonbit_decref(_M0L7_2abindS1429);
    }
    moonbit_decref(_M0L17error__to__stringS1428);
    moonbit_decref(_M0L14handle__resultS1426);
    _result_4116.tag = 1;
    _result_4116.data.ok = 0;
    return _result_4116;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1431 =
      _M0L7_2abindS1429;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1432 = _M0L7_2aSomeS1431;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3633 = _M0L4_2axS1432->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1433 = _M0L8_2afieldS3633;
    moonbit_string_t* _M0L8_2afieldS3632 = _M0L4_2axS1432->$1;
    int32_t _M0L6_2acntS4003 = Moonbit_object_header(_M0L4_2axS1432)->rc;
    moonbit_string_t* _M0L8_2aattrsS1434;
    if (_M0L6_2acntS4003 > 1) {
      int32_t _M0L11_2anew__cntS4004 = _M0L6_2acntS4003 - 1;
      Moonbit_object_header(_M0L4_2axS1432)->rc = _M0L11_2anew__cntS4004;
      moonbit_incref(_M0L8_2afieldS3632);
      moonbit_incref(_M0L4_2afS1433);
    } else if (_M0L6_2acntS4003 == 1) {
      #line 417 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1432);
    }
    _M0L8_2aattrsS1434 = _M0L8_2afieldS3632;
    _M0L1fS1408 = _M0L4_2afS1433;
    _M0L5attrsS1409 = _M0L8_2aattrsS1434;
    goto join_1407;
  }
  join_1407:;
  _M0L6_2atmpS3585 = Moonbit_array_length(_M0L5attrsS1409);
  if (_M0L6_2atmpS3585 >= 1) {
    moonbit_string_t _M0L6_2atmpS3631 = (moonbit_string_t)_M0L5attrsS1409[0];
    moonbit_string_t _M0L7_2anameS1413 = _M0L6_2atmpS3631;
    moonbit_incref(_M0L7_2anameS1413);
    _M0L4nameS1412 = _M0L7_2anameS1413;
    goto join_1411;
  } else {
    _M0L4nameS1410 = (moonbit_string_t)moonbit_string_literal_3.data;
  }
  goto joinlet_4117;
  join_1411:;
  _M0L4nameS1410 = _M0L4nameS1412;
  joinlet_4117:;
  #line 420 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1414 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1409);
  while (1) {
    moonbit_string_t _M0L4attrS1416;
    moonbit_string_t _M0L7_2abindS1423;
    int32_t _M0L6_2atmpS3569;
    int64_t _M0L6_2atmpS3568;
    moonbit_incref(_M0L5_2aitS1414);
    #line 422 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1423 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1414);
    if (_M0L7_2abindS1423 == 0) {
      if (_M0L7_2abindS1423) {
        moonbit_decref(_M0L7_2abindS1423);
      }
      moonbit_decref(_M0L5_2aitS1414);
    } else {
      moonbit_string_t _M0L7_2aSomeS1424 = _M0L7_2abindS1423;
      moonbit_string_t _M0L7_2aattrS1425 = _M0L7_2aSomeS1424;
      _M0L4attrS1416 = _M0L7_2aattrS1425;
      goto join_1415;
    }
    goto joinlet_4119;
    join_1415:;
    _M0L6_2atmpS3569 = Moonbit_array_length(_M0L4attrS1416);
    _M0L6_2atmpS3568 = (int64_t)_M0L6_2atmpS3569;
    moonbit_incref(_M0L4attrS1416);
    #line 423 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1416, 5, 0, _M0L6_2atmpS3568)
    ) {
      int32_t _M0L6_2atmpS3575 = _M0L4attrS1416[0];
      int32_t _M0L4_2axS1417 = _M0L6_2atmpS3575;
      if (_M0L4_2axS1417 == 112) {
        int32_t _M0L6_2atmpS3574 = _M0L4attrS1416[1];
        int32_t _M0L4_2axS1418 = _M0L6_2atmpS3574;
        if (_M0L4_2axS1418 == 97) {
          int32_t _M0L6_2atmpS3573 = _M0L4attrS1416[2];
          int32_t _M0L4_2axS1419 = _M0L6_2atmpS3573;
          if (_M0L4_2axS1419 == 110) {
            int32_t _M0L6_2atmpS3572 = _M0L4attrS1416[3];
            int32_t _M0L4_2axS1420 = _M0L6_2atmpS3572;
            if (_M0L4_2axS1420 == 105) {
              int32_t _M0L6_2atmpS3630 = _M0L4attrS1416[4];
              int32_t _M0L6_2atmpS3571;
              int32_t _M0L4_2axS1421;
              moonbit_decref(_M0L4attrS1416);
              _M0L6_2atmpS3571 = _M0L6_2atmpS3630;
              _M0L4_2axS1421 = _M0L6_2atmpS3571;
              if (_M0L4_2axS1421 == 99) {
                void* _M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3570;
                struct moonbit_result_0 _result_4120;
                moonbit_decref(_M0L17error__to__stringS1428);
                moonbit_decref(_M0L14handle__resultS1426);
                moonbit_decref(_M0L5_2aitS1414);
                moonbit_decref(_M0L1fS1408);
                _M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3570
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3570)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
                ((struct _M0DTPC15error5Error130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3570)->$0
                = _M0L4nameS1410;
                _result_4120.tag = 0;
                _result_4120.data.err
                = _M0L130clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3570;
                return _result_4120;
              }
            } else {
              moonbit_decref(_M0L4attrS1416);
            }
          } else {
            moonbit_decref(_M0L4attrS1416);
          }
        } else {
          moonbit_decref(_M0L4attrS1416);
        }
      } else {
        moonbit_decref(_M0L4attrS1416);
      }
    } else {
      moonbit_decref(_M0L4attrS1416);
    }
    continue;
    joinlet_4119:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1426);
  moonbit_incref(_M0L4nameS1410);
  _closure_4121
  = (struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__*)moonbit_malloc(sizeof(struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__));
  Moonbit_object_header(_closure_4121)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__, $0) >> 2, 2, 0);
  _closure_4121->code
  = &_M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testC3582l430;
  _closure_4121->$0 = _M0L14handle__resultS1426;
  _closure_4121->$1 = _M0L4nameS1410;
  _M0L6_2atmpS3576 = (struct _M0TWEOc*)_closure_4121;
  _closure_4122
  = (struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__*)moonbit_malloc(sizeof(struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__));
  Moonbit_object_header(_closure_4122)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__, $0) >> 2, 3, 0);
  _closure_4122->code
  = &_M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testC3578l431;
  _closure_4122->$0 = _M0L17error__to__stringS1428;
  _closure_4122->$1 = _M0L14handle__resultS1426;
  _closure_4122->$2 = _M0L4nameS1410;
  _M0L6_2atmpS3577 = (struct _M0TWRPC15error5ErrorEu*)_closure_4122;
  #line 428 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1408, _M0L6_2atmpS3576, _M0L6_2atmpS3577);
  _result_4123.tag = 1;
  _result_4123.data.ok = 1;
  return _result_4123;
}

int32_t _M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testC3582l430(
  struct _M0TWEOc* _M0L6_2aenvS3583
) {
  struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__* _M0L14_2acasted__envS3584;
  moonbit_string_t _M0L8_2afieldS3635;
  moonbit_string_t _M0L4nameS1410;
  struct _M0TWssbEu* _M0L8_2afieldS3634;
  int32_t _M0L6_2acntS4005;
  struct _M0TWssbEu* _M0L14handle__resultS1426;
  #line 430 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3584
  = (struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3582__l430__*)_M0L6_2aenvS3583;
  _M0L8_2afieldS3635 = _M0L14_2acasted__envS3584->$1;
  _M0L4nameS1410 = _M0L8_2afieldS3635;
  _M0L8_2afieldS3634 = _M0L14_2acasted__envS3584->$0;
  _M0L6_2acntS4005 = Moonbit_object_header(_M0L14_2acasted__envS3584)->rc;
  if (_M0L6_2acntS4005 > 1) {
    int32_t _M0L11_2anew__cntS4006 = _M0L6_2acntS4005 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3584)->rc
    = _M0L11_2anew__cntS4006;
    moonbit_incref(_M0L4nameS1410);
    moonbit_incref(_M0L8_2afieldS3634);
  } else if (_M0L6_2acntS4005 == 1) {
    #line 430 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3584);
  }
  _M0L14handle__resultS1426 = _M0L8_2afieldS3634;
  #line 430 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1426->code(_M0L14handle__resultS1426, _M0L4nameS1410, (moonbit_string_t)moonbit_string_literal_3.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal25buildinfo__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testC3578l431(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3579,
  void* _M0L3errS1427
) {
  struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__* _M0L14_2acasted__envS3580;
  moonbit_string_t _M0L8_2afieldS3638;
  moonbit_string_t _M0L4nameS1410;
  struct _M0TWssbEu* _M0L8_2afieldS3637;
  struct _M0TWssbEu* _M0L14handle__resultS1426;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3636;
  int32_t _M0L6_2acntS4007;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1428;
  moonbit_string_t _M0L6_2atmpS3581;
  #line 431 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3580
  = (struct _M0R233_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fbuildinfo__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3578__l431__*)_M0L6_2aenvS3579;
  _M0L8_2afieldS3638 = _M0L14_2acasted__envS3580->$2;
  _M0L4nameS1410 = _M0L8_2afieldS3638;
  _M0L8_2afieldS3637 = _M0L14_2acasted__envS3580->$1;
  _M0L14handle__resultS1426 = _M0L8_2afieldS3637;
  _M0L8_2afieldS3636 = _M0L14_2acasted__envS3580->$0;
  _M0L6_2acntS4007 = Moonbit_object_header(_M0L14_2acasted__envS3580)->rc;
  if (_M0L6_2acntS4007 > 1) {
    int32_t _M0L11_2anew__cntS4008 = _M0L6_2acntS4007 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3580)->rc
    = _M0L11_2anew__cntS4008;
    moonbit_incref(_M0L4nameS1410);
    moonbit_incref(_M0L14handle__resultS1426);
    moonbit_incref(_M0L8_2afieldS3636);
  } else if (_M0L6_2acntS4007 == 1) {
    #line 431 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3580);
  }
  _M0L17error__to__stringS1428 = _M0L8_2afieldS3636;
  #line 431 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3581
  = _M0L17error__to__stringS1428->code(_M0L17error__to__stringS1428, _M0L3errS1427);
  #line 431 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1426->code(_M0L14handle__resultS1426, _M0L4nameS1410, _M0L6_2atmpS3581, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1399,
  struct _M0TWEOc* _M0L6on__okS1400,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1397
) {
  void* _M0L11_2atry__errS1395;
  struct moonbit_result_0 _tmp_4125;
  void* _M0L3errS1396;
  #line 375 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4125 = _M0L1fS1399->code(_M0L1fS1399);
  if (_tmp_4125.tag) {
    int32_t const _M0L5_2aokS3566 = _tmp_4125.data.ok;
    moonbit_decref(_M0L7on__errS1397);
  } else {
    void* const _M0L6_2aerrS3567 = _tmp_4125.data.err;
    moonbit_decref(_M0L6on__okS1400);
    _M0L11_2atry__errS1395 = _M0L6_2aerrS3567;
    goto join_1394;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1400->code(_M0L6on__okS1400);
  goto joinlet_4124;
  join_1394:;
  _M0L3errS1396 = _M0L11_2atry__errS1395;
  #line 383 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1397->code(_M0L7on__errS1397, _M0L3errS1396);
  joinlet_4124:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1354;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1367;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1372;
  struct _M0TUsiE** _M0L6_2atmpS3565;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1379;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1380;
  moonbit_string_t _M0L6_2atmpS3564;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1381;
  int32_t _M0L7_2abindS1382;
  int32_t _M0L2__S1383;
  #line 193 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1354 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1367
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1372 = 0;
  _M0L6_2atmpS3565 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1379
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1379)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1379->$0 = _M0L6_2atmpS3565;
  _M0L16file__and__indexS1379->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1380
  = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1367(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1367);
  #line 284 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3564 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1380, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1381
  = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1372(_M0L51moonbit__test__driver__internal__split__mbt__stringS1372, _M0L6_2atmpS3564, 47);
  _M0L7_2abindS1382 = _M0L10test__argsS1381->$1;
  _M0L2__S1383 = 0;
  while (1) {
    if (_M0L2__S1383 < _M0L7_2abindS1382) {
      moonbit_string_t* _M0L8_2afieldS3640 = _M0L10test__argsS1381->$0;
      moonbit_string_t* _M0L3bufS3563 = _M0L8_2afieldS3640;
      moonbit_string_t _M0L6_2atmpS3639 =
        (moonbit_string_t)_M0L3bufS3563[_M0L2__S1383];
      moonbit_string_t _M0L3argS1384 = _M0L6_2atmpS3639;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1385;
      moonbit_string_t _M0L4fileS1386;
      moonbit_string_t _M0L5rangeS1387;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1388;
      moonbit_string_t _M0L6_2atmpS3561;
      int32_t _M0L5startS1389;
      moonbit_string_t _M0L6_2atmpS3560;
      int32_t _M0L3endS1390;
      int32_t _M0L1iS1391;
      int32_t _M0L6_2atmpS3562;
      moonbit_incref(_M0L3argS1384);
      #line 288 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1385
      = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1372(_M0L51moonbit__test__driver__internal__split__mbt__stringS1372, _M0L3argS1384, 58);
      moonbit_incref(_M0L16file__and__rangeS1385);
      #line 289 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1386
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1385, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1387
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1385, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1388
      = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1372(_M0L51moonbit__test__driver__internal__split__mbt__stringS1372, _M0L5rangeS1387, 45);
      moonbit_incref(_M0L15start__and__endS1388);
      #line 294 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3561
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1388, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1389
      = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1354(_M0L45moonbit__test__driver__internal__parse__int__S1354, _M0L6_2atmpS3561);
      #line 295 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3560
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1388, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1390
      = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1354(_M0L45moonbit__test__driver__internal__parse__int__S1354, _M0L6_2atmpS3560);
      _M0L1iS1391 = _M0L5startS1389;
      while (1) {
        if (_M0L1iS1391 < _M0L3endS1390) {
          struct _M0TUsiE* _M0L8_2atupleS3558;
          int32_t _M0L6_2atmpS3559;
          moonbit_incref(_M0L4fileS1386);
          _M0L8_2atupleS3558
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3558)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3558->$0 = _M0L4fileS1386;
          _M0L8_2atupleS3558->$1 = _M0L1iS1391;
          moonbit_incref(_M0L16file__and__indexS1379);
          #line 297 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1379, _M0L8_2atupleS3558);
          _M0L6_2atmpS3559 = _M0L1iS1391 + 1;
          _M0L1iS1391 = _M0L6_2atmpS3559;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1386);
        }
        break;
      }
      _M0L6_2atmpS3562 = _M0L2__S1383 + 1;
      _M0L2__S1383 = _M0L6_2atmpS3562;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1381);
    }
    break;
  }
  return _M0L16file__and__indexS1379;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1372(
  int32_t _M0L6_2aenvS3539,
  moonbit_string_t _M0L1sS1373,
  int32_t _M0L3sepS1374
) {
  moonbit_string_t* _M0L6_2atmpS3557;
  struct _M0TPB5ArrayGsE* _M0L3resS1375;
  struct _M0TPC13ref3RefGiE* _M0L1iS1376;
  struct _M0TPC13ref3RefGiE* _M0L5startS1377;
  int32_t _M0L3valS3552;
  int32_t _M0L6_2atmpS3553;
  #line 261 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3557 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1375
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1375)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1375->$0 = _M0L6_2atmpS3557;
  _M0L3resS1375->$1 = 0;
  _M0L1iS1376
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1376)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1376->$0 = 0;
  _M0L5startS1377
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1377)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1377->$0 = 0;
  while (1) {
    int32_t _M0L3valS3540 = _M0L1iS1376->$0;
    int32_t _M0L6_2atmpS3541 = Moonbit_array_length(_M0L1sS1373);
    if (_M0L3valS3540 < _M0L6_2atmpS3541) {
      int32_t _M0L3valS3544 = _M0L1iS1376->$0;
      int32_t _M0L6_2atmpS3543;
      int32_t _M0L6_2atmpS3542;
      int32_t _M0L3valS3551;
      int32_t _M0L6_2atmpS3550;
      if (
        _M0L3valS3544 < 0
        || _M0L3valS3544 >= Moonbit_array_length(_M0L1sS1373)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3543 = _M0L1sS1373[_M0L3valS3544];
      _M0L6_2atmpS3542 = _M0L6_2atmpS3543;
      if (_M0L6_2atmpS3542 == _M0L3sepS1374) {
        int32_t _M0L3valS3546 = _M0L5startS1377->$0;
        int32_t _M0L3valS3547 = _M0L1iS1376->$0;
        moonbit_string_t _M0L6_2atmpS3545;
        int32_t _M0L3valS3549;
        int32_t _M0L6_2atmpS3548;
        moonbit_incref(_M0L1sS1373);
        #line 270 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS3545
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1373, _M0L3valS3546, _M0L3valS3547);
        moonbit_incref(_M0L3resS1375);
        #line 270 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1375, _M0L6_2atmpS3545);
        _M0L3valS3549 = _M0L1iS1376->$0;
        _M0L6_2atmpS3548 = _M0L3valS3549 + 1;
        _M0L5startS1377->$0 = _M0L6_2atmpS3548;
      }
      _M0L3valS3551 = _M0L1iS1376->$0;
      _M0L6_2atmpS3550 = _M0L3valS3551 + 1;
      _M0L1iS1376->$0 = _M0L6_2atmpS3550;
      continue;
    } else {
      moonbit_decref(_M0L1iS1376);
    }
    break;
  }
  _M0L3valS3552 = _M0L5startS1377->$0;
  _M0L6_2atmpS3553 = Moonbit_array_length(_M0L1sS1373);
  if (_M0L3valS3552 < _M0L6_2atmpS3553) {
    int32_t _M0L8_2afieldS3641 = _M0L5startS1377->$0;
    int32_t _M0L3valS3555;
    int32_t _M0L6_2atmpS3556;
    moonbit_string_t _M0L6_2atmpS3554;
    moonbit_decref(_M0L5startS1377);
    _M0L3valS3555 = _M0L8_2afieldS3641;
    _M0L6_2atmpS3556 = Moonbit_array_length(_M0L1sS1373);
    #line 276 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS3554
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1373, _M0L3valS3555, _M0L6_2atmpS3556);
    moonbit_incref(_M0L3resS1375);
    #line 276 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1375, _M0L6_2atmpS3554);
  } else {
    moonbit_decref(_M0L5startS1377);
    moonbit_decref(_M0L1sS1373);
  }
  return _M0L3resS1375;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1367(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360
) {
  moonbit_bytes_t* _M0L3tmpS1368;
  int32_t _M0L6_2atmpS3538;
  struct _M0TPB5ArrayGsE* _M0L3resS1369;
  int32_t _M0L1iS1370;
  #line 250 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1368
  = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3538 = Moonbit_array_length(_M0L3tmpS1368);
  #line 254 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1369 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3538);
  _M0L1iS1370 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3534 = Moonbit_array_length(_M0L3tmpS1368);
    if (_M0L1iS1370 < _M0L6_2atmpS3534) {
      moonbit_bytes_t _M0L6_2atmpS3642;
      moonbit_bytes_t _M0L6_2atmpS3536;
      moonbit_string_t _M0L6_2atmpS3535;
      int32_t _M0L6_2atmpS3537;
      if (
        _M0L1iS1370 < 0 || _M0L1iS1370 >= Moonbit_array_length(_M0L3tmpS1368)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3642 = (moonbit_bytes_t)_M0L3tmpS1368[_M0L1iS1370];
      _M0L6_2atmpS3536 = _M0L6_2atmpS3642;
      moonbit_incref(_M0L6_2atmpS3536);
      #line 256 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3535
      = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360, _M0L6_2atmpS3536);
      moonbit_incref(_M0L3resS1369);
      #line 256 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1369, _M0L6_2atmpS3535);
      _M0L6_2atmpS3537 = _M0L1iS1370 + 1;
      _M0L1iS1370 = _M0L6_2atmpS3537;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1368);
    }
    break;
  }
  return _M0L3resS1369;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1360(
  int32_t _M0L6_2aenvS3448,
  moonbit_bytes_t _M0L5bytesS1361
) {
  struct _M0TPB13StringBuilder* _M0L3resS1362;
  int32_t _M0L3lenS1363;
  struct _M0TPC13ref3RefGiE* _M0L1iS1364;
  #line 206 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1362 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1363 = Moonbit_array_length(_M0L5bytesS1361);
  _M0L1iS1364
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1364)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1364->$0 = 0;
  while (1) {
    int32_t _M0L3valS3449 = _M0L1iS1364->$0;
    if (_M0L3valS3449 < _M0L3lenS1363) {
      int32_t _M0L3valS3533 = _M0L1iS1364->$0;
      int32_t _M0L6_2atmpS3532;
      int32_t _M0L6_2atmpS3531;
      struct _M0TPC13ref3RefGiE* _M0L1cS1365;
      int32_t _M0L3valS3450;
      if (
        _M0L3valS3533 < 0
        || _M0L3valS3533 >= Moonbit_array_length(_M0L5bytesS1361)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3532 = _M0L5bytesS1361[_M0L3valS3533];
      _M0L6_2atmpS3531 = (int32_t)_M0L6_2atmpS3532;
      _M0L1cS1365
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1365)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1365->$0 = _M0L6_2atmpS3531;
      _M0L3valS3450 = _M0L1cS1365->$0;
      if (_M0L3valS3450 < 128) {
        int32_t _M0L8_2afieldS3643 = _M0L1cS1365->$0;
        int32_t _M0L3valS3452;
        int32_t _M0L6_2atmpS3451;
        int32_t _M0L3valS3454;
        int32_t _M0L6_2atmpS3453;
        moonbit_decref(_M0L1cS1365);
        _M0L3valS3452 = _M0L8_2afieldS3643;
        _M0L6_2atmpS3451 = _M0L3valS3452;
        moonbit_incref(_M0L3resS1362);
        #line 215 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1362, _M0L6_2atmpS3451);
        _M0L3valS3454 = _M0L1iS1364->$0;
        _M0L6_2atmpS3453 = _M0L3valS3454 + 1;
        _M0L1iS1364->$0 = _M0L6_2atmpS3453;
      } else {
        int32_t _M0L3valS3455 = _M0L1cS1365->$0;
        if (_M0L3valS3455 < 224) {
          int32_t _M0L3valS3457 = _M0L1iS1364->$0;
          int32_t _M0L6_2atmpS3456 = _M0L3valS3457 + 1;
          int32_t _M0L3valS3466;
          int32_t _M0L6_2atmpS3465;
          int32_t _M0L6_2atmpS3459;
          int32_t _M0L3valS3464;
          int32_t _M0L6_2atmpS3463;
          int32_t _M0L6_2atmpS3462;
          int32_t _M0L6_2atmpS3461;
          int32_t _M0L6_2atmpS3460;
          int32_t _M0L6_2atmpS3458;
          int32_t _M0L8_2afieldS3644;
          int32_t _M0L3valS3468;
          int32_t _M0L6_2atmpS3467;
          int32_t _M0L3valS3470;
          int32_t _M0L6_2atmpS3469;
          if (_M0L6_2atmpS3456 >= _M0L3lenS1363) {
            moonbit_decref(_M0L1cS1365);
            moonbit_decref(_M0L1iS1364);
            moonbit_decref(_M0L5bytesS1361);
            break;
          }
          _M0L3valS3466 = _M0L1cS1365->$0;
          _M0L6_2atmpS3465 = _M0L3valS3466 & 31;
          _M0L6_2atmpS3459 = _M0L6_2atmpS3465 << 6;
          _M0L3valS3464 = _M0L1iS1364->$0;
          _M0L6_2atmpS3463 = _M0L3valS3464 + 1;
          if (
            _M0L6_2atmpS3463 < 0
            || _M0L6_2atmpS3463 >= Moonbit_array_length(_M0L5bytesS1361)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3462 = _M0L5bytesS1361[_M0L6_2atmpS3463];
          _M0L6_2atmpS3461 = (int32_t)_M0L6_2atmpS3462;
          _M0L6_2atmpS3460 = _M0L6_2atmpS3461 & 63;
          _M0L6_2atmpS3458 = _M0L6_2atmpS3459 | _M0L6_2atmpS3460;
          _M0L1cS1365->$0 = _M0L6_2atmpS3458;
          _M0L8_2afieldS3644 = _M0L1cS1365->$0;
          moonbit_decref(_M0L1cS1365);
          _M0L3valS3468 = _M0L8_2afieldS3644;
          _M0L6_2atmpS3467 = _M0L3valS3468;
          moonbit_incref(_M0L3resS1362);
          #line 222 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1362, _M0L6_2atmpS3467);
          _M0L3valS3470 = _M0L1iS1364->$0;
          _M0L6_2atmpS3469 = _M0L3valS3470 + 2;
          _M0L1iS1364->$0 = _M0L6_2atmpS3469;
        } else {
          int32_t _M0L3valS3471 = _M0L1cS1365->$0;
          if (_M0L3valS3471 < 240) {
            int32_t _M0L3valS3473 = _M0L1iS1364->$0;
            int32_t _M0L6_2atmpS3472 = _M0L3valS3473 + 2;
            int32_t _M0L3valS3489;
            int32_t _M0L6_2atmpS3488;
            int32_t _M0L6_2atmpS3481;
            int32_t _M0L3valS3487;
            int32_t _M0L6_2atmpS3486;
            int32_t _M0L6_2atmpS3485;
            int32_t _M0L6_2atmpS3484;
            int32_t _M0L6_2atmpS3483;
            int32_t _M0L6_2atmpS3482;
            int32_t _M0L6_2atmpS3475;
            int32_t _M0L3valS3480;
            int32_t _M0L6_2atmpS3479;
            int32_t _M0L6_2atmpS3478;
            int32_t _M0L6_2atmpS3477;
            int32_t _M0L6_2atmpS3476;
            int32_t _M0L6_2atmpS3474;
            int32_t _M0L8_2afieldS3645;
            int32_t _M0L3valS3491;
            int32_t _M0L6_2atmpS3490;
            int32_t _M0L3valS3493;
            int32_t _M0L6_2atmpS3492;
            if (_M0L6_2atmpS3472 >= _M0L3lenS1363) {
              moonbit_decref(_M0L1cS1365);
              moonbit_decref(_M0L1iS1364);
              moonbit_decref(_M0L5bytesS1361);
              break;
            }
            _M0L3valS3489 = _M0L1cS1365->$0;
            _M0L6_2atmpS3488 = _M0L3valS3489 & 15;
            _M0L6_2atmpS3481 = _M0L6_2atmpS3488 << 12;
            _M0L3valS3487 = _M0L1iS1364->$0;
            _M0L6_2atmpS3486 = _M0L3valS3487 + 1;
            if (
              _M0L6_2atmpS3486 < 0
              || _M0L6_2atmpS3486 >= Moonbit_array_length(_M0L5bytesS1361)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3485 = _M0L5bytesS1361[_M0L6_2atmpS3486];
            _M0L6_2atmpS3484 = (int32_t)_M0L6_2atmpS3485;
            _M0L6_2atmpS3483 = _M0L6_2atmpS3484 & 63;
            _M0L6_2atmpS3482 = _M0L6_2atmpS3483 << 6;
            _M0L6_2atmpS3475 = _M0L6_2atmpS3481 | _M0L6_2atmpS3482;
            _M0L3valS3480 = _M0L1iS1364->$0;
            _M0L6_2atmpS3479 = _M0L3valS3480 + 2;
            if (
              _M0L6_2atmpS3479 < 0
              || _M0L6_2atmpS3479 >= Moonbit_array_length(_M0L5bytesS1361)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3478 = _M0L5bytesS1361[_M0L6_2atmpS3479];
            _M0L6_2atmpS3477 = (int32_t)_M0L6_2atmpS3478;
            _M0L6_2atmpS3476 = _M0L6_2atmpS3477 & 63;
            _M0L6_2atmpS3474 = _M0L6_2atmpS3475 | _M0L6_2atmpS3476;
            _M0L1cS1365->$0 = _M0L6_2atmpS3474;
            _M0L8_2afieldS3645 = _M0L1cS1365->$0;
            moonbit_decref(_M0L1cS1365);
            _M0L3valS3491 = _M0L8_2afieldS3645;
            _M0L6_2atmpS3490 = _M0L3valS3491;
            moonbit_incref(_M0L3resS1362);
            #line 231 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1362, _M0L6_2atmpS3490);
            _M0L3valS3493 = _M0L1iS1364->$0;
            _M0L6_2atmpS3492 = _M0L3valS3493 + 3;
            _M0L1iS1364->$0 = _M0L6_2atmpS3492;
          } else {
            int32_t _M0L3valS3495 = _M0L1iS1364->$0;
            int32_t _M0L6_2atmpS3494 = _M0L3valS3495 + 3;
            int32_t _M0L3valS3518;
            int32_t _M0L6_2atmpS3517;
            int32_t _M0L6_2atmpS3510;
            int32_t _M0L3valS3516;
            int32_t _M0L6_2atmpS3515;
            int32_t _M0L6_2atmpS3514;
            int32_t _M0L6_2atmpS3513;
            int32_t _M0L6_2atmpS3512;
            int32_t _M0L6_2atmpS3511;
            int32_t _M0L6_2atmpS3503;
            int32_t _M0L3valS3509;
            int32_t _M0L6_2atmpS3508;
            int32_t _M0L6_2atmpS3507;
            int32_t _M0L6_2atmpS3506;
            int32_t _M0L6_2atmpS3505;
            int32_t _M0L6_2atmpS3504;
            int32_t _M0L6_2atmpS3497;
            int32_t _M0L3valS3502;
            int32_t _M0L6_2atmpS3501;
            int32_t _M0L6_2atmpS3500;
            int32_t _M0L6_2atmpS3499;
            int32_t _M0L6_2atmpS3498;
            int32_t _M0L6_2atmpS3496;
            int32_t _M0L3valS3520;
            int32_t _M0L6_2atmpS3519;
            int32_t _M0L3valS3524;
            int32_t _M0L6_2atmpS3523;
            int32_t _M0L6_2atmpS3522;
            int32_t _M0L6_2atmpS3521;
            int32_t _M0L8_2afieldS3646;
            int32_t _M0L3valS3528;
            int32_t _M0L6_2atmpS3527;
            int32_t _M0L6_2atmpS3526;
            int32_t _M0L6_2atmpS3525;
            int32_t _M0L3valS3530;
            int32_t _M0L6_2atmpS3529;
            if (_M0L6_2atmpS3494 >= _M0L3lenS1363) {
              moonbit_decref(_M0L1cS1365);
              moonbit_decref(_M0L1iS1364);
              moonbit_decref(_M0L5bytesS1361);
              break;
            }
            _M0L3valS3518 = _M0L1cS1365->$0;
            _M0L6_2atmpS3517 = _M0L3valS3518 & 7;
            _M0L6_2atmpS3510 = _M0L6_2atmpS3517 << 18;
            _M0L3valS3516 = _M0L1iS1364->$0;
            _M0L6_2atmpS3515 = _M0L3valS3516 + 1;
            if (
              _M0L6_2atmpS3515 < 0
              || _M0L6_2atmpS3515 >= Moonbit_array_length(_M0L5bytesS1361)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3514 = _M0L5bytesS1361[_M0L6_2atmpS3515];
            _M0L6_2atmpS3513 = (int32_t)_M0L6_2atmpS3514;
            _M0L6_2atmpS3512 = _M0L6_2atmpS3513 & 63;
            _M0L6_2atmpS3511 = _M0L6_2atmpS3512 << 12;
            _M0L6_2atmpS3503 = _M0L6_2atmpS3510 | _M0L6_2atmpS3511;
            _M0L3valS3509 = _M0L1iS1364->$0;
            _M0L6_2atmpS3508 = _M0L3valS3509 + 2;
            if (
              _M0L6_2atmpS3508 < 0
              || _M0L6_2atmpS3508 >= Moonbit_array_length(_M0L5bytesS1361)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3507 = _M0L5bytesS1361[_M0L6_2atmpS3508];
            _M0L6_2atmpS3506 = (int32_t)_M0L6_2atmpS3507;
            _M0L6_2atmpS3505 = _M0L6_2atmpS3506 & 63;
            _M0L6_2atmpS3504 = _M0L6_2atmpS3505 << 6;
            _M0L6_2atmpS3497 = _M0L6_2atmpS3503 | _M0L6_2atmpS3504;
            _M0L3valS3502 = _M0L1iS1364->$0;
            _M0L6_2atmpS3501 = _M0L3valS3502 + 3;
            if (
              _M0L6_2atmpS3501 < 0
              || _M0L6_2atmpS3501 >= Moonbit_array_length(_M0L5bytesS1361)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3500 = _M0L5bytesS1361[_M0L6_2atmpS3501];
            _M0L6_2atmpS3499 = (int32_t)_M0L6_2atmpS3500;
            _M0L6_2atmpS3498 = _M0L6_2atmpS3499 & 63;
            _M0L6_2atmpS3496 = _M0L6_2atmpS3497 | _M0L6_2atmpS3498;
            _M0L1cS1365->$0 = _M0L6_2atmpS3496;
            _M0L3valS3520 = _M0L1cS1365->$0;
            _M0L6_2atmpS3519 = _M0L3valS3520 - 65536;
            _M0L1cS1365->$0 = _M0L6_2atmpS3519;
            _M0L3valS3524 = _M0L1cS1365->$0;
            _M0L6_2atmpS3523 = _M0L3valS3524 >> 10;
            _M0L6_2atmpS3522 = _M0L6_2atmpS3523 + 55296;
            _M0L6_2atmpS3521 = _M0L6_2atmpS3522;
            moonbit_incref(_M0L3resS1362);
            #line 242 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1362, _M0L6_2atmpS3521);
            _M0L8_2afieldS3646 = _M0L1cS1365->$0;
            moonbit_decref(_M0L1cS1365);
            _M0L3valS3528 = _M0L8_2afieldS3646;
            _M0L6_2atmpS3527 = _M0L3valS3528 & 1023;
            _M0L6_2atmpS3526 = _M0L6_2atmpS3527 + 56320;
            _M0L6_2atmpS3525 = _M0L6_2atmpS3526;
            moonbit_incref(_M0L3resS1362);
            #line 243 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1362, _M0L6_2atmpS3525);
            _M0L3valS3530 = _M0L1iS1364->$0;
            _M0L6_2atmpS3529 = _M0L3valS3530 + 4;
            _M0L1iS1364->$0 = _M0L6_2atmpS3529;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1364);
      moonbit_decref(_M0L5bytesS1361);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1362);
}

int32_t _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1354(
  int32_t _M0L6_2aenvS3441,
  moonbit_string_t _M0L1sS1355
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1356;
  int32_t _M0L3lenS1357;
  int32_t _M0L1iS1358;
  int32_t _M0L8_2afieldS3647;
  #line 197 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1356
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1356)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1356->$0 = 0;
  _M0L3lenS1357 = Moonbit_array_length(_M0L1sS1355);
  _M0L1iS1358 = 0;
  while (1) {
    if (_M0L1iS1358 < _M0L3lenS1357) {
      int32_t _M0L3valS3446 = _M0L3resS1356->$0;
      int32_t _M0L6_2atmpS3443 = _M0L3valS3446 * 10;
      int32_t _M0L6_2atmpS3445;
      int32_t _M0L6_2atmpS3444;
      int32_t _M0L6_2atmpS3442;
      int32_t _M0L6_2atmpS3447;
      if (
        _M0L1iS1358 < 0 || _M0L1iS1358 >= Moonbit_array_length(_M0L1sS1355)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3445 = _M0L1sS1355[_M0L1iS1358];
      _M0L6_2atmpS3444 = _M0L6_2atmpS3445 - 48;
      _M0L6_2atmpS3442 = _M0L6_2atmpS3443 + _M0L6_2atmpS3444;
      _M0L3resS1356->$0 = _M0L6_2atmpS3442;
      _M0L6_2atmpS3447 = _M0L1iS1358 + 1;
      _M0L1iS1358 = _M0L6_2atmpS3447;
      continue;
    } else {
      moonbit_decref(_M0L1sS1355);
    }
    break;
  }
  _M0L8_2afieldS3647 = _M0L3resS1356->$0;
  moonbit_decref(_M0L3resS1356);
  return _M0L8_2afieldS3647;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1334,
  moonbit_string_t _M0L12_2adiscard__S1335,
  int32_t _M0L12_2adiscard__S1336,
  struct _M0TWssbEu* _M0L12_2adiscard__S1337,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1338
) {
  struct moonbit_result_0 _result_4132;
  #line 34 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1338);
  moonbit_decref(_M0L12_2adiscard__S1337);
  moonbit_decref(_M0L12_2adiscard__S1335);
  moonbit_decref(_M0L12_2adiscard__S1334);
  _result_4132.tag = 1;
  _result_4132.data.ok = 0;
  return _result_4132;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1339,
  moonbit_string_t _M0L12_2adiscard__S1340,
  int32_t _M0L12_2adiscard__S1341,
  struct _M0TWssbEu* _M0L12_2adiscard__S1342,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1343
) {
  struct moonbit_result_0 _result_4133;
  #line 34 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1343);
  moonbit_decref(_M0L12_2adiscard__S1342);
  moonbit_decref(_M0L12_2adiscard__S1340);
  moonbit_decref(_M0L12_2adiscard__S1339);
  _result_4133.tag = 1;
  _result_4133.data.ok = 0;
  return _result_4133;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1344,
  moonbit_string_t _M0L12_2adiscard__S1345,
  int32_t _M0L12_2adiscard__S1346,
  struct _M0TWssbEu* _M0L12_2adiscard__S1347,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1348
) {
  struct moonbit_result_0 _result_4134;
  #line 34 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1348);
  moonbit_decref(_M0L12_2adiscard__S1347);
  moonbit_decref(_M0L12_2adiscard__S1345);
  moonbit_decref(_M0L12_2adiscard__S1344);
  _result_4134.tag = 1;
  _result_4134.data.ok = 0;
  return _result_4134;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal25buildinfo__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1349,
  moonbit_string_t _M0L12_2adiscard__S1350,
  int32_t _M0L12_2adiscard__S1351,
  struct _M0TWssbEu* _M0L12_2adiscard__S1352,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1353
) {
  struct moonbit_result_0 _result_4135;
  #line 34 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1353);
  moonbit_decref(_M0L12_2adiscard__S1352);
  moonbit_decref(_M0L12_2adiscard__S1350);
  moonbit_decref(_M0L12_2adiscard__S1349);
  _result_4135.tag = 1;
  _result_4135.data.ok = 0;
  return _result_4135;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal25buildinfo__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1333
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1333);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45____test__76657273696f6e5f746573742e6d6274__1(
  
) {
  struct moonbit_result_1 _tmp_4136;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v1S1327;
  struct moonbit_result_1 _tmp_4138;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v2S1328;
  struct moonbit_result_1 _tmp_4140;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v3S1329;
  struct moonbit_result_1 _tmp_4142;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v4S1330;
  struct moonbit_result_1 _tmp_4144;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v5S1331;
  struct moonbit_result_1 _tmp_4146;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v6S1332;
  int32_t _M0L6_2atmpS3349;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3350;
  struct _M0TPB6ToJson _M0L6_2atmpS3340;
  void* _M0L6_2atmpS3348;
  void* _M0L6_2atmpS3341;
  moonbit_string_t _M0L6_2atmpS3344;
  moonbit_string_t _M0L6_2atmpS3345;
  moonbit_string_t _M0L6_2atmpS3346;
  moonbit_string_t _M0L6_2atmpS3347;
  moonbit_string_t* _M0L6_2atmpS3343;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3342;
  struct moonbit_result_0 _tmp_4148;
  int32_t _M0L6_2atmpS3362;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3363;
  struct _M0TPB6ToJson _M0L6_2atmpS3353;
  void* _M0L6_2atmpS3361;
  void* _M0L6_2atmpS3354;
  moonbit_string_t _M0L6_2atmpS3357;
  moonbit_string_t _M0L6_2atmpS3358;
  moonbit_string_t _M0L6_2atmpS3359;
  moonbit_string_t _M0L6_2atmpS3360;
  moonbit_string_t* _M0L6_2atmpS3356;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3355;
  struct moonbit_result_0 _tmp_4150;
  int32_t _M0L6_2atmpS3375;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3376;
  struct _M0TPB6ToJson _M0L6_2atmpS3366;
  void* _M0L6_2atmpS3374;
  void* _M0L6_2atmpS3367;
  moonbit_string_t _M0L6_2atmpS3370;
  moonbit_string_t _M0L6_2atmpS3371;
  moonbit_string_t _M0L6_2atmpS3372;
  moonbit_string_t _M0L6_2atmpS3373;
  moonbit_string_t* _M0L6_2atmpS3369;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3368;
  struct moonbit_result_0 _tmp_4152;
  int32_t _M0L6_2atmpS3388;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3389;
  struct _M0TPB6ToJson _M0L6_2atmpS3379;
  void* _M0L6_2atmpS3387;
  void* _M0L6_2atmpS3380;
  moonbit_string_t _M0L6_2atmpS3383;
  moonbit_string_t _M0L6_2atmpS3384;
  moonbit_string_t _M0L6_2atmpS3385;
  moonbit_string_t _M0L6_2atmpS3386;
  moonbit_string_t* _M0L6_2atmpS3382;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3381;
  struct moonbit_result_0 _tmp_4154;
  int32_t _M0L6_2atmpS3401;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3402;
  struct _M0TPB6ToJson _M0L6_2atmpS3392;
  void* _M0L6_2atmpS3400;
  void* _M0L6_2atmpS3393;
  moonbit_string_t _M0L6_2atmpS3396;
  moonbit_string_t _M0L6_2atmpS3397;
  moonbit_string_t _M0L6_2atmpS3398;
  moonbit_string_t _M0L6_2atmpS3399;
  moonbit_string_t* _M0L6_2atmpS3395;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3394;
  struct moonbit_result_0 _tmp_4156;
  int32_t _M0L6_2atmpS3414;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3415;
  struct _M0TPB6ToJson _M0L6_2atmpS3405;
  void* _M0L6_2atmpS3413;
  void* _M0L6_2atmpS3406;
  moonbit_string_t _M0L6_2atmpS3409;
  moonbit_string_t _M0L6_2atmpS3410;
  moonbit_string_t _M0L6_2atmpS3411;
  moonbit_string_t _M0L6_2atmpS3412;
  moonbit_string_t* _M0L6_2atmpS3408;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3407;
  struct moonbit_result_0 _tmp_4158;
  int32_t _M0L6_2atmpS3427;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3428;
  struct _M0TPB6ToJson _M0L6_2atmpS3418;
  void* _M0L6_2atmpS3426;
  void* _M0L6_2atmpS3419;
  moonbit_string_t _M0L6_2atmpS3422;
  moonbit_string_t _M0L6_2atmpS3423;
  moonbit_string_t _M0L6_2atmpS3424;
  moonbit_string_t _M0L6_2atmpS3425;
  moonbit_string_t* _M0L6_2atmpS3421;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3420;
  #line 15 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  #line 16 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4136
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_12.data);
  if (_tmp_4136.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3439 =
      _tmp_4136.data.ok;
    _M0L2v1S1327 = _M0L5_2aokS3439;
  } else {
    void* const _M0L6_2aerrS3440 = _tmp_4136.data.err;
    struct moonbit_result_0 _result_4137;
    _result_4137.tag = 0;
    _result_4137.data.err = _M0L6_2aerrS3440;
    return _result_4137;
  }
  #line 17 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4138
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_13.data);
  if (_tmp_4138.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3437 =
      _tmp_4138.data.ok;
    _M0L2v2S1328 = _M0L5_2aokS3437;
  } else {
    void* const _M0L6_2aerrS3438 = _tmp_4138.data.err;
    struct moonbit_result_0 _result_4139;
    moonbit_decref(_M0L2v1S1327);
    _result_4139.tag = 0;
    _result_4139.data.err = _M0L6_2aerrS3438;
    return _result_4139;
  }
  #line 18 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4140
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_14.data);
  if (_tmp_4140.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3435 =
      _tmp_4140.data.ok;
    _M0L2v3S1329 = _M0L5_2aokS3435;
  } else {
    void* const _M0L6_2aerrS3436 = _tmp_4140.data.err;
    struct moonbit_result_0 _result_4141;
    moonbit_decref(_M0L2v2S1328);
    moonbit_decref(_M0L2v1S1327);
    _result_4141.tag = 0;
    _result_4141.data.err = _M0L6_2aerrS3436;
    return _result_4141;
  }
  #line 19 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4142
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_15.data);
  if (_tmp_4142.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3433 =
      _tmp_4142.data.ok;
    _M0L2v4S1330 = _M0L5_2aokS3433;
  } else {
    void* const _M0L6_2aerrS3434 = _tmp_4142.data.err;
    struct moonbit_result_0 _result_4143;
    moonbit_decref(_M0L2v3S1329);
    moonbit_decref(_M0L2v2S1328);
    moonbit_decref(_M0L2v1S1327);
    _result_4143.tag = 0;
    _result_4143.data.err = _M0L6_2aerrS3434;
    return _result_4143;
  }
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4144
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_16.data);
  if (_tmp_4144.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3431 =
      _tmp_4144.data.ok;
    _M0L2v5S1331 = _M0L5_2aokS3431;
  } else {
    void* const _M0L6_2aerrS3432 = _tmp_4144.data.err;
    struct moonbit_result_0 _result_4145;
    moonbit_decref(_M0L2v4S1330);
    moonbit_decref(_M0L2v3S1329);
    moonbit_decref(_M0L2v2S1328);
    moonbit_decref(_M0L2v1S1327);
    _result_4145.tag = 0;
    _result_4145.data.err = _M0L6_2aerrS3432;
    return _result_4145;
  }
  #line 21 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4146
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_17.data);
  if (_tmp_4146.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3429 =
      _tmp_4146.data.ok;
    _M0L2v6S1332 = _M0L5_2aokS3429;
  } else {
    void* const _M0L6_2aerrS3430 = _tmp_4146.data.err;
    struct moonbit_result_0 _result_4147;
    moonbit_decref(_M0L2v5S1331);
    moonbit_decref(_M0L2v4S1330);
    moonbit_decref(_M0L2v3S1329);
    moonbit_decref(_M0L2v2S1328);
    moonbit_decref(_M0L2v1S1327);
    _result_4147.tag = 0;
    _result_4147.data.err = _M0L6_2aerrS3430;
    return _result_4147;
  }
  moonbit_incref(_M0L2v1S1327);
  moonbit_incref(_M0L2v1S1327);
  #line 22 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3349
  = _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB2Eq5equal(_M0L2v1S1327, _M0L2v1S1327);
  _M0L14_2aboxed__selfS3350
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3350)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3350->$0 = _M0L6_2atmpS3349;
  _M0L6_2atmpS3340
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3350
  };
  #line 22 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3348 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3341 = _M0L6_2atmpS3348;
  _M0L6_2atmpS3344 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3345 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3346 = 0;
  _M0L6_2atmpS3347 = 0;
  _M0L6_2atmpS3343 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3343[0] = _M0L6_2atmpS3344;
  _M0L6_2atmpS3343[1] = _M0L6_2atmpS3345;
  _M0L6_2atmpS3343[2] = _M0L6_2atmpS3346;
  _M0L6_2atmpS3343[3] = _M0L6_2atmpS3347;
  _M0L6_2atmpS3342
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3342)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3342->$0 = _M0L6_2atmpS3343;
  _M0L6_2atmpS3342->$1 = 4;
  #line 22 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4148
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3340, _M0L6_2atmpS3341, (moonbit_string_t)moonbit_string_literal_20.data, _M0L6_2atmpS3342);
  if (_tmp_4148.tag) {
    int32_t const _M0L5_2aokS3351 = _tmp_4148.data.ok;
  } else {
    void* const _M0L6_2aerrS3352 = _tmp_4148.data.err;
    struct moonbit_result_0 _result_4149;
    moonbit_decref(_M0L2v6S1332);
    moonbit_decref(_M0L2v5S1331);
    moonbit_decref(_M0L2v4S1330);
    moonbit_decref(_M0L2v3S1329);
    moonbit_decref(_M0L2v2S1328);
    moonbit_decref(_M0L2v1S1327);
    _result_4149.tag = 0;
    _result_4149.data.err = _M0L6_2aerrS3352;
    return _result_4149;
  }
  moonbit_incref(_M0L2v2S1328);
  moonbit_incref(_M0L2v1S1327);
  #line 23 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3362
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal9buildinfo7VersionE(_M0L2v1S1327, _M0L2v2S1328);
  _M0L14_2aboxed__selfS3363
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3363)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3363->$0 = _M0L6_2atmpS3362;
  _M0L6_2atmpS3353
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3363
  };
  #line 23 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3361 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3354 = _M0L6_2atmpS3361;
  _M0L6_2atmpS3357 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3358 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3359 = 0;
  _M0L6_2atmpS3360 = 0;
  _M0L6_2atmpS3356 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3356[0] = _M0L6_2atmpS3357;
  _M0L6_2atmpS3356[1] = _M0L6_2atmpS3358;
  _M0L6_2atmpS3356[2] = _M0L6_2atmpS3359;
  _M0L6_2atmpS3356[3] = _M0L6_2atmpS3360;
  _M0L6_2atmpS3355
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3355)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3355->$0 = _M0L6_2atmpS3356;
  _M0L6_2atmpS3355->$1 = 4;
  #line 23 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4150
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3353, _M0L6_2atmpS3354, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS3355);
  if (_tmp_4150.tag) {
    int32_t const _M0L5_2aokS3364 = _tmp_4150.data.ok;
  } else {
    void* const _M0L6_2aerrS3365 = _tmp_4150.data.err;
    struct moonbit_result_0 _result_4151;
    moonbit_decref(_M0L2v6S1332);
    moonbit_decref(_M0L2v5S1331);
    moonbit_decref(_M0L2v4S1330);
    moonbit_decref(_M0L2v3S1329);
    moonbit_decref(_M0L2v2S1328);
    moonbit_decref(_M0L2v1S1327);
    _result_4151.tag = 0;
    _result_4151.data.err = _M0L6_2aerrS3365;
    return _result_4151;
  }
  moonbit_incref(_M0L2v2S1328);
  moonbit_incref(_M0L2v1S1327);
  #line 24 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3375
  = _M0IP016_24default__implPB7Compare6op__gtGRP48clawteam8clawteam8internal9buildinfo7VersionE(_M0L2v2S1328, _M0L2v1S1327);
  _M0L14_2aboxed__selfS3376
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3376)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3376->$0 = _M0L6_2atmpS3375;
  _M0L6_2atmpS3366
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3376
  };
  #line 24 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3374 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3367 = _M0L6_2atmpS3374;
  _M0L6_2atmpS3370 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS3371 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3372 = 0;
  _M0L6_2atmpS3373 = 0;
  _M0L6_2atmpS3369 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3369[0] = _M0L6_2atmpS3370;
  _M0L6_2atmpS3369[1] = _M0L6_2atmpS3371;
  _M0L6_2atmpS3369[2] = _M0L6_2atmpS3372;
  _M0L6_2atmpS3369[3] = _M0L6_2atmpS3373;
  _M0L6_2atmpS3368
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3368)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3368->$0 = _M0L6_2atmpS3369;
  _M0L6_2atmpS3368->$1 = 4;
  #line 24 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4152
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3366, _M0L6_2atmpS3367, (moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS3368);
  if (_tmp_4152.tag) {
    int32_t const _M0L5_2aokS3377 = _tmp_4152.data.ok;
  } else {
    void* const _M0L6_2aerrS3378 = _tmp_4152.data.err;
    struct moonbit_result_0 _result_4153;
    moonbit_decref(_M0L2v6S1332);
    moonbit_decref(_M0L2v5S1331);
    moonbit_decref(_M0L2v4S1330);
    moonbit_decref(_M0L2v3S1329);
    moonbit_decref(_M0L2v2S1328);
    moonbit_decref(_M0L2v1S1327);
    _result_4153.tag = 0;
    _result_4153.data.err = _M0L6_2aerrS3378;
    return _result_4153;
  }
  moonbit_incref(_M0L2v3S1329);
  #line 25 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3388
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal9buildinfo7VersionE(_M0L2v2S1328, _M0L2v3S1329);
  _M0L14_2aboxed__selfS3389
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3389)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3389->$0 = _M0L6_2atmpS3388;
  _M0L6_2atmpS3379
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3389
  };
  #line 25 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3387 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3380 = _M0L6_2atmpS3387;
  _M0L6_2atmpS3383 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS3384 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3385 = 0;
  _M0L6_2atmpS3386 = 0;
  _M0L6_2atmpS3382 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3382[0] = _M0L6_2atmpS3383;
  _M0L6_2atmpS3382[1] = _M0L6_2atmpS3384;
  _M0L6_2atmpS3382[2] = _M0L6_2atmpS3385;
  _M0L6_2atmpS3382[3] = _M0L6_2atmpS3386;
  _M0L6_2atmpS3381
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3381)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3381->$0 = _M0L6_2atmpS3382;
  _M0L6_2atmpS3381->$1 = 4;
  #line 25 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4154
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3379, _M0L6_2atmpS3380, (moonbit_string_t)moonbit_string_literal_29.data, _M0L6_2atmpS3381);
  if (_tmp_4154.tag) {
    int32_t const _M0L5_2aokS3390 = _tmp_4154.data.ok;
  } else {
    void* const _M0L6_2aerrS3391 = _tmp_4154.data.err;
    struct moonbit_result_0 _result_4155;
    moonbit_decref(_M0L2v6S1332);
    moonbit_decref(_M0L2v5S1331);
    moonbit_decref(_M0L2v4S1330);
    moonbit_decref(_M0L2v3S1329);
    moonbit_decref(_M0L2v1S1327);
    _result_4155.tag = 0;
    _result_4155.data.err = _M0L6_2aerrS3391;
    return _result_4155;
  }
  #line 26 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3401
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal9buildinfo7VersionE(_M0L2v3S1329, _M0L2v4S1330);
  _M0L14_2aboxed__selfS3402
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3402)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3402->$0 = _M0L6_2atmpS3401;
  _M0L6_2atmpS3392
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3402
  };
  #line 26 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3400 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3393 = _M0L6_2atmpS3400;
  _M0L6_2atmpS3396 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3397 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3398 = 0;
  _M0L6_2atmpS3399 = 0;
  _M0L6_2atmpS3395 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3395[0] = _M0L6_2atmpS3396;
  _M0L6_2atmpS3395[1] = _M0L6_2atmpS3397;
  _M0L6_2atmpS3395[2] = _M0L6_2atmpS3398;
  _M0L6_2atmpS3395[3] = _M0L6_2atmpS3399;
  _M0L6_2atmpS3394
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3394)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3394->$0 = _M0L6_2atmpS3395;
  _M0L6_2atmpS3394->$1 = 4;
  #line 26 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4156
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3392, _M0L6_2atmpS3393, (moonbit_string_t)moonbit_string_literal_32.data, _M0L6_2atmpS3394);
  if (_tmp_4156.tag) {
    int32_t const _M0L5_2aokS3403 = _tmp_4156.data.ok;
  } else {
    void* const _M0L6_2aerrS3404 = _tmp_4156.data.err;
    struct moonbit_result_0 _result_4157;
    moonbit_decref(_M0L2v6S1332);
    moonbit_decref(_M0L2v5S1331);
    moonbit_decref(_M0L2v1S1327);
    _result_4157.tag = 0;
    _result_4157.data.err = _M0L6_2aerrS3404;
    return _result_4157;
  }
  moonbit_incref(_M0L2v5S1331);
  #line 27 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3414
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal9buildinfo7VersionE(_M0L2v1S1327, _M0L2v5S1331);
  _M0L14_2aboxed__selfS3415
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3415)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3415->$0 = _M0L6_2atmpS3414;
  _M0L6_2atmpS3405
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3415
  };
  #line 27 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3413 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3406 = _M0L6_2atmpS3413;
  _M0L6_2atmpS3409 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3410 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3411 = 0;
  _M0L6_2atmpS3412 = 0;
  _M0L6_2atmpS3408 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3408[0] = _M0L6_2atmpS3409;
  _M0L6_2atmpS3408[1] = _M0L6_2atmpS3410;
  _M0L6_2atmpS3408[2] = _M0L6_2atmpS3411;
  _M0L6_2atmpS3408[3] = _M0L6_2atmpS3412;
  _M0L6_2atmpS3407
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3407)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3407->$0 = _M0L6_2atmpS3408;
  _M0L6_2atmpS3407->$1 = 4;
  #line 27 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4158
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3405, _M0L6_2atmpS3406, (moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS3407);
  if (_tmp_4158.tag) {
    int32_t const _M0L5_2aokS3416 = _tmp_4158.data.ok;
  } else {
    void* const _M0L6_2aerrS3417 = _tmp_4158.data.err;
    struct moonbit_result_0 _result_4159;
    moonbit_decref(_M0L2v6S1332);
    moonbit_decref(_M0L2v5S1331);
    _result_4159.tag = 0;
    _result_4159.data.err = _M0L6_2aerrS3417;
    return _result_4159;
  }
  #line 28 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3427
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal9buildinfo7VersionE(_M0L2v5S1331, _M0L2v6S1332);
  _M0L14_2aboxed__selfS3428
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3428)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3428->$0 = _M0L6_2atmpS3427;
  _M0L6_2atmpS3418
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3428
  };
  #line 28 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3426 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3419 = _M0L6_2atmpS3426;
  _M0L6_2atmpS3422 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3423 = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS3424 = 0;
  _M0L6_2atmpS3425 = 0;
  _M0L6_2atmpS3421 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3421[0] = _M0L6_2atmpS3422;
  _M0L6_2atmpS3421[1] = _M0L6_2atmpS3423;
  _M0L6_2atmpS3421[2] = _M0L6_2atmpS3424;
  _M0L6_2atmpS3421[3] = _M0L6_2atmpS3425;
  _M0L6_2atmpS3420
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3420)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3420->$0 = _M0L6_2atmpS3421;
  _M0L6_2atmpS3420->$1 = 4;
  #line 28 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3418, _M0L6_2atmpS3419, (moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS3420);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test45____test__76657273696f6e5f746573742e6d6274__0(
  
) {
  struct moonbit_result_1 _tmp_4160;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v1S1322;
  struct _M0TPB6ToJson _M0L6_2atmpS3282;
  moonbit_string_t _M0L6_2atmpS3302;
  void* _M0L6_2atmpS3301;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3294;
  moonbit_string_t _M0L6_2atmpS3300;
  void* _M0L6_2atmpS3299;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3295;
  moonbit_string_t _M0L6_2atmpS3298;
  void* _M0L6_2atmpS3297;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3296;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1323;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3293;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3292;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3291;
  void* _M0L6_2atmpS3290;
  void* _M0L6_2atmpS3283;
  moonbit_string_t _M0L6_2atmpS3286;
  moonbit_string_t _M0L6_2atmpS3287;
  moonbit_string_t _M0L6_2atmpS3288;
  moonbit_string_t _M0L6_2atmpS3289;
  moonbit_string_t* _M0L6_2atmpS3285;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3284;
  struct moonbit_result_0 _tmp_4162;
  struct moonbit_result_1 _tmp_4164;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L2v2S1324;
  struct _M0TPB6ToJson _M0L6_2atmpS3305;
  moonbit_string_t _M0L6_2atmpS3335;
  void* _M0L6_2atmpS3334;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3317;
  moonbit_string_t _M0L6_2atmpS3333;
  void* _M0L6_2atmpS3332;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3318;
  moonbit_string_t _M0L6_2atmpS3331;
  void* _M0L6_2atmpS3330;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3319;
  moonbit_string_t _M0L6_2atmpS3329;
  void* _M0L6_2atmpS3328;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3325;
  void* _M0L6_2atmpS3327;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3326;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1326;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3324;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3323;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3322;
  void* _M0L6_2atmpS3321;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3320;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1325;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3316;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3315;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3314;
  void* _M0L6_2atmpS3313;
  void* _M0L6_2atmpS3306;
  moonbit_string_t _M0L6_2atmpS3309;
  moonbit_string_t _M0L6_2atmpS3310;
  moonbit_string_t _M0L6_2atmpS3311;
  moonbit_string_t _M0L6_2atmpS3312;
  moonbit_string_t* _M0L6_2atmpS3308;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3307;
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  #line 3 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4160
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_39.data);
  if (_tmp_4160.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3338 =
      _tmp_4160.data.ok;
    _M0L2v1S1322 = _M0L5_2aokS3338;
  } else {
    void* const _M0L6_2aerrS3339 = _tmp_4160.data.err;
    struct moonbit_result_0 _result_4161;
    _result_4161.tag = 0;
    _result_4161.data.err = _M0L6_2aerrS3339;
    return _result_4161;
  }
  _M0L6_2atmpS3282
  = (struct _M0TPB6ToJson){
    _M0FP0127clawteam_2fclawteam_2finternal_2fbuildinfo_2fVersion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L2v1S1322
  };
  _M0L6_2atmpS3302 = 0;
  #line 4 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3301 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3302);
  _M0L8_2atupleS3294
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3294)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3294->$0 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L8_2atupleS3294->$1 = _M0L6_2atmpS3301;
  _M0L6_2atmpS3300 = 0;
  #line 4 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3299 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3300);
  _M0L8_2atupleS3295
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3295)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3295->$0 = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L8_2atupleS3295->$1 = _M0L6_2atmpS3299;
  _M0L6_2atmpS3298 = 0;
  #line 4 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3297 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3298);
  _M0L8_2atupleS3296
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3296)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3296->$0 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L8_2atupleS3296->$1 = _M0L6_2atmpS3297;
  _M0L7_2abindS1323 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1323[0] = _M0L8_2atupleS3294;
  _M0L7_2abindS1323[1] = _M0L8_2atupleS3295;
  _M0L7_2abindS1323[2] = _M0L8_2atupleS3296;
  _M0L6_2atmpS3293 = _M0L7_2abindS1323;
  _M0L6_2atmpS3292
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3293
  };
  #line 4 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3291 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3292);
  #line 4 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3290 = _M0MPC14json4Json6object(_M0L6_2atmpS3291);
  _M0L6_2atmpS3283 = _M0L6_2atmpS3290;
  _M0L6_2atmpS3286 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3287 = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3288 = 0;
  _M0L6_2atmpS3289 = 0;
  _M0L6_2atmpS3285 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3285[0] = _M0L6_2atmpS3286;
  _M0L6_2atmpS3285[1] = _M0L6_2atmpS3287;
  _M0L6_2atmpS3285[2] = _M0L6_2atmpS3288;
  _M0L6_2atmpS3285[3] = _M0L6_2atmpS3289;
  _M0L6_2atmpS3284
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3284)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3284->$0 = _M0L6_2atmpS3285;
  _M0L6_2atmpS3284->$1 = 4;
  #line 4 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4162
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3282, _M0L6_2atmpS3283, (moonbit_string_t)moonbit_string_literal_45.data, _M0L6_2atmpS3284);
  if (_tmp_4162.tag) {
    int32_t const _M0L5_2aokS3303 = _tmp_4162.data.ok;
  } else {
    void* const _M0L6_2aerrS3304 = _tmp_4162.data.err;
    struct moonbit_result_0 _result_4163;
    _result_4163.tag = 0;
    _result_4163.data.err = _M0L6_2aerrS3304;
    return _result_4163;
  }
  #line 5 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _tmp_4164
  = _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse((moonbit_string_t)moonbit_string_literal_46.data);
  if (_tmp_4164.tag) {
    struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* const _M0L5_2aokS3336 =
      _tmp_4164.data.ok;
    _M0L2v2S1324 = _M0L5_2aokS3336;
  } else {
    void* const _M0L6_2aerrS3337 = _tmp_4164.data.err;
    struct moonbit_result_0 _result_4165;
    _result_4165.tag = 0;
    _result_4165.data.err = _M0L6_2aerrS3337;
    return _result_4165;
  }
  _M0L6_2atmpS3305
  = (struct _M0TPB6ToJson){
    _M0FP0127clawteam_2fclawteam_2finternal_2fbuildinfo_2fVersion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L2v2S1324
  };
  _M0L6_2atmpS3335 = 0;
  #line 7 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3334 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS3335);
  _M0L8_2atupleS3317
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3317)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3317->$0 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L8_2atupleS3317->$1 = _M0L6_2atmpS3334;
  _M0L6_2atmpS3333 = 0;
  #line 8 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3332 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS3333);
  _M0L8_2atupleS3318
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3318)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3318->$0 = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L8_2atupleS3318->$1 = _M0L6_2atmpS3332;
  _M0L6_2atmpS3331 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3330 = _M0MPC14json4Json6number(0x1.8p+1, _M0L6_2atmpS3331);
  _M0L8_2atupleS3319
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3319)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3319->$0 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L8_2atupleS3319->$1 = _M0L6_2atmpS3330;
  _M0L6_2atmpS3329 = 0;
  #line 10 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3328 = _M0MPC14json4Json6number(0x1.68p+5, _M0L6_2atmpS3329);
  _M0L8_2atupleS3325
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3325)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3325->$0 = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L8_2atupleS3325->$1 = _M0L6_2atmpS3328;
  #line 10 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3327
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_48.data);
  _M0L8_2atupleS3326
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3326)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3326->$0 = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L8_2atupleS3326->$1 = _M0L6_2atmpS3327;
  _M0L7_2abindS1326 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1326[0] = _M0L8_2atupleS3325;
  _M0L7_2abindS1326[1] = _M0L8_2atupleS3326;
  _M0L6_2atmpS3324 = _M0L7_2abindS1326;
  _M0L6_2atmpS3323
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3324
  };
  #line 10 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3322 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3323);
  #line 10 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3321 = _M0MPC14json4Json6object(_M0L6_2atmpS3322);
  _M0L8_2atupleS3320
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3320)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3320->$0 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L8_2atupleS3320->$1 = _M0L6_2atmpS3321;
  _M0L7_2abindS1325 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1325[0] = _M0L8_2atupleS3317;
  _M0L7_2abindS1325[1] = _M0L8_2atupleS3318;
  _M0L7_2abindS1325[2] = _M0L8_2atupleS3319;
  _M0L7_2abindS1325[3] = _M0L8_2atupleS3320;
  _M0L6_2atmpS3316 = _M0L7_2abindS1325;
  _M0L6_2atmpS3315
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 4, _M0L6_2atmpS3316
  };
  #line 6 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3314 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3315);
  #line 6 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  _M0L6_2atmpS3313 = _M0MPC14json4Json6object(_M0L6_2atmpS3314);
  _M0L6_2atmpS3306 = _M0L6_2atmpS3313;
  _M0L6_2atmpS3309 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3310 = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS3311 = 0;
  _M0L6_2atmpS3312 = 0;
  _M0L6_2atmpS3308 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3308[0] = _M0L6_2atmpS3309;
  _M0L6_2atmpS3308[1] = _M0L6_2atmpS3310;
  _M0L6_2atmpS3308[2] = _M0L6_2atmpS3311;
  _M0L6_2atmpS3308[3] = _M0L6_2atmpS3312;
  _M0L6_2atmpS3307
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3307)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3307->$0 = _M0L6_2atmpS3308;
  _M0L6_2atmpS3307->$1 = 4;
  #line 6 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3305, _M0L6_2atmpS3306, (moonbit_string_t)moonbit_string_literal_53.data, _M0L6_2atmpS3307);
}

int32_t _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB2Eq5equal(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L4selfS1311,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L5otherS1312
) {
  int32_t _M0L5majorS3280;
  int32_t _M0L5majorS3281;
  #line 89 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L5majorS3280 = _M0L4selfS1311->$0;
  _M0L5majorS3281 = _M0L5otherS1312->$0;
  if (_M0L5majorS3280 == _M0L5majorS3281) {
    int32_t _M0L5minorS3278 = _M0L4selfS1311->$1;
    int32_t _M0L5minorS3279 = _M0L5otherS1312->$1;
    if (_M0L5minorS3278 == _M0L5minorS3279) {
      int32_t _M0L5patchS3276 = _M0L4selfS1311->$2;
      int32_t _M0L5patchS3277 = _M0L5otherS1312->$2;
      if (_M0L5patchS3276 == _M0L5patchS3277) {
        struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L2b1S1314;
        struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L2b2S1315;
        struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L8_2afieldS3652 =
          _M0L4selfS1311->$3;
        int32_t _M0L6_2acntS4009 = Moonbit_object_header(_M0L4selfS1311)->rc;
        struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2abindS1316;
        struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L8_2afieldS3651;
        int32_t _M0L6_2acntS4011;
        struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2abindS1317;
        int32_t _M0L8_2afieldS3649;
        int32_t _M0L6numberS3274;
        int32_t _M0L8_2afieldS3648;
        int32_t _M0L6numberS3275;
        if (_M0L6_2acntS4009 > 1) {
          int32_t _M0L11_2anew__cntS4010 = _M0L6_2acntS4009 - 1;
          Moonbit_object_header(_M0L4selfS1311)->rc = _M0L11_2anew__cntS4010;
          if (_M0L8_2afieldS3652) {
            moonbit_incref(_M0L8_2afieldS3652);
          }
        } else if (_M0L6_2acntS4009 == 1) {
          #line 93 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
          moonbit_free(_M0L4selfS1311);
        }
        _M0L7_2abindS1316 = _M0L8_2afieldS3652;
        _M0L8_2afieldS3651 = _M0L5otherS1312->$3;
        _M0L6_2acntS4011 = Moonbit_object_header(_M0L5otherS1312)->rc;
        if (_M0L6_2acntS4011 > 1) {
          int32_t _M0L11_2anew__cntS4012 = _M0L6_2acntS4011 - 1;
          Moonbit_object_header(_M0L5otherS1312)->rc = _M0L11_2anew__cntS4012;
          if (_M0L8_2afieldS3651) {
            moonbit_incref(_M0L8_2afieldS3651);
          }
        } else if (_M0L6_2acntS4011 == 1) {
          #line 93 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
          moonbit_free(_M0L5otherS1312);
        }
        _M0L7_2abindS1317 = _M0L8_2afieldS3651;
        if (_M0L7_2abindS1316 == 0) {
          int32_t _M0L6_2atmpS3650;
          if (_M0L7_2abindS1316) {
            moonbit_decref(_M0L7_2abindS1316);
          }
          _M0L6_2atmpS3650 = _M0L7_2abindS1317 == 0;
          if (_M0L7_2abindS1317) {
            moonbit_decref(_M0L7_2abindS1317);
          }
          return _M0L6_2atmpS3650;
        } else {
          struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2aSomeS1318 =
            _M0L7_2abindS1316;
          struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L5_2ab1S1319 =
            _M0L7_2aSomeS1318;
          if (_M0L7_2abindS1317 == 0) {
            moonbit_decref(_M0L5_2ab1S1319);
            if (_M0L7_2abindS1317) {
              moonbit_decref(_M0L7_2abindS1317);
            }
            return 0;
          } else {
            struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2aSomeS1320 =
              _M0L7_2abindS1317;
            struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L5_2ab2S1321 =
              _M0L7_2aSomeS1320;
            _M0L2b1S1314 = _M0L5_2ab1S1319;
            _M0L2b2S1315 = _M0L5_2ab2S1321;
            goto join_1313;
          }
        }
        join_1313:;
        _M0L8_2afieldS3649 = _M0L2b1S1314->$0;
        moonbit_decref(_M0L2b1S1314);
        _M0L6numberS3274 = _M0L8_2afieldS3649;
        _M0L8_2afieldS3648 = _M0L2b2S1315->$0;
        moonbit_decref(_M0L2b2S1315);
        _M0L6numberS3275 = _M0L8_2afieldS3648;
        return _M0L6numberS3274 == _M0L6numberS3275;
      } else {
        moonbit_decref(_M0L5otherS1312);
        moonbit_decref(_M0L4selfS1311);
        return 0;
      }
    } else {
      moonbit_decref(_M0L5otherS1312);
      moonbit_decref(_M0L4selfS1311);
      return 0;
    }
  } else {
    moonbit_decref(_M0L5otherS1312);
    moonbit_decref(_M0L4selfS1311);
    return 0;
  }
}

int32_t _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB7Compare7compare(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L4selfS1300,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L5otherS1301
) {
  int32_t _M0L5majorS3260;
  int32_t _M0L5majorS3261;
  int32_t _M0L5minorS3264;
  int32_t _M0L5minorS3265;
  int32_t _M0L5patchS3268;
  int32_t _M0L5patchS3269;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L2b1S1303;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L2b2S1304;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L8_2afieldS3657;
  int32_t _M0L6_2acntS4013;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2abindS1305;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L8_2afieldS3656;
  int32_t _M0L6_2acntS4015;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2abindS1306;
  int32_t _M0L8_2afieldS3654;
  int32_t _M0L6numberS3272;
  int32_t _M0L8_2afieldS3653;
  int32_t _M0L6numberS3273;
  #line 70 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L5majorS3260 = _M0L4selfS1300->$0;
  _M0L5majorS3261 = _M0L5otherS1301->$0;
  if (_M0L5majorS3260 != _M0L5majorS3261) {
    int32_t _M0L8_2afieldS3663 = _M0L4selfS1300->$0;
    int32_t _M0L5majorS3262;
    int32_t _M0L8_2afieldS3662;
    int32_t _M0L5majorS3263;
    moonbit_decref(_M0L4selfS1300);
    _M0L5majorS3262 = _M0L8_2afieldS3663;
    _M0L8_2afieldS3662 = _M0L5otherS1301->$0;
    moonbit_decref(_M0L5otherS1301);
    _M0L5majorS3263 = _M0L8_2afieldS3662;
    return (_M0L5majorS3262 >= _M0L5majorS3263)
           - (_M0L5majorS3262 <= _M0L5majorS3263);
  }
  _M0L5minorS3264 = _M0L4selfS1300->$1;
  _M0L5minorS3265 = _M0L5otherS1301->$1;
  if (_M0L5minorS3264 != _M0L5minorS3265) {
    int32_t _M0L8_2afieldS3661 = _M0L4selfS1300->$1;
    int32_t _M0L5minorS3266;
    int32_t _M0L8_2afieldS3660;
    int32_t _M0L5minorS3267;
    moonbit_decref(_M0L4selfS1300);
    _M0L5minorS3266 = _M0L8_2afieldS3661;
    _M0L8_2afieldS3660 = _M0L5otherS1301->$1;
    moonbit_decref(_M0L5otherS1301);
    _M0L5minorS3267 = _M0L8_2afieldS3660;
    return (_M0L5minorS3266 >= _M0L5minorS3267)
           - (_M0L5minorS3266 <= _M0L5minorS3267);
  }
  _M0L5patchS3268 = _M0L4selfS1300->$2;
  _M0L5patchS3269 = _M0L5otherS1301->$2;
  if (_M0L5patchS3268 != _M0L5patchS3269) {
    int32_t _M0L8_2afieldS3659 = _M0L4selfS1300->$2;
    int32_t _M0L5patchS3270;
    int32_t _M0L8_2afieldS3658;
    int32_t _M0L5patchS3271;
    moonbit_decref(_M0L4selfS1300);
    _M0L5patchS3270 = _M0L8_2afieldS3659;
    _M0L8_2afieldS3658 = _M0L5otherS1301->$2;
    moonbit_decref(_M0L5otherS1301);
    _M0L5patchS3271 = _M0L8_2afieldS3658;
    return (_M0L5patchS3270 >= _M0L5patchS3271)
           - (_M0L5patchS3270 <= _M0L5patchS3271);
  }
  _M0L8_2afieldS3657 = _M0L4selfS1300->$3;
  _M0L6_2acntS4013 = Moonbit_object_header(_M0L4selfS1300)->rc;
  if (_M0L6_2acntS4013 > 1) {
    int32_t _M0L11_2anew__cntS4014 = _M0L6_2acntS4013 - 1;
    Moonbit_object_header(_M0L4selfS1300)->rc = _M0L11_2anew__cntS4014;
    if (_M0L8_2afieldS3657) {
      moonbit_incref(_M0L8_2afieldS3657);
    }
  } else if (_M0L6_2acntS4013 == 1) {
    #line 80 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
    moonbit_free(_M0L4selfS1300);
  }
  _M0L7_2abindS1305 = _M0L8_2afieldS3657;
  _M0L8_2afieldS3656 = _M0L5otherS1301->$3;
  _M0L6_2acntS4015 = Moonbit_object_header(_M0L5otherS1301)->rc;
  if (_M0L6_2acntS4015 > 1) {
    int32_t _M0L11_2anew__cntS4016 = _M0L6_2acntS4015 - 1;
    Moonbit_object_header(_M0L5otherS1301)->rc = _M0L11_2anew__cntS4016;
    if (_M0L8_2afieldS3656) {
      moonbit_incref(_M0L8_2afieldS3656);
    }
  } else if (_M0L6_2acntS4015 == 1) {
    #line 80 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
    moonbit_free(_M0L5otherS1301);
  }
  _M0L7_2abindS1306 = _M0L8_2afieldS3656;
  if (_M0L7_2abindS1305 == 0) {
    int32_t _M0L6_2atmpS3655;
    if (_M0L7_2abindS1305) {
      moonbit_decref(_M0L7_2abindS1305);
    }
    _M0L6_2atmpS3655 = _M0L7_2abindS1306 == 0;
    if (_M0L7_2abindS1306) {
      moonbit_decref(_M0L7_2abindS1306);
    }
    if (_M0L6_2atmpS3655) {
      return 0;
    } else {
      return -1;
    }
  } else {
    struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2aSomeS1307 =
      _M0L7_2abindS1305;
    if (_M0L7_2abindS1306 == 0) {
      if (_M0L7_2aSomeS1307) {
        moonbit_decref(_M0L7_2aSomeS1307);
      }
      if (_M0L7_2abindS1306) {
        moonbit_decref(_M0L7_2abindS1306);
      }
      return 1;
    } else {
      struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L5_2ab1S1308 =
        _M0L7_2aSomeS1307;
      struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2aSomeS1309 =
        _M0L7_2abindS1306;
      struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L5_2ab2S1310 =
        _M0L7_2aSomeS1309;
      _M0L2b1S1303 = _M0L5_2ab1S1308;
      _M0L2b2S1304 = _M0L5_2ab2S1310;
      goto join_1302;
    }
  }
  join_1302:;
  _M0L8_2afieldS3654 = _M0L2b1S1303->$0;
  moonbit_decref(_M0L2b1S1303);
  _M0L6numberS3272 = _M0L8_2afieldS3654;
  _M0L8_2afieldS3653 = _M0L2b2S1304->$0;
  moonbit_decref(_M0L2b2S1304);
  _M0L6numberS3273 = _M0L8_2afieldS3653;
  return (_M0L6numberS3272 >= _M0L6numberS3273)
         - (_M0L6numberS3272 <= _M0L6numberS3273);
}

struct moonbit_result_1 _M0MP48clawteam8clawteam8internal9buildinfo7Version5parse(
  moonbit_string_t _M0L6stringS1246
) {
  int32_t _M0L6_2atmpS3259;
  struct _M0TPC16string10StringView _M0L7_2abindS1245;
  moonbit_string_t _M0L7_2adataS1247;
  int32_t _M0L8_2astartS1248;
  int32_t _M0L6_2atmpS3258;
  int32_t _M0L6_2aendS1249;
  int32_t _M0Lm9_2acursorS1250;
  int32_t _M0Lm13accept__stateS1251;
  int32_t _M0Lm10match__endS1252;
  int32_t _M0Lm20match__tag__saver__0S1253;
  int32_t _M0Lm20match__tag__saver__1S1254;
  int32_t _M0Lm20match__tag__saver__2S1255;
  int32_t _M0Lm20match__tag__saver__3S1256;
  int32_t _M0Lm6tag__1S1259;
  int32_t _M0Lm6tag__0S1260;
  int32_t _M0Lm6tag__3S1261;
  int32_t _M0Lm6tag__2S1262;
  int32_t _M0Lm6tag__4S1263;
  int32_t _M0Lm6tag__5S1264;
  struct _M0TPC16string10StringView _M0L13build__numberS1266;
  struct _M0TPC16string10StringView _M0L6commitS1267;
  struct _M0TPC16string10StringView _M0L5majorS1268;
  struct _M0TPC16string10StringView _M0L5minorS1269;
  struct _M0TPC16string10StringView _M0L5patchS1270;
  struct _M0TPC16string10StringView _M0L5majorS1276;
  struct _M0TPC16string10StringView _M0L5minorS1277;
  struct _M0TPC16string10StringView _M0L5patchS1278;
  int32_t _M0L6_2atmpS3227;
  struct moonbit_result_0 _tmp_4179;
  int32_t _M0L5majorS1279;
  struct moonbit_result_0 _tmp_4181;
  int32_t _M0L5minorS1280;
  struct moonbit_result_0 _tmp_4183;
  int32_t _M0L5patchS1281;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L6_2atmpS3175;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L6_2atmpS3174;
  struct moonbit_result_1 _result_4185;
  struct moonbit_result_0 _tmp_4186;
  int32_t _M0L5majorS1271;
  struct moonbit_result_0 _tmp_4188;
  int32_t _M0L5minorS1272;
  struct moonbit_result_0 _tmp_4190;
  int32_t _M0L5patchS1273;
  struct moonbit_result_0 _tmp_4192;
  int32_t _M0L13build__numberS1274;
  moonbit_string_t _M0L6_2atmpS3165;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L6_2atmpS3164;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L6_2atmpS3163;
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L6_2atmpS3162;
  struct moonbit_result_1 _result_4194;
  #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3259 = Moonbit_array_length(_M0L6stringS1246);
  moonbit_incref(_M0L6stringS1246);
  _M0L7_2abindS1245
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3259, _M0L6stringS1246
  };
  moonbit_incref(_M0L7_2abindS1245.$0);
  #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L7_2adataS1247 = _M0MPC16string10StringView4data(_M0L7_2abindS1245);
  moonbit_incref(_M0L7_2abindS1245.$0);
  #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L8_2astartS1248
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS1245);
  #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3258 = _M0MPC16string10StringView6length(_M0L7_2abindS1245);
  _M0L6_2aendS1249 = _M0L8_2astartS1248 + _M0L6_2atmpS3258;
  _M0Lm9_2acursorS1250 = _M0L8_2astartS1248;
  _M0Lm13accept__stateS1251 = -1;
  _M0Lm10match__endS1252 = -1;
  _M0Lm20match__tag__saver__0S1253 = -1;
  _M0Lm20match__tag__saver__1S1254 = -1;
  _M0Lm20match__tag__saver__2S1255 = -1;
  _M0Lm20match__tag__saver__3S1256 = -1;
  _M0Lm6tag__1S1259 = -1;
  _M0Lm6tag__0S1260 = -1;
  _M0Lm6tag__3S1261 = -1;
  _M0Lm6tag__2S1262 = -1;
  _M0Lm6tag__4S1263 = -1;
  _M0Lm6tag__5S1264 = -1;
  _M0L6_2atmpS3227 = _M0Lm9_2acursorS1250;
  if (_M0L6_2atmpS3227 < _M0L6_2aendS1249) {
    int32_t _M0L6_2atmpS3257 = _M0Lm9_2acursorS1250;
    int32_t _M0L10next__charS1283;
    int32_t _M0L6_2atmpS3228;
    moonbit_incref(_M0L7_2adataS1247);
    #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
    _M0L10next__charS1283
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3257);
    _M0L6_2atmpS3228 = _M0Lm9_2acursorS1250;
    _M0Lm9_2acursorS1250 = _M0L6_2atmpS3228 + 1;
    if (_M0L10next__charS1283 >= 48 && _M0L10next__charS1283 <= 57) {
      struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L6_2atmpS3229;
      struct moonbit_result_1 _result_4178;
      while (1) {
        int32_t _M0L6_2atmpS3230;
        _M0Lm6tag__0S1260 = _M0Lm9_2acursorS1250;
        _M0Lm6tag__1S1259 = _M0Lm9_2acursorS1250;
        _M0L6_2atmpS3230 = _M0Lm9_2acursorS1250;
        if (_M0L6_2atmpS3230 < _M0L6_2aendS1249) {
          int32_t _M0L6_2atmpS3256 = _M0Lm9_2acursorS1250;
          int32_t _M0L10next__charS1284;
          int32_t _M0L6_2atmpS3231;
          moonbit_incref(_M0L7_2adataS1247);
          #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
          _M0L10next__charS1284
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3256);
          _M0L6_2atmpS3231 = _M0Lm9_2acursorS1250;
          _M0Lm9_2acursorS1250 = _M0L6_2atmpS3231 + 1;
          if (_M0L10next__charS1284 < 47) {
            if (_M0L10next__charS1284 < 46) {
              goto join_1282;
            } else {
              int32_t _M0L6_2atmpS3232 = _M0Lm9_2acursorS1250;
              if (_M0L6_2atmpS3232 < _M0L6_2aendS1249) {
                int32_t _M0L6_2atmpS3255 = _M0Lm9_2acursorS1250;
                int32_t _M0L10next__charS1285;
                int32_t _M0L6_2atmpS3233;
                moonbit_incref(_M0L7_2adataS1247);
                #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                _M0L10next__charS1285
                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3255);
                _M0L6_2atmpS3233 = _M0Lm9_2acursorS1250;
                _M0Lm9_2acursorS1250 = _M0L6_2atmpS3233 + 1;
                if (
                  _M0L10next__charS1285 >= 48 && _M0L10next__charS1285 <= 57
                ) {
                  while (1) {
                    int32_t _M0L6_2atmpS3234;
                    _M0Lm6tag__2S1262 = _M0Lm9_2acursorS1250;
                    _M0Lm6tag__3S1261 = _M0Lm9_2acursorS1250;
                    _M0L6_2atmpS3234 = _M0Lm9_2acursorS1250;
                    if (_M0L6_2atmpS3234 < _M0L6_2aendS1249) {
                      int32_t _M0L6_2atmpS3254 = _M0Lm9_2acursorS1250;
                      int32_t _M0L10next__charS1286;
                      int32_t _M0L6_2atmpS3235;
                      moonbit_incref(_M0L7_2adataS1247);
                      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                      _M0L10next__charS1286
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3254);
                      _M0L6_2atmpS3235 = _M0Lm9_2acursorS1250;
                      _M0Lm9_2acursorS1250 = _M0L6_2atmpS3235 + 1;
                      if (_M0L10next__charS1286 < 47) {
                        if (_M0L10next__charS1286 < 46) {
                          goto join_1282;
                        } else {
                          int32_t _M0L6_2atmpS3236 = _M0Lm9_2acursorS1250;
                          if (_M0L6_2atmpS3236 < _M0L6_2aendS1249) {
                            int32_t _M0L6_2atmpS3253 = _M0Lm9_2acursorS1250;
                            int32_t _M0L10next__charS1287;
                            int32_t _M0L6_2atmpS3237;
                            moonbit_incref(_M0L7_2adataS1247);
                            #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                            _M0L10next__charS1287
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3253);
                            _M0L6_2atmpS3237 = _M0Lm9_2acursorS1250;
                            _M0Lm9_2acursorS1250 = _M0L6_2atmpS3237 + 1;
                            if (
                              _M0L10next__charS1287 >= 48
                              && _M0L10next__charS1287 <= 57
                            ) {
                              while (1) {
                                int32_t _M0L6_2atmpS3238;
                                _M0Lm6tag__4S1263 = _M0Lm9_2acursorS1250;
                                _M0L6_2atmpS3238 = _M0Lm9_2acursorS1250;
                                if (_M0L6_2atmpS3238 < _M0L6_2aendS1249) {
                                  int32_t _M0L6_2atmpS3252 =
                                    _M0Lm9_2acursorS1250;
                                  int32_t _M0L10next__charS1288;
                                  int32_t _M0L6_2atmpS3239;
                                  moonbit_incref(_M0L7_2adataS1247);
                                  #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                                  _M0L10next__charS1288
                                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3252);
                                  _M0L6_2atmpS3239 = _M0Lm9_2acursorS1250;
                                  _M0Lm9_2acursorS1250 = _M0L6_2atmpS3239 + 1;
                                  if (_M0L10next__charS1288 < 44) {
                                    if (_M0L10next__charS1288 < 43) {
                                      goto join_1282;
                                    } else {
                                      int32_t _M0L6_2atmpS3240 =
                                        _M0Lm9_2acursorS1250;
                                      if (
                                        _M0L6_2atmpS3240 < _M0L6_2aendS1249
                                      ) {
                                        int32_t _M0L6_2atmpS3251 =
                                          _M0Lm9_2acursorS1250;
                                        int32_t _M0L10next__charS1289;
                                        int32_t _M0L6_2atmpS3241;
                                        moonbit_incref(_M0L7_2adataS1247);
                                        #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                                        _M0L10next__charS1289
                                        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3251);
                                        _M0L6_2atmpS3241
                                        = _M0Lm9_2acursorS1250;
                                        _M0Lm9_2acursorS1250
                                        = _M0L6_2atmpS3241 + 1;
                                        if (
                                          _M0L10next__charS1289 >= 48
                                          && _M0L10next__charS1289 <= 57
                                        ) {
                                          while (1) {
                                            int32_t _M0L6_2atmpS3242;
                                            _M0Lm6tag__5S1264
                                            = _M0Lm9_2acursorS1250;
                                            _M0L6_2atmpS3242
                                            = _M0Lm9_2acursorS1250;
                                            if (
                                              _M0L6_2atmpS3242
                                              < _M0L6_2aendS1249
                                            ) {
                                              int32_t _M0L6_2atmpS3250 =
                                                _M0Lm9_2acursorS1250;
                                              int32_t _M0L10next__charS1290;
                                              int32_t _M0L6_2atmpS3243;
                                              moonbit_incref(_M0L7_2adataS1247);
                                              #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                                              _M0L10next__charS1290
                                              = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3250);
                                              _M0L6_2atmpS3243
                                              = _M0Lm9_2acursorS1250;
                                              _M0Lm9_2acursorS1250
                                              = _M0L6_2atmpS3243 + 1;
                                              if (_M0L10next__charS1290 < 47) {
                                                if (
                                                  _M0L10next__charS1290 < 46
                                                ) {
                                                  goto join_1282;
                                                } else {
                                                  int32_t _M0L6_2atmpS3244 =
                                                    _M0Lm9_2acursorS1250;
                                                  if (
                                                    _M0L6_2atmpS3244
                                                    < _M0L6_2aendS1249
                                                  ) {
                                                    int32_t _M0L6_2atmpS3249 =
                                                      _M0Lm9_2acursorS1250;
                                                    int32_t _M0L10next__charS1295;
                                                    int32_t _M0L6_2atmpS3248;
                                                    moonbit_incref(_M0L7_2adataS1247);
                                                    #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                                                    _M0L10next__charS1295
                                                    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3249);
                                                    _M0L6_2atmpS3248
                                                    = _M0Lm9_2acursorS1250;
                                                    _M0Lm9_2acursorS1250
                                                    = _M0L6_2atmpS3248 + 1;
                                                    if (
                                                      _M0L10next__charS1295
                                                      < 65
                                                    ) {
                                                      if (
                                                        _M0L10next__charS1295
                                                        >= 48
                                                        && _M0L10next__charS1295
                                                           <= 57
                                                      ) {
                                                        goto join_1291;
                                                      } else {
                                                        goto join_1282;
                                                      }
                                                    } else if (
                                                             _M0L10next__charS1295
                                                             > 70
                                                           ) {
                                                      if (
                                                        _M0L10next__charS1295
                                                        >= 97
                                                        && _M0L10next__charS1295
                                                           <= 102
                                                      ) {
                                                        goto join_1291;
                                                      } else {
                                                        goto join_1282;
                                                      }
                                                    } else {
                                                      goto join_1291;
                                                    }
                                                    goto joinlet_4175;
                                                    join_1291:;
                                                    while (1) {
                                                      int32_t _M0L6_2atmpS3245 =
                                                        _M0Lm9_2acursorS1250;
                                                      if (
                                                        _M0L6_2atmpS3245
                                                        < _M0L6_2aendS1249
                                                      ) {
                                                        int32_t _M0L6_2atmpS3247 =
                                                          _M0Lm9_2acursorS1250;
                                                        int32_t _M0L10next__charS1294;
                                                        int32_t _M0L6_2atmpS3246;
                                                        moonbit_incref(_M0L7_2adataS1247);
                                                        #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
                                                        _M0L10next__charS1294
                                                        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1247, _M0L6_2atmpS3247);
                                                        _M0L6_2atmpS3246
                                                        = _M0Lm9_2acursorS1250;
                                                        _M0Lm9_2acursorS1250
                                                        = _M0L6_2atmpS3246
                                                          + 1;
                                                        if (
                                                          _M0L10next__charS1294
                                                          < 65
                                                        ) {
                                                          if (
                                                            _M0L10next__charS1294
                                                            >= 48
                                                            && _M0L10next__charS1294
                                                               <= 57
                                                          ) {
                                                            goto join_1292;
                                                          } else {
                                                            goto join_1282;
                                                          }
                                                        } else if (
                                                                 _M0L10next__charS1294
                                                                 > 70
                                                               ) {
                                                          if (
                                                            _M0L10next__charS1294
                                                            >= 97
                                                            && _M0L10next__charS1294
                                                               <= 102
                                                          ) {
                                                            goto join_1292;
                                                          } else {
                                                            goto join_1282;
                                                          }
                                                        } else {
                                                          goto join_1292;
                                                        }
                                                        goto joinlet_4177;
                                                        join_1292:;
                                                        continue;
                                                        joinlet_4177:;
                                                      } else {
                                                        _M0Lm20match__tag__saver__0S1253
                                                        = _M0Lm6tag__1S1259;
                                                        _M0Lm20match__tag__saver__1S1254
                                                        = _M0Lm6tag__3S1261;
                                                        _M0Lm20match__tag__saver__2S1255
                                                        = _M0Lm6tag__4S1263;
                                                        _M0Lm20match__tag__saver__3S1256
                                                        = _M0Lm6tag__5S1264;
                                                        _M0Lm13accept__stateS1251
                                                        = 1;
                                                        _M0Lm10match__endS1252
                                                        = _M0Lm9_2acursorS1250;
                                                        goto join_1282;
                                                      }
                                                      break;
                                                    }
                                                    joinlet_4175:;
                                                  } else {
                                                    goto join_1282;
                                                  }
                                                }
                                              } else if (
                                                       _M0L10next__charS1290
                                                       > 47
                                                     ) {
                                                if (
                                                  _M0L10next__charS1290 < 58
                                                ) {
                                                  continue;
                                                } else {
                                                  goto join_1282;
                                                }
                                              } else {
                                                goto join_1282;
                                              }
                                            } else {
                                              goto join_1282;
                                            }
                                            break;
                                          }
                                        } else {
                                          goto join_1282;
                                        }
                                      } else {
                                        goto join_1282;
                                      }
                                    }
                                  } else if (_M0L10next__charS1288 > 47) {
                                    if (_M0L10next__charS1288 < 58) {
                                      continue;
                                    } else {
                                      goto join_1282;
                                    }
                                  } else {
                                    goto join_1282;
                                  }
                                } else {
                                  _M0Lm20match__tag__saver__0S1253
                                  = _M0Lm6tag__0S1260;
                                  _M0Lm20match__tag__saver__1S1254
                                  = _M0Lm6tag__2S1262;
                                  _M0Lm13accept__stateS1251 = 0;
                                  _M0Lm10match__endS1252
                                  = _M0Lm9_2acursorS1250;
                                  goto join_1282;
                                }
                                break;
                              }
                            } else {
                              goto join_1282;
                            }
                          } else {
                            goto join_1282;
                          }
                        }
                      } else if (_M0L10next__charS1286 > 47) {
                        if (_M0L10next__charS1286 < 58) {
                          continue;
                        } else {
                          goto join_1282;
                        }
                      } else {
                        goto join_1282;
                      }
                    } else {
                      goto join_1282;
                    }
                    break;
                  }
                } else {
                  goto join_1282;
                }
              } else {
                goto join_1282;
              }
            }
          } else if (_M0L10next__charS1284 > 47) {
            if (_M0L10next__charS1284 < 58) {
              continue;
            } else {
              goto join_1282;
            }
          } else {
            goto join_1282;
          }
        } else {
          goto join_1282;
        }
        break;
      }
      _result_4178.tag = 1;
      _result_4178.data.ok = _M0L6_2atmpS3229;
      return _result_4178;
    } else {
      goto join_1282;
    }
  } else {
    goto join_1282;
  }
  join_1282:;
  switch (_M0Lm13accept__stateS1251) {
    case 0: {
      int64_t _M0L6_2atmpS3195;
      int32_t _M0L6_2atmpS3197;
      int64_t _M0L6_2atmpS3196;
      struct _M0TPC16string10StringView _M0L6_2atmpS3182;
      int32_t _M0L6_2atmpS3194;
      int32_t _M0L6_2atmpS3193;
      int64_t _M0L6_2atmpS3190;
      int32_t _M0L6_2atmpS3192;
      int64_t _M0L6_2atmpS3191;
      struct _M0TPC16string10StringView _M0L6_2atmpS3183;
      int32_t _M0L6_2atmpS3189;
      int32_t _M0L6_2atmpS3188;
      int64_t _M0L6_2atmpS3185;
      int32_t _M0L6_2atmpS3187;
      int64_t _M0L6_2atmpS3186;
      struct _M0TPC16string10StringView _M0L6_2atmpS3184;
      moonbit_decref(_M0L6stringS1246);
      _M0L6_2atmpS3195 = (int64_t)_M0L8_2astartS1248;
      _M0L6_2atmpS3197 = _M0Lm20match__tag__saver__0S1253;
      _M0L6_2atmpS3196 = (int64_t)_M0L6_2atmpS3197;
      moonbit_incref(_M0L7_2adataS1247);
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3182
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3195, _M0L6_2atmpS3196);
      _M0L6_2atmpS3194 = _M0Lm20match__tag__saver__0S1253;
      _M0L6_2atmpS3193 = _M0L6_2atmpS3194 + 1;
      _M0L6_2atmpS3190 = (int64_t)_M0L6_2atmpS3193;
      _M0L6_2atmpS3192 = _M0Lm20match__tag__saver__1S1254;
      _M0L6_2atmpS3191 = (int64_t)_M0L6_2atmpS3192;
      moonbit_incref(_M0L7_2adataS1247);
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3183
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3190, _M0L6_2atmpS3191);
      _M0L6_2atmpS3189 = _M0Lm20match__tag__saver__1S1254;
      _M0L6_2atmpS3188 = _M0L6_2atmpS3189 + 1;
      _M0L6_2atmpS3185 = (int64_t)_M0L6_2atmpS3188;
      _M0L6_2atmpS3187 = _M0Lm10match__endS1252;
      _M0L6_2atmpS3186 = (int64_t)_M0L6_2atmpS3187;
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3184
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3185, _M0L6_2atmpS3186);
      _M0L5majorS1276 = _M0L6_2atmpS3182;
      _M0L5minorS1277 = _M0L6_2atmpS3183;
      _M0L5patchS1278 = _M0L6_2atmpS3184;
      goto join_1275;
      break;
    }
    
    case 1: {
      int32_t _M0L6_2atmpS3225;
      int32_t _M0L6_2atmpS3224;
      int64_t _M0L6_2atmpS3221;
      int32_t _M0L6_2atmpS3223;
      int64_t _M0L6_2atmpS3222;
      struct _M0TPC16string10StringView _M0L6_2atmpS3198;
      int32_t _M0L6_2atmpS3220;
      int32_t _M0L6_2atmpS3219;
      int64_t _M0L6_2atmpS3216;
      int32_t _M0L6_2atmpS3218;
      int64_t _M0L6_2atmpS3217;
      struct _M0TPC16string10StringView _M0L6_2atmpS3199;
      int64_t _M0L6_2atmpS3213;
      int32_t _M0L6_2atmpS3215;
      int64_t _M0L6_2atmpS3214;
      struct _M0TPC16string10StringView _M0L6_2atmpS3200;
      int32_t _M0L6_2atmpS3212;
      int32_t _M0L6_2atmpS3211;
      int64_t _M0L6_2atmpS3208;
      int32_t _M0L6_2atmpS3210;
      int64_t _M0L6_2atmpS3209;
      struct _M0TPC16string10StringView _M0L6_2atmpS3201;
      int32_t _M0L6_2atmpS3207;
      int32_t _M0L6_2atmpS3206;
      int64_t _M0L6_2atmpS3203;
      int32_t _M0L6_2atmpS3205;
      int64_t _M0L6_2atmpS3204;
      struct _M0TPC16string10StringView _M0L6_2atmpS3202;
      moonbit_decref(_M0L6stringS1246);
      _M0L6_2atmpS3225 = _M0Lm20match__tag__saver__2S1255;
      _M0L6_2atmpS3224 = _M0L6_2atmpS3225 + 1;
      _M0L6_2atmpS3221 = (int64_t)_M0L6_2atmpS3224;
      _M0L6_2atmpS3223 = _M0Lm20match__tag__saver__3S1256;
      _M0L6_2atmpS3222 = (int64_t)_M0L6_2atmpS3223;
      moonbit_incref(_M0L7_2adataS1247);
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3198
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3221, _M0L6_2atmpS3222);
      _M0L6_2atmpS3220 = _M0Lm20match__tag__saver__3S1256;
      _M0L6_2atmpS3219 = _M0L6_2atmpS3220 + 1;
      _M0L6_2atmpS3216 = (int64_t)_M0L6_2atmpS3219;
      _M0L6_2atmpS3218 = _M0Lm10match__endS1252;
      _M0L6_2atmpS3217 = (int64_t)_M0L6_2atmpS3218;
      moonbit_incref(_M0L7_2adataS1247);
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3199
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3216, _M0L6_2atmpS3217);
      _M0L6_2atmpS3213 = (int64_t)_M0L8_2astartS1248;
      _M0L6_2atmpS3215 = _M0Lm20match__tag__saver__0S1253;
      _M0L6_2atmpS3214 = (int64_t)_M0L6_2atmpS3215;
      moonbit_incref(_M0L7_2adataS1247);
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3200
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3213, _M0L6_2atmpS3214);
      _M0L6_2atmpS3212 = _M0Lm20match__tag__saver__0S1253;
      _M0L6_2atmpS3211 = _M0L6_2atmpS3212 + 1;
      _M0L6_2atmpS3208 = (int64_t)_M0L6_2atmpS3211;
      _M0L6_2atmpS3210 = _M0Lm20match__tag__saver__1S1254;
      _M0L6_2atmpS3209 = (int64_t)_M0L6_2atmpS3210;
      moonbit_incref(_M0L7_2adataS1247);
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3201
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3208, _M0L6_2atmpS3209);
      _M0L6_2atmpS3207 = _M0Lm20match__tag__saver__1S1254;
      _M0L6_2atmpS3206 = _M0L6_2atmpS3207 + 1;
      _M0L6_2atmpS3203 = (int64_t)_M0L6_2atmpS3206;
      _M0L6_2atmpS3205 = _M0Lm20match__tag__saver__2S1255;
      _M0L6_2atmpS3204 = (int64_t)_M0L6_2atmpS3205;
      #line 29 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3202
      = _M0MPC16string6String4view(_M0L7_2adataS1247, _M0L6_2atmpS3203, _M0L6_2atmpS3204);
      _M0L13build__numberS1266 = _M0L6_2atmpS3198;
      _M0L6commitS1267 = _M0L6_2atmpS3199;
      _M0L5majorS1268 = _M0L6_2atmpS3200;
      _M0L5minorS1269 = _M0L6_2atmpS3201;
      _M0L5patchS1270 = _M0L6_2atmpS3202;
      goto join_1265;
      break;
    }
    default: {
      moonbit_string_t _M0L6_2atmpS3664;
      moonbit_string_t _M0L6_2atmpS3226;
      moonbit_decref(_M0L7_2adataS1247);
      #line 65 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      _M0L6_2atmpS3664
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_54.data, _M0L6stringS1246);
      moonbit_decref(_M0L6stringS1246);
      _M0L6_2atmpS3226 = _M0L6_2atmpS3664;
      #line 65 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
      return _M0FPB4failGRP48clawteam8clawteam8internal9buildinfo7VersionE(_M0L6_2atmpS3226, (moonbit_string_t)moonbit_string_literal_55.data);
      break;
    }
  }
  join_1275:;
  #line 38 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _tmp_4179 = _M0FPC17strconv18parse__int_2einner(_M0L5majorS1276, 0);
  if (_tmp_4179.tag) {
    int32_t const _M0L5_2aokS3180 = _tmp_4179.data.ok;
    _M0L5majorS1279 = _M0L5_2aokS3180;
  } else {
    void* const _M0L6_2aerrS3181 = _tmp_4179.data.err;
    struct moonbit_result_1 _result_4180;
    moonbit_decref(_M0L5patchS1278.$0);
    moonbit_decref(_M0L5minorS1277.$0);
    _result_4180.tag = 0;
    _result_4180.data.err = _M0L6_2aerrS3181;
    return _result_4180;
  }
  #line 39 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _tmp_4181 = _M0FPC17strconv18parse__int_2einner(_M0L5minorS1277, 0);
  if (_tmp_4181.tag) {
    int32_t const _M0L5_2aokS3178 = _tmp_4181.data.ok;
    _M0L5minorS1280 = _M0L5_2aokS3178;
  } else {
    void* const _M0L6_2aerrS3179 = _tmp_4181.data.err;
    struct moonbit_result_1 _result_4182;
    moonbit_decref(_M0L5patchS1278.$0);
    _result_4182.tag = 0;
    _result_4182.data.err = _M0L6_2aerrS3179;
    return _result_4182;
  }
  #line 40 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _tmp_4183 = _M0FPC17strconv18parse__int_2einner(_M0L5patchS1278, 0);
  if (_tmp_4183.tag) {
    int32_t const _M0L5_2aokS3176 = _tmp_4183.data.ok;
    _M0L5patchS1281 = _M0L5_2aokS3176;
  } else {
    void* const _M0L6_2aerrS3177 = _tmp_4183.data.err;
    struct moonbit_result_1 _result_4184;
    _result_4184.tag = 0;
    _result_4184.data.err = _M0L6_2aerrS3177;
    return _result_4184;
  }
  _M0L6_2atmpS3175 = 0;
  _M0L6_2atmpS3174
  = (struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal9buildinfo7Version));
  Moonbit_object_header(_M0L6_2atmpS3174)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal9buildinfo7Version, $3) >> 2, 1, 0);
  _M0L6_2atmpS3174->$0 = _M0L5majorS1279;
  _M0L6_2atmpS3174->$1 = _M0L5minorS1280;
  _M0L6_2atmpS3174->$2 = _M0L5patchS1281;
  _M0L6_2atmpS3174->$3 = _M0L6_2atmpS3175;
  _result_4185.tag = 1;
  _result_4185.data.ok = _M0L6_2atmpS3174;
  return _result_4185;
  join_1265:;
  #line 54 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _tmp_4186 = _M0FPC17strconv18parse__int_2einner(_M0L5majorS1268, 0);
  if (_tmp_4186.tag) {
    int32_t const _M0L5_2aokS3172 = _tmp_4186.data.ok;
    _M0L5majorS1271 = _M0L5_2aokS3172;
  } else {
    void* const _M0L6_2aerrS3173 = _tmp_4186.data.err;
    struct moonbit_result_1 _result_4187;
    moonbit_decref(_M0L5patchS1270.$0);
    moonbit_decref(_M0L5minorS1269.$0);
    moonbit_decref(_M0L6commitS1267.$0);
    moonbit_decref(_M0L13build__numberS1266.$0);
    _result_4187.tag = 0;
    _result_4187.data.err = _M0L6_2aerrS3173;
    return _result_4187;
  }
  #line 55 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _tmp_4188 = _M0FPC17strconv18parse__int_2einner(_M0L5minorS1269, 0);
  if (_tmp_4188.tag) {
    int32_t const _M0L5_2aokS3170 = _tmp_4188.data.ok;
    _M0L5minorS1272 = _M0L5_2aokS3170;
  } else {
    void* const _M0L6_2aerrS3171 = _tmp_4188.data.err;
    struct moonbit_result_1 _result_4189;
    moonbit_decref(_M0L5patchS1270.$0);
    moonbit_decref(_M0L6commitS1267.$0);
    moonbit_decref(_M0L13build__numberS1266.$0);
    _result_4189.tag = 0;
    _result_4189.data.err = _M0L6_2aerrS3171;
    return _result_4189;
  }
  #line 56 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _tmp_4190 = _M0FPC17strconv18parse__int_2einner(_M0L5patchS1270, 0);
  if (_tmp_4190.tag) {
    int32_t const _M0L5_2aokS3168 = _tmp_4190.data.ok;
    _M0L5patchS1273 = _M0L5_2aokS3168;
  } else {
    void* const _M0L6_2aerrS3169 = _tmp_4190.data.err;
    struct moonbit_result_1 _result_4191;
    moonbit_decref(_M0L6commitS1267.$0);
    moonbit_decref(_M0L13build__numberS1266.$0);
    _result_4191.tag = 0;
    _result_4191.data.err = _M0L6_2aerrS3169;
    return _result_4191;
  }
  #line 57 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _tmp_4192
  = _M0FPC17strconv18parse__int_2einner(_M0L13build__numberS1266, 0);
  if (_tmp_4192.tag) {
    int32_t const _M0L5_2aokS3166 = _tmp_4192.data.ok;
    _M0L13build__numberS1274 = _M0L5_2aokS3166;
  } else {
    void* const _M0L6_2aerrS3167 = _tmp_4192.data.err;
    struct moonbit_result_1 _result_4193;
    moonbit_decref(_M0L6commitS1267.$0);
    _result_4193.tag = 0;
    _result_4193.data.err = _M0L6_2aerrS3167;
    return _result_4193;
  }
  #line 62 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3165
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L6commitS1267);
  _M0L6_2atmpS3164
  = (struct _M0TP48clawteam8clawteam8internal9buildinfo5Build*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal9buildinfo5Build));
  Moonbit_object_header(_M0L6_2atmpS3164)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal9buildinfo5Build, $1) >> 2, 1, 0);
  _M0L6_2atmpS3164->$0 = _M0L13build__numberS1274;
  _M0L6_2atmpS3164->$1 = _M0L6_2atmpS3165;
  _M0L6_2atmpS3163 = _M0L6_2atmpS3164;
  _M0L6_2atmpS3162
  = (struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal9buildinfo7Version));
  Moonbit_object_header(_M0L6_2atmpS3162)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal9buildinfo7Version, $3) >> 2, 1, 0);
  _M0L6_2atmpS3162->$0 = _M0L5majorS1271;
  _M0L6_2atmpS3162->$1 = _M0L5minorS1272;
  _M0L6_2atmpS3162->$2 = _M0L5patchS1273;
  _M0L6_2atmpS3162->$3 = _M0L6_2atmpS3163;
  _result_4194.tag = 1;
  _result_4194.data.ok = _M0L6_2atmpS3162;
  return _result_4194;
}

void* _M0IP48clawteam8clawteam8internal9buildinfo5BuildPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L8_2ax__29S1244
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1243;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3161;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3160;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1242;
  int32_t _M0L6numberS3157;
  void* _M0L6_2atmpS3156;
  moonbit_string_t _M0L8_2afieldS3665;
  int32_t _M0L6_2acntS4017;
  moonbit_string_t _M0L6commitS3159;
  void* _M0L6_2atmpS3158;
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L7_2abindS1243 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3161 = _M0L7_2abindS1243;
  _M0L6_2atmpS3160
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3161
  };
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_24mapS1242 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3160);
  _M0L6numberS3157 = _M0L8_2ax__29S1244->$0;
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3156 = _M0IPC13int3IntPB6ToJson8to__json(_M0L6numberS3157);
  moonbit_incref(_M0L6_24mapS1242);
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1242, (moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS3156);
  _M0L8_2afieldS3665 = _M0L8_2ax__29S1244->$1;
  _M0L6_2acntS4017 = Moonbit_object_header(_M0L8_2ax__29S1244)->rc;
  if (_M0L6_2acntS4017 > 1) {
    int32_t _M0L11_2anew__cntS4018 = _M0L6_2acntS4017 - 1;
    Moonbit_object_header(_M0L8_2ax__29S1244)->rc = _M0L11_2anew__cntS4018;
    moonbit_incref(_M0L8_2afieldS3665);
  } else if (_M0L6_2acntS4017 == 1) {
    #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
    moonbit_free(_M0L8_2ax__29S1244);
  }
  _M0L6commitS3159 = _M0L8_2afieldS3665;
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3158
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L6commitS3159);
  moonbit_incref(_M0L6_24mapS1242);
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1242, (moonbit_string_t)moonbit_string_literal_49.data, _M0L6_2atmpS3158);
  #line 20 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1242);
}

void* _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L8_2ax__32S1236
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1235;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3155;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3154;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1234;
  int32_t _M0L5majorS3148;
  void* _M0L6_2atmpS3147;
  int32_t _M0L5minorS3150;
  void* _M0L6_2atmpS3149;
  int32_t _M0L5patchS3152;
  void* _M0L6_2atmpS3151;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L8_24innerS1238;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L8_2afieldS3666;
  int32_t _M0L6_2acntS4019;
  struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2abindS1239;
  void* _M0L6_2atmpS3153;
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L7_2abindS1235 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3155 = _M0L7_2abindS1235;
  _M0L6_2atmpS3154
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3155
  };
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_24mapS1234 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3154);
  _M0L5majorS3148 = _M0L8_2ax__32S1236->$0;
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3147 = _M0IPC13int3IntPB6ToJson8to__json(_M0L5majorS3148);
  moonbit_incref(_M0L6_24mapS1234);
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1234, (moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS3147);
  _M0L5minorS3150 = _M0L8_2ax__32S1236->$1;
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3149 = _M0IPC13int3IntPB6ToJson8to__json(_M0L5minorS3150);
  moonbit_incref(_M0L6_24mapS1234);
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1234, (moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS3149);
  _M0L5patchS3152 = _M0L8_2ax__32S1236->$2;
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3151 = _M0IPC13int3IntPB6ToJson8to__json(_M0L5patchS3152);
  moonbit_incref(_M0L6_24mapS1234);
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1234, (moonbit_string_t)moonbit_string_literal_42.data, _M0L6_2atmpS3151);
  _M0L8_2afieldS3666 = _M0L8_2ax__32S1236->$3;
  _M0L6_2acntS4019 = Moonbit_object_header(_M0L8_2ax__32S1236)->rc;
  if (_M0L6_2acntS4019 > 1) {
    int32_t _M0L11_2anew__cntS4020 = _M0L6_2acntS4019 - 1;
    Moonbit_object_header(_M0L8_2ax__32S1236)->rc = _M0L11_2anew__cntS4020;
    if (_M0L8_2afieldS3666) {
      moonbit_incref(_M0L8_2afieldS3666);
    }
  } else if (_M0L6_2acntS4019 == 1) {
    #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
    moonbit_free(_M0L8_2ax__32S1236);
  }
  _M0L7_2abindS1239 = _M0L8_2afieldS3666;
  if (_M0L7_2abindS1239 == 0) {
    if (_M0L7_2abindS1239) {
      moonbit_decref(_M0L7_2abindS1239);
    }
  } else {
    struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L7_2aSomeS1240 =
      _M0L7_2abindS1239;
    struct _M0TP48clawteam8clawteam8internal9buildinfo5Build* _M0L11_2a_24innerS1241 =
      _M0L7_2aSomeS1240;
    _M0L8_24innerS1238 = _M0L11_2a_24innerS1241;
    goto join_1237;
  }
  goto joinlet_4195;
  join_1237:;
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0L6_2atmpS3153
  = _M0IP48clawteam8clawteam8internal9buildinfo5BuildPB6ToJson8to__json(_M0L8_24innerS1238);
  moonbit_incref(_M0L6_24mapS1234);
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1234, (moonbit_string_t)moonbit_string_literal_50.data, _M0L6_2atmpS3153);
  joinlet_4195:;
  #line 2 "E:\\moonbit\\clawteam\\internal\\buildinfo\\version.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1234);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1229,
  void* _M0L7contentS1231,
  moonbit_string_t _M0L3locS1225,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1227
) {
  moonbit_string_t _M0L3locS1224;
  moonbit_string_t _M0L9args__locS1226;
  void* _M0L6_2atmpS3145;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3146;
  moonbit_string_t _M0L6actualS1228;
  moonbit_string_t _M0L4wantS1230;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1224 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1225);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1226 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1227);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3145 = _M0L3objS1229.$0->$method_0(_M0L3objS1229.$1);
  _M0L6_2atmpS3146 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1228
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3145, 0, 0, _M0L6_2atmpS3146);
  if (_M0L7contentS1231 == 0) {
    void* _M0L6_2atmpS3142;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3143;
    if (_M0L7contentS1231) {
      moonbit_decref(_M0L7contentS1231);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3142
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_3.data);
    _M0L6_2atmpS3143 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1230
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3142, 0, 0, _M0L6_2atmpS3143);
  } else {
    void* _M0L7_2aSomeS1232 = _M0L7contentS1231;
    void* _M0L4_2axS1233 = _M0L7_2aSomeS1232;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3144 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1230
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1233, 0, 0, _M0L6_2atmpS3144);
  }
  moonbit_incref(_M0L4wantS1230);
  moonbit_incref(_M0L6actualS1228);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1228, _M0L4wantS1230)
  ) {
    moonbit_string_t _M0L6_2atmpS3140;
    moonbit_string_t _M0L6_2atmpS3674;
    moonbit_string_t _M0L6_2atmpS3139;
    moonbit_string_t _M0L6_2atmpS3673;
    moonbit_string_t _M0L6_2atmpS3137;
    moonbit_string_t _M0L6_2atmpS3138;
    moonbit_string_t _M0L6_2atmpS3672;
    moonbit_string_t _M0L6_2atmpS3136;
    moonbit_string_t _M0L6_2atmpS3671;
    moonbit_string_t _M0L6_2atmpS3133;
    moonbit_string_t _M0L6_2atmpS3135;
    moonbit_string_t _M0L6_2atmpS3134;
    moonbit_string_t _M0L6_2atmpS3670;
    moonbit_string_t _M0L6_2atmpS3132;
    moonbit_string_t _M0L6_2atmpS3669;
    moonbit_string_t _M0L6_2atmpS3129;
    moonbit_string_t _M0L6_2atmpS3131;
    moonbit_string_t _M0L6_2atmpS3130;
    moonbit_string_t _M0L6_2atmpS3668;
    moonbit_string_t _M0L6_2atmpS3128;
    moonbit_string_t _M0L6_2atmpS3667;
    moonbit_string_t _M0L6_2atmpS3127;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3126;
    struct moonbit_result_0 _result_4196;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3140
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1224);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3674
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_56.data, _M0L6_2atmpS3140);
    moonbit_decref(_M0L6_2atmpS3140);
    _M0L6_2atmpS3139 = _M0L6_2atmpS3674;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3673
    = moonbit_add_string(_M0L6_2atmpS3139, (moonbit_string_t)moonbit_string_literal_57.data);
    moonbit_decref(_M0L6_2atmpS3139);
    _M0L6_2atmpS3137 = _M0L6_2atmpS3673;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3138
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1226);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3672 = moonbit_add_string(_M0L6_2atmpS3137, _M0L6_2atmpS3138);
    moonbit_decref(_M0L6_2atmpS3137);
    moonbit_decref(_M0L6_2atmpS3138);
    _M0L6_2atmpS3136 = _M0L6_2atmpS3672;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3671
    = moonbit_add_string(_M0L6_2atmpS3136, (moonbit_string_t)moonbit_string_literal_58.data);
    moonbit_decref(_M0L6_2atmpS3136);
    _M0L6_2atmpS3133 = _M0L6_2atmpS3671;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3135 = _M0MPC16string6String6escape(_M0L4wantS1230);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3134
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3135);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3670 = moonbit_add_string(_M0L6_2atmpS3133, _M0L6_2atmpS3134);
    moonbit_decref(_M0L6_2atmpS3133);
    moonbit_decref(_M0L6_2atmpS3134);
    _M0L6_2atmpS3132 = _M0L6_2atmpS3670;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3669
    = moonbit_add_string(_M0L6_2atmpS3132, (moonbit_string_t)moonbit_string_literal_59.data);
    moonbit_decref(_M0L6_2atmpS3132);
    _M0L6_2atmpS3129 = _M0L6_2atmpS3669;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3131 = _M0MPC16string6String6escape(_M0L6actualS1228);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3130
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3131);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3668 = moonbit_add_string(_M0L6_2atmpS3129, _M0L6_2atmpS3130);
    moonbit_decref(_M0L6_2atmpS3129);
    moonbit_decref(_M0L6_2atmpS3130);
    _M0L6_2atmpS3128 = _M0L6_2atmpS3668;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3667
    = moonbit_add_string(_M0L6_2atmpS3128, (moonbit_string_t)moonbit_string_literal_60.data);
    moonbit_decref(_M0L6_2atmpS3128);
    _M0L6_2atmpS3127 = _M0L6_2atmpS3667;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3126
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3126)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3126)->$0
    = _M0L6_2atmpS3127;
    _result_4196.tag = 0;
    _result_4196.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3126;
    return _result_4196;
  } else {
    int32_t _M0L6_2atmpS3141;
    struct moonbit_result_0 _result_4197;
    moonbit_decref(_M0L4wantS1230);
    moonbit_decref(_M0L6actualS1228);
    moonbit_decref(_M0L9args__locS1226);
    moonbit_decref(_M0L3locS1224);
    _M0L6_2atmpS3141 = 0;
    _result_4197.tag = 1;
    _result_4197.data.ok = _M0L6_2atmpS3141;
    return _result_4197;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1223,
  int32_t _M0L13escape__slashS1195,
  int32_t _M0L6indentS1190,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1216
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1182;
  void** _M0L6_2atmpS3125;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1183;
  int32_t _M0Lm5depthS1184;
  void* _M0L6_2atmpS3124;
  void* _M0L8_2aparamS1185;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1182 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS3125 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1183
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1183)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1183->$0 = _M0L6_2atmpS3125;
  _M0L5stackS1183->$1 = 0;
  _M0Lm5depthS1184 = 0;
  _M0L6_2atmpS3124 = _M0L4selfS1223;
  _M0L8_2aparamS1185 = _M0L6_2atmpS3124;
  _2aloop_1201:;
  while (1) {
    if (_M0L8_2aparamS1185 == 0) {
      int32_t _M0L3lenS3086;
      if (_M0L8_2aparamS1185) {
        moonbit_decref(_M0L8_2aparamS1185);
      }
      _M0L3lenS3086 = _M0L5stackS1183->$1;
      if (_M0L3lenS3086 == 0) {
        if (_M0L8replacerS1216) {
          moonbit_decref(_M0L8replacerS1216);
        }
        moonbit_decref(_M0L5stackS1183);
        break;
      } else {
        void** _M0L8_2afieldS3682 = _M0L5stackS1183->$0;
        void** _M0L3bufS3110 = _M0L8_2afieldS3682;
        int32_t _M0L3lenS3112 = _M0L5stackS1183->$1;
        int32_t _M0L6_2atmpS3111 = _M0L3lenS3112 - 1;
        void* _M0L6_2atmpS3681 = (void*)_M0L3bufS3110[_M0L6_2atmpS3111];
        void* _M0L4_2axS1202 = _M0L6_2atmpS3681;
        switch (Moonbit_object_tag(_M0L4_2axS1202)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1203 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1202;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3677 =
              _M0L8_2aArrayS1203->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1204 =
              _M0L8_2afieldS3677;
            int32_t _M0L4_2aiS1205 = _M0L8_2aArrayS1203->$1;
            int32_t _M0L3lenS3098 = _M0L6_2aarrS1204->$1;
            if (_M0L4_2aiS1205 < _M0L3lenS3098) {
              int32_t _if__result_4199;
              void** _M0L8_2afieldS3676;
              void** _M0L3bufS3104;
              void* _M0L6_2atmpS3675;
              void* _M0L7elementS1206;
              int32_t _M0L6_2atmpS3099;
              void* _M0L6_2atmpS3102;
              if (_M0L4_2aiS1205 < 0) {
                _if__result_4199 = 1;
              } else {
                int32_t _M0L3lenS3103 = _M0L6_2aarrS1204->$1;
                _if__result_4199 = _M0L4_2aiS1205 >= _M0L3lenS3103;
              }
              if (_if__result_4199) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3676 = _M0L6_2aarrS1204->$0;
              _M0L3bufS3104 = _M0L8_2afieldS3676;
              _M0L6_2atmpS3675 = (void*)_M0L3bufS3104[_M0L4_2aiS1205];
              _M0L7elementS1206 = _M0L6_2atmpS3675;
              _M0L6_2atmpS3099 = _M0L4_2aiS1205 + 1;
              _M0L8_2aArrayS1203->$1 = _M0L6_2atmpS3099;
              if (_M0L4_2aiS1205 > 0) {
                int32_t _M0L6_2atmpS3101;
                moonbit_string_t _M0L6_2atmpS3100;
                moonbit_incref(_M0L7elementS1206);
                moonbit_incref(_M0L3bufS1182);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 44);
                _M0L6_2atmpS3101 = _M0Lm5depthS1184;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3100
                = _M0FPC14json11indent__str(_M0L6_2atmpS3101, _M0L6indentS1190);
                moonbit_incref(_M0L3bufS1182);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3100);
              } else {
                moonbit_incref(_M0L7elementS1206);
              }
              _M0L6_2atmpS3102 = _M0L7elementS1206;
              _M0L8_2aparamS1185 = _M0L6_2atmpS3102;
              goto _2aloop_1201;
            } else {
              int32_t _M0L6_2atmpS3105 = _M0Lm5depthS1184;
              void* _M0L6_2atmpS3106;
              int32_t _M0L6_2atmpS3108;
              moonbit_string_t _M0L6_2atmpS3107;
              void* _M0L6_2atmpS3109;
              _M0Lm5depthS1184 = _M0L6_2atmpS3105 - 1;
              moonbit_incref(_M0L5stackS1183);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3106
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1183);
              if (_M0L6_2atmpS3106) {
                moonbit_decref(_M0L6_2atmpS3106);
              }
              _M0L6_2atmpS3108 = _M0Lm5depthS1184;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3107
              = _M0FPC14json11indent__str(_M0L6_2atmpS3108, _M0L6indentS1190);
              moonbit_incref(_M0L3bufS1182);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3107);
              moonbit_incref(_M0L3bufS1182);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 93);
              _M0L6_2atmpS3109 = 0;
              _M0L8_2aparamS1185 = _M0L6_2atmpS3109;
              goto _2aloop_1201;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1207 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1202;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3680 =
              _M0L9_2aObjectS1207->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1208 =
              _M0L8_2afieldS3680;
            int32_t _M0L8_2afirstS1209 = _M0L9_2aObjectS1207->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1210;
            moonbit_incref(_M0L11_2aiteratorS1208);
            moonbit_incref(_M0L9_2aObjectS1207);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1210
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1208);
            if (_M0L7_2abindS1210 == 0) {
              int32_t _M0L6_2atmpS3087;
              void* _M0L6_2atmpS3088;
              int32_t _M0L6_2atmpS3090;
              moonbit_string_t _M0L6_2atmpS3089;
              void* _M0L6_2atmpS3091;
              if (_M0L7_2abindS1210) {
                moonbit_decref(_M0L7_2abindS1210);
              }
              moonbit_decref(_M0L9_2aObjectS1207);
              _M0L6_2atmpS3087 = _M0Lm5depthS1184;
              _M0Lm5depthS1184 = _M0L6_2atmpS3087 - 1;
              moonbit_incref(_M0L5stackS1183);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3088
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1183);
              if (_M0L6_2atmpS3088) {
                moonbit_decref(_M0L6_2atmpS3088);
              }
              _M0L6_2atmpS3090 = _M0Lm5depthS1184;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3089
              = _M0FPC14json11indent__str(_M0L6_2atmpS3090, _M0L6indentS1190);
              moonbit_incref(_M0L3bufS1182);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3089);
              moonbit_incref(_M0L3bufS1182);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 125);
              _M0L6_2atmpS3091 = 0;
              _M0L8_2aparamS1185 = _M0L6_2atmpS3091;
              goto _2aloop_1201;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1211 = _M0L7_2abindS1210;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1212 = _M0L7_2aSomeS1211;
              moonbit_string_t _M0L8_2afieldS3679 = _M0L4_2axS1212->$0;
              moonbit_string_t _M0L4_2akS1213 = _M0L8_2afieldS3679;
              void* _M0L8_2afieldS3678 = _M0L4_2axS1212->$1;
              int32_t _M0L6_2acntS4021 =
                Moonbit_object_header(_M0L4_2axS1212)->rc;
              void* _M0L4_2avS1214;
              void* _M0Lm2v2S1215;
              moonbit_string_t _M0L6_2atmpS3095;
              void* _M0L6_2atmpS3097;
              void* _M0L6_2atmpS3096;
              if (_M0L6_2acntS4021 > 1) {
                int32_t _M0L11_2anew__cntS4022 = _M0L6_2acntS4021 - 1;
                Moonbit_object_header(_M0L4_2axS1212)->rc
                = _M0L11_2anew__cntS4022;
                moonbit_incref(_M0L8_2afieldS3678);
                moonbit_incref(_M0L4_2akS1213);
              } else if (_M0L6_2acntS4021 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1212);
              }
              _M0L4_2avS1214 = _M0L8_2afieldS3678;
              _M0Lm2v2S1215 = _M0L4_2avS1214;
              if (_M0L8replacerS1216 == 0) {
                moonbit_incref(_M0Lm2v2S1215);
                moonbit_decref(_M0L4_2avS1214);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1217 =
                  _M0L8replacerS1216;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1218 =
                  _M0L7_2aSomeS1217;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1219 =
                  _M0L11_2areplacerS1218;
                void* _M0L7_2abindS1220;
                moonbit_incref(_M0L7_2afuncS1219);
                moonbit_incref(_M0L4_2akS1213);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1220
                = _M0L7_2afuncS1219->code(_M0L7_2afuncS1219, _M0L4_2akS1213, _M0L4_2avS1214);
                if (_M0L7_2abindS1220 == 0) {
                  void* _M0L6_2atmpS3092;
                  if (_M0L7_2abindS1220) {
                    moonbit_decref(_M0L7_2abindS1220);
                  }
                  moonbit_decref(_M0L4_2akS1213);
                  moonbit_decref(_M0L9_2aObjectS1207);
                  _M0L6_2atmpS3092 = 0;
                  _M0L8_2aparamS1185 = _M0L6_2atmpS3092;
                  goto _2aloop_1201;
                } else {
                  void* _M0L7_2aSomeS1221 = _M0L7_2abindS1220;
                  void* _M0L4_2avS1222 = _M0L7_2aSomeS1221;
                  _M0Lm2v2S1215 = _M0L4_2avS1222;
                }
              }
              if (!_M0L8_2afirstS1209) {
                int32_t _M0L6_2atmpS3094;
                moonbit_string_t _M0L6_2atmpS3093;
                moonbit_incref(_M0L3bufS1182);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 44);
                _M0L6_2atmpS3094 = _M0Lm5depthS1184;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3093
                = _M0FPC14json11indent__str(_M0L6_2atmpS3094, _M0L6indentS1190);
                moonbit_incref(_M0L3bufS1182);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3093);
              }
              moonbit_incref(_M0L3bufS1182);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3095
              = _M0FPC14json6escape(_M0L4_2akS1213, _M0L13escape__slashS1195);
              moonbit_incref(_M0L3bufS1182);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3095);
              moonbit_incref(_M0L3bufS1182);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 34);
              moonbit_incref(_M0L3bufS1182);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 58);
              if (_M0L6indentS1190 > 0) {
                moonbit_incref(_M0L3bufS1182);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 32);
              }
              _M0L9_2aObjectS1207->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1207);
              _M0L6_2atmpS3097 = _M0Lm2v2S1215;
              _M0L6_2atmpS3096 = _M0L6_2atmpS3097;
              _M0L8_2aparamS1185 = _M0L6_2atmpS3096;
              goto _2aloop_1201;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1186 = _M0L8_2aparamS1185;
      void* _M0L8_2avalueS1187 = _M0L7_2aSomeS1186;
      void* _M0L6_2atmpS3123;
      switch (Moonbit_object_tag(_M0L8_2avalueS1187)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1188 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1187;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3683 =
            _M0L9_2aObjectS1188->$0;
          int32_t _M0L6_2acntS4023 =
            Moonbit_object_header(_M0L9_2aObjectS1188)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1189;
          if (_M0L6_2acntS4023 > 1) {
            int32_t _M0L11_2anew__cntS4024 = _M0L6_2acntS4023 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1188)->rc
            = _M0L11_2anew__cntS4024;
            moonbit_incref(_M0L8_2afieldS3683);
          } else if (_M0L6_2acntS4023 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1188);
          }
          _M0L10_2amembersS1189 = _M0L8_2afieldS3683;
          moonbit_incref(_M0L10_2amembersS1189);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1189)) {
            moonbit_decref(_M0L10_2amembersS1189);
            moonbit_incref(_M0L3bufS1182);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, (moonbit_string_t)moonbit_string_literal_61.data);
          } else {
            int32_t _M0L6_2atmpS3118 = _M0Lm5depthS1184;
            int32_t _M0L6_2atmpS3120;
            moonbit_string_t _M0L6_2atmpS3119;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS3122;
            void* _M0L6ObjectS3121;
            _M0Lm5depthS1184 = _M0L6_2atmpS3118 + 1;
            moonbit_incref(_M0L3bufS1182);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 123);
            _M0L6_2atmpS3120 = _M0Lm5depthS1184;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3119
            = _M0FPC14json11indent__str(_M0L6_2atmpS3120, _M0L6indentS1190);
            moonbit_incref(_M0L3bufS1182);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3119);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3122
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1189);
            _M0L6ObjectS3121
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS3121)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3121)->$0
            = _M0L6_2atmpS3122;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3121)->$1
            = 1;
            moonbit_incref(_M0L5stackS1183);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1183, _M0L6ObjectS3121);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1191 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1187;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3684 =
            _M0L8_2aArrayS1191->$0;
          int32_t _M0L6_2acntS4025 =
            Moonbit_object_header(_M0L8_2aArrayS1191)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1192;
          if (_M0L6_2acntS4025 > 1) {
            int32_t _M0L11_2anew__cntS4026 = _M0L6_2acntS4025 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1191)->rc
            = _M0L11_2anew__cntS4026;
            moonbit_incref(_M0L8_2afieldS3684);
          } else if (_M0L6_2acntS4025 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1191);
          }
          _M0L6_2aarrS1192 = _M0L8_2afieldS3684;
          moonbit_incref(_M0L6_2aarrS1192);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1192)) {
            moonbit_decref(_M0L6_2aarrS1192);
            moonbit_incref(_M0L3bufS1182);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, (moonbit_string_t)moonbit_string_literal_62.data);
          } else {
            int32_t _M0L6_2atmpS3114 = _M0Lm5depthS1184;
            int32_t _M0L6_2atmpS3116;
            moonbit_string_t _M0L6_2atmpS3115;
            void* _M0L5ArrayS3117;
            _M0Lm5depthS1184 = _M0L6_2atmpS3114 + 1;
            moonbit_incref(_M0L3bufS1182);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 91);
            _M0L6_2atmpS3116 = _M0Lm5depthS1184;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3115
            = _M0FPC14json11indent__str(_M0L6_2atmpS3116, _M0L6indentS1190);
            moonbit_incref(_M0L3bufS1182);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3115);
            _M0L5ArrayS3117
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS3117)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3117)->$0
            = _M0L6_2aarrS1192;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3117)->$1
            = 0;
            moonbit_incref(_M0L5stackS1183);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1183, _M0L5ArrayS3117);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1193 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1187;
          moonbit_string_t _M0L8_2afieldS3685 = _M0L9_2aStringS1193->$0;
          int32_t _M0L6_2acntS4027 =
            Moonbit_object_header(_M0L9_2aStringS1193)->rc;
          moonbit_string_t _M0L4_2asS1194;
          moonbit_string_t _M0L6_2atmpS3113;
          if (_M0L6_2acntS4027 > 1) {
            int32_t _M0L11_2anew__cntS4028 = _M0L6_2acntS4027 - 1;
            Moonbit_object_header(_M0L9_2aStringS1193)->rc
            = _M0L11_2anew__cntS4028;
            moonbit_incref(_M0L8_2afieldS3685);
          } else if (_M0L6_2acntS4027 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1193);
          }
          _M0L4_2asS1194 = _M0L8_2afieldS3685;
          moonbit_incref(_M0L3bufS1182);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3113
          = _M0FPC14json6escape(_M0L4_2asS1194, _M0L13escape__slashS1195);
          moonbit_incref(_M0L3bufS1182);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L6_2atmpS3113);
          moonbit_incref(_M0L3bufS1182);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1182, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1196 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1187;
          double _M0L4_2anS1197 = _M0L9_2aNumberS1196->$0;
          moonbit_string_t _M0L8_2afieldS3686 = _M0L9_2aNumberS1196->$1;
          int32_t _M0L6_2acntS4029 =
            Moonbit_object_header(_M0L9_2aNumberS1196)->rc;
          moonbit_string_t _M0L7_2areprS1198;
          if (_M0L6_2acntS4029 > 1) {
            int32_t _M0L11_2anew__cntS4030 = _M0L6_2acntS4029 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1196)->rc
            = _M0L11_2anew__cntS4030;
            if (_M0L8_2afieldS3686) {
              moonbit_incref(_M0L8_2afieldS3686);
            }
          } else if (_M0L6_2acntS4029 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1196);
          }
          _M0L7_2areprS1198 = _M0L8_2afieldS3686;
          if (_M0L7_2areprS1198 == 0) {
            if (_M0L7_2areprS1198) {
              moonbit_decref(_M0L7_2areprS1198);
            }
            moonbit_incref(_M0L3bufS1182);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1182, _M0L4_2anS1197);
          } else {
            moonbit_string_t _M0L7_2aSomeS1199 = _M0L7_2areprS1198;
            moonbit_string_t _M0L4_2arS1200 = _M0L7_2aSomeS1199;
            moonbit_incref(_M0L3bufS1182);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, _M0L4_2arS1200);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1182);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, (moonbit_string_t)moonbit_string_literal_63.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1182);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, (moonbit_string_t)moonbit_string_literal_64.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1187);
          moonbit_incref(_M0L3bufS1182);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1182, (moonbit_string_t)moonbit_string_literal_65.data);
          break;
        }
      }
      _M0L6_2atmpS3123 = 0;
      _M0L8_2aparamS1185 = _M0L6_2atmpS3123;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1182);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1181,
  int32_t _M0L6indentS1179
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1179 == 0) {
    return (moonbit_string_t)moonbit_string_literal_3.data;
  } else {
    int32_t _M0L6spacesS1180 = _M0L6indentS1179 * _M0L5levelS1181;
    switch (_M0L6spacesS1180) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_66.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_67.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_68.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_69.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_70.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_71.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_72.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_73.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_74.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS3085;
        moonbit_string_t _M0L6_2atmpS3687;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3085
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_75.data, _M0L6spacesS1180);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3687
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS3085);
        moonbit_decref(_M0L6_2atmpS3085);
        return _M0L6_2atmpS3687;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1171,
  int32_t _M0L13escape__slashS1176
) {
  int32_t _M0L6_2atmpS3084;
  struct _M0TPB13StringBuilder* _M0L3bufS1170;
  struct _M0TWEOc* _M0L5_2aitS1172;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3084 = Moonbit_array_length(_M0L3strS1171);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1170 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3084);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1172 = _M0MPC16string6String4iter(_M0L3strS1171);
  while (1) {
    int32_t _M0L7_2abindS1173;
    moonbit_incref(_M0L5_2aitS1172);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1173 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1172);
    if (_M0L7_2abindS1173 == -1) {
      moonbit_decref(_M0L5_2aitS1172);
    } else {
      int32_t _M0L7_2aSomeS1174 = _M0L7_2abindS1173;
      int32_t _M0L4_2acS1175 = _M0L7_2aSomeS1174;
      if (_M0L4_2acS1175 == 34) {
        moonbit_incref(_M0L3bufS1170);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_76.data);
      } else if (_M0L4_2acS1175 == 92) {
        moonbit_incref(_M0L3bufS1170);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_77.data);
      } else if (_M0L4_2acS1175 == 47) {
        if (_M0L13escape__slashS1176) {
          moonbit_incref(_M0L3bufS1170);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_78.data);
        } else {
          moonbit_incref(_M0L3bufS1170);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1170, _M0L4_2acS1175);
        }
      } else if (_M0L4_2acS1175 == 10) {
        moonbit_incref(_M0L3bufS1170);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_79.data);
      } else if (_M0L4_2acS1175 == 13) {
        moonbit_incref(_M0L3bufS1170);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_80.data);
      } else if (_M0L4_2acS1175 == 8) {
        moonbit_incref(_M0L3bufS1170);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_81.data);
      } else if (_M0L4_2acS1175 == 9) {
        moonbit_incref(_M0L3bufS1170);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_82.data);
      } else {
        int32_t _M0L4codeS1177 = _M0L4_2acS1175;
        if (_M0L4codeS1177 == 12) {
          moonbit_incref(_M0L3bufS1170);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_83.data);
        } else if (_M0L4codeS1177 < 32) {
          int32_t _M0L6_2atmpS3083;
          moonbit_string_t _M0L6_2atmpS3082;
          moonbit_incref(_M0L3bufS1170);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, (moonbit_string_t)moonbit_string_literal_84.data);
          _M0L6_2atmpS3083 = _M0L4codeS1177 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3082 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS3083);
          moonbit_incref(_M0L3bufS1170);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1170, _M0L6_2atmpS3082);
        } else {
          moonbit_incref(_M0L3bufS1170);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1170, _M0L4_2acS1175);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1170);
}

int32_t _M0IPC17strconv12StrConvErrorPB4Show6output(
  void* _M0L4selfS1166,
  struct _M0TPB6Logger _M0L6loggerS1169
) {
  struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError* _M0L15_2aStrConvErrorS1167;
  moonbit_string_t _M0L8_2afieldS3688;
  int32_t _M0L6_2acntS4031;
  moonbit_string_t _M0L6_2aerrS1168;
  #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
  _M0L15_2aStrConvErrorS1167
  = (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError*)_M0L4selfS1166;
  _M0L8_2afieldS3688 = _M0L15_2aStrConvErrorS1167->$0;
  _M0L6_2acntS4031 = Moonbit_object_header(_M0L15_2aStrConvErrorS1167)->rc;
  if (_M0L6_2acntS4031 > 1) {
    int32_t _M0L11_2anew__cntS4032 = _M0L6_2acntS4031 - 1;
    Moonbit_object_header(_M0L15_2aStrConvErrorS1167)->rc
    = _M0L11_2anew__cntS4032;
    moonbit_incref(_M0L8_2afieldS3688);
  } else if (_M0L6_2acntS4031 == 1) {
    #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
    moonbit_free(_M0L15_2aStrConvErrorS1167);
  }
  _M0L6_2aerrS1168 = _M0L8_2afieldS3688;
  #line 24 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
  _M0L6loggerS1169.$0->$method_0(_M0L6loggerS1169.$1, _M0L6_2aerrS1168);
  return 0;
}

struct moonbit_result_0 _M0FPC17strconv18parse__int_2einner(
  struct _M0TPC16string10StringView _M0L3strS1164,
  int32_t _M0L4baseS1165
) {
  struct moonbit_result_2 _tmp_4201;
  int64_t _M0L1nS1163;
  int32_t _if__result_4203;
  int32_t _M0L6_2atmpS3079;
  struct moonbit_result_0 _result_4206;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
  _tmp_4201
  = _M0FPC17strconv20parse__int64_2einner(_M0L3strS1164, _M0L4baseS1165);
  if (_tmp_4201.tag) {
    int64_t const _M0L5_2aokS3080 = _tmp_4201.data.ok;
    _M0L1nS1163 = _M0L5_2aokS3080;
  } else {
    void* const _M0L6_2aerrS3081 = _tmp_4201.data.err;
    struct moonbit_result_0 _result_4202;
    _result_4202.tag = 0;
    _result_4202.data.err = _M0L6_2aerrS3081;
    return _result_4202;
  }
  if (_M0L1nS1163 < -2147483648ll) {
    _if__result_4203 = 1;
  } else {
    _if__result_4203 = _M0L1nS1163 > 2147483647ll;
  }
  if (_if__result_4203) {
    struct moonbit_result_0 _tmp_4204;
    #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    _tmp_4204 = _M0FPC17strconv10range__errGuE();
    if (_tmp_4204.tag) {
      int32_t const _M0L5_2aokS3077 = _tmp_4204.data.ok;
    } else {
      void* const _M0L6_2aerrS3078 = _tmp_4204.data.err;
      struct moonbit_result_0 _result_4205;
      _result_4205.tag = 0;
      _result_4205.data.err = _M0L6_2aerrS3078;
      return _result_4205;
    }
  }
  _M0L6_2atmpS3079 = (int32_t)_M0L1nS1163;
  _result_4206.tag = 1;
  _result_4206.data.ok = _M0L6_2atmpS3079;
  return _result_4206;
}

struct moonbit_result_2 _M0FPC17strconv20parse__int64_2einner(
  struct _M0TPC16string10StringView _M0L3strS1122,
  int32_t _M0L4baseS1135
) {
  int32_t _M0L6_2atmpS2961;
  struct _M0TPC16string10StringView _M0L6_2atmpS2960;
  #line 85 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
  _M0L6_2atmpS2961
  = Moonbit_array_length(_M0FPC17strconv20parse__int64_2einnerN7_2abindS543);
  moonbit_incref(_M0FPC17strconv20parse__int64_2einnerN7_2abindS543);
  _M0L6_2atmpS2960
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2961, _M0FPC17strconv20parse__int64_2einnerN7_2abindS543
  };
  moonbit_incref(_M0L3strS1122.$0);
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGRPC16string10StringViewE(_M0L3strS1122, _M0L6_2atmpS2960)
  ) {
    struct _M0TPC16string10StringView _M0L4restS1125;
    struct _M0TUbRPC16string10StringViewE* _M0L7_2abindS1123;
    struct _M0TPC16string10StringView _M0L7_2abindS1126;
    moonbit_string_t _M0L8_2afieldS3721;
    moonbit_string_t _M0L3strS3052;
    int32_t _M0L5startS3053;
    int32_t _M0L3endS3055;
    int64_t _M0L6_2atmpS3054;
    int32_t _M0L6_2anegS1132;
    struct _M0TPC16string10StringView _M0L8_2afieldS3712;
    int32_t _M0L6_2acntS4033;
    struct _M0TPC16string10StringView _M0L7_2arestS1133;
    struct moonbit_result_3 _tmp_4208;
    struct _M0TUiRPC16string10StringViewbE* _M0L7_2abindS1134;
    int32_t _M0L12_2anum__baseS1136;
    struct _M0TPC16string10StringView _M0L8_2afieldS3711;
    struct _M0TPC16string10StringView _M0L7_2arestS1137;
    int32_t _M0L8_2afieldS3710;
    int32_t _M0L6_2acntS4035;
    int32_t _M0L20_2aallow__underscoreS1138;
    int64_t _M0L19overflow__thresholdS1139;
    moonbit_string_t _M0L8_2afieldS3709;
    moonbit_string_t _M0L3strS3028;
    int32_t _M0L5startS3029;
    int32_t _M0L3endS3031;
    int64_t _M0L6_2atmpS3030;
    int32_t _M0L10has__digitS1140;
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    _M0L7_2abindS1126
    = _M0MPC16string10StringView12view_2einner(_M0L3strS1122, 0, 4294967296ll);
    _M0L8_2afieldS3721 = _M0L7_2abindS1126.$0;
    _M0L3strS3052 = _M0L8_2afieldS3721;
    _M0L5startS3053 = _M0L7_2abindS1126.$1;
    _M0L3endS3055 = _M0L7_2abindS1126.$2;
    _M0L6_2atmpS3054 = (int64_t)_M0L3endS3055;
    moonbit_incref(_M0L3strS3052);
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3052, 1, _M0L5startS3053, _M0L6_2atmpS3054)
    ) {
      moonbit_string_t _M0L8_2afieldS3720 = _M0L7_2abindS1126.$0;
      moonbit_string_t _M0L3strS3070 = _M0L8_2afieldS3720;
      moonbit_string_t _M0L8_2afieldS3719 = _M0L7_2abindS1126.$0;
      moonbit_string_t _M0L3strS3073 = _M0L8_2afieldS3719;
      int32_t _M0L5startS3074 = _M0L7_2abindS1126.$1;
      int32_t _M0L3endS3076 = _M0L7_2abindS1126.$2;
      int64_t _M0L6_2atmpS3075 = (int64_t)_M0L3endS3076;
      int64_t _M0L6_2atmpS3072;
      int32_t _M0L6_2atmpS3071;
      int32_t _M0L4_2axS1127;
      moonbit_incref(_M0L3strS3073);
      moonbit_incref(_M0L3strS3070);
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L6_2atmpS3072
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3073, 0, _M0L5startS3074, _M0L6_2atmpS3075);
      _M0L6_2atmpS3071 = (int32_t)_M0L6_2atmpS3072;
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L4_2axS1127
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3070, _M0L6_2atmpS3071);
      if (_M0L4_2axS1127 == 43) {
        moonbit_string_t _M0L8_2afieldS3715 = _M0L7_2abindS1126.$0;
        moonbit_string_t _M0L3strS3063 = _M0L8_2afieldS3715;
        moonbit_string_t _M0L8_2afieldS3714 = _M0L7_2abindS1126.$0;
        moonbit_string_t _M0L3strS3066 = _M0L8_2afieldS3714;
        int32_t _M0L5startS3067 = _M0L7_2abindS1126.$1;
        int32_t _M0L3endS3069 = _M0L7_2abindS1126.$2;
        int64_t _M0L6_2atmpS3068 = (int64_t)_M0L3endS3069;
        int64_t _M0L7_2abindS1490;
        int32_t _M0L6_2atmpS3064;
        int32_t _M0L8_2afieldS3713;
        int32_t _M0L3endS3065;
        struct _M0TPC16string10StringView _M0L4_2axS1128;
        moonbit_incref(_M0L3strS3066);
        moonbit_incref(_M0L3strS3063);
        #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        _M0L7_2abindS1490
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3066, 1, _M0L5startS3067, _M0L6_2atmpS3068);
        if (_M0L7_2abindS1490 == 4294967296ll) {
          _M0L6_2atmpS3064 = _M0L7_2abindS1126.$2;
        } else {
          int64_t _M0L7_2aSomeS1129 = _M0L7_2abindS1490;
          _M0L6_2atmpS3064 = (int32_t)_M0L7_2aSomeS1129;
        }
        _M0L8_2afieldS3713 = _M0L7_2abindS1126.$2;
        moonbit_decref(_M0L7_2abindS1126.$0);
        _M0L3endS3065 = _M0L8_2afieldS3713;
        _M0L4_2axS1128
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3064, _M0L3endS3065, _M0L3strS3063
        };
        _M0L7_2abindS1123
        = (struct _M0TUbRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TUbRPC16string10StringViewE));
        Moonbit_object_header(_M0L7_2abindS1123)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TUbRPC16string10StringViewE, $1_0) >> 2, 1, 0);
        _M0L7_2abindS1123->$0 = 0;
        _M0L7_2abindS1123->$1_0 = _M0L4_2axS1128.$0;
        _M0L7_2abindS1123->$1_1 = _M0L4_2axS1128.$1;
        _M0L7_2abindS1123->$1_2 = _M0L4_2axS1128.$2;
      } else if (_M0L4_2axS1127 == 45) {
        moonbit_string_t _M0L8_2afieldS3718 = _M0L7_2abindS1126.$0;
        moonbit_string_t _M0L3strS3056 = _M0L8_2afieldS3718;
        moonbit_string_t _M0L8_2afieldS3717 = _M0L7_2abindS1126.$0;
        moonbit_string_t _M0L3strS3059 = _M0L8_2afieldS3717;
        int32_t _M0L5startS3060 = _M0L7_2abindS1126.$1;
        int32_t _M0L3endS3062 = _M0L7_2abindS1126.$2;
        int64_t _M0L6_2atmpS3061 = (int64_t)_M0L3endS3062;
        int64_t _M0L7_2abindS1491;
        int32_t _M0L6_2atmpS3057;
        int32_t _M0L8_2afieldS3716;
        int32_t _M0L3endS3058;
        struct _M0TPC16string10StringView _M0L4_2axS1130;
        moonbit_incref(_M0L3strS3059);
        moonbit_incref(_M0L3strS3056);
        #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        _M0L7_2abindS1491
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3059, 1, _M0L5startS3060, _M0L6_2atmpS3061);
        if (_M0L7_2abindS1491 == 4294967296ll) {
          _M0L6_2atmpS3057 = _M0L7_2abindS1126.$2;
        } else {
          int64_t _M0L7_2aSomeS1131 = _M0L7_2abindS1491;
          _M0L6_2atmpS3057 = (int32_t)_M0L7_2aSomeS1131;
        }
        _M0L8_2afieldS3716 = _M0L7_2abindS1126.$2;
        moonbit_decref(_M0L7_2abindS1126.$0);
        _M0L3endS3058 = _M0L8_2afieldS3716;
        _M0L4_2axS1130
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3057, _M0L3endS3058, _M0L3strS3056
        };
        _M0L7_2abindS1123
        = (struct _M0TUbRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TUbRPC16string10StringViewE));
        Moonbit_object_header(_M0L7_2abindS1123)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TUbRPC16string10StringViewE, $1_0) >> 2, 1, 0);
        _M0L7_2abindS1123->$0 = 1;
        _M0L7_2abindS1123->$1_0 = _M0L4_2axS1130.$0;
        _M0L7_2abindS1123->$1_1 = _M0L4_2axS1130.$1;
        _M0L7_2abindS1123->$1_2 = _M0L4_2axS1130.$2;
      } else {
        _M0L4restS1125 = _M0L7_2abindS1126;
        goto join_1124;
      }
    } else {
      _M0L4restS1125 = _M0L7_2abindS1126;
      goto join_1124;
    }
    goto joinlet_4207;
    join_1124:;
    _M0L7_2abindS1123
    = (struct _M0TUbRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TUbRPC16string10StringViewE));
    Moonbit_object_header(_M0L7_2abindS1123)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUbRPC16string10StringViewE, $1_0) >> 2, 1, 0);
    _M0L7_2abindS1123->$0 = 0;
    _M0L7_2abindS1123->$1_0 = _M0L4restS1125.$0;
    _M0L7_2abindS1123->$1_1 = _M0L4restS1125.$1;
    _M0L7_2abindS1123->$1_2 = _M0L4restS1125.$2;
    joinlet_4207:;
    _M0L6_2anegS1132 = _M0L7_2abindS1123->$0;
    _M0L8_2afieldS3712
    = (struct _M0TPC16string10StringView){
      _M0L7_2abindS1123->$1_1,
        _M0L7_2abindS1123->$1_2,
        _M0L7_2abindS1123->$1_0
    };
    _M0L6_2acntS4033 = Moonbit_object_header(_M0L7_2abindS1123)->rc;
    if (_M0L6_2acntS4033 > 1) {
      int32_t _M0L11_2anew__cntS4034 = _M0L6_2acntS4033 - 1;
      Moonbit_object_header(_M0L7_2abindS1123)->rc = _M0L11_2anew__cntS4034;
      moonbit_incref(_M0L8_2afieldS3712.$0);
    } else if (_M0L6_2acntS4033 == 1) {
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      moonbit_free(_M0L7_2abindS1123);
    }
    _M0L7_2arestS1133 = _M0L8_2afieldS3712;
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    _tmp_4208
    = _M0FPC17strconv25check__and__consume__base(_M0L7_2arestS1133, _M0L4baseS1135);
    if (_tmp_4208.tag) {
      struct _M0TUiRPC16string10StringViewbE* const _M0L5_2aokS3050 =
        _tmp_4208.data.ok;
      _M0L7_2abindS1134 = _M0L5_2aokS3050;
    } else {
      void* const _M0L6_2aerrS3051 = _tmp_4208.data.err;
      struct moonbit_result_2 _result_4209;
      _result_4209.tag = 0;
      _result_4209.data.err = _M0L6_2aerrS3051;
      return _result_4209;
    }
    _M0L12_2anum__baseS1136 = _M0L7_2abindS1134->$0;
    _M0L8_2afieldS3711
    = (struct _M0TPC16string10StringView){
      _M0L7_2abindS1134->$1_1,
        _M0L7_2abindS1134->$1_2,
        _M0L7_2abindS1134->$1_0
    };
    _M0L7_2arestS1137 = _M0L8_2afieldS3711;
    _M0L8_2afieldS3710 = _M0L7_2abindS1134->$2;
    _M0L6_2acntS4035 = Moonbit_object_header(_M0L7_2abindS1134)->rc;
    if (_M0L6_2acntS4035 > 1) {
      int32_t _M0L11_2anew__cntS4036 = _M0L6_2acntS4035 - 1;
      Moonbit_object_header(_M0L7_2abindS1134)->rc = _M0L11_2anew__cntS4036;
      moonbit_incref(_M0L7_2arestS1137.$0);
    } else if (_M0L6_2acntS4035 == 1) {
      #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      moonbit_free(_M0L7_2abindS1134);
    }
    _M0L20_2aallow__underscoreS1138 = _M0L8_2afieldS3710;
    #line 100 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    _M0L19overflow__thresholdS1139
    = _M0FPC17strconv19overflow__threshold(_M0L12_2anum__baseS1136, _M0L6_2anegS1132);
    _M0L8_2afieldS3709 = _M0L7_2arestS1137.$0;
    _M0L3strS3028 = _M0L8_2afieldS3709;
    _M0L5startS3029 = _M0L7_2arestS1137.$1;
    _M0L3endS3031 = _M0L7_2arestS1137.$2;
    _M0L6_2atmpS3030 = (int64_t)_M0L3endS3031;
    moonbit_incref(_M0L3strS3028);
    #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3028, 1, _M0L5startS3029, _M0L6_2atmpS3030)
    ) {
      moonbit_string_t _M0L8_2afieldS3708 = _M0L7_2arestS1137.$0;
      moonbit_string_t _M0L3strS3043 = _M0L8_2afieldS3708;
      moonbit_string_t _M0L8_2afieldS3707 = _M0L7_2arestS1137.$0;
      moonbit_string_t _M0L3strS3046 = _M0L8_2afieldS3707;
      int32_t _M0L5startS3047 = _M0L7_2arestS1137.$1;
      int32_t _M0L3endS3049 = _M0L7_2arestS1137.$2;
      int64_t _M0L6_2atmpS3048 = (int64_t)_M0L3endS3049;
      int64_t _M0L6_2atmpS3045;
      int32_t _M0L6_2atmpS3044;
      int32_t _M0L4_2axS1141;
      moonbit_incref(_M0L3strS3046);
      moonbit_incref(_M0L3strS3043);
      #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L6_2atmpS3045
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3046, 0, _M0L5startS3047, _M0L6_2atmpS3048);
      _M0L6_2atmpS3044 = (int32_t)_M0L6_2atmpS3045;
      #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L4_2axS1141
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3043, _M0L6_2atmpS3044);
      if (_M0L4_2axS1141 >= 48 && _M0L4_2axS1141 <= 57) {
        _M0L10has__digitS1140 = 1;
      } else if (_M0L4_2axS1141 >= 97 && _M0L4_2axS1141 <= 122) {
        _M0L10has__digitS1140 = 1;
      } else if (_M0L4_2axS1141 >= 65 && _M0L4_2axS1141 <= 90) {
        _M0L10has__digitS1140 = 1;
      } else {
        moonbit_string_t _M0L8_2afieldS3706 = _M0L7_2arestS1137.$0;
        moonbit_string_t _M0L3strS3032 = _M0L8_2afieldS3706;
        int32_t _M0L5startS3033 = _M0L7_2arestS1137.$1;
        int32_t _M0L3endS3035 = _M0L7_2arestS1137.$2;
        int64_t _M0L6_2atmpS3034 = (int64_t)_M0L3endS3035;
        moonbit_incref(_M0L3strS3032);
        #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        if (
          _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3032, 2, _M0L5startS3033, _M0L6_2atmpS3034)
        ) {
          if (_M0L4_2axS1141 == 95) {
            moonbit_string_t _M0L8_2afieldS3705 = _M0L7_2arestS1137.$0;
            moonbit_string_t _M0L3strS3036 = _M0L8_2afieldS3705;
            moonbit_string_t _M0L8_2afieldS3704 = _M0L7_2arestS1137.$0;
            moonbit_string_t _M0L3strS3039 = _M0L8_2afieldS3704;
            int32_t _M0L5startS3040 = _M0L7_2arestS1137.$1;
            int32_t _M0L3endS3042 = _M0L7_2arestS1137.$2;
            int64_t _M0L6_2atmpS3041 = (int64_t)_M0L3endS3042;
            int64_t _M0L6_2atmpS3038;
            int32_t _M0L6_2atmpS3037;
            int32_t _M0L4_2axS1142;
            moonbit_incref(_M0L3strS3039);
            moonbit_incref(_M0L3strS3036);
            #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
            _M0L6_2atmpS3038
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3039, 1, _M0L5startS3040, _M0L6_2atmpS3041);
            _M0L6_2atmpS3037 = (int32_t)_M0L6_2atmpS3038;
            #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
            _M0L4_2axS1142
            = _M0MPC16string6String16unsafe__char__at(_M0L3strS3036, _M0L6_2atmpS3037);
            _M0L10has__digitS1140
            = _M0L4_2axS1142 >= 48 && _M0L4_2axS1142 <= 57
              || _M0L4_2axS1142 >= 97 && _M0L4_2axS1142 <= 122
              || _M0L4_2axS1142 >= 65 && _M0L4_2axS1142 <= 90
              || 0;
          } else {
            _M0L10has__digitS1140 = 0;
          }
        } else {
          _M0L10has__digitS1140 = 0;
        }
      }
    } else {
      _M0L10has__digitS1140 = 0;
    }
    if (_M0L10has__digitS1140) {
      int64_t _M0L6_2atmpS2962;
      struct _M0TPC16string10StringView _M0L11_2aparam__0S1143 =
        _M0L7_2arestS1137;
      int64_t _M0L11_2aparam__1S1144 = 0ll;
      int32_t _M0L11_2aparam__2S1145 = _M0L20_2aallow__underscoreS1138;
      struct moonbit_result_2 _result_4229;
      while (1) {
        int64_t _M0L3accS1147;
        struct _M0TPC16string10StringView _M0L4restS1148;
        int32_t _M0L1cS1149;
        moonbit_string_t _M0L8_2afieldS3703 = _M0L11_2aparam__0S1143.$0;
        moonbit_string_t _M0L3strS2981 = _M0L8_2afieldS3703;
        int32_t _M0L5startS2982 = _M0L11_2aparam__0S1143.$1;
        int32_t _M0L3endS2984 = _M0L11_2aparam__0S1143.$2;
        int64_t _M0L6_2atmpS2983 = (int64_t)_M0L3endS2984;
        int32_t _M0L1cS1150;
        int32_t _M0L1dS1151;
        moonbit_incref(_M0L3strS2981);
        #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        if (
          _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2981, 1, _M0L5startS2982, _M0L6_2atmpS2983)
        ) {
          moonbit_string_t _M0L8_2afieldS3693 = _M0L11_2aparam__0S1143.$0;
          moonbit_string_t _M0L3strS2994 = _M0L8_2afieldS3693;
          moonbit_string_t _M0L8_2afieldS3692 = _M0L11_2aparam__0S1143.$0;
          moonbit_string_t _M0L3strS2997 = _M0L8_2afieldS3692;
          int32_t _M0L5startS2998 = _M0L11_2aparam__0S1143.$1;
          int32_t _M0L3endS3000 = _M0L11_2aparam__0S1143.$2;
          int64_t _M0L6_2atmpS2999 = (int64_t)_M0L3endS3000;
          int64_t _M0L6_2atmpS2996;
          int32_t _M0L6_2atmpS2995;
          int32_t _M0L4_2axS1155;
          moonbit_incref(_M0L3strS2997);
          moonbit_incref(_M0L3strS2994);
          #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L6_2atmpS2996
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2997, 0, _M0L5startS2998, _M0L6_2atmpS2999);
          _M0L6_2atmpS2995 = (int32_t)_M0L6_2atmpS2996;
          #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L4_2axS1155
          = _M0MPC16string6String16unsafe__char__at(_M0L3strS2994, _M0L6_2atmpS2995);
          if (_M0L4_2axS1155 == 95) {
            struct moonbit_result_2 _tmp_4212;
            moonbit_decref(_M0L11_2aparam__0S1143.$0);
            #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
            _tmp_4212 = _M0FPC17strconv11syntax__errGlE();
            if (_tmp_4212.tag) {
              int64_t const _M0L5_2aokS2992 = _tmp_4212.data.ok;
              _M0L6_2atmpS2962 = _M0L5_2aokS2992;
            } else {
              void* const _M0L6_2aerrS2993 = _tmp_4212.data.err;
              struct moonbit_result_2 _result_4213;
              _result_4213.tag = 0;
              _result_4213.data.err = _M0L6_2aerrS2993;
              return _result_4213;
            }
          } else {
            moonbit_string_t _M0L8_2afieldS3691 = _M0L11_2aparam__0S1143.$0;
            moonbit_string_t _M0L3strS2985 = _M0L8_2afieldS3691;
            moonbit_string_t _M0L8_2afieldS3690 = _M0L11_2aparam__0S1143.$0;
            moonbit_string_t _M0L3strS2988 = _M0L8_2afieldS3690;
            int32_t _M0L5startS2989 = _M0L11_2aparam__0S1143.$1;
            int32_t _M0L3endS2991 = _M0L11_2aparam__0S1143.$2;
            int64_t _M0L6_2atmpS2990 = (int64_t)_M0L3endS2991;
            int64_t _M0L7_2abindS1492;
            int32_t _M0L6_2atmpS2986;
            int32_t _M0L8_2afieldS3689;
            int32_t _M0L3endS2987;
            struct _M0TPC16string10StringView _M0L4_2axS1156;
            moonbit_incref(_M0L3strS2988);
            moonbit_incref(_M0L3strS2985);
            #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
            _M0L7_2abindS1492
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2988, 1, _M0L5startS2989, _M0L6_2atmpS2990);
            if (_M0L7_2abindS1492 == 4294967296ll) {
              _M0L6_2atmpS2986 = _M0L11_2aparam__0S1143.$2;
            } else {
              int64_t _M0L7_2aSomeS1157 = _M0L7_2abindS1492;
              _M0L6_2atmpS2986 = (int32_t)_M0L7_2aSomeS1157;
            }
            _M0L8_2afieldS3689 = _M0L11_2aparam__0S1143.$2;
            moonbit_decref(_M0L11_2aparam__0S1143.$0);
            _M0L3endS2987 = _M0L8_2afieldS3689;
            _M0L4_2axS1156
            = (struct _M0TPC16string10StringView){
              _M0L6_2atmpS2986, _M0L3endS2987, _M0L3strS2985
            };
            _M0L3accS1147 = _M0L11_2aparam__1S1144;
            _M0L4restS1148 = _M0L4_2axS1156;
            _M0L1cS1149 = _M0L4_2axS1155;
            goto join_1146;
          }
        } else {
          moonbit_string_t _M0L8_2afieldS3702 = _M0L11_2aparam__0S1143.$0;
          moonbit_string_t _M0L3strS3001 = _M0L8_2afieldS3702;
          int32_t _M0L5startS3002 = _M0L11_2aparam__0S1143.$1;
          int32_t _M0L3endS3004 = _M0L11_2aparam__0S1143.$2;
          int64_t _M0L6_2atmpS3003 = (int64_t)_M0L3endS3004;
          moonbit_incref(_M0L3strS3001);
          #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          if (
            _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3001, 1, _M0L5startS3002, _M0L6_2atmpS3003)
          ) {
            moonbit_string_t _M0L8_2afieldS3701 = _M0L11_2aparam__0S1143.$0;
            moonbit_string_t _M0L3strS3021 = _M0L8_2afieldS3701;
            moonbit_string_t _M0L8_2afieldS3700 = _M0L11_2aparam__0S1143.$0;
            moonbit_string_t _M0L3strS3024 = _M0L8_2afieldS3700;
            int32_t _M0L5startS3025 = _M0L11_2aparam__0S1143.$1;
            int32_t _M0L3endS3027 = _M0L11_2aparam__0S1143.$2;
            int64_t _M0L6_2atmpS3026 = (int64_t)_M0L3endS3027;
            int64_t _M0L6_2atmpS3023;
            int32_t _M0L6_2atmpS3022;
            int32_t _M0L4_2axS1158;
            moonbit_incref(_M0L3strS3024);
            moonbit_incref(_M0L3strS3021);
            #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
            _M0L6_2atmpS3023
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3024, 0, _M0L5startS3025, _M0L6_2atmpS3026);
            _M0L6_2atmpS3022 = (int32_t)_M0L6_2atmpS3023;
            #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
            _M0L4_2axS1158
            = _M0MPC16string6String16unsafe__char__at(_M0L3strS3021, _M0L6_2atmpS3022);
            if (_M0L4_2axS1158 == 95) {
              if (_M0L11_2aparam__2S1145 == 0) {
                struct moonbit_result_2 _tmp_4214;
                moonbit_decref(_M0L11_2aparam__0S1143.$0);
                #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
                _tmp_4214 = _M0FPC17strconv11syntax__errGlE();
                if (_tmp_4214.tag) {
                  int64_t const _M0L5_2aokS3019 = _tmp_4214.data.ok;
                  _M0L6_2atmpS2962 = _M0L5_2aokS3019;
                } else {
                  void* const _M0L6_2aerrS3020 = _tmp_4214.data.err;
                  struct moonbit_result_2 _result_4215;
                  _result_4215.tag = 0;
                  _result_4215.data.err = _M0L6_2aerrS3020;
                  return _result_4215;
                }
              } else {
                moonbit_string_t _M0L8_2afieldS3696 =
                  _M0L11_2aparam__0S1143.$0;
                moonbit_string_t _M0L3strS3012 = _M0L8_2afieldS3696;
                moonbit_string_t _M0L8_2afieldS3695 =
                  _M0L11_2aparam__0S1143.$0;
                moonbit_string_t _M0L3strS3015 = _M0L8_2afieldS3695;
                int32_t _M0L5startS3016 = _M0L11_2aparam__0S1143.$1;
                int32_t _M0L3endS3018 = _M0L11_2aparam__0S1143.$2;
                int64_t _M0L6_2atmpS3017 = (int64_t)_M0L3endS3018;
                int64_t _M0L7_2abindS1493;
                int32_t _M0L6_2atmpS3013;
                int32_t _M0L8_2afieldS3694;
                int32_t _M0L3endS3014;
                struct _M0TPC16string10StringView _M0L4_2axS1159;
                int64_t _tmp_4216;
                moonbit_incref(_M0L3strS3015);
                moonbit_incref(_M0L3strS3012);
                #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
                _M0L7_2abindS1493
                = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3015, 1, _M0L5startS3016, _M0L6_2atmpS3017);
                if (_M0L7_2abindS1493 == 4294967296ll) {
                  _M0L6_2atmpS3013 = _M0L11_2aparam__0S1143.$2;
                } else {
                  int64_t _M0L7_2aSomeS1160 = _M0L7_2abindS1493;
                  _M0L6_2atmpS3013 = (int32_t)_M0L7_2aSomeS1160;
                }
                _M0L8_2afieldS3694 = _M0L11_2aparam__0S1143.$2;
                moonbit_decref(_M0L11_2aparam__0S1143.$0);
                _M0L3endS3014 = _M0L8_2afieldS3694;
                _M0L4_2axS1159
                = (struct _M0TPC16string10StringView){
                  _M0L6_2atmpS3013, _M0L3endS3014, _M0L3strS3012
                };
                _tmp_4216 = _M0L11_2aparam__1S1144;
                _M0L11_2aparam__0S1143 = _M0L4_2axS1159;
                _M0L11_2aparam__1S1144 = _tmp_4216;
                _M0L11_2aparam__2S1145 = 0;
                continue;
              }
            } else {
              moonbit_string_t _M0L8_2afieldS3699 = _M0L11_2aparam__0S1143.$0;
              moonbit_string_t _M0L3strS3005 = _M0L8_2afieldS3699;
              moonbit_string_t _M0L8_2afieldS3698 = _M0L11_2aparam__0S1143.$0;
              moonbit_string_t _M0L3strS3008 = _M0L8_2afieldS3698;
              int32_t _M0L5startS3009 = _M0L11_2aparam__0S1143.$1;
              int32_t _M0L3endS3011 = _M0L11_2aparam__0S1143.$2;
              int64_t _M0L6_2atmpS3010 = (int64_t)_M0L3endS3011;
              int64_t _M0L7_2abindS1494;
              int32_t _M0L6_2atmpS3006;
              int32_t _M0L8_2afieldS3697;
              int32_t _M0L3endS3007;
              struct _M0TPC16string10StringView _M0L4_2axS1161;
              moonbit_incref(_M0L3strS3008);
              moonbit_incref(_M0L3strS3005);
              #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
              _M0L7_2abindS1494
              = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3008, 1, _M0L5startS3009, _M0L6_2atmpS3010);
              if (_M0L7_2abindS1494 == 4294967296ll) {
                _M0L6_2atmpS3006 = _M0L11_2aparam__0S1143.$2;
              } else {
                int64_t _M0L7_2aSomeS1162 = _M0L7_2abindS1494;
                _M0L6_2atmpS3006 = (int32_t)_M0L7_2aSomeS1162;
              }
              _M0L8_2afieldS3697 = _M0L11_2aparam__0S1143.$2;
              moonbit_decref(_M0L11_2aparam__0S1143.$0);
              _M0L3endS3007 = _M0L8_2afieldS3697;
              _M0L4_2axS1161
              = (struct _M0TPC16string10StringView){
                _M0L6_2atmpS3006, _M0L3endS3007, _M0L3strS3005
              };
              _M0L3accS1147 = _M0L11_2aparam__1S1144;
              _M0L4restS1148 = _M0L4_2axS1161;
              _M0L1cS1149 = _M0L4_2axS1158;
              goto join_1146;
            }
          } else {
            moonbit_decref(_M0L11_2aparam__0S1143.$0);
            _M0L6_2atmpS2962 = _M0L11_2aparam__1S1144;
          }
        }
        goto joinlet_4211;
        join_1146:;
        _M0L1cS1150 = _M0L1cS1149;
        if (_M0L1cS1150 >= 48 && _M0L1cS1150 <= 57) {
          _M0L1dS1151 = _M0L1cS1150 - 48;
        } else if (_M0L1cS1150 >= 97 && _M0L1cS1150 <= 122) {
          _M0L1dS1151 = _M0L1cS1150 + -87;
        } else if (_M0L1cS1150 >= 65 && _M0L1cS1150 <= 90) {
          _M0L1dS1151 = _M0L1cS1150 + -55;
        } else {
          struct moonbit_result_0 _tmp_4217;
          #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _tmp_4217 = _M0FPC17strconv11syntax__errGiE();
          if (_tmp_4217.tag) {
            int32_t const _M0L5_2aokS2979 = _tmp_4217.data.ok;
            _M0L1dS1151 = _M0L5_2aokS2979;
          } else {
            void* const _M0L6_2aerrS2980 = _tmp_4217.data.err;
            struct moonbit_result_2 _result_4218;
            moonbit_decref(_M0L4restS1148.$0);
            _result_4218.tag = 0;
            _result_4218.data.err = _M0L6_2aerrS2980;
            return _result_4218;
          }
        }
        if (_M0L1dS1151 < _M0L12_2anum__baseS1136) {
          if (_M0L6_2anegS1132) {
            if (_M0L3accS1147 >= _M0L19overflow__thresholdS1139) {
              int64_t _M0L6_2atmpS2967 = (int64_t)_M0L12_2anum__baseS1136;
              int64_t _M0L6_2atmpS2965 = _M0L3accS1147 * _M0L6_2atmpS2967;
              int64_t _M0L6_2atmpS2966 = (int64_t)_M0L1dS1151;
              int64_t _M0L9next__accS1152 =
                _M0L6_2atmpS2965 - _M0L6_2atmpS2966;
              if (_M0L9next__accS1152 <= _M0L3accS1147) {
                _M0L11_2aparam__0S1143 = _M0L4restS1148;
                _M0L11_2aparam__1S1144 = _M0L9next__accS1152;
                _M0L11_2aparam__2S1145 = 1;
                continue;
              } else {
                struct moonbit_result_2 _tmp_4219;
                moonbit_decref(_M0L4restS1148.$0);
                #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
                _tmp_4219 = _M0FPC17strconv10range__errGlE();
                if (_tmp_4219.tag) {
                  int64_t const _M0L5_2aokS2963 = _tmp_4219.data.ok;
                  _M0L6_2atmpS2962 = _M0L5_2aokS2963;
                } else {
                  void* const _M0L6_2aerrS2964 = _tmp_4219.data.err;
                  struct moonbit_result_2 _result_4220;
                  _result_4220.tag = 0;
                  _result_4220.data.err = _M0L6_2aerrS2964;
                  return _result_4220;
                }
              }
            } else {
              struct moonbit_result_2 _tmp_4221;
              moonbit_decref(_M0L4restS1148.$0);
              #line 122 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
              _tmp_4221 = _M0FPC17strconv10range__errGlE();
              if (_tmp_4221.tag) {
                int64_t const _M0L5_2aokS2968 = _tmp_4221.data.ok;
                _M0L6_2atmpS2962 = _M0L5_2aokS2968;
              } else {
                void* const _M0L6_2aerrS2969 = _tmp_4221.data.err;
                struct moonbit_result_2 _result_4222;
                _result_4222.tag = 0;
                _result_4222.data.err = _M0L6_2aerrS2969;
                return _result_4222;
              }
            }
          } else if (_M0L3accS1147 < _M0L19overflow__thresholdS1139) {
            int64_t _M0L6_2atmpS2974 = (int64_t)_M0L12_2anum__baseS1136;
            int64_t _M0L6_2atmpS2972 = _M0L3accS1147 * _M0L6_2atmpS2974;
            int64_t _M0L6_2atmpS2973 = (int64_t)_M0L1dS1151;
            int64_t _M0L9next__accS1154 = _M0L6_2atmpS2972 + _M0L6_2atmpS2973;
            if (_M0L9next__accS1154 >= _M0L3accS1147) {
              _M0L11_2aparam__0S1143 = _M0L4restS1148;
              _M0L11_2aparam__1S1144 = _M0L9next__accS1154;
              _M0L11_2aparam__2S1145 = 1;
              continue;
            } else {
              struct moonbit_result_2 _tmp_4223;
              moonbit_decref(_M0L4restS1148.$0);
              #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
              _tmp_4223 = _M0FPC17strconv10range__errGlE();
              if (_tmp_4223.tag) {
                int64_t const _M0L5_2aokS2970 = _tmp_4223.data.ok;
                _M0L6_2atmpS2962 = _M0L5_2aokS2970;
              } else {
                void* const _M0L6_2aerrS2971 = _tmp_4223.data.err;
                struct moonbit_result_2 _result_4224;
                _result_4224.tag = 0;
                _result_4224.data.err = _M0L6_2aerrS2971;
                return _result_4224;
              }
            }
          } else {
            struct moonbit_result_2 _tmp_4225;
            moonbit_decref(_M0L4restS1148.$0);
            #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
            _tmp_4225 = _M0FPC17strconv10range__errGlE();
            if (_tmp_4225.tag) {
              int64_t const _M0L5_2aokS2975 = _tmp_4225.data.ok;
              _M0L6_2atmpS2962 = _M0L5_2aokS2975;
            } else {
              void* const _M0L6_2aerrS2976 = _tmp_4225.data.err;
              struct moonbit_result_2 _result_4226;
              _result_4226.tag = 0;
              _result_4226.data.err = _M0L6_2aerrS2976;
              return _result_4226;
            }
          }
        } else {
          struct moonbit_result_2 _tmp_4227;
          moonbit_decref(_M0L4restS1148.$0);
          #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _tmp_4227 = _M0FPC17strconv11syntax__errGlE();
          if (_tmp_4227.tag) {
            int64_t const _M0L5_2aokS2977 = _tmp_4227.data.ok;
            _M0L6_2atmpS2962 = _M0L5_2aokS2977;
          } else {
            void* const _M0L6_2aerrS2978 = _tmp_4227.data.err;
            struct moonbit_result_2 _result_4228;
            _result_4228.tag = 0;
            _result_4228.data.err = _M0L6_2aerrS2978;
            return _result_4228;
          }
        }
        joinlet_4211:;
        break;
      }
      _result_4229.tag = 1;
      _result_4229.data.ok = _M0L6_2atmpS2962;
      return _result_4229;
    } else {
      moonbit_decref(_M0L7_2arestS1137.$0);
      #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      return _M0FPC17strconv11syntax__errGlE();
    }
  } else {
    moonbit_decref(_M0L3strS1122.$0);
    #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    return _M0FPC17strconv11syntax__errGlE();
  }
}

int64_t _M0FPC17strconv19overflow__threshold(
  int32_t _M0L4baseS1121,
  int32_t _M0L3negS1120
) {
  #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
  if (!_M0L3negS1120) {
    if (_M0L4baseS1121 == 10) {
      return 922337203685477581ll;
    } else if (_M0L4baseS1121 == 16) {
      return 576460752303423488ll;
    } else {
      int64_t _M0L6_2atmpS2958 = (int64_t)_M0L4baseS1121;
      int64_t _M0L6_2atmpS2957 = 9223372036854775807ll / _M0L6_2atmpS2958;
      return _M0L6_2atmpS2957 + 1ll;
    }
  } else if (_M0L4baseS1121 == 10) {
    return -922337203685477580ll;
  } else if (_M0L4baseS1121 == 16) {
    return -576460752303423488ll;
  } else {
    int64_t _M0L6_2atmpS2959 = (int64_t)_M0L4baseS1121;
    return (int64_t)0x8000000000000000ll / _M0L6_2atmpS2959;
  }
}

struct moonbit_result_0 _M0FPC17strconv11syntax__errGiE() {
  void* _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2955;
  struct moonbit_result_0 _result_4230;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
  moonbit_incref(_M0FPC17strconv16syntax__err__str);
  _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2955
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError));
  Moonbit_object_header(_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2955)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError*)_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2955)->$0
  = _M0FPC17strconv16syntax__err__str;
  _result_4230.tag = 0;
  _result_4230.data.err
  = _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2955;
  return _result_4230;
}

struct moonbit_result_2 _M0FPC17strconv11syntax__errGlE() {
  void* _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2956;
  struct moonbit_result_2 _result_4231;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
  moonbit_incref(_M0FPC17strconv16syntax__err__str);
  _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2956
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError));
  Moonbit_object_header(_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2956)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError*)_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2956)->$0
  = _M0FPC17strconv16syntax__err__str;
  _result_4231.tag = 0;
  _result_4231.data.err
  = _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2956;
  return _result_4231;
}

struct moonbit_result_0 _M0FPC17strconv10range__errGuE() {
  void* _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2953;
  struct moonbit_result_0 _result_4232;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
  moonbit_incref(_M0FPC17strconv15range__err__str);
  _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2953
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError));
  Moonbit_object_header(_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2953)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError*)_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2953)->$0
  = _M0FPC17strconv15range__err__str;
  _result_4232.tag = 0;
  _result_4232.data.err
  = _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2953;
  return _result_4232;
}

struct moonbit_result_2 _M0FPC17strconv10range__errGlE() {
  void* _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2954;
  struct moonbit_result_2 _result_4233;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
  moonbit_incref(_M0FPC17strconv15range__err__str);
  _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2954
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError));
  Moonbit_object_header(_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2954)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError*)_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2954)->$0
  = _M0FPC17strconv15range__err__str;
  _result_4233.tag = 0;
  _result_4233.data.err
  = _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2954;
  return _result_4233;
}

struct moonbit_result_3 _M0FPC17strconv25check__and__consume__base(
  struct _M0TPC16string10StringView _M0L4viewS1078,
  int32_t _M0L4baseS1076
) {
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
  if (_M0L4baseS1076 == 0) {
    struct _M0TPC16string10StringView _M0L4restS1080;
    struct _M0TPC16string10StringView _M0L4restS1082;
    struct _M0TPC16string10StringView _M0L4restS1084;
    moonbit_string_t _M0L8_2afieldS3744 = _M0L4viewS1078.$0;
    moonbit_string_t _M0L3strS2829 = _M0L8_2afieldS3744;
    int32_t _M0L5startS2830 = _M0L4viewS1078.$1;
    int32_t _M0L3endS2832 = _M0L4viewS1078.$2;
    int64_t _M0L6_2atmpS2831 = (int64_t)_M0L3endS2832;
    struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2828;
    struct moonbit_result_3 _result_4238;
    struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2827;
    struct moonbit_result_3 _result_4239;
    struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2826;
    struct moonbit_result_3 _result_4240;
    struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2825;
    struct moonbit_result_3 _result_4241;
    moonbit_incref(_M0L3strS2829);
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS2829, 2, _M0L5startS2830, _M0L6_2atmpS2831)
    ) {
      moonbit_string_t _M0L8_2afieldS3743 = _M0L4viewS1078.$0;
      moonbit_string_t _M0L3strS2882 = _M0L8_2afieldS3743;
      moonbit_string_t _M0L8_2afieldS3742 = _M0L4viewS1078.$0;
      moonbit_string_t _M0L3strS2885 = _M0L8_2afieldS3742;
      int32_t _M0L5startS2886 = _M0L4viewS1078.$1;
      int32_t _M0L3endS2888 = _M0L4viewS1078.$2;
      int64_t _M0L6_2atmpS2887 = (int64_t)_M0L3endS2888;
      int64_t _M0L6_2atmpS2884;
      int32_t _M0L6_2atmpS2883;
      int32_t _M0L4_2axS1085;
      moonbit_incref(_M0L3strS2885);
      moonbit_incref(_M0L3strS2882);
      #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L6_2atmpS2884
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2885, 0, _M0L5startS2886, _M0L6_2atmpS2887);
      _M0L6_2atmpS2883 = (int32_t)_M0L6_2atmpS2884;
      #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L4_2axS1085
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2882, _M0L6_2atmpS2883);
      if (_M0L4_2axS1085 == 48) {
        moonbit_string_t _M0L8_2afieldS3741 = _M0L4viewS1078.$0;
        moonbit_string_t _M0L3strS2875 = _M0L8_2afieldS3741;
        moonbit_string_t _M0L8_2afieldS3740 = _M0L4viewS1078.$0;
        moonbit_string_t _M0L3strS2878 = _M0L8_2afieldS3740;
        int32_t _M0L5startS2879 = _M0L4viewS1078.$1;
        int32_t _M0L3endS2881 = _M0L4viewS1078.$2;
        int64_t _M0L6_2atmpS2880 = (int64_t)_M0L3endS2881;
        int64_t _M0L6_2atmpS2877;
        int32_t _M0L6_2atmpS2876;
        int32_t _M0L4_2axS1086;
        moonbit_incref(_M0L3strS2878);
        moonbit_incref(_M0L3strS2875);
        #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        _M0L6_2atmpS2877
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2878, 1, _M0L5startS2879, _M0L6_2atmpS2880);
        _M0L6_2atmpS2876 = (int32_t)_M0L6_2atmpS2877;
        #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        _M0L4_2axS1086
        = _M0MPC16string6String16unsafe__char__at(_M0L3strS2875, _M0L6_2atmpS2876);
        if (_M0L4_2axS1086 == 120) {
          moonbit_string_t _M0L8_2afieldS3724 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2868 = _M0L8_2afieldS3724;
          moonbit_string_t _M0L8_2afieldS3723 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2871 = _M0L8_2afieldS3723;
          int32_t _M0L5startS2872 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2874 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2873 = (int64_t)_M0L3endS2874;
          int64_t _M0L7_2abindS1478;
          int32_t _M0L6_2atmpS2869;
          int32_t _M0L8_2afieldS3722;
          int32_t _M0L3endS2870;
          struct _M0TPC16string10StringView _M0L4_2axS1087;
          moonbit_incref(_M0L3strS2871);
          moonbit_incref(_M0L3strS2868);
          #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1478
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2871, 2, _M0L5startS2872, _M0L6_2atmpS2873);
          if (_M0L7_2abindS1478 == 4294967296ll) {
            _M0L6_2atmpS2869 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1088 = _M0L7_2abindS1478;
            _M0L6_2atmpS2869 = (int32_t)_M0L7_2aSomeS1088;
          }
          _M0L8_2afieldS3722 = _M0L4viewS1078.$2;
          moonbit_decref(_M0L4viewS1078.$0);
          _M0L3endS2870 = _M0L8_2afieldS3722;
          _M0L4_2axS1087
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2869, _M0L3endS2870, _M0L3strS2868
          };
          _M0L4restS1084 = _M0L4_2axS1087;
          goto join_1083;
        } else if (_M0L4_2axS1086 == 88) {
          moonbit_string_t _M0L8_2afieldS3727 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2861 = _M0L8_2afieldS3727;
          moonbit_string_t _M0L8_2afieldS3726 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2864 = _M0L8_2afieldS3726;
          int32_t _M0L5startS2865 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2867 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2866 = (int64_t)_M0L3endS2867;
          int64_t _M0L7_2abindS1479;
          int32_t _M0L6_2atmpS2862;
          int32_t _M0L8_2afieldS3725;
          int32_t _M0L3endS2863;
          struct _M0TPC16string10StringView _M0L4_2axS1089;
          moonbit_incref(_M0L3strS2864);
          moonbit_incref(_M0L3strS2861);
          #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1479
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2864, 2, _M0L5startS2865, _M0L6_2atmpS2866);
          if (_M0L7_2abindS1479 == 4294967296ll) {
            _M0L6_2atmpS2862 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1090 = _M0L7_2abindS1479;
            _M0L6_2atmpS2862 = (int32_t)_M0L7_2aSomeS1090;
          }
          _M0L8_2afieldS3725 = _M0L4viewS1078.$2;
          moonbit_decref(_M0L4viewS1078.$0);
          _M0L3endS2863 = _M0L8_2afieldS3725;
          _M0L4_2axS1089
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2862, _M0L3endS2863, _M0L3strS2861
          };
          _M0L4restS1084 = _M0L4_2axS1089;
          goto join_1083;
        } else if (_M0L4_2axS1086 == 111) {
          moonbit_string_t _M0L8_2afieldS3730 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2854 = _M0L8_2afieldS3730;
          moonbit_string_t _M0L8_2afieldS3729 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2857 = _M0L8_2afieldS3729;
          int32_t _M0L5startS2858 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2860 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2859 = (int64_t)_M0L3endS2860;
          int64_t _M0L7_2abindS1480;
          int32_t _M0L6_2atmpS2855;
          int32_t _M0L8_2afieldS3728;
          int32_t _M0L3endS2856;
          struct _M0TPC16string10StringView _M0L4_2axS1091;
          moonbit_incref(_M0L3strS2857);
          moonbit_incref(_M0L3strS2854);
          #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1480
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2857, 2, _M0L5startS2858, _M0L6_2atmpS2859);
          if (_M0L7_2abindS1480 == 4294967296ll) {
            _M0L6_2atmpS2855 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1092 = _M0L7_2abindS1480;
            _M0L6_2atmpS2855 = (int32_t)_M0L7_2aSomeS1092;
          }
          _M0L8_2afieldS3728 = _M0L4viewS1078.$2;
          moonbit_decref(_M0L4viewS1078.$0);
          _M0L3endS2856 = _M0L8_2afieldS3728;
          _M0L4_2axS1091
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2855, _M0L3endS2856, _M0L3strS2854
          };
          _M0L4restS1082 = _M0L4_2axS1091;
          goto join_1081;
        } else if (_M0L4_2axS1086 == 79) {
          moonbit_string_t _M0L8_2afieldS3733 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2847 = _M0L8_2afieldS3733;
          moonbit_string_t _M0L8_2afieldS3732 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2850 = _M0L8_2afieldS3732;
          int32_t _M0L5startS2851 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2853 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2852 = (int64_t)_M0L3endS2853;
          int64_t _M0L7_2abindS1481;
          int32_t _M0L6_2atmpS2848;
          int32_t _M0L8_2afieldS3731;
          int32_t _M0L3endS2849;
          struct _M0TPC16string10StringView _M0L4_2axS1093;
          moonbit_incref(_M0L3strS2850);
          moonbit_incref(_M0L3strS2847);
          #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1481
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2850, 2, _M0L5startS2851, _M0L6_2atmpS2852);
          if (_M0L7_2abindS1481 == 4294967296ll) {
            _M0L6_2atmpS2848 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1094 = _M0L7_2abindS1481;
            _M0L6_2atmpS2848 = (int32_t)_M0L7_2aSomeS1094;
          }
          _M0L8_2afieldS3731 = _M0L4viewS1078.$2;
          moonbit_decref(_M0L4viewS1078.$0);
          _M0L3endS2849 = _M0L8_2afieldS3731;
          _M0L4_2axS1093
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2848, _M0L3endS2849, _M0L3strS2847
          };
          _M0L4restS1082 = _M0L4_2axS1093;
          goto join_1081;
        } else if (_M0L4_2axS1086 == 98) {
          moonbit_string_t _M0L8_2afieldS3736 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2840 = _M0L8_2afieldS3736;
          moonbit_string_t _M0L8_2afieldS3735 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2843 = _M0L8_2afieldS3735;
          int32_t _M0L5startS2844 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2846 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2845 = (int64_t)_M0L3endS2846;
          int64_t _M0L7_2abindS1482;
          int32_t _M0L6_2atmpS2841;
          int32_t _M0L8_2afieldS3734;
          int32_t _M0L3endS2842;
          struct _M0TPC16string10StringView _M0L4_2axS1095;
          moonbit_incref(_M0L3strS2843);
          moonbit_incref(_M0L3strS2840);
          #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1482
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2843, 2, _M0L5startS2844, _M0L6_2atmpS2845);
          if (_M0L7_2abindS1482 == 4294967296ll) {
            _M0L6_2atmpS2841 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1096 = _M0L7_2abindS1482;
            _M0L6_2atmpS2841 = (int32_t)_M0L7_2aSomeS1096;
          }
          _M0L8_2afieldS3734 = _M0L4viewS1078.$2;
          moonbit_decref(_M0L4viewS1078.$0);
          _M0L3endS2842 = _M0L8_2afieldS3734;
          _M0L4_2axS1095
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2841, _M0L3endS2842, _M0L3strS2840
          };
          _M0L4restS1080 = _M0L4_2axS1095;
          goto join_1079;
        } else if (_M0L4_2axS1086 == 66) {
          moonbit_string_t _M0L8_2afieldS3739 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2833 = _M0L8_2afieldS3739;
          moonbit_string_t _M0L8_2afieldS3738 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2836 = _M0L8_2afieldS3738;
          int32_t _M0L5startS2837 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2839 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2838 = (int64_t)_M0L3endS2839;
          int64_t _M0L7_2abindS1483;
          int32_t _M0L6_2atmpS2834;
          int32_t _M0L8_2afieldS3737;
          int32_t _M0L3endS2835;
          struct _M0TPC16string10StringView _M0L4_2axS1097;
          moonbit_incref(_M0L3strS2836);
          moonbit_incref(_M0L3strS2833);
          #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1483
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2836, 2, _M0L5startS2837, _M0L6_2atmpS2838);
          if (_M0L7_2abindS1483 == 4294967296ll) {
            _M0L6_2atmpS2834 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1098 = _M0L7_2abindS1483;
            _M0L6_2atmpS2834 = (int32_t)_M0L7_2aSomeS1098;
          }
          _M0L8_2afieldS3737 = _M0L4viewS1078.$2;
          moonbit_decref(_M0L4viewS1078.$0);
          _M0L3endS2835 = _M0L8_2afieldS3737;
          _M0L4_2axS1097
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2834, _M0L3endS2835, _M0L3strS2833
          };
          _M0L4restS1080 = _M0L4_2axS1097;
          goto join_1079;
        } else {
          goto join_1077;
        }
      } else {
        goto join_1077;
      }
    } else {
      goto join_1077;
    }
    join_1083:;
    _M0L8_2atupleS2828
    = (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
    Moonbit_object_header(_M0L8_2atupleS2828)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS2828->$0 = 16;
    _M0L8_2atupleS2828->$1_0 = _M0L4restS1084.$0;
    _M0L8_2atupleS2828->$1_1 = _M0L4restS1084.$1;
    _M0L8_2atupleS2828->$1_2 = _M0L4restS1084.$2;
    _M0L8_2atupleS2828->$2 = 1;
    _result_4238.tag = 1;
    _result_4238.data.ok = _M0L8_2atupleS2828;
    return _result_4238;
    join_1081:;
    _M0L8_2atupleS2827
    = (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
    Moonbit_object_header(_M0L8_2atupleS2827)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS2827->$0 = 8;
    _M0L8_2atupleS2827->$1_0 = _M0L4restS1082.$0;
    _M0L8_2atupleS2827->$1_1 = _M0L4restS1082.$1;
    _M0L8_2atupleS2827->$1_2 = _M0L4restS1082.$2;
    _M0L8_2atupleS2827->$2 = 1;
    _result_4239.tag = 1;
    _result_4239.data.ok = _M0L8_2atupleS2827;
    return _result_4239;
    join_1079:;
    _M0L8_2atupleS2826
    = (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
    Moonbit_object_header(_M0L8_2atupleS2826)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS2826->$0 = 2;
    _M0L8_2atupleS2826->$1_0 = _M0L4restS1080.$0;
    _M0L8_2atupleS2826->$1_1 = _M0L4restS1080.$1;
    _M0L8_2atupleS2826->$1_2 = _M0L4restS1080.$2;
    _M0L8_2atupleS2826->$2 = 1;
    _result_4240.tag = 1;
    _result_4240.data.ok = _M0L8_2atupleS2826;
    return _result_4240;
    join_1077:;
    _M0L8_2atupleS2825
    = (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
    Moonbit_object_header(_M0L8_2atupleS2825)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS2825->$0 = 10;
    _M0L8_2atupleS2825->$1_0 = _M0L4viewS1078.$0;
    _M0L8_2atupleS2825->$1_1 = _M0L4viewS1078.$1;
    _M0L8_2atupleS2825->$1_2 = _M0L4viewS1078.$2;
    _M0L8_2atupleS2825->$2 = 0;
    _result_4241.tag = 1;
    _result_4241.data.ok = _M0L8_2atupleS2825;
    return _result_4241;
  } else {
    struct _M0TPC16string10StringView _M0L4restS1101;
    struct _M0TPC16string10StringView _M0L4restS1103;
    struct _M0TPC16string10StringView _M0L4restS1105;
    moonbit_string_t _M0L8_2afieldS3761 = _M0L4viewS1078.$0;
    moonbit_string_t _M0L3strS2893 = _M0L8_2afieldS3761;
    int32_t _M0L5startS2894 = _M0L4viewS1078.$1;
    int32_t _M0L3endS2896 = _M0L4viewS1078.$2;
    int64_t _M0L6_2atmpS2895 = (int64_t)_M0L3endS2896;
    struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2892;
    struct moonbit_result_3 _result_4246;
    struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2891;
    struct moonbit_result_3 _result_4247;
    struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2890;
    struct moonbit_result_3 _result_4248;
    moonbit_incref(_M0L3strS2893);
    #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS2893, 2, _M0L5startS2894, _M0L6_2atmpS2895)
    ) {
      moonbit_string_t _M0L8_2afieldS3760 = _M0L4viewS1078.$0;
      moonbit_string_t _M0L3strS2946 = _M0L8_2afieldS3760;
      moonbit_string_t _M0L8_2afieldS3759 = _M0L4viewS1078.$0;
      moonbit_string_t _M0L3strS2949 = _M0L8_2afieldS3759;
      int32_t _M0L5startS2950 = _M0L4viewS1078.$1;
      int32_t _M0L3endS2952 = _M0L4viewS1078.$2;
      int64_t _M0L6_2atmpS2951 = (int64_t)_M0L3endS2952;
      int64_t _M0L6_2atmpS2948;
      int32_t _M0L6_2atmpS2947;
      int32_t _M0L4_2axS1106;
      moonbit_incref(_M0L3strS2949);
      moonbit_incref(_M0L3strS2946);
      #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L6_2atmpS2948
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2949, 0, _M0L5startS2950, _M0L6_2atmpS2951);
      _M0L6_2atmpS2947 = (int32_t)_M0L6_2atmpS2948;
      #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      _M0L4_2axS1106
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2946, _M0L6_2atmpS2947);
      if (_M0L4_2axS1106 == 48) {
        moonbit_string_t _M0L8_2afieldS3758 = _M0L4viewS1078.$0;
        moonbit_string_t _M0L3strS2939 = _M0L8_2afieldS3758;
        moonbit_string_t _M0L8_2afieldS3757 = _M0L4viewS1078.$0;
        moonbit_string_t _M0L3strS2942 = _M0L8_2afieldS3757;
        int32_t _M0L5startS2943 = _M0L4viewS1078.$1;
        int32_t _M0L3endS2945 = _M0L4viewS1078.$2;
        int64_t _M0L6_2atmpS2944 = (int64_t)_M0L3endS2945;
        int64_t _M0L6_2atmpS2941;
        int32_t _M0L6_2atmpS2940;
        int32_t _M0L4_2axS1107;
        moonbit_incref(_M0L3strS2942);
        moonbit_incref(_M0L3strS2939);
        #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        _M0L6_2atmpS2941
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2942, 1, _M0L5startS2943, _M0L6_2atmpS2944);
        _M0L6_2atmpS2940 = (int32_t)_M0L6_2atmpS2941;
        #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
        _M0L4_2axS1107
        = _M0MPC16string6String16unsafe__char__at(_M0L3strS2939, _M0L6_2atmpS2940);
        if (_M0L4_2axS1107 == 120) {
          moonbit_string_t _M0L8_2afieldS3746 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2932 = _M0L8_2afieldS3746;
          moonbit_string_t _M0L8_2afieldS3745 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2935 = _M0L8_2afieldS3745;
          int32_t _M0L5startS2936 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2938 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2937 = (int64_t)_M0L3endS2938;
          int64_t _M0L7_2abindS1484;
          int32_t _M0L6_2atmpS2933;
          int32_t _M0L3endS2934;
          struct _M0TPC16string10StringView _M0L4_2axS1108;
          moonbit_incref(_M0L3strS2935);
          moonbit_incref(_M0L3strS2932);
          #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1484
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2935, 2, _M0L5startS2936, _M0L6_2atmpS2937);
          if (_M0L7_2abindS1484 == 4294967296ll) {
            _M0L6_2atmpS2933 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1109 = _M0L7_2abindS1484;
            _M0L6_2atmpS2933 = (int32_t)_M0L7_2aSomeS1109;
          }
          _M0L3endS2934 = _M0L4viewS1078.$2;
          _M0L4_2axS1108
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2933, _M0L3endS2934, _M0L3strS2932
          };
          if (_M0L4baseS1076 == 16) {
            moonbit_decref(_M0L4viewS1078.$0);
            _M0L4restS1105 = _M0L4_2axS1108;
            goto join_1104;
          } else {
            moonbit_decref(_M0L4_2axS1108.$0);
            goto join_1099;
          }
        } else if (_M0L4_2axS1107 == 88) {
          moonbit_string_t _M0L8_2afieldS3748 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2925 = _M0L8_2afieldS3748;
          moonbit_string_t _M0L8_2afieldS3747 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2928 = _M0L8_2afieldS3747;
          int32_t _M0L5startS2929 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2931 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2930 = (int64_t)_M0L3endS2931;
          int64_t _M0L7_2abindS1485;
          int32_t _M0L6_2atmpS2926;
          int32_t _M0L3endS2927;
          struct _M0TPC16string10StringView _M0L4_2axS1110;
          moonbit_incref(_M0L3strS2928);
          moonbit_incref(_M0L3strS2925);
          #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1485
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2928, 2, _M0L5startS2929, _M0L6_2atmpS2930);
          if (_M0L7_2abindS1485 == 4294967296ll) {
            _M0L6_2atmpS2926 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1111 = _M0L7_2abindS1485;
            _M0L6_2atmpS2926 = (int32_t)_M0L7_2aSomeS1111;
          }
          _M0L3endS2927 = _M0L4viewS1078.$2;
          _M0L4_2axS1110
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2926, _M0L3endS2927, _M0L3strS2925
          };
          if (_M0L4baseS1076 == 16) {
            moonbit_decref(_M0L4viewS1078.$0);
            _M0L4restS1105 = _M0L4_2axS1110;
            goto join_1104;
          } else {
            moonbit_decref(_M0L4_2axS1110.$0);
            goto join_1099;
          }
        } else if (_M0L4_2axS1107 == 111) {
          moonbit_string_t _M0L8_2afieldS3750 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2918 = _M0L8_2afieldS3750;
          moonbit_string_t _M0L8_2afieldS3749 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2921 = _M0L8_2afieldS3749;
          int32_t _M0L5startS2922 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2924 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2923 = (int64_t)_M0L3endS2924;
          int64_t _M0L7_2abindS1486;
          int32_t _M0L6_2atmpS2919;
          int32_t _M0L3endS2920;
          struct _M0TPC16string10StringView _M0L4_2axS1112;
          moonbit_incref(_M0L3strS2921);
          moonbit_incref(_M0L3strS2918);
          #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1486
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2921, 2, _M0L5startS2922, _M0L6_2atmpS2923);
          if (_M0L7_2abindS1486 == 4294967296ll) {
            _M0L6_2atmpS2919 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1113 = _M0L7_2abindS1486;
            _M0L6_2atmpS2919 = (int32_t)_M0L7_2aSomeS1113;
          }
          _M0L3endS2920 = _M0L4viewS1078.$2;
          _M0L4_2axS1112
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2919, _M0L3endS2920, _M0L3strS2918
          };
          if (_M0L4baseS1076 == 8) {
            moonbit_decref(_M0L4viewS1078.$0);
            _M0L4restS1103 = _M0L4_2axS1112;
            goto join_1102;
          } else {
            moonbit_decref(_M0L4_2axS1112.$0);
            goto join_1099;
          }
        } else if (_M0L4_2axS1107 == 79) {
          moonbit_string_t _M0L8_2afieldS3752 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2911 = _M0L8_2afieldS3752;
          moonbit_string_t _M0L8_2afieldS3751 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2914 = _M0L8_2afieldS3751;
          int32_t _M0L5startS2915 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2917 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2916 = (int64_t)_M0L3endS2917;
          int64_t _M0L7_2abindS1487;
          int32_t _M0L6_2atmpS2912;
          int32_t _M0L3endS2913;
          struct _M0TPC16string10StringView _M0L4_2axS1114;
          moonbit_incref(_M0L3strS2914);
          moonbit_incref(_M0L3strS2911);
          #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1487
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2914, 2, _M0L5startS2915, _M0L6_2atmpS2916);
          if (_M0L7_2abindS1487 == 4294967296ll) {
            _M0L6_2atmpS2912 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1115 = _M0L7_2abindS1487;
            _M0L6_2atmpS2912 = (int32_t)_M0L7_2aSomeS1115;
          }
          _M0L3endS2913 = _M0L4viewS1078.$2;
          _M0L4_2axS1114
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2912, _M0L3endS2913, _M0L3strS2911
          };
          if (_M0L4baseS1076 == 8) {
            moonbit_decref(_M0L4viewS1078.$0);
            _M0L4restS1103 = _M0L4_2axS1114;
            goto join_1102;
          } else {
            moonbit_decref(_M0L4_2axS1114.$0);
            goto join_1099;
          }
        } else if (_M0L4_2axS1107 == 98) {
          moonbit_string_t _M0L8_2afieldS3754 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2904 = _M0L8_2afieldS3754;
          moonbit_string_t _M0L8_2afieldS3753 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2907 = _M0L8_2afieldS3753;
          int32_t _M0L5startS2908 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2910 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2909 = (int64_t)_M0L3endS2910;
          int64_t _M0L7_2abindS1488;
          int32_t _M0L6_2atmpS2905;
          int32_t _M0L3endS2906;
          struct _M0TPC16string10StringView _M0L4_2axS1116;
          moonbit_incref(_M0L3strS2907);
          moonbit_incref(_M0L3strS2904);
          #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1488
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2907, 2, _M0L5startS2908, _M0L6_2atmpS2909);
          if (_M0L7_2abindS1488 == 4294967296ll) {
            _M0L6_2atmpS2905 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1117 = _M0L7_2abindS1488;
            _M0L6_2atmpS2905 = (int32_t)_M0L7_2aSomeS1117;
          }
          _M0L3endS2906 = _M0L4viewS1078.$2;
          _M0L4_2axS1116
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2905, _M0L3endS2906, _M0L3strS2904
          };
          if (_M0L4baseS1076 == 2) {
            moonbit_decref(_M0L4viewS1078.$0);
            _M0L4restS1101 = _M0L4_2axS1116;
            goto join_1100;
          } else {
            moonbit_decref(_M0L4_2axS1116.$0);
            goto join_1099;
          }
        } else if (_M0L4_2axS1107 == 66) {
          moonbit_string_t _M0L8_2afieldS3756 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2897 = _M0L8_2afieldS3756;
          moonbit_string_t _M0L8_2afieldS3755 = _M0L4viewS1078.$0;
          moonbit_string_t _M0L3strS2900 = _M0L8_2afieldS3755;
          int32_t _M0L5startS2901 = _M0L4viewS1078.$1;
          int32_t _M0L3endS2903 = _M0L4viewS1078.$2;
          int64_t _M0L6_2atmpS2902 = (int64_t)_M0L3endS2903;
          int64_t _M0L7_2abindS1489;
          int32_t _M0L6_2atmpS2898;
          int32_t _M0L3endS2899;
          struct _M0TPC16string10StringView _M0L4_2axS1118;
          moonbit_incref(_M0L3strS2900);
          moonbit_incref(_M0L3strS2897);
          #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
          _M0L7_2abindS1489
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2900, 2, _M0L5startS2901, _M0L6_2atmpS2902);
          if (_M0L7_2abindS1489 == 4294967296ll) {
            _M0L6_2atmpS2898 = _M0L4viewS1078.$2;
          } else {
            int64_t _M0L7_2aSomeS1119 = _M0L7_2abindS1489;
            _M0L6_2atmpS2898 = (int32_t)_M0L7_2aSomeS1119;
          }
          _M0L3endS2899 = _M0L4viewS1078.$2;
          _M0L4_2axS1118
          = (struct _M0TPC16string10StringView){
            _M0L6_2atmpS2898, _M0L3endS2899, _M0L3strS2897
          };
          if (_M0L4baseS1076 == 2) {
            moonbit_decref(_M0L4viewS1078.$0);
            _M0L4restS1101 = _M0L4_2axS1118;
            goto join_1100;
          } else {
            moonbit_decref(_M0L4_2axS1118.$0);
            goto join_1099;
          }
        } else {
          goto join_1099;
        }
      } else {
        goto join_1099;
      }
    } else {
      goto join_1099;
    }
    join_1104:;
    _M0L8_2atupleS2892
    = (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
    Moonbit_object_header(_M0L8_2atupleS2892)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS2892->$0 = 16;
    _M0L8_2atupleS2892->$1_0 = _M0L4restS1105.$0;
    _M0L8_2atupleS2892->$1_1 = _M0L4restS1105.$1;
    _M0L8_2atupleS2892->$1_2 = _M0L4restS1105.$2;
    _M0L8_2atupleS2892->$2 = 1;
    _result_4246.tag = 1;
    _result_4246.data.ok = _M0L8_2atupleS2892;
    return _result_4246;
    join_1102:;
    _M0L8_2atupleS2891
    = (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
    Moonbit_object_header(_M0L8_2atupleS2891)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS2891->$0 = 8;
    _M0L8_2atupleS2891->$1_0 = _M0L4restS1103.$0;
    _M0L8_2atupleS2891->$1_1 = _M0L4restS1103.$1;
    _M0L8_2atupleS2891->$1_2 = _M0L4restS1103.$2;
    _M0L8_2atupleS2891->$2 = 1;
    _result_4247.tag = 1;
    _result_4247.data.ok = _M0L8_2atupleS2891;
    return _result_4247;
    join_1100:;
    _M0L8_2atupleS2890
    = (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
    Moonbit_object_header(_M0L8_2atupleS2890)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS2890->$0 = 2;
    _M0L8_2atupleS2890->$1_0 = _M0L4restS1101.$0;
    _M0L8_2atupleS2890->$1_1 = _M0L4restS1101.$1;
    _M0L8_2atupleS2890->$1_2 = _M0L4restS1101.$2;
    _M0L8_2atupleS2890->$2 = 1;
    _result_4248.tag = 1;
    _result_4248.data.ok = _M0L8_2atupleS2890;
    return _result_4248;
    join_1099:;
    if (_M0L4baseS1076 >= 2 && _M0L4baseS1076 <= 36) {
      struct _M0TUiRPC16string10StringViewbE* _M0L8_2atupleS2889 =
        (struct _M0TUiRPC16string10StringViewbE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewbE));
      struct moonbit_result_3 _result_4249;
      Moonbit_object_header(_M0L8_2atupleS2889)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewbE, $1_0) >> 2, 1, 0);
      _M0L8_2atupleS2889->$0 = _M0L4baseS1076;
      _M0L8_2atupleS2889->$1_0 = _M0L4viewS1078.$0;
      _M0L8_2atupleS2889->$1_1 = _M0L4viewS1078.$1;
      _M0L8_2atupleS2889->$1_2 = _M0L4viewS1078.$2;
      _M0L8_2atupleS2889->$2 = 0;
      _result_4249.tag = 1;
      _result_4249.data.ok = _M0L8_2atupleS2889;
      return _result_4249;
    } else {
      moonbit_decref(_M0L4viewS1078.$0);
      #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\int.mbt"
      return _M0FPC17strconv9base__errGUiRPC16string10StringViewbEE();
    }
  }
}

struct moonbit_result_3 _M0FPC17strconv9base__errGUiRPC16string10StringViewbEE(
  
) {
  void* _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2824;
  struct moonbit_result_3 _result_4250;
  #line 48 "C:\\Users\\Administrator\\.moon\\lib\\core\\strconv\\errors.mbt"
  moonbit_incref(_M0FPC17strconv14base__err__str);
  _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2824
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError));
  Moonbit_object_header(_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2824)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvError*)_M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2824)->$0
  = _M0FPC17strconv14base__err__str;
  _result_4250.tag = 0;
  _result_4250.data.err
  = _M0L58moonbitlang_2fcore_2fstrconv_2eStrConvError_2eStrConvErrorS2824;
  return _result_4250;
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1075
) {
  int32_t _M0L8_2afieldS3762;
  int32_t _M0L3lenS2823;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3762 = _M0L4selfS1075->$1;
  moonbit_decref(_M0L4selfS1075);
  _M0L3lenS2823 = _M0L8_2afieldS3762;
  return _M0L3lenS2823 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1072
) {
  int32_t _M0L3lenS1071;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1071 = _M0L4selfS1072->$1;
  if (_M0L3lenS1071 == 0) {
    moonbit_decref(_M0L4selfS1072);
    return 0;
  } else {
    int32_t _M0L5indexS1073 = _M0L3lenS1071 - 1;
    void** _M0L8_2afieldS3766 = _M0L4selfS1072->$0;
    void** _M0L3bufS2822 = _M0L8_2afieldS3766;
    void* _M0L6_2atmpS3765 = (void*)_M0L3bufS2822[_M0L5indexS1073];
    void* _M0L1vS1074 = _M0L6_2atmpS3765;
    void** _M0L8_2afieldS3764 = _M0L4selfS1072->$0;
    void** _M0L3bufS2821 = _M0L8_2afieldS3764;
    void* _M0L6_2aoldS3763;
    if (
      _M0L5indexS1073 < 0
      || _M0L5indexS1073 >= Moonbit_array_length(_M0L3bufS2821)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3763 = (void*)_M0L3bufS2821[_M0L5indexS1073];
    moonbit_incref(_M0L1vS1074);
    moonbit_decref(_M0L6_2aoldS3763);
    if (
      _M0L5indexS1073 < 0
      || _M0L5indexS1073 >= Moonbit_array_length(_M0L3bufS2821)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2821[_M0L5indexS1073]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1072->$1 = _M0L5indexS1073;
    moonbit_decref(_M0L4selfS1072);
    return _M0L1vS1074;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1069,
  struct _M0TPB6Logger _M0L6loggerS1070
) {
  moonbit_string_t _M0L6_2atmpS2820;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2819;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2820 = _M0L4selfS1069;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2819 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2820);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2819, _M0L6loggerS1070);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1046,
  struct _M0TPB6Logger _M0L6loggerS1068
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3775;
  struct _M0TPC16string10StringView _M0L3pkgS1045;
  moonbit_string_t _M0L7_2adataS1047;
  int32_t _M0L8_2astartS1048;
  int32_t _M0L6_2atmpS2818;
  int32_t _M0L6_2aendS1049;
  int32_t _M0Lm9_2acursorS1050;
  int32_t _M0Lm13accept__stateS1051;
  int32_t _M0Lm10match__endS1052;
  int32_t _M0Lm20match__tag__saver__0S1053;
  int32_t _M0Lm6tag__0S1054;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1055;
  struct _M0TPC16string10StringView _M0L8_2afieldS3774;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1064;
  void* _M0L8_2afieldS3773;
  int32_t _M0L6_2acntS4037;
  void* _M0L16_2apackage__nameS1065;
  struct _M0TPC16string10StringView _M0L8_2afieldS3771;
  struct _M0TPC16string10StringView _M0L8filenameS2795;
  struct _M0TPC16string10StringView _M0L8_2afieldS3770;
  struct _M0TPC16string10StringView _M0L11start__lineS2796;
  struct _M0TPC16string10StringView _M0L8_2afieldS3769;
  struct _M0TPC16string10StringView _M0L13start__columnS2797;
  struct _M0TPC16string10StringView _M0L8_2afieldS3768;
  struct _M0TPC16string10StringView _M0L9end__lineS2798;
  struct _M0TPC16string10StringView _M0L8_2afieldS3767;
  int32_t _M0L6_2acntS4041;
  struct _M0TPC16string10StringView _M0L11end__columnS2799;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3775
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1046->$0_1, _M0L4selfS1046->$0_2, _M0L4selfS1046->$0_0
  };
  _M0L3pkgS1045 = _M0L8_2afieldS3775;
  moonbit_incref(_M0L3pkgS1045.$0);
  moonbit_incref(_M0L3pkgS1045.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1047 = _M0MPC16string10StringView4data(_M0L3pkgS1045);
  moonbit_incref(_M0L3pkgS1045.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1048
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1045);
  moonbit_incref(_M0L3pkgS1045.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2818 = _M0MPC16string10StringView6length(_M0L3pkgS1045);
  _M0L6_2aendS1049 = _M0L8_2astartS1048 + _M0L6_2atmpS2818;
  _M0Lm9_2acursorS1050 = _M0L8_2astartS1048;
  _M0Lm13accept__stateS1051 = -1;
  _M0Lm10match__endS1052 = -1;
  _M0Lm20match__tag__saver__0S1053 = -1;
  _M0Lm6tag__0S1054 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2810 = _M0Lm9_2acursorS1050;
    if (_M0L6_2atmpS2810 < _M0L6_2aendS1049) {
      int32_t _M0L6_2atmpS2817 = _M0Lm9_2acursorS1050;
      int32_t _M0L10next__charS1059;
      int32_t _M0L6_2atmpS2811;
      moonbit_incref(_M0L7_2adataS1047);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1059
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1047, _M0L6_2atmpS2817);
      _M0L6_2atmpS2811 = _M0Lm9_2acursorS1050;
      _M0Lm9_2acursorS1050 = _M0L6_2atmpS2811 + 1;
      if (_M0L10next__charS1059 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2812;
          _M0Lm6tag__0S1054 = _M0Lm9_2acursorS1050;
          _M0L6_2atmpS2812 = _M0Lm9_2acursorS1050;
          if (_M0L6_2atmpS2812 < _M0L6_2aendS1049) {
            int32_t _M0L6_2atmpS2816 = _M0Lm9_2acursorS1050;
            int32_t _M0L10next__charS1060;
            int32_t _M0L6_2atmpS2813;
            moonbit_incref(_M0L7_2adataS1047);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1060
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1047, _M0L6_2atmpS2816);
            _M0L6_2atmpS2813 = _M0Lm9_2acursorS1050;
            _M0Lm9_2acursorS1050 = _M0L6_2atmpS2813 + 1;
            if (_M0L10next__charS1060 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2814 = _M0Lm9_2acursorS1050;
                if (_M0L6_2atmpS2814 < _M0L6_2aendS1049) {
                  int32_t _M0L6_2atmpS2815 = _M0Lm9_2acursorS1050;
                  _M0Lm9_2acursorS1050 = _M0L6_2atmpS2815 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1053 = _M0Lm6tag__0S1054;
                  _M0Lm13accept__stateS1051 = 0;
                  _M0Lm10match__endS1052 = _M0Lm9_2acursorS1050;
                  goto join_1056;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1056;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1056;
    }
    break;
  }
  goto joinlet_4251;
  join_1056:;
  switch (_M0Lm13accept__stateS1051) {
    case 0: {
      int32_t _M0L6_2atmpS2808;
      int32_t _M0L6_2atmpS2807;
      int64_t _M0L6_2atmpS2804;
      int32_t _M0L6_2atmpS2806;
      int64_t _M0L6_2atmpS2805;
      struct _M0TPC16string10StringView _M0L13package__nameS1057;
      int64_t _M0L6_2atmpS2801;
      int32_t _M0L6_2atmpS2803;
      int64_t _M0L6_2atmpS2802;
      struct _M0TPC16string10StringView _M0L12module__nameS1058;
      void* _M0L4SomeS2800;
      moonbit_decref(_M0L3pkgS1045.$0);
      _M0L6_2atmpS2808 = _M0Lm20match__tag__saver__0S1053;
      _M0L6_2atmpS2807 = _M0L6_2atmpS2808 + 1;
      _M0L6_2atmpS2804 = (int64_t)_M0L6_2atmpS2807;
      _M0L6_2atmpS2806 = _M0Lm10match__endS1052;
      _M0L6_2atmpS2805 = (int64_t)_M0L6_2atmpS2806;
      moonbit_incref(_M0L7_2adataS1047);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1057
      = _M0MPC16string6String4view(_M0L7_2adataS1047, _M0L6_2atmpS2804, _M0L6_2atmpS2805);
      _M0L6_2atmpS2801 = (int64_t)_M0L8_2astartS1048;
      _M0L6_2atmpS2803 = _M0Lm20match__tag__saver__0S1053;
      _M0L6_2atmpS2802 = (int64_t)_M0L6_2atmpS2803;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1058
      = _M0MPC16string6String4view(_M0L7_2adataS1047, _M0L6_2atmpS2801, _M0L6_2atmpS2802);
      _M0L4SomeS2800
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2800)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2800)->$0_0
      = _M0L13package__nameS1057.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2800)->$0_1
      = _M0L13package__nameS1057.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2800)->$0_2
      = _M0L13package__nameS1057.$2;
      _M0L7_2abindS1055
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1055)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1055->$0_0 = _M0L12module__nameS1058.$0;
      _M0L7_2abindS1055->$0_1 = _M0L12module__nameS1058.$1;
      _M0L7_2abindS1055->$0_2 = _M0L12module__nameS1058.$2;
      _M0L7_2abindS1055->$1 = _M0L4SomeS2800;
      break;
    }
    default: {
      void* _M0L4NoneS2809;
      moonbit_decref(_M0L7_2adataS1047);
      _M0L4NoneS2809
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1055
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1055)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1055->$0_0 = _M0L3pkgS1045.$0;
      _M0L7_2abindS1055->$0_1 = _M0L3pkgS1045.$1;
      _M0L7_2abindS1055->$0_2 = _M0L3pkgS1045.$2;
      _M0L7_2abindS1055->$1 = _M0L4NoneS2809;
      break;
    }
  }
  joinlet_4251:;
  _M0L8_2afieldS3774
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1055->$0_1, _M0L7_2abindS1055->$0_2, _M0L7_2abindS1055->$0_0
  };
  _M0L15_2amodule__nameS1064 = _M0L8_2afieldS3774;
  _M0L8_2afieldS3773 = _M0L7_2abindS1055->$1;
  _M0L6_2acntS4037 = Moonbit_object_header(_M0L7_2abindS1055)->rc;
  if (_M0L6_2acntS4037 > 1) {
    int32_t _M0L11_2anew__cntS4038 = _M0L6_2acntS4037 - 1;
    Moonbit_object_header(_M0L7_2abindS1055)->rc = _M0L11_2anew__cntS4038;
    moonbit_incref(_M0L8_2afieldS3773);
    moonbit_incref(_M0L15_2amodule__nameS1064.$0);
  } else if (_M0L6_2acntS4037 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1055);
  }
  _M0L16_2apackage__nameS1065 = _M0L8_2afieldS3773;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1065)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1066 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1065;
      struct _M0TPC16string10StringView _M0L8_2afieldS3772 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1066->$0_1,
                                              _M0L7_2aSomeS1066->$0_2,
                                              _M0L7_2aSomeS1066->$0_0};
      int32_t _M0L6_2acntS4039 = Moonbit_object_header(_M0L7_2aSomeS1066)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1067;
      if (_M0L6_2acntS4039 > 1) {
        int32_t _M0L11_2anew__cntS4040 = _M0L6_2acntS4039 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1066)->rc = _M0L11_2anew__cntS4040;
        moonbit_incref(_M0L8_2afieldS3772.$0);
      } else if (_M0L6_2acntS4039 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1066);
      }
      _M0L12_2apkg__nameS1067 = _M0L8_2afieldS3772;
      if (_M0L6loggerS1068.$1) {
        moonbit_incref(_M0L6loggerS1068.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1068.$0->$method_2(_M0L6loggerS1068.$1, _M0L12_2apkg__nameS1067);
      if (_M0L6loggerS1068.$1) {
        moonbit_incref(_M0L6loggerS1068.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1068.$0->$method_3(_M0L6loggerS1068.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1065);
      break;
    }
  }
  _M0L8_2afieldS3771
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1046->$1_1, _M0L4selfS1046->$1_2, _M0L4selfS1046->$1_0
  };
  _M0L8filenameS2795 = _M0L8_2afieldS3771;
  moonbit_incref(_M0L8filenameS2795.$0);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_2(_M0L6loggerS1068.$1, _M0L8filenameS2795);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_3(_M0L6loggerS1068.$1, 58);
  _M0L8_2afieldS3770
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1046->$2_1, _M0L4selfS1046->$2_2, _M0L4selfS1046->$2_0
  };
  _M0L11start__lineS2796 = _M0L8_2afieldS3770;
  moonbit_incref(_M0L11start__lineS2796.$0);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_2(_M0L6loggerS1068.$1, _M0L11start__lineS2796);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_3(_M0L6loggerS1068.$1, 58);
  _M0L8_2afieldS3769
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1046->$3_1, _M0L4selfS1046->$3_2, _M0L4selfS1046->$3_0
  };
  _M0L13start__columnS2797 = _M0L8_2afieldS3769;
  moonbit_incref(_M0L13start__columnS2797.$0);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_2(_M0L6loggerS1068.$1, _M0L13start__columnS2797);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_3(_M0L6loggerS1068.$1, 45);
  _M0L8_2afieldS3768
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1046->$4_1, _M0L4selfS1046->$4_2, _M0L4selfS1046->$4_0
  };
  _M0L9end__lineS2798 = _M0L8_2afieldS3768;
  moonbit_incref(_M0L9end__lineS2798.$0);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_2(_M0L6loggerS1068.$1, _M0L9end__lineS2798);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_3(_M0L6loggerS1068.$1, 58);
  _M0L8_2afieldS3767
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1046->$5_1, _M0L4selfS1046->$5_2, _M0L4selfS1046->$5_0
  };
  _M0L6_2acntS4041 = Moonbit_object_header(_M0L4selfS1046)->rc;
  if (_M0L6_2acntS4041 > 1) {
    int32_t _M0L11_2anew__cntS4047 = _M0L6_2acntS4041 - 1;
    Moonbit_object_header(_M0L4selfS1046)->rc = _M0L11_2anew__cntS4047;
    moonbit_incref(_M0L8_2afieldS3767.$0);
  } else if (_M0L6_2acntS4041 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4046 =
      (struct _M0TPC16string10StringView){_M0L4selfS1046->$4_1,
                                            _M0L4selfS1046->$4_2,
                                            _M0L4selfS1046->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4045;
    struct _M0TPC16string10StringView _M0L8_2afieldS4044;
    struct _M0TPC16string10StringView _M0L8_2afieldS4043;
    struct _M0TPC16string10StringView _M0L8_2afieldS4042;
    moonbit_decref(_M0L8_2afieldS4046.$0);
    _M0L8_2afieldS4045
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1046->$3_1, _M0L4selfS1046->$3_2, _M0L4selfS1046->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4045.$0);
    _M0L8_2afieldS4044
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1046->$2_1, _M0L4selfS1046->$2_2, _M0L4selfS1046->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4044.$0);
    _M0L8_2afieldS4043
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1046->$1_1, _M0L4selfS1046->$1_2, _M0L4selfS1046->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4043.$0);
    _M0L8_2afieldS4042
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1046->$0_1, _M0L4selfS1046->$0_2, _M0L4selfS1046->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4042.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1046);
  }
  _M0L11end__columnS2799 = _M0L8_2afieldS3767;
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_2(_M0L6loggerS1068.$1, _M0L11end__columnS2799);
  if (_M0L6loggerS1068.$1) {
    moonbit_incref(_M0L6loggerS1068.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_3(_M0L6loggerS1068.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1068.$0->$method_2(_M0L6loggerS1068.$1, _M0L15_2amodule__nameS1064);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1044) {
  moonbit_string_t _M0L6_2atmpS2794;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2794
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1044);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2794);
  moonbit_decref(_M0L6_2atmpS2794);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1043,
  struct _M0TPB6Logger _M0L6loggerS1042
) {
  moonbit_string_t _M0L6_2atmpS2793;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2793 = _M0MPC16double6Double10to__string(_M0L4selfS1043);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1042.$0->$method_0(_M0L6loggerS1042.$1, _M0L6_2atmpS2793);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1041) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1041);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1028) {
  uint64_t _M0L4bitsS1029;
  uint64_t _M0L6_2atmpS2792;
  uint64_t _M0L6_2atmpS2791;
  int32_t _M0L8ieeeSignS1030;
  uint64_t _M0L12ieeeMantissaS1031;
  uint64_t _M0L6_2atmpS2790;
  uint64_t _M0L6_2atmpS2789;
  int32_t _M0L12ieeeExponentS1032;
  int32_t _if__result_4255;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1033;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1034;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2788;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1028 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
  }
  _M0L4bitsS1029 = *(int64_t*)&_M0L3valS1028;
  _M0L6_2atmpS2792 = _M0L4bitsS1029 >> 63;
  _M0L6_2atmpS2791 = _M0L6_2atmpS2792 & 1ull;
  _M0L8ieeeSignS1030 = _M0L6_2atmpS2791 != 0ull;
  _M0L12ieeeMantissaS1031 = _M0L4bitsS1029 & 4503599627370495ull;
  _M0L6_2atmpS2790 = _M0L4bitsS1029 >> 52;
  _M0L6_2atmpS2789 = _M0L6_2atmpS2790 & 2047ull;
  _M0L12ieeeExponentS1032 = (int32_t)_M0L6_2atmpS2789;
  if (_M0L12ieeeExponentS1032 == 2047) {
    _if__result_4255 = 1;
  } else if (_M0L12ieeeExponentS1032 == 0) {
    _if__result_4255 = _M0L12ieeeMantissaS1031 == 0ull;
  } else {
    _if__result_4255 = 0;
  }
  if (_if__result_4255) {
    int32_t _M0L6_2atmpS2777 = _M0L12ieeeExponentS1032 != 0;
    int32_t _M0L6_2atmpS2778 = _M0L12ieeeMantissaS1031 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1030, _M0L6_2atmpS2777, _M0L6_2atmpS2778);
  }
  _M0Lm1vS1033 = _M0FPB31ryu__to__string_2erecord_2f1027;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1034
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1031, _M0L12ieeeExponentS1032);
  if (_M0L5smallS1034 == 0) {
    uint32_t _M0L6_2atmpS2779;
    if (_M0L5smallS1034) {
      moonbit_decref(_M0L5smallS1034);
    }
    _M0L6_2atmpS2779 = *(uint32_t*)&_M0L12ieeeExponentS1032;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1033 = _M0FPB3d2d(_M0L12ieeeMantissaS1031, _M0L6_2atmpS2779);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1035 = _M0L5smallS1034;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1036 = _M0L7_2aSomeS1035;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1037 = _M0L4_2afS1036;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2787 = _M0Lm1xS1037;
      uint64_t _M0L8_2afieldS3778 = _M0L6_2atmpS2787->$0;
      uint64_t _M0L8mantissaS2786 = _M0L8_2afieldS3778;
      uint64_t _M0L1qS1038 = _M0L8mantissaS2786 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2785 = _M0Lm1xS1037;
      uint64_t _M0L8_2afieldS3777 = _M0L6_2atmpS2785->$0;
      uint64_t _M0L8mantissaS2783 = _M0L8_2afieldS3777;
      uint64_t _M0L6_2atmpS2784 = 10ull * _M0L1qS1038;
      uint64_t _M0L1rS1039 = _M0L8mantissaS2783 - _M0L6_2atmpS2784;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2782;
      int32_t _M0L8_2afieldS3776;
      int32_t _M0L8exponentS2781;
      int32_t _M0L6_2atmpS2780;
      if (_M0L1rS1039 != 0ull) {
        break;
      }
      _M0L6_2atmpS2782 = _M0Lm1xS1037;
      _M0L8_2afieldS3776 = _M0L6_2atmpS2782->$1;
      moonbit_decref(_M0L6_2atmpS2782);
      _M0L8exponentS2781 = _M0L8_2afieldS3776;
      _M0L6_2atmpS2780 = _M0L8exponentS2781 + 1;
      _M0Lm1xS1037
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1037)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1037->$0 = _M0L1qS1038;
      _M0Lm1xS1037->$1 = _M0L6_2atmpS2780;
      continue;
      break;
    }
    _M0Lm1vS1033 = _M0Lm1xS1037;
  }
  _M0L6_2atmpS2788 = _M0Lm1vS1033;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2788, _M0L8ieeeSignS1030);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1022,
  int32_t _M0L12ieeeExponentS1024
) {
  uint64_t _M0L2m2S1021;
  int32_t _M0L6_2atmpS2776;
  int32_t _M0L2e2S1023;
  int32_t _M0L6_2atmpS2775;
  uint64_t _M0L6_2atmpS2774;
  uint64_t _M0L4maskS1025;
  uint64_t _M0L8fractionS1026;
  int32_t _M0L6_2atmpS2773;
  uint64_t _M0L6_2atmpS2772;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2771;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1021 = 4503599627370496ull | _M0L12ieeeMantissaS1022;
  _M0L6_2atmpS2776 = _M0L12ieeeExponentS1024 - 1023;
  _M0L2e2S1023 = _M0L6_2atmpS2776 - 52;
  if (_M0L2e2S1023 > 0) {
    return 0;
  }
  if (_M0L2e2S1023 < -52) {
    return 0;
  }
  _M0L6_2atmpS2775 = -_M0L2e2S1023;
  _M0L6_2atmpS2774 = 1ull << (_M0L6_2atmpS2775 & 63);
  _M0L4maskS1025 = _M0L6_2atmpS2774 - 1ull;
  _M0L8fractionS1026 = _M0L2m2S1021 & _M0L4maskS1025;
  if (_M0L8fractionS1026 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2773 = -_M0L2e2S1023;
  _M0L6_2atmpS2772 = _M0L2m2S1021 >> (_M0L6_2atmpS2773 & 63);
  _M0L6_2atmpS2771
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2771)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2771->$0 = _M0L6_2atmpS2772;
  _M0L6_2atmpS2771->$1 = 0;
  return _M0L6_2atmpS2771;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS995,
  int32_t _M0L4signS993
) {
  int32_t _M0L6_2atmpS2770;
  moonbit_bytes_t _M0L6resultS991;
  int32_t _M0Lm5indexS992;
  uint64_t _M0Lm6outputS994;
  uint64_t _M0L6_2atmpS2769;
  int32_t _M0L7olengthS996;
  int32_t _M0L8_2afieldS3779;
  int32_t _M0L8exponentS2768;
  int32_t _M0L6_2atmpS2767;
  int32_t _M0Lm3expS997;
  int32_t _M0L6_2atmpS2766;
  int32_t _M0L6_2atmpS2764;
  int32_t _M0L18scientificNotationS998;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2770 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS991 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2770);
  _M0Lm5indexS992 = 0;
  if (_M0L4signS993) {
    int32_t _M0L6_2atmpS2639 = _M0Lm5indexS992;
    int32_t _M0L6_2atmpS2640;
    if (
      _M0L6_2atmpS2639 < 0
      || _M0L6_2atmpS2639 >= Moonbit_array_length(_M0L6resultS991)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS991[_M0L6_2atmpS2639] = 45;
    _M0L6_2atmpS2640 = _M0Lm5indexS992;
    _M0Lm5indexS992 = _M0L6_2atmpS2640 + 1;
  }
  _M0Lm6outputS994 = _M0L1vS995->$0;
  _M0L6_2atmpS2769 = _M0Lm6outputS994;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS996 = _M0FPB17decimal__length17(_M0L6_2atmpS2769);
  _M0L8_2afieldS3779 = _M0L1vS995->$1;
  moonbit_decref(_M0L1vS995);
  _M0L8exponentS2768 = _M0L8_2afieldS3779;
  _M0L6_2atmpS2767 = _M0L8exponentS2768 + _M0L7olengthS996;
  _M0Lm3expS997 = _M0L6_2atmpS2767 - 1;
  _M0L6_2atmpS2766 = _M0Lm3expS997;
  if (_M0L6_2atmpS2766 >= -6) {
    int32_t _M0L6_2atmpS2765 = _M0Lm3expS997;
    _M0L6_2atmpS2764 = _M0L6_2atmpS2765 < 21;
  } else {
    _M0L6_2atmpS2764 = 0;
  }
  _M0L18scientificNotationS998 = !_M0L6_2atmpS2764;
  if (_M0L18scientificNotationS998) {
    int32_t _M0L7_2abindS999 = _M0L7olengthS996 - 1;
    int32_t _M0L1iS1000 = 0;
    int32_t _M0L6_2atmpS2650;
    uint64_t _M0L6_2atmpS2655;
    int32_t _M0L6_2atmpS2654;
    int32_t _M0L6_2atmpS2653;
    int32_t _M0L6_2atmpS2652;
    int32_t _M0L6_2atmpS2651;
    int32_t _M0L6_2atmpS2659;
    int32_t _M0L6_2atmpS2660;
    int32_t _M0L6_2atmpS2661;
    int32_t _M0L6_2atmpS2662;
    int32_t _M0L6_2atmpS2663;
    int32_t _M0L6_2atmpS2669;
    int32_t _M0L6_2atmpS2702;
    while (1) {
      if (_M0L1iS1000 < _M0L7_2abindS999) {
        uint64_t _M0L6_2atmpS2648 = _M0Lm6outputS994;
        uint64_t _M0L1cS1001 = _M0L6_2atmpS2648 % 10ull;
        uint64_t _M0L6_2atmpS2641 = _M0Lm6outputS994;
        int32_t _M0L6_2atmpS2647;
        int32_t _M0L6_2atmpS2646;
        int32_t _M0L6_2atmpS2642;
        int32_t _M0L6_2atmpS2645;
        int32_t _M0L6_2atmpS2644;
        int32_t _M0L6_2atmpS2643;
        int32_t _M0L6_2atmpS2649;
        _M0Lm6outputS994 = _M0L6_2atmpS2641 / 10ull;
        _M0L6_2atmpS2647 = _M0Lm5indexS992;
        _M0L6_2atmpS2646 = _M0L6_2atmpS2647 + _M0L7olengthS996;
        _M0L6_2atmpS2642 = _M0L6_2atmpS2646 - _M0L1iS1000;
        _M0L6_2atmpS2645 = (int32_t)_M0L1cS1001;
        _M0L6_2atmpS2644 = 48 + _M0L6_2atmpS2645;
        _M0L6_2atmpS2643 = _M0L6_2atmpS2644 & 0xff;
        if (
          _M0L6_2atmpS2642 < 0
          || _M0L6_2atmpS2642 >= Moonbit_array_length(_M0L6resultS991)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS991[_M0L6_2atmpS2642] = _M0L6_2atmpS2643;
        _M0L6_2atmpS2649 = _M0L1iS1000 + 1;
        _M0L1iS1000 = _M0L6_2atmpS2649;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2650 = _M0Lm5indexS992;
    _M0L6_2atmpS2655 = _M0Lm6outputS994;
    _M0L6_2atmpS2654 = (int32_t)_M0L6_2atmpS2655;
    _M0L6_2atmpS2653 = _M0L6_2atmpS2654 % 10;
    _M0L6_2atmpS2652 = 48 + _M0L6_2atmpS2653;
    _M0L6_2atmpS2651 = _M0L6_2atmpS2652 & 0xff;
    if (
      _M0L6_2atmpS2650 < 0
      || _M0L6_2atmpS2650 >= Moonbit_array_length(_M0L6resultS991)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS991[_M0L6_2atmpS2650] = _M0L6_2atmpS2651;
    if (_M0L7olengthS996 > 1) {
      int32_t _M0L6_2atmpS2657 = _M0Lm5indexS992;
      int32_t _M0L6_2atmpS2656 = _M0L6_2atmpS2657 + 1;
      if (
        _M0L6_2atmpS2656 < 0
        || _M0L6_2atmpS2656 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2656] = 46;
    } else {
      int32_t _M0L6_2atmpS2658 = _M0Lm5indexS992;
      _M0Lm5indexS992 = _M0L6_2atmpS2658 - 1;
    }
    _M0L6_2atmpS2659 = _M0Lm5indexS992;
    _M0L6_2atmpS2660 = _M0L7olengthS996 + 1;
    _M0Lm5indexS992 = _M0L6_2atmpS2659 + _M0L6_2atmpS2660;
    _M0L6_2atmpS2661 = _M0Lm5indexS992;
    if (
      _M0L6_2atmpS2661 < 0
      || _M0L6_2atmpS2661 >= Moonbit_array_length(_M0L6resultS991)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS991[_M0L6_2atmpS2661] = 101;
    _M0L6_2atmpS2662 = _M0Lm5indexS992;
    _M0Lm5indexS992 = _M0L6_2atmpS2662 + 1;
    _M0L6_2atmpS2663 = _M0Lm3expS997;
    if (_M0L6_2atmpS2663 < 0) {
      int32_t _M0L6_2atmpS2664 = _M0Lm5indexS992;
      int32_t _M0L6_2atmpS2665;
      int32_t _M0L6_2atmpS2666;
      if (
        _M0L6_2atmpS2664 < 0
        || _M0L6_2atmpS2664 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2664] = 45;
      _M0L6_2atmpS2665 = _M0Lm5indexS992;
      _M0Lm5indexS992 = _M0L6_2atmpS2665 + 1;
      _M0L6_2atmpS2666 = _M0Lm3expS997;
      _M0Lm3expS997 = -_M0L6_2atmpS2666;
    } else {
      int32_t _M0L6_2atmpS2667 = _M0Lm5indexS992;
      int32_t _M0L6_2atmpS2668;
      if (
        _M0L6_2atmpS2667 < 0
        || _M0L6_2atmpS2667 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2667] = 43;
      _M0L6_2atmpS2668 = _M0Lm5indexS992;
      _M0Lm5indexS992 = _M0L6_2atmpS2668 + 1;
    }
    _M0L6_2atmpS2669 = _M0Lm3expS997;
    if (_M0L6_2atmpS2669 >= 100) {
      int32_t _M0L6_2atmpS2685 = _M0Lm3expS997;
      int32_t _M0L1aS1003 = _M0L6_2atmpS2685 / 100;
      int32_t _M0L6_2atmpS2684 = _M0Lm3expS997;
      int32_t _M0L6_2atmpS2683 = _M0L6_2atmpS2684 / 10;
      int32_t _M0L1bS1004 = _M0L6_2atmpS2683 % 10;
      int32_t _M0L6_2atmpS2682 = _M0Lm3expS997;
      int32_t _M0L1cS1005 = _M0L6_2atmpS2682 % 10;
      int32_t _M0L6_2atmpS2670 = _M0Lm5indexS992;
      int32_t _M0L6_2atmpS2672 = 48 + _M0L1aS1003;
      int32_t _M0L6_2atmpS2671 = _M0L6_2atmpS2672 & 0xff;
      int32_t _M0L6_2atmpS2676;
      int32_t _M0L6_2atmpS2673;
      int32_t _M0L6_2atmpS2675;
      int32_t _M0L6_2atmpS2674;
      int32_t _M0L6_2atmpS2680;
      int32_t _M0L6_2atmpS2677;
      int32_t _M0L6_2atmpS2679;
      int32_t _M0L6_2atmpS2678;
      int32_t _M0L6_2atmpS2681;
      if (
        _M0L6_2atmpS2670 < 0
        || _M0L6_2atmpS2670 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2670] = _M0L6_2atmpS2671;
      _M0L6_2atmpS2676 = _M0Lm5indexS992;
      _M0L6_2atmpS2673 = _M0L6_2atmpS2676 + 1;
      _M0L6_2atmpS2675 = 48 + _M0L1bS1004;
      _M0L6_2atmpS2674 = _M0L6_2atmpS2675 & 0xff;
      if (
        _M0L6_2atmpS2673 < 0
        || _M0L6_2atmpS2673 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2673] = _M0L6_2atmpS2674;
      _M0L6_2atmpS2680 = _M0Lm5indexS992;
      _M0L6_2atmpS2677 = _M0L6_2atmpS2680 + 2;
      _M0L6_2atmpS2679 = 48 + _M0L1cS1005;
      _M0L6_2atmpS2678 = _M0L6_2atmpS2679 & 0xff;
      if (
        _M0L6_2atmpS2677 < 0
        || _M0L6_2atmpS2677 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2677] = _M0L6_2atmpS2678;
      _M0L6_2atmpS2681 = _M0Lm5indexS992;
      _M0Lm5indexS992 = _M0L6_2atmpS2681 + 3;
    } else {
      int32_t _M0L6_2atmpS2686 = _M0Lm3expS997;
      if (_M0L6_2atmpS2686 >= 10) {
        int32_t _M0L6_2atmpS2696 = _M0Lm3expS997;
        int32_t _M0L1aS1006 = _M0L6_2atmpS2696 / 10;
        int32_t _M0L6_2atmpS2695 = _M0Lm3expS997;
        int32_t _M0L1bS1007 = _M0L6_2atmpS2695 % 10;
        int32_t _M0L6_2atmpS2687 = _M0Lm5indexS992;
        int32_t _M0L6_2atmpS2689 = 48 + _M0L1aS1006;
        int32_t _M0L6_2atmpS2688 = _M0L6_2atmpS2689 & 0xff;
        int32_t _M0L6_2atmpS2693;
        int32_t _M0L6_2atmpS2690;
        int32_t _M0L6_2atmpS2692;
        int32_t _M0L6_2atmpS2691;
        int32_t _M0L6_2atmpS2694;
        if (
          _M0L6_2atmpS2687 < 0
          || _M0L6_2atmpS2687 >= Moonbit_array_length(_M0L6resultS991)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS991[_M0L6_2atmpS2687] = _M0L6_2atmpS2688;
        _M0L6_2atmpS2693 = _M0Lm5indexS992;
        _M0L6_2atmpS2690 = _M0L6_2atmpS2693 + 1;
        _M0L6_2atmpS2692 = 48 + _M0L1bS1007;
        _M0L6_2atmpS2691 = _M0L6_2atmpS2692 & 0xff;
        if (
          _M0L6_2atmpS2690 < 0
          || _M0L6_2atmpS2690 >= Moonbit_array_length(_M0L6resultS991)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS991[_M0L6_2atmpS2690] = _M0L6_2atmpS2691;
        _M0L6_2atmpS2694 = _M0Lm5indexS992;
        _M0Lm5indexS992 = _M0L6_2atmpS2694 + 2;
      } else {
        int32_t _M0L6_2atmpS2697 = _M0Lm5indexS992;
        int32_t _M0L6_2atmpS2700 = _M0Lm3expS997;
        int32_t _M0L6_2atmpS2699 = 48 + _M0L6_2atmpS2700;
        int32_t _M0L6_2atmpS2698 = _M0L6_2atmpS2699 & 0xff;
        int32_t _M0L6_2atmpS2701;
        if (
          _M0L6_2atmpS2697 < 0
          || _M0L6_2atmpS2697 >= Moonbit_array_length(_M0L6resultS991)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS991[_M0L6_2atmpS2697] = _M0L6_2atmpS2698;
        _M0L6_2atmpS2701 = _M0Lm5indexS992;
        _M0Lm5indexS992 = _M0L6_2atmpS2701 + 1;
      }
    }
    _M0L6_2atmpS2702 = _M0Lm5indexS992;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS991, 0, _M0L6_2atmpS2702);
  } else {
    int32_t _M0L6_2atmpS2703 = _M0Lm3expS997;
    int32_t _M0L6_2atmpS2763;
    if (_M0L6_2atmpS2703 < 0) {
      int32_t _M0L6_2atmpS2704 = _M0Lm5indexS992;
      int32_t _M0L6_2atmpS2705;
      int32_t _M0L6_2atmpS2706;
      int32_t _M0L6_2atmpS2707;
      int32_t _M0L1iS1008;
      int32_t _M0L7currentS1010;
      int32_t _M0L1iS1011;
      if (
        _M0L6_2atmpS2704 < 0
        || _M0L6_2atmpS2704 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2704] = 48;
      _M0L6_2atmpS2705 = _M0Lm5indexS992;
      _M0Lm5indexS992 = _M0L6_2atmpS2705 + 1;
      _M0L6_2atmpS2706 = _M0Lm5indexS992;
      if (
        _M0L6_2atmpS2706 < 0
        || _M0L6_2atmpS2706 >= Moonbit_array_length(_M0L6resultS991)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS991[_M0L6_2atmpS2706] = 46;
      _M0L6_2atmpS2707 = _M0Lm5indexS992;
      _M0Lm5indexS992 = _M0L6_2atmpS2707 + 1;
      _M0L1iS1008 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2708 = _M0Lm3expS997;
        if (_M0L1iS1008 > _M0L6_2atmpS2708) {
          int32_t _M0L6_2atmpS2709 = _M0Lm5indexS992;
          int32_t _M0L6_2atmpS2710;
          int32_t _M0L6_2atmpS2711;
          if (
            _M0L6_2atmpS2709 < 0
            || _M0L6_2atmpS2709 >= Moonbit_array_length(_M0L6resultS991)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS991[_M0L6_2atmpS2709] = 48;
          _M0L6_2atmpS2710 = _M0Lm5indexS992;
          _M0Lm5indexS992 = _M0L6_2atmpS2710 + 1;
          _M0L6_2atmpS2711 = _M0L1iS1008 - 1;
          _M0L1iS1008 = _M0L6_2atmpS2711;
          continue;
        }
        break;
      }
      _M0L7currentS1010 = _M0Lm5indexS992;
      _M0L1iS1011 = 0;
      while (1) {
        if (_M0L1iS1011 < _M0L7olengthS996) {
          int32_t _M0L6_2atmpS2719 = _M0L7currentS1010 + _M0L7olengthS996;
          int32_t _M0L6_2atmpS2718 = _M0L6_2atmpS2719 - _M0L1iS1011;
          int32_t _M0L6_2atmpS2712 = _M0L6_2atmpS2718 - 1;
          uint64_t _M0L6_2atmpS2717 = _M0Lm6outputS994;
          uint64_t _M0L6_2atmpS2716 = _M0L6_2atmpS2717 % 10ull;
          int32_t _M0L6_2atmpS2715 = (int32_t)_M0L6_2atmpS2716;
          int32_t _M0L6_2atmpS2714 = 48 + _M0L6_2atmpS2715;
          int32_t _M0L6_2atmpS2713 = _M0L6_2atmpS2714 & 0xff;
          uint64_t _M0L6_2atmpS2720;
          int32_t _M0L6_2atmpS2721;
          int32_t _M0L6_2atmpS2722;
          if (
            _M0L6_2atmpS2712 < 0
            || _M0L6_2atmpS2712 >= Moonbit_array_length(_M0L6resultS991)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS991[_M0L6_2atmpS2712] = _M0L6_2atmpS2713;
          _M0L6_2atmpS2720 = _M0Lm6outputS994;
          _M0Lm6outputS994 = _M0L6_2atmpS2720 / 10ull;
          _M0L6_2atmpS2721 = _M0Lm5indexS992;
          _M0Lm5indexS992 = _M0L6_2atmpS2721 + 1;
          _M0L6_2atmpS2722 = _M0L1iS1011 + 1;
          _M0L1iS1011 = _M0L6_2atmpS2722;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2724 = _M0Lm3expS997;
      int32_t _M0L6_2atmpS2723 = _M0L6_2atmpS2724 + 1;
      if (_M0L6_2atmpS2723 >= _M0L7olengthS996) {
        int32_t _M0L1iS1013 = 0;
        int32_t _M0L6_2atmpS2736;
        int32_t _M0L6_2atmpS2740;
        int32_t _M0L7_2abindS1015;
        int32_t _M0L2__S1016;
        while (1) {
          if (_M0L1iS1013 < _M0L7olengthS996) {
            int32_t _M0L6_2atmpS2733 = _M0Lm5indexS992;
            int32_t _M0L6_2atmpS2732 = _M0L6_2atmpS2733 + _M0L7olengthS996;
            int32_t _M0L6_2atmpS2731 = _M0L6_2atmpS2732 - _M0L1iS1013;
            int32_t _M0L6_2atmpS2725 = _M0L6_2atmpS2731 - 1;
            uint64_t _M0L6_2atmpS2730 = _M0Lm6outputS994;
            uint64_t _M0L6_2atmpS2729 = _M0L6_2atmpS2730 % 10ull;
            int32_t _M0L6_2atmpS2728 = (int32_t)_M0L6_2atmpS2729;
            int32_t _M0L6_2atmpS2727 = 48 + _M0L6_2atmpS2728;
            int32_t _M0L6_2atmpS2726 = _M0L6_2atmpS2727 & 0xff;
            uint64_t _M0L6_2atmpS2734;
            int32_t _M0L6_2atmpS2735;
            if (
              _M0L6_2atmpS2725 < 0
              || _M0L6_2atmpS2725 >= Moonbit_array_length(_M0L6resultS991)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS991[_M0L6_2atmpS2725] = _M0L6_2atmpS2726;
            _M0L6_2atmpS2734 = _M0Lm6outputS994;
            _M0Lm6outputS994 = _M0L6_2atmpS2734 / 10ull;
            _M0L6_2atmpS2735 = _M0L1iS1013 + 1;
            _M0L1iS1013 = _M0L6_2atmpS2735;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2736 = _M0Lm5indexS992;
        _M0Lm5indexS992 = _M0L6_2atmpS2736 + _M0L7olengthS996;
        _M0L6_2atmpS2740 = _M0Lm3expS997;
        _M0L7_2abindS1015 = _M0L6_2atmpS2740 + 1;
        _M0L2__S1016 = _M0L7olengthS996;
        while (1) {
          if (_M0L2__S1016 < _M0L7_2abindS1015) {
            int32_t _M0L6_2atmpS2737 = _M0Lm5indexS992;
            int32_t _M0L6_2atmpS2738;
            int32_t _M0L6_2atmpS2739;
            if (
              _M0L6_2atmpS2737 < 0
              || _M0L6_2atmpS2737 >= Moonbit_array_length(_M0L6resultS991)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS991[_M0L6_2atmpS2737] = 48;
            _M0L6_2atmpS2738 = _M0Lm5indexS992;
            _M0Lm5indexS992 = _M0L6_2atmpS2738 + 1;
            _M0L6_2atmpS2739 = _M0L2__S1016 + 1;
            _M0L2__S1016 = _M0L6_2atmpS2739;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2762 = _M0Lm5indexS992;
        int32_t _M0Lm7currentS1018 = _M0L6_2atmpS2762 + 1;
        int32_t _M0L1iS1019 = 0;
        int32_t _M0L6_2atmpS2760;
        int32_t _M0L6_2atmpS2761;
        while (1) {
          if (_M0L1iS1019 < _M0L7olengthS996) {
            int32_t _M0L6_2atmpS2743 = _M0L7olengthS996 - _M0L1iS1019;
            int32_t _M0L6_2atmpS2741 = _M0L6_2atmpS2743 - 1;
            int32_t _M0L6_2atmpS2742 = _M0Lm3expS997;
            int32_t _M0L6_2atmpS2757;
            int32_t _M0L6_2atmpS2756;
            int32_t _M0L6_2atmpS2755;
            int32_t _M0L6_2atmpS2749;
            uint64_t _M0L6_2atmpS2754;
            uint64_t _M0L6_2atmpS2753;
            int32_t _M0L6_2atmpS2752;
            int32_t _M0L6_2atmpS2751;
            int32_t _M0L6_2atmpS2750;
            uint64_t _M0L6_2atmpS2758;
            int32_t _M0L6_2atmpS2759;
            if (_M0L6_2atmpS2741 == _M0L6_2atmpS2742) {
              int32_t _M0L6_2atmpS2747 = _M0Lm7currentS1018;
              int32_t _M0L6_2atmpS2746 = _M0L6_2atmpS2747 + _M0L7olengthS996;
              int32_t _M0L6_2atmpS2745 = _M0L6_2atmpS2746 - _M0L1iS1019;
              int32_t _M0L6_2atmpS2744 = _M0L6_2atmpS2745 - 1;
              int32_t _M0L6_2atmpS2748;
              if (
                _M0L6_2atmpS2744 < 0
                || _M0L6_2atmpS2744 >= Moonbit_array_length(_M0L6resultS991)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS991[_M0L6_2atmpS2744] = 46;
              _M0L6_2atmpS2748 = _M0Lm7currentS1018;
              _M0Lm7currentS1018 = _M0L6_2atmpS2748 - 1;
            }
            _M0L6_2atmpS2757 = _M0Lm7currentS1018;
            _M0L6_2atmpS2756 = _M0L6_2atmpS2757 + _M0L7olengthS996;
            _M0L6_2atmpS2755 = _M0L6_2atmpS2756 - _M0L1iS1019;
            _M0L6_2atmpS2749 = _M0L6_2atmpS2755 - 1;
            _M0L6_2atmpS2754 = _M0Lm6outputS994;
            _M0L6_2atmpS2753 = _M0L6_2atmpS2754 % 10ull;
            _M0L6_2atmpS2752 = (int32_t)_M0L6_2atmpS2753;
            _M0L6_2atmpS2751 = 48 + _M0L6_2atmpS2752;
            _M0L6_2atmpS2750 = _M0L6_2atmpS2751 & 0xff;
            if (
              _M0L6_2atmpS2749 < 0
              || _M0L6_2atmpS2749 >= Moonbit_array_length(_M0L6resultS991)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS991[_M0L6_2atmpS2749] = _M0L6_2atmpS2750;
            _M0L6_2atmpS2758 = _M0Lm6outputS994;
            _M0Lm6outputS994 = _M0L6_2atmpS2758 / 10ull;
            _M0L6_2atmpS2759 = _M0L1iS1019 + 1;
            _M0L1iS1019 = _M0L6_2atmpS2759;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2760 = _M0Lm5indexS992;
        _M0L6_2atmpS2761 = _M0L7olengthS996 + 1;
        _M0Lm5indexS992 = _M0L6_2atmpS2760 + _M0L6_2atmpS2761;
      }
    }
    _M0L6_2atmpS2763 = _M0Lm5indexS992;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS991, 0, _M0L6_2atmpS2763);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS937,
  uint32_t _M0L12ieeeExponentS936
) {
  int32_t _M0Lm2e2S934;
  uint64_t _M0Lm2m2S935;
  uint64_t _M0L6_2atmpS2638;
  uint64_t _M0L6_2atmpS2637;
  int32_t _M0L4evenS938;
  uint64_t _M0L6_2atmpS2636;
  uint64_t _M0L2mvS939;
  int32_t _M0L7mmShiftS940;
  uint64_t _M0Lm2vrS941;
  uint64_t _M0Lm2vpS942;
  uint64_t _M0Lm2vmS943;
  int32_t _M0Lm3e10S944;
  int32_t _M0Lm17vmIsTrailingZerosS945;
  int32_t _M0Lm17vrIsTrailingZerosS946;
  int32_t _M0L6_2atmpS2538;
  int32_t _M0Lm7removedS965;
  int32_t _M0Lm16lastRemovedDigitS966;
  uint64_t _M0Lm6outputS967;
  int32_t _M0L6_2atmpS2634;
  int32_t _M0L6_2atmpS2635;
  int32_t _M0L3expS990;
  uint64_t _M0L6_2atmpS2633;
  struct _M0TPB17FloatingDecimal64* _block_4268;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S934 = 0;
  _M0Lm2m2S935 = 0ull;
  if (_M0L12ieeeExponentS936 == 0u) {
    _M0Lm2e2S934 = -1076;
    _M0Lm2m2S935 = _M0L12ieeeMantissaS937;
  } else {
    int32_t _M0L6_2atmpS2537 = *(int32_t*)&_M0L12ieeeExponentS936;
    int32_t _M0L6_2atmpS2536 = _M0L6_2atmpS2537 - 1023;
    int32_t _M0L6_2atmpS2535 = _M0L6_2atmpS2536 - 52;
    _M0Lm2e2S934 = _M0L6_2atmpS2535 - 2;
    _M0Lm2m2S935 = 4503599627370496ull | _M0L12ieeeMantissaS937;
  }
  _M0L6_2atmpS2638 = _M0Lm2m2S935;
  _M0L6_2atmpS2637 = _M0L6_2atmpS2638 & 1ull;
  _M0L4evenS938 = _M0L6_2atmpS2637 == 0ull;
  _M0L6_2atmpS2636 = _M0Lm2m2S935;
  _M0L2mvS939 = 4ull * _M0L6_2atmpS2636;
  if (_M0L12ieeeMantissaS937 != 0ull) {
    _M0L7mmShiftS940 = 1;
  } else {
    _M0L7mmShiftS940 = _M0L12ieeeExponentS936 <= 1u;
  }
  _M0Lm2vrS941 = 0ull;
  _M0Lm2vpS942 = 0ull;
  _M0Lm2vmS943 = 0ull;
  _M0Lm3e10S944 = 0;
  _M0Lm17vmIsTrailingZerosS945 = 0;
  _M0Lm17vrIsTrailingZerosS946 = 0;
  _M0L6_2atmpS2538 = _M0Lm2e2S934;
  if (_M0L6_2atmpS2538 >= 0) {
    int32_t _M0L6_2atmpS2560 = _M0Lm2e2S934;
    int32_t _M0L6_2atmpS2556;
    int32_t _M0L6_2atmpS2559;
    int32_t _M0L6_2atmpS2558;
    int32_t _M0L6_2atmpS2557;
    int32_t _M0L1qS947;
    int32_t _M0L6_2atmpS2555;
    int32_t _M0L6_2atmpS2554;
    int32_t _M0L1kS948;
    int32_t _M0L6_2atmpS2553;
    int32_t _M0L6_2atmpS2552;
    int32_t _M0L6_2atmpS2551;
    int32_t _M0L1iS949;
    struct _M0TPB8Pow5Pair _M0L4pow5S950;
    uint64_t _M0L6_2atmpS2550;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS951;
    uint64_t _M0L8_2avrOutS952;
    uint64_t _M0L8_2avpOutS953;
    uint64_t _M0L8_2avmOutS954;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2556 = _M0FPB9log10Pow2(_M0L6_2atmpS2560);
    _M0L6_2atmpS2559 = _M0Lm2e2S934;
    _M0L6_2atmpS2558 = _M0L6_2atmpS2559 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2557 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2558);
    _M0L1qS947 = _M0L6_2atmpS2556 - _M0L6_2atmpS2557;
    _M0Lm3e10S944 = _M0L1qS947;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2555 = _M0FPB8pow5bits(_M0L1qS947);
    _M0L6_2atmpS2554 = 125 + _M0L6_2atmpS2555;
    _M0L1kS948 = _M0L6_2atmpS2554 - 1;
    _M0L6_2atmpS2553 = _M0Lm2e2S934;
    _M0L6_2atmpS2552 = -_M0L6_2atmpS2553;
    _M0L6_2atmpS2551 = _M0L6_2atmpS2552 + _M0L1qS947;
    _M0L1iS949 = _M0L6_2atmpS2551 + _M0L1kS948;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S950 = _M0FPB22double__computeInvPow5(_M0L1qS947);
    _M0L6_2atmpS2550 = _M0Lm2m2S935;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS951
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2550, _M0L4pow5S950, _M0L1iS949, _M0L7mmShiftS940);
    _M0L8_2avrOutS952 = _M0L7_2abindS951.$0;
    _M0L8_2avpOutS953 = _M0L7_2abindS951.$1;
    _M0L8_2avmOutS954 = _M0L7_2abindS951.$2;
    _M0Lm2vrS941 = _M0L8_2avrOutS952;
    _M0Lm2vpS942 = _M0L8_2avpOutS953;
    _M0Lm2vmS943 = _M0L8_2avmOutS954;
    if (_M0L1qS947 <= 21) {
      int32_t _M0L6_2atmpS2546 = (int32_t)_M0L2mvS939;
      uint64_t _M0L6_2atmpS2549 = _M0L2mvS939 / 5ull;
      int32_t _M0L6_2atmpS2548 = (int32_t)_M0L6_2atmpS2549;
      int32_t _M0L6_2atmpS2547 = 5 * _M0L6_2atmpS2548;
      int32_t _M0L6mvMod5S955 = _M0L6_2atmpS2546 - _M0L6_2atmpS2547;
      if (_M0L6mvMod5S955 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS946
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS939, _M0L1qS947);
      } else if (_M0L4evenS938) {
        uint64_t _M0L6_2atmpS2540 = _M0L2mvS939 - 1ull;
        uint64_t _M0L6_2atmpS2541;
        uint64_t _M0L6_2atmpS2539;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2541 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS940);
        _M0L6_2atmpS2539 = _M0L6_2atmpS2540 - _M0L6_2atmpS2541;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS945
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2539, _M0L1qS947);
      } else {
        uint64_t _M0L6_2atmpS2542 = _M0Lm2vpS942;
        uint64_t _M0L6_2atmpS2545 = _M0L2mvS939 + 2ull;
        int32_t _M0L6_2atmpS2544;
        uint64_t _M0L6_2atmpS2543;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2544
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2545, _M0L1qS947);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2543 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2544);
        _M0Lm2vpS942 = _M0L6_2atmpS2542 - _M0L6_2atmpS2543;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2574 = _M0Lm2e2S934;
    int32_t _M0L6_2atmpS2573 = -_M0L6_2atmpS2574;
    int32_t _M0L6_2atmpS2568;
    int32_t _M0L6_2atmpS2572;
    int32_t _M0L6_2atmpS2571;
    int32_t _M0L6_2atmpS2570;
    int32_t _M0L6_2atmpS2569;
    int32_t _M0L1qS956;
    int32_t _M0L6_2atmpS2561;
    int32_t _M0L6_2atmpS2567;
    int32_t _M0L6_2atmpS2566;
    int32_t _M0L1iS957;
    int32_t _M0L6_2atmpS2565;
    int32_t _M0L1kS958;
    int32_t _M0L1jS959;
    struct _M0TPB8Pow5Pair _M0L4pow5S960;
    uint64_t _M0L6_2atmpS2564;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS961;
    uint64_t _M0L8_2avrOutS962;
    uint64_t _M0L8_2avpOutS963;
    uint64_t _M0L8_2avmOutS964;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2568 = _M0FPB9log10Pow5(_M0L6_2atmpS2573);
    _M0L6_2atmpS2572 = _M0Lm2e2S934;
    _M0L6_2atmpS2571 = -_M0L6_2atmpS2572;
    _M0L6_2atmpS2570 = _M0L6_2atmpS2571 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2569 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2570);
    _M0L1qS956 = _M0L6_2atmpS2568 - _M0L6_2atmpS2569;
    _M0L6_2atmpS2561 = _M0Lm2e2S934;
    _M0Lm3e10S944 = _M0L1qS956 + _M0L6_2atmpS2561;
    _M0L6_2atmpS2567 = _M0Lm2e2S934;
    _M0L6_2atmpS2566 = -_M0L6_2atmpS2567;
    _M0L1iS957 = _M0L6_2atmpS2566 - _M0L1qS956;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2565 = _M0FPB8pow5bits(_M0L1iS957);
    _M0L1kS958 = _M0L6_2atmpS2565 - 125;
    _M0L1jS959 = _M0L1qS956 - _M0L1kS958;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S960 = _M0FPB19double__computePow5(_M0L1iS957);
    _M0L6_2atmpS2564 = _M0Lm2m2S935;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS961
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2564, _M0L4pow5S960, _M0L1jS959, _M0L7mmShiftS940);
    _M0L8_2avrOutS962 = _M0L7_2abindS961.$0;
    _M0L8_2avpOutS963 = _M0L7_2abindS961.$1;
    _M0L8_2avmOutS964 = _M0L7_2abindS961.$2;
    _M0Lm2vrS941 = _M0L8_2avrOutS962;
    _M0Lm2vpS942 = _M0L8_2avpOutS963;
    _M0Lm2vmS943 = _M0L8_2avmOutS964;
    if (_M0L1qS956 <= 1) {
      _M0Lm17vrIsTrailingZerosS946 = 1;
      if (_M0L4evenS938) {
        int32_t _M0L6_2atmpS2562;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2562 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS940);
        _M0Lm17vmIsTrailingZerosS945 = _M0L6_2atmpS2562 == 1;
      } else {
        uint64_t _M0L6_2atmpS2563 = _M0Lm2vpS942;
        _M0Lm2vpS942 = _M0L6_2atmpS2563 - 1ull;
      }
    } else if (_M0L1qS956 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS946
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS939, _M0L1qS956);
    }
  }
  _M0Lm7removedS965 = 0;
  _M0Lm16lastRemovedDigitS966 = 0;
  _M0Lm6outputS967 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS945 || _M0Lm17vrIsTrailingZerosS946) {
    int32_t _if__result_4265;
    uint64_t _M0L6_2atmpS2604;
    uint64_t _M0L6_2atmpS2610;
    uint64_t _M0L6_2atmpS2611;
    int32_t _if__result_4266;
    int32_t _M0L6_2atmpS2607;
    int64_t _M0L6_2atmpS2606;
    uint64_t _M0L6_2atmpS2605;
    while (1) {
      uint64_t _M0L6_2atmpS2587 = _M0Lm2vpS942;
      uint64_t _M0L7vpDiv10S968 = _M0L6_2atmpS2587 / 10ull;
      uint64_t _M0L6_2atmpS2586 = _M0Lm2vmS943;
      uint64_t _M0L7vmDiv10S969 = _M0L6_2atmpS2586 / 10ull;
      uint64_t _M0L6_2atmpS2585;
      int32_t _M0L6_2atmpS2582;
      int32_t _M0L6_2atmpS2584;
      int32_t _M0L6_2atmpS2583;
      int32_t _M0L7vmMod10S971;
      uint64_t _M0L6_2atmpS2581;
      uint64_t _M0L7vrDiv10S972;
      uint64_t _M0L6_2atmpS2580;
      int32_t _M0L6_2atmpS2577;
      int32_t _M0L6_2atmpS2579;
      int32_t _M0L6_2atmpS2578;
      int32_t _M0L7vrMod10S973;
      int32_t _M0L6_2atmpS2576;
      if (_M0L7vpDiv10S968 <= _M0L7vmDiv10S969) {
        break;
      }
      _M0L6_2atmpS2585 = _M0Lm2vmS943;
      _M0L6_2atmpS2582 = (int32_t)_M0L6_2atmpS2585;
      _M0L6_2atmpS2584 = (int32_t)_M0L7vmDiv10S969;
      _M0L6_2atmpS2583 = 10 * _M0L6_2atmpS2584;
      _M0L7vmMod10S971 = _M0L6_2atmpS2582 - _M0L6_2atmpS2583;
      _M0L6_2atmpS2581 = _M0Lm2vrS941;
      _M0L7vrDiv10S972 = _M0L6_2atmpS2581 / 10ull;
      _M0L6_2atmpS2580 = _M0Lm2vrS941;
      _M0L6_2atmpS2577 = (int32_t)_M0L6_2atmpS2580;
      _M0L6_2atmpS2579 = (int32_t)_M0L7vrDiv10S972;
      _M0L6_2atmpS2578 = 10 * _M0L6_2atmpS2579;
      _M0L7vrMod10S973 = _M0L6_2atmpS2577 - _M0L6_2atmpS2578;
      if (_M0Lm17vmIsTrailingZerosS945) {
        _M0Lm17vmIsTrailingZerosS945 = _M0L7vmMod10S971 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS945 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS946) {
        int32_t _M0L6_2atmpS2575 = _M0Lm16lastRemovedDigitS966;
        _M0Lm17vrIsTrailingZerosS946 = _M0L6_2atmpS2575 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS946 = 0;
      }
      _M0Lm16lastRemovedDigitS966 = _M0L7vrMod10S973;
      _M0Lm2vrS941 = _M0L7vrDiv10S972;
      _M0Lm2vpS942 = _M0L7vpDiv10S968;
      _M0Lm2vmS943 = _M0L7vmDiv10S969;
      _M0L6_2atmpS2576 = _M0Lm7removedS965;
      _M0Lm7removedS965 = _M0L6_2atmpS2576 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS945) {
      while (1) {
        uint64_t _M0L6_2atmpS2600 = _M0Lm2vmS943;
        uint64_t _M0L7vmDiv10S974 = _M0L6_2atmpS2600 / 10ull;
        uint64_t _M0L6_2atmpS2599 = _M0Lm2vmS943;
        int32_t _M0L6_2atmpS2596 = (int32_t)_M0L6_2atmpS2599;
        int32_t _M0L6_2atmpS2598 = (int32_t)_M0L7vmDiv10S974;
        int32_t _M0L6_2atmpS2597 = 10 * _M0L6_2atmpS2598;
        int32_t _M0L7vmMod10S975 = _M0L6_2atmpS2596 - _M0L6_2atmpS2597;
        uint64_t _M0L6_2atmpS2595;
        uint64_t _M0L7vpDiv10S977;
        uint64_t _M0L6_2atmpS2594;
        uint64_t _M0L7vrDiv10S978;
        uint64_t _M0L6_2atmpS2593;
        int32_t _M0L6_2atmpS2590;
        int32_t _M0L6_2atmpS2592;
        int32_t _M0L6_2atmpS2591;
        int32_t _M0L7vrMod10S979;
        int32_t _M0L6_2atmpS2589;
        if (_M0L7vmMod10S975 != 0) {
          break;
        }
        _M0L6_2atmpS2595 = _M0Lm2vpS942;
        _M0L7vpDiv10S977 = _M0L6_2atmpS2595 / 10ull;
        _M0L6_2atmpS2594 = _M0Lm2vrS941;
        _M0L7vrDiv10S978 = _M0L6_2atmpS2594 / 10ull;
        _M0L6_2atmpS2593 = _M0Lm2vrS941;
        _M0L6_2atmpS2590 = (int32_t)_M0L6_2atmpS2593;
        _M0L6_2atmpS2592 = (int32_t)_M0L7vrDiv10S978;
        _M0L6_2atmpS2591 = 10 * _M0L6_2atmpS2592;
        _M0L7vrMod10S979 = _M0L6_2atmpS2590 - _M0L6_2atmpS2591;
        if (_M0Lm17vrIsTrailingZerosS946) {
          int32_t _M0L6_2atmpS2588 = _M0Lm16lastRemovedDigitS966;
          _M0Lm17vrIsTrailingZerosS946 = _M0L6_2atmpS2588 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS946 = 0;
        }
        _M0Lm16lastRemovedDigitS966 = _M0L7vrMod10S979;
        _M0Lm2vrS941 = _M0L7vrDiv10S978;
        _M0Lm2vpS942 = _M0L7vpDiv10S977;
        _M0Lm2vmS943 = _M0L7vmDiv10S974;
        _M0L6_2atmpS2589 = _M0Lm7removedS965;
        _M0Lm7removedS965 = _M0L6_2atmpS2589 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS946) {
      int32_t _M0L6_2atmpS2603 = _M0Lm16lastRemovedDigitS966;
      if (_M0L6_2atmpS2603 == 5) {
        uint64_t _M0L6_2atmpS2602 = _M0Lm2vrS941;
        uint64_t _M0L6_2atmpS2601 = _M0L6_2atmpS2602 % 2ull;
        _if__result_4265 = _M0L6_2atmpS2601 == 0ull;
      } else {
        _if__result_4265 = 0;
      }
    } else {
      _if__result_4265 = 0;
    }
    if (_if__result_4265) {
      _M0Lm16lastRemovedDigitS966 = 4;
    }
    _M0L6_2atmpS2604 = _M0Lm2vrS941;
    _M0L6_2atmpS2610 = _M0Lm2vrS941;
    _M0L6_2atmpS2611 = _M0Lm2vmS943;
    if (_M0L6_2atmpS2610 == _M0L6_2atmpS2611) {
      if (!_M0L4evenS938) {
        _if__result_4266 = 1;
      } else {
        int32_t _M0L6_2atmpS2609 = _M0Lm17vmIsTrailingZerosS945;
        _if__result_4266 = !_M0L6_2atmpS2609;
      }
    } else {
      _if__result_4266 = 0;
    }
    if (_if__result_4266) {
      _M0L6_2atmpS2607 = 1;
    } else {
      int32_t _M0L6_2atmpS2608 = _M0Lm16lastRemovedDigitS966;
      _M0L6_2atmpS2607 = _M0L6_2atmpS2608 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2606 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2607);
    _M0L6_2atmpS2605 = *(uint64_t*)&_M0L6_2atmpS2606;
    _M0Lm6outputS967 = _M0L6_2atmpS2604 + _M0L6_2atmpS2605;
  } else {
    int32_t _M0Lm7roundUpS980 = 0;
    uint64_t _M0L6_2atmpS2632 = _M0Lm2vpS942;
    uint64_t _M0L8vpDiv100S981 = _M0L6_2atmpS2632 / 100ull;
    uint64_t _M0L6_2atmpS2631 = _M0Lm2vmS943;
    uint64_t _M0L8vmDiv100S982 = _M0L6_2atmpS2631 / 100ull;
    uint64_t _M0L6_2atmpS2626;
    uint64_t _M0L6_2atmpS2629;
    uint64_t _M0L6_2atmpS2630;
    int32_t _M0L6_2atmpS2628;
    uint64_t _M0L6_2atmpS2627;
    if (_M0L8vpDiv100S981 > _M0L8vmDiv100S982) {
      uint64_t _M0L6_2atmpS2617 = _M0Lm2vrS941;
      uint64_t _M0L8vrDiv100S983 = _M0L6_2atmpS2617 / 100ull;
      uint64_t _M0L6_2atmpS2616 = _M0Lm2vrS941;
      int32_t _M0L6_2atmpS2613 = (int32_t)_M0L6_2atmpS2616;
      int32_t _M0L6_2atmpS2615 = (int32_t)_M0L8vrDiv100S983;
      int32_t _M0L6_2atmpS2614 = 100 * _M0L6_2atmpS2615;
      int32_t _M0L8vrMod100S984 = _M0L6_2atmpS2613 - _M0L6_2atmpS2614;
      int32_t _M0L6_2atmpS2612;
      _M0Lm7roundUpS980 = _M0L8vrMod100S984 >= 50;
      _M0Lm2vrS941 = _M0L8vrDiv100S983;
      _M0Lm2vpS942 = _M0L8vpDiv100S981;
      _M0Lm2vmS943 = _M0L8vmDiv100S982;
      _M0L6_2atmpS2612 = _M0Lm7removedS965;
      _M0Lm7removedS965 = _M0L6_2atmpS2612 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2625 = _M0Lm2vpS942;
      uint64_t _M0L7vpDiv10S985 = _M0L6_2atmpS2625 / 10ull;
      uint64_t _M0L6_2atmpS2624 = _M0Lm2vmS943;
      uint64_t _M0L7vmDiv10S986 = _M0L6_2atmpS2624 / 10ull;
      uint64_t _M0L6_2atmpS2623;
      uint64_t _M0L7vrDiv10S988;
      uint64_t _M0L6_2atmpS2622;
      int32_t _M0L6_2atmpS2619;
      int32_t _M0L6_2atmpS2621;
      int32_t _M0L6_2atmpS2620;
      int32_t _M0L7vrMod10S989;
      int32_t _M0L6_2atmpS2618;
      if (_M0L7vpDiv10S985 <= _M0L7vmDiv10S986) {
        break;
      }
      _M0L6_2atmpS2623 = _M0Lm2vrS941;
      _M0L7vrDiv10S988 = _M0L6_2atmpS2623 / 10ull;
      _M0L6_2atmpS2622 = _M0Lm2vrS941;
      _M0L6_2atmpS2619 = (int32_t)_M0L6_2atmpS2622;
      _M0L6_2atmpS2621 = (int32_t)_M0L7vrDiv10S988;
      _M0L6_2atmpS2620 = 10 * _M0L6_2atmpS2621;
      _M0L7vrMod10S989 = _M0L6_2atmpS2619 - _M0L6_2atmpS2620;
      _M0Lm7roundUpS980 = _M0L7vrMod10S989 >= 5;
      _M0Lm2vrS941 = _M0L7vrDiv10S988;
      _M0Lm2vpS942 = _M0L7vpDiv10S985;
      _M0Lm2vmS943 = _M0L7vmDiv10S986;
      _M0L6_2atmpS2618 = _M0Lm7removedS965;
      _M0Lm7removedS965 = _M0L6_2atmpS2618 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2626 = _M0Lm2vrS941;
    _M0L6_2atmpS2629 = _M0Lm2vrS941;
    _M0L6_2atmpS2630 = _M0Lm2vmS943;
    _M0L6_2atmpS2628
    = _M0L6_2atmpS2629 == _M0L6_2atmpS2630 || _M0Lm7roundUpS980;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2627 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2628);
    _M0Lm6outputS967 = _M0L6_2atmpS2626 + _M0L6_2atmpS2627;
  }
  _M0L6_2atmpS2634 = _M0Lm3e10S944;
  _M0L6_2atmpS2635 = _M0Lm7removedS965;
  _M0L3expS990 = _M0L6_2atmpS2634 + _M0L6_2atmpS2635;
  _M0L6_2atmpS2633 = _M0Lm6outputS967;
  _block_4268
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4268)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4268->$0 = _M0L6_2atmpS2633;
  _block_4268->$1 = _M0L3expS990;
  return _block_4268;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS933) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS933) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS932) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS932) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS931) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS931) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS930) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS930 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS930 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS930 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS930 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS930 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS930 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS930 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS930 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS930 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS930 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS930 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS930 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS930 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS930 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS930 >= 100ull) {
    return 3;
  }
  if (_M0L1vS930 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS913) {
  int32_t _M0L6_2atmpS2534;
  int32_t _M0L6_2atmpS2533;
  int32_t _M0L4baseS912;
  int32_t _M0L5base2S914;
  int32_t _M0L6offsetS915;
  int32_t _M0L6_2atmpS2532;
  uint64_t _M0L4mul0S916;
  int32_t _M0L6_2atmpS2531;
  int32_t _M0L6_2atmpS2530;
  uint64_t _M0L4mul1S917;
  uint64_t _M0L1mS918;
  struct _M0TPB7Umul128 _M0L7_2abindS919;
  uint64_t _M0L7_2alow1S920;
  uint64_t _M0L8_2ahigh1S921;
  struct _M0TPB7Umul128 _M0L7_2abindS922;
  uint64_t _M0L7_2alow0S923;
  uint64_t _M0L8_2ahigh0S924;
  uint64_t _M0L3sumS925;
  uint64_t _M0Lm5high1S926;
  int32_t _M0L6_2atmpS2528;
  int32_t _M0L6_2atmpS2529;
  int32_t _M0L5deltaS927;
  uint64_t _M0L6_2atmpS2527;
  uint64_t _M0L6_2atmpS2519;
  int32_t _M0L6_2atmpS2526;
  uint32_t _M0L6_2atmpS2523;
  int32_t _M0L6_2atmpS2525;
  int32_t _M0L6_2atmpS2524;
  uint32_t _M0L6_2atmpS2522;
  uint32_t _M0L6_2atmpS2521;
  uint64_t _M0L6_2atmpS2520;
  uint64_t _M0L1aS928;
  uint64_t _M0L6_2atmpS2518;
  uint64_t _M0L1bS929;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2534 = _M0L1iS913 + 26;
  _M0L6_2atmpS2533 = _M0L6_2atmpS2534 - 1;
  _M0L4baseS912 = _M0L6_2atmpS2533 / 26;
  _M0L5base2S914 = _M0L4baseS912 * 26;
  _M0L6offsetS915 = _M0L5base2S914 - _M0L1iS913;
  _M0L6_2atmpS2532 = _M0L4baseS912 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S916
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2532);
  _M0L6_2atmpS2531 = _M0L4baseS912 * 2;
  _M0L6_2atmpS2530 = _M0L6_2atmpS2531 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S917
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2530);
  if (_M0L6offsetS915 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S916, _M0L4mul1S917};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS918
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS915);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS919 = _M0FPB7umul128(_M0L1mS918, _M0L4mul1S917);
  _M0L7_2alow1S920 = _M0L7_2abindS919.$0;
  _M0L8_2ahigh1S921 = _M0L7_2abindS919.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS922 = _M0FPB7umul128(_M0L1mS918, _M0L4mul0S916);
  _M0L7_2alow0S923 = _M0L7_2abindS922.$0;
  _M0L8_2ahigh0S924 = _M0L7_2abindS922.$1;
  _M0L3sumS925 = _M0L8_2ahigh0S924 + _M0L7_2alow1S920;
  _M0Lm5high1S926 = _M0L8_2ahigh1S921;
  if (_M0L3sumS925 < _M0L8_2ahigh0S924) {
    uint64_t _M0L6_2atmpS2517 = _M0Lm5high1S926;
    _M0Lm5high1S926 = _M0L6_2atmpS2517 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2528 = _M0FPB8pow5bits(_M0L5base2S914);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2529 = _M0FPB8pow5bits(_M0L1iS913);
  _M0L5deltaS927 = _M0L6_2atmpS2528 - _M0L6_2atmpS2529;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2527
  = _M0FPB13shiftright128(_M0L7_2alow0S923, _M0L3sumS925, _M0L5deltaS927);
  _M0L6_2atmpS2519 = _M0L6_2atmpS2527 + 1ull;
  _M0L6_2atmpS2526 = _M0L1iS913 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2523
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2526);
  _M0L6_2atmpS2525 = _M0L1iS913 % 16;
  _M0L6_2atmpS2524 = _M0L6_2atmpS2525 << 1;
  _M0L6_2atmpS2522 = _M0L6_2atmpS2523 >> (_M0L6_2atmpS2524 & 31);
  _M0L6_2atmpS2521 = _M0L6_2atmpS2522 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2520 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2521);
  _M0L1aS928 = _M0L6_2atmpS2519 + _M0L6_2atmpS2520;
  _M0L6_2atmpS2518 = _M0Lm5high1S926;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS929
  = _M0FPB13shiftright128(_M0L3sumS925, _M0L6_2atmpS2518, _M0L5deltaS927);
  return (struct _M0TPB8Pow5Pair){_M0L1aS928, _M0L1bS929};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS895) {
  int32_t _M0L4baseS894;
  int32_t _M0L5base2S896;
  int32_t _M0L6offsetS897;
  int32_t _M0L6_2atmpS2516;
  uint64_t _M0L4mul0S898;
  int32_t _M0L6_2atmpS2515;
  int32_t _M0L6_2atmpS2514;
  uint64_t _M0L4mul1S899;
  uint64_t _M0L1mS900;
  struct _M0TPB7Umul128 _M0L7_2abindS901;
  uint64_t _M0L7_2alow1S902;
  uint64_t _M0L8_2ahigh1S903;
  struct _M0TPB7Umul128 _M0L7_2abindS904;
  uint64_t _M0L7_2alow0S905;
  uint64_t _M0L8_2ahigh0S906;
  uint64_t _M0L3sumS907;
  uint64_t _M0Lm5high1S908;
  int32_t _M0L6_2atmpS2512;
  int32_t _M0L6_2atmpS2513;
  int32_t _M0L5deltaS909;
  uint64_t _M0L6_2atmpS2504;
  int32_t _M0L6_2atmpS2511;
  uint32_t _M0L6_2atmpS2508;
  int32_t _M0L6_2atmpS2510;
  int32_t _M0L6_2atmpS2509;
  uint32_t _M0L6_2atmpS2507;
  uint32_t _M0L6_2atmpS2506;
  uint64_t _M0L6_2atmpS2505;
  uint64_t _M0L1aS910;
  uint64_t _M0L6_2atmpS2503;
  uint64_t _M0L1bS911;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS894 = _M0L1iS895 / 26;
  _M0L5base2S896 = _M0L4baseS894 * 26;
  _M0L6offsetS897 = _M0L1iS895 - _M0L5base2S896;
  _M0L6_2atmpS2516 = _M0L4baseS894 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S898
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2516);
  _M0L6_2atmpS2515 = _M0L4baseS894 * 2;
  _M0L6_2atmpS2514 = _M0L6_2atmpS2515 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S899
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2514);
  if (_M0L6offsetS897 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S898, _M0L4mul1S899};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS900
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS897);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS901 = _M0FPB7umul128(_M0L1mS900, _M0L4mul1S899);
  _M0L7_2alow1S902 = _M0L7_2abindS901.$0;
  _M0L8_2ahigh1S903 = _M0L7_2abindS901.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS904 = _M0FPB7umul128(_M0L1mS900, _M0L4mul0S898);
  _M0L7_2alow0S905 = _M0L7_2abindS904.$0;
  _M0L8_2ahigh0S906 = _M0L7_2abindS904.$1;
  _M0L3sumS907 = _M0L8_2ahigh0S906 + _M0L7_2alow1S902;
  _M0Lm5high1S908 = _M0L8_2ahigh1S903;
  if (_M0L3sumS907 < _M0L8_2ahigh0S906) {
    uint64_t _M0L6_2atmpS2502 = _M0Lm5high1S908;
    _M0Lm5high1S908 = _M0L6_2atmpS2502 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2512 = _M0FPB8pow5bits(_M0L1iS895);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2513 = _M0FPB8pow5bits(_M0L5base2S896);
  _M0L5deltaS909 = _M0L6_2atmpS2512 - _M0L6_2atmpS2513;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2504
  = _M0FPB13shiftright128(_M0L7_2alow0S905, _M0L3sumS907, _M0L5deltaS909);
  _M0L6_2atmpS2511 = _M0L1iS895 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2508
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2511);
  _M0L6_2atmpS2510 = _M0L1iS895 % 16;
  _M0L6_2atmpS2509 = _M0L6_2atmpS2510 << 1;
  _M0L6_2atmpS2507 = _M0L6_2atmpS2508 >> (_M0L6_2atmpS2509 & 31);
  _M0L6_2atmpS2506 = _M0L6_2atmpS2507 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2505 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2506);
  _M0L1aS910 = _M0L6_2atmpS2504 + _M0L6_2atmpS2505;
  _M0L6_2atmpS2503 = _M0Lm5high1S908;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS911
  = _M0FPB13shiftright128(_M0L3sumS907, _M0L6_2atmpS2503, _M0L5deltaS909);
  return (struct _M0TPB8Pow5Pair){_M0L1aS910, _M0L1bS911};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS868,
  struct _M0TPB8Pow5Pair _M0L3mulS865,
  int32_t _M0L1jS881,
  int32_t _M0L7mmShiftS883
) {
  uint64_t _M0L7_2amul0S864;
  uint64_t _M0L7_2amul1S866;
  uint64_t _M0L1mS867;
  struct _M0TPB7Umul128 _M0L7_2abindS869;
  uint64_t _M0L5_2aloS870;
  uint64_t _M0L6_2atmpS871;
  struct _M0TPB7Umul128 _M0L7_2abindS872;
  uint64_t _M0L6_2alo2S873;
  uint64_t _M0L6_2ahi2S874;
  uint64_t _M0L3midS875;
  uint64_t _M0L6_2atmpS2501;
  uint64_t _M0L2hiS876;
  uint64_t _M0L3lo2S877;
  uint64_t _M0L6_2atmpS2499;
  uint64_t _M0L6_2atmpS2500;
  uint64_t _M0L4mid2S878;
  uint64_t _M0L6_2atmpS2498;
  uint64_t _M0L3hi2S879;
  int32_t _M0L6_2atmpS2497;
  int32_t _M0L6_2atmpS2496;
  uint64_t _M0L2vpS880;
  uint64_t _M0Lm2vmS882;
  int32_t _M0L6_2atmpS2495;
  int32_t _M0L6_2atmpS2494;
  uint64_t _M0L2vrS893;
  uint64_t _M0L6_2atmpS2493;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S864 = _M0L3mulS865.$0;
  _M0L7_2amul1S866 = _M0L3mulS865.$1;
  _M0L1mS867 = _M0L1mS868 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS869 = _M0FPB7umul128(_M0L1mS867, _M0L7_2amul0S864);
  _M0L5_2aloS870 = _M0L7_2abindS869.$0;
  _M0L6_2atmpS871 = _M0L7_2abindS869.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS872 = _M0FPB7umul128(_M0L1mS867, _M0L7_2amul1S866);
  _M0L6_2alo2S873 = _M0L7_2abindS872.$0;
  _M0L6_2ahi2S874 = _M0L7_2abindS872.$1;
  _M0L3midS875 = _M0L6_2atmpS871 + _M0L6_2alo2S873;
  if (_M0L3midS875 < _M0L6_2atmpS871) {
    _M0L6_2atmpS2501 = 1ull;
  } else {
    _M0L6_2atmpS2501 = 0ull;
  }
  _M0L2hiS876 = _M0L6_2ahi2S874 + _M0L6_2atmpS2501;
  _M0L3lo2S877 = _M0L5_2aloS870 + _M0L7_2amul0S864;
  _M0L6_2atmpS2499 = _M0L3midS875 + _M0L7_2amul1S866;
  if (_M0L3lo2S877 < _M0L5_2aloS870) {
    _M0L6_2atmpS2500 = 1ull;
  } else {
    _M0L6_2atmpS2500 = 0ull;
  }
  _M0L4mid2S878 = _M0L6_2atmpS2499 + _M0L6_2atmpS2500;
  if (_M0L4mid2S878 < _M0L3midS875) {
    _M0L6_2atmpS2498 = 1ull;
  } else {
    _M0L6_2atmpS2498 = 0ull;
  }
  _M0L3hi2S879 = _M0L2hiS876 + _M0L6_2atmpS2498;
  _M0L6_2atmpS2497 = _M0L1jS881 - 64;
  _M0L6_2atmpS2496 = _M0L6_2atmpS2497 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS880
  = _M0FPB13shiftright128(_M0L4mid2S878, _M0L3hi2S879, _M0L6_2atmpS2496);
  _M0Lm2vmS882 = 0ull;
  if (_M0L7mmShiftS883) {
    uint64_t _M0L3lo3S884 = _M0L5_2aloS870 - _M0L7_2amul0S864;
    uint64_t _M0L6_2atmpS2483 = _M0L3midS875 - _M0L7_2amul1S866;
    uint64_t _M0L6_2atmpS2484;
    uint64_t _M0L4mid3S885;
    uint64_t _M0L6_2atmpS2482;
    uint64_t _M0L3hi3S886;
    int32_t _M0L6_2atmpS2481;
    int32_t _M0L6_2atmpS2480;
    if (_M0L5_2aloS870 < _M0L3lo3S884) {
      _M0L6_2atmpS2484 = 1ull;
    } else {
      _M0L6_2atmpS2484 = 0ull;
    }
    _M0L4mid3S885 = _M0L6_2atmpS2483 - _M0L6_2atmpS2484;
    if (_M0L3midS875 < _M0L4mid3S885) {
      _M0L6_2atmpS2482 = 1ull;
    } else {
      _M0L6_2atmpS2482 = 0ull;
    }
    _M0L3hi3S886 = _M0L2hiS876 - _M0L6_2atmpS2482;
    _M0L6_2atmpS2481 = _M0L1jS881 - 64;
    _M0L6_2atmpS2480 = _M0L6_2atmpS2481 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS882
    = _M0FPB13shiftright128(_M0L4mid3S885, _M0L3hi3S886, _M0L6_2atmpS2480);
  } else {
    uint64_t _M0L3lo3S887 = _M0L5_2aloS870 + _M0L5_2aloS870;
    uint64_t _M0L6_2atmpS2491 = _M0L3midS875 + _M0L3midS875;
    uint64_t _M0L6_2atmpS2492;
    uint64_t _M0L4mid3S888;
    uint64_t _M0L6_2atmpS2489;
    uint64_t _M0L6_2atmpS2490;
    uint64_t _M0L3hi3S889;
    uint64_t _M0L3lo4S890;
    uint64_t _M0L6_2atmpS2487;
    uint64_t _M0L6_2atmpS2488;
    uint64_t _M0L4mid4S891;
    uint64_t _M0L6_2atmpS2486;
    uint64_t _M0L3hi4S892;
    int32_t _M0L6_2atmpS2485;
    if (_M0L3lo3S887 < _M0L5_2aloS870) {
      _M0L6_2atmpS2492 = 1ull;
    } else {
      _M0L6_2atmpS2492 = 0ull;
    }
    _M0L4mid3S888 = _M0L6_2atmpS2491 + _M0L6_2atmpS2492;
    _M0L6_2atmpS2489 = _M0L2hiS876 + _M0L2hiS876;
    if (_M0L4mid3S888 < _M0L3midS875) {
      _M0L6_2atmpS2490 = 1ull;
    } else {
      _M0L6_2atmpS2490 = 0ull;
    }
    _M0L3hi3S889 = _M0L6_2atmpS2489 + _M0L6_2atmpS2490;
    _M0L3lo4S890 = _M0L3lo3S887 - _M0L7_2amul0S864;
    _M0L6_2atmpS2487 = _M0L4mid3S888 - _M0L7_2amul1S866;
    if (_M0L3lo3S887 < _M0L3lo4S890) {
      _M0L6_2atmpS2488 = 1ull;
    } else {
      _M0L6_2atmpS2488 = 0ull;
    }
    _M0L4mid4S891 = _M0L6_2atmpS2487 - _M0L6_2atmpS2488;
    if (_M0L4mid3S888 < _M0L4mid4S891) {
      _M0L6_2atmpS2486 = 1ull;
    } else {
      _M0L6_2atmpS2486 = 0ull;
    }
    _M0L3hi4S892 = _M0L3hi3S889 - _M0L6_2atmpS2486;
    _M0L6_2atmpS2485 = _M0L1jS881 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS882
    = _M0FPB13shiftright128(_M0L4mid4S891, _M0L3hi4S892, _M0L6_2atmpS2485);
  }
  _M0L6_2atmpS2495 = _M0L1jS881 - 64;
  _M0L6_2atmpS2494 = _M0L6_2atmpS2495 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS893
  = _M0FPB13shiftright128(_M0L3midS875, _M0L2hiS876, _M0L6_2atmpS2494);
  _M0L6_2atmpS2493 = _M0Lm2vmS882;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS893,
                                                _M0L2vpS880,
                                                _M0L6_2atmpS2493};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS862,
  int32_t _M0L1pS863
) {
  uint64_t _M0L6_2atmpS2479;
  uint64_t _M0L6_2atmpS2478;
  uint64_t _M0L6_2atmpS2477;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2479 = 1ull << (_M0L1pS863 & 63);
  _M0L6_2atmpS2478 = _M0L6_2atmpS2479 - 1ull;
  _M0L6_2atmpS2477 = _M0L5valueS862 & _M0L6_2atmpS2478;
  return _M0L6_2atmpS2477 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS860,
  int32_t _M0L1pS861
) {
  int32_t _M0L6_2atmpS2476;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2476 = _M0FPB10pow5Factor(_M0L5valueS860);
  return _M0L6_2atmpS2476 >= _M0L1pS861;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS856) {
  uint64_t _M0L6_2atmpS2464;
  uint64_t _M0L6_2atmpS2465;
  uint64_t _M0L6_2atmpS2466;
  uint64_t _M0L6_2atmpS2467;
  int32_t _M0Lm5countS857;
  uint64_t _M0Lm5valueS858;
  uint64_t _M0L6_2atmpS2475;
  moonbit_string_t _M0L6_2atmpS2474;
  moonbit_string_t _M0L6_2atmpS3780;
  moonbit_string_t _M0L6_2atmpS2473;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2464 = _M0L5valueS856 % 5ull;
  if (_M0L6_2atmpS2464 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2465 = _M0L5valueS856 % 25ull;
  if (_M0L6_2atmpS2465 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2466 = _M0L5valueS856 % 125ull;
  if (_M0L6_2atmpS2466 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2467 = _M0L5valueS856 % 625ull;
  if (_M0L6_2atmpS2467 != 0ull) {
    return 3;
  }
  _M0Lm5countS857 = 4;
  _M0Lm5valueS858 = _M0L5valueS856 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2468 = _M0Lm5valueS858;
    if (_M0L6_2atmpS2468 > 0ull) {
      uint64_t _M0L6_2atmpS2470 = _M0Lm5valueS858;
      uint64_t _M0L6_2atmpS2469 = _M0L6_2atmpS2470 % 5ull;
      uint64_t _M0L6_2atmpS2471;
      int32_t _M0L6_2atmpS2472;
      if (_M0L6_2atmpS2469 != 0ull) {
        return _M0Lm5countS857;
      }
      _M0L6_2atmpS2471 = _M0Lm5valueS858;
      _M0Lm5valueS858 = _M0L6_2atmpS2471 / 5ull;
      _M0L6_2atmpS2472 = _M0Lm5countS857;
      _M0Lm5countS857 = _M0L6_2atmpS2472 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2475 = _M0Lm5valueS858;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2474
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2475);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3780
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_86.data, _M0L6_2atmpS2474);
  moonbit_decref(_M0L6_2atmpS2474);
  _M0L6_2atmpS2473 = _M0L6_2atmpS3780;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2473, (moonbit_string_t)moonbit_string_literal_87.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS855,
  uint64_t _M0L2hiS853,
  int32_t _M0L4distS854
) {
  int32_t _M0L6_2atmpS2463;
  uint64_t _M0L6_2atmpS2461;
  uint64_t _M0L6_2atmpS2462;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2463 = 64 - _M0L4distS854;
  _M0L6_2atmpS2461 = _M0L2hiS853 << (_M0L6_2atmpS2463 & 63);
  _M0L6_2atmpS2462 = _M0L2loS855 >> (_M0L4distS854 & 63);
  return _M0L6_2atmpS2461 | _M0L6_2atmpS2462;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS843,
  uint64_t _M0L1bS846
) {
  uint64_t _M0L3aLoS842;
  uint64_t _M0L3aHiS844;
  uint64_t _M0L3bLoS845;
  uint64_t _M0L3bHiS847;
  uint64_t _M0L1xS848;
  uint64_t _M0L6_2atmpS2459;
  uint64_t _M0L6_2atmpS2460;
  uint64_t _M0L1yS849;
  uint64_t _M0L6_2atmpS2457;
  uint64_t _M0L6_2atmpS2458;
  uint64_t _M0L1zS850;
  uint64_t _M0L6_2atmpS2455;
  uint64_t _M0L6_2atmpS2456;
  uint64_t _M0L6_2atmpS2453;
  uint64_t _M0L6_2atmpS2454;
  uint64_t _M0L1wS851;
  uint64_t _M0L2loS852;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS842 = _M0L1aS843 & 4294967295ull;
  _M0L3aHiS844 = _M0L1aS843 >> 32;
  _M0L3bLoS845 = _M0L1bS846 & 4294967295ull;
  _M0L3bHiS847 = _M0L1bS846 >> 32;
  _M0L1xS848 = _M0L3aLoS842 * _M0L3bLoS845;
  _M0L6_2atmpS2459 = _M0L3aHiS844 * _M0L3bLoS845;
  _M0L6_2atmpS2460 = _M0L1xS848 >> 32;
  _M0L1yS849 = _M0L6_2atmpS2459 + _M0L6_2atmpS2460;
  _M0L6_2atmpS2457 = _M0L3aLoS842 * _M0L3bHiS847;
  _M0L6_2atmpS2458 = _M0L1yS849 & 4294967295ull;
  _M0L1zS850 = _M0L6_2atmpS2457 + _M0L6_2atmpS2458;
  _M0L6_2atmpS2455 = _M0L3aHiS844 * _M0L3bHiS847;
  _M0L6_2atmpS2456 = _M0L1yS849 >> 32;
  _M0L6_2atmpS2453 = _M0L6_2atmpS2455 + _M0L6_2atmpS2456;
  _M0L6_2atmpS2454 = _M0L1zS850 >> 32;
  _M0L1wS851 = _M0L6_2atmpS2453 + _M0L6_2atmpS2454;
  _M0L2loS852 = _M0L1aS843 * _M0L1bS846;
  return (struct _M0TPB7Umul128){_M0L2loS852, _M0L1wS851};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS837,
  int32_t _M0L4fromS841,
  int32_t _M0L2toS839
) {
  int32_t _M0L6_2atmpS2452;
  struct _M0TPB13StringBuilder* _M0L3bufS836;
  int32_t _M0L1iS838;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2452 = Moonbit_array_length(_M0L5bytesS837);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS836 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2452);
  _M0L1iS838 = _M0L4fromS841;
  while (1) {
    if (_M0L1iS838 < _M0L2toS839) {
      int32_t _M0L6_2atmpS2450;
      int32_t _M0L6_2atmpS2449;
      int32_t _M0L6_2atmpS2451;
      if (
        _M0L1iS838 < 0 || _M0L1iS838 >= Moonbit_array_length(_M0L5bytesS837)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2450 = (int32_t)_M0L5bytesS837[_M0L1iS838];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2449 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2450);
      moonbit_incref(_M0L3bufS836);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS836, _M0L6_2atmpS2449);
      _M0L6_2atmpS2451 = _M0L1iS838 + 1;
      _M0L1iS838 = _M0L6_2atmpS2451;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS837);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS836);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS835) {
  int32_t _M0L6_2atmpS2448;
  uint32_t _M0L6_2atmpS2447;
  uint32_t _M0L6_2atmpS2446;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2448 = _M0L1eS835 * 78913;
  _M0L6_2atmpS2447 = *(uint32_t*)&_M0L6_2atmpS2448;
  _M0L6_2atmpS2446 = _M0L6_2atmpS2447 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2446;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS834) {
  int32_t _M0L6_2atmpS2445;
  uint32_t _M0L6_2atmpS2444;
  uint32_t _M0L6_2atmpS2443;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2445 = _M0L1eS834 * 732923;
  _M0L6_2atmpS2444 = *(uint32_t*)&_M0L6_2atmpS2445;
  _M0L6_2atmpS2443 = _M0L6_2atmpS2444 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2443;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS832,
  int32_t _M0L8exponentS833,
  int32_t _M0L8mantissaS830
) {
  moonbit_string_t _M0L1sS831;
  moonbit_string_t _M0L6_2atmpS3781;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS830) {
    return (moonbit_string_t)moonbit_string_literal_88.data;
  }
  if (_M0L4signS832) {
    _M0L1sS831 = (moonbit_string_t)moonbit_string_literal_89.data;
  } else {
    _M0L1sS831 = (moonbit_string_t)moonbit_string_literal_3.data;
  }
  if (_M0L8exponentS833) {
    moonbit_string_t _M0L6_2atmpS3782;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3782
    = moonbit_add_string(_M0L1sS831, (moonbit_string_t)moonbit_string_literal_90.data);
    moonbit_decref(_M0L1sS831);
    return _M0L6_2atmpS3782;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3781
  = moonbit_add_string(_M0L1sS831, (moonbit_string_t)moonbit_string_literal_91.data);
  moonbit_decref(_M0L1sS831);
  return _M0L6_2atmpS3781;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS829) {
  int32_t _M0L6_2atmpS2442;
  uint32_t _M0L6_2atmpS2441;
  uint32_t _M0L6_2atmpS2440;
  int32_t _M0L6_2atmpS2439;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2442 = _M0L1eS829 * 1217359;
  _M0L6_2atmpS2441 = *(uint32_t*)&_M0L6_2atmpS2442;
  _M0L6_2atmpS2440 = _M0L6_2atmpS2441 >> 19;
  _M0L6_2atmpS2439 = *(int32_t*)&_M0L6_2atmpS2440;
  return _M0L6_2atmpS2439 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS828,
  struct _M0TPB6Hasher* _M0L6hasherS827
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS827, _M0L4selfS828);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS826,
  struct _M0TPB6Hasher* _M0L6hasherS825
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS825, _M0L4selfS826);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS823,
  moonbit_string_t _M0L5valueS821
) {
  int32_t _M0L7_2abindS820;
  int32_t _M0L1iS822;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS820 = Moonbit_array_length(_M0L5valueS821);
  _M0L1iS822 = 0;
  while (1) {
    if (_M0L1iS822 < _M0L7_2abindS820) {
      int32_t _M0L6_2atmpS2437 = _M0L5valueS821[_M0L1iS822];
      int32_t _M0L6_2atmpS2436 = (int32_t)_M0L6_2atmpS2437;
      uint32_t _M0L6_2atmpS2435 = *(uint32_t*)&_M0L6_2atmpS2436;
      int32_t _M0L6_2atmpS2438;
      moonbit_incref(_M0L4selfS823);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS823, _M0L6_2atmpS2435);
      _M0L6_2atmpS2438 = _M0L1iS822 + 1;
      _M0L1iS822 = _M0L6_2atmpS2438;
      continue;
    } else {
      moonbit_decref(_M0L4selfS823);
      moonbit_decref(_M0L5valueS821);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS818,
  int32_t _M0L3idxS819
) {
  int32_t _M0L6_2atmpS3783;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3783 = _M0L4selfS818[_M0L3idxS819];
  moonbit_decref(_M0L4selfS818);
  return _M0L6_2atmpS3783;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS817) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS817;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS816) {
  double _M0L6_2atmpS2433;
  moonbit_string_t _M0L6_2atmpS2434;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2433 = (double)_M0L4selfS816;
  _M0L6_2atmpS2434 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2433, _M0L6_2atmpS2434);
}

void* _M0IPC14bool4BoolPB6ToJson8to__json(int32_t _M0L4selfS815) {
  #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS815) {
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json7boolean(1);
  } else {
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json7boolean(0);
  }
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS814) {
  void* _block_4272;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4272 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_4272)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_4272)->$0 = _M0L6objectS814;
  return _block_4272;
}

void* _M0MPC14json4Json7boolean(int32_t _M0L7booleanS813) {
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L7booleanS813) {
    return (struct moonbit_object*)&moonbit_constant_constructor_1 + 1;
  } else {
    return (struct moonbit_object*)&moonbit_constant_constructor_2 + 1;
  }
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS812) {
  void* _block_4273;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4273 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4273)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4273)->$0 = _M0L6stringS812;
  return _block_4273;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS805
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3784;
  int32_t _M0L6_2acntS4048;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2432;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS804;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__* _closure_4274;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2427;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3784 = _M0L4selfS805->$5;
  _M0L6_2acntS4048 = Moonbit_object_header(_M0L4selfS805)->rc;
  if (_M0L6_2acntS4048 > 1) {
    int32_t _M0L11_2anew__cntS4050 = _M0L6_2acntS4048 - 1;
    Moonbit_object_header(_M0L4selfS805)->rc = _M0L11_2anew__cntS4050;
    if (_M0L8_2afieldS3784) {
      moonbit_incref(_M0L8_2afieldS3784);
    }
  } else if (_M0L6_2acntS4048 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4049 = _M0L4selfS805->$0;
    moonbit_decref(_M0L8_2afieldS4049);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS805);
  }
  _M0L4headS2432 = _M0L8_2afieldS3784;
  _M0L11curr__entryS804
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS804)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS804->$0 = _M0L4headS2432;
  _closure_4274
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__));
  Moonbit_object_header(_closure_4274)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__, $0) >> 2, 1, 0);
  _closure_4274->code = &_M0MPB3Map4iterGsRPB4JsonEC2428l591;
  _closure_4274->$0 = _M0L11curr__entryS804;
  _M0L6_2atmpS2427 = (struct _M0TWEOUsRPB4JsonE*)_closure_4274;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2427);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2428l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2429
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__* _M0L14_2acasted__envS2430;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3790;
  int32_t _M0L6_2acntS4051;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS804;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3789;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS806;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2430
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2428__l591__*)_M0L6_2aenvS2429;
  _M0L8_2afieldS3790 = _M0L14_2acasted__envS2430->$0;
  _M0L6_2acntS4051 = Moonbit_object_header(_M0L14_2acasted__envS2430)->rc;
  if (_M0L6_2acntS4051 > 1) {
    int32_t _M0L11_2anew__cntS4052 = _M0L6_2acntS4051 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2430)->rc
    = _M0L11_2anew__cntS4052;
    moonbit_incref(_M0L8_2afieldS3790);
  } else if (_M0L6_2acntS4051 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2430);
  }
  _M0L11curr__entryS804 = _M0L8_2afieldS3790;
  _M0L8_2afieldS3789 = _M0L11curr__entryS804->$0;
  _M0L7_2abindS806 = _M0L8_2afieldS3789;
  if (_M0L7_2abindS806 == 0) {
    moonbit_decref(_M0L11curr__entryS804);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS807 = _M0L7_2abindS806;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS808 = _M0L7_2aSomeS807;
    moonbit_string_t _M0L8_2afieldS3788 = _M0L4_2axS808->$4;
    moonbit_string_t _M0L6_2akeyS809 = _M0L8_2afieldS3788;
    void* _M0L8_2afieldS3787 = _M0L4_2axS808->$5;
    void* _M0L8_2avalueS810 = _M0L8_2afieldS3787;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3786 = _M0L4_2axS808->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS811 = _M0L8_2afieldS3786;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3785 =
      _M0L11curr__entryS804->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2431;
    if (_M0L7_2anextS811) {
      moonbit_incref(_M0L7_2anextS811);
    }
    moonbit_incref(_M0L8_2avalueS810);
    moonbit_incref(_M0L6_2akeyS809);
    if (_M0L6_2aoldS3785) {
      moonbit_decref(_M0L6_2aoldS3785);
    }
    _M0L11curr__entryS804->$0 = _M0L7_2anextS811;
    moonbit_decref(_M0L11curr__entryS804);
    _M0L8_2atupleS2431
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2431)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2431->$0 = _M0L6_2akeyS809;
    _M0L8_2atupleS2431->$1 = _M0L8_2avalueS810;
    return _M0L8_2atupleS2431;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS803
) {
  int32_t _M0L8_2afieldS3791;
  int32_t _M0L4sizeS2426;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3791 = _M0L4selfS803->$1;
  moonbit_decref(_M0L4selfS803);
  _M0L4sizeS2426 = _M0L8_2afieldS3791;
  return _M0L4sizeS2426 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS790,
  int32_t _M0L3keyS786
) {
  int32_t _M0L4hashS785;
  int32_t _M0L14capacity__maskS2411;
  int32_t _M0L6_2atmpS2410;
  int32_t _M0L1iS787;
  int32_t _M0L3idxS788;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS785 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS786);
  _M0L14capacity__maskS2411 = _M0L4selfS790->$3;
  _M0L6_2atmpS2410 = _M0L4hashS785 & _M0L14capacity__maskS2411;
  _M0L1iS787 = 0;
  _M0L3idxS788 = _M0L6_2atmpS2410;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3795 =
      _M0L4selfS790->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2409 =
      _M0L8_2afieldS3795;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3794;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS789;
    if (
      _M0L3idxS788 < 0
      || _M0L3idxS788 >= Moonbit_array_length(_M0L7entriesS2409)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3794
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2409[
        _M0L3idxS788
      ];
    _M0L7_2abindS789 = _M0L6_2atmpS3794;
    if (_M0L7_2abindS789 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2398;
      if (_M0L7_2abindS789) {
        moonbit_incref(_M0L7_2abindS789);
      }
      moonbit_decref(_M0L4selfS790);
      if (_M0L7_2abindS789) {
        moonbit_decref(_M0L7_2abindS789);
      }
      _M0L6_2atmpS2398 = 0;
      return _M0L6_2atmpS2398;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS791 =
        _M0L7_2abindS789;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS792 =
        _M0L7_2aSomeS791;
      int32_t _M0L4hashS2400 = _M0L8_2aentryS792->$3;
      int32_t _if__result_4276;
      int32_t _M0L8_2afieldS3792;
      int32_t _M0L3pslS2403;
      int32_t _M0L6_2atmpS2405;
      int32_t _M0L6_2atmpS2407;
      int32_t _M0L14capacity__maskS2408;
      int32_t _M0L6_2atmpS2406;
      if (_M0L4hashS2400 == _M0L4hashS785) {
        int32_t _M0L3keyS2399 = _M0L8_2aentryS792->$4;
        _if__result_4276 = _M0L3keyS2399 == _M0L3keyS786;
      } else {
        _if__result_4276 = 0;
      }
      if (_if__result_4276) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3793;
        int32_t _M0L6_2acntS4053;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2402;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2401;
        moonbit_incref(_M0L8_2aentryS792);
        moonbit_decref(_M0L4selfS790);
        _M0L8_2afieldS3793 = _M0L8_2aentryS792->$5;
        _M0L6_2acntS4053 = Moonbit_object_header(_M0L8_2aentryS792)->rc;
        if (_M0L6_2acntS4053 > 1) {
          int32_t _M0L11_2anew__cntS4055 = _M0L6_2acntS4053 - 1;
          Moonbit_object_header(_M0L8_2aentryS792)->rc
          = _M0L11_2anew__cntS4055;
          moonbit_incref(_M0L8_2afieldS3793);
        } else if (_M0L6_2acntS4053 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4054 =
            _M0L8_2aentryS792->$1;
          if (_M0L8_2afieldS4054) {
            moonbit_decref(_M0L8_2afieldS4054);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS792);
        }
        _M0L5valueS2402 = _M0L8_2afieldS3793;
        _M0L6_2atmpS2401 = _M0L5valueS2402;
        return _M0L6_2atmpS2401;
      } else {
        moonbit_incref(_M0L8_2aentryS792);
      }
      _M0L8_2afieldS3792 = _M0L8_2aentryS792->$2;
      moonbit_decref(_M0L8_2aentryS792);
      _M0L3pslS2403 = _M0L8_2afieldS3792;
      if (_M0L1iS787 > _M0L3pslS2403) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2404;
        moonbit_decref(_M0L4selfS790);
        _M0L6_2atmpS2404 = 0;
        return _M0L6_2atmpS2404;
      }
      _M0L6_2atmpS2405 = _M0L1iS787 + 1;
      _M0L6_2atmpS2407 = _M0L3idxS788 + 1;
      _M0L14capacity__maskS2408 = _M0L4selfS790->$3;
      _M0L6_2atmpS2406 = _M0L6_2atmpS2407 & _M0L14capacity__maskS2408;
      _M0L1iS787 = _M0L6_2atmpS2405;
      _M0L3idxS788 = _M0L6_2atmpS2406;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS799,
  moonbit_string_t _M0L3keyS795
) {
  int32_t _M0L4hashS794;
  int32_t _M0L14capacity__maskS2425;
  int32_t _M0L6_2atmpS2424;
  int32_t _M0L1iS796;
  int32_t _M0L3idxS797;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS795);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS794 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS795);
  _M0L14capacity__maskS2425 = _M0L4selfS799->$3;
  _M0L6_2atmpS2424 = _M0L4hashS794 & _M0L14capacity__maskS2425;
  _M0L1iS796 = 0;
  _M0L3idxS797 = _M0L6_2atmpS2424;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3801 =
      _M0L4selfS799->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2423 =
      _M0L8_2afieldS3801;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3800;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS798;
    if (
      _M0L3idxS797 < 0
      || _M0L3idxS797 >= Moonbit_array_length(_M0L7entriesS2423)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3800
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2423[
        _M0L3idxS797
      ];
    _M0L7_2abindS798 = _M0L6_2atmpS3800;
    if (_M0L7_2abindS798 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2412;
      if (_M0L7_2abindS798) {
        moonbit_incref(_M0L7_2abindS798);
      }
      moonbit_decref(_M0L4selfS799);
      if (_M0L7_2abindS798) {
        moonbit_decref(_M0L7_2abindS798);
      }
      moonbit_decref(_M0L3keyS795);
      _M0L6_2atmpS2412 = 0;
      return _M0L6_2atmpS2412;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS800 =
        _M0L7_2abindS798;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS801 =
        _M0L7_2aSomeS800;
      int32_t _M0L4hashS2414 = _M0L8_2aentryS801->$3;
      int32_t _if__result_4278;
      int32_t _M0L8_2afieldS3796;
      int32_t _M0L3pslS2417;
      int32_t _M0L6_2atmpS2419;
      int32_t _M0L6_2atmpS2421;
      int32_t _M0L14capacity__maskS2422;
      int32_t _M0L6_2atmpS2420;
      if (_M0L4hashS2414 == _M0L4hashS794) {
        moonbit_string_t _M0L8_2afieldS3799 = _M0L8_2aentryS801->$4;
        moonbit_string_t _M0L3keyS2413 = _M0L8_2afieldS3799;
        int32_t _M0L6_2atmpS3798;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3798
        = moonbit_val_array_equal(_M0L3keyS2413, _M0L3keyS795);
        _if__result_4278 = _M0L6_2atmpS3798;
      } else {
        _if__result_4278 = 0;
      }
      if (_if__result_4278) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3797;
        int32_t _M0L6_2acntS4056;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2416;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2415;
        moonbit_incref(_M0L8_2aentryS801);
        moonbit_decref(_M0L4selfS799);
        moonbit_decref(_M0L3keyS795);
        _M0L8_2afieldS3797 = _M0L8_2aentryS801->$5;
        _M0L6_2acntS4056 = Moonbit_object_header(_M0L8_2aentryS801)->rc;
        if (_M0L6_2acntS4056 > 1) {
          int32_t _M0L11_2anew__cntS4059 = _M0L6_2acntS4056 - 1;
          Moonbit_object_header(_M0L8_2aentryS801)->rc
          = _M0L11_2anew__cntS4059;
          moonbit_incref(_M0L8_2afieldS3797);
        } else if (_M0L6_2acntS4056 == 1) {
          moonbit_string_t _M0L8_2afieldS4058 = _M0L8_2aentryS801->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4057;
          moonbit_decref(_M0L8_2afieldS4058);
          _M0L8_2afieldS4057 = _M0L8_2aentryS801->$1;
          if (_M0L8_2afieldS4057) {
            moonbit_decref(_M0L8_2afieldS4057);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS801);
        }
        _M0L5valueS2416 = _M0L8_2afieldS3797;
        _M0L6_2atmpS2415 = _M0L5valueS2416;
        return _M0L6_2atmpS2415;
      } else {
        moonbit_incref(_M0L8_2aentryS801);
      }
      _M0L8_2afieldS3796 = _M0L8_2aentryS801->$2;
      moonbit_decref(_M0L8_2aentryS801);
      _M0L3pslS2417 = _M0L8_2afieldS3796;
      if (_M0L1iS796 > _M0L3pslS2417) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2418;
        moonbit_decref(_M0L4selfS799);
        moonbit_decref(_M0L3keyS795);
        _M0L6_2atmpS2418 = 0;
        return _M0L6_2atmpS2418;
      }
      _M0L6_2atmpS2419 = _M0L1iS796 + 1;
      _M0L6_2atmpS2421 = _M0L3idxS797 + 1;
      _M0L14capacity__maskS2422 = _M0L4selfS799->$3;
      _M0L6_2atmpS2420 = _M0L6_2atmpS2421 & _M0L14capacity__maskS2422;
      _M0L1iS796 = _M0L6_2atmpS2419;
      _M0L3idxS797 = _M0L6_2atmpS2420;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS762
) {
  int32_t _M0L6lengthS761;
  int32_t _M0Lm8capacityS763;
  int32_t _M0L6_2atmpS2363;
  int32_t _M0L6_2atmpS2362;
  int32_t _M0L6_2atmpS2373;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS764;
  int32_t _M0L3endS2371;
  int32_t _M0L5startS2372;
  int32_t _M0L7_2abindS765;
  int32_t _M0L2__S766;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS762.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS761
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS762);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS763 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS761);
  _M0L6_2atmpS2363 = _M0Lm8capacityS763;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2362 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2363);
  if (_M0L6lengthS761 > _M0L6_2atmpS2362) {
    int32_t _M0L6_2atmpS2364 = _M0Lm8capacityS763;
    _M0Lm8capacityS763 = _M0L6_2atmpS2364 * 2;
  }
  _M0L6_2atmpS2373 = _M0Lm8capacityS763;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS764
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2373);
  _M0L3endS2371 = _M0L3arrS762.$2;
  _M0L5startS2372 = _M0L3arrS762.$1;
  _M0L7_2abindS765 = _M0L3endS2371 - _M0L5startS2372;
  _M0L2__S766 = 0;
  while (1) {
    if (_M0L2__S766 < _M0L7_2abindS765) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3805 =
        _M0L3arrS762.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2368 =
        _M0L8_2afieldS3805;
      int32_t _M0L5startS2370 = _M0L3arrS762.$1;
      int32_t _M0L6_2atmpS2369 = _M0L5startS2370 + _M0L2__S766;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3804 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2368[
          _M0L6_2atmpS2369
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS767 =
        _M0L6_2atmpS3804;
      moonbit_string_t _M0L8_2afieldS3803 = _M0L1eS767->$0;
      moonbit_string_t _M0L6_2atmpS2365 = _M0L8_2afieldS3803;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3802 =
        _M0L1eS767->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2366 =
        _M0L8_2afieldS3802;
      int32_t _M0L6_2atmpS2367;
      moonbit_incref(_M0L6_2atmpS2366);
      moonbit_incref(_M0L6_2atmpS2365);
      moonbit_incref(_M0L1mS764);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS764, _M0L6_2atmpS2365, _M0L6_2atmpS2366);
      _M0L6_2atmpS2367 = _M0L2__S766 + 1;
      _M0L2__S766 = _M0L6_2atmpS2367;
      continue;
    } else {
      moonbit_decref(_M0L3arrS762.$0);
    }
    break;
  }
  return _M0L1mS764;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS770
) {
  int32_t _M0L6lengthS769;
  int32_t _M0Lm8capacityS771;
  int32_t _M0L6_2atmpS2375;
  int32_t _M0L6_2atmpS2374;
  int32_t _M0L6_2atmpS2385;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS772;
  int32_t _M0L3endS2383;
  int32_t _M0L5startS2384;
  int32_t _M0L7_2abindS773;
  int32_t _M0L2__S774;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS770.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS769
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS770);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS771 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS769);
  _M0L6_2atmpS2375 = _M0Lm8capacityS771;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2374 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2375);
  if (_M0L6lengthS769 > _M0L6_2atmpS2374) {
    int32_t _M0L6_2atmpS2376 = _M0Lm8capacityS771;
    _M0Lm8capacityS771 = _M0L6_2atmpS2376 * 2;
  }
  _M0L6_2atmpS2385 = _M0Lm8capacityS771;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS772
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2385);
  _M0L3endS2383 = _M0L3arrS770.$2;
  _M0L5startS2384 = _M0L3arrS770.$1;
  _M0L7_2abindS773 = _M0L3endS2383 - _M0L5startS2384;
  _M0L2__S774 = 0;
  while (1) {
    if (_M0L2__S774 < _M0L7_2abindS773) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3808 =
        _M0L3arrS770.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2380 =
        _M0L8_2afieldS3808;
      int32_t _M0L5startS2382 = _M0L3arrS770.$1;
      int32_t _M0L6_2atmpS2381 = _M0L5startS2382 + _M0L2__S774;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3807 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2380[
          _M0L6_2atmpS2381
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS775 = _M0L6_2atmpS3807;
      int32_t _M0L6_2atmpS2377 = _M0L1eS775->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3806 =
        _M0L1eS775->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2378 =
        _M0L8_2afieldS3806;
      int32_t _M0L6_2atmpS2379;
      moonbit_incref(_M0L6_2atmpS2378);
      moonbit_incref(_M0L1mS772);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS772, _M0L6_2atmpS2377, _M0L6_2atmpS2378);
      _M0L6_2atmpS2379 = _M0L2__S774 + 1;
      _M0L2__S774 = _M0L6_2atmpS2379;
      continue;
    } else {
      moonbit_decref(_M0L3arrS770.$0);
    }
    break;
  }
  return _M0L1mS772;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS778
) {
  int32_t _M0L6lengthS777;
  int32_t _M0Lm8capacityS779;
  int32_t _M0L6_2atmpS2387;
  int32_t _M0L6_2atmpS2386;
  int32_t _M0L6_2atmpS2397;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS780;
  int32_t _M0L3endS2395;
  int32_t _M0L5startS2396;
  int32_t _M0L7_2abindS781;
  int32_t _M0L2__S782;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS778.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS777 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS778);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS779 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS777);
  _M0L6_2atmpS2387 = _M0Lm8capacityS779;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2386 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2387);
  if (_M0L6lengthS777 > _M0L6_2atmpS2386) {
    int32_t _M0L6_2atmpS2388 = _M0Lm8capacityS779;
    _M0Lm8capacityS779 = _M0L6_2atmpS2388 * 2;
  }
  _M0L6_2atmpS2397 = _M0Lm8capacityS779;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS780 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2397);
  _M0L3endS2395 = _M0L3arrS778.$2;
  _M0L5startS2396 = _M0L3arrS778.$1;
  _M0L7_2abindS781 = _M0L3endS2395 - _M0L5startS2396;
  _M0L2__S782 = 0;
  while (1) {
    if (_M0L2__S782 < _M0L7_2abindS781) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS3812 = _M0L3arrS778.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2392 = _M0L8_2afieldS3812;
      int32_t _M0L5startS2394 = _M0L3arrS778.$1;
      int32_t _M0L6_2atmpS2393 = _M0L5startS2394 + _M0L2__S782;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS3811 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2392[_M0L6_2atmpS2393];
      struct _M0TUsRPB4JsonE* _M0L1eS783 = _M0L6_2atmpS3811;
      moonbit_string_t _M0L8_2afieldS3810 = _M0L1eS783->$0;
      moonbit_string_t _M0L6_2atmpS2389 = _M0L8_2afieldS3810;
      void* _M0L8_2afieldS3809 = _M0L1eS783->$1;
      void* _M0L6_2atmpS2390 = _M0L8_2afieldS3809;
      int32_t _M0L6_2atmpS2391;
      moonbit_incref(_M0L6_2atmpS2390);
      moonbit_incref(_M0L6_2atmpS2389);
      moonbit_incref(_M0L1mS780);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS780, _M0L6_2atmpS2389, _M0L6_2atmpS2390);
      _M0L6_2atmpS2391 = _M0L2__S782 + 1;
      _M0L2__S782 = _M0L6_2atmpS2391;
      continue;
    } else {
      moonbit_decref(_M0L3arrS778.$0);
    }
    break;
  }
  return _M0L1mS780;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS752,
  moonbit_string_t _M0L3keyS753,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS754
) {
  int32_t _M0L6_2atmpS2359;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS753);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2359 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS753);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS752, _M0L3keyS753, _M0L5valueS754, _M0L6_2atmpS2359);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS755,
  int32_t _M0L3keyS756,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS757
) {
  int32_t _M0L6_2atmpS2360;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2360 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS756);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS755, _M0L3keyS756, _M0L5valueS757, _M0L6_2atmpS2360);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS758,
  moonbit_string_t _M0L3keyS759,
  void* _M0L5valueS760
) {
  int32_t _M0L6_2atmpS2361;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS759);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2361 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS759);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS758, _M0L3keyS759, _M0L5valueS760, _M0L6_2atmpS2361);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS720
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3819;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS719;
  int32_t _M0L8capacityS2344;
  int32_t _M0L13new__capacityS721;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2339;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2338;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3818;
  int32_t _M0L6_2atmpS2340;
  int32_t _M0L8capacityS2342;
  int32_t _M0L6_2atmpS2341;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2343;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3817;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS722;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3819 = _M0L4selfS720->$5;
  _M0L9old__headS719 = _M0L8_2afieldS3819;
  _M0L8capacityS2344 = _M0L4selfS720->$2;
  _M0L13new__capacityS721 = _M0L8capacityS2344 << 1;
  _M0L6_2atmpS2339 = 0;
  _M0L6_2atmpS2338
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS721, _M0L6_2atmpS2339);
  _M0L6_2aoldS3818 = _M0L4selfS720->$0;
  if (_M0L9old__headS719) {
    moonbit_incref(_M0L9old__headS719);
  }
  moonbit_decref(_M0L6_2aoldS3818);
  _M0L4selfS720->$0 = _M0L6_2atmpS2338;
  _M0L4selfS720->$2 = _M0L13new__capacityS721;
  _M0L6_2atmpS2340 = _M0L13new__capacityS721 - 1;
  _M0L4selfS720->$3 = _M0L6_2atmpS2340;
  _M0L8capacityS2342 = _M0L4selfS720->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2341 = _M0FPB21calc__grow__threshold(_M0L8capacityS2342);
  _M0L4selfS720->$4 = _M0L6_2atmpS2341;
  _M0L4selfS720->$1 = 0;
  _M0L6_2atmpS2343 = 0;
  _M0L6_2aoldS3817 = _M0L4selfS720->$5;
  if (_M0L6_2aoldS3817) {
    moonbit_decref(_M0L6_2aoldS3817);
  }
  _M0L4selfS720->$5 = _M0L6_2atmpS2343;
  _M0L4selfS720->$6 = -1;
  _M0L8_2aparamS722 = _M0L9old__headS719;
  while (1) {
    if (_M0L8_2aparamS722 == 0) {
      if (_M0L8_2aparamS722) {
        moonbit_decref(_M0L8_2aparamS722);
      }
      moonbit_decref(_M0L4selfS720);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS723 =
        _M0L8_2aparamS722;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS724 =
        _M0L7_2aSomeS723;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3816 =
        _M0L4_2axS724->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS725 =
        _M0L8_2afieldS3816;
      moonbit_string_t _M0L8_2afieldS3815 = _M0L4_2axS724->$4;
      moonbit_string_t _M0L6_2akeyS726 = _M0L8_2afieldS3815;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3814 =
        _M0L4_2axS724->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS727 =
        _M0L8_2afieldS3814;
      int32_t _M0L8_2afieldS3813 = _M0L4_2axS724->$3;
      int32_t _M0L6_2acntS4060 = Moonbit_object_header(_M0L4_2axS724)->rc;
      int32_t _M0L7_2ahashS728;
      if (_M0L6_2acntS4060 > 1) {
        int32_t _M0L11_2anew__cntS4061 = _M0L6_2acntS4060 - 1;
        Moonbit_object_header(_M0L4_2axS724)->rc = _M0L11_2anew__cntS4061;
        moonbit_incref(_M0L8_2avalueS727);
        moonbit_incref(_M0L6_2akeyS726);
        if (_M0L7_2anextS725) {
          moonbit_incref(_M0L7_2anextS725);
        }
      } else if (_M0L6_2acntS4060 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS724);
      }
      _M0L7_2ahashS728 = _M0L8_2afieldS3813;
      moonbit_incref(_M0L4selfS720);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS720, _M0L6_2akeyS726, _M0L8_2avalueS727, _M0L7_2ahashS728);
      _M0L8_2aparamS722 = _M0L7_2anextS725;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS731
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3825;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS730;
  int32_t _M0L8capacityS2351;
  int32_t _M0L13new__capacityS732;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2346;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2345;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3824;
  int32_t _M0L6_2atmpS2347;
  int32_t _M0L8capacityS2349;
  int32_t _M0L6_2atmpS2348;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2350;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3823;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS733;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3825 = _M0L4selfS731->$5;
  _M0L9old__headS730 = _M0L8_2afieldS3825;
  _M0L8capacityS2351 = _M0L4selfS731->$2;
  _M0L13new__capacityS732 = _M0L8capacityS2351 << 1;
  _M0L6_2atmpS2346 = 0;
  _M0L6_2atmpS2345
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS732, _M0L6_2atmpS2346);
  _M0L6_2aoldS3824 = _M0L4selfS731->$0;
  if (_M0L9old__headS730) {
    moonbit_incref(_M0L9old__headS730);
  }
  moonbit_decref(_M0L6_2aoldS3824);
  _M0L4selfS731->$0 = _M0L6_2atmpS2345;
  _M0L4selfS731->$2 = _M0L13new__capacityS732;
  _M0L6_2atmpS2347 = _M0L13new__capacityS732 - 1;
  _M0L4selfS731->$3 = _M0L6_2atmpS2347;
  _M0L8capacityS2349 = _M0L4selfS731->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2348 = _M0FPB21calc__grow__threshold(_M0L8capacityS2349);
  _M0L4selfS731->$4 = _M0L6_2atmpS2348;
  _M0L4selfS731->$1 = 0;
  _M0L6_2atmpS2350 = 0;
  _M0L6_2aoldS3823 = _M0L4selfS731->$5;
  if (_M0L6_2aoldS3823) {
    moonbit_decref(_M0L6_2aoldS3823);
  }
  _M0L4selfS731->$5 = _M0L6_2atmpS2350;
  _M0L4selfS731->$6 = -1;
  _M0L8_2aparamS733 = _M0L9old__headS730;
  while (1) {
    if (_M0L8_2aparamS733 == 0) {
      if (_M0L8_2aparamS733) {
        moonbit_decref(_M0L8_2aparamS733);
      }
      moonbit_decref(_M0L4selfS731);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS734 =
        _M0L8_2aparamS733;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS735 =
        _M0L7_2aSomeS734;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3822 =
        _M0L4_2axS735->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS736 =
        _M0L8_2afieldS3822;
      int32_t _M0L6_2akeyS737 = _M0L4_2axS735->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3821 =
        _M0L4_2axS735->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS738 =
        _M0L8_2afieldS3821;
      int32_t _M0L8_2afieldS3820 = _M0L4_2axS735->$3;
      int32_t _M0L6_2acntS4062 = Moonbit_object_header(_M0L4_2axS735)->rc;
      int32_t _M0L7_2ahashS739;
      if (_M0L6_2acntS4062 > 1) {
        int32_t _M0L11_2anew__cntS4063 = _M0L6_2acntS4062 - 1;
        Moonbit_object_header(_M0L4_2axS735)->rc = _M0L11_2anew__cntS4063;
        moonbit_incref(_M0L8_2avalueS738);
        if (_M0L7_2anextS736) {
          moonbit_incref(_M0L7_2anextS736);
        }
      } else if (_M0L6_2acntS4062 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS735);
      }
      _M0L7_2ahashS739 = _M0L8_2afieldS3820;
      moonbit_incref(_M0L4selfS731);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS731, _M0L6_2akeyS737, _M0L8_2avalueS738, _M0L7_2ahashS739);
      _M0L8_2aparamS733 = _M0L7_2anextS736;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS742
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3832;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS741;
  int32_t _M0L8capacityS2358;
  int32_t _M0L13new__capacityS743;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2353;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2352;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS3831;
  int32_t _M0L6_2atmpS2354;
  int32_t _M0L8capacityS2356;
  int32_t _M0L6_2atmpS2355;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2357;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3830;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS744;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3832 = _M0L4selfS742->$5;
  _M0L9old__headS741 = _M0L8_2afieldS3832;
  _M0L8capacityS2358 = _M0L4selfS742->$2;
  _M0L13new__capacityS743 = _M0L8capacityS2358 << 1;
  _M0L6_2atmpS2353 = 0;
  _M0L6_2atmpS2352
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS743, _M0L6_2atmpS2353);
  _M0L6_2aoldS3831 = _M0L4selfS742->$0;
  if (_M0L9old__headS741) {
    moonbit_incref(_M0L9old__headS741);
  }
  moonbit_decref(_M0L6_2aoldS3831);
  _M0L4selfS742->$0 = _M0L6_2atmpS2352;
  _M0L4selfS742->$2 = _M0L13new__capacityS743;
  _M0L6_2atmpS2354 = _M0L13new__capacityS743 - 1;
  _M0L4selfS742->$3 = _M0L6_2atmpS2354;
  _M0L8capacityS2356 = _M0L4selfS742->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2355 = _M0FPB21calc__grow__threshold(_M0L8capacityS2356);
  _M0L4selfS742->$4 = _M0L6_2atmpS2355;
  _M0L4selfS742->$1 = 0;
  _M0L6_2atmpS2357 = 0;
  _M0L6_2aoldS3830 = _M0L4selfS742->$5;
  if (_M0L6_2aoldS3830) {
    moonbit_decref(_M0L6_2aoldS3830);
  }
  _M0L4selfS742->$5 = _M0L6_2atmpS2357;
  _M0L4selfS742->$6 = -1;
  _M0L8_2aparamS744 = _M0L9old__headS741;
  while (1) {
    if (_M0L8_2aparamS744 == 0) {
      if (_M0L8_2aparamS744) {
        moonbit_decref(_M0L8_2aparamS744);
      }
      moonbit_decref(_M0L4selfS742);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS745 = _M0L8_2aparamS744;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS746 = _M0L7_2aSomeS745;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3829 = _M0L4_2axS746->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS747 = _M0L8_2afieldS3829;
      moonbit_string_t _M0L8_2afieldS3828 = _M0L4_2axS746->$4;
      moonbit_string_t _M0L6_2akeyS748 = _M0L8_2afieldS3828;
      void* _M0L8_2afieldS3827 = _M0L4_2axS746->$5;
      void* _M0L8_2avalueS749 = _M0L8_2afieldS3827;
      int32_t _M0L8_2afieldS3826 = _M0L4_2axS746->$3;
      int32_t _M0L6_2acntS4064 = Moonbit_object_header(_M0L4_2axS746)->rc;
      int32_t _M0L7_2ahashS750;
      if (_M0L6_2acntS4064 > 1) {
        int32_t _M0L11_2anew__cntS4065 = _M0L6_2acntS4064 - 1;
        Moonbit_object_header(_M0L4_2axS746)->rc = _M0L11_2anew__cntS4065;
        moonbit_incref(_M0L8_2avalueS749);
        moonbit_incref(_M0L6_2akeyS748);
        if (_M0L7_2anextS747) {
          moonbit_incref(_M0L7_2anextS747);
        }
      } else if (_M0L6_2acntS4064 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS746);
      }
      _M0L7_2ahashS750 = _M0L8_2afieldS3826;
      moonbit_incref(_M0L4selfS742);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS742, _M0L6_2akeyS748, _M0L8_2avalueS749, _M0L7_2ahashS750);
      _M0L8_2aparamS744 = _M0L7_2anextS747;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS674,
  moonbit_string_t _M0L3keyS680,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS681,
  int32_t _M0L4hashS676
) {
  int32_t _M0L14capacity__maskS2301;
  int32_t _M0L6_2atmpS2300;
  int32_t _M0L3pslS671;
  int32_t _M0L3idxS672;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2301 = _M0L4selfS674->$3;
  _M0L6_2atmpS2300 = _M0L4hashS676 & _M0L14capacity__maskS2301;
  _M0L3pslS671 = 0;
  _M0L3idxS672 = _M0L6_2atmpS2300;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3837 =
      _M0L4selfS674->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2299 =
      _M0L8_2afieldS3837;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3836;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS673;
    if (
      _M0L3idxS672 < 0
      || _M0L3idxS672 >= Moonbit_array_length(_M0L7entriesS2299)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3836
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2299[
        _M0L3idxS672
      ];
    _M0L7_2abindS673 = _M0L6_2atmpS3836;
    if (_M0L7_2abindS673 == 0) {
      int32_t _M0L4sizeS2284 = _M0L4selfS674->$1;
      int32_t _M0L8grow__atS2285 = _M0L4selfS674->$4;
      int32_t _M0L7_2abindS677;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS678;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS679;
      if (_M0L4sizeS2284 >= _M0L8grow__atS2285) {
        int32_t _M0L14capacity__maskS2287;
        int32_t _M0L6_2atmpS2286;
        moonbit_incref(_M0L4selfS674);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS674);
        _M0L14capacity__maskS2287 = _M0L4selfS674->$3;
        _M0L6_2atmpS2286 = _M0L4hashS676 & _M0L14capacity__maskS2287;
        _M0L3pslS671 = 0;
        _M0L3idxS672 = _M0L6_2atmpS2286;
        continue;
      }
      _M0L7_2abindS677 = _M0L4selfS674->$6;
      _M0L7_2abindS678 = 0;
      _M0L5entryS679
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS679)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS679->$0 = _M0L7_2abindS677;
      _M0L5entryS679->$1 = _M0L7_2abindS678;
      _M0L5entryS679->$2 = _M0L3pslS671;
      _M0L5entryS679->$3 = _M0L4hashS676;
      _M0L5entryS679->$4 = _M0L3keyS680;
      _M0L5entryS679->$5 = _M0L5valueS681;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS674, _M0L3idxS672, _M0L5entryS679);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS682 =
        _M0L7_2abindS673;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS683 =
        _M0L7_2aSomeS682;
      int32_t _M0L4hashS2289 = _M0L14_2acurr__entryS683->$3;
      int32_t _if__result_4286;
      int32_t _M0L3pslS2290;
      int32_t _M0L6_2atmpS2295;
      int32_t _M0L6_2atmpS2297;
      int32_t _M0L14capacity__maskS2298;
      int32_t _M0L6_2atmpS2296;
      if (_M0L4hashS2289 == _M0L4hashS676) {
        moonbit_string_t _M0L8_2afieldS3835 = _M0L14_2acurr__entryS683->$4;
        moonbit_string_t _M0L3keyS2288 = _M0L8_2afieldS3835;
        int32_t _M0L6_2atmpS3834;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3834
        = moonbit_val_array_equal(_M0L3keyS2288, _M0L3keyS680);
        _if__result_4286 = _M0L6_2atmpS3834;
      } else {
        _if__result_4286 = 0;
      }
      if (_if__result_4286) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3833;
        moonbit_incref(_M0L14_2acurr__entryS683);
        moonbit_decref(_M0L3keyS680);
        moonbit_decref(_M0L4selfS674);
        _M0L6_2aoldS3833 = _M0L14_2acurr__entryS683->$5;
        moonbit_decref(_M0L6_2aoldS3833);
        _M0L14_2acurr__entryS683->$5 = _M0L5valueS681;
        moonbit_decref(_M0L14_2acurr__entryS683);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS683);
      }
      _M0L3pslS2290 = _M0L14_2acurr__entryS683->$2;
      if (_M0L3pslS671 > _M0L3pslS2290) {
        int32_t _M0L4sizeS2291 = _M0L4selfS674->$1;
        int32_t _M0L8grow__atS2292 = _M0L4selfS674->$4;
        int32_t _M0L7_2abindS684;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS685;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS686;
        if (_M0L4sizeS2291 >= _M0L8grow__atS2292) {
          int32_t _M0L14capacity__maskS2294;
          int32_t _M0L6_2atmpS2293;
          moonbit_decref(_M0L14_2acurr__entryS683);
          moonbit_incref(_M0L4selfS674);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS674);
          _M0L14capacity__maskS2294 = _M0L4selfS674->$3;
          _M0L6_2atmpS2293 = _M0L4hashS676 & _M0L14capacity__maskS2294;
          _M0L3pslS671 = 0;
          _M0L3idxS672 = _M0L6_2atmpS2293;
          continue;
        }
        moonbit_incref(_M0L4selfS674);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS674, _M0L3idxS672, _M0L14_2acurr__entryS683);
        _M0L7_2abindS684 = _M0L4selfS674->$6;
        _M0L7_2abindS685 = 0;
        _M0L5entryS686
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS686)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS686->$0 = _M0L7_2abindS684;
        _M0L5entryS686->$1 = _M0L7_2abindS685;
        _M0L5entryS686->$2 = _M0L3pslS671;
        _M0L5entryS686->$3 = _M0L4hashS676;
        _M0L5entryS686->$4 = _M0L3keyS680;
        _M0L5entryS686->$5 = _M0L5valueS681;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS674, _M0L3idxS672, _M0L5entryS686);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS683);
      }
      _M0L6_2atmpS2295 = _M0L3pslS671 + 1;
      _M0L6_2atmpS2297 = _M0L3idxS672 + 1;
      _M0L14capacity__maskS2298 = _M0L4selfS674->$3;
      _M0L6_2atmpS2296 = _M0L6_2atmpS2297 & _M0L14capacity__maskS2298;
      _M0L3pslS671 = _M0L6_2atmpS2295;
      _M0L3idxS672 = _M0L6_2atmpS2296;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS690,
  int32_t _M0L3keyS696,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS697,
  int32_t _M0L4hashS692
) {
  int32_t _M0L14capacity__maskS2319;
  int32_t _M0L6_2atmpS2318;
  int32_t _M0L3pslS687;
  int32_t _M0L3idxS688;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2319 = _M0L4selfS690->$3;
  _M0L6_2atmpS2318 = _M0L4hashS692 & _M0L14capacity__maskS2319;
  _M0L3pslS687 = 0;
  _M0L3idxS688 = _M0L6_2atmpS2318;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3840 =
      _M0L4selfS690->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2317 =
      _M0L8_2afieldS3840;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3839;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS689;
    if (
      _M0L3idxS688 < 0
      || _M0L3idxS688 >= Moonbit_array_length(_M0L7entriesS2317)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3839
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2317[
        _M0L3idxS688
      ];
    _M0L7_2abindS689 = _M0L6_2atmpS3839;
    if (_M0L7_2abindS689 == 0) {
      int32_t _M0L4sizeS2302 = _M0L4selfS690->$1;
      int32_t _M0L8grow__atS2303 = _M0L4selfS690->$4;
      int32_t _M0L7_2abindS693;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS694;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS695;
      if (_M0L4sizeS2302 >= _M0L8grow__atS2303) {
        int32_t _M0L14capacity__maskS2305;
        int32_t _M0L6_2atmpS2304;
        moonbit_incref(_M0L4selfS690);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS690);
        _M0L14capacity__maskS2305 = _M0L4selfS690->$3;
        _M0L6_2atmpS2304 = _M0L4hashS692 & _M0L14capacity__maskS2305;
        _M0L3pslS687 = 0;
        _M0L3idxS688 = _M0L6_2atmpS2304;
        continue;
      }
      _M0L7_2abindS693 = _M0L4selfS690->$6;
      _M0L7_2abindS694 = 0;
      _M0L5entryS695
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS695)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS695->$0 = _M0L7_2abindS693;
      _M0L5entryS695->$1 = _M0L7_2abindS694;
      _M0L5entryS695->$2 = _M0L3pslS687;
      _M0L5entryS695->$3 = _M0L4hashS692;
      _M0L5entryS695->$4 = _M0L3keyS696;
      _M0L5entryS695->$5 = _M0L5valueS697;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS690, _M0L3idxS688, _M0L5entryS695);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS698 =
        _M0L7_2abindS689;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS699 =
        _M0L7_2aSomeS698;
      int32_t _M0L4hashS2307 = _M0L14_2acurr__entryS699->$3;
      int32_t _if__result_4288;
      int32_t _M0L3pslS2308;
      int32_t _M0L6_2atmpS2313;
      int32_t _M0L6_2atmpS2315;
      int32_t _M0L14capacity__maskS2316;
      int32_t _M0L6_2atmpS2314;
      if (_M0L4hashS2307 == _M0L4hashS692) {
        int32_t _M0L3keyS2306 = _M0L14_2acurr__entryS699->$4;
        _if__result_4288 = _M0L3keyS2306 == _M0L3keyS696;
      } else {
        _if__result_4288 = 0;
      }
      if (_if__result_4288) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3838;
        moonbit_incref(_M0L14_2acurr__entryS699);
        moonbit_decref(_M0L4selfS690);
        _M0L6_2aoldS3838 = _M0L14_2acurr__entryS699->$5;
        moonbit_decref(_M0L6_2aoldS3838);
        _M0L14_2acurr__entryS699->$5 = _M0L5valueS697;
        moonbit_decref(_M0L14_2acurr__entryS699);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS699);
      }
      _M0L3pslS2308 = _M0L14_2acurr__entryS699->$2;
      if (_M0L3pslS687 > _M0L3pslS2308) {
        int32_t _M0L4sizeS2309 = _M0L4selfS690->$1;
        int32_t _M0L8grow__atS2310 = _M0L4selfS690->$4;
        int32_t _M0L7_2abindS700;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS701;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS702;
        if (_M0L4sizeS2309 >= _M0L8grow__atS2310) {
          int32_t _M0L14capacity__maskS2312;
          int32_t _M0L6_2atmpS2311;
          moonbit_decref(_M0L14_2acurr__entryS699);
          moonbit_incref(_M0L4selfS690);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS690);
          _M0L14capacity__maskS2312 = _M0L4selfS690->$3;
          _M0L6_2atmpS2311 = _M0L4hashS692 & _M0L14capacity__maskS2312;
          _M0L3pslS687 = 0;
          _M0L3idxS688 = _M0L6_2atmpS2311;
          continue;
        }
        moonbit_incref(_M0L4selfS690);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS690, _M0L3idxS688, _M0L14_2acurr__entryS699);
        _M0L7_2abindS700 = _M0L4selfS690->$6;
        _M0L7_2abindS701 = 0;
        _M0L5entryS702
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS702)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS702->$0 = _M0L7_2abindS700;
        _M0L5entryS702->$1 = _M0L7_2abindS701;
        _M0L5entryS702->$2 = _M0L3pslS687;
        _M0L5entryS702->$3 = _M0L4hashS692;
        _M0L5entryS702->$4 = _M0L3keyS696;
        _M0L5entryS702->$5 = _M0L5valueS697;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS690, _M0L3idxS688, _M0L5entryS702);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS699);
      }
      _M0L6_2atmpS2313 = _M0L3pslS687 + 1;
      _M0L6_2atmpS2315 = _M0L3idxS688 + 1;
      _M0L14capacity__maskS2316 = _M0L4selfS690->$3;
      _M0L6_2atmpS2314 = _M0L6_2atmpS2315 & _M0L14capacity__maskS2316;
      _M0L3pslS687 = _M0L6_2atmpS2313;
      _M0L3idxS688 = _M0L6_2atmpS2314;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS706,
  moonbit_string_t _M0L3keyS712,
  void* _M0L5valueS713,
  int32_t _M0L4hashS708
) {
  int32_t _M0L14capacity__maskS2337;
  int32_t _M0L6_2atmpS2336;
  int32_t _M0L3pslS703;
  int32_t _M0L3idxS704;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2337 = _M0L4selfS706->$3;
  _M0L6_2atmpS2336 = _M0L4hashS708 & _M0L14capacity__maskS2337;
  _M0L3pslS703 = 0;
  _M0L3idxS704 = _M0L6_2atmpS2336;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3845 = _M0L4selfS706->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2335 = _M0L8_2afieldS3845;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3844;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS705;
    if (
      _M0L3idxS704 < 0
      || _M0L3idxS704 >= Moonbit_array_length(_M0L7entriesS2335)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3844
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2335[_M0L3idxS704];
    _M0L7_2abindS705 = _M0L6_2atmpS3844;
    if (_M0L7_2abindS705 == 0) {
      int32_t _M0L4sizeS2320 = _M0L4selfS706->$1;
      int32_t _M0L8grow__atS2321 = _M0L4selfS706->$4;
      int32_t _M0L7_2abindS709;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS710;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS711;
      if (_M0L4sizeS2320 >= _M0L8grow__atS2321) {
        int32_t _M0L14capacity__maskS2323;
        int32_t _M0L6_2atmpS2322;
        moonbit_incref(_M0L4selfS706);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS706);
        _M0L14capacity__maskS2323 = _M0L4selfS706->$3;
        _M0L6_2atmpS2322 = _M0L4hashS708 & _M0L14capacity__maskS2323;
        _M0L3pslS703 = 0;
        _M0L3idxS704 = _M0L6_2atmpS2322;
        continue;
      }
      _M0L7_2abindS709 = _M0L4selfS706->$6;
      _M0L7_2abindS710 = 0;
      _M0L5entryS711
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS711)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS711->$0 = _M0L7_2abindS709;
      _M0L5entryS711->$1 = _M0L7_2abindS710;
      _M0L5entryS711->$2 = _M0L3pslS703;
      _M0L5entryS711->$3 = _M0L4hashS708;
      _M0L5entryS711->$4 = _M0L3keyS712;
      _M0L5entryS711->$5 = _M0L5valueS713;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS706, _M0L3idxS704, _M0L5entryS711);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS714 = _M0L7_2abindS705;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS715 =
        _M0L7_2aSomeS714;
      int32_t _M0L4hashS2325 = _M0L14_2acurr__entryS715->$3;
      int32_t _if__result_4290;
      int32_t _M0L3pslS2326;
      int32_t _M0L6_2atmpS2331;
      int32_t _M0L6_2atmpS2333;
      int32_t _M0L14capacity__maskS2334;
      int32_t _M0L6_2atmpS2332;
      if (_M0L4hashS2325 == _M0L4hashS708) {
        moonbit_string_t _M0L8_2afieldS3843 = _M0L14_2acurr__entryS715->$4;
        moonbit_string_t _M0L3keyS2324 = _M0L8_2afieldS3843;
        int32_t _M0L6_2atmpS3842;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3842
        = moonbit_val_array_equal(_M0L3keyS2324, _M0L3keyS712);
        _if__result_4290 = _M0L6_2atmpS3842;
      } else {
        _if__result_4290 = 0;
      }
      if (_if__result_4290) {
        void* _M0L6_2aoldS3841;
        moonbit_incref(_M0L14_2acurr__entryS715);
        moonbit_decref(_M0L3keyS712);
        moonbit_decref(_M0L4selfS706);
        _M0L6_2aoldS3841 = _M0L14_2acurr__entryS715->$5;
        moonbit_decref(_M0L6_2aoldS3841);
        _M0L14_2acurr__entryS715->$5 = _M0L5valueS713;
        moonbit_decref(_M0L14_2acurr__entryS715);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS715);
      }
      _M0L3pslS2326 = _M0L14_2acurr__entryS715->$2;
      if (_M0L3pslS703 > _M0L3pslS2326) {
        int32_t _M0L4sizeS2327 = _M0L4selfS706->$1;
        int32_t _M0L8grow__atS2328 = _M0L4selfS706->$4;
        int32_t _M0L7_2abindS716;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS717;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS718;
        if (_M0L4sizeS2327 >= _M0L8grow__atS2328) {
          int32_t _M0L14capacity__maskS2330;
          int32_t _M0L6_2atmpS2329;
          moonbit_decref(_M0L14_2acurr__entryS715);
          moonbit_incref(_M0L4selfS706);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS706);
          _M0L14capacity__maskS2330 = _M0L4selfS706->$3;
          _M0L6_2atmpS2329 = _M0L4hashS708 & _M0L14capacity__maskS2330;
          _M0L3pslS703 = 0;
          _M0L3idxS704 = _M0L6_2atmpS2329;
          continue;
        }
        moonbit_incref(_M0L4selfS706);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS706, _M0L3idxS704, _M0L14_2acurr__entryS715);
        _M0L7_2abindS716 = _M0L4selfS706->$6;
        _M0L7_2abindS717 = 0;
        _M0L5entryS718
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS718)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS718->$0 = _M0L7_2abindS716;
        _M0L5entryS718->$1 = _M0L7_2abindS717;
        _M0L5entryS718->$2 = _M0L3pslS703;
        _M0L5entryS718->$3 = _M0L4hashS708;
        _M0L5entryS718->$4 = _M0L3keyS712;
        _M0L5entryS718->$5 = _M0L5valueS713;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS706, _M0L3idxS704, _M0L5entryS718);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS715);
      }
      _M0L6_2atmpS2331 = _M0L3pslS703 + 1;
      _M0L6_2atmpS2333 = _M0L3idxS704 + 1;
      _M0L14capacity__maskS2334 = _M0L4selfS706->$3;
      _M0L6_2atmpS2332 = _M0L6_2atmpS2333 & _M0L14capacity__maskS2334;
      _M0L3pslS703 = _M0L6_2atmpS2331;
      _M0L3idxS704 = _M0L6_2atmpS2332;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS645,
  int32_t _M0L3idxS650,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS649
) {
  int32_t _M0L3pslS2251;
  int32_t _M0L6_2atmpS2247;
  int32_t _M0L6_2atmpS2249;
  int32_t _M0L14capacity__maskS2250;
  int32_t _M0L6_2atmpS2248;
  int32_t _M0L3pslS641;
  int32_t _M0L3idxS642;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS643;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2251 = _M0L5entryS649->$2;
  _M0L6_2atmpS2247 = _M0L3pslS2251 + 1;
  _M0L6_2atmpS2249 = _M0L3idxS650 + 1;
  _M0L14capacity__maskS2250 = _M0L4selfS645->$3;
  _M0L6_2atmpS2248 = _M0L6_2atmpS2249 & _M0L14capacity__maskS2250;
  _M0L3pslS641 = _M0L6_2atmpS2247;
  _M0L3idxS642 = _M0L6_2atmpS2248;
  _M0L5entryS643 = _M0L5entryS649;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3847 =
      _M0L4selfS645->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2246 =
      _M0L8_2afieldS3847;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3846;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS644;
    if (
      _M0L3idxS642 < 0
      || _M0L3idxS642 >= Moonbit_array_length(_M0L7entriesS2246)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3846
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2246[
        _M0L3idxS642
      ];
    _M0L7_2abindS644 = _M0L6_2atmpS3846;
    if (_M0L7_2abindS644 == 0) {
      _M0L5entryS643->$2 = _M0L3pslS641;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS645, _M0L5entryS643, _M0L3idxS642);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS647 =
        _M0L7_2abindS644;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS648 =
        _M0L7_2aSomeS647;
      int32_t _M0L3pslS2236 = _M0L14_2acurr__entryS648->$2;
      if (_M0L3pslS641 > _M0L3pslS2236) {
        int32_t _M0L3pslS2241;
        int32_t _M0L6_2atmpS2237;
        int32_t _M0L6_2atmpS2239;
        int32_t _M0L14capacity__maskS2240;
        int32_t _M0L6_2atmpS2238;
        _M0L5entryS643->$2 = _M0L3pslS641;
        moonbit_incref(_M0L14_2acurr__entryS648);
        moonbit_incref(_M0L4selfS645);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS645, _M0L5entryS643, _M0L3idxS642);
        _M0L3pslS2241 = _M0L14_2acurr__entryS648->$2;
        _M0L6_2atmpS2237 = _M0L3pslS2241 + 1;
        _M0L6_2atmpS2239 = _M0L3idxS642 + 1;
        _M0L14capacity__maskS2240 = _M0L4selfS645->$3;
        _M0L6_2atmpS2238 = _M0L6_2atmpS2239 & _M0L14capacity__maskS2240;
        _M0L3pslS641 = _M0L6_2atmpS2237;
        _M0L3idxS642 = _M0L6_2atmpS2238;
        _M0L5entryS643 = _M0L14_2acurr__entryS648;
        continue;
      } else {
        int32_t _M0L6_2atmpS2242 = _M0L3pslS641 + 1;
        int32_t _M0L6_2atmpS2244 = _M0L3idxS642 + 1;
        int32_t _M0L14capacity__maskS2245 = _M0L4selfS645->$3;
        int32_t _M0L6_2atmpS2243 =
          _M0L6_2atmpS2244 & _M0L14capacity__maskS2245;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4292 =
          _M0L5entryS643;
        _M0L3pslS641 = _M0L6_2atmpS2242;
        _M0L3idxS642 = _M0L6_2atmpS2243;
        _M0L5entryS643 = _tmp_4292;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS655,
  int32_t _M0L3idxS660,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS659
) {
  int32_t _M0L3pslS2267;
  int32_t _M0L6_2atmpS2263;
  int32_t _M0L6_2atmpS2265;
  int32_t _M0L14capacity__maskS2266;
  int32_t _M0L6_2atmpS2264;
  int32_t _M0L3pslS651;
  int32_t _M0L3idxS652;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS653;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2267 = _M0L5entryS659->$2;
  _M0L6_2atmpS2263 = _M0L3pslS2267 + 1;
  _M0L6_2atmpS2265 = _M0L3idxS660 + 1;
  _M0L14capacity__maskS2266 = _M0L4selfS655->$3;
  _M0L6_2atmpS2264 = _M0L6_2atmpS2265 & _M0L14capacity__maskS2266;
  _M0L3pslS651 = _M0L6_2atmpS2263;
  _M0L3idxS652 = _M0L6_2atmpS2264;
  _M0L5entryS653 = _M0L5entryS659;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3849 =
      _M0L4selfS655->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2262 =
      _M0L8_2afieldS3849;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3848;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS654;
    if (
      _M0L3idxS652 < 0
      || _M0L3idxS652 >= Moonbit_array_length(_M0L7entriesS2262)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3848
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2262[
        _M0L3idxS652
      ];
    _M0L7_2abindS654 = _M0L6_2atmpS3848;
    if (_M0L7_2abindS654 == 0) {
      _M0L5entryS653->$2 = _M0L3pslS651;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS655, _M0L5entryS653, _M0L3idxS652);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS657 =
        _M0L7_2abindS654;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS658 =
        _M0L7_2aSomeS657;
      int32_t _M0L3pslS2252 = _M0L14_2acurr__entryS658->$2;
      if (_M0L3pslS651 > _M0L3pslS2252) {
        int32_t _M0L3pslS2257;
        int32_t _M0L6_2atmpS2253;
        int32_t _M0L6_2atmpS2255;
        int32_t _M0L14capacity__maskS2256;
        int32_t _M0L6_2atmpS2254;
        _M0L5entryS653->$2 = _M0L3pslS651;
        moonbit_incref(_M0L14_2acurr__entryS658);
        moonbit_incref(_M0L4selfS655);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS655, _M0L5entryS653, _M0L3idxS652);
        _M0L3pslS2257 = _M0L14_2acurr__entryS658->$2;
        _M0L6_2atmpS2253 = _M0L3pslS2257 + 1;
        _M0L6_2atmpS2255 = _M0L3idxS652 + 1;
        _M0L14capacity__maskS2256 = _M0L4selfS655->$3;
        _M0L6_2atmpS2254 = _M0L6_2atmpS2255 & _M0L14capacity__maskS2256;
        _M0L3pslS651 = _M0L6_2atmpS2253;
        _M0L3idxS652 = _M0L6_2atmpS2254;
        _M0L5entryS653 = _M0L14_2acurr__entryS658;
        continue;
      } else {
        int32_t _M0L6_2atmpS2258 = _M0L3pslS651 + 1;
        int32_t _M0L6_2atmpS2260 = _M0L3idxS652 + 1;
        int32_t _M0L14capacity__maskS2261 = _M0L4selfS655->$3;
        int32_t _M0L6_2atmpS2259 =
          _M0L6_2atmpS2260 & _M0L14capacity__maskS2261;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4294 =
          _M0L5entryS653;
        _M0L3pslS651 = _M0L6_2atmpS2258;
        _M0L3idxS652 = _M0L6_2atmpS2259;
        _M0L5entryS653 = _tmp_4294;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS665,
  int32_t _M0L3idxS670,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS669
) {
  int32_t _M0L3pslS2283;
  int32_t _M0L6_2atmpS2279;
  int32_t _M0L6_2atmpS2281;
  int32_t _M0L14capacity__maskS2282;
  int32_t _M0L6_2atmpS2280;
  int32_t _M0L3pslS661;
  int32_t _M0L3idxS662;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS663;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2283 = _M0L5entryS669->$2;
  _M0L6_2atmpS2279 = _M0L3pslS2283 + 1;
  _M0L6_2atmpS2281 = _M0L3idxS670 + 1;
  _M0L14capacity__maskS2282 = _M0L4selfS665->$3;
  _M0L6_2atmpS2280 = _M0L6_2atmpS2281 & _M0L14capacity__maskS2282;
  _M0L3pslS661 = _M0L6_2atmpS2279;
  _M0L3idxS662 = _M0L6_2atmpS2280;
  _M0L5entryS663 = _M0L5entryS669;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3851 = _M0L4selfS665->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2278 = _M0L8_2afieldS3851;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3850;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS664;
    if (
      _M0L3idxS662 < 0
      || _M0L3idxS662 >= Moonbit_array_length(_M0L7entriesS2278)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3850
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2278[_M0L3idxS662];
    _M0L7_2abindS664 = _M0L6_2atmpS3850;
    if (_M0L7_2abindS664 == 0) {
      _M0L5entryS663->$2 = _M0L3pslS661;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS665, _M0L5entryS663, _M0L3idxS662);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS667 = _M0L7_2abindS664;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS668 =
        _M0L7_2aSomeS667;
      int32_t _M0L3pslS2268 = _M0L14_2acurr__entryS668->$2;
      if (_M0L3pslS661 > _M0L3pslS2268) {
        int32_t _M0L3pslS2273;
        int32_t _M0L6_2atmpS2269;
        int32_t _M0L6_2atmpS2271;
        int32_t _M0L14capacity__maskS2272;
        int32_t _M0L6_2atmpS2270;
        _M0L5entryS663->$2 = _M0L3pslS661;
        moonbit_incref(_M0L14_2acurr__entryS668);
        moonbit_incref(_M0L4selfS665);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS665, _M0L5entryS663, _M0L3idxS662);
        _M0L3pslS2273 = _M0L14_2acurr__entryS668->$2;
        _M0L6_2atmpS2269 = _M0L3pslS2273 + 1;
        _M0L6_2atmpS2271 = _M0L3idxS662 + 1;
        _M0L14capacity__maskS2272 = _M0L4selfS665->$3;
        _M0L6_2atmpS2270 = _M0L6_2atmpS2271 & _M0L14capacity__maskS2272;
        _M0L3pslS661 = _M0L6_2atmpS2269;
        _M0L3idxS662 = _M0L6_2atmpS2270;
        _M0L5entryS663 = _M0L14_2acurr__entryS668;
        continue;
      } else {
        int32_t _M0L6_2atmpS2274 = _M0L3pslS661 + 1;
        int32_t _M0L6_2atmpS2276 = _M0L3idxS662 + 1;
        int32_t _M0L14capacity__maskS2277 = _M0L4selfS665->$3;
        int32_t _M0L6_2atmpS2275 =
          _M0L6_2atmpS2276 & _M0L14capacity__maskS2277;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_4296 = _M0L5entryS663;
        _M0L3pslS661 = _M0L6_2atmpS2274;
        _M0L3idxS662 = _M0L6_2atmpS2275;
        _M0L5entryS663 = _tmp_4296;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS623,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS625,
  int32_t _M0L8new__idxS624
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3854;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2230;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2231;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3853;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3852;
  int32_t _M0L6_2acntS4066;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS626;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3854 = _M0L4selfS623->$0;
  _M0L7entriesS2230 = _M0L8_2afieldS3854;
  moonbit_incref(_M0L5entryS625);
  _M0L6_2atmpS2231 = _M0L5entryS625;
  if (
    _M0L8new__idxS624 < 0
    || _M0L8new__idxS624 >= Moonbit_array_length(_M0L7entriesS2230)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3853
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2230[
      _M0L8new__idxS624
    ];
  if (_M0L6_2aoldS3853) {
    moonbit_decref(_M0L6_2aoldS3853);
  }
  _M0L7entriesS2230[_M0L8new__idxS624] = _M0L6_2atmpS2231;
  _M0L8_2afieldS3852 = _M0L5entryS625->$1;
  _M0L6_2acntS4066 = Moonbit_object_header(_M0L5entryS625)->rc;
  if (_M0L6_2acntS4066 > 1) {
    int32_t _M0L11_2anew__cntS4069 = _M0L6_2acntS4066 - 1;
    Moonbit_object_header(_M0L5entryS625)->rc = _M0L11_2anew__cntS4069;
    if (_M0L8_2afieldS3852) {
      moonbit_incref(_M0L8_2afieldS3852);
    }
  } else if (_M0L6_2acntS4066 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4068 =
      _M0L5entryS625->$5;
    moonbit_string_t _M0L8_2afieldS4067;
    moonbit_decref(_M0L8_2afieldS4068);
    _M0L8_2afieldS4067 = _M0L5entryS625->$4;
    moonbit_decref(_M0L8_2afieldS4067);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS625);
  }
  _M0L7_2abindS626 = _M0L8_2afieldS3852;
  if (_M0L7_2abindS626 == 0) {
    if (_M0L7_2abindS626) {
      moonbit_decref(_M0L7_2abindS626);
    }
    _M0L4selfS623->$6 = _M0L8new__idxS624;
    moonbit_decref(_M0L4selfS623);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS627;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS628;
    moonbit_decref(_M0L4selfS623);
    _M0L7_2aSomeS627 = _M0L7_2abindS626;
    _M0L7_2anextS628 = _M0L7_2aSomeS627;
    _M0L7_2anextS628->$0 = _M0L8new__idxS624;
    moonbit_decref(_M0L7_2anextS628);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS629,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS631,
  int32_t _M0L8new__idxS630
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3857;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2232;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2233;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3856;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3855;
  int32_t _M0L6_2acntS4070;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS632;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3857 = _M0L4selfS629->$0;
  _M0L7entriesS2232 = _M0L8_2afieldS3857;
  moonbit_incref(_M0L5entryS631);
  _M0L6_2atmpS2233 = _M0L5entryS631;
  if (
    _M0L8new__idxS630 < 0
    || _M0L8new__idxS630 >= Moonbit_array_length(_M0L7entriesS2232)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3856
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2232[
      _M0L8new__idxS630
    ];
  if (_M0L6_2aoldS3856) {
    moonbit_decref(_M0L6_2aoldS3856);
  }
  _M0L7entriesS2232[_M0L8new__idxS630] = _M0L6_2atmpS2233;
  _M0L8_2afieldS3855 = _M0L5entryS631->$1;
  _M0L6_2acntS4070 = Moonbit_object_header(_M0L5entryS631)->rc;
  if (_M0L6_2acntS4070 > 1) {
    int32_t _M0L11_2anew__cntS4072 = _M0L6_2acntS4070 - 1;
    Moonbit_object_header(_M0L5entryS631)->rc = _M0L11_2anew__cntS4072;
    if (_M0L8_2afieldS3855) {
      moonbit_incref(_M0L8_2afieldS3855);
    }
  } else if (_M0L6_2acntS4070 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4071 =
      _M0L5entryS631->$5;
    moonbit_decref(_M0L8_2afieldS4071);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS631);
  }
  _M0L7_2abindS632 = _M0L8_2afieldS3855;
  if (_M0L7_2abindS632 == 0) {
    if (_M0L7_2abindS632) {
      moonbit_decref(_M0L7_2abindS632);
    }
    _M0L4selfS629->$6 = _M0L8new__idxS630;
    moonbit_decref(_M0L4selfS629);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS633;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS634;
    moonbit_decref(_M0L4selfS629);
    _M0L7_2aSomeS633 = _M0L7_2abindS632;
    _M0L7_2anextS634 = _M0L7_2aSomeS633;
    _M0L7_2anextS634->$0 = _M0L8new__idxS630;
    moonbit_decref(_M0L7_2anextS634);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS635,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS637,
  int32_t _M0L8new__idxS636
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3860;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2234;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2235;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3859;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3858;
  int32_t _M0L6_2acntS4073;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS638;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3860 = _M0L4selfS635->$0;
  _M0L7entriesS2234 = _M0L8_2afieldS3860;
  moonbit_incref(_M0L5entryS637);
  _M0L6_2atmpS2235 = _M0L5entryS637;
  if (
    _M0L8new__idxS636 < 0
    || _M0L8new__idxS636 >= Moonbit_array_length(_M0L7entriesS2234)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3859
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2234[_M0L8new__idxS636];
  if (_M0L6_2aoldS3859) {
    moonbit_decref(_M0L6_2aoldS3859);
  }
  _M0L7entriesS2234[_M0L8new__idxS636] = _M0L6_2atmpS2235;
  _M0L8_2afieldS3858 = _M0L5entryS637->$1;
  _M0L6_2acntS4073 = Moonbit_object_header(_M0L5entryS637)->rc;
  if (_M0L6_2acntS4073 > 1) {
    int32_t _M0L11_2anew__cntS4076 = _M0L6_2acntS4073 - 1;
    Moonbit_object_header(_M0L5entryS637)->rc = _M0L11_2anew__cntS4076;
    if (_M0L8_2afieldS3858) {
      moonbit_incref(_M0L8_2afieldS3858);
    }
  } else if (_M0L6_2acntS4073 == 1) {
    void* _M0L8_2afieldS4075 = _M0L5entryS637->$5;
    moonbit_string_t _M0L8_2afieldS4074;
    moonbit_decref(_M0L8_2afieldS4075);
    _M0L8_2afieldS4074 = _M0L5entryS637->$4;
    moonbit_decref(_M0L8_2afieldS4074);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS637);
  }
  _M0L7_2abindS638 = _M0L8_2afieldS3858;
  if (_M0L7_2abindS638 == 0) {
    if (_M0L7_2abindS638) {
      moonbit_decref(_M0L7_2abindS638);
    }
    _M0L4selfS635->$6 = _M0L8new__idxS636;
    moonbit_decref(_M0L4selfS635);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS639;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS640;
    moonbit_decref(_M0L4selfS635);
    _M0L7_2aSomeS639 = _M0L7_2abindS638;
    _M0L7_2anextS640 = _M0L7_2aSomeS639;
    _M0L7_2anextS640->$0 = _M0L8new__idxS636;
    moonbit_decref(_M0L7_2anextS640);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS612,
  int32_t _M0L3idxS614,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS613
) {
  int32_t _M0L7_2abindS611;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3862;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2208;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2209;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3861;
  int32_t _M0L4sizeS2211;
  int32_t _M0L6_2atmpS2210;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS611 = _M0L4selfS612->$6;
  switch (_M0L7_2abindS611) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2203;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3863;
      moonbit_incref(_M0L5entryS613);
      _M0L6_2atmpS2203 = _M0L5entryS613;
      _M0L6_2aoldS3863 = _M0L4selfS612->$5;
      if (_M0L6_2aoldS3863) {
        moonbit_decref(_M0L6_2aoldS3863);
      }
      _M0L4selfS612->$5 = _M0L6_2atmpS2203;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3866 =
        _M0L4selfS612->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2207 =
        _M0L8_2afieldS3866;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3865;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2206;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2204;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2205;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3864;
      if (
        _M0L7_2abindS611 < 0
        || _M0L7_2abindS611 >= Moonbit_array_length(_M0L7entriesS2207)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3865
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2207[
          _M0L7_2abindS611
        ];
      _M0L6_2atmpS2206 = _M0L6_2atmpS3865;
      if (_M0L6_2atmpS2206) {
        moonbit_incref(_M0L6_2atmpS2206);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2204
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2206);
      moonbit_incref(_M0L5entryS613);
      _M0L6_2atmpS2205 = _M0L5entryS613;
      _M0L6_2aoldS3864 = _M0L6_2atmpS2204->$1;
      if (_M0L6_2aoldS3864) {
        moonbit_decref(_M0L6_2aoldS3864);
      }
      _M0L6_2atmpS2204->$1 = _M0L6_2atmpS2205;
      moonbit_decref(_M0L6_2atmpS2204);
      break;
    }
  }
  _M0L4selfS612->$6 = _M0L3idxS614;
  _M0L8_2afieldS3862 = _M0L4selfS612->$0;
  _M0L7entriesS2208 = _M0L8_2afieldS3862;
  _M0L6_2atmpS2209 = _M0L5entryS613;
  if (
    _M0L3idxS614 < 0
    || _M0L3idxS614 >= Moonbit_array_length(_M0L7entriesS2208)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3861
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2208[
      _M0L3idxS614
    ];
  if (_M0L6_2aoldS3861) {
    moonbit_decref(_M0L6_2aoldS3861);
  }
  _M0L7entriesS2208[_M0L3idxS614] = _M0L6_2atmpS2209;
  _M0L4sizeS2211 = _M0L4selfS612->$1;
  _M0L6_2atmpS2210 = _M0L4sizeS2211 + 1;
  _M0L4selfS612->$1 = _M0L6_2atmpS2210;
  moonbit_decref(_M0L4selfS612);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS616,
  int32_t _M0L3idxS618,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS617
) {
  int32_t _M0L7_2abindS615;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3868;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2217;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2218;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3867;
  int32_t _M0L4sizeS2220;
  int32_t _M0L6_2atmpS2219;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS615 = _M0L4selfS616->$6;
  switch (_M0L7_2abindS615) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2212;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3869;
      moonbit_incref(_M0L5entryS617);
      _M0L6_2atmpS2212 = _M0L5entryS617;
      _M0L6_2aoldS3869 = _M0L4selfS616->$5;
      if (_M0L6_2aoldS3869) {
        moonbit_decref(_M0L6_2aoldS3869);
      }
      _M0L4selfS616->$5 = _M0L6_2atmpS2212;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3872 =
        _M0L4selfS616->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2216 =
        _M0L8_2afieldS3872;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3871;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2215;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2213;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2214;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3870;
      if (
        _M0L7_2abindS615 < 0
        || _M0L7_2abindS615 >= Moonbit_array_length(_M0L7entriesS2216)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3871
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2216[
          _M0L7_2abindS615
        ];
      _M0L6_2atmpS2215 = _M0L6_2atmpS3871;
      if (_M0L6_2atmpS2215) {
        moonbit_incref(_M0L6_2atmpS2215);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2213
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2215);
      moonbit_incref(_M0L5entryS617);
      _M0L6_2atmpS2214 = _M0L5entryS617;
      _M0L6_2aoldS3870 = _M0L6_2atmpS2213->$1;
      if (_M0L6_2aoldS3870) {
        moonbit_decref(_M0L6_2aoldS3870);
      }
      _M0L6_2atmpS2213->$1 = _M0L6_2atmpS2214;
      moonbit_decref(_M0L6_2atmpS2213);
      break;
    }
  }
  _M0L4selfS616->$6 = _M0L3idxS618;
  _M0L8_2afieldS3868 = _M0L4selfS616->$0;
  _M0L7entriesS2217 = _M0L8_2afieldS3868;
  _M0L6_2atmpS2218 = _M0L5entryS617;
  if (
    _M0L3idxS618 < 0
    || _M0L3idxS618 >= Moonbit_array_length(_M0L7entriesS2217)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3867
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2217[
      _M0L3idxS618
    ];
  if (_M0L6_2aoldS3867) {
    moonbit_decref(_M0L6_2aoldS3867);
  }
  _M0L7entriesS2217[_M0L3idxS618] = _M0L6_2atmpS2218;
  _M0L4sizeS2220 = _M0L4selfS616->$1;
  _M0L6_2atmpS2219 = _M0L4sizeS2220 + 1;
  _M0L4selfS616->$1 = _M0L6_2atmpS2219;
  moonbit_decref(_M0L4selfS616);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS620,
  int32_t _M0L3idxS622,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS621
) {
  int32_t _M0L7_2abindS619;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3874;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2226;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2227;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3873;
  int32_t _M0L4sizeS2229;
  int32_t _M0L6_2atmpS2228;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS619 = _M0L4selfS620->$6;
  switch (_M0L7_2abindS619) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2221;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3875;
      moonbit_incref(_M0L5entryS621);
      _M0L6_2atmpS2221 = _M0L5entryS621;
      _M0L6_2aoldS3875 = _M0L4selfS620->$5;
      if (_M0L6_2aoldS3875) {
        moonbit_decref(_M0L6_2aoldS3875);
      }
      _M0L4selfS620->$5 = _M0L6_2atmpS2221;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3878 = _M0L4selfS620->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2225 = _M0L8_2afieldS3878;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3877;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2224;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2222;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2223;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3876;
      if (
        _M0L7_2abindS619 < 0
        || _M0L7_2abindS619 >= Moonbit_array_length(_M0L7entriesS2225)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3877
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2225[_M0L7_2abindS619];
      _M0L6_2atmpS2224 = _M0L6_2atmpS3877;
      if (_M0L6_2atmpS2224) {
        moonbit_incref(_M0L6_2atmpS2224);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2222
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2224);
      moonbit_incref(_M0L5entryS621);
      _M0L6_2atmpS2223 = _M0L5entryS621;
      _M0L6_2aoldS3876 = _M0L6_2atmpS2222->$1;
      if (_M0L6_2aoldS3876) {
        moonbit_decref(_M0L6_2aoldS3876);
      }
      _M0L6_2atmpS2222->$1 = _M0L6_2atmpS2223;
      moonbit_decref(_M0L6_2atmpS2222);
      break;
    }
  }
  _M0L4selfS620->$6 = _M0L3idxS622;
  _M0L8_2afieldS3874 = _M0L4selfS620->$0;
  _M0L7entriesS2226 = _M0L8_2afieldS3874;
  _M0L6_2atmpS2227 = _M0L5entryS621;
  if (
    _M0L3idxS622 < 0
    || _M0L3idxS622 >= Moonbit_array_length(_M0L7entriesS2226)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3873
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2226[_M0L3idxS622];
  if (_M0L6_2aoldS3873) {
    moonbit_decref(_M0L6_2aoldS3873);
  }
  _M0L7entriesS2226[_M0L3idxS622] = _M0L6_2atmpS2227;
  _M0L4sizeS2229 = _M0L4selfS620->$1;
  _M0L6_2atmpS2228 = _M0L4sizeS2229 + 1;
  _M0L4selfS620->$1 = _M0L6_2atmpS2228;
  moonbit_decref(_M0L4selfS620);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS594
) {
  int32_t _M0L8capacityS593;
  int32_t _M0L7_2abindS595;
  int32_t _M0L7_2abindS596;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2200;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS597;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS598;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4297;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS593
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS594);
  _M0L7_2abindS595 = _M0L8capacityS593 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS596 = _M0FPB21calc__grow__threshold(_M0L8capacityS593);
  _M0L6_2atmpS2200 = 0;
  _M0L7_2abindS597
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS593, _M0L6_2atmpS2200);
  _M0L7_2abindS598 = 0;
  _block_4297
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4297)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4297->$0 = _M0L7_2abindS597;
  _block_4297->$1 = 0;
  _block_4297->$2 = _M0L8capacityS593;
  _block_4297->$3 = _M0L7_2abindS595;
  _block_4297->$4 = _M0L7_2abindS596;
  _block_4297->$5 = _M0L7_2abindS598;
  _block_4297->$6 = -1;
  return _block_4297;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS600
) {
  int32_t _M0L8capacityS599;
  int32_t _M0L7_2abindS601;
  int32_t _M0L7_2abindS602;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2201;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS603;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS604;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4298;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS599
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS600);
  _M0L7_2abindS601 = _M0L8capacityS599 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS602 = _M0FPB21calc__grow__threshold(_M0L8capacityS599);
  _M0L6_2atmpS2201 = 0;
  _M0L7_2abindS603
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS599, _M0L6_2atmpS2201);
  _M0L7_2abindS604 = 0;
  _block_4298
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4298)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4298->$0 = _M0L7_2abindS603;
  _block_4298->$1 = 0;
  _block_4298->$2 = _M0L8capacityS599;
  _block_4298->$3 = _M0L7_2abindS601;
  _block_4298->$4 = _M0L7_2abindS602;
  _block_4298->$5 = _M0L7_2abindS604;
  _block_4298->$6 = -1;
  return _block_4298;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS606
) {
  int32_t _M0L8capacityS605;
  int32_t _M0L7_2abindS607;
  int32_t _M0L7_2abindS608;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2202;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS609;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS610;
  struct _M0TPB3MapGsRPB4JsonE* _block_4299;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS605
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS606);
  _M0L7_2abindS607 = _M0L8capacityS605 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS608 = _M0FPB21calc__grow__threshold(_M0L8capacityS605);
  _M0L6_2atmpS2202 = 0;
  _M0L7_2abindS609
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS605, _M0L6_2atmpS2202);
  _M0L7_2abindS610 = 0;
  _block_4299
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_4299)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_4299->$0 = _M0L7_2abindS609;
  _block_4299->$1 = 0;
  _block_4299->$2 = _M0L8capacityS605;
  _block_4299->$3 = _M0L7_2abindS607;
  _block_4299->$4 = _M0L7_2abindS608;
  _block_4299->$5 = _M0L7_2abindS610;
  _block_4299->$6 = -1;
  return _block_4299;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS592) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS592 >= 0) {
    int32_t _M0L6_2atmpS2199;
    int32_t _M0L6_2atmpS2198;
    int32_t _M0L6_2atmpS2197;
    int32_t _M0L6_2atmpS2196;
    if (_M0L4selfS592 <= 1) {
      return 1;
    }
    if (_M0L4selfS592 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2199 = _M0L4selfS592 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2198 = moonbit_clz32(_M0L6_2atmpS2199);
    _M0L6_2atmpS2197 = _M0L6_2atmpS2198 - 1;
    _M0L6_2atmpS2196 = 2147483647 >> (_M0L6_2atmpS2197 & 31);
    return _M0L6_2atmpS2196 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS591) {
  int32_t _M0L6_2atmpS2195;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2195 = _M0L8capacityS591 * 13;
  return _M0L6_2atmpS2195 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS585
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS585 == 0) {
    if (_M0L4selfS585) {
      moonbit_decref(_M0L4selfS585);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS586 =
      _M0L4selfS585;
    return _M0L7_2aSomeS586;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS587
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS587 == 0) {
    if (_M0L4selfS587) {
      moonbit_decref(_M0L4selfS587);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS588 =
      _M0L4selfS587;
    return _M0L7_2aSomeS588;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS589
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS589 == 0) {
    if (_M0L4selfS589) {
      moonbit_decref(_M0L4selfS589);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS590 = _M0L4selfS589;
    return _M0L7_2aSomeS590;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS584
) {
  moonbit_string_t* _M0L6_2atmpS2194;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2194 = _M0L4selfS584;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2194);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS580,
  int32_t _M0L5indexS581
) {
  uint64_t* _M0L6_2atmpS2192;
  uint64_t _M0L6_2atmpS3879;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2192 = _M0L4selfS580;
  if (
    _M0L5indexS581 < 0
    || _M0L5indexS581 >= Moonbit_array_length(_M0L6_2atmpS2192)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3879 = (uint64_t)_M0L6_2atmpS2192[_M0L5indexS581];
  moonbit_decref(_M0L6_2atmpS2192);
  return _M0L6_2atmpS3879;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS582,
  int32_t _M0L5indexS583
) {
  uint32_t* _M0L6_2atmpS2193;
  uint32_t _M0L6_2atmpS3880;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2193 = _M0L4selfS582;
  if (
    _M0L5indexS583 < 0
    || _M0L5indexS583 >= Moonbit_array_length(_M0L6_2atmpS2193)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3880 = (uint32_t)_M0L6_2atmpS2193[_M0L5indexS583];
  moonbit_decref(_M0L6_2atmpS2193);
  return _M0L6_2atmpS3880;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS579
) {
  moonbit_string_t* _M0L6_2atmpS2190;
  int32_t _M0L6_2atmpS3881;
  int32_t _M0L6_2atmpS2191;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2189;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS579);
  _M0L6_2atmpS2190 = _M0L4selfS579;
  _M0L6_2atmpS3881 = Moonbit_array_length(_M0L4selfS579);
  moonbit_decref(_M0L4selfS579);
  _M0L6_2atmpS2191 = _M0L6_2atmpS3881;
  _M0L6_2atmpS2189
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2191, _M0L6_2atmpS2190
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2189);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS577
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS576;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__* _closure_4300;
  struct _M0TWEOs* _M0L6_2atmpS2177;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS576
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS576)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS576->$0 = 0;
  _closure_4300
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__));
  Moonbit_object_header(_closure_4300)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__, $0_0) >> 2, 2, 0);
  _closure_4300->code = &_M0MPC15array9ArrayView4iterGsEC2178l570;
  _closure_4300->$0_0 = _M0L4selfS577.$0;
  _closure_4300->$0_1 = _M0L4selfS577.$1;
  _closure_4300->$0_2 = _M0L4selfS577.$2;
  _closure_4300->$1 = _M0L1iS576;
  _M0L6_2atmpS2177 = (struct _M0TWEOs*)_closure_4300;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2177);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2178l570(
  struct _M0TWEOs* _M0L6_2aenvS2179
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__* _M0L14_2acasted__envS2180;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3886;
  struct _M0TPC13ref3RefGiE* _M0L1iS576;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3885;
  int32_t _M0L6_2acntS4077;
  struct _M0TPB9ArrayViewGsE _M0L4selfS577;
  int32_t _M0L3valS2181;
  int32_t _M0L6_2atmpS2182;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2180
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2178__l570__*)_M0L6_2aenvS2179;
  _M0L8_2afieldS3886 = _M0L14_2acasted__envS2180->$1;
  _M0L1iS576 = _M0L8_2afieldS3886;
  _M0L8_2afieldS3885
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2180->$0_1,
      _M0L14_2acasted__envS2180->$0_2,
      _M0L14_2acasted__envS2180->$0_0
  };
  _M0L6_2acntS4077 = Moonbit_object_header(_M0L14_2acasted__envS2180)->rc;
  if (_M0L6_2acntS4077 > 1) {
    int32_t _M0L11_2anew__cntS4078 = _M0L6_2acntS4077 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2180)->rc
    = _M0L11_2anew__cntS4078;
    moonbit_incref(_M0L1iS576);
    moonbit_incref(_M0L8_2afieldS3885.$0);
  } else if (_M0L6_2acntS4077 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2180);
  }
  _M0L4selfS577 = _M0L8_2afieldS3885;
  _M0L3valS2181 = _M0L1iS576->$0;
  moonbit_incref(_M0L4selfS577.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2182 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS577);
  if (_M0L3valS2181 < _M0L6_2atmpS2182) {
    moonbit_string_t* _M0L8_2afieldS3884 = _M0L4selfS577.$0;
    moonbit_string_t* _M0L3bufS2185 = _M0L8_2afieldS3884;
    int32_t _M0L8_2afieldS3883 = _M0L4selfS577.$1;
    int32_t _M0L5startS2187 = _M0L8_2afieldS3883;
    int32_t _M0L3valS2188 = _M0L1iS576->$0;
    int32_t _M0L6_2atmpS2186 = _M0L5startS2187 + _M0L3valS2188;
    moonbit_string_t _M0L6_2atmpS3882 =
      (moonbit_string_t)_M0L3bufS2185[_M0L6_2atmpS2186];
    moonbit_string_t _M0L4elemS578;
    int32_t _M0L3valS2184;
    int32_t _M0L6_2atmpS2183;
    moonbit_incref(_M0L6_2atmpS3882);
    moonbit_decref(_M0L3bufS2185);
    _M0L4elemS578 = _M0L6_2atmpS3882;
    _M0L3valS2184 = _M0L1iS576->$0;
    _M0L6_2atmpS2183 = _M0L3valS2184 + 1;
    _M0L1iS576->$0 = _M0L6_2atmpS2183;
    moonbit_decref(_M0L1iS576);
    return _M0L4elemS578;
  } else {
    moonbit_decref(_M0L4selfS577.$0);
    moonbit_decref(_M0L1iS576);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS575
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS575;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS574,
  struct _M0TPB6Logger _M0L6loggerS573
) {
  moonbit_string_t _M0L6_2atmpS2176;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2176
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS574, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS573.$0->$method_0(_M0L6loggerS573.$1, _M0L6_2atmpS2176);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS572,
  struct _M0TPB6Logger _M0L6loggerS571
) {
  moonbit_string_t _M0L6_2atmpS2175;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2175 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS572, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS571.$0->$method_0(_M0L6loggerS571.$1, _M0L6_2atmpS2175);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS566) {
  int32_t _M0L3lenS565;
  struct _M0TPC13ref3RefGiE* _M0L5indexS567;
  struct _M0R38String_3a_3aiter_2eanon__u2159__l247__* _closure_4301;
  struct _M0TWEOc* _M0L6_2atmpS2158;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS565 = Moonbit_array_length(_M0L4selfS566);
  _M0L5indexS567
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS567)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS567->$0 = 0;
  _closure_4301
  = (struct _M0R38String_3a_3aiter_2eanon__u2159__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2159__l247__));
  Moonbit_object_header(_closure_4301)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2159__l247__, $0) >> 2, 2, 0);
  _closure_4301->code = &_M0MPC16string6String4iterC2159l247;
  _closure_4301->$0 = _M0L5indexS567;
  _closure_4301->$1 = _M0L4selfS566;
  _closure_4301->$2 = _M0L3lenS565;
  _M0L6_2atmpS2158 = (struct _M0TWEOc*)_closure_4301;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2158);
}

int32_t _M0MPC16string6String4iterC2159l247(
  struct _M0TWEOc* _M0L6_2aenvS2160
) {
  struct _M0R38String_3a_3aiter_2eanon__u2159__l247__* _M0L14_2acasted__envS2161;
  int32_t _M0L3lenS565;
  moonbit_string_t _M0L8_2afieldS3889;
  moonbit_string_t _M0L4selfS566;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3888;
  int32_t _M0L6_2acntS4079;
  struct _M0TPC13ref3RefGiE* _M0L5indexS567;
  int32_t _M0L3valS2162;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2161
  = (struct _M0R38String_3a_3aiter_2eanon__u2159__l247__*)_M0L6_2aenvS2160;
  _M0L3lenS565 = _M0L14_2acasted__envS2161->$2;
  _M0L8_2afieldS3889 = _M0L14_2acasted__envS2161->$1;
  _M0L4selfS566 = _M0L8_2afieldS3889;
  _M0L8_2afieldS3888 = _M0L14_2acasted__envS2161->$0;
  _M0L6_2acntS4079 = Moonbit_object_header(_M0L14_2acasted__envS2161)->rc;
  if (_M0L6_2acntS4079 > 1) {
    int32_t _M0L11_2anew__cntS4080 = _M0L6_2acntS4079 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2161)->rc
    = _M0L11_2anew__cntS4080;
    moonbit_incref(_M0L4selfS566);
    moonbit_incref(_M0L8_2afieldS3888);
  } else if (_M0L6_2acntS4079 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2161);
  }
  _M0L5indexS567 = _M0L8_2afieldS3888;
  _M0L3valS2162 = _M0L5indexS567->$0;
  if (_M0L3valS2162 < _M0L3lenS565) {
    int32_t _M0L3valS2174 = _M0L5indexS567->$0;
    int32_t _M0L2c1S568 = _M0L4selfS566[_M0L3valS2174];
    int32_t _if__result_4302;
    int32_t _M0L3valS2172;
    int32_t _M0L6_2atmpS2171;
    int32_t _M0L6_2atmpS2173;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S568)) {
      int32_t _M0L3valS2164 = _M0L5indexS567->$0;
      int32_t _M0L6_2atmpS2163 = _M0L3valS2164 + 1;
      _if__result_4302 = _M0L6_2atmpS2163 < _M0L3lenS565;
    } else {
      _if__result_4302 = 0;
    }
    if (_if__result_4302) {
      int32_t _M0L3valS2170 = _M0L5indexS567->$0;
      int32_t _M0L6_2atmpS2169 = _M0L3valS2170 + 1;
      int32_t _M0L6_2atmpS3887 = _M0L4selfS566[_M0L6_2atmpS2169];
      int32_t _M0L2c2S569;
      moonbit_decref(_M0L4selfS566);
      _M0L2c2S569 = _M0L6_2atmpS3887;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S569)) {
        int32_t _M0L6_2atmpS2167 = (int32_t)_M0L2c1S568;
        int32_t _M0L6_2atmpS2168 = (int32_t)_M0L2c2S569;
        int32_t _M0L1cS570;
        int32_t _M0L3valS2166;
        int32_t _M0L6_2atmpS2165;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS570
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2167, _M0L6_2atmpS2168);
        _M0L3valS2166 = _M0L5indexS567->$0;
        _M0L6_2atmpS2165 = _M0L3valS2166 + 2;
        _M0L5indexS567->$0 = _M0L6_2atmpS2165;
        moonbit_decref(_M0L5indexS567);
        return _M0L1cS570;
      }
    } else {
      moonbit_decref(_M0L4selfS566);
    }
    _M0L3valS2172 = _M0L5indexS567->$0;
    _M0L6_2atmpS2171 = _M0L3valS2172 + 1;
    _M0L5indexS567->$0 = _M0L6_2atmpS2171;
    moonbit_decref(_M0L5indexS567);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2173 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S568);
    return _M0L6_2atmpS2173;
  } else {
    moonbit_decref(_M0L5indexS567);
    moonbit_decref(_M0L4selfS566);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS556,
  moonbit_string_t _M0L5valueS558
) {
  int32_t _M0L3lenS2143;
  moonbit_string_t* _M0L6_2atmpS2145;
  int32_t _M0L6_2atmpS3892;
  int32_t _M0L6_2atmpS2144;
  int32_t _M0L6lengthS557;
  moonbit_string_t* _M0L8_2afieldS3891;
  moonbit_string_t* _M0L3bufS2146;
  moonbit_string_t _M0L6_2aoldS3890;
  int32_t _M0L6_2atmpS2147;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2143 = _M0L4selfS556->$1;
  moonbit_incref(_M0L4selfS556);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2145 = _M0MPC15array5Array6bufferGsE(_M0L4selfS556);
  _M0L6_2atmpS3892 = Moonbit_array_length(_M0L6_2atmpS2145);
  moonbit_decref(_M0L6_2atmpS2145);
  _M0L6_2atmpS2144 = _M0L6_2atmpS3892;
  if (_M0L3lenS2143 == _M0L6_2atmpS2144) {
    moonbit_incref(_M0L4selfS556);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS556);
  }
  _M0L6lengthS557 = _M0L4selfS556->$1;
  _M0L8_2afieldS3891 = _M0L4selfS556->$0;
  _M0L3bufS2146 = _M0L8_2afieldS3891;
  _M0L6_2aoldS3890 = (moonbit_string_t)_M0L3bufS2146[_M0L6lengthS557];
  moonbit_decref(_M0L6_2aoldS3890);
  _M0L3bufS2146[_M0L6lengthS557] = _M0L5valueS558;
  _M0L6_2atmpS2147 = _M0L6lengthS557 + 1;
  _M0L4selfS556->$1 = _M0L6_2atmpS2147;
  moonbit_decref(_M0L4selfS556);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS559,
  struct _M0TUsiE* _M0L5valueS561
) {
  int32_t _M0L3lenS2148;
  struct _M0TUsiE** _M0L6_2atmpS2150;
  int32_t _M0L6_2atmpS3895;
  int32_t _M0L6_2atmpS2149;
  int32_t _M0L6lengthS560;
  struct _M0TUsiE** _M0L8_2afieldS3894;
  struct _M0TUsiE** _M0L3bufS2151;
  struct _M0TUsiE* _M0L6_2aoldS3893;
  int32_t _M0L6_2atmpS2152;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2148 = _M0L4selfS559->$1;
  moonbit_incref(_M0L4selfS559);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2150 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS559);
  _M0L6_2atmpS3895 = Moonbit_array_length(_M0L6_2atmpS2150);
  moonbit_decref(_M0L6_2atmpS2150);
  _M0L6_2atmpS2149 = _M0L6_2atmpS3895;
  if (_M0L3lenS2148 == _M0L6_2atmpS2149) {
    moonbit_incref(_M0L4selfS559);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS559);
  }
  _M0L6lengthS560 = _M0L4selfS559->$1;
  _M0L8_2afieldS3894 = _M0L4selfS559->$0;
  _M0L3bufS2151 = _M0L8_2afieldS3894;
  _M0L6_2aoldS3893 = (struct _M0TUsiE*)_M0L3bufS2151[_M0L6lengthS560];
  if (_M0L6_2aoldS3893) {
    moonbit_decref(_M0L6_2aoldS3893);
  }
  _M0L3bufS2151[_M0L6lengthS560] = _M0L5valueS561;
  _M0L6_2atmpS2152 = _M0L6lengthS560 + 1;
  _M0L4selfS559->$1 = _M0L6_2atmpS2152;
  moonbit_decref(_M0L4selfS559);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS562,
  void* _M0L5valueS564
) {
  int32_t _M0L3lenS2153;
  void** _M0L6_2atmpS2155;
  int32_t _M0L6_2atmpS3898;
  int32_t _M0L6_2atmpS2154;
  int32_t _M0L6lengthS563;
  void** _M0L8_2afieldS3897;
  void** _M0L3bufS2156;
  void* _M0L6_2aoldS3896;
  int32_t _M0L6_2atmpS2157;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2153 = _M0L4selfS562->$1;
  moonbit_incref(_M0L4selfS562);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2155
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS562);
  _M0L6_2atmpS3898 = Moonbit_array_length(_M0L6_2atmpS2155);
  moonbit_decref(_M0L6_2atmpS2155);
  _M0L6_2atmpS2154 = _M0L6_2atmpS3898;
  if (_M0L3lenS2153 == _M0L6_2atmpS2154) {
    moonbit_incref(_M0L4selfS562);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS562);
  }
  _M0L6lengthS563 = _M0L4selfS562->$1;
  _M0L8_2afieldS3897 = _M0L4selfS562->$0;
  _M0L3bufS2156 = _M0L8_2afieldS3897;
  _M0L6_2aoldS3896 = (void*)_M0L3bufS2156[_M0L6lengthS563];
  moonbit_decref(_M0L6_2aoldS3896);
  _M0L3bufS2156[_M0L6lengthS563] = _M0L5valueS564;
  _M0L6_2atmpS2157 = _M0L6lengthS563 + 1;
  _M0L4selfS562->$1 = _M0L6_2atmpS2157;
  moonbit_decref(_M0L4selfS562);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS548) {
  int32_t _M0L8old__capS547;
  int32_t _M0L8new__capS549;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS547 = _M0L4selfS548->$1;
  if (_M0L8old__capS547 == 0) {
    _M0L8new__capS549 = 8;
  } else {
    _M0L8new__capS549 = _M0L8old__capS547 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS548, _M0L8new__capS549);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS551
) {
  int32_t _M0L8old__capS550;
  int32_t _M0L8new__capS552;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS550 = _M0L4selfS551->$1;
  if (_M0L8old__capS550 == 0) {
    _M0L8new__capS552 = 8;
  } else {
    _M0L8new__capS552 = _M0L8old__capS550 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS551, _M0L8new__capS552);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS554
) {
  int32_t _M0L8old__capS553;
  int32_t _M0L8new__capS555;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS553 = _M0L4selfS554->$1;
  if (_M0L8old__capS553 == 0) {
    _M0L8new__capS555 = 8;
  } else {
    _M0L8new__capS555 = _M0L8old__capS553 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS554, _M0L8new__capS555);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS532,
  int32_t _M0L13new__capacityS530
) {
  moonbit_string_t* _M0L8new__bufS529;
  moonbit_string_t* _M0L8_2afieldS3900;
  moonbit_string_t* _M0L8old__bufS531;
  int32_t _M0L8old__capS533;
  int32_t _M0L9copy__lenS534;
  moonbit_string_t* _M0L6_2aoldS3899;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS529
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS530, (moonbit_string_t)moonbit_string_literal_3.data);
  _M0L8_2afieldS3900 = _M0L4selfS532->$0;
  _M0L8old__bufS531 = _M0L8_2afieldS3900;
  _M0L8old__capS533 = Moonbit_array_length(_M0L8old__bufS531);
  if (_M0L8old__capS533 < _M0L13new__capacityS530) {
    _M0L9copy__lenS534 = _M0L8old__capS533;
  } else {
    _M0L9copy__lenS534 = _M0L13new__capacityS530;
  }
  moonbit_incref(_M0L8old__bufS531);
  moonbit_incref(_M0L8new__bufS529);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS529, 0, _M0L8old__bufS531, 0, _M0L9copy__lenS534);
  _M0L6_2aoldS3899 = _M0L4selfS532->$0;
  moonbit_decref(_M0L6_2aoldS3899);
  _M0L4selfS532->$0 = _M0L8new__bufS529;
  moonbit_decref(_M0L4selfS532);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS538,
  int32_t _M0L13new__capacityS536
) {
  struct _M0TUsiE** _M0L8new__bufS535;
  struct _M0TUsiE** _M0L8_2afieldS3902;
  struct _M0TUsiE** _M0L8old__bufS537;
  int32_t _M0L8old__capS539;
  int32_t _M0L9copy__lenS540;
  struct _M0TUsiE** _M0L6_2aoldS3901;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS535
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS536, 0);
  _M0L8_2afieldS3902 = _M0L4selfS538->$0;
  _M0L8old__bufS537 = _M0L8_2afieldS3902;
  _M0L8old__capS539 = Moonbit_array_length(_M0L8old__bufS537);
  if (_M0L8old__capS539 < _M0L13new__capacityS536) {
    _M0L9copy__lenS540 = _M0L8old__capS539;
  } else {
    _M0L9copy__lenS540 = _M0L13new__capacityS536;
  }
  moonbit_incref(_M0L8old__bufS537);
  moonbit_incref(_M0L8new__bufS535);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS535, 0, _M0L8old__bufS537, 0, _M0L9copy__lenS540);
  _M0L6_2aoldS3901 = _M0L4selfS538->$0;
  moonbit_decref(_M0L6_2aoldS3901);
  _M0L4selfS538->$0 = _M0L8new__bufS535;
  moonbit_decref(_M0L4selfS538);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS544,
  int32_t _M0L13new__capacityS542
) {
  void** _M0L8new__bufS541;
  void** _M0L8_2afieldS3904;
  void** _M0L8old__bufS543;
  int32_t _M0L8old__capS545;
  int32_t _M0L9copy__lenS546;
  void** _M0L6_2aoldS3903;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS541
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS542, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3904 = _M0L4selfS544->$0;
  _M0L8old__bufS543 = _M0L8_2afieldS3904;
  _M0L8old__capS545 = Moonbit_array_length(_M0L8old__bufS543);
  if (_M0L8old__capS545 < _M0L13new__capacityS542) {
    _M0L9copy__lenS546 = _M0L8old__capS545;
  } else {
    _M0L9copy__lenS546 = _M0L13new__capacityS542;
  }
  moonbit_incref(_M0L8old__bufS543);
  moonbit_incref(_M0L8new__bufS541);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS541, 0, _M0L8old__bufS543, 0, _M0L9copy__lenS546);
  _M0L6_2aoldS3903 = _M0L4selfS544->$0;
  moonbit_decref(_M0L6_2aoldS3903);
  _M0L4selfS544->$0 = _M0L8new__bufS541;
  moonbit_decref(_M0L4selfS544);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS528
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS528 == 0) {
    moonbit_string_t* _M0L6_2atmpS2141 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4303 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4303)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4303->$0 = _M0L6_2atmpS2141;
    _block_4303->$1 = 0;
    return _block_4303;
  } else {
    moonbit_string_t* _M0L6_2atmpS2142 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS528, (moonbit_string_t)moonbit_string_literal_3.data);
    struct _M0TPB5ArrayGsE* _block_4304 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4304)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4304->$0 = _M0L6_2atmpS2142;
    _block_4304->$1 = 0;
    return _block_4304;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS522,
  int32_t _M0L1nS521
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS521 <= 0) {
    moonbit_decref(_M0L4selfS522);
    return (moonbit_string_t)moonbit_string_literal_3.data;
  } else if (_M0L1nS521 == 1) {
    return _M0L4selfS522;
  } else {
    int32_t _M0L3lenS523 = Moonbit_array_length(_M0L4selfS522);
    int32_t _M0L6_2atmpS2140 = _M0L3lenS523 * _M0L1nS521;
    struct _M0TPB13StringBuilder* _M0L3bufS524;
    moonbit_string_t _M0L3strS525;
    int32_t _M0L2__S526;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS524 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2140);
    _M0L3strS525 = _M0L4selfS522;
    _M0L2__S526 = 0;
    while (1) {
      if (_M0L2__S526 < _M0L1nS521) {
        int32_t _M0L6_2atmpS2139;
        moonbit_incref(_M0L3strS525);
        moonbit_incref(_M0L3bufS524);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS524, _M0L3strS525);
        _M0L6_2atmpS2139 = _M0L2__S526 + 1;
        _M0L2__S526 = _M0L6_2atmpS2139;
        continue;
      } else {
        moonbit_decref(_M0L3strS525);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS524);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS519,
  struct _M0TPC16string10StringView _M0L3strS520
) {
  int32_t _M0L3lenS2127;
  int32_t _M0L6_2atmpS2129;
  int32_t _M0L6_2atmpS2128;
  int32_t _M0L6_2atmpS2126;
  moonbit_bytes_t _M0L8_2afieldS3905;
  moonbit_bytes_t _M0L4dataS2130;
  int32_t _M0L3lenS2131;
  moonbit_string_t _M0L6_2atmpS2132;
  int32_t _M0L6_2atmpS2133;
  int32_t _M0L6_2atmpS2134;
  int32_t _M0L3lenS2136;
  int32_t _M0L6_2atmpS2138;
  int32_t _M0L6_2atmpS2137;
  int32_t _M0L6_2atmpS2135;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2127 = _M0L4selfS519->$1;
  moonbit_incref(_M0L3strS520.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2129 = _M0MPC16string10StringView6length(_M0L3strS520);
  _M0L6_2atmpS2128 = _M0L6_2atmpS2129 * 2;
  _M0L6_2atmpS2126 = _M0L3lenS2127 + _M0L6_2atmpS2128;
  moonbit_incref(_M0L4selfS519);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS519, _M0L6_2atmpS2126);
  _M0L8_2afieldS3905 = _M0L4selfS519->$0;
  _M0L4dataS2130 = _M0L8_2afieldS3905;
  _M0L3lenS2131 = _M0L4selfS519->$1;
  moonbit_incref(_M0L4dataS2130);
  moonbit_incref(_M0L3strS520.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2132 = _M0MPC16string10StringView4data(_M0L3strS520);
  moonbit_incref(_M0L3strS520.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2133 = _M0MPC16string10StringView13start__offset(_M0L3strS520);
  moonbit_incref(_M0L3strS520.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2134 = _M0MPC16string10StringView6length(_M0L3strS520);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2130, _M0L3lenS2131, _M0L6_2atmpS2132, _M0L6_2atmpS2133, _M0L6_2atmpS2134);
  _M0L3lenS2136 = _M0L4selfS519->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2138 = _M0MPC16string10StringView6length(_M0L3strS520);
  _M0L6_2atmpS2137 = _M0L6_2atmpS2138 * 2;
  _M0L6_2atmpS2135 = _M0L3lenS2136 + _M0L6_2atmpS2137;
  _M0L4selfS519->$1 = _M0L6_2atmpS2135;
  moonbit_decref(_M0L4selfS519);
  return 0;
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS516,
  int32_t _M0L1iS517,
  int32_t _M0L13start__offsetS518,
  int64_t _M0L11end__offsetS514
) {
  int32_t _M0L11end__offsetS513;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS514 == 4294967296ll) {
    _M0L11end__offsetS513 = Moonbit_array_length(_M0L4selfS516);
  } else {
    int64_t _M0L7_2aSomeS515 = _M0L11end__offsetS514;
    _M0L11end__offsetS513 = (int32_t)_M0L7_2aSomeS515;
  }
  if (_M0L1iS517 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS516, _M0L1iS517, _M0L13start__offsetS518, _M0L11end__offsetS513);
  } else {
    int32_t _M0L6_2atmpS2125 = -_M0L1iS517;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS516, _M0L6_2atmpS2125, _M0L13start__offsetS518, _M0L11end__offsetS513);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS511,
  int32_t _M0L1nS509,
  int32_t _M0L13start__offsetS505,
  int32_t _M0L11end__offsetS506
) {
  int32_t _if__result_4306;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS505 >= 0) {
    _if__result_4306 = _M0L13start__offsetS505 <= _M0L11end__offsetS506;
  } else {
    _if__result_4306 = 0;
  }
  if (_if__result_4306) {
    int32_t _M0Lm13utf16__offsetS507 = _M0L13start__offsetS505;
    int32_t _M0Lm11char__countS508 = 0;
    int32_t _M0L6_2atmpS2123;
    int32_t _if__result_4309;
    while (1) {
      int32_t _M0L6_2atmpS2117 = _M0Lm13utf16__offsetS507;
      int32_t _if__result_4308;
      if (_M0L6_2atmpS2117 < _M0L11end__offsetS506) {
        int32_t _M0L6_2atmpS2116 = _M0Lm11char__countS508;
        _if__result_4308 = _M0L6_2atmpS2116 < _M0L1nS509;
      } else {
        _if__result_4308 = 0;
      }
      if (_if__result_4308) {
        int32_t _M0L6_2atmpS2121 = _M0Lm13utf16__offsetS507;
        int32_t _M0L1cS510 = _M0L4selfS511[_M0L6_2atmpS2121];
        int32_t _M0L6_2atmpS2120;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS510)) {
          int32_t _M0L6_2atmpS2118 = _M0Lm13utf16__offsetS507;
          _M0Lm13utf16__offsetS507 = _M0L6_2atmpS2118 + 2;
        } else {
          int32_t _M0L6_2atmpS2119 = _M0Lm13utf16__offsetS507;
          _M0Lm13utf16__offsetS507 = _M0L6_2atmpS2119 + 1;
        }
        _M0L6_2atmpS2120 = _M0Lm11char__countS508;
        _M0Lm11char__countS508 = _M0L6_2atmpS2120 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS511);
      }
      break;
    }
    _M0L6_2atmpS2123 = _M0Lm11char__countS508;
    if (_M0L6_2atmpS2123 < _M0L1nS509) {
      _if__result_4309 = 1;
    } else {
      int32_t _M0L6_2atmpS2122 = _M0Lm13utf16__offsetS507;
      _if__result_4309 = _M0L6_2atmpS2122 >= _M0L11end__offsetS506;
    }
    if (_if__result_4309) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2124 = _M0Lm13utf16__offsetS507;
      return (int64_t)_M0L6_2atmpS2124;
    }
  } else {
    moonbit_decref(_M0L4selfS511);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_92.data, (moonbit_string_t)moonbit_string_literal_93.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS503,
  int32_t _M0L1nS501,
  int32_t _M0L13start__offsetS500,
  int32_t _M0L11end__offsetS499
) {
  int32_t _M0Lm11char__countS497;
  int32_t _M0Lm13utf16__offsetS498;
  int32_t _M0L6_2atmpS2114;
  int32_t _if__result_4312;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS497 = 0;
  _M0Lm13utf16__offsetS498 = _M0L11end__offsetS499;
  while (1) {
    int32_t _M0L6_2atmpS2107 = _M0Lm13utf16__offsetS498;
    int32_t _M0L6_2atmpS2106 = _M0L6_2atmpS2107 - 1;
    int32_t _if__result_4311;
    if (_M0L6_2atmpS2106 >= _M0L13start__offsetS500) {
      int32_t _M0L6_2atmpS2105 = _M0Lm11char__countS497;
      _if__result_4311 = _M0L6_2atmpS2105 < _M0L1nS501;
    } else {
      _if__result_4311 = 0;
    }
    if (_if__result_4311) {
      int32_t _M0L6_2atmpS2112 = _M0Lm13utf16__offsetS498;
      int32_t _M0L6_2atmpS2111 = _M0L6_2atmpS2112 - 1;
      int32_t _M0L1cS502 = _M0L4selfS503[_M0L6_2atmpS2111];
      int32_t _M0L6_2atmpS2110;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS502)) {
        int32_t _M0L6_2atmpS2108 = _M0Lm13utf16__offsetS498;
        _M0Lm13utf16__offsetS498 = _M0L6_2atmpS2108 - 2;
      } else {
        int32_t _M0L6_2atmpS2109 = _M0Lm13utf16__offsetS498;
        _M0Lm13utf16__offsetS498 = _M0L6_2atmpS2109 - 1;
      }
      _M0L6_2atmpS2110 = _M0Lm11char__countS497;
      _M0Lm11char__countS497 = _M0L6_2atmpS2110 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS503);
    }
    break;
  }
  _M0L6_2atmpS2114 = _M0Lm11char__countS497;
  if (_M0L6_2atmpS2114 < _M0L1nS501) {
    _if__result_4312 = 1;
  } else {
    int32_t _M0L6_2atmpS2113 = _M0Lm13utf16__offsetS498;
    _if__result_4312 = _M0L6_2atmpS2113 < _M0L13start__offsetS500;
  }
  if (_if__result_4312) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2115 = _M0Lm13utf16__offsetS498;
    return (int64_t)_M0L6_2atmpS2115;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS489,
  int32_t _M0L3lenS492,
  int32_t _M0L13start__offsetS496,
  int64_t _M0L11end__offsetS487
) {
  int32_t _M0L11end__offsetS486;
  int32_t _M0L5indexS490;
  int32_t _M0L5countS491;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS487 == 4294967296ll) {
    _M0L11end__offsetS486 = Moonbit_array_length(_M0L4selfS489);
  } else {
    int64_t _M0L7_2aSomeS488 = _M0L11end__offsetS487;
    _M0L11end__offsetS486 = (int32_t)_M0L7_2aSomeS488;
  }
  _M0L5indexS490 = _M0L13start__offsetS496;
  _M0L5countS491 = 0;
  while (1) {
    int32_t _if__result_4314;
    if (_M0L5indexS490 < _M0L11end__offsetS486) {
      _if__result_4314 = _M0L5countS491 < _M0L3lenS492;
    } else {
      _if__result_4314 = 0;
    }
    if (_if__result_4314) {
      int32_t _M0L2c1S493 = _M0L4selfS489[_M0L5indexS490];
      int32_t _if__result_4315;
      int32_t _M0L6_2atmpS2103;
      int32_t _M0L6_2atmpS2104;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S493)) {
        int32_t _M0L6_2atmpS2099 = _M0L5indexS490 + 1;
        _if__result_4315 = _M0L6_2atmpS2099 < _M0L11end__offsetS486;
      } else {
        _if__result_4315 = 0;
      }
      if (_if__result_4315) {
        int32_t _M0L6_2atmpS2102 = _M0L5indexS490 + 1;
        int32_t _M0L2c2S494 = _M0L4selfS489[_M0L6_2atmpS2102];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S494)) {
          int32_t _M0L6_2atmpS2100 = _M0L5indexS490 + 2;
          int32_t _M0L6_2atmpS2101 = _M0L5countS491 + 1;
          _M0L5indexS490 = _M0L6_2atmpS2100;
          _M0L5countS491 = _M0L6_2atmpS2101;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_94.data, (moonbit_string_t)moonbit_string_literal_95.data);
        }
      }
      _M0L6_2atmpS2103 = _M0L5indexS490 + 1;
      _M0L6_2atmpS2104 = _M0L5countS491 + 1;
      _M0L5indexS490 = _M0L6_2atmpS2103;
      _M0L5countS491 = _M0L6_2atmpS2104;
      continue;
    } else {
      moonbit_decref(_M0L4selfS489);
      return _M0L5countS491 >= _M0L3lenS492;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS478,
  int32_t _M0L3lenS481,
  int32_t _M0L13start__offsetS485,
  int64_t _M0L11end__offsetS476
) {
  int32_t _M0L11end__offsetS475;
  int32_t _M0L5indexS479;
  int32_t _M0L5countS480;
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS476 == 4294967296ll) {
    _M0L11end__offsetS475 = Moonbit_array_length(_M0L4selfS478);
  } else {
    int64_t _M0L7_2aSomeS477 = _M0L11end__offsetS476;
    _M0L11end__offsetS475 = (int32_t)_M0L7_2aSomeS477;
  }
  _M0L5indexS479 = _M0L13start__offsetS485;
  _M0L5countS480 = 0;
  while (1) {
    int32_t _if__result_4317;
    if (_M0L5indexS479 < _M0L11end__offsetS475) {
      _if__result_4317 = _M0L5countS480 < _M0L3lenS481;
    } else {
      _if__result_4317 = 0;
    }
    if (_if__result_4317) {
      int32_t _M0L2c1S482 = _M0L4selfS478[_M0L5indexS479];
      int32_t _if__result_4318;
      int32_t _M0L6_2atmpS2097;
      int32_t _M0L6_2atmpS2098;
      #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S482)) {
        int32_t _M0L6_2atmpS2093 = _M0L5indexS479 + 1;
        _if__result_4318 = _M0L6_2atmpS2093 < _M0L11end__offsetS475;
      } else {
        _if__result_4318 = 0;
      }
      if (_if__result_4318) {
        int32_t _M0L6_2atmpS2096 = _M0L5indexS479 + 1;
        int32_t _M0L2c2S483 = _M0L4selfS478[_M0L6_2atmpS2096];
        #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S483)) {
          int32_t _M0L6_2atmpS2094 = _M0L5indexS479 + 2;
          int32_t _M0L6_2atmpS2095 = _M0L5countS480 + 1;
          _M0L5indexS479 = _M0L6_2atmpS2094;
          _M0L5countS480 = _M0L6_2atmpS2095;
          continue;
        } else {
          #line 426 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_94.data, (moonbit_string_t)moonbit_string_literal_96.data);
        }
      }
      _M0L6_2atmpS2097 = _M0L5indexS479 + 1;
      _M0L6_2atmpS2098 = _M0L5countS480 + 1;
      _M0L5indexS479 = _M0L6_2atmpS2097;
      _M0L5countS480 = _M0L6_2atmpS2098;
      continue;
    } else {
      moonbit_decref(_M0L4selfS478);
      if (_M0L5countS480 == _M0L3lenS481) {
        return _M0L5indexS479 == _M0L11end__offsetS475;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS471
) {
  int32_t _M0L3endS2085;
  int32_t _M0L8_2afieldS3906;
  int32_t _M0L5startS2086;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2085 = _M0L4selfS471.$2;
  _M0L8_2afieldS3906 = _M0L4selfS471.$1;
  moonbit_decref(_M0L4selfS471.$0);
  _M0L5startS2086 = _M0L8_2afieldS3906;
  return _M0L3endS2085 - _M0L5startS2086;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS472
) {
  int32_t _M0L3endS2087;
  int32_t _M0L8_2afieldS3907;
  int32_t _M0L5startS2088;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2087 = _M0L4selfS472.$2;
  _M0L8_2afieldS3907 = _M0L4selfS472.$1;
  moonbit_decref(_M0L4selfS472.$0);
  _M0L5startS2088 = _M0L8_2afieldS3907;
  return _M0L3endS2087 - _M0L5startS2088;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS473
) {
  int32_t _M0L3endS2089;
  int32_t _M0L8_2afieldS3908;
  int32_t _M0L5startS2090;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2089 = _M0L4selfS473.$2;
  _M0L8_2afieldS3908 = _M0L4selfS473.$1;
  moonbit_decref(_M0L4selfS473.$0);
  _M0L5startS2090 = _M0L8_2afieldS3908;
  return _M0L3endS2089 - _M0L5startS2090;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS474
) {
  int32_t _M0L3endS2091;
  int32_t _M0L8_2afieldS3909;
  int32_t _M0L5startS2092;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2091 = _M0L4selfS474.$2;
  _M0L8_2afieldS3909 = _M0L4selfS474.$1;
  moonbit_decref(_M0L4selfS474.$0);
  _M0L5startS2092 = _M0L8_2afieldS3909;
  return _M0L3endS2091 - _M0L5startS2092;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS469,
  int64_t _M0L19start__offset_2eoptS467,
  int64_t _M0L11end__offsetS470
) {
  int32_t _M0L13start__offsetS466;
  if (_M0L19start__offset_2eoptS467 == 4294967296ll) {
    _M0L13start__offsetS466 = 0;
  } else {
    int64_t _M0L7_2aSomeS468 = _M0L19start__offset_2eoptS467;
    _M0L13start__offsetS466 = (int32_t)_M0L7_2aSomeS468;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS469, _M0L13start__offsetS466, _M0L11end__offsetS470);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS464,
  int32_t _M0L13start__offsetS465,
  int64_t _M0L11end__offsetS462
) {
  int32_t _M0L11end__offsetS461;
  int32_t _if__result_4319;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS462 == 4294967296ll) {
    _M0L11end__offsetS461 = Moonbit_array_length(_M0L4selfS464);
  } else {
    int64_t _M0L7_2aSomeS463 = _M0L11end__offsetS462;
    _M0L11end__offsetS461 = (int32_t)_M0L7_2aSomeS463;
  }
  if (_M0L13start__offsetS465 >= 0) {
    if (_M0L13start__offsetS465 <= _M0L11end__offsetS461) {
      int32_t _M0L6_2atmpS2084 = Moonbit_array_length(_M0L4selfS464);
      _if__result_4319 = _M0L11end__offsetS461 <= _M0L6_2atmpS2084;
    } else {
      _if__result_4319 = 0;
    }
  } else {
    _if__result_4319 = 0;
  }
  if (_if__result_4319) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS465,
                                                 _M0L11end__offsetS461,
                                                 _M0L4selfS464};
  } else {
    moonbit_decref(_M0L4selfS464);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_97.data, (moonbit_string_t)moonbit_string_literal_98.data);
  }
}

int32_t _M0IPC16string10StringViewPB2Eq5equal(
  struct _M0TPC16string10StringView _M0L4selfS457,
  struct _M0TPC16string10StringView _M0L5otherS458
) {
  int32_t _M0L3lenS456;
  int32_t _M0L6_2atmpS2070;
  #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  moonbit_incref(_M0L4selfS457.$0);
  #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS456 = _M0MPC16string10StringView6length(_M0L4selfS457);
  moonbit_incref(_M0L5otherS458.$0);
  #line 271 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6_2atmpS2070 = _M0MPC16string10StringView6length(_M0L5otherS458);
  if (_M0L3lenS456 == _M0L6_2atmpS2070) {
    moonbit_string_t _M0L8_2afieldS3916 = _M0L4selfS457.$0;
    moonbit_string_t _M0L3strS2073 = _M0L8_2afieldS3916;
    moonbit_string_t _M0L8_2afieldS3915 = _M0L5otherS458.$0;
    moonbit_string_t _M0L3strS2074 = _M0L8_2afieldS3915;
    int32_t _M0L6_2atmpS3914 = _M0L3strS2073 == _M0L3strS2074;
    int32_t _if__result_4320;
    int32_t _M0L1iS459;
    if (_M0L6_2atmpS3914) {
      int32_t _M0L5startS2071 = _M0L4selfS457.$1;
      int32_t _M0L5startS2072 = _M0L5otherS458.$1;
      _if__result_4320 = _M0L5startS2071 == _M0L5startS2072;
    } else {
      _if__result_4320 = 0;
    }
    if (_if__result_4320) {
      moonbit_decref(_M0L5otherS458.$0);
      moonbit_decref(_M0L4selfS457.$0);
      return 1;
    }
    _M0L1iS459 = 0;
    while (1) {
      if (_M0L1iS459 < _M0L3lenS456) {
        moonbit_string_t _M0L8_2afieldS3913 = _M0L4selfS457.$0;
        moonbit_string_t _M0L3strS2080 = _M0L8_2afieldS3913;
        int32_t _M0L5startS2082 = _M0L4selfS457.$1;
        int32_t _M0L6_2atmpS2081 = _M0L5startS2082 + _M0L1iS459;
        int32_t _M0L6_2atmpS3912 = _M0L3strS2080[_M0L6_2atmpS2081];
        int32_t _M0L6_2atmpS2075 = _M0L6_2atmpS3912;
        moonbit_string_t _M0L8_2afieldS3911 = _M0L5otherS458.$0;
        moonbit_string_t _M0L3strS2077 = _M0L8_2afieldS3911;
        int32_t _M0L5startS2079 = _M0L5otherS458.$1;
        int32_t _M0L6_2atmpS2078 = _M0L5startS2079 + _M0L1iS459;
        int32_t _M0L6_2atmpS3910 = _M0L3strS2077[_M0L6_2atmpS2078];
        int32_t _M0L6_2atmpS2076 = _M0L6_2atmpS3910;
        int32_t _M0L6_2atmpS2083;
        if (_M0L6_2atmpS2075 == _M0L6_2atmpS2076) {
          
        } else {
          moonbit_decref(_M0L5otherS458.$0);
          moonbit_decref(_M0L4selfS457.$0);
          return 0;
        }
        _M0L6_2atmpS2083 = _M0L1iS459 + 1;
        _M0L1iS459 = _M0L6_2atmpS2083;
        continue;
      } else {
        moonbit_decref(_M0L5otherS458.$0);
        moonbit_decref(_M0L4selfS457.$0);
      }
      break;
    }
    return 1;
  } else {
    moonbit_decref(_M0L5otherS458.$0);
    moonbit_decref(_M0L4selfS457.$0);
    return 0;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS455
) {
  moonbit_string_t _M0L8_2afieldS3918;
  moonbit_string_t _M0L3strS2067;
  int32_t _M0L5startS2068;
  int32_t _M0L8_2afieldS3917;
  int32_t _M0L3endS2069;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3918 = _M0L4selfS455.$0;
  _M0L3strS2067 = _M0L8_2afieldS3918;
  _M0L5startS2068 = _M0L4selfS455.$1;
  _M0L8_2afieldS3917 = _M0L4selfS455.$2;
  _M0L3endS2069 = _M0L8_2afieldS3917;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2067, _M0L5startS2068, _M0L3endS2069);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS453,
  struct _M0TPB6Logger _M0L6loggerS454
) {
  moonbit_string_t _M0L8_2afieldS3920;
  moonbit_string_t _M0L3strS2064;
  int32_t _M0L5startS2065;
  int32_t _M0L8_2afieldS3919;
  int32_t _M0L3endS2066;
  moonbit_string_t _M0L6substrS452;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3920 = _M0L4selfS453.$0;
  _M0L3strS2064 = _M0L8_2afieldS3920;
  _M0L5startS2065 = _M0L4selfS453.$1;
  _M0L8_2afieldS3919 = _M0L4selfS453.$2;
  _M0L3endS2066 = _M0L8_2afieldS3919;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS452
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2064, _M0L5startS2065, _M0L3endS2066);
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
    int32_t _M0L6_2atmpS2048;
    int32_t _M0L6_2atmpS2049;
    int32_t _M0L6_2atmpS2050;
    int32_t _tmp_4325;
    int32_t _tmp_4326;
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
        int32_t _M0L6_2atmpS2051;
        int32_t _M0L6_2atmpS2052;
        moonbit_incref(_M0L6_2aenvS443);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_79.data);
        _M0L6_2atmpS2051 = _M0L1iS446 + 1;
        _M0L6_2atmpS2052 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2051;
        _M0L3segS447 = _M0L6_2atmpS2052;
        goto _2afor_448;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2053;
        int32_t _M0L6_2atmpS2054;
        moonbit_incref(_M0L6_2aenvS443);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_80.data);
        _M0L6_2atmpS2053 = _M0L1iS446 + 1;
        _M0L6_2atmpS2054 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2053;
        _M0L3segS447 = _M0L6_2atmpS2054;
        goto _2afor_448;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2055;
        int32_t _M0L6_2atmpS2056;
        moonbit_incref(_M0L6_2aenvS443);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_81.data);
        _M0L6_2atmpS2055 = _M0L1iS446 + 1;
        _M0L6_2atmpS2056 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2055;
        _M0L3segS447 = _M0L6_2atmpS2056;
        goto _2afor_448;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2057;
        int32_t _M0L6_2atmpS2058;
        moonbit_incref(_M0L6_2aenvS443);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
        if (_M0L6loggerS442.$1) {
          moonbit_incref(_M0L6loggerS442.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_82.data);
        _M0L6_2atmpS2057 = _M0L1iS446 + 1;
        _M0L6_2atmpS2058 = _M0L1iS446 + 1;
        _M0L1iS446 = _M0L6_2atmpS2057;
        _M0L3segS447 = _M0L6_2atmpS2058;
        goto _2afor_448;
        break;
      }
      default: {
        if (_M0L4codeS449 < 32) {
          int32_t _M0L6_2atmpS2060;
          moonbit_string_t _M0L6_2atmpS2059;
          int32_t _M0L6_2atmpS2061;
          int32_t _M0L6_2atmpS2062;
          moonbit_incref(_M0L6_2aenvS443);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS443, _M0L3segS447, _M0L1iS446);
          if (_M0L6loggerS442.$1) {
            moonbit_incref(_M0L6loggerS442.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, (moonbit_string_t)moonbit_string_literal_99.data);
          _M0L6_2atmpS2060 = _M0L4codeS449 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2059 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2060);
          if (_M0L6loggerS442.$1) {
            moonbit_incref(_M0L6loggerS442.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS442.$0->$method_0(_M0L6loggerS442.$1, _M0L6_2atmpS2059);
          if (_M0L6loggerS442.$1) {
            moonbit_incref(_M0L6loggerS442.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS442.$0->$method_3(_M0L6loggerS442.$1, 125);
          _M0L6_2atmpS2061 = _M0L1iS446 + 1;
          _M0L6_2atmpS2062 = _M0L1iS446 + 1;
          _M0L1iS446 = _M0L6_2atmpS2061;
          _M0L3segS447 = _M0L6_2atmpS2062;
          goto _2afor_448;
        } else {
          int32_t _M0L6_2atmpS2063 = _M0L1iS446 + 1;
          int32_t _tmp_4324 = _M0L3segS447;
          _M0L1iS446 = _M0L6_2atmpS2063;
          _M0L3segS447 = _tmp_4324;
          goto _2afor_448;
        }
        break;
      }
    }
    goto joinlet_4323;
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
    _M0L6_2atmpS2048 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS451);
    if (_M0L6loggerS442.$1) {
      moonbit_incref(_M0L6loggerS442.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS442.$0->$method_3(_M0L6loggerS442.$1, _M0L6_2atmpS2048);
    _M0L6_2atmpS2049 = _M0L1iS446 + 1;
    _M0L6_2atmpS2050 = _M0L1iS446 + 1;
    _M0L1iS446 = _M0L6_2atmpS2049;
    _M0L3segS447 = _M0L6_2atmpS2050;
    continue;
    joinlet_4323:;
    _tmp_4325 = _M0L1iS446;
    _tmp_4326 = _M0L3segS447;
    _M0L1iS446 = _tmp_4325;
    _M0L3segS447 = _tmp_4326;
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
  struct _M0TPB6Logger _M0L8_2afieldS3922;
  struct _M0TPB6Logger _M0L6loggerS437;
  moonbit_string_t _M0L8_2afieldS3921;
  int32_t _M0L6_2acntS4081;
  moonbit_string_t _M0L4selfS439;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3922
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS438->$1_0, _M0L6_2aenvS438->$1_1
  };
  _M0L6loggerS437 = _M0L8_2afieldS3922;
  _M0L8_2afieldS3921 = _M0L6_2aenvS438->$0;
  _M0L6_2acntS4081 = Moonbit_object_header(_M0L6_2aenvS438)->rc;
  if (_M0L6_2acntS4081 > 1) {
    int32_t _M0L11_2anew__cntS4082 = _M0L6_2acntS4081 - 1;
    Moonbit_object_header(_M0L6_2aenvS438)->rc = _M0L11_2anew__cntS4082;
    if (_M0L6loggerS437.$1) {
      moonbit_incref(_M0L6loggerS437.$1);
    }
    moonbit_incref(_M0L8_2afieldS3921);
  } else if (_M0L6_2acntS4081 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS438);
  }
  _M0L4selfS439 = _M0L8_2afieldS3921;
  if (_M0L1iS440 > _M0L3segS441) {
    int32_t _M0L6_2atmpS2047 = _M0L1iS440 - _M0L3segS441;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS437.$0->$method_1(_M0L6loggerS437.$1, _M0L4selfS439, _M0L3segS441, _M0L6_2atmpS2047);
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
  int32_t _M0L6_2atmpS2044;
  int32_t _M0L6_2atmpS2043;
  int32_t _M0L6_2atmpS2046;
  int32_t _M0L6_2atmpS2045;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2042;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS435 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2044 = _M0IPC14byte4BytePB3Div3div(_M0L1bS436, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2043
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2044);
  moonbit_incref(_M0L7_2aselfS435);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS435, _M0L6_2atmpS2043);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2046 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS436, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2045
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2046);
  moonbit_incref(_M0L7_2aselfS435);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS435, _M0L6_2atmpS2045);
  _M0L6_2atmpS2042 = _M0L7_2aselfS435;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2042);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS434) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS434 < 10) {
    int32_t _M0L6_2atmpS2039;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2039 = _M0IPC14byte4BytePB3Add3add(_M0L1iS434, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2039);
  } else {
    int32_t _M0L6_2atmpS2041;
    int32_t _M0L6_2atmpS2040;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2041 = _M0IPC14byte4BytePB3Add3add(_M0L1iS434, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2040 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2041, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2040);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS432,
  int32_t _M0L4thatS433
) {
  int32_t _M0L6_2atmpS2037;
  int32_t _M0L6_2atmpS2038;
  int32_t _M0L6_2atmpS2036;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2037 = (int32_t)_M0L4selfS432;
  _M0L6_2atmpS2038 = (int32_t)_M0L4thatS433;
  _M0L6_2atmpS2036 = _M0L6_2atmpS2037 - _M0L6_2atmpS2038;
  return _M0L6_2atmpS2036 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS430,
  int32_t _M0L4thatS431
) {
  int32_t _M0L6_2atmpS2034;
  int32_t _M0L6_2atmpS2035;
  int32_t _M0L6_2atmpS2033;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2034 = (int32_t)_M0L4selfS430;
  _M0L6_2atmpS2035 = (int32_t)_M0L4thatS431;
  _M0L6_2atmpS2033 = _M0L6_2atmpS2034 % _M0L6_2atmpS2035;
  return _M0L6_2atmpS2033 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS428,
  int32_t _M0L4thatS429
) {
  int32_t _M0L6_2atmpS2031;
  int32_t _M0L6_2atmpS2032;
  int32_t _M0L6_2atmpS2030;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2031 = (int32_t)_M0L4selfS428;
  _M0L6_2atmpS2032 = (int32_t)_M0L4thatS429;
  _M0L6_2atmpS2030 = _M0L6_2atmpS2031 / _M0L6_2atmpS2032;
  return _M0L6_2atmpS2030 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS426,
  int32_t _M0L4thatS427
) {
  int32_t _M0L6_2atmpS2028;
  int32_t _M0L6_2atmpS2029;
  int32_t _M0L6_2atmpS2027;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2028 = (int32_t)_M0L4selfS426;
  _M0L6_2atmpS2029 = (int32_t)_M0L4thatS427;
  _M0L6_2atmpS2027 = _M0L6_2atmpS2028 + _M0L6_2atmpS2029;
  return _M0L6_2atmpS2027 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS423,
  int32_t _M0L5startS421,
  int32_t _M0L3endS422
) {
  int32_t _if__result_4327;
  int32_t _M0L3lenS424;
  int32_t _M0L6_2atmpS2025;
  int32_t _M0L6_2atmpS2026;
  moonbit_bytes_t _M0L5bytesS425;
  moonbit_bytes_t _M0L6_2atmpS2024;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS421 == 0) {
    int32_t _M0L6_2atmpS2023 = Moonbit_array_length(_M0L3strS423);
    _if__result_4327 = _M0L3endS422 == _M0L6_2atmpS2023;
  } else {
    _if__result_4327 = 0;
  }
  if (_if__result_4327) {
    return _M0L3strS423;
  }
  _M0L3lenS424 = _M0L3endS422 - _M0L5startS421;
  _M0L6_2atmpS2025 = _M0L3lenS424 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2026 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS425
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2025, _M0L6_2atmpS2026);
  moonbit_incref(_M0L5bytesS425);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS425, 0, _M0L3strS423, _M0L5startS421, _M0L3lenS424);
  _M0L6_2atmpS2024 = _M0L5bytesS425;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2024, 0, 4294967296ll);
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView _M0L4selfS419,
  int32_t _M0L13start__offsetS420,
  int64_t _M0L11end__offsetS417
) {
  int32_t _M0L11end__offsetS416;
  int32_t _if__result_4328;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS417 == 4294967296ll) {
    moonbit_incref(_M0L4selfS419.$0);
    #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L11end__offsetS416 = _M0MPC16string10StringView6length(_M0L4selfS419);
  } else {
    int64_t _M0L7_2aSomeS418 = _M0L11end__offsetS417;
    _M0L11end__offsetS416 = (int32_t)_M0L7_2aSomeS418;
  }
  if (_M0L13start__offsetS420 >= 0) {
    if (_M0L13start__offsetS420 <= _M0L11end__offsetS416) {
      int32_t _M0L6_2atmpS2017;
      moonbit_incref(_M0L4selfS419.$0);
      #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2017 = _M0MPC16string10StringView6length(_M0L4selfS419);
      _if__result_4328 = _M0L11end__offsetS416 <= _M0L6_2atmpS2017;
    } else {
      _if__result_4328 = 0;
    }
  } else {
    _if__result_4328 = 0;
  }
  if (_if__result_4328) {
    moonbit_string_t _M0L8_2afieldS3924 = _M0L4selfS419.$0;
    moonbit_string_t _M0L3strS2018 = _M0L8_2afieldS3924;
    int32_t _M0L5startS2022 = _M0L4selfS419.$1;
    int32_t _M0L6_2atmpS2019 = _M0L5startS2022 + _M0L13start__offsetS420;
    int32_t _M0L8_2afieldS3923 = _M0L4selfS419.$1;
    int32_t _M0L5startS2021 = _M0L8_2afieldS3923;
    int32_t _M0L6_2atmpS2020 = _M0L5startS2021 + _M0L11end__offsetS416;
    return (struct _M0TPC16string10StringView){_M0L6_2atmpS2019,
                                                 _M0L6_2atmpS2020,
                                                 _M0L3strS2018};
  } else {
    moonbit_decref(_M0L4selfS419.$0);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_97.data, (moonbit_string_t)moonbit_string_literal_100.data);
  }
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS413) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS413;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS414
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS414;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS415) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS415;
}

struct moonbit_result_1 _M0FPB4failGRP48clawteam8clawteam8internal9buildinfo7VersionE(
  moonbit_string_t _M0L3msgS412,
  moonbit_string_t _M0L3locS411
) {
  moonbit_string_t _M0L6_2atmpS2016;
  moonbit_string_t _M0L6_2atmpS3926;
  moonbit_string_t _M0L6_2atmpS2014;
  moonbit_string_t _M0L6_2atmpS2015;
  moonbit_string_t _M0L6_2atmpS3925;
  moonbit_string_t _M0L6_2atmpS2013;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2012;
  struct moonbit_result_1 _result_4329;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS2016
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS411);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS3926
  = moonbit_add_string(_M0L6_2atmpS2016, (moonbit_string_t)moonbit_string_literal_101.data);
  moonbit_decref(_M0L6_2atmpS2016);
  _M0L6_2atmpS2014 = _M0L6_2atmpS3926;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS2015 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS412);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS3925 = moonbit_add_string(_M0L6_2atmpS2014, _M0L6_2atmpS2015);
  moonbit_decref(_M0L6_2atmpS2014);
  moonbit_decref(_M0L6_2atmpS2015);
  _M0L6_2atmpS2013 = _M0L6_2atmpS3925;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2012
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2012)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2012)->$0
  = _M0L6_2atmpS2013;
  _result_4329.tag = 0;
  _result_4329.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2012;
  return _result_4329;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS403,
  int32_t _M0L5radixS402
) {
  int32_t _if__result_4330;
  uint16_t* _M0L6bufferS404;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS402 < 2) {
    _if__result_4330 = 1;
  } else {
    _if__result_4330 = _M0L5radixS402 > 36;
  }
  if (_if__result_4330) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_102.data, (moonbit_string_t)moonbit_string_literal_103.data);
  }
  if (_M0L4selfS403 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
  }
  switch (_M0L5radixS402) {
    case 10: {
      int32_t _M0L3lenS405;
      uint16_t* _M0L6bufferS406;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS405 = _M0FPB12dec__count64(_M0L4selfS403);
      _M0L6bufferS406 = (uint16_t*)moonbit_make_string(_M0L3lenS405, 0);
      moonbit_incref(_M0L6bufferS406);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS406, _M0L4selfS403, 0, _M0L3lenS405);
      _M0L6bufferS404 = _M0L6bufferS406;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS407;
      uint16_t* _M0L6bufferS408;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS407 = _M0FPB12hex__count64(_M0L4selfS403);
      _M0L6bufferS408 = (uint16_t*)moonbit_make_string(_M0L3lenS407, 0);
      moonbit_incref(_M0L6bufferS408);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS408, _M0L4selfS403, 0, _M0L3lenS407);
      _M0L6bufferS404 = _M0L6bufferS408;
      break;
    }
    default: {
      int32_t _M0L3lenS409;
      uint16_t* _M0L6bufferS410;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS409 = _M0FPB14radix__count64(_M0L4selfS403, _M0L5radixS402);
      _M0L6bufferS410 = (uint16_t*)moonbit_make_string(_M0L3lenS409, 0);
      moonbit_incref(_M0L6bufferS410);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS410, _M0L4selfS403, 0, _M0L3lenS409, _M0L5radixS402);
      _M0L6bufferS404 = _M0L6bufferS410;
      break;
    }
  }
  return _M0L6bufferS404;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS392,
  uint64_t _M0L3numS380,
  int32_t _M0L12digit__startS383,
  int32_t _M0L10total__lenS382
) {
  uint64_t _M0Lm3numS379;
  int32_t _M0Lm6offsetS381;
  uint64_t _M0L6_2atmpS2011;
  int32_t _M0Lm9remainingS394;
  int32_t _M0L6_2atmpS1992;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS379 = _M0L3numS380;
  _M0Lm6offsetS381 = _M0L10total__lenS382 - _M0L12digit__startS383;
  while (1) {
    uint64_t _M0L6_2atmpS1955 = _M0Lm3numS379;
    if (_M0L6_2atmpS1955 >= 10000ull) {
      uint64_t _M0L6_2atmpS1978 = _M0Lm3numS379;
      uint64_t _M0L1tS384 = _M0L6_2atmpS1978 / 10000ull;
      uint64_t _M0L6_2atmpS1977 = _M0Lm3numS379;
      uint64_t _M0L6_2atmpS1976 = _M0L6_2atmpS1977 % 10000ull;
      int32_t _M0L1rS385 = (int32_t)_M0L6_2atmpS1976;
      int32_t _M0L2d1S386;
      int32_t _M0L2d2S387;
      int32_t _M0L6_2atmpS1956;
      int32_t _M0L6_2atmpS1975;
      int32_t _M0L6_2atmpS1974;
      int32_t _M0L6d1__hiS388;
      int32_t _M0L6_2atmpS1973;
      int32_t _M0L6_2atmpS1972;
      int32_t _M0L6d1__loS389;
      int32_t _M0L6_2atmpS1971;
      int32_t _M0L6_2atmpS1970;
      int32_t _M0L6d2__hiS390;
      int32_t _M0L6_2atmpS1969;
      int32_t _M0L6_2atmpS1968;
      int32_t _M0L6d2__loS391;
      int32_t _M0L6_2atmpS1958;
      int32_t _M0L6_2atmpS1957;
      int32_t _M0L6_2atmpS1961;
      int32_t _M0L6_2atmpS1960;
      int32_t _M0L6_2atmpS1959;
      int32_t _M0L6_2atmpS1964;
      int32_t _M0L6_2atmpS1963;
      int32_t _M0L6_2atmpS1962;
      int32_t _M0L6_2atmpS1967;
      int32_t _M0L6_2atmpS1966;
      int32_t _M0L6_2atmpS1965;
      _M0Lm3numS379 = _M0L1tS384;
      _M0L2d1S386 = _M0L1rS385 / 100;
      _M0L2d2S387 = _M0L1rS385 % 100;
      _M0L6_2atmpS1956 = _M0Lm6offsetS381;
      _M0Lm6offsetS381 = _M0L6_2atmpS1956 - 4;
      _M0L6_2atmpS1975 = _M0L2d1S386 / 10;
      _M0L6_2atmpS1974 = 48 + _M0L6_2atmpS1975;
      _M0L6d1__hiS388 = (uint16_t)_M0L6_2atmpS1974;
      _M0L6_2atmpS1973 = _M0L2d1S386 % 10;
      _M0L6_2atmpS1972 = 48 + _M0L6_2atmpS1973;
      _M0L6d1__loS389 = (uint16_t)_M0L6_2atmpS1972;
      _M0L6_2atmpS1971 = _M0L2d2S387 / 10;
      _M0L6_2atmpS1970 = 48 + _M0L6_2atmpS1971;
      _M0L6d2__hiS390 = (uint16_t)_M0L6_2atmpS1970;
      _M0L6_2atmpS1969 = _M0L2d2S387 % 10;
      _M0L6_2atmpS1968 = 48 + _M0L6_2atmpS1969;
      _M0L6d2__loS391 = (uint16_t)_M0L6_2atmpS1968;
      _M0L6_2atmpS1958 = _M0Lm6offsetS381;
      _M0L6_2atmpS1957 = _M0L12digit__startS383 + _M0L6_2atmpS1958;
      _M0L6bufferS392[_M0L6_2atmpS1957] = _M0L6d1__hiS388;
      _M0L6_2atmpS1961 = _M0Lm6offsetS381;
      _M0L6_2atmpS1960 = _M0L12digit__startS383 + _M0L6_2atmpS1961;
      _M0L6_2atmpS1959 = _M0L6_2atmpS1960 + 1;
      _M0L6bufferS392[_M0L6_2atmpS1959] = _M0L6d1__loS389;
      _M0L6_2atmpS1964 = _M0Lm6offsetS381;
      _M0L6_2atmpS1963 = _M0L12digit__startS383 + _M0L6_2atmpS1964;
      _M0L6_2atmpS1962 = _M0L6_2atmpS1963 + 2;
      _M0L6bufferS392[_M0L6_2atmpS1962] = _M0L6d2__hiS390;
      _M0L6_2atmpS1967 = _M0Lm6offsetS381;
      _M0L6_2atmpS1966 = _M0L12digit__startS383 + _M0L6_2atmpS1967;
      _M0L6_2atmpS1965 = _M0L6_2atmpS1966 + 3;
      _M0L6bufferS392[_M0L6_2atmpS1965] = _M0L6d2__loS391;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2011 = _M0Lm3numS379;
  _M0Lm9remainingS394 = (int32_t)_M0L6_2atmpS2011;
  while (1) {
    int32_t _M0L6_2atmpS1979 = _M0Lm9remainingS394;
    if (_M0L6_2atmpS1979 >= 100) {
      int32_t _M0L6_2atmpS1991 = _M0Lm9remainingS394;
      int32_t _M0L1tS395 = _M0L6_2atmpS1991 / 100;
      int32_t _M0L6_2atmpS1990 = _M0Lm9remainingS394;
      int32_t _M0L1dS396 = _M0L6_2atmpS1990 % 100;
      int32_t _M0L6_2atmpS1980;
      int32_t _M0L6_2atmpS1989;
      int32_t _M0L6_2atmpS1988;
      int32_t _M0L5d__hiS397;
      int32_t _M0L6_2atmpS1987;
      int32_t _M0L6_2atmpS1986;
      int32_t _M0L5d__loS398;
      int32_t _M0L6_2atmpS1982;
      int32_t _M0L6_2atmpS1981;
      int32_t _M0L6_2atmpS1985;
      int32_t _M0L6_2atmpS1984;
      int32_t _M0L6_2atmpS1983;
      _M0Lm9remainingS394 = _M0L1tS395;
      _M0L6_2atmpS1980 = _M0Lm6offsetS381;
      _M0Lm6offsetS381 = _M0L6_2atmpS1980 - 2;
      _M0L6_2atmpS1989 = _M0L1dS396 / 10;
      _M0L6_2atmpS1988 = 48 + _M0L6_2atmpS1989;
      _M0L5d__hiS397 = (uint16_t)_M0L6_2atmpS1988;
      _M0L6_2atmpS1987 = _M0L1dS396 % 10;
      _M0L6_2atmpS1986 = 48 + _M0L6_2atmpS1987;
      _M0L5d__loS398 = (uint16_t)_M0L6_2atmpS1986;
      _M0L6_2atmpS1982 = _M0Lm6offsetS381;
      _M0L6_2atmpS1981 = _M0L12digit__startS383 + _M0L6_2atmpS1982;
      _M0L6bufferS392[_M0L6_2atmpS1981] = _M0L5d__hiS397;
      _M0L6_2atmpS1985 = _M0Lm6offsetS381;
      _M0L6_2atmpS1984 = _M0L12digit__startS383 + _M0L6_2atmpS1985;
      _M0L6_2atmpS1983 = _M0L6_2atmpS1984 + 1;
      _M0L6bufferS392[_M0L6_2atmpS1983] = _M0L5d__loS398;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1992 = _M0Lm9remainingS394;
  if (_M0L6_2atmpS1992 >= 10) {
    int32_t _M0L6_2atmpS1993 = _M0Lm6offsetS381;
    int32_t _M0L6_2atmpS2004;
    int32_t _M0L6_2atmpS2003;
    int32_t _M0L6_2atmpS2002;
    int32_t _M0L5d__hiS400;
    int32_t _M0L6_2atmpS2001;
    int32_t _M0L6_2atmpS2000;
    int32_t _M0L6_2atmpS1999;
    int32_t _M0L5d__loS401;
    int32_t _M0L6_2atmpS1995;
    int32_t _M0L6_2atmpS1994;
    int32_t _M0L6_2atmpS1998;
    int32_t _M0L6_2atmpS1997;
    int32_t _M0L6_2atmpS1996;
    _M0Lm6offsetS381 = _M0L6_2atmpS1993 - 2;
    _M0L6_2atmpS2004 = _M0Lm9remainingS394;
    _M0L6_2atmpS2003 = _M0L6_2atmpS2004 / 10;
    _M0L6_2atmpS2002 = 48 + _M0L6_2atmpS2003;
    _M0L5d__hiS400 = (uint16_t)_M0L6_2atmpS2002;
    _M0L6_2atmpS2001 = _M0Lm9remainingS394;
    _M0L6_2atmpS2000 = _M0L6_2atmpS2001 % 10;
    _M0L6_2atmpS1999 = 48 + _M0L6_2atmpS2000;
    _M0L5d__loS401 = (uint16_t)_M0L6_2atmpS1999;
    _M0L6_2atmpS1995 = _M0Lm6offsetS381;
    _M0L6_2atmpS1994 = _M0L12digit__startS383 + _M0L6_2atmpS1995;
    _M0L6bufferS392[_M0L6_2atmpS1994] = _M0L5d__hiS400;
    _M0L6_2atmpS1998 = _M0Lm6offsetS381;
    _M0L6_2atmpS1997 = _M0L12digit__startS383 + _M0L6_2atmpS1998;
    _M0L6_2atmpS1996 = _M0L6_2atmpS1997 + 1;
    _M0L6bufferS392[_M0L6_2atmpS1996] = _M0L5d__loS401;
    moonbit_decref(_M0L6bufferS392);
  } else {
    int32_t _M0L6_2atmpS2005 = _M0Lm6offsetS381;
    int32_t _M0L6_2atmpS2010;
    int32_t _M0L6_2atmpS2006;
    int32_t _M0L6_2atmpS2009;
    int32_t _M0L6_2atmpS2008;
    int32_t _M0L6_2atmpS2007;
    _M0Lm6offsetS381 = _M0L6_2atmpS2005 - 1;
    _M0L6_2atmpS2010 = _M0Lm6offsetS381;
    _M0L6_2atmpS2006 = _M0L12digit__startS383 + _M0L6_2atmpS2010;
    _M0L6_2atmpS2009 = _M0Lm9remainingS394;
    _M0L6_2atmpS2008 = 48 + _M0L6_2atmpS2009;
    _M0L6_2atmpS2007 = (uint16_t)_M0L6_2atmpS2008;
    _M0L6bufferS392[_M0L6_2atmpS2006] = _M0L6_2atmpS2007;
    moonbit_decref(_M0L6bufferS392);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS374,
  uint64_t _M0L3numS368,
  int32_t _M0L12digit__startS366,
  int32_t _M0L10total__lenS365,
  int32_t _M0L5radixS370
) {
  int32_t _M0Lm6offsetS364;
  uint64_t _M0Lm1nS367;
  uint64_t _M0L4baseS369;
  int32_t _M0L6_2atmpS1937;
  int32_t _M0L6_2atmpS1936;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS364 = _M0L10total__lenS365 - _M0L12digit__startS366;
  _M0Lm1nS367 = _M0L3numS368;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS369 = _M0MPC13int3Int10to__uint64(_M0L5radixS370);
  _M0L6_2atmpS1937 = _M0L5radixS370 - 1;
  _M0L6_2atmpS1936 = _M0L5radixS370 & _M0L6_2atmpS1937;
  if (_M0L6_2atmpS1936 == 0) {
    int32_t _M0L5shiftS371;
    uint64_t _M0L4maskS372;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS371 = moonbit_ctz32(_M0L5radixS370);
    _M0L4maskS372 = _M0L4baseS369 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1938 = _M0Lm1nS367;
      if (_M0L6_2atmpS1938 > 0ull) {
        int32_t _M0L6_2atmpS1939 = _M0Lm6offsetS364;
        uint64_t _M0L6_2atmpS1945;
        uint64_t _M0L6_2atmpS1944;
        int32_t _M0L5digitS373;
        int32_t _M0L6_2atmpS1942;
        int32_t _M0L6_2atmpS1940;
        int32_t _M0L6_2atmpS1941;
        uint64_t _M0L6_2atmpS1943;
        _M0Lm6offsetS364 = _M0L6_2atmpS1939 - 1;
        _M0L6_2atmpS1945 = _M0Lm1nS367;
        _M0L6_2atmpS1944 = _M0L6_2atmpS1945 & _M0L4maskS372;
        _M0L5digitS373 = (int32_t)_M0L6_2atmpS1944;
        _M0L6_2atmpS1942 = _M0Lm6offsetS364;
        _M0L6_2atmpS1940 = _M0L12digit__startS366 + _M0L6_2atmpS1942;
        _M0L6_2atmpS1941
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS373
        ];
        _M0L6bufferS374[_M0L6_2atmpS1940] = _M0L6_2atmpS1941;
        _M0L6_2atmpS1943 = _M0Lm1nS367;
        _M0Lm1nS367 = _M0L6_2atmpS1943 >> (_M0L5shiftS371 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS374);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1946 = _M0Lm1nS367;
      if (_M0L6_2atmpS1946 > 0ull) {
        int32_t _M0L6_2atmpS1947 = _M0Lm6offsetS364;
        uint64_t _M0L6_2atmpS1954;
        uint64_t _M0L1qS376;
        uint64_t _M0L6_2atmpS1952;
        uint64_t _M0L6_2atmpS1953;
        uint64_t _M0L6_2atmpS1951;
        int32_t _M0L5digitS377;
        int32_t _M0L6_2atmpS1950;
        int32_t _M0L6_2atmpS1948;
        int32_t _M0L6_2atmpS1949;
        _M0Lm6offsetS364 = _M0L6_2atmpS1947 - 1;
        _M0L6_2atmpS1954 = _M0Lm1nS367;
        _M0L1qS376 = _M0L6_2atmpS1954 / _M0L4baseS369;
        _M0L6_2atmpS1952 = _M0Lm1nS367;
        _M0L6_2atmpS1953 = _M0L1qS376 * _M0L4baseS369;
        _M0L6_2atmpS1951 = _M0L6_2atmpS1952 - _M0L6_2atmpS1953;
        _M0L5digitS377 = (int32_t)_M0L6_2atmpS1951;
        _M0L6_2atmpS1950 = _M0Lm6offsetS364;
        _M0L6_2atmpS1948 = _M0L12digit__startS366 + _M0L6_2atmpS1950;
        _M0L6_2atmpS1949
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS377
        ];
        _M0L6bufferS374[_M0L6_2atmpS1948] = _M0L6_2atmpS1949;
        _M0Lm1nS367 = _M0L1qS376;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS374);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS361,
  uint64_t _M0L3numS357,
  int32_t _M0L12digit__startS355,
  int32_t _M0L10total__lenS354
) {
  int32_t _M0Lm6offsetS353;
  uint64_t _M0Lm1nS356;
  int32_t _M0L6_2atmpS1932;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS353 = _M0L10total__lenS354 - _M0L12digit__startS355;
  _M0Lm1nS356 = _M0L3numS357;
  while (1) {
    int32_t _M0L6_2atmpS1920 = _M0Lm6offsetS353;
    if (_M0L6_2atmpS1920 >= 2) {
      int32_t _M0L6_2atmpS1921 = _M0Lm6offsetS353;
      uint64_t _M0L6_2atmpS1931;
      uint64_t _M0L6_2atmpS1930;
      int32_t _M0L9byte__valS358;
      int32_t _M0L2hiS359;
      int32_t _M0L2loS360;
      int32_t _M0L6_2atmpS1924;
      int32_t _M0L6_2atmpS1922;
      int32_t _M0L6_2atmpS1923;
      int32_t _M0L6_2atmpS1928;
      int32_t _M0L6_2atmpS1927;
      int32_t _M0L6_2atmpS1925;
      int32_t _M0L6_2atmpS1926;
      uint64_t _M0L6_2atmpS1929;
      _M0Lm6offsetS353 = _M0L6_2atmpS1921 - 2;
      _M0L6_2atmpS1931 = _M0Lm1nS356;
      _M0L6_2atmpS1930 = _M0L6_2atmpS1931 & 255ull;
      _M0L9byte__valS358 = (int32_t)_M0L6_2atmpS1930;
      _M0L2hiS359 = _M0L9byte__valS358 / 16;
      _M0L2loS360 = _M0L9byte__valS358 % 16;
      _M0L6_2atmpS1924 = _M0Lm6offsetS353;
      _M0L6_2atmpS1922 = _M0L12digit__startS355 + _M0L6_2atmpS1924;
      _M0L6_2atmpS1923
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2hiS359
      ];
      _M0L6bufferS361[_M0L6_2atmpS1922] = _M0L6_2atmpS1923;
      _M0L6_2atmpS1928 = _M0Lm6offsetS353;
      _M0L6_2atmpS1927 = _M0L12digit__startS355 + _M0L6_2atmpS1928;
      _M0L6_2atmpS1925 = _M0L6_2atmpS1927 + 1;
      _M0L6_2atmpS1926
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS360
      ];
      _M0L6bufferS361[_M0L6_2atmpS1925] = _M0L6_2atmpS1926;
      _M0L6_2atmpS1929 = _M0Lm1nS356;
      _M0Lm1nS356 = _M0L6_2atmpS1929 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1932 = _M0Lm6offsetS353;
  if (_M0L6_2atmpS1932 == 1) {
    uint64_t _M0L6_2atmpS1935 = _M0Lm1nS356;
    uint64_t _M0L6_2atmpS1934 = _M0L6_2atmpS1935 & 15ull;
    int32_t _M0L6nibbleS363 = (int32_t)_M0L6_2atmpS1934;
    int32_t _M0L6_2atmpS1933 =
      ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS363];
    _M0L6bufferS361[_M0L12digit__startS355] = _M0L6_2atmpS1933;
    moonbit_decref(_M0L6bufferS361);
  } else {
    moonbit_decref(_M0L6bufferS361);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS347,
  int32_t _M0L5radixS350
) {
  uint64_t _M0Lm3numS348;
  uint64_t _M0L4baseS349;
  int32_t _M0Lm5countS351;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS347 == 0ull) {
    return 1;
  }
  _M0Lm3numS348 = _M0L5valueS347;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS349 = _M0MPC13int3Int10to__uint64(_M0L5radixS350);
  _M0Lm5countS351 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1917 = _M0Lm3numS348;
    if (_M0L6_2atmpS1917 > 0ull) {
      int32_t _M0L6_2atmpS1918 = _M0Lm5countS351;
      uint64_t _M0L6_2atmpS1919;
      _M0Lm5countS351 = _M0L6_2atmpS1918 + 1;
      _M0L6_2atmpS1919 = _M0Lm3numS348;
      _M0Lm3numS348 = _M0L6_2atmpS1919 / _M0L4baseS349;
      continue;
    }
    break;
  }
  return _M0Lm5countS351;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS345) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS345 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS346;
    int32_t _M0L6_2atmpS1916;
    int32_t _M0L6_2atmpS1915;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS346 = moonbit_clz64(_M0L5valueS345);
    _M0L6_2atmpS1916 = 63 - _M0L14leading__zerosS346;
    _M0L6_2atmpS1915 = _M0L6_2atmpS1916 / 4;
    return _M0L6_2atmpS1915 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS344) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS344 >= 10000000000ull) {
    if (_M0L5valueS344 >= 100000000000000ull) {
      if (_M0L5valueS344 >= 10000000000000000ull) {
        if (_M0L5valueS344 >= 1000000000000000000ull) {
          if (_M0L5valueS344 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS344 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS344 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS344 >= 1000000000000ull) {
      if (_M0L5valueS344 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS344 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS344 >= 100000ull) {
    if (_M0L5valueS344 >= 10000000ull) {
      if (_M0L5valueS344 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS344 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS344 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS344 >= 1000ull) {
    if (_M0L5valueS344 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS344 >= 100ull) {
    return 3;
  } else if (_M0L5valueS344 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS328,
  int32_t _M0L5radixS327
) {
  int32_t _if__result_4337;
  int32_t _M0L12is__negativeS329;
  uint32_t _M0L3numS330;
  uint16_t* _M0L6bufferS331;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS327 < 2) {
    _if__result_4337 = 1;
  } else {
    _if__result_4337 = _M0L5radixS327 > 36;
  }
  if (_if__result_4337) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_102.data, (moonbit_string_t)moonbit_string_literal_105.data);
  }
  if (_M0L4selfS328 == 0) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
  }
  _M0L12is__negativeS329 = _M0L4selfS328 < 0;
  if (_M0L12is__negativeS329) {
    int32_t _M0L6_2atmpS1914 = -_M0L4selfS328;
    _M0L3numS330 = *(uint32_t*)&_M0L6_2atmpS1914;
  } else {
    _M0L3numS330 = *(uint32_t*)&_M0L4selfS328;
  }
  switch (_M0L5radixS327) {
    case 10: {
      int32_t _M0L10digit__lenS332;
      int32_t _M0L6_2atmpS1911;
      int32_t _M0L10total__lenS333;
      uint16_t* _M0L6bufferS334;
      int32_t _M0L12digit__startS335;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS332 = _M0FPB12dec__count32(_M0L3numS330);
      if (_M0L12is__negativeS329) {
        _M0L6_2atmpS1911 = 1;
      } else {
        _M0L6_2atmpS1911 = 0;
      }
      _M0L10total__lenS333 = _M0L10digit__lenS332 + _M0L6_2atmpS1911;
      _M0L6bufferS334
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS333, 0);
      if (_M0L12is__negativeS329) {
        _M0L12digit__startS335 = 1;
      } else {
        _M0L12digit__startS335 = 0;
      }
      moonbit_incref(_M0L6bufferS334);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS334, _M0L3numS330, _M0L12digit__startS335, _M0L10total__lenS333);
      _M0L6bufferS331 = _M0L6bufferS334;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS336;
      int32_t _M0L6_2atmpS1912;
      int32_t _M0L10total__lenS337;
      uint16_t* _M0L6bufferS338;
      int32_t _M0L12digit__startS339;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS336 = _M0FPB12hex__count32(_M0L3numS330);
      if (_M0L12is__negativeS329) {
        _M0L6_2atmpS1912 = 1;
      } else {
        _M0L6_2atmpS1912 = 0;
      }
      _M0L10total__lenS337 = _M0L10digit__lenS336 + _M0L6_2atmpS1912;
      _M0L6bufferS338
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS337, 0);
      if (_M0L12is__negativeS329) {
        _M0L12digit__startS339 = 1;
      } else {
        _M0L12digit__startS339 = 0;
      }
      moonbit_incref(_M0L6bufferS338);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS338, _M0L3numS330, _M0L12digit__startS339, _M0L10total__lenS337);
      _M0L6bufferS331 = _M0L6bufferS338;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS340;
      int32_t _M0L6_2atmpS1913;
      int32_t _M0L10total__lenS341;
      uint16_t* _M0L6bufferS342;
      int32_t _M0L12digit__startS343;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS340
      = _M0FPB14radix__count32(_M0L3numS330, _M0L5radixS327);
      if (_M0L12is__negativeS329) {
        _M0L6_2atmpS1913 = 1;
      } else {
        _M0L6_2atmpS1913 = 0;
      }
      _M0L10total__lenS341 = _M0L10digit__lenS340 + _M0L6_2atmpS1913;
      _M0L6bufferS342
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS341, 0);
      if (_M0L12is__negativeS329) {
        _M0L12digit__startS343 = 1;
      } else {
        _M0L12digit__startS343 = 0;
      }
      moonbit_incref(_M0L6bufferS342);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS342, _M0L3numS330, _M0L12digit__startS343, _M0L10total__lenS341, _M0L5radixS327);
      _M0L6bufferS331 = _M0L6bufferS342;
      break;
    }
  }
  if (_M0L12is__negativeS329) {
    _M0L6bufferS331[0] = 45;
  }
  return _M0L6bufferS331;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS321,
  int32_t _M0L5radixS324
) {
  uint32_t _M0Lm3numS322;
  uint32_t _M0L4baseS323;
  int32_t _M0Lm5countS325;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS321 == 0u) {
    return 1;
  }
  _M0Lm3numS322 = _M0L5valueS321;
  _M0L4baseS323 = *(uint32_t*)&_M0L5radixS324;
  _M0Lm5countS325 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1908 = _M0Lm3numS322;
    if (_M0L6_2atmpS1908 > 0u) {
      int32_t _M0L6_2atmpS1909 = _M0Lm5countS325;
      uint32_t _M0L6_2atmpS1910;
      _M0Lm5countS325 = _M0L6_2atmpS1909 + 1;
      _M0L6_2atmpS1910 = _M0Lm3numS322;
      _M0Lm3numS322 = _M0L6_2atmpS1910 / _M0L4baseS323;
      continue;
    }
    break;
  }
  return _M0Lm5countS325;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS319) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS319 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS320;
    int32_t _M0L6_2atmpS1907;
    int32_t _M0L6_2atmpS1906;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS320 = moonbit_clz32(_M0L5valueS319);
    _M0L6_2atmpS1907 = 31 - _M0L14leading__zerosS320;
    _M0L6_2atmpS1906 = _M0L6_2atmpS1907 / 4;
    return _M0L6_2atmpS1906 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS318) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS318 >= 100000u) {
    if (_M0L5valueS318 >= 10000000u) {
      if (_M0L5valueS318 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS318 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS318 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS318 >= 1000u) {
    if (_M0L5valueS318 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS318 >= 100u) {
    return 3;
  } else if (_M0L5valueS318 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS308,
  uint32_t _M0L3numS296,
  int32_t _M0L12digit__startS299,
  int32_t _M0L10total__lenS298
) {
  uint32_t _M0Lm3numS295;
  int32_t _M0Lm6offsetS297;
  uint32_t _M0L6_2atmpS1905;
  int32_t _M0Lm9remainingS310;
  int32_t _M0L6_2atmpS1886;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS295 = _M0L3numS296;
  _M0Lm6offsetS297 = _M0L10total__lenS298 - _M0L12digit__startS299;
  while (1) {
    uint32_t _M0L6_2atmpS1849 = _M0Lm3numS295;
    if (_M0L6_2atmpS1849 >= 10000u) {
      uint32_t _M0L6_2atmpS1872 = _M0Lm3numS295;
      uint32_t _M0L1tS300 = _M0L6_2atmpS1872 / 10000u;
      uint32_t _M0L6_2atmpS1871 = _M0Lm3numS295;
      uint32_t _M0L6_2atmpS1870 = _M0L6_2atmpS1871 % 10000u;
      int32_t _M0L1rS301 = *(int32_t*)&_M0L6_2atmpS1870;
      int32_t _M0L2d1S302;
      int32_t _M0L2d2S303;
      int32_t _M0L6_2atmpS1850;
      int32_t _M0L6_2atmpS1869;
      int32_t _M0L6_2atmpS1868;
      int32_t _M0L6d1__hiS304;
      int32_t _M0L6_2atmpS1867;
      int32_t _M0L6_2atmpS1866;
      int32_t _M0L6d1__loS305;
      int32_t _M0L6_2atmpS1865;
      int32_t _M0L6_2atmpS1864;
      int32_t _M0L6d2__hiS306;
      int32_t _M0L6_2atmpS1863;
      int32_t _M0L6_2atmpS1862;
      int32_t _M0L6d2__loS307;
      int32_t _M0L6_2atmpS1852;
      int32_t _M0L6_2atmpS1851;
      int32_t _M0L6_2atmpS1855;
      int32_t _M0L6_2atmpS1854;
      int32_t _M0L6_2atmpS1853;
      int32_t _M0L6_2atmpS1858;
      int32_t _M0L6_2atmpS1857;
      int32_t _M0L6_2atmpS1856;
      int32_t _M0L6_2atmpS1861;
      int32_t _M0L6_2atmpS1860;
      int32_t _M0L6_2atmpS1859;
      _M0Lm3numS295 = _M0L1tS300;
      _M0L2d1S302 = _M0L1rS301 / 100;
      _M0L2d2S303 = _M0L1rS301 % 100;
      _M0L6_2atmpS1850 = _M0Lm6offsetS297;
      _M0Lm6offsetS297 = _M0L6_2atmpS1850 - 4;
      _M0L6_2atmpS1869 = _M0L2d1S302 / 10;
      _M0L6_2atmpS1868 = 48 + _M0L6_2atmpS1869;
      _M0L6d1__hiS304 = (uint16_t)_M0L6_2atmpS1868;
      _M0L6_2atmpS1867 = _M0L2d1S302 % 10;
      _M0L6_2atmpS1866 = 48 + _M0L6_2atmpS1867;
      _M0L6d1__loS305 = (uint16_t)_M0L6_2atmpS1866;
      _M0L6_2atmpS1865 = _M0L2d2S303 / 10;
      _M0L6_2atmpS1864 = 48 + _M0L6_2atmpS1865;
      _M0L6d2__hiS306 = (uint16_t)_M0L6_2atmpS1864;
      _M0L6_2atmpS1863 = _M0L2d2S303 % 10;
      _M0L6_2atmpS1862 = 48 + _M0L6_2atmpS1863;
      _M0L6d2__loS307 = (uint16_t)_M0L6_2atmpS1862;
      _M0L6_2atmpS1852 = _M0Lm6offsetS297;
      _M0L6_2atmpS1851 = _M0L12digit__startS299 + _M0L6_2atmpS1852;
      _M0L6bufferS308[_M0L6_2atmpS1851] = _M0L6d1__hiS304;
      _M0L6_2atmpS1855 = _M0Lm6offsetS297;
      _M0L6_2atmpS1854 = _M0L12digit__startS299 + _M0L6_2atmpS1855;
      _M0L6_2atmpS1853 = _M0L6_2atmpS1854 + 1;
      _M0L6bufferS308[_M0L6_2atmpS1853] = _M0L6d1__loS305;
      _M0L6_2atmpS1858 = _M0Lm6offsetS297;
      _M0L6_2atmpS1857 = _M0L12digit__startS299 + _M0L6_2atmpS1858;
      _M0L6_2atmpS1856 = _M0L6_2atmpS1857 + 2;
      _M0L6bufferS308[_M0L6_2atmpS1856] = _M0L6d2__hiS306;
      _M0L6_2atmpS1861 = _M0Lm6offsetS297;
      _M0L6_2atmpS1860 = _M0L12digit__startS299 + _M0L6_2atmpS1861;
      _M0L6_2atmpS1859 = _M0L6_2atmpS1860 + 3;
      _M0L6bufferS308[_M0L6_2atmpS1859] = _M0L6d2__loS307;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1905 = _M0Lm3numS295;
  _M0Lm9remainingS310 = *(int32_t*)&_M0L6_2atmpS1905;
  while (1) {
    int32_t _M0L6_2atmpS1873 = _M0Lm9remainingS310;
    if (_M0L6_2atmpS1873 >= 100) {
      int32_t _M0L6_2atmpS1885 = _M0Lm9remainingS310;
      int32_t _M0L1tS311 = _M0L6_2atmpS1885 / 100;
      int32_t _M0L6_2atmpS1884 = _M0Lm9remainingS310;
      int32_t _M0L1dS312 = _M0L6_2atmpS1884 % 100;
      int32_t _M0L6_2atmpS1874;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L5d__hiS313;
      int32_t _M0L6_2atmpS1881;
      int32_t _M0L6_2atmpS1880;
      int32_t _M0L5d__loS314;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6_2atmpS1875;
      int32_t _M0L6_2atmpS1879;
      int32_t _M0L6_2atmpS1878;
      int32_t _M0L6_2atmpS1877;
      _M0Lm9remainingS310 = _M0L1tS311;
      _M0L6_2atmpS1874 = _M0Lm6offsetS297;
      _M0Lm6offsetS297 = _M0L6_2atmpS1874 - 2;
      _M0L6_2atmpS1883 = _M0L1dS312 / 10;
      _M0L6_2atmpS1882 = 48 + _M0L6_2atmpS1883;
      _M0L5d__hiS313 = (uint16_t)_M0L6_2atmpS1882;
      _M0L6_2atmpS1881 = _M0L1dS312 % 10;
      _M0L6_2atmpS1880 = 48 + _M0L6_2atmpS1881;
      _M0L5d__loS314 = (uint16_t)_M0L6_2atmpS1880;
      _M0L6_2atmpS1876 = _M0Lm6offsetS297;
      _M0L6_2atmpS1875 = _M0L12digit__startS299 + _M0L6_2atmpS1876;
      _M0L6bufferS308[_M0L6_2atmpS1875] = _M0L5d__hiS313;
      _M0L6_2atmpS1879 = _M0Lm6offsetS297;
      _M0L6_2atmpS1878 = _M0L12digit__startS299 + _M0L6_2atmpS1879;
      _M0L6_2atmpS1877 = _M0L6_2atmpS1878 + 1;
      _M0L6bufferS308[_M0L6_2atmpS1877] = _M0L5d__loS314;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1886 = _M0Lm9remainingS310;
  if (_M0L6_2atmpS1886 >= 10) {
    int32_t _M0L6_2atmpS1887 = _M0Lm6offsetS297;
    int32_t _M0L6_2atmpS1898;
    int32_t _M0L6_2atmpS1897;
    int32_t _M0L6_2atmpS1896;
    int32_t _M0L5d__hiS316;
    int32_t _M0L6_2atmpS1895;
    int32_t _M0L6_2atmpS1894;
    int32_t _M0L6_2atmpS1893;
    int32_t _M0L5d__loS317;
    int32_t _M0L6_2atmpS1889;
    int32_t _M0L6_2atmpS1888;
    int32_t _M0L6_2atmpS1892;
    int32_t _M0L6_2atmpS1891;
    int32_t _M0L6_2atmpS1890;
    _M0Lm6offsetS297 = _M0L6_2atmpS1887 - 2;
    _M0L6_2atmpS1898 = _M0Lm9remainingS310;
    _M0L6_2atmpS1897 = _M0L6_2atmpS1898 / 10;
    _M0L6_2atmpS1896 = 48 + _M0L6_2atmpS1897;
    _M0L5d__hiS316 = (uint16_t)_M0L6_2atmpS1896;
    _M0L6_2atmpS1895 = _M0Lm9remainingS310;
    _M0L6_2atmpS1894 = _M0L6_2atmpS1895 % 10;
    _M0L6_2atmpS1893 = 48 + _M0L6_2atmpS1894;
    _M0L5d__loS317 = (uint16_t)_M0L6_2atmpS1893;
    _M0L6_2atmpS1889 = _M0Lm6offsetS297;
    _M0L6_2atmpS1888 = _M0L12digit__startS299 + _M0L6_2atmpS1889;
    _M0L6bufferS308[_M0L6_2atmpS1888] = _M0L5d__hiS316;
    _M0L6_2atmpS1892 = _M0Lm6offsetS297;
    _M0L6_2atmpS1891 = _M0L12digit__startS299 + _M0L6_2atmpS1892;
    _M0L6_2atmpS1890 = _M0L6_2atmpS1891 + 1;
    _M0L6bufferS308[_M0L6_2atmpS1890] = _M0L5d__loS317;
    moonbit_decref(_M0L6bufferS308);
  } else {
    int32_t _M0L6_2atmpS1899 = _M0Lm6offsetS297;
    int32_t _M0L6_2atmpS1904;
    int32_t _M0L6_2atmpS1900;
    int32_t _M0L6_2atmpS1903;
    int32_t _M0L6_2atmpS1902;
    int32_t _M0L6_2atmpS1901;
    _M0Lm6offsetS297 = _M0L6_2atmpS1899 - 1;
    _M0L6_2atmpS1904 = _M0Lm6offsetS297;
    _M0L6_2atmpS1900 = _M0L12digit__startS299 + _M0L6_2atmpS1904;
    _M0L6_2atmpS1903 = _M0Lm9remainingS310;
    _M0L6_2atmpS1902 = 48 + _M0L6_2atmpS1903;
    _M0L6_2atmpS1901 = (uint16_t)_M0L6_2atmpS1902;
    _M0L6bufferS308[_M0L6_2atmpS1900] = _M0L6_2atmpS1901;
    moonbit_decref(_M0L6bufferS308);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS290,
  uint32_t _M0L3numS284,
  int32_t _M0L12digit__startS282,
  int32_t _M0L10total__lenS281,
  int32_t _M0L5radixS286
) {
  int32_t _M0Lm6offsetS280;
  uint32_t _M0Lm1nS283;
  uint32_t _M0L4baseS285;
  int32_t _M0L6_2atmpS1831;
  int32_t _M0L6_2atmpS1830;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS280 = _M0L10total__lenS281 - _M0L12digit__startS282;
  _M0Lm1nS283 = _M0L3numS284;
  _M0L4baseS285 = *(uint32_t*)&_M0L5radixS286;
  _M0L6_2atmpS1831 = _M0L5radixS286 - 1;
  _M0L6_2atmpS1830 = _M0L5radixS286 & _M0L6_2atmpS1831;
  if (_M0L6_2atmpS1830 == 0) {
    int32_t _M0L5shiftS287;
    uint32_t _M0L4maskS288;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS287 = moonbit_ctz32(_M0L5radixS286);
    _M0L4maskS288 = _M0L4baseS285 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1832 = _M0Lm1nS283;
      if (_M0L6_2atmpS1832 > 0u) {
        int32_t _M0L6_2atmpS1833 = _M0Lm6offsetS280;
        uint32_t _M0L6_2atmpS1839;
        uint32_t _M0L6_2atmpS1838;
        int32_t _M0L5digitS289;
        int32_t _M0L6_2atmpS1836;
        int32_t _M0L6_2atmpS1834;
        int32_t _M0L6_2atmpS1835;
        uint32_t _M0L6_2atmpS1837;
        _M0Lm6offsetS280 = _M0L6_2atmpS1833 - 1;
        _M0L6_2atmpS1839 = _M0Lm1nS283;
        _M0L6_2atmpS1838 = _M0L6_2atmpS1839 & _M0L4maskS288;
        _M0L5digitS289 = *(int32_t*)&_M0L6_2atmpS1838;
        _M0L6_2atmpS1836 = _M0Lm6offsetS280;
        _M0L6_2atmpS1834 = _M0L12digit__startS282 + _M0L6_2atmpS1836;
        _M0L6_2atmpS1835
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS289
        ];
        _M0L6bufferS290[_M0L6_2atmpS1834] = _M0L6_2atmpS1835;
        _M0L6_2atmpS1837 = _M0Lm1nS283;
        _M0Lm1nS283 = _M0L6_2atmpS1837 >> (_M0L5shiftS287 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS290);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1840 = _M0Lm1nS283;
      if (_M0L6_2atmpS1840 > 0u) {
        int32_t _M0L6_2atmpS1841 = _M0Lm6offsetS280;
        uint32_t _M0L6_2atmpS1848;
        uint32_t _M0L1qS292;
        uint32_t _M0L6_2atmpS1846;
        uint32_t _M0L6_2atmpS1847;
        uint32_t _M0L6_2atmpS1845;
        int32_t _M0L5digitS293;
        int32_t _M0L6_2atmpS1844;
        int32_t _M0L6_2atmpS1842;
        int32_t _M0L6_2atmpS1843;
        _M0Lm6offsetS280 = _M0L6_2atmpS1841 - 1;
        _M0L6_2atmpS1848 = _M0Lm1nS283;
        _M0L1qS292 = _M0L6_2atmpS1848 / _M0L4baseS285;
        _M0L6_2atmpS1846 = _M0Lm1nS283;
        _M0L6_2atmpS1847 = _M0L1qS292 * _M0L4baseS285;
        _M0L6_2atmpS1845 = _M0L6_2atmpS1846 - _M0L6_2atmpS1847;
        _M0L5digitS293 = *(int32_t*)&_M0L6_2atmpS1845;
        _M0L6_2atmpS1844 = _M0Lm6offsetS280;
        _M0L6_2atmpS1842 = _M0L12digit__startS282 + _M0L6_2atmpS1844;
        _M0L6_2atmpS1843
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS293
        ];
        _M0L6bufferS290[_M0L6_2atmpS1842] = _M0L6_2atmpS1843;
        _M0Lm1nS283 = _M0L1qS292;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS290);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS277,
  uint32_t _M0L3numS273,
  int32_t _M0L12digit__startS271,
  int32_t _M0L10total__lenS270
) {
  int32_t _M0Lm6offsetS269;
  uint32_t _M0Lm1nS272;
  int32_t _M0L6_2atmpS1826;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS269 = _M0L10total__lenS270 - _M0L12digit__startS271;
  _M0Lm1nS272 = _M0L3numS273;
  while (1) {
    int32_t _M0L6_2atmpS1814 = _M0Lm6offsetS269;
    if (_M0L6_2atmpS1814 >= 2) {
      int32_t _M0L6_2atmpS1815 = _M0Lm6offsetS269;
      uint32_t _M0L6_2atmpS1825;
      uint32_t _M0L6_2atmpS1824;
      int32_t _M0L9byte__valS274;
      int32_t _M0L2hiS275;
      int32_t _M0L2loS276;
      int32_t _M0L6_2atmpS1818;
      int32_t _M0L6_2atmpS1816;
      int32_t _M0L6_2atmpS1817;
      int32_t _M0L6_2atmpS1822;
      int32_t _M0L6_2atmpS1821;
      int32_t _M0L6_2atmpS1819;
      int32_t _M0L6_2atmpS1820;
      uint32_t _M0L6_2atmpS1823;
      _M0Lm6offsetS269 = _M0L6_2atmpS1815 - 2;
      _M0L6_2atmpS1825 = _M0Lm1nS272;
      _M0L6_2atmpS1824 = _M0L6_2atmpS1825 & 255u;
      _M0L9byte__valS274 = *(int32_t*)&_M0L6_2atmpS1824;
      _M0L2hiS275 = _M0L9byte__valS274 / 16;
      _M0L2loS276 = _M0L9byte__valS274 % 16;
      _M0L6_2atmpS1818 = _M0Lm6offsetS269;
      _M0L6_2atmpS1816 = _M0L12digit__startS271 + _M0L6_2atmpS1818;
      _M0L6_2atmpS1817
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2hiS275
      ];
      _M0L6bufferS277[_M0L6_2atmpS1816] = _M0L6_2atmpS1817;
      _M0L6_2atmpS1822 = _M0Lm6offsetS269;
      _M0L6_2atmpS1821 = _M0L12digit__startS271 + _M0L6_2atmpS1822;
      _M0L6_2atmpS1819 = _M0L6_2atmpS1821 + 1;
      _M0L6_2atmpS1820
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS276
      ];
      _M0L6bufferS277[_M0L6_2atmpS1819] = _M0L6_2atmpS1820;
      _M0L6_2atmpS1823 = _M0Lm1nS272;
      _M0Lm1nS272 = _M0L6_2atmpS1823 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1826 = _M0Lm6offsetS269;
  if (_M0L6_2atmpS1826 == 1) {
    uint32_t _M0L6_2atmpS1829 = _M0Lm1nS272;
    uint32_t _M0L6_2atmpS1828 = _M0L6_2atmpS1829 & 15u;
    int32_t _M0L6nibbleS279 = *(int32_t*)&_M0L6_2atmpS1828;
    int32_t _M0L6_2atmpS1827 =
      ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS279];
    _M0L6bufferS277[_M0L12digit__startS271] = _M0L6_2atmpS1827;
    moonbit_decref(_M0L6bufferS277);
  } else {
    moonbit_decref(_M0L6bufferS277);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS264) {
  struct _M0TWEOs* _M0L7_2afuncS263;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS263 = _M0L4selfS264;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS263->code(_M0L7_2afuncS263);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS266
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS265;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS265 = _M0L4selfS266;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS265->code(_M0L7_2afuncS265);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS268) {
  struct _M0TWEOc* _M0L7_2afuncS267;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS267 = _M0L4selfS268;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS267->code(_M0L7_2afuncS267);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS254
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS253;
  struct _M0TPB6Logger _M0L6_2atmpS1809;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS253 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS253);
  _M0L6_2atmpS1809
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS253
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS254, _M0L6_2atmpS1809);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS253);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS256
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS255;
  struct _M0TPB6Logger _M0L6_2atmpS1810;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS255 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS255);
  _M0L6_2atmpS1810
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS255
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS256, _M0L6_2atmpS1810);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS255);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS258
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS257;
  struct _M0TPB6Logger _M0L6_2atmpS1811;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS257 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS257);
  _M0L6_2atmpS1811
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS257
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS258, _M0L6_2atmpS1811);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS257);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS260
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS259;
  struct _M0TPB6Logger _M0L6_2atmpS1812;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS259 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS259);
  _M0L6_2atmpS1812
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS259
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS260, _M0L6_2atmpS1812);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS259);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPC17strconv12StrConvErrorE(
  void* _M0L4selfS262
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS261;
  struct _M0TPB6Logger _M0L6_2atmpS1813;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS261 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS261);
  _M0L6_2atmpS1813
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS261
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC17strconv12StrConvErrorPB4Show6output(_M0L4selfS262, _M0L6_2atmpS1813);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS261);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS252
) {
  int32_t _M0L8_2afieldS3927;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3927 = _M0L4selfS252.$1;
  moonbit_decref(_M0L4selfS252.$0);
  return _M0L8_2afieldS3927;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS251
) {
  int32_t _M0L3endS1807;
  int32_t _M0L8_2afieldS3928;
  int32_t _M0L5startS1808;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1807 = _M0L4selfS251.$2;
  _M0L8_2afieldS3928 = _M0L4selfS251.$1;
  moonbit_decref(_M0L4selfS251.$0);
  _M0L5startS1808 = _M0L8_2afieldS3928;
  return _M0L3endS1807 - _M0L5startS1808;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS250
) {
  moonbit_string_t _M0L8_2afieldS3929;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3929 = _M0L4selfS250.$0;
  return _M0L8_2afieldS3929;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS246,
  moonbit_string_t _M0L5valueS247,
  int32_t _M0L5startS248,
  int32_t _M0L3lenS249
) {
  int32_t _M0L6_2atmpS1806;
  int64_t _M0L6_2atmpS1805;
  struct _M0TPC16string10StringView _M0L6_2atmpS1804;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1806 = _M0L5startS248 + _M0L3lenS249;
  _M0L6_2atmpS1805 = (int64_t)_M0L6_2atmpS1806;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1804
  = _M0MPC16string6String11sub_2einner(_M0L5valueS247, _M0L5startS248, _M0L6_2atmpS1805);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS246, _M0L6_2atmpS1804);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS239,
  int32_t _M0L5startS245,
  int64_t _M0L3endS241
) {
  int32_t _M0L3lenS238;
  int32_t _M0L3endS240;
  int32_t _M0L5startS244;
  int32_t _if__result_4344;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS238 = Moonbit_array_length(_M0L4selfS239);
  if (_M0L3endS241 == 4294967296ll) {
    _M0L3endS240 = _M0L3lenS238;
  } else {
    int64_t _M0L7_2aSomeS242 = _M0L3endS241;
    int32_t _M0L6_2aendS243 = (int32_t)_M0L7_2aSomeS242;
    if (_M0L6_2aendS243 < 0) {
      _M0L3endS240 = _M0L3lenS238 + _M0L6_2aendS243;
    } else {
      _M0L3endS240 = _M0L6_2aendS243;
    }
  }
  if (_M0L5startS245 < 0) {
    _M0L5startS244 = _M0L3lenS238 + _M0L5startS245;
  } else {
    _M0L5startS244 = _M0L5startS245;
  }
  if (_M0L5startS244 >= 0) {
    if (_M0L5startS244 <= _M0L3endS240) {
      _if__result_4344 = _M0L3endS240 <= _M0L3lenS238;
    } else {
      _if__result_4344 = 0;
    }
  } else {
    _if__result_4344 = 0;
  }
  if (_if__result_4344) {
    if (_M0L5startS244 < _M0L3lenS238) {
      int32_t _M0L6_2atmpS1801 = _M0L4selfS239[_M0L5startS244];
      int32_t _M0L6_2atmpS1800;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1800
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1801);
      if (!_M0L6_2atmpS1800) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS240 < _M0L3lenS238) {
      int32_t _M0L6_2atmpS1803 = _M0L4selfS239[_M0L3endS240];
      int32_t _M0L6_2atmpS1802;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1802
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1803);
      if (!_M0L6_2atmpS1802) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS244,
                                                 _M0L3endS240,
                                                 _M0L4selfS239};
  } else {
    moonbit_decref(_M0L4selfS239);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS235) {
  struct _M0TPB6Hasher* _M0L1hS234;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS234 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS234);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS234, _M0L4selfS235);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS234);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS237
) {
  struct _M0TPB6Hasher* _M0L1hS236;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS236 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS236);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS236, _M0L4selfS237);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS236);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS232) {
  int32_t _M0L4seedS231;
  if (_M0L10seed_2eoptS232 == 4294967296ll) {
    _M0L4seedS231 = 0;
  } else {
    int64_t _M0L7_2aSomeS233 = _M0L10seed_2eoptS232;
    _M0L4seedS231 = (int32_t)_M0L7_2aSomeS233;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS231);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS230) {
  uint32_t _M0L6_2atmpS1799;
  uint32_t _M0L6_2atmpS1798;
  struct _M0TPB6Hasher* _block_4345;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1799 = *(uint32_t*)&_M0L4seedS230;
  _M0L6_2atmpS1798 = _M0L6_2atmpS1799 + 374761393u;
  _block_4345
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4345)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4345->$0 = _M0L6_2atmpS1798;
  return _block_4345;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS229) {
  uint32_t _M0L6_2atmpS1797;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1797 = _M0MPB6Hasher9avalanche(_M0L4selfS229);
  return *(int32_t*)&_M0L6_2atmpS1797;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS228) {
  uint32_t _M0L8_2afieldS3930;
  uint32_t _M0Lm3accS227;
  uint32_t _M0L6_2atmpS1786;
  uint32_t _M0L6_2atmpS1788;
  uint32_t _M0L6_2atmpS1787;
  uint32_t _M0L6_2atmpS1789;
  uint32_t _M0L6_2atmpS1790;
  uint32_t _M0L6_2atmpS1792;
  uint32_t _M0L6_2atmpS1791;
  uint32_t _M0L6_2atmpS1793;
  uint32_t _M0L6_2atmpS1794;
  uint32_t _M0L6_2atmpS1796;
  uint32_t _M0L6_2atmpS1795;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3930 = _M0L4selfS228->$0;
  moonbit_decref(_M0L4selfS228);
  _M0Lm3accS227 = _M0L8_2afieldS3930;
  _M0L6_2atmpS1786 = _M0Lm3accS227;
  _M0L6_2atmpS1788 = _M0Lm3accS227;
  _M0L6_2atmpS1787 = _M0L6_2atmpS1788 >> 15;
  _M0Lm3accS227 = _M0L6_2atmpS1786 ^ _M0L6_2atmpS1787;
  _M0L6_2atmpS1789 = _M0Lm3accS227;
  _M0Lm3accS227 = _M0L6_2atmpS1789 * 2246822519u;
  _M0L6_2atmpS1790 = _M0Lm3accS227;
  _M0L6_2atmpS1792 = _M0Lm3accS227;
  _M0L6_2atmpS1791 = _M0L6_2atmpS1792 >> 13;
  _M0Lm3accS227 = _M0L6_2atmpS1790 ^ _M0L6_2atmpS1791;
  _M0L6_2atmpS1793 = _M0Lm3accS227;
  _M0Lm3accS227 = _M0L6_2atmpS1793 * 3266489917u;
  _M0L6_2atmpS1794 = _M0Lm3accS227;
  _M0L6_2atmpS1796 = _M0Lm3accS227;
  _M0L6_2atmpS1795 = _M0L6_2atmpS1796 >> 16;
  _M0Lm3accS227 = _M0L6_2atmpS1794 ^ _M0L6_2atmpS1795;
  return _M0Lm3accS227;
}

int32_t _M0IP016_24default__implPB7Compare6op__gtGRP48clawteam8clawteam8internal9buildinfo7VersionE(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L1xS225,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L1yS226
) {
  int32_t _M0L6_2atmpS1785;
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 51 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1785
  = _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB7Compare7compare(_M0L1xS225, _M0L1yS226);
  return _M0L6_2atmpS1785 > 0;
}

int32_t _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal9buildinfo7VersionE(
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L1xS223,
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L1yS224
) {
  int32_t _M0L6_2atmpS1784;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1784
  = _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB7Compare7compare(_M0L1xS223, _M0L1yS224);
  return _M0L6_2atmpS1784 < 0;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS219,
  moonbit_string_t _M0L1yS220
) {
  int32_t _M0L6_2atmpS3931;
  int32_t _M0L6_2atmpS1782;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3931 = moonbit_val_array_equal(_M0L1xS219, _M0L1yS220);
  moonbit_decref(_M0L1xS219);
  moonbit_decref(_M0L1yS220);
  _M0L6_2atmpS1782 = _M0L6_2atmpS3931;
  return !_M0L6_2atmpS1782;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGRPC16string10StringViewE(
  struct _M0TPC16string10StringView _M0L1xS221,
  struct _M0TPC16string10StringView _M0L1yS222
) {
  int32_t _M0L6_2atmpS1783;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1783
  = _M0IPC16string10StringViewPB2Eq5equal(_M0L1xS221, _M0L1yS222);
  return !_M0L6_2atmpS1783;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS216,
  int32_t _M0L5valueS215
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS215, _M0L4selfS216);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS218,
  moonbit_string_t _M0L5valueS217
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS217, _M0L4selfS218);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS214) {
  int64_t _M0L6_2atmpS1781;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1781 = (int64_t)_M0L4selfS214;
  return *(uint64_t*)&_M0L6_2atmpS1781;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS212,
  moonbit_string_t _M0L4reprS213
) {
  void* _block_4346;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4346 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_4346)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_4346)->$0 = _M0L6numberS212;
  ((struct _M0DTPB4Json6Number*)_block_4346)->$1 = _M0L4reprS213;
  return _block_4346;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS210,
  int32_t _M0L5valueS211
) {
  uint32_t _M0L6_2atmpS1780;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1780 = *(uint32_t*)&_M0L5valueS211;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS210, _M0L6_2atmpS1780);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS203
) {
  struct _M0TPB13StringBuilder* _M0L3bufS201;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS202;
  int32_t _M0L7_2abindS204;
  int32_t _M0L1iS205;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS201 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS202 = _M0L4selfS203;
  moonbit_incref(_M0L3bufS201);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS201, 91);
  _M0L7_2abindS204 = _M0L7_2aselfS202->$1;
  _M0L1iS205 = 0;
  while (1) {
    if (_M0L1iS205 < _M0L7_2abindS204) {
      int32_t _if__result_4348;
      moonbit_string_t* _M0L8_2afieldS3933;
      moonbit_string_t* _M0L3bufS1778;
      moonbit_string_t _M0L6_2atmpS3932;
      moonbit_string_t _M0L4itemS206;
      int32_t _M0L6_2atmpS1779;
      if (_M0L1iS205 != 0) {
        moonbit_incref(_M0L3bufS201);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS201, (moonbit_string_t)moonbit_string_literal_106.data);
      }
      if (_M0L1iS205 < 0) {
        _if__result_4348 = 1;
      } else {
        int32_t _M0L3lenS1777 = _M0L7_2aselfS202->$1;
        _if__result_4348 = _M0L1iS205 >= _M0L3lenS1777;
      }
      if (_if__result_4348) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3933 = _M0L7_2aselfS202->$0;
      _M0L3bufS1778 = _M0L8_2afieldS3933;
      _M0L6_2atmpS3932 = (moonbit_string_t)_M0L3bufS1778[_M0L1iS205];
      _M0L4itemS206 = _M0L6_2atmpS3932;
      if (_M0L4itemS206 == 0) {
        moonbit_incref(_M0L3bufS201);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS201, (moonbit_string_t)moonbit_string_literal_65.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS207 = _M0L4itemS206;
        moonbit_string_t _M0L6_2alocS208 = _M0L7_2aSomeS207;
        moonbit_string_t _M0L6_2atmpS1776;
        moonbit_incref(_M0L6_2alocS208);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1776
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS208);
        moonbit_incref(_M0L3bufS201);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS201, _M0L6_2atmpS1776);
      }
      _M0L6_2atmpS1779 = _M0L1iS205 + 1;
      _M0L1iS205 = _M0L6_2atmpS1779;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS202);
    }
    break;
  }
  moonbit_incref(_M0L3bufS201);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS201, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS201);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS200
) {
  moonbit_string_t _M0L6_2atmpS1775;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1774;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1775 = _M0L4selfS200;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1774 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1775);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1774);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS199
) {
  struct _M0TPB13StringBuilder* _M0L2sbS198;
  struct _M0TPC16string10StringView _M0L8_2afieldS3946;
  struct _M0TPC16string10StringView _M0L3pkgS1759;
  moonbit_string_t _M0L6_2atmpS1758;
  moonbit_string_t _M0L6_2atmpS3945;
  moonbit_string_t _M0L6_2atmpS1757;
  moonbit_string_t _M0L6_2atmpS3944;
  moonbit_string_t _M0L6_2atmpS1756;
  struct _M0TPC16string10StringView _M0L8_2afieldS3943;
  struct _M0TPC16string10StringView _M0L8filenameS1760;
  struct _M0TPC16string10StringView _M0L8_2afieldS3942;
  struct _M0TPC16string10StringView _M0L11start__lineS1763;
  moonbit_string_t _M0L6_2atmpS1762;
  moonbit_string_t _M0L6_2atmpS3941;
  moonbit_string_t _M0L6_2atmpS1761;
  struct _M0TPC16string10StringView _M0L8_2afieldS3940;
  struct _M0TPC16string10StringView _M0L13start__columnS1766;
  moonbit_string_t _M0L6_2atmpS1765;
  moonbit_string_t _M0L6_2atmpS3939;
  moonbit_string_t _M0L6_2atmpS1764;
  struct _M0TPC16string10StringView _M0L8_2afieldS3938;
  struct _M0TPC16string10StringView _M0L9end__lineS1769;
  moonbit_string_t _M0L6_2atmpS1768;
  moonbit_string_t _M0L6_2atmpS3937;
  moonbit_string_t _M0L6_2atmpS1767;
  struct _M0TPC16string10StringView _M0L8_2afieldS3936;
  int32_t _M0L6_2acntS4083;
  struct _M0TPC16string10StringView _M0L11end__columnS1773;
  moonbit_string_t _M0L6_2atmpS1772;
  moonbit_string_t _M0L6_2atmpS3935;
  moonbit_string_t _M0L6_2atmpS1771;
  moonbit_string_t _M0L6_2atmpS3934;
  moonbit_string_t _M0L6_2atmpS1770;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS198 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3946
  = (struct _M0TPC16string10StringView){
    _M0L4selfS199->$0_1, _M0L4selfS199->$0_2, _M0L4selfS199->$0_0
  };
  _M0L3pkgS1759 = _M0L8_2afieldS3946;
  moonbit_incref(_M0L3pkgS1759.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1758
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1759);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3945
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_107.data, _M0L6_2atmpS1758);
  moonbit_decref(_M0L6_2atmpS1758);
  _M0L6_2atmpS1757 = _M0L6_2atmpS3945;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3944
  = moonbit_add_string(_M0L6_2atmpS1757, (moonbit_string_t)moonbit_string_literal_108.data);
  moonbit_decref(_M0L6_2atmpS1757);
  _M0L6_2atmpS1756 = _M0L6_2atmpS3944;
  moonbit_incref(_M0L2sbS198);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS198, _M0L6_2atmpS1756);
  moonbit_incref(_M0L2sbS198);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS198, (moonbit_string_t)moonbit_string_literal_109.data);
  _M0L8_2afieldS3943
  = (struct _M0TPC16string10StringView){
    _M0L4selfS199->$1_1, _M0L4selfS199->$1_2, _M0L4selfS199->$1_0
  };
  _M0L8filenameS1760 = _M0L8_2afieldS3943;
  moonbit_incref(_M0L8filenameS1760.$0);
  moonbit_incref(_M0L2sbS198);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS198, _M0L8filenameS1760);
  _M0L8_2afieldS3942
  = (struct _M0TPC16string10StringView){
    _M0L4selfS199->$2_1, _M0L4selfS199->$2_2, _M0L4selfS199->$2_0
  };
  _M0L11start__lineS1763 = _M0L8_2afieldS3942;
  moonbit_incref(_M0L11start__lineS1763.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1762
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1763);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3941
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_110.data, _M0L6_2atmpS1762);
  moonbit_decref(_M0L6_2atmpS1762);
  _M0L6_2atmpS1761 = _M0L6_2atmpS3941;
  moonbit_incref(_M0L2sbS198);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS198, _M0L6_2atmpS1761);
  _M0L8_2afieldS3940
  = (struct _M0TPC16string10StringView){
    _M0L4selfS199->$3_1, _M0L4selfS199->$3_2, _M0L4selfS199->$3_0
  };
  _M0L13start__columnS1766 = _M0L8_2afieldS3940;
  moonbit_incref(_M0L13start__columnS1766.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1765
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1766);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3939
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_111.data, _M0L6_2atmpS1765);
  moonbit_decref(_M0L6_2atmpS1765);
  _M0L6_2atmpS1764 = _M0L6_2atmpS3939;
  moonbit_incref(_M0L2sbS198);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS198, _M0L6_2atmpS1764);
  _M0L8_2afieldS3938
  = (struct _M0TPC16string10StringView){
    _M0L4selfS199->$4_1, _M0L4selfS199->$4_2, _M0L4selfS199->$4_0
  };
  _M0L9end__lineS1769 = _M0L8_2afieldS3938;
  moonbit_incref(_M0L9end__lineS1769.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1768
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1769);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3937
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_112.data, _M0L6_2atmpS1768);
  moonbit_decref(_M0L6_2atmpS1768);
  _M0L6_2atmpS1767 = _M0L6_2atmpS3937;
  moonbit_incref(_M0L2sbS198);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS198, _M0L6_2atmpS1767);
  _M0L8_2afieldS3936
  = (struct _M0TPC16string10StringView){
    _M0L4selfS199->$5_1, _M0L4selfS199->$5_2, _M0L4selfS199->$5_0
  };
  _M0L6_2acntS4083 = Moonbit_object_header(_M0L4selfS199)->rc;
  if (_M0L6_2acntS4083 > 1) {
    int32_t _M0L11_2anew__cntS4089 = _M0L6_2acntS4083 - 1;
    Moonbit_object_header(_M0L4selfS199)->rc = _M0L11_2anew__cntS4089;
    moonbit_incref(_M0L8_2afieldS3936.$0);
  } else if (_M0L6_2acntS4083 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4088 =
      (struct _M0TPC16string10StringView){_M0L4selfS199->$4_1,
                                            _M0L4selfS199->$4_2,
                                            _M0L4selfS199->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4087;
    struct _M0TPC16string10StringView _M0L8_2afieldS4086;
    struct _M0TPC16string10StringView _M0L8_2afieldS4085;
    struct _M0TPC16string10StringView _M0L8_2afieldS4084;
    moonbit_decref(_M0L8_2afieldS4088.$0);
    _M0L8_2afieldS4087
    = (struct _M0TPC16string10StringView){
      _M0L4selfS199->$3_1, _M0L4selfS199->$3_2, _M0L4selfS199->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4087.$0);
    _M0L8_2afieldS4086
    = (struct _M0TPC16string10StringView){
      _M0L4selfS199->$2_1, _M0L4selfS199->$2_2, _M0L4selfS199->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4086.$0);
    _M0L8_2afieldS4085
    = (struct _M0TPC16string10StringView){
      _M0L4selfS199->$1_1, _M0L4selfS199->$1_2, _M0L4selfS199->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4085.$0);
    _M0L8_2afieldS4084
    = (struct _M0TPC16string10StringView){
      _M0L4selfS199->$0_1, _M0L4selfS199->$0_2, _M0L4selfS199->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4084.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS199);
  }
  _M0L11end__columnS1773 = _M0L8_2afieldS3936;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1772
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1773);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3935
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_113.data, _M0L6_2atmpS1772);
  moonbit_decref(_M0L6_2atmpS1772);
  _M0L6_2atmpS1771 = _M0L6_2atmpS3935;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3934
  = moonbit_add_string(_M0L6_2atmpS1771, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS1771);
  _M0L6_2atmpS1770 = _M0L6_2atmpS3934;
  moonbit_incref(_M0L2sbS198);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS198, _M0L6_2atmpS1770);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS198);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS196,
  moonbit_string_t _M0L3strS197
) {
  int32_t _M0L3lenS1746;
  int32_t _M0L6_2atmpS1748;
  int32_t _M0L6_2atmpS1747;
  int32_t _M0L6_2atmpS1745;
  moonbit_bytes_t _M0L8_2afieldS3948;
  moonbit_bytes_t _M0L4dataS1749;
  int32_t _M0L3lenS1750;
  int32_t _M0L6_2atmpS1751;
  int32_t _M0L3lenS1753;
  int32_t _M0L6_2atmpS3947;
  int32_t _M0L6_2atmpS1755;
  int32_t _M0L6_2atmpS1754;
  int32_t _M0L6_2atmpS1752;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1746 = _M0L4selfS196->$1;
  _M0L6_2atmpS1748 = Moonbit_array_length(_M0L3strS197);
  _M0L6_2atmpS1747 = _M0L6_2atmpS1748 * 2;
  _M0L6_2atmpS1745 = _M0L3lenS1746 + _M0L6_2atmpS1747;
  moonbit_incref(_M0L4selfS196);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS196, _M0L6_2atmpS1745);
  _M0L8_2afieldS3948 = _M0L4selfS196->$0;
  _M0L4dataS1749 = _M0L8_2afieldS3948;
  _M0L3lenS1750 = _M0L4selfS196->$1;
  _M0L6_2atmpS1751 = Moonbit_array_length(_M0L3strS197);
  moonbit_incref(_M0L4dataS1749);
  moonbit_incref(_M0L3strS197);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1749, _M0L3lenS1750, _M0L3strS197, 0, _M0L6_2atmpS1751);
  _M0L3lenS1753 = _M0L4selfS196->$1;
  _M0L6_2atmpS3947 = Moonbit_array_length(_M0L3strS197);
  moonbit_decref(_M0L3strS197);
  _M0L6_2atmpS1755 = _M0L6_2atmpS3947;
  _M0L6_2atmpS1754 = _M0L6_2atmpS1755 * 2;
  _M0L6_2atmpS1752 = _M0L3lenS1753 + _M0L6_2atmpS1754;
  _M0L4selfS196->$1 = _M0L6_2atmpS1752;
  moonbit_decref(_M0L4selfS196);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS188,
  int32_t _M0L13bytes__offsetS183,
  moonbit_string_t _M0L3strS190,
  int32_t _M0L11str__offsetS186,
  int32_t _M0L6lengthS184
) {
  int32_t _M0L6_2atmpS1744;
  int32_t _M0L6_2atmpS1743;
  int32_t _M0L2e1S182;
  int32_t _M0L6_2atmpS1742;
  int32_t _M0L2e2S185;
  int32_t _M0L4len1S187;
  int32_t _M0L4len2S189;
  int32_t _if__result_4349;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1744 = _M0L6lengthS184 * 2;
  _M0L6_2atmpS1743 = _M0L13bytes__offsetS183 + _M0L6_2atmpS1744;
  _M0L2e1S182 = _M0L6_2atmpS1743 - 1;
  _M0L6_2atmpS1742 = _M0L11str__offsetS186 + _M0L6lengthS184;
  _M0L2e2S185 = _M0L6_2atmpS1742 - 1;
  _M0L4len1S187 = Moonbit_array_length(_M0L4selfS188);
  _M0L4len2S189 = Moonbit_array_length(_M0L3strS190);
  if (_M0L6lengthS184 >= 0) {
    if (_M0L13bytes__offsetS183 >= 0) {
      if (_M0L2e1S182 < _M0L4len1S187) {
        if (_M0L11str__offsetS186 >= 0) {
          _if__result_4349 = _M0L2e2S185 < _M0L4len2S189;
        } else {
          _if__result_4349 = 0;
        }
      } else {
        _if__result_4349 = 0;
      }
    } else {
      _if__result_4349 = 0;
    }
  } else {
    _if__result_4349 = 0;
  }
  if (_if__result_4349) {
    int32_t _M0L16end__str__offsetS191 =
      _M0L11str__offsetS186 + _M0L6lengthS184;
    int32_t _M0L1iS192 = _M0L11str__offsetS186;
    int32_t _M0L1jS193 = _M0L13bytes__offsetS183;
    while (1) {
      if (_M0L1iS192 < _M0L16end__str__offsetS191) {
        int32_t _M0L6_2atmpS1739 = _M0L3strS190[_M0L1iS192];
        int32_t _M0L6_2atmpS1738 = (int32_t)_M0L6_2atmpS1739;
        uint32_t _M0L1cS194 = *(uint32_t*)&_M0L6_2atmpS1738;
        uint32_t _M0L6_2atmpS1734 = _M0L1cS194 & 255u;
        int32_t _M0L6_2atmpS1733;
        int32_t _M0L6_2atmpS1735;
        uint32_t _M0L6_2atmpS1737;
        int32_t _M0L6_2atmpS1736;
        int32_t _M0L6_2atmpS1740;
        int32_t _M0L6_2atmpS1741;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1733 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1734);
        if (
          _M0L1jS193 < 0 || _M0L1jS193 >= Moonbit_array_length(_M0L4selfS188)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS188[_M0L1jS193] = _M0L6_2atmpS1733;
        _M0L6_2atmpS1735 = _M0L1jS193 + 1;
        _M0L6_2atmpS1737 = _M0L1cS194 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1736 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1737);
        if (
          _M0L6_2atmpS1735 < 0
          || _M0L6_2atmpS1735 >= Moonbit_array_length(_M0L4selfS188)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS188[_M0L6_2atmpS1735] = _M0L6_2atmpS1736;
        _M0L6_2atmpS1740 = _M0L1iS192 + 1;
        _M0L6_2atmpS1741 = _M0L1jS193 + 2;
        _M0L1iS192 = _M0L6_2atmpS1740;
        _M0L1jS193 = _M0L6_2atmpS1741;
        continue;
      } else {
        moonbit_decref(_M0L3strS190);
        moonbit_decref(_M0L4selfS188);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS190);
    moonbit_decref(_M0L4selfS188);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS179,
  double _M0L3objS178
) {
  struct _M0TPB6Logger _M0L6_2atmpS1731;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1731
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS179
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS178, _M0L6_2atmpS1731);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS181,
  struct _M0TPC16string10StringView _M0L3objS180
) {
  struct _M0TPB6Logger _M0L6_2atmpS1732;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1732
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS181
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS180, _M0L6_2atmpS1732);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS124
) {
  int32_t _M0L6_2atmpS1730;
  struct _M0TPC16string10StringView _M0L7_2abindS123;
  moonbit_string_t _M0L7_2adataS125;
  int32_t _M0L8_2astartS126;
  int32_t _M0L6_2atmpS1729;
  int32_t _M0L6_2aendS127;
  int32_t _M0Lm9_2acursorS128;
  int32_t _M0Lm13accept__stateS129;
  int32_t _M0Lm10match__endS130;
  int32_t _M0Lm20match__tag__saver__0S131;
  int32_t _M0Lm20match__tag__saver__1S132;
  int32_t _M0Lm20match__tag__saver__2S133;
  int32_t _M0Lm20match__tag__saver__3S134;
  int32_t _M0Lm20match__tag__saver__4S135;
  int32_t _M0Lm6tag__0S136;
  int32_t _M0Lm6tag__1S137;
  int32_t _M0Lm9tag__1__1S138;
  int32_t _M0Lm9tag__1__2S139;
  int32_t _M0Lm6tag__3S140;
  int32_t _M0Lm6tag__2S141;
  int32_t _M0Lm9tag__2__1S142;
  int32_t _M0Lm6tag__4S143;
  int32_t _M0L6_2atmpS1687;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1730 = Moonbit_array_length(_M0L4reprS124);
  _M0L7_2abindS123
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1730, _M0L4reprS124
  };
  moonbit_incref(_M0L7_2abindS123.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS125 = _M0MPC16string10StringView4data(_M0L7_2abindS123);
  moonbit_incref(_M0L7_2abindS123.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS126
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS123);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1729 = _M0MPC16string10StringView6length(_M0L7_2abindS123);
  _M0L6_2aendS127 = _M0L8_2astartS126 + _M0L6_2atmpS1729;
  _M0Lm9_2acursorS128 = _M0L8_2astartS126;
  _M0Lm13accept__stateS129 = -1;
  _M0Lm10match__endS130 = -1;
  _M0Lm20match__tag__saver__0S131 = -1;
  _M0Lm20match__tag__saver__1S132 = -1;
  _M0Lm20match__tag__saver__2S133 = -1;
  _M0Lm20match__tag__saver__3S134 = -1;
  _M0Lm20match__tag__saver__4S135 = -1;
  _M0Lm6tag__0S136 = -1;
  _M0Lm6tag__1S137 = -1;
  _M0Lm9tag__1__1S138 = -1;
  _M0Lm9tag__1__2S139 = -1;
  _M0Lm6tag__3S140 = -1;
  _M0Lm6tag__2S141 = -1;
  _M0Lm9tag__2__1S142 = -1;
  _M0Lm6tag__4S143 = -1;
  _M0L6_2atmpS1687 = _M0Lm9_2acursorS128;
  if (_M0L6_2atmpS1687 < _M0L6_2aendS127) {
    int32_t _M0L6_2atmpS1689 = _M0Lm9_2acursorS128;
    int32_t _M0L6_2atmpS1688;
    moonbit_incref(_M0L7_2adataS125);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1688
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1689);
    if (_M0L6_2atmpS1688 == 64) {
      int32_t _M0L6_2atmpS1690 = _M0Lm9_2acursorS128;
      _M0Lm9_2acursorS128 = _M0L6_2atmpS1690 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1691;
        _M0Lm6tag__0S136 = _M0Lm9_2acursorS128;
        _M0L6_2atmpS1691 = _M0Lm9_2acursorS128;
        if (_M0L6_2atmpS1691 < _M0L6_2aendS127) {
          int32_t _M0L6_2atmpS1728 = _M0Lm9_2acursorS128;
          int32_t _M0L10next__charS151;
          int32_t _M0L6_2atmpS1692;
          moonbit_incref(_M0L7_2adataS125);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS151
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1728);
          _M0L6_2atmpS1692 = _M0Lm9_2acursorS128;
          _M0Lm9_2acursorS128 = _M0L6_2atmpS1692 + 1;
          if (_M0L10next__charS151 == 58) {
            int32_t _M0L6_2atmpS1693 = _M0Lm9_2acursorS128;
            if (_M0L6_2atmpS1693 < _M0L6_2aendS127) {
              int32_t _M0L6_2atmpS1694 = _M0Lm9_2acursorS128;
              int32_t _M0L12dispatch__15S152;
              _M0Lm9_2acursorS128 = _M0L6_2atmpS1694 + 1;
              _M0L12dispatch__15S152 = 0;
              loop__label__15_155:;
              while (1) {
                int32_t _M0L6_2atmpS1695;
                switch (_M0L12dispatch__15S152) {
                  case 3: {
                    int32_t _M0L6_2atmpS1698;
                    _M0Lm9tag__1__2S139 = _M0Lm9tag__1__1S138;
                    _M0Lm9tag__1__1S138 = _M0Lm6tag__1S137;
                    _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                    _M0L6_2atmpS1698 = _M0Lm9_2acursorS128;
                    if (_M0L6_2atmpS1698 < _M0L6_2aendS127) {
                      int32_t _M0L6_2atmpS1703 = _M0Lm9_2acursorS128;
                      int32_t _M0L10next__charS159;
                      int32_t _M0L6_2atmpS1699;
                      moonbit_incref(_M0L7_2adataS125);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS159
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1703);
                      _M0L6_2atmpS1699 = _M0Lm9_2acursorS128;
                      _M0Lm9_2acursorS128 = _M0L6_2atmpS1699 + 1;
                      if (_M0L10next__charS159 < 58) {
                        if (_M0L10next__charS159 < 48) {
                          goto join_158;
                        } else {
                          int32_t _M0L6_2atmpS1700;
                          _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                          _M0Lm9tag__2__1S142 = _M0Lm6tag__2S141;
                          _M0Lm6tag__2S141 = _M0Lm9_2acursorS128;
                          _M0Lm6tag__3S140 = _M0Lm9_2acursorS128;
                          _M0L6_2atmpS1700 = _M0Lm9_2acursorS128;
                          if (_M0L6_2atmpS1700 < _M0L6_2aendS127) {
                            int32_t _M0L6_2atmpS1702 = _M0Lm9_2acursorS128;
                            int32_t _M0L10next__charS161;
                            int32_t _M0L6_2atmpS1701;
                            moonbit_incref(_M0L7_2adataS125);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS161
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1702);
                            _M0L6_2atmpS1701 = _M0Lm9_2acursorS128;
                            _M0Lm9_2acursorS128 = _M0L6_2atmpS1701 + 1;
                            if (_M0L10next__charS161 < 48) {
                              if (_M0L10next__charS161 == 45) {
                                goto join_153;
                              } else {
                                goto join_160;
                              }
                            } else if (_M0L10next__charS161 > 57) {
                              if (_M0L10next__charS161 < 59) {
                                _M0L12dispatch__15S152 = 3;
                                goto loop__label__15_155;
                              } else {
                                goto join_160;
                              }
                            } else {
                              _M0L12dispatch__15S152 = 6;
                              goto loop__label__15_155;
                            }
                            join_160:;
                            _M0L12dispatch__15S152 = 0;
                            goto loop__label__15_155;
                          } else {
                            goto join_144;
                          }
                        }
                      } else if (_M0L10next__charS159 > 58) {
                        goto join_158;
                      } else {
                        _M0L12dispatch__15S152 = 1;
                        goto loop__label__15_155;
                      }
                      join_158:;
                      _M0L12dispatch__15S152 = 0;
                      goto loop__label__15_155;
                    } else {
                      goto join_144;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1704;
                    _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                    _M0Lm6tag__2S141 = _M0Lm9_2acursorS128;
                    _M0L6_2atmpS1704 = _M0Lm9_2acursorS128;
                    if (_M0L6_2atmpS1704 < _M0L6_2aendS127) {
                      int32_t _M0L6_2atmpS1706 = _M0Lm9_2acursorS128;
                      int32_t _M0L10next__charS163;
                      int32_t _M0L6_2atmpS1705;
                      moonbit_incref(_M0L7_2adataS125);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS163
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1706);
                      _M0L6_2atmpS1705 = _M0Lm9_2acursorS128;
                      _M0Lm9_2acursorS128 = _M0L6_2atmpS1705 + 1;
                      if (_M0L10next__charS163 < 58) {
                        if (_M0L10next__charS163 < 48) {
                          goto join_162;
                        } else {
                          _M0L12dispatch__15S152 = 2;
                          goto loop__label__15_155;
                        }
                      } else if (_M0L10next__charS163 > 58) {
                        goto join_162;
                      } else {
                        _M0L12dispatch__15S152 = 3;
                        goto loop__label__15_155;
                      }
                      join_162:;
                      _M0L12dispatch__15S152 = 0;
                      goto loop__label__15_155;
                    } else {
                      goto join_144;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1707;
                    _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                    _M0L6_2atmpS1707 = _M0Lm9_2acursorS128;
                    if (_M0L6_2atmpS1707 < _M0L6_2aendS127) {
                      int32_t _M0L6_2atmpS1709 = _M0Lm9_2acursorS128;
                      int32_t _M0L10next__charS164;
                      int32_t _M0L6_2atmpS1708;
                      moonbit_incref(_M0L7_2adataS125);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS164
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1709);
                      _M0L6_2atmpS1708 = _M0Lm9_2acursorS128;
                      _M0Lm9_2acursorS128 = _M0L6_2atmpS1708 + 1;
                      if (_M0L10next__charS164 == 58) {
                        _M0L12dispatch__15S152 = 1;
                        goto loop__label__15_155;
                      } else {
                        _M0L12dispatch__15S152 = 0;
                        goto loop__label__15_155;
                      }
                    } else {
                      goto join_144;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1710;
                    _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                    _M0Lm6tag__4S143 = _M0Lm9_2acursorS128;
                    _M0L6_2atmpS1710 = _M0Lm9_2acursorS128;
                    if (_M0L6_2atmpS1710 < _M0L6_2aendS127) {
                      int32_t _M0L6_2atmpS1718 = _M0Lm9_2acursorS128;
                      int32_t _M0L10next__charS166;
                      int32_t _M0L6_2atmpS1711;
                      moonbit_incref(_M0L7_2adataS125);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS166
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1718);
                      _M0L6_2atmpS1711 = _M0Lm9_2acursorS128;
                      _M0Lm9_2acursorS128 = _M0L6_2atmpS1711 + 1;
                      if (_M0L10next__charS166 < 58) {
                        if (_M0L10next__charS166 < 48) {
                          goto join_165;
                        } else {
                          _M0L12dispatch__15S152 = 4;
                          goto loop__label__15_155;
                        }
                      } else if (_M0L10next__charS166 > 58) {
                        goto join_165;
                      } else {
                        int32_t _M0L6_2atmpS1712;
                        _M0Lm9tag__1__2S139 = _M0Lm9tag__1__1S138;
                        _M0Lm9tag__1__1S138 = _M0Lm6tag__1S137;
                        _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                        _M0L6_2atmpS1712 = _M0Lm9_2acursorS128;
                        if (_M0L6_2atmpS1712 < _M0L6_2aendS127) {
                          int32_t _M0L6_2atmpS1717 = _M0Lm9_2acursorS128;
                          int32_t _M0L10next__charS168;
                          int32_t _M0L6_2atmpS1713;
                          moonbit_incref(_M0L7_2adataS125);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS168
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1717);
                          _M0L6_2atmpS1713 = _M0Lm9_2acursorS128;
                          _M0Lm9_2acursorS128 = _M0L6_2atmpS1713 + 1;
                          if (_M0L10next__charS168 < 58) {
                            if (_M0L10next__charS168 < 48) {
                              goto join_167;
                            } else {
                              int32_t _M0L6_2atmpS1714;
                              _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                              _M0Lm9tag__2__1S142 = _M0Lm6tag__2S141;
                              _M0Lm6tag__2S141 = _M0Lm9_2acursorS128;
                              _M0L6_2atmpS1714 = _M0Lm9_2acursorS128;
                              if (_M0L6_2atmpS1714 < _M0L6_2aendS127) {
                                int32_t _M0L6_2atmpS1716 =
                                  _M0Lm9_2acursorS128;
                                int32_t _M0L10next__charS170;
                                int32_t _M0L6_2atmpS1715;
                                moonbit_incref(_M0L7_2adataS125);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS170
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1716);
                                _M0L6_2atmpS1715 = _M0Lm9_2acursorS128;
                                _M0Lm9_2acursorS128 = _M0L6_2atmpS1715 + 1;
                                if (_M0L10next__charS170 < 58) {
                                  if (_M0L10next__charS170 < 48) {
                                    goto join_169;
                                  } else {
                                    _M0L12dispatch__15S152 = 5;
                                    goto loop__label__15_155;
                                  }
                                } else if (_M0L10next__charS170 > 58) {
                                  goto join_169;
                                } else {
                                  _M0L12dispatch__15S152 = 3;
                                  goto loop__label__15_155;
                                }
                                join_169:;
                                _M0L12dispatch__15S152 = 0;
                                goto loop__label__15_155;
                              } else {
                                goto join_157;
                              }
                            }
                          } else if (_M0L10next__charS168 > 58) {
                            goto join_167;
                          } else {
                            _M0L12dispatch__15S152 = 1;
                            goto loop__label__15_155;
                          }
                          join_167:;
                          _M0L12dispatch__15S152 = 0;
                          goto loop__label__15_155;
                        } else {
                          goto join_144;
                        }
                      }
                      join_165:;
                      _M0L12dispatch__15S152 = 0;
                      goto loop__label__15_155;
                    } else {
                      goto join_144;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1719;
                    _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                    _M0Lm6tag__2S141 = _M0Lm9_2acursorS128;
                    _M0L6_2atmpS1719 = _M0Lm9_2acursorS128;
                    if (_M0L6_2atmpS1719 < _M0L6_2aendS127) {
                      int32_t _M0L6_2atmpS1721 = _M0Lm9_2acursorS128;
                      int32_t _M0L10next__charS172;
                      int32_t _M0L6_2atmpS1720;
                      moonbit_incref(_M0L7_2adataS125);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS172
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1721);
                      _M0L6_2atmpS1720 = _M0Lm9_2acursorS128;
                      _M0Lm9_2acursorS128 = _M0L6_2atmpS1720 + 1;
                      if (_M0L10next__charS172 < 58) {
                        if (_M0L10next__charS172 < 48) {
                          goto join_171;
                        } else {
                          _M0L12dispatch__15S152 = 5;
                          goto loop__label__15_155;
                        }
                      } else if (_M0L10next__charS172 > 58) {
                        goto join_171;
                      } else {
                        _M0L12dispatch__15S152 = 3;
                        goto loop__label__15_155;
                      }
                      join_171:;
                      _M0L12dispatch__15S152 = 0;
                      goto loop__label__15_155;
                    } else {
                      goto join_157;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1722;
                    _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                    _M0Lm6tag__2S141 = _M0Lm9_2acursorS128;
                    _M0Lm6tag__3S140 = _M0Lm9_2acursorS128;
                    _M0L6_2atmpS1722 = _M0Lm9_2acursorS128;
                    if (_M0L6_2atmpS1722 < _M0L6_2aendS127) {
                      int32_t _M0L6_2atmpS1724 = _M0Lm9_2acursorS128;
                      int32_t _M0L10next__charS174;
                      int32_t _M0L6_2atmpS1723;
                      moonbit_incref(_M0L7_2adataS125);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS174
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1724);
                      _M0L6_2atmpS1723 = _M0Lm9_2acursorS128;
                      _M0Lm9_2acursorS128 = _M0L6_2atmpS1723 + 1;
                      if (_M0L10next__charS174 < 48) {
                        if (_M0L10next__charS174 == 45) {
                          goto join_153;
                        } else {
                          goto join_173;
                        }
                      } else if (_M0L10next__charS174 > 57) {
                        if (_M0L10next__charS174 < 59) {
                          _M0L12dispatch__15S152 = 3;
                          goto loop__label__15_155;
                        } else {
                          goto join_173;
                        }
                      } else {
                        _M0L12dispatch__15S152 = 6;
                        goto loop__label__15_155;
                      }
                      join_173:;
                      _M0L12dispatch__15S152 = 0;
                      goto loop__label__15_155;
                    } else {
                      goto join_144;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1725;
                    _M0Lm9tag__1__1S138 = _M0Lm6tag__1S137;
                    _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                    _M0L6_2atmpS1725 = _M0Lm9_2acursorS128;
                    if (_M0L6_2atmpS1725 < _M0L6_2aendS127) {
                      int32_t _M0L6_2atmpS1727 = _M0Lm9_2acursorS128;
                      int32_t _M0L10next__charS176;
                      int32_t _M0L6_2atmpS1726;
                      moonbit_incref(_M0L7_2adataS125);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS176
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1727);
                      _M0L6_2atmpS1726 = _M0Lm9_2acursorS128;
                      _M0Lm9_2acursorS128 = _M0L6_2atmpS1726 + 1;
                      if (_M0L10next__charS176 < 58) {
                        if (_M0L10next__charS176 < 48) {
                          goto join_175;
                        } else {
                          _M0L12dispatch__15S152 = 2;
                          goto loop__label__15_155;
                        }
                      } else if (_M0L10next__charS176 > 58) {
                        goto join_175;
                      } else {
                        _M0L12dispatch__15S152 = 1;
                        goto loop__label__15_155;
                      }
                      join_175:;
                      _M0L12dispatch__15S152 = 0;
                      goto loop__label__15_155;
                    } else {
                      goto join_144;
                    }
                    break;
                  }
                  default: {
                    goto join_144;
                    break;
                  }
                }
                join_157:;
                _M0Lm6tag__1S137 = _M0Lm9tag__1__2S139;
                _M0Lm6tag__2S141 = _M0Lm9tag__2__1S142;
                _M0Lm20match__tag__saver__0S131 = _M0Lm6tag__0S136;
                _M0Lm20match__tag__saver__1S132 = _M0Lm6tag__1S137;
                _M0Lm20match__tag__saver__2S133 = _M0Lm6tag__2S141;
                _M0Lm20match__tag__saver__3S134 = _M0Lm6tag__3S140;
                _M0Lm20match__tag__saver__4S135 = _M0Lm6tag__4S143;
                _M0Lm13accept__stateS129 = 0;
                _M0Lm10match__endS130 = _M0Lm9_2acursorS128;
                goto join_144;
                join_153:;
                _M0Lm9tag__1__1S138 = _M0Lm9tag__1__2S139;
                _M0Lm6tag__1S137 = _M0Lm9_2acursorS128;
                _M0Lm6tag__2S141 = _M0Lm9tag__2__1S142;
                _M0L6_2atmpS1695 = _M0Lm9_2acursorS128;
                if (_M0L6_2atmpS1695 < _M0L6_2aendS127) {
                  int32_t _M0L6_2atmpS1697 = _M0Lm9_2acursorS128;
                  int32_t _M0L10next__charS156;
                  int32_t _M0L6_2atmpS1696;
                  moonbit_incref(_M0L7_2adataS125);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS156
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS125, _M0L6_2atmpS1697);
                  _M0L6_2atmpS1696 = _M0Lm9_2acursorS128;
                  _M0Lm9_2acursorS128 = _M0L6_2atmpS1696 + 1;
                  if (_M0L10next__charS156 < 58) {
                    if (_M0L10next__charS156 < 48) {
                      goto join_154;
                    } else {
                      _M0L12dispatch__15S152 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS156 > 58) {
                    goto join_154;
                  } else {
                    _M0L12dispatch__15S152 = 1;
                    continue;
                  }
                  join_154:;
                  _M0L12dispatch__15S152 = 0;
                  continue;
                } else {
                  goto join_144;
                }
                break;
              }
            } else {
              goto join_144;
            }
          } else {
            continue;
          }
        } else {
          goto join_144;
        }
        break;
      }
    } else {
      goto join_144;
    }
  } else {
    goto join_144;
  }
  join_144:;
  switch (_M0Lm13accept__stateS129) {
    case 0: {
      int32_t _M0L6_2atmpS1686 = _M0Lm20match__tag__saver__1S132;
      int32_t _M0L6_2atmpS1685 = _M0L6_2atmpS1686 + 1;
      int64_t _M0L6_2atmpS1682 = (int64_t)_M0L6_2atmpS1685;
      int32_t _M0L6_2atmpS1684 = _M0Lm20match__tag__saver__2S133;
      int64_t _M0L6_2atmpS1683 = (int64_t)_M0L6_2atmpS1684;
      struct _M0TPC16string10StringView _M0L11start__lineS145;
      int32_t _M0L6_2atmpS1681;
      int32_t _M0L6_2atmpS1680;
      int64_t _M0L6_2atmpS1677;
      int32_t _M0L6_2atmpS1679;
      int64_t _M0L6_2atmpS1678;
      struct _M0TPC16string10StringView _M0L13start__columnS146;
      int32_t _M0L6_2atmpS1676;
      int64_t _M0L6_2atmpS1673;
      int32_t _M0L6_2atmpS1675;
      int64_t _M0L6_2atmpS1674;
      struct _M0TPC16string10StringView _M0L3pkgS147;
      int32_t _M0L6_2atmpS1672;
      int32_t _M0L6_2atmpS1671;
      int64_t _M0L6_2atmpS1668;
      int32_t _M0L6_2atmpS1670;
      int64_t _M0L6_2atmpS1669;
      struct _M0TPC16string10StringView _M0L8filenameS148;
      int32_t _M0L6_2atmpS1667;
      int32_t _M0L6_2atmpS1666;
      int64_t _M0L6_2atmpS1663;
      int32_t _M0L6_2atmpS1665;
      int64_t _M0L6_2atmpS1664;
      struct _M0TPC16string10StringView _M0L9end__lineS149;
      int32_t _M0L6_2atmpS1662;
      int32_t _M0L6_2atmpS1661;
      int64_t _M0L6_2atmpS1658;
      int32_t _M0L6_2atmpS1660;
      int64_t _M0L6_2atmpS1659;
      struct _M0TPC16string10StringView _M0L11end__columnS150;
      struct _M0TPB13SourceLocRepr* _block_4366;
      moonbit_incref(_M0L7_2adataS125);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS145
      = _M0MPC16string6String4view(_M0L7_2adataS125, _M0L6_2atmpS1682, _M0L6_2atmpS1683);
      _M0L6_2atmpS1681 = _M0Lm20match__tag__saver__2S133;
      _M0L6_2atmpS1680 = _M0L6_2atmpS1681 + 1;
      _M0L6_2atmpS1677 = (int64_t)_M0L6_2atmpS1680;
      _M0L6_2atmpS1679 = _M0Lm20match__tag__saver__3S134;
      _M0L6_2atmpS1678 = (int64_t)_M0L6_2atmpS1679;
      moonbit_incref(_M0L7_2adataS125);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS146
      = _M0MPC16string6String4view(_M0L7_2adataS125, _M0L6_2atmpS1677, _M0L6_2atmpS1678);
      _M0L6_2atmpS1676 = _M0L8_2astartS126 + 1;
      _M0L6_2atmpS1673 = (int64_t)_M0L6_2atmpS1676;
      _M0L6_2atmpS1675 = _M0Lm20match__tag__saver__0S131;
      _M0L6_2atmpS1674 = (int64_t)_M0L6_2atmpS1675;
      moonbit_incref(_M0L7_2adataS125);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS147
      = _M0MPC16string6String4view(_M0L7_2adataS125, _M0L6_2atmpS1673, _M0L6_2atmpS1674);
      _M0L6_2atmpS1672 = _M0Lm20match__tag__saver__0S131;
      _M0L6_2atmpS1671 = _M0L6_2atmpS1672 + 1;
      _M0L6_2atmpS1668 = (int64_t)_M0L6_2atmpS1671;
      _M0L6_2atmpS1670 = _M0Lm20match__tag__saver__1S132;
      _M0L6_2atmpS1669 = (int64_t)_M0L6_2atmpS1670;
      moonbit_incref(_M0L7_2adataS125);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS148
      = _M0MPC16string6String4view(_M0L7_2adataS125, _M0L6_2atmpS1668, _M0L6_2atmpS1669);
      _M0L6_2atmpS1667 = _M0Lm20match__tag__saver__3S134;
      _M0L6_2atmpS1666 = _M0L6_2atmpS1667 + 1;
      _M0L6_2atmpS1663 = (int64_t)_M0L6_2atmpS1666;
      _M0L6_2atmpS1665 = _M0Lm20match__tag__saver__4S135;
      _M0L6_2atmpS1664 = (int64_t)_M0L6_2atmpS1665;
      moonbit_incref(_M0L7_2adataS125);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS149
      = _M0MPC16string6String4view(_M0L7_2adataS125, _M0L6_2atmpS1663, _M0L6_2atmpS1664);
      _M0L6_2atmpS1662 = _M0Lm20match__tag__saver__4S135;
      _M0L6_2atmpS1661 = _M0L6_2atmpS1662 + 1;
      _M0L6_2atmpS1658 = (int64_t)_M0L6_2atmpS1661;
      _M0L6_2atmpS1660 = _M0Lm10match__endS130;
      _M0L6_2atmpS1659 = (int64_t)_M0L6_2atmpS1660;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS150
      = _M0MPC16string6String4view(_M0L7_2adataS125, _M0L6_2atmpS1658, _M0L6_2atmpS1659);
      _block_4366
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4366)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4366->$0_0 = _M0L3pkgS147.$0;
      _block_4366->$0_1 = _M0L3pkgS147.$1;
      _block_4366->$0_2 = _M0L3pkgS147.$2;
      _block_4366->$1_0 = _M0L8filenameS148.$0;
      _block_4366->$1_1 = _M0L8filenameS148.$1;
      _block_4366->$1_2 = _M0L8filenameS148.$2;
      _block_4366->$2_0 = _M0L11start__lineS145.$0;
      _block_4366->$2_1 = _M0L11start__lineS145.$1;
      _block_4366->$2_2 = _M0L11start__lineS145.$2;
      _block_4366->$3_0 = _M0L13start__columnS146.$0;
      _block_4366->$3_1 = _M0L13start__columnS146.$1;
      _block_4366->$3_2 = _M0L13start__columnS146.$2;
      _block_4366->$4_0 = _M0L9end__lineS149.$0;
      _block_4366->$4_1 = _M0L9end__lineS149.$1;
      _block_4366->$4_2 = _M0L9end__lineS149.$2;
      _block_4366->$5_0 = _M0L11end__columnS150.$0;
      _block_4366->$5_1 = _M0L11end__columnS150.$1;
      _block_4366->$5_2 = _M0L11end__columnS150.$2;
      return _block_4366;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS125);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS121,
  int32_t _M0L5indexS122
) {
  int32_t _M0L3lenS120;
  int32_t _if__result_4367;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS120 = _M0L4selfS121->$1;
  if (_M0L5indexS122 >= 0) {
    _if__result_4367 = _M0L5indexS122 < _M0L3lenS120;
  } else {
    _if__result_4367 = 0;
  }
  if (_if__result_4367) {
    moonbit_string_t* _M0L6_2atmpS1657;
    moonbit_string_t _M0L6_2atmpS3949;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1657 = _M0MPC15array5Array6bufferGsE(_M0L4selfS121);
    if (
      _M0L5indexS122 < 0
      || _M0L5indexS122 >= Moonbit_array_length(_M0L6_2atmpS1657)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3949 = (moonbit_string_t)_M0L6_2atmpS1657[_M0L5indexS122];
    moonbit_incref(_M0L6_2atmpS3949);
    moonbit_decref(_M0L6_2atmpS1657);
    return _M0L6_2atmpS3949;
  } else {
    moonbit_decref(_M0L4selfS121);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS117
) {
  moonbit_string_t* _M0L8_2afieldS3950;
  int32_t _M0L6_2acntS4090;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3950 = _M0L4selfS117->$0;
  _M0L6_2acntS4090 = Moonbit_object_header(_M0L4selfS117)->rc;
  if (_M0L6_2acntS4090 > 1) {
    int32_t _M0L11_2anew__cntS4091 = _M0L6_2acntS4090 - 1;
    Moonbit_object_header(_M0L4selfS117)->rc = _M0L11_2anew__cntS4091;
    moonbit_incref(_M0L8_2afieldS3950);
  } else if (_M0L6_2acntS4090 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS117);
  }
  return _M0L8_2afieldS3950;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS118
) {
  struct _M0TUsiE** _M0L8_2afieldS3951;
  int32_t _M0L6_2acntS4092;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3951 = _M0L4selfS118->$0;
  _M0L6_2acntS4092 = Moonbit_object_header(_M0L4selfS118)->rc;
  if (_M0L6_2acntS4092 > 1) {
    int32_t _M0L11_2anew__cntS4093 = _M0L6_2acntS4092 - 1;
    Moonbit_object_header(_M0L4selfS118)->rc = _M0L11_2anew__cntS4093;
    moonbit_incref(_M0L8_2afieldS3951);
  } else if (_M0L6_2acntS4092 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS118);
  }
  return _M0L8_2afieldS3951;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS119
) {
  void** _M0L8_2afieldS3952;
  int32_t _M0L6_2acntS4094;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3952 = _M0L4selfS119->$0;
  _M0L6_2acntS4094 = Moonbit_object_header(_M0L4selfS119)->rc;
  if (_M0L6_2acntS4094 > 1) {
    int32_t _M0L11_2anew__cntS4095 = _M0L6_2acntS4094 - 1;
    Moonbit_object_header(_M0L4selfS119)->rc = _M0L11_2anew__cntS4095;
    moonbit_incref(_M0L8_2afieldS3952);
  } else if (_M0L6_2acntS4094 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS119);
  }
  return _M0L8_2afieldS3952;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS116) {
  struct _M0TPB13StringBuilder* _M0L3bufS115;
  struct _M0TPB6Logger _M0L6_2atmpS1656;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS115 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS115);
  _M0L6_2atmpS1656
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS115
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS116, _M0L6_2atmpS1656);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS115);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS112,
  int32_t _M0L5indexS113
) {
  int32_t _M0L2c1S111;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S111 = _M0L4selfS112[_M0L5indexS113];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S111)) {
    int32_t _M0L6_2atmpS1655 = _M0L5indexS113 + 1;
    int32_t _M0L6_2atmpS3953 = _M0L4selfS112[_M0L6_2atmpS1655];
    int32_t _M0L2c2S114;
    int32_t _M0L6_2atmpS1653;
    int32_t _M0L6_2atmpS1654;
    moonbit_decref(_M0L4selfS112);
    _M0L2c2S114 = _M0L6_2atmpS3953;
    _M0L6_2atmpS1653 = (int32_t)_M0L2c1S111;
    _M0L6_2atmpS1654 = (int32_t)_M0L2c2S114;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1653, _M0L6_2atmpS1654);
  } else {
    moonbit_decref(_M0L4selfS112);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S111);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS110) {
  int32_t _M0L6_2atmpS1652;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1652 = (int32_t)_M0L4selfS110;
  return _M0L6_2atmpS1652;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS108,
  int32_t _M0L8trailingS109
) {
  int32_t _M0L6_2atmpS1651;
  int32_t _M0L6_2atmpS1650;
  int32_t _M0L6_2atmpS1649;
  int32_t _M0L6_2atmpS1648;
  int32_t _M0L6_2atmpS1647;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1651 = _M0L7leadingS108 - 55296;
  _M0L6_2atmpS1650 = _M0L6_2atmpS1651 * 1024;
  _M0L6_2atmpS1649 = _M0L6_2atmpS1650 + _M0L8trailingS109;
  _M0L6_2atmpS1648 = _M0L6_2atmpS1649 - 56320;
  _M0L6_2atmpS1647 = _M0L6_2atmpS1648 + 65536;
  return _M0L6_2atmpS1647;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS107) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS107 >= 56320) {
    return _M0L4selfS107 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS106) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS106 >= 55296) {
    return _M0L4selfS106 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS103,
  int32_t _M0L2chS105
) {
  int32_t _M0L3lenS1642;
  int32_t _M0L6_2atmpS1641;
  moonbit_bytes_t _M0L8_2afieldS3954;
  moonbit_bytes_t _M0L4dataS1645;
  int32_t _M0L3lenS1646;
  int32_t _M0L3incS104;
  int32_t _M0L3lenS1644;
  int32_t _M0L6_2atmpS1643;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1642 = _M0L4selfS103->$1;
  _M0L6_2atmpS1641 = _M0L3lenS1642 + 4;
  moonbit_incref(_M0L4selfS103);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS103, _M0L6_2atmpS1641);
  _M0L8_2afieldS3954 = _M0L4selfS103->$0;
  _M0L4dataS1645 = _M0L8_2afieldS3954;
  _M0L3lenS1646 = _M0L4selfS103->$1;
  moonbit_incref(_M0L4dataS1645);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS104
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1645, _M0L3lenS1646, _M0L2chS105);
  _M0L3lenS1644 = _M0L4selfS103->$1;
  _M0L6_2atmpS1643 = _M0L3lenS1644 + _M0L3incS104;
  _M0L4selfS103->$1 = _M0L6_2atmpS1643;
  moonbit_decref(_M0L4selfS103);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS98,
  int32_t _M0L8requiredS99
) {
  moonbit_bytes_t _M0L8_2afieldS3958;
  moonbit_bytes_t _M0L4dataS1640;
  int32_t _M0L6_2atmpS3957;
  int32_t _M0L12current__lenS97;
  int32_t _M0Lm13enough__spaceS100;
  int32_t _M0L6_2atmpS1638;
  int32_t _M0L6_2atmpS1639;
  moonbit_bytes_t _M0L9new__dataS102;
  moonbit_bytes_t _M0L8_2afieldS3956;
  moonbit_bytes_t _M0L4dataS1636;
  int32_t _M0L3lenS1637;
  moonbit_bytes_t _M0L6_2aoldS3955;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3958 = _M0L4selfS98->$0;
  _M0L4dataS1640 = _M0L8_2afieldS3958;
  _M0L6_2atmpS3957 = Moonbit_array_length(_M0L4dataS1640);
  _M0L12current__lenS97 = _M0L6_2atmpS3957;
  if (_M0L8requiredS99 <= _M0L12current__lenS97) {
    moonbit_decref(_M0L4selfS98);
    return 0;
  }
  _M0Lm13enough__spaceS100 = _M0L12current__lenS97;
  while (1) {
    int32_t _M0L6_2atmpS1634 = _M0Lm13enough__spaceS100;
    if (_M0L6_2atmpS1634 < _M0L8requiredS99) {
      int32_t _M0L6_2atmpS1635 = _M0Lm13enough__spaceS100;
      _M0Lm13enough__spaceS100 = _M0L6_2atmpS1635 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1638 = _M0Lm13enough__spaceS100;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1639 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS102
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1638, _M0L6_2atmpS1639);
  _M0L8_2afieldS3956 = _M0L4selfS98->$0;
  _M0L4dataS1636 = _M0L8_2afieldS3956;
  _M0L3lenS1637 = _M0L4selfS98->$1;
  moonbit_incref(_M0L4dataS1636);
  moonbit_incref(_M0L9new__dataS102);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS102, 0, _M0L4dataS1636, 0, _M0L3lenS1637);
  _M0L6_2aoldS3955 = _M0L4selfS98->$0;
  moonbit_decref(_M0L6_2aoldS3955);
  _M0L4selfS98->$0 = _M0L9new__dataS102;
  moonbit_decref(_M0L4selfS98);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS92,
  int32_t _M0L6offsetS93,
  int32_t _M0L5valueS91
) {
  uint32_t _M0L4codeS90;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS90 = _M0MPC14char4Char8to__uint(_M0L5valueS91);
  if (_M0L4codeS90 < 65536u) {
    uint32_t _M0L6_2atmpS1617 = _M0L4codeS90 & 255u;
    int32_t _M0L6_2atmpS1616;
    int32_t _M0L6_2atmpS1618;
    uint32_t _M0L6_2atmpS1620;
    int32_t _M0L6_2atmpS1619;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1616 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1617);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1616;
    _M0L6_2atmpS1618 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1620 = _M0L4codeS90 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1619 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1620);
    if (
      _M0L6_2atmpS1618 < 0
      || _M0L6_2atmpS1618 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1618] = _M0L6_2atmpS1619;
    moonbit_decref(_M0L4selfS92);
    return 2;
  } else if (_M0L4codeS90 < 1114112u) {
    uint32_t _M0L2hiS94 = _M0L4codeS90 - 65536u;
    uint32_t _M0L6_2atmpS1633 = _M0L2hiS94 >> 10;
    uint32_t _M0L2loS95 = _M0L6_2atmpS1633 | 55296u;
    uint32_t _M0L6_2atmpS1632 = _M0L2hiS94 & 1023u;
    uint32_t _M0L2hiS96 = _M0L6_2atmpS1632 | 56320u;
    uint32_t _M0L6_2atmpS1622 = _M0L2loS95 & 255u;
    int32_t _M0L6_2atmpS1621;
    int32_t _M0L6_2atmpS1623;
    uint32_t _M0L6_2atmpS1625;
    int32_t _M0L6_2atmpS1624;
    int32_t _M0L6_2atmpS1626;
    uint32_t _M0L6_2atmpS1628;
    int32_t _M0L6_2atmpS1627;
    int32_t _M0L6_2atmpS1629;
    uint32_t _M0L6_2atmpS1631;
    int32_t _M0L6_2atmpS1630;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1621 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1622);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1621;
    _M0L6_2atmpS1623 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1625 = _M0L2loS95 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1624 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1625);
    if (
      _M0L6_2atmpS1623 < 0
      || _M0L6_2atmpS1623 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1623] = _M0L6_2atmpS1624;
    _M0L6_2atmpS1626 = _M0L6offsetS93 + 2;
    _M0L6_2atmpS1628 = _M0L2hiS96 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1627 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1628);
    if (
      _M0L6_2atmpS1626 < 0
      || _M0L6_2atmpS1626 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1626] = _M0L6_2atmpS1627;
    _M0L6_2atmpS1629 = _M0L6offsetS93 + 3;
    _M0L6_2atmpS1631 = _M0L2hiS96 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1630 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1631);
    if (
      _M0L6_2atmpS1629 < 0
      || _M0L6_2atmpS1629 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1629] = _M0L6_2atmpS1630;
    moonbit_decref(_M0L4selfS92);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS92);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_114.data, (moonbit_string_t)moonbit_string_literal_115.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1615;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1615 = *(int32_t*)&_M0L4selfS89;
  return _M0L6_2atmpS1615 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS88) {
  int32_t _M0L6_2atmpS1614;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1614 = _M0L4selfS88;
  return *(uint32_t*)&_M0L6_2atmpS1614;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS87
) {
  moonbit_bytes_t _M0L8_2afieldS3960;
  moonbit_bytes_t _M0L4dataS1613;
  moonbit_bytes_t _M0L6_2atmpS1610;
  int32_t _M0L8_2afieldS3959;
  int32_t _M0L3lenS1612;
  int64_t _M0L6_2atmpS1611;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3960 = _M0L4selfS87->$0;
  _M0L4dataS1613 = _M0L8_2afieldS3960;
  moonbit_incref(_M0L4dataS1613);
  _M0L6_2atmpS1610 = _M0L4dataS1613;
  _M0L8_2afieldS3959 = _M0L4selfS87->$1;
  moonbit_decref(_M0L4selfS87);
  _M0L3lenS1612 = _M0L8_2afieldS3959;
  _M0L6_2atmpS1611 = (int64_t)_M0L3lenS1612;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1610, 0, _M0L6_2atmpS1611);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS82,
  int32_t _M0L6offsetS86,
  int64_t _M0L6lengthS84
) {
  int32_t _M0L3lenS81;
  int32_t _M0L6lengthS83;
  int32_t _if__result_4369;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS81 = Moonbit_array_length(_M0L4selfS82);
  if (_M0L6lengthS84 == 4294967296ll) {
    _M0L6lengthS83 = _M0L3lenS81 - _M0L6offsetS86;
  } else {
    int64_t _M0L7_2aSomeS85 = _M0L6lengthS84;
    _M0L6lengthS83 = (int32_t)_M0L7_2aSomeS85;
  }
  if (_M0L6offsetS86 >= 0) {
    if (_M0L6lengthS83 >= 0) {
      int32_t _M0L6_2atmpS1609 = _M0L6offsetS86 + _M0L6lengthS83;
      _if__result_4369 = _M0L6_2atmpS1609 <= _M0L3lenS81;
    } else {
      _if__result_4369 = 0;
    }
  } else {
    _if__result_4369 = 0;
  }
  if (_if__result_4369) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS82, _M0L6offsetS86, _M0L6lengthS83);
  } else {
    moonbit_decref(_M0L4selfS82);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS79
) {
  int32_t _M0L7initialS78;
  moonbit_bytes_t _M0L4dataS80;
  struct _M0TPB13StringBuilder* _block_4370;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS79 < 1) {
    _M0L7initialS78 = 1;
  } else {
    _M0L7initialS78 = _M0L10size__hintS79;
  }
  _M0L4dataS80 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS78, 0);
  _block_4370
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4370)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4370->$0 = _M0L4dataS80;
  _block_4370->$1 = 0;
  return _block_4370;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS77) {
  int32_t _M0L6_2atmpS1608;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1608 = (int32_t)_M0L4selfS77;
  return _M0L6_2atmpS1608;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS62,
  int32_t _M0L11dst__offsetS63,
  moonbit_string_t* _M0L3srcS64,
  int32_t _M0L11src__offsetS65,
  int32_t _M0L3lenS66
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS62, _M0L11dst__offsetS63, _M0L3srcS64, _M0L11src__offsetS65, _M0L3lenS66);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS67,
  int32_t _M0L11dst__offsetS68,
  struct _M0TUsiE** _M0L3srcS69,
  int32_t _M0L11src__offsetS70,
  int32_t _M0L3lenS71
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS67, _M0L11dst__offsetS68, _M0L3srcS69, _M0L11src__offsetS70, _M0L3lenS71);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS72,
  int32_t _M0L11dst__offsetS73,
  void** _M0L3srcS74,
  int32_t _M0L11src__offsetS75,
  int32_t _M0L3lenS76
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS72, _M0L11dst__offsetS73, _M0L3srcS74, _M0L11src__offsetS75, _M0L3lenS76);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS26,
  int32_t _M0L11dst__offsetS28,
  moonbit_bytes_t _M0L3srcS27,
  int32_t _M0L11src__offsetS29,
  int32_t _M0L3lenS31
) {
  int32_t _if__result_4371;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS26 == _M0L3srcS27) {
    _if__result_4371 = _M0L11dst__offsetS28 < _M0L11src__offsetS29;
  } else {
    _if__result_4371 = 0;
  }
  if (_if__result_4371) {
    int32_t _M0L1iS30 = 0;
    while (1) {
      if (_M0L1iS30 < _M0L3lenS31) {
        int32_t _M0L6_2atmpS1572 = _M0L11dst__offsetS28 + _M0L1iS30;
        int32_t _M0L6_2atmpS1574 = _M0L11src__offsetS29 + _M0L1iS30;
        int32_t _M0L6_2atmpS1573;
        int32_t _M0L6_2atmpS1575;
        if (
          _M0L6_2atmpS1574 < 0
          || _M0L6_2atmpS1574 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1573 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1574];
        if (
          _M0L6_2atmpS1572 < 0
          || _M0L6_2atmpS1572 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1572] = _M0L6_2atmpS1573;
        _M0L6_2atmpS1575 = _M0L1iS30 + 1;
        _M0L1iS30 = _M0L6_2atmpS1575;
        continue;
      } else {
        moonbit_decref(_M0L3srcS27);
        moonbit_decref(_M0L3dstS26);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1580 = _M0L3lenS31 - 1;
    int32_t _M0L1iS33 = _M0L6_2atmpS1580;
    while (1) {
      if (_M0L1iS33 >= 0) {
        int32_t _M0L6_2atmpS1576 = _M0L11dst__offsetS28 + _M0L1iS33;
        int32_t _M0L6_2atmpS1578 = _M0L11src__offsetS29 + _M0L1iS33;
        int32_t _M0L6_2atmpS1577;
        int32_t _M0L6_2atmpS1579;
        if (
          _M0L6_2atmpS1578 < 0
          || _M0L6_2atmpS1578 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1577 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1578];
        if (
          _M0L6_2atmpS1576 < 0
          || _M0L6_2atmpS1576 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1576] = _M0L6_2atmpS1577;
        _M0L6_2atmpS1579 = _M0L1iS33 - 1;
        _M0L1iS33 = _M0L6_2atmpS1579;
        continue;
      } else {
        moonbit_decref(_M0L3srcS27);
        moonbit_decref(_M0L3dstS26);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS35,
  int32_t _M0L11dst__offsetS37,
  moonbit_string_t* _M0L3srcS36,
  int32_t _M0L11src__offsetS38,
  int32_t _M0L3lenS40
) {
  int32_t _if__result_4374;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS35 == _M0L3srcS36) {
    _if__result_4374 = _M0L11dst__offsetS37 < _M0L11src__offsetS38;
  } else {
    _if__result_4374 = 0;
  }
  if (_if__result_4374) {
    int32_t _M0L1iS39 = 0;
    while (1) {
      if (_M0L1iS39 < _M0L3lenS40) {
        int32_t _M0L6_2atmpS1581 = _M0L11dst__offsetS37 + _M0L1iS39;
        int32_t _M0L6_2atmpS1583 = _M0L11src__offsetS38 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS3962;
        moonbit_string_t _M0L6_2atmpS1582;
        moonbit_string_t _M0L6_2aoldS3961;
        int32_t _M0L6_2atmpS1584;
        if (
          _M0L6_2atmpS1583 < 0
          || _M0L6_2atmpS1583 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3962 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1583];
        _M0L6_2atmpS1582 = _M0L6_2atmpS3962;
        if (
          _M0L6_2atmpS1581 < 0
          || _M0L6_2atmpS1581 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3961 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1581];
        moonbit_incref(_M0L6_2atmpS1582);
        moonbit_decref(_M0L6_2aoldS3961);
        _M0L3dstS35[_M0L6_2atmpS1581] = _M0L6_2atmpS1582;
        _M0L6_2atmpS1584 = _M0L1iS39 + 1;
        _M0L1iS39 = _M0L6_2atmpS1584;
        continue;
      } else {
        moonbit_decref(_M0L3srcS36);
        moonbit_decref(_M0L3dstS35);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1589 = _M0L3lenS40 - 1;
    int32_t _M0L1iS42 = _M0L6_2atmpS1589;
    while (1) {
      if (_M0L1iS42 >= 0) {
        int32_t _M0L6_2atmpS1585 = _M0L11dst__offsetS37 + _M0L1iS42;
        int32_t _M0L6_2atmpS1587 = _M0L11src__offsetS38 + _M0L1iS42;
        moonbit_string_t _M0L6_2atmpS3964;
        moonbit_string_t _M0L6_2atmpS1586;
        moonbit_string_t _M0L6_2aoldS3963;
        int32_t _M0L6_2atmpS1588;
        if (
          _M0L6_2atmpS1587 < 0
          || _M0L6_2atmpS1587 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3964 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1587];
        _M0L6_2atmpS1586 = _M0L6_2atmpS3964;
        if (
          _M0L6_2atmpS1585 < 0
          || _M0L6_2atmpS1585 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3963 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1585];
        moonbit_incref(_M0L6_2atmpS1586);
        moonbit_decref(_M0L6_2aoldS3963);
        _M0L3dstS35[_M0L6_2atmpS1585] = _M0L6_2atmpS1586;
        _M0L6_2atmpS1588 = _M0L1iS42 - 1;
        _M0L1iS42 = _M0L6_2atmpS1588;
        continue;
      } else {
        moonbit_decref(_M0L3srcS36);
        moonbit_decref(_M0L3dstS35);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS44,
  int32_t _M0L11dst__offsetS46,
  struct _M0TUsiE** _M0L3srcS45,
  int32_t _M0L11src__offsetS47,
  int32_t _M0L3lenS49
) {
  int32_t _if__result_4377;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS44 == _M0L3srcS45) {
    _if__result_4377 = _M0L11dst__offsetS46 < _M0L11src__offsetS47;
  } else {
    _if__result_4377 = 0;
  }
  if (_if__result_4377) {
    int32_t _M0L1iS48 = 0;
    while (1) {
      if (_M0L1iS48 < _M0L3lenS49) {
        int32_t _M0L6_2atmpS1590 = _M0L11dst__offsetS46 + _M0L1iS48;
        int32_t _M0L6_2atmpS1592 = _M0L11src__offsetS47 + _M0L1iS48;
        struct _M0TUsiE* _M0L6_2atmpS3966;
        struct _M0TUsiE* _M0L6_2atmpS1591;
        struct _M0TUsiE* _M0L6_2aoldS3965;
        int32_t _M0L6_2atmpS1593;
        if (
          _M0L6_2atmpS1592 < 0
          || _M0L6_2atmpS1592 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3966 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1592];
        _M0L6_2atmpS1591 = _M0L6_2atmpS3966;
        if (
          _M0L6_2atmpS1590 < 0
          || _M0L6_2atmpS1590 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3965 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1590];
        if (_M0L6_2atmpS1591) {
          moonbit_incref(_M0L6_2atmpS1591);
        }
        if (_M0L6_2aoldS3965) {
          moonbit_decref(_M0L6_2aoldS3965);
        }
        _M0L3dstS44[_M0L6_2atmpS1590] = _M0L6_2atmpS1591;
        _M0L6_2atmpS1593 = _M0L1iS48 + 1;
        _M0L1iS48 = _M0L6_2atmpS1593;
        continue;
      } else {
        moonbit_decref(_M0L3srcS45);
        moonbit_decref(_M0L3dstS44);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1598 = _M0L3lenS49 - 1;
    int32_t _M0L1iS51 = _M0L6_2atmpS1598;
    while (1) {
      if (_M0L1iS51 >= 0) {
        int32_t _M0L6_2atmpS1594 = _M0L11dst__offsetS46 + _M0L1iS51;
        int32_t _M0L6_2atmpS1596 = _M0L11src__offsetS47 + _M0L1iS51;
        struct _M0TUsiE* _M0L6_2atmpS3968;
        struct _M0TUsiE* _M0L6_2atmpS1595;
        struct _M0TUsiE* _M0L6_2aoldS3967;
        int32_t _M0L6_2atmpS1597;
        if (
          _M0L6_2atmpS1596 < 0
          || _M0L6_2atmpS1596 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3968 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1596];
        _M0L6_2atmpS1595 = _M0L6_2atmpS3968;
        if (
          _M0L6_2atmpS1594 < 0
          || _M0L6_2atmpS1594 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3967 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1594];
        if (_M0L6_2atmpS1595) {
          moonbit_incref(_M0L6_2atmpS1595);
        }
        if (_M0L6_2aoldS3967) {
          moonbit_decref(_M0L6_2aoldS3967);
        }
        _M0L3dstS44[_M0L6_2atmpS1594] = _M0L6_2atmpS1595;
        _M0L6_2atmpS1597 = _M0L1iS51 - 1;
        _M0L1iS51 = _M0L6_2atmpS1597;
        continue;
      } else {
        moonbit_decref(_M0L3srcS45);
        moonbit_decref(_M0L3dstS44);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS53,
  int32_t _M0L11dst__offsetS55,
  void** _M0L3srcS54,
  int32_t _M0L11src__offsetS56,
  int32_t _M0L3lenS58
) {
  int32_t _if__result_4380;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS53 == _M0L3srcS54) {
    _if__result_4380 = _M0L11dst__offsetS55 < _M0L11src__offsetS56;
  } else {
    _if__result_4380 = 0;
  }
  if (_if__result_4380) {
    int32_t _M0L1iS57 = 0;
    while (1) {
      if (_M0L1iS57 < _M0L3lenS58) {
        int32_t _M0L6_2atmpS1599 = _M0L11dst__offsetS55 + _M0L1iS57;
        int32_t _M0L6_2atmpS1601 = _M0L11src__offsetS56 + _M0L1iS57;
        void* _M0L6_2atmpS3970;
        void* _M0L6_2atmpS1600;
        void* _M0L6_2aoldS3969;
        int32_t _M0L6_2atmpS1602;
        if (
          _M0L6_2atmpS1601 < 0
          || _M0L6_2atmpS1601 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3970 = (void*)_M0L3srcS54[_M0L6_2atmpS1601];
        _M0L6_2atmpS1600 = _M0L6_2atmpS3970;
        if (
          _M0L6_2atmpS1599 < 0
          || _M0L6_2atmpS1599 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3969 = (void*)_M0L3dstS53[_M0L6_2atmpS1599];
        moonbit_incref(_M0L6_2atmpS1600);
        moonbit_decref(_M0L6_2aoldS3969);
        _M0L3dstS53[_M0L6_2atmpS1599] = _M0L6_2atmpS1600;
        _M0L6_2atmpS1602 = _M0L1iS57 + 1;
        _M0L1iS57 = _M0L6_2atmpS1602;
        continue;
      } else {
        moonbit_decref(_M0L3srcS54);
        moonbit_decref(_M0L3dstS53);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1607 = _M0L3lenS58 - 1;
    int32_t _M0L1iS60 = _M0L6_2atmpS1607;
    while (1) {
      if (_M0L1iS60 >= 0) {
        int32_t _M0L6_2atmpS1603 = _M0L11dst__offsetS55 + _M0L1iS60;
        int32_t _M0L6_2atmpS1605 = _M0L11src__offsetS56 + _M0L1iS60;
        void* _M0L6_2atmpS3972;
        void* _M0L6_2atmpS1604;
        void* _M0L6_2aoldS3971;
        int32_t _M0L6_2atmpS1606;
        if (
          _M0L6_2atmpS1605 < 0
          || _M0L6_2atmpS1605 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3972 = (void*)_M0L3srcS54[_M0L6_2atmpS1605];
        _M0L6_2atmpS1604 = _M0L6_2atmpS3972;
        if (
          _M0L6_2atmpS1603 < 0
          || _M0L6_2atmpS1603 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3971 = (void*)_M0L3dstS53[_M0L6_2atmpS1603];
        moonbit_incref(_M0L6_2atmpS1604);
        moonbit_decref(_M0L6_2aoldS3971);
        _M0L3dstS53[_M0L6_2atmpS1603] = _M0L6_2atmpS1604;
        _M0L6_2atmpS1606 = _M0L1iS60 - 1;
        _M0L1iS60 = _M0L6_2atmpS1606;
        continue;
      } else {
        moonbit_decref(_M0L3srcS54);
        moonbit_decref(_M0L3dstS53);
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
  moonbit_string_t _M0L6_2atmpS1556;
  moonbit_string_t _M0L6_2atmpS3975;
  moonbit_string_t _M0L6_2atmpS1554;
  moonbit_string_t _M0L6_2atmpS1555;
  moonbit_string_t _M0L6_2atmpS3974;
  moonbit_string_t _M0L6_2atmpS1553;
  moonbit_string_t _M0L6_2atmpS3973;
  moonbit_string_t _M0L6_2atmpS1552;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1556 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3975
  = moonbit_add_string(_M0L6_2atmpS1556, (moonbit_string_t)moonbit_string_literal_116.data);
  moonbit_decref(_M0L6_2atmpS1556);
  _M0L6_2atmpS1554 = _M0L6_2atmpS3975;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1555
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3974 = moonbit_add_string(_M0L6_2atmpS1554, _M0L6_2atmpS1555);
  moonbit_decref(_M0L6_2atmpS1554);
  moonbit_decref(_M0L6_2atmpS1555);
  _M0L6_2atmpS1553 = _M0L6_2atmpS3974;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3973
  = moonbit_add_string(_M0L6_2atmpS1553, (moonbit_string_t)moonbit_string_literal_66.data);
  moonbit_decref(_M0L6_2atmpS1553);
  _M0L6_2atmpS1552 = _M0L6_2atmpS3973;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1552);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1561;
  moonbit_string_t _M0L6_2atmpS3978;
  moonbit_string_t _M0L6_2atmpS1559;
  moonbit_string_t _M0L6_2atmpS1560;
  moonbit_string_t _M0L6_2atmpS3977;
  moonbit_string_t _M0L6_2atmpS1558;
  moonbit_string_t _M0L6_2atmpS3976;
  moonbit_string_t _M0L6_2atmpS1557;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1561 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3978
  = moonbit_add_string(_M0L6_2atmpS1561, (moonbit_string_t)moonbit_string_literal_116.data);
  moonbit_decref(_M0L6_2atmpS1561);
  _M0L6_2atmpS1559 = _M0L6_2atmpS3978;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1560
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3977 = moonbit_add_string(_M0L6_2atmpS1559, _M0L6_2atmpS1560);
  moonbit_decref(_M0L6_2atmpS1559);
  moonbit_decref(_M0L6_2atmpS1560);
  _M0L6_2atmpS1558 = _M0L6_2atmpS3977;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3976
  = moonbit_add_string(_M0L6_2atmpS1558, (moonbit_string_t)moonbit_string_literal_66.data);
  moonbit_decref(_M0L6_2atmpS1558);
  _M0L6_2atmpS1557 = _M0L6_2atmpS3976;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1557);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1566;
  moonbit_string_t _M0L6_2atmpS3981;
  moonbit_string_t _M0L6_2atmpS1564;
  moonbit_string_t _M0L6_2atmpS1565;
  moonbit_string_t _M0L6_2atmpS3980;
  moonbit_string_t _M0L6_2atmpS1563;
  moonbit_string_t _M0L6_2atmpS3979;
  moonbit_string_t _M0L6_2atmpS1562;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1566 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3981
  = moonbit_add_string(_M0L6_2atmpS1566, (moonbit_string_t)moonbit_string_literal_116.data);
  moonbit_decref(_M0L6_2atmpS1566);
  _M0L6_2atmpS1564 = _M0L6_2atmpS3981;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1565
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3980 = moonbit_add_string(_M0L6_2atmpS1564, _M0L6_2atmpS1565);
  moonbit_decref(_M0L6_2atmpS1564);
  moonbit_decref(_M0L6_2atmpS1565);
  _M0L6_2atmpS1563 = _M0L6_2atmpS3980;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3979
  = moonbit_add_string(_M0L6_2atmpS1563, (moonbit_string_t)moonbit_string_literal_66.data);
  moonbit_decref(_M0L6_2atmpS1563);
  _M0L6_2atmpS1562 = _M0L6_2atmpS3979;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1562);
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1571;
  moonbit_string_t _M0L6_2atmpS3984;
  moonbit_string_t _M0L6_2atmpS1569;
  moonbit_string_t _M0L6_2atmpS1570;
  moonbit_string_t _M0L6_2atmpS3983;
  moonbit_string_t _M0L6_2atmpS1568;
  moonbit_string_t _M0L6_2atmpS3982;
  moonbit_string_t _M0L6_2atmpS1567;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1571 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3984
  = moonbit_add_string(_M0L6_2atmpS1571, (moonbit_string_t)moonbit_string_literal_116.data);
  moonbit_decref(_M0L6_2atmpS1571);
  _M0L6_2atmpS1569 = _M0L6_2atmpS3984;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1570
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3983 = moonbit_add_string(_M0L6_2atmpS1569, _M0L6_2atmpS1570);
  moonbit_decref(_M0L6_2atmpS1569);
  moonbit_decref(_M0L6_2atmpS1570);
  _M0L6_2atmpS1568 = _M0L6_2atmpS3983;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3982
  = moonbit_add_string(_M0L6_2atmpS1568, (moonbit_string_t)moonbit_string_literal_66.data);
  moonbit_decref(_M0L6_2atmpS1568);
  _M0L6_2atmpS1567 = _M0L6_2atmpS3982;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1567);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5valueS17
) {
  uint32_t _M0L3accS1551;
  uint32_t _M0L6_2atmpS1550;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1551 = _M0L4selfS16->$0;
  _M0L6_2atmpS1550 = _M0L3accS1551 + 4u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1550;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS16, _M0L5valueS17);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5inputS15
) {
  uint32_t _M0L3accS1548;
  uint32_t _M0L6_2atmpS1549;
  uint32_t _M0L6_2atmpS1547;
  uint32_t _M0L6_2atmpS1546;
  uint32_t _M0L6_2atmpS1545;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1548 = _M0L4selfS14->$0;
  _M0L6_2atmpS1549 = _M0L5inputS15 * 3266489917u;
  _M0L6_2atmpS1547 = _M0L3accS1548 + _M0L6_2atmpS1549;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1546 = _M0FPB4rotl(_M0L6_2atmpS1547, 17);
  _M0L6_2atmpS1545 = _M0L6_2atmpS1546 * 668265263u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1545;
  moonbit_decref(_M0L4selfS14);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS12, int32_t _M0L1rS13) {
  uint32_t _M0L6_2atmpS1542;
  int32_t _M0L6_2atmpS1544;
  uint32_t _M0L6_2atmpS1543;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1542 = _M0L1xS12 << (_M0L1rS13 & 31);
  _M0L6_2atmpS1544 = 32 - _M0L1rS13;
  _M0L6_2atmpS1543 = _M0L1xS12 >> (_M0L6_2atmpS1544 & 31);
  return _M0L6_2atmpS1542 | _M0L6_2atmpS1543;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S8,
  struct _M0TPB6Logger _M0L10_2ax__4934S11
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS9;
  moonbit_string_t _M0L8_2afieldS3985;
  int32_t _M0L6_2acntS4096;
  moonbit_string_t _M0L15_2a_2aarg__4935S10;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS9
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S8;
  _M0L8_2afieldS3985 = _M0L10_2aFailureS9->$0;
  _M0L6_2acntS4096 = Moonbit_object_header(_M0L10_2aFailureS9)->rc;
  if (_M0L6_2acntS4096 > 1) {
    int32_t _M0L11_2anew__cntS4097 = _M0L6_2acntS4096 - 1;
    Moonbit_object_header(_M0L10_2aFailureS9)->rc = _M0L11_2anew__cntS4097;
    moonbit_incref(_M0L8_2afieldS3985);
  } else if (_M0L6_2acntS4096 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS9);
  }
  _M0L15_2a_2aarg__4935S10 = _M0L8_2afieldS3985;
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_117.data);
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S11, _M0L15_2a_2aarg__4935S10);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_118.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS7) {
  void* _block_4383;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4383 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4383)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4383)->$0 = _M0L4selfS7;
  return _block_4383;
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

int64_t _M0FPC15abort5abortGOiE(moonbit_string_t _M0L3msgS4) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1477) {
  switch (Moonbit_object_tag(_M0L4_2aeS1477)) {
    case 4: {
      moonbit_decref(_M0L4_2aeS1477);
      return (moonbit_string_t)moonbit_string_literal_119.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1477);
      return (moonbit_string_t)moonbit_string_literal_120.data;
      break;
    }
    
    case 5: {
      moonbit_decref(_M0L4_2aeS1477);
      return (moonbit_string_t)moonbit_string_literal_121.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1477);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1477);
      return (moonbit_string_t)moonbit_string_literal_122.data;
      break;
    }
    default: {
      return _M0IP016_24default__implPB4Show10to__stringGRPC17strconv12StrConvErrorE(_M0L4_2aeS1477);
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1516,
  int32_t _M0L8_2aparamS1515
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1514 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1516;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1514, _M0L8_2aparamS1515);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1513,
  struct _M0TPC16string10StringView _M0L8_2aparamS1512
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1511 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1513;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1511, _M0L8_2aparamS1512);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1510,
  moonbit_string_t _M0L8_2aparamS1507,
  int32_t _M0L8_2aparamS1508,
  int32_t _M0L8_2aparamS1509
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1506 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1510;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1506, _M0L8_2aparamS1507, _M0L8_2aparamS1508, _M0L8_2aparamS1509);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1505,
  moonbit_string_t _M0L8_2aparamS1504
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1503 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1505;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1503, _M0L8_2aparamS1504);
  return 0;
}

void* _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1501
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1502 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1501;
  int32_t _M0L8_2afieldS3986 = _M0L14_2aboxed__selfS1502->$0;
  int32_t _M0L7_2aselfS1500;
  moonbit_decref(_M0L14_2aboxed__selfS1502);
  _M0L7_2aselfS1500 = _M0L8_2afieldS3986;
  return _M0IPC14bool4BoolPB6ToJson8to__json(_M0L7_2aselfS1500);
}

void* _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1499
) {
  struct _M0TP48clawteam8clawteam8internal9buildinfo7Version* _M0L7_2aselfS1498 =
    (struct _M0TP48clawteam8clawteam8internal9buildinfo7Version*)_M0L11_2aobj__ptrS1499;
  return _M0IP48clawteam8clawteam8internal9buildinfo7VersionPB6ToJson8to__json(_M0L7_2aselfS1498);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1541 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1540;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1536;
  moonbit_string_t* _M0L6_2atmpS1539;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1538;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1537;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1402;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1535;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1534;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1533;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1524;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1403;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1532;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1531;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1530;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1525;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1404;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1529;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1528;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1527;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1526;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1401;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1523;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1522;
  _M0L6_2atmpS1541[0] = (moonbit_string_t)moonbit_string_literal_123.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test51____test__76657273696f6e5f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1540
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1540)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1540->$0
  = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test51____test__76657273696f6e5f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1540->$1 = _M0L6_2atmpS1541;
  _M0L8_2atupleS1536
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1536)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1536->$0 = 0;
  _M0L8_2atupleS1536->$1 = _M0L8_2atupleS1540;
  _M0L6_2atmpS1539 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1539[0] = (moonbit_string_t)moonbit_string_literal_124.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test51____test__76657273696f6e5f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1538
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1538)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1538->$0
  = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test51____test__76657273696f6e5f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1538->$1 = _M0L6_2atmpS1539;
  _M0L8_2atupleS1537
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1537)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1537->$0 = 1;
  _M0L8_2atupleS1537->$1 = _M0L8_2atupleS1538;
  _M0L7_2abindS1402
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1402[0] = _M0L8_2atupleS1536;
  _M0L7_2abindS1402[1] = _M0L8_2atupleS1537;
  _M0L6_2atmpS1535 = _M0L7_2abindS1402;
  _M0L6_2atmpS1534
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 2, _M0L6_2atmpS1535
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1533
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1534);
  _M0L8_2atupleS1524
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1524)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1524->$0 = (moonbit_string_t)moonbit_string_literal_125.data;
  _M0L8_2atupleS1524->$1 = _M0L6_2atmpS1533;
  _M0L7_2abindS1403
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1532 = _M0L7_2abindS1403;
  _M0L6_2atmpS1531
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1532
  };
  #line 402 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1530
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1531);
  _M0L8_2atupleS1525
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1525)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1525->$0 = (moonbit_string_t)moonbit_string_literal_126.data;
  _M0L8_2atupleS1525->$1 = _M0L6_2atmpS1530;
  _M0L7_2abindS1404
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1529 = _M0L7_2abindS1404;
  _M0L6_2atmpS1528
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1529
  };
  #line 404 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1527
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1528);
  _M0L8_2atupleS1526
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1526)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1526->$0 = (moonbit_string_t)moonbit_string_literal_127.data;
  _M0L8_2atupleS1526->$1 = _M0L6_2atmpS1527;
  _M0L7_2abindS1401
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1401[0] = _M0L8_2atupleS1524;
  _M0L7_2abindS1401[1] = _M0L8_2atupleS1525;
  _M0L7_2abindS1401[2] = _M0L8_2atupleS1526;
  _M0L6_2atmpS1523 = _M0L7_2abindS1401;
  _M0L6_2atmpS1522
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 3, _M0L6_2atmpS1523
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1522);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1521;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1471;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1472;
  int32_t _M0L7_2abindS1473;
  int32_t _M0L2__S1474;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1521
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1471
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1471)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1471->$0 = _M0L6_2atmpS1521;
  _M0L12async__testsS1471->$1 = 0;
  #line 443 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1472
  = _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1473 = _M0L7_2abindS1472->$1;
  _M0L2__S1474 = 0;
  while (1) {
    if (_M0L2__S1474 < _M0L7_2abindS1473) {
      struct _M0TUsiE** _M0L8_2afieldS3990 = _M0L7_2abindS1472->$0;
      struct _M0TUsiE** _M0L3bufS1520 = _M0L8_2afieldS3990;
      struct _M0TUsiE* _M0L6_2atmpS3989 =
        (struct _M0TUsiE*)_M0L3bufS1520[_M0L2__S1474];
      struct _M0TUsiE* _M0L3argS1475 = _M0L6_2atmpS3989;
      moonbit_string_t _M0L8_2afieldS3988 = _M0L3argS1475->$0;
      moonbit_string_t _M0L6_2atmpS1517 = _M0L8_2afieldS3988;
      int32_t _M0L8_2afieldS3987 = _M0L3argS1475->$1;
      int32_t _M0L6_2atmpS1518 = _M0L8_2afieldS3987;
      int32_t _M0L6_2atmpS1519;
      moonbit_incref(_M0L6_2atmpS1517);
      moonbit_incref(_M0L12async__testsS1471);
      #line 444 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal25buildinfo__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1471, _M0L6_2atmpS1517, _M0L6_2atmpS1518);
      _M0L6_2atmpS1519 = _M0L2__S1474 + 1;
      _M0L2__S1474 = _M0L6_2atmpS1519;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1472);
    }
    break;
  }
  #line 446 "E:\\moonbit\\clawteam\\internal\\buildinfo\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal25buildinfo__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal25buildinfo__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1471);
  return 0;
}