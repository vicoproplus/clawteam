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
struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPC15error5ErrorE2Ok;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0TWssbEu;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOc;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools17replace__in__file5InputRPC14json15JsonDecodeErrorE3Err;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPC15error5ErrorE3Err;

struct _M0R38String_3a_3aiter_2eanon__u2098__l247__;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGRPB4JsonRPC15error5ErrorE3Err;

struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools17replace__in__file33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0TWEOs;

struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok;

struct _M0DTPC14json8JsonPath3Key;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TURPC14json8JsonPathsE;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0KTPB6ToJsonTPB5ArrayGRPB4JsonE;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TPC13ref3RefGOOsE;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input;

struct _M0DTPC14json8JsonPath5Index;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16option6OptionGOsE4Some;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TWRPB4JsonERPB4Json;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE3Err;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok;

struct _M0DTPC16result6ResultGRPB4JsonRPC15error5ErrorE2Ok;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__;

struct _M0TPC13ref3RefGOsE;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools17replace__in__file33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE2Ok;

struct _M0TUsRPB4JsonE;

struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools17replace__in__file5InputRPC14json15JsonDecodeErrorE2Ok;

struct _M0DTPB4Json6Object;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPC15error5ErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
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

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools17replace__in__file5InputRPC14json15JsonDecodeErrorE3Err {
  void* $0;
  
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

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2098__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
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

struct _M0DTPC16result6ResultGRPB4JsonRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools17replace__in__file33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error {
  struct moonbit_result_1(* code)(
    struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error*,
    void*
  );
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0DTPC14json8JsonPath3Key {
  void* $0;
  moonbit_string_t $1;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TURPC14json8JsonPathsE {
  void* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0KTPB6ToJsonTPB5ArrayGRPB4JsonE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
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

struct _M0TPC13ref3RefGOOsE {
  void* $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  
};

struct _M0DTPC14json8JsonPath5Index {
  int32_t $1;
  void* $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16option6OptionGOsE4Some {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError {
  struct _M0TURPC14json8JsonPathsE* $0;
  
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

struct _M0TWRPB4JsonERPB4Json {
  void*(* code)(struct _M0TWRPB4JsonERPB4Json*, void*);
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE3Err {
  void* $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok {
  void* $0;
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPC15error5ErrorE2Ok {
  void* $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0TPC13ref3RefGOsE {
  moonbit_string_t $0;
  
};

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools17replace__in__file33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE2Ok {
  moonbit_string_t $0;
  
};

struct _M0TUsRPB4JsonE {
  moonbit_string_t $0;
  void* $1;
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $2_0;
  
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

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools17replace__in__file5InputRPC14json15JsonDecodeErrorE2Ok {
  struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input* $0;
  
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

struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct moonbit_result_3 {
  int tag;
  union {
    struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input* ok;
    void* err;
    
  } data;
  
};

struct moonbit_result_2 {
  int tag;
  union { struct _M0TPB5ArrayGRPB4JsonE* ok; void* err;  } data;
  
};

struct moonbit_result_1 {
  int tag;
  union { void* ok; void* err;  } data;
  
};

struct moonbit_result_4 {
  int tag;
  union { moonbit_string_t ok; void* err;  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools17replace__in__file43____test__736368656d612e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IPC14json4JsonPB6ToJson18to__json_2edyncall(
  struct _M0TWRPB4JsonERPB4Json*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1387(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN14handle__resultS1378(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testC3090l427(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testC3086l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1311(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1306(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1293(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools17replace__in__file34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0(
  
);

struct moonbit_result_1 _M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0N11round__tripS1267(
  struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error*,
  void*
);

void* _M0IP48clawteam8clawteam5tools17replace__in__file5InputPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input*
);

struct moonbit_result_3 _M0IP48clawteam8clawteam5tools17replace__in__file5InputPC14json8FromJson10from__json(
  void*,
  void*
);

struct moonbit_result_4 _M0IPC16string6StringPC14json8FromJson10from__json(
  void*,
  void*
);

struct moonbit_result_3 _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools17replace__in__file5InputE(
  void*,
  void*
);

struct moonbit_result_3 _M0FPC14json18from__json_2einnerGRP48clawteam8clawteam5tools17replace__in__file5InputE(
  void*,
  void*
);

void* _M0IPC14json4JsonPB6ToJson8to__json(void*);

int32_t _M0IPC14json8JsonPathPB4Show6output(void*, struct _M0TPB6Logger);

int32_t _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(
  void*,
  struct _M0TPB6Logger
);

void* _M0MPC14json8JsonPath8add__key(void*, moonbit_string_t);

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

struct moonbit_result_4 _M0FPC14json13decode__errorGsE(
  void*,
  moonbit_string_t
);

int32_t _M0IPC14json15JsonDecodeErrorPB4Show6output(
  void*,
  struct _M0TPB6Logger
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

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*
);

struct moonbit_result_2 _M0MPC15array5Array3mapGRPB4JsonRPB4JsonEHRPC15error5Error(
  struct _M0TPB5ArrayGRPB4JsonE*,
  struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRPB4JsonRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*,
  struct _M0TWRPB4JsonERPB4Json*
);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2382l591(
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

void* _M0MPB3Map3getGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
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

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2117l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2098l247(struct _M0TWEOc*);

int32_t _M0MPC16string6String13contains__any(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView13contains__any(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView14contains__char(
  struct _M0TPC16string10StringView,
  int32_t
);

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

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView4iterC1971l198(struct _M0TWEOc*);

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

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView,
  int32_t
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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPC14json15JsonDecodeErrorE(
  void*
);

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

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE*);

int32_t _M0MPB6Logger13write__objectGRPC14json8JsonPathE(
  struct _M0TPB6Logger,
  void*
);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

int32_t _M0MPB6Logger13write__objectGiE(struct _M0TPB6Logger, int32_t);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

int64_t _M0FPC15abort5abortGOiE(moonbit_string_t);

moonbit_string_t _M0FP15Error10to__string(void*);

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPB4JsonE(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_1 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    112, 97, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 116, 111, 111, 108, 115, 47, 114, 101, 112, 108, 97, 99, 101, 
    95, 105, 110, 95, 102, 105, 108, 101, 34, 44, 32, 34, 102, 105, 108, 
    101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    77, 105, 115, 115, 105, 110, 103, 32, 102, 105, 101, 108, 100, 32, 
    114, 101, 112, 108, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[108]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 107), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 112, 108, 97, 
    99, 101, 95, 105, 110, 95, 102, 105, 108, 101, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    110, 101, 119, 32, 99, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[63]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 62), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 112, 108, 
    97, 99, 101, 95, 105, 110, 95, 102, 105, 108, 101, 58, 115, 99, 104, 
    101, 109, 97, 46, 109, 98, 116, 58, 52, 49, 58, 53, 50, 45, 52, 52, 
    58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 101, 97, 114, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    74, 115, 111, 110, 68, 101, 99, 111, 100, 101, 69, 114, 114, 111, 
    114, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_58 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    77, 105, 115, 115, 105, 110, 103, 32, 102, 105, 101, 108, 100, 32, 
    112, 97, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 50, 54, 58, 57, 45, 
    52, 50, 54, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    73, 110, 112, 117, 116, 32, 70, 114, 111, 109, 74, 115, 111, 110, 
    47, 84, 111, 74, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_57 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_44 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_35 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    114, 101, 112, 108, 97, 99, 101, 95, 105, 110, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    101, 120, 97, 109, 112, 108, 101, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    44, 32, 34, 109, 101, 115, 115, 97, 103, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_62 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    83, 116, 114, 105, 110, 103, 58, 58, 102, 114, 111, 109, 95, 106, 
    115, 111, 110, 58, 32, 101, 120, 112, 101, 99, 116, 101, 100, 32, 
    115, 116, 114, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    111, 108, 100, 32, 99, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    114, 101, 112, 108, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_78 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_55 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 40, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 112, 108, 
    97, 99, 101, 95, 105, 110, 95, 102, 105, 108, 101, 58, 115, 99, 104, 
    101, 109, 97, 46, 109, 98, 116, 58, 52, 49, 58, 49, 54, 45, 52, 49, 
    58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    126, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    69, 120, 112, 101, 99, 116, 101, 100, 32, 111, 98, 106, 101, 99, 
    116, 32, 116, 111, 32, 100, 101, 115, 101, 114, 105, 97, 108, 105, 
    122, 101, 32, 73, 110, 112, 117, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 112, 108, 
    97, 99, 101, 95, 105, 110, 95, 102, 105, 108, 101, 58, 115, 99, 104, 
    101, 109, 97, 46, 109, 98, 116, 58, 52, 49, 58, 51, 45, 52, 52, 58, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    126, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 99, 104, 101, 109, 97, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[106]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 105), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 112, 108, 97, 
    99, 101, 95, 105, 110, 95, 102, 105, 108, 101, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 
    114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 
    115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    126, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1387$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1387
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error data;
  
} const _M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0N11round__tripS1267$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0N11round__tripS1267
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPB4JsonERPB4Json data; 
} const _M0IPC14json4JsonPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IPC14json4JsonPB6ToJson18to__json_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam5tools17replace__in__file43____test__736368656d612e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools17replace__in__file43____test__736368656d612e6d6274__0_2edyncall
  };

struct _M0TWRPB4JsonERPB4Json* _M0IPC14json4JsonPB6ToJson14to__json_2eclo =
  (struct _M0TWRPB4JsonERPB4Json*)&_M0IPC14json4JsonPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam5tools17replace__in__file39____test__736368656d612e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam5tools17replace__in__file43____test__736368656d612e6d6274__0_2edyncall$closure.data;

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
} _M0FP0152moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPB4JsonE}
  };

struct _M0BTPB6ToJson* _M0FP0152moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0152moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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

moonbit_string_t _M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376 =
  (moonbit_string_t)moonbit_string_literal_0.data;

struct { int32_t rc; uint32_t meta; struct _M0TPB17FloatingDecimal64 data; 
} _M0FPB31ryu__to__string_2erecord_2f1063$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1063 =
  &_M0FPB31ryu__to__string_2erecord_2f1063$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam5tools17replace__in__file48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools17replace__in__file43____test__736368656d612e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3122
) {
  return _M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0();
}

void* _M0IPC14json4JsonPB6ToJson18to__json_2edyncall(
  struct _M0TWRPB4JsonERPB4Json* _M0L6_2aenvS3121,
  void* _M0L4selfS1203
) {
  return _M0IPC14json4JsonPB6ToJson8to__json(_M0L4selfS1203);
}

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1408,
  moonbit_string_t _M0L8filenameS1383,
  int32_t _M0L5indexS1386
) {
  struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378* _closure_3560;
  struct _M0TWssbEu* _M0L14handle__resultS1378;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1387;
  void* _M0L11_2atry__errS1402;
  struct moonbit_result_0 _tmp_3562;
  int32_t _handle__error__result_3563;
  int32_t _M0L6_2atmpS3109;
  void* _M0L3errS1403;
  moonbit_string_t _M0L4nameS1405;
  struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1406;
  moonbit_string_t _M0L8_2afieldS3123;
  int32_t _M0L6_2acntS3441;
  moonbit_string_t _M0L7_2anameS1407;
  #line 526 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1383);
  _closure_3560
  = (struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378*)moonbit_malloc(sizeof(struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378));
  Moonbit_object_header(_closure_3560)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378, $1) >> 2, 1, 0);
  _closure_3560->code
  = &_M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN14handle__resultS1378;
  _closure_3560->$0 = _M0L5indexS1386;
  _closure_3560->$1 = _M0L8filenameS1383;
  _M0L14handle__resultS1378 = (struct _M0TWssbEu*)_closure_3560;
  _M0L17error__to__stringS1387
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1387$closure.data;
  moonbit_incref(_M0L12async__testsS1408);
  moonbit_incref(_M0L17error__to__stringS1387);
  moonbit_incref(_M0L8filenameS1383);
  moonbit_incref(_M0L14handle__resultS1378);
  #line 560 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _tmp_3562
  = _M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__test(_M0L12async__testsS1408, _M0L8filenameS1383, _M0L5indexS1386, _M0L14handle__resultS1378, _M0L17error__to__stringS1387);
  if (_tmp_3562.tag) {
    int32_t const _M0L5_2aokS3118 = _tmp_3562.data.ok;
    _handle__error__result_3563 = _M0L5_2aokS3118;
  } else {
    void* const _M0L6_2aerrS3119 = _tmp_3562.data.err;
    moonbit_decref(_M0L12async__testsS1408);
    moonbit_decref(_M0L17error__to__stringS1387);
    moonbit_decref(_M0L8filenameS1383);
    _M0L11_2atry__errS1402 = _M0L6_2aerrS3119;
    goto join_1401;
  }
  if (_handle__error__result_3563) {
    moonbit_decref(_M0L12async__testsS1408);
    moonbit_decref(_M0L17error__to__stringS1387);
    moonbit_decref(_M0L8filenameS1383);
    _M0L6_2atmpS3109 = 1;
  } else {
    struct moonbit_result_0 _tmp_3564;
    int32_t _handle__error__result_3565;
    moonbit_incref(_M0L12async__testsS1408);
    moonbit_incref(_M0L17error__to__stringS1387);
    moonbit_incref(_M0L8filenameS1383);
    moonbit_incref(_M0L14handle__resultS1378);
    #line 563 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    _tmp_3564
    = _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1408, _M0L8filenameS1383, _M0L5indexS1386, _M0L14handle__resultS1378, _M0L17error__to__stringS1387);
    if (_tmp_3564.tag) {
      int32_t const _M0L5_2aokS3116 = _tmp_3564.data.ok;
      _handle__error__result_3565 = _M0L5_2aokS3116;
    } else {
      void* const _M0L6_2aerrS3117 = _tmp_3564.data.err;
      moonbit_decref(_M0L12async__testsS1408);
      moonbit_decref(_M0L17error__to__stringS1387);
      moonbit_decref(_M0L8filenameS1383);
      _M0L11_2atry__errS1402 = _M0L6_2aerrS3117;
      goto join_1401;
    }
    if (_handle__error__result_3565) {
      moonbit_decref(_M0L12async__testsS1408);
      moonbit_decref(_M0L17error__to__stringS1387);
      moonbit_decref(_M0L8filenameS1383);
      _M0L6_2atmpS3109 = 1;
    } else {
      struct moonbit_result_0 _tmp_3566;
      int32_t _handle__error__result_3567;
      moonbit_incref(_M0L12async__testsS1408);
      moonbit_incref(_M0L17error__to__stringS1387);
      moonbit_incref(_M0L8filenameS1383);
      moonbit_incref(_M0L14handle__resultS1378);
      #line 566 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _tmp_3566
      = _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1408, _M0L8filenameS1383, _M0L5indexS1386, _M0L14handle__resultS1378, _M0L17error__to__stringS1387);
      if (_tmp_3566.tag) {
        int32_t const _M0L5_2aokS3114 = _tmp_3566.data.ok;
        _handle__error__result_3567 = _M0L5_2aokS3114;
      } else {
        void* const _M0L6_2aerrS3115 = _tmp_3566.data.err;
        moonbit_decref(_M0L12async__testsS1408);
        moonbit_decref(_M0L17error__to__stringS1387);
        moonbit_decref(_M0L8filenameS1383);
        _M0L11_2atry__errS1402 = _M0L6_2aerrS3115;
        goto join_1401;
      }
      if (_handle__error__result_3567) {
        moonbit_decref(_M0L12async__testsS1408);
        moonbit_decref(_M0L17error__to__stringS1387);
        moonbit_decref(_M0L8filenameS1383);
        _M0L6_2atmpS3109 = 1;
      } else {
        struct moonbit_result_0 _tmp_3568;
        int32_t _handle__error__result_3569;
        moonbit_incref(_M0L12async__testsS1408);
        moonbit_incref(_M0L17error__to__stringS1387);
        moonbit_incref(_M0L8filenameS1383);
        moonbit_incref(_M0L14handle__resultS1378);
        #line 569 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        _tmp_3568
        = _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1408, _M0L8filenameS1383, _M0L5indexS1386, _M0L14handle__resultS1378, _M0L17error__to__stringS1387);
        if (_tmp_3568.tag) {
          int32_t const _M0L5_2aokS3112 = _tmp_3568.data.ok;
          _handle__error__result_3569 = _M0L5_2aokS3112;
        } else {
          void* const _M0L6_2aerrS3113 = _tmp_3568.data.err;
          moonbit_decref(_M0L12async__testsS1408);
          moonbit_decref(_M0L17error__to__stringS1387);
          moonbit_decref(_M0L8filenameS1383);
          _M0L11_2atry__errS1402 = _M0L6_2aerrS3113;
          goto join_1401;
        }
        if (_handle__error__result_3569) {
          moonbit_decref(_M0L12async__testsS1408);
          moonbit_decref(_M0L17error__to__stringS1387);
          moonbit_decref(_M0L8filenameS1383);
          _M0L6_2atmpS3109 = 1;
        } else {
          struct moonbit_result_0 _tmp_3570;
          moonbit_incref(_M0L14handle__resultS1378);
          #line 572 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
          _tmp_3570
          = _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1408, _M0L8filenameS1383, _M0L5indexS1386, _M0L14handle__resultS1378, _M0L17error__to__stringS1387);
          if (_tmp_3570.tag) {
            int32_t const _M0L5_2aokS3110 = _tmp_3570.data.ok;
            _M0L6_2atmpS3109 = _M0L5_2aokS3110;
          } else {
            void* const _M0L6_2aerrS3111 = _tmp_3570.data.err;
            _M0L11_2atry__errS1402 = _M0L6_2aerrS3111;
            goto join_1401;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3109) {
    void* _M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3120 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3120)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
    ((struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3120)->$0
    = (moonbit_string_t)moonbit_string_literal_1.data;
    _M0L11_2atry__errS1402
    = _M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3120;
    goto join_1401;
  } else {
    moonbit_decref(_M0L14handle__resultS1378);
  }
  goto joinlet_3561;
  join_1401:;
  _M0L3errS1403 = _M0L11_2atry__errS1402;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1406
  = (struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1403;
  _M0L8_2afieldS3123 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1406->$0;
  _M0L6_2acntS3441
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1406)->rc;
  if (_M0L6_2acntS3441 > 1) {
    int32_t _M0L11_2anew__cntS3442 = _M0L6_2acntS3441 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1406)->rc
    = _M0L11_2anew__cntS3442;
    moonbit_incref(_M0L8_2afieldS3123);
  } else if (_M0L6_2acntS3441 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1406);
  }
  _M0L7_2anameS1407 = _M0L8_2afieldS3123;
  _M0L4nameS1405 = _M0L7_2anameS1407;
  goto join_1404;
  goto joinlet_3571;
  join_1404:;
  #line 580 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN14handle__resultS1378(_M0L14handle__resultS1378, _M0L4nameS1405, (moonbit_string_t)moonbit_string_literal_2.data, 1);
  joinlet_3571:;
  joinlet_3561:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1387(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3108,
  void* _M0L3errS1388
) {
  void* _M0L1eS1390;
  moonbit_string_t _M0L1eS1392;
  #line 549 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3108);
  switch (Moonbit_object_tag(_M0L3errS1388)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1393 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1388;
      moonbit_string_t _M0L8_2afieldS3124 = _M0L10_2aFailureS1393->$0;
      int32_t _M0L6_2acntS3443 =
        Moonbit_object_header(_M0L10_2aFailureS1393)->rc;
      moonbit_string_t _M0L4_2aeS1394;
      if (_M0L6_2acntS3443 > 1) {
        int32_t _M0L11_2anew__cntS3444 = _M0L6_2acntS3443 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1393)->rc
        = _M0L11_2anew__cntS3444;
        moonbit_incref(_M0L8_2afieldS3124);
      } else if (_M0L6_2acntS3443 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1393);
      }
      _M0L4_2aeS1394 = _M0L8_2afieldS3124;
      _M0L1eS1392 = _M0L4_2aeS1394;
      goto join_1391;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1395 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1388;
      moonbit_string_t _M0L8_2afieldS3125 = _M0L15_2aInspectErrorS1395->$0;
      int32_t _M0L6_2acntS3445 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1395)->rc;
      moonbit_string_t _M0L4_2aeS1396;
      if (_M0L6_2acntS3445 > 1) {
        int32_t _M0L11_2anew__cntS3446 = _M0L6_2acntS3445 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1395)->rc
        = _M0L11_2anew__cntS3446;
        moonbit_incref(_M0L8_2afieldS3125);
      } else if (_M0L6_2acntS3445 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1395);
      }
      _M0L4_2aeS1396 = _M0L8_2afieldS3125;
      _M0L1eS1392 = _M0L4_2aeS1396;
      goto join_1391;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1397 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1388;
      moonbit_string_t _M0L8_2afieldS3126 = _M0L16_2aSnapshotErrorS1397->$0;
      int32_t _M0L6_2acntS3447 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1397)->rc;
      moonbit_string_t _M0L4_2aeS1398;
      if (_M0L6_2acntS3447 > 1) {
        int32_t _M0L11_2anew__cntS3448 = _M0L6_2acntS3447 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1397)->rc
        = _M0L11_2anew__cntS3448;
        moonbit_incref(_M0L8_2afieldS3126);
      } else if (_M0L6_2acntS3447 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1397);
      }
      _M0L4_2aeS1398 = _M0L8_2afieldS3126;
      _M0L1eS1392 = _M0L4_2aeS1398;
      goto join_1391;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1399 =
        (struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1388;
      moonbit_string_t _M0L8_2afieldS3127 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1399->$0;
      int32_t _M0L6_2acntS3449 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1399)->rc;
      moonbit_string_t _M0L4_2aeS1400;
      if (_M0L6_2acntS3449 > 1) {
        int32_t _M0L11_2anew__cntS3450 = _M0L6_2acntS3449 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1399)->rc
        = _M0L11_2anew__cntS3450;
        moonbit_incref(_M0L8_2afieldS3127);
      } else if (_M0L6_2acntS3449 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1399);
      }
      _M0L4_2aeS1400 = _M0L8_2afieldS3127;
      _M0L1eS1392 = _M0L4_2aeS1400;
      goto join_1391;
      break;
    }
    default: {
      _M0L1eS1390 = _M0L3errS1388;
      goto join_1389;
      break;
    }
  }
  join_1391:;
  return _M0L1eS1392;
  join_1389:;
  #line 555 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1390);
}

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__executeN14handle__resultS1378(
  struct _M0TWssbEu* _M0L6_2aenvS3094,
  moonbit_string_t _M0L8testnameS1379,
  moonbit_string_t _M0L7messageS1380,
  int32_t _M0L7skippedS1381
) {
  struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378* _M0L14_2acasted__envS3095;
  moonbit_string_t _M0L8_2afieldS3137;
  moonbit_string_t _M0L8filenameS1383;
  int32_t _M0L8_2afieldS3136;
  int32_t _M0L6_2acntS3451;
  int32_t _M0L5indexS1386;
  int32_t _if__result_3574;
  moonbit_string_t _M0L10file__nameS1382;
  moonbit_string_t _M0L10test__nameS1384;
  moonbit_string_t _M0L7messageS1385;
  moonbit_string_t _M0L6_2atmpS3107;
  moonbit_string_t _M0L6_2atmpS3135;
  moonbit_string_t _M0L6_2atmpS3106;
  moonbit_string_t _M0L6_2atmpS3134;
  moonbit_string_t _M0L6_2atmpS3104;
  moonbit_string_t _M0L6_2atmpS3105;
  moonbit_string_t _M0L6_2atmpS3133;
  moonbit_string_t _M0L6_2atmpS3103;
  moonbit_string_t _M0L6_2atmpS3132;
  moonbit_string_t _M0L6_2atmpS3101;
  moonbit_string_t _M0L6_2atmpS3102;
  moonbit_string_t _M0L6_2atmpS3131;
  moonbit_string_t _M0L6_2atmpS3100;
  moonbit_string_t _M0L6_2atmpS3130;
  moonbit_string_t _M0L6_2atmpS3098;
  moonbit_string_t _M0L6_2atmpS3099;
  moonbit_string_t _M0L6_2atmpS3129;
  moonbit_string_t _M0L6_2atmpS3097;
  moonbit_string_t _M0L6_2atmpS3128;
  moonbit_string_t _M0L6_2atmpS3096;
  #line 533 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3095
  = (struct _M0R121_24clawteam_2fclawteam_2ftools_2freplace__in__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1378*)_M0L6_2aenvS3094;
  _M0L8_2afieldS3137 = _M0L14_2acasted__envS3095->$1;
  _M0L8filenameS1383 = _M0L8_2afieldS3137;
  _M0L8_2afieldS3136 = _M0L14_2acasted__envS3095->$0;
  _M0L6_2acntS3451 = Moonbit_object_header(_M0L14_2acasted__envS3095)->rc;
  if (_M0L6_2acntS3451 > 1) {
    int32_t _M0L11_2anew__cntS3452 = _M0L6_2acntS3451 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3095)->rc
    = _M0L11_2anew__cntS3452;
    moonbit_incref(_M0L8filenameS1383);
  } else if (_M0L6_2acntS3451 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3095);
  }
  _M0L5indexS1386 = _M0L8_2afieldS3136;
  if (!_M0L7skippedS1381) {
    _if__result_3574 = 1;
  } else {
    _if__result_3574 = 0;
  }
  if (_if__result_3574) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1382 = _M0MPC16string6String6escape(_M0L8filenameS1383);
  #line 540 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1384 = _M0MPC16string6String6escape(_M0L8testnameS1379);
  #line 541 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1385 = _M0MPC16string6String6escape(_M0L7messageS1380);
  #line 542 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_3.data);
  #line 544 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3107
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1382);
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3135
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_4.data, _M0L6_2atmpS3107);
  moonbit_decref(_M0L6_2atmpS3107);
  _M0L6_2atmpS3106 = _M0L6_2atmpS3135;
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3134
  = moonbit_add_string(_M0L6_2atmpS3106, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3106);
  _M0L6_2atmpS3104 = _M0L6_2atmpS3134;
  #line 544 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3105
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1386);
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3133 = moonbit_add_string(_M0L6_2atmpS3104, _M0L6_2atmpS3105);
  moonbit_decref(_M0L6_2atmpS3104);
  moonbit_decref(_M0L6_2atmpS3105);
  _M0L6_2atmpS3103 = _M0L6_2atmpS3133;
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3132
  = moonbit_add_string(_M0L6_2atmpS3103, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3103);
  _M0L6_2atmpS3101 = _M0L6_2atmpS3132;
  #line 544 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3102
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1384);
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3131 = moonbit_add_string(_M0L6_2atmpS3101, _M0L6_2atmpS3102);
  moonbit_decref(_M0L6_2atmpS3101);
  moonbit_decref(_M0L6_2atmpS3102);
  _M0L6_2atmpS3100 = _M0L6_2atmpS3131;
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3130
  = moonbit_add_string(_M0L6_2atmpS3100, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3100);
  _M0L6_2atmpS3098 = _M0L6_2atmpS3130;
  #line 544 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3099
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1385);
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3129 = moonbit_add_string(_M0L6_2atmpS3098, _M0L6_2atmpS3099);
  moonbit_decref(_M0L6_2atmpS3098);
  moonbit_decref(_M0L6_2atmpS3099);
  _M0L6_2atmpS3097 = _M0L6_2atmpS3129;
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3128
  = moonbit_add_string(_M0L6_2atmpS3097, (moonbit_string_t)moonbit_string_literal_8.data);
  moonbit_decref(_M0L6_2atmpS3097);
  _M0L6_2atmpS3096 = _M0L6_2atmpS3128;
  #line 543 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3096);
  #line 546 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_9.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1377,
  moonbit_string_t _M0L8filenameS1374,
  int32_t _M0L5indexS1368,
  struct _M0TWssbEu* _M0L14handle__resultS1364,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1366
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1344;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1373;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1346;
  moonbit_string_t* _M0L5attrsS1347;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1367;
  moonbit_string_t _M0L4nameS1350;
  moonbit_string_t _M0L4nameS1348;
  int32_t _M0L6_2atmpS3093;
  struct _M0TWEOs* _M0L5_2aitS1352;
  struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__* _closure_3583;
  struct _M0TWEOc* _M0L6_2atmpS3084;
  struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__* _closure_3584;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3085;
  struct moonbit_result_0 _result_3585;
  #line 407 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1377);
  moonbit_incref(_M0FP48clawteam8clawteam5tools17replace__in__file48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1373
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam5tools17replace__in__file48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1374);
  if (_M0L7_2abindS1373 == 0) {
    struct moonbit_result_0 _result_3576;
    if (_M0L7_2abindS1373) {
      moonbit_decref(_M0L7_2abindS1373);
    }
    moonbit_decref(_M0L17error__to__stringS1366);
    moonbit_decref(_M0L14handle__resultS1364);
    _result_3576.tag = 1;
    _result_3576.data.ok = 0;
    return _result_3576;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1375 =
      _M0L7_2abindS1373;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1376 =
      _M0L7_2aSomeS1375;
    _M0L10index__mapS1344 = _M0L13_2aindex__mapS1376;
    goto join_1343;
  }
  join_1343:;
  #line 416 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1367
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1344, _M0L5indexS1368);
  if (_M0L7_2abindS1367 == 0) {
    struct moonbit_result_0 _result_3578;
    if (_M0L7_2abindS1367) {
      moonbit_decref(_M0L7_2abindS1367);
    }
    moonbit_decref(_M0L17error__to__stringS1366);
    moonbit_decref(_M0L14handle__resultS1364);
    _result_3578.tag = 1;
    _result_3578.data.ok = 0;
    return _result_3578;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1369 =
      _M0L7_2abindS1367;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1370 = _M0L7_2aSomeS1369;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3141 = _M0L4_2axS1370->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1371 = _M0L8_2afieldS3141;
    moonbit_string_t* _M0L8_2afieldS3140 = _M0L4_2axS1370->$1;
    int32_t _M0L6_2acntS3453 = Moonbit_object_header(_M0L4_2axS1370)->rc;
    moonbit_string_t* _M0L8_2aattrsS1372;
    if (_M0L6_2acntS3453 > 1) {
      int32_t _M0L11_2anew__cntS3454 = _M0L6_2acntS3453 - 1;
      Moonbit_object_header(_M0L4_2axS1370)->rc = _M0L11_2anew__cntS3454;
      moonbit_incref(_M0L8_2afieldS3140);
      moonbit_incref(_M0L4_2afS1371);
    } else if (_M0L6_2acntS3453 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1370);
    }
    _M0L8_2aattrsS1372 = _M0L8_2afieldS3140;
    _M0L1fS1346 = _M0L4_2afS1371;
    _M0L5attrsS1347 = _M0L8_2aattrsS1372;
    goto join_1345;
  }
  join_1345:;
  _M0L6_2atmpS3093 = Moonbit_array_length(_M0L5attrsS1347);
  if (_M0L6_2atmpS3093 >= 1) {
    moonbit_string_t _M0L6_2atmpS3139 = (moonbit_string_t)_M0L5attrsS1347[0];
    moonbit_string_t _M0L7_2anameS1351 = _M0L6_2atmpS3139;
    moonbit_incref(_M0L7_2anameS1351);
    _M0L4nameS1350 = _M0L7_2anameS1351;
    goto join_1349;
  } else {
    _M0L4nameS1348 = (moonbit_string_t)moonbit_string_literal_1.data;
  }
  goto joinlet_3579;
  join_1349:;
  _M0L4nameS1348 = _M0L4nameS1350;
  joinlet_3579:;
  #line 417 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1352 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1347);
  while (1) {
    moonbit_string_t _M0L4attrS1354;
    moonbit_string_t _M0L7_2abindS1361;
    int32_t _M0L6_2atmpS3077;
    int64_t _M0L6_2atmpS3076;
    moonbit_incref(_M0L5_2aitS1352);
    #line 419 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1361 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1352);
    if (_M0L7_2abindS1361 == 0) {
      if (_M0L7_2abindS1361) {
        moonbit_decref(_M0L7_2abindS1361);
      }
      moonbit_decref(_M0L5_2aitS1352);
    } else {
      moonbit_string_t _M0L7_2aSomeS1362 = _M0L7_2abindS1361;
      moonbit_string_t _M0L7_2aattrS1363 = _M0L7_2aSomeS1362;
      _M0L4attrS1354 = _M0L7_2aattrS1363;
      goto join_1353;
    }
    goto joinlet_3581;
    join_1353:;
    _M0L6_2atmpS3077 = Moonbit_array_length(_M0L4attrS1354);
    _M0L6_2atmpS3076 = (int64_t)_M0L6_2atmpS3077;
    moonbit_incref(_M0L4attrS1354);
    #line 420 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1354, 5, 0, _M0L6_2atmpS3076)
    ) {
      int32_t _M0L6_2atmpS3083 = _M0L4attrS1354[0];
      int32_t _M0L4_2axS1355 = _M0L6_2atmpS3083;
      if (_M0L4_2axS1355 == 112) {
        int32_t _M0L6_2atmpS3082 = _M0L4attrS1354[1];
        int32_t _M0L4_2axS1356 = _M0L6_2atmpS3082;
        if (_M0L4_2axS1356 == 97) {
          int32_t _M0L6_2atmpS3081 = _M0L4attrS1354[2];
          int32_t _M0L4_2axS1357 = _M0L6_2atmpS3081;
          if (_M0L4_2axS1357 == 110) {
            int32_t _M0L6_2atmpS3080 = _M0L4attrS1354[3];
            int32_t _M0L4_2axS1358 = _M0L6_2atmpS3080;
            if (_M0L4_2axS1358 == 105) {
              int32_t _M0L6_2atmpS3138 = _M0L4attrS1354[4];
              int32_t _M0L6_2atmpS3079;
              int32_t _M0L4_2axS1359;
              moonbit_decref(_M0L4attrS1354);
              _M0L6_2atmpS3079 = _M0L6_2atmpS3138;
              _M0L4_2axS1359 = _M0L6_2atmpS3079;
              if (_M0L4_2axS1359 == 99) {
                void* _M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3078;
                struct moonbit_result_0 _result_3582;
                moonbit_decref(_M0L17error__to__stringS1366);
                moonbit_decref(_M0L14handle__resultS1364);
                moonbit_decref(_M0L5_2aitS1352);
                moonbit_decref(_M0L1fS1346);
                _M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3078
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3078)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
                ((struct _M0DTPC15error5Error119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3078)->$0
                = _M0L4nameS1348;
                _result_3582.tag = 0;
                _result_3582.data.err
                = _M0L119clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3078;
                return _result_3582;
              }
            } else {
              moonbit_decref(_M0L4attrS1354);
            }
          } else {
            moonbit_decref(_M0L4attrS1354);
          }
        } else {
          moonbit_decref(_M0L4attrS1354);
        }
      } else {
        moonbit_decref(_M0L4attrS1354);
      }
    } else {
      moonbit_decref(_M0L4attrS1354);
    }
    continue;
    joinlet_3581:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1364);
  moonbit_incref(_M0L4nameS1348);
  _closure_3583
  = (struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__*)moonbit_malloc(sizeof(struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__));
  Moonbit_object_header(_closure_3583)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__, $0) >> 2, 2, 0);
  _closure_3583->code
  = &_M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testC3090l427;
  _closure_3583->$0 = _M0L14handle__resultS1364;
  _closure_3583->$1 = _M0L4nameS1348;
  _M0L6_2atmpS3084 = (struct _M0TWEOc*)_closure_3583;
  _closure_3584
  = (struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__*)moonbit_malloc(sizeof(struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__));
  Moonbit_object_header(_closure_3584)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__, $0) >> 2, 3, 0);
  _closure_3584->code
  = &_M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testC3086l428;
  _closure_3584->$0 = _M0L17error__to__stringS1366;
  _closure_3584->$1 = _M0L14handle__resultS1364;
  _closure_3584->$2 = _M0L4nameS1348;
  _M0L6_2atmpS3085 = (struct _M0TWRPC15error5ErrorEu*)_closure_3584;
  #line 425 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools17replace__in__file45moonbit__test__driver__internal__catch__error(_M0L1fS1346, _M0L6_2atmpS3084, _M0L6_2atmpS3085);
  _result_3585.tag = 1;
  _result_3585.data.ok = 1;
  return _result_3585;
}

int32_t _M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testC3090l427(
  struct _M0TWEOc* _M0L6_2aenvS3091
) {
  struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__* _M0L14_2acasted__envS3092;
  moonbit_string_t _M0L8_2afieldS3143;
  moonbit_string_t _M0L4nameS1348;
  struct _M0TWssbEu* _M0L8_2afieldS3142;
  int32_t _M0L6_2acntS3455;
  struct _M0TWssbEu* _M0L14handle__resultS1364;
  #line 427 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3092
  = (struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3090__l427__*)_M0L6_2aenvS3091;
  _M0L8_2afieldS3143 = _M0L14_2acasted__envS3092->$1;
  _M0L4nameS1348 = _M0L8_2afieldS3143;
  _M0L8_2afieldS3142 = _M0L14_2acasted__envS3092->$0;
  _M0L6_2acntS3455 = Moonbit_object_header(_M0L14_2acasted__envS3092)->rc;
  if (_M0L6_2acntS3455 > 1) {
    int32_t _M0L11_2anew__cntS3456 = _M0L6_2acntS3455 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3092)->rc
    = _M0L11_2anew__cntS3456;
    moonbit_incref(_M0L4nameS1348);
    moonbit_incref(_M0L8_2afieldS3142);
  } else if (_M0L6_2acntS3455 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3092);
  }
  _M0L14handle__resultS1364 = _M0L8_2afieldS3142;
  #line 427 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1364->code(_M0L14handle__resultS1364, _M0L4nameS1348, (moonbit_string_t)moonbit_string_literal_1.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam5tools17replace__in__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testC3086l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3087,
  void* _M0L3errS1365
) {
  struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__* _M0L14_2acasted__envS3088;
  moonbit_string_t _M0L8_2afieldS3146;
  moonbit_string_t _M0L4nameS1348;
  struct _M0TWssbEu* _M0L8_2afieldS3145;
  struct _M0TWssbEu* _M0L14handle__resultS1364;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3144;
  int32_t _M0L6_2acntS3457;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1366;
  moonbit_string_t _M0L6_2atmpS3089;
  #line 428 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3088
  = (struct _M0R211_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2freplace__in__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3086__l428__*)_M0L6_2aenvS3087;
  _M0L8_2afieldS3146 = _M0L14_2acasted__envS3088->$2;
  _M0L4nameS1348 = _M0L8_2afieldS3146;
  _M0L8_2afieldS3145 = _M0L14_2acasted__envS3088->$1;
  _M0L14handle__resultS1364 = _M0L8_2afieldS3145;
  _M0L8_2afieldS3144 = _M0L14_2acasted__envS3088->$0;
  _M0L6_2acntS3457 = Moonbit_object_header(_M0L14_2acasted__envS3088)->rc;
  if (_M0L6_2acntS3457 > 1) {
    int32_t _M0L11_2anew__cntS3458 = _M0L6_2acntS3457 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3088)->rc
    = _M0L11_2anew__cntS3458;
    moonbit_incref(_M0L4nameS1348);
    moonbit_incref(_M0L14handle__resultS1364);
    moonbit_incref(_M0L8_2afieldS3144);
  } else if (_M0L6_2acntS3457 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3088);
  }
  _M0L17error__to__stringS1366 = _M0L8_2afieldS3144;
  #line 428 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3089
  = _M0L17error__to__stringS1366->code(_M0L17error__to__stringS1366, _M0L3errS1365);
  #line 428 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1364->code(_M0L14handle__resultS1364, _M0L4nameS1348, _M0L6_2atmpS3089, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1338,
  struct _M0TWEOc* _M0L6on__okS1339,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1336
) {
  void* _M0L11_2atry__errS1334;
  struct moonbit_result_0 _tmp_3587;
  void* _M0L3errS1335;
  #line 375 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _tmp_3587 = _M0L1fS1338->code(_M0L1fS1338);
  if (_tmp_3587.tag) {
    int32_t const _M0L5_2aokS3074 = _tmp_3587.data.ok;
    moonbit_decref(_M0L7on__errS1336);
  } else {
    void* const _M0L6_2aerrS3075 = _tmp_3587.data.err;
    moonbit_decref(_M0L6on__okS1339);
    _M0L11_2atry__errS1334 = _M0L6_2aerrS3075;
    goto join_1333;
  }
  #line 382 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1339->code(_M0L6on__okS1339);
  goto joinlet_3586;
  join_1333:;
  _M0L3errS1335 = _M0L11_2atry__errS1334;
  #line 383 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1336->code(_M0L7on__errS1336, _M0L3errS1335);
  joinlet_3586:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1293;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1306;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1311;
  struct _M0TUsiE** _M0L6_2atmpS3073;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1318;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1319;
  moonbit_string_t _M0L6_2atmpS3072;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1320;
  int32_t _M0L7_2abindS1321;
  int32_t _M0L2__S1322;
  #line 193 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1293 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1306
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1311 = 0;
  _M0L6_2atmpS3073 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1318
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1318)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1318->$0 = _M0L6_2atmpS3073;
  _M0L16file__and__indexS1318->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1319
  = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1306(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1306);
  #line 284 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3072 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1319, 1);
  #line 283 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1320
  = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1311(_M0L51moonbit__test__driver__internal__split__mbt__stringS1311, _M0L6_2atmpS3072, 47);
  _M0L7_2abindS1321 = _M0L10test__argsS1320->$1;
  _M0L2__S1322 = 0;
  while (1) {
    if (_M0L2__S1322 < _M0L7_2abindS1321) {
      moonbit_string_t* _M0L8_2afieldS3148 = _M0L10test__argsS1320->$0;
      moonbit_string_t* _M0L3bufS3071 = _M0L8_2afieldS3148;
      moonbit_string_t _M0L6_2atmpS3147 =
        (moonbit_string_t)_M0L3bufS3071[_M0L2__S1322];
      moonbit_string_t _M0L3argS1323 = _M0L6_2atmpS3147;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1324;
      moonbit_string_t _M0L4fileS1325;
      moonbit_string_t _M0L5rangeS1326;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1327;
      moonbit_string_t _M0L6_2atmpS3069;
      int32_t _M0L5startS1328;
      moonbit_string_t _M0L6_2atmpS3068;
      int32_t _M0L3endS1329;
      int32_t _M0L1iS1330;
      int32_t _M0L6_2atmpS3070;
      moonbit_incref(_M0L3argS1323);
      #line 288 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1324
      = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1311(_M0L51moonbit__test__driver__internal__split__mbt__stringS1311, _M0L3argS1323, 58);
      moonbit_incref(_M0L16file__and__rangeS1324);
      #line 289 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1325
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1324, 0);
      #line 290 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1326
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1324, 1);
      #line 291 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1327
      = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1311(_M0L51moonbit__test__driver__internal__split__mbt__stringS1311, _M0L5rangeS1326, 45);
      moonbit_incref(_M0L15start__and__endS1327);
      #line 294 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3069
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1327, 0);
      #line 294 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1328
      = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1293(_M0L45moonbit__test__driver__internal__parse__int__S1293, _M0L6_2atmpS3069);
      #line 295 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3068
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1327, 1);
      #line 295 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1329
      = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1293(_M0L45moonbit__test__driver__internal__parse__int__S1293, _M0L6_2atmpS3068);
      _M0L1iS1330 = _M0L5startS1328;
      while (1) {
        if (_M0L1iS1330 < _M0L3endS1329) {
          struct _M0TUsiE* _M0L8_2atupleS3066;
          int32_t _M0L6_2atmpS3067;
          moonbit_incref(_M0L4fileS1325);
          _M0L8_2atupleS3066
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3066)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3066->$0 = _M0L4fileS1325;
          _M0L8_2atupleS3066->$1 = _M0L1iS1330;
          moonbit_incref(_M0L16file__and__indexS1318);
          #line 297 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1318, _M0L8_2atupleS3066);
          _M0L6_2atmpS3067 = _M0L1iS1330 + 1;
          _M0L1iS1330 = _M0L6_2atmpS3067;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1325);
        }
        break;
      }
      _M0L6_2atmpS3070 = _M0L2__S1322 + 1;
      _M0L2__S1322 = _M0L6_2atmpS3070;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1320);
    }
    break;
  }
  return _M0L16file__and__indexS1318;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1311(
  int32_t _M0L6_2aenvS3047,
  moonbit_string_t _M0L1sS1312,
  int32_t _M0L3sepS1313
) {
  moonbit_string_t* _M0L6_2atmpS3065;
  struct _M0TPB5ArrayGsE* _M0L3resS1314;
  struct _M0TPC13ref3RefGiE* _M0L1iS1315;
  struct _M0TPC13ref3RefGiE* _M0L5startS1316;
  int32_t _M0L3valS3060;
  int32_t _M0L6_2atmpS3061;
  #line 261 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3065 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1314
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1314)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1314->$0 = _M0L6_2atmpS3065;
  _M0L3resS1314->$1 = 0;
  _M0L1iS1315
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1315)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1315->$0 = 0;
  _M0L5startS1316
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1316)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1316->$0 = 0;
  while (1) {
    int32_t _M0L3valS3048 = _M0L1iS1315->$0;
    int32_t _M0L6_2atmpS3049 = Moonbit_array_length(_M0L1sS1312);
    if (_M0L3valS3048 < _M0L6_2atmpS3049) {
      int32_t _M0L3valS3052 = _M0L1iS1315->$0;
      int32_t _M0L6_2atmpS3051;
      int32_t _M0L6_2atmpS3050;
      int32_t _M0L3valS3059;
      int32_t _M0L6_2atmpS3058;
      if (
        _M0L3valS3052 < 0
        || _M0L3valS3052 >= Moonbit_array_length(_M0L1sS1312)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3051 = _M0L1sS1312[_M0L3valS3052];
      _M0L6_2atmpS3050 = _M0L6_2atmpS3051;
      if (_M0L6_2atmpS3050 == _M0L3sepS1313) {
        int32_t _M0L3valS3054 = _M0L5startS1316->$0;
        int32_t _M0L3valS3055 = _M0L1iS1315->$0;
        moonbit_string_t _M0L6_2atmpS3053;
        int32_t _M0L3valS3057;
        int32_t _M0L6_2atmpS3056;
        moonbit_incref(_M0L1sS1312);
        #line 270 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3053
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1312, _M0L3valS3054, _M0L3valS3055);
        moonbit_incref(_M0L3resS1314);
        #line 270 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1314, _M0L6_2atmpS3053);
        _M0L3valS3057 = _M0L1iS1315->$0;
        _M0L6_2atmpS3056 = _M0L3valS3057 + 1;
        _M0L5startS1316->$0 = _M0L6_2atmpS3056;
      }
      _M0L3valS3059 = _M0L1iS1315->$0;
      _M0L6_2atmpS3058 = _M0L3valS3059 + 1;
      _M0L1iS1315->$0 = _M0L6_2atmpS3058;
      continue;
    } else {
      moonbit_decref(_M0L1iS1315);
    }
    break;
  }
  _M0L3valS3060 = _M0L5startS1316->$0;
  _M0L6_2atmpS3061 = Moonbit_array_length(_M0L1sS1312);
  if (_M0L3valS3060 < _M0L6_2atmpS3061) {
    int32_t _M0L8_2afieldS3149 = _M0L5startS1316->$0;
    int32_t _M0L3valS3063;
    int32_t _M0L6_2atmpS3064;
    moonbit_string_t _M0L6_2atmpS3062;
    moonbit_decref(_M0L5startS1316);
    _M0L3valS3063 = _M0L8_2afieldS3149;
    _M0L6_2atmpS3064 = Moonbit_array_length(_M0L1sS1312);
    #line 276 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3062
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1312, _M0L3valS3063, _M0L6_2atmpS3064);
    moonbit_incref(_M0L3resS1314);
    #line 276 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1314, _M0L6_2atmpS3062);
  } else {
    moonbit_decref(_M0L5startS1316);
    moonbit_decref(_M0L1sS1312);
  }
  return _M0L3resS1314;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1306(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299
) {
  moonbit_bytes_t* _M0L3tmpS1307;
  int32_t _M0L6_2atmpS3046;
  struct _M0TPB5ArrayGsE* _M0L3resS1308;
  int32_t _M0L1iS1309;
  #line 250 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1307
  = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3046 = Moonbit_array_length(_M0L3tmpS1307);
  #line 254 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1308 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3046);
  _M0L1iS1309 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3042 = Moonbit_array_length(_M0L3tmpS1307);
    if (_M0L1iS1309 < _M0L6_2atmpS3042) {
      moonbit_bytes_t _M0L6_2atmpS3150;
      moonbit_bytes_t _M0L6_2atmpS3044;
      moonbit_string_t _M0L6_2atmpS3043;
      int32_t _M0L6_2atmpS3045;
      if (
        _M0L1iS1309 < 0 || _M0L1iS1309 >= Moonbit_array_length(_M0L3tmpS1307)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3150 = (moonbit_bytes_t)_M0L3tmpS1307[_M0L1iS1309];
      _M0L6_2atmpS3044 = _M0L6_2atmpS3150;
      moonbit_incref(_M0L6_2atmpS3044);
      #line 256 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3043
      = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299, _M0L6_2atmpS3044);
      moonbit_incref(_M0L3resS1308);
      #line 256 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1308, _M0L6_2atmpS3043);
      _M0L6_2atmpS3045 = _M0L1iS1309 + 1;
      _M0L1iS1309 = _M0L6_2atmpS3045;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1307);
    }
    break;
  }
  return _M0L3resS1308;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1299(
  int32_t _M0L6_2aenvS2956,
  moonbit_bytes_t _M0L5bytesS1300
) {
  struct _M0TPB13StringBuilder* _M0L3resS1301;
  int32_t _M0L3lenS1302;
  struct _M0TPC13ref3RefGiE* _M0L1iS1303;
  #line 206 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1301 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1302 = Moonbit_array_length(_M0L5bytesS1300);
  _M0L1iS1303
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1303)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1303->$0 = 0;
  while (1) {
    int32_t _M0L3valS2957 = _M0L1iS1303->$0;
    if (_M0L3valS2957 < _M0L3lenS1302) {
      int32_t _M0L3valS3041 = _M0L1iS1303->$0;
      int32_t _M0L6_2atmpS3040;
      int32_t _M0L6_2atmpS3039;
      struct _M0TPC13ref3RefGiE* _M0L1cS1304;
      int32_t _M0L3valS2958;
      if (
        _M0L3valS3041 < 0
        || _M0L3valS3041 >= Moonbit_array_length(_M0L5bytesS1300)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3040 = _M0L5bytesS1300[_M0L3valS3041];
      _M0L6_2atmpS3039 = (int32_t)_M0L6_2atmpS3040;
      _M0L1cS1304
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1304)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1304->$0 = _M0L6_2atmpS3039;
      _M0L3valS2958 = _M0L1cS1304->$0;
      if (_M0L3valS2958 < 128) {
        int32_t _M0L8_2afieldS3151 = _M0L1cS1304->$0;
        int32_t _M0L3valS2960;
        int32_t _M0L6_2atmpS2959;
        int32_t _M0L3valS2962;
        int32_t _M0L6_2atmpS2961;
        moonbit_decref(_M0L1cS1304);
        _M0L3valS2960 = _M0L8_2afieldS3151;
        _M0L6_2atmpS2959 = _M0L3valS2960;
        moonbit_incref(_M0L3resS1301);
        #line 215 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1301, _M0L6_2atmpS2959);
        _M0L3valS2962 = _M0L1iS1303->$0;
        _M0L6_2atmpS2961 = _M0L3valS2962 + 1;
        _M0L1iS1303->$0 = _M0L6_2atmpS2961;
      } else {
        int32_t _M0L3valS2963 = _M0L1cS1304->$0;
        if (_M0L3valS2963 < 224) {
          int32_t _M0L3valS2965 = _M0L1iS1303->$0;
          int32_t _M0L6_2atmpS2964 = _M0L3valS2965 + 1;
          int32_t _M0L3valS2974;
          int32_t _M0L6_2atmpS2973;
          int32_t _M0L6_2atmpS2967;
          int32_t _M0L3valS2972;
          int32_t _M0L6_2atmpS2971;
          int32_t _M0L6_2atmpS2970;
          int32_t _M0L6_2atmpS2969;
          int32_t _M0L6_2atmpS2968;
          int32_t _M0L6_2atmpS2966;
          int32_t _M0L8_2afieldS3152;
          int32_t _M0L3valS2976;
          int32_t _M0L6_2atmpS2975;
          int32_t _M0L3valS2978;
          int32_t _M0L6_2atmpS2977;
          if (_M0L6_2atmpS2964 >= _M0L3lenS1302) {
            moonbit_decref(_M0L1cS1304);
            moonbit_decref(_M0L1iS1303);
            moonbit_decref(_M0L5bytesS1300);
            break;
          }
          _M0L3valS2974 = _M0L1cS1304->$0;
          _M0L6_2atmpS2973 = _M0L3valS2974 & 31;
          _M0L6_2atmpS2967 = _M0L6_2atmpS2973 << 6;
          _M0L3valS2972 = _M0L1iS1303->$0;
          _M0L6_2atmpS2971 = _M0L3valS2972 + 1;
          if (
            _M0L6_2atmpS2971 < 0
            || _M0L6_2atmpS2971 >= Moonbit_array_length(_M0L5bytesS1300)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2970 = _M0L5bytesS1300[_M0L6_2atmpS2971];
          _M0L6_2atmpS2969 = (int32_t)_M0L6_2atmpS2970;
          _M0L6_2atmpS2968 = _M0L6_2atmpS2969 & 63;
          _M0L6_2atmpS2966 = _M0L6_2atmpS2967 | _M0L6_2atmpS2968;
          _M0L1cS1304->$0 = _M0L6_2atmpS2966;
          _M0L8_2afieldS3152 = _M0L1cS1304->$0;
          moonbit_decref(_M0L1cS1304);
          _M0L3valS2976 = _M0L8_2afieldS3152;
          _M0L6_2atmpS2975 = _M0L3valS2976;
          moonbit_incref(_M0L3resS1301);
          #line 222 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1301, _M0L6_2atmpS2975);
          _M0L3valS2978 = _M0L1iS1303->$0;
          _M0L6_2atmpS2977 = _M0L3valS2978 + 2;
          _M0L1iS1303->$0 = _M0L6_2atmpS2977;
        } else {
          int32_t _M0L3valS2979 = _M0L1cS1304->$0;
          if (_M0L3valS2979 < 240) {
            int32_t _M0L3valS2981 = _M0L1iS1303->$0;
            int32_t _M0L6_2atmpS2980 = _M0L3valS2981 + 2;
            int32_t _M0L3valS2997;
            int32_t _M0L6_2atmpS2996;
            int32_t _M0L6_2atmpS2989;
            int32_t _M0L3valS2995;
            int32_t _M0L6_2atmpS2994;
            int32_t _M0L6_2atmpS2993;
            int32_t _M0L6_2atmpS2992;
            int32_t _M0L6_2atmpS2991;
            int32_t _M0L6_2atmpS2990;
            int32_t _M0L6_2atmpS2983;
            int32_t _M0L3valS2988;
            int32_t _M0L6_2atmpS2987;
            int32_t _M0L6_2atmpS2986;
            int32_t _M0L6_2atmpS2985;
            int32_t _M0L6_2atmpS2984;
            int32_t _M0L6_2atmpS2982;
            int32_t _M0L8_2afieldS3153;
            int32_t _M0L3valS2999;
            int32_t _M0L6_2atmpS2998;
            int32_t _M0L3valS3001;
            int32_t _M0L6_2atmpS3000;
            if (_M0L6_2atmpS2980 >= _M0L3lenS1302) {
              moonbit_decref(_M0L1cS1304);
              moonbit_decref(_M0L1iS1303);
              moonbit_decref(_M0L5bytesS1300);
              break;
            }
            _M0L3valS2997 = _M0L1cS1304->$0;
            _M0L6_2atmpS2996 = _M0L3valS2997 & 15;
            _M0L6_2atmpS2989 = _M0L6_2atmpS2996 << 12;
            _M0L3valS2995 = _M0L1iS1303->$0;
            _M0L6_2atmpS2994 = _M0L3valS2995 + 1;
            if (
              _M0L6_2atmpS2994 < 0
              || _M0L6_2atmpS2994 >= Moonbit_array_length(_M0L5bytesS1300)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2993 = _M0L5bytesS1300[_M0L6_2atmpS2994];
            _M0L6_2atmpS2992 = (int32_t)_M0L6_2atmpS2993;
            _M0L6_2atmpS2991 = _M0L6_2atmpS2992 & 63;
            _M0L6_2atmpS2990 = _M0L6_2atmpS2991 << 6;
            _M0L6_2atmpS2983 = _M0L6_2atmpS2989 | _M0L6_2atmpS2990;
            _M0L3valS2988 = _M0L1iS1303->$0;
            _M0L6_2atmpS2987 = _M0L3valS2988 + 2;
            if (
              _M0L6_2atmpS2987 < 0
              || _M0L6_2atmpS2987 >= Moonbit_array_length(_M0L5bytesS1300)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2986 = _M0L5bytesS1300[_M0L6_2atmpS2987];
            _M0L6_2atmpS2985 = (int32_t)_M0L6_2atmpS2986;
            _M0L6_2atmpS2984 = _M0L6_2atmpS2985 & 63;
            _M0L6_2atmpS2982 = _M0L6_2atmpS2983 | _M0L6_2atmpS2984;
            _M0L1cS1304->$0 = _M0L6_2atmpS2982;
            _M0L8_2afieldS3153 = _M0L1cS1304->$0;
            moonbit_decref(_M0L1cS1304);
            _M0L3valS2999 = _M0L8_2afieldS3153;
            _M0L6_2atmpS2998 = _M0L3valS2999;
            moonbit_incref(_M0L3resS1301);
            #line 231 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1301, _M0L6_2atmpS2998);
            _M0L3valS3001 = _M0L1iS1303->$0;
            _M0L6_2atmpS3000 = _M0L3valS3001 + 3;
            _M0L1iS1303->$0 = _M0L6_2atmpS3000;
          } else {
            int32_t _M0L3valS3003 = _M0L1iS1303->$0;
            int32_t _M0L6_2atmpS3002 = _M0L3valS3003 + 3;
            int32_t _M0L3valS3026;
            int32_t _M0L6_2atmpS3025;
            int32_t _M0L6_2atmpS3018;
            int32_t _M0L3valS3024;
            int32_t _M0L6_2atmpS3023;
            int32_t _M0L6_2atmpS3022;
            int32_t _M0L6_2atmpS3021;
            int32_t _M0L6_2atmpS3020;
            int32_t _M0L6_2atmpS3019;
            int32_t _M0L6_2atmpS3011;
            int32_t _M0L3valS3017;
            int32_t _M0L6_2atmpS3016;
            int32_t _M0L6_2atmpS3015;
            int32_t _M0L6_2atmpS3014;
            int32_t _M0L6_2atmpS3013;
            int32_t _M0L6_2atmpS3012;
            int32_t _M0L6_2atmpS3005;
            int32_t _M0L3valS3010;
            int32_t _M0L6_2atmpS3009;
            int32_t _M0L6_2atmpS3008;
            int32_t _M0L6_2atmpS3007;
            int32_t _M0L6_2atmpS3006;
            int32_t _M0L6_2atmpS3004;
            int32_t _M0L3valS3028;
            int32_t _M0L6_2atmpS3027;
            int32_t _M0L3valS3032;
            int32_t _M0L6_2atmpS3031;
            int32_t _M0L6_2atmpS3030;
            int32_t _M0L6_2atmpS3029;
            int32_t _M0L8_2afieldS3154;
            int32_t _M0L3valS3036;
            int32_t _M0L6_2atmpS3035;
            int32_t _M0L6_2atmpS3034;
            int32_t _M0L6_2atmpS3033;
            int32_t _M0L3valS3038;
            int32_t _M0L6_2atmpS3037;
            if (_M0L6_2atmpS3002 >= _M0L3lenS1302) {
              moonbit_decref(_M0L1cS1304);
              moonbit_decref(_M0L1iS1303);
              moonbit_decref(_M0L5bytesS1300);
              break;
            }
            _M0L3valS3026 = _M0L1cS1304->$0;
            _M0L6_2atmpS3025 = _M0L3valS3026 & 7;
            _M0L6_2atmpS3018 = _M0L6_2atmpS3025 << 18;
            _M0L3valS3024 = _M0L1iS1303->$0;
            _M0L6_2atmpS3023 = _M0L3valS3024 + 1;
            if (
              _M0L6_2atmpS3023 < 0
              || _M0L6_2atmpS3023 >= Moonbit_array_length(_M0L5bytesS1300)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3022 = _M0L5bytesS1300[_M0L6_2atmpS3023];
            _M0L6_2atmpS3021 = (int32_t)_M0L6_2atmpS3022;
            _M0L6_2atmpS3020 = _M0L6_2atmpS3021 & 63;
            _M0L6_2atmpS3019 = _M0L6_2atmpS3020 << 12;
            _M0L6_2atmpS3011 = _M0L6_2atmpS3018 | _M0L6_2atmpS3019;
            _M0L3valS3017 = _M0L1iS1303->$0;
            _M0L6_2atmpS3016 = _M0L3valS3017 + 2;
            if (
              _M0L6_2atmpS3016 < 0
              || _M0L6_2atmpS3016 >= Moonbit_array_length(_M0L5bytesS1300)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3015 = _M0L5bytesS1300[_M0L6_2atmpS3016];
            _M0L6_2atmpS3014 = (int32_t)_M0L6_2atmpS3015;
            _M0L6_2atmpS3013 = _M0L6_2atmpS3014 & 63;
            _M0L6_2atmpS3012 = _M0L6_2atmpS3013 << 6;
            _M0L6_2atmpS3005 = _M0L6_2atmpS3011 | _M0L6_2atmpS3012;
            _M0L3valS3010 = _M0L1iS1303->$0;
            _M0L6_2atmpS3009 = _M0L3valS3010 + 3;
            if (
              _M0L6_2atmpS3009 < 0
              || _M0L6_2atmpS3009 >= Moonbit_array_length(_M0L5bytesS1300)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3008 = _M0L5bytesS1300[_M0L6_2atmpS3009];
            _M0L6_2atmpS3007 = (int32_t)_M0L6_2atmpS3008;
            _M0L6_2atmpS3006 = _M0L6_2atmpS3007 & 63;
            _M0L6_2atmpS3004 = _M0L6_2atmpS3005 | _M0L6_2atmpS3006;
            _M0L1cS1304->$0 = _M0L6_2atmpS3004;
            _M0L3valS3028 = _M0L1cS1304->$0;
            _M0L6_2atmpS3027 = _M0L3valS3028 - 65536;
            _M0L1cS1304->$0 = _M0L6_2atmpS3027;
            _M0L3valS3032 = _M0L1cS1304->$0;
            _M0L6_2atmpS3031 = _M0L3valS3032 >> 10;
            _M0L6_2atmpS3030 = _M0L6_2atmpS3031 + 55296;
            _M0L6_2atmpS3029 = _M0L6_2atmpS3030;
            moonbit_incref(_M0L3resS1301);
            #line 242 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1301, _M0L6_2atmpS3029);
            _M0L8_2afieldS3154 = _M0L1cS1304->$0;
            moonbit_decref(_M0L1cS1304);
            _M0L3valS3036 = _M0L8_2afieldS3154;
            _M0L6_2atmpS3035 = _M0L3valS3036 & 1023;
            _M0L6_2atmpS3034 = _M0L6_2atmpS3035 + 56320;
            _M0L6_2atmpS3033 = _M0L6_2atmpS3034;
            moonbit_incref(_M0L3resS1301);
            #line 243 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1301, _M0L6_2atmpS3033);
            _M0L3valS3038 = _M0L1iS1303->$0;
            _M0L6_2atmpS3037 = _M0L3valS3038 + 4;
            _M0L1iS1303->$0 = _M0L6_2atmpS3037;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1303);
      moonbit_decref(_M0L5bytesS1300);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1301);
}

int32_t _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1293(
  int32_t _M0L6_2aenvS2949,
  moonbit_string_t _M0L1sS1294
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1295;
  int32_t _M0L3lenS1296;
  int32_t _M0L1iS1297;
  int32_t _M0L8_2afieldS3155;
  #line 197 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1295
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1295)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1295->$0 = 0;
  _M0L3lenS1296 = Moonbit_array_length(_M0L1sS1294);
  _M0L1iS1297 = 0;
  while (1) {
    if (_M0L1iS1297 < _M0L3lenS1296) {
      int32_t _M0L3valS2954 = _M0L3resS1295->$0;
      int32_t _M0L6_2atmpS2951 = _M0L3valS2954 * 10;
      int32_t _M0L6_2atmpS2953;
      int32_t _M0L6_2atmpS2952;
      int32_t _M0L6_2atmpS2950;
      int32_t _M0L6_2atmpS2955;
      if (
        _M0L1iS1297 < 0 || _M0L1iS1297 >= Moonbit_array_length(_M0L1sS1294)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2953 = _M0L1sS1294[_M0L1iS1297];
      _M0L6_2atmpS2952 = _M0L6_2atmpS2953 - 48;
      _M0L6_2atmpS2950 = _M0L6_2atmpS2951 + _M0L6_2atmpS2952;
      _M0L3resS1295->$0 = _M0L6_2atmpS2950;
      _M0L6_2atmpS2955 = _M0L1iS1297 + 1;
      _M0L1iS1297 = _M0L6_2atmpS2955;
      continue;
    } else {
      moonbit_decref(_M0L1sS1294);
    }
    break;
  }
  _M0L8_2afieldS3155 = _M0L3resS1295->$0;
  moonbit_decref(_M0L3resS1295);
  return _M0L8_2afieldS3155;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1273,
  moonbit_string_t _M0L12_2adiscard__S1274,
  int32_t _M0L12_2adiscard__S1275,
  struct _M0TWssbEu* _M0L12_2adiscard__S1276,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1277
) {
  struct moonbit_result_0 _result_3594;
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1277);
  moonbit_decref(_M0L12_2adiscard__S1276);
  moonbit_decref(_M0L12_2adiscard__S1274);
  moonbit_decref(_M0L12_2adiscard__S1273);
  _result_3594.tag = 1;
  _result_3594.data.ok = 0;
  return _result_3594;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1278,
  moonbit_string_t _M0L12_2adiscard__S1279,
  int32_t _M0L12_2adiscard__S1280,
  struct _M0TWssbEu* _M0L12_2adiscard__S1281,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1282
) {
  struct moonbit_result_0 _result_3595;
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1282);
  moonbit_decref(_M0L12_2adiscard__S1281);
  moonbit_decref(_M0L12_2adiscard__S1279);
  moonbit_decref(_M0L12_2adiscard__S1278);
  _result_3595.tag = 1;
  _result_3595.data.ok = 0;
  return _result_3595;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1283,
  moonbit_string_t _M0L12_2adiscard__S1284,
  int32_t _M0L12_2adiscard__S1285,
  struct _M0TWssbEu* _M0L12_2adiscard__S1286,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1287
) {
  struct moonbit_result_0 _result_3596;
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1287);
  moonbit_decref(_M0L12_2adiscard__S1286);
  moonbit_decref(_M0L12_2adiscard__S1284);
  moonbit_decref(_M0L12_2adiscard__S1283);
  _result_3596.tag = 1;
  _result_3596.data.ok = 0;
  return _result_3596;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools17replace__in__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1288,
  moonbit_string_t _M0L12_2adiscard__S1289,
  int32_t _M0L12_2adiscard__S1290,
  struct _M0TWssbEu* _M0L12_2adiscard__S1291,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1292
) {
  struct moonbit_result_0 _result_3597;
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1292);
  moonbit_decref(_M0L12_2adiscard__S1291);
  moonbit_decref(_M0L12_2adiscard__S1289);
  moonbit_decref(_M0L12_2adiscard__S1288);
  _result_3597.tag = 1;
  _result_3597.data.ok = 0;
  return _result_3597;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools17replace__in__file34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1272
) {
  #line 12 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1272);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0(
  
) {
  void* _M0L6_2atmpS2948;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2943;
  void* _M0L6_2atmpS2947;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2944;
  void* _M0L6_2atmpS2946;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2945;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1265;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2942;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2941;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2940;
  void* _M0L6_2atmpS2931;
  void* _M0L6_2atmpS2939;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2936;
  void* _M0L6_2atmpS2938;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2937;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1266;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2935;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2934;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2933;
  void* _M0L6_2atmpS2932;
  void** _M0L6_2atmpS2930;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L11json__inputS1264;
  struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error* _M0L11round__tripS1267;
  struct moonbit_result_2 _tmp_3598;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2927;
  struct _M0TPB6ToJson _M0L6_2atmpS2898;
  void* _M0L6_2atmpS2926;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2921;
  void* _M0L6_2atmpS2925;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2922;
  void* _M0L6_2atmpS2924;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2923;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1270;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2920;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2919;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2918;
  void* _M0L6_2atmpS2909;
  void* _M0L6_2atmpS2917;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2914;
  void* _M0L6_2atmpS2916;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2915;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1271;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2913;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2912;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2911;
  void* _M0L6_2atmpS2910;
  void** _M0L6_2atmpS2908;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2907;
  void* _M0L6_2atmpS2906;
  void* _M0L6_2atmpS2899;
  moonbit_string_t _M0L6_2atmpS2902;
  moonbit_string_t _M0L6_2atmpS2903;
  moonbit_string_t _M0L6_2atmpS2904;
  moonbit_string_t _M0L6_2atmpS2905;
  moonbit_string_t* _M0L6_2atmpS2901;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2900;
  #line 31 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  #line 33 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2948
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2943
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2943)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2943->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2943->$1 = _M0L6_2atmpS2948;
  #line 33 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2947
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS2944
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2944)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2944->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2944->$1 = _M0L6_2atmpS2947;
  #line 33 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2946
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS2945
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2945)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2945->$0 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L8_2atupleS2945->$1 = _M0L6_2atmpS2946;
  _M0L7_2abindS1265 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1265[0] = _M0L8_2atupleS2943;
  _M0L7_2abindS1265[1] = _M0L8_2atupleS2944;
  _M0L7_2abindS1265[2] = _M0L8_2atupleS2945;
  _M0L6_2atmpS2942 = _M0L7_2abindS1265;
  _M0L6_2atmpS2941
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS2942
  };
  #line 33 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2940 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2941);
  #line 33 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2931 = _M0MPC14json4Json6object(_M0L6_2atmpS2940);
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2939
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2936
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2936)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2936->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2936->$1 = _M0L6_2atmpS2939;
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2938
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS2937
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2937)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2937->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2937->$1 = _M0L6_2atmpS2938;
  _M0L7_2abindS1266 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1266[0] = _M0L8_2atupleS2936;
  _M0L7_2abindS1266[1] = _M0L8_2atupleS2937;
  _M0L6_2atmpS2935 = _M0L7_2abindS1266;
  _M0L6_2atmpS2934
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2935
  };
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2933 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2934);
  #line 34 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2932 = _M0MPC14json4Json6object(_M0L6_2atmpS2933);
  _M0L6_2atmpS2930 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2930[0] = _M0L6_2atmpS2931;
  _M0L6_2atmpS2930[1] = _M0L6_2atmpS2932;
  _M0L11json__inputS1264
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L11json__inputS1264)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L11json__inputS1264->$0 = _M0L6_2atmpS2930;
  _M0L11json__inputS1264->$1 = 2;
  _M0L11round__tripS1267
  = (struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error*)&_M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0N11round__tripS1267$closure.data;
  #line 41 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _tmp_3598
  = _M0MPC15array5Array3mapGRPB4JsonRPB4JsonEHRPC15error5Error(_M0L11json__inputS1264, _M0L11round__tripS1267);
  if (_tmp_3598.tag) {
    struct _M0TPB5ArrayGRPB4JsonE* const _M0L5_2aokS2928 = _tmp_3598.data.ok;
    _M0L6_2atmpS2927 = _M0L5_2aokS2928;
  } else {
    void* const _M0L6_2aerrS2929 = _tmp_3598.data.err;
    struct moonbit_result_0 _result_3599;
    _result_3599.tag = 0;
    _result_3599.data.err = _M0L6_2aerrS2929;
    return _result_3599;
  }
  _M0L6_2atmpS2898
  = (struct _M0TPB6ToJson){
    _M0FP0152moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS2927
  };
  #line 42 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2926
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2921
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2921)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2921->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2921->$1 = _M0L6_2atmpS2926;
  #line 42 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2925
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS2922
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2922)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2922->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2922->$1 = _M0L6_2atmpS2925;
  #line 42 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2924
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS2923
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2923)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2923->$0 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L8_2atupleS2923->$1 = _M0L6_2atmpS2924;
  _M0L7_2abindS1270 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1270[0] = _M0L8_2atupleS2921;
  _M0L7_2abindS1270[1] = _M0L8_2atupleS2922;
  _M0L7_2abindS1270[2] = _M0L8_2atupleS2923;
  _M0L6_2atmpS2920 = _M0L7_2abindS1270;
  _M0L6_2atmpS2919
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS2920
  };
  #line 42 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2918 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2919);
  #line 42 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2909 = _M0MPC14json4Json6object(_M0L6_2atmpS2918);
  #line 43 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2917
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2914
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2914)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2914->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2914->$1 = _M0L6_2atmpS2917;
  #line 43 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2916
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS2915
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2915)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2915->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2915->$1 = _M0L6_2atmpS2916;
  _M0L7_2abindS1271 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1271[0] = _M0L8_2atupleS2914;
  _M0L7_2abindS1271[1] = _M0L8_2atupleS2915;
  _M0L6_2atmpS2913 = _M0L7_2abindS1271;
  _M0L6_2atmpS2912
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2913
  };
  #line 43 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2911 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2912);
  #line 43 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2910 = _M0MPC14json4Json6object(_M0L6_2atmpS2911);
  _M0L6_2atmpS2908 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2908[0] = _M0L6_2atmpS2909;
  _M0L6_2atmpS2908[1] = _M0L6_2atmpS2910;
  _M0L6_2atmpS2907
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS2907)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2907->$0 = _M0L6_2atmpS2908;
  _M0L6_2atmpS2907->$1 = 2;
  #line 41 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2906 = _M0MPC14json4Json5array(_M0L6_2atmpS2907);
  _M0L6_2atmpS2899 = _M0L6_2atmpS2906;
  _M0L6_2atmpS2902 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS2903 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS2904 = 0;
  _M0L6_2atmpS2905 = 0;
  _M0L6_2atmpS2901 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2901[0] = _M0L6_2atmpS2902;
  _M0L6_2atmpS2901[1] = _M0L6_2atmpS2903;
  _M0L6_2atmpS2901[2] = _M0L6_2atmpS2904;
  _M0L6_2atmpS2901[3] = _M0L6_2atmpS2905;
  _M0L6_2atmpS2900
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2900)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2900->$0 = _M0L6_2atmpS2901;
  _M0L6_2atmpS2900->$1 = 4;
  #line 41 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2898, _M0L6_2atmpS2899, (moonbit_string_t)moonbit_string_literal_18.data, _M0L6_2atmpS2900);
}

struct moonbit_result_1 _M0FP48clawteam8clawteam5tools17replace__in__file33____test__736368656d612e6d6274__0N11round__tripS1267(
  struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error* _M0L6_2aenvS2893,
  void* _M0L1xS1268
) {
  void* _M0L6_2atmpS2895;
  struct moonbit_result_3 _tmp_3600;
  struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input* _M0L5inputS1269;
  void* _M0L6_2atmpS2894;
  struct moonbit_result_1 _result_3602;
  #line 36 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  moonbit_decref(_M0L6_2aenvS2893);
  _M0L6_2atmpS2895 = 0;
  #line 37 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _tmp_3600
  = _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools17replace__in__file5InputE(_M0L1xS1268, _M0L6_2atmpS2895);
  if (_tmp_3600.tag) {
    struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input* const _M0L5_2aokS2896 =
      _tmp_3600.data.ok;
    _M0L5inputS1269 = _M0L5_2aokS2896;
  } else {
    void* const _M0L6_2aerrS2897 = _tmp_3600.data.err;
    struct moonbit_result_1 _result_3601;
    _result_3601.tag = 0;
    _result_3601.data.err = _M0L6_2aerrS2897;
    return _result_3601;
  }
  #line 38 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2894
  = _M0IP48clawteam8clawteam5tools17replace__in__file5InputPB6ToJson8to__json(_M0L5inputS1269);
  _result_3602.tag = 1;
  _result_3602.data.ok = _M0L6_2atmpS2894;
  return _result_3602;
}

void* _M0IP48clawteam8clawteam5tools17replace__in__file5InputPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input* _M0L9_2ax__100S1258
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1257;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2892;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2891;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1256;
  moonbit_string_t _M0L8_2afieldS3158;
  moonbit_string_t _M0L4pathS2887;
  void* _M0L6_2atmpS2886;
  moonbit_string_t _M0L8_2afieldS3157;
  moonbit_string_t _M0L7replaceS2889;
  void* _M0L6_2atmpS2888;
  moonbit_string_t _M0L8_24innerS1260;
  moonbit_string_t _M0L8_2afieldS3156;
  int32_t _M0L6_2acntS3459;
  moonbit_string_t _M0L7_2abindS1261;
  void* _M0L6_2atmpS2890;
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L7_2abindS1257 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2892 = _M0L7_2abindS1257;
  _M0L6_2atmpS2891
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS2892
  };
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_24mapS1256 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2891);
  _M0L8_2afieldS3158 = _M0L9_2ax__100S1258->$0;
  _M0L4pathS2887 = _M0L8_2afieldS3158;
  moonbit_incref(_M0L4pathS2887);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2886 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4pathS2887);
  moonbit_incref(_M0L6_24mapS1256);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1256, (moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS2886);
  _M0L8_2afieldS3157 = _M0L9_2ax__100S1258->$1;
  _M0L7replaceS2889 = _M0L8_2afieldS3157;
  moonbit_incref(_M0L7replaceS2889);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2888
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L7replaceS2889);
  moonbit_incref(_M0L6_24mapS1256);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1256, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2888);
  _M0L8_2afieldS3156 = _M0L9_2ax__100S1258->$2;
  _M0L6_2acntS3459 = Moonbit_object_header(_M0L9_2ax__100S1258)->rc;
  if (_M0L6_2acntS3459 > 1) {
    int32_t _M0L11_2anew__cntS3462 = _M0L6_2acntS3459 - 1;
    Moonbit_object_header(_M0L9_2ax__100S1258)->rc = _M0L11_2anew__cntS3462;
    if (_M0L8_2afieldS3156) {
      moonbit_incref(_M0L8_2afieldS3156);
    }
  } else if (_M0L6_2acntS3459 == 1) {
    moonbit_string_t _M0L8_2afieldS3461 = _M0L9_2ax__100S1258->$1;
    moonbit_string_t _M0L8_2afieldS3460;
    moonbit_decref(_M0L8_2afieldS3461);
    _M0L8_2afieldS3460 = _M0L9_2ax__100S1258->$0;
    moonbit_decref(_M0L8_2afieldS3460);
    #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
    moonbit_free(_M0L9_2ax__100S1258);
  }
  _M0L7_2abindS1261 = _M0L8_2afieldS3156;
  if (_M0L7_2abindS1261 == 0) {
    if (_M0L7_2abindS1261) {
      moonbit_decref(_M0L7_2abindS1261);
    }
  } else {
    moonbit_string_t _M0L7_2aSomeS1262 = _M0L7_2abindS1261;
    moonbit_string_t _M0L11_2a_24innerS1263 = _M0L7_2aSomeS1262;
    _M0L8_24innerS1260 = _M0L11_2a_24innerS1263;
    goto join_1259;
  }
  goto joinlet_3603;
  join_1259:;
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2890
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L8_24innerS1260);
  moonbit_incref(_M0L6_24mapS1256);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1256, (moonbit_string_t)moonbit_string_literal_15.data, _M0L6_2atmpS2890);
  joinlet_3603:;
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1256);
}

struct moonbit_result_3 _M0IP48clawteam8clawteam5tools17replace__in__file5InputPC14json8FromJson10from__json(
  void* _M0L9_2ax__104S1235,
  void* _M0L9_2ax__105S1221
) {
  void* _M0L4NoneS2885;
  struct _M0TPC13ref3RefGOOsE* _M0L18_2ade__search__108S1214;
  moonbit_string_t _M0L6_2atmpS2884;
  struct _M0TPC13ref3RefGOsE* _M0L19_2ade__replace__107S1215;
  moonbit_string_t _M0L6_2atmpS2883;
  struct _M0TPC13ref3RefGOsE* _M0L16_2ade__path__106S1216;
  struct _M0TPB3MapGsRPB4JsonE* _M0L5__mapS1218;
  void* _M0L3__vS1220;
  void* _M0L7_2abindS1222;
  void* _M0L6_2atmpS2862;
  struct moonbit_result_4 _tmp_3607;
  moonbit_string_t _M0L6_2atmpS2861;
  moonbit_string_t _M0L6_2atmpS2860;
  moonbit_string_t _M0L6_2aoldS3165;
  void* _M0L3__vS1226;
  void* _M0L7_2abindS1227;
  void* _M0L6_2atmpS2867;
  struct moonbit_result_4 _tmp_3610;
  moonbit_string_t _M0L6_2atmpS2866;
  moonbit_string_t _M0L6_2atmpS2865;
  moonbit_string_t _M0L6_2aoldS3164;
  void* _M0L3__vS1231;
  void* _M0L7_2abindS1232;
  void* _M0L6_2atmpS2873;
  struct moonbit_result_4 _tmp_3613;
  moonbit_string_t _M0L6_2atmpS2872;
  moonbit_string_t _M0L6_2atmpS2871;
  void* _M0L4SomeS2870;
  void* _M0L6_2aoldS3163;
  moonbit_string_t _M0L1vS1240;
  moonbit_string_t _M0L18_2ade__search__108S1238;
  void* _M0L8_2afieldS3162;
  int32_t _M0L6_2acntS3465;
  void* _M0L7_2abindS1241;
  moonbit_string_t _M0L1vS1246;
  moonbit_string_t _M0L19_2ade__replace__107S1244;
  moonbit_string_t _M0L8_2afieldS3160;
  int32_t _M0L6_2acntS3469;
  moonbit_string_t _M0L7_2abindS1247;
  moonbit_string_t _M0L1vS1252;
  moonbit_string_t _M0L16_2ade__path__106S1250;
  moonbit_string_t _M0L8_2afieldS3159;
  int32_t _M0L6_2acntS3471;
  moonbit_string_t _M0L7_2abindS1253;
  struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input* _M0L6_2atmpS2878;
  struct moonbit_result_3 _result_3620;
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L4NoneS2885
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L18_2ade__search__108S1214
  = (struct _M0TPC13ref3RefGOOsE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGOOsE));
  Moonbit_object_header(_M0L18_2ade__search__108S1214)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGOOsE, $0) >> 2, 1, 0);
  _M0L18_2ade__search__108S1214->$0 = _M0L4NoneS2885;
  _M0L6_2atmpS2884 = 0;
  _M0L19_2ade__replace__107S1215
  = (struct _M0TPC13ref3RefGOsE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGOsE));
  Moonbit_object_header(_M0L19_2ade__replace__107S1215)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGOsE, $0) >> 2, 1, 0);
  _M0L19_2ade__replace__107S1215->$0 = _M0L6_2atmpS2884;
  _M0L6_2atmpS2883 = 0;
  _M0L16_2ade__path__106S1216
  = (struct _M0TPC13ref3RefGOsE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGOsE));
  Moonbit_object_header(_M0L16_2ade__path__106S1216)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGOsE, $0) >> 2, 1, 0);
  _M0L16_2ade__path__106S1216->$0 = _M0L6_2atmpS2883;
  switch (Moonbit_object_tag(_M0L9_2ax__104S1235)) {
    case 6: {
      struct _M0DTPB4Json6Object* _M0L9_2aObjectS1236 =
        (struct _M0DTPB4Json6Object*)_M0L9_2ax__104S1235;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3166 =
        _M0L9_2aObjectS1236->$0;
      int32_t _M0L6_2acntS3463 =
        Moonbit_object_header(_M0L9_2aObjectS1236)->rc;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2a__mapS1237;
      if (_M0L6_2acntS3463 > 1) {
        int32_t _M0L11_2anew__cntS3464 = _M0L6_2acntS3463 - 1;
        Moonbit_object_header(_M0L9_2aObjectS1236)->rc
        = _M0L11_2anew__cntS3464;
        moonbit_incref(_M0L8_2afieldS3166);
      } else if (_M0L6_2acntS3463 == 1) {
        #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
        moonbit_free(_M0L9_2aObjectS1236);
      }
      _M0L8_2a__mapS1237 = _M0L8_2afieldS3166;
      _M0L5__mapS1218 = _M0L8_2a__mapS1237;
      goto join_1217;
      break;
    }
    default: {
      struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2877;
      void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2876;
      struct moonbit_result_3 _result_3605;
      moonbit_decref(_M0L9_2ax__104S1235);
      moonbit_decref(_M0L16_2ade__path__106S1216);
      moonbit_decref(_M0L19_2ade__replace__107S1215);
      moonbit_decref(_M0L18_2ade__search__108S1214);
      _M0L8_2atupleS2877
      = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
      Moonbit_object_header(_M0L8_2atupleS2877)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
      _M0L8_2atupleS2877->$0 = _M0L9_2ax__105S1221;
      _M0L8_2atupleS2877->$1
      = (moonbit_string_t)moonbit_string_literal_19.data;
      _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2876
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
      Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2876)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
      ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2876)->$0
      = _M0L8_2atupleS2877;
      _result_3605.tag = 0;
      _result_3605.data.err
      = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2876;
      return _result_3605;
      break;
    }
  }
  goto joinlet_3604;
  join_1217:;
  moonbit_incref(_M0L5__mapS1218);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L7_2abindS1222
  = _M0MPB3Map3getGsRPB4JsonE(_M0L5__mapS1218, (moonbit_string_t)moonbit_string_literal_11.data);
  if (_M0L7_2abindS1222 == 0) {
    if (_M0L7_2abindS1222) {
      moonbit_decref(_M0L7_2abindS1222);
    }
  } else {
    void* _M0L7_2aSomeS1223 = _M0L7_2abindS1222;
    void* _M0L6_2a__vS1224 = _M0L7_2aSomeS1223;
    _M0L3__vS1220 = _M0L6_2a__vS1224;
    goto join_1219;
  }
  goto joinlet_3606;
  join_1219:;
  moonbit_incref(_M0L9_2ax__105S1221);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2862
  = _M0MPC14json8JsonPath8add__key(_M0L9_2ax__105S1221, (moonbit_string_t)moonbit_string_literal_11.data);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _tmp_3607
  = _M0IPC16string6StringPC14json8FromJson10from__json(_M0L3__vS1220, _M0L6_2atmpS2862);
  if (_tmp_3607.tag) {
    moonbit_string_t const _M0L5_2aokS2863 = _tmp_3607.data.ok;
    _M0L6_2atmpS2861 = _M0L5_2aokS2863;
  } else {
    void* const _M0L6_2aerrS2864 = _tmp_3607.data.err;
    struct moonbit_result_3 _result_3608;
    moonbit_decref(_M0L9_2ax__105S1221);
    moonbit_decref(_M0L5__mapS1218);
    moonbit_decref(_M0L16_2ade__path__106S1216);
    moonbit_decref(_M0L19_2ade__replace__107S1215);
    moonbit_decref(_M0L18_2ade__search__108S1214);
    _result_3608.tag = 0;
    _result_3608.data.err = _M0L6_2aerrS2864;
    return _result_3608;
  }
  _M0L6_2atmpS2860 = _M0L6_2atmpS2861;
  _M0L6_2aoldS3165 = _M0L16_2ade__path__106S1216->$0;
  if (_M0L6_2aoldS3165) {
    moonbit_decref(_M0L6_2aoldS3165);
  }
  _M0L16_2ade__path__106S1216->$0 = _M0L6_2atmpS2860;
  joinlet_3606:;
  moonbit_incref(_M0L5__mapS1218);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L7_2abindS1227
  = _M0MPB3Map3getGsRPB4JsonE(_M0L5__mapS1218, (moonbit_string_t)moonbit_string_literal_13.data);
  if (_M0L7_2abindS1227 == 0) {
    if (_M0L7_2abindS1227) {
      moonbit_decref(_M0L7_2abindS1227);
    }
  } else {
    void* _M0L7_2aSomeS1228 = _M0L7_2abindS1227;
    void* _M0L6_2a__vS1229 = _M0L7_2aSomeS1228;
    _M0L3__vS1226 = _M0L6_2a__vS1229;
    goto join_1225;
  }
  goto joinlet_3609;
  join_1225:;
  moonbit_incref(_M0L9_2ax__105S1221);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2867
  = _M0MPC14json8JsonPath8add__key(_M0L9_2ax__105S1221, (moonbit_string_t)moonbit_string_literal_13.data);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _tmp_3610
  = _M0IPC16string6StringPC14json8FromJson10from__json(_M0L3__vS1226, _M0L6_2atmpS2867);
  if (_tmp_3610.tag) {
    moonbit_string_t const _M0L5_2aokS2868 = _tmp_3610.data.ok;
    _M0L6_2atmpS2866 = _M0L5_2aokS2868;
  } else {
    void* const _M0L6_2aerrS2869 = _tmp_3610.data.err;
    struct moonbit_result_3 _result_3611;
    moonbit_decref(_M0L9_2ax__105S1221);
    moonbit_decref(_M0L5__mapS1218);
    moonbit_decref(_M0L16_2ade__path__106S1216);
    moonbit_decref(_M0L19_2ade__replace__107S1215);
    moonbit_decref(_M0L18_2ade__search__108S1214);
    _result_3611.tag = 0;
    _result_3611.data.err = _M0L6_2aerrS2869;
    return _result_3611;
  }
  _M0L6_2atmpS2865 = _M0L6_2atmpS2866;
  _M0L6_2aoldS3164 = _M0L19_2ade__replace__107S1215->$0;
  if (_M0L6_2aoldS3164) {
    moonbit_decref(_M0L6_2aoldS3164);
  }
  _M0L19_2ade__replace__107S1215->$0 = _M0L6_2atmpS2865;
  joinlet_3609:;
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L7_2abindS1232
  = _M0MPB3Map3getGsRPB4JsonE(_M0L5__mapS1218, (moonbit_string_t)moonbit_string_literal_15.data);
  if (_M0L7_2abindS1232 == 0) {
    if (_M0L7_2abindS1232) {
      moonbit_decref(_M0L7_2abindS1232);
    }
  } else {
    void* _M0L7_2aSomeS1233 = _M0L7_2abindS1232;
    void* _M0L6_2a__vS1234 = _M0L7_2aSomeS1233;
    _M0L3__vS1231 = _M0L6_2a__vS1234;
    goto join_1230;
  }
  goto joinlet_3612;
  join_1230:;
  moonbit_incref(_M0L9_2ax__105S1221);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _M0L6_2atmpS2873
  = _M0MPC14json8JsonPath8add__key(_M0L9_2ax__105S1221, (moonbit_string_t)moonbit_string_literal_15.data);
  #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
  _tmp_3613
  = _M0IPC16string6StringPC14json8FromJson10from__json(_M0L3__vS1231, _M0L6_2atmpS2873);
  if (_tmp_3613.tag) {
    moonbit_string_t const _M0L5_2aokS2874 = _tmp_3613.data.ok;
    _M0L6_2atmpS2872 = _M0L5_2aokS2874;
  } else {
    void* const _M0L6_2aerrS2875 = _tmp_3613.data.err;
    struct moonbit_result_3 _result_3614;
    moonbit_decref(_M0L9_2ax__105S1221);
    moonbit_decref(_M0L16_2ade__path__106S1216);
    moonbit_decref(_M0L19_2ade__replace__107S1215);
    moonbit_decref(_M0L18_2ade__search__108S1214);
    _result_3614.tag = 0;
    _result_3614.data.err = _M0L6_2aerrS2875;
    return _result_3614;
  }
  _M0L6_2atmpS2871 = _M0L6_2atmpS2872;
  _M0L4SomeS2870
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGOsE4Some));
  Moonbit_object_header(_M0L4SomeS2870)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGOsE4Some, $0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGOsE4Some*)_M0L4SomeS2870)->$0
  = _M0L6_2atmpS2871;
  _M0L6_2aoldS3163 = _M0L18_2ade__search__108S1214->$0;
  moonbit_decref(_M0L6_2aoldS3163);
  _M0L18_2ade__search__108S1214->$0 = _M0L4SomeS2870;
  joinlet_3612:;
  joinlet_3604:;
  _M0L8_2afieldS3162 = _M0L18_2ade__search__108S1214->$0;
  _M0L6_2acntS3465 = Moonbit_object_header(_M0L18_2ade__search__108S1214)->rc;
  if (_M0L6_2acntS3465 > 1) {
    int32_t _M0L11_2anew__cntS3466 = _M0L6_2acntS3465 - 1;
    Moonbit_object_header(_M0L18_2ade__search__108S1214)->rc
    = _M0L11_2anew__cntS3466;
    moonbit_incref(_M0L8_2afieldS3162);
  } else if (_M0L6_2acntS3465 == 1) {
    #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
    moonbit_free(_M0L18_2ade__search__108S1214);
  }
  _M0L7_2abindS1241 = _M0L8_2afieldS3162;
  switch (Moonbit_object_tag(_M0L7_2abindS1241)) {
    case 1: {
      struct _M0DTPC16option6OptionGOsE4Some* _M0L7_2aSomeS1242 =
        (struct _M0DTPC16option6OptionGOsE4Some*)_M0L7_2abindS1241;
      moonbit_string_t _M0L8_2afieldS3161 = _M0L7_2aSomeS1242->$0;
      int32_t _M0L6_2acntS3467 = Moonbit_object_header(_M0L7_2aSomeS1242)->rc;
      moonbit_string_t _M0L4_2avS1243;
      if (_M0L6_2acntS3467 > 1) {
        int32_t _M0L11_2anew__cntS3468 = _M0L6_2acntS3467 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1242)->rc = _M0L11_2anew__cntS3468;
        if (_M0L8_2afieldS3161) {
          moonbit_incref(_M0L8_2afieldS3161);
        }
      } else if (_M0L6_2acntS3467 == 1) {
        #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
        moonbit_free(_M0L7_2aSomeS1242);
      }
      _M0L4_2avS1243 = _M0L8_2afieldS3161;
      _M0L1vS1240 = _M0L4_2avS1243;
      goto join_1239;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS1241);
      _M0L18_2ade__search__108S1238 = 0;
      break;
    }
  }
  goto joinlet_3615;
  join_1239:;
  _M0L18_2ade__search__108S1238 = _M0L1vS1240;
  joinlet_3615:;
  _M0L8_2afieldS3160 = _M0L19_2ade__replace__107S1215->$0;
  _M0L6_2acntS3469
  = Moonbit_object_header(_M0L19_2ade__replace__107S1215)->rc;
  if (_M0L6_2acntS3469 > 1) {
    int32_t _M0L11_2anew__cntS3470 = _M0L6_2acntS3469 - 1;
    Moonbit_object_header(_M0L19_2ade__replace__107S1215)->rc
    = _M0L11_2anew__cntS3470;
    if (_M0L8_2afieldS3160) {
      moonbit_incref(_M0L8_2afieldS3160);
    }
  } else if (_M0L6_2acntS3469 == 1) {
    #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
    moonbit_free(_M0L19_2ade__replace__107S1215);
  }
  _M0L7_2abindS1247 = _M0L8_2afieldS3160;
  if (_M0L7_2abindS1247 == 0) {
    struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2882;
    void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2881;
    struct moonbit_result_3 _result_3617;
    if (_M0L7_2abindS1247) {
      moonbit_decref(_M0L7_2abindS1247);
    }
    if (_M0L18_2ade__search__108S1238) {
      moonbit_decref(_M0L18_2ade__search__108S1238);
    }
    moonbit_decref(_M0L16_2ade__path__106S1216);
    _M0L8_2atupleS2882
    = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
    Moonbit_object_header(_M0L8_2atupleS2882)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2882->$0 = _M0L9_2ax__105S1221;
    _M0L8_2atupleS2882->$1 = (moonbit_string_t)moonbit_string_literal_20.data;
    _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2881
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
    Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2881)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2881)->$0
    = _M0L8_2atupleS2882;
    _result_3617.tag = 0;
    _result_3617.data.err
    = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2881;
    return _result_3617;
  } else {
    moonbit_string_t _M0L7_2aSomeS1248 = _M0L7_2abindS1247;
    moonbit_string_t _M0L4_2avS1249 = _M0L7_2aSomeS1248;
    _M0L1vS1246 = _M0L4_2avS1249;
    goto join_1245;
  }
  goto joinlet_3616;
  join_1245:;
  _M0L19_2ade__replace__107S1244 = _M0L1vS1246;
  joinlet_3616:;
  _M0L8_2afieldS3159 = _M0L16_2ade__path__106S1216->$0;
  _M0L6_2acntS3471 = Moonbit_object_header(_M0L16_2ade__path__106S1216)->rc;
  if (_M0L6_2acntS3471 > 1) {
    int32_t _M0L11_2anew__cntS3472 = _M0L6_2acntS3471 - 1;
    Moonbit_object_header(_M0L16_2ade__path__106S1216)->rc
    = _M0L11_2anew__cntS3472;
    if (_M0L8_2afieldS3159) {
      moonbit_incref(_M0L8_2afieldS3159);
    }
  } else if (_M0L6_2acntS3471 == 1) {
    #line 24 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\schema.mbt"
    moonbit_free(_M0L16_2ade__path__106S1216);
  }
  _M0L7_2abindS1253 = _M0L8_2afieldS3159;
  if (_M0L7_2abindS1253 == 0) {
    struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2880;
    void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879;
    struct moonbit_result_3 _result_3619;
    if (_M0L7_2abindS1253) {
      moonbit_decref(_M0L7_2abindS1253);
    }
    moonbit_decref(_M0L19_2ade__replace__107S1244);
    if (_M0L18_2ade__search__108S1238) {
      moonbit_decref(_M0L18_2ade__search__108S1238);
    }
    _M0L8_2atupleS2880
    = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
    Moonbit_object_header(_M0L8_2atupleS2880)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2880->$0 = _M0L9_2ax__105S1221;
    _M0L8_2atupleS2880->$1 = (moonbit_string_t)moonbit_string_literal_21.data;
    _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
    Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879)->$0
    = _M0L8_2atupleS2880;
    _result_3619.tag = 0;
    _result_3619.data.err
    = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879;
    return _result_3619;
  } else {
    moonbit_string_t _M0L7_2aSomeS1254;
    moonbit_string_t _M0L4_2avS1255;
    moonbit_decref(_M0L9_2ax__105S1221);
    _M0L7_2aSomeS1254 = _M0L7_2abindS1253;
    _M0L4_2avS1255 = _M0L7_2aSomeS1254;
    _M0L1vS1252 = _M0L4_2avS1255;
    goto join_1251;
  }
  goto joinlet_3618;
  join_1251:;
  _M0L16_2ade__path__106S1250 = _M0L1vS1252;
  joinlet_3618:;
  _M0L6_2atmpS2878
  = (struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input));
  Moonbit_object_header(_M0L6_2atmpS2878)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam5tools17replace__in__file5Input, $0) >> 2, 3, 0);
  _M0L6_2atmpS2878->$0 = _M0L16_2ade__path__106S1250;
  _M0L6_2atmpS2878->$1 = _M0L19_2ade__replace__107S1244;
  _M0L6_2atmpS2878->$2 = _M0L18_2ade__search__108S1238;
  _result_3620.tag = 1;
  _result_3620.data.ok = _M0L6_2atmpS2878;
  return _result_3620;
}

struct moonbit_result_4 _M0IPC16string6StringPC14json8FromJson10from__json(
  void* _M0L4jsonS1210,
  void* _M0L4pathS1213
) {
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  switch (Moonbit_object_tag(_M0L4jsonS1210)) {
    case 4: {
      struct _M0DTPB4Json6String* _M0L9_2aStringS1211;
      moonbit_string_t _M0L8_2afieldS3167;
      int32_t _M0L6_2acntS3473;
      moonbit_string_t _M0L4_2aaS1212;
      struct moonbit_result_4 _result_3621;
      moonbit_decref(_M0L4pathS1213);
      _M0L9_2aStringS1211 = (struct _M0DTPB4Json6String*)_M0L4jsonS1210;
      _M0L8_2afieldS3167 = _M0L9_2aStringS1211->$0;
      _M0L6_2acntS3473 = Moonbit_object_header(_M0L9_2aStringS1211)->rc;
      if (_M0L6_2acntS3473 > 1) {
        int32_t _M0L11_2anew__cntS3474 = _M0L6_2acntS3473 - 1;
        Moonbit_object_header(_M0L9_2aStringS1211)->rc
        = _M0L11_2anew__cntS3474;
        moonbit_incref(_M0L8_2afieldS3167);
      } else if (_M0L6_2acntS3473 == 1) {
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
        moonbit_free(_M0L9_2aStringS1211);
      }
      _M0L4_2aaS1212 = _M0L8_2afieldS3167;
      _result_3621.tag = 1;
      _result_3621.data.ok = _M0L4_2aaS1212;
      return _result_3621;
      break;
    }
    default: {
      moonbit_decref(_M0L4jsonS1210);
      #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
      return _M0FPC14json13decode__errorGsE(_M0L4pathS1213, (moonbit_string_t)moonbit_string_literal_22.data);
      break;
    }
  }
}

struct moonbit_result_3 _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools17replace__in__file5InputE(
  void* _M0L4jsonS1209,
  void* _M0L10path_2eoptS1207
) {
  void* _M0L4pathS1206;
  if (_M0L10path_2eoptS1207 == 0) {
    if (_M0L10path_2eoptS1207) {
      moonbit_decref(_M0L10path_2eoptS1207);
    }
    _M0L4pathS1206
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    void* _M0L7_2aSomeS1208 = _M0L10path_2eoptS1207;
    _M0L4pathS1206 = _M0L7_2aSomeS1208;
  }
  return _M0FPC14json18from__json_2einnerGRP48clawteam8clawteam5tools17replace__in__file5InputE(_M0L4jsonS1209, _M0L4pathS1206);
}

struct moonbit_result_3 _M0FPC14json18from__json_2einnerGRP48clawteam8clawteam5tools17replace__in__file5InputE(
  void* _M0L4jsonS1204,
  void* _M0L4pathS1205
) {
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  return _M0IP48clawteam8clawteam5tools17replace__in__file5InputPC14json8FromJson10from__json(_M0L4jsonS1204, _M0L4pathS1205);
}

void* _M0IPC14json4JsonPB6ToJson8to__json(void* _M0L4selfS1203) {
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0L4selfS1203;
}

int32_t _M0IPC14json8JsonPathPB4Show6output(
  void* _M0L4selfS1201,
  struct _M0TPB6Logger _M0L6loggerS1202
) {
  #line 35 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(_M0L4selfS1201, _M0L6loggerS1202);
  return 0;
}

int32_t _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(
  void* _M0L4pathS1186,
  struct _M0TPB6Logger _M0L6loggerS1190
) {
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  switch (Moonbit_object_tag(_M0L4pathS1186)) {
    case 0: {
      if (_M0L6loggerS1190.$1) {
        moonbit_decref(_M0L6loggerS1190.$1);
      }
      break;
    }
    
    case 1: {
      struct _M0DTPC14json8JsonPath3Key* _M0L6_2aKeyS1187 =
        (struct _M0DTPC14json8JsonPath3Key*)_M0L4pathS1186;
      void* _M0L8_2afieldS3169 = _M0L6_2aKeyS1187->$0;
      void* _M0L9_2aparentS1188 = _M0L8_2afieldS3169;
      moonbit_string_t _M0L8_2afieldS3168 = _M0L6_2aKeyS1187->$1;
      int32_t _M0L6_2acntS3475 = Moonbit_object_header(_M0L6_2aKeyS1187)->rc;
      moonbit_string_t _M0L6_2akeyS1189;
      int32_t _M0L16_2areturn__valueS1192;
      int32_t _M0L6_2atmpS2858;
      struct _M0TPC16string10StringView _M0L6_2atmpS2857;
      int32_t _M0L6_2atmpS2856;
      struct _M0TWEOc* _M0L5_2aitS1193;
      if (_M0L6_2acntS3475 > 1) {
        int32_t _M0L11_2anew__cntS3476 = _M0L6_2acntS3475 - 1;
        Moonbit_object_header(_M0L6_2aKeyS1187)->rc = _M0L11_2anew__cntS3476;
        moonbit_incref(_M0L8_2afieldS3168);
        moonbit_incref(_M0L9_2aparentS1188);
      } else if (_M0L6_2acntS3475 == 1) {
        #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        moonbit_free(_M0L6_2aKeyS1187);
      }
      _M0L6_2akeyS1189 = _M0L8_2afieldS3168;
      if (_M0L6loggerS1190.$1) {
        moonbit_incref(_M0L6loggerS1190.$1);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(_M0L9_2aparentS1188, _M0L6loggerS1190);
      if (_M0L6loggerS1190.$1) {
        moonbit_incref(_M0L6loggerS1190.$1);
      }
      #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L6loggerS1190.$0->$method_3(_M0L6loggerS1190.$1, 47);
      _M0L6_2atmpS2858
      = Moonbit_array_length(_M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376);
      moonbit_incref(_M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376);
      _M0L6_2atmpS2857
      = (struct _M0TPC16string10StringView){
        0,
          _M0L6_2atmpS2858,
          _M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376
      };
      moonbit_incref(_M0L6_2akeyS1189);
      #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L6_2atmpS2856
      = _M0MPC16string6String13contains__any(_M0L6_2akeyS1189, _M0L6_2atmpS2857);
      if (!_M0L6_2atmpS2856) {
        int32_t _M0L6_2atmpS2859;
        #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        _M0L6loggerS1190.$0->$method_0(_M0L6loggerS1190.$1, _M0L6_2akeyS1189);
        _M0L6_2atmpS2859 = 0;
        _M0L16_2areturn__valueS1192 = _M0L6_2atmpS2859;
        goto join_1191;
      }
      #line 42 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L5_2aitS1193 = _M0MPC16string6String4iter(_M0L6_2akeyS1189);
      while (1) {
        int32_t _M0L7_2abindS1194;
        moonbit_incref(_M0L5_2aitS1193);
        #line 42 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        _M0L7_2abindS1194 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1193);
        if (_M0L7_2abindS1194 == -1) {
          moonbit_decref(_M0L5_2aitS1193);
          if (_M0L6loggerS1190.$1) {
            moonbit_decref(_M0L6loggerS1190.$1);
          }
        } else {
          int32_t _M0L7_2aSomeS1195 = _M0L7_2abindS1194;
          int32_t _M0L5_2achS1196 = _M0L7_2aSomeS1195;
          if (_M0L5_2achS1196 == 126) {
            if (_M0L6loggerS1190.$1) {
              moonbit_incref(_M0L6loggerS1190.$1);
            }
            #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
            _M0L6loggerS1190.$0->$method_0(_M0L6loggerS1190.$1, (moonbit_string_t)moonbit_string_literal_23.data);
          } else if (_M0L5_2achS1196 == 47) {
            if (_M0L6loggerS1190.$1) {
              moonbit_incref(_M0L6loggerS1190.$1);
            }
            #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
            _M0L6loggerS1190.$0->$method_0(_M0L6loggerS1190.$1, (moonbit_string_t)moonbit_string_literal_24.data);
          } else {
            if (_M0L6loggerS1190.$1) {
              moonbit_incref(_M0L6loggerS1190.$1);
            }
            #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
            _M0L6loggerS1190.$0->$method_3(_M0L6loggerS1190.$1, _M0L5_2achS1196);
          }
          continue;
        }
        break;
      }
      goto joinlet_3622;
      join_1191:;
      joinlet_3622:;
      break;
    }
    default: {
      struct _M0DTPC14json8JsonPath5Index* _M0L8_2aIndexS1198 =
        (struct _M0DTPC14json8JsonPath5Index*)_M0L4pathS1186;
      void* _M0L8_2afieldS3171 = _M0L8_2aIndexS1198->$0;
      void* _M0L9_2aparentS1199 = _M0L8_2afieldS3171;
      int32_t _M0L8_2afieldS3170 = _M0L8_2aIndexS1198->$1;
      int32_t _M0L6_2acntS3477 =
        Moonbit_object_header(_M0L8_2aIndexS1198)->rc;
      int32_t _M0L8_2aindexS1200;
      if (_M0L6_2acntS3477 > 1) {
        int32_t _M0L11_2anew__cntS3478 = _M0L6_2acntS3477 - 1;
        Moonbit_object_header(_M0L8_2aIndexS1198)->rc
        = _M0L11_2anew__cntS3478;
        moonbit_incref(_M0L9_2aparentS1199);
      } else if (_M0L6_2acntS3477 == 1) {
        #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        moonbit_free(_M0L8_2aIndexS1198);
      }
      _M0L8_2aindexS1200 = _M0L8_2afieldS3170;
      if (_M0L6loggerS1190.$1) {
        moonbit_incref(_M0L6loggerS1190.$1);
      }
      #line 61 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(_M0L9_2aparentS1199, _M0L6loggerS1190);
      if (_M0L6loggerS1190.$1) {
        moonbit_incref(_M0L6loggerS1190.$1);
      }
      #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L6loggerS1190.$0->$method_3(_M0L6loggerS1190.$1, 47);
      #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0MPB6Logger13write__objectGiE(_M0L6loggerS1190, _M0L8_2aindexS1200);
      break;
    }
  }
  return 0;
}

void* _M0MPC14json8JsonPath8add__key(
  void* _M0L4selfS1184,
  moonbit_string_t _M0L3keyS1185
) {
  void* _block_3624;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  _block_3624
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json8JsonPath3Key));
  Moonbit_object_header(_block_3624)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json8JsonPath3Key, $0) >> 2, 2, 1);
  ((struct _M0DTPC14json8JsonPath3Key*)_block_3624)->$0 = _M0L4selfS1184;
  ((struct _M0DTPC14json8JsonPath3Key*)_block_3624)->$1 = _M0L3keyS1185;
  return _block_3624;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1179,
  void* _M0L7contentS1181,
  moonbit_string_t _M0L3locS1175,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1177
) {
  moonbit_string_t _M0L3locS1174;
  moonbit_string_t _M0L9args__locS1176;
  void* _M0L6_2atmpS2854;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2855;
  moonbit_string_t _M0L6actualS1178;
  moonbit_string_t _M0L4wantS1180;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1174 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1175);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1176 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1177);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2854 = _M0L3objS1179.$0->$method_0(_M0L3objS1179.$1);
  _M0L6_2atmpS2855 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1178
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2854, 0, 0, _M0L6_2atmpS2855);
  if (_M0L7contentS1181 == 0) {
    void* _M0L6_2atmpS2851;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2852;
    if (_M0L7contentS1181) {
      moonbit_decref(_M0L7contentS1181);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2851
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_1.data);
    _M0L6_2atmpS2852 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1180
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2851, 0, 0, _M0L6_2atmpS2852);
  } else {
    void* _M0L7_2aSomeS1182 = _M0L7contentS1181;
    void* _M0L4_2axS1183 = _M0L7_2aSomeS1182;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2853 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1180
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1183, 0, 0, _M0L6_2atmpS2853);
  }
  moonbit_incref(_M0L4wantS1180);
  moonbit_incref(_M0L6actualS1178);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1178, _M0L4wantS1180)
  ) {
    moonbit_string_t _M0L6_2atmpS2849;
    moonbit_string_t _M0L6_2atmpS3179;
    moonbit_string_t _M0L6_2atmpS2848;
    moonbit_string_t _M0L6_2atmpS3178;
    moonbit_string_t _M0L6_2atmpS2846;
    moonbit_string_t _M0L6_2atmpS2847;
    moonbit_string_t _M0L6_2atmpS3177;
    moonbit_string_t _M0L6_2atmpS2845;
    moonbit_string_t _M0L6_2atmpS3176;
    moonbit_string_t _M0L6_2atmpS2842;
    moonbit_string_t _M0L6_2atmpS2844;
    moonbit_string_t _M0L6_2atmpS2843;
    moonbit_string_t _M0L6_2atmpS3175;
    moonbit_string_t _M0L6_2atmpS2841;
    moonbit_string_t _M0L6_2atmpS3174;
    moonbit_string_t _M0L6_2atmpS2838;
    moonbit_string_t _M0L6_2atmpS2840;
    moonbit_string_t _M0L6_2atmpS2839;
    moonbit_string_t _M0L6_2atmpS3173;
    moonbit_string_t _M0L6_2atmpS2837;
    moonbit_string_t _M0L6_2atmpS3172;
    moonbit_string_t _M0L6_2atmpS2836;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2835;
    struct moonbit_result_0 _result_3625;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2849
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1174);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3179
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_25.data, _M0L6_2atmpS2849);
    moonbit_decref(_M0L6_2atmpS2849);
    _M0L6_2atmpS2848 = _M0L6_2atmpS3179;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3178
    = moonbit_add_string(_M0L6_2atmpS2848, (moonbit_string_t)moonbit_string_literal_26.data);
    moonbit_decref(_M0L6_2atmpS2848);
    _M0L6_2atmpS2846 = _M0L6_2atmpS3178;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2847
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1176);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3177 = moonbit_add_string(_M0L6_2atmpS2846, _M0L6_2atmpS2847);
    moonbit_decref(_M0L6_2atmpS2846);
    moonbit_decref(_M0L6_2atmpS2847);
    _M0L6_2atmpS2845 = _M0L6_2atmpS3177;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3176
    = moonbit_add_string(_M0L6_2atmpS2845, (moonbit_string_t)moonbit_string_literal_27.data);
    moonbit_decref(_M0L6_2atmpS2845);
    _M0L6_2atmpS2842 = _M0L6_2atmpS3176;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2844 = _M0MPC16string6String6escape(_M0L4wantS1180);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2843
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2844);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3175 = moonbit_add_string(_M0L6_2atmpS2842, _M0L6_2atmpS2843);
    moonbit_decref(_M0L6_2atmpS2842);
    moonbit_decref(_M0L6_2atmpS2843);
    _M0L6_2atmpS2841 = _M0L6_2atmpS3175;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3174
    = moonbit_add_string(_M0L6_2atmpS2841, (moonbit_string_t)moonbit_string_literal_28.data);
    moonbit_decref(_M0L6_2atmpS2841);
    _M0L6_2atmpS2838 = _M0L6_2atmpS3174;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2840 = _M0MPC16string6String6escape(_M0L6actualS1178);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2839
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2840);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3173 = moonbit_add_string(_M0L6_2atmpS2838, _M0L6_2atmpS2839);
    moonbit_decref(_M0L6_2atmpS2838);
    moonbit_decref(_M0L6_2atmpS2839);
    _M0L6_2atmpS2837 = _M0L6_2atmpS3173;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3172
    = moonbit_add_string(_M0L6_2atmpS2837, (moonbit_string_t)moonbit_string_literal_29.data);
    moonbit_decref(_M0L6_2atmpS2837);
    _M0L6_2atmpS2836 = _M0L6_2atmpS3172;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2835
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2835)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2835)->$0
    = _M0L6_2atmpS2836;
    _result_3625.tag = 0;
    _result_3625.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2835;
    return _result_3625;
  } else {
    int32_t _M0L6_2atmpS2850;
    struct moonbit_result_0 _result_3626;
    moonbit_decref(_M0L4wantS1180);
    moonbit_decref(_M0L6actualS1178);
    moonbit_decref(_M0L9args__locS1176);
    moonbit_decref(_M0L3locS1174);
    _M0L6_2atmpS2850 = 0;
    _result_3626.tag = 1;
    _result_3626.data.ok = _M0L6_2atmpS2850;
    return _result_3626;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1173,
  int32_t _M0L13escape__slashS1145,
  int32_t _M0L6indentS1140,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1166
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1132;
  void** _M0L6_2atmpS2834;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1133;
  int32_t _M0Lm5depthS1134;
  void* _M0L6_2atmpS2833;
  void* _M0L8_2aparamS1135;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1132 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2834 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1133
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1133)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1133->$0 = _M0L6_2atmpS2834;
  _M0L5stackS1133->$1 = 0;
  _M0Lm5depthS1134 = 0;
  _M0L6_2atmpS2833 = _M0L4selfS1173;
  _M0L8_2aparamS1135 = _M0L6_2atmpS2833;
  _2aloop_1151:;
  while (1) {
    if (_M0L8_2aparamS1135 == 0) {
      int32_t _M0L3lenS2795;
      if (_M0L8_2aparamS1135) {
        moonbit_decref(_M0L8_2aparamS1135);
      }
      _M0L3lenS2795 = _M0L5stackS1133->$1;
      if (_M0L3lenS2795 == 0) {
        if (_M0L8replacerS1166) {
          moonbit_decref(_M0L8replacerS1166);
        }
        moonbit_decref(_M0L5stackS1133);
        break;
      } else {
        void** _M0L8_2afieldS3187 = _M0L5stackS1133->$0;
        void** _M0L3bufS2819 = _M0L8_2afieldS3187;
        int32_t _M0L3lenS2821 = _M0L5stackS1133->$1;
        int32_t _M0L6_2atmpS2820 = _M0L3lenS2821 - 1;
        void* _M0L6_2atmpS3186 = (void*)_M0L3bufS2819[_M0L6_2atmpS2820];
        void* _M0L4_2axS1152 = _M0L6_2atmpS3186;
        switch (Moonbit_object_tag(_M0L4_2axS1152)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1153 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1152;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3182 =
              _M0L8_2aArrayS1153->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1154 =
              _M0L8_2afieldS3182;
            int32_t _M0L4_2aiS1155 = _M0L8_2aArrayS1153->$1;
            int32_t _M0L3lenS2807 = _M0L6_2aarrS1154->$1;
            if (_M0L4_2aiS1155 < _M0L3lenS2807) {
              int32_t _if__result_3628;
              void** _M0L8_2afieldS3181;
              void** _M0L3bufS2813;
              void* _M0L6_2atmpS3180;
              void* _M0L7elementS1156;
              int32_t _M0L6_2atmpS2808;
              void* _M0L6_2atmpS2811;
              if (_M0L4_2aiS1155 < 0) {
                _if__result_3628 = 1;
              } else {
                int32_t _M0L3lenS2812 = _M0L6_2aarrS1154->$1;
                _if__result_3628 = _M0L4_2aiS1155 >= _M0L3lenS2812;
              }
              if (_if__result_3628) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3181 = _M0L6_2aarrS1154->$0;
              _M0L3bufS2813 = _M0L8_2afieldS3181;
              _M0L6_2atmpS3180 = (void*)_M0L3bufS2813[_M0L4_2aiS1155];
              _M0L7elementS1156 = _M0L6_2atmpS3180;
              _M0L6_2atmpS2808 = _M0L4_2aiS1155 + 1;
              _M0L8_2aArrayS1153->$1 = _M0L6_2atmpS2808;
              if (_M0L4_2aiS1155 > 0) {
                int32_t _M0L6_2atmpS2810;
                moonbit_string_t _M0L6_2atmpS2809;
                moonbit_incref(_M0L7elementS1156);
                moonbit_incref(_M0L3bufS1132);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 44);
                _M0L6_2atmpS2810 = _M0Lm5depthS1134;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2809
                = _M0FPC14json11indent__str(_M0L6_2atmpS2810, _M0L6indentS1140);
                moonbit_incref(_M0L3bufS1132);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2809);
              } else {
                moonbit_incref(_M0L7elementS1156);
              }
              _M0L6_2atmpS2811 = _M0L7elementS1156;
              _M0L8_2aparamS1135 = _M0L6_2atmpS2811;
              goto _2aloop_1151;
            } else {
              int32_t _M0L6_2atmpS2814 = _M0Lm5depthS1134;
              void* _M0L6_2atmpS2815;
              int32_t _M0L6_2atmpS2817;
              moonbit_string_t _M0L6_2atmpS2816;
              void* _M0L6_2atmpS2818;
              _M0Lm5depthS1134 = _M0L6_2atmpS2814 - 1;
              moonbit_incref(_M0L5stackS1133);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2815
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1133);
              if (_M0L6_2atmpS2815) {
                moonbit_decref(_M0L6_2atmpS2815);
              }
              _M0L6_2atmpS2817 = _M0Lm5depthS1134;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2816
              = _M0FPC14json11indent__str(_M0L6_2atmpS2817, _M0L6indentS1140);
              moonbit_incref(_M0L3bufS1132);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2816);
              moonbit_incref(_M0L3bufS1132);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 93);
              _M0L6_2atmpS2818 = 0;
              _M0L8_2aparamS1135 = _M0L6_2atmpS2818;
              goto _2aloop_1151;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1157 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1152;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3185 =
              _M0L9_2aObjectS1157->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1158 =
              _M0L8_2afieldS3185;
            int32_t _M0L8_2afirstS1159 = _M0L9_2aObjectS1157->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1160;
            moonbit_incref(_M0L11_2aiteratorS1158);
            moonbit_incref(_M0L9_2aObjectS1157);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1160
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1158);
            if (_M0L7_2abindS1160 == 0) {
              int32_t _M0L6_2atmpS2796;
              void* _M0L6_2atmpS2797;
              int32_t _M0L6_2atmpS2799;
              moonbit_string_t _M0L6_2atmpS2798;
              void* _M0L6_2atmpS2800;
              if (_M0L7_2abindS1160) {
                moonbit_decref(_M0L7_2abindS1160);
              }
              moonbit_decref(_M0L9_2aObjectS1157);
              _M0L6_2atmpS2796 = _M0Lm5depthS1134;
              _M0Lm5depthS1134 = _M0L6_2atmpS2796 - 1;
              moonbit_incref(_M0L5stackS1133);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2797
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1133);
              if (_M0L6_2atmpS2797) {
                moonbit_decref(_M0L6_2atmpS2797);
              }
              _M0L6_2atmpS2799 = _M0Lm5depthS1134;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2798
              = _M0FPC14json11indent__str(_M0L6_2atmpS2799, _M0L6indentS1140);
              moonbit_incref(_M0L3bufS1132);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2798);
              moonbit_incref(_M0L3bufS1132);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 125);
              _M0L6_2atmpS2800 = 0;
              _M0L8_2aparamS1135 = _M0L6_2atmpS2800;
              goto _2aloop_1151;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1161 = _M0L7_2abindS1160;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1162 = _M0L7_2aSomeS1161;
              moonbit_string_t _M0L8_2afieldS3184 = _M0L4_2axS1162->$0;
              moonbit_string_t _M0L4_2akS1163 = _M0L8_2afieldS3184;
              void* _M0L8_2afieldS3183 = _M0L4_2axS1162->$1;
              int32_t _M0L6_2acntS3479 =
                Moonbit_object_header(_M0L4_2axS1162)->rc;
              void* _M0L4_2avS1164;
              void* _M0Lm2v2S1165;
              moonbit_string_t _M0L6_2atmpS2804;
              void* _M0L6_2atmpS2806;
              void* _M0L6_2atmpS2805;
              if (_M0L6_2acntS3479 > 1) {
                int32_t _M0L11_2anew__cntS3480 = _M0L6_2acntS3479 - 1;
                Moonbit_object_header(_M0L4_2axS1162)->rc
                = _M0L11_2anew__cntS3480;
                moonbit_incref(_M0L8_2afieldS3183);
                moonbit_incref(_M0L4_2akS1163);
              } else if (_M0L6_2acntS3479 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1162);
              }
              _M0L4_2avS1164 = _M0L8_2afieldS3183;
              _M0Lm2v2S1165 = _M0L4_2avS1164;
              if (_M0L8replacerS1166 == 0) {
                moonbit_incref(_M0Lm2v2S1165);
                moonbit_decref(_M0L4_2avS1164);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1167 =
                  _M0L8replacerS1166;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1168 =
                  _M0L7_2aSomeS1167;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1169 =
                  _M0L11_2areplacerS1168;
                void* _M0L7_2abindS1170;
                moonbit_incref(_M0L7_2afuncS1169);
                moonbit_incref(_M0L4_2akS1163);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1170
                = _M0L7_2afuncS1169->code(_M0L7_2afuncS1169, _M0L4_2akS1163, _M0L4_2avS1164);
                if (_M0L7_2abindS1170 == 0) {
                  void* _M0L6_2atmpS2801;
                  if (_M0L7_2abindS1170) {
                    moonbit_decref(_M0L7_2abindS1170);
                  }
                  moonbit_decref(_M0L4_2akS1163);
                  moonbit_decref(_M0L9_2aObjectS1157);
                  _M0L6_2atmpS2801 = 0;
                  _M0L8_2aparamS1135 = _M0L6_2atmpS2801;
                  goto _2aloop_1151;
                } else {
                  void* _M0L7_2aSomeS1171 = _M0L7_2abindS1170;
                  void* _M0L4_2avS1172 = _M0L7_2aSomeS1171;
                  _M0Lm2v2S1165 = _M0L4_2avS1172;
                }
              }
              if (!_M0L8_2afirstS1159) {
                int32_t _M0L6_2atmpS2803;
                moonbit_string_t _M0L6_2atmpS2802;
                moonbit_incref(_M0L3bufS1132);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 44);
                _M0L6_2atmpS2803 = _M0Lm5depthS1134;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2802
                = _M0FPC14json11indent__str(_M0L6_2atmpS2803, _M0L6indentS1140);
                moonbit_incref(_M0L3bufS1132);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2802);
              }
              moonbit_incref(_M0L3bufS1132);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2804
              = _M0FPC14json6escape(_M0L4_2akS1163, _M0L13escape__slashS1145);
              moonbit_incref(_M0L3bufS1132);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2804);
              moonbit_incref(_M0L3bufS1132);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 34);
              moonbit_incref(_M0L3bufS1132);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 58);
              if (_M0L6indentS1140 > 0) {
                moonbit_incref(_M0L3bufS1132);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 32);
              }
              _M0L9_2aObjectS1157->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1157);
              _M0L6_2atmpS2806 = _M0Lm2v2S1165;
              _M0L6_2atmpS2805 = _M0L6_2atmpS2806;
              _M0L8_2aparamS1135 = _M0L6_2atmpS2805;
              goto _2aloop_1151;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1136 = _M0L8_2aparamS1135;
      void* _M0L8_2avalueS1137 = _M0L7_2aSomeS1136;
      void* _M0L6_2atmpS2832;
      switch (Moonbit_object_tag(_M0L8_2avalueS1137)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1138 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1137;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3188 =
            _M0L9_2aObjectS1138->$0;
          int32_t _M0L6_2acntS3481 =
            Moonbit_object_header(_M0L9_2aObjectS1138)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1139;
          if (_M0L6_2acntS3481 > 1) {
            int32_t _M0L11_2anew__cntS3482 = _M0L6_2acntS3481 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1138)->rc
            = _M0L11_2anew__cntS3482;
            moonbit_incref(_M0L8_2afieldS3188);
          } else if (_M0L6_2acntS3481 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1138);
          }
          _M0L10_2amembersS1139 = _M0L8_2afieldS3188;
          moonbit_incref(_M0L10_2amembersS1139);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1139)) {
            moonbit_decref(_M0L10_2amembersS1139);
            moonbit_incref(_M0L3bufS1132);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, (moonbit_string_t)moonbit_string_literal_30.data);
          } else {
            int32_t _M0L6_2atmpS2827 = _M0Lm5depthS1134;
            int32_t _M0L6_2atmpS2829;
            moonbit_string_t _M0L6_2atmpS2828;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2831;
            void* _M0L6ObjectS2830;
            _M0Lm5depthS1134 = _M0L6_2atmpS2827 + 1;
            moonbit_incref(_M0L3bufS1132);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 123);
            _M0L6_2atmpS2829 = _M0Lm5depthS1134;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2828
            = _M0FPC14json11indent__str(_M0L6_2atmpS2829, _M0L6indentS1140);
            moonbit_incref(_M0L3bufS1132);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2828);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2831
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1139);
            _M0L6ObjectS2830
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2830)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2830)->$0
            = _M0L6_2atmpS2831;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2830)->$1
            = 1;
            moonbit_incref(_M0L5stackS1133);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1133, _M0L6ObjectS2830);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1141 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1137;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3189 =
            _M0L8_2aArrayS1141->$0;
          int32_t _M0L6_2acntS3483 =
            Moonbit_object_header(_M0L8_2aArrayS1141)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1142;
          if (_M0L6_2acntS3483 > 1) {
            int32_t _M0L11_2anew__cntS3484 = _M0L6_2acntS3483 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1141)->rc
            = _M0L11_2anew__cntS3484;
            moonbit_incref(_M0L8_2afieldS3189);
          } else if (_M0L6_2acntS3483 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1141);
          }
          _M0L6_2aarrS1142 = _M0L8_2afieldS3189;
          moonbit_incref(_M0L6_2aarrS1142);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1142)) {
            moonbit_decref(_M0L6_2aarrS1142);
            moonbit_incref(_M0L3bufS1132);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, (moonbit_string_t)moonbit_string_literal_31.data);
          } else {
            int32_t _M0L6_2atmpS2823 = _M0Lm5depthS1134;
            int32_t _M0L6_2atmpS2825;
            moonbit_string_t _M0L6_2atmpS2824;
            void* _M0L5ArrayS2826;
            _M0Lm5depthS1134 = _M0L6_2atmpS2823 + 1;
            moonbit_incref(_M0L3bufS1132);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 91);
            _M0L6_2atmpS2825 = _M0Lm5depthS1134;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2824
            = _M0FPC14json11indent__str(_M0L6_2atmpS2825, _M0L6indentS1140);
            moonbit_incref(_M0L3bufS1132);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2824);
            _M0L5ArrayS2826
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2826)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2826)->$0
            = _M0L6_2aarrS1142;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2826)->$1
            = 0;
            moonbit_incref(_M0L5stackS1133);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1133, _M0L5ArrayS2826);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1143 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1137;
          moonbit_string_t _M0L8_2afieldS3190 = _M0L9_2aStringS1143->$0;
          int32_t _M0L6_2acntS3485 =
            Moonbit_object_header(_M0L9_2aStringS1143)->rc;
          moonbit_string_t _M0L4_2asS1144;
          moonbit_string_t _M0L6_2atmpS2822;
          if (_M0L6_2acntS3485 > 1) {
            int32_t _M0L11_2anew__cntS3486 = _M0L6_2acntS3485 - 1;
            Moonbit_object_header(_M0L9_2aStringS1143)->rc
            = _M0L11_2anew__cntS3486;
            moonbit_incref(_M0L8_2afieldS3190);
          } else if (_M0L6_2acntS3485 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1143);
          }
          _M0L4_2asS1144 = _M0L8_2afieldS3190;
          moonbit_incref(_M0L3bufS1132);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2822
          = _M0FPC14json6escape(_M0L4_2asS1144, _M0L13escape__slashS1145);
          moonbit_incref(_M0L3bufS1132);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L6_2atmpS2822);
          moonbit_incref(_M0L3bufS1132);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1132, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1146 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1137;
          double _M0L4_2anS1147 = _M0L9_2aNumberS1146->$0;
          moonbit_string_t _M0L8_2afieldS3191 = _M0L9_2aNumberS1146->$1;
          int32_t _M0L6_2acntS3487 =
            Moonbit_object_header(_M0L9_2aNumberS1146)->rc;
          moonbit_string_t _M0L7_2areprS1148;
          if (_M0L6_2acntS3487 > 1) {
            int32_t _M0L11_2anew__cntS3488 = _M0L6_2acntS3487 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1146)->rc
            = _M0L11_2anew__cntS3488;
            if (_M0L8_2afieldS3191) {
              moonbit_incref(_M0L8_2afieldS3191);
            }
          } else if (_M0L6_2acntS3487 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1146);
          }
          _M0L7_2areprS1148 = _M0L8_2afieldS3191;
          if (_M0L7_2areprS1148 == 0) {
            if (_M0L7_2areprS1148) {
              moonbit_decref(_M0L7_2areprS1148);
            }
            moonbit_incref(_M0L3bufS1132);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1132, _M0L4_2anS1147);
          } else {
            moonbit_string_t _M0L7_2aSomeS1149 = _M0L7_2areprS1148;
            moonbit_string_t _M0L4_2arS1150 = _M0L7_2aSomeS1149;
            moonbit_incref(_M0L3bufS1132);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, _M0L4_2arS1150);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1132);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, (moonbit_string_t)moonbit_string_literal_32.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1132);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, (moonbit_string_t)moonbit_string_literal_33.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1137);
          moonbit_incref(_M0L3bufS1132);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1132, (moonbit_string_t)moonbit_string_literal_34.data);
          break;
        }
      }
      _M0L6_2atmpS2832 = 0;
      _M0L8_2aparamS1135 = _M0L6_2atmpS2832;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1132);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1131,
  int32_t _M0L6indentS1129
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1129 == 0) {
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else {
    int32_t _M0L6spacesS1130 = _M0L6indentS1129 * _M0L5levelS1131;
    switch (_M0L6spacesS1130) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_35.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_36.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_37.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_38.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_39.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_40.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_41.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_42.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_43.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2794;
        moonbit_string_t _M0L6_2atmpS3192;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2794
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_44.data, _M0L6spacesS1130);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3192
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS2794);
        moonbit_decref(_M0L6_2atmpS2794);
        return _M0L6_2atmpS3192;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1121,
  int32_t _M0L13escape__slashS1126
) {
  int32_t _M0L6_2atmpS2793;
  struct _M0TPB13StringBuilder* _M0L3bufS1120;
  struct _M0TWEOc* _M0L5_2aitS1122;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2793 = Moonbit_array_length(_M0L3strS1121);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1120 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2793);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1122 = _M0MPC16string6String4iter(_M0L3strS1121);
  while (1) {
    int32_t _M0L7_2abindS1123;
    moonbit_incref(_M0L5_2aitS1122);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1123 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1122);
    if (_M0L7_2abindS1123 == -1) {
      moonbit_decref(_M0L5_2aitS1122);
    } else {
      int32_t _M0L7_2aSomeS1124 = _M0L7_2abindS1123;
      int32_t _M0L4_2acS1125 = _M0L7_2aSomeS1124;
      if (_M0L4_2acS1125 == 34) {
        moonbit_incref(_M0L3bufS1120);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_45.data);
      } else if (_M0L4_2acS1125 == 92) {
        moonbit_incref(_M0L3bufS1120);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_46.data);
      } else if (_M0L4_2acS1125 == 47) {
        if (_M0L13escape__slashS1126) {
          moonbit_incref(_M0L3bufS1120);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_47.data);
        } else {
          moonbit_incref(_M0L3bufS1120);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1120, _M0L4_2acS1125);
        }
      } else if (_M0L4_2acS1125 == 10) {
        moonbit_incref(_M0L3bufS1120);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_48.data);
      } else if (_M0L4_2acS1125 == 13) {
        moonbit_incref(_M0L3bufS1120);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_49.data);
      } else if (_M0L4_2acS1125 == 8) {
        moonbit_incref(_M0L3bufS1120);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_50.data);
      } else if (_M0L4_2acS1125 == 9) {
        moonbit_incref(_M0L3bufS1120);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_51.data);
      } else {
        int32_t _M0L4codeS1127 = _M0L4_2acS1125;
        if (_M0L4codeS1127 == 12) {
          moonbit_incref(_M0L3bufS1120);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_52.data);
        } else if (_M0L4codeS1127 < 32) {
          int32_t _M0L6_2atmpS2792;
          moonbit_string_t _M0L6_2atmpS2791;
          moonbit_incref(_M0L3bufS1120);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, (moonbit_string_t)moonbit_string_literal_53.data);
          _M0L6_2atmpS2792 = _M0L4codeS1127 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2791 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2792);
          moonbit_incref(_M0L3bufS1120);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1120, _M0L6_2atmpS2791);
        } else {
          moonbit_incref(_M0L3bufS1120);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1120, _M0L4_2acS1125);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1120);
}

struct moonbit_result_4 _M0FPC14json13decode__errorGsE(
  void* _M0L4pathS1118,
  moonbit_string_t _M0L3msgS1119
) {
  struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2790;
  void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2789;
  struct moonbit_result_4 _result_3630;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L8_2atupleS2790
  = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
  Moonbit_object_header(_M0L8_2atupleS2790)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2790->$0 = _M0L4pathS1118;
  _M0L8_2atupleS2790->$1 = _M0L3msgS1119;
  _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2789
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
  Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2789)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2789)->$0
  = _M0L8_2atupleS2790;
  _result_3630.tag = 0;
  _result_3630.data.err
  = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2789;
  return _result_3630;
}

int32_t _M0IPC14json15JsonDecodeErrorPB4Show6output(
  void* _M0L9_2ax__628S1112,
  struct _M0TPB6Logger _M0L9_2ax__629S1115
) {
  struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError* _M0L18_2aJsonDecodeErrorS1113;
  struct _M0TURPC14json8JsonPathsE* _M0L8_2afieldS3195;
  int32_t _M0L6_2acntS3489;
  struct _M0TURPC14json8JsonPathsE* _M0L14_2a_2aarg__630S1114;
  void* _M0L8_2afieldS3194;
  void* _M0L13_2a_2ax0__631S1116;
  moonbit_string_t _M0L8_2afieldS3193;
  int32_t _M0L6_2acntS3491;
  moonbit_string_t _M0L13_2a_2ax1__632S1117;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L18_2aJsonDecodeErrorS1113
  = (struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L9_2ax__628S1112;
  _M0L8_2afieldS3195 = _M0L18_2aJsonDecodeErrorS1113->$0;
  _M0L6_2acntS3489 = Moonbit_object_header(_M0L18_2aJsonDecodeErrorS1113)->rc;
  if (_M0L6_2acntS3489 > 1) {
    int32_t _M0L11_2anew__cntS3490 = _M0L6_2acntS3489 - 1;
    Moonbit_object_header(_M0L18_2aJsonDecodeErrorS1113)->rc
    = _M0L11_2anew__cntS3490;
    moonbit_incref(_M0L8_2afieldS3195);
  } else if (_M0L6_2acntS3489 == 1) {
    #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
    moonbit_free(_M0L18_2aJsonDecodeErrorS1113);
  }
  _M0L14_2a_2aarg__630S1114 = _M0L8_2afieldS3195;
  if (_M0L9_2ax__629S1115.$1) {
    moonbit_incref(_M0L9_2ax__629S1115.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1115.$0->$method_0(_M0L9_2ax__629S1115.$1, (moonbit_string_t)moonbit_string_literal_54.data);
  _M0L8_2afieldS3194 = _M0L14_2a_2aarg__630S1114->$0;
  _M0L13_2a_2ax0__631S1116 = _M0L8_2afieldS3194;
  _M0L8_2afieldS3193 = _M0L14_2a_2aarg__630S1114->$1;
  _M0L6_2acntS3491 = Moonbit_object_header(_M0L14_2a_2aarg__630S1114)->rc;
  if (_M0L6_2acntS3491 > 1) {
    int32_t _M0L11_2anew__cntS3492 = _M0L6_2acntS3491 - 1;
    Moonbit_object_header(_M0L14_2a_2aarg__630S1114)->rc
    = _M0L11_2anew__cntS3492;
    moonbit_incref(_M0L8_2afieldS3193);
    moonbit_incref(_M0L13_2a_2ax0__631S1116);
  } else if (_M0L6_2acntS3491 == 1) {
    #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
    moonbit_free(_M0L14_2a_2aarg__630S1114);
  }
  _M0L13_2a_2ax1__632S1117 = _M0L8_2afieldS3193;
  if (_M0L9_2ax__629S1115.$1) {
    moonbit_incref(_M0L9_2ax__629S1115.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1115.$0->$method_0(_M0L9_2ax__629S1115.$1, (moonbit_string_t)moonbit_string_literal_55.data);
  if (_M0L9_2ax__629S1115.$1) {
    moonbit_incref(_M0L9_2ax__629S1115.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0MPB6Logger13write__objectGRPC14json8JsonPathE(_M0L9_2ax__629S1115, _M0L13_2a_2ax0__631S1116);
  if (_M0L9_2ax__629S1115.$1) {
    moonbit_incref(_M0L9_2ax__629S1115.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1115.$0->$method_0(_M0L9_2ax__629S1115.$1, (moonbit_string_t)moonbit_string_literal_56.data);
  if (_M0L9_2ax__629S1115.$1) {
    moonbit_incref(_M0L9_2ax__629S1115.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L9_2ax__629S1115, _M0L13_2a_2ax1__632S1117);
  if (_M0L9_2ax__629S1115.$1) {
    moonbit_incref(_M0L9_2ax__629S1115.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1115.$0->$method_0(_M0L9_2ax__629S1115.$1, (moonbit_string_t)moonbit_string_literal_57.data);
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1115.$0->$method_0(_M0L9_2ax__629S1115.$1, (moonbit_string_t)moonbit_string_literal_57.data);
  return 0;
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1111
) {
  int32_t _M0L8_2afieldS3196;
  int32_t _M0L3lenS2788;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3196 = _M0L4selfS1111->$1;
  moonbit_decref(_M0L4selfS1111);
  _M0L3lenS2788 = _M0L8_2afieldS3196;
  return _M0L3lenS2788 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1108
) {
  int32_t _M0L3lenS1107;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1107 = _M0L4selfS1108->$1;
  if (_M0L3lenS1107 == 0) {
    moonbit_decref(_M0L4selfS1108);
    return 0;
  } else {
    int32_t _M0L5indexS1109 = _M0L3lenS1107 - 1;
    void** _M0L8_2afieldS3200 = _M0L4selfS1108->$0;
    void** _M0L3bufS2787 = _M0L8_2afieldS3200;
    void* _M0L6_2atmpS3199 = (void*)_M0L3bufS2787[_M0L5indexS1109];
    void* _M0L1vS1110 = _M0L6_2atmpS3199;
    void** _M0L8_2afieldS3198 = _M0L4selfS1108->$0;
    void** _M0L3bufS2786 = _M0L8_2afieldS3198;
    void* _M0L6_2aoldS3197;
    if (
      _M0L5indexS1109 < 0
      || _M0L5indexS1109 >= Moonbit_array_length(_M0L3bufS2786)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3197 = (void*)_M0L3bufS2786[_M0L5indexS1109];
    moonbit_incref(_M0L1vS1110);
    moonbit_decref(_M0L6_2aoldS3197);
    if (
      _M0L5indexS1109 < 0
      || _M0L5indexS1109 >= Moonbit_array_length(_M0L3bufS2786)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2786[_M0L5indexS1109]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1108->$1 = _M0L5indexS1109;
    moonbit_decref(_M0L4selfS1108);
    return _M0L1vS1110;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1105,
  struct _M0TPB6Logger _M0L6loggerS1106
) {
  moonbit_string_t _M0L6_2atmpS2785;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2784;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2785 = _M0L4selfS1105;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2784 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2785);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2784, _M0L6loggerS1106);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1082,
  struct _M0TPB6Logger _M0L6loggerS1104
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3209;
  struct _M0TPC16string10StringView _M0L3pkgS1081;
  moonbit_string_t _M0L7_2adataS1083;
  int32_t _M0L8_2astartS1084;
  int32_t _M0L6_2atmpS2783;
  int32_t _M0L6_2aendS1085;
  int32_t _M0Lm9_2acursorS1086;
  int32_t _M0Lm13accept__stateS1087;
  int32_t _M0Lm10match__endS1088;
  int32_t _M0Lm20match__tag__saver__0S1089;
  int32_t _M0Lm6tag__0S1090;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1091;
  struct _M0TPC16string10StringView _M0L8_2afieldS3208;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1100;
  void* _M0L8_2afieldS3207;
  int32_t _M0L6_2acntS3493;
  void* _M0L16_2apackage__nameS1101;
  struct _M0TPC16string10StringView _M0L8_2afieldS3205;
  struct _M0TPC16string10StringView _M0L8filenameS2760;
  struct _M0TPC16string10StringView _M0L8_2afieldS3204;
  struct _M0TPC16string10StringView _M0L11start__lineS2761;
  struct _M0TPC16string10StringView _M0L8_2afieldS3203;
  struct _M0TPC16string10StringView _M0L13start__columnS2762;
  struct _M0TPC16string10StringView _M0L8_2afieldS3202;
  struct _M0TPC16string10StringView _M0L9end__lineS2763;
  struct _M0TPC16string10StringView _M0L8_2afieldS3201;
  int32_t _M0L6_2acntS3497;
  struct _M0TPC16string10StringView _M0L11end__columnS2764;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3209
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1082->$0_1, _M0L4selfS1082->$0_2, _M0L4selfS1082->$0_0
  };
  _M0L3pkgS1081 = _M0L8_2afieldS3209;
  moonbit_incref(_M0L3pkgS1081.$0);
  moonbit_incref(_M0L3pkgS1081.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1083 = _M0MPC16string10StringView4data(_M0L3pkgS1081);
  moonbit_incref(_M0L3pkgS1081.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1084
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1081);
  moonbit_incref(_M0L3pkgS1081.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2783 = _M0MPC16string10StringView6length(_M0L3pkgS1081);
  _M0L6_2aendS1085 = _M0L8_2astartS1084 + _M0L6_2atmpS2783;
  _M0Lm9_2acursorS1086 = _M0L8_2astartS1084;
  _M0Lm13accept__stateS1087 = -1;
  _M0Lm10match__endS1088 = -1;
  _M0Lm20match__tag__saver__0S1089 = -1;
  _M0Lm6tag__0S1090 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2775 = _M0Lm9_2acursorS1086;
    if (_M0L6_2atmpS2775 < _M0L6_2aendS1085) {
      int32_t _M0L6_2atmpS2782 = _M0Lm9_2acursorS1086;
      int32_t _M0L10next__charS1095;
      int32_t _M0L6_2atmpS2776;
      moonbit_incref(_M0L7_2adataS1083);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1095
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1083, _M0L6_2atmpS2782);
      _M0L6_2atmpS2776 = _M0Lm9_2acursorS1086;
      _M0Lm9_2acursorS1086 = _M0L6_2atmpS2776 + 1;
      if (_M0L10next__charS1095 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2777;
          _M0Lm6tag__0S1090 = _M0Lm9_2acursorS1086;
          _M0L6_2atmpS2777 = _M0Lm9_2acursorS1086;
          if (_M0L6_2atmpS2777 < _M0L6_2aendS1085) {
            int32_t _M0L6_2atmpS2781 = _M0Lm9_2acursorS1086;
            int32_t _M0L10next__charS1096;
            int32_t _M0L6_2atmpS2778;
            moonbit_incref(_M0L7_2adataS1083);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1096
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1083, _M0L6_2atmpS2781);
            _M0L6_2atmpS2778 = _M0Lm9_2acursorS1086;
            _M0Lm9_2acursorS1086 = _M0L6_2atmpS2778 + 1;
            if (_M0L10next__charS1096 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2779 = _M0Lm9_2acursorS1086;
                if (_M0L6_2atmpS2779 < _M0L6_2aendS1085) {
                  int32_t _M0L6_2atmpS2780 = _M0Lm9_2acursorS1086;
                  _M0Lm9_2acursorS1086 = _M0L6_2atmpS2780 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1089 = _M0Lm6tag__0S1090;
                  _M0Lm13accept__stateS1087 = 0;
                  _M0Lm10match__endS1088 = _M0Lm9_2acursorS1086;
                  goto join_1092;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1092;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1092;
    }
    break;
  }
  goto joinlet_3631;
  join_1092:;
  switch (_M0Lm13accept__stateS1087) {
    case 0: {
      int32_t _M0L6_2atmpS2773;
      int32_t _M0L6_2atmpS2772;
      int64_t _M0L6_2atmpS2769;
      int32_t _M0L6_2atmpS2771;
      int64_t _M0L6_2atmpS2770;
      struct _M0TPC16string10StringView _M0L13package__nameS1093;
      int64_t _M0L6_2atmpS2766;
      int32_t _M0L6_2atmpS2768;
      int64_t _M0L6_2atmpS2767;
      struct _M0TPC16string10StringView _M0L12module__nameS1094;
      void* _M0L4SomeS2765;
      moonbit_decref(_M0L3pkgS1081.$0);
      _M0L6_2atmpS2773 = _M0Lm20match__tag__saver__0S1089;
      _M0L6_2atmpS2772 = _M0L6_2atmpS2773 + 1;
      _M0L6_2atmpS2769 = (int64_t)_M0L6_2atmpS2772;
      _M0L6_2atmpS2771 = _M0Lm10match__endS1088;
      _M0L6_2atmpS2770 = (int64_t)_M0L6_2atmpS2771;
      moonbit_incref(_M0L7_2adataS1083);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1093
      = _M0MPC16string6String4view(_M0L7_2adataS1083, _M0L6_2atmpS2769, _M0L6_2atmpS2770);
      _M0L6_2atmpS2766 = (int64_t)_M0L8_2astartS1084;
      _M0L6_2atmpS2768 = _M0Lm20match__tag__saver__0S1089;
      _M0L6_2atmpS2767 = (int64_t)_M0L6_2atmpS2768;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1094
      = _M0MPC16string6String4view(_M0L7_2adataS1083, _M0L6_2atmpS2766, _M0L6_2atmpS2767);
      _M0L4SomeS2765
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2765)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2765)->$0_0
      = _M0L13package__nameS1093.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2765)->$0_1
      = _M0L13package__nameS1093.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2765)->$0_2
      = _M0L13package__nameS1093.$2;
      _M0L7_2abindS1091
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1091)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1091->$0_0 = _M0L12module__nameS1094.$0;
      _M0L7_2abindS1091->$0_1 = _M0L12module__nameS1094.$1;
      _M0L7_2abindS1091->$0_2 = _M0L12module__nameS1094.$2;
      _M0L7_2abindS1091->$1 = _M0L4SomeS2765;
      break;
    }
    default: {
      void* _M0L4NoneS2774;
      moonbit_decref(_M0L7_2adataS1083);
      _M0L4NoneS2774
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1091
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1091)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1091->$0_0 = _M0L3pkgS1081.$0;
      _M0L7_2abindS1091->$0_1 = _M0L3pkgS1081.$1;
      _M0L7_2abindS1091->$0_2 = _M0L3pkgS1081.$2;
      _M0L7_2abindS1091->$1 = _M0L4NoneS2774;
      break;
    }
  }
  joinlet_3631:;
  _M0L8_2afieldS3208
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1091->$0_1, _M0L7_2abindS1091->$0_2, _M0L7_2abindS1091->$0_0
  };
  _M0L15_2amodule__nameS1100 = _M0L8_2afieldS3208;
  _M0L8_2afieldS3207 = _M0L7_2abindS1091->$1;
  _M0L6_2acntS3493 = Moonbit_object_header(_M0L7_2abindS1091)->rc;
  if (_M0L6_2acntS3493 > 1) {
    int32_t _M0L11_2anew__cntS3494 = _M0L6_2acntS3493 - 1;
    Moonbit_object_header(_M0L7_2abindS1091)->rc = _M0L11_2anew__cntS3494;
    moonbit_incref(_M0L8_2afieldS3207);
    moonbit_incref(_M0L15_2amodule__nameS1100.$0);
  } else if (_M0L6_2acntS3493 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1091);
  }
  _M0L16_2apackage__nameS1101 = _M0L8_2afieldS3207;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1101)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1102 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1101;
      struct _M0TPC16string10StringView _M0L8_2afieldS3206 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1102->$0_1,
                                              _M0L7_2aSomeS1102->$0_2,
                                              _M0L7_2aSomeS1102->$0_0};
      int32_t _M0L6_2acntS3495 = Moonbit_object_header(_M0L7_2aSomeS1102)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1103;
      if (_M0L6_2acntS3495 > 1) {
        int32_t _M0L11_2anew__cntS3496 = _M0L6_2acntS3495 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1102)->rc = _M0L11_2anew__cntS3496;
        moonbit_incref(_M0L8_2afieldS3206.$0);
      } else if (_M0L6_2acntS3495 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1102);
      }
      _M0L12_2apkg__nameS1103 = _M0L8_2afieldS3206;
      if (_M0L6loggerS1104.$1) {
        moonbit_incref(_M0L6loggerS1104.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1104.$0->$method_2(_M0L6loggerS1104.$1, _M0L12_2apkg__nameS1103);
      if (_M0L6loggerS1104.$1) {
        moonbit_incref(_M0L6loggerS1104.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1104.$0->$method_3(_M0L6loggerS1104.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1101);
      break;
    }
  }
  _M0L8_2afieldS3205
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1082->$1_1, _M0L4selfS1082->$1_2, _M0L4selfS1082->$1_0
  };
  _M0L8filenameS2760 = _M0L8_2afieldS3205;
  moonbit_incref(_M0L8filenameS2760.$0);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_2(_M0L6loggerS1104.$1, _M0L8filenameS2760);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_3(_M0L6loggerS1104.$1, 58);
  _M0L8_2afieldS3204
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1082->$2_1, _M0L4selfS1082->$2_2, _M0L4selfS1082->$2_0
  };
  _M0L11start__lineS2761 = _M0L8_2afieldS3204;
  moonbit_incref(_M0L11start__lineS2761.$0);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_2(_M0L6loggerS1104.$1, _M0L11start__lineS2761);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_3(_M0L6loggerS1104.$1, 58);
  _M0L8_2afieldS3203
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1082->$3_1, _M0L4selfS1082->$3_2, _M0L4selfS1082->$3_0
  };
  _M0L13start__columnS2762 = _M0L8_2afieldS3203;
  moonbit_incref(_M0L13start__columnS2762.$0);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_2(_M0L6loggerS1104.$1, _M0L13start__columnS2762);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_3(_M0L6loggerS1104.$1, 45);
  _M0L8_2afieldS3202
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1082->$4_1, _M0L4selfS1082->$4_2, _M0L4selfS1082->$4_0
  };
  _M0L9end__lineS2763 = _M0L8_2afieldS3202;
  moonbit_incref(_M0L9end__lineS2763.$0);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_2(_M0L6loggerS1104.$1, _M0L9end__lineS2763);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_3(_M0L6loggerS1104.$1, 58);
  _M0L8_2afieldS3201
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1082->$5_1, _M0L4selfS1082->$5_2, _M0L4selfS1082->$5_0
  };
  _M0L6_2acntS3497 = Moonbit_object_header(_M0L4selfS1082)->rc;
  if (_M0L6_2acntS3497 > 1) {
    int32_t _M0L11_2anew__cntS3503 = _M0L6_2acntS3497 - 1;
    Moonbit_object_header(_M0L4selfS1082)->rc = _M0L11_2anew__cntS3503;
    moonbit_incref(_M0L8_2afieldS3201.$0);
  } else if (_M0L6_2acntS3497 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3502 =
      (struct _M0TPC16string10StringView){_M0L4selfS1082->$4_1,
                                            _M0L4selfS1082->$4_2,
                                            _M0L4selfS1082->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3501;
    struct _M0TPC16string10StringView _M0L8_2afieldS3500;
    struct _M0TPC16string10StringView _M0L8_2afieldS3499;
    struct _M0TPC16string10StringView _M0L8_2afieldS3498;
    moonbit_decref(_M0L8_2afieldS3502.$0);
    _M0L8_2afieldS3501
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1082->$3_1, _M0L4selfS1082->$3_2, _M0L4selfS1082->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3501.$0);
    _M0L8_2afieldS3500
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1082->$2_1, _M0L4selfS1082->$2_2, _M0L4selfS1082->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3500.$0);
    _M0L8_2afieldS3499
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1082->$1_1, _M0L4selfS1082->$1_2, _M0L4selfS1082->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3499.$0);
    _M0L8_2afieldS3498
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1082->$0_1, _M0L4selfS1082->$0_2, _M0L4selfS1082->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3498.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1082);
  }
  _M0L11end__columnS2764 = _M0L8_2afieldS3201;
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_2(_M0L6loggerS1104.$1, _M0L11end__columnS2764);
  if (_M0L6loggerS1104.$1) {
    moonbit_incref(_M0L6loggerS1104.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_3(_M0L6loggerS1104.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1104.$0->$method_2(_M0L6loggerS1104.$1, _M0L15_2amodule__nameS1100);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1080) {
  moonbit_string_t _M0L6_2atmpS2759;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2759
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1080);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2759);
  moonbit_decref(_M0L6_2atmpS2759);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1079,
  struct _M0TPB6Logger _M0L6loggerS1078
) {
  moonbit_string_t _M0L6_2atmpS2758;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2758 = _M0MPC16double6Double10to__string(_M0L4selfS1079);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1078.$0->$method_0(_M0L6loggerS1078.$1, _M0L6_2atmpS2758);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1077) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1077);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1064) {
  uint64_t _M0L4bitsS1065;
  uint64_t _M0L6_2atmpS2757;
  uint64_t _M0L6_2atmpS2756;
  int32_t _M0L8ieeeSignS1066;
  uint64_t _M0L12ieeeMantissaS1067;
  uint64_t _M0L6_2atmpS2755;
  uint64_t _M0L6_2atmpS2754;
  int32_t _M0L12ieeeExponentS1068;
  int32_t _if__result_3635;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1069;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1070;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2753;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1064 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_58.data;
  }
  _M0L4bitsS1065 = *(int64_t*)&_M0L3valS1064;
  _M0L6_2atmpS2757 = _M0L4bitsS1065 >> 63;
  _M0L6_2atmpS2756 = _M0L6_2atmpS2757 & 1ull;
  _M0L8ieeeSignS1066 = _M0L6_2atmpS2756 != 0ull;
  _M0L12ieeeMantissaS1067 = _M0L4bitsS1065 & 4503599627370495ull;
  _M0L6_2atmpS2755 = _M0L4bitsS1065 >> 52;
  _M0L6_2atmpS2754 = _M0L6_2atmpS2755 & 2047ull;
  _M0L12ieeeExponentS1068 = (int32_t)_M0L6_2atmpS2754;
  if (_M0L12ieeeExponentS1068 == 2047) {
    _if__result_3635 = 1;
  } else if (_M0L12ieeeExponentS1068 == 0) {
    _if__result_3635 = _M0L12ieeeMantissaS1067 == 0ull;
  } else {
    _if__result_3635 = 0;
  }
  if (_if__result_3635) {
    int32_t _M0L6_2atmpS2742 = _M0L12ieeeExponentS1068 != 0;
    int32_t _M0L6_2atmpS2743 = _M0L12ieeeMantissaS1067 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1066, _M0L6_2atmpS2742, _M0L6_2atmpS2743);
  }
  _M0Lm1vS1069 = _M0FPB31ryu__to__string_2erecord_2f1063;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1070
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1067, _M0L12ieeeExponentS1068);
  if (_M0L5smallS1070 == 0) {
    uint32_t _M0L6_2atmpS2744;
    if (_M0L5smallS1070) {
      moonbit_decref(_M0L5smallS1070);
    }
    _M0L6_2atmpS2744 = *(uint32_t*)&_M0L12ieeeExponentS1068;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1069 = _M0FPB3d2d(_M0L12ieeeMantissaS1067, _M0L6_2atmpS2744);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1071 = _M0L5smallS1070;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1072 = _M0L7_2aSomeS1071;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1073 = _M0L4_2afS1072;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2752 = _M0Lm1xS1073;
      uint64_t _M0L8_2afieldS3212 = _M0L6_2atmpS2752->$0;
      uint64_t _M0L8mantissaS2751 = _M0L8_2afieldS3212;
      uint64_t _M0L1qS1074 = _M0L8mantissaS2751 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2750 = _M0Lm1xS1073;
      uint64_t _M0L8_2afieldS3211 = _M0L6_2atmpS2750->$0;
      uint64_t _M0L8mantissaS2748 = _M0L8_2afieldS3211;
      uint64_t _M0L6_2atmpS2749 = 10ull * _M0L1qS1074;
      uint64_t _M0L1rS1075 = _M0L8mantissaS2748 - _M0L6_2atmpS2749;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2747;
      int32_t _M0L8_2afieldS3210;
      int32_t _M0L8exponentS2746;
      int32_t _M0L6_2atmpS2745;
      if (_M0L1rS1075 != 0ull) {
        break;
      }
      _M0L6_2atmpS2747 = _M0Lm1xS1073;
      _M0L8_2afieldS3210 = _M0L6_2atmpS2747->$1;
      moonbit_decref(_M0L6_2atmpS2747);
      _M0L8exponentS2746 = _M0L8_2afieldS3210;
      _M0L6_2atmpS2745 = _M0L8exponentS2746 + 1;
      _M0Lm1xS1073
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1073)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1073->$0 = _M0L1qS1074;
      _M0Lm1xS1073->$1 = _M0L6_2atmpS2745;
      continue;
      break;
    }
    _M0Lm1vS1069 = _M0Lm1xS1073;
  }
  _M0L6_2atmpS2753 = _M0Lm1vS1069;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2753, _M0L8ieeeSignS1066);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1058,
  int32_t _M0L12ieeeExponentS1060
) {
  uint64_t _M0L2m2S1057;
  int32_t _M0L6_2atmpS2741;
  int32_t _M0L2e2S1059;
  int32_t _M0L6_2atmpS2740;
  uint64_t _M0L6_2atmpS2739;
  uint64_t _M0L4maskS1061;
  uint64_t _M0L8fractionS1062;
  int32_t _M0L6_2atmpS2738;
  uint64_t _M0L6_2atmpS2737;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2736;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1057 = 4503599627370496ull | _M0L12ieeeMantissaS1058;
  _M0L6_2atmpS2741 = _M0L12ieeeExponentS1060 - 1023;
  _M0L2e2S1059 = _M0L6_2atmpS2741 - 52;
  if (_M0L2e2S1059 > 0) {
    return 0;
  }
  if (_M0L2e2S1059 < -52) {
    return 0;
  }
  _M0L6_2atmpS2740 = -_M0L2e2S1059;
  _M0L6_2atmpS2739 = 1ull << (_M0L6_2atmpS2740 & 63);
  _M0L4maskS1061 = _M0L6_2atmpS2739 - 1ull;
  _M0L8fractionS1062 = _M0L2m2S1057 & _M0L4maskS1061;
  if (_M0L8fractionS1062 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2738 = -_M0L2e2S1059;
  _M0L6_2atmpS2737 = _M0L2m2S1057 >> (_M0L6_2atmpS2738 & 63);
  _M0L6_2atmpS2736
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2736)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2736->$0 = _M0L6_2atmpS2737;
  _M0L6_2atmpS2736->$1 = 0;
  return _M0L6_2atmpS2736;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1031,
  int32_t _M0L4signS1029
) {
  int32_t _M0L6_2atmpS2735;
  moonbit_bytes_t _M0L6resultS1027;
  int32_t _M0Lm5indexS1028;
  uint64_t _M0Lm6outputS1030;
  uint64_t _M0L6_2atmpS2734;
  int32_t _M0L7olengthS1032;
  int32_t _M0L8_2afieldS3213;
  int32_t _M0L8exponentS2733;
  int32_t _M0L6_2atmpS2732;
  int32_t _M0Lm3expS1033;
  int32_t _M0L6_2atmpS2731;
  int32_t _M0L6_2atmpS2729;
  int32_t _M0L18scientificNotationS1034;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2735 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1027
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2735);
  _M0Lm5indexS1028 = 0;
  if (_M0L4signS1029) {
    int32_t _M0L6_2atmpS2604 = _M0Lm5indexS1028;
    int32_t _M0L6_2atmpS2605;
    if (
      _M0L6_2atmpS2604 < 0
      || _M0L6_2atmpS2604 >= Moonbit_array_length(_M0L6resultS1027)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1027[_M0L6_2atmpS2604] = 45;
    _M0L6_2atmpS2605 = _M0Lm5indexS1028;
    _M0Lm5indexS1028 = _M0L6_2atmpS2605 + 1;
  }
  _M0Lm6outputS1030 = _M0L1vS1031->$0;
  _M0L6_2atmpS2734 = _M0Lm6outputS1030;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1032 = _M0FPB17decimal__length17(_M0L6_2atmpS2734);
  _M0L8_2afieldS3213 = _M0L1vS1031->$1;
  moonbit_decref(_M0L1vS1031);
  _M0L8exponentS2733 = _M0L8_2afieldS3213;
  _M0L6_2atmpS2732 = _M0L8exponentS2733 + _M0L7olengthS1032;
  _M0Lm3expS1033 = _M0L6_2atmpS2732 - 1;
  _M0L6_2atmpS2731 = _M0Lm3expS1033;
  if (_M0L6_2atmpS2731 >= -6) {
    int32_t _M0L6_2atmpS2730 = _M0Lm3expS1033;
    _M0L6_2atmpS2729 = _M0L6_2atmpS2730 < 21;
  } else {
    _M0L6_2atmpS2729 = 0;
  }
  _M0L18scientificNotationS1034 = !_M0L6_2atmpS2729;
  if (_M0L18scientificNotationS1034) {
    int32_t _M0L7_2abindS1035 = _M0L7olengthS1032 - 1;
    int32_t _M0L1iS1036 = 0;
    int32_t _M0L6_2atmpS2615;
    uint64_t _M0L6_2atmpS2620;
    int32_t _M0L6_2atmpS2619;
    int32_t _M0L6_2atmpS2618;
    int32_t _M0L6_2atmpS2617;
    int32_t _M0L6_2atmpS2616;
    int32_t _M0L6_2atmpS2624;
    int32_t _M0L6_2atmpS2625;
    int32_t _M0L6_2atmpS2626;
    int32_t _M0L6_2atmpS2627;
    int32_t _M0L6_2atmpS2628;
    int32_t _M0L6_2atmpS2634;
    int32_t _M0L6_2atmpS2667;
    while (1) {
      if (_M0L1iS1036 < _M0L7_2abindS1035) {
        uint64_t _M0L6_2atmpS2613 = _M0Lm6outputS1030;
        uint64_t _M0L1cS1037 = _M0L6_2atmpS2613 % 10ull;
        uint64_t _M0L6_2atmpS2606 = _M0Lm6outputS1030;
        int32_t _M0L6_2atmpS2612;
        int32_t _M0L6_2atmpS2611;
        int32_t _M0L6_2atmpS2607;
        int32_t _M0L6_2atmpS2610;
        int32_t _M0L6_2atmpS2609;
        int32_t _M0L6_2atmpS2608;
        int32_t _M0L6_2atmpS2614;
        _M0Lm6outputS1030 = _M0L6_2atmpS2606 / 10ull;
        _M0L6_2atmpS2612 = _M0Lm5indexS1028;
        _M0L6_2atmpS2611 = _M0L6_2atmpS2612 + _M0L7olengthS1032;
        _M0L6_2atmpS2607 = _M0L6_2atmpS2611 - _M0L1iS1036;
        _M0L6_2atmpS2610 = (int32_t)_M0L1cS1037;
        _M0L6_2atmpS2609 = 48 + _M0L6_2atmpS2610;
        _M0L6_2atmpS2608 = _M0L6_2atmpS2609 & 0xff;
        if (
          _M0L6_2atmpS2607 < 0
          || _M0L6_2atmpS2607 >= Moonbit_array_length(_M0L6resultS1027)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1027[_M0L6_2atmpS2607] = _M0L6_2atmpS2608;
        _M0L6_2atmpS2614 = _M0L1iS1036 + 1;
        _M0L1iS1036 = _M0L6_2atmpS2614;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2615 = _M0Lm5indexS1028;
    _M0L6_2atmpS2620 = _M0Lm6outputS1030;
    _M0L6_2atmpS2619 = (int32_t)_M0L6_2atmpS2620;
    _M0L6_2atmpS2618 = _M0L6_2atmpS2619 % 10;
    _M0L6_2atmpS2617 = 48 + _M0L6_2atmpS2618;
    _M0L6_2atmpS2616 = _M0L6_2atmpS2617 & 0xff;
    if (
      _M0L6_2atmpS2615 < 0
      || _M0L6_2atmpS2615 >= Moonbit_array_length(_M0L6resultS1027)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1027[_M0L6_2atmpS2615] = _M0L6_2atmpS2616;
    if (_M0L7olengthS1032 > 1) {
      int32_t _M0L6_2atmpS2622 = _M0Lm5indexS1028;
      int32_t _M0L6_2atmpS2621 = _M0L6_2atmpS2622 + 1;
      if (
        _M0L6_2atmpS2621 < 0
        || _M0L6_2atmpS2621 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2621] = 46;
    } else {
      int32_t _M0L6_2atmpS2623 = _M0Lm5indexS1028;
      _M0Lm5indexS1028 = _M0L6_2atmpS2623 - 1;
    }
    _M0L6_2atmpS2624 = _M0Lm5indexS1028;
    _M0L6_2atmpS2625 = _M0L7olengthS1032 + 1;
    _M0Lm5indexS1028 = _M0L6_2atmpS2624 + _M0L6_2atmpS2625;
    _M0L6_2atmpS2626 = _M0Lm5indexS1028;
    if (
      _M0L6_2atmpS2626 < 0
      || _M0L6_2atmpS2626 >= Moonbit_array_length(_M0L6resultS1027)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1027[_M0L6_2atmpS2626] = 101;
    _M0L6_2atmpS2627 = _M0Lm5indexS1028;
    _M0Lm5indexS1028 = _M0L6_2atmpS2627 + 1;
    _M0L6_2atmpS2628 = _M0Lm3expS1033;
    if (_M0L6_2atmpS2628 < 0) {
      int32_t _M0L6_2atmpS2629 = _M0Lm5indexS1028;
      int32_t _M0L6_2atmpS2630;
      int32_t _M0L6_2atmpS2631;
      if (
        _M0L6_2atmpS2629 < 0
        || _M0L6_2atmpS2629 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2629] = 45;
      _M0L6_2atmpS2630 = _M0Lm5indexS1028;
      _M0Lm5indexS1028 = _M0L6_2atmpS2630 + 1;
      _M0L6_2atmpS2631 = _M0Lm3expS1033;
      _M0Lm3expS1033 = -_M0L6_2atmpS2631;
    } else {
      int32_t _M0L6_2atmpS2632 = _M0Lm5indexS1028;
      int32_t _M0L6_2atmpS2633;
      if (
        _M0L6_2atmpS2632 < 0
        || _M0L6_2atmpS2632 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2632] = 43;
      _M0L6_2atmpS2633 = _M0Lm5indexS1028;
      _M0Lm5indexS1028 = _M0L6_2atmpS2633 + 1;
    }
    _M0L6_2atmpS2634 = _M0Lm3expS1033;
    if (_M0L6_2atmpS2634 >= 100) {
      int32_t _M0L6_2atmpS2650 = _M0Lm3expS1033;
      int32_t _M0L1aS1039 = _M0L6_2atmpS2650 / 100;
      int32_t _M0L6_2atmpS2649 = _M0Lm3expS1033;
      int32_t _M0L6_2atmpS2648 = _M0L6_2atmpS2649 / 10;
      int32_t _M0L1bS1040 = _M0L6_2atmpS2648 % 10;
      int32_t _M0L6_2atmpS2647 = _M0Lm3expS1033;
      int32_t _M0L1cS1041 = _M0L6_2atmpS2647 % 10;
      int32_t _M0L6_2atmpS2635 = _M0Lm5indexS1028;
      int32_t _M0L6_2atmpS2637 = 48 + _M0L1aS1039;
      int32_t _M0L6_2atmpS2636 = _M0L6_2atmpS2637 & 0xff;
      int32_t _M0L6_2atmpS2641;
      int32_t _M0L6_2atmpS2638;
      int32_t _M0L6_2atmpS2640;
      int32_t _M0L6_2atmpS2639;
      int32_t _M0L6_2atmpS2645;
      int32_t _M0L6_2atmpS2642;
      int32_t _M0L6_2atmpS2644;
      int32_t _M0L6_2atmpS2643;
      int32_t _M0L6_2atmpS2646;
      if (
        _M0L6_2atmpS2635 < 0
        || _M0L6_2atmpS2635 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2635] = _M0L6_2atmpS2636;
      _M0L6_2atmpS2641 = _M0Lm5indexS1028;
      _M0L6_2atmpS2638 = _M0L6_2atmpS2641 + 1;
      _M0L6_2atmpS2640 = 48 + _M0L1bS1040;
      _M0L6_2atmpS2639 = _M0L6_2atmpS2640 & 0xff;
      if (
        _M0L6_2atmpS2638 < 0
        || _M0L6_2atmpS2638 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2638] = _M0L6_2atmpS2639;
      _M0L6_2atmpS2645 = _M0Lm5indexS1028;
      _M0L6_2atmpS2642 = _M0L6_2atmpS2645 + 2;
      _M0L6_2atmpS2644 = 48 + _M0L1cS1041;
      _M0L6_2atmpS2643 = _M0L6_2atmpS2644 & 0xff;
      if (
        _M0L6_2atmpS2642 < 0
        || _M0L6_2atmpS2642 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2642] = _M0L6_2atmpS2643;
      _M0L6_2atmpS2646 = _M0Lm5indexS1028;
      _M0Lm5indexS1028 = _M0L6_2atmpS2646 + 3;
    } else {
      int32_t _M0L6_2atmpS2651 = _M0Lm3expS1033;
      if (_M0L6_2atmpS2651 >= 10) {
        int32_t _M0L6_2atmpS2661 = _M0Lm3expS1033;
        int32_t _M0L1aS1042 = _M0L6_2atmpS2661 / 10;
        int32_t _M0L6_2atmpS2660 = _M0Lm3expS1033;
        int32_t _M0L1bS1043 = _M0L6_2atmpS2660 % 10;
        int32_t _M0L6_2atmpS2652 = _M0Lm5indexS1028;
        int32_t _M0L6_2atmpS2654 = 48 + _M0L1aS1042;
        int32_t _M0L6_2atmpS2653 = _M0L6_2atmpS2654 & 0xff;
        int32_t _M0L6_2atmpS2658;
        int32_t _M0L6_2atmpS2655;
        int32_t _M0L6_2atmpS2657;
        int32_t _M0L6_2atmpS2656;
        int32_t _M0L6_2atmpS2659;
        if (
          _M0L6_2atmpS2652 < 0
          || _M0L6_2atmpS2652 >= Moonbit_array_length(_M0L6resultS1027)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1027[_M0L6_2atmpS2652] = _M0L6_2atmpS2653;
        _M0L6_2atmpS2658 = _M0Lm5indexS1028;
        _M0L6_2atmpS2655 = _M0L6_2atmpS2658 + 1;
        _M0L6_2atmpS2657 = 48 + _M0L1bS1043;
        _M0L6_2atmpS2656 = _M0L6_2atmpS2657 & 0xff;
        if (
          _M0L6_2atmpS2655 < 0
          || _M0L6_2atmpS2655 >= Moonbit_array_length(_M0L6resultS1027)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1027[_M0L6_2atmpS2655] = _M0L6_2atmpS2656;
        _M0L6_2atmpS2659 = _M0Lm5indexS1028;
        _M0Lm5indexS1028 = _M0L6_2atmpS2659 + 2;
      } else {
        int32_t _M0L6_2atmpS2662 = _M0Lm5indexS1028;
        int32_t _M0L6_2atmpS2665 = _M0Lm3expS1033;
        int32_t _M0L6_2atmpS2664 = 48 + _M0L6_2atmpS2665;
        int32_t _M0L6_2atmpS2663 = _M0L6_2atmpS2664 & 0xff;
        int32_t _M0L6_2atmpS2666;
        if (
          _M0L6_2atmpS2662 < 0
          || _M0L6_2atmpS2662 >= Moonbit_array_length(_M0L6resultS1027)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1027[_M0L6_2atmpS2662] = _M0L6_2atmpS2663;
        _M0L6_2atmpS2666 = _M0Lm5indexS1028;
        _M0Lm5indexS1028 = _M0L6_2atmpS2666 + 1;
      }
    }
    _M0L6_2atmpS2667 = _M0Lm5indexS1028;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1027, 0, _M0L6_2atmpS2667);
  } else {
    int32_t _M0L6_2atmpS2668 = _M0Lm3expS1033;
    int32_t _M0L6_2atmpS2728;
    if (_M0L6_2atmpS2668 < 0) {
      int32_t _M0L6_2atmpS2669 = _M0Lm5indexS1028;
      int32_t _M0L6_2atmpS2670;
      int32_t _M0L6_2atmpS2671;
      int32_t _M0L6_2atmpS2672;
      int32_t _M0L1iS1044;
      int32_t _M0L7currentS1046;
      int32_t _M0L1iS1047;
      if (
        _M0L6_2atmpS2669 < 0
        || _M0L6_2atmpS2669 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2669] = 48;
      _M0L6_2atmpS2670 = _M0Lm5indexS1028;
      _M0Lm5indexS1028 = _M0L6_2atmpS2670 + 1;
      _M0L6_2atmpS2671 = _M0Lm5indexS1028;
      if (
        _M0L6_2atmpS2671 < 0
        || _M0L6_2atmpS2671 >= Moonbit_array_length(_M0L6resultS1027)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1027[_M0L6_2atmpS2671] = 46;
      _M0L6_2atmpS2672 = _M0Lm5indexS1028;
      _M0Lm5indexS1028 = _M0L6_2atmpS2672 + 1;
      _M0L1iS1044 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2673 = _M0Lm3expS1033;
        if (_M0L1iS1044 > _M0L6_2atmpS2673) {
          int32_t _M0L6_2atmpS2674 = _M0Lm5indexS1028;
          int32_t _M0L6_2atmpS2675;
          int32_t _M0L6_2atmpS2676;
          if (
            _M0L6_2atmpS2674 < 0
            || _M0L6_2atmpS2674 >= Moonbit_array_length(_M0L6resultS1027)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1027[_M0L6_2atmpS2674] = 48;
          _M0L6_2atmpS2675 = _M0Lm5indexS1028;
          _M0Lm5indexS1028 = _M0L6_2atmpS2675 + 1;
          _M0L6_2atmpS2676 = _M0L1iS1044 - 1;
          _M0L1iS1044 = _M0L6_2atmpS2676;
          continue;
        }
        break;
      }
      _M0L7currentS1046 = _M0Lm5indexS1028;
      _M0L1iS1047 = 0;
      while (1) {
        if (_M0L1iS1047 < _M0L7olengthS1032) {
          int32_t _M0L6_2atmpS2684 = _M0L7currentS1046 + _M0L7olengthS1032;
          int32_t _M0L6_2atmpS2683 = _M0L6_2atmpS2684 - _M0L1iS1047;
          int32_t _M0L6_2atmpS2677 = _M0L6_2atmpS2683 - 1;
          uint64_t _M0L6_2atmpS2682 = _M0Lm6outputS1030;
          uint64_t _M0L6_2atmpS2681 = _M0L6_2atmpS2682 % 10ull;
          int32_t _M0L6_2atmpS2680 = (int32_t)_M0L6_2atmpS2681;
          int32_t _M0L6_2atmpS2679 = 48 + _M0L6_2atmpS2680;
          int32_t _M0L6_2atmpS2678 = _M0L6_2atmpS2679 & 0xff;
          uint64_t _M0L6_2atmpS2685;
          int32_t _M0L6_2atmpS2686;
          int32_t _M0L6_2atmpS2687;
          if (
            _M0L6_2atmpS2677 < 0
            || _M0L6_2atmpS2677 >= Moonbit_array_length(_M0L6resultS1027)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1027[_M0L6_2atmpS2677] = _M0L6_2atmpS2678;
          _M0L6_2atmpS2685 = _M0Lm6outputS1030;
          _M0Lm6outputS1030 = _M0L6_2atmpS2685 / 10ull;
          _M0L6_2atmpS2686 = _M0Lm5indexS1028;
          _M0Lm5indexS1028 = _M0L6_2atmpS2686 + 1;
          _M0L6_2atmpS2687 = _M0L1iS1047 + 1;
          _M0L1iS1047 = _M0L6_2atmpS2687;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2689 = _M0Lm3expS1033;
      int32_t _M0L6_2atmpS2688 = _M0L6_2atmpS2689 + 1;
      if (_M0L6_2atmpS2688 >= _M0L7olengthS1032) {
        int32_t _M0L1iS1049 = 0;
        int32_t _M0L6_2atmpS2701;
        int32_t _M0L6_2atmpS2705;
        int32_t _M0L7_2abindS1051;
        int32_t _M0L2__S1052;
        while (1) {
          if (_M0L1iS1049 < _M0L7olengthS1032) {
            int32_t _M0L6_2atmpS2698 = _M0Lm5indexS1028;
            int32_t _M0L6_2atmpS2697 = _M0L6_2atmpS2698 + _M0L7olengthS1032;
            int32_t _M0L6_2atmpS2696 = _M0L6_2atmpS2697 - _M0L1iS1049;
            int32_t _M0L6_2atmpS2690 = _M0L6_2atmpS2696 - 1;
            uint64_t _M0L6_2atmpS2695 = _M0Lm6outputS1030;
            uint64_t _M0L6_2atmpS2694 = _M0L6_2atmpS2695 % 10ull;
            int32_t _M0L6_2atmpS2693 = (int32_t)_M0L6_2atmpS2694;
            int32_t _M0L6_2atmpS2692 = 48 + _M0L6_2atmpS2693;
            int32_t _M0L6_2atmpS2691 = _M0L6_2atmpS2692 & 0xff;
            uint64_t _M0L6_2atmpS2699;
            int32_t _M0L6_2atmpS2700;
            if (
              _M0L6_2atmpS2690 < 0
              || _M0L6_2atmpS2690 >= Moonbit_array_length(_M0L6resultS1027)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1027[_M0L6_2atmpS2690] = _M0L6_2atmpS2691;
            _M0L6_2atmpS2699 = _M0Lm6outputS1030;
            _M0Lm6outputS1030 = _M0L6_2atmpS2699 / 10ull;
            _M0L6_2atmpS2700 = _M0L1iS1049 + 1;
            _M0L1iS1049 = _M0L6_2atmpS2700;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2701 = _M0Lm5indexS1028;
        _M0Lm5indexS1028 = _M0L6_2atmpS2701 + _M0L7olengthS1032;
        _M0L6_2atmpS2705 = _M0Lm3expS1033;
        _M0L7_2abindS1051 = _M0L6_2atmpS2705 + 1;
        _M0L2__S1052 = _M0L7olengthS1032;
        while (1) {
          if (_M0L2__S1052 < _M0L7_2abindS1051) {
            int32_t _M0L6_2atmpS2702 = _M0Lm5indexS1028;
            int32_t _M0L6_2atmpS2703;
            int32_t _M0L6_2atmpS2704;
            if (
              _M0L6_2atmpS2702 < 0
              || _M0L6_2atmpS2702 >= Moonbit_array_length(_M0L6resultS1027)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1027[_M0L6_2atmpS2702] = 48;
            _M0L6_2atmpS2703 = _M0Lm5indexS1028;
            _M0Lm5indexS1028 = _M0L6_2atmpS2703 + 1;
            _M0L6_2atmpS2704 = _M0L2__S1052 + 1;
            _M0L2__S1052 = _M0L6_2atmpS2704;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2727 = _M0Lm5indexS1028;
        int32_t _M0Lm7currentS1054 = _M0L6_2atmpS2727 + 1;
        int32_t _M0L1iS1055 = 0;
        int32_t _M0L6_2atmpS2725;
        int32_t _M0L6_2atmpS2726;
        while (1) {
          if (_M0L1iS1055 < _M0L7olengthS1032) {
            int32_t _M0L6_2atmpS2708 = _M0L7olengthS1032 - _M0L1iS1055;
            int32_t _M0L6_2atmpS2706 = _M0L6_2atmpS2708 - 1;
            int32_t _M0L6_2atmpS2707 = _M0Lm3expS1033;
            int32_t _M0L6_2atmpS2722;
            int32_t _M0L6_2atmpS2721;
            int32_t _M0L6_2atmpS2720;
            int32_t _M0L6_2atmpS2714;
            uint64_t _M0L6_2atmpS2719;
            uint64_t _M0L6_2atmpS2718;
            int32_t _M0L6_2atmpS2717;
            int32_t _M0L6_2atmpS2716;
            int32_t _M0L6_2atmpS2715;
            uint64_t _M0L6_2atmpS2723;
            int32_t _M0L6_2atmpS2724;
            if (_M0L6_2atmpS2706 == _M0L6_2atmpS2707) {
              int32_t _M0L6_2atmpS2712 = _M0Lm7currentS1054;
              int32_t _M0L6_2atmpS2711 = _M0L6_2atmpS2712 + _M0L7olengthS1032;
              int32_t _M0L6_2atmpS2710 = _M0L6_2atmpS2711 - _M0L1iS1055;
              int32_t _M0L6_2atmpS2709 = _M0L6_2atmpS2710 - 1;
              int32_t _M0L6_2atmpS2713;
              if (
                _M0L6_2atmpS2709 < 0
                || _M0L6_2atmpS2709 >= Moonbit_array_length(_M0L6resultS1027)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1027[_M0L6_2atmpS2709] = 46;
              _M0L6_2atmpS2713 = _M0Lm7currentS1054;
              _M0Lm7currentS1054 = _M0L6_2atmpS2713 - 1;
            }
            _M0L6_2atmpS2722 = _M0Lm7currentS1054;
            _M0L6_2atmpS2721 = _M0L6_2atmpS2722 + _M0L7olengthS1032;
            _M0L6_2atmpS2720 = _M0L6_2atmpS2721 - _M0L1iS1055;
            _M0L6_2atmpS2714 = _M0L6_2atmpS2720 - 1;
            _M0L6_2atmpS2719 = _M0Lm6outputS1030;
            _M0L6_2atmpS2718 = _M0L6_2atmpS2719 % 10ull;
            _M0L6_2atmpS2717 = (int32_t)_M0L6_2atmpS2718;
            _M0L6_2atmpS2716 = 48 + _M0L6_2atmpS2717;
            _M0L6_2atmpS2715 = _M0L6_2atmpS2716 & 0xff;
            if (
              _M0L6_2atmpS2714 < 0
              || _M0L6_2atmpS2714 >= Moonbit_array_length(_M0L6resultS1027)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1027[_M0L6_2atmpS2714] = _M0L6_2atmpS2715;
            _M0L6_2atmpS2723 = _M0Lm6outputS1030;
            _M0Lm6outputS1030 = _M0L6_2atmpS2723 / 10ull;
            _M0L6_2atmpS2724 = _M0L1iS1055 + 1;
            _M0L1iS1055 = _M0L6_2atmpS2724;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2725 = _M0Lm5indexS1028;
        _M0L6_2atmpS2726 = _M0L7olengthS1032 + 1;
        _M0Lm5indexS1028 = _M0L6_2atmpS2725 + _M0L6_2atmpS2726;
      }
    }
    _M0L6_2atmpS2728 = _M0Lm5indexS1028;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1027, 0, _M0L6_2atmpS2728);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS973,
  uint32_t _M0L12ieeeExponentS972
) {
  int32_t _M0Lm2e2S970;
  uint64_t _M0Lm2m2S971;
  uint64_t _M0L6_2atmpS2603;
  uint64_t _M0L6_2atmpS2602;
  int32_t _M0L4evenS974;
  uint64_t _M0L6_2atmpS2601;
  uint64_t _M0L2mvS975;
  int32_t _M0L7mmShiftS976;
  uint64_t _M0Lm2vrS977;
  uint64_t _M0Lm2vpS978;
  uint64_t _M0Lm2vmS979;
  int32_t _M0Lm3e10S980;
  int32_t _M0Lm17vmIsTrailingZerosS981;
  int32_t _M0Lm17vrIsTrailingZerosS982;
  int32_t _M0L6_2atmpS2503;
  int32_t _M0Lm7removedS1001;
  int32_t _M0Lm16lastRemovedDigitS1002;
  uint64_t _M0Lm6outputS1003;
  int32_t _M0L6_2atmpS2599;
  int32_t _M0L6_2atmpS2600;
  int32_t _M0L3expS1026;
  uint64_t _M0L6_2atmpS2598;
  struct _M0TPB17FloatingDecimal64* _block_3648;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S970 = 0;
  _M0Lm2m2S971 = 0ull;
  if (_M0L12ieeeExponentS972 == 0u) {
    _M0Lm2e2S970 = -1076;
    _M0Lm2m2S971 = _M0L12ieeeMantissaS973;
  } else {
    int32_t _M0L6_2atmpS2502 = *(int32_t*)&_M0L12ieeeExponentS972;
    int32_t _M0L6_2atmpS2501 = _M0L6_2atmpS2502 - 1023;
    int32_t _M0L6_2atmpS2500 = _M0L6_2atmpS2501 - 52;
    _M0Lm2e2S970 = _M0L6_2atmpS2500 - 2;
    _M0Lm2m2S971 = 4503599627370496ull | _M0L12ieeeMantissaS973;
  }
  _M0L6_2atmpS2603 = _M0Lm2m2S971;
  _M0L6_2atmpS2602 = _M0L6_2atmpS2603 & 1ull;
  _M0L4evenS974 = _M0L6_2atmpS2602 == 0ull;
  _M0L6_2atmpS2601 = _M0Lm2m2S971;
  _M0L2mvS975 = 4ull * _M0L6_2atmpS2601;
  if (_M0L12ieeeMantissaS973 != 0ull) {
    _M0L7mmShiftS976 = 1;
  } else {
    _M0L7mmShiftS976 = _M0L12ieeeExponentS972 <= 1u;
  }
  _M0Lm2vrS977 = 0ull;
  _M0Lm2vpS978 = 0ull;
  _M0Lm2vmS979 = 0ull;
  _M0Lm3e10S980 = 0;
  _M0Lm17vmIsTrailingZerosS981 = 0;
  _M0Lm17vrIsTrailingZerosS982 = 0;
  _M0L6_2atmpS2503 = _M0Lm2e2S970;
  if (_M0L6_2atmpS2503 >= 0) {
    int32_t _M0L6_2atmpS2525 = _M0Lm2e2S970;
    int32_t _M0L6_2atmpS2521;
    int32_t _M0L6_2atmpS2524;
    int32_t _M0L6_2atmpS2523;
    int32_t _M0L6_2atmpS2522;
    int32_t _M0L1qS983;
    int32_t _M0L6_2atmpS2520;
    int32_t _M0L6_2atmpS2519;
    int32_t _M0L1kS984;
    int32_t _M0L6_2atmpS2518;
    int32_t _M0L6_2atmpS2517;
    int32_t _M0L6_2atmpS2516;
    int32_t _M0L1iS985;
    struct _M0TPB8Pow5Pair _M0L4pow5S986;
    uint64_t _M0L6_2atmpS2515;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS987;
    uint64_t _M0L8_2avrOutS988;
    uint64_t _M0L8_2avpOutS989;
    uint64_t _M0L8_2avmOutS990;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2521 = _M0FPB9log10Pow2(_M0L6_2atmpS2525);
    _M0L6_2atmpS2524 = _M0Lm2e2S970;
    _M0L6_2atmpS2523 = _M0L6_2atmpS2524 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2522 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2523);
    _M0L1qS983 = _M0L6_2atmpS2521 - _M0L6_2atmpS2522;
    _M0Lm3e10S980 = _M0L1qS983;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2520 = _M0FPB8pow5bits(_M0L1qS983);
    _M0L6_2atmpS2519 = 125 + _M0L6_2atmpS2520;
    _M0L1kS984 = _M0L6_2atmpS2519 - 1;
    _M0L6_2atmpS2518 = _M0Lm2e2S970;
    _M0L6_2atmpS2517 = -_M0L6_2atmpS2518;
    _M0L6_2atmpS2516 = _M0L6_2atmpS2517 + _M0L1qS983;
    _M0L1iS985 = _M0L6_2atmpS2516 + _M0L1kS984;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S986 = _M0FPB22double__computeInvPow5(_M0L1qS983);
    _M0L6_2atmpS2515 = _M0Lm2m2S971;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS987
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2515, _M0L4pow5S986, _M0L1iS985, _M0L7mmShiftS976);
    _M0L8_2avrOutS988 = _M0L7_2abindS987.$0;
    _M0L8_2avpOutS989 = _M0L7_2abindS987.$1;
    _M0L8_2avmOutS990 = _M0L7_2abindS987.$2;
    _M0Lm2vrS977 = _M0L8_2avrOutS988;
    _M0Lm2vpS978 = _M0L8_2avpOutS989;
    _M0Lm2vmS979 = _M0L8_2avmOutS990;
    if (_M0L1qS983 <= 21) {
      int32_t _M0L6_2atmpS2511 = (int32_t)_M0L2mvS975;
      uint64_t _M0L6_2atmpS2514 = _M0L2mvS975 / 5ull;
      int32_t _M0L6_2atmpS2513 = (int32_t)_M0L6_2atmpS2514;
      int32_t _M0L6_2atmpS2512 = 5 * _M0L6_2atmpS2513;
      int32_t _M0L6mvMod5S991 = _M0L6_2atmpS2511 - _M0L6_2atmpS2512;
      if (_M0L6mvMod5S991 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS982
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS975, _M0L1qS983);
      } else if (_M0L4evenS974) {
        uint64_t _M0L6_2atmpS2505 = _M0L2mvS975 - 1ull;
        uint64_t _M0L6_2atmpS2506;
        uint64_t _M0L6_2atmpS2504;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2506 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS976);
        _M0L6_2atmpS2504 = _M0L6_2atmpS2505 - _M0L6_2atmpS2506;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS981
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2504, _M0L1qS983);
      } else {
        uint64_t _M0L6_2atmpS2507 = _M0Lm2vpS978;
        uint64_t _M0L6_2atmpS2510 = _M0L2mvS975 + 2ull;
        int32_t _M0L6_2atmpS2509;
        uint64_t _M0L6_2atmpS2508;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2509
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2510, _M0L1qS983);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2508 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2509);
        _M0Lm2vpS978 = _M0L6_2atmpS2507 - _M0L6_2atmpS2508;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2539 = _M0Lm2e2S970;
    int32_t _M0L6_2atmpS2538 = -_M0L6_2atmpS2539;
    int32_t _M0L6_2atmpS2533;
    int32_t _M0L6_2atmpS2537;
    int32_t _M0L6_2atmpS2536;
    int32_t _M0L6_2atmpS2535;
    int32_t _M0L6_2atmpS2534;
    int32_t _M0L1qS992;
    int32_t _M0L6_2atmpS2526;
    int32_t _M0L6_2atmpS2532;
    int32_t _M0L6_2atmpS2531;
    int32_t _M0L1iS993;
    int32_t _M0L6_2atmpS2530;
    int32_t _M0L1kS994;
    int32_t _M0L1jS995;
    struct _M0TPB8Pow5Pair _M0L4pow5S996;
    uint64_t _M0L6_2atmpS2529;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS997;
    uint64_t _M0L8_2avrOutS998;
    uint64_t _M0L8_2avpOutS999;
    uint64_t _M0L8_2avmOutS1000;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2533 = _M0FPB9log10Pow5(_M0L6_2atmpS2538);
    _M0L6_2atmpS2537 = _M0Lm2e2S970;
    _M0L6_2atmpS2536 = -_M0L6_2atmpS2537;
    _M0L6_2atmpS2535 = _M0L6_2atmpS2536 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2534 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2535);
    _M0L1qS992 = _M0L6_2atmpS2533 - _M0L6_2atmpS2534;
    _M0L6_2atmpS2526 = _M0Lm2e2S970;
    _M0Lm3e10S980 = _M0L1qS992 + _M0L6_2atmpS2526;
    _M0L6_2atmpS2532 = _M0Lm2e2S970;
    _M0L6_2atmpS2531 = -_M0L6_2atmpS2532;
    _M0L1iS993 = _M0L6_2atmpS2531 - _M0L1qS992;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2530 = _M0FPB8pow5bits(_M0L1iS993);
    _M0L1kS994 = _M0L6_2atmpS2530 - 125;
    _M0L1jS995 = _M0L1qS992 - _M0L1kS994;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S996 = _M0FPB19double__computePow5(_M0L1iS993);
    _M0L6_2atmpS2529 = _M0Lm2m2S971;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS997
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2529, _M0L4pow5S996, _M0L1jS995, _M0L7mmShiftS976);
    _M0L8_2avrOutS998 = _M0L7_2abindS997.$0;
    _M0L8_2avpOutS999 = _M0L7_2abindS997.$1;
    _M0L8_2avmOutS1000 = _M0L7_2abindS997.$2;
    _M0Lm2vrS977 = _M0L8_2avrOutS998;
    _M0Lm2vpS978 = _M0L8_2avpOutS999;
    _M0Lm2vmS979 = _M0L8_2avmOutS1000;
    if (_M0L1qS992 <= 1) {
      _M0Lm17vrIsTrailingZerosS982 = 1;
      if (_M0L4evenS974) {
        int32_t _M0L6_2atmpS2527;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2527 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS976);
        _M0Lm17vmIsTrailingZerosS981 = _M0L6_2atmpS2527 == 1;
      } else {
        uint64_t _M0L6_2atmpS2528 = _M0Lm2vpS978;
        _M0Lm2vpS978 = _M0L6_2atmpS2528 - 1ull;
      }
    } else if (_M0L1qS992 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS982
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS975, _M0L1qS992);
    }
  }
  _M0Lm7removedS1001 = 0;
  _M0Lm16lastRemovedDigitS1002 = 0;
  _M0Lm6outputS1003 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS981 || _M0Lm17vrIsTrailingZerosS982) {
    int32_t _if__result_3645;
    uint64_t _M0L6_2atmpS2569;
    uint64_t _M0L6_2atmpS2575;
    uint64_t _M0L6_2atmpS2576;
    int32_t _if__result_3646;
    int32_t _M0L6_2atmpS2572;
    int64_t _M0L6_2atmpS2571;
    uint64_t _M0L6_2atmpS2570;
    while (1) {
      uint64_t _M0L6_2atmpS2552 = _M0Lm2vpS978;
      uint64_t _M0L7vpDiv10S1004 = _M0L6_2atmpS2552 / 10ull;
      uint64_t _M0L6_2atmpS2551 = _M0Lm2vmS979;
      uint64_t _M0L7vmDiv10S1005 = _M0L6_2atmpS2551 / 10ull;
      uint64_t _M0L6_2atmpS2550;
      int32_t _M0L6_2atmpS2547;
      int32_t _M0L6_2atmpS2549;
      int32_t _M0L6_2atmpS2548;
      int32_t _M0L7vmMod10S1007;
      uint64_t _M0L6_2atmpS2546;
      uint64_t _M0L7vrDiv10S1008;
      uint64_t _M0L6_2atmpS2545;
      int32_t _M0L6_2atmpS2542;
      int32_t _M0L6_2atmpS2544;
      int32_t _M0L6_2atmpS2543;
      int32_t _M0L7vrMod10S1009;
      int32_t _M0L6_2atmpS2541;
      if (_M0L7vpDiv10S1004 <= _M0L7vmDiv10S1005) {
        break;
      }
      _M0L6_2atmpS2550 = _M0Lm2vmS979;
      _M0L6_2atmpS2547 = (int32_t)_M0L6_2atmpS2550;
      _M0L6_2atmpS2549 = (int32_t)_M0L7vmDiv10S1005;
      _M0L6_2atmpS2548 = 10 * _M0L6_2atmpS2549;
      _M0L7vmMod10S1007 = _M0L6_2atmpS2547 - _M0L6_2atmpS2548;
      _M0L6_2atmpS2546 = _M0Lm2vrS977;
      _M0L7vrDiv10S1008 = _M0L6_2atmpS2546 / 10ull;
      _M0L6_2atmpS2545 = _M0Lm2vrS977;
      _M0L6_2atmpS2542 = (int32_t)_M0L6_2atmpS2545;
      _M0L6_2atmpS2544 = (int32_t)_M0L7vrDiv10S1008;
      _M0L6_2atmpS2543 = 10 * _M0L6_2atmpS2544;
      _M0L7vrMod10S1009 = _M0L6_2atmpS2542 - _M0L6_2atmpS2543;
      if (_M0Lm17vmIsTrailingZerosS981) {
        _M0Lm17vmIsTrailingZerosS981 = _M0L7vmMod10S1007 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS981 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS982) {
        int32_t _M0L6_2atmpS2540 = _M0Lm16lastRemovedDigitS1002;
        _M0Lm17vrIsTrailingZerosS982 = _M0L6_2atmpS2540 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS982 = 0;
      }
      _M0Lm16lastRemovedDigitS1002 = _M0L7vrMod10S1009;
      _M0Lm2vrS977 = _M0L7vrDiv10S1008;
      _M0Lm2vpS978 = _M0L7vpDiv10S1004;
      _M0Lm2vmS979 = _M0L7vmDiv10S1005;
      _M0L6_2atmpS2541 = _M0Lm7removedS1001;
      _M0Lm7removedS1001 = _M0L6_2atmpS2541 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS981) {
      while (1) {
        uint64_t _M0L6_2atmpS2565 = _M0Lm2vmS979;
        uint64_t _M0L7vmDiv10S1010 = _M0L6_2atmpS2565 / 10ull;
        uint64_t _M0L6_2atmpS2564 = _M0Lm2vmS979;
        int32_t _M0L6_2atmpS2561 = (int32_t)_M0L6_2atmpS2564;
        int32_t _M0L6_2atmpS2563 = (int32_t)_M0L7vmDiv10S1010;
        int32_t _M0L6_2atmpS2562 = 10 * _M0L6_2atmpS2563;
        int32_t _M0L7vmMod10S1011 = _M0L6_2atmpS2561 - _M0L6_2atmpS2562;
        uint64_t _M0L6_2atmpS2560;
        uint64_t _M0L7vpDiv10S1013;
        uint64_t _M0L6_2atmpS2559;
        uint64_t _M0L7vrDiv10S1014;
        uint64_t _M0L6_2atmpS2558;
        int32_t _M0L6_2atmpS2555;
        int32_t _M0L6_2atmpS2557;
        int32_t _M0L6_2atmpS2556;
        int32_t _M0L7vrMod10S1015;
        int32_t _M0L6_2atmpS2554;
        if (_M0L7vmMod10S1011 != 0) {
          break;
        }
        _M0L6_2atmpS2560 = _M0Lm2vpS978;
        _M0L7vpDiv10S1013 = _M0L6_2atmpS2560 / 10ull;
        _M0L6_2atmpS2559 = _M0Lm2vrS977;
        _M0L7vrDiv10S1014 = _M0L6_2atmpS2559 / 10ull;
        _M0L6_2atmpS2558 = _M0Lm2vrS977;
        _M0L6_2atmpS2555 = (int32_t)_M0L6_2atmpS2558;
        _M0L6_2atmpS2557 = (int32_t)_M0L7vrDiv10S1014;
        _M0L6_2atmpS2556 = 10 * _M0L6_2atmpS2557;
        _M0L7vrMod10S1015 = _M0L6_2atmpS2555 - _M0L6_2atmpS2556;
        if (_M0Lm17vrIsTrailingZerosS982) {
          int32_t _M0L6_2atmpS2553 = _M0Lm16lastRemovedDigitS1002;
          _M0Lm17vrIsTrailingZerosS982 = _M0L6_2atmpS2553 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS982 = 0;
        }
        _M0Lm16lastRemovedDigitS1002 = _M0L7vrMod10S1015;
        _M0Lm2vrS977 = _M0L7vrDiv10S1014;
        _M0Lm2vpS978 = _M0L7vpDiv10S1013;
        _M0Lm2vmS979 = _M0L7vmDiv10S1010;
        _M0L6_2atmpS2554 = _M0Lm7removedS1001;
        _M0Lm7removedS1001 = _M0L6_2atmpS2554 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS982) {
      int32_t _M0L6_2atmpS2568 = _M0Lm16lastRemovedDigitS1002;
      if (_M0L6_2atmpS2568 == 5) {
        uint64_t _M0L6_2atmpS2567 = _M0Lm2vrS977;
        uint64_t _M0L6_2atmpS2566 = _M0L6_2atmpS2567 % 2ull;
        _if__result_3645 = _M0L6_2atmpS2566 == 0ull;
      } else {
        _if__result_3645 = 0;
      }
    } else {
      _if__result_3645 = 0;
    }
    if (_if__result_3645) {
      _M0Lm16lastRemovedDigitS1002 = 4;
    }
    _M0L6_2atmpS2569 = _M0Lm2vrS977;
    _M0L6_2atmpS2575 = _M0Lm2vrS977;
    _M0L6_2atmpS2576 = _M0Lm2vmS979;
    if (_M0L6_2atmpS2575 == _M0L6_2atmpS2576) {
      if (!_M0L4evenS974) {
        _if__result_3646 = 1;
      } else {
        int32_t _M0L6_2atmpS2574 = _M0Lm17vmIsTrailingZerosS981;
        _if__result_3646 = !_M0L6_2atmpS2574;
      }
    } else {
      _if__result_3646 = 0;
    }
    if (_if__result_3646) {
      _M0L6_2atmpS2572 = 1;
    } else {
      int32_t _M0L6_2atmpS2573 = _M0Lm16lastRemovedDigitS1002;
      _M0L6_2atmpS2572 = _M0L6_2atmpS2573 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2571 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2572);
    _M0L6_2atmpS2570 = *(uint64_t*)&_M0L6_2atmpS2571;
    _M0Lm6outputS1003 = _M0L6_2atmpS2569 + _M0L6_2atmpS2570;
  } else {
    int32_t _M0Lm7roundUpS1016 = 0;
    uint64_t _M0L6_2atmpS2597 = _M0Lm2vpS978;
    uint64_t _M0L8vpDiv100S1017 = _M0L6_2atmpS2597 / 100ull;
    uint64_t _M0L6_2atmpS2596 = _M0Lm2vmS979;
    uint64_t _M0L8vmDiv100S1018 = _M0L6_2atmpS2596 / 100ull;
    uint64_t _M0L6_2atmpS2591;
    uint64_t _M0L6_2atmpS2594;
    uint64_t _M0L6_2atmpS2595;
    int32_t _M0L6_2atmpS2593;
    uint64_t _M0L6_2atmpS2592;
    if (_M0L8vpDiv100S1017 > _M0L8vmDiv100S1018) {
      uint64_t _M0L6_2atmpS2582 = _M0Lm2vrS977;
      uint64_t _M0L8vrDiv100S1019 = _M0L6_2atmpS2582 / 100ull;
      uint64_t _M0L6_2atmpS2581 = _M0Lm2vrS977;
      int32_t _M0L6_2atmpS2578 = (int32_t)_M0L6_2atmpS2581;
      int32_t _M0L6_2atmpS2580 = (int32_t)_M0L8vrDiv100S1019;
      int32_t _M0L6_2atmpS2579 = 100 * _M0L6_2atmpS2580;
      int32_t _M0L8vrMod100S1020 = _M0L6_2atmpS2578 - _M0L6_2atmpS2579;
      int32_t _M0L6_2atmpS2577;
      _M0Lm7roundUpS1016 = _M0L8vrMod100S1020 >= 50;
      _M0Lm2vrS977 = _M0L8vrDiv100S1019;
      _M0Lm2vpS978 = _M0L8vpDiv100S1017;
      _M0Lm2vmS979 = _M0L8vmDiv100S1018;
      _M0L6_2atmpS2577 = _M0Lm7removedS1001;
      _M0Lm7removedS1001 = _M0L6_2atmpS2577 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2590 = _M0Lm2vpS978;
      uint64_t _M0L7vpDiv10S1021 = _M0L6_2atmpS2590 / 10ull;
      uint64_t _M0L6_2atmpS2589 = _M0Lm2vmS979;
      uint64_t _M0L7vmDiv10S1022 = _M0L6_2atmpS2589 / 10ull;
      uint64_t _M0L6_2atmpS2588;
      uint64_t _M0L7vrDiv10S1024;
      uint64_t _M0L6_2atmpS2587;
      int32_t _M0L6_2atmpS2584;
      int32_t _M0L6_2atmpS2586;
      int32_t _M0L6_2atmpS2585;
      int32_t _M0L7vrMod10S1025;
      int32_t _M0L6_2atmpS2583;
      if (_M0L7vpDiv10S1021 <= _M0L7vmDiv10S1022) {
        break;
      }
      _M0L6_2atmpS2588 = _M0Lm2vrS977;
      _M0L7vrDiv10S1024 = _M0L6_2atmpS2588 / 10ull;
      _M0L6_2atmpS2587 = _M0Lm2vrS977;
      _M0L6_2atmpS2584 = (int32_t)_M0L6_2atmpS2587;
      _M0L6_2atmpS2586 = (int32_t)_M0L7vrDiv10S1024;
      _M0L6_2atmpS2585 = 10 * _M0L6_2atmpS2586;
      _M0L7vrMod10S1025 = _M0L6_2atmpS2584 - _M0L6_2atmpS2585;
      _M0Lm7roundUpS1016 = _M0L7vrMod10S1025 >= 5;
      _M0Lm2vrS977 = _M0L7vrDiv10S1024;
      _M0Lm2vpS978 = _M0L7vpDiv10S1021;
      _M0Lm2vmS979 = _M0L7vmDiv10S1022;
      _M0L6_2atmpS2583 = _M0Lm7removedS1001;
      _M0Lm7removedS1001 = _M0L6_2atmpS2583 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2591 = _M0Lm2vrS977;
    _M0L6_2atmpS2594 = _M0Lm2vrS977;
    _M0L6_2atmpS2595 = _M0Lm2vmS979;
    _M0L6_2atmpS2593
    = _M0L6_2atmpS2594 == _M0L6_2atmpS2595 || _M0Lm7roundUpS1016;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2592 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2593);
    _M0Lm6outputS1003 = _M0L6_2atmpS2591 + _M0L6_2atmpS2592;
  }
  _M0L6_2atmpS2599 = _M0Lm3e10S980;
  _M0L6_2atmpS2600 = _M0Lm7removedS1001;
  _M0L3expS1026 = _M0L6_2atmpS2599 + _M0L6_2atmpS2600;
  _M0L6_2atmpS2598 = _M0Lm6outputS1003;
  _block_3648
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3648)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3648->$0 = _M0L6_2atmpS2598;
  _block_3648->$1 = _M0L3expS1026;
  return _block_3648;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS969) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS969) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS968) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS968) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS967) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS967) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS966) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS966 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS966 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS966 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS966 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS966 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS966 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS966 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS966 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS966 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS966 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS966 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS966 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS966 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS966 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS966 >= 100ull) {
    return 3;
  }
  if (_M0L1vS966 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS949) {
  int32_t _M0L6_2atmpS2499;
  int32_t _M0L6_2atmpS2498;
  int32_t _M0L4baseS948;
  int32_t _M0L5base2S950;
  int32_t _M0L6offsetS951;
  int32_t _M0L6_2atmpS2497;
  uint64_t _M0L4mul0S952;
  int32_t _M0L6_2atmpS2496;
  int32_t _M0L6_2atmpS2495;
  uint64_t _M0L4mul1S953;
  uint64_t _M0L1mS954;
  struct _M0TPB7Umul128 _M0L7_2abindS955;
  uint64_t _M0L7_2alow1S956;
  uint64_t _M0L8_2ahigh1S957;
  struct _M0TPB7Umul128 _M0L7_2abindS958;
  uint64_t _M0L7_2alow0S959;
  uint64_t _M0L8_2ahigh0S960;
  uint64_t _M0L3sumS961;
  uint64_t _M0Lm5high1S962;
  int32_t _M0L6_2atmpS2493;
  int32_t _M0L6_2atmpS2494;
  int32_t _M0L5deltaS963;
  uint64_t _M0L6_2atmpS2492;
  uint64_t _M0L6_2atmpS2484;
  int32_t _M0L6_2atmpS2491;
  uint32_t _M0L6_2atmpS2488;
  int32_t _M0L6_2atmpS2490;
  int32_t _M0L6_2atmpS2489;
  uint32_t _M0L6_2atmpS2487;
  uint32_t _M0L6_2atmpS2486;
  uint64_t _M0L6_2atmpS2485;
  uint64_t _M0L1aS964;
  uint64_t _M0L6_2atmpS2483;
  uint64_t _M0L1bS965;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2499 = _M0L1iS949 + 26;
  _M0L6_2atmpS2498 = _M0L6_2atmpS2499 - 1;
  _M0L4baseS948 = _M0L6_2atmpS2498 / 26;
  _M0L5base2S950 = _M0L4baseS948 * 26;
  _M0L6offsetS951 = _M0L5base2S950 - _M0L1iS949;
  _M0L6_2atmpS2497 = _M0L4baseS948 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S952
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2497);
  _M0L6_2atmpS2496 = _M0L4baseS948 * 2;
  _M0L6_2atmpS2495 = _M0L6_2atmpS2496 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S953
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2495);
  if (_M0L6offsetS951 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S952, _M0L4mul1S953};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS954
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS951);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS955 = _M0FPB7umul128(_M0L1mS954, _M0L4mul1S953);
  _M0L7_2alow1S956 = _M0L7_2abindS955.$0;
  _M0L8_2ahigh1S957 = _M0L7_2abindS955.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS958 = _M0FPB7umul128(_M0L1mS954, _M0L4mul0S952);
  _M0L7_2alow0S959 = _M0L7_2abindS958.$0;
  _M0L8_2ahigh0S960 = _M0L7_2abindS958.$1;
  _M0L3sumS961 = _M0L8_2ahigh0S960 + _M0L7_2alow1S956;
  _M0Lm5high1S962 = _M0L8_2ahigh1S957;
  if (_M0L3sumS961 < _M0L8_2ahigh0S960) {
    uint64_t _M0L6_2atmpS2482 = _M0Lm5high1S962;
    _M0Lm5high1S962 = _M0L6_2atmpS2482 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2493 = _M0FPB8pow5bits(_M0L5base2S950);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2494 = _M0FPB8pow5bits(_M0L1iS949);
  _M0L5deltaS963 = _M0L6_2atmpS2493 - _M0L6_2atmpS2494;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2492
  = _M0FPB13shiftright128(_M0L7_2alow0S959, _M0L3sumS961, _M0L5deltaS963);
  _M0L6_2atmpS2484 = _M0L6_2atmpS2492 + 1ull;
  _M0L6_2atmpS2491 = _M0L1iS949 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2488
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2491);
  _M0L6_2atmpS2490 = _M0L1iS949 % 16;
  _M0L6_2atmpS2489 = _M0L6_2atmpS2490 << 1;
  _M0L6_2atmpS2487 = _M0L6_2atmpS2488 >> (_M0L6_2atmpS2489 & 31);
  _M0L6_2atmpS2486 = _M0L6_2atmpS2487 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2485 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2486);
  _M0L1aS964 = _M0L6_2atmpS2484 + _M0L6_2atmpS2485;
  _M0L6_2atmpS2483 = _M0Lm5high1S962;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS965
  = _M0FPB13shiftright128(_M0L3sumS961, _M0L6_2atmpS2483, _M0L5deltaS963);
  return (struct _M0TPB8Pow5Pair){_M0L1aS964, _M0L1bS965};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS931) {
  int32_t _M0L4baseS930;
  int32_t _M0L5base2S932;
  int32_t _M0L6offsetS933;
  int32_t _M0L6_2atmpS2481;
  uint64_t _M0L4mul0S934;
  int32_t _M0L6_2atmpS2480;
  int32_t _M0L6_2atmpS2479;
  uint64_t _M0L4mul1S935;
  uint64_t _M0L1mS936;
  struct _M0TPB7Umul128 _M0L7_2abindS937;
  uint64_t _M0L7_2alow1S938;
  uint64_t _M0L8_2ahigh1S939;
  struct _M0TPB7Umul128 _M0L7_2abindS940;
  uint64_t _M0L7_2alow0S941;
  uint64_t _M0L8_2ahigh0S942;
  uint64_t _M0L3sumS943;
  uint64_t _M0Lm5high1S944;
  int32_t _M0L6_2atmpS2477;
  int32_t _M0L6_2atmpS2478;
  int32_t _M0L5deltaS945;
  uint64_t _M0L6_2atmpS2469;
  int32_t _M0L6_2atmpS2476;
  uint32_t _M0L6_2atmpS2473;
  int32_t _M0L6_2atmpS2475;
  int32_t _M0L6_2atmpS2474;
  uint32_t _M0L6_2atmpS2472;
  uint32_t _M0L6_2atmpS2471;
  uint64_t _M0L6_2atmpS2470;
  uint64_t _M0L1aS946;
  uint64_t _M0L6_2atmpS2468;
  uint64_t _M0L1bS947;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS930 = _M0L1iS931 / 26;
  _M0L5base2S932 = _M0L4baseS930 * 26;
  _M0L6offsetS933 = _M0L1iS931 - _M0L5base2S932;
  _M0L6_2atmpS2481 = _M0L4baseS930 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S934
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2481);
  _M0L6_2atmpS2480 = _M0L4baseS930 * 2;
  _M0L6_2atmpS2479 = _M0L6_2atmpS2480 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S935
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2479);
  if (_M0L6offsetS933 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S934, _M0L4mul1S935};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS936
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS933);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS937 = _M0FPB7umul128(_M0L1mS936, _M0L4mul1S935);
  _M0L7_2alow1S938 = _M0L7_2abindS937.$0;
  _M0L8_2ahigh1S939 = _M0L7_2abindS937.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS940 = _M0FPB7umul128(_M0L1mS936, _M0L4mul0S934);
  _M0L7_2alow0S941 = _M0L7_2abindS940.$0;
  _M0L8_2ahigh0S942 = _M0L7_2abindS940.$1;
  _M0L3sumS943 = _M0L8_2ahigh0S942 + _M0L7_2alow1S938;
  _M0Lm5high1S944 = _M0L8_2ahigh1S939;
  if (_M0L3sumS943 < _M0L8_2ahigh0S942) {
    uint64_t _M0L6_2atmpS2467 = _M0Lm5high1S944;
    _M0Lm5high1S944 = _M0L6_2atmpS2467 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2477 = _M0FPB8pow5bits(_M0L1iS931);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2478 = _M0FPB8pow5bits(_M0L5base2S932);
  _M0L5deltaS945 = _M0L6_2atmpS2477 - _M0L6_2atmpS2478;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2469
  = _M0FPB13shiftright128(_M0L7_2alow0S941, _M0L3sumS943, _M0L5deltaS945);
  _M0L6_2atmpS2476 = _M0L1iS931 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2473
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2476);
  _M0L6_2atmpS2475 = _M0L1iS931 % 16;
  _M0L6_2atmpS2474 = _M0L6_2atmpS2475 << 1;
  _M0L6_2atmpS2472 = _M0L6_2atmpS2473 >> (_M0L6_2atmpS2474 & 31);
  _M0L6_2atmpS2471 = _M0L6_2atmpS2472 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2470 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2471);
  _M0L1aS946 = _M0L6_2atmpS2469 + _M0L6_2atmpS2470;
  _M0L6_2atmpS2468 = _M0Lm5high1S944;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS947
  = _M0FPB13shiftright128(_M0L3sumS943, _M0L6_2atmpS2468, _M0L5deltaS945);
  return (struct _M0TPB8Pow5Pair){_M0L1aS946, _M0L1bS947};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS904,
  struct _M0TPB8Pow5Pair _M0L3mulS901,
  int32_t _M0L1jS917,
  int32_t _M0L7mmShiftS919
) {
  uint64_t _M0L7_2amul0S900;
  uint64_t _M0L7_2amul1S902;
  uint64_t _M0L1mS903;
  struct _M0TPB7Umul128 _M0L7_2abindS905;
  uint64_t _M0L5_2aloS906;
  uint64_t _M0L6_2atmpS907;
  struct _M0TPB7Umul128 _M0L7_2abindS908;
  uint64_t _M0L6_2alo2S909;
  uint64_t _M0L6_2ahi2S910;
  uint64_t _M0L3midS911;
  uint64_t _M0L6_2atmpS2466;
  uint64_t _M0L2hiS912;
  uint64_t _M0L3lo2S913;
  uint64_t _M0L6_2atmpS2464;
  uint64_t _M0L6_2atmpS2465;
  uint64_t _M0L4mid2S914;
  uint64_t _M0L6_2atmpS2463;
  uint64_t _M0L3hi2S915;
  int32_t _M0L6_2atmpS2462;
  int32_t _M0L6_2atmpS2461;
  uint64_t _M0L2vpS916;
  uint64_t _M0Lm2vmS918;
  int32_t _M0L6_2atmpS2460;
  int32_t _M0L6_2atmpS2459;
  uint64_t _M0L2vrS929;
  uint64_t _M0L6_2atmpS2458;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S900 = _M0L3mulS901.$0;
  _M0L7_2amul1S902 = _M0L3mulS901.$1;
  _M0L1mS903 = _M0L1mS904 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS905 = _M0FPB7umul128(_M0L1mS903, _M0L7_2amul0S900);
  _M0L5_2aloS906 = _M0L7_2abindS905.$0;
  _M0L6_2atmpS907 = _M0L7_2abindS905.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS908 = _M0FPB7umul128(_M0L1mS903, _M0L7_2amul1S902);
  _M0L6_2alo2S909 = _M0L7_2abindS908.$0;
  _M0L6_2ahi2S910 = _M0L7_2abindS908.$1;
  _M0L3midS911 = _M0L6_2atmpS907 + _M0L6_2alo2S909;
  if (_M0L3midS911 < _M0L6_2atmpS907) {
    _M0L6_2atmpS2466 = 1ull;
  } else {
    _M0L6_2atmpS2466 = 0ull;
  }
  _M0L2hiS912 = _M0L6_2ahi2S910 + _M0L6_2atmpS2466;
  _M0L3lo2S913 = _M0L5_2aloS906 + _M0L7_2amul0S900;
  _M0L6_2atmpS2464 = _M0L3midS911 + _M0L7_2amul1S902;
  if (_M0L3lo2S913 < _M0L5_2aloS906) {
    _M0L6_2atmpS2465 = 1ull;
  } else {
    _M0L6_2atmpS2465 = 0ull;
  }
  _M0L4mid2S914 = _M0L6_2atmpS2464 + _M0L6_2atmpS2465;
  if (_M0L4mid2S914 < _M0L3midS911) {
    _M0L6_2atmpS2463 = 1ull;
  } else {
    _M0L6_2atmpS2463 = 0ull;
  }
  _M0L3hi2S915 = _M0L2hiS912 + _M0L6_2atmpS2463;
  _M0L6_2atmpS2462 = _M0L1jS917 - 64;
  _M0L6_2atmpS2461 = _M0L6_2atmpS2462 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS916
  = _M0FPB13shiftright128(_M0L4mid2S914, _M0L3hi2S915, _M0L6_2atmpS2461);
  _M0Lm2vmS918 = 0ull;
  if (_M0L7mmShiftS919) {
    uint64_t _M0L3lo3S920 = _M0L5_2aloS906 - _M0L7_2amul0S900;
    uint64_t _M0L6_2atmpS2448 = _M0L3midS911 - _M0L7_2amul1S902;
    uint64_t _M0L6_2atmpS2449;
    uint64_t _M0L4mid3S921;
    uint64_t _M0L6_2atmpS2447;
    uint64_t _M0L3hi3S922;
    int32_t _M0L6_2atmpS2446;
    int32_t _M0L6_2atmpS2445;
    if (_M0L5_2aloS906 < _M0L3lo3S920) {
      _M0L6_2atmpS2449 = 1ull;
    } else {
      _M0L6_2atmpS2449 = 0ull;
    }
    _M0L4mid3S921 = _M0L6_2atmpS2448 - _M0L6_2atmpS2449;
    if (_M0L3midS911 < _M0L4mid3S921) {
      _M0L6_2atmpS2447 = 1ull;
    } else {
      _M0L6_2atmpS2447 = 0ull;
    }
    _M0L3hi3S922 = _M0L2hiS912 - _M0L6_2atmpS2447;
    _M0L6_2atmpS2446 = _M0L1jS917 - 64;
    _M0L6_2atmpS2445 = _M0L6_2atmpS2446 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS918
    = _M0FPB13shiftright128(_M0L4mid3S921, _M0L3hi3S922, _M0L6_2atmpS2445);
  } else {
    uint64_t _M0L3lo3S923 = _M0L5_2aloS906 + _M0L5_2aloS906;
    uint64_t _M0L6_2atmpS2456 = _M0L3midS911 + _M0L3midS911;
    uint64_t _M0L6_2atmpS2457;
    uint64_t _M0L4mid3S924;
    uint64_t _M0L6_2atmpS2454;
    uint64_t _M0L6_2atmpS2455;
    uint64_t _M0L3hi3S925;
    uint64_t _M0L3lo4S926;
    uint64_t _M0L6_2atmpS2452;
    uint64_t _M0L6_2atmpS2453;
    uint64_t _M0L4mid4S927;
    uint64_t _M0L6_2atmpS2451;
    uint64_t _M0L3hi4S928;
    int32_t _M0L6_2atmpS2450;
    if (_M0L3lo3S923 < _M0L5_2aloS906) {
      _M0L6_2atmpS2457 = 1ull;
    } else {
      _M0L6_2atmpS2457 = 0ull;
    }
    _M0L4mid3S924 = _M0L6_2atmpS2456 + _M0L6_2atmpS2457;
    _M0L6_2atmpS2454 = _M0L2hiS912 + _M0L2hiS912;
    if (_M0L4mid3S924 < _M0L3midS911) {
      _M0L6_2atmpS2455 = 1ull;
    } else {
      _M0L6_2atmpS2455 = 0ull;
    }
    _M0L3hi3S925 = _M0L6_2atmpS2454 + _M0L6_2atmpS2455;
    _M0L3lo4S926 = _M0L3lo3S923 - _M0L7_2amul0S900;
    _M0L6_2atmpS2452 = _M0L4mid3S924 - _M0L7_2amul1S902;
    if (_M0L3lo3S923 < _M0L3lo4S926) {
      _M0L6_2atmpS2453 = 1ull;
    } else {
      _M0L6_2atmpS2453 = 0ull;
    }
    _M0L4mid4S927 = _M0L6_2atmpS2452 - _M0L6_2atmpS2453;
    if (_M0L4mid3S924 < _M0L4mid4S927) {
      _M0L6_2atmpS2451 = 1ull;
    } else {
      _M0L6_2atmpS2451 = 0ull;
    }
    _M0L3hi4S928 = _M0L3hi3S925 - _M0L6_2atmpS2451;
    _M0L6_2atmpS2450 = _M0L1jS917 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS918
    = _M0FPB13shiftright128(_M0L4mid4S927, _M0L3hi4S928, _M0L6_2atmpS2450);
  }
  _M0L6_2atmpS2460 = _M0L1jS917 - 64;
  _M0L6_2atmpS2459 = _M0L6_2atmpS2460 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS929
  = _M0FPB13shiftright128(_M0L3midS911, _M0L2hiS912, _M0L6_2atmpS2459);
  _M0L6_2atmpS2458 = _M0Lm2vmS918;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS929,
                                                _M0L2vpS916,
                                                _M0L6_2atmpS2458};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS898,
  int32_t _M0L1pS899
) {
  uint64_t _M0L6_2atmpS2444;
  uint64_t _M0L6_2atmpS2443;
  uint64_t _M0L6_2atmpS2442;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2444 = 1ull << (_M0L1pS899 & 63);
  _M0L6_2atmpS2443 = _M0L6_2atmpS2444 - 1ull;
  _M0L6_2atmpS2442 = _M0L5valueS898 & _M0L6_2atmpS2443;
  return _M0L6_2atmpS2442 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS896,
  int32_t _M0L1pS897
) {
  int32_t _M0L6_2atmpS2441;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2441 = _M0FPB10pow5Factor(_M0L5valueS896);
  return _M0L6_2atmpS2441 >= _M0L1pS897;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS892) {
  uint64_t _M0L6_2atmpS2429;
  uint64_t _M0L6_2atmpS2430;
  uint64_t _M0L6_2atmpS2431;
  uint64_t _M0L6_2atmpS2432;
  int32_t _M0Lm5countS893;
  uint64_t _M0Lm5valueS894;
  uint64_t _M0L6_2atmpS2440;
  moonbit_string_t _M0L6_2atmpS2439;
  moonbit_string_t _M0L6_2atmpS3214;
  moonbit_string_t _M0L6_2atmpS2438;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2429 = _M0L5valueS892 % 5ull;
  if (_M0L6_2atmpS2429 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2430 = _M0L5valueS892 % 25ull;
  if (_M0L6_2atmpS2430 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2431 = _M0L5valueS892 % 125ull;
  if (_M0L6_2atmpS2431 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2432 = _M0L5valueS892 % 625ull;
  if (_M0L6_2atmpS2432 != 0ull) {
    return 3;
  }
  _M0Lm5countS893 = 4;
  _M0Lm5valueS894 = _M0L5valueS892 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2433 = _M0Lm5valueS894;
    if (_M0L6_2atmpS2433 > 0ull) {
      uint64_t _M0L6_2atmpS2435 = _M0Lm5valueS894;
      uint64_t _M0L6_2atmpS2434 = _M0L6_2atmpS2435 % 5ull;
      uint64_t _M0L6_2atmpS2436;
      int32_t _M0L6_2atmpS2437;
      if (_M0L6_2atmpS2434 != 0ull) {
        return _M0Lm5countS893;
      }
      _M0L6_2atmpS2436 = _M0Lm5valueS894;
      _M0Lm5valueS894 = _M0L6_2atmpS2436 / 5ull;
      _M0L6_2atmpS2437 = _M0Lm5countS893;
      _M0Lm5countS893 = _M0L6_2atmpS2437 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2440 = _M0Lm5valueS894;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2439
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2440);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3214
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_59.data, _M0L6_2atmpS2439);
  moonbit_decref(_M0L6_2atmpS2439);
  _M0L6_2atmpS2438 = _M0L6_2atmpS3214;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2438, (moonbit_string_t)moonbit_string_literal_60.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS891,
  uint64_t _M0L2hiS889,
  int32_t _M0L4distS890
) {
  int32_t _M0L6_2atmpS2428;
  uint64_t _M0L6_2atmpS2426;
  uint64_t _M0L6_2atmpS2427;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2428 = 64 - _M0L4distS890;
  _M0L6_2atmpS2426 = _M0L2hiS889 << (_M0L6_2atmpS2428 & 63);
  _M0L6_2atmpS2427 = _M0L2loS891 >> (_M0L4distS890 & 63);
  return _M0L6_2atmpS2426 | _M0L6_2atmpS2427;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS879,
  uint64_t _M0L1bS882
) {
  uint64_t _M0L3aLoS878;
  uint64_t _M0L3aHiS880;
  uint64_t _M0L3bLoS881;
  uint64_t _M0L3bHiS883;
  uint64_t _M0L1xS884;
  uint64_t _M0L6_2atmpS2424;
  uint64_t _M0L6_2atmpS2425;
  uint64_t _M0L1yS885;
  uint64_t _M0L6_2atmpS2422;
  uint64_t _M0L6_2atmpS2423;
  uint64_t _M0L1zS886;
  uint64_t _M0L6_2atmpS2420;
  uint64_t _M0L6_2atmpS2421;
  uint64_t _M0L6_2atmpS2418;
  uint64_t _M0L6_2atmpS2419;
  uint64_t _M0L1wS887;
  uint64_t _M0L2loS888;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS878 = _M0L1aS879 & 4294967295ull;
  _M0L3aHiS880 = _M0L1aS879 >> 32;
  _M0L3bLoS881 = _M0L1bS882 & 4294967295ull;
  _M0L3bHiS883 = _M0L1bS882 >> 32;
  _M0L1xS884 = _M0L3aLoS878 * _M0L3bLoS881;
  _M0L6_2atmpS2424 = _M0L3aHiS880 * _M0L3bLoS881;
  _M0L6_2atmpS2425 = _M0L1xS884 >> 32;
  _M0L1yS885 = _M0L6_2atmpS2424 + _M0L6_2atmpS2425;
  _M0L6_2atmpS2422 = _M0L3aLoS878 * _M0L3bHiS883;
  _M0L6_2atmpS2423 = _M0L1yS885 & 4294967295ull;
  _M0L1zS886 = _M0L6_2atmpS2422 + _M0L6_2atmpS2423;
  _M0L6_2atmpS2420 = _M0L3aHiS880 * _M0L3bHiS883;
  _M0L6_2atmpS2421 = _M0L1yS885 >> 32;
  _M0L6_2atmpS2418 = _M0L6_2atmpS2420 + _M0L6_2atmpS2421;
  _M0L6_2atmpS2419 = _M0L1zS886 >> 32;
  _M0L1wS887 = _M0L6_2atmpS2418 + _M0L6_2atmpS2419;
  _M0L2loS888 = _M0L1aS879 * _M0L1bS882;
  return (struct _M0TPB7Umul128){_M0L2loS888, _M0L1wS887};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS873,
  int32_t _M0L4fromS877,
  int32_t _M0L2toS875
) {
  int32_t _M0L6_2atmpS2417;
  struct _M0TPB13StringBuilder* _M0L3bufS872;
  int32_t _M0L1iS874;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2417 = Moonbit_array_length(_M0L5bytesS873);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS872 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2417);
  _M0L1iS874 = _M0L4fromS877;
  while (1) {
    if (_M0L1iS874 < _M0L2toS875) {
      int32_t _M0L6_2atmpS2415;
      int32_t _M0L6_2atmpS2414;
      int32_t _M0L6_2atmpS2416;
      if (
        _M0L1iS874 < 0 || _M0L1iS874 >= Moonbit_array_length(_M0L5bytesS873)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2415 = (int32_t)_M0L5bytesS873[_M0L1iS874];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2414 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2415);
      moonbit_incref(_M0L3bufS872);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS872, _M0L6_2atmpS2414);
      _M0L6_2atmpS2416 = _M0L1iS874 + 1;
      _M0L1iS874 = _M0L6_2atmpS2416;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS873);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS872);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS871) {
  int32_t _M0L6_2atmpS2413;
  uint32_t _M0L6_2atmpS2412;
  uint32_t _M0L6_2atmpS2411;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2413 = _M0L1eS871 * 78913;
  _M0L6_2atmpS2412 = *(uint32_t*)&_M0L6_2atmpS2413;
  _M0L6_2atmpS2411 = _M0L6_2atmpS2412 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2411;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS870) {
  int32_t _M0L6_2atmpS2410;
  uint32_t _M0L6_2atmpS2409;
  uint32_t _M0L6_2atmpS2408;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2410 = _M0L1eS870 * 732923;
  _M0L6_2atmpS2409 = *(uint32_t*)&_M0L6_2atmpS2410;
  _M0L6_2atmpS2408 = _M0L6_2atmpS2409 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2408;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS868,
  int32_t _M0L8exponentS869,
  int32_t _M0L8mantissaS866
) {
  moonbit_string_t _M0L1sS867;
  moonbit_string_t _M0L6_2atmpS3215;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS866) {
    return (moonbit_string_t)moonbit_string_literal_61.data;
  }
  if (_M0L4signS868) {
    _M0L1sS867 = (moonbit_string_t)moonbit_string_literal_62.data;
  } else {
    _M0L1sS867 = (moonbit_string_t)moonbit_string_literal_1.data;
  }
  if (_M0L8exponentS869) {
    moonbit_string_t _M0L6_2atmpS3216;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3216
    = moonbit_add_string(_M0L1sS867, (moonbit_string_t)moonbit_string_literal_63.data);
    moonbit_decref(_M0L1sS867);
    return _M0L6_2atmpS3216;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3215
  = moonbit_add_string(_M0L1sS867, (moonbit_string_t)moonbit_string_literal_64.data);
  moonbit_decref(_M0L1sS867);
  return _M0L6_2atmpS3215;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS865) {
  int32_t _M0L6_2atmpS2407;
  uint32_t _M0L6_2atmpS2406;
  uint32_t _M0L6_2atmpS2405;
  int32_t _M0L6_2atmpS2404;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2407 = _M0L1eS865 * 1217359;
  _M0L6_2atmpS2406 = *(uint32_t*)&_M0L6_2atmpS2407;
  _M0L6_2atmpS2405 = _M0L6_2atmpS2406 >> 19;
  _M0L6_2atmpS2404 = *(int32_t*)&_M0L6_2atmpS2405;
  return _M0L6_2atmpS2404 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS864,
  struct _M0TPB6Hasher* _M0L6hasherS863
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS863, _M0L4selfS864);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS862,
  struct _M0TPB6Hasher* _M0L6hasherS861
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS861, _M0L4selfS862);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS859,
  moonbit_string_t _M0L5valueS857
) {
  int32_t _M0L7_2abindS856;
  int32_t _M0L1iS858;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS856 = Moonbit_array_length(_M0L5valueS857);
  _M0L1iS858 = 0;
  while (1) {
    if (_M0L1iS858 < _M0L7_2abindS856) {
      int32_t _M0L6_2atmpS2402 = _M0L5valueS857[_M0L1iS858];
      int32_t _M0L6_2atmpS2401 = (int32_t)_M0L6_2atmpS2402;
      uint32_t _M0L6_2atmpS2400 = *(uint32_t*)&_M0L6_2atmpS2401;
      int32_t _M0L6_2atmpS2403;
      moonbit_incref(_M0L4selfS859);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS859, _M0L6_2atmpS2400);
      _M0L6_2atmpS2403 = _M0L1iS858 + 1;
      _M0L1iS858 = _M0L6_2atmpS2403;
      continue;
    } else {
      moonbit_decref(_M0L4selfS859);
      moonbit_decref(_M0L5valueS857);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS854,
  int32_t _M0L3idxS855
) {
  int32_t _M0L6_2atmpS3217;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3217 = _M0L4selfS854[_M0L3idxS855];
  moonbit_decref(_M0L4selfS854);
  return _M0L6_2atmpS3217;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS853
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2399;
  void* _block_3652;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IPC14json4JsonPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2399
  = _M0MPC15array5Array3mapGRPB4JsonRPB4JsonE(_M0L4selfS853, _M0IPC14json4JsonPB6ToJson14to__json_2eclo);
  _block_3652 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3652)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3652)->$0 = _M0L6_2atmpS2399;
  return _block_3652;
}

struct moonbit_result_2 _M0MPC15array5Array3mapGRPB4JsonRPB4JsonEHRPC15error5Error(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS840,
  struct _M0TWRPB4JsonERPB4JsonQRPC15error5Error* _M0L1fS844
) {
  int32_t _M0L3lenS2393;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS839;
  int32_t _M0L7_2abindS841;
  int32_t _M0L1iS842;
  struct moonbit_result_2 _result_3656;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2393 = _M0L4selfS840->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS839 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2393);
  _M0L7_2abindS841 = _M0L4selfS840->$1;
  _M0L1iS842 = 0;
  while (1) {
    if (_M0L1iS842 < _M0L7_2abindS841) {
      void** _M0L8_2afieldS3221 = _M0L4selfS840->$0;
      void** _M0L3bufS2392 = _M0L8_2afieldS3221;
      void* _M0L6_2atmpS3220 = (void*)_M0L3bufS2392[_M0L1iS842];
      void* _M0L1vS843 = _M0L6_2atmpS3220;
      void** _M0L8_2afieldS3219 = _M0L3arrS839->$0;
      void** _M0L3bufS2387 = _M0L8_2afieldS3219;
      struct moonbit_result_1 _tmp_3654;
      void* _M0L6_2atmpS2388;
      void* _M0L6_2aoldS3218;
      int32_t _M0L6_2atmpS2391;
      moonbit_incref(_M0L3bufS2387);
      moonbit_incref(_M0L1fS844);
      moonbit_incref(_M0L1vS843);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _tmp_3654 = _M0L1fS844->code(_M0L1fS844, _M0L1vS843);
      if (_tmp_3654.tag) {
        void* const _M0L5_2aokS2389 = _tmp_3654.data.ok;
        _M0L6_2atmpS2388 = _M0L5_2aokS2389;
      } else {
        void* const _M0L6_2aerrS2390 = _tmp_3654.data.err;
        struct moonbit_result_2 _result_3655;
        moonbit_decref(_M0L3bufS2387);
        moonbit_decref(_M0L1fS844);
        moonbit_decref(_M0L4selfS840);
        moonbit_decref(_M0L3arrS839);
        _result_3655.tag = 0;
        _result_3655.data.err = _M0L6_2aerrS2390;
        return _result_3655;
      }
      _M0L6_2aoldS3218 = (void*)_M0L3bufS2387[_M0L1iS842];
      moonbit_decref(_M0L6_2aoldS3218);
      _M0L3bufS2387[_M0L1iS842] = _M0L6_2atmpS2388;
      moonbit_decref(_M0L3bufS2387);
      _M0L6_2atmpS2391 = _M0L1iS842 + 1;
      _M0L1iS842 = _M0L6_2atmpS2391;
      continue;
    } else {
      moonbit_decref(_M0L1fS844);
      moonbit_decref(_M0L4selfS840);
    }
    break;
  }
  _result_3656.tag = 1;
  _result_3656.data.ok = _M0L3arrS839;
  return _result_3656;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRPB4JsonRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS847,
  struct _M0TWRPB4JsonERPB4Json* _M0L1fS851
) {
  int32_t _M0L3lenS2398;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS846;
  int32_t _M0L7_2abindS848;
  int32_t _M0L1iS849;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2398 = _M0L4selfS847->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS846 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2398);
  _M0L7_2abindS848 = _M0L4selfS847->$1;
  _M0L1iS849 = 0;
  while (1) {
    if (_M0L1iS849 < _M0L7_2abindS848) {
      void** _M0L8_2afieldS3225 = _M0L4selfS847->$0;
      void** _M0L3bufS2397 = _M0L8_2afieldS3225;
      void* _M0L6_2atmpS3224 = (void*)_M0L3bufS2397[_M0L1iS849];
      void* _M0L1vS850 = _M0L6_2atmpS3224;
      void** _M0L8_2afieldS3223 = _M0L3arrS846->$0;
      void** _M0L3bufS2394 = _M0L8_2afieldS3223;
      void* _M0L6_2atmpS2395;
      void* _M0L6_2aoldS3222;
      int32_t _M0L6_2atmpS2396;
      moonbit_incref(_M0L3bufS2394);
      moonbit_incref(_M0L1fS851);
      moonbit_incref(_M0L1vS850);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2395 = _M0L1fS851->code(_M0L1fS851, _M0L1vS850);
      _M0L6_2aoldS3222 = (void*)_M0L3bufS2394[_M0L1iS849];
      moonbit_decref(_M0L6_2aoldS3222);
      _M0L3bufS2394[_M0L1iS849] = _M0L6_2atmpS2395;
      moonbit_decref(_M0L3bufS2394);
      _M0L6_2atmpS2396 = _M0L1iS849 + 1;
      _M0L1iS849 = _M0L6_2atmpS2396;
      continue;
    } else {
      moonbit_decref(_M0L1fS851);
      moonbit_decref(_M0L4selfS847);
    }
    break;
  }
  return _M0L3arrS846;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS838) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS838;
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS837) {
  void* _block_3658;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3658 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3658)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3658)->$0 = _M0L6objectS837;
  return _block_3658;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS836) {
  void* _block_3659;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3659 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3659)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3659)->$0 = _M0L6stringS836;
  return _block_3659;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS829
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3226;
  int32_t _M0L6_2acntS3504;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2386;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS828;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__* _closure_3660;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2381;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3226 = _M0L4selfS829->$5;
  _M0L6_2acntS3504 = Moonbit_object_header(_M0L4selfS829)->rc;
  if (_M0L6_2acntS3504 > 1) {
    int32_t _M0L11_2anew__cntS3506 = _M0L6_2acntS3504 - 1;
    Moonbit_object_header(_M0L4selfS829)->rc = _M0L11_2anew__cntS3506;
    if (_M0L8_2afieldS3226) {
      moonbit_incref(_M0L8_2afieldS3226);
    }
  } else if (_M0L6_2acntS3504 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3505 = _M0L4selfS829->$0;
    moonbit_decref(_M0L8_2afieldS3505);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS829);
  }
  _M0L4headS2386 = _M0L8_2afieldS3226;
  _M0L11curr__entryS828
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS828)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS828->$0 = _M0L4headS2386;
  _closure_3660
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__));
  Moonbit_object_header(_closure_3660)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__, $0) >> 2, 1, 0);
  _closure_3660->code = &_M0MPB3Map4iterGsRPB4JsonEC2382l591;
  _closure_3660->$0 = _M0L11curr__entryS828;
  _M0L6_2atmpS2381 = (struct _M0TWEOUsRPB4JsonE*)_closure_3660;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2381);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2382l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2383
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__* _M0L14_2acasted__envS2384;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3232;
  int32_t _M0L6_2acntS3507;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS828;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3231;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS830;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2384
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2382__l591__*)_M0L6_2aenvS2383;
  _M0L8_2afieldS3232 = _M0L14_2acasted__envS2384->$0;
  _M0L6_2acntS3507 = Moonbit_object_header(_M0L14_2acasted__envS2384)->rc;
  if (_M0L6_2acntS3507 > 1) {
    int32_t _M0L11_2anew__cntS3508 = _M0L6_2acntS3507 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2384)->rc
    = _M0L11_2anew__cntS3508;
    moonbit_incref(_M0L8_2afieldS3232);
  } else if (_M0L6_2acntS3507 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2384);
  }
  _M0L11curr__entryS828 = _M0L8_2afieldS3232;
  _M0L8_2afieldS3231 = _M0L11curr__entryS828->$0;
  _M0L7_2abindS830 = _M0L8_2afieldS3231;
  if (_M0L7_2abindS830 == 0) {
    moonbit_decref(_M0L11curr__entryS828);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS831 = _M0L7_2abindS830;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS832 = _M0L7_2aSomeS831;
    moonbit_string_t _M0L8_2afieldS3230 = _M0L4_2axS832->$4;
    moonbit_string_t _M0L6_2akeyS833 = _M0L8_2afieldS3230;
    void* _M0L8_2afieldS3229 = _M0L4_2axS832->$5;
    void* _M0L8_2avalueS834 = _M0L8_2afieldS3229;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3228 = _M0L4_2axS832->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS835 = _M0L8_2afieldS3228;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3227 =
      _M0L11curr__entryS828->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2385;
    if (_M0L7_2anextS835) {
      moonbit_incref(_M0L7_2anextS835);
    }
    moonbit_incref(_M0L8_2avalueS834);
    moonbit_incref(_M0L6_2akeyS833);
    if (_M0L6_2aoldS3227) {
      moonbit_decref(_M0L6_2aoldS3227);
    }
    _M0L11curr__entryS828->$0 = _M0L7_2anextS835;
    moonbit_decref(_M0L11curr__entryS828);
    _M0L8_2atupleS2385
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2385)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2385->$0 = _M0L6_2akeyS833;
    _M0L8_2atupleS2385->$1 = _M0L8_2avalueS834;
    return _M0L8_2atupleS2385;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS827
) {
  int32_t _M0L8_2afieldS3233;
  int32_t _M0L4sizeS2380;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3233 = _M0L4selfS827->$1;
  moonbit_decref(_M0L4selfS827);
  _M0L4sizeS2380 = _M0L8_2afieldS3233;
  return _M0L4sizeS2380 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS805,
  int32_t _M0L3keyS801
) {
  int32_t _M0L4hashS800;
  int32_t _M0L14capacity__maskS2351;
  int32_t _M0L6_2atmpS2350;
  int32_t _M0L1iS802;
  int32_t _M0L3idxS803;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS800 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS801);
  _M0L14capacity__maskS2351 = _M0L4selfS805->$3;
  _M0L6_2atmpS2350 = _M0L4hashS800 & _M0L14capacity__maskS2351;
  _M0L1iS802 = 0;
  _M0L3idxS803 = _M0L6_2atmpS2350;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3237 =
      _M0L4selfS805->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2349 =
      _M0L8_2afieldS3237;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3236;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS804;
    if (
      _M0L3idxS803 < 0
      || _M0L3idxS803 >= Moonbit_array_length(_M0L7entriesS2349)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3236
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2349[
        _M0L3idxS803
      ];
    _M0L7_2abindS804 = _M0L6_2atmpS3236;
    if (_M0L7_2abindS804 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2338;
      if (_M0L7_2abindS804) {
        moonbit_incref(_M0L7_2abindS804);
      }
      moonbit_decref(_M0L4selfS805);
      if (_M0L7_2abindS804) {
        moonbit_decref(_M0L7_2abindS804);
      }
      _M0L6_2atmpS2338 = 0;
      return _M0L6_2atmpS2338;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS806 =
        _M0L7_2abindS804;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS807 =
        _M0L7_2aSomeS806;
      int32_t _M0L4hashS2340 = _M0L8_2aentryS807->$3;
      int32_t _if__result_3662;
      int32_t _M0L8_2afieldS3234;
      int32_t _M0L3pslS2343;
      int32_t _M0L6_2atmpS2345;
      int32_t _M0L6_2atmpS2347;
      int32_t _M0L14capacity__maskS2348;
      int32_t _M0L6_2atmpS2346;
      if (_M0L4hashS2340 == _M0L4hashS800) {
        int32_t _M0L3keyS2339 = _M0L8_2aentryS807->$4;
        _if__result_3662 = _M0L3keyS2339 == _M0L3keyS801;
      } else {
        _if__result_3662 = 0;
      }
      if (_if__result_3662) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3235;
        int32_t _M0L6_2acntS3509;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2342;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2341;
        moonbit_incref(_M0L8_2aentryS807);
        moonbit_decref(_M0L4selfS805);
        _M0L8_2afieldS3235 = _M0L8_2aentryS807->$5;
        _M0L6_2acntS3509 = Moonbit_object_header(_M0L8_2aentryS807)->rc;
        if (_M0L6_2acntS3509 > 1) {
          int32_t _M0L11_2anew__cntS3511 = _M0L6_2acntS3509 - 1;
          Moonbit_object_header(_M0L8_2aentryS807)->rc
          = _M0L11_2anew__cntS3511;
          moonbit_incref(_M0L8_2afieldS3235);
        } else if (_M0L6_2acntS3509 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3510 =
            _M0L8_2aentryS807->$1;
          if (_M0L8_2afieldS3510) {
            moonbit_decref(_M0L8_2afieldS3510);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS807);
        }
        _M0L5valueS2342 = _M0L8_2afieldS3235;
        _M0L6_2atmpS2341 = _M0L5valueS2342;
        return _M0L6_2atmpS2341;
      } else {
        moonbit_incref(_M0L8_2aentryS807);
      }
      _M0L8_2afieldS3234 = _M0L8_2aentryS807->$2;
      moonbit_decref(_M0L8_2aentryS807);
      _M0L3pslS2343 = _M0L8_2afieldS3234;
      if (_M0L1iS802 > _M0L3pslS2343) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2344;
        moonbit_decref(_M0L4selfS805);
        _M0L6_2atmpS2344 = 0;
        return _M0L6_2atmpS2344;
      }
      _M0L6_2atmpS2345 = _M0L1iS802 + 1;
      _M0L6_2atmpS2347 = _M0L3idxS803 + 1;
      _M0L14capacity__maskS2348 = _M0L4selfS805->$3;
      _M0L6_2atmpS2346 = _M0L6_2atmpS2347 & _M0L14capacity__maskS2348;
      _M0L1iS802 = _M0L6_2atmpS2345;
      _M0L3idxS803 = _M0L6_2atmpS2346;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS814,
  moonbit_string_t _M0L3keyS810
) {
  int32_t _M0L4hashS809;
  int32_t _M0L14capacity__maskS2365;
  int32_t _M0L6_2atmpS2364;
  int32_t _M0L1iS811;
  int32_t _M0L3idxS812;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS810);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS809 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS810);
  _M0L14capacity__maskS2365 = _M0L4selfS814->$3;
  _M0L6_2atmpS2364 = _M0L4hashS809 & _M0L14capacity__maskS2365;
  _M0L1iS811 = 0;
  _M0L3idxS812 = _M0L6_2atmpS2364;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3243 =
      _M0L4selfS814->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2363 =
      _M0L8_2afieldS3243;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3242;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS813;
    if (
      _M0L3idxS812 < 0
      || _M0L3idxS812 >= Moonbit_array_length(_M0L7entriesS2363)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3242
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2363[
        _M0L3idxS812
      ];
    _M0L7_2abindS813 = _M0L6_2atmpS3242;
    if (_M0L7_2abindS813 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2352;
      if (_M0L7_2abindS813) {
        moonbit_incref(_M0L7_2abindS813);
      }
      moonbit_decref(_M0L4selfS814);
      if (_M0L7_2abindS813) {
        moonbit_decref(_M0L7_2abindS813);
      }
      moonbit_decref(_M0L3keyS810);
      _M0L6_2atmpS2352 = 0;
      return _M0L6_2atmpS2352;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS815 =
        _M0L7_2abindS813;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS816 =
        _M0L7_2aSomeS815;
      int32_t _M0L4hashS2354 = _M0L8_2aentryS816->$3;
      int32_t _if__result_3664;
      int32_t _M0L8_2afieldS3238;
      int32_t _M0L3pslS2357;
      int32_t _M0L6_2atmpS2359;
      int32_t _M0L6_2atmpS2361;
      int32_t _M0L14capacity__maskS2362;
      int32_t _M0L6_2atmpS2360;
      if (_M0L4hashS2354 == _M0L4hashS809) {
        moonbit_string_t _M0L8_2afieldS3241 = _M0L8_2aentryS816->$4;
        moonbit_string_t _M0L3keyS2353 = _M0L8_2afieldS3241;
        int32_t _M0L6_2atmpS3240;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3240
        = moonbit_val_array_equal(_M0L3keyS2353, _M0L3keyS810);
        _if__result_3664 = _M0L6_2atmpS3240;
      } else {
        _if__result_3664 = 0;
      }
      if (_if__result_3664) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3239;
        int32_t _M0L6_2acntS3512;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2356;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2355;
        moonbit_incref(_M0L8_2aentryS816);
        moonbit_decref(_M0L4selfS814);
        moonbit_decref(_M0L3keyS810);
        _M0L8_2afieldS3239 = _M0L8_2aentryS816->$5;
        _M0L6_2acntS3512 = Moonbit_object_header(_M0L8_2aentryS816)->rc;
        if (_M0L6_2acntS3512 > 1) {
          int32_t _M0L11_2anew__cntS3515 = _M0L6_2acntS3512 - 1;
          Moonbit_object_header(_M0L8_2aentryS816)->rc
          = _M0L11_2anew__cntS3515;
          moonbit_incref(_M0L8_2afieldS3239);
        } else if (_M0L6_2acntS3512 == 1) {
          moonbit_string_t _M0L8_2afieldS3514 = _M0L8_2aentryS816->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3513;
          moonbit_decref(_M0L8_2afieldS3514);
          _M0L8_2afieldS3513 = _M0L8_2aentryS816->$1;
          if (_M0L8_2afieldS3513) {
            moonbit_decref(_M0L8_2afieldS3513);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS816);
        }
        _M0L5valueS2356 = _M0L8_2afieldS3239;
        _M0L6_2atmpS2355 = _M0L5valueS2356;
        return _M0L6_2atmpS2355;
      } else {
        moonbit_incref(_M0L8_2aentryS816);
      }
      _M0L8_2afieldS3238 = _M0L8_2aentryS816->$2;
      moonbit_decref(_M0L8_2aentryS816);
      _M0L3pslS2357 = _M0L8_2afieldS3238;
      if (_M0L1iS811 > _M0L3pslS2357) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2358;
        moonbit_decref(_M0L4selfS814);
        moonbit_decref(_M0L3keyS810);
        _M0L6_2atmpS2358 = 0;
        return _M0L6_2atmpS2358;
      }
      _M0L6_2atmpS2359 = _M0L1iS811 + 1;
      _M0L6_2atmpS2361 = _M0L3idxS812 + 1;
      _M0L14capacity__maskS2362 = _M0L4selfS814->$3;
      _M0L6_2atmpS2360 = _M0L6_2atmpS2361 & _M0L14capacity__maskS2362;
      _M0L1iS811 = _M0L6_2atmpS2359;
      _M0L3idxS812 = _M0L6_2atmpS2360;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS823,
  moonbit_string_t _M0L3keyS819
) {
  int32_t _M0L4hashS818;
  int32_t _M0L14capacity__maskS2379;
  int32_t _M0L6_2atmpS2378;
  int32_t _M0L1iS820;
  int32_t _M0L3idxS821;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS819);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS818 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS819);
  _M0L14capacity__maskS2379 = _M0L4selfS823->$3;
  _M0L6_2atmpS2378 = _M0L4hashS818 & _M0L14capacity__maskS2379;
  _M0L1iS820 = 0;
  _M0L3idxS821 = _M0L6_2atmpS2378;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3249 = _M0L4selfS823->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2377 = _M0L8_2afieldS3249;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3248;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS822;
    if (
      _M0L3idxS821 < 0
      || _M0L3idxS821 >= Moonbit_array_length(_M0L7entriesS2377)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3248
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2377[_M0L3idxS821];
    _M0L7_2abindS822 = _M0L6_2atmpS3248;
    if (_M0L7_2abindS822 == 0) {
      void* _M0L6_2atmpS2366;
      if (_M0L7_2abindS822) {
        moonbit_incref(_M0L7_2abindS822);
      }
      moonbit_decref(_M0L4selfS823);
      if (_M0L7_2abindS822) {
        moonbit_decref(_M0L7_2abindS822);
      }
      moonbit_decref(_M0L3keyS819);
      _M0L6_2atmpS2366 = 0;
      return _M0L6_2atmpS2366;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS824 = _M0L7_2abindS822;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aentryS825 = _M0L7_2aSomeS824;
      int32_t _M0L4hashS2368 = _M0L8_2aentryS825->$3;
      int32_t _if__result_3666;
      int32_t _M0L8_2afieldS3244;
      int32_t _M0L3pslS2371;
      int32_t _M0L6_2atmpS2373;
      int32_t _M0L6_2atmpS2375;
      int32_t _M0L14capacity__maskS2376;
      int32_t _M0L6_2atmpS2374;
      if (_M0L4hashS2368 == _M0L4hashS818) {
        moonbit_string_t _M0L8_2afieldS3247 = _M0L8_2aentryS825->$4;
        moonbit_string_t _M0L3keyS2367 = _M0L8_2afieldS3247;
        int32_t _M0L6_2atmpS3246;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3246
        = moonbit_val_array_equal(_M0L3keyS2367, _M0L3keyS819);
        _if__result_3666 = _M0L6_2atmpS3246;
      } else {
        _if__result_3666 = 0;
      }
      if (_if__result_3666) {
        void* _M0L8_2afieldS3245;
        int32_t _M0L6_2acntS3516;
        void* _M0L5valueS2370;
        void* _M0L6_2atmpS2369;
        moonbit_incref(_M0L8_2aentryS825);
        moonbit_decref(_M0L4selfS823);
        moonbit_decref(_M0L3keyS819);
        _M0L8_2afieldS3245 = _M0L8_2aentryS825->$5;
        _M0L6_2acntS3516 = Moonbit_object_header(_M0L8_2aentryS825)->rc;
        if (_M0L6_2acntS3516 > 1) {
          int32_t _M0L11_2anew__cntS3519 = _M0L6_2acntS3516 - 1;
          Moonbit_object_header(_M0L8_2aentryS825)->rc
          = _M0L11_2anew__cntS3519;
          moonbit_incref(_M0L8_2afieldS3245);
        } else if (_M0L6_2acntS3516 == 1) {
          moonbit_string_t _M0L8_2afieldS3518 = _M0L8_2aentryS825->$4;
          struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3517;
          moonbit_decref(_M0L8_2afieldS3518);
          _M0L8_2afieldS3517 = _M0L8_2aentryS825->$1;
          if (_M0L8_2afieldS3517) {
            moonbit_decref(_M0L8_2afieldS3517);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS825);
        }
        _M0L5valueS2370 = _M0L8_2afieldS3245;
        _M0L6_2atmpS2369 = _M0L5valueS2370;
        return _M0L6_2atmpS2369;
      } else {
        moonbit_incref(_M0L8_2aentryS825);
      }
      _M0L8_2afieldS3244 = _M0L8_2aentryS825->$2;
      moonbit_decref(_M0L8_2aentryS825);
      _M0L3pslS2371 = _M0L8_2afieldS3244;
      if (_M0L1iS820 > _M0L3pslS2371) {
        void* _M0L6_2atmpS2372;
        moonbit_decref(_M0L4selfS823);
        moonbit_decref(_M0L3keyS819);
        _M0L6_2atmpS2372 = 0;
        return _M0L6_2atmpS2372;
      }
      _M0L6_2atmpS2373 = _M0L1iS820 + 1;
      _M0L6_2atmpS2375 = _M0L3idxS821 + 1;
      _M0L14capacity__maskS2376 = _M0L4selfS823->$3;
      _M0L6_2atmpS2374 = _M0L6_2atmpS2375 & _M0L14capacity__maskS2376;
      _M0L1iS820 = _M0L6_2atmpS2373;
      _M0L3idxS821 = _M0L6_2atmpS2374;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS777
) {
  int32_t _M0L6lengthS776;
  int32_t _M0Lm8capacityS778;
  int32_t _M0L6_2atmpS2303;
  int32_t _M0L6_2atmpS2302;
  int32_t _M0L6_2atmpS2313;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS779;
  int32_t _M0L3endS2311;
  int32_t _M0L5startS2312;
  int32_t _M0L7_2abindS780;
  int32_t _M0L2__S781;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS777.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS776
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS777);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS778 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS776);
  _M0L6_2atmpS2303 = _M0Lm8capacityS778;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2302 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2303);
  if (_M0L6lengthS776 > _M0L6_2atmpS2302) {
    int32_t _M0L6_2atmpS2304 = _M0Lm8capacityS778;
    _M0Lm8capacityS778 = _M0L6_2atmpS2304 * 2;
  }
  _M0L6_2atmpS2313 = _M0Lm8capacityS778;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS779
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2313);
  _M0L3endS2311 = _M0L3arrS777.$2;
  _M0L5startS2312 = _M0L3arrS777.$1;
  _M0L7_2abindS780 = _M0L3endS2311 - _M0L5startS2312;
  _M0L2__S781 = 0;
  while (1) {
    if (_M0L2__S781 < _M0L7_2abindS780) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3253 =
        _M0L3arrS777.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2308 =
        _M0L8_2afieldS3253;
      int32_t _M0L5startS2310 = _M0L3arrS777.$1;
      int32_t _M0L6_2atmpS2309 = _M0L5startS2310 + _M0L2__S781;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3252 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2308[
          _M0L6_2atmpS2309
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS782 =
        _M0L6_2atmpS3252;
      moonbit_string_t _M0L8_2afieldS3251 = _M0L1eS782->$0;
      moonbit_string_t _M0L6_2atmpS2305 = _M0L8_2afieldS3251;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3250 =
        _M0L1eS782->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2306 =
        _M0L8_2afieldS3250;
      int32_t _M0L6_2atmpS2307;
      moonbit_incref(_M0L6_2atmpS2306);
      moonbit_incref(_M0L6_2atmpS2305);
      moonbit_incref(_M0L1mS779);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS779, _M0L6_2atmpS2305, _M0L6_2atmpS2306);
      _M0L6_2atmpS2307 = _M0L2__S781 + 1;
      _M0L2__S781 = _M0L6_2atmpS2307;
      continue;
    } else {
      moonbit_decref(_M0L3arrS777.$0);
    }
    break;
  }
  return _M0L1mS779;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS785
) {
  int32_t _M0L6lengthS784;
  int32_t _M0Lm8capacityS786;
  int32_t _M0L6_2atmpS2315;
  int32_t _M0L6_2atmpS2314;
  int32_t _M0L6_2atmpS2325;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS787;
  int32_t _M0L3endS2323;
  int32_t _M0L5startS2324;
  int32_t _M0L7_2abindS788;
  int32_t _M0L2__S789;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS785.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS784
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS785);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS786 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS784);
  _M0L6_2atmpS2315 = _M0Lm8capacityS786;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2314 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2315);
  if (_M0L6lengthS784 > _M0L6_2atmpS2314) {
    int32_t _M0L6_2atmpS2316 = _M0Lm8capacityS786;
    _M0Lm8capacityS786 = _M0L6_2atmpS2316 * 2;
  }
  _M0L6_2atmpS2325 = _M0Lm8capacityS786;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS787
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2325);
  _M0L3endS2323 = _M0L3arrS785.$2;
  _M0L5startS2324 = _M0L3arrS785.$1;
  _M0L7_2abindS788 = _M0L3endS2323 - _M0L5startS2324;
  _M0L2__S789 = 0;
  while (1) {
    if (_M0L2__S789 < _M0L7_2abindS788) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3256 =
        _M0L3arrS785.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2320 =
        _M0L8_2afieldS3256;
      int32_t _M0L5startS2322 = _M0L3arrS785.$1;
      int32_t _M0L6_2atmpS2321 = _M0L5startS2322 + _M0L2__S789;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3255 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2320[
          _M0L6_2atmpS2321
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS790 = _M0L6_2atmpS3255;
      int32_t _M0L6_2atmpS2317 = _M0L1eS790->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3254 =
        _M0L1eS790->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2318 =
        _M0L8_2afieldS3254;
      int32_t _M0L6_2atmpS2319;
      moonbit_incref(_M0L6_2atmpS2318);
      moonbit_incref(_M0L1mS787);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS787, _M0L6_2atmpS2317, _M0L6_2atmpS2318);
      _M0L6_2atmpS2319 = _M0L2__S789 + 1;
      _M0L2__S789 = _M0L6_2atmpS2319;
      continue;
    } else {
      moonbit_decref(_M0L3arrS785.$0);
    }
    break;
  }
  return _M0L1mS787;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS793
) {
  int32_t _M0L6lengthS792;
  int32_t _M0Lm8capacityS794;
  int32_t _M0L6_2atmpS2327;
  int32_t _M0L6_2atmpS2326;
  int32_t _M0L6_2atmpS2337;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS795;
  int32_t _M0L3endS2335;
  int32_t _M0L5startS2336;
  int32_t _M0L7_2abindS796;
  int32_t _M0L2__S797;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS793.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS792 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS793);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS794 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS792);
  _M0L6_2atmpS2327 = _M0Lm8capacityS794;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2326 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2327);
  if (_M0L6lengthS792 > _M0L6_2atmpS2326) {
    int32_t _M0L6_2atmpS2328 = _M0Lm8capacityS794;
    _M0Lm8capacityS794 = _M0L6_2atmpS2328 * 2;
  }
  _M0L6_2atmpS2337 = _M0Lm8capacityS794;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS795 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2337);
  _M0L3endS2335 = _M0L3arrS793.$2;
  _M0L5startS2336 = _M0L3arrS793.$1;
  _M0L7_2abindS796 = _M0L3endS2335 - _M0L5startS2336;
  _M0L2__S797 = 0;
  while (1) {
    if (_M0L2__S797 < _M0L7_2abindS796) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS3260 = _M0L3arrS793.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2332 = _M0L8_2afieldS3260;
      int32_t _M0L5startS2334 = _M0L3arrS793.$1;
      int32_t _M0L6_2atmpS2333 = _M0L5startS2334 + _M0L2__S797;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS3259 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2332[_M0L6_2atmpS2333];
      struct _M0TUsRPB4JsonE* _M0L1eS798 = _M0L6_2atmpS3259;
      moonbit_string_t _M0L8_2afieldS3258 = _M0L1eS798->$0;
      moonbit_string_t _M0L6_2atmpS2329 = _M0L8_2afieldS3258;
      void* _M0L8_2afieldS3257 = _M0L1eS798->$1;
      void* _M0L6_2atmpS2330 = _M0L8_2afieldS3257;
      int32_t _M0L6_2atmpS2331;
      moonbit_incref(_M0L6_2atmpS2330);
      moonbit_incref(_M0L6_2atmpS2329);
      moonbit_incref(_M0L1mS795);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS795, _M0L6_2atmpS2329, _M0L6_2atmpS2330);
      _M0L6_2atmpS2331 = _M0L2__S797 + 1;
      _M0L2__S797 = _M0L6_2atmpS2331;
      continue;
    } else {
      moonbit_decref(_M0L3arrS793.$0);
    }
    break;
  }
  return _M0L1mS795;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS767,
  moonbit_string_t _M0L3keyS768,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS769
) {
  int32_t _M0L6_2atmpS2299;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS768);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2299 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS768);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS767, _M0L3keyS768, _M0L5valueS769, _M0L6_2atmpS2299);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS770,
  int32_t _M0L3keyS771,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS772
) {
  int32_t _M0L6_2atmpS2300;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2300 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS771);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS770, _M0L3keyS771, _M0L5valueS772, _M0L6_2atmpS2300);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS773,
  moonbit_string_t _M0L3keyS774,
  void* _M0L5valueS775
) {
  int32_t _M0L6_2atmpS2301;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS774);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2301 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS774);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS773, _M0L3keyS774, _M0L5valueS775, _M0L6_2atmpS2301);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS735
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3267;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS734;
  int32_t _M0L8capacityS2284;
  int32_t _M0L13new__capacityS736;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2279;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2278;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3266;
  int32_t _M0L6_2atmpS2280;
  int32_t _M0L8capacityS2282;
  int32_t _M0L6_2atmpS2281;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2283;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3265;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS737;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3267 = _M0L4selfS735->$5;
  _M0L9old__headS734 = _M0L8_2afieldS3267;
  _M0L8capacityS2284 = _M0L4selfS735->$2;
  _M0L13new__capacityS736 = _M0L8capacityS2284 << 1;
  _M0L6_2atmpS2279 = 0;
  _M0L6_2atmpS2278
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS736, _M0L6_2atmpS2279);
  _M0L6_2aoldS3266 = _M0L4selfS735->$0;
  if (_M0L9old__headS734) {
    moonbit_incref(_M0L9old__headS734);
  }
  moonbit_decref(_M0L6_2aoldS3266);
  _M0L4selfS735->$0 = _M0L6_2atmpS2278;
  _M0L4selfS735->$2 = _M0L13new__capacityS736;
  _M0L6_2atmpS2280 = _M0L13new__capacityS736 - 1;
  _M0L4selfS735->$3 = _M0L6_2atmpS2280;
  _M0L8capacityS2282 = _M0L4selfS735->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2281 = _M0FPB21calc__grow__threshold(_M0L8capacityS2282);
  _M0L4selfS735->$4 = _M0L6_2atmpS2281;
  _M0L4selfS735->$1 = 0;
  _M0L6_2atmpS2283 = 0;
  _M0L6_2aoldS3265 = _M0L4selfS735->$5;
  if (_M0L6_2aoldS3265) {
    moonbit_decref(_M0L6_2aoldS3265);
  }
  _M0L4selfS735->$5 = _M0L6_2atmpS2283;
  _M0L4selfS735->$6 = -1;
  _M0L8_2aparamS737 = _M0L9old__headS734;
  while (1) {
    if (_M0L8_2aparamS737 == 0) {
      if (_M0L8_2aparamS737) {
        moonbit_decref(_M0L8_2aparamS737);
      }
      moonbit_decref(_M0L4selfS735);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS738 =
        _M0L8_2aparamS737;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS739 =
        _M0L7_2aSomeS738;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3264 =
        _M0L4_2axS739->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS740 =
        _M0L8_2afieldS3264;
      moonbit_string_t _M0L8_2afieldS3263 = _M0L4_2axS739->$4;
      moonbit_string_t _M0L6_2akeyS741 = _M0L8_2afieldS3263;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3262 =
        _M0L4_2axS739->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS742 =
        _M0L8_2afieldS3262;
      int32_t _M0L8_2afieldS3261 = _M0L4_2axS739->$3;
      int32_t _M0L6_2acntS3520 = Moonbit_object_header(_M0L4_2axS739)->rc;
      int32_t _M0L7_2ahashS743;
      if (_M0L6_2acntS3520 > 1) {
        int32_t _M0L11_2anew__cntS3521 = _M0L6_2acntS3520 - 1;
        Moonbit_object_header(_M0L4_2axS739)->rc = _M0L11_2anew__cntS3521;
        moonbit_incref(_M0L8_2avalueS742);
        moonbit_incref(_M0L6_2akeyS741);
        if (_M0L7_2anextS740) {
          moonbit_incref(_M0L7_2anextS740);
        }
      } else if (_M0L6_2acntS3520 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS739);
      }
      _M0L7_2ahashS743 = _M0L8_2afieldS3261;
      moonbit_incref(_M0L4selfS735);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS735, _M0L6_2akeyS741, _M0L8_2avalueS742, _M0L7_2ahashS743);
      _M0L8_2aparamS737 = _M0L7_2anextS740;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS746
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3273;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS745;
  int32_t _M0L8capacityS2291;
  int32_t _M0L13new__capacityS747;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2286;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2285;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3272;
  int32_t _M0L6_2atmpS2287;
  int32_t _M0L8capacityS2289;
  int32_t _M0L6_2atmpS2288;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2290;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3271;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS748;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3273 = _M0L4selfS746->$5;
  _M0L9old__headS745 = _M0L8_2afieldS3273;
  _M0L8capacityS2291 = _M0L4selfS746->$2;
  _M0L13new__capacityS747 = _M0L8capacityS2291 << 1;
  _M0L6_2atmpS2286 = 0;
  _M0L6_2atmpS2285
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS747, _M0L6_2atmpS2286);
  _M0L6_2aoldS3272 = _M0L4selfS746->$0;
  if (_M0L9old__headS745) {
    moonbit_incref(_M0L9old__headS745);
  }
  moonbit_decref(_M0L6_2aoldS3272);
  _M0L4selfS746->$0 = _M0L6_2atmpS2285;
  _M0L4selfS746->$2 = _M0L13new__capacityS747;
  _M0L6_2atmpS2287 = _M0L13new__capacityS747 - 1;
  _M0L4selfS746->$3 = _M0L6_2atmpS2287;
  _M0L8capacityS2289 = _M0L4selfS746->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2288 = _M0FPB21calc__grow__threshold(_M0L8capacityS2289);
  _M0L4selfS746->$4 = _M0L6_2atmpS2288;
  _M0L4selfS746->$1 = 0;
  _M0L6_2atmpS2290 = 0;
  _M0L6_2aoldS3271 = _M0L4selfS746->$5;
  if (_M0L6_2aoldS3271) {
    moonbit_decref(_M0L6_2aoldS3271);
  }
  _M0L4selfS746->$5 = _M0L6_2atmpS2290;
  _M0L4selfS746->$6 = -1;
  _M0L8_2aparamS748 = _M0L9old__headS745;
  while (1) {
    if (_M0L8_2aparamS748 == 0) {
      if (_M0L8_2aparamS748) {
        moonbit_decref(_M0L8_2aparamS748);
      }
      moonbit_decref(_M0L4selfS746);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS749 =
        _M0L8_2aparamS748;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS750 =
        _M0L7_2aSomeS749;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3270 =
        _M0L4_2axS750->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS751 =
        _M0L8_2afieldS3270;
      int32_t _M0L6_2akeyS752 = _M0L4_2axS750->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3269 =
        _M0L4_2axS750->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS753 =
        _M0L8_2afieldS3269;
      int32_t _M0L8_2afieldS3268 = _M0L4_2axS750->$3;
      int32_t _M0L6_2acntS3522 = Moonbit_object_header(_M0L4_2axS750)->rc;
      int32_t _M0L7_2ahashS754;
      if (_M0L6_2acntS3522 > 1) {
        int32_t _M0L11_2anew__cntS3523 = _M0L6_2acntS3522 - 1;
        Moonbit_object_header(_M0L4_2axS750)->rc = _M0L11_2anew__cntS3523;
        moonbit_incref(_M0L8_2avalueS753);
        if (_M0L7_2anextS751) {
          moonbit_incref(_M0L7_2anextS751);
        }
      } else if (_M0L6_2acntS3522 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS750);
      }
      _M0L7_2ahashS754 = _M0L8_2afieldS3268;
      moonbit_incref(_M0L4selfS746);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS746, _M0L6_2akeyS752, _M0L8_2avalueS753, _M0L7_2ahashS754);
      _M0L8_2aparamS748 = _M0L7_2anextS751;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS757
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3280;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS756;
  int32_t _M0L8capacityS2298;
  int32_t _M0L13new__capacityS758;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2293;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2292;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS3279;
  int32_t _M0L6_2atmpS2294;
  int32_t _M0L8capacityS2296;
  int32_t _M0L6_2atmpS2295;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2297;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3278;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS759;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3280 = _M0L4selfS757->$5;
  _M0L9old__headS756 = _M0L8_2afieldS3280;
  _M0L8capacityS2298 = _M0L4selfS757->$2;
  _M0L13new__capacityS758 = _M0L8capacityS2298 << 1;
  _M0L6_2atmpS2293 = 0;
  _M0L6_2atmpS2292
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS758, _M0L6_2atmpS2293);
  _M0L6_2aoldS3279 = _M0L4selfS757->$0;
  if (_M0L9old__headS756) {
    moonbit_incref(_M0L9old__headS756);
  }
  moonbit_decref(_M0L6_2aoldS3279);
  _M0L4selfS757->$0 = _M0L6_2atmpS2292;
  _M0L4selfS757->$2 = _M0L13new__capacityS758;
  _M0L6_2atmpS2294 = _M0L13new__capacityS758 - 1;
  _M0L4selfS757->$3 = _M0L6_2atmpS2294;
  _M0L8capacityS2296 = _M0L4selfS757->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2295 = _M0FPB21calc__grow__threshold(_M0L8capacityS2296);
  _M0L4selfS757->$4 = _M0L6_2atmpS2295;
  _M0L4selfS757->$1 = 0;
  _M0L6_2atmpS2297 = 0;
  _M0L6_2aoldS3278 = _M0L4selfS757->$5;
  if (_M0L6_2aoldS3278) {
    moonbit_decref(_M0L6_2aoldS3278);
  }
  _M0L4selfS757->$5 = _M0L6_2atmpS2297;
  _M0L4selfS757->$6 = -1;
  _M0L8_2aparamS759 = _M0L9old__headS756;
  while (1) {
    if (_M0L8_2aparamS759 == 0) {
      if (_M0L8_2aparamS759) {
        moonbit_decref(_M0L8_2aparamS759);
      }
      moonbit_decref(_M0L4selfS757);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS760 = _M0L8_2aparamS759;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS761 = _M0L7_2aSomeS760;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3277 = _M0L4_2axS761->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS762 = _M0L8_2afieldS3277;
      moonbit_string_t _M0L8_2afieldS3276 = _M0L4_2axS761->$4;
      moonbit_string_t _M0L6_2akeyS763 = _M0L8_2afieldS3276;
      void* _M0L8_2afieldS3275 = _M0L4_2axS761->$5;
      void* _M0L8_2avalueS764 = _M0L8_2afieldS3275;
      int32_t _M0L8_2afieldS3274 = _M0L4_2axS761->$3;
      int32_t _M0L6_2acntS3524 = Moonbit_object_header(_M0L4_2axS761)->rc;
      int32_t _M0L7_2ahashS765;
      if (_M0L6_2acntS3524 > 1) {
        int32_t _M0L11_2anew__cntS3525 = _M0L6_2acntS3524 - 1;
        Moonbit_object_header(_M0L4_2axS761)->rc = _M0L11_2anew__cntS3525;
        moonbit_incref(_M0L8_2avalueS764);
        moonbit_incref(_M0L6_2akeyS763);
        if (_M0L7_2anextS762) {
          moonbit_incref(_M0L7_2anextS762);
        }
      } else if (_M0L6_2acntS3524 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS761);
      }
      _M0L7_2ahashS765 = _M0L8_2afieldS3274;
      moonbit_incref(_M0L4selfS757);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS757, _M0L6_2akeyS763, _M0L8_2avalueS764, _M0L7_2ahashS765);
      _M0L8_2aparamS759 = _M0L7_2anextS762;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS689,
  moonbit_string_t _M0L3keyS695,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS696,
  int32_t _M0L4hashS691
) {
  int32_t _M0L14capacity__maskS2241;
  int32_t _M0L6_2atmpS2240;
  int32_t _M0L3pslS686;
  int32_t _M0L3idxS687;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2241 = _M0L4selfS689->$3;
  _M0L6_2atmpS2240 = _M0L4hashS691 & _M0L14capacity__maskS2241;
  _M0L3pslS686 = 0;
  _M0L3idxS687 = _M0L6_2atmpS2240;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3285 =
      _M0L4selfS689->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2239 =
      _M0L8_2afieldS3285;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3284;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS688;
    if (
      _M0L3idxS687 < 0
      || _M0L3idxS687 >= Moonbit_array_length(_M0L7entriesS2239)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3284
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2239[
        _M0L3idxS687
      ];
    _M0L7_2abindS688 = _M0L6_2atmpS3284;
    if (_M0L7_2abindS688 == 0) {
      int32_t _M0L4sizeS2224 = _M0L4selfS689->$1;
      int32_t _M0L8grow__atS2225 = _M0L4selfS689->$4;
      int32_t _M0L7_2abindS692;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS693;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS694;
      if (_M0L4sizeS2224 >= _M0L8grow__atS2225) {
        int32_t _M0L14capacity__maskS2227;
        int32_t _M0L6_2atmpS2226;
        moonbit_incref(_M0L4selfS689);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689);
        _M0L14capacity__maskS2227 = _M0L4selfS689->$3;
        _M0L6_2atmpS2226 = _M0L4hashS691 & _M0L14capacity__maskS2227;
        _M0L3pslS686 = 0;
        _M0L3idxS687 = _M0L6_2atmpS2226;
        continue;
      }
      _M0L7_2abindS692 = _M0L4selfS689->$6;
      _M0L7_2abindS693 = 0;
      _M0L5entryS694
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS694)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS694->$0 = _M0L7_2abindS692;
      _M0L5entryS694->$1 = _M0L7_2abindS693;
      _M0L5entryS694->$2 = _M0L3pslS686;
      _M0L5entryS694->$3 = _M0L4hashS691;
      _M0L5entryS694->$4 = _M0L3keyS695;
      _M0L5entryS694->$5 = _M0L5valueS696;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689, _M0L3idxS687, _M0L5entryS694);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS697 =
        _M0L7_2abindS688;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS698 =
        _M0L7_2aSomeS697;
      int32_t _M0L4hashS2229 = _M0L14_2acurr__entryS698->$3;
      int32_t _if__result_3674;
      int32_t _M0L3pslS2230;
      int32_t _M0L6_2atmpS2235;
      int32_t _M0L6_2atmpS2237;
      int32_t _M0L14capacity__maskS2238;
      int32_t _M0L6_2atmpS2236;
      if (_M0L4hashS2229 == _M0L4hashS691) {
        moonbit_string_t _M0L8_2afieldS3283 = _M0L14_2acurr__entryS698->$4;
        moonbit_string_t _M0L3keyS2228 = _M0L8_2afieldS3283;
        int32_t _M0L6_2atmpS3282;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3282
        = moonbit_val_array_equal(_M0L3keyS2228, _M0L3keyS695);
        _if__result_3674 = _M0L6_2atmpS3282;
      } else {
        _if__result_3674 = 0;
      }
      if (_if__result_3674) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3281;
        moonbit_incref(_M0L14_2acurr__entryS698);
        moonbit_decref(_M0L3keyS695);
        moonbit_decref(_M0L4selfS689);
        _M0L6_2aoldS3281 = _M0L14_2acurr__entryS698->$5;
        moonbit_decref(_M0L6_2aoldS3281);
        _M0L14_2acurr__entryS698->$5 = _M0L5valueS696;
        moonbit_decref(_M0L14_2acurr__entryS698);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS698);
      }
      _M0L3pslS2230 = _M0L14_2acurr__entryS698->$2;
      if (_M0L3pslS686 > _M0L3pslS2230) {
        int32_t _M0L4sizeS2231 = _M0L4selfS689->$1;
        int32_t _M0L8grow__atS2232 = _M0L4selfS689->$4;
        int32_t _M0L7_2abindS699;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS700;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS701;
        if (_M0L4sizeS2231 >= _M0L8grow__atS2232) {
          int32_t _M0L14capacity__maskS2234;
          int32_t _M0L6_2atmpS2233;
          moonbit_decref(_M0L14_2acurr__entryS698);
          moonbit_incref(_M0L4selfS689);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689);
          _M0L14capacity__maskS2234 = _M0L4selfS689->$3;
          _M0L6_2atmpS2233 = _M0L4hashS691 & _M0L14capacity__maskS2234;
          _M0L3pslS686 = 0;
          _M0L3idxS687 = _M0L6_2atmpS2233;
          continue;
        }
        moonbit_incref(_M0L4selfS689);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689, _M0L3idxS687, _M0L14_2acurr__entryS698);
        _M0L7_2abindS699 = _M0L4selfS689->$6;
        _M0L7_2abindS700 = 0;
        _M0L5entryS701
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS701)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS701->$0 = _M0L7_2abindS699;
        _M0L5entryS701->$1 = _M0L7_2abindS700;
        _M0L5entryS701->$2 = _M0L3pslS686;
        _M0L5entryS701->$3 = _M0L4hashS691;
        _M0L5entryS701->$4 = _M0L3keyS695;
        _M0L5entryS701->$5 = _M0L5valueS696;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689, _M0L3idxS687, _M0L5entryS701);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS698);
      }
      _M0L6_2atmpS2235 = _M0L3pslS686 + 1;
      _M0L6_2atmpS2237 = _M0L3idxS687 + 1;
      _M0L14capacity__maskS2238 = _M0L4selfS689->$3;
      _M0L6_2atmpS2236 = _M0L6_2atmpS2237 & _M0L14capacity__maskS2238;
      _M0L3pslS686 = _M0L6_2atmpS2235;
      _M0L3idxS687 = _M0L6_2atmpS2236;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS705,
  int32_t _M0L3keyS711,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS712,
  int32_t _M0L4hashS707
) {
  int32_t _M0L14capacity__maskS2259;
  int32_t _M0L6_2atmpS2258;
  int32_t _M0L3pslS702;
  int32_t _M0L3idxS703;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2259 = _M0L4selfS705->$3;
  _M0L6_2atmpS2258 = _M0L4hashS707 & _M0L14capacity__maskS2259;
  _M0L3pslS702 = 0;
  _M0L3idxS703 = _M0L6_2atmpS2258;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3288 =
      _M0L4selfS705->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2257 =
      _M0L8_2afieldS3288;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3287;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS704;
    if (
      _M0L3idxS703 < 0
      || _M0L3idxS703 >= Moonbit_array_length(_M0L7entriesS2257)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3287
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2257[
        _M0L3idxS703
      ];
    _M0L7_2abindS704 = _M0L6_2atmpS3287;
    if (_M0L7_2abindS704 == 0) {
      int32_t _M0L4sizeS2242 = _M0L4selfS705->$1;
      int32_t _M0L8grow__atS2243 = _M0L4selfS705->$4;
      int32_t _M0L7_2abindS708;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS709;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS710;
      if (_M0L4sizeS2242 >= _M0L8grow__atS2243) {
        int32_t _M0L14capacity__maskS2245;
        int32_t _M0L6_2atmpS2244;
        moonbit_incref(_M0L4selfS705);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705);
        _M0L14capacity__maskS2245 = _M0L4selfS705->$3;
        _M0L6_2atmpS2244 = _M0L4hashS707 & _M0L14capacity__maskS2245;
        _M0L3pslS702 = 0;
        _M0L3idxS703 = _M0L6_2atmpS2244;
        continue;
      }
      _M0L7_2abindS708 = _M0L4selfS705->$6;
      _M0L7_2abindS709 = 0;
      _M0L5entryS710
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS710)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS710->$0 = _M0L7_2abindS708;
      _M0L5entryS710->$1 = _M0L7_2abindS709;
      _M0L5entryS710->$2 = _M0L3pslS702;
      _M0L5entryS710->$3 = _M0L4hashS707;
      _M0L5entryS710->$4 = _M0L3keyS711;
      _M0L5entryS710->$5 = _M0L5valueS712;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705, _M0L3idxS703, _M0L5entryS710);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS713 =
        _M0L7_2abindS704;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS714 =
        _M0L7_2aSomeS713;
      int32_t _M0L4hashS2247 = _M0L14_2acurr__entryS714->$3;
      int32_t _if__result_3676;
      int32_t _M0L3pslS2248;
      int32_t _M0L6_2atmpS2253;
      int32_t _M0L6_2atmpS2255;
      int32_t _M0L14capacity__maskS2256;
      int32_t _M0L6_2atmpS2254;
      if (_M0L4hashS2247 == _M0L4hashS707) {
        int32_t _M0L3keyS2246 = _M0L14_2acurr__entryS714->$4;
        _if__result_3676 = _M0L3keyS2246 == _M0L3keyS711;
      } else {
        _if__result_3676 = 0;
      }
      if (_if__result_3676) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3286;
        moonbit_incref(_M0L14_2acurr__entryS714);
        moonbit_decref(_M0L4selfS705);
        _M0L6_2aoldS3286 = _M0L14_2acurr__entryS714->$5;
        moonbit_decref(_M0L6_2aoldS3286);
        _M0L14_2acurr__entryS714->$5 = _M0L5valueS712;
        moonbit_decref(_M0L14_2acurr__entryS714);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS714);
      }
      _M0L3pslS2248 = _M0L14_2acurr__entryS714->$2;
      if (_M0L3pslS702 > _M0L3pslS2248) {
        int32_t _M0L4sizeS2249 = _M0L4selfS705->$1;
        int32_t _M0L8grow__atS2250 = _M0L4selfS705->$4;
        int32_t _M0L7_2abindS715;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS716;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS717;
        if (_M0L4sizeS2249 >= _M0L8grow__atS2250) {
          int32_t _M0L14capacity__maskS2252;
          int32_t _M0L6_2atmpS2251;
          moonbit_decref(_M0L14_2acurr__entryS714);
          moonbit_incref(_M0L4selfS705);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705);
          _M0L14capacity__maskS2252 = _M0L4selfS705->$3;
          _M0L6_2atmpS2251 = _M0L4hashS707 & _M0L14capacity__maskS2252;
          _M0L3pslS702 = 0;
          _M0L3idxS703 = _M0L6_2atmpS2251;
          continue;
        }
        moonbit_incref(_M0L4selfS705);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705, _M0L3idxS703, _M0L14_2acurr__entryS714);
        _M0L7_2abindS715 = _M0L4selfS705->$6;
        _M0L7_2abindS716 = 0;
        _M0L5entryS717
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS717)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS717->$0 = _M0L7_2abindS715;
        _M0L5entryS717->$1 = _M0L7_2abindS716;
        _M0L5entryS717->$2 = _M0L3pslS702;
        _M0L5entryS717->$3 = _M0L4hashS707;
        _M0L5entryS717->$4 = _M0L3keyS711;
        _M0L5entryS717->$5 = _M0L5valueS712;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705, _M0L3idxS703, _M0L5entryS717);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS714);
      }
      _M0L6_2atmpS2253 = _M0L3pslS702 + 1;
      _M0L6_2atmpS2255 = _M0L3idxS703 + 1;
      _M0L14capacity__maskS2256 = _M0L4selfS705->$3;
      _M0L6_2atmpS2254 = _M0L6_2atmpS2255 & _M0L14capacity__maskS2256;
      _M0L3pslS702 = _M0L6_2atmpS2253;
      _M0L3idxS703 = _M0L6_2atmpS2254;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS721,
  moonbit_string_t _M0L3keyS727,
  void* _M0L5valueS728,
  int32_t _M0L4hashS723
) {
  int32_t _M0L14capacity__maskS2277;
  int32_t _M0L6_2atmpS2276;
  int32_t _M0L3pslS718;
  int32_t _M0L3idxS719;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2277 = _M0L4selfS721->$3;
  _M0L6_2atmpS2276 = _M0L4hashS723 & _M0L14capacity__maskS2277;
  _M0L3pslS718 = 0;
  _M0L3idxS719 = _M0L6_2atmpS2276;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3293 = _M0L4selfS721->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2275 = _M0L8_2afieldS3293;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3292;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS720;
    if (
      _M0L3idxS719 < 0
      || _M0L3idxS719 >= Moonbit_array_length(_M0L7entriesS2275)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3292
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2275[_M0L3idxS719];
    _M0L7_2abindS720 = _M0L6_2atmpS3292;
    if (_M0L7_2abindS720 == 0) {
      int32_t _M0L4sizeS2260 = _M0L4selfS721->$1;
      int32_t _M0L8grow__atS2261 = _M0L4selfS721->$4;
      int32_t _M0L7_2abindS724;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS725;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS726;
      if (_M0L4sizeS2260 >= _M0L8grow__atS2261) {
        int32_t _M0L14capacity__maskS2263;
        int32_t _M0L6_2atmpS2262;
        moonbit_incref(_M0L4selfS721);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS721);
        _M0L14capacity__maskS2263 = _M0L4selfS721->$3;
        _M0L6_2atmpS2262 = _M0L4hashS723 & _M0L14capacity__maskS2263;
        _M0L3pslS718 = 0;
        _M0L3idxS719 = _M0L6_2atmpS2262;
        continue;
      }
      _M0L7_2abindS724 = _M0L4selfS721->$6;
      _M0L7_2abindS725 = 0;
      _M0L5entryS726
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS726)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS726->$0 = _M0L7_2abindS724;
      _M0L5entryS726->$1 = _M0L7_2abindS725;
      _M0L5entryS726->$2 = _M0L3pslS718;
      _M0L5entryS726->$3 = _M0L4hashS723;
      _M0L5entryS726->$4 = _M0L3keyS727;
      _M0L5entryS726->$5 = _M0L5valueS728;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS721, _M0L3idxS719, _M0L5entryS726);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS729 = _M0L7_2abindS720;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS730 =
        _M0L7_2aSomeS729;
      int32_t _M0L4hashS2265 = _M0L14_2acurr__entryS730->$3;
      int32_t _if__result_3678;
      int32_t _M0L3pslS2266;
      int32_t _M0L6_2atmpS2271;
      int32_t _M0L6_2atmpS2273;
      int32_t _M0L14capacity__maskS2274;
      int32_t _M0L6_2atmpS2272;
      if (_M0L4hashS2265 == _M0L4hashS723) {
        moonbit_string_t _M0L8_2afieldS3291 = _M0L14_2acurr__entryS730->$4;
        moonbit_string_t _M0L3keyS2264 = _M0L8_2afieldS3291;
        int32_t _M0L6_2atmpS3290;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3290
        = moonbit_val_array_equal(_M0L3keyS2264, _M0L3keyS727);
        _if__result_3678 = _M0L6_2atmpS3290;
      } else {
        _if__result_3678 = 0;
      }
      if (_if__result_3678) {
        void* _M0L6_2aoldS3289;
        moonbit_incref(_M0L14_2acurr__entryS730);
        moonbit_decref(_M0L3keyS727);
        moonbit_decref(_M0L4selfS721);
        _M0L6_2aoldS3289 = _M0L14_2acurr__entryS730->$5;
        moonbit_decref(_M0L6_2aoldS3289);
        _M0L14_2acurr__entryS730->$5 = _M0L5valueS728;
        moonbit_decref(_M0L14_2acurr__entryS730);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS730);
      }
      _M0L3pslS2266 = _M0L14_2acurr__entryS730->$2;
      if (_M0L3pslS718 > _M0L3pslS2266) {
        int32_t _M0L4sizeS2267 = _M0L4selfS721->$1;
        int32_t _M0L8grow__atS2268 = _M0L4selfS721->$4;
        int32_t _M0L7_2abindS731;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS732;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS733;
        if (_M0L4sizeS2267 >= _M0L8grow__atS2268) {
          int32_t _M0L14capacity__maskS2270;
          int32_t _M0L6_2atmpS2269;
          moonbit_decref(_M0L14_2acurr__entryS730);
          moonbit_incref(_M0L4selfS721);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS721);
          _M0L14capacity__maskS2270 = _M0L4selfS721->$3;
          _M0L6_2atmpS2269 = _M0L4hashS723 & _M0L14capacity__maskS2270;
          _M0L3pslS718 = 0;
          _M0L3idxS719 = _M0L6_2atmpS2269;
          continue;
        }
        moonbit_incref(_M0L4selfS721);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS721, _M0L3idxS719, _M0L14_2acurr__entryS730);
        _M0L7_2abindS731 = _M0L4selfS721->$6;
        _M0L7_2abindS732 = 0;
        _M0L5entryS733
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS733)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS733->$0 = _M0L7_2abindS731;
        _M0L5entryS733->$1 = _M0L7_2abindS732;
        _M0L5entryS733->$2 = _M0L3pslS718;
        _M0L5entryS733->$3 = _M0L4hashS723;
        _M0L5entryS733->$4 = _M0L3keyS727;
        _M0L5entryS733->$5 = _M0L5valueS728;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS721, _M0L3idxS719, _M0L5entryS733);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS730);
      }
      _M0L6_2atmpS2271 = _M0L3pslS718 + 1;
      _M0L6_2atmpS2273 = _M0L3idxS719 + 1;
      _M0L14capacity__maskS2274 = _M0L4selfS721->$3;
      _M0L6_2atmpS2272 = _M0L6_2atmpS2273 & _M0L14capacity__maskS2274;
      _M0L3pslS718 = _M0L6_2atmpS2271;
      _M0L3idxS719 = _M0L6_2atmpS2272;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS660,
  int32_t _M0L3idxS665,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS664
) {
  int32_t _M0L3pslS2191;
  int32_t _M0L6_2atmpS2187;
  int32_t _M0L6_2atmpS2189;
  int32_t _M0L14capacity__maskS2190;
  int32_t _M0L6_2atmpS2188;
  int32_t _M0L3pslS656;
  int32_t _M0L3idxS657;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS658;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2191 = _M0L5entryS664->$2;
  _M0L6_2atmpS2187 = _M0L3pslS2191 + 1;
  _M0L6_2atmpS2189 = _M0L3idxS665 + 1;
  _M0L14capacity__maskS2190 = _M0L4selfS660->$3;
  _M0L6_2atmpS2188 = _M0L6_2atmpS2189 & _M0L14capacity__maskS2190;
  _M0L3pslS656 = _M0L6_2atmpS2187;
  _M0L3idxS657 = _M0L6_2atmpS2188;
  _M0L5entryS658 = _M0L5entryS664;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3295 =
      _M0L4selfS660->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2186 =
      _M0L8_2afieldS3295;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3294;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS659;
    if (
      _M0L3idxS657 < 0
      || _M0L3idxS657 >= Moonbit_array_length(_M0L7entriesS2186)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3294
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2186[
        _M0L3idxS657
      ];
    _M0L7_2abindS659 = _M0L6_2atmpS3294;
    if (_M0L7_2abindS659 == 0) {
      _M0L5entryS658->$2 = _M0L3pslS656;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS660, _M0L5entryS658, _M0L3idxS657);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS662 =
        _M0L7_2abindS659;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS663 =
        _M0L7_2aSomeS662;
      int32_t _M0L3pslS2176 = _M0L14_2acurr__entryS663->$2;
      if (_M0L3pslS656 > _M0L3pslS2176) {
        int32_t _M0L3pslS2181;
        int32_t _M0L6_2atmpS2177;
        int32_t _M0L6_2atmpS2179;
        int32_t _M0L14capacity__maskS2180;
        int32_t _M0L6_2atmpS2178;
        _M0L5entryS658->$2 = _M0L3pslS656;
        moonbit_incref(_M0L14_2acurr__entryS663);
        moonbit_incref(_M0L4selfS660);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS660, _M0L5entryS658, _M0L3idxS657);
        _M0L3pslS2181 = _M0L14_2acurr__entryS663->$2;
        _M0L6_2atmpS2177 = _M0L3pslS2181 + 1;
        _M0L6_2atmpS2179 = _M0L3idxS657 + 1;
        _M0L14capacity__maskS2180 = _M0L4selfS660->$3;
        _M0L6_2atmpS2178 = _M0L6_2atmpS2179 & _M0L14capacity__maskS2180;
        _M0L3pslS656 = _M0L6_2atmpS2177;
        _M0L3idxS657 = _M0L6_2atmpS2178;
        _M0L5entryS658 = _M0L14_2acurr__entryS663;
        continue;
      } else {
        int32_t _M0L6_2atmpS2182 = _M0L3pslS656 + 1;
        int32_t _M0L6_2atmpS2184 = _M0L3idxS657 + 1;
        int32_t _M0L14capacity__maskS2185 = _M0L4selfS660->$3;
        int32_t _M0L6_2atmpS2183 =
          _M0L6_2atmpS2184 & _M0L14capacity__maskS2185;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3680 =
          _M0L5entryS658;
        _M0L3pslS656 = _M0L6_2atmpS2182;
        _M0L3idxS657 = _M0L6_2atmpS2183;
        _M0L5entryS658 = _tmp_3680;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS670,
  int32_t _M0L3idxS675,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS674
) {
  int32_t _M0L3pslS2207;
  int32_t _M0L6_2atmpS2203;
  int32_t _M0L6_2atmpS2205;
  int32_t _M0L14capacity__maskS2206;
  int32_t _M0L6_2atmpS2204;
  int32_t _M0L3pslS666;
  int32_t _M0L3idxS667;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS668;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2207 = _M0L5entryS674->$2;
  _M0L6_2atmpS2203 = _M0L3pslS2207 + 1;
  _M0L6_2atmpS2205 = _M0L3idxS675 + 1;
  _M0L14capacity__maskS2206 = _M0L4selfS670->$3;
  _M0L6_2atmpS2204 = _M0L6_2atmpS2205 & _M0L14capacity__maskS2206;
  _M0L3pslS666 = _M0L6_2atmpS2203;
  _M0L3idxS667 = _M0L6_2atmpS2204;
  _M0L5entryS668 = _M0L5entryS674;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3297 =
      _M0L4selfS670->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2202 =
      _M0L8_2afieldS3297;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3296;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS669;
    if (
      _M0L3idxS667 < 0
      || _M0L3idxS667 >= Moonbit_array_length(_M0L7entriesS2202)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3296
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2202[
        _M0L3idxS667
      ];
    _M0L7_2abindS669 = _M0L6_2atmpS3296;
    if (_M0L7_2abindS669 == 0) {
      _M0L5entryS668->$2 = _M0L3pslS666;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS670, _M0L5entryS668, _M0L3idxS667);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS672 =
        _M0L7_2abindS669;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS673 =
        _M0L7_2aSomeS672;
      int32_t _M0L3pslS2192 = _M0L14_2acurr__entryS673->$2;
      if (_M0L3pslS666 > _M0L3pslS2192) {
        int32_t _M0L3pslS2197;
        int32_t _M0L6_2atmpS2193;
        int32_t _M0L6_2atmpS2195;
        int32_t _M0L14capacity__maskS2196;
        int32_t _M0L6_2atmpS2194;
        _M0L5entryS668->$2 = _M0L3pslS666;
        moonbit_incref(_M0L14_2acurr__entryS673);
        moonbit_incref(_M0L4selfS670);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS670, _M0L5entryS668, _M0L3idxS667);
        _M0L3pslS2197 = _M0L14_2acurr__entryS673->$2;
        _M0L6_2atmpS2193 = _M0L3pslS2197 + 1;
        _M0L6_2atmpS2195 = _M0L3idxS667 + 1;
        _M0L14capacity__maskS2196 = _M0L4selfS670->$3;
        _M0L6_2atmpS2194 = _M0L6_2atmpS2195 & _M0L14capacity__maskS2196;
        _M0L3pslS666 = _M0L6_2atmpS2193;
        _M0L3idxS667 = _M0L6_2atmpS2194;
        _M0L5entryS668 = _M0L14_2acurr__entryS673;
        continue;
      } else {
        int32_t _M0L6_2atmpS2198 = _M0L3pslS666 + 1;
        int32_t _M0L6_2atmpS2200 = _M0L3idxS667 + 1;
        int32_t _M0L14capacity__maskS2201 = _M0L4selfS670->$3;
        int32_t _M0L6_2atmpS2199 =
          _M0L6_2atmpS2200 & _M0L14capacity__maskS2201;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3682 =
          _M0L5entryS668;
        _M0L3pslS666 = _M0L6_2atmpS2198;
        _M0L3idxS667 = _M0L6_2atmpS2199;
        _M0L5entryS668 = _tmp_3682;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS680,
  int32_t _M0L3idxS685,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS684
) {
  int32_t _M0L3pslS2223;
  int32_t _M0L6_2atmpS2219;
  int32_t _M0L6_2atmpS2221;
  int32_t _M0L14capacity__maskS2222;
  int32_t _M0L6_2atmpS2220;
  int32_t _M0L3pslS676;
  int32_t _M0L3idxS677;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS678;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2223 = _M0L5entryS684->$2;
  _M0L6_2atmpS2219 = _M0L3pslS2223 + 1;
  _M0L6_2atmpS2221 = _M0L3idxS685 + 1;
  _M0L14capacity__maskS2222 = _M0L4selfS680->$3;
  _M0L6_2atmpS2220 = _M0L6_2atmpS2221 & _M0L14capacity__maskS2222;
  _M0L3pslS676 = _M0L6_2atmpS2219;
  _M0L3idxS677 = _M0L6_2atmpS2220;
  _M0L5entryS678 = _M0L5entryS684;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3299 = _M0L4selfS680->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2218 = _M0L8_2afieldS3299;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3298;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS679;
    if (
      _M0L3idxS677 < 0
      || _M0L3idxS677 >= Moonbit_array_length(_M0L7entriesS2218)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3298
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2218[_M0L3idxS677];
    _M0L7_2abindS679 = _M0L6_2atmpS3298;
    if (_M0L7_2abindS679 == 0) {
      _M0L5entryS678->$2 = _M0L3pslS676;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS680, _M0L5entryS678, _M0L3idxS677);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS682 = _M0L7_2abindS679;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS683 =
        _M0L7_2aSomeS682;
      int32_t _M0L3pslS2208 = _M0L14_2acurr__entryS683->$2;
      if (_M0L3pslS676 > _M0L3pslS2208) {
        int32_t _M0L3pslS2213;
        int32_t _M0L6_2atmpS2209;
        int32_t _M0L6_2atmpS2211;
        int32_t _M0L14capacity__maskS2212;
        int32_t _M0L6_2atmpS2210;
        _M0L5entryS678->$2 = _M0L3pslS676;
        moonbit_incref(_M0L14_2acurr__entryS683);
        moonbit_incref(_M0L4selfS680);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS680, _M0L5entryS678, _M0L3idxS677);
        _M0L3pslS2213 = _M0L14_2acurr__entryS683->$2;
        _M0L6_2atmpS2209 = _M0L3pslS2213 + 1;
        _M0L6_2atmpS2211 = _M0L3idxS677 + 1;
        _M0L14capacity__maskS2212 = _M0L4selfS680->$3;
        _M0L6_2atmpS2210 = _M0L6_2atmpS2211 & _M0L14capacity__maskS2212;
        _M0L3pslS676 = _M0L6_2atmpS2209;
        _M0L3idxS677 = _M0L6_2atmpS2210;
        _M0L5entryS678 = _M0L14_2acurr__entryS683;
        continue;
      } else {
        int32_t _M0L6_2atmpS2214 = _M0L3pslS676 + 1;
        int32_t _M0L6_2atmpS2216 = _M0L3idxS677 + 1;
        int32_t _M0L14capacity__maskS2217 = _M0L4selfS680->$3;
        int32_t _M0L6_2atmpS2215 =
          _M0L6_2atmpS2216 & _M0L14capacity__maskS2217;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_3684 = _M0L5entryS678;
        _M0L3pslS676 = _M0L6_2atmpS2214;
        _M0L3idxS677 = _M0L6_2atmpS2215;
        _M0L5entryS678 = _tmp_3684;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS638,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS640,
  int32_t _M0L8new__idxS639
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3302;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2170;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2171;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3301;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3300;
  int32_t _M0L6_2acntS3526;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS641;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3302 = _M0L4selfS638->$0;
  _M0L7entriesS2170 = _M0L8_2afieldS3302;
  moonbit_incref(_M0L5entryS640);
  _M0L6_2atmpS2171 = _M0L5entryS640;
  if (
    _M0L8new__idxS639 < 0
    || _M0L8new__idxS639 >= Moonbit_array_length(_M0L7entriesS2170)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3301
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2170[
      _M0L8new__idxS639
    ];
  if (_M0L6_2aoldS3301) {
    moonbit_decref(_M0L6_2aoldS3301);
  }
  _M0L7entriesS2170[_M0L8new__idxS639] = _M0L6_2atmpS2171;
  _M0L8_2afieldS3300 = _M0L5entryS640->$1;
  _M0L6_2acntS3526 = Moonbit_object_header(_M0L5entryS640)->rc;
  if (_M0L6_2acntS3526 > 1) {
    int32_t _M0L11_2anew__cntS3529 = _M0L6_2acntS3526 - 1;
    Moonbit_object_header(_M0L5entryS640)->rc = _M0L11_2anew__cntS3529;
    if (_M0L8_2afieldS3300) {
      moonbit_incref(_M0L8_2afieldS3300);
    }
  } else if (_M0L6_2acntS3526 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3528 =
      _M0L5entryS640->$5;
    moonbit_string_t _M0L8_2afieldS3527;
    moonbit_decref(_M0L8_2afieldS3528);
    _M0L8_2afieldS3527 = _M0L5entryS640->$4;
    moonbit_decref(_M0L8_2afieldS3527);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS640);
  }
  _M0L7_2abindS641 = _M0L8_2afieldS3300;
  if (_M0L7_2abindS641 == 0) {
    if (_M0L7_2abindS641) {
      moonbit_decref(_M0L7_2abindS641);
    }
    _M0L4selfS638->$6 = _M0L8new__idxS639;
    moonbit_decref(_M0L4selfS638);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS642;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS643;
    moonbit_decref(_M0L4selfS638);
    _M0L7_2aSomeS642 = _M0L7_2abindS641;
    _M0L7_2anextS643 = _M0L7_2aSomeS642;
    _M0L7_2anextS643->$0 = _M0L8new__idxS639;
    moonbit_decref(_M0L7_2anextS643);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS644,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS646,
  int32_t _M0L8new__idxS645
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3305;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2172;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2173;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3304;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3303;
  int32_t _M0L6_2acntS3530;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS647;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3305 = _M0L4selfS644->$0;
  _M0L7entriesS2172 = _M0L8_2afieldS3305;
  moonbit_incref(_M0L5entryS646);
  _M0L6_2atmpS2173 = _M0L5entryS646;
  if (
    _M0L8new__idxS645 < 0
    || _M0L8new__idxS645 >= Moonbit_array_length(_M0L7entriesS2172)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3304
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2172[
      _M0L8new__idxS645
    ];
  if (_M0L6_2aoldS3304) {
    moonbit_decref(_M0L6_2aoldS3304);
  }
  _M0L7entriesS2172[_M0L8new__idxS645] = _M0L6_2atmpS2173;
  _M0L8_2afieldS3303 = _M0L5entryS646->$1;
  _M0L6_2acntS3530 = Moonbit_object_header(_M0L5entryS646)->rc;
  if (_M0L6_2acntS3530 > 1) {
    int32_t _M0L11_2anew__cntS3532 = _M0L6_2acntS3530 - 1;
    Moonbit_object_header(_M0L5entryS646)->rc = _M0L11_2anew__cntS3532;
    if (_M0L8_2afieldS3303) {
      moonbit_incref(_M0L8_2afieldS3303);
    }
  } else if (_M0L6_2acntS3530 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3531 =
      _M0L5entryS646->$5;
    moonbit_decref(_M0L8_2afieldS3531);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS646);
  }
  _M0L7_2abindS647 = _M0L8_2afieldS3303;
  if (_M0L7_2abindS647 == 0) {
    if (_M0L7_2abindS647) {
      moonbit_decref(_M0L7_2abindS647);
    }
    _M0L4selfS644->$6 = _M0L8new__idxS645;
    moonbit_decref(_M0L4selfS644);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS648;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS649;
    moonbit_decref(_M0L4selfS644);
    _M0L7_2aSomeS648 = _M0L7_2abindS647;
    _M0L7_2anextS649 = _M0L7_2aSomeS648;
    _M0L7_2anextS649->$0 = _M0L8new__idxS645;
    moonbit_decref(_M0L7_2anextS649);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS650,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS652,
  int32_t _M0L8new__idxS651
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3308;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2174;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2175;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3307;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3306;
  int32_t _M0L6_2acntS3533;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS653;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3308 = _M0L4selfS650->$0;
  _M0L7entriesS2174 = _M0L8_2afieldS3308;
  moonbit_incref(_M0L5entryS652);
  _M0L6_2atmpS2175 = _M0L5entryS652;
  if (
    _M0L8new__idxS651 < 0
    || _M0L8new__idxS651 >= Moonbit_array_length(_M0L7entriesS2174)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3307
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2174[_M0L8new__idxS651];
  if (_M0L6_2aoldS3307) {
    moonbit_decref(_M0L6_2aoldS3307);
  }
  _M0L7entriesS2174[_M0L8new__idxS651] = _M0L6_2atmpS2175;
  _M0L8_2afieldS3306 = _M0L5entryS652->$1;
  _M0L6_2acntS3533 = Moonbit_object_header(_M0L5entryS652)->rc;
  if (_M0L6_2acntS3533 > 1) {
    int32_t _M0L11_2anew__cntS3536 = _M0L6_2acntS3533 - 1;
    Moonbit_object_header(_M0L5entryS652)->rc = _M0L11_2anew__cntS3536;
    if (_M0L8_2afieldS3306) {
      moonbit_incref(_M0L8_2afieldS3306);
    }
  } else if (_M0L6_2acntS3533 == 1) {
    void* _M0L8_2afieldS3535 = _M0L5entryS652->$5;
    moonbit_string_t _M0L8_2afieldS3534;
    moonbit_decref(_M0L8_2afieldS3535);
    _M0L8_2afieldS3534 = _M0L5entryS652->$4;
    moonbit_decref(_M0L8_2afieldS3534);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS652);
  }
  _M0L7_2abindS653 = _M0L8_2afieldS3306;
  if (_M0L7_2abindS653 == 0) {
    if (_M0L7_2abindS653) {
      moonbit_decref(_M0L7_2abindS653);
    }
    _M0L4selfS650->$6 = _M0L8new__idxS651;
    moonbit_decref(_M0L4selfS650);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS654;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS655;
    moonbit_decref(_M0L4selfS650);
    _M0L7_2aSomeS654 = _M0L7_2abindS653;
    _M0L7_2anextS655 = _M0L7_2aSomeS654;
    _M0L7_2anextS655->$0 = _M0L8new__idxS651;
    moonbit_decref(_M0L7_2anextS655);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS627,
  int32_t _M0L3idxS629,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS628
) {
  int32_t _M0L7_2abindS626;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3310;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2148;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2149;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3309;
  int32_t _M0L4sizeS2151;
  int32_t _M0L6_2atmpS2150;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS626 = _M0L4selfS627->$6;
  switch (_M0L7_2abindS626) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2143;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3311;
      moonbit_incref(_M0L5entryS628);
      _M0L6_2atmpS2143 = _M0L5entryS628;
      _M0L6_2aoldS3311 = _M0L4selfS627->$5;
      if (_M0L6_2aoldS3311) {
        moonbit_decref(_M0L6_2aoldS3311);
      }
      _M0L4selfS627->$5 = _M0L6_2atmpS2143;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3314 =
        _M0L4selfS627->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2147 =
        _M0L8_2afieldS3314;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3313;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2146;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2144;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2145;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3312;
      if (
        _M0L7_2abindS626 < 0
        || _M0L7_2abindS626 >= Moonbit_array_length(_M0L7entriesS2147)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3313
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2147[
          _M0L7_2abindS626
        ];
      _M0L6_2atmpS2146 = _M0L6_2atmpS3313;
      if (_M0L6_2atmpS2146) {
        moonbit_incref(_M0L6_2atmpS2146);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2144
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2146);
      moonbit_incref(_M0L5entryS628);
      _M0L6_2atmpS2145 = _M0L5entryS628;
      _M0L6_2aoldS3312 = _M0L6_2atmpS2144->$1;
      if (_M0L6_2aoldS3312) {
        moonbit_decref(_M0L6_2aoldS3312);
      }
      _M0L6_2atmpS2144->$1 = _M0L6_2atmpS2145;
      moonbit_decref(_M0L6_2atmpS2144);
      break;
    }
  }
  _M0L4selfS627->$6 = _M0L3idxS629;
  _M0L8_2afieldS3310 = _M0L4selfS627->$0;
  _M0L7entriesS2148 = _M0L8_2afieldS3310;
  _M0L6_2atmpS2149 = _M0L5entryS628;
  if (
    _M0L3idxS629 < 0
    || _M0L3idxS629 >= Moonbit_array_length(_M0L7entriesS2148)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3309
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2148[
      _M0L3idxS629
    ];
  if (_M0L6_2aoldS3309) {
    moonbit_decref(_M0L6_2aoldS3309);
  }
  _M0L7entriesS2148[_M0L3idxS629] = _M0L6_2atmpS2149;
  _M0L4sizeS2151 = _M0L4selfS627->$1;
  _M0L6_2atmpS2150 = _M0L4sizeS2151 + 1;
  _M0L4selfS627->$1 = _M0L6_2atmpS2150;
  moonbit_decref(_M0L4selfS627);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS631,
  int32_t _M0L3idxS633,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS632
) {
  int32_t _M0L7_2abindS630;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3316;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2157;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2158;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3315;
  int32_t _M0L4sizeS2160;
  int32_t _M0L6_2atmpS2159;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS630 = _M0L4selfS631->$6;
  switch (_M0L7_2abindS630) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2152;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3317;
      moonbit_incref(_M0L5entryS632);
      _M0L6_2atmpS2152 = _M0L5entryS632;
      _M0L6_2aoldS3317 = _M0L4selfS631->$5;
      if (_M0L6_2aoldS3317) {
        moonbit_decref(_M0L6_2aoldS3317);
      }
      _M0L4selfS631->$5 = _M0L6_2atmpS2152;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3320 =
        _M0L4selfS631->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2156 =
        _M0L8_2afieldS3320;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3319;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2155;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2153;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2154;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3318;
      if (
        _M0L7_2abindS630 < 0
        || _M0L7_2abindS630 >= Moonbit_array_length(_M0L7entriesS2156)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3319
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2156[
          _M0L7_2abindS630
        ];
      _M0L6_2atmpS2155 = _M0L6_2atmpS3319;
      if (_M0L6_2atmpS2155) {
        moonbit_incref(_M0L6_2atmpS2155);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2153
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2155);
      moonbit_incref(_M0L5entryS632);
      _M0L6_2atmpS2154 = _M0L5entryS632;
      _M0L6_2aoldS3318 = _M0L6_2atmpS2153->$1;
      if (_M0L6_2aoldS3318) {
        moonbit_decref(_M0L6_2aoldS3318);
      }
      _M0L6_2atmpS2153->$1 = _M0L6_2atmpS2154;
      moonbit_decref(_M0L6_2atmpS2153);
      break;
    }
  }
  _M0L4selfS631->$6 = _M0L3idxS633;
  _M0L8_2afieldS3316 = _M0L4selfS631->$0;
  _M0L7entriesS2157 = _M0L8_2afieldS3316;
  _M0L6_2atmpS2158 = _M0L5entryS632;
  if (
    _M0L3idxS633 < 0
    || _M0L3idxS633 >= Moonbit_array_length(_M0L7entriesS2157)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3315
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2157[
      _M0L3idxS633
    ];
  if (_M0L6_2aoldS3315) {
    moonbit_decref(_M0L6_2aoldS3315);
  }
  _M0L7entriesS2157[_M0L3idxS633] = _M0L6_2atmpS2158;
  _M0L4sizeS2160 = _M0L4selfS631->$1;
  _M0L6_2atmpS2159 = _M0L4sizeS2160 + 1;
  _M0L4selfS631->$1 = _M0L6_2atmpS2159;
  moonbit_decref(_M0L4selfS631);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS635,
  int32_t _M0L3idxS637,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS636
) {
  int32_t _M0L7_2abindS634;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3322;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2166;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2167;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3321;
  int32_t _M0L4sizeS2169;
  int32_t _M0L6_2atmpS2168;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS634 = _M0L4selfS635->$6;
  switch (_M0L7_2abindS634) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2161;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3323;
      moonbit_incref(_M0L5entryS636);
      _M0L6_2atmpS2161 = _M0L5entryS636;
      _M0L6_2aoldS3323 = _M0L4selfS635->$5;
      if (_M0L6_2aoldS3323) {
        moonbit_decref(_M0L6_2aoldS3323);
      }
      _M0L4selfS635->$5 = _M0L6_2atmpS2161;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3326 = _M0L4selfS635->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2165 = _M0L8_2afieldS3326;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3325;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2164;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2162;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2163;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3324;
      if (
        _M0L7_2abindS634 < 0
        || _M0L7_2abindS634 >= Moonbit_array_length(_M0L7entriesS2165)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3325
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2165[_M0L7_2abindS634];
      _M0L6_2atmpS2164 = _M0L6_2atmpS3325;
      if (_M0L6_2atmpS2164) {
        moonbit_incref(_M0L6_2atmpS2164);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2162
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2164);
      moonbit_incref(_M0L5entryS636);
      _M0L6_2atmpS2163 = _M0L5entryS636;
      _M0L6_2aoldS3324 = _M0L6_2atmpS2162->$1;
      if (_M0L6_2aoldS3324) {
        moonbit_decref(_M0L6_2aoldS3324);
      }
      _M0L6_2atmpS2162->$1 = _M0L6_2atmpS2163;
      moonbit_decref(_M0L6_2atmpS2162);
      break;
    }
  }
  _M0L4selfS635->$6 = _M0L3idxS637;
  _M0L8_2afieldS3322 = _M0L4selfS635->$0;
  _M0L7entriesS2166 = _M0L8_2afieldS3322;
  _M0L6_2atmpS2167 = _M0L5entryS636;
  if (
    _M0L3idxS637 < 0
    || _M0L3idxS637 >= Moonbit_array_length(_M0L7entriesS2166)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3321
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2166[_M0L3idxS637];
  if (_M0L6_2aoldS3321) {
    moonbit_decref(_M0L6_2aoldS3321);
  }
  _M0L7entriesS2166[_M0L3idxS637] = _M0L6_2atmpS2167;
  _M0L4sizeS2169 = _M0L4selfS635->$1;
  _M0L6_2atmpS2168 = _M0L4sizeS2169 + 1;
  _M0L4selfS635->$1 = _M0L6_2atmpS2168;
  moonbit_decref(_M0L4selfS635);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS609
) {
  int32_t _M0L8capacityS608;
  int32_t _M0L7_2abindS610;
  int32_t _M0L7_2abindS611;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2140;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS612;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS613;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3685;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS608
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS609);
  _M0L7_2abindS610 = _M0L8capacityS608 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS611 = _M0FPB21calc__grow__threshold(_M0L8capacityS608);
  _M0L6_2atmpS2140 = 0;
  _M0L7_2abindS612
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS608, _M0L6_2atmpS2140);
  _M0L7_2abindS613 = 0;
  _block_3685
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3685)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3685->$0 = _M0L7_2abindS612;
  _block_3685->$1 = 0;
  _block_3685->$2 = _M0L8capacityS608;
  _block_3685->$3 = _M0L7_2abindS610;
  _block_3685->$4 = _M0L7_2abindS611;
  _block_3685->$5 = _M0L7_2abindS613;
  _block_3685->$6 = -1;
  return _block_3685;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS615
) {
  int32_t _M0L8capacityS614;
  int32_t _M0L7_2abindS616;
  int32_t _M0L7_2abindS617;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2141;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS618;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS619;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3686;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS614
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS615);
  _M0L7_2abindS616 = _M0L8capacityS614 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS617 = _M0FPB21calc__grow__threshold(_M0L8capacityS614);
  _M0L6_2atmpS2141 = 0;
  _M0L7_2abindS618
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS614, _M0L6_2atmpS2141);
  _M0L7_2abindS619 = 0;
  _block_3686
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3686)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3686->$0 = _M0L7_2abindS618;
  _block_3686->$1 = 0;
  _block_3686->$2 = _M0L8capacityS614;
  _block_3686->$3 = _M0L7_2abindS616;
  _block_3686->$4 = _M0L7_2abindS617;
  _block_3686->$5 = _M0L7_2abindS619;
  _block_3686->$6 = -1;
  return _block_3686;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS621
) {
  int32_t _M0L8capacityS620;
  int32_t _M0L7_2abindS622;
  int32_t _M0L7_2abindS623;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2142;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS624;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS625;
  struct _M0TPB3MapGsRPB4JsonE* _block_3687;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS620
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS621);
  _M0L7_2abindS622 = _M0L8capacityS620 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS623 = _M0FPB21calc__grow__threshold(_M0L8capacityS620);
  _M0L6_2atmpS2142 = 0;
  _M0L7_2abindS624
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS620, _M0L6_2atmpS2142);
  _M0L7_2abindS625 = 0;
  _block_3687
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_3687)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_3687->$0 = _M0L7_2abindS624;
  _block_3687->$1 = 0;
  _block_3687->$2 = _M0L8capacityS620;
  _block_3687->$3 = _M0L7_2abindS622;
  _block_3687->$4 = _M0L7_2abindS623;
  _block_3687->$5 = _M0L7_2abindS625;
  _block_3687->$6 = -1;
  return _block_3687;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS607) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS607 >= 0) {
    int32_t _M0L6_2atmpS2139;
    int32_t _M0L6_2atmpS2138;
    int32_t _M0L6_2atmpS2137;
    int32_t _M0L6_2atmpS2136;
    if (_M0L4selfS607 <= 1) {
      return 1;
    }
    if (_M0L4selfS607 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2139 = _M0L4selfS607 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2138 = moonbit_clz32(_M0L6_2atmpS2139);
    _M0L6_2atmpS2137 = _M0L6_2atmpS2138 - 1;
    _M0L6_2atmpS2136 = 2147483647 >> (_M0L6_2atmpS2137 & 31);
    return _M0L6_2atmpS2136 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS606) {
  int32_t _M0L6_2atmpS2135;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2135 = _M0L8capacityS606 * 13;
  return _M0L6_2atmpS2135 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS600
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS600 == 0) {
    if (_M0L4selfS600) {
      moonbit_decref(_M0L4selfS600);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS601 =
      _M0L4selfS600;
    return _M0L7_2aSomeS601;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS602
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS602 == 0) {
    if (_M0L4selfS602) {
      moonbit_decref(_M0L4selfS602);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS603 =
      _M0L4selfS602;
    return _M0L7_2aSomeS603;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS604
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS604 == 0) {
    if (_M0L4selfS604) {
      moonbit_decref(_M0L4selfS604);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS605 = _M0L4selfS604;
    return _M0L7_2aSomeS605;
  }
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t _M0L3lenS599
) {
  void** _M0L6_2atmpS2134;
  struct _M0TPB5ArrayGRPB4JsonE* _block_3688;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2134
  = (void**)moonbit_make_ref_array(_M0L3lenS599, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _block_3688
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_block_3688)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _block_3688->$0 = _M0L6_2atmpS2134;
  _block_3688->$1 = _M0L3lenS599;
  return _block_3688;
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS598
) {
  moonbit_string_t* _M0L6_2atmpS2133;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2133 = _M0L4selfS598;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2133);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS594,
  int32_t _M0L5indexS595
) {
  uint64_t* _M0L6_2atmpS2131;
  uint64_t _M0L6_2atmpS3327;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2131 = _M0L4selfS594;
  if (
    _M0L5indexS595 < 0
    || _M0L5indexS595 >= Moonbit_array_length(_M0L6_2atmpS2131)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3327 = (uint64_t)_M0L6_2atmpS2131[_M0L5indexS595];
  moonbit_decref(_M0L6_2atmpS2131);
  return _M0L6_2atmpS3327;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS596,
  int32_t _M0L5indexS597
) {
  uint32_t* _M0L6_2atmpS2132;
  uint32_t _M0L6_2atmpS3328;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2132 = _M0L4selfS596;
  if (
    _M0L5indexS597 < 0
    || _M0L5indexS597 >= Moonbit_array_length(_M0L6_2atmpS2132)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3328 = (uint32_t)_M0L6_2atmpS2132[_M0L5indexS597];
  moonbit_decref(_M0L6_2atmpS2132);
  return _M0L6_2atmpS3328;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS593
) {
  moonbit_string_t* _M0L6_2atmpS2129;
  int32_t _M0L6_2atmpS3329;
  int32_t _M0L6_2atmpS2130;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2128;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS593);
  _M0L6_2atmpS2129 = _M0L4selfS593;
  _M0L6_2atmpS3329 = Moonbit_array_length(_M0L4selfS593);
  moonbit_decref(_M0L4selfS593);
  _M0L6_2atmpS2130 = _M0L6_2atmpS3329;
  _M0L6_2atmpS2128
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2130, _M0L6_2atmpS2129
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2128);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS591
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS590;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__* _closure_3689;
  struct _M0TWEOs* _M0L6_2atmpS2116;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS590
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS590)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS590->$0 = 0;
  _closure_3689
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__));
  Moonbit_object_header(_closure_3689)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__, $0_0) >> 2, 2, 0);
  _closure_3689->code = &_M0MPC15array9ArrayView4iterGsEC2117l570;
  _closure_3689->$0_0 = _M0L4selfS591.$0;
  _closure_3689->$0_1 = _M0L4selfS591.$1;
  _closure_3689->$0_2 = _M0L4selfS591.$2;
  _closure_3689->$1 = _M0L1iS590;
  _M0L6_2atmpS2116 = (struct _M0TWEOs*)_closure_3689;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2116);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2117l570(
  struct _M0TWEOs* _M0L6_2aenvS2118
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__* _M0L14_2acasted__envS2119;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3334;
  struct _M0TPC13ref3RefGiE* _M0L1iS590;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3333;
  int32_t _M0L6_2acntS3537;
  struct _M0TPB9ArrayViewGsE _M0L4selfS591;
  int32_t _M0L3valS2120;
  int32_t _M0L6_2atmpS2121;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2119
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2117__l570__*)_M0L6_2aenvS2118;
  _M0L8_2afieldS3334 = _M0L14_2acasted__envS2119->$1;
  _M0L1iS590 = _M0L8_2afieldS3334;
  _M0L8_2afieldS3333
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2119->$0_1,
      _M0L14_2acasted__envS2119->$0_2,
      _M0L14_2acasted__envS2119->$0_0
  };
  _M0L6_2acntS3537 = Moonbit_object_header(_M0L14_2acasted__envS2119)->rc;
  if (_M0L6_2acntS3537 > 1) {
    int32_t _M0L11_2anew__cntS3538 = _M0L6_2acntS3537 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2119)->rc
    = _M0L11_2anew__cntS3538;
    moonbit_incref(_M0L1iS590);
    moonbit_incref(_M0L8_2afieldS3333.$0);
  } else if (_M0L6_2acntS3537 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2119);
  }
  _M0L4selfS591 = _M0L8_2afieldS3333;
  _M0L3valS2120 = _M0L1iS590->$0;
  moonbit_incref(_M0L4selfS591.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2121 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS591);
  if (_M0L3valS2120 < _M0L6_2atmpS2121) {
    moonbit_string_t* _M0L8_2afieldS3332 = _M0L4selfS591.$0;
    moonbit_string_t* _M0L3bufS2124 = _M0L8_2afieldS3332;
    int32_t _M0L8_2afieldS3331 = _M0L4selfS591.$1;
    int32_t _M0L5startS2126 = _M0L8_2afieldS3331;
    int32_t _M0L3valS2127 = _M0L1iS590->$0;
    int32_t _M0L6_2atmpS2125 = _M0L5startS2126 + _M0L3valS2127;
    moonbit_string_t _M0L6_2atmpS3330 =
      (moonbit_string_t)_M0L3bufS2124[_M0L6_2atmpS2125];
    moonbit_string_t _M0L4elemS592;
    int32_t _M0L3valS2123;
    int32_t _M0L6_2atmpS2122;
    moonbit_incref(_M0L6_2atmpS3330);
    moonbit_decref(_M0L3bufS2124);
    _M0L4elemS592 = _M0L6_2atmpS3330;
    _M0L3valS2123 = _M0L1iS590->$0;
    _M0L6_2atmpS2122 = _M0L3valS2123 + 1;
    _M0L1iS590->$0 = _M0L6_2atmpS2122;
    moonbit_decref(_M0L1iS590);
    return _M0L4elemS592;
  } else {
    moonbit_decref(_M0L4selfS591.$0);
    moonbit_decref(_M0L1iS590);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS589
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS589;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS588,
  struct _M0TPB6Logger _M0L6loggerS587
) {
  moonbit_string_t _M0L6_2atmpS2115;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2115
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS588, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS587.$0->$method_0(_M0L6loggerS587.$1, _M0L6_2atmpS2115);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS586,
  struct _M0TPB6Logger _M0L6loggerS585
) {
  moonbit_string_t _M0L6_2atmpS2114;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2114 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS586, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS585.$0->$method_0(_M0L6loggerS585.$1, _M0L6_2atmpS2114);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS580) {
  int32_t _M0L3lenS579;
  struct _M0TPC13ref3RefGiE* _M0L5indexS581;
  struct _M0R38String_3a_3aiter_2eanon__u2098__l247__* _closure_3690;
  struct _M0TWEOc* _M0L6_2atmpS2097;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS579 = Moonbit_array_length(_M0L4selfS580);
  _M0L5indexS581
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS581)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS581->$0 = 0;
  _closure_3690
  = (struct _M0R38String_3a_3aiter_2eanon__u2098__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2098__l247__));
  Moonbit_object_header(_closure_3690)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2098__l247__, $0) >> 2, 2, 0);
  _closure_3690->code = &_M0MPC16string6String4iterC2098l247;
  _closure_3690->$0 = _M0L5indexS581;
  _closure_3690->$1 = _M0L4selfS580;
  _closure_3690->$2 = _M0L3lenS579;
  _M0L6_2atmpS2097 = (struct _M0TWEOc*)_closure_3690;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2097);
}

int32_t _M0MPC16string6String4iterC2098l247(
  struct _M0TWEOc* _M0L6_2aenvS2099
) {
  struct _M0R38String_3a_3aiter_2eanon__u2098__l247__* _M0L14_2acasted__envS2100;
  int32_t _M0L3lenS579;
  moonbit_string_t _M0L8_2afieldS3337;
  moonbit_string_t _M0L4selfS580;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3336;
  int32_t _M0L6_2acntS3539;
  struct _M0TPC13ref3RefGiE* _M0L5indexS581;
  int32_t _M0L3valS2101;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2100
  = (struct _M0R38String_3a_3aiter_2eanon__u2098__l247__*)_M0L6_2aenvS2099;
  _M0L3lenS579 = _M0L14_2acasted__envS2100->$2;
  _M0L8_2afieldS3337 = _M0L14_2acasted__envS2100->$1;
  _M0L4selfS580 = _M0L8_2afieldS3337;
  _M0L8_2afieldS3336 = _M0L14_2acasted__envS2100->$0;
  _M0L6_2acntS3539 = Moonbit_object_header(_M0L14_2acasted__envS2100)->rc;
  if (_M0L6_2acntS3539 > 1) {
    int32_t _M0L11_2anew__cntS3540 = _M0L6_2acntS3539 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2100)->rc
    = _M0L11_2anew__cntS3540;
    moonbit_incref(_M0L4selfS580);
    moonbit_incref(_M0L8_2afieldS3336);
  } else if (_M0L6_2acntS3539 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2100);
  }
  _M0L5indexS581 = _M0L8_2afieldS3336;
  _M0L3valS2101 = _M0L5indexS581->$0;
  if (_M0L3valS2101 < _M0L3lenS579) {
    int32_t _M0L3valS2113 = _M0L5indexS581->$0;
    int32_t _M0L2c1S582 = _M0L4selfS580[_M0L3valS2113];
    int32_t _if__result_3691;
    int32_t _M0L3valS2111;
    int32_t _M0L6_2atmpS2110;
    int32_t _M0L6_2atmpS2112;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S582)) {
      int32_t _M0L3valS2103 = _M0L5indexS581->$0;
      int32_t _M0L6_2atmpS2102 = _M0L3valS2103 + 1;
      _if__result_3691 = _M0L6_2atmpS2102 < _M0L3lenS579;
    } else {
      _if__result_3691 = 0;
    }
    if (_if__result_3691) {
      int32_t _M0L3valS2109 = _M0L5indexS581->$0;
      int32_t _M0L6_2atmpS2108 = _M0L3valS2109 + 1;
      int32_t _M0L6_2atmpS3335 = _M0L4selfS580[_M0L6_2atmpS2108];
      int32_t _M0L2c2S583;
      moonbit_decref(_M0L4selfS580);
      _M0L2c2S583 = _M0L6_2atmpS3335;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S583)) {
        int32_t _M0L6_2atmpS2106 = (int32_t)_M0L2c1S582;
        int32_t _M0L6_2atmpS2107 = (int32_t)_M0L2c2S583;
        int32_t _M0L1cS584;
        int32_t _M0L3valS2105;
        int32_t _M0L6_2atmpS2104;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS584
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2106, _M0L6_2atmpS2107);
        _M0L3valS2105 = _M0L5indexS581->$0;
        _M0L6_2atmpS2104 = _M0L3valS2105 + 2;
        _M0L5indexS581->$0 = _M0L6_2atmpS2104;
        moonbit_decref(_M0L5indexS581);
        return _M0L1cS584;
      }
    } else {
      moonbit_decref(_M0L4selfS580);
    }
    _M0L3valS2111 = _M0L5indexS581->$0;
    _M0L6_2atmpS2110 = _M0L3valS2111 + 1;
    _M0L5indexS581->$0 = _M0L6_2atmpS2110;
    moonbit_decref(_M0L5indexS581);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2112 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S582);
    return _M0L6_2atmpS2112;
  } else {
    moonbit_decref(_M0L5indexS581);
    moonbit_decref(_M0L4selfS580);
    return -1;
  }
}

int32_t _M0MPC16string6String13contains__any(
  moonbit_string_t _M0L4selfS577,
  struct _M0TPC16string10StringView _M0L5charsS578
) {
  int32_t _M0L6_2atmpS2096;
  struct _M0TPC16string10StringView _M0L6_2atmpS2095;
  #line 559 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2096 = Moonbit_array_length(_M0L4selfS577);
  _M0L6_2atmpS2095
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2096, _M0L4selfS577
  };
  #line 560 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView13contains__any(_M0L6_2atmpS2095, _M0L5charsS578);
}

int32_t _M0MPC16string10StringView13contains__any(
  struct _M0TPC16string10StringView _M0L4selfS571,
  struct _M0TPC16string10StringView _M0L5charsS569
) {
  moonbit_string_t _M0L8_2afieldS3342;
  moonbit_string_t _M0L3strS2080;
  int32_t _M0L5startS2081;
  int32_t _M0L3endS2083;
  int64_t _M0L6_2atmpS2082;
  #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8_2afieldS3342 = _M0L5charsS569.$0;
  _M0L3strS2080 = _M0L8_2afieldS3342;
  _M0L5startS2081 = _M0L5charsS569.$1;
  _M0L3endS2083 = _M0L5charsS569.$2;
  _M0L6_2atmpS2082 = (int64_t)_M0L3endS2083;
  moonbit_incref(_M0L3strS2080);
  #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (
    _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2080, 0, _M0L5startS2081, _M0L6_2atmpS2082)
  ) {
    moonbit_decref(_M0L4selfS571.$0);
    moonbit_decref(_M0L5charsS569.$0);
    return 0;
  } else {
    moonbit_string_t _M0L8_2afieldS3341 = _M0L5charsS569.$0;
    moonbit_string_t _M0L3strS2084 = _M0L8_2afieldS3341;
    int32_t _M0L5startS2085 = _M0L5charsS569.$1;
    int32_t _M0L3endS2087 = _M0L5charsS569.$2;
    int64_t _M0L6_2atmpS2086 = (int64_t)_M0L3endS2087;
    moonbit_incref(_M0L3strS2084);
    #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2084, 1, _M0L5startS2085, _M0L6_2atmpS2086)
    ) {
      moonbit_string_t _M0L8_2afieldS3340 = _M0L5charsS569.$0;
      moonbit_string_t _M0L3strS2088 = _M0L8_2afieldS3340;
      moonbit_string_t _M0L8_2afieldS3339 = _M0L5charsS569.$0;
      moonbit_string_t _M0L3strS2091 = _M0L8_2afieldS3339;
      int32_t _M0L5startS2092 = _M0L5charsS569.$1;
      int32_t _M0L8_2afieldS3338 = _M0L5charsS569.$2;
      int32_t _M0L3endS2094;
      int64_t _M0L6_2atmpS2093;
      int64_t _M0L6_2atmpS2090;
      int32_t _M0L6_2atmpS2089;
      int32_t _M0L4_2acS570;
      moonbit_incref(_M0L3strS2088);
      _M0L3endS2094 = _M0L8_2afieldS3338;
      _M0L6_2atmpS2093 = (int64_t)_M0L3endS2094;
      #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2090
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2091, 0, _M0L5startS2092, _M0L6_2atmpS2093);
      _M0L6_2atmpS2089 = (int32_t)_M0L6_2atmpS2090;
      #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L4_2acS570
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2088, _M0L6_2atmpS2089);
      #line 545 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      return _M0MPC16string10StringView14contains__char(_M0L4selfS571, _M0L4_2acS570);
    } else {
      struct _M0TWEOc* _M0L5_2aitS572;
      #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L5_2aitS572 = _M0MPC16string10StringView4iter(_M0L4selfS571);
      while (1) {
        int32_t _M0L7_2abindS573;
        moonbit_incref(_M0L5_2aitS572);
        #line 547 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0L7_2abindS573 = _M0MPB4Iter4nextGcE(_M0L5_2aitS572);
        if (_M0L7_2abindS573 == -1) {
          moonbit_decref(_M0L5_2aitS572);
          moonbit_decref(_M0L5charsS569.$0);
          return 0;
        } else {
          int32_t _M0L7_2aSomeS574 = _M0L7_2abindS573;
          int32_t _M0L4_2acS575 = _M0L7_2aSomeS574;
          moonbit_incref(_M0L5charsS569.$0);
          #line 548 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          if (
            _M0MPC16string10StringView14contains__char(_M0L5charsS569, _M0L4_2acS575)
          ) {
            moonbit_decref(_M0L5_2aitS572);
            moonbit_decref(_M0L5charsS569.$0);
            return 1;
          }
          continue;
        }
        break;
      }
    }
  }
}

int32_t _M0MPC16string10StringView14contains__char(
  struct _M0TPC16string10StringView _M0L4selfS559,
  int32_t _M0L1cS561
) {
  int32_t _M0L3lenS558;
  #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L4selfS559.$0);
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L3lenS558 = _M0MPC16string10StringView6length(_M0L4selfS559);
  if (_M0L3lenS558 > 0) {
    int32_t _M0L1cS560 = _M0L1cS561;
    if (_M0L1cS560 <= 65535) {
      int32_t _M0L1iS562 = 0;
      while (1) {
        if (_M0L1iS562 < _M0L3lenS558) {
          int32_t _M0L6_2atmpS2066;
          int32_t _M0L6_2atmpS2065;
          int32_t _M0L6_2atmpS2067;
          moonbit_incref(_M0L4selfS559.$0);
          #line 598 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2066
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS559, _M0L1iS562);
          _M0L6_2atmpS2065 = (int32_t)_M0L6_2atmpS2066;
          if (_M0L6_2atmpS2065 == _M0L1cS560) {
            moonbit_decref(_M0L4selfS559.$0);
            return 1;
          }
          _M0L6_2atmpS2067 = _M0L1iS562 + 1;
          _M0L1iS562 = _M0L6_2atmpS2067;
          continue;
        } else {
          moonbit_decref(_M0L4selfS559.$0);
        }
        break;
      }
    } else if (_M0L3lenS558 >= 2) {
      int32_t _M0L3adjS564 = _M0L1cS560 - 65536;
      int32_t _M0L6_2atmpS2079 = _M0L3adjS564 >> 10;
      int32_t _M0L4highS565 = 55296 + _M0L6_2atmpS2079;
      int32_t _M0L6_2atmpS2078 = _M0L3adjS564 & 1023;
      int32_t _M0L3lowS566 = 56320 + _M0L6_2atmpS2078;
      int32_t _M0Lm1iS567 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2068 = _M0Lm1iS567;
        int32_t _M0L6_2atmpS2069 = _M0L3lenS558 - 1;
        if (_M0L6_2atmpS2068 < _M0L6_2atmpS2069) {
          int32_t _M0L6_2atmpS2072 = _M0Lm1iS567;
          int32_t _M0L6_2atmpS2071;
          int32_t _M0L6_2atmpS2070;
          int32_t _M0L6_2atmpS2077;
          moonbit_incref(_M0L4selfS559.$0);
          #line 612 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2071
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS559, _M0L6_2atmpS2072);
          _M0L6_2atmpS2070 = (int32_t)_M0L6_2atmpS2071;
          if (_M0L6_2atmpS2070 == _M0L4highS565) {
            int32_t _M0L6_2atmpS2073 = _M0Lm1iS567;
            int32_t _M0L6_2atmpS2076;
            int32_t _M0L6_2atmpS2075;
            int32_t _M0L6_2atmpS2074;
            _M0Lm1iS567 = _M0L6_2atmpS2073 + 1;
            _M0L6_2atmpS2076 = _M0Lm1iS567;
            moonbit_incref(_M0L4selfS559.$0);
            #line 614 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            _M0L6_2atmpS2075
            = _M0MPC16string10StringView11unsafe__get(_M0L4selfS559, _M0L6_2atmpS2076);
            _M0L6_2atmpS2074 = (int32_t)_M0L6_2atmpS2075;
            if (_M0L6_2atmpS2074 == _M0L3lowS566) {
              moonbit_decref(_M0L4selfS559.$0);
              return 1;
            }
          }
          _M0L6_2atmpS2077 = _M0Lm1iS567;
          _M0Lm1iS567 = _M0L6_2atmpS2077 + 1;
          continue;
        } else {
          moonbit_decref(_M0L4selfS559.$0);
        }
        break;
      }
    } else {
      moonbit_decref(_M0L4selfS559.$0);
      return 0;
    }
    return 0;
  } else {
    moonbit_decref(_M0L4selfS559.$0);
    return 0;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS549,
  moonbit_string_t _M0L5valueS551
) {
  int32_t _M0L3lenS2050;
  moonbit_string_t* _M0L6_2atmpS2052;
  int32_t _M0L6_2atmpS3345;
  int32_t _M0L6_2atmpS2051;
  int32_t _M0L6lengthS550;
  moonbit_string_t* _M0L8_2afieldS3344;
  moonbit_string_t* _M0L3bufS2053;
  moonbit_string_t _M0L6_2aoldS3343;
  int32_t _M0L6_2atmpS2054;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2050 = _M0L4selfS549->$1;
  moonbit_incref(_M0L4selfS549);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2052 = _M0MPC15array5Array6bufferGsE(_M0L4selfS549);
  _M0L6_2atmpS3345 = Moonbit_array_length(_M0L6_2atmpS2052);
  moonbit_decref(_M0L6_2atmpS2052);
  _M0L6_2atmpS2051 = _M0L6_2atmpS3345;
  if (_M0L3lenS2050 == _M0L6_2atmpS2051) {
    moonbit_incref(_M0L4selfS549);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS549);
  }
  _M0L6lengthS550 = _M0L4selfS549->$1;
  _M0L8_2afieldS3344 = _M0L4selfS549->$0;
  _M0L3bufS2053 = _M0L8_2afieldS3344;
  _M0L6_2aoldS3343 = (moonbit_string_t)_M0L3bufS2053[_M0L6lengthS550];
  moonbit_decref(_M0L6_2aoldS3343);
  _M0L3bufS2053[_M0L6lengthS550] = _M0L5valueS551;
  _M0L6_2atmpS2054 = _M0L6lengthS550 + 1;
  _M0L4selfS549->$1 = _M0L6_2atmpS2054;
  moonbit_decref(_M0L4selfS549);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS552,
  struct _M0TUsiE* _M0L5valueS554
) {
  int32_t _M0L3lenS2055;
  struct _M0TUsiE** _M0L6_2atmpS2057;
  int32_t _M0L6_2atmpS3348;
  int32_t _M0L6_2atmpS2056;
  int32_t _M0L6lengthS553;
  struct _M0TUsiE** _M0L8_2afieldS3347;
  struct _M0TUsiE** _M0L3bufS2058;
  struct _M0TUsiE* _M0L6_2aoldS3346;
  int32_t _M0L6_2atmpS2059;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2055 = _M0L4selfS552->$1;
  moonbit_incref(_M0L4selfS552);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2057 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS552);
  _M0L6_2atmpS3348 = Moonbit_array_length(_M0L6_2atmpS2057);
  moonbit_decref(_M0L6_2atmpS2057);
  _M0L6_2atmpS2056 = _M0L6_2atmpS3348;
  if (_M0L3lenS2055 == _M0L6_2atmpS2056) {
    moonbit_incref(_M0L4selfS552);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS552);
  }
  _M0L6lengthS553 = _M0L4selfS552->$1;
  _M0L8_2afieldS3347 = _M0L4selfS552->$0;
  _M0L3bufS2058 = _M0L8_2afieldS3347;
  _M0L6_2aoldS3346 = (struct _M0TUsiE*)_M0L3bufS2058[_M0L6lengthS553];
  if (_M0L6_2aoldS3346) {
    moonbit_decref(_M0L6_2aoldS3346);
  }
  _M0L3bufS2058[_M0L6lengthS553] = _M0L5valueS554;
  _M0L6_2atmpS2059 = _M0L6lengthS553 + 1;
  _M0L4selfS552->$1 = _M0L6_2atmpS2059;
  moonbit_decref(_M0L4selfS552);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS555,
  void* _M0L5valueS557
) {
  int32_t _M0L3lenS2060;
  void** _M0L6_2atmpS2062;
  int32_t _M0L6_2atmpS3351;
  int32_t _M0L6_2atmpS2061;
  int32_t _M0L6lengthS556;
  void** _M0L8_2afieldS3350;
  void** _M0L3bufS2063;
  void* _M0L6_2aoldS3349;
  int32_t _M0L6_2atmpS2064;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2060 = _M0L4selfS555->$1;
  moonbit_incref(_M0L4selfS555);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2062
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS555);
  _M0L6_2atmpS3351 = Moonbit_array_length(_M0L6_2atmpS2062);
  moonbit_decref(_M0L6_2atmpS2062);
  _M0L6_2atmpS2061 = _M0L6_2atmpS3351;
  if (_M0L3lenS2060 == _M0L6_2atmpS2061) {
    moonbit_incref(_M0L4selfS555);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS555);
  }
  _M0L6lengthS556 = _M0L4selfS555->$1;
  _M0L8_2afieldS3350 = _M0L4selfS555->$0;
  _M0L3bufS2063 = _M0L8_2afieldS3350;
  _M0L6_2aoldS3349 = (void*)_M0L3bufS2063[_M0L6lengthS556];
  moonbit_decref(_M0L6_2aoldS3349);
  _M0L3bufS2063[_M0L6lengthS556] = _M0L5valueS557;
  _M0L6_2atmpS2064 = _M0L6lengthS556 + 1;
  _M0L4selfS555->$1 = _M0L6_2atmpS2064;
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
  moonbit_string_t* _M0L8_2afieldS3353;
  moonbit_string_t* _M0L8old__bufS524;
  int32_t _M0L8old__capS526;
  int32_t _M0L9copy__lenS527;
  moonbit_string_t* _M0L6_2aoldS3352;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS522
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS523, (moonbit_string_t)moonbit_string_literal_1.data);
  _M0L8_2afieldS3353 = _M0L4selfS525->$0;
  _M0L8old__bufS524 = _M0L8_2afieldS3353;
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
  _M0L6_2aoldS3352 = _M0L4selfS525->$0;
  moonbit_decref(_M0L6_2aoldS3352);
  _M0L4selfS525->$0 = _M0L8new__bufS522;
  moonbit_decref(_M0L4selfS525);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS531,
  int32_t _M0L13new__capacityS529
) {
  struct _M0TUsiE** _M0L8new__bufS528;
  struct _M0TUsiE** _M0L8_2afieldS3355;
  struct _M0TUsiE** _M0L8old__bufS530;
  int32_t _M0L8old__capS532;
  int32_t _M0L9copy__lenS533;
  struct _M0TUsiE** _M0L6_2aoldS3354;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS528
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS529, 0);
  _M0L8_2afieldS3355 = _M0L4selfS531->$0;
  _M0L8old__bufS530 = _M0L8_2afieldS3355;
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
  _M0L6_2aoldS3354 = _M0L4selfS531->$0;
  moonbit_decref(_M0L6_2aoldS3354);
  _M0L4selfS531->$0 = _M0L8new__bufS528;
  moonbit_decref(_M0L4selfS531);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS537,
  int32_t _M0L13new__capacityS535
) {
  void** _M0L8new__bufS534;
  void** _M0L8_2afieldS3357;
  void** _M0L8old__bufS536;
  int32_t _M0L8old__capS538;
  int32_t _M0L9copy__lenS539;
  void** _M0L6_2aoldS3356;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS534
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS535, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3357 = _M0L4selfS537->$0;
  _M0L8old__bufS536 = _M0L8_2afieldS3357;
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
  _M0L6_2aoldS3356 = _M0L4selfS537->$0;
  moonbit_decref(_M0L6_2aoldS3356);
  _M0L4selfS537->$0 = _M0L8new__bufS534;
  moonbit_decref(_M0L4selfS537);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS521
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS521 == 0) {
    moonbit_string_t* _M0L6_2atmpS2048 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3695 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3695)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3695->$0 = _M0L6_2atmpS2048;
    _block_3695->$1 = 0;
    return _block_3695;
  } else {
    moonbit_string_t* _M0L6_2atmpS2049 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS521, (moonbit_string_t)moonbit_string_literal_1.data);
    struct _M0TPB5ArrayGsE* _block_3696 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3696)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3696->$0 = _M0L6_2atmpS2049;
    _block_3696->$1 = 0;
    return _block_3696;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS515,
  int32_t _M0L1nS514
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS514 <= 0) {
    moonbit_decref(_M0L4selfS515);
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else if (_M0L1nS514 == 1) {
    return _M0L4selfS515;
  } else {
    int32_t _M0L3lenS516 = Moonbit_array_length(_M0L4selfS515);
    int32_t _M0L6_2atmpS2047 = _M0L3lenS516 * _M0L1nS514;
    struct _M0TPB13StringBuilder* _M0L3bufS517;
    moonbit_string_t _M0L3strS518;
    int32_t _M0L2__S519;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS517 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2047);
    _M0L3strS518 = _M0L4selfS515;
    _M0L2__S519 = 0;
    while (1) {
      if (_M0L2__S519 < _M0L1nS514) {
        int32_t _M0L6_2atmpS2046;
        moonbit_incref(_M0L3strS518);
        moonbit_incref(_M0L3bufS517);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS517, _M0L3strS518);
        _M0L6_2atmpS2046 = _M0L2__S519 + 1;
        _M0L2__S519 = _M0L6_2atmpS2046;
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
  int32_t _M0L3lenS2034;
  int32_t _M0L6_2atmpS2036;
  int32_t _M0L6_2atmpS2035;
  int32_t _M0L6_2atmpS2033;
  moonbit_bytes_t _M0L8_2afieldS3358;
  moonbit_bytes_t _M0L4dataS2037;
  int32_t _M0L3lenS2038;
  moonbit_string_t _M0L6_2atmpS2039;
  int32_t _M0L6_2atmpS2040;
  int32_t _M0L6_2atmpS2041;
  int32_t _M0L3lenS2043;
  int32_t _M0L6_2atmpS2045;
  int32_t _M0L6_2atmpS2044;
  int32_t _M0L6_2atmpS2042;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2034 = _M0L4selfS512->$1;
  moonbit_incref(_M0L3strS513.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2036 = _M0MPC16string10StringView6length(_M0L3strS513);
  _M0L6_2atmpS2035 = _M0L6_2atmpS2036 * 2;
  _M0L6_2atmpS2033 = _M0L3lenS2034 + _M0L6_2atmpS2035;
  moonbit_incref(_M0L4selfS512);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS512, _M0L6_2atmpS2033);
  _M0L8_2afieldS3358 = _M0L4selfS512->$0;
  _M0L4dataS2037 = _M0L8_2afieldS3358;
  _M0L3lenS2038 = _M0L4selfS512->$1;
  moonbit_incref(_M0L4dataS2037);
  moonbit_incref(_M0L3strS513.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2039 = _M0MPC16string10StringView4data(_M0L3strS513);
  moonbit_incref(_M0L3strS513.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2040 = _M0MPC16string10StringView13start__offset(_M0L3strS513);
  moonbit_incref(_M0L3strS513.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2041 = _M0MPC16string10StringView6length(_M0L3strS513);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2037, _M0L3lenS2038, _M0L6_2atmpS2039, _M0L6_2atmpS2040, _M0L6_2atmpS2041);
  _M0L3lenS2043 = _M0L4selfS512->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2045 = _M0MPC16string10StringView6length(_M0L3strS513);
  _M0L6_2atmpS2044 = _M0L6_2atmpS2045 * 2;
  _M0L6_2atmpS2042 = _M0L3lenS2043 + _M0L6_2atmpS2044;
  _M0L4selfS512->$1 = _M0L6_2atmpS2042;
  moonbit_decref(_M0L4selfS512);
  return 0;
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS509,
  int32_t _M0L1iS510,
  int32_t _M0L13start__offsetS511,
  int64_t _M0L11end__offsetS507
) {
  int32_t _M0L11end__offsetS506;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS507 == 4294967296ll) {
    _M0L11end__offsetS506 = Moonbit_array_length(_M0L4selfS509);
  } else {
    int64_t _M0L7_2aSomeS508 = _M0L11end__offsetS507;
    _M0L11end__offsetS506 = (int32_t)_M0L7_2aSomeS508;
  }
  if (_M0L1iS510 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS509, _M0L1iS510, _M0L13start__offsetS511, _M0L11end__offsetS506);
  } else {
    int32_t _M0L6_2atmpS2032 = -_M0L1iS510;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS509, _M0L6_2atmpS2032, _M0L13start__offsetS511, _M0L11end__offsetS506);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS504,
  int32_t _M0L1nS502,
  int32_t _M0L13start__offsetS498,
  int32_t _M0L11end__offsetS499
) {
  int32_t _if__result_3698;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS498 >= 0) {
    _if__result_3698 = _M0L13start__offsetS498 <= _M0L11end__offsetS499;
  } else {
    _if__result_3698 = 0;
  }
  if (_if__result_3698) {
    int32_t _M0Lm13utf16__offsetS500 = _M0L13start__offsetS498;
    int32_t _M0Lm11char__countS501 = 0;
    int32_t _M0L6_2atmpS2030;
    int32_t _if__result_3701;
    while (1) {
      int32_t _M0L6_2atmpS2024 = _M0Lm13utf16__offsetS500;
      int32_t _if__result_3700;
      if (_M0L6_2atmpS2024 < _M0L11end__offsetS499) {
        int32_t _M0L6_2atmpS2023 = _M0Lm11char__countS501;
        _if__result_3700 = _M0L6_2atmpS2023 < _M0L1nS502;
      } else {
        _if__result_3700 = 0;
      }
      if (_if__result_3700) {
        int32_t _M0L6_2atmpS2028 = _M0Lm13utf16__offsetS500;
        int32_t _M0L1cS503 = _M0L4selfS504[_M0L6_2atmpS2028];
        int32_t _M0L6_2atmpS2027;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS503)) {
          int32_t _M0L6_2atmpS2025 = _M0Lm13utf16__offsetS500;
          _M0Lm13utf16__offsetS500 = _M0L6_2atmpS2025 + 2;
        } else {
          int32_t _M0L6_2atmpS2026 = _M0Lm13utf16__offsetS500;
          _M0Lm13utf16__offsetS500 = _M0L6_2atmpS2026 + 1;
        }
        _M0L6_2atmpS2027 = _M0Lm11char__countS501;
        _M0Lm11char__countS501 = _M0L6_2atmpS2027 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS504);
      }
      break;
    }
    _M0L6_2atmpS2030 = _M0Lm11char__countS501;
    if (_M0L6_2atmpS2030 < _M0L1nS502) {
      _if__result_3701 = 1;
    } else {
      int32_t _M0L6_2atmpS2029 = _M0Lm13utf16__offsetS500;
      _if__result_3701 = _M0L6_2atmpS2029 >= _M0L11end__offsetS499;
    }
    if (_if__result_3701) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2031 = _M0Lm13utf16__offsetS500;
      return (int64_t)_M0L6_2atmpS2031;
    }
  } else {
    moonbit_decref(_M0L4selfS504);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_65.data, (moonbit_string_t)moonbit_string_literal_66.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS496,
  int32_t _M0L1nS494,
  int32_t _M0L13start__offsetS493,
  int32_t _M0L11end__offsetS492
) {
  int32_t _M0Lm11char__countS490;
  int32_t _M0Lm13utf16__offsetS491;
  int32_t _M0L6_2atmpS2021;
  int32_t _if__result_3704;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS490 = 0;
  _M0Lm13utf16__offsetS491 = _M0L11end__offsetS492;
  while (1) {
    int32_t _M0L6_2atmpS2014 = _M0Lm13utf16__offsetS491;
    int32_t _M0L6_2atmpS2013 = _M0L6_2atmpS2014 - 1;
    int32_t _if__result_3703;
    if (_M0L6_2atmpS2013 >= _M0L13start__offsetS493) {
      int32_t _M0L6_2atmpS2012 = _M0Lm11char__countS490;
      _if__result_3703 = _M0L6_2atmpS2012 < _M0L1nS494;
    } else {
      _if__result_3703 = 0;
    }
    if (_if__result_3703) {
      int32_t _M0L6_2atmpS2019 = _M0Lm13utf16__offsetS491;
      int32_t _M0L6_2atmpS2018 = _M0L6_2atmpS2019 - 1;
      int32_t _M0L1cS495 = _M0L4selfS496[_M0L6_2atmpS2018];
      int32_t _M0L6_2atmpS2017;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS495)) {
        int32_t _M0L6_2atmpS2015 = _M0Lm13utf16__offsetS491;
        _M0Lm13utf16__offsetS491 = _M0L6_2atmpS2015 - 2;
      } else {
        int32_t _M0L6_2atmpS2016 = _M0Lm13utf16__offsetS491;
        _M0Lm13utf16__offsetS491 = _M0L6_2atmpS2016 - 1;
      }
      _M0L6_2atmpS2017 = _M0Lm11char__countS490;
      _M0Lm11char__countS490 = _M0L6_2atmpS2017 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS496);
    }
    break;
  }
  _M0L6_2atmpS2021 = _M0Lm11char__countS490;
  if (_M0L6_2atmpS2021 < _M0L1nS494) {
    _if__result_3704 = 1;
  } else {
    int32_t _M0L6_2atmpS2020 = _M0Lm13utf16__offsetS491;
    _if__result_3704 = _M0L6_2atmpS2020 < _M0L13start__offsetS493;
  }
  if (_if__result_3704) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2022 = _M0Lm13utf16__offsetS491;
    return (int64_t)_M0L6_2atmpS2022;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS482,
  int32_t _M0L3lenS485,
  int32_t _M0L13start__offsetS489,
  int64_t _M0L11end__offsetS480
) {
  int32_t _M0L11end__offsetS479;
  int32_t _M0L5indexS483;
  int32_t _M0L5countS484;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS480 == 4294967296ll) {
    _M0L11end__offsetS479 = Moonbit_array_length(_M0L4selfS482);
  } else {
    int64_t _M0L7_2aSomeS481 = _M0L11end__offsetS480;
    _M0L11end__offsetS479 = (int32_t)_M0L7_2aSomeS481;
  }
  _M0L5indexS483 = _M0L13start__offsetS489;
  _M0L5countS484 = 0;
  while (1) {
    int32_t _if__result_3706;
    if (_M0L5indexS483 < _M0L11end__offsetS479) {
      _if__result_3706 = _M0L5countS484 < _M0L3lenS485;
    } else {
      _if__result_3706 = 0;
    }
    if (_if__result_3706) {
      int32_t _M0L2c1S486 = _M0L4selfS482[_M0L5indexS483];
      int32_t _if__result_3707;
      int32_t _M0L6_2atmpS2010;
      int32_t _M0L6_2atmpS2011;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S486)) {
        int32_t _M0L6_2atmpS2006 = _M0L5indexS483 + 1;
        _if__result_3707 = _M0L6_2atmpS2006 < _M0L11end__offsetS479;
      } else {
        _if__result_3707 = 0;
      }
      if (_if__result_3707) {
        int32_t _M0L6_2atmpS2009 = _M0L5indexS483 + 1;
        int32_t _M0L2c2S487 = _M0L4selfS482[_M0L6_2atmpS2009];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S487)) {
          int32_t _M0L6_2atmpS2007 = _M0L5indexS483 + 2;
          int32_t _M0L6_2atmpS2008 = _M0L5countS484 + 1;
          _M0L5indexS483 = _M0L6_2atmpS2007;
          _M0L5countS484 = _M0L6_2atmpS2008;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_67.data, (moonbit_string_t)moonbit_string_literal_68.data);
        }
      }
      _M0L6_2atmpS2010 = _M0L5indexS483 + 1;
      _M0L6_2atmpS2011 = _M0L5countS484 + 1;
      _M0L5indexS483 = _M0L6_2atmpS2010;
      _M0L5countS484 = _M0L6_2atmpS2011;
      continue;
    } else {
      moonbit_decref(_M0L4selfS482);
      return _M0L5countS484 >= _M0L3lenS485;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS471,
  int32_t _M0L3lenS474,
  int32_t _M0L13start__offsetS478,
  int64_t _M0L11end__offsetS469
) {
  int32_t _M0L11end__offsetS468;
  int32_t _M0L5indexS472;
  int32_t _M0L5countS473;
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS469 == 4294967296ll) {
    _M0L11end__offsetS468 = Moonbit_array_length(_M0L4selfS471);
  } else {
    int64_t _M0L7_2aSomeS470 = _M0L11end__offsetS469;
    _M0L11end__offsetS468 = (int32_t)_M0L7_2aSomeS470;
  }
  _M0L5indexS472 = _M0L13start__offsetS478;
  _M0L5countS473 = 0;
  while (1) {
    int32_t _if__result_3709;
    if (_M0L5indexS472 < _M0L11end__offsetS468) {
      _if__result_3709 = _M0L5countS473 < _M0L3lenS474;
    } else {
      _if__result_3709 = 0;
    }
    if (_if__result_3709) {
      int32_t _M0L2c1S475 = _M0L4selfS471[_M0L5indexS472];
      int32_t _if__result_3710;
      int32_t _M0L6_2atmpS2004;
      int32_t _M0L6_2atmpS2005;
      #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S475)) {
        int32_t _M0L6_2atmpS2000 = _M0L5indexS472 + 1;
        _if__result_3710 = _M0L6_2atmpS2000 < _M0L11end__offsetS468;
      } else {
        _if__result_3710 = 0;
      }
      if (_if__result_3710) {
        int32_t _M0L6_2atmpS2003 = _M0L5indexS472 + 1;
        int32_t _M0L2c2S476 = _M0L4selfS471[_M0L6_2atmpS2003];
        #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S476)) {
          int32_t _M0L6_2atmpS2001 = _M0L5indexS472 + 2;
          int32_t _M0L6_2atmpS2002 = _M0L5countS473 + 1;
          _M0L5indexS472 = _M0L6_2atmpS2001;
          _M0L5countS473 = _M0L6_2atmpS2002;
          continue;
        } else {
          #line 426 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_67.data, (moonbit_string_t)moonbit_string_literal_69.data);
        }
      }
      _M0L6_2atmpS2004 = _M0L5indexS472 + 1;
      _M0L6_2atmpS2005 = _M0L5countS473 + 1;
      _M0L5indexS472 = _M0L6_2atmpS2004;
      _M0L5countS473 = _M0L6_2atmpS2005;
      continue;
    } else {
      moonbit_decref(_M0L4selfS471);
      if (_M0L5countS473 == _M0L3lenS474) {
        return _M0L5indexS472 == _M0L11end__offsetS468;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS464
) {
  int32_t _M0L3endS1992;
  int32_t _M0L8_2afieldS3359;
  int32_t _M0L5startS1993;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1992 = _M0L4selfS464.$2;
  _M0L8_2afieldS3359 = _M0L4selfS464.$1;
  moonbit_decref(_M0L4selfS464.$0);
  _M0L5startS1993 = _M0L8_2afieldS3359;
  return _M0L3endS1992 - _M0L5startS1993;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS465
) {
  int32_t _M0L3endS1994;
  int32_t _M0L8_2afieldS3360;
  int32_t _M0L5startS1995;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1994 = _M0L4selfS465.$2;
  _M0L8_2afieldS3360 = _M0L4selfS465.$1;
  moonbit_decref(_M0L4selfS465.$0);
  _M0L5startS1995 = _M0L8_2afieldS3360;
  return _M0L3endS1994 - _M0L5startS1995;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS466
) {
  int32_t _M0L3endS1996;
  int32_t _M0L8_2afieldS3361;
  int32_t _M0L5startS1997;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1996 = _M0L4selfS466.$2;
  _M0L8_2afieldS3361 = _M0L4selfS466.$1;
  moonbit_decref(_M0L4selfS466.$0);
  _M0L5startS1997 = _M0L8_2afieldS3361;
  return _M0L3endS1996 - _M0L5startS1997;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS467
) {
  int32_t _M0L3endS1998;
  int32_t _M0L8_2afieldS3362;
  int32_t _M0L5startS1999;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1998 = _M0L4selfS467.$2;
  _M0L8_2afieldS3362 = _M0L4selfS467.$1;
  moonbit_decref(_M0L4selfS467.$0);
  _M0L5startS1999 = _M0L8_2afieldS3362;
  return _M0L3endS1998 - _M0L5startS1999;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS462,
  int64_t _M0L19start__offset_2eoptS460,
  int64_t _M0L11end__offsetS463
) {
  int32_t _M0L13start__offsetS459;
  if (_M0L19start__offset_2eoptS460 == 4294967296ll) {
    _M0L13start__offsetS459 = 0;
  } else {
    int64_t _M0L7_2aSomeS461 = _M0L19start__offset_2eoptS460;
    _M0L13start__offsetS459 = (int32_t)_M0L7_2aSomeS461;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS462, _M0L13start__offsetS459, _M0L11end__offsetS463);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS457,
  int32_t _M0L13start__offsetS458,
  int64_t _M0L11end__offsetS455
) {
  int32_t _M0L11end__offsetS454;
  int32_t _if__result_3711;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS455 == 4294967296ll) {
    _M0L11end__offsetS454 = Moonbit_array_length(_M0L4selfS457);
  } else {
    int64_t _M0L7_2aSomeS456 = _M0L11end__offsetS455;
    _M0L11end__offsetS454 = (int32_t)_M0L7_2aSomeS456;
  }
  if (_M0L13start__offsetS458 >= 0) {
    if (_M0L13start__offsetS458 <= _M0L11end__offsetS454) {
      int32_t _M0L6_2atmpS1991 = Moonbit_array_length(_M0L4selfS457);
      _if__result_3711 = _M0L11end__offsetS454 <= _M0L6_2atmpS1991;
    } else {
      _if__result_3711 = 0;
    }
  } else {
    _if__result_3711 = 0;
  }
  if (_if__result_3711) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS458,
                                                 _M0L11end__offsetS454,
                                                 _M0L4selfS457};
  } else {
    moonbit_decref(_M0L4selfS457);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_70.data, (moonbit_string_t)moonbit_string_literal_71.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS449
) {
  int32_t _M0L5startS448;
  int32_t _M0L3endS450;
  struct _M0TPC13ref3RefGiE* _M0L5indexS451;
  struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__* _closure_3712;
  struct _M0TWEOc* _M0L6_2atmpS1970;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L5startS448 = _M0L4selfS449.$1;
  _M0L3endS450 = _M0L4selfS449.$2;
  _M0L5indexS451
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS451)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS451->$0 = _M0L5startS448;
  _closure_3712
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__));
  Moonbit_object_header(_closure_3712)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__, $0) >> 2, 2, 0);
  _closure_3712->code = &_M0MPC16string10StringView4iterC1971l198;
  _closure_3712->$0 = _M0L5indexS451;
  _closure_3712->$1 = _M0L3endS450;
  _closure_3712->$2_0 = _M0L4selfS449.$0;
  _closure_3712->$2_1 = _M0L4selfS449.$1;
  _closure_3712->$2_2 = _M0L4selfS449.$2;
  _M0L6_2atmpS1970 = (struct _M0TWEOc*)_closure_3712;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1970);
}

int32_t _M0MPC16string10StringView4iterC1971l198(
  struct _M0TWEOc* _M0L6_2aenvS1972
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__* _M0L14_2acasted__envS1973;
  struct _M0TPC16string10StringView _M0L8_2afieldS3368;
  struct _M0TPC16string10StringView _M0L4selfS449;
  int32_t _M0L3endS450;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3367;
  int32_t _M0L6_2acntS3541;
  struct _M0TPC13ref3RefGiE* _M0L5indexS451;
  int32_t _M0L3valS1974;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L14_2acasted__envS1973
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1971__l198__*)_M0L6_2aenvS1972;
  _M0L8_2afieldS3368
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS1973->$2_1,
      _M0L14_2acasted__envS1973->$2_2,
      _M0L14_2acasted__envS1973->$2_0
  };
  _M0L4selfS449 = _M0L8_2afieldS3368;
  _M0L3endS450 = _M0L14_2acasted__envS1973->$1;
  _M0L8_2afieldS3367 = _M0L14_2acasted__envS1973->$0;
  _M0L6_2acntS3541 = Moonbit_object_header(_M0L14_2acasted__envS1973)->rc;
  if (_M0L6_2acntS3541 > 1) {
    int32_t _M0L11_2anew__cntS3542 = _M0L6_2acntS3541 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1973)->rc
    = _M0L11_2anew__cntS3542;
    moonbit_incref(_M0L4selfS449.$0);
    moonbit_incref(_M0L8_2afieldS3367);
  } else if (_M0L6_2acntS3541 == 1) {
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS1973);
  }
  _M0L5indexS451 = _M0L8_2afieldS3367;
  _M0L3valS1974 = _M0L5indexS451->$0;
  if (_M0L3valS1974 < _M0L3endS450) {
    moonbit_string_t _M0L8_2afieldS3366 = _M0L4selfS449.$0;
    moonbit_string_t _M0L3strS1989 = _M0L8_2afieldS3366;
    int32_t _M0L3valS1990 = _M0L5indexS451->$0;
    int32_t _M0L6_2atmpS3365 = _M0L3strS1989[_M0L3valS1990];
    int32_t _M0L2c1S452 = _M0L6_2atmpS3365;
    int32_t _if__result_3713;
    int32_t _M0L3valS1987;
    int32_t _M0L6_2atmpS1986;
    int32_t _M0L6_2atmpS1988;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S452)) {
      int32_t _M0L3valS1977 = _M0L5indexS451->$0;
      int32_t _M0L6_2atmpS1975 = _M0L3valS1977 + 1;
      int32_t _M0L3endS1976 = _M0L4selfS449.$2;
      _if__result_3713 = _M0L6_2atmpS1975 < _M0L3endS1976;
    } else {
      _if__result_3713 = 0;
    }
    if (_if__result_3713) {
      moonbit_string_t _M0L8_2afieldS3364 = _M0L4selfS449.$0;
      moonbit_string_t _M0L3strS1983 = _M0L8_2afieldS3364;
      int32_t _M0L3valS1985 = _M0L5indexS451->$0;
      int32_t _M0L6_2atmpS1984 = _M0L3valS1985 + 1;
      int32_t _M0L6_2atmpS3363 = _M0L3strS1983[_M0L6_2atmpS1984];
      int32_t _M0L2c2S453;
      moonbit_decref(_M0L3strS1983);
      _M0L2c2S453 = _M0L6_2atmpS3363;
      #line 203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S453)) {
        int32_t _M0L3valS1979 = _M0L5indexS451->$0;
        int32_t _M0L6_2atmpS1978 = _M0L3valS1979 + 2;
        int32_t _M0L6_2atmpS1981;
        int32_t _M0L6_2atmpS1982;
        int32_t _M0L6_2atmpS1980;
        _M0L5indexS451->$0 = _M0L6_2atmpS1978;
        moonbit_decref(_M0L5indexS451);
        _M0L6_2atmpS1981 = (int32_t)_M0L2c1S452;
        _M0L6_2atmpS1982 = (int32_t)_M0L2c2S453;
        #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        _M0L6_2atmpS1980
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1981, _M0L6_2atmpS1982);
        return _M0L6_2atmpS1980;
      }
    } else {
      moonbit_decref(_M0L4selfS449.$0);
    }
    _M0L3valS1987 = _M0L5indexS451->$0;
    _M0L6_2atmpS1986 = _M0L3valS1987 + 1;
    _M0L5indexS451->$0 = _M0L6_2atmpS1986;
    moonbit_decref(_M0L5indexS451);
    #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L6_2atmpS1988 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S452);
    return _M0L6_2atmpS1988;
  } else {
    moonbit_decref(_M0L5indexS451);
    moonbit_decref(_M0L4selfS449.$0);
    return -1;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS447
) {
  moonbit_string_t _M0L8_2afieldS3370;
  moonbit_string_t _M0L3strS1967;
  int32_t _M0L5startS1968;
  int32_t _M0L8_2afieldS3369;
  int32_t _M0L3endS1969;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3370 = _M0L4selfS447.$0;
  _M0L3strS1967 = _M0L8_2afieldS3370;
  _M0L5startS1968 = _M0L4selfS447.$1;
  _M0L8_2afieldS3369 = _M0L4selfS447.$2;
  _M0L3endS1969 = _M0L8_2afieldS3369;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1967, _M0L5startS1968, _M0L3endS1969);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS445,
  struct _M0TPB6Logger _M0L6loggerS446
) {
  moonbit_string_t _M0L8_2afieldS3372;
  moonbit_string_t _M0L3strS1964;
  int32_t _M0L5startS1965;
  int32_t _M0L8_2afieldS3371;
  int32_t _M0L3endS1966;
  moonbit_string_t _M0L6substrS444;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3372 = _M0L4selfS445.$0;
  _M0L3strS1964 = _M0L8_2afieldS3372;
  _M0L5startS1965 = _M0L4selfS445.$1;
  _M0L8_2afieldS3371 = _M0L4selfS445.$2;
  _M0L3endS1966 = _M0L8_2afieldS3371;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS444
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1964, _M0L5startS1965, _M0L3endS1966);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS444, _M0L6loggerS446);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS436,
  struct _M0TPB6Logger _M0L6loggerS434
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS435;
  int32_t _M0L3lenS437;
  int32_t _M0L1iS438;
  int32_t _M0L3segS439;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS434.$1) {
    moonbit_incref(_M0L6loggerS434.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS434.$0->$method_3(_M0L6loggerS434.$1, 34);
  moonbit_incref(_M0L4selfS436);
  if (_M0L6loggerS434.$1) {
    moonbit_incref(_M0L6loggerS434.$1);
  }
  _M0L6_2aenvS435
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS435)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS435->$0 = _M0L4selfS436;
  _M0L6_2aenvS435->$1_0 = _M0L6loggerS434.$0;
  _M0L6_2aenvS435->$1_1 = _M0L6loggerS434.$1;
  _M0L3lenS437 = Moonbit_array_length(_M0L4selfS436);
  _M0L1iS438 = 0;
  _M0L3segS439 = 0;
  _2afor_440:;
  while (1) {
    int32_t _M0L4codeS441;
    int32_t _M0L1cS443;
    int32_t _M0L6_2atmpS1948;
    int32_t _M0L6_2atmpS1949;
    int32_t _M0L6_2atmpS1950;
    int32_t _tmp_3717;
    int32_t _tmp_3718;
    if (_M0L1iS438 >= _M0L3lenS437) {
      moonbit_decref(_M0L4selfS436);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS435, _M0L3segS439, _M0L1iS438);
      break;
    }
    _M0L4codeS441 = _M0L4selfS436[_M0L1iS438];
    switch (_M0L4codeS441) {
      case 34: {
        _M0L1cS443 = _M0L4codeS441;
        goto join_442;
        break;
      }
      
      case 92: {
        _M0L1cS443 = _M0L4codeS441;
        goto join_442;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1951;
        int32_t _M0L6_2atmpS1952;
        moonbit_incref(_M0L6_2aenvS435);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS435, _M0L3segS439, _M0L1iS438);
        if (_M0L6loggerS434.$1) {
          moonbit_incref(_M0L6loggerS434.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS434.$0->$method_0(_M0L6loggerS434.$1, (moonbit_string_t)moonbit_string_literal_48.data);
        _M0L6_2atmpS1951 = _M0L1iS438 + 1;
        _M0L6_2atmpS1952 = _M0L1iS438 + 1;
        _M0L1iS438 = _M0L6_2atmpS1951;
        _M0L3segS439 = _M0L6_2atmpS1952;
        goto _2afor_440;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1953;
        int32_t _M0L6_2atmpS1954;
        moonbit_incref(_M0L6_2aenvS435);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS435, _M0L3segS439, _M0L1iS438);
        if (_M0L6loggerS434.$1) {
          moonbit_incref(_M0L6loggerS434.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS434.$0->$method_0(_M0L6loggerS434.$1, (moonbit_string_t)moonbit_string_literal_49.data);
        _M0L6_2atmpS1953 = _M0L1iS438 + 1;
        _M0L6_2atmpS1954 = _M0L1iS438 + 1;
        _M0L1iS438 = _M0L6_2atmpS1953;
        _M0L3segS439 = _M0L6_2atmpS1954;
        goto _2afor_440;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1955;
        int32_t _M0L6_2atmpS1956;
        moonbit_incref(_M0L6_2aenvS435);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS435, _M0L3segS439, _M0L1iS438);
        if (_M0L6loggerS434.$1) {
          moonbit_incref(_M0L6loggerS434.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS434.$0->$method_0(_M0L6loggerS434.$1, (moonbit_string_t)moonbit_string_literal_50.data);
        _M0L6_2atmpS1955 = _M0L1iS438 + 1;
        _M0L6_2atmpS1956 = _M0L1iS438 + 1;
        _M0L1iS438 = _M0L6_2atmpS1955;
        _M0L3segS439 = _M0L6_2atmpS1956;
        goto _2afor_440;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1957;
        int32_t _M0L6_2atmpS1958;
        moonbit_incref(_M0L6_2aenvS435);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS435, _M0L3segS439, _M0L1iS438);
        if (_M0L6loggerS434.$1) {
          moonbit_incref(_M0L6loggerS434.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS434.$0->$method_0(_M0L6loggerS434.$1, (moonbit_string_t)moonbit_string_literal_51.data);
        _M0L6_2atmpS1957 = _M0L1iS438 + 1;
        _M0L6_2atmpS1958 = _M0L1iS438 + 1;
        _M0L1iS438 = _M0L6_2atmpS1957;
        _M0L3segS439 = _M0L6_2atmpS1958;
        goto _2afor_440;
        break;
      }
      default: {
        if (_M0L4codeS441 < 32) {
          int32_t _M0L6_2atmpS1960;
          moonbit_string_t _M0L6_2atmpS1959;
          int32_t _M0L6_2atmpS1961;
          int32_t _M0L6_2atmpS1962;
          moonbit_incref(_M0L6_2aenvS435);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS435, _M0L3segS439, _M0L1iS438);
          if (_M0L6loggerS434.$1) {
            moonbit_incref(_M0L6loggerS434.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS434.$0->$method_0(_M0L6loggerS434.$1, (moonbit_string_t)moonbit_string_literal_72.data);
          _M0L6_2atmpS1960 = _M0L4codeS441 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1959 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1960);
          if (_M0L6loggerS434.$1) {
            moonbit_incref(_M0L6loggerS434.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS434.$0->$method_0(_M0L6loggerS434.$1, _M0L6_2atmpS1959);
          if (_M0L6loggerS434.$1) {
            moonbit_incref(_M0L6loggerS434.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS434.$0->$method_3(_M0L6loggerS434.$1, 125);
          _M0L6_2atmpS1961 = _M0L1iS438 + 1;
          _M0L6_2atmpS1962 = _M0L1iS438 + 1;
          _M0L1iS438 = _M0L6_2atmpS1961;
          _M0L3segS439 = _M0L6_2atmpS1962;
          goto _2afor_440;
        } else {
          int32_t _M0L6_2atmpS1963 = _M0L1iS438 + 1;
          int32_t _tmp_3716 = _M0L3segS439;
          _M0L1iS438 = _M0L6_2atmpS1963;
          _M0L3segS439 = _tmp_3716;
          goto _2afor_440;
        }
        break;
      }
    }
    goto joinlet_3715;
    join_442:;
    moonbit_incref(_M0L6_2aenvS435);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS435, _M0L3segS439, _M0L1iS438);
    if (_M0L6loggerS434.$1) {
      moonbit_incref(_M0L6loggerS434.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS434.$0->$method_3(_M0L6loggerS434.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1948 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS443);
    if (_M0L6loggerS434.$1) {
      moonbit_incref(_M0L6loggerS434.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS434.$0->$method_3(_M0L6loggerS434.$1, _M0L6_2atmpS1948);
    _M0L6_2atmpS1949 = _M0L1iS438 + 1;
    _M0L6_2atmpS1950 = _M0L1iS438 + 1;
    _M0L1iS438 = _M0L6_2atmpS1949;
    _M0L3segS439 = _M0L6_2atmpS1950;
    continue;
    joinlet_3715:;
    _tmp_3717 = _M0L1iS438;
    _tmp_3718 = _M0L3segS439;
    _M0L1iS438 = _tmp_3717;
    _M0L3segS439 = _tmp_3718;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS434.$0->$method_3(_M0L6loggerS434.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS430,
  int32_t _M0L3segS433,
  int32_t _M0L1iS432
) {
  struct _M0TPB6Logger _M0L8_2afieldS3374;
  struct _M0TPB6Logger _M0L6loggerS429;
  moonbit_string_t _M0L8_2afieldS3373;
  int32_t _M0L6_2acntS3543;
  moonbit_string_t _M0L4selfS431;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3374
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS430->$1_0, _M0L6_2aenvS430->$1_1
  };
  _M0L6loggerS429 = _M0L8_2afieldS3374;
  _M0L8_2afieldS3373 = _M0L6_2aenvS430->$0;
  _M0L6_2acntS3543 = Moonbit_object_header(_M0L6_2aenvS430)->rc;
  if (_M0L6_2acntS3543 > 1) {
    int32_t _M0L11_2anew__cntS3544 = _M0L6_2acntS3543 - 1;
    Moonbit_object_header(_M0L6_2aenvS430)->rc = _M0L11_2anew__cntS3544;
    if (_M0L6loggerS429.$1) {
      moonbit_incref(_M0L6loggerS429.$1);
    }
    moonbit_incref(_M0L8_2afieldS3373);
  } else if (_M0L6_2acntS3543 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS430);
  }
  _M0L4selfS431 = _M0L8_2afieldS3373;
  if (_M0L1iS432 > _M0L3segS433) {
    int32_t _M0L6_2atmpS1947 = _M0L1iS432 - _M0L3segS433;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS429.$0->$method_1(_M0L6loggerS429.$1, _M0L4selfS431, _M0L3segS433, _M0L6_2atmpS1947);
  } else {
    moonbit_decref(_M0L4selfS431);
    if (_M0L6loggerS429.$1) {
      moonbit_decref(_M0L6loggerS429.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS428) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS427;
  int32_t _M0L6_2atmpS1944;
  int32_t _M0L6_2atmpS1943;
  int32_t _M0L6_2atmpS1946;
  int32_t _M0L6_2atmpS1945;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1942;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS427 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1944 = _M0IPC14byte4BytePB3Div3div(_M0L1bS428, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1943
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1944);
  moonbit_incref(_M0L7_2aselfS427);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS427, _M0L6_2atmpS1943);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1946 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS428, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1945
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1946);
  moonbit_incref(_M0L7_2aselfS427);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS427, _M0L6_2atmpS1945);
  _M0L6_2atmpS1942 = _M0L7_2aselfS427;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1942);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS426) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS426 < 10) {
    int32_t _M0L6_2atmpS1939;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1939 = _M0IPC14byte4BytePB3Add3add(_M0L1iS426, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1939);
  } else {
    int32_t _M0L6_2atmpS1941;
    int32_t _M0L6_2atmpS1940;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1941 = _M0IPC14byte4BytePB3Add3add(_M0L1iS426, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1940 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1941, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1940);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS424,
  int32_t _M0L4thatS425
) {
  int32_t _M0L6_2atmpS1937;
  int32_t _M0L6_2atmpS1938;
  int32_t _M0L6_2atmpS1936;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1937 = (int32_t)_M0L4selfS424;
  _M0L6_2atmpS1938 = (int32_t)_M0L4thatS425;
  _M0L6_2atmpS1936 = _M0L6_2atmpS1937 - _M0L6_2atmpS1938;
  return _M0L6_2atmpS1936 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS422,
  int32_t _M0L4thatS423
) {
  int32_t _M0L6_2atmpS1934;
  int32_t _M0L6_2atmpS1935;
  int32_t _M0L6_2atmpS1933;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1934 = (int32_t)_M0L4selfS422;
  _M0L6_2atmpS1935 = (int32_t)_M0L4thatS423;
  _M0L6_2atmpS1933 = _M0L6_2atmpS1934 % _M0L6_2atmpS1935;
  return _M0L6_2atmpS1933 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS420,
  int32_t _M0L4thatS421
) {
  int32_t _M0L6_2atmpS1931;
  int32_t _M0L6_2atmpS1932;
  int32_t _M0L6_2atmpS1930;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1931 = (int32_t)_M0L4selfS420;
  _M0L6_2atmpS1932 = (int32_t)_M0L4thatS421;
  _M0L6_2atmpS1930 = _M0L6_2atmpS1931 / _M0L6_2atmpS1932;
  return _M0L6_2atmpS1930 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS418,
  int32_t _M0L4thatS419
) {
  int32_t _M0L6_2atmpS1928;
  int32_t _M0L6_2atmpS1929;
  int32_t _M0L6_2atmpS1927;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1928 = (int32_t)_M0L4selfS418;
  _M0L6_2atmpS1929 = (int32_t)_M0L4thatS419;
  _M0L6_2atmpS1927 = _M0L6_2atmpS1928 + _M0L6_2atmpS1929;
  return _M0L6_2atmpS1927 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS415,
  int32_t _M0L5startS413,
  int32_t _M0L3endS414
) {
  int32_t _if__result_3719;
  int32_t _M0L3lenS416;
  int32_t _M0L6_2atmpS1925;
  int32_t _M0L6_2atmpS1926;
  moonbit_bytes_t _M0L5bytesS417;
  moonbit_bytes_t _M0L6_2atmpS1924;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS413 == 0) {
    int32_t _M0L6_2atmpS1923 = Moonbit_array_length(_M0L3strS415);
    _if__result_3719 = _M0L3endS414 == _M0L6_2atmpS1923;
  } else {
    _if__result_3719 = 0;
  }
  if (_if__result_3719) {
    return _M0L3strS415;
  }
  _M0L3lenS416 = _M0L3endS414 - _M0L5startS413;
  _M0L6_2atmpS1925 = _M0L3lenS416 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1926 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS417
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1925, _M0L6_2atmpS1926);
  moonbit_incref(_M0L5bytesS417);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS417, 0, _M0L3strS415, _M0L5startS413, _M0L3lenS416);
  _M0L6_2atmpS1924 = _M0L5bytesS417;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1924, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS410) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS410;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS411
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS411;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS412) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS412;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS408,
  int32_t _M0L5indexS409
) {
  moonbit_string_t _M0L8_2afieldS3377;
  moonbit_string_t _M0L3strS1920;
  int32_t _M0L8_2afieldS3376;
  int32_t _M0L5startS1922;
  int32_t _M0L6_2atmpS1921;
  int32_t _M0L6_2atmpS3375;
  #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3377 = _M0L4selfS408.$0;
  _M0L3strS1920 = _M0L8_2afieldS3377;
  _M0L8_2afieldS3376 = _M0L4selfS408.$1;
  _M0L5startS1922 = _M0L8_2afieldS3376;
  _M0L6_2atmpS1921 = _M0L5startS1922 + _M0L5indexS409;
  _M0L6_2atmpS3375 = _M0L3strS1920[_M0L6_2atmpS1921];
  moonbit_decref(_M0L3strS1920);
  return _M0L6_2atmpS3375;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS400,
  int32_t _M0L5radixS399
) {
  int32_t _if__result_3720;
  uint16_t* _M0L6bufferS401;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS399 < 2) {
    _if__result_3720 = 1;
  } else {
    _if__result_3720 = _M0L5radixS399 > 36;
  }
  if (_if__result_3720) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_73.data, (moonbit_string_t)moonbit_string_literal_74.data);
  }
  if (_M0L4selfS400 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_58.data;
  }
  switch (_M0L5radixS399) {
    case 10: {
      int32_t _M0L3lenS402;
      uint16_t* _M0L6bufferS403;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS402 = _M0FPB12dec__count64(_M0L4selfS400);
      _M0L6bufferS403 = (uint16_t*)moonbit_make_string(_M0L3lenS402, 0);
      moonbit_incref(_M0L6bufferS403);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS403, _M0L4selfS400, 0, _M0L3lenS402);
      _M0L6bufferS401 = _M0L6bufferS403;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS404;
      uint16_t* _M0L6bufferS405;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS404 = _M0FPB12hex__count64(_M0L4selfS400);
      _M0L6bufferS405 = (uint16_t*)moonbit_make_string(_M0L3lenS404, 0);
      moonbit_incref(_M0L6bufferS405);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS405, _M0L4selfS400, 0, _M0L3lenS404);
      _M0L6bufferS401 = _M0L6bufferS405;
      break;
    }
    default: {
      int32_t _M0L3lenS406;
      uint16_t* _M0L6bufferS407;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS406 = _M0FPB14radix__count64(_M0L4selfS400, _M0L5radixS399);
      _M0L6bufferS407 = (uint16_t*)moonbit_make_string(_M0L3lenS406, 0);
      moonbit_incref(_M0L6bufferS407);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS407, _M0L4selfS400, 0, _M0L3lenS406, _M0L5radixS399);
      _M0L6bufferS401 = _M0L6bufferS407;
      break;
    }
  }
  return _M0L6bufferS401;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS389,
  uint64_t _M0L3numS377,
  int32_t _M0L12digit__startS380,
  int32_t _M0L10total__lenS379
) {
  uint64_t _M0Lm3numS376;
  int32_t _M0Lm6offsetS378;
  uint64_t _M0L6_2atmpS1919;
  int32_t _M0Lm9remainingS391;
  int32_t _M0L6_2atmpS1900;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS376 = _M0L3numS377;
  _M0Lm6offsetS378 = _M0L10total__lenS379 - _M0L12digit__startS380;
  while (1) {
    uint64_t _M0L6_2atmpS1863 = _M0Lm3numS376;
    if (_M0L6_2atmpS1863 >= 10000ull) {
      uint64_t _M0L6_2atmpS1886 = _M0Lm3numS376;
      uint64_t _M0L1tS381 = _M0L6_2atmpS1886 / 10000ull;
      uint64_t _M0L6_2atmpS1885 = _M0Lm3numS376;
      uint64_t _M0L6_2atmpS1884 = _M0L6_2atmpS1885 % 10000ull;
      int32_t _M0L1rS382 = (int32_t)_M0L6_2atmpS1884;
      int32_t _M0L2d1S383;
      int32_t _M0L2d2S384;
      int32_t _M0L6_2atmpS1864;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L6d1__hiS385;
      int32_t _M0L6_2atmpS1881;
      int32_t _M0L6_2atmpS1880;
      int32_t _M0L6d1__loS386;
      int32_t _M0L6_2atmpS1879;
      int32_t _M0L6_2atmpS1878;
      int32_t _M0L6d2__hiS387;
      int32_t _M0L6_2atmpS1877;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6d2__loS388;
      int32_t _M0L6_2atmpS1866;
      int32_t _M0L6_2atmpS1865;
      int32_t _M0L6_2atmpS1869;
      int32_t _M0L6_2atmpS1868;
      int32_t _M0L6_2atmpS1867;
      int32_t _M0L6_2atmpS1872;
      int32_t _M0L6_2atmpS1871;
      int32_t _M0L6_2atmpS1870;
      int32_t _M0L6_2atmpS1875;
      int32_t _M0L6_2atmpS1874;
      int32_t _M0L6_2atmpS1873;
      _M0Lm3numS376 = _M0L1tS381;
      _M0L2d1S383 = _M0L1rS382 / 100;
      _M0L2d2S384 = _M0L1rS382 % 100;
      _M0L6_2atmpS1864 = _M0Lm6offsetS378;
      _M0Lm6offsetS378 = _M0L6_2atmpS1864 - 4;
      _M0L6_2atmpS1883 = _M0L2d1S383 / 10;
      _M0L6_2atmpS1882 = 48 + _M0L6_2atmpS1883;
      _M0L6d1__hiS385 = (uint16_t)_M0L6_2atmpS1882;
      _M0L6_2atmpS1881 = _M0L2d1S383 % 10;
      _M0L6_2atmpS1880 = 48 + _M0L6_2atmpS1881;
      _M0L6d1__loS386 = (uint16_t)_M0L6_2atmpS1880;
      _M0L6_2atmpS1879 = _M0L2d2S384 / 10;
      _M0L6_2atmpS1878 = 48 + _M0L6_2atmpS1879;
      _M0L6d2__hiS387 = (uint16_t)_M0L6_2atmpS1878;
      _M0L6_2atmpS1877 = _M0L2d2S384 % 10;
      _M0L6_2atmpS1876 = 48 + _M0L6_2atmpS1877;
      _M0L6d2__loS388 = (uint16_t)_M0L6_2atmpS1876;
      _M0L6_2atmpS1866 = _M0Lm6offsetS378;
      _M0L6_2atmpS1865 = _M0L12digit__startS380 + _M0L6_2atmpS1866;
      _M0L6bufferS389[_M0L6_2atmpS1865] = _M0L6d1__hiS385;
      _M0L6_2atmpS1869 = _M0Lm6offsetS378;
      _M0L6_2atmpS1868 = _M0L12digit__startS380 + _M0L6_2atmpS1869;
      _M0L6_2atmpS1867 = _M0L6_2atmpS1868 + 1;
      _M0L6bufferS389[_M0L6_2atmpS1867] = _M0L6d1__loS386;
      _M0L6_2atmpS1872 = _M0Lm6offsetS378;
      _M0L6_2atmpS1871 = _M0L12digit__startS380 + _M0L6_2atmpS1872;
      _M0L6_2atmpS1870 = _M0L6_2atmpS1871 + 2;
      _M0L6bufferS389[_M0L6_2atmpS1870] = _M0L6d2__hiS387;
      _M0L6_2atmpS1875 = _M0Lm6offsetS378;
      _M0L6_2atmpS1874 = _M0L12digit__startS380 + _M0L6_2atmpS1875;
      _M0L6_2atmpS1873 = _M0L6_2atmpS1874 + 3;
      _M0L6bufferS389[_M0L6_2atmpS1873] = _M0L6d2__loS388;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1919 = _M0Lm3numS376;
  _M0Lm9remainingS391 = (int32_t)_M0L6_2atmpS1919;
  while (1) {
    int32_t _M0L6_2atmpS1887 = _M0Lm9remainingS391;
    if (_M0L6_2atmpS1887 >= 100) {
      int32_t _M0L6_2atmpS1899 = _M0Lm9remainingS391;
      int32_t _M0L1tS392 = _M0L6_2atmpS1899 / 100;
      int32_t _M0L6_2atmpS1898 = _M0Lm9remainingS391;
      int32_t _M0L1dS393 = _M0L6_2atmpS1898 % 100;
      int32_t _M0L6_2atmpS1888;
      int32_t _M0L6_2atmpS1897;
      int32_t _M0L6_2atmpS1896;
      int32_t _M0L5d__hiS394;
      int32_t _M0L6_2atmpS1895;
      int32_t _M0L6_2atmpS1894;
      int32_t _M0L5d__loS395;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6_2atmpS1889;
      int32_t _M0L6_2atmpS1893;
      int32_t _M0L6_2atmpS1892;
      int32_t _M0L6_2atmpS1891;
      _M0Lm9remainingS391 = _M0L1tS392;
      _M0L6_2atmpS1888 = _M0Lm6offsetS378;
      _M0Lm6offsetS378 = _M0L6_2atmpS1888 - 2;
      _M0L6_2atmpS1897 = _M0L1dS393 / 10;
      _M0L6_2atmpS1896 = 48 + _M0L6_2atmpS1897;
      _M0L5d__hiS394 = (uint16_t)_M0L6_2atmpS1896;
      _M0L6_2atmpS1895 = _M0L1dS393 % 10;
      _M0L6_2atmpS1894 = 48 + _M0L6_2atmpS1895;
      _M0L5d__loS395 = (uint16_t)_M0L6_2atmpS1894;
      _M0L6_2atmpS1890 = _M0Lm6offsetS378;
      _M0L6_2atmpS1889 = _M0L12digit__startS380 + _M0L6_2atmpS1890;
      _M0L6bufferS389[_M0L6_2atmpS1889] = _M0L5d__hiS394;
      _M0L6_2atmpS1893 = _M0Lm6offsetS378;
      _M0L6_2atmpS1892 = _M0L12digit__startS380 + _M0L6_2atmpS1893;
      _M0L6_2atmpS1891 = _M0L6_2atmpS1892 + 1;
      _M0L6bufferS389[_M0L6_2atmpS1891] = _M0L5d__loS395;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1900 = _M0Lm9remainingS391;
  if (_M0L6_2atmpS1900 >= 10) {
    int32_t _M0L6_2atmpS1901 = _M0Lm6offsetS378;
    int32_t _M0L6_2atmpS1912;
    int32_t _M0L6_2atmpS1911;
    int32_t _M0L6_2atmpS1910;
    int32_t _M0L5d__hiS397;
    int32_t _M0L6_2atmpS1909;
    int32_t _M0L6_2atmpS1908;
    int32_t _M0L6_2atmpS1907;
    int32_t _M0L5d__loS398;
    int32_t _M0L6_2atmpS1903;
    int32_t _M0L6_2atmpS1902;
    int32_t _M0L6_2atmpS1906;
    int32_t _M0L6_2atmpS1905;
    int32_t _M0L6_2atmpS1904;
    _M0Lm6offsetS378 = _M0L6_2atmpS1901 - 2;
    _M0L6_2atmpS1912 = _M0Lm9remainingS391;
    _M0L6_2atmpS1911 = _M0L6_2atmpS1912 / 10;
    _M0L6_2atmpS1910 = 48 + _M0L6_2atmpS1911;
    _M0L5d__hiS397 = (uint16_t)_M0L6_2atmpS1910;
    _M0L6_2atmpS1909 = _M0Lm9remainingS391;
    _M0L6_2atmpS1908 = _M0L6_2atmpS1909 % 10;
    _M0L6_2atmpS1907 = 48 + _M0L6_2atmpS1908;
    _M0L5d__loS398 = (uint16_t)_M0L6_2atmpS1907;
    _M0L6_2atmpS1903 = _M0Lm6offsetS378;
    _M0L6_2atmpS1902 = _M0L12digit__startS380 + _M0L6_2atmpS1903;
    _M0L6bufferS389[_M0L6_2atmpS1902] = _M0L5d__hiS397;
    _M0L6_2atmpS1906 = _M0Lm6offsetS378;
    _M0L6_2atmpS1905 = _M0L12digit__startS380 + _M0L6_2atmpS1906;
    _M0L6_2atmpS1904 = _M0L6_2atmpS1905 + 1;
    _M0L6bufferS389[_M0L6_2atmpS1904] = _M0L5d__loS398;
    moonbit_decref(_M0L6bufferS389);
  } else {
    int32_t _M0L6_2atmpS1913 = _M0Lm6offsetS378;
    int32_t _M0L6_2atmpS1918;
    int32_t _M0L6_2atmpS1914;
    int32_t _M0L6_2atmpS1917;
    int32_t _M0L6_2atmpS1916;
    int32_t _M0L6_2atmpS1915;
    _M0Lm6offsetS378 = _M0L6_2atmpS1913 - 1;
    _M0L6_2atmpS1918 = _M0Lm6offsetS378;
    _M0L6_2atmpS1914 = _M0L12digit__startS380 + _M0L6_2atmpS1918;
    _M0L6_2atmpS1917 = _M0Lm9remainingS391;
    _M0L6_2atmpS1916 = 48 + _M0L6_2atmpS1917;
    _M0L6_2atmpS1915 = (uint16_t)_M0L6_2atmpS1916;
    _M0L6bufferS389[_M0L6_2atmpS1914] = _M0L6_2atmpS1915;
    moonbit_decref(_M0L6bufferS389);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS371,
  uint64_t _M0L3numS365,
  int32_t _M0L12digit__startS363,
  int32_t _M0L10total__lenS362,
  int32_t _M0L5radixS367
) {
  int32_t _M0Lm6offsetS361;
  uint64_t _M0Lm1nS364;
  uint64_t _M0L4baseS366;
  int32_t _M0L6_2atmpS1845;
  int32_t _M0L6_2atmpS1844;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS361 = _M0L10total__lenS362 - _M0L12digit__startS363;
  _M0Lm1nS364 = _M0L3numS365;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS366 = _M0MPC13int3Int10to__uint64(_M0L5radixS367);
  _M0L6_2atmpS1845 = _M0L5radixS367 - 1;
  _M0L6_2atmpS1844 = _M0L5radixS367 & _M0L6_2atmpS1845;
  if (_M0L6_2atmpS1844 == 0) {
    int32_t _M0L5shiftS368;
    uint64_t _M0L4maskS369;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS368 = moonbit_ctz32(_M0L5radixS367);
    _M0L4maskS369 = _M0L4baseS366 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1846 = _M0Lm1nS364;
      if (_M0L6_2atmpS1846 > 0ull) {
        int32_t _M0L6_2atmpS1847 = _M0Lm6offsetS361;
        uint64_t _M0L6_2atmpS1853;
        uint64_t _M0L6_2atmpS1852;
        int32_t _M0L5digitS370;
        int32_t _M0L6_2atmpS1850;
        int32_t _M0L6_2atmpS1848;
        int32_t _M0L6_2atmpS1849;
        uint64_t _M0L6_2atmpS1851;
        _M0Lm6offsetS361 = _M0L6_2atmpS1847 - 1;
        _M0L6_2atmpS1853 = _M0Lm1nS364;
        _M0L6_2atmpS1852 = _M0L6_2atmpS1853 & _M0L4maskS369;
        _M0L5digitS370 = (int32_t)_M0L6_2atmpS1852;
        _M0L6_2atmpS1850 = _M0Lm6offsetS361;
        _M0L6_2atmpS1848 = _M0L12digit__startS363 + _M0L6_2atmpS1850;
        _M0L6_2atmpS1849
        = ((moonbit_string_t)moonbit_string_literal_75.data)[
          _M0L5digitS370
        ];
        _M0L6bufferS371[_M0L6_2atmpS1848] = _M0L6_2atmpS1849;
        _M0L6_2atmpS1851 = _M0Lm1nS364;
        _M0Lm1nS364 = _M0L6_2atmpS1851 >> (_M0L5shiftS368 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS371);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1854 = _M0Lm1nS364;
      if (_M0L6_2atmpS1854 > 0ull) {
        int32_t _M0L6_2atmpS1855 = _M0Lm6offsetS361;
        uint64_t _M0L6_2atmpS1862;
        uint64_t _M0L1qS373;
        uint64_t _M0L6_2atmpS1860;
        uint64_t _M0L6_2atmpS1861;
        uint64_t _M0L6_2atmpS1859;
        int32_t _M0L5digitS374;
        int32_t _M0L6_2atmpS1858;
        int32_t _M0L6_2atmpS1856;
        int32_t _M0L6_2atmpS1857;
        _M0Lm6offsetS361 = _M0L6_2atmpS1855 - 1;
        _M0L6_2atmpS1862 = _M0Lm1nS364;
        _M0L1qS373 = _M0L6_2atmpS1862 / _M0L4baseS366;
        _M0L6_2atmpS1860 = _M0Lm1nS364;
        _M0L6_2atmpS1861 = _M0L1qS373 * _M0L4baseS366;
        _M0L6_2atmpS1859 = _M0L6_2atmpS1860 - _M0L6_2atmpS1861;
        _M0L5digitS374 = (int32_t)_M0L6_2atmpS1859;
        _M0L6_2atmpS1858 = _M0Lm6offsetS361;
        _M0L6_2atmpS1856 = _M0L12digit__startS363 + _M0L6_2atmpS1858;
        _M0L6_2atmpS1857
        = ((moonbit_string_t)moonbit_string_literal_75.data)[
          _M0L5digitS374
        ];
        _M0L6bufferS371[_M0L6_2atmpS1856] = _M0L6_2atmpS1857;
        _M0Lm1nS364 = _M0L1qS373;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS371);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS358,
  uint64_t _M0L3numS354,
  int32_t _M0L12digit__startS352,
  int32_t _M0L10total__lenS351
) {
  int32_t _M0Lm6offsetS350;
  uint64_t _M0Lm1nS353;
  int32_t _M0L6_2atmpS1840;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS350 = _M0L10total__lenS351 - _M0L12digit__startS352;
  _M0Lm1nS353 = _M0L3numS354;
  while (1) {
    int32_t _M0L6_2atmpS1828 = _M0Lm6offsetS350;
    if (_M0L6_2atmpS1828 >= 2) {
      int32_t _M0L6_2atmpS1829 = _M0Lm6offsetS350;
      uint64_t _M0L6_2atmpS1839;
      uint64_t _M0L6_2atmpS1838;
      int32_t _M0L9byte__valS355;
      int32_t _M0L2hiS356;
      int32_t _M0L2loS357;
      int32_t _M0L6_2atmpS1832;
      int32_t _M0L6_2atmpS1830;
      int32_t _M0L6_2atmpS1831;
      int32_t _M0L6_2atmpS1836;
      int32_t _M0L6_2atmpS1835;
      int32_t _M0L6_2atmpS1833;
      int32_t _M0L6_2atmpS1834;
      uint64_t _M0L6_2atmpS1837;
      _M0Lm6offsetS350 = _M0L6_2atmpS1829 - 2;
      _M0L6_2atmpS1839 = _M0Lm1nS353;
      _M0L6_2atmpS1838 = _M0L6_2atmpS1839 & 255ull;
      _M0L9byte__valS355 = (int32_t)_M0L6_2atmpS1838;
      _M0L2hiS356 = _M0L9byte__valS355 / 16;
      _M0L2loS357 = _M0L9byte__valS355 % 16;
      _M0L6_2atmpS1832 = _M0Lm6offsetS350;
      _M0L6_2atmpS1830 = _M0L12digit__startS352 + _M0L6_2atmpS1832;
      _M0L6_2atmpS1831
      = ((moonbit_string_t)moonbit_string_literal_75.data)[
        _M0L2hiS356
      ];
      _M0L6bufferS358[_M0L6_2atmpS1830] = _M0L6_2atmpS1831;
      _M0L6_2atmpS1836 = _M0Lm6offsetS350;
      _M0L6_2atmpS1835 = _M0L12digit__startS352 + _M0L6_2atmpS1836;
      _M0L6_2atmpS1833 = _M0L6_2atmpS1835 + 1;
      _M0L6_2atmpS1834
      = ((moonbit_string_t)moonbit_string_literal_75.data)[
        _M0L2loS357
      ];
      _M0L6bufferS358[_M0L6_2atmpS1833] = _M0L6_2atmpS1834;
      _M0L6_2atmpS1837 = _M0Lm1nS353;
      _M0Lm1nS353 = _M0L6_2atmpS1837 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1840 = _M0Lm6offsetS350;
  if (_M0L6_2atmpS1840 == 1) {
    uint64_t _M0L6_2atmpS1843 = _M0Lm1nS353;
    uint64_t _M0L6_2atmpS1842 = _M0L6_2atmpS1843 & 15ull;
    int32_t _M0L6nibbleS360 = (int32_t)_M0L6_2atmpS1842;
    int32_t _M0L6_2atmpS1841 =
      ((moonbit_string_t)moonbit_string_literal_75.data)[_M0L6nibbleS360];
    _M0L6bufferS358[_M0L12digit__startS352] = _M0L6_2atmpS1841;
    moonbit_decref(_M0L6bufferS358);
  } else {
    moonbit_decref(_M0L6bufferS358);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS344,
  int32_t _M0L5radixS347
) {
  uint64_t _M0Lm3numS345;
  uint64_t _M0L4baseS346;
  int32_t _M0Lm5countS348;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS344 == 0ull) {
    return 1;
  }
  _M0Lm3numS345 = _M0L5valueS344;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS346 = _M0MPC13int3Int10to__uint64(_M0L5radixS347);
  _M0Lm5countS348 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1825 = _M0Lm3numS345;
    if (_M0L6_2atmpS1825 > 0ull) {
      int32_t _M0L6_2atmpS1826 = _M0Lm5countS348;
      uint64_t _M0L6_2atmpS1827;
      _M0Lm5countS348 = _M0L6_2atmpS1826 + 1;
      _M0L6_2atmpS1827 = _M0Lm3numS345;
      _M0Lm3numS345 = _M0L6_2atmpS1827 / _M0L4baseS346;
      continue;
    }
    break;
  }
  return _M0Lm5countS348;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS342) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS342 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS343;
    int32_t _M0L6_2atmpS1824;
    int32_t _M0L6_2atmpS1823;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS343 = moonbit_clz64(_M0L5valueS342);
    _M0L6_2atmpS1824 = 63 - _M0L14leading__zerosS343;
    _M0L6_2atmpS1823 = _M0L6_2atmpS1824 / 4;
    return _M0L6_2atmpS1823 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS341) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS341 >= 10000000000ull) {
    if (_M0L5valueS341 >= 100000000000000ull) {
      if (_M0L5valueS341 >= 10000000000000000ull) {
        if (_M0L5valueS341 >= 1000000000000000000ull) {
          if (_M0L5valueS341 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS341 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS341 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS341 >= 1000000000000ull) {
      if (_M0L5valueS341 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS341 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS341 >= 100000ull) {
    if (_M0L5valueS341 >= 10000000ull) {
      if (_M0L5valueS341 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS341 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS341 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS341 >= 1000ull) {
    if (_M0L5valueS341 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS341 >= 100ull) {
    return 3;
  } else if (_M0L5valueS341 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS325,
  int32_t _M0L5radixS324
) {
  int32_t _if__result_3727;
  int32_t _M0L12is__negativeS326;
  uint32_t _M0L3numS327;
  uint16_t* _M0L6bufferS328;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS324 < 2) {
    _if__result_3727 = 1;
  } else {
    _if__result_3727 = _M0L5radixS324 > 36;
  }
  if (_if__result_3727) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_73.data, (moonbit_string_t)moonbit_string_literal_76.data);
  }
  if (_M0L4selfS325 == 0) {
    return (moonbit_string_t)moonbit_string_literal_58.data;
  }
  _M0L12is__negativeS326 = _M0L4selfS325 < 0;
  if (_M0L12is__negativeS326) {
    int32_t _M0L6_2atmpS1822 = -_M0L4selfS325;
    _M0L3numS327 = *(uint32_t*)&_M0L6_2atmpS1822;
  } else {
    _M0L3numS327 = *(uint32_t*)&_M0L4selfS325;
  }
  switch (_M0L5radixS324) {
    case 10: {
      int32_t _M0L10digit__lenS329;
      int32_t _M0L6_2atmpS1819;
      int32_t _M0L10total__lenS330;
      uint16_t* _M0L6bufferS331;
      int32_t _M0L12digit__startS332;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS329 = _M0FPB12dec__count32(_M0L3numS327);
      if (_M0L12is__negativeS326) {
        _M0L6_2atmpS1819 = 1;
      } else {
        _M0L6_2atmpS1819 = 0;
      }
      _M0L10total__lenS330 = _M0L10digit__lenS329 + _M0L6_2atmpS1819;
      _M0L6bufferS331
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS330, 0);
      if (_M0L12is__negativeS326) {
        _M0L12digit__startS332 = 1;
      } else {
        _M0L12digit__startS332 = 0;
      }
      moonbit_incref(_M0L6bufferS331);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS331, _M0L3numS327, _M0L12digit__startS332, _M0L10total__lenS330);
      _M0L6bufferS328 = _M0L6bufferS331;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS333;
      int32_t _M0L6_2atmpS1820;
      int32_t _M0L10total__lenS334;
      uint16_t* _M0L6bufferS335;
      int32_t _M0L12digit__startS336;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS333 = _M0FPB12hex__count32(_M0L3numS327);
      if (_M0L12is__negativeS326) {
        _M0L6_2atmpS1820 = 1;
      } else {
        _M0L6_2atmpS1820 = 0;
      }
      _M0L10total__lenS334 = _M0L10digit__lenS333 + _M0L6_2atmpS1820;
      _M0L6bufferS335
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS334, 0);
      if (_M0L12is__negativeS326) {
        _M0L12digit__startS336 = 1;
      } else {
        _M0L12digit__startS336 = 0;
      }
      moonbit_incref(_M0L6bufferS335);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS335, _M0L3numS327, _M0L12digit__startS336, _M0L10total__lenS334);
      _M0L6bufferS328 = _M0L6bufferS335;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS337;
      int32_t _M0L6_2atmpS1821;
      int32_t _M0L10total__lenS338;
      uint16_t* _M0L6bufferS339;
      int32_t _M0L12digit__startS340;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS337
      = _M0FPB14radix__count32(_M0L3numS327, _M0L5radixS324);
      if (_M0L12is__negativeS326) {
        _M0L6_2atmpS1821 = 1;
      } else {
        _M0L6_2atmpS1821 = 0;
      }
      _M0L10total__lenS338 = _M0L10digit__lenS337 + _M0L6_2atmpS1821;
      _M0L6bufferS339
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS338, 0);
      if (_M0L12is__negativeS326) {
        _M0L12digit__startS340 = 1;
      } else {
        _M0L12digit__startS340 = 0;
      }
      moonbit_incref(_M0L6bufferS339);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS339, _M0L3numS327, _M0L12digit__startS340, _M0L10total__lenS338, _M0L5radixS324);
      _M0L6bufferS328 = _M0L6bufferS339;
      break;
    }
  }
  if (_M0L12is__negativeS326) {
    _M0L6bufferS328[0] = 45;
  }
  return _M0L6bufferS328;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS318,
  int32_t _M0L5radixS321
) {
  uint32_t _M0Lm3numS319;
  uint32_t _M0L4baseS320;
  int32_t _M0Lm5countS322;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS318 == 0u) {
    return 1;
  }
  _M0Lm3numS319 = _M0L5valueS318;
  _M0L4baseS320 = *(uint32_t*)&_M0L5radixS321;
  _M0Lm5countS322 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1816 = _M0Lm3numS319;
    if (_M0L6_2atmpS1816 > 0u) {
      int32_t _M0L6_2atmpS1817 = _M0Lm5countS322;
      uint32_t _M0L6_2atmpS1818;
      _M0Lm5countS322 = _M0L6_2atmpS1817 + 1;
      _M0L6_2atmpS1818 = _M0Lm3numS319;
      _M0Lm3numS319 = _M0L6_2atmpS1818 / _M0L4baseS320;
      continue;
    }
    break;
  }
  return _M0Lm5countS322;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS316) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS316 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS317;
    int32_t _M0L6_2atmpS1815;
    int32_t _M0L6_2atmpS1814;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS317 = moonbit_clz32(_M0L5valueS316);
    _M0L6_2atmpS1815 = 31 - _M0L14leading__zerosS317;
    _M0L6_2atmpS1814 = _M0L6_2atmpS1815 / 4;
    return _M0L6_2atmpS1814 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS315) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS315 >= 100000u) {
    if (_M0L5valueS315 >= 10000000u) {
      if (_M0L5valueS315 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS315 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS315 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS315 >= 1000u) {
    if (_M0L5valueS315 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS315 >= 100u) {
    return 3;
  } else if (_M0L5valueS315 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS305,
  uint32_t _M0L3numS293,
  int32_t _M0L12digit__startS296,
  int32_t _M0L10total__lenS295
) {
  uint32_t _M0Lm3numS292;
  int32_t _M0Lm6offsetS294;
  uint32_t _M0L6_2atmpS1813;
  int32_t _M0Lm9remainingS307;
  int32_t _M0L6_2atmpS1794;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS292 = _M0L3numS293;
  _M0Lm6offsetS294 = _M0L10total__lenS295 - _M0L12digit__startS296;
  while (1) {
    uint32_t _M0L6_2atmpS1757 = _M0Lm3numS292;
    if (_M0L6_2atmpS1757 >= 10000u) {
      uint32_t _M0L6_2atmpS1780 = _M0Lm3numS292;
      uint32_t _M0L1tS297 = _M0L6_2atmpS1780 / 10000u;
      uint32_t _M0L6_2atmpS1779 = _M0Lm3numS292;
      uint32_t _M0L6_2atmpS1778 = _M0L6_2atmpS1779 % 10000u;
      int32_t _M0L1rS298 = *(int32_t*)&_M0L6_2atmpS1778;
      int32_t _M0L2d1S299;
      int32_t _M0L2d2S300;
      int32_t _M0L6_2atmpS1758;
      int32_t _M0L6_2atmpS1777;
      int32_t _M0L6_2atmpS1776;
      int32_t _M0L6d1__hiS301;
      int32_t _M0L6_2atmpS1775;
      int32_t _M0L6_2atmpS1774;
      int32_t _M0L6d1__loS302;
      int32_t _M0L6_2atmpS1773;
      int32_t _M0L6_2atmpS1772;
      int32_t _M0L6d2__hiS303;
      int32_t _M0L6_2atmpS1771;
      int32_t _M0L6_2atmpS1770;
      int32_t _M0L6d2__loS304;
      int32_t _M0L6_2atmpS1760;
      int32_t _M0L6_2atmpS1759;
      int32_t _M0L6_2atmpS1763;
      int32_t _M0L6_2atmpS1762;
      int32_t _M0L6_2atmpS1761;
      int32_t _M0L6_2atmpS1766;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L6_2atmpS1764;
      int32_t _M0L6_2atmpS1769;
      int32_t _M0L6_2atmpS1768;
      int32_t _M0L6_2atmpS1767;
      _M0Lm3numS292 = _M0L1tS297;
      _M0L2d1S299 = _M0L1rS298 / 100;
      _M0L2d2S300 = _M0L1rS298 % 100;
      _M0L6_2atmpS1758 = _M0Lm6offsetS294;
      _M0Lm6offsetS294 = _M0L6_2atmpS1758 - 4;
      _M0L6_2atmpS1777 = _M0L2d1S299 / 10;
      _M0L6_2atmpS1776 = 48 + _M0L6_2atmpS1777;
      _M0L6d1__hiS301 = (uint16_t)_M0L6_2atmpS1776;
      _M0L6_2atmpS1775 = _M0L2d1S299 % 10;
      _M0L6_2atmpS1774 = 48 + _M0L6_2atmpS1775;
      _M0L6d1__loS302 = (uint16_t)_M0L6_2atmpS1774;
      _M0L6_2atmpS1773 = _M0L2d2S300 / 10;
      _M0L6_2atmpS1772 = 48 + _M0L6_2atmpS1773;
      _M0L6d2__hiS303 = (uint16_t)_M0L6_2atmpS1772;
      _M0L6_2atmpS1771 = _M0L2d2S300 % 10;
      _M0L6_2atmpS1770 = 48 + _M0L6_2atmpS1771;
      _M0L6d2__loS304 = (uint16_t)_M0L6_2atmpS1770;
      _M0L6_2atmpS1760 = _M0Lm6offsetS294;
      _M0L6_2atmpS1759 = _M0L12digit__startS296 + _M0L6_2atmpS1760;
      _M0L6bufferS305[_M0L6_2atmpS1759] = _M0L6d1__hiS301;
      _M0L6_2atmpS1763 = _M0Lm6offsetS294;
      _M0L6_2atmpS1762 = _M0L12digit__startS296 + _M0L6_2atmpS1763;
      _M0L6_2atmpS1761 = _M0L6_2atmpS1762 + 1;
      _M0L6bufferS305[_M0L6_2atmpS1761] = _M0L6d1__loS302;
      _M0L6_2atmpS1766 = _M0Lm6offsetS294;
      _M0L6_2atmpS1765 = _M0L12digit__startS296 + _M0L6_2atmpS1766;
      _M0L6_2atmpS1764 = _M0L6_2atmpS1765 + 2;
      _M0L6bufferS305[_M0L6_2atmpS1764] = _M0L6d2__hiS303;
      _M0L6_2atmpS1769 = _M0Lm6offsetS294;
      _M0L6_2atmpS1768 = _M0L12digit__startS296 + _M0L6_2atmpS1769;
      _M0L6_2atmpS1767 = _M0L6_2atmpS1768 + 3;
      _M0L6bufferS305[_M0L6_2atmpS1767] = _M0L6d2__loS304;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1813 = _M0Lm3numS292;
  _M0Lm9remainingS307 = *(int32_t*)&_M0L6_2atmpS1813;
  while (1) {
    int32_t _M0L6_2atmpS1781 = _M0Lm9remainingS307;
    if (_M0L6_2atmpS1781 >= 100) {
      int32_t _M0L6_2atmpS1793 = _M0Lm9remainingS307;
      int32_t _M0L1tS308 = _M0L6_2atmpS1793 / 100;
      int32_t _M0L6_2atmpS1792 = _M0Lm9remainingS307;
      int32_t _M0L1dS309 = _M0L6_2atmpS1792 % 100;
      int32_t _M0L6_2atmpS1782;
      int32_t _M0L6_2atmpS1791;
      int32_t _M0L6_2atmpS1790;
      int32_t _M0L5d__hiS310;
      int32_t _M0L6_2atmpS1789;
      int32_t _M0L6_2atmpS1788;
      int32_t _M0L5d__loS311;
      int32_t _M0L6_2atmpS1784;
      int32_t _M0L6_2atmpS1783;
      int32_t _M0L6_2atmpS1787;
      int32_t _M0L6_2atmpS1786;
      int32_t _M0L6_2atmpS1785;
      _M0Lm9remainingS307 = _M0L1tS308;
      _M0L6_2atmpS1782 = _M0Lm6offsetS294;
      _M0Lm6offsetS294 = _M0L6_2atmpS1782 - 2;
      _M0L6_2atmpS1791 = _M0L1dS309 / 10;
      _M0L6_2atmpS1790 = 48 + _M0L6_2atmpS1791;
      _M0L5d__hiS310 = (uint16_t)_M0L6_2atmpS1790;
      _M0L6_2atmpS1789 = _M0L1dS309 % 10;
      _M0L6_2atmpS1788 = 48 + _M0L6_2atmpS1789;
      _M0L5d__loS311 = (uint16_t)_M0L6_2atmpS1788;
      _M0L6_2atmpS1784 = _M0Lm6offsetS294;
      _M0L6_2atmpS1783 = _M0L12digit__startS296 + _M0L6_2atmpS1784;
      _M0L6bufferS305[_M0L6_2atmpS1783] = _M0L5d__hiS310;
      _M0L6_2atmpS1787 = _M0Lm6offsetS294;
      _M0L6_2atmpS1786 = _M0L12digit__startS296 + _M0L6_2atmpS1787;
      _M0L6_2atmpS1785 = _M0L6_2atmpS1786 + 1;
      _M0L6bufferS305[_M0L6_2atmpS1785] = _M0L5d__loS311;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1794 = _M0Lm9remainingS307;
  if (_M0L6_2atmpS1794 >= 10) {
    int32_t _M0L6_2atmpS1795 = _M0Lm6offsetS294;
    int32_t _M0L6_2atmpS1806;
    int32_t _M0L6_2atmpS1805;
    int32_t _M0L6_2atmpS1804;
    int32_t _M0L5d__hiS313;
    int32_t _M0L6_2atmpS1803;
    int32_t _M0L6_2atmpS1802;
    int32_t _M0L6_2atmpS1801;
    int32_t _M0L5d__loS314;
    int32_t _M0L6_2atmpS1797;
    int32_t _M0L6_2atmpS1796;
    int32_t _M0L6_2atmpS1800;
    int32_t _M0L6_2atmpS1799;
    int32_t _M0L6_2atmpS1798;
    _M0Lm6offsetS294 = _M0L6_2atmpS1795 - 2;
    _M0L6_2atmpS1806 = _M0Lm9remainingS307;
    _M0L6_2atmpS1805 = _M0L6_2atmpS1806 / 10;
    _M0L6_2atmpS1804 = 48 + _M0L6_2atmpS1805;
    _M0L5d__hiS313 = (uint16_t)_M0L6_2atmpS1804;
    _M0L6_2atmpS1803 = _M0Lm9remainingS307;
    _M0L6_2atmpS1802 = _M0L6_2atmpS1803 % 10;
    _M0L6_2atmpS1801 = 48 + _M0L6_2atmpS1802;
    _M0L5d__loS314 = (uint16_t)_M0L6_2atmpS1801;
    _M0L6_2atmpS1797 = _M0Lm6offsetS294;
    _M0L6_2atmpS1796 = _M0L12digit__startS296 + _M0L6_2atmpS1797;
    _M0L6bufferS305[_M0L6_2atmpS1796] = _M0L5d__hiS313;
    _M0L6_2atmpS1800 = _M0Lm6offsetS294;
    _M0L6_2atmpS1799 = _M0L12digit__startS296 + _M0L6_2atmpS1800;
    _M0L6_2atmpS1798 = _M0L6_2atmpS1799 + 1;
    _M0L6bufferS305[_M0L6_2atmpS1798] = _M0L5d__loS314;
    moonbit_decref(_M0L6bufferS305);
  } else {
    int32_t _M0L6_2atmpS1807 = _M0Lm6offsetS294;
    int32_t _M0L6_2atmpS1812;
    int32_t _M0L6_2atmpS1808;
    int32_t _M0L6_2atmpS1811;
    int32_t _M0L6_2atmpS1810;
    int32_t _M0L6_2atmpS1809;
    _M0Lm6offsetS294 = _M0L6_2atmpS1807 - 1;
    _M0L6_2atmpS1812 = _M0Lm6offsetS294;
    _M0L6_2atmpS1808 = _M0L12digit__startS296 + _M0L6_2atmpS1812;
    _M0L6_2atmpS1811 = _M0Lm9remainingS307;
    _M0L6_2atmpS1810 = 48 + _M0L6_2atmpS1811;
    _M0L6_2atmpS1809 = (uint16_t)_M0L6_2atmpS1810;
    _M0L6bufferS305[_M0L6_2atmpS1808] = _M0L6_2atmpS1809;
    moonbit_decref(_M0L6bufferS305);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS287,
  uint32_t _M0L3numS281,
  int32_t _M0L12digit__startS279,
  int32_t _M0L10total__lenS278,
  int32_t _M0L5radixS283
) {
  int32_t _M0Lm6offsetS277;
  uint32_t _M0Lm1nS280;
  uint32_t _M0L4baseS282;
  int32_t _M0L6_2atmpS1739;
  int32_t _M0L6_2atmpS1738;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS277 = _M0L10total__lenS278 - _M0L12digit__startS279;
  _M0Lm1nS280 = _M0L3numS281;
  _M0L4baseS282 = *(uint32_t*)&_M0L5radixS283;
  _M0L6_2atmpS1739 = _M0L5radixS283 - 1;
  _M0L6_2atmpS1738 = _M0L5radixS283 & _M0L6_2atmpS1739;
  if (_M0L6_2atmpS1738 == 0) {
    int32_t _M0L5shiftS284;
    uint32_t _M0L4maskS285;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS284 = moonbit_ctz32(_M0L5radixS283);
    _M0L4maskS285 = _M0L4baseS282 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1740 = _M0Lm1nS280;
      if (_M0L6_2atmpS1740 > 0u) {
        int32_t _M0L6_2atmpS1741 = _M0Lm6offsetS277;
        uint32_t _M0L6_2atmpS1747;
        uint32_t _M0L6_2atmpS1746;
        int32_t _M0L5digitS286;
        int32_t _M0L6_2atmpS1744;
        int32_t _M0L6_2atmpS1742;
        int32_t _M0L6_2atmpS1743;
        uint32_t _M0L6_2atmpS1745;
        _M0Lm6offsetS277 = _M0L6_2atmpS1741 - 1;
        _M0L6_2atmpS1747 = _M0Lm1nS280;
        _M0L6_2atmpS1746 = _M0L6_2atmpS1747 & _M0L4maskS285;
        _M0L5digitS286 = *(int32_t*)&_M0L6_2atmpS1746;
        _M0L6_2atmpS1744 = _M0Lm6offsetS277;
        _M0L6_2atmpS1742 = _M0L12digit__startS279 + _M0L6_2atmpS1744;
        _M0L6_2atmpS1743
        = ((moonbit_string_t)moonbit_string_literal_75.data)[
          _M0L5digitS286
        ];
        _M0L6bufferS287[_M0L6_2atmpS1742] = _M0L6_2atmpS1743;
        _M0L6_2atmpS1745 = _M0Lm1nS280;
        _M0Lm1nS280 = _M0L6_2atmpS1745 >> (_M0L5shiftS284 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS287);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1748 = _M0Lm1nS280;
      if (_M0L6_2atmpS1748 > 0u) {
        int32_t _M0L6_2atmpS1749 = _M0Lm6offsetS277;
        uint32_t _M0L6_2atmpS1756;
        uint32_t _M0L1qS289;
        uint32_t _M0L6_2atmpS1754;
        uint32_t _M0L6_2atmpS1755;
        uint32_t _M0L6_2atmpS1753;
        int32_t _M0L5digitS290;
        int32_t _M0L6_2atmpS1752;
        int32_t _M0L6_2atmpS1750;
        int32_t _M0L6_2atmpS1751;
        _M0Lm6offsetS277 = _M0L6_2atmpS1749 - 1;
        _M0L6_2atmpS1756 = _M0Lm1nS280;
        _M0L1qS289 = _M0L6_2atmpS1756 / _M0L4baseS282;
        _M0L6_2atmpS1754 = _M0Lm1nS280;
        _M0L6_2atmpS1755 = _M0L1qS289 * _M0L4baseS282;
        _M0L6_2atmpS1753 = _M0L6_2atmpS1754 - _M0L6_2atmpS1755;
        _M0L5digitS290 = *(int32_t*)&_M0L6_2atmpS1753;
        _M0L6_2atmpS1752 = _M0Lm6offsetS277;
        _M0L6_2atmpS1750 = _M0L12digit__startS279 + _M0L6_2atmpS1752;
        _M0L6_2atmpS1751
        = ((moonbit_string_t)moonbit_string_literal_75.data)[
          _M0L5digitS290
        ];
        _M0L6bufferS287[_M0L6_2atmpS1750] = _M0L6_2atmpS1751;
        _M0Lm1nS280 = _M0L1qS289;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS287);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS274,
  uint32_t _M0L3numS270,
  int32_t _M0L12digit__startS268,
  int32_t _M0L10total__lenS267
) {
  int32_t _M0Lm6offsetS266;
  uint32_t _M0Lm1nS269;
  int32_t _M0L6_2atmpS1734;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS266 = _M0L10total__lenS267 - _M0L12digit__startS268;
  _M0Lm1nS269 = _M0L3numS270;
  while (1) {
    int32_t _M0L6_2atmpS1722 = _M0Lm6offsetS266;
    if (_M0L6_2atmpS1722 >= 2) {
      int32_t _M0L6_2atmpS1723 = _M0Lm6offsetS266;
      uint32_t _M0L6_2atmpS1733;
      uint32_t _M0L6_2atmpS1732;
      int32_t _M0L9byte__valS271;
      int32_t _M0L2hiS272;
      int32_t _M0L2loS273;
      int32_t _M0L6_2atmpS1726;
      int32_t _M0L6_2atmpS1724;
      int32_t _M0L6_2atmpS1725;
      int32_t _M0L6_2atmpS1730;
      int32_t _M0L6_2atmpS1729;
      int32_t _M0L6_2atmpS1727;
      int32_t _M0L6_2atmpS1728;
      uint32_t _M0L6_2atmpS1731;
      _M0Lm6offsetS266 = _M0L6_2atmpS1723 - 2;
      _M0L6_2atmpS1733 = _M0Lm1nS269;
      _M0L6_2atmpS1732 = _M0L6_2atmpS1733 & 255u;
      _M0L9byte__valS271 = *(int32_t*)&_M0L6_2atmpS1732;
      _M0L2hiS272 = _M0L9byte__valS271 / 16;
      _M0L2loS273 = _M0L9byte__valS271 % 16;
      _M0L6_2atmpS1726 = _M0Lm6offsetS266;
      _M0L6_2atmpS1724 = _M0L12digit__startS268 + _M0L6_2atmpS1726;
      _M0L6_2atmpS1725
      = ((moonbit_string_t)moonbit_string_literal_75.data)[
        _M0L2hiS272
      ];
      _M0L6bufferS274[_M0L6_2atmpS1724] = _M0L6_2atmpS1725;
      _M0L6_2atmpS1730 = _M0Lm6offsetS266;
      _M0L6_2atmpS1729 = _M0L12digit__startS268 + _M0L6_2atmpS1730;
      _M0L6_2atmpS1727 = _M0L6_2atmpS1729 + 1;
      _M0L6_2atmpS1728
      = ((moonbit_string_t)moonbit_string_literal_75.data)[
        _M0L2loS273
      ];
      _M0L6bufferS274[_M0L6_2atmpS1727] = _M0L6_2atmpS1728;
      _M0L6_2atmpS1731 = _M0Lm1nS269;
      _M0Lm1nS269 = _M0L6_2atmpS1731 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1734 = _M0Lm6offsetS266;
  if (_M0L6_2atmpS1734 == 1) {
    uint32_t _M0L6_2atmpS1737 = _M0Lm1nS269;
    uint32_t _M0L6_2atmpS1736 = _M0L6_2atmpS1737 & 15u;
    int32_t _M0L6nibbleS276 = *(int32_t*)&_M0L6_2atmpS1736;
    int32_t _M0L6_2atmpS1735 =
      ((moonbit_string_t)moonbit_string_literal_75.data)[_M0L6nibbleS276];
    _M0L6bufferS274[_M0L12digit__startS268] = _M0L6_2atmpS1735;
    moonbit_decref(_M0L6bufferS274);
  } else {
    moonbit_decref(_M0L6bufferS274);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS261) {
  struct _M0TWEOs* _M0L7_2afuncS260;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS260 = _M0L4selfS261;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS260->code(_M0L7_2afuncS260);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS263
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS262;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS262 = _M0L4selfS263;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS262->code(_M0L7_2afuncS262);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS265) {
  struct _M0TWEOc* _M0L7_2afuncS264;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS264 = _M0L4selfS265;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS264->code(_M0L7_2afuncS264);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS251
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS250;
  struct _M0TPB6Logger _M0L6_2atmpS1717;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS250 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS250);
  _M0L6_2atmpS1717
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS250
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS251, _M0L6_2atmpS1717);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS250);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS253
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS252;
  struct _M0TPB6Logger _M0L6_2atmpS1718;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS252 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS252);
  _M0L6_2atmpS1718
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS252
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS253, _M0L6_2atmpS1718);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS252);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS255
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS254;
  struct _M0TPB6Logger _M0L6_2atmpS1719;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS254 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS254);
  _M0L6_2atmpS1719
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS254
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS255, _M0L6_2atmpS1719);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS254);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPC14json15JsonDecodeErrorE(
  void* _M0L4selfS257
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS256;
  struct _M0TPB6Logger _M0L6_2atmpS1720;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS256 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS256);
  _M0L6_2atmpS1720
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS256
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14json15JsonDecodeErrorPB4Show6output(_M0L4selfS257, _M0L6_2atmpS1720);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS256);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS259
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS258;
  struct _M0TPB6Logger _M0L6_2atmpS1721;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS258 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS258);
  _M0L6_2atmpS1721
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS258
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS259, _M0L6_2atmpS1721);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS258);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS249
) {
  int32_t _M0L8_2afieldS3378;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3378 = _M0L4selfS249.$1;
  moonbit_decref(_M0L4selfS249.$0);
  return _M0L8_2afieldS3378;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS248
) {
  int32_t _M0L3endS1715;
  int32_t _M0L8_2afieldS3379;
  int32_t _M0L5startS1716;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1715 = _M0L4selfS248.$2;
  _M0L8_2afieldS3379 = _M0L4selfS248.$1;
  moonbit_decref(_M0L4selfS248.$0);
  _M0L5startS1716 = _M0L8_2afieldS3379;
  return _M0L3endS1715 - _M0L5startS1716;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS247
) {
  moonbit_string_t _M0L8_2afieldS3380;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3380 = _M0L4selfS247.$0;
  return _M0L8_2afieldS3380;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS243,
  moonbit_string_t _M0L5valueS244,
  int32_t _M0L5startS245,
  int32_t _M0L3lenS246
) {
  int32_t _M0L6_2atmpS1714;
  int64_t _M0L6_2atmpS1713;
  struct _M0TPC16string10StringView _M0L6_2atmpS1712;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1714 = _M0L5startS245 + _M0L3lenS246;
  _M0L6_2atmpS1713 = (int64_t)_M0L6_2atmpS1714;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1712
  = _M0MPC16string6String11sub_2einner(_M0L5valueS244, _M0L5startS245, _M0L6_2atmpS1713);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS243, _M0L6_2atmpS1712);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS236,
  int32_t _M0L5startS242,
  int64_t _M0L3endS238
) {
  int32_t _M0L3lenS235;
  int32_t _M0L3endS237;
  int32_t _M0L5startS241;
  int32_t _if__result_3734;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS235 = Moonbit_array_length(_M0L4selfS236);
  if (_M0L3endS238 == 4294967296ll) {
    _M0L3endS237 = _M0L3lenS235;
  } else {
    int64_t _M0L7_2aSomeS239 = _M0L3endS238;
    int32_t _M0L6_2aendS240 = (int32_t)_M0L7_2aSomeS239;
    if (_M0L6_2aendS240 < 0) {
      _M0L3endS237 = _M0L3lenS235 + _M0L6_2aendS240;
    } else {
      _M0L3endS237 = _M0L6_2aendS240;
    }
  }
  if (_M0L5startS242 < 0) {
    _M0L5startS241 = _M0L3lenS235 + _M0L5startS242;
  } else {
    _M0L5startS241 = _M0L5startS242;
  }
  if (_M0L5startS241 >= 0) {
    if (_M0L5startS241 <= _M0L3endS237) {
      _if__result_3734 = _M0L3endS237 <= _M0L3lenS235;
    } else {
      _if__result_3734 = 0;
    }
  } else {
    _if__result_3734 = 0;
  }
  if (_if__result_3734) {
    if (_M0L5startS241 < _M0L3lenS235) {
      int32_t _M0L6_2atmpS1709 = _M0L4selfS236[_M0L5startS241];
      int32_t _M0L6_2atmpS1708;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1708
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1709);
      if (!_M0L6_2atmpS1708) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS237 < _M0L3lenS235) {
      int32_t _M0L6_2atmpS1711 = _M0L4selfS236[_M0L3endS237];
      int32_t _M0L6_2atmpS1710;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1710
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1711);
      if (!_M0L6_2atmpS1710) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS241,
                                                 _M0L3endS237,
                                                 _M0L4selfS236};
  } else {
    moonbit_decref(_M0L4selfS236);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS232) {
  struct _M0TPB6Hasher* _M0L1hS231;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS231 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS231);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS231, _M0L4selfS232);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS231);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS234
) {
  struct _M0TPB6Hasher* _M0L1hS233;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS233 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS233);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS233, _M0L4selfS234);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS233);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS229) {
  int32_t _M0L4seedS228;
  if (_M0L10seed_2eoptS229 == 4294967296ll) {
    _M0L4seedS228 = 0;
  } else {
    int64_t _M0L7_2aSomeS230 = _M0L10seed_2eoptS229;
    _M0L4seedS228 = (int32_t)_M0L7_2aSomeS230;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS228);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS227) {
  uint32_t _M0L6_2atmpS1707;
  uint32_t _M0L6_2atmpS1706;
  struct _M0TPB6Hasher* _block_3735;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1707 = *(uint32_t*)&_M0L4seedS227;
  _M0L6_2atmpS1706 = _M0L6_2atmpS1707 + 374761393u;
  _block_3735
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3735)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3735->$0 = _M0L6_2atmpS1706;
  return _block_3735;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS226) {
  uint32_t _M0L6_2atmpS1705;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1705 = _M0MPB6Hasher9avalanche(_M0L4selfS226);
  return *(int32_t*)&_M0L6_2atmpS1705;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS225) {
  uint32_t _M0L8_2afieldS3381;
  uint32_t _M0Lm3accS224;
  uint32_t _M0L6_2atmpS1694;
  uint32_t _M0L6_2atmpS1696;
  uint32_t _M0L6_2atmpS1695;
  uint32_t _M0L6_2atmpS1697;
  uint32_t _M0L6_2atmpS1698;
  uint32_t _M0L6_2atmpS1700;
  uint32_t _M0L6_2atmpS1699;
  uint32_t _M0L6_2atmpS1701;
  uint32_t _M0L6_2atmpS1702;
  uint32_t _M0L6_2atmpS1704;
  uint32_t _M0L6_2atmpS1703;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3381 = _M0L4selfS225->$0;
  moonbit_decref(_M0L4selfS225);
  _M0Lm3accS224 = _M0L8_2afieldS3381;
  _M0L6_2atmpS1694 = _M0Lm3accS224;
  _M0L6_2atmpS1696 = _M0Lm3accS224;
  _M0L6_2atmpS1695 = _M0L6_2atmpS1696 >> 15;
  _M0Lm3accS224 = _M0L6_2atmpS1694 ^ _M0L6_2atmpS1695;
  _M0L6_2atmpS1697 = _M0Lm3accS224;
  _M0Lm3accS224 = _M0L6_2atmpS1697 * 2246822519u;
  _M0L6_2atmpS1698 = _M0Lm3accS224;
  _M0L6_2atmpS1700 = _M0Lm3accS224;
  _M0L6_2atmpS1699 = _M0L6_2atmpS1700 >> 13;
  _M0Lm3accS224 = _M0L6_2atmpS1698 ^ _M0L6_2atmpS1699;
  _M0L6_2atmpS1701 = _M0Lm3accS224;
  _M0Lm3accS224 = _M0L6_2atmpS1701 * 3266489917u;
  _M0L6_2atmpS1702 = _M0Lm3accS224;
  _M0L6_2atmpS1704 = _M0Lm3accS224;
  _M0L6_2atmpS1703 = _M0L6_2atmpS1704 >> 16;
  _M0Lm3accS224 = _M0L6_2atmpS1702 ^ _M0L6_2atmpS1703;
  return _M0Lm3accS224;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS222,
  moonbit_string_t _M0L1yS223
) {
  int32_t _M0L6_2atmpS3382;
  int32_t _M0L6_2atmpS1693;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3382 = moonbit_val_array_equal(_M0L1xS222, _M0L1yS223);
  moonbit_decref(_M0L1xS222);
  moonbit_decref(_M0L1yS223);
  _M0L6_2atmpS1693 = _M0L6_2atmpS3382;
  return !_M0L6_2atmpS1693;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS219,
  int32_t _M0L5valueS218
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS218, _M0L4selfS219);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS221,
  moonbit_string_t _M0L5valueS220
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS220, _M0L4selfS221);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS217) {
  int64_t _M0L6_2atmpS1692;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1692 = (int64_t)_M0L4selfS217;
  return *(uint64_t*)&_M0L6_2atmpS1692;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS215,
  int32_t _M0L5valueS216
) {
  uint32_t _M0L6_2atmpS1691;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1691 = *(uint32_t*)&_M0L5valueS216;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS215, _M0L6_2atmpS1691);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS208
) {
  struct _M0TPB13StringBuilder* _M0L3bufS206;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS207;
  int32_t _M0L7_2abindS209;
  int32_t _M0L1iS210;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS206 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS207 = _M0L4selfS208;
  moonbit_incref(_M0L3bufS206);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS206, 91);
  _M0L7_2abindS209 = _M0L7_2aselfS207->$1;
  _M0L1iS210 = 0;
  while (1) {
    if (_M0L1iS210 < _M0L7_2abindS209) {
      int32_t _if__result_3737;
      moonbit_string_t* _M0L8_2afieldS3384;
      moonbit_string_t* _M0L3bufS1689;
      moonbit_string_t _M0L6_2atmpS3383;
      moonbit_string_t _M0L4itemS211;
      int32_t _M0L6_2atmpS1690;
      if (_M0L1iS210 != 0) {
        moonbit_incref(_M0L3bufS206);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS206, (moonbit_string_t)moonbit_string_literal_56.data);
      }
      if (_M0L1iS210 < 0) {
        _if__result_3737 = 1;
      } else {
        int32_t _M0L3lenS1688 = _M0L7_2aselfS207->$1;
        _if__result_3737 = _M0L1iS210 >= _M0L3lenS1688;
      }
      if (_if__result_3737) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3384 = _M0L7_2aselfS207->$0;
      _M0L3bufS1689 = _M0L8_2afieldS3384;
      _M0L6_2atmpS3383 = (moonbit_string_t)_M0L3bufS1689[_M0L1iS210];
      _M0L4itemS211 = _M0L6_2atmpS3383;
      if (_M0L4itemS211 == 0) {
        moonbit_incref(_M0L3bufS206);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS206, (moonbit_string_t)moonbit_string_literal_34.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS212 = _M0L4itemS211;
        moonbit_string_t _M0L6_2alocS213 = _M0L7_2aSomeS212;
        moonbit_string_t _M0L6_2atmpS1687;
        moonbit_incref(_M0L6_2alocS213);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1687
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS213);
        moonbit_incref(_M0L3bufS206);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS206, _M0L6_2atmpS1687);
      }
      _M0L6_2atmpS1690 = _M0L1iS210 + 1;
      _M0L1iS210 = _M0L6_2atmpS1690;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS207);
    }
    break;
  }
  moonbit_incref(_M0L3bufS206);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS206, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS206);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS205
) {
  moonbit_string_t _M0L6_2atmpS1686;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1685;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1686 = _M0L4selfS205;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1685 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1686);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1685);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS204
) {
  struct _M0TPB13StringBuilder* _M0L2sbS203;
  struct _M0TPC16string10StringView _M0L8_2afieldS3397;
  struct _M0TPC16string10StringView _M0L3pkgS1670;
  moonbit_string_t _M0L6_2atmpS1669;
  moonbit_string_t _M0L6_2atmpS3396;
  moonbit_string_t _M0L6_2atmpS1668;
  moonbit_string_t _M0L6_2atmpS3395;
  moonbit_string_t _M0L6_2atmpS1667;
  struct _M0TPC16string10StringView _M0L8_2afieldS3394;
  struct _M0TPC16string10StringView _M0L8filenameS1671;
  struct _M0TPC16string10StringView _M0L8_2afieldS3393;
  struct _M0TPC16string10StringView _M0L11start__lineS1674;
  moonbit_string_t _M0L6_2atmpS1673;
  moonbit_string_t _M0L6_2atmpS3392;
  moonbit_string_t _M0L6_2atmpS1672;
  struct _M0TPC16string10StringView _M0L8_2afieldS3391;
  struct _M0TPC16string10StringView _M0L13start__columnS1677;
  moonbit_string_t _M0L6_2atmpS1676;
  moonbit_string_t _M0L6_2atmpS3390;
  moonbit_string_t _M0L6_2atmpS1675;
  struct _M0TPC16string10StringView _M0L8_2afieldS3389;
  struct _M0TPC16string10StringView _M0L9end__lineS1680;
  moonbit_string_t _M0L6_2atmpS1679;
  moonbit_string_t _M0L6_2atmpS3388;
  moonbit_string_t _M0L6_2atmpS1678;
  struct _M0TPC16string10StringView _M0L8_2afieldS3387;
  int32_t _M0L6_2acntS3545;
  struct _M0TPC16string10StringView _M0L11end__columnS1684;
  moonbit_string_t _M0L6_2atmpS1683;
  moonbit_string_t _M0L6_2atmpS3386;
  moonbit_string_t _M0L6_2atmpS1682;
  moonbit_string_t _M0L6_2atmpS3385;
  moonbit_string_t _M0L6_2atmpS1681;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS203 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3397
  = (struct _M0TPC16string10StringView){
    _M0L4selfS204->$0_1, _M0L4selfS204->$0_2, _M0L4selfS204->$0_0
  };
  _M0L3pkgS1670 = _M0L8_2afieldS3397;
  moonbit_incref(_M0L3pkgS1670.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1669
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1670);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3396
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_77.data, _M0L6_2atmpS1669);
  moonbit_decref(_M0L6_2atmpS1669);
  _M0L6_2atmpS1668 = _M0L6_2atmpS3396;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3395
  = moonbit_add_string(_M0L6_2atmpS1668, (moonbit_string_t)moonbit_string_literal_78.data);
  moonbit_decref(_M0L6_2atmpS1668);
  _M0L6_2atmpS1667 = _M0L6_2atmpS3395;
  moonbit_incref(_M0L2sbS203);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS203, _M0L6_2atmpS1667);
  moonbit_incref(_M0L2sbS203);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS203, (moonbit_string_t)moonbit_string_literal_79.data);
  _M0L8_2afieldS3394
  = (struct _M0TPC16string10StringView){
    _M0L4selfS204->$1_1, _M0L4selfS204->$1_2, _M0L4selfS204->$1_0
  };
  _M0L8filenameS1671 = _M0L8_2afieldS3394;
  moonbit_incref(_M0L8filenameS1671.$0);
  moonbit_incref(_M0L2sbS203);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS203, _M0L8filenameS1671);
  _M0L8_2afieldS3393
  = (struct _M0TPC16string10StringView){
    _M0L4selfS204->$2_1, _M0L4selfS204->$2_2, _M0L4selfS204->$2_0
  };
  _M0L11start__lineS1674 = _M0L8_2afieldS3393;
  moonbit_incref(_M0L11start__lineS1674.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1673
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1674);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3392
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_80.data, _M0L6_2atmpS1673);
  moonbit_decref(_M0L6_2atmpS1673);
  _M0L6_2atmpS1672 = _M0L6_2atmpS3392;
  moonbit_incref(_M0L2sbS203);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS203, _M0L6_2atmpS1672);
  _M0L8_2afieldS3391
  = (struct _M0TPC16string10StringView){
    _M0L4selfS204->$3_1, _M0L4selfS204->$3_2, _M0L4selfS204->$3_0
  };
  _M0L13start__columnS1677 = _M0L8_2afieldS3391;
  moonbit_incref(_M0L13start__columnS1677.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1676
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1677);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3390
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_81.data, _M0L6_2atmpS1676);
  moonbit_decref(_M0L6_2atmpS1676);
  _M0L6_2atmpS1675 = _M0L6_2atmpS3390;
  moonbit_incref(_M0L2sbS203);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS203, _M0L6_2atmpS1675);
  _M0L8_2afieldS3389
  = (struct _M0TPC16string10StringView){
    _M0L4selfS204->$4_1, _M0L4selfS204->$4_2, _M0L4selfS204->$4_0
  };
  _M0L9end__lineS1680 = _M0L8_2afieldS3389;
  moonbit_incref(_M0L9end__lineS1680.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1679
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1680);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3388
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_82.data, _M0L6_2atmpS1679);
  moonbit_decref(_M0L6_2atmpS1679);
  _M0L6_2atmpS1678 = _M0L6_2atmpS3388;
  moonbit_incref(_M0L2sbS203);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS203, _M0L6_2atmpS1678);
  _M0L8_2afieldS3387
  = (struct _M0TPC16string10StringView){
    _M0L4selfS204->$5_1, _M0L4selfS204->$5_2, _M0L4selfS204->$5_0
  };
  _M0L6_2acntS3545 = Moonbit_object_header(_M0L4selfS204)->rc;
  if (_M0L6_2acntS3545 > 1) {
    int32_t _M0L11_2anew__cntS3551 = _M0L6_2acntS3545 - 1;
    Moonbit_object_header(_M0L4selfS204)->rc = _M0L11_2anew__cntS3551;
    moonbit_incref(_M0L8_2afieldS3387.$0);
  } else if (_M0L6_2acntS3545 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3550 =
      (struct _M0TPC16string10StringView){_M0L4selfS204->$4_1,
                                            _M0L4selfS204->$4_2,
                                            _M0L4selfS204->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3549;
    struct _M0TPC16string10StringView _M0L8_2afieldS3548;
    struct _M0TPC16string10StringView _M0L8_2afieldS3547;
    struct _M0TPC16string10StringView _M0L8_2afieldS3546;
    moonbit_decref(_M0L8_2afieldS3550.$0);
    _M0L8_2afieldS3549
    = (struct _M0TPC16string10StringView){
      _M0L4selfS204->$3_1, _M0L4selfS204->$3_2, _M0L4selfS204->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3549.$0);
    _M0L8_2afieldS3548
    = (struct _M0TPC16string10StringView){
      _M0L4selfS204->$2_1, _M0L4selfS204->$2_2, _M0L4selfS204->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3548.$0);
    _M0L8_2afieldS3547
    = (struct _M0TPC16string10StringView){
      _M0L4selfS204->$1_1, _M0L4selfS204->$1_2, _M0L4selfS204->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3547.$0);
    _M0L8_2afieldS3546
    = (struct _M0TPC16string10StringView){
      _M0L4selfS204->$0_1, _M0L4selfS204->$0_2, _M0L4selfS204->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3546.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS204);
  }
  _M0L11end__columnS1684 = _M0L8_2afieldS3387;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1683
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1684);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3386
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_83.data, _M0L6_2atmpS1683);
  moonbit_decref(_M0L6_2atmpS1683);
  _M0L6_2atmpS1682 = _M0L6_2atmpS3386;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3385
  = moonbit_add_string(_M0L6_2atmpS1682, (moonbit_string_t)moonbit_string_literal_8.data);
  moonbit_decref(_M0L6_2atmpS1682);
  _M0L6_2atmpS1681 = _M0L6_2atmpS3385;
  moonbit_incref(_M0L2sbS203);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS203, _M0L6_2atmpS1681);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS203);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS201,
  moonbit_string_t _M0L3strS202
) {
  int32_t _M0L3lenS1657;
  int32_t _M0L6_2atmpS1659;
  int32_t _M0L6_2atmpS1658;
  int32_t _M0L6_2atmpS1656;
  moonbit_bytes_t _M0L8_2afieldS3399;
  moonbit_bytes_t _M0L4dataS1660;
  int32_t _M0L3lenS1661;
  int32_t _M0L6_2atmpS1662;
  int32_t _M0L3lenS1664;
  int32_t _M0L6_2atmpS3398;
  int32_t _M0L6_2atmpS1666;
  int32_t _M0L6_2atmpS1665;
  int32_t _M0L6_2atmpS1663;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1657 = _M0L4selfS201->$1;
  _M0L6_2atmpS1659 = Moonbit_array_length(_M0L3strS202);
  _M0L6_2atmpS1658 = _M0L6_2atmpS1659 * 2;
  _M0L6_2atmpS1656 = _M0L3lenS1657 + _M0L6_2atmpS1658;
  moonbit_incref(_M0L4selfS201);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS201, _M0L6_2atmpS1656);
  _M0L8_2afieldS3399 = _M0L4selfS201->$0;
  _M0L4dataS1660 = _M0L8_2afieldS3399;
  _M0L3lenS1661 = _M0L4selfS201->$1;
  _M0L6_2atmpS1662 = Moonbit_array_length(_M0L3strS202);
  moonbit_incref(_M0L4dataS1660);
  moonbit_incref(_M0L3strS202);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1660, _M0L3lenS1661, _M0L3strS202, 0, _M0L6_2atmpS1662);
  _M0L3lenS1664 = _M0L4selfS201->$1;
  _M0L6_2atmpS3398 = Moonbit_array_length(_M0L3strS202);
  moonbit_decref(_M0L3strS202);
  _M0L6_2atmpS1666 = _M0L6_2atmpS3398;
  _M0L6_2atmpS1665 = _M0L6_2atmpS1666 * 2;
  _M0L6_2atmpS1663 = _M0L3lenS1664 + _M0L6_2atmpS1665;
  _M0L4selfS201->$1 = _M0L6_2atmpS1663;
  moonbit_decref(_M0L4selfS201);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS193,
  int32_t _M0L13bytes__offsetS188,
  moonbit_string_t _M0L3strS195,
  int32_t _M0L11str__offsetS191,
  int32_t _M0L6lengthS189
) {
  int32_t _M0L6_2atmpS1655;
  int32_t _M0L6_2atmpS1654;
  int32_t _M0L2e1S187;
  int32_t _M0L6_2atmpS1653;
  int32_t _M0L2e2S190;
  int32_t _M0L4len1S192;
  int32_t _M0L4len2S194;
  int32_t _if__result_3738;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1655 = _M0L6lengthS189 * 2;
  _M0L6_2atmpS1654 = _M0L13bytes__offsetS188 + _M0L6_2atmpS1655;
  _M0L2e1S187 = _M0L6_2atmpS1654 - 1;
  _M0L6_2atmpS1653 = _M0L11str__offsetS191 + _M0L6lengthS189;
  _M0L2e2S190 = _M0L6_2atmpS1653 - 1;
  _M0L4len1S192 = Moonbit_array_length(_M0L4selfS193);
  _M0L4len2S194 = Moonbit_array_length(_M0L3strS195);
  if (_M0L6lengthS189 >= 0) {
    if (_M0L13bytes__offsetS188 >= 0) {
      if (_M0L2e1S187 < _M0L4len1S192) {
        if (_M0L11str__offsetS191 >= 0) {
          _if__result_3738 = _M0L2e2S190 < _M0L4len2S194;
        } else {
          _if__result_3738 = 0;
        }
      } else {
        _if__result_3738 = 0;
      }
    } else {
      _if__result_3738 = 0;
    }
  } else {
    _if__result_3738 = 0;
  }
  if (_if__result_3738) {
    int32_t _M0L16end__str__offsetS196 =
      _M0L11str__offsetS191 + _M0L6lengthS189;
    int32_t _M0L1iS197 = _M0L11str__offsetS191;
    int32_t _M0L1jS198 = _M0L13bytes__offsetS188;
    while (1) {
      if (_M0L1iS197 < _M0L16end__str__offsetS196) {
        int32_t _M0L6_2atmpS1650 = _M0L3strS195[_M0L1iS197];
        int32_t _M0L6_2atmpS1649 = (int32_t)_M0L6_2atmpS1650;
        uint32_t _M0L1cS199 = *(uint32_t*)&_M0L6_2atmpS1649;
        uint32_t _M0L6_2atmpS1645 = _M0L1cS199 & 255u;
        int32_t _M0L6_2atmpS1644;
        int32_t _M0L6_2atmpS1646;
        uint32_t _M0L6_2atmpS1648;
        int32_t _M0L6_2atmpS1647;
        int32_t _M0L6_2atmpS1651;
        int32_t _M0L6_2atmpS1652;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1644 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1645);
        if (
          _M0L1jS198 < 0 || _M0L1jS198 >= Moonbit_array_length(_M0L4selfS193)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS193[_M0L1jS198] = _M0L6_2atmpS1644;
        _M0L6_2atmpS1646 = _M0L1jS198 + 1;
        _M0L6_2atmpS1648 = _M0L1cS199 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1647 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1648);
        if (
          _M0L6_2atmpS1646 < 0
          || _M0L6_2atmpS1646 >= Moonbit_array_length(_M0L4selfS193)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS193[_M0L6_2atmpS1646] = _M0L6_2atmpS1647;
        _M0L6_2atmpS1651 = _M0L1iS197 + 1;
        _M0L6_2atmpS1652 = _M0L1jS198 + 2;
        _M0L1iS197 = _M0L6_2atmpS1651;
        _M0L1jS198 = _M0L6_2atmpS1652;
        continue;
      } else {
        moonbit_decref(_M0L3strS195);
        moonbit_decref(_M0L4selfS193);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS195);
    moonbit_decref(_M0L4selfS193);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS184,
  double _M0L3objS183
) {
  struct _M0TPB6Logger _M0L6_2atmpS1642;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1642
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS184
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS183, _M0L6_2atmpS1642);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS186,
  struct _M0TPC16string10StringView _M0L3objS185
) {
  struct _M0TPB6Logger _M0L6_2atmpS1643;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1643
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS186
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS185, _M0L6_2atmpS1643);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS129
) {
  int32_t _M0L6_2atmpS1641;
  struct _M0TPC16string10StringView _M0L7_2abindS128;
  moonbit_string_t _M0L7_2adataS130;
  int32_t _M0L8_2astartS131;
  int32_t _M0L6_2atmpS1640;
  int32_t _M0L6_2aendS132;
  int32_t _M0Lm9_2acursorS133;
  int32_t _M0Lm13accept__stateS134;
  int32_t _M0Lm10match__endS135;
  int32_t _M0Lm20match__tag__saver__0S136;
  int32_t _M0Lm20match__tag__saver__1S137;
  int32_t _M0Lm20match__tag__saver__2S138;
  int32_t _M0Lm20match__tag__saver__3S139;
  int32_t _M0Lm20match__tag__saver__4S140;
  int32_t _M0Lm6tag__0S141;
  int32_t _M0Lm6tag__1S142;
  int32_t _M0Lm9tag__1__1S143;
  int32_t _M0Lm9tag__1__2S144;
  int32_t _M0Lm6tag__3S145;
  int32_t _M0Lm6tag__2S146;
  int32_t _M0Lm9tag__2__1S147;
  int32_t _M0Lm6tag__4S148;
  int32_t _M0L6_2atmpS1598;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1641 = Moonbit_array_length(_M0L4reprS129);
  _M0L7_2abindS128
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1641, _M0L4reprS129
  };
  moonbit_incref(_M0L7_2abindS128.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS130 = _M0MPC16string10StringView4data(_M0L7_2abindS128);
  moonbit_incref(_M0L7_2abindS128.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS131
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS128);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1640 = _M0MPC16string10StringView6length(_M0L7_2abindS128);
  _M0L6_2aendS132 = _M0L8_2astartS131 + _M0L6_2atmpS1640;
  _M0Lm9_2acursorS133 = _M0L8_2astartS131;
  _M0Lm13accept__stateS134 = -1;
  _M0Lm10match__endS135 = -1;
  _M0Lm20match__tag__saver__0S136 = -1;
  _M0Lm20match__tag__saver__1S137 = -1;
  _M0Lm20match__tag__saver__2S138 = -1;
  _M0Lm20match__tag__saver__3S139 = -1;
  _M0Lm20match__tag__saver__4S140 = -1;
  _M0Lm6tag__0S141 = -1;
  _M0Lm6tag__1S142 = -1;
  _M0Lm9tag__1__1S143 = -1;
  _M0Lm9tag__1__2S144 = -1;
  _M0Lm6tag__3S145 = -1;
  _M0Lm6tag__2S146 = -1;
  _M0Lm9tag__2__1S147 = -1;
  _M0Lm6tag__4S148 = -1;
  _M0L6_2atmpS1598 = _M0Lm9_2acursorS133;
  if (_M0L6_2atmpS1598 < _M0L6_2aendS132) {
    int32_t _M0L6_2atmpS1600 = _M0Lm9_2acursorS133;
    int32_t _M0L6_2atmpS1599;
    moonbit_incref(_M0L7_2adataS130);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1599
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1600);
    if (_M0L6_2atmpS1599 == 64) {
      int32_t _M0L6_2atmpS1601 = _M0Lm9_2acursorS133;
      _M0Lm9_2acursorS133 = _M0L6_2atmpS1601 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1602;
        _M0Lm6tag__0S141 = _M0Lm9_2acursorS133;
        _M0L6_2atmpS1602 = _M0Lm9_2acursorS133;
        if (_M0L6_2atmpS1602 < _M0L6_2aendS132) {
          int32_t _M0L6_2atmpS1639 = _M0Lm9_2acursorS133;
          int32_t _M0L10next__charS156;
          int32_t _M0L6_2atmpS1603;
          moonbit_incref(_M0L7_2adataS130);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS156
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1639);
          _M0L6_2atmpS1603 = _M0Lm9_2acursorS133;
          _M0Lm9_2acursorS133 = _M0L6_2atmpS1603 + 1;
          if (_M0L10next__charS156 == 58) {
            int32_t _M0L6_2atmpS1604 = _M0Lm9_2acursorS133;
            if (_M0L6_2atmpS1604 < _M0L6_2aendS132) {
              int32_t _M0L6_2atmpS1605 = _M0Lm9_2acursorS133;
              int32_t _M0L12dispatch__15S157;
              _M0Lm9_2acursorS133 = _M0L6_2atmpS1605 + 1;
              _M0L12dispatch__15S157 = 0;
              loop__label__15_160:;
              while (1) {
                int32_t _M0L6_2atmpS1606;
                switch (_M0L12dispatch__15S157) {
                  case 3: {
                    int32_t _M0L6_2atmpS1609;
                    _M0Lm9tag__1__2S144 = _M0Lm9tag__1__1S143;
                    _M0Lm9tag__1__1S143 = _M0Lm6tag__1S142;
                    _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                    _M0L6_2atmpS1609 = _M0Lm9_2acursorS133;
                    if (_M0L6_2atmpS1609 < _M0L6_2aendS132) {
                      int32_t _M0L6_2atmpS1614 = _M0Lm9_2acursorS133;
                      int32_t _M0L10next__charS164;
                      int32_t _M0L6_2atmpS1610;
                      moonbit_incref(_M0L7_2adataS130);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS164
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1614);
                      _M0L6_2atmpS1610 = _M0Lm9_2acursorS133;
                      _M0Lm9_2acursorS133 = _M0L6_2atmpS1610 + 1;
                      if (_M0L10next__charS164 < 58) {
                        if (_M0L10next__charS164 < 48) {
                          goto join_163;
                        } else {
                          int32_t _M0L6_2atmpS1611;
                          _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                          _M0Lm9tag__2__1S147 = _M0Lm6tag__2S146;
                          _M0Lm6tag__2S146 = _M0Lm9_2acursorS133;
                          _M0Lm6tag__3S145 = _M0Lm9_2acursorS133;
                          _M0L6_2atmpS1611 = _M0Lm9_2acursorS133;
                          if (_M0L6_2atmpS1611 < _M0L6_2aendS132) {
                            int32_t _M0L6_2atmpS1613 = _M0Lm9_2acursorS133;
                            int32_t _M0L10next__charS166;
                            int32_t _M0L6_2atmpS1612;
                            moonbit_incref(_M0L7_2adataS130);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS166
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1613);
                            _M0L6_2atmpS1612 = _M0Lm9_2acursorS133;
                            _M0Lm9_2acursorS133 = _M0L6_2atmpS1612 + 1;
                            if (_M0L10next__charS166 < 48) {
                              if (_M0L10next__charS166 == 45) {
                                goto join_158;
                              } else {
                                goto join_165;
                              }
                            } else if (_M0L10next__charS166 > 57) {
                              if (_M0L10next__charS166 < 59) {
                                _M0L12dispatch__15S157 = 3;
                                goto loop__label__15_160;
                              } else {
                                goto join_165;
                              }
                            } else {
                              _M0L12dispatch__15S157 = 6;
                              goto loop__label__15_160;
                            }
                            join_165:;
                            _M0L12dispatch__15S157 = 0;
                            goto loop__label__15_160;
                          } else {
                            goto join_149;
                          }
                        }
                      } else if (_M0L10next__charS164 > 58) {
                        goto join_163;
                      } else {
                        _M0L12dispatch__15S157 = 1;
                        goto loop__label__15_160;
                      }
                      join_163:;
                      _M0L12dispatch__15S157 = 0;
                      goto loop__label__15_160;
                    } else {
                      goto join_149;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1615;
                    _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                    _M0Lm6tag__2S146 = _M0Lm9_2acursorS133;
                    _M0L6_2atmpS1615 = _M0Lm9_2acursorS133;
                    if (_M0L6_2atmpS1615 < _M0L6_2aendS132) {
                      int32_t _M0L6_2atmpS1617 = _M0Lm9_2acursorS133;
                      int32_t _M0L10next__charS168;
                      int32_t _M0L6_2atmpS1616;
                      moonbit_incref(_M0L7_2adataS130);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS168
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1617);
                      _M0L6_2atmpS1616 = _M0Lm9_2acursorS133;
                      _M0Lm9_2acursorS133 = _M0L6_2atmpS1616 + 1;
                      if (_M0L10next__charS168 < 58) {
                        if (_M0L10next__charS168 < 48) {
                          goto join_167;
                        } else {
                          _M0L12dispatch__15S157 = 2;
                          goto loop__label__15_160;
                        }
                      } else if (_M0L10next__charS168 > 58) {
                        goto join_167;
                      } else {
                        _M0L12dispatch__15S157 = 3;
                        goto loop__label__15_160;
                      }
                      join_167:;
                      _M0L12dispatch__15S157 = 0;
                      goto loop__label__15_160;
                    } else {
                      goto join_149;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1618;
                    _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                    _M0L6_2atmpS1618 = _M0Lm9_2acursorS133;
                    if (_M0L6_2atmpS1618 < _M0L6_2aendS132) {
                      int32_t _M0L6_2atmpS1620 = _M0Lm9_2acursorS133;
                      int32_t _M0L10next__charS169;
                      int32_t _M0L6_2atmpS1619;
                      moonbit_incref(_M0L7_2adataS130);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS169
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1620);
                      _M0L6_2atmpS1619 = _M0Lm9_2acursorS133;
                      _M0Lm9_2acursorS133 = _M0L6_2atmpS1619 + 1;
                      if (_M0L10next__charS169 == 58) {
                        _M0L12dispatch__15S157 = 1;
                        goto loop__label__15_160;
                      } else {
                        _M0L12dispatch__15S157 = 0;
                        goto loop__label__15_160;
                      }
                    } else {
                      goto join_149;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1621;
                    _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                    _M0Lm6tag__4S148 = _M0Lm9_2acursorS133;
                    _M0L6_2atmpS1621 = _M0Lm9_2acursorS133;
                    if (_M0L6_2atmpS1621 < _M0L6_2aendS132) {
                      int32_t _M0L6_2atmpS1629 = _M0Lm9_2acursorS133;
                      int32_t _M0L10next__charS171;
                      int32_t _M0L6_2atmpS1622;
                      moonbit_incref(_M0L7_2adataS130);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS171
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1629);
                      _M0L6_2atmpS1622 = _M0Lm9_2acursorS133;
                      _M0Lm9_2acursorS133 = _M0L6_2atmpS1622 + 1;
                      if (_M0L10next__charS171 < 58) {
                        if (_M0L10next__charS171 < 48) {
                          goto join_170;
                        } else {
                          _M0L12dispatch__15S157 = 4;
                          goto loop__label__15_160;
                        }
                      } else if (_M0L10next__charS171 > 58) {
                        goto join_170;
                      } else {
                        int32_t _M0L6_2atmpS1623;
                        _M0Lm9tag__1__2S144 = _M0Lm9tag__1__1S143;
                        _M0Lm9tag__1__1S143 = _M0Lm6tag__1S142;
                        _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                        _M0L6_2atmpS1623 = _M0Lm9_2acursorS133;
                        if (_M0L6_2atmpS1623 < _M0L6_2aendS132) {
                          int32_t _M0L6_2atmpS1628 = _M0Lm9_2acursorS133;
                          int32_t _M0L10next__charS173;
                          int32_t _M0L6_2atmpS1624;
                          moonbit_incref(_M0L7_2adataS130);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS173
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1628);
                          _M0L6_2atmpS1624 = _M0Lm9_2acursorS133;
                          _M0Lm9_2acursorS133 = _M0L6_2atmpS1624 + 1;
                          if (_M0L10next__charS173 < 58) {
                            if (_M0L10next__charS173 < 48) {
                              goto join_172;
                            } else {
                              int32_t _M0L6_2atmpS1625;
                              _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                              _M0Lm9tag__2__1S147 = _M0Lm6tag__2S146;
                              _M0Lm6tag__2S146 = _M0Lm9_2acursorS133;
                              _M0L6_2atmpS1625 = _M0Lm9_2acursorS133;
                              if (_M0L6_2atmpS1625 < _M0L6_2aendS132) {
                                int32_t _M0L6_2atmpS1627 =
                                  _M0Lm9_2acursorS133;
                                int32_t _M0L10next__charS175;
                                int32_t _M0L6_2atmpS1626;
                                moonbit_incref(_M0L7_2adataS130);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS175
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1627);
                                _M0L6_2atmpS1626 = _M0Lm9_2acursorS133;
                                _M0Lm9_2acursorS133 = _M0L6_2atmpS1626 + 1;
                                if (_M0L10next__charS175 < 58) {
                                  if (_M0L10next__charS175 < 48) {
                                    goto join_174;
                                  } else {
                                    _M0L12dispatch__15S157 = 5;
                                    goto loop__label__15_160;
                                  }
                                } else if (_M0L10next__charS175 > 58) {
                                  goto join_174;
                                } else {
                                  _M0L12dispatch__15S157 = 3;
                                  goto loop__label__15_160;
                                }
                                join_174:;
                                _M0L12dispatch__15S157 = 0;
                                goto loop__label__15_160;
                              } else {
                                goto join_162;
                              }
                            }
                          } else if (_M0L10next__charS173 > 58) {
                            goto join_172;
                          } else {
                            _M0L12dispatch__15S157 = 1;
                            goto loop__label__15_160;
                          }
                          join_172:;
                          _M0L12dispatch__15S157 = 0;
                          goto loop__label__15_160;
                        } else {
                          goto join_149;
                        }
                      }
                      join_170:;
                      _M0L12dispatch__15S157 = 0;
                      goto loop__label__15_160;
                    } else {
                      goto join_149;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1630;
                    _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                    _M0Lm6tag__2S146 = _M0Lm9_2acursorS133;
                    _M0L6_2atmpS1630 = _M0Lm9_2acursorS133;
                    if (_M0L6_2atmpS1630 < _M0L6_2aendS132) {
                      int32_t _M0L6_2atmpS1632 = _M0Lm9_2acursorS133;
                      int32_t _M0L10next__charS177;
                      int32_t _M0L6_2atmpS1631;
                      moonbit_incref(_M0L7_2adataS130);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS177
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1632);
                      _M0L6_2atmpS1631 = _M0Lm9_2acursorS133;
                      _M0Lm9_2acursorS133 = _M0L6_2atmpS1631 + 1;
                      if (_M0L10next__charS177 < 58) {
                        if (_M0L10next__charS177 < 48) {
                          goto join_176;
                        } else {
                          _M0L12dispatch__15S157 = 5;
                          goto loop__label__15_160;
                        }
                      } else if (_M0L10next__charS177 > 58) {
                        goto join_176;
                      } else {
                        _M0L12dispatch__15S157 = 3;
                        goto loop__label__15_160;
                      }
                      join_176:;
                      _M0L12dispatch__15S157 = 0;
                      goto loop__label__15_160;
                    } else {
                      goto join_162;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1633;
                    _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                    _M0Lm6tag__2S146 = _M0Lm9_2acursorS133;
                    _M0Lm6tag__3S145 = _M0Lm9_2acursorS133;
                    _M0L6_2atmpS1633 = _M0Lm9_2acursorS133;
                    if (_M0L6_2atmpS1633 < _M0L6_2aendS132) {
                      int32_t _M0L6_2atmpS1635 = _M0Lm9_2acursorS133;
                      int32_t _M0L10next__charS179;
                      int32_t _M0L6_2atmpS1634;
                      moonbit_incref(_M0L7_2adataS130);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS179
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1635);
                      _M0L6_2atmpS1634 = _M0Lm9_2acursorS133;
                      _M0Lm9_2acursorS133 = _M0L6_2atmpS1634 + 1;
                      if (_M0L10next__charS179 < 48) {
                        if (_M0L10next__charS179 == 45) {
                          goto join_158;
                        } else {
                          goto join_178;
                        }
                      } else if (_M0L10next__charS179 > 57) {
                        if (_M0L10next__charS179 < 59) {
                          _M0L12dispatch__15S157 = 3;
                          goto loop__label__15_160;
                        } else {
                          goto join_178;
                        }
                      } else {
                        _M0L12dispatch__15S157 = 6;
                        goto loop__label__15_160;
                      }
                      join_178:;
                      _M0L12dispatch__15S157 = 0;
                      goto loop__label__15_160;
                    } else {
                      goto join_149;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1636;
                    _M0Lm9tag__1__1S143 = _M0Lm6tag__1S142;
                    _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                    _M0L6_2atmpS1636 = _M0Lm9_2acursorS133;
                    if (_M0L6_2atmpS1636 < _M0L6_2aendS132) {
                      int32_t _M0L6_2atmpS1638 = _M0Lm9_2acursorS133;
                      int32_t _M0L10next__charS181;
                      int32_t _M0L6_2atmpS1637;
                      moonbit_incref(_M0L7_2adataS130);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS181
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1638);
                      _M0L6_2atmpS1637 = _M0Lm9_2acursorS133;
                      _M0Lm9_2acursorS133 = _M0L6_2atmpS1637 + 1;
                      if (_M0L10next__charS181 < 58) {
                        if (_M0L10next__charS181 < 48) {
                          goto join_180;
                        } else {
                          _M0L12dispatch__15S157 = 2;
                          goto loop__label__15_160;
                        }
                      } else if (_M0L10next__charS181 > 58) {
                        goto join_180;
                      } else {
                        _M0L12dispatch__15S157 = 1;
                        goto loop__label__15_160;
                      }
                      join_180:;
                      _M0L12dispatch__15S157 = 0;
                      goto loop__label__15_160;
                    } else {
                      goto join_149;
                    }
                    break;
                  }
                  default: {
                    goto join_149;
                    break;
                  }
                }
                join_162:;
                _M0Lm6tag__1S142 = _M0Lm9tag__1__2S144;
                _M0Lm6tag__2S146 = _M0Lm9tag__2__1S147;
                _M0Lm20match__tag__saver__0S136 = _M0Lm6tag__0S141;
                _M0Lm20match__tag__saver__1S137 = _M0Lm6tag__1S142;
                _M0Lm20match__tag__saver__2S138 = _M0Lm6tag__2S146;
                _M0Lm20match__tag__saver__3S139 = _M0Lm6tag__3S145;
                _M0Lm20match__tag__saver__4S140 = _M0Lm6tag__4S148;
                _M0Lm13accept__stateS134 = 0;
                _M0Lm10match__endS135 = _M0Lm9_2acursorS133;
                goto join_149;
                join_158:;
                _M0Lm9tag__1__1S143 = _M0Lm9tag__1__2S144;
                _M0Lm6tag__1S142 = _M0Lm9_2acursorS133;
                _M0Lm6tag__2S146 = _M0Lm9tag__2__1S147;
                _M0L6_2atmpS1606 = _M0Lm9_2acursorS133;
                if (_M0L6_2atmpS1606 < _M0L6_2aendS132) {
                  int32_t _M0L6_2atmpS1608 = _M0Lm9_2acursorS133;
                  int32_t _M0L10next__charS161;
                  int32_t _M0L6_2atmpS1607;
                  moonbit_incref(_M0L7_2adataS130);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS161
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS130, _M0L6_2atmpS1608);
                  _M0L6_2atmpS1607 = _M0Lm9_2acursorS133;
                  _M0Lm9_2acursorS133 = _M0L6_2atmpS1607 + 1;
                  if (_M0L10next__charS161 < 58) {
                    if (_M0L10next__charS161 < 48) {
                      goto join_159;
                    } else {
                      _M0L12dispatch__15S157 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS161 > 58) {
                    goto join_159;
                  } else {
                    _M0L12dispatch__15S157 = 1;
                    continue;
                  }
                  join_159:;
                  _M0L12dispatch__15S157 = 0;
                  continue;
                } else {
                  goto join_149;
                }
                break;
              }
            } else {
              goto join_149;
            }
          } else {
            continue;
          }
        } else {
          goto join_149;
        }
        break;
      }
    } else {
      goto join_149;
    }
  } else {
    goto join_149;
  }
  join_149:;
  switch (_M0Lm13accept__stateS134) {
    case 0: {
      int32_t _M0L6_2atmpS1597 = _M0Lm20match__tag__saver__1S137;
      int32_t _M0L6_2atmpS1596 = _M0L6_2atmpS1597 + 1;
      int64_t _M0L6_2atmpS1593 = (int64_t)_M0L6_2atmpS1596;
      int32_t _M0L6_2atmpS1595 = _M0Lm20match__tag__saver__2S138;
      int64_t _M0L6_2atmpS1594 = (int64_t)_M0L6_2atmpS1595;
      struct _M0TPC16string10StringView _M0L11start__lineS150;
      int32_t _M0L6_2atmpS1592;
      int32_t _M0L6_2atmpS1591;
      int64_t _M0L6_2atmpS1588;
      int32_t _M0L6_2atmpS1590;
      int64_t _M0L6_2atmpS1589;
      struct _M0TPC16string10StringView _M0L13start__columnS151;
      int32_t _M0L6_2atmpS1587;
      int64_t _M0L6_2atmpS1584;
      int32_t _M0L6_2atmpS1586;
      int64_t _M0L6_2atmpS1585;
      struct _M0TPC16string10StringView _M0L3pkgS152;
      int32_t _M0L6_2atmpS1583;
      int32_t _M0L6_2atmpS1582;
      int64_t _M0L6_2atmpS1579;
      int32_t _M0L6_2atmpS1581;
      int64_t _M0L6_2atmpS1580;
      struct _M0TPC16string10StringView _M0L8filenameS153;
      int32_t _M0L6_2atmpS1578;
      int32_t _M0L6_2atmpS1577;
      int64_t _M0L6_2atmpS1574;
      int32_t _M0L6_2atmpS1576;
      int64_t _M0L6_2atmpS1575;
      struct _M0TPC16string10StringView _M0L9end__lineS154;
      int32_t _M0L6_2atmpS1573;
      int32_t _M0L6_2atmpS1572;
      int64_t _M0L6_2atmpS1569;
      int32_t _M0L6_2atmpS1571;
      int64_t _M0L6_2atmpS1570;
      struct _M0TPC16string10StringView _M0L11end__columnS155;
      struct _M0TPB13SourceLocRepr* _block_3755;
      moonbit_incref(_M0L7_2adataS130);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS150
      = _M0MPC16string6String4view(_M0L7_2adataS130, _M0L6_2atmpS1593, _M0L6_2atmpS1594);
      _M0L6_2atmpS1592 = _M0Lm20match__tag__saver__2S138;
      _M0L6_2atmpS1591 = _M0L6_2atmpS1592 + 1;
      _M0L6_2atmpS1588 = (int64_t)_M0L6_2atmpS1591;
      _M0L6_2atmpS1590 = _M0Lm20match__tag__saver__3S139;
      _M0L6_2atmpS1589 = (int64_t)_M0L6_2atmpS1590;
      moonbit_incref(_M0L7_2adataS130);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS151
      = _M0MPC16string6String4view(_M0L7_2adataS130, _M0L6_2atmpS1588, _M0L6_2atmpS1589);
      _M0L6_2atmpS1587 = _M0L8_2astartS131 + 1;
      _M0L6_2atmpS1584 = (int64_t)_M0L6_2atmpS1587;
      _M0L6_2atmpS1586 = _M0Lm20match__tag__saver__0S136;
      _M0L6_2atmpS1585 = (int64_t)_M0L6_2atmpS1586;
      moonbit_incref(_M0L7_2adataS130);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS152
      = _M0MPC16string6String4view(_M0L7_2adataS130, _M0L6_2atmpS1584, _M0L6_2atmpS1585);
      _M0L6_2atmpS1583 = _M0Lm20match__tag__saver__0S136;
      _M0L6_2atmpS1582 = _M0L6_2atmpS1583 + 1;
      _M0L6_2atmpS1579 = (int64_t)_M0L6_2atmpS1582;
      _M0L6_2atmpS1581 = _M0Lm20match__tag__saver__1S137;
      _M0L6_2atmpS1580 = (int64_t)_M0L6_2atmpS1581;
      moonbit_incref(_M0L7_2adataS130);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS153
      = _M0MPC16string6String4view(_M0L7_2adataS130, _M0L6_2atmpS1579, _M0L6_2atmpS1580);
      _M0L6_2atmpS1578 = _M0Lm20match__tag__saver__3S139;
      _M0L6_2atmpS1577 = _M0L6_2atmpS1578 + 1;
      _M0L6_2atmpS1574 = (int64_t)_M0L6_2atmpS1577;
      _M0L6_2atmpS1576 = _M0Lm20match__tag__saver__4S140;
      _M0L6_2atmpS1575 = (int64_t)_M0L6_2atmpS1576;
      moonbit_incref(_M0L7_2adataS130);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS154
      = _M0MPC16string6String4view(_M0L7_2adataS130, _M0L6_2atmpS1574, _M0L6_2atmpS1575);
      _M0L6_2atmpS1573 = _M0Lm20match__tag__saver__4S140;
      _M0L6_2atmpS1572 = _M0L6_2atmpS1573 + 1;
      _M0L6_2atmpS1569 = (int64_t)_M0L6_2atmpS1572;
      _M0L6_2atmpS1571 = _M0Lm10match__endS135;
      _M0L6_2atmpS1570 = (int64_t)_M0L6_2atmpS1571;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS155
      = _M0MPC16string6String4view(_M0L7_2adataS130, _M0L6_2atmpS1569, _M0L6_2atmpS1570);
      _block_3755
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3755)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3755->$0_0 = _M0L3pkgS152.$0;
      _block_3755->$0_1 = _M0L3pkgS152.$1;
      _block_3755->$0_2 = _M0L3pkgS152.$2;
      _block_3755->$1_0 = _M0L8filenameS153.$0;
      _block_3755->$1_1 = _M0L8filenameS153.$1;
      _block_3755->$1_2 = _M0L8filenameS153.$2;
      _block_3755->$2_0 = _M0L11start__lineS150.$0;
      _block_3755->$2_1 = _M0L11start__lineS150.$1;
      _block_3755->$2_2 = _M0L11start__lineS150.$2;
      _block_3755->$3_0 = _M0L13start__columnS151.$0;
      _block_3755->$3_1 = _M0L13start__columnS151.$1;
      _block_3755->$3_2 = _M0L13start__columnS151.$2;
      _block_3755->$4_0 = _M0L9end__lineS154.$0;
      _block_3755->$4_1 = _M0L9end__lineS154.$1;
      _block_3755->$4_2 = _M0L9end__lineS154.$2;
      _block_3755->$5_0 = _M0L11end__columnS155.$0;
      _block_3755->$5_1 = _M0L11end__columnS155.$1;
      _block_3755->$5_2 = _M0L11end__columnS155.$2;
      return _block_3755;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS130);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS126,
  int32_t _M0L5indexS127
) {
  int32_t _M0L3lenS125;
  int32_t _if__result_3756;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS125 = _M0L4selfS126->$1;
  if (_M0L5indexS127 >= 0) {
    _if__result_3756 = _M0L5indexS127 < _M0L3lenS125;
  } else {
    _if__result_3756 = 0;
  }
  if (_if__result_3756) {
    moonbit_string_t* _M0L6_2atmpS1568;
    moonbit_string_t _M0L6_2atmpS3400;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1568 = _M0MPC15array5Array6bufferGsE(_M0L4selfS126);
    if (
      _M0L5indexS127 < 0
      || _M0L5indexS127 >= Moonbit_array_length(_M0L6_2atmpS1568)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3400 = (moonbit_string_t)_M0L6_2atmpS1568[_M0L5indexS127];
    moonbit_incref(_M0L6_2atmpS3400);
    moonbit_decref(_M0L6_2atmpS1568);
    return _M0L6_2atmpS3400;
  } else {
    moonbit_decref(_M0L4selfS126);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS122
) {
  moonbit_string_t* _M0L8_2afieldS3401;
  int32_t _M0L6_2acntS3552;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3401 = _M0L4selfS122->$0;
  _M0L6_2acntS3552 = Moonbit_object_header(_M0L4selfS122)->rc;
  if (_M0L6_2acntS3552 > 1) {
    int32_t _M0L11_2anew__cntS3553 = _M0L6_2acntS3552 - 1;
    Moonbit_object_header(_M0L4selfS122)->rc = _M0L11_2anew__cntS3553;
    moonbit_incref(_M0L8_2afieldS3401);
  } else if (_M0L6_2acntS3552 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS122);
  }
  return _M0L8_2afieldS3401;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS123
) {
  struct _M0TUsiE** _M0L8_2afieldS3402;
  int32_t _M0L6_2acntS3554;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3402 = _M0L4selfS123->$0;
  _M0L6_2acntS3554 = Moonbit_object_header(_M0L4selfS123)->rc;
  if (_M0L6_2acntS3554 > 1) {
    int32_t _M0L11_2anew__cntS3555 = _M0L6_2acntS3554 - 1;
    Moonbit_object_header(_M0L4selfS123)->rc = _M0L11_2anew__cntS3555;
    moonbit_incref(_M0L8_2afieldS3402);
  } else if (_M0L6_2acntS3554 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS123);
  }
  return _M0L8_2afieldS3402;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS124
) {
  void** _M0L8_2afieldS3403;
  int32_t _M0L6_2acntS3556;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3403 = _M0L4selfS124->$0;
  _M0L6_2acntS3556 = Moonbit_object_header(_M0L4selfS124)->rc;
  if (_M0L6_2acntS3556 > 1) {
    int32_t _M0L11_2anew__cntS3557 = _M0L6_2acntS3556 - 1;
    Moonbit_object_header(_M0L4selfS124)->rc = _M0L11_2anew__cntS3557;
    moonbit_incref(_M0L8_2afieldS3403);
  } else if (_M0L6_2acntS3556 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS124);
  }
  return _M0L8_2afieldS3403;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS121) {
  struct _M0TPB13StringBuilder* _M0L3bufS120;
  struct _M0TPB6Logger _M0L6_2atmpS1567;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS120 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS120);
  _M0L6_2atmpS1567
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS120
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS121, _M0L6_2atmpS1567);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS120);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS117,
  int32_t _M0L5indexS118
) {
  int32_t _M0L2c1S116;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S116 = _M0L4selfS117[_M0L5indexS118];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S116)) {
    int32_t _M0L6_2atmpS1566 = _M0L5indexS118 + 1;
    int32_t _M0L6_2atmpS3404 = _M0L4selfS117[_M0L6_2atmpS1566];
    int32_t _M0L2c2S119;
    int32_t _M0L6_2atmpS1564;
    int32_t _M0L6_2atmpS1565;
    moonbit_decref(_M0L4selfS117);
    _M0L2c2S119 = _M0L6_2atmpS3404;
    _M0L6_2atmpS1564 = (int32_t)_M0L2c1S116;
    _M0L6_2atmpS1565 = (int32_t)_M0L2c2S119;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1564, _M0L6_2atmpS1565);
  } else {
    moonbit_decref(_M0L4selfS117);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S116);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS115) {
  int32_t _M0L6_2atmpS1563;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1563 = (int32_t)_M0L4selfS115;
  return _M0L6_2atmpS1563;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS113,
  int32_t _M0L8trailingS114
) {
  int32_t _M0L6_2atmpS1562;
  int32_t _M0L6_2atmpS1561;
  int32_t _M0L6_2atmpS1560;
  int32_t _M0L6_2atmpS1559;
  int32_t _M0L6_2atmpS1558;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1562 = _M0L7leadingS113 - 55296;
  _M0L6_2atmpS1561 = _M0L6_2atmpS1562 * 1024;
  _M0L6_2atmpS1560 = _M0L6_2atmpS1561 + _M0L8trailingS114;
  _M0L6_2atmpS1559 = _M0L6_2atmpS1560 - 56320;
  _M0L6_2atmpS1558 = _M0L6_2atmpS1559 + 65536;
  return _M0L6_2atmpS1558;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS112) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS112 >= 56320) {
    return _M0L4selfS112 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS111) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS111 >= 55296) {
    return _M0L4selfS111 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS108,
  int32_t _M0L2chS110
) {
  int32_t _M0L3lenS1553;
  int32_t _M0L6_2atmpS1552;
  moonbit_bytes_t _M0L8_2afieldS3405;
  moonbit_bytes_t _M0L4dataS1556;
  int32_t _M0L3lenS1557;
  int32_t _M0L3incS109;
  int32_t _M0L3lenS1555;
  int32_t _M0L6_2atmpS1554;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1553 = _M0L4selfS108->$1;
  _M0L6_2atmpS1552 = _M0L3lenS1553 + 4;
  moonbit_incref(_M0L4selfS108);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS108, _M0L6_2atmpS1552);
  _M0L8_2afieldS3405 = _M0L4selfS108->$0;
  _M0L4dataS1556 = _M0L8_2afieldS3405;
  _M0L3lenS1557 = _M0L4selfS108->$1;
  moonbit_incref(_M0L4dataS1556);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS109
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1556, _M0L3lenS1557, _M0L2chS110);
  _M0L3lenS1555 = _M0L4selfS108->$1;
  _M0L6_2atmpS1554 = _M0L3lenS1555 + _M0L3incS109;
  _M0L4selfS108->$1 = _M0L6_2atmpS1554;
  moonbit_decref(_M0L4selfS108);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS103,
  int32_t _M0L8requiredS104
) {
  moonbit_bytes_t _M0L8_2afieldS3409;
  moonbit_bytes_t _M0L4dataS1551;
  int32_t _M0L6_2atmpS3408;
  int32_t _M0L12current__lenS102;
  int32_t _M0Lm13enough__spaceS105;
  int32_t _M0L6_2atmpS1549;
  int32_t _M0L6_2atmpS1550;
  moonbit_bytes_t _M0L9new__dataS107;
  moonbit_bytes_t _M0L8_2afieldS3407;
  moonbit_bytes_t _M0L4dataS1547;
  int32_t _M0L3lenS1548;
  moonbit_bytes_t _M0L6_2aoldS3406;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3409 = _M0L4selfS103->$0;
  _M0L4dataS1551 = _M0L8_2afieldS3409;
  _M0L6_2atmpS3408 = Moonbit_array_length(_M0L4dataS1551);
  _M0L12current__lenS102 = _M0L6_2atmpS3408;
  if (_M0L8requiredS104 <= _M0L12current__lenS102) {
    moonbit_decref(_M0L4selfS103);
    return 0;
  }
  _M0Lm13enough__spaceS105 = _M0L12current__lenS102;
  while (1) {
    int32_t _M0L6_2atmpS1545 = _M0Lm13enough__spaceS105;
    if (_M0L6_2atmpS1545 < _M0L8requiredS104) {
      int32_t _M0L6_2atmpS1546 = _M0Lm13enough__spaceS105;
      _M0Lm13enough__spaceS105 = _M0L6_2atmpS1546 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1549 = _M0Lm13enough__spaceS105;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1550 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS107
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1549, _M0L6_2atmpS1550);
  _M0L8_2afieldS3407 = _M0L4selfS103->$0;
  _M0L4dataS1547 = _M0L8_2afieldS3407;
  _M0L3lenS1548 = _M0L4selfS103->$1;
  moonbit_incref(_M0L4dataS1547);
  moonbit_incref(_M0L9new__dataS107);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS107, 0, _M0L4dataS1547, 0, _M0L3lenS1548);
  _M0L6_2aoldS3406 = _M0L4selfS103->$0;
  moonbit_decref(_M0L6_2aoldS3406);
  _M0L4selfS103->$0 = _M0L9new__dataS107;
  moonbit_decref(_M0L4selfS103);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS97,
  int32_t _M0L6offsetS98,
  int32_t _M0L5valueS96
) {
  uint32_t _M0L4codeS95;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS95 = _M0MPC14char4Char8to__uint(_M0L5valueS96);
  if (_M0L4codeS95 < 65536u) {
    uint32_t _M0L6_2atmpS1528 = _M0L4codeS95 & 255u;
    int32_t _M0L6_2atmpS1527;
    int32_t _M0L6_2atmpS1529;
    uint32_t _M0L6_2atmpS1531;
    int32_t _M0L6_2atmpS1530;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1527 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1528);
    if (
      _M0L6offsetS98 < 0
      || _M0L6offsetS98 >= Moonbit_array_length(_M0L4selfS97)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS97[_M0L6offsetS98] = _M0L6_2atmpS1527;
    _M0L6_2atmpS1529 = _M0L6offsetS98 + 1;
    _M0L6_2atmpS1531 = _M0L4codeS95 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1530 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1531);
    if (
      _M0L6_2atmpS1529 < 0
      || _M0L6_2atmpS1529 >= Moonbit_array_length(_M0L4selfS97)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS97[_M0L6_2atmpS1529] = _M0L6_2atmpS1530;
    moonbit_decref(_M0L4selfS97);
    return 2;
  } else if (_M0L4codeS95 < 1114112u) {
    uint32_t _M0L2hiS99 = _M0L4codeS95 - 65536u;
    uint32_t _M0L6_2atmpS1544 = _M0L2hiS99 >> 10;
    uint32_t _M0L2loS100 = _M0L6_2atmpS1544 | 55296u;
    uint32_t _M0L6_2atmpS1543 = _M0L2hiS99 & 1023u;
    uint32_t _M0L2hiS101 = _M0L6_2atmpS1543 | 56320u;
    uint32_t _M0L6_2atmpS1533 = _M0L2loS100 & 255u;
    int32_t _M0L6_2atmpS1532;
    int32_t _M0L6_2atmpS1534;
    uint32_t _M0L6_2atmpS1536;
    int32_t _M0L6_2atmpS1535;
    int32_t _M0L6_2atmpS1537;
    uint32_t _M0L6_2atmpS1539;
    int32_t _M0L6_2atmpS1538;
    int32_t _M0L6_2atmpS1540;
    uint32_t _M0L6_2atmpS1542;
    int32_t _M0L6_2atmpS1541;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1532 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1533);
    if (
      _M0L6offsetS98 < 0
      || _M0L6offsetS98 >= Moonbit_array_length(_M0L4selfS97)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS97[_M0L6offsetS98] = _M0L6_2atmpS1532;
    _M0L6_2atmpS1534 = _M0L6offsetS98 + 1;
    _M0L6_2atmpS1536 = _M0L2loS100 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1535 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1536);
    if (
      _M0L6_2atmpS1534 < 0
      || _M0L6_2atmpS1534 >= Moonbit_array_length(_M0L4selfS97)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS97[_M0L6_2atmpS1534] = _M0L6_2atmpS1535;
    _M0L6_2atmpS1537 = _M0L6offsetS98 + 2;
    _M0L6_2atmpS1539 = _M0L2hiS101 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1538 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1539);
    if (
      _M0L6_2atmpS1537 < 0
      || _M0L6_2atmpS1537 >= Moonbit_array_length(_M0L4selfS97)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS97[_M0L6_2atmpS1537] = _M0L6_2atmpS1538;
    _M0L6_2atmpS1540 = _M0L6offsetS98 + 3;
    _M0L6_2atmpS1542 = _M0L2hiS101 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1541 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1542);
    if (
      _M0L6_2atmpS1540 < 0
      || _M0L6_2atmpS1540 >= Moonbit_array_length(_M0L4selfS97)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS97[_M0L6_2atmpS1540] = _M0L6_2atmpS1541;
    moonbit_decref(_M0L4selfS97);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS97);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_84.data, (moonbit_string_t)moonbit_string_literal_85.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS94) {
  int32_t _M0L6_2atmpS1526;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1526 = *(int32_t*)&_M0L4selfS94;
  return _M0L6_2atmpS1526 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS93) {
  int32_t _M0L6_2atmpS1525;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1525 = _M0L4selfS93;
  return *(uint32_t*)&_M0L6_2atmpS1525;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS92
) {
  moonbit_bytes_t _M0L8_2afieldS3411;
  moonbit_bytes_t _M0L4dataS1524;
  moonbit_bytes_t _M0L6_2atmpS1521;
  int32_t _M0L8_2afieldS3410;
  int32_t _M0L3lenS1523;
  int64_t _M0L6_2atmpS1522;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3411 = _M0L4selfS92->$0;
  _M0L4dataS1524 = _M0L8_2afieldS3411;
  moonbit_incref(_M0L4dataS1524);
  _M0L6_2atmpS1521 = _M0L4dataS1524;
  _M0L8_2afieldS3410 = _M0L4selfS92->$1;
  moonbit_decref(_M0L4selfS92);
  _M0L3lenS1523 = _M0L8_2afieldS3410;
  _M0L6_2atmpS1522 = (int64_t)_M0L3lenS1523;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1521, 0, _M0L6_2atmpS1522);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS87,
  int32_t _M0L6offsetS91,
  int64_t _M0L6lengthS89
) {
  int32_t _M0L3lenS86;
  int32_t _M0L6lengthS88;
  int32_t _if__result_3758;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS86 = Moonbit_array_length(_M0L4selfS87);
  if (_M0L6lengthS89 == 4294967296ll) {
    _M0L6lengthS88 = _M0L3lenS86 - _M0L6offsetS91;
  } else {
    int64_t _M0L7_2aSomeS90 = _M0L6lengthS89;
    _M0L6lengthS88 = (int32_t)_M0L7_2aSomeS90;
  }
  if (_M0L6offsetS91 >= 0) {
    if (_M0L6lengthS88 >= 0) {
      int32_t _M0L6_2atmpS1520 = _M0L6offsetS91 + _M0L6lengthS88;
      _if__result_3758 = _M0L6_2atmpS1520 <= _M0L3lenS86;
    } else {
      _if__result_3758 = 0;
    }
  } else {
    _if__result_3758 = 0;
  }
  if (_if__result_3758) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS87, _M0L6offsetS91, _M0L6lengthS88);
  } else {
    moonbit_decref(_M0L4selfS87);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS84
) {
  int32_t _M0L7initialS83;
  moonbit_bytes_t _M0L4dataS85;
  struct _M0TPB13StringBuilder* _block_3759;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS84 < 1) {
    _M0L7initialS83 = 1;
  } else {
    _M0L7initialS83 = _M0L10size__hintS84;
  }
  _M0L4dataS85 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS83, 0);
  _block_3759
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3759)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3759->$0 = _M0L4dataS85;
  _block_3759->$1 = 0;
  return _block_3759;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS82) {
  int32_t _M0L6_2atmpS1519;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1519 = (int32_t)_M0L4selfS82;
  return _M0L6_2atmpS1519;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS67,
  int32_t _M0L11dst__offsetS68,
  moonbit_string_t* _M0L3srcS69,
  int32_t _M0L11src__offsetS70,
  int32_t _M0L3lenS71
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS67, _M0L11dst__offsetS68, _M0L3srcS69, _M0L11src__offsetS70, _M0L3lenS71);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS72,
  int32_t _M0L11dst__offsetS73,
  struct _M0TUsiE** _M0L3srcS74,
  int32_t _M0L11src__offsetS75,
  int32_t _M0L3lenS76
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS72, _M0L11dst__offsetS73, _M0L3srcS74, _M0L11src__offsetS75, _M0L3lenS76);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS77,
  int32_t _M0L11dst__offsetS78,
  void** _M0L3srcS79,
  int32_t _M0L11src__offsetS80,
  int32_t _M0L3lenS81
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS77, _M0L11dst__offsetS78, _M0L3srcS79, _M0L11src__offsetS80, _M0L3lenS81);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS31,
  int32_t _M0L11dst__offsetS33,
  moonbit_bytes_t _M0L3srcS32,
  int32_t _M0L11src__offsetS34,
  int32_t _M0L3lenS36
) {
  int32_t _if__result_3760;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS31 == _M0L3srcS32) {
    _if__result_3760 = _M0L11dst__offsetS33 < _M0L11src__offsetS34;
  } else {
    _if__result_3760 = 0;
  }
  if (_if__result_3760) {
    int32_t _M0L1iS35 = 0;
    while (1) {
      if (_M0L1iS35 < _M0L3lenS36) {
        int32_t _M0L6_2atmpS1483 = _M0L11dst__offsetS33 + _M0L1iS35;
        int32_t _M0L6_2atmpS1485 = _M0L11src__offsetS34 + _M0L1iS35;
        int32_t _M0L6_2atmpS1484;
        int32_t _M0L6_2atmpS1486;
        if (
          _M0L6_2atmpS1485 < 0
          || _M0L6_2atmpS1485 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1484 = (int32_t)_M0L3srcS32[_M0L6_2atmpS1485];
        if (
          _M0L6_2atmpS1483 < 0
          || _M0L6_2atmpS1483 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS31[_M0L6_2atmpS1483] = _M0L6_2atmpS1484;
        _M0L6_2atmpS1486 = _M0L1iS35 + 1;
        _M0L1iS35 = _M0L6_2atmpS1486;
        continue;
      } else {
        moonbit_decref(_M0L3srcS32);
        moonbit_decref(_M0L3dstS31);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1491 = _M0L3lenS36 - 1;
    int32_t _M0L1iS38 = _M0L6_2atmpS1491;
    while (1) {
      if (_M0L1iS38 >= 0) {
        int32_t _M0L6_2atmpS1487 = _M0L11dst__offsetS33 + _M0L1iS38;
        int32_t _M0L6_2atmpS1489 = _M0L11src__offsetS34 + _M0L1iS38;
        int32_t _M0L6_2atmpS1488;
        int32_t _M0L6_2atmpS1490;
        if (
          _M0L6_2atmpS1489 < 0
          || _M0L6_2atmpS1489 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1488 = (int32_t)_M0L3srcS32[_M0L6_2atmpS1489];
        if (
          _M0L6_2atmpS1487 < 0
          || _M0L6_2atmpS1487 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS31[_M0L6_2atmpS1487] = _M0L6_2atmpS1488;
        _M0L6_2atmpS1490 = _M0L1iS38 - 1;
        _M0L1iS38 = _M0L6_2atmpS1490;
        continue;
      } else {
        moonbit_decref(_M0L3srcS32);
        moonbit_decref(_M0L3dstS31);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS40,
  int32_t _M0L11dst__offsetS42,
  moonbit_string_t* _M0L3srcS41,
  int32_t _M0L11src__offsetS43,
  int32_t _M0L3lenS45
) {
  int32_t _if__result_3763;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS40 == _M0L3srcS41) {
    _if__result_3763 = _M0L11dst__offsetS42 < _M0L11src__offsetS43;
  } else {
    _if__result_3763 = 0;
  }
  if (_if__result_3763) {
    int32_t _M0L1iS44 = 0;
    while (1) {
      if (_M0L1iS44 < _M0L3lenS45) {
        int32_t _M0L6_2atmpS1492 = _M0L11dst__offsetS42 + _M0L1iS44;
        int32_t _M0L6_2atmpS1494 = _M0L11src__offsetS43 + _M0L1iS44;
        moonbit_string_t _M0L6_2atmpS3413;
        moonbit_string_t _M0L6_2atmpS1493;
        moonbit_string_t _M0L6_2aoldS3412;
        int32_t _M0L6_2atmpS1495;
        if (
          _M0L6_2atmpS1494 < 0
          || _M0L6_2atmpS1494 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3413 = (moonbit_string_t)_M0L3srcS41[_M0L6_2atmpS1494];
        _M0L6_2atmpS1493 = _M0L6_2atmpS3413;
        if (
          _M0L6_2atmpS1492 < 0
          || _M0L6_2atmpS1492 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3412 = (moonbit_string_t)_M0L3dstS40[_M0L6_2atmpS1492];
        moonbit_incref(_M0L6_2atmpS1493);
        moonbit_decref(_M0L6_2aoldS3412);
        _M0L3dstS40[_M0L6_2atmpS1492] = _M0L6_2atmpS1493;
        _M0L6_2atmpS1495 = _M0L1iS44 + 1;
        _M0L1iS44 = _M0L6_2atmpS1495;
        continue;
      } else {
        moonbit_decref(_M0L3srcS41);
        moonbit_decref(_M0L3dstS40);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1500 = _M0L3lenS45 - 1;
    int32_t _M0L1iS47 = _M0L6_2atmpS1500;
    while (1) {
      if (_M0L1iS47 >= 0) {
        int32_t _M0L6_2atmpS1496 = _M0L11dst__offsetS42 + _M0L1iS47;
        int32_t _M0L6_2atmpS1498 = _M0L11src__offsetS43 + _M0L1iS47;
        moonbit_string_t _M0L6_2atmpS3415;
        moonbit_string_t _M0L6_2atmpS1497;
        moonbit_string_t _M0L6_2aoldS3414;
        int32_t _M0L6_2atmpS1499;
        if (
          _M0L6_2atmpS1498 < 0
          || _M0L6_2atmpS1498 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3415 = (moonbit_string_t)_M0L3srcS41[_M0L6_2atmpS1498];
        _M0L6_2atmpS1497 = _M0L6_2atmpS3415;
        if (
          _M0L6_2atmpS1496 < 0
          || _M0L6_2atmpS1496 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3414 = (moonbit_string_t)_M0L3dstS40[_M0L6_2atmpS1496];
        moonbit_incref(_M0L6_2atmpS1497);
        moonbit_decref(_M0L6_2aoldS3414);
        _M0L3dstS40[_M0L6_2atmpS1496] = _M0L6_2atmpS1497;
        _M0L6_2atmpS1499 = _M0L1iS47 - 1;
        _M0L1iS47 = _M0L6_2atmpS1499;
        continue;
      } else {
        moonbit_decref(_M0L3srcS41);
        moonbit_decref(_M0L3dstS40);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS49,
  int32_t _M0L11dst__offsetS51,
  struct _M0TUsiE** _M0L3srcS50,
  int32_t _M0L11src__offsetS52,
  int32_t _M0L3lenS54
) {
  int32_t _if__result_3766;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS49 == _M0L3srcS50) {
    _if__result_3766 = _M0L11dst__offsetS51 < _M0L11src__offsetS52;
  } else {
    _if__result_3766 = 0;
  }
  if (_if__result_3766) {
    int32_t _M0L1iS53 = 0;
    while (1) {
      if (_M0L1iS53 < _M0L3lenS54) {
        int32_t _M0L6_2atmpS1501 = _M0L11dst__offsetS51 + _M0L1iS53;
        int32_t _M0L6_2atmpS1503 = _M0L11src__offsetS52 + _M0L1iS53;
        struct _M0TUsiE* _M0L6_2atmpS3417;
        struct _M0TUsiE* _M0L6_2atmpS1502;
        struct _M0TUsiE* _M0L6_2aoldS3416;
        int32_t _M0L6_2atmpS1504;
        if (
          _M0L6_2atmpS1503 < 0
          || _M0L6_2atmpS1503 >= Moonbit_array_length(_M0L3srcS50)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3417 = (struct _M0TUsiE*)_M0L3srcS50[_M0L6_2atmpS1503];
        _M0L6_2atmpS1502 = _M0L6_2atmpS3417;
        if (
          _M0L6_2atmpS1501 < 0
          || _M0L6_2atmpS1501 >= Moonbit_array_length(_M0L3dstS49)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3416 = (struct _M0TUsiE*)_M0L3dstS49[_M0L6_2atmpS1501];
        if (_M0L6_2atmpS1502) {
          moonbit_incref(_M0L6_2atmpS1502);
        }
        if (_M0L6_2aoldS3416) {
          moonbit_decref(_M0L6_2aoldS3416);
        }
        _M0L3dstS49[_M0L6_2atmpS1501] = _M0L6_2atmpS1502;
        _M0L6_2atmpS1504 = _M0L1iS53 + 1;
        _M0L1iS53 = _M0L6_2atmpS1504;
        continue;
      } else {
        moonbit_decref(_M0L3srcS50);
        moonbit_decref(_M0L3dstS49);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1509 = _M0L3lenS54 - 1;
    int32_t _M0L1iS56 = _M0L6_2atmpS1509;
    while (1) {
      if (_M0L1iS56 >= 0) {
        int32_t _M0L6_2atmpS1505 = _M0L11dst__offsetS51 + _M0L1iS56;
        int32_t _M0L6_2atmpS1507 = _M0L11src__offsetS52 + _M0L1iS56;
        struct _M0TUsiE* _M0L6_2atmpS3419;
        struct _M0TUsiE* _M0L6_2atmpS1506;
        struct _M0TUsiE* _M0L6_2aoldS3418;
        int32_t _M0L6_2atmpS1508;
        if (
          _M0L6_2atmpS1507 < 0
          || _M0L6_2atmpS1507 >= Moonbit_array_length(_M0L3srcS50)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3419 = (struct _M0TUsiE*)_M0L3srcS50[_M0L6_2atmpS1507];
        _M0L6_2atmpS1506 = _M0L6_2atmpS3419;
        if (
          _M0L6_2atmpS1505 < 0
          || _M0L6_2atmpS1505 >= Moonbit_array_length(_M0L3dstS49)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3418 = (struct _M0TUsiE*)_M0L3dstS49[_M0L6_2atmpS1505];
        if (_M0L6_2atmpS1506) {
          moonbit_incref(_M0L6_2atmpS1506);
        }
        if (_M0L6_2aoldS3418) {
          moonbit_decref(_M0L6_2aoldS3418);
        }
        _M0L3dstS49[_M0L6_2atmpS1505] = _M0L6_2atmpS1506;
        _M0L6_2atmpS1508 = _M0L1iS56 - 1;
        _M0L1iS56 = _M0L6_2atmpS1508;
        continue;
      } else {
        moonbit_decref(_M0L3srcS50);
        moonbit_decref(_M0L3dstS49);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS58,
  int32_t _M0L11dst__offsetS60,
  void** _M0L3srcS59,
  int32_t _M0L11src__offsetS61,
  int32_t _M0L3lenS63
) {
  int32_t _if__result_3769;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS58 == _M0L3srcS59) {
    _if__result_3769 = _M0L11dst__offsetS60 < _M0L11src__offsetS61;
  } else {
    _if__result_3769 = 0;
  }
  if (_if__result_3769) {
    int32_t _M0L1iS62 = 0;
    while (1) {
      if (_M0L1iS62 < _M0L3lenS63) {
        int32_t _M0L6_2atmpS1510 = _M0L11dst__offsetS60 + _M0L1iS62;
        int32_t _M0L6_2atmpS1512 = _M0L11src__offsetS61 + _M0L1iS62;
        void* _M0L6_2atmpS3421;
        void* _M0L6_2atmpS1511;
        void* _M0L6_2aoldS3420;
        int32_t _M0L6_2atmpS1513;
        if (
          _M0L6_2atmpS1512 < 0
          || _M0L6_2atmpS1512 >= Moonbit_array_length(_M0L3srcS59)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3421 = (void*)_M0L3srcS59[_M0L6_2atmpS1512];
        _M0L6_2atmpS1511 = _M0L6_2atmpS3421;
        if (
          _M0L6_2atmpS1510 < 0
          || _M0L6_2atmpS1510 >= Moonbit_array_length(_M0L3dstS58)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3420 = (void*)_M0L3dstS58[_M0L6_2atmpS1510];
        moonbit_incref(_M0L6_2atmpS1511);
        moonbit_decref(_M0L6_2aoldS3420);
        _M0L3dstS58[_M0L6_2atmpS1510] = _M0L6_2atmpS1511;
        _M0L6_2atmpS1513 = _M0L1iS62 + 1;
        _M0L1iS62 = _M0L6_2atmpS1513;
        continue;
      } else {
        moonbit_decref(_M0L3srcS59);
        moonbit_decref(_M0L3dstS58);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1518 = _M0L3lenS63 - 1;
    int32_t _M0L1iS65 = _M0L6_2atmpS1518;
    while (1) {
      if (_M0L1iS65 >= 0) {
        int32_t _M0L6_2atmpS1514 = _M0L11dst__offsetS60 + _M0L1iS65;
        int32_t _M0L6_2atmpS1516 = _M0L11src__offsetS61 + _M0L1iS65;
        void* _M0L6_2atmpS3423;
        void* _M0L6_2atmpS1515;
        void* _M0L6_2aoldS3422;
        int32_t _M0L6_2atmpS1517;
        if (
          _M0L6_2atmpS1516 < 0
          || _M0L6_2atmpS1516 >= Moonbit_array_length(_M0L3srcS59)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3423 = (void*)_M0L3srcS59[_M0L6_2atmpS1516];
        _M0L6_2atmpS1515 = _M0L6_2atmpS3423;
        if (
          _M0L6_2atmpS1514 < 0
          || _M0L6_2atmpS1514 >= Moonbit_array_length(_M0L3dstS58)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3422 = (void*)_M0L3dstS58[_M0L6_2atmpS1514];
        moonbit_incref(_M0L6_2atmpS1515);
        moonbit_decref(_M0L6_2aoldS3422);
        _M0L3dstS58[_M0L6_2atmpS1514] = _M0L6_2atmpS1515;
        _M0L6_2atmpS1517 = _M0L1iS65 - 1;
        _M0L1iS65 = _M0L6_2atmpS1517;
        continue;
      } else {
        moonbit_decref(_M0L3srcS59);
        moonbit_decref(_M0L3dstS58);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS1467;
  moonbit_string_t _M0L6_2atmpS3426;
  moonbit_string_t _M0L6_2atmpS1465;
  moonbit_string_t _M0L6_2atmpS1466;
  moonbit_string_t _M0L6_2atmpS3425;
  moonbit_string_t _M0L6_2atmpS1464;
  moonbit_string_t _M0L6_2atmpS3424;
  moonbit_string_t _M0L6_2atmpS1463;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1467 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3426
  = moonbit_add_string(_M0L6_2atmpS1467, (moonbit_string_t)moonbit_string_literal_86.data);
  moonbit_decref(_M0L6_2atmpS1467);
  _M0L6_2atmpS1465 = _M0L6_2atmpS3426;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1466
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3425 = moonbit_add_string(_M0L6_2atmpS1465, _M0L6_2atmpS1466);
  moonbit_decref(_M0L6_2atmpS1465);
  moonbit_decref(_M0L6_2atmpS1466);
  _M0L6_2atmpS1464 = _M0L6_2atmpS3425;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3424
  = moonbit_add_string(_M0L6_2atmpS1464, (moonbit_string_t)moonbit_string_literal_35.data);
  moonbit_decref(_M0L6_2atmpS1464);
  _M0L6_2atmpS1463 = _M0L6_2atmpS3424;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1463);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS25,
  moonbit_string_t _M0L3locS26
) {
  moonbit_string_t _M0L6_2atmpS1472;
  moonbit_string_t _M0L6_2atmpS3429;
  moonbit_string_t _M0L6_2atmpS1470;
  moonbit_string_t _M0L6_2atmpS1471;
  moonbit_string_t _M0L6_2atmpS3428;
  moonbit_string_t _M0L6_2atmpS1469;
  moonbit_string_t _M0L6_2atmpS3427;
  moonbit_string_t _M0L6_2atmpS1468;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1472 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3429
  = moonbit_add_string(_M0L6_2atmpS1472, (moonbit_string_t)moonbit_string_literal_86.data);
  moonbit_decref(_M0L6_2atmpS1472);
  _M0L6_2atmpS1470 = _M0L6_2atmpS3429;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1471
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3428 = moonbit_add_string(_M0L6_2atmpS1470, _M0L6_2atmpS1471);
  moonbit_decref(_M0L6_2atmpS1470);
  moonbit_decref(_M0L6_2atmpS1471);
  _M0L6_2atmpS1469 = _M0L6_2atmpS3428;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3427
  = moonbit_add_string(_M0L6_2atmpS1469, (moonbit_string_t)moonbit_string_literal_35.data);
  moonbit_decref(_M0L6_2atmpS1469);
  _M0L6_2atmpS1468 = _M0L6_2atmpS3427;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1468);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS27,
  moonbit_string_t _M0L3locS28
) {
  moonbit_string_t _M0L6_2atmpS1477;
  moonbit_string_t _M0L6_2atmpS3432;
  moonbit_string_t _M0L6_2atmpS1475;
  moonbit_string_t _M0L6_2atmpS1476;
  moonbit_string_t _M0L6_2atmpS3431;
  moonbit_string_t _M0L6_2atmpS1474;
  moonbit_string_t _M0L6_2atmpS3430;
  moonbit_string_t _M0L6_2atmpS1473;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1477 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS27);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3432
  = moonbit_add_string(_M0L6_2atmpS1477, (moonbit_string_t)moonbit_string_literal_86.data);
  moonbit_decref(_M0L6_2atmpS1477);
  _M0L6_2atmpS1475 = _M0L6_2atmpS3432;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1476
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS28);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3431 = moonbit_add_string(_M0L6_2atmpS1475, _M0L6_2atmpS1476);
  moonbit_decref(_M0L6_2atmpS1475);
  moonbit_decref(_M0L6_2atmpS1476);
  _M0L6_2atmpS1474 = _M0L6_2atmpS3431;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3430
  = moonbit_add_string(_M0L6_2atmpS1474, (moonbit_string_t)moonbit_string_literal_35.data);
  moonbit_decref(_M0L6_2atmpS1474);
  _M0L6_2atmpS1473 = _M0L6_2atmpS3430;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1473);
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS29,
  moonbit_string_t _M0L3locS30
) {
  moonbit_string_t _M0L6_2atmpS1482;
  moonbit_string_t _M0L6_2atmpS3435;
  moonbit_string_t _M0L6_2atmpS1480;
  moonbit_string_t _M0L6_2atmpS1481;
  moonbit_string_t _M0L6_2atmpS3434;
  moonbit_string_t _M0L6_2atmpS1479;
  moonbit_string_t _M0L6_2atmpS3433;
  moonbit_string_t _M0L6_2atmpS1478;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1482 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS29);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3435
  = moonbit_add_string(_M0L6_2atmpS1482, (moonbit_string_t)moonbit_string_literal_86.data);
  moonbit_decref(_M0L6_2atmpS1482);
  _M0L6_2atmpS1480 = _M0L6_2atmpS3435;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1481
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS30);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3434 = moonbit_add_string(_M0L6_2atmpS1480, _M0L6_2atmpS1481);
  moonbit_decref(_M0L6_2atmpS1480);
  moonbit_decref(_M0L6_2atmpS1481);
  _M0L6_2atmpS1479 = _M0L6_2atmpS3434;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3433
  = moonbit_add_string(_M0L6_2atmpS1479, (moonbit_string_t)moonbit_string_literal_35.data);
  moonbit_decref(_M0L6_2atmpS1479);
  _M0L6_2atmpS1478 = _M0L6_2atmpS3433;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1478);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS21,
  uint32_t _M0L5valueS22
) {
  uint32_t _M0L3accS1462;
  uint32_t _M0L6_2atmpS1461;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1462 = _M0L4selfS21->$0;
  _M0L6_2atmpS1461 = _M0L3accS1462 + 4u;
  _M0L4selfS21->$0 = _M0L6_2atmpS1461;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS21, _M0L5valueS22);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS19,
  uint32_t _M0L5inputS20
) {
  uint32_t _M0L3accS1459;
  uint32_t _M0L6_2atmpS1460;
  uint32_t _M0L6_2atmpS1458;
  uint32_t _M0L6_2atmpS1457;
  uint32_t _M0L6_2atmpS1456;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1459 = _M0L4selfS19->$0;
  _M0L6_2atmpS1460 = _M0L5inputS20 * 3266489917u;
  _M0L6_2atmpS1458 = _M0L3accS1459 + _M0L6_2atmpS1460;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1457 = _M0FPB4rotl(_M0L6_2atmpS1458, 17);
  _M0L6_2atmpS1456 = _M0L6_2atmpS1457 * 668265263u;
  _M0L4selfS19->$0 = _M0L6_2atmpS1456;
  moonbit_decref(_M0L4selfS19);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS17, int32_t _M0L1rS18) {
  uint32_t _M0L6_2atmpS1453;
  int32_t _M0L6_2atmpS1455;
  uint32_t _M0L6_2atmpS1454;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1453 = _M0L1xS17 << (_M0L1rS18 & 31);
  _M0L6_2atmpS1455 = 32 - _M0L1rS18;
  _M0L6_2atmpS1454 = _M0L1xS17 >> (_M0L6_2atmpS1455 & 31);
  return _M0L6_2atmpS1453 | _M0L6_2atmpS1454;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S13,
  struct _M0TPB6Logger _M0L10_2ax__4934S16
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS14;
  moonbit_string_t _M0L8_2afieldS3436;
  int32_t _M0L6_2acntS3558;
  moonbit_string_t _M0L15_2a_2aarg__4935S15;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS14
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S13;
  _M0L8_2afieldS3436 = _M0L10_2aFailureS14->$0;
  _M0L6_2acntS3558 = Moonbit_object_header(_M0L10_2aFailureS14)->rc;
  if (_M0L6_2acntS3558 > 1) {
    int32_t _M0L11_2anew__cntS3559 = _M0L6_2acntS3558 - 1;
    Moonbit_object_header(_M0L10_2aFailureS14)->rc = _M0L11_2anew__cntS3559;
    moonbit_incref(_M0L8_2afieldS3436);
  } else if (_M0L6_2acntS3558 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS14);
  }
  _M0L15_2a_2aarg__4935S15 = _M0L8_2afieldS3436;
  if (_M0L10_2ax__4934S16.$1) {
    moonbit_incref(_M0L10_2ax__4934S16.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S16.$0->$method_0(_M0L10_2ax__4934S16.$1, (moonbit_string_t)moonbit_string_literal_87.data);
  if (_M0L10_2ax__4934S16.$1) {
    moonbit_incref(_M0L10_2ax__4934S16.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S16, _M0L15_2a_2aarg__4935S15);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S16.$0->$method_0(_M0L10_2ax__4934S16.$1, (moonbit_string_t)moonbit_string_literal_57.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS12) {
  void* _block_3772;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3772 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3772)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3772)->$0 = _M0L4selfS12;
  return _block_3772;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS11) {
  void* _block_3773;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3773 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3773)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3773)->$0 = _M0L5arrayS11;
  return _block_3773;
}

int32_t _M0MPB6Logger13write__objectGRPC14json8JsonPathE(
  struct _M0TPB6Logger _M0L4selfS6,
  void* _M0L3objS5
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14json8JsonPathPB4Show6output(_M0L3objS5, _M0L4selfS6);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS8,
  moonbit_string_t _M0L3objS7
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS7, _M0L4selfS8);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGiE(
  struct _M0TPB6Logger _M0L4selfS10,
  int32_t _M0L3objS9
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L3objS9, _M0L4selfS10);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1415) {
  switch (Moonbit_object_tag(_M0L4_2aeS1415)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1415);
      return (moonbit_string_t)moonbit_string_literal_88.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1415);
      return (moonbit_string_t)moonbit_string_literal_89.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1415);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1415);
      return (moonbit_string_t)moonbit_string_literal_90.data;
      break;
    }
    
    case 1: {
      return _M0IP016_24default__implPB4Show10to__stringGRPC14json15JsonDecodeErrorE(_M0L4_2aeS1415);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1415);
      return (moonbit_string_t)moonbit_string_literal_91.data;
      break;
    }
  }
}

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPB4JsonE(
  void* _M0L11_2aobj__ptrS1434
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L7_2aselfS1433 =
    (struct _M0TPB5ArrayGRPB4JsonE*)_M0L11_2aobj__ptrS1434;
  return _M0IPC15array5ArrayPB6ToJson8to__jsonGRPB4JsonE(_M0L7_2aselfS1433);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1432,
  int32_t _M0L8_2aparamS1431
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1430 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1432;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1430, _M0L8_2aparamS1431);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1429,
  struct _M0TPC16string10StringView _M0L8_2aparamS1428
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1427 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1429;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1427, _M0L8_2aparamS1428);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1426,
  moonbit_string_t _M0L8_2aparamS1423,
  int32_t _M0L8_2aparamS1424,
  int32_t _M0L8_2aparamS1425
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1422 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1426;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1422, _M0L8_2aparamS1423, _M0L8_2aparamS1424, _M0L8_2aparamS1425);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1421,
  moonbit_string_t _M0L8_2aparamS1420
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1419 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1421;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1419, _M0L8_2aparamS1420);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1452 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1451;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1450;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1341;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1449;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1448;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1447;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1442;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1342;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1446;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1445;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1444;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1443;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1340;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1441;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1440;
  _M0L6_2atmpS1452[0] = (moonbit_string_t)moonbit_string_literal_92.data;
  moonbit_incref(_M0FP48clawteam8clawteam5tools17replace__in__file39____test__736368656d612e6d6274__0_2eclo);
  _M0L8_2atupleS1451
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1451)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1451->$0
  = _M0FP48clawteam8clawteam5tools17replace__in__file39____test__736368656d612e6d6274__0_2eclo;
  _M0L8_2atupleS1451->$1 = _M0L6_2atmpS1452;
  _M0L8_2atupleS1450
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1450)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1450->$0 = 0;
  _M0L8_2atupleS1450->$1 = _M0L8_2atupleS1451;
  _M0L7_2abindS1341
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1341[0] = _M0L8_2atupleS1450;
  _M0L6_2atmpS1449 = _M0L7_2abindS1341;
  _M0L6_2atmpS1448
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1449
  };
  #line 398 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1447
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1448);
  _M0L8_2atupleS1442
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1442)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1442->$0 = (moonbit_string_t)moonbit_string_literal_93.data;
  _M0L8_2atupleS1442->$1 = _M0L6_2atmpS1447;
  _M0L7_2abindS1342
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1446 = _M0L7_2abindS1342;
  _M0L6_2atmpS1445
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1446
  };
  #line 401 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1444
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1445);
  _M0L8_2atupleS1443
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1443)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1443->$0 = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L8_2atupleS1443->$1 = _M0L6_2atmpS1444;
  _M0L7_2abindS1340
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1340[0] = _M0L8_2atupleS1442;
  _M0L7_2abindS1340[1] = _M0L8_2atupleS1443;
  _M0L6_2atmpS1441 = _M0L7_2abindS1340;
  _M0L6_2atmpS1440
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1441
  };
  #line 397 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools17replace__in__file48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1440);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1439;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1409;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1410;
  int32_t _M0L7_2abindS1411;
  int32_t _M0L2__S1412;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1439
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1409
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1409)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1409->$0 = _M0L6_2atmpS1439;
  _M0L12async__testsS1409->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1410
  = _M0FP48clawteam8clawteam5tools17replace__in__file52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1411 = _M0L7_2abindS1410->$1;
  _M0L2__S1412 = 0;
  while (1) {
    if (_M0L2__S1412 < _M0L7_2abindS1411) {
      struct _M0TUsiE** _M0L8_2afieldS3440 = _M0L7_2abindS1410->$0;
      struct _M0TUsiE** _M0L3bufS1438 = _M0L8_2afieldS3440;
      struct _M0TUsiE* _M0L6_2atmpS3439 =
        (struct _M0TUsiE*)_M0L3bufS1438[_M0L2__S1412];
      struct _M0TUsiE* _M0L3argS1413 = _M0L6_2atmpS3439;
      moonbit_string_t _M0L8_2afieldS3438 = _M0L3argS1413->$0;
      moonbit_string_t _M0L6_2atmpS1435 = _M0L8_2afieldS3438;
      int32_t _M0L8_2afieldS3437 = _M0L3argS1413->$1;
      int32_t _M0L6_2atmpS1436 = _M0L8_2afieldS3437;
      int32_t _M0L6_2atmpS1437;
      moonbit_incref(_M0L6_2atmpS1435);
      moonbit_incref(_M0L12async__testsS1409);
      #line 441 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam5tools17replace__in__file44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1409, _M0L6_2atmpS1435, _M0L6_2atmpS1436);
      _M0L6_2atmpS1437 = _M0L2__S1412 + 1;
      _M0L2__S1412 = _M0L6_2atmpS1437;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1410);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\tools\\replace_in_file\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam5tools17replace__in__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools17replace__in__file34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1409);
  return 0;
}