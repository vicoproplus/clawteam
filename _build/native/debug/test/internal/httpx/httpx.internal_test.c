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
struct _M0TP411moonbitlang5async8internal10io__buffer6Buffer;

struct _M0TPB5EntryGRPC16string10StringViewRPC16string10StringViewE;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0TWOsEu;

struct _M0TWssbEu;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOc;

struct _M0TWERPC16option6OptionGRPC16string10StringViewE;

struct _M0TP311moonbitlang5async4http6Reader;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5EntryGssE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0KTPB6ToJsonTPB5ArrayGRPC16string10StringViewE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TP48clawteam8clawteam8internal5httpx13RequestReader;

struct _M0DTPC16result6ResultGOiRPC15error5ErrorE3Err;

struct _M0TWRPC16string10StringViewERPB4Json;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGOOzRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOzRPC15error5ErrorE3Err;

struct _M0BTP311moonbitlang5async2io4Data;

struct _M0TWiEu;

struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE;

struct _M0TPB19MulShiftAll64Result;

struct _M0TP311moonbitlang5async2io6Writer;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTP311moonbitlang5async4http8BodyKind7Chunked;

struct _M0TPB5ArrayGRPC16string10StringViewE;

struct _M0TP311moonbitlang5async2io6Reader;

struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TP311moonbitlang5async4http6Sender;

struct _M0DTPB4Json6Number;

struct _M0DTPC16option6OptionGRP311moonbitlang5async2io4DataE4Some;

struct _M0TWEORPB4Json;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0TWEOs;

struct _M0TP311moonbitlang5async6socket3Tcp;

struct _M0BTP311moonbitlang5async2io6Writer;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal5httpx33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE;

struct _M0DTPC16result6ResultGOOzRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGOOsRPC15error5ErrorE3Err;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16option6OptionGOzE4Some;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0TPC15bytes9BytesView;

struct _M0BTP311moonbitlang5async2io6Reader;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE;

struct _M0TWOzEu;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TP311moonbitlang5async4http16ServerConnection;

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE;

struct _M0TP411moonbitlang5async8internal11event__loop8IoHandle;

struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__;

struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE;

struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__;

struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16option6OptionGOsE4Some;

struct _M0DTP311moonbitlang5async4http8BodyKind5Fixed;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWzEu;

struct _M0TPB3MapGRPC16string10StringViewRPC16string10StringViewE;

struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TWRP311moonbitlang5async2io4DataEu;

struct _M0KTPB6ToJsonTPB4IterGRP48clawteam8clawteam8internal5httpx6MethodE;

struct _M0R38String_3a_3aiter_2eanon__u2340__l247__;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal5httpx33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0Y4Bool;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0DTPC16result6ResultGORP311moonbitlang5async2io4DataRPC15error5ErrorE3Err;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE;

struct _M0DTPC16result6ResultGORP311moonbitlang5async2io4DataRPC15error5ErrorE2Ok;

struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__;

struct _M0DTPC16result6ResultGOzRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0TPB3MapGssE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TP311moonbitlang5async2io4Data;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TWEOi;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TPB9ArrayViewGRPC16string10StringViewE;

struct _M0DTPB4Json6Object;

struct _M0KTPB6ToJsonS4Bool;

struct _M0KTPB6ToJsonTPB4IterGRPC16string10StringViewE;

struct _M0TPC16string10StringView;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TWiERPB4Json;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16result6ResultGOOsRPC15error5ErrorE2Ok;

struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TP48clawteam8clawteam8internal5httpx5Route;

struct _M0TP48clawteam8clawteam8internal5httpx14ResponseWriter;

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE;

struct _M0DTPC16result6ResultGOiRPC15error5ErrorE2Ok;

struct _M0TP48clawteam8clawteam8internal5httpx5Layer;

struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0TP411moonbitlang5async8internal10io__buffer6Buffer {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0TPB5EntryGRPC16string10StringViewRPC16string10StringViewE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4_1;
  int32_t $4_2;
  int32_t $5_1;
  int32_t $5_2;
  struct _M0TPB5EntryGRPC16string10StringViewRPC16string10StringViewE* $1;
  moonbit_string_t $4_0;
  moonbit_string_t $5_0;
  
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

struct _M0TWOsEu {
  int32_t(* code)(struct _M0TWOsEu*, moonbit_string_t);
  
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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  
};

struct _M0TP311moonbitlang5async4http6Reader {
  void* $0;
  struct _M0BTP311moonbitlang5async2io6Reader* $1_0;
  void* $1_1;
  struct _M0TP411moonbitlang5async8internal10io__buffer6Buffer* $2;
  
};

struct _M0TPB5ArrayGRPC14json10WriteFrameE {
  int32_t $1;
  void** $0;
  
};

struct _M0TPB17FloatingDecimal64 {
  uint64_t $0;
  int32_t $1;
  
};

struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** $0;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* $5;
  
};

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  void* $1;
  
};

struct _M0TPB5EntryGssE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGssE* $1;
  moonbit_string_t $4;
  moonbit_string_t $5;
  
};

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0KTPB6ToJsonTPB5ArrayGRPC16string10StringViewE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0TP48clawteam8clawteam8internal5httpx13RequestReader {
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0BTP311moonbitlang5async2io6Reader* $0_0;
  void* $0_1;
  moonbit_string_t $2_0;
  struct _M0TPB3MapGRPC16string10StringViewRPC16string10StringViewE* $3;
  
};

struct _M0DTPC16result6ResultGOiRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TWRPC16string10StringViewERPB4Json {
  void*(* code)(
    struct _M0TWRPC16string10StringViewERPB4Json*,
    struct _M0TPC16string10StringView
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

struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGOOzRPC15error5ErrorE2Ok {
  void* $0;
  
};

struct _M0DTPC16result6ResultGOzRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0BTP311moonbitlang5async2io4Data {
  struct _M0TPC15bytes9BytesView(* $method_0)(void*);
  moonbit_bytes_t(* $method_1)(void*);
  
};

struct _M0TWiEu {
  int32_t(* code)(struct _M0TWiEu*, int32_t);
  
};

struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4_1;
  int32_t $4_2;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* $1;
  moonbit_string_t $4_0;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* $5;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TP311moonbitlang5async2io6Writer {
  struct _M0BTP311moonbitlang5async2io6Writer* $0;
  void* $1;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0DTP311moonbitlang5async4http8BodyKind7Chunked {
  int32_t $0;
  
};

struct _M0TPB5ArrayGRPC16string10StringViewE {
  int32_t $1;
  struct _M0TPC16string10StringView* $0;
  
};

struct _M0TP311moonbitlang5async2io6Reader {
  struct _M0BTP311moonbitlang5async2io6Reader* $0;
  void* $1;
  
};

struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE {
  int32_t $1;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** $0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0TP311moonbitlang5async4http6Sender {
  int32_t $1;
  int32_t $3;
  struct _M0BTP311moonbitlang5async2io6Writer* $0_0;
  void* $0_1;
  moonbit_bytes_t $2;
  moonbit_bytes_t $4;
  
};

struct _M0DTPB4Json6Number {
  double $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16option6OptionGRP311moonbitlang5async2io4DataE4Some {
  struct _M0BTP311moonbitlang5async2io4Data* $0_0;
  void* $0_1;
  
};

struct _M0TWEORPB4Json {
  void*(* code)(struct _M0TWEORPB4Json*);
  
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

struct _M0TP311moonbitlang5async6socket3Tcp {
  int32_t $1;
  struct _M0TP411moonbitlang5async8internal11event__loop8IoHandle* $0;
  struct _M0TP411moonbitlang5async8internal10io__buffer6Buffer* $2;
  
};

struct _M0BTP311moonbitlang5async2io6Writer {
  struct moonbit_result_1(* $method_0)(
    void*,
    moonbit_bytes_t,
    int32_t,
    int32_t,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_0(* $method_1)(
    void*,
    struct _M0TP311moonbitlang5async2io4Data,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_0(* $method_2)(
    void*,
    struct _M0TP311moonbitlang5async2io6Reader,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_0(* $method_3)(
    void*,
    struct _M0TPC16string10StringView,
    int32_t,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal5httpx33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE {
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* $0;
  struct _M0TPB5ArrayGRPC16string10StringViewE* $1;
  
};

struct _M0DTPC16result6ResultGOOzRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGOOsRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16option6OptionGOzE4Some {
  moonbit_bytes_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE {
  int32_t $1;
  int32_t $2;
  struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** $0;
  
};

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0BTP311moonbitlang5async2io6Reader {
  struct _M0TP411moonbitlang5async8internal10io__buffer6Buffer*(* $method_0)(
    void*
  );
  struct moonbit_result_1(* $method_1)(
    void*,
    moonbit_bytes_t,
    int32_t,
    int32_t,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_1(* $method_2)(
    void*,
    moonbit_bytes_t,
    int64_t,
    int64_t,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_1(* $method_3)(
    void*,
    int32_t,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_2(* $method_4)(
    void*,
    int32_t,
    struct _M0TWzEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_3(* $method_5)(
    void*,
    int64_t,
    struct _M0TWOzEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_4(* $method_6)(
    void*,
    struct _M0TWRP311moonbitlang5async2io4DataEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  struct moonbit_result_5(* $method_7)(
    void*,
    struct _M0TPC16string10StringView,
    struct _M0TWOsEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE {
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $0;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $1;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $2;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $3;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $4;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $5;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $6;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $7;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* $8;
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE {
  int32_t $1;
  int32_t* $0;
  
};

struct _M0TWOzEu {
  int32_t(* code)(struct _M0TWOzEu*, moonbit_bytes_t);
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TP311moonbitlang5async4http16ServerConnection {
  int32_t $3;
  struct _M0TP311moonbitlang5async4http6Reader* $0;
  struct _M0TP311moonbitlang5async6socket3Tcp* $1;
  struct _M0TP311moonbitlang5async4http6Sender* $2;
  
};

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE {
  struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*(* code)(
    struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
  );
  
};

struct _M0TP411moonbitlang5async8internal11event__loop8IoHandle {
  void* $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int64_t $4;
  int32_t $5;
  int64_t $6;
  
};

struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__ {
  int64_t(* code)(struct _M0TWEOi*);
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* $0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE {
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* $0;
  struct _M0TPB5ArrayGRPC16string10StringViewE* $1;
  
};

struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__ {
  void*(* code)(struct _M0TWEORPB4Json*);
  struct _M0TWiERPB4Json* $0;
  struct _M0TWEOi* $1;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16option6OptionGOsE4Some {
  moonbit_string_t $0;
  
};

struct _M0DTP311moonbitlang5async4http8BodyKind5Fixed {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err {
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

struct _M0TWzEu {
  int32_t(* code)(struct _M0TWzEu*, moonbit_bytes_t);
  
};

struct _M0TPB3MapGRPC16string10StringViewRPC16string10StringViewE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGRPC16string10StringViewRPC16string10StringViewE** $0;
  struct _M0TPB5EntryGRPC16string10StringViewRPC16string10StringViewE* $5;
  
};

struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__ {
  struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*(* code)(
    struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
  );
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0TWRP311moonbitlang5async2io4DataEu {
  int32_t(* code)(
    struct _M0TWRP311moonbitlang5async2io4DataEu*,
    struct _M0TP311moonbitlang5async2io4Data
  );
  
};

struct _M0KTPB6ToJsonTPB4IterGRP48clawteam8clawteam8internal5httpx6MethodE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2340__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal5httpx33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0DTPC16result6ResultGORP311moonbitlang5async2io4DataRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE {
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* $0;
  
};

struct _M0DTPC16result6ResultGORP311moonbitlang5async2io4DataRPC15error5ErrorE2Ok {
  void* $0;
  
};

struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TP48clawteam8clawteam8internal5httpx13RequestReader*,
    struct _M0TP48clawteam8clawteam8internal5httpx14ResponseWriter*,
    struct _M0TWiEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok {
  void* $0;
  
};

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
};

struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGOzRPC15error5ErrorE2Ok {
  moonbit_bytes_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPB4Json6String {
  moonbit_string_t $0;
  
};

struct _M0TPB3MapGssE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGssE** $0;
  struct _M0TPB5EntryGssE* $5;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0TP311moonbitlang5async2io4Data {
  struct _M0BTP311moonbitlang5async2io4Data* $0;
  void* $1;
  
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

struct _M0TWEOi {
  int64_t(* code)(struct _M0TWEOi*);
  
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

struct _M0TPB9ArrayViewGRPC16string10StringViewE {
  int32_t $1;
  int32_t $2;
  struct _M0TPC16string10StringView* $0;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0KTPB6ToJsonS4Bool {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0KTPB6ToJsonTPB4IterGRPC16string10StringViewE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0TPB5EntryGsRPB4JsonE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB4JsonE* $1;
  moonbit_string_t $4;
  void* $5;
  
};

struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* $0;
  
};

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
};

struct _M0TWiERPB4Json {
  void*(* code)(struct _M0TWiERPB4Json*, int32_t);
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGOOsRPC15error5ErrorE2Ok {
  void* $0;
  
};

struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
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

struct _M0TP48clawteam8clawteam8internal5httpx5Route {
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error* $0;
  struct _M0TPB5ArrayGRPC16string10StringViewE* $1;
  
};

struct _M0TP48clawteam8clawteam8internal5httpx14ResponseWriter {
  int32_t $0;
  struct _M0TPB3MapGssE* $1;
  struct _M0TP311moonbitlang5async4http16ServerConnection* $2;
  
};

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* $1;
  
};

struct _M0DTPC16result6ResultGOiRPC15error5ErrorE2Ok {
  int64_t $0;
  
};

struct _M0TP48clawteam8clawteam8internal5httpx5Layer {
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* $0;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* $1;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* $2;
  
};

struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE {
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* $0;
  
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

struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__ {
  void*(* code)(struct _M0TWEORPB4Json*);
  struct _M0TWRPC16string10StringViewERPB4Json* $0;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* $1;
  
};

struct _M0TPB7Umul128 {
  uint64_t $0;
  uint64_t $1;
  
};

struct _M0TPB8Pow5Pair {
  uint64_t $0;
  uint64_t $1;
  
};

struct moonbit_result_4 {
  int tag;
  union { void* ok; void* err;  } data;
  
};

struct moonbit_result_5 {
  int tag;
  union { void* ok; void* err;  } data;
  
};

struct moonbit_result_3 {
  int tag;
  union { void* ok; void* err;  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_1 {
  int tag;
  union { int64_t ok; void* err;  } data;
  
};

struct moonbit_result_2 {
  int tag;
  union { moonbit_bytes_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IPC16string10StringViewPB6ToJson18to__json_2edyncall(
  struct _M0TWRPC16string10StringViewERPB4Json*,
  struct _M0TPC16string10StringView
);

int32_t _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN17error__to__stringS1565(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN14handle__resultS1556(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testC3644l454(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testC3640l455(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal5httpx45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1476(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1471(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1458(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal5httpx28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal5httpx34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1C3499l235(
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
  struct _M0TP48clawteam8clawteam8internal5httpx13RequestReader*,
  struct _M0TP48clawteam8clawteam8internal5httpx14ResponseWriter*,
  struct _M0TWiEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0C3416l210(
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
  struct _M0TP48clawteam8clawteam8internal5httpx13RequestReader*,
  struct _M0TP48clawteam8clawteam8internal5httpx14ResponseWriter*,
  struct _M0TWiEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0MP48clawteam8clawteam8internal5httpx9MethodMap7op__getGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE*,
  int32_t
);

moonbit_string_t _M0IP48clawteam8clawteam8internal5httpx6MethodPB4Show10to__string(
  int32_t
);

struct _M0TWEOi* _M0MP48clawteam8clawteam8internal5httpx9MethodMap4keysGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE*
);

int64_t _M0MP48clawteam8clawteam8internal5httpx9MethodMap4keysGRP48clawteam8clawteam8internal5httpx5RouteEC3251l74(
  struct _M0TWEOi*
);

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0MP48clawteam8clawteam8internal5httpx9MethodMap3getGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE*,
  int32_t
);

struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE* _M0MP48clawteam8clawteam8internal5httpx5Layer5route(
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer*,
  struct _M0TPB9ArrayViewGRPC16string10StringViewE
);

int32_t _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer*,
  int32_t,
  struct _M0TPB9ArrayViewGRPC16string10StringViewE,
  struct _M0TP48clawteam8clawteam8internal5httpx5Route*
);

int32_t _M0MP48clawteam8clawteam8internal5httpx9MethodMap7op__setGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE*,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal5httpx5Route*
);

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MP48clawteam8clawteam8internal5httpx5Layer3new(
  
);

struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0MP48clawteam8clawteam8internal5httpx9MethodMap3newGRP48clawteam8clawteam8internal5httpx5RouteE(
  
);

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0MP48clawteam8clawteam8internal5httpx5Route3new(
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*
);

void* _M0IP48clawteam8clawteam8internal5httpx6MethodPB6ToJson8to__json(
  int32_t
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

int32_t _M0IPC16string10StringViewPB4Hash13hash__combine(
  struct _M0TPC16string10StringView,
  struct _M0TPB6Hasher*
);

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

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB5Iter24nextGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

void* _M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

void* _M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TWEOi*
);

void* _M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodEC2673l78(
  struct _M0TWiERPB4Json*,
  int32_t
);

void* _M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewEC2668l78(
  struct _M0TWRPC16string10StringViewERPB4Json*,
  struct _M0TPC16string10StringView
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRPC16string10StringViewRPB4JsonE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TWRPC16string10StringViewERPB4Json*
);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0IPC14bool4BoolPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json7boolean(int32_t);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

void* _M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEC2655l613(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map5iter2GRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map4iterGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2649l591(
  struct _M0TWEOUsRPB4JsonE*
);

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map4iterGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEC2643l591(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(struct _M0TPB3MapGsRPB4JsonE*);

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
  struct _M0TPC16string10StringView
);

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t
);

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MPB3Map3getGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
  struct _M0TPC16string10StringView
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map11from__arrayGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE
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

int32_t _M0MPB3Map3setGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
  struct _M0TPC16string10StringView,
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer*
);

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map4growGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
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

int32_t _M0MPB3Map15set__with__hashGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
  struct _M0TPC16string10StringView,
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer*,
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

int32_t _M0MPB3Map10push__awayGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
  int32_t,
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
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

int32_t _M0MPB3Map10set__entryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*,
  int32_t,
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map11new_2einnerGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  int32_t
);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MPC16option6Option6unwrapGRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer*
);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPC16option6Option6unwrapGRPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2368l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPB4Iter9to__arrayGRPB4JsonE(
  struct _M0TWEORPB4Json*
);

struct _M0TWEORPB4Json* _M0MPB4Iter3mapGRPC16string10StringViewRPB4JsonE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*,
  struct _M0TWRPC16string10StringViewERPB4Json*
);

struct _M0TWEORPB4Json* _M0MPB4Iter3mapGRP48clawteam8clawteam8internal5httpx6MethodRPB4JsonE(
  struct _M0TWEOi*,
  struct _M0TWiERPB4Json*
);

void* _M0MPB4Iter3mapGRP48clawteam8clawteam8internal5httpx6MethodRPB4JsonEC2360l317(
  struct _M0TWEORPB4Json*
);

void* _M0MPB4Iter3mapGRPC16string10StringViewRPB4JsonEC2356l317(
  struct _M0TWEORPB4Json*
);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2340l247(struct _M0TWEOc*);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array4pushGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC15array5Array4pushGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE*,
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*
);

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*,
  void*
);

int32_t _M0MPC15array5Array4pushGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*,
  void*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array7reallocGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
);

int32_t _M0MPC15array5Array7reallocGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE*
);

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

int32_t _M0MPC15array5Array7reallocGRPB4JsonE(struct _M0TPB5ArrayGRPB4JsonE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(int32_t);

moonbit_string_t _M0MPC16string6String6repeat(moonbit_string_t, int32_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

void* _M0IPC16string10StringViewPB6ToJson8to__json(
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

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

int32_t _M0MPC15array9ArrayView6lengthGsE(struct _M0TPB9ArrayViewGsE);

int32_t _M0MPC15array9ArrayView6lengthGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE
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

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs*);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TWEOi* _M0MPB4Iter3newGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TWEOi*
);

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB4Iter3newGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

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

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB4Iter4nextGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*
);

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE*
);

void* _M0MPB4Iter4nextGRPB4JsonE(struct _M0TWEORPB4Json*);

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

int64_t _M0MPB4Iter4nextGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TWEOi*
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

int32_t _M0IP016_24default__implPB4Hash4hashGRPC16string10StringViewE(
  struct _M0TPC16string10StringView
);

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

int32_t _M0MPB6Hasher7combineGRPC16string10StringViewE(
  struct _M0TPB6Hasher*,
  struct _M0TPC16string10StringView
);

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

int32_t _M0MPC15array5Array2atGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE*
);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

struct _M0TPC16string10StringView* _M0MPC15array5Array6bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
);

int32_t* _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE*
);

struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0MPC15array5Array6bufferGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE*
);

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

void** _M0MPC15array5Array6bufferGRPB4JsonE(struct _M0TPB5ArrayGRPB4JsonE*);

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

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(
  struct _M0TPC16string10StringView*,
  int32_t,
  struct _M0TPC16string10StringView*,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**,
  int32_t,
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**,
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPB4JsonE(
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(
  struct _M0TPC16string10StringView*,
  int32_t,
  struct _M0TPC16string10StringView*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE(
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**,
  int32_t,
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**,
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPB4JsonEE(
  void**,
  int32_t,
  void**,
  int32_t,
  int32_t
);

int32_t _M0FPB5abortGiE(moonbit_string_t, moonbit_string_t);

int32_t _M0FPB5abortGuE(moonbit_string_t, moonbit_string_t);

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0FPB5abortGRP48clawteam8clawteam8internal5httpx5RouteE(
  moonbit_string_t,
  moonbit_string_t
);

int64_t _M0FPB5abortGOiE(moonbit_string_t, moonbit_string_t);

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

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0FPC15abort5abortGRP48clawteam8clawteam8internal5httpx5RouteE(
  moonbit_string_t
);

int64_t _M0FPC15abort5abortGOiE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FP15Error10to__string(void*);

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPC16string10StringViewE(
  void*
);

void* _M0IPB4IterPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal5httpx6MethodE(
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

void* _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

void* _M0IPB4IterPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPC16string10StringViewE(
  void*
);

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 51, 58, 51, 45, 50, 49, 51, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    104, 101, 108, 108, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_139 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    104, 101, 97, 100, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_142 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    101, 118, 101, 110, 116, 95, 115, 116, 114, 101, 97, 109, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 52, 58, 53, 45, 50, 50, 52, 58, 57, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_116 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_17 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    84, 82, 65, 67, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 52, 50, 58, 51, 45, 50, 52, 53, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    117, 115, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_137 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    109, 101, 116, 104, 111, 100, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    67, 79, 78, 78, 69, 67, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_136 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    109, 101, 116, 104, 111, 100, 95, 109, 97, 112, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    72, 101, 97, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_122 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    114, 111, 117, 116, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_83 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 52, 58, 51, 45, 50, 49, 52, 58, 54, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 116, 116, 112, 
    120, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    76, 97, 121, 101, 114, 58, 58, 114, 111, 117, 116, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    71, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_84 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 51, 57, 58, 53, 45, 50, 51, 57, 58, 54, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_140 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    104, 97, 110, 100, 108, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_138 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    106, 115, 111, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    80, 85, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    109, 101, 115, 115, 97, 103, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_96 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[101]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 100), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 116, 
    116, 112, 120, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 52, 55, 58, 51, 45, 50, 52, 55, 58, 54, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    67, 111, 110, 110, 101, 99, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    80, 117, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 109, 101, 116, 104, 111, 100, 95, 109, 97, 
    112, 46, 109, 98, 116, 58, 51, 50, 58, 53, 45, 51, 50, 58, 53, 53, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 52, 51, 58, 53, 45, 50, 52, 51, 58, 54, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    71, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_134 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    109, 105, 100, 100, 108, 101, 119, 97, 114, 101, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    80, 79, 83, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_141 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    102, 105, 108, 101, 95, 115, 101, 114, 118, 101, 114, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    123, 105, 100, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 57, 58, 51, 45, 50, 50, 50, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 101, 114, 118, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_143 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    99, 111, 114, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    79, 80, 84, 73, 79, 78, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 51, 58, 51, 45, 50, 50, 54, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    114, 101, 115, 112, 111, 110, 115, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    80, 65, 84, 67, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 51, 56, 58, 51, 45, 50, 52, 49, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 116, 97, 116, 117, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 51, 58, 52, 54, 45, 50, 49, 51, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    80, 111, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 57, 58, 49, 51, 45, 50, 50, 57, 58, 49, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 53, 58, 51, 45, 50, 49, 56, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    68, 101, 108, 101, 116, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 55, 58, 49, 51, 45, 50, 49, 55, 58, 49, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 52, 48, 58, 49, 51, 45, 50, 52, 48, 58, 49, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    105, 100, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 52, 52, 58, 49, 51, 45, 50, 52, 52, 58, 49, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 48, 58, 53, 45, 50, 50, 48, 58, 55, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 51, 58, 49, 54, 45, 50, 49, 51, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_74 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    76, 97, 121, 101, 114, 58, 58, 97, 100, 100, 95, 104, 97, 110, 100, 
    108, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_121 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    118, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    114, 101, 113, 117, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 49, 58, 49, 51, 45, 50, 50, 49, 58, 50, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 52, 55, 58, 49, 54, 45, 50, 52, 55, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 53, 58, 49, 51, 45, 50, 50, 53, 58, 50, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[99]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 98), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 116, 
    116, 112, 120, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_135 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    109, 101, 116, 104, 111, 100, 95, 115, 101, 116, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 56, 58, 53, 45, 50, 50, 56, 58, 57, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 52, 58, 49, 54, 45, 50, 49, 52, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    72, 69, 65, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 52, 58, 54, 48, 45, 50, 49, 52, 58, 54, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    77, 101, 116, 104, 111, 100, 77, 97, 112, 58, 32, 110, 111, 32, 118, 
    97, 108, 117, 101, 32, 102, 111, 114, 32, 109, 101, 116, 104, 111, 
    100, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 52, 55, 58, 53, 57, 45, 50, 52, 55, 58, 54, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    80, 97, 116, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    68, 69, 76, 69, 84, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 49, 54, 58, 53, 45, 50, 49, 54, 58, 53, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_112 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    84, 114, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    79, 112, 116, 105, 111, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 58, 114, 111, 117, 116, 101, 114, 46, 109, 98, 
    116, 58, 50, 50, 55, 58, 51, 45, 50, 51, 48, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct moonbit_object const moonbit_constant_constructor_1 =
  { -1, Moonbit_make_regular_object_header(2, 0, 1)};

struct moonbit_object const moonbit_constant_constructor_2 =
  { -1, Moonbit_make_regular_object_header(2, 0, 2)};

struct { int32_t rc; uint32_t meta; struct _M0TWiERPB4Json data; 
} const _M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodEC2673l78$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodEC2673l78
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRPC16string10StringViewERPB4Json data;
  
} const _M0IPC16string10StringViewPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IPC16string10StringViewPB6ToJson18to__json_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error data;
  
} const _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0C3416l210$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0C3416l210
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN17error__to__stringS1565$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN17error__to__stringS1565
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__0_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRPC16string10StringViewERPB4Json data;
  
} const _M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewEC2668l78$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewEC2668l78
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error data;
  
} const _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1C3499l235$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1C3499l235
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__1_2edyncall
  };

struct _M0TWRPC16string10StringViewERPB4Json* _M0IPC16string10StringViewPB6ToJson14to__json_2eclo =
  (struct _M0TWRPC16string10StringViewERPB4Json*)&_M0IPC16string10StringViewPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal5httpx39____test__726f757465722e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal5httpx39____test__726f757465722e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__1_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0156moonbitlang_2fcore_2fbuiltin_2fIter_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPB4IterPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPC16string10StringViewE}
  };

struct _M0BTPB6ToJson* _M0FP0156moonbitlang_2fcore_2fbuiltin_2fIter_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0156moonbitlang_2fcore_2fbuiltin_2fIter_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0163moonbitlang_2fcore_2fbuiltin_2fIter_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPB4IterPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal5httpx6MethodE}
  };

struct _M0BTPB6ToJson* _M0FP0163moonbitlang_2fcore_2fbuiltin_2fIter_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0163moonbitlang_2fcore_2fbuiltin_2fIter_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0157moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPC16string10StringViewE}
  };

struct _M0BTPB6ToJson* _M0FP0157moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0157moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE* _M0FP48clawteam8clawteam8internal5httpx7methods;

struct { int32_t rc; uint32_t meta; struct _M0TPB17FloatingDecimal64 data; 
} _M0FPB31ryu__to__string_2erecord_2f1176$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1176 =
  &_M0FPB31ryu__to__string_2erecord_2f1176$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal5httpx48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3677
) {
  return _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx43____test__726f757465722e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3676
) {
  return _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0();
}

void* _M0IPC16string10StringViewPB6ToJson18to__json_2edyncall(
  struct _M0TWRPC16string10StringViewERPB4Json* _M0L6_2aenvS3675,
  struct _M0TPC16string10StringView _M0L4selfS560
) {
  return _M0IPC16string10StringViewPB6ToJson8to__json(_M0L4selfS560);
}

int32_t _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1586,
  moonbit_string_t _M0L8filenameS1561,
  int32_t _M0L5indexS1564
) {
  struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556* _closure_4424;
  struct _M0TWssbEu* _M0L14handle__resultS1556;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1565;
  void* _M0L11_2atry__errS1580;
  struct moonbit_result_0 _tmp_4426;
  int32_t _handle__error__result_4427;
  int32_t _M0L6_2atmpS3663;
  void* _M0L3errS1581;
  moonbit_string_t _M0L4nameS1583;
  struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1584;
  moonbit_string_t _M0L8_2afieldS3678;
  int32_t _M0L6_2acntS4124;
  moonbit_string_t _M0L7_2anameS1585;
  #line 553 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1561);
  _closure_4424
  = (struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556*)moonbit_malloc(sizeof(struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556));
  Moonbit_object_header(_closure_4424)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556, $1) >> 2, 1, 0);
  _closure_4424->code
  = &_M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN14handle__resultS1556;
  _closure_4424->$0 = _M0L5indexS1564;
  _closure_4424->$1 = _M0L8filenameS1561;
  _M0L14handle__resultS1556 = (struct _M0TWssbEu*)_closure_4424;
  _M0L17error__to__stringS1565
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN17error__to__stringS1565$closure.data;
  moonbit_incref(_M0L12async__testsS1586);
  moonbit_incref(_M0L17error__to__stringS1565);
  moonbit_incref(_M0L8filenameS1561);
  moonbit_incref(_M0L14handle__resultS1556);
  #line 587 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _tmp_4426
  = _M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__test(_M0L12async__testsS1586, _M0L8filenameS1561, _M0L5indexS1564, _M0L14handle__resultS1556, _M0L17error__to__stringS1565);
  if (_tmp_4426.tag) {
    int32_t const _M0L5_2aokS3672 = _tmp_4426.data.ok;
    _handle__error__result_4427 = _M0L5_2aokS3672;
  } else {
    void* const _M0L6_2aerrS3673 = _tmp_4426.data.err;
    moonbit_decref(_M0L12async__testsS1586);
    moonbit_decref(_M0L17error__to__stringS1565);
    moonbit_decref(_M0L8filenameS1561);
    _M0L11_2atry__errS1580 = _M0L6_2aerrS3673;
    goto join_1579;
  }
  if (_handle__error__result_4427) {
    moonbit_decref(_M0L12async__testsS1586);
    moonbit_decref(_M0L17error__to__stringS1565);
    moonbit_decref(_M0L8filenameS1561);
    _M0L6_2atmpS3663 = 1;
  } else {
    struct moonbit_result_0 _tmp_4428;
    int32_t _handle__error__result_4429;
    moonbit_incref(_M0L12async__testsS1586);
    moonbit_incref(_M0L17error__to__stringS1565);
    moonbit_incref(_M0L8filenameS1561);
    moonbit_incref(_M0L14handle__resultS1556);
    #line 590 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    _tmp_4428
    = _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1586, _M0L8filenameS1561, _M0L5indexS1564, _M0L14handle__resultS1556, _M0L17error__to__stringS1565);
    if (_tmp_4428.tag) {
      int32_t const _M0L5_2aokS3670 = _tmp_4428.data.ok;
      _handle__error__result_4429 = _M0L5_2aokS3670;
    } else {
      void* const _M0L6_2aerrS3671 = _tmp_4428.data.err;
      moonbit_decref(_M0L12async__testsS1586);
      moonbit_decref(_M0L17error__to__stringS1565);
      moonbit_decref(_M0L8filenameS1561);
      _M0L11_2atry__errS1580 = _M0L6_2aerrS3671;
      goto join_1579;
    }
    if (_handle__error__result_4429) {
      moonbit_decref(_M0L12async__testsS1586);
      moonbit_decref(_M0L17error__to__stringS1565);
      moonbit_decref(_M0L8filenameS1561);
      _M0L6_2atmpS3663 = 1;
    } else {
      struct moonbit_result_0 _tmp_4430;
      int32_t _handle__error__result_4431;
      moonbit_incref(_M0L12async__testsS1586);
      moonbit_incref(_M0L17error__to__stringS1565);
      moonbit_incref(_M0L8filenameS1561);
      moonbit_incref(_M0L14handle__resultS1556);
      #line 593 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _tmp_4430
      = _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1586, _M0L8filenameS1561, _M0L5indexS1564, _M0L14handle__resultS1556, _M0L17error__to__stringS1565);
      if (_tmp_4430.tag) {
        int32_t const _M0L5_2aokS3668 = _tmp_4430.data.ok;
        _handle__error__result_4431 = _M0L5_2aokS3668;
      } else {
        void* const _M0L6_2aerrS3669 = _tmp_4430.data.err;
        moonbit_decref(_M0L12async__testsS1586);
        moonbit_decref(_M0L17error__to__stringS1565);
        moonbit_decref(_M0L8filenameS1561);
        _M0L11_2atry__errS1580 = _M0L6_2aerrS3669;
        goto join_1579;
      }
      if (_handle__error__result_4431) {
        moonbit_decref(_M0L12async__testsS1586);
        moonbit_decref(_M0L17error__to__stringS1565);
        moonbit_decref(_M0L8filenameS1561);
        _M0L6_2atmpS3663 = 1;
      } else {
        struct moonbit_result_0 _tmp_4432;
        int32_t _handle__error__result_4433;
        moonbit_incref(_M0L12async__testsS1586);
        moonbit_incref(_M0L17error__to__stringS1565);
        moonbit_incref(_M0L8filenameS1561);
        moonbit_incref(_M0L14handle__resultS1556);
        #line 596 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        _tmp_4432
        = _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1586, _M0L8filenameS1561, _M0L5indexS1564, _M0L14handle__resultS1556, _M0L17error__to__stringS1565);
        if (_tmp_4432.tag) {
          int32_t const _M0L5_2aokS3666 = _tmp_4432.data.ok;
          _handle__error__result_4433 = _M0L5_2aokS3666;
        } else {
          void* const _M0L6_2aerrS3667 = _tmp_4432.data.err;
          moonbit_decref(_M0L12async__testsS1586);
          moonbit_decref(_M0L17error__to__stringS1565);
          moonbit_decref(_M0L8filenameS1561);
          _M0L11_2atry__errS1580 = _M0L6_2aerrS3667;
          goto join_1579;
        }
        if (_handle__error__result_4433) {
          moonbit_decref(_M0L12async__testsS1586);
          moonbit_decref(_M0L17error__to__stringS1565);
          moonbit_decref(_M0L8filenameS1561);
          _M0L6_2atmpS3663 = 1;
        } else {
          struct moonbit_result_0 _tmp_4434;
          moonbit_incref(_M0L14handle__resultS1556);
          #line 599 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
          _tmp_4434
          = _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1586, _M0L8filenameS1561, _M0L5indexS1564, _M0L14handle__resultS1556, _M0L17error__to__stringS1565);
          if (_tmp_4434.tag) {
            int32_t const _M0L5_2aokS3664 = _tmp_4434.data.ok;
            _M0L6_2atmpS3663 = _M0L5_2aokS3664;
          } else {
            void* const _M0L6_2aerrS3665 = _tmp_4434.data.err;
            _M0L11_2atry__errS1580 = _M0L6_2aerrS3665;
            goto join_1579;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3663) {
    void* _M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3674 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3674)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3674)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1580
    = _M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3674;
    goto join_1579;
  } else {
    moonbit_decref(_M0L14handle__resultS1556);
  }
  goto joinlet_4425;
  join_1579:;
  _M0L3errS1581 = _M0L11_2atry__errS1580;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1584
  = (struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1581;
  _M0L8_2afieldS3678 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1584->$0;
  _M0L6_2acntS4124
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1584)->rc;
  if (_M0L6_2acntS4124 > 1) {
    int32_t _M0L11_2anew__cntS4125 = _M0L6_2acntS4124 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1584)->rc
    = _M0L11_2anew__cntS4125;
    moonbit_incref(_M0L8_2afieldS3678);
  } else if (_M0L6_2acntS4124 == 1) {
    #line 606 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1584);
  }
  _M0L7_2anameS1585 = _M0L8_2afieldS3678;
  _M0L4nameS1583 = _M0L7_2anameS1585;
  goto join_1582;
  goto joinlet_4435;
  join_1582:;
  #line 607 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN14handle__resultS1556(_M0L14handle__resultS1556, _M0L4nameS1583, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4435:;
  joinlet_4425:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN17error__to__stringS1565(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3662,
  void* _M0L3errS1566
) {
  void* _M0L1eS1568;
  moonbit_string_t _M0L1eS1570;
  #line 576 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3662);
  switch (Moonbit_object_tag(_M0L3errS1566)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1571 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1566;
      moonbit_string_t _M0L8_2afieldS3679 = _M0L10_2aFailureS1571->$0;
      int32_t _M0L6_2acntS4126 =
        Moonbit_object_header(_M0L10_2aFailureS1571)->rc;
      moonbit_string_t _M0L4_2aeS1572;
      if (_M0L6_2acntS4126 > 1) {
        int32_t _M0L11_2anew__cntS4127 = _M0L6_2acntS4126 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1571)->rc
        = _M0L11_2anew__cntS4127;
        moonbit_incref(_M0L8_2afieldS3679);
      } else if (_M0L6_2acntS4126 == 1) {
        #line 577 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1571);
      }
      _M0L4_2aeS1572 = _M0L8_2afieldS3679;
      _M0L1eS1570 = _M0L4_2aeS1572;
      goto join_1569;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1573 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1566;
      moonbit_string_t _M0L8_2afieldS3680 = _M0L15_2aInspectErrorS1573->$0;
      int32_t _M0L6_2acntS4128 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1573)->rc;
      moonbit_string_t _M0L4_2aeS1574;
      if (_M0L6_2acntS4128 > 1) {
        int32_t _M0L11_2anew__cntS4129 = _M0L6_2acntS4128 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1573)->rc
        = _M0L11_2anew__cntS4129;
        moonbit_incref(_M0L8_2afieldS3680);
      } else if (_M0L6_2acntS4128 == 1) {
        #line 577 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1573);
      }
      _M0L4_2aeS1574 = _M0L8_2afieldS3680;
      _M0L1eS1570 = _M0L4_2aeS1574;
      goto join_1569;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1575 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1566;
      moonbit_string_t _M0L8_2afieldS3681 = _M0L16_2aSnapshotErrorS1575->$0;
      int32_t _M0L6_2acntS4130 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1575)->rc;
      moonbit_string_t _M0L4_2aeS1576;
      if (_M0L6_2acntS4130 > 1) {
        int32_t _M0L11_2anew__cntS4131 = _M0L6_2acntS4130 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1575)->rc
        = _M0L11_2anew__cntS4131;
        moonbit_incref(_M0L8_2afieldS3681);
      } else if (_M0L6_2acntS4130 == 1) {
        #line 577 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1575);
      }
      _M0L4_2aeS1576 = _M0L8_2afieldS3681;
      _M0L1eS1570 = _M0L4_2aeS1576;
      goto join_1569;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1577 =
        (struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1566;
      moonbit_string_t _M0L8_2afieldS3682 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1577->$0;
      int32_t _M0L6_2acntS4132 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1577)->rc;
      moonbit_string_t _M0L4_2aeS1578;
      if (_M0L6_2acntS4132 > 1) {
        int32_t _M0L11_2anew__cntS4133 = _M0L6_2acntS4132 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1577)->rc
        = _M0L11_2anew__cntS4133;
        moonbit_incref(_M0L8_2afieldS3682);
      } else if (_M0L6_2acntS4132 == 1) {
        #line 577 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1577);
      }
      _M0L4_2aeS1578 = _M0L8_2afieldS3682;
      _M0L1eS1570 = _M0L4_2aeS1578;
      goto join_1569;
      break;
    }
    default: {
      _M0L1eS1568 = _M0L3errS1566;
      goto join_1567;
      break;
    }
  }
  join_1569:;
  return _M0L1eS1570;
  join_1567:;
  #line 582 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1568);
}

int32_t _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__executeN14handle__resultS1556(
  struct _M0TWssbEu* _M0L6_2aenvS3648,
  moonbit_string_t _M0L8testnameS1557,
  moonbit_string_t _M0L7messageS1558,
  int32_t _M0L7skippedS1559
) {
  struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556* _M0L14_2acasted__envS3649;
  moonbit_string_t _M0L8_2afieldS3692;
  moonbit_string_t _M0L8filenameS1561;
  int32_t _M0L8_2afieldS3691;
  int32_t _M0L6_2acntS4134;
  int32_t _M0L5indexS1564;
  int32_t _if__result_4438;
  moonbit_string_t _M0L10file__nameS1560;
  moonbit_string_t _M0L10test__nameS1562;
  moonbit_string_t _M0L7messageS1563;
  moonbit_string_t _M0L6_2atmpS3661;
  moonbit_string_t _M0L6_2atmpS3690;
  moonbit_string_t _M0L6_2atmpS3660;
  moonbit_string_t _M0L6_2atmpS3689;
  moonbit_string_t _M0L6_2atmpS3658;
  moonbit_string_t _M0L6_2atmpS3659;
  moonbit_string_t _M0L6_2atmpS3688;
  moonbit_string_t _M0L6_2atmpS3657;
  moonbit_string_t _M0L6_2atmpS3687;
  moonbit_string_t _M0L6_2atmpS3655;
  moonbit_string_t _M0L6_2atmpS3656;
  moonbit_string_t _M0L6_2atmpS3686;
  moonbit_string_t _M0L6_2atmpS3654;
  moonbit_string_t _M0L6_2atmpS3685;
  moonbit_string_t _M0L6_2atmpS3652;
  moonbit_string_t _M0L6_2atmpS3653;
  moonbit_string_t _M0L6_2atmpS3684;
  moonbit_string_t _M0L6_2atmpS3651;
  moonbit_string_t _M0L6_2atmpS3683;
  moonbit_string_t _M0L6_2atmpS3650;
  #line 560 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3649
  = (struct _M0R112_24clawteam_2fclawteam_2finternal_2fhttpx_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1556*)_M0L6_2aenvS3648;
  _M0L8_2afieldS3692 = _M0L14_2acasted__envS3649->$1;
  _M0L8filenameS1561 = _M0L8_2afieldS3692;
  _M0L8_2afieldS3691 = _M0L14_2acasted__envS3649->$0;
  _M0L6_2acntS4134 = Moonbit_object_header(_M0L14_2acasted__envS3649)->rc;
  if (_M0L6_2acntS4134 > 1) {
    int32_t _M0L11_2anew__cntS4135 = _M0L6_2acntS4134 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3649)->rc
    = _M0L11_2anew__cntS4135;
    moonbit_incref(_M0L8filenameS1561);
  } else if (_M0L6_2acntS4134 == 1) {
    #line 560 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3649);
  }
  _M0L5indexS1564 = _M0L8_2afieldS3691;
  if (!_M0L7skippedS1559) {
    _if__result_4438 = 1;
  } else {
    _if__result_4438 = 0;
  }
  if (_if__result_4438) {
    
  }
  #line 566 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1560 = _M0MPC16string6String6escape(_M0L8filenameS1561);
  #line 567 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1562 = _M0MPC16string6String6escape(_M0L8testnameS1557);
  #line 568 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1563 = _M0MPC16string6String6escape(_M0L7messageS1558);
  #line 569 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 571 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3661
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1560);
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3690
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3661);
  moonbit_decref(_M0L6_2atmpS3661);
  _M0L6_2atmpS3660 = _M0L6_2atmpS3690;
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3689
  = moonbit_add_string(_M0L6_2atmpS3660, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3660);
  _M0L6_2atmpS3658 = _M0L6_2atmpS3689;
  #line 571 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3659
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1564);
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3688 = moonbit_add_string(_M0L6_2atmpS3658, _M0L6_2atmpS3659);
  moonbit_decref(_M0L6_2atmpS3658);
  moonbit_decref(_M0L6_2atmpS3659);
  _M0L6_2atmpS3657 = _M0L6_2atmpS3688;
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3687
  = moonbit_add_string(_M0L6_2atmpS3657, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3657);
  _M0L6_2atmpS3655 = _M0L6_2atmpS3687;
  #line 571 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3656
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1562);
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3686 = moonbit_add_string(_M0L6_2atmpS3655, _M0L6_2atmpS3656);
  moonbit_decref(_M0L6_2atmpS3655);
  moonbit_decref(_M0L6_2atmpS3656);
  _M0L6_2atmpS3654 = _M0L6_2atmpS3686;
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3685
  = moonbit_add_string(_M0L6_2atmpS3654, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3654);
  _M0L6_2atmpS3652 = _M0L6_2atmpS3685;
  #line 571 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3653
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1563);
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3684 = moonbit_add_string(_M0L6_2atmpS3652, _M0L6_2atmpS3653);
  moonbit_decref(_M0L6_2atmpS3652);
  moonbit_decref(_M0L6_2atmpS3653);
  _M0L6_2atmpS3651 = _M0L6_2atmpS3684;
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3683
  = moonbit_add_string(_M0L6_2atmpS3651, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3651);
  _M0L6_2atmpS3650 = _M0L6_2atmpS3683;
  #line 570 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3650);
  #line 573 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1555,
  moonbit_string_t _M0L8filenameS1552,
  int32_t _M0L5indexS1546,
  struct _M0TWssbEu* _M0L14handle__resultS1542,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1544
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1522;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1551;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1524;
  moonbit_string_t* _M0L5attrsS1525;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1545;
  moonbit_string_t _M0L4nameS1528;
  moonbit_string_t _M0L4nameS1526;
  int32_t _M0L6_2atmpS3647;
  struct _M0TWEOs* _M0L5_2aitS1530;
  struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__* _closure_4447;
  struct _M0TWEOc* _M0L6_2atmpS3638;
  struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__* _closure_4448;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3639;
  struct moonbit_result_0 _result_4449;
  #line 434 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1555);
  moonbit_incref(_M0FP48clawteam8clawteam8internal5httpx48moonbit__test__driver__internal__no__args__tests);
  #line 441 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1551
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal5httpx48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1552);
  if (_M0L7_2abindS1551 == 0) {
    struct moonbit_result_0 _result_4440;
    if (_M0L7_2abindS1551) {
      moonbit_decref(_M0L7_2abindS1551);
    }
    moonbit_decref(_M0L17error__to__stringS1544);
    moonbit_decref(_M0L14handle__resultS1542);
    _result_4440.tag = 1;
    _result_4440.data.ok = 0;
    return _result_4440;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1553 =
      _M0L7_2abindS1551;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1554 =
      _M0L7_2aSomeS1553;
    _M0L10index__mapS1522 = _M0L13_2aindex__mapS1554;
    goto join_1521;
  }
  join_1521:;
  #line 443 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1545
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1522, _M0L5indexS1546);
  if (_M0L7_2abindS1545 == 0) {
    struct moonbit_result_0 _result_4442;
    if (_M0L7_2abindS1545) {
      moonbit_decref(_M0L7_2abindS1545);
    }
    moonbit_decref(_M0L17error__to__stringS1544);
    moonbit_decref(_M0L14handle__resultS1542);
    _result_4442.tag = 1;
    _result_4442.data.ok = 0;
    return _result_4442;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1547 =
      _M0L7_2abindS1545;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1548 = _M0L7_2aSomeS1547;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3696 = _M0L4_2axS1548->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1549 = _M0L8_2afieldS3696;
    moonbit_string_t* _M0L8_2afieldS3695 = _M0L4_2axS1548->$1;
    int32_t _M0L6_2acntS4136 = Moonbit_object_header(_M0L4_2axS1548)->rc;
    moonbit_string_t* _M0L8_2aattrsS1550;
    if (_M0L6_2acntS4136 > 1) {
      int32_t _M0L11_2anew__cntS4137 = _M0L6_2acntS4136 - 1;
      Moonbit_object_header(_M0L4_2axS1548)->rc = _M0L11_2anew__cntS4137;
      moonbit_incref(_M0L8_2afieldS3695);
      moonbit_incref(_M0L4_2afS1549);
    } else if (_M0L6_2acntS4136 == 1) {
      #line 441 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1548);
    }
    _M0L8_2aattrsS1550 = _M0L8_2afieldS3695;
    _M0L1fS1524 = _M0L4_2afS1549;
    _M0L5attrsS1525 = _M0L8_2aattrsS1550;
    goto join_1523;
  }
  join_1523:;
  _M0L6_2atmpS3647 = Moonbit_array_length(_M0L5attrsS1525);
  if (_M0L6_2atmpS3647 >= 1) {
    moonbit_string_t _M0L6_2atmpS3694 = (moonbit_string_t)_M0L5attrsS1525[0];
    moonbit_string_t _M0L7_2anameS1529 = _M0L6_2atmpS3694;
    moonbit_incref(_M0L7_2anameS1529);
    _M0L4nameS1528 = _M0L7_2anameS1529;
    goto join_1527;
  } else {
    _M0L4nameS1526 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4443;
  join_1527:;
  _M0L4nameS1526 = _M0L4nameS1528;
  joinlet_4443:;
  #line 444 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1530 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1525);
  while (1) {
    moonbit_string_t _M0L4attrS1532;
    moonbit_string_t _M0L7_2abindS1539;
    int32_t _M0L6_2atmpS3631;
    int64_t _M0L6_2atmpS3630;
    moonbit_incref(_M0L5_2aitS1530);
    #line 446 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1539 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1530);
    if (_M0L7_2abindS1539 == 0) {
      if (_M0L7_2abindS1539) {
        moonbit_decref(_M0L7_2abindS1539);
      }
      moonbit_decref(_M0L5_2aitS1530);
    } else {
      moonbit_string_t _M0L7_2aSomeS1540 = _M0L7_2abindS1539;
      moonbit_string_t _M0L7_2aattrS1541 = _M0L7_2aSomeS1540;
      _M0L4attrS1532 = _M0L7_2aattrS1541;
      goto join_1531;
    }
    goto joinlet_4445;
    join_1531:;
    _M0L6_2atmpS3631 = Moonbit_array_length(_M0L4attrS1532);
    _M0L6_2atmpS3630 = (int64_t)_M0L6_2atmpS3631;
    moonbit_incref(_M0L4attrS1532);
    #line 447 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1532, 5, 0, _M0L6_2atmpS3630)
    ) {
      int32_t _M0L6_2atmpS3637 = _M0L4attrS1532[0];
      int32_t _M0L4_2axS1533 = _M0L6_2atmpS3637;
      if (_M0L4_2axS1533 == 112) {
        int32_t _M0L6_2atmpS3636 = _M0L4attrS1532[1];
        int32_t _M0L4_2axS1534 = _M0L6_2atmpS3636;
        if (_M0L4_2axS1534 == 97) {
          int32_t _M0L6_2atmpS3635 = _M0L4attrS1532[2];
          int32_t _M0L4_2axS1535 = _M0L6_2atmpS3635;
          if (_M0L4_2axS1535 == 110) {
            int32_t _M0L6_2atmpS3634 = _M0L4attrS1532[3];
            int32_t _M0L4_2axS1536 = _M0L6_2atmpS3634;
            if (_M0L4_2axS1536 == 105) {
              int32_t _M0L6_2atmpS3693 = _M0L4attrS1532[4];
              int32_t _M0L6_2atmpS3633;
              int32_t _M0L4_2axS1537;
              moonbit_decref(_M0L4attrS1532);
              _M0L6_2atmpS3633 = _M0L6_2atmpS3693;
              _M0L4_2axS1537 = _M0L6_2atmpS3633;
              if (_M0L4_2axS1537 == 99) {
                void* _M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3632;
                struct moonbit_result_0 _result_4446;
                moonbit_decref(_M0L17error__to__stringS1544);
                moonbit_decref(_M0L14handle__resultS1542);
                moonbit_decref(_M0L5_2aitS1530);
                moonbit_decref(_M0L1fS1524);
                _M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3632
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3632)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3632)->$0
                = _M0L4nameS1526;
                _result_4446.tag = 0;
                _result_4446.data.err
                = _M0L110clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3632;
                return _result_4446;
              }
            } else {
              moonbit_decref(_M0L4attrS1532);
            }
          } else {
            moonbit_decref(_M0L4attrS1532);
          }
        } else {
          moonbit_decref(_M0L4attrS1532);
        }
      } else {
        moonbit_decref(_M0L4attrS1532);
      }
    } else {
      moonbit_decref(_M0L4attrS1532);
    }
    continue;
    joinlet_4445:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1542);
  moonbit_incref(_M0L4nameS1526);
  _closure_4447
  = (struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__*)moonbit_malloc(sizeof(struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__));
  Moonbit_object_header(_closure_4447)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__, $0) >> 2, 2, 0);
  _closure_4447->code
  = &_M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testC3644l454;
  _closure_4447->$0 = _M0L14handle__resultS1542;
  _closure_4447->$1 = _M0L4nameS1526;
  _M0L6_2atmpS3638 = (struct _M0TWEOc*)_closure_4447;
  _closure_4448
  = (struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__*)moonbit_malloc(sizeof(struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__));
  Moonbit_object_header(_closure_4448)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__, $0) >> 2, 3, 0);
  _closure_4448->code
  = &_M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testC3640l455;
  _closure_4448->$0 = _M0L17error__to__stringS1544;
  _closure_4448->$1 = _M0L14handle__resultS1542;
  _closure_4448->$2 = _M0L4nameS1526;
  _M0L6_2atmpS3639 = (struct _M0TWRPC15error5ErrorEu*)_closure_4448;
  #line 452 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal5httpx45moonbit__test__driver__internal__catch__error(_M0L1fS1524, _M0L6_2atmpS3638, _M0L6_2atmpS3639);
  _result_4449.tag = 1;
  _result_4449.data.ok = 1;
  return _result_4449;
}

int32_t _M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testC3644l454(
  struct _M0TWEOc* _M0L6_2aenvS3645
) {
  struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__* _M0L14_2acasted__envS3646;
  moonbit_string_t _M0L8_2afieldS3698;
  moonbit_string_t _M0L4nameS1526;
  struct _M0TWssbEu* _M0L8_2afieldS3697;
  int32_t _M0L6_2acntS4138;
  struct _M0TWssbEu* _M0L14handle__resultS1542;
  #line 454 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3646
  = (struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3644__l454__*)_M0L6_2aenvS3645;
  _M0L8_2afieldS3698 = _M0L14_2acasted__envS3646->$1;
  _M0L4nameS1526 = _M0L8_2afieldS3698;
  _M0L8_2afieldS3697 = _M0L14_2acasted__envS3646->$0;
  _M0L6_2acntS4138 = Moonbit_object_header(_M0L14_2acasted__envS3646)->rc;
  if (_M0L6_2acntS4138 > 1) {
    int32_t _M0L11_2anew__cntS4139 = _M0L6_2acntS4138 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3646)->rc
    = _M0L11_2anew__cntS4139;
    moonbit_incref(_M0L4nameS1526);
    moonbit_incref(_M0L8_2afieldS3697);
  } else if (_M0L6_2acntS4138 == 1) {
    #line 454 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3646);
  }
  _M0L14handle__resultS1542 = _M0L8_2afieldS3697;
  #line 454 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1542->code(_M0L14handle__resultS1542, _M0L4nameS1526, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal5httpx41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testC3640l455(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3641,
  void* _M0L3errS1543
) {
  struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__* _M0L14_2acasted__envS3642;
  moonbit_string_t _M0L8_2afieldS3701;
  moonbit_string_t _M0L4nameS1526;
  struct _M0TWssbEu* _M0L8_2afieldS3700;
  struct _M0TWssbEu* _M0L14handle__resultS1542;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3699;
  int32_t _M0L6_2acntS4140;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1544;
  moonbit_string_t _M0L6_2atmpS3643;
  #line 455 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3642
  = (struct _M0R193_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3640__l455__*)_M0L6_2aenvS3641;
  _M0L8_2afieldS3701 = _M0L14_2acasted__envS3642->$2;
  _M0L4nameS1526 = _M0L8_2afieldS3701;
  _M0L8_2afieldS3700 = _M0L14_2acasted__envS3642->$1;
  _M0L14handle__resultS1542 = _M0L8_2afieldS3700;
  _M0L8_2afieldS3699 = _M0L14_2acasted__envS3642->$0;
  _M0L6_2acntS4140 = Moonbit_object_header(_M0L14_2acasted__envS3642)->rc;
  if (_M0L6_2acntS4140 > 1) {
    int32_t _M0L11_2anew__cntS4141 = _M0L6_2acntS4140 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3642)->rc
    = _M0L11_2anew__cntS4141;
    moonbit_incref(_M0L4nameS1526);
    moonbit_incref(_M0L14handle__resultS1542);
    moonbit_incref(_M0L8_2afieldS3699);
  } else if (_M0L6_2acntS4140 == 1) {
    #line 455 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3642);
  }
  _M0L17error__to__stringS1544 = _M0L8_2afieldS3699;
  #line 455 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3643
  = _M0L17error__to__stringS1544->code(_M0L17error__to__stringS1544, _M0L3errS1543);
  #line 455 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1542->code(_M0L14handle__resultS1542, _M0L4nameS1526, _M0L6_2atmpS3643, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal5httpx45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1503,
  struct _M0TWEOc* _M0L6on__okS1504,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1501
) {
  void* _M0L11_2atry__errS1499;
  struct moonbit_result_0 _tmp_4451;
  void* _M0L3errS1500;
  #line 375 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _tmp_4451 = _M0L1fS1503->code(_M0L1fS1503);
  if (_tmp_4451.tag) {
    int32_t const _M0L5_2aokS3628 = _tmp_4451.data.ok;
    moonbit_decref(_M0L7on__errS1501);
  } else {
    void* const _M0L6_2aerrS3629 = _tmp_4451.data.err;
    moonbit_decref(_M0L6on__okS1504);
    _M0L11_2atry__errS1499 = _M0L6_2aerrS3629;
    goto join_1498;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1504->code(_M0L6on__okS1504);
  goto joinlet_4450;
  join_1498:;
  _M0L3errS1500 = _M0L11_2atry__errS1499;
  #line 383 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1501->code(_M0L7on__errS1501, _M0L3errS1500);
  joinlet_4450:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1458;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1471;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1476;
  struct _M0TUsiE** _M0L6_2atmpS3627;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1483;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1484;
  moonbit_string_t _M0L6_2atmpS3626;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1485;
  int32_t _M0L7_2abindS1486;
  int32_t _M0L2__S1487;
  #line 193 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1458 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1471
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1476 = 0;
  _M0L6_2atmpS3627 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1483
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1483)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1483->$0 = _M0L6_2atmpS3627;
  _M0L16file__and__indexS1483->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1484
  = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1471(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1471);
  #line 284 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3626 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1484, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1485
  = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1476(_M0L51moonbit__test__driver__internal__split__mbt__stringS1476, _M0L6_2atmpS3626, 47);
  _M0L7_2abindS1486 = _M0L10test__argsS1485->$1;
  _M0L2__S1487 = 0;
  while (1) {
    if (_M0L2__S1487 < _M0L7_2abindS1486) {
      moonbit_string_t* _M0L8_2afieldS3703 = _M0L10test__argsS1485->$0;
      moonbit_string_t* _M0L3bufS3625 = _M0L8_2afieldS3703;
      moonbit_string_t _M0L6_2atmpS3702 =
        (moonbit_string_t)_M0L3bufS3625[_M0L2__S1487];
      moonbit_string_t _M0L3argS1488 = _M0L6_2atmpS3702;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1489;
      moonbit_string_t _M0L4fileS1490;
      moonbit_string_t _M0L5rangeS1491;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1492;
      moonbit_string_t _M0L6_2atmpS3623;
      int32_t _M0L5startS1493;
      moonbit_string_t _M0L6_2atmpS3622;
      int32_t _M0L3endS1494;
      int32_t _M0L1iS1495;
      int32_t _M0L6_2atmpS3624;
      moonbit_incref(_M0L3argS1488);
      #line 288 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1489
      = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1476(_M0L51moonbit__test__driver__internal__split__mbt__stringS1476, _M0L3argS1488, 58);
      moonbit_incref(_M0L16file__and__rangeS1489);
      #line 289 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1490
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1489, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1491
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1489, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1492
      = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1476(_M0L51moonbit__test__driver__internal__split__mbt__stringS1476, _M0L5rangeS1491, 45);
      moonbit_incref(_M0L15start__and__endS1492);
      #line 294 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3623
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1492, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1493
      = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1458(_M0L45moonbit__test__driver__internal__parse__int__S1458, _M0L6_2atmpS3623);
      #line 295 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3622
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1492, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1494
      = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1458(_M0L45moonbit__test__driver__internal__parse__int__S1458, _M0L6_2atmpS3622);
      _M0L1iS1495 = _M0L5startS1493;
      while (1) {
        if (_M0L1iS1495 < _M0L3endS1494) {
          struct _M0TUsiE* _M0L8_2atupleS3620;
          int32_t _M0L6_2atmpS3621;
          moonbit_incref(_M0L4fileS1490);
          _M0L8_2atupleS3620
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3620)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3620->$0 = _M0L4fileS1490;
          _M0L8_2atupleS3620->$1 = _M0L1iS1495;
          moonbit_incref(_M0L16file__and__indexS1483);
          #line 297 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1483, _M0L8_2atupleS3620);
          _M0L6_2atmpS3621 = _M0L1iS1495 + 1;
          _M0L1iS1495 = _M0L6_2atmpS3621;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1490);
        }
        break;
      }
      _M0L6_2atmpS3624 = _M0L2__S1487 + 1;
      _M0L2__S1487 = _M0L6_2atmpS3624;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1485);
    }
    break;
  }
  return _M0L16file__and__indexS1483;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1476(
  int32_t _M0L6_2aenvS3601,
  moonbit_string_t _M0L1sS1477,
  int32_t _M0L3sepS1478
) {
  moonbit_string_t* _M0L6_2atmpS3619;
  struct _M0TPB5ArrayGsE* _M0L3resS1479;
  struct _M0TPC13ref3RefGiE* _M0L1iS1480;
  struct _M0TPC13ref3RefGiE* _M0L5startS1481;
  int32_t _M0L3valS3614;
  int32_t _M0L6_2atmpS3615;
  #line 261 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3619 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1479
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1479)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1479->$0 = _M0L6_2atmpS3619;
  _M0L3resS1479->$1 = 0;
  _M0L1iS1480
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1480)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1480->$0 = 0;
  _M0L5startS1481
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1481)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1481->$0 = 0;
  while (1) {
    int32_t _M0L3valS3602 = _M0L1iS1480->$0;
    int32_t _M0L6_2atmpS3603 = Moonbit_array_length(_M0L1sS1477);
    if (_M0L3valS3602 < _M0L6_2atmpS3603) {
      int32_t _M0L3valS3606 = _M0L1iS1480->$0;
      int32_t _M0L6_2atmpS3605;
      int32_t _M0L6_2atmpS3604;
      int32_t _M0L3valS3613;
      int32_t _M0L6_2atmpS3612;
      if (
        _M0L3valS3606 < 0
        || _M0L3valS3606 >= Moonbit_array_length(_M0L1sS1477)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3605 = _M0L1sS1477[_M0L3valS3606];
      _M0L6_2atmpS3604 = _M0L6_2atmpS3605;
      if (_M0L6_2atmpS3604 == _M0L3sepS1478) {
        int32_t _M0L3valS3608 = _M0L5startS1481->$0;
        int32_t _M0L3valS3609 = _M0L1iS1480->$0;
        moonbit_string_t _M0L6_2atmpS3607;
        int32_t _M0L3valS3611;
        int32_t _M0L6_2atmpS3610;
        moonbit_incref(_M0L1sS1477);
        #line 270 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3607
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1477, _M0L3valS3608, _M0L3valS3609);
        moonbit_incref(_M0L3resS1479);
        #line 270 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1479, _M0L6_2atmpS3607);
        _M0L3valS3611 = _M0L1iS1480->$0;
        _M0L6_2atmpS3610 = _M0L3valS3611 + 1;
        _M0L5startS1481->$0 = _M0L6_2atmpS3610;
      }
      _M0L3valS3613 = _M0L1iS1480->$0;
      _M0L6_2atmpS3612 = _M0L3valS3613 + 1;
      _M0L1iS1480->$0 = _M0L6_2atmpS3612;
      continue;
    } else {
      moonbit_decref(_M0L1iS1480);
    }
    break;
  }
  _M0L3valS3614 = _M0L5startS1481->$0;
  _M0L6_2atmpS3615 = Moonbit_array_length(_M0L1sS1477);
  if (_M0L3valS3614 < _M0L6_2atmpS3615) {
    int32_t _M0L8_2afieldS3704 = _M0L5startS1481->$0;
    int32_t _M0L3valS3617;
    int32_t _M0L6_2atmpS3618;
    moonbit_string_t _M0L6_2atmpS3616;
    moonbit_decref(_M0L5startS1481);
    _M0L3valS3617 = _M0L8_2afieldS3704;
    _M0L6_2atmpS3618 = Moonbit_array_length(_M0L1sS1477);
    #line 276 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3616
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1477, _M0L3valS3617, _M0L6_2atmpS3618);
    moonbit_incref(_M0L3resS1479);
    #line 276 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1479, _M0L6_2atmpS3616);
  } else {
    moonbit_decref(_M0L5startS1481);
    moonbit_decref(_M0L1sS1477);
  }
  return _M0L3resS1479;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1471(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464
) {
  moonbit_bytes_t* _M0L3tmpS1472;
  int32_t _M0L6_2atmpS3600;
  struct _M0TPB5ArrayGsE* _M0L3resS1473;
  int32_t _M0L1iS1474;
  #line 250 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1472
  = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3600 = Moonbit_array_length(_M0L3tmpS1472);
  #line 254 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1473 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3600);
  _M0L1iS1474 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3596 = Moonbit_array_length(_M0L3tmpS1472);
    if (_M0L1iS1474 < _M0L6_2atmpS3596) {
      moonbit_bytes_t _M0L6_2atmpS3705;
      moonbit_bytes_t _M0L6_2atmpS3598;
      moonbit_string_t _M0L6_2atmpS3597;
      int32_t _M0L6_2atmpS3599;
      if (
        _M0L1iS1474 < 0 || _M0L1iS1474 >= Moonbit_array_length(_M0L3tmpS1472)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3705 = (moonbit_bytes_t)_M0L3tmpS1472[_M0L1iS1474];
      _M0L6_2atmpS3598 = _M0L6_2atmpS3705;
      moonbit_incref(_M0L6_2atmpS3598);
      #line 256 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3597
      = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464, _M0L6_2atmpS3598);
      moonbit_incref(_M0L3resS1473);
      #line 256 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1473, _M0L6_2atmpS3597);
      _M0L6_2atmpS3599 = _M0L1iS1474 + 1;
      _M0L1iS1474 = _M0L6_2atmpS3599;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1472);
    }
    break;
  }
  return _M0L3resS1473;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1464(
  int32_t _M0L6_2aenvS3510,
  moonbit_bytes_t _M0L5bytesS1465
) {
  struct _M0TPB13StringBuilder* _M0L3resS1466;
  int32_t _M0L3lenS1467;
  struct _M0TPC13ref3RefGiE* _M0L1iS1468;
  #line 206 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1466 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1467 = Moonbit_array_length(_M0L5bytesS1465);
  _M0L1iS1468
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1468)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1468->$0 = 0;
  while (1) {
    int32_t _M0L3valS3511 = _M0L1iS1468->$0;
    if (_M0L3valS3511 < _M0L3lenS1467) {
      int32_t _M0L3valS3595 = _M0L1iS1468->$0;
      int32_t _M0L6_2atmpS3594;
      int32_t _M0L6_2atmpS3593;
      struct _M0TPC13ref3RefGiE* _M0L1cS1469;
      int32_t _M0L3valS3512;
      if (
        _M0L3valS3595 < 0
        || _M0L3valS3595 >= Moonbit_array_length(_M0L5bytesS1465)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3594 = _M0L5bytesS1465[_M0L3valS3595];
      _M0L6_2atmpS3593 = (int32_t)_M0L6_2atmpS3594;
      _M0L1cS1469
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1469)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1469->$0 = _M0L6_2atmpS3593;
      _M0L3valS3512 = _M0L1cS1469->$0;
      if (_M0L3valS3512 < 128) {
        int32_t _M0L8_2afieldS3706 = _M0L1cS1469->$0;
        int32_t _M0L3valS3514;
        int32_t _M0L6_2atmpS3513;
        int32_t _M0L3valS3516;
        int32_t _M0L6_2atmpS3515;
        moonbit_decref(_M0L1cS1469);
        _M0L3valS3514 = _M0L8_2afieldS3706;
        _M0L6_2atmpS3513 = _M0L3valS3514;
        moonbit_incref(_M0L3resS1466);
        #line 215 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1466, _M0L6_2atmpS3513);
        _M0L3valS3516 = _M0L1iS1468->$0;
        _M0L6_2atmpS3515 = _M0L3valS3516 + 1;
        _M0L1iS1468->$0 = _M0L6_2atmpS3515;
      } else {
        int32_t _M0L3valS3517 = _M0L1cS1469->$0;
        if (_M0L3valS3517 < 224) {
          int32_t _M0L3valS3519 = _M0L1iS1468->$0;
          int32_t _M0L6_2atmpS3518 = _M0L3valS3519 + 1;
          int32_t _M0L3valS3528;
          int32_t _M0L6_2atmpS3527;
          int32_t _M0L6_2atmpS3521;
          int32_t _M0L3valS3526;
          int32_t _M0L6_2atmpS3525;
          int32_t _M0L6_2atmpS3524;
          int32_t _M0L6_2atmpS3523;
          int32_t _M0L6_2atmpS3522;
          int32_t _M0L6_2atmpS3520;
          int32_t _M0L8_2afieldS3707;
          int32_t _M0L3valS3530;
          int32_t _M0L6_2atmpS3529;
          int32_t _M0L3valS3532;
          int32_t _M0L6_2atmpS3531;
          if (_M0L6_2atmpS3518 >= _M0L3lenS1467) {
            moonbit_decref(_M0L1cS1469);
            moonbit_decref(_M0L1iS1468);
            moonbit_decref(_M0L5bytesS1465);
            break;
          }
          _M0L3valS3528 = _M0L1cS1469->$0;
          _M0L6_2atmpS3527 = _M0L3valS3528 & 31;
          _M0L6_2atmpS3521 = _M0L6_2atmpS3527 << 6;
          _M0L3valS3526 = _M0L1iS1468->$0;
          _M0L6_2atmpS3525 = _M0L3valS3526 + 1;
          if (
            _M0L6_2atmpS3525 < 0
            || _M0L6_2atmpS3525 >= Moonbit_array_length(_M0L5bytesS1465)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3524 = _M0L5bytesS1465[_M0L6_2atmpS3525];
          _M0L6_2atmpS3523 = (int32_t)_M0L6_2atmpS3524;
          _M0L6_2atmpS3522 = _M0L6_2atmpS3523 & 63;
          _M0L6_2atmpS3520 = _M0L6_2atmpS3521 | _M0L6_2atmpS3522;
          _M0L1cS1469->$0 = _M0L6_2atmpS3520;
          _M0L8_2afieldS3707 = _M0L1cS1469->$0;
          moonbit_decref(_M0L1cS1469);
          _M0L3valS3530 = _M0L8_2afieldS3707;
          _M0L6_2atmpS3529 = _M0L3valS3530;
          moonbit_incref(_M0L3resS1466);
          #line 222 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1466, _M0L6_2atmpS3529);
          _M0L3valS3532 = _M0L1iS1468->$0;
          _M0L6_2atmpS3531 = _M0L3valS3532 + 2;
          _M0L1iS1468->$0 = _M0L6_2atmpS3531;
        } else {
          int32_t _M0L3valS3533 = _M0L1cS1469->$0;
          if (_M0L3valS3533 < 240) {
            int32_t _M0L3valS3535 = _M0L1iS1468->$0;
            int32_t _M0L6_2atmpS3534 = _M0L3valS3535 + 2;
            int32_t _M0L3valS3551;
            int32_t _M0L6_2atmpS3550;
            int32_t _M0L6_2atmpS3543;
            int32_t _M0L3valS3549;
            int32_t _M0L6_2atmpS3548;
            int32_t _M0L6_2atmpS3547;
            int32_t _M0L6_2atmpS3546;
            int32_t _M0L6_2atmpS3545;
            int32_t _M0L6_2atmpS3544;
            int32_t _M0L6_2atmpS3537;
            int32_t _M0L3valS3542;
            int32_t _M0L6_2atmpS3541;
            int32_t _M0L6_2atmpS3540;
            int32_t _M0L6_2atmpS3539;
            int32_t _M0L6_2atmpS3538;
            int32_t _M0L6_2atmpS3536;
            int32_t _M0L8_2afieldS3708;
            int32_t _M0L3valS3553;
            int32_t _M0L6_2atmpS3552;
            int32_t _M0L3valS3555;
            int32_t _M0L6_2atmpS3554;
            if (_M0L6_2atmpS3534 >= _M0L3lenS1467) {
              moonbit_decref(_M0L1cS1469);
              moonbit_decref(_M0L1iS1468);
              moonbit_decref(_M0L5bytesS1465);
              break;
            }
            _M0L3valS3551 = _M0L1cS1469->$0;
            _M0L6_2atmpS3550 = _M0L3valS3551 & 15;
            _M0L6_2atmpS3543 = _M0L6_2atmpS3550 << 12;
            _M0L3valS3549 = _M0L1iS1468->$0;
            _M0L6_2atmpS3548 = _M0L3valS3549 + 1;
            if (
              _M0L6_2atmpS3548 < 0
              || _M0L6_2atmpS3548 >= Moonbit_array_length(_M0L5bytesS1465)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3547 = _M0L5bytesS1465[_M0L6_2atmpS3548];
            _M0L6_2atmpS3546 = (int32_t)_M0L6_2atmpS3547;
            _M0L6_2atmpS3545 = _M0L6_2atmpS3546 & 63;
            _M0L6_2atmpS3544 = _M0L6_2atmpS3545 << 6;
            _M0L6_2atmpS3537 = _M0L6_2atmpS3543 | _M0L6_2atmpS3544;
            _M0L3valS3542 = _M0L1iS1468->$0;
            _M0L6_2atmpS3541 = _M0L3valS3542 + 2;
            if (
              _M0L6_2atmpS3541 < 0
              || _M0L6_2atmpS3541 >= Moonbit_array_length(_M0L5bytesS1465)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3540 = _M0L5bytesS1465[_M0L6_2atmpS3541];
            _M0L6_2atmpS3539 = (int32_t)_M0L6_2atmpS3540;
            _M0L6_2atmpS3538 = _M0L6_2atmpS3539 & 63;
            _M0L6_2atmpS3536 = _M0L6_2atmpS3537 | _M0L6_2atmpS3538;
            _M0L1cS1469->$0 = _M0L6_2atmpS3536;
            _M0L8_2afieldS3708 = _M0L1cS1469->$0;
            moonbit_decref(_M0L1cS1469);
            _M0L3valS3553 = _M0L8_2afieldS3708;
            _M0L6_2atmpS3552 = _M0L3valS3553;
            moonbit_incref(_M0L3resS1466);
            #line 231 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1466, _M0L6_2atmpS3552);
            _M0L3valS3555 = _M0L1iS1468->$0;
            _M0L6_2atmpS3554 = _M0L3valS3555 + 3;
            _M0L1iS1468->$0 = _M0L6_2atmpS3554;
          } else {
            int32_t _M0L3valS3557 = _M0L1iS1468->$0;
            int32_t _M0L6_2atmpS3556 = _M0L3valS3557 + 3;
            int32_t _M0L3valS3580;
            int32_t _M0L6_2atmpS3579;
            int32_t _M0L6_2atmpS3572;
            int32_t _M0L3valS3578;
            int32_t _M0L6_2atmpS3577;
            int32_t _M0L6_2atmpS3576;
            int32_t _M0L6_2atmpS3575;
            int32_t _M0L6_2atmpS3574;
            int32_t _M0L6_2atmpS3573;
            int32_t _M0L6_2atmpS3565;
            int32_t _M0L3valS3571;
            int32_t _M0L6_2atmpS3570;
            int32_t _M0L6_2atmpS3569;
            int32_t _M0L6_2atmpS3568;
            int32_t _M0L6_2atmpS3567;
            int32_t _M0L6_2atmpS3566;
            int32_t _M0L6_2atmpS3559;
            int32_t _M0L3valS3564;
            int32_t _M0L6_2atmpS3563;
            int32_t _M0L6_2atmpS3562;
            int32_t _M0L6_2atmpS3561;
            int32_t _M0L6_2atmpS3560;
            int32_t _M0L6_2atmpS3558;
            int32_t _M0L3valS3582;
            int32_t _M0L6_2atmpS3581;
            int32_t _M0L3valS3586;
            int32_t _M0L6_2atmpS3585;
            int32_t _M0L6_2atmpS3584;
            int32_t _M0L6_2atmpS3583;
            int32_t _M0L8_2afieldS3709;
            int32_t _M0L3valS3590;
            int32_t _M0L6_2atmpS3589;
            int32_t _M0L6_2atmpS3588;
            int32_t _M0L6_2atmpS3587;
            int32_t _M0L3valS3592;
            int32_t _M0L6_2atmpS3591;
            if (_M0L6_2atmpS3556 >= _M0L3lenS1467) {
              moonbit_decref(_M0L1cS1469);
              moonbit_decref(_M0L1iS1468);
              moonbit_decref(_M0L5bytesS1465);
              break;
            }
            _M0L3valS3580 = _M0L1cS1469->$0;
            _M0L6_2atmpS3579 = _M0L3valS3580 & 7;
            _M0L6_2atmpS3572 = _M0L6_2atmpS3579 << 18;
            _M0L3valS3578 = _M0L1iS1468->$0;
            _M0L6_2atmpS3577 = _M0L3valS3578 + 1;
            if (
              _M0L6_2atmpS3577 < 0
              || _M0L6_2atmpS3577 >= Moonbit_array_length(_M0L5bytesS1465)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3576 = _M0L5bytesS1465[_M0L6_2atmpS3577];
            _M0L6_2atmpS3575 = (int32_t)_M0L6_2atmpS3576;
            _M0L6_2atmpS3574 = _M0L6_2atmpS3575 & 63;
            _M0L6_2atmpS3573 = _M0L6_2atmpS3574 << 12;
            _M0L6_2atmpS3565 = _M0L6_2atmpS3572 | _M0L6_2atmpS3573;
            _M0L3valS3571 = _M0L1iS1468->$0;
            _M0L6_2atmpS3570 = _M0L3valS3571 + 2;
            if (
              _M0L6_2atmpS3570 < 0
              || _M0L6_2atmpS3570 >= Moonbit_array_length(_M0L5bytesS1465)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3569 = _M0L5bytesS1465[_M0L6_2atmpS3570];
            _M0L6_2atmpS3568 = (int32_t)_M0L6_2atmpS3569;
            _M0L6_2atmpS3567 = _M0L6_2atmpS3568 & 63;
            _M0L6_2atmpS3566 = _M0L6_2atmpS3567 << 6;
            _M0L6_2atmpS3559 = _M0L6_2atmpS3565 | _M0L6_2atmpS3566;
            _M0L3valS3564 = _M0L1iS1468->$0;
            _M0L6_2atmpS3563 = _M0L3valS3564 + 3;
            if (
              _M0L6_2atmpS3563 < 0
              || _M0L6_2atmpS3563 >= Moonbit_array_length(_M0L5bytesS1465)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3562 = _M0L5bytesS1465[_M0L6_2atmpS3563];
            _M0L6_2atmpS3561 = (int32_t)_M0L6_2atmpS3562;
            _M0L6_2atmpS3560 = _M0L6_2atmpS3561 & 63;
            _M0L6_2atmpS3558 = _M0L6_2atmpS3559 | _M0L6_2atmpS3560;
            _M0L1cS1469->$0 = _M0L6_2atmpS3558;
            _M0L3valS3582 = _M0L1cS1469->$0;
            _M0L6_2atmpS3581 = _M0L3valS3582 - 65536;
            _M0L1cS1469->$0 = _M0L6_2atmpS3581;
            _M0L3valS3586 = _M0L1cS1469->$0;
            _M0L6_2atmpS3585 = _M0L3valS3586 >> 10;
            _M0L6_2atmpS3584 = _M0L6_2atmpS3585 + 55296;
            _M0L6_2atmpS3583 = _M0L6_2atmpS3584;
            moonbit_incref(_M0L3resS1466);
            #line 242 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1466, _M0L6_2atmpS3583);
            _M0L8_2afieldS3709 = _M0L1cS1469->$0;
            moonbit_decref(_M0L1cS1469);
            _M0L3valS3590 = _M0L8_2afieldS3709;
            _M0L6_2atmpS3589 = _M0L3valS3590 & 1023;
            _M0L6_2atmpS3588 = _M0L6_2atmpS3589 + 56320;
            _M0L6_2atmpS3587 = _M0L6_2atmpS3588;
            moonbit_incref(_M0L3resS1466);
            #line 243 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1466, _M0L6_2atmpS3587);
            _M0L3valS3592 = _M0L1iS1468->$0;
            _M0L6_2atmpS3591 = _M0L3valS3592 + 4;
            _M0L1iS1468->$0 = _M0L6_2atmpS3591;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1468);
      moonbit_decref(_M0L5bytesS1465);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1466);
}

int32_t _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1458(
  int32_t _M0L6_2aenvS3503,
  moonbit_string_t _M0L1sS1459
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1460;
  int32_t _M0L3lenS1461;
  int32_t _M0L1iS1462;
  int32_t _M0L8_2afieldS3710;
  #line 197 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1460
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1460)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1460->$0 = 0;
  _M0L3lenS1461 = Moonbit_array_length(_M0L1sS1459);
  _M0L1iS1462 = 0;
  while (1) {
    if (_M0L1iS1462 < _M0L3lenS1461) {
      int32_t _M0L3valS3508 = _M0L3resS1460->$0;
      int32_t _M0L6_2atmpS3505 = _M0L3valS3508 * 10;
      int32_t _M0L6_2atmpS3507;
      int32_t _M0L6_2atmpS3506;
      int32_t _M0L6_2atmpS3504;
      int32_t _M0L6_2atmpS3509;
      if (
        _M0L1iS1462 < 0 || _M0L1iS1462 >= Moonbit_array_length(_M0L1sS1459)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3507 = _M0L1sS1459[_M0L1iS1462];
      _M0L6_2atmpS3506 = _M0L6_2atmpS3507 - 48;
      _M0L6_2atmpS3504 = _M0L6_2atmpS3505 + _M0L6_2atmpS3506;
      _M0L3resS1460->$0 = _M0L6_2atmpS3504;
      _M0L6_2atmpS3509 = _M0L1iS1462 + 1;
      _M0L1iS1462 = _M0L6_2atmpS3509;
      continue;
    } else {
      moonbit_decref(_M0L1sS1459);
    }
    break;
  }
  _M0L8_2afieldS3710 = _M0L3resS1460->$0;
  moonbit_decref(_M0L3resS1460);
  return _M0L8_2afieldS3710;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1438,
  moonbit_string_t _M0L12_2adiscard__S1439,
  int32_t _M0L12_2adiscard__S1440,
  struct _M0TWssbEu* _M0L12_2adiscard__S1441,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1442
) {
  struct moonbit_result_0 _result_4458;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1442);
  moonbit_decref(_M0L12_2adiscard__S1441);
  moonbit_decref(_M0L12_2adiscard__S1439);
  moonbit_decref(_M0L12_2adiscard__S1438);
  _result_4458.tag = 1;
  _result_4458.data.ok = 0;
  return _result_4458;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1443,
  moonbit_string_t _M0L12_2adiscard__S1444,
  int32_t _M0L12_2adiscard__S1445,
  struct _M0TWssbEu* _M0L12_2adiscard__S1446,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1447
) {
  struct moonbit_result_0 _result_4459;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1447);
  moonbit_decref(_M0L12_2adiscard__S1446);
  moonbit_decref(_M0L12_2adiscard__S1444);
  moonbit_decref(_M0L12_2adiscard__S1443);
  _result_4459.tag = 1;
  _result_4459.data.ok = 0;
  return _result_4459;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1448,
  moonbit_string_t _M0L12_2adiscard__S1449,
  int32_t _M0L12_2adiscard__S1450,
  struct _M0TWssbEu* _M0L12_2adiscard__S1451,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1452
) {
  struct moonbit_result_0 _result_4460;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1452);
  moonbit_decref(_M0L12_2adiscard__S1451);
  moonbit_decref(_M0L12_2adiscard__S1449);
  moonbit_decref(_M0L12_2adiscard__S1448);
  _result_4460.tag = 1;
  _result_4460.data.ok = 0;
  return _result_4460;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal5httpx21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal5httpx50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1453,
  moonbit_string_t _M0L12_2adiscard__S1454,
  int32_t _M0L12_2adiscard__S1455,
  struct _M0TWssbEu* _M0L12_2adiscard__S1456,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1457
) {
  struct moonbit_result_0 _result_4461;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1457);
  moonbit_decref(_M0L12_2adiscard__S1456);
  moonbit_decref(_M0L12_2adiscard__S1454);
  moonbit_decref(_M0L12_2adiscard__S1453);
  _result_4461.tag = 1;
  _result_4461.data.ok = 0;
  return _result_4461;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal5httpx28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal5httpx34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1437
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1437);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1(
  
) {
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error* _M0L6_2atmpS3498;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L5routeS1409;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5layerS1414;
  moonbit_string_t _M0L7_2abindS1416;
  int32_t _M0L6_2atmpS3429;
  struct _M0TPC16string10StringView _M0L6_2atmpS3422;
  moonbit_string_t _M0L7_2abindS1417;
  int32_t _M0L6_2atmpS3428;
  struct _M0TPC16string10StringView _M0L6_2atmpS3423;
  moonbit_string_t _M0L7_2abindS1418;
  int32_t _M0L6_2atmpS3427;
  struct _M0TPC16string10StringView _M0L6_2atmpS3424;
  moonbit_string_t _M0L7_2abindS1419;
  int32_t _M0L6_2atmpS3426;
  struct _M0TPC16string10StringView _M0L6_2atmpS3425;
  struct _M0TPC16string10StringView* _M0L7_2abindS1415;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3421;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS3420;
  moonbit_string_t _M0L7_2abindS1422;
  int32_t _M0L6_2atmpS3451;
  struct _M0TPC16string10StringView _M0L6_2atmpS3444;
  moonbit_string_t _M0L7_2abindS1423;
  int32_t _M0L6_2atmpS3450;
  struct _M0TPC16string10StringView _M0L6_2atmpS3445;
  moonbit_string_t _M0L7_2abindS1424;
  int32_t _M0L6_2atmpS3449;
  struct _M0TPC16string10StringView _M0L6_2atmpS3446;
  moonbit_string_t _M0L7_2abindS1425;
  int32_t _M0L6_2atmpS3448;
  struct _M0TPC16string10StringView _M0L6_2atmpS3447;
  struct _M0TPC16string10StringView* _M0L7_2abindS1421;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3443;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS3442;
  struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE* _M0L7_2abindS1420;
  int32_t _M0L6_2atmpS3713;
  int32_t _M0L6_2atmpS3441;
  int32_t _M0L6_2atmpS3439;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3440;
  struct _M0TPB6ToJson _M0L6_2atmpS3430;
  void* _M0L6_2atmpS3438;
  void* _M0L6_2atmpS3431;
  moonbit_string_t _M0L6_2atmpS3434;
  moonbit_string_t _M0L6_2atmpS3435;
  moonbit_string_t _M0L6_2atmpS3436;
  moonbit_string_t _M0L6_2atmpS3437;
  moonbit_string_t* _M0L6_2atmpS3433;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3432;
  struct moonbit_result_0 _tmp_4462;
  moonbit_string_t _M0L7_2abindS1428;
  int32_t _M0L6_2atmpS3475;
  struct _M0TPC16string10StringView _M0L6_2atmpS3468;
  moonbit_string_t _M0L7_2abindS1429;
  int32_t _M0L6_2atmpS3474;
  struct _M0TPC16string10StringView _M0L6_2atmpS3469;
  moonbit_string_t _M0L7_2abindS1430;
  int32_t _M0L6_2atmpS3473;
  struct _M0TPC16string10StringView _M0L6_2atmpS3470;
  moonbit_string_t _M0L7_2abindS1431;
  int32_t _M0L6_2atmpS3472;
  struct _M0TPC16string10StringView _M0L6_2atmpS3471;
  struct _M0TPC16string10StringView* _M0L7_2abindS1427;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3467;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS3466;
  struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE* _M0L7_2abindS1426;
  int32_t _M0L6_2atmpS3712;
  int32_t _M0L6_2atmpS3465;
  int32_t _M0L6_2atmpS3463;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3464;
  struct _M0TPB6ToJson _M0L6_2atmpS3454;
  void* _M0L6_2atmpS3462;
  void* _M0L6_2atmpS3455;
  moonbit_string_t _M0L6_2atmpS3458;
  moonbit_string_t _M0L6_2atmpS3459;
  moonbit_string_t _M0L6_2atmpS3460;
  moonbit_string_t _M0L6_2atmpS3461;
  moonbit_string_t* _M0L6_2atmpS3457;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3456;
  struct moonbit_result_0 _tmp_4464;
  moonbit_string_t _M0L7_2abindS1433;
  int32_t _M0L6_2atmpS3481;
  struct _M0TPC16string10StringView _M0L6_2atmpS3480;
  struct _M0TPC16string10StringView* _M0L7_2abindS1432;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3479;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS3478;
  moonbit_string_t _M0L7_2abindS1436;
  int32_t _M0L6_2atmpS3497;
  struct _M0TPC16string10StringView _M0L6_2atmpS3496;
  struct _M0TPC16string10StringView* _M0L7_2abindS1435;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3495;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS3494;
  struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE* _M0L7_2abindS1434;
  int32_t _M0L6_2atmpS3711;
  int32_t _M0L6_2atmpS3493;
  int32_t _M0L6_2atmpS3491;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3492;
  struct _M0TPB6ToJson _M0L6_2atmpS3482;
  void* _M0L6_2atmpS3490;
  void* _M0L6_2atmpS3483;
  moonbit_string_t _M0L6_2atmpS3486;
  moonbit_string_t _M0L6_2atmpS3487;
  moonbit_string_t _M0L6_2atmpS3488;
  moonbit_string_t _M0L6_2atmpS3489;
  moonbit_string_t* _M0L6_2atmpS3485;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3484;
  #line 234 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3498
  = (struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1C3499l235$closure.data;
  #line 235 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L5routeS1409
  = _M0MP48clawteam8clawteam8internal5httpx5Route3new(_M0L6_2atmpS3498);
  #line 236 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L5layerS1414 = _M0MP48clawteam8clawteam8internal5httpx5Layer3new();
  _M0L7_2abindS1416 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3429 = Moonbit_array_length(_M0L7_2abindS1416);
  _M0L6_2atmpS3422
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3429, _M0L7_2abindS1416
  };
  _M0L7_2abindS1417 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3428 = Moonbit_array_length(_M0L7_2abindS1417);
  _M0L6_2atmpS3423
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3428, _M0L7_2abindS1417
  };
  _M0L7_2abindS1418 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3427 = Moonbit_array_length(_M0L7_2abindS1418);
  _M0L6_2atmpS3424
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3427, _M0L7_2abindS1418
  };
  _M0L7_2abindS1419 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3426 = Moonbit_array_length(_M0L7_2abindS1419);
  _M0L6_2atmpS3425
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3426, _M0L7_2abindS1419
  };
  _M0L7_2abindS1415
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(4, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L7_2abindS1415[0] = _M0L6_2atmpS3422;
  _M0L7_2abindS1415[1] = _M0L6_2atmpS3423;
  _M0L7_2abindS1415[2] = _M0L6_2atmpS3424;
  _M0L7_2abindS1415[3] = _M0L6_2atmpS3425;
  _M0L6_2atmpS3421 = _M0L7_2abindS1415;
  _M0L6_2atmpS3420
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, 4, _M0L6_2atmpS3421
  };
  moonbit_incref(_M0L5layerS1414);
  moonbit_incref(_M0L5routeS1409);
  #line 237 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(_M0L5layerS1414, 0, _M0L6_2atmpS3420, _M0L5routeS1409);
  _M0L7_2abindS1422 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3451 = Moonbit_array_length(_M0L7_2abindS1422);
  _M0L6_2atmpS3444
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3451, _M0L7_2abindS1422
  };
  _M0L7_2abindS1423 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3450 = Moonbit_array_length(_M0L7_2abindS1423);
  _M0L6_2atmpS3445
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3450, _M0L7_2abindS1423
  };
  _M0L7_2abindS1424 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3449 = Moonbit_array_length(_M0L7_2abindS1424);
  _M0L6_2atmpS3446
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3449, _M0L7_2abindS1424
  };
  _M0L7_2abindS1425 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3448 = Moonbit_array_length(_M0L7_2abindS1425);
  _M0L6_2atmpS3447
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3448, _M0L7_2abindS1425
  };
  _M0L7_2abindS1421
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(4, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L7_2abindS1421[0] = _M0L6_2atmpS3444;
  _M0L7_2abindS1421[1] = _M0L6_2atmpS3445;
  _M0L7_2abindS1421[2] = _M0L6_2atmpS3446;
  _M0L7_2abindS1421[3] = _M0L6_2atmpS3447;
  _M0L6_2atmpS3443 = _M0L7_2abindS1421;
  _M0L6_2atmpS3442
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, 4, _M0L6_2atmpS3443
  };
  moonbit_incref(_M0L5layerS1414);
  #line 239 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L7_2abindS1420
  = _M0MP48clawteam8clawteam8internal5httpx5Layer5route(_M0L5layerS1414, _M0L6_2atmpS3442);
  _M0L6_2atmpS3713 = _M0L7_2abindS1420 == 0;
  if (_M0L7_2abindS1420) {
    moonbit_decref(_M0L7_2abindS1420);
  }
  _M0L6_2atmpS3441 = _M0L6_2atmpS3713;
  _M0L6_2atmpS3439 = !_M0L6_2atmpS3441;
  _M0L14_2aboxed__selfS3440
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3440)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3440->$0 = _M0L6_2atmpS3439;
  _M0L6_2atmpS3430
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3440
  };
  #line 240 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3438 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3431 = _M0L6_2atmpS3438;
  _M0L6_2atmpS3434 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3435 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3436 = 0;
  _M0L6_2atmpS3437 = 0;
  _M0L6_2atmpS3433 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3433[0] = _M0L6_2atmpS3434;
  _M0L6_2atmpS3433[1] = _M0L6_2atmpS3435;
  _M0L6_2atmpS3433[2] = _M0L6_2atmpS3436;
  _M0L6_2atmpS3433[3] = _M0L6_2atmpS3437;
  _M0L6_2atmpS3432
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3432)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3432->$0 = _M0L6_2atmpS3433;
  _M0L6_2atmpS3432->$1 = 4;
  #line 238 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _tmp_4462
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3430, _M0L6_2atmpS3431, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS3432);
  if (_tmp_4462.tag) {
    int32_t const _M0L5_2aokS3452 = _tmp_4462.data.ok;
  } else {
    void* const _M0L6_2aerrS3453 = _tmp_4462.data.err;
    struct moonbit_result_0 _result_4463;
    moonbit_decref(_M0L5layerS1414);
    moonbit_decref(_M0L5routeS1409);
    _result_4463.tag = 0;
    _result_4463.data.err = _M0L6_2aerrS3453;
    return _result_4463;
  }
  _M0L7_2abindS1428 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3475 = Moonbit_array_length(_M0L7_2abindS1428);
  _M0L6_2atmpS3468
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3475, _M0L7_2abindS1428
  };
  _M0L7_2abindS1429 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3474 = Moonbit_array_length(_M0L7_2abindS1429);
  _M0L6_2atmpS3469
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3474, _M0L7_2abindS1429
  };
  _M0L7_2abindS1430 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3473 = Moonbit_array_length(_M0L7_2abindS1430);
  _M0L6_2atmpS3470
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3473, _M0L7_2abindS1430
  };
  _M0L7_2abindS1431 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3472 = Moonbit_array_length(_M0L7_2abindS1431);
  _M0L6_2atmpS3471
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3472, _M0L7_2abindS1431
  };
  _M0L7_2abindS1427
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(4, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L7_2abindS1427[0] = _M0L6_2atmpS3468;
  _M0L7_2abindS1427[1] = _M0L6_2atmpS3469;
  _M0L7_2abindS1427[2] = _M0L6_2atmpS3470;
  _M0L7_2abindS1427[3] = _M0L6_2atmpS3471;
  _M0L6_2atmpS3467 = _M0L7_2abindS1427;
  _M0L6_2atmpS3466
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, 4, _M0L6_2atmpS3467
  };
  moonbit_incref(_M0L5layerS1414);
  #line 243 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L7_2abindS1426
  = _M0MP48clawteam8clawteam8internal5httpx5Layer5route(_M0L5layerS1414, _M0L6_2atmpS3466);
  _M0L6_2atmpS3712 = _M0L7_2abindS1426 == 0;
  if (_M0L7_2abindS1426) {
    moonbit_decref(_M0L7_2abindS1426);
  }
  _M0L6_2atmpS3465 = _M0L6_2atmpS3712;
  _M0L6_2atmpS3463 = !_M0L6_2atmpS3465;
  _M0L14_2aboxed__selfS3464
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3464)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3464->$0 = _M0L6_2atmpS3463;
  _M0L6_2atmpS3454
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3464
  };
  #line 244 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3462 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3455 = _M0L6_2atmpS3462;
  _M0L6_2atmpS3458 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3459 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3460 = 0;
  _M0L6_2atmpS3461 = 0;
  _M0L6_2atmpS3457 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3457[0] = _M0L6_2atmpS3458;
  _M0L6_2atmpS3457[1] = _M0L6_2atmpS3459;
  _M0L6_2atmpS3457[2] = _M0L6_2atmpS3460;
  _M0L6_2atmpS3457[3] = _M0L6_2atmpS3461;
  _M0L6_2atmpS3456
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3456)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3456->$0 = _M0L6_2atmpS3457;
  _M0L6_2atmpS3456->$1 = 4;
  #line 242 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _tmp_4464
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3454, _M0L6_2atmpS3455, (moonbit_string_t)moonbit_string_literal_20.data, _M0L6_2atmpS3456);
  if (_tmp_4464.tag) {
    int32_t const _M0L5_2aokS3476 = _tmp_4464.data.ok;
  } else {
    void* const _M0L6_2aerrS3477 = _tmp_4464.data.err;
    struct moonbit_result_0 _result_4465;
    moonbit_decref(_M0L5layerS1414);
    moonbit_decref(_M0L5routeS1409);
    _result_4465.tag = 0;
    _result_4465.data.err = _M0L6_2aerrS3477;
    return _result_4465;
  }
  _M0L7_2abindS1433 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3481 = Moonbit_array_length(_M0L7_2abindS1433);
  _M0L6_2atmpS3480
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3481, _M0L7_2abindS1433
  };
  _M0L7_2abindS1432
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(1, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L7_2abindS1432[0] = _M0L6_2atmpS3480;
  _M0L6_2atmpS3479 = _M0L7_2abindS1432;
  _M0L6_2atmpS3478
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, 1, _M0L6_2atmpS3479
  };
  moonbit_incref(_M0L5layerS1414);
  #line 246 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(_M0L5layerS1414, 2, _M0L6_2atmpS3478, _M0L5routeS1409);
  _M0L7_2abindS1436 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3497 = Moonbit_array_length(_M0L7_2abindS1436);
  _M0L6_2atmpS3496
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3497, _M0L7_2abindS1436
  };
  _M0L7_2abindS1435
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(1, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L7_2abindS1435[0] = _M0L6_2atmpS3496;
  _M0L6_2atmpS3495 = _M0L7_2abindS1435;
  _M0L6_2atmpS3494
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, 1, _M0L6_2atmpS3495
  };
  #line 247 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L7_2abindS1434
  = _M0MP48clawteam8clawteam8internal5httpx5Layer5route(_M0L5layerS1414, _M0L6_2atmpS3494);
  _M0L6_2atmpS3711 = _M0L7_2abindS1434 == 0;
  if (_M0L7_2abindS1434) {
    moonbit_decref(_M0L7_2abindS1434);
  }
  _M0L6_2atmpS3493 = _M0L6_2atmpS3711;
  _M0L6_2atmpS3491 = !_M0L6_2atmpS3493;
  _M0L14_2aboxed__selfS3492
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3492)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3492->$0 = _M0L6_2atmpS3491;
  _M0L6_2atmpS3482
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3492
  };
  #line 247 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3490 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3483 = _M0L6_2atmpS3490;
  _M0L6_2atmpS3486 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3487 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3488 = 0;
  _M0L6_2atmpS3489 = 0;
  _M0L6_2atmpS3485 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3485[0] = _M0L6_2atmpS3486;
  _M0L6_2atmpS3485[1] = _M0L6_2atmpS3487;
  _M0L6_2atmpS3485[2] = _M0L6_2atmpS3488;
  _M0L6_2atmpS3485[3] = _M0L6_2atmpS3489;
  _M0L6_2atmpS3484
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3484)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3484->$0 = _M0L6_2atmpS3485;
  _M0L6_2atmpS3484->$1 = 4;
  #line 247 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3482, _M0L6_2atmpS3483, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS3484);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__1C3499l235(
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error* _M0L6_2aenvS3500,
  struct _M0TP48clawteam8clawteam8internal5httpx13RequestReader* _M0L12_2adiscard__S1410,
  struct _M0TP48clawteam8clawteam8internal5httpx14ResponseWriter* _M0L12_2adiscard__S1411,
  struct _M0TWiEu* _M0L7_2acontS1412,
  struct _M0TWRPC15error5ErrorEu* _M0L12_2aerr__contS1413
) {
  int32_t _M0L6_2atmpS3502;
  int32_t _M0L6_2atmpS3501;
  struct moonbit_result_0 _result_4466;
  #line 235 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  moonbit_decref(_M0L12_2aerr__contS1413);
  moonbit_decref(_M0L7_2acontS1412);
  moonbit_decref(_M0L12_2adiscard__S1411);
  moonbit_decref(_M0L12_2adiscard__S1410);
  moonbit_decref(_M0L6_2aenvS3500);
  _M0L6_2atmpS3502 = 0;
  _M0L6_2atmpS3501 = _M0L6_2atmpS3502;
  _result_4466.tag = 1;
  _result_4466.data.ok = _M0L6_2atmpS3501;
  return _result_4466;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0(
  
) {
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error* _M0L6_2atmpS3415;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L5routeS1386;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5layerS1391;
  moonbit_string_t _M0L7_2abindS1393;
  int32_t _M0L6_2atmpS3271;
  struct _M0TPC16string10StringView _M0L6_2atmpS3264;
  moonbit_string_t _M0L7_2abindS1394;
  int32_t _M0L6_2atmpS3270;
  struct _M0TPC16string10StringView _M0L6_2atmpS3265;
  moonbit_string_t _M0L7_2abindS1395;
  int32_t _M0L6_2atmpS3269;
  struct _M0TPC16string10StringView _M0L6_2atmpS3266;
  moonbit_string_t _M0L7_2abindS1396;
  int32_t _M0L6_2atmpS3268;
  struct _M0TPC16string10StringView _M0L6_2atmpS3267;
  struct _M0TPC16string10StringView* _M0L7_2abindS1392;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3263;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS3262;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3735;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3285;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS3284;
  struct _M0TPB6ToJson _M0L6_2atmpS3272;
  void* _M0L6_2atmpS3283;
  void** _M0L6_2atmpS3282;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3281;
  void* _M0L6_2atmpS3280;
  void* _M0L6_2atmpS3273;
  moonbit_string_t _M0L6_2atmpS3276;
  moonbit_string_t _M0L6_2atmpS3277;
  moonbit_string_t _M0L6_2atmpS3278;
  moonbit_string_t _M0L6_2atmpS3279;
  moonbit_string_t* _M0L6_2atmpS3275;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3274;
  struct moonbit_result_0 _tmp_4467;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3734;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3303;
  moonbit_string_t _M0L7_2abindS1397;
  int32_t _M0L6_2atmpS3305;
  struct _M0TPC16string10StringView _M0L6_2atmpS3304;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3302;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3733;
  int32_t _M0L6_2acntS4142;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3301;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS3300;
  struct _M0TPB6ToJson _M0L6_2atmpS3288;
  void* _M0L6_2atmpS3299;
  void** _M0L6_2atmpS3298;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3297;
  void* _M0L6_2atmpS3296;
  void* _M0L6_2atmpS3289;
  moonbit_string_t _M0L6_2atmpS3292;
  moonbit_string_t _M0L6_2atmpS3293;
  moonbit_string_t _M0L6_2atmpS3294;
  moonbit_string_t _M0L6_2atmpS3295;
  moonbit_string_t* _M0L6_2atmpS3291;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3290;
  struct moonbit_result_0 _tmp_4469;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3732;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3325;
  moonbit_string_t _M0L7_2abindS1399;
  int32_t _M0L6_2atmpS3327;
  struct _M0TPC16string10StringView _M0L6_2atmpS3326;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3324;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3731;
  int32_t _M0L6_2acntS4146;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3321;
  moonbit_string_t _M0L7_2abindS1400;
  int32_t _M0L6_2atmpS3323;
  struct _M0TPC16string10StringView _M0L6_2atmpS3322;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3320;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3730;
  int32_t _M0L6_2acntS4150;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2abindS1398;
  int32_t _M0L6_2atmpS3729;
  int32_t _M0L6_2atmpS3319;
  int32_t _M0L6_2atmpS3317;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3318;
  struct _M0TPB6ToJson _M0L6_2atmpS3308;
  void* _M0L6_2atmpS3316;
  void* _M0L6_2atmpS3309;
  moonbit_string_t _M0L6_2atmpS3312;
  moonbit_string_t _M0L6_2atmpS3313;
  moonbit_string_t _M0L6_2atmpS3314;
  moonbit_string_t _M0L6_2atmpS3315;
  moonbit_string_t* _M0L6_2atmpS3311;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3310;
  struct moonbit_result_0 _tmp_4471;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3728;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3351;
  moonbit_string_t _M0L7_2abindS1401;
  int32_t _M0L6_2atmpS3353;
  struct _M0TPC16string10StringView _M0L6_2atmpS3352;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3350;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3727;
  int32_t _M0L6_2acntS4154;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3347;
  moonbit_string_t _M0L7_2abindS1402;
  int32_t _M0L6_2atmpS3349;
  struct _M0TPC16string10StringView _M0L6_2atmpS3348;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3346;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3726;
  int32_t _M0L6_2acntS4158;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7defaultS3345;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3344;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3725;
  int32_t _M0L6_2acntS4162;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3343;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS3342;
  struct _M0TPB6ToJson _M0L6_2atmpS3330;
  void* _M0L6_2atmpS3341;
  void** _M0L6_2atmpS3340;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3339;
  void* _M0L6_2atmpS3338;
  void* _M0L6_2atmpS3331;
  moonbit_string_t _M0L6_2atmpS3334;
  moonbit_string_t _M0L6_2atmpS3335;
  moonbit_string_t _M0L6_2atmpS3336;
  moonbit_string_t _M0L6_2atmpS3337;
  moonbit_string_t* _M0L6_2atmpS3333;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3332;
  struct moonbit_result_0 _tmp_4473;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3724;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3381;
  moonbit_string_t _M0L7_2abindS1403;
  int32_t _M0L6_2atmpS3383;
  struct _M0TPC16string10StringView _M0L6_2atmpS3382;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3380;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3723;
  int32_t _M0L6_2acntS4166;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3377;
  moonbit_string_t _M0L7_2abindS1404;
  int32_t _M0L6_2atmpS3379;
  struct _M0TPC16string10StringView _M0L6_2atmpS3378;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3376;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3722;
  int32_t _M0L6_2acntS4170;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7defaultS3375;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3374;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3721;
  int32_t _M0L6_2acntS4174;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3371;
  moonbit_string_t _M0L7_2abindS1405;
  int32_t _M0L6_2atmpS3373;
  struct _M0TPC16string10StringView _M0L6_2atmpS3372;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3370;
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS3720;
  int32_t _M0L6_2acntS4178;
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L6routesS3369;
  struct _M0TWEOi* _M0L6_2atmpS3368;
  struct _M0TPB6ToJson _M0L6_2atmpS3356;
  void* _M0L6_2atmpS3367;
  void** _M0L6_2atmpS3366;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3365;
  void* _M0L6_2atmpS3364;
  void* _M0L6_2atmpS3357;
  moonbit_string_t _M0L6_2atmpS3360;
  moonbit_string_t _M0L6_2atmpS3361;
  moonbit_string_t _M0L6_2atmpS3362;
  moonbit_string_t _M0L6_2atmpS3363;
  moonbit_string_t* _M0L6_2atmpS3359;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3358;
  struct moonbit_result_0 _tmp_4475;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3719;
  int32_t _M0L6_2acntS4182;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3412;
  moonbit_string_t _M0L7_2abindS1406;
  int32_t _M0L6_2atmpS3414;
  struct _M0TPC16string10StringView _M0L6_2atmpS3413;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3411;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3718;
  int32_t _M0L6_2acntS4186;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3408;
  moonbit_string_t _M0L7_2abindS1407;
  int32_t _M0L6_2atmpS3410;
  struct _M0TPC16string10StringView _M0L6_2atmpS3409;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3407;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3717;
  int32_t _M0L6_2acntS4190;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7defaultS3406;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3405;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3716;
  int32_t _M0L6_2acntS4194;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3402;
  moonbit_string_t _M0L7_2abindS1408;
  int32_t _M0L6_2atmpS3404;
  struct _M0TPC16string10StringView _M0L6_2atmpS3403;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3401;
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS3715;
  int32_t _M0L6_2acntS4198;
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L6routesS3400;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3399;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS3714;
  int32_t _M0L6_2acntS4202;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L5namesS3398;
  struct _M0TPB6ToJson _M0L6_2atmpS3386;
  void* _M0L6_2atmpS3397;
  void** _M0L6_2atmpS3396;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3395;
  void* _M0L6_2atmpS3394;
  void* _M0L6_2atmpS3387;
  moonbit_string_t _M0L6_2atmpS3390;
  moonbit_string_t _M0L6_2atmpS3391;
  moonbit_string_t _M0L6_2atmpS3392;
  moonbit_string_t _M0L6_2atmpS3393;
  moonbit_string_t* _M0L6_2atmpS3389;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3388;
  #line 209 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3415
  = (struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0C3416l210$closure.data;
  #line 210 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L5routeS1386
  = _M0MP48clawteam8clawteam8internal5httpx5Route3new(_M0L6_2atmpS3415);
  #line 211 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L5layerS1391 = _M0MP48clawteam8clawteam8internal5httpx5Layer3new();
  _M0L7_2abindS1393 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3271 = Moonbit_array_length(_M0L7_2abindS1393);
  _M0L6_2atmpS3264
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3271, _M0L7_2abindS1393
  };
  _M0L7_2abindS1394 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3270 = Moonbit_array_length(_M0L7_2abindS1394);
  _M0L6_2atmpS3265
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3270, _M0L7_2abindS1394
  };
  _M0L7_2abindS1395 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3269 = Moonbit_array_length(_M0L7_2abindS1395);
  _M0L6_2atmpS3266
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3269, _M0L7_2abindS1395
  };
  _M0L7_2abindS1396 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3268 = Moonbit_array_length(_M0L7_2abindS1396);
  _M0L6_2atmpS3267
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3268, _M0L7_2abindS1396
  };
  _M0L7_2abindS1392
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(4, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L7_2abindS1392[0] = _M0L6_2atmpS3264;
  _M0L7_2abindS1392[1] = _M0L6_2atmpS3265;
  _M0L7_2abindS1392[2] = _M0L6_2atmpS3266;
  _M0L7_2abindS1392[3] = _M0L6_2atmpS3267;
  _M0L6_2atmpS3263 = _M0L7_2abindS1392;
  _M0L6_2atmpS3262
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, 4, _M0L6_2atmpS3263
  };
  moonbit_incref(_M0L5layerS1391);
  #line 212 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(_M0L5layerS1391, 0, _M0L6_2atmpS3262, _M0L5routeS1386);
  _M0L8_2afieldS3735 = _M0L5layerS1391->$1;
  _M0L7matchesS3285 = _M0L8_2afieldS3735;
  moonbit_incref(_M0L7matchesS3285);
  #line 213 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3284
  = _M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3285);
  _M0L6_2atmpS3272
  = (struct _M0TPB6ToJson){
    _M0FP0156moonbitlang_2fcore_2fbuiltin_2fIter_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3284
  };
  #line 213 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3283
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L6_2atmpS3282 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3282[0] = _M0L6_2atmpS3283;
  _M0L6_2atmpS3281
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3281)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3281->$0 = _M0L6_2atmpS3282;
  _M0L6_2atmpS3281->$1 = 1;
  #line 213 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3280 = _M0MPC14json4Json5array(_M0L6_2atmpS3281);
  _M0L6_2atmpS3273 = _M0L6_2atmpS3280;
  _M0L6_2atmpS3276 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3277 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3278 = 0;
  _M0L6_2atmpS3279 = 0;
  _M0L6_2atmpS3275 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3275[0] = _M0L6_2atmpS3276;
  _M0L6_2atmpS3275[1] = _M0L6_2atmpS3277;
  _M0L6_2atmpS3275[2] = _M0L6_2atmpS3278;
  _M0L6_2atmpS3275[3] = _M0L6_2atmpS3279;
  _M0L6_2atmpS3274
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3274)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3274->$0 = _M0L6_2atmpS3275;
  _M0L6_2atmpS3274->$1 = 4;
  #line 213 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _tmp_4467
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3272, _M0L6_2atmpS3273, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS3274);
  if (_tmp_4467.tag) {
    int32_t const _M0L5_2aokS3286 = _tmp_4467.data.ok;
  } else {
    void* const _M0L6_2aerrS3287 = _tmp_4467.data.err;
    struct moonbit_result_0 _result_4468;
    moonbit_decref(_M0L5layerS1391);
    _result_4468.tag = 0;
    _result_4468.data.err = _M0L6_2aerrS3287;
    return _result_4468;
  }
  _M0L8_2afieldS3734 = _M0L5layerS1391->$1;
  _M0L7matchesS3303 = _M0L8_2afieldS3734;
  _M0L7_2abindS1397 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3305 = Moonbit_array_length(_M0L7_2abindS1397);
  _M0L6_2atmpS3304
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3305, _M0L7_2abindS1397
  };
  moonbit_incref(_M0L7matchesS3303);
  #line 214 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3302
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3303, _M0L6_2atmpS3304);
  _M0L8_2afieldS3733 = _M0L6_2atmpS3302->$1;
  _M0L6_2acntS4142 = Moonbit_object_header(_M0L6_2atmpS3302)->rc;
  if (_M0L6_2acntS4142 > 1) {
    int32_t _M0L11_2anew__cntS4145 = _M0L6_2acntS4142 - 1;
    Moonbit_object_header(_M0L6_2atmpS3302)->rc = _M0L11_2anew__cntS4145;
    moonbit_incref(_M0L8_2afieldS3733);
  } else if (_M0L6_2acntS4142 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4144 =
      _M0L6_2atmpS3302->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4143;
    if (_M0L8_2afieldS4144) {
      moonbit_decref(_M0L8_2afieldS4144);
    }
    _M0L8_2afieldS4143 = _M0L6_2atmpS3302->$0;
    moonbit_decref(_M0L8_2afieldS4143);
    #line 214 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3302);
  }
  _M0L7matchesS3301 = _M0L8_2afieldS3733;
  #line 214 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3300
  = _M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3301);
  _M0L6_2atmpS3288
  = (struct _M0TPB6ToJson){
    _M0FP0156moonbitlang_2fcore_2fbuiltin_2fIter_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3300
  };
  #line 214 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3299
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L6_2atmpS3298 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3298[0] = _M0L6_2atmpS3299;
  _M0L6_2atmpS3297
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3297)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3297->$0 = _M0L6_2atmpS3298;
  _M0L6_2atmpS3297->$1 = 1;
  #line 214 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3296 = _M0MPC14json4Json5array(_M0L6_2atmpS3297);
  _M0L6_2atmpS3289 = _M0L6_2atmpS3296;
  _M0L6_2atmpS3292 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3293 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3294 = 0;
  _M0L6_2atmpS3295 = 0;
  _M0L6_2atmpS3291 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3291[0] = _M0L6_2atmpS3292;
  _M0L6_2atmpS3291[1] = _M0L6_2atmpS3293;
  _M0L6_2atmpS3291[2] = _M0L6_2atmpS3294;
  _M0L6_2atmpS3291[3] = _M0L6_2atmpS3295;
  _M0L6_2atmpS3290
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3290)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3290->$0 = _M0L6_2atmpS3291;
  _M0L6_2atmpS3290->$1 = 4;
  #line 214 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _tmp_4469
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3288, _M0L6_2atmpS3289, (moonbit_string_t)moonbit_string_literal_30.data, _M0L6_2atmpS3290);
  if (_tmp_4469.tag) {
    int32_t const _M0L5_2aokS3306 = _tmp_4469.data.ok;
  } else {
    void* const _M0L6_2aerrS3307 = _tmp_4469.data.err;
    struct moonbit_result_0 _result_4470;
    moonbit_decref(_M0L5layerS1391);
    _result_4470.tag = 0;
    _result_4470.data.err = _M0L6_2aerrS3307;
    return _result_4470;
  }
  _M0L8_2afieldS3732 = _M0L5layerS1391->$1;
  _M0L7matchesS3325 = _M0L8_2afieldS3732;
  _M0L7_2abindS1399 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3327 = Moonbit_array_length(_M0L7_2abindS1399);
  _M0L6_2atmpS3326
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3327, _M0L7_2abindS1399
  };
  moonbit_incref(_M0L7matchesS3325);
  #line 216 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3324
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3325, _M0L6_2atmpS3326);
  _M0L8_2afieldS3731 = _M0L6_2atmpS3324->$1;
  _M0L6_2acntS4146 = Moonbit_object_header(_M0L6_2atmpS3324)->rc;
  if (_M0L6_2acntS4146 > 1) {
    int32_t _M0L11_2anew__cntS4149 = _M0L6_2acntS4146 - 1;
    Moonbit_object_header(_M0L6_2atmpS3324)->rc = _M0L11_2anew__cntS4149;
    moonbit_incref(_M0L8_2afieldS3731);
  } else if (_M0L6_2acntS4146 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4148 =
      _M0L6_2atmpS3324->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4147;
    if (_M0L8_2afieldS4148) {
      moonbit_decref(_M0L8_2afieldS4148);
    }
    _M0L8_2afieldS4147 = _M0L6_2atmpS3324->$0;
    moonbit_decref(_M0L8_2afieldS4147);
    #line 216 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3324);
  }
  _M0L7matchesS3321 = _M0L8_2afieldS3731;
  _M0L7_2abindS1400 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3323 = Moonbit_array_length(_M0L7_2abindS1400);
  _M0L6_2atmpS3322
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3323, _M0L7_2abindS1400
  };
  #line 216 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3320
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3321, _M0L6_2atmpS3322);
  _M0L8_2afieldS3730 = _M0L6_2atmpS3320->$2;
  _M0L6_2acntS4150 = Moonbit_object_header(_M0L6_2atmpS3320)->rc;
  if (_M0L6_2acntS4150 > 1) {
    int32_t _M0L11_2anew__cntS4153 = _M0L6_2acntS4150 - 1;
    Moonbit_object_header(_M0L6_2atmpS3320)->rc = _M0L11_2anew__cntS4153;
    if (_M0L8_2afieldS3730) {
      moonbit_incref(_M0L8_2afieldS3730);
    }
  } else if (_M0L6_2acntS4150 == 1) {
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4152 =
      _M0L6_2atmpS3320->$1;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4151;
    moonbit_decref(_M0L8_2afieldS4152);
    _M0L8_2afieldS4151 = _M0L6_2atmpS3320->$0;
    moonbit_decref(_M0L8_2afieldS4151);
    #line 216 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3320);
  }
  _M0L7_2abindS1398 = _M0L8_2afieldS3730;
  _M0L6_2atmpS3729 = _M0L7_2abindS1398 == 0;
  if (_M0L7_2abindS1398) {
    moonbit_decref(_M0L7_2abindS1398);
  }
  _M0L6_2atmpS3319 = _M0L6_2atmpS3729;
  _M0L6_2atmpS3317 = !_M0L6_2atmpS3319;
  _M0L14_2aboxed__selfS3318
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3318)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3318->$0 = _M0L6_2atmpS3317;
  _M0L6_2atmpS3308
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3318
  };
  #line 217 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3316 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3309 = _M0L6_2atmpS3316;
  _M0L6_2atmpS3312 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3313 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS3314 = 0;
  _M0L6_2atmpS3315 = 0;
  _M0L6_2atmpS3311 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3311[0] = _M0L6_2atmpS3312;
  _M0L6_2atmpS3311[1] = _M0L6_2atmpS3313;
  _M0L6_2atmpS3311[2] = _M0L6_2atmpS3314;
  _M0L6_2atmpS3311[3] = _M0L6_2atmpS3315;
  _M0L6_2atmpS3310
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3310)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3310->$0 = _M0L6_2atmpS3311;
  _M0L6_2atmpS3310->$1 = 4;
  #line 215 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _tmp_4471
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3308, _M0L6_2atmpS3309, (moonbit_string_t)moonbit_string_literal_33.data, _M0L6_2atmpS3310);
  if (_tmp_4471.tag) {
    int32_t const _M0L5_2aokS3328 = _tmp_4471.data.ok;
  } else {
    void* const _M0L6_2aerrS3329 = _tmp_4471.data.err;
    struct moonbit_result_0 _result_4472;
    moonbit_decref(_M0L5layerS1391);
    _result_4472.tag = 0;
    _result_4472.data.err = _M0L6_2aerrS3329;
    return _result_4472;
  }
  _M0L8_2afieldS3728 = _M0L5layerS1391->$1;
  _M0L7matchesS3351 = _M0L8_2afieldS3728;
  _M0L7_2abindS1401 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3353 = Moonbit_array_length(_M0L7_2abindS1401);
  _M0L6_2atmpS3352
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3353, _M0L7_2abindS1401
  };
  moonbit_incref(_M0L7matchesS3351);
  #line 220 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3350
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3351, _M0L6_2atmpS3352);
  _M0L8_2afieldS3727 = _M0L6_2atmpS3350->$1;
  _M0L6_2acntS4154 = Moonbit_object_header(_M0L6_2atmpS3350)->rc;
  if (_M0L6_2acntS4154 > 1) {
    int32_t _M0L11_2anew__cntS4157 = _M0L6_2acntS4154 - 1;
    Moonbit_object_header(_M0L6_2atmpS3350)->rc = _M0L11_2anew__cntS4157;
    moonbit_incref(_M0L8_2afieldS3727);
  } else if (_M0L6_2acntS4154 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4156 =
      _M0L6_2atmpS3350->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4155;
    if (_M0L8_2afieldS4156) {
      moonbit_decref(_M0L8_2afieldS4156);
    }
    _M0L8_2afieldS4155 = _M0L6_2atmpS3350->$0;
    moonbit_decref(_M0L8_2afieldS4155);
    #line 220 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3350);
  }
  _M0L7matchesS3347 = _M0L8_2afieldS3727;
  _M0L7_2abindS1402 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3349 = Moonbit_array_length(_M0L7_2abindS1402);
  _M0L6_2atmpS3348
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3349, _M0L7_2abindS1402
  };
  #line 220 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3346
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3347, _M0L6_2atmpS3348);
  _M0L8_2afieldS3726 = _M0L6_2atmpS3346->$2;
  _M0L6_2acntS4158 = Moonbit_object_header(_M0L6_2atmpS3346)->rc;
  if (_M0L6_2acntS4158 > 1) {
    int32_t _M0L11_2anew__cntS4161 = _M0L6_2acntS4158 - 1;
    Moonbit_object_header(_M0L6_2atmpS3346)->rc = _M0L11_2anew__cntS4161;
    if (_M0L8_2afieldS3726) {
      moonbit_incref(_M0L8_2afieldS3726);
    }
  } else if (_M0L6_2acntS4158 == 1) {
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4160 =
      _M0L6_2atmpS3346->$1;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4159;
    moonbit_decref(_M0L8_2afieldS4160);
    _M0L8_2afieldS4159 = _M0L6_2atmpS3346->$0;
    moonbit_decref(_M0L8_2afieldS4159);
    #line 220 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3346);
  }
  _M0L7defaultS3345 = _M0L8_2afieldS3726;
  #line 220 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3344
  = _M0MPC16option6Option6unwrapGRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7defaultS3345);
  _M0L8_2afieldS3725 = _M0L6_2atmpS3344->$1;
  _M0L6_2acntS4162 = Moonbit_object_header(_M0L6_2atmpS3344)->rc;
  if (_M0L6_2acntS4162 > 1) {
    int32_t _M0L11_2anew__cntS4165 = _M0L6_2acntS4162 - 1;
    Moonbit_object_header(_M0L6_2atmpS3344)->rc = _M0L11_2anew__cntS4165;
    moonbit_incref(_M0L8_2afieldS3725);
  } else if (_M0L6_2acntS4162 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4164 =
      _M0L6_2atmpS3344->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4163;
    if (_M0L8_2afieldS4164) {
      moonbit_decref(_M0L8_2afieldS4164);
    }
    _M0L8_2afieldS4163 = _M0L6_2atmpS3344->$0;
    moonbit_decref(_M0L8_2afieldS4163);
    #line 220 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3344);
  }
  _M0L7matchesS3343 = _M0L8_2afieldS3725;
  #line 220 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3342
  = _M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3343);
  _M0L6_2atmpS3330
  = (struct _M0TPB6ToJson){
    _M0FP0156moonbitlang_2fcore_2fbuiltin_2fIter_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3342
  };
  #line 221 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3341
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L6_2atmpS3340 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3340[0] = _M0L6_2atmpS3341;
  _M0L6_2atmpS3339
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3339)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3339->$0 = _M0L6_2atmpS3340;
  _M0L6_2atmpS3339->$1 = 1;
  #line 221 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3338 = _M0MPC14json4Json5array(_M0L6_2atmpS3339);
  _M0L6_2atmpS3331 = _M0L6_2atmpS3338;
  _M0L6_2atmpS3334 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3335 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS3336 = 0;
  _M0L6_2atmpS3337 = 0;
  _M0L6_2atmpS3333 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3333[0] = _M0L6_2atmpS3334;
  _M0L6_2atmpS3333[1] = _M0L6_2atmpS3335;
  _M0L6_2atmpS3333[2] = _M0L6_2atmpS3336;
  _M0L6_2atmpS3333[3] = _M0L6_2atmpS3337;
  _M0L6_2atmpS3332
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3332)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3332->$0 = _M0L6_2atmpS3333;
  _M0L6_2atmpS3332->$1 = 4;
  #line 219 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _tmp_4473
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3330, _M0L6_2atmpS3331, (moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS3332);
  if (_tmp_4473.tag) {
    int32_t const _M0L5_2aokS3354 = _tmp_4473.data.ok;
  } else {
    void* const _M0L6_2aerrS3355 = _tmp_4473.data.err;
    struct moonbit_result_0 _result_4474;
    moonbit_decref(_M0L5layerS1391);
    _result_4474.tag = 0;
    _result_4474.data.err = _M0L6_2aerrS3355;
    return _result_4474;
  }
  _M0L8_2afieldS3724 = _M0L5layerS1391->$1;
  _M0L7matchesS3381 = _M0L8_2afieldS3724;
  _M0L7_2abindS1403 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3383 = Moonbit_array_length(_M0L7_2abindS1403);
  _M0L6_2atmpS3382
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3383, _M0L7_2abindS1403
  };
  moonbit_incref(_M0L7matchesS3381);
  #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3380
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3381, _M0L6_2atmpS3382);
  _M0L8_2afieldS3723 = _M0L6_2atmpS3380->$1;
  _M0L6_2acntS4166 = Moonbit_object_header(_M0L6_2atmpS3380)->rc;
  if (_M0L6_2acntS4166 > 1) {
    int32_t _M0L11_2anew__cntS4169 = _M0L6_2acntS4166 - 1;
    Moonbit_object_header(_M0L6_2atmpS3380)->rc = _M0L11_2anew__cntS4169;
    moonbit_incref(_M0L8_2afieldS3723);
  } else if (_M0L6_2acntS4166 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4168 =
      _M0L6_2atmpS3380->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4167;
    if (_M0L8_2afieldS4168) {
      moonbit_decref(_M0L8_2afieldS4168);
    }
    _M0L8_2afieldS4167 = _M0L6_2atmpS3380->$0;
    moonbit_decref(_M0L8_2afieldS4167);
    #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3380);
  }
  _M0L7matchesS3377 = _M0L8_2afieldS3723;
  _M0L7_2abindS1404 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3379 = Moonbit_array_length(_M0L7_2abindS1404);
  _M0L6_2atmpS3378
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3379, _M0L7_2abindS1404
  };
  #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3376
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3377, _M0L6_2atmpS3378);
  _M0L8_2afieldS3722 = _M0L6_2atmpS3376->$2;
  _M0L6_2acntS4170 = Moonbit_object_header(_M0L6_2atmpS3376)->rc;
  if (_M0L6_2acntS4170 > 1) {
    int32_t _M0L11_2anew__cntS4173 = _M0L6_2acntS4170 - 1;
    Moonbit_object_header(_M0L6_2atmpS3376)->rc = _M0L11_2anew__cntS4173;
    if (_M0L8_2afieldS3722) {
      moonbit_incref(_M0L8_2afieldS3722);
    }
  } else if (_M0L6_2acntS4170 == 1) {
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4172 =
      _M0L6_2atmpS3376->$1;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4171;
    moonbit_decref(_M0L8_2afieldS4172);
    _M0L8_2afieldS4171 = _M0L6_2atmpS3376->$0;
    moonbit_decref(_M0L8_2afieldS4171);
    #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3376);
  }
  _M0L7defaultS3375 = _M0L8_2afieldS3722;
  #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3374
  = _M0MPC16option6Option6unwrapGRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7defaultS3375);
  _M0L8_2afieldS3721 = _M0L6_2atmpS3374->$1;
  _M0L6_2acntS4174 = Moonbit_object_header(_M0L6_2atmpS3374)->rc;
  if (_M0L6_2acntS4174 > 1) {
    int32_t _M0L11_2anew__cntS4177 = _M0L6_2acntS4174 - 1;
    Moonbit_object_header(_M0L6_2atmpS3374)->rc = _M0L11_2anew__cntS4177;
    moonbit_incref(_M0L8_2afieldS3721);
  } else if (_M0L6_2acntS4174 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4176 =
      _M0L6_2atmpS3374->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4175;
    if (_M0L8_2afieldS4176) {
      moonbit_decref(_M0L8_2afieldS4176);
    }
    _M0L8_2afieldS4175 = _M0L6_2atmpS3374->$0;
    moonbit_decref(_M0L8_2afieldS4175);
    #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3374);
  }
  _M0L7matchesS3371 = _M0L8_2afieldS3721;
  _M0L7_2abindS1405 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3373 = Moonbit_array_length(_M0L7_2abindS1405);
  _M0L6_2atmpS3372
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3373, _M0L7_2abindS1405
  };
  #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3370
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3371, _M0L6_2atmpS3372);
  _M0L8_2afieldS3720 = _M0L6_2atmpS3370->$0;
  _M0L6_2acntS4178 = Moonbit_object_header(_M0L6_2atmpS3370)->rc;
  if (_M0L6_2acntS4178 > 1) {
    int32_t _M0L11_2anew__cntS4181 = _M0L6_2acntS4178 - 1;
    Moonbit_object_header(_M0L6_2atmpS3370)->rc = _M0L11_2anew__cntS4181;
    moonbit_incref(_M0L8_2afieldS3720);
  } else if (_M0L6_2acntS4178 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4180 =
      _M0L6_2atmpS3370->$2;
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4179;
    if (_M0L8_2afieldS4180) {
      moonbit_decref(_M0L8_2afieldS4180);
    }
    _M0L8_2afieldS4179 = _M0L6_2atmpS3370->$1;
    moonbit_decref(_M0L8_2afieldS4179);
    #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3370);
  }
  _M0L6routesS3369 = _M0L8_2afieldS3720;
  #line 224 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3368
  = _M0MP48clawteam8clawteam8internal5httpx9MethodMap4keysGRP48clawteam8clawteam8internal5httpx5RouteE(_M0L6routesS3369);
  _M0L6_2atmpS3356
  = (struct _M0TPB6ToJson){
    _M0FP0163moonbitlang_2fcore_2fbuiltin_2fIter_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3368
  };
  #line 225 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3367
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_37.data);
  _M0L6_2atmpS3366 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3366[0] = _M0L6_2atmpS3367;
  _M0L6_2atmpS3365
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3365)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3365->$0 = _M0L6_2atmpS3366;
  _M0L6_2atmpS3365->$1 = 1;
  #line 225 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3364 = _M0MPC14json4Json5array(_M0L6_2atmpS3365);
  _M0L6_2atmpS3357 = _M0L6_2atmpS3364;
  _M0L6_2atmpS3360 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3361 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3362 = 0;
  _M0L6_2atmpS3363 = 0;
  _M0L6_2atmpS3359 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3359[0] = _M0L6_2atmpS3360;
  _M0L6_2atmpS3359[1] = _M0L6_2atmpS3361;
  _M0L6_2atmpS3359[2] = _M0L6_2atmpS3362;
  _M0L6_2atmpS3359[3] = _M0L6_2atmpS3363;
  _M0L6_2atmpS3358
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3358)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3358->$0 = _M0L6_2atmpS3359;
  _M0L6_2atmpS3358->$1 = 4;
  #line 223 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _tmp_4475
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3356, _M0L6_2atmpS3357, (moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS3358);
  if (_tmp_4475.tag) {
    int32_t const _M0L5_2aokS3384 = _tmp_4475.data.ok;
  } else {
    void* const _M0L6_2aerrS3385 = _tmp_4475.data.err;
    struct moonbit_result_0 _result_4476;
    moonbit_decref(_M0L5layerS1391);
    _result_4476.tag = 0;
    _result_4476.data.err = _M0L6_2aerrS3385;
    return _result_4476;
  }
  _M0L8_2afieldS3719 = _M0L5layerS1391->$1;
  _M0L6_2acntS4182 = Moonbit_object_header(_M0L5layerS1391)->rc;
  if (_M0L6_2acntS4182 > 1) {
    int32_t _M0L11_2anew__cntS4185 = _M0L6_2acntS4182 - 1;
    Moonbit_object_header(_M0L5layerS1391)->rc = _M0L11_2anew__cntS4185;
    moonbit_incref(_M0L8_2afieldS3719);
  } else if (_M0L6_2acntS4182 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4184 =
      _M0L5layerS1391->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4183;
    if (_M0L8_2afieldS4184) {
      moonbit_decref(_M0L8_2afieldS4184);
    }
    _M0L8_2afieldS4183 = _M0L5layerS1391->$0;
    moonbit_decref(_M0L8_2afieldS4183);
    #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L5layerS1391);
  }
  _M0L7matchesS3412 = _M0L8_2afieldS3719;
  _M0L7_2abindS1406 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3414 = Moonbit_array_length(_M0L7_2abindS1406);
  _M0L6_2atmpS3413
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3414, _M0L7_2abindS1406
  };
  #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3411
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3412, _M0L6_2atmpS3413);
  _M0L8_2afieldS3718 = _M0L6_2atmpS3411->$1;
  _M0L6_2acntS4186 = Moonbit_object_header(_M0L6_2atmpS3411)->rc;
  if (_M0L6_2acntS4186 > 1) {
    int32_t _M0L11_2anew__cntS4189 = _M0L6_2acntS4186 - 1;
    Moonbit_object_header(_M0L6_2atmpS3411)->rc = _M0L11_2anew__cntS4189;
    moonbit_incref(_M0L8_2afieldS3718);
  } else if (_M0L6_2acntS4186 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4188 =
      _M0L6_2atmpS3411->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4187;
    if (_M0L8_2afieldS4188) {
      moonbit_decref(_M0L8_2afieldS4188);
    }
    _M0L8_2afieldS4187 = _M0L6_2atmpS3411->$0;
    moonbit_decref(_M0L8_2afieldS4187);
    #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3411);
  }
  _M0L7matchesS3408 = _M0L8_2afieldS3718;
  _M0L7_2abindS1407 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3410 = Moonbit_array_length(_M0L7_2abindS1407);
  _M0L6_2atmpS3409
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3410, _M0L7_2abindS1407
  };
  #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3407
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3408, _M0L6_2atmpS3409);
  _M0L8_2afieldS3717 = _M0L6_2atmpS3407->$2;
  _M0L6_2acntS4190 = Moonbit_object_header(_M0L6_2atmpS3407)->rc;
  if (_M0L6_2acntS4190 > 1) {
    int32_t _M0L11_2anew__cntS4193 = _M0L6_2acntS4190 - 1;
    Moonbit_object_header(_M0L6_2atmpS3407)->rc = _M0L11_2anew__cntS4193;
    if (_M0L8_2afieldS3717) {
      moonbit_incref(_M0L8_2afieldS3717);
    }
  } else if (_M0L6_2acntS4190 == 1) {
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4192 =
      _M0L6_2atmpS3407->$1;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4191;
    moonbit_decref(_M0L8_2afieldS4192);
    _M0L8_2afieldS4191 = _M0L6_2atmpS3407->$0;
    moonbit_decref(_M0L8_2afieldS4191);
    #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3407);
  }
  _M0L7defaultS3406 = _M0L8_2afieldS3717;
  #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3405
  = _M0MPC16option6Option6unwrapGRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7defaultS3406);
  _M0L8_2afieldS3716 = _M0L6_2atmpS3405->$1;
  _M0L6_2acntS4194 = Moonbit_object_header(_M0L6_2atmpS3405)->rc;
  if (_M0L6_2acntS4194 > 1) {
    int32_t _M0L11_2anew__cntS4197 = _M0L6_2acntS4194 - 1;
    Moonbit_object_header(_M0L6_2atmpS3405)->rc = _M0L11_2anew__cntS4197;
    moonbit_incref(_M0L8_2afieldS3716);
  } else if (_M0L6_2acntS4194 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4196 =
      _M0L6_2atmpS3405->$2;
    struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4195;
    if (_M0L8_2afieldS4196) {
      moonbit_decref(_M0L8_2afieldS4196);
    }
    _M0L8_2afieldS4195 = _M0L6_2atmpS3405->$0;
    moonbit_decref(_M0L8_2afieldS4195);
    #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3405);
  }
  _M0L7matchesS3402 = _M0L8_2afieldS3716;
  _M0L7_2abindS1408 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3404 = Moonbit_array_length(_M0L7_2abindS1408);
  _M0L6_2atmpS3403
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3404, _M0L7_2abindS1408
  };
  #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3401
  = _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3402, _M0L6_2atmpS3403);
  _M0L8_2afieldS3715 = _M0L6_2atmpS3401->$0;
  _M0L6_2acntS4198 = Moonbit_object_header(_M0L6_2atmpS3401)->rc;
  if (_M0L6_2acntS4198 > 1) {
    int32_t _M0L11_2anew__cntS4201 = _M0L6_2acntS4198 - 1;
    Moonbit_object_header(_M0L6_2atmpS3401)->rc = _M0L11_2anew__cntS4201;
    moonbit_incref(_M0L8_2afieldS3715);
  } else if (_M0L6_2acntS4198 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4200 =
      _M0L6_2atmpS3401->$2;
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4199;
    if (_M0L8_2afieldS4200) {
      moonbit_decref(_M0L8_2afieldS4200);
    }
    _M0L8_2afieldS4199 = _M0L6_2atmpS3401->$1;
    moonbit_decref(_M0L8_2afieldS4199);
    #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3401);
  }
  _M0L6routesS3400 = _M0L8_2afieldS3715;
  #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3399
  = _M0MP48clawteam8clawteam8internal5httpx9MethodMap7op__getGRP48clawteam8clawteam8internal5httpx5RouteE(_M0L6routesS3400, 0);
  _M0L8_2afieldS3714 = _M0L6_2atmpS3399->$1;
  _M0L6_2acntS4202 = Moonbit_object_header(_M0L6_2atmpS3399)->rc;
  if (_M0L6_2acntS4202 > 1) {
    int32_t _M0L11_2anew__cntS4204 = _M0L6_2acntS4202 - 1;
    Moonbit_object_header(_M0L6_2atmpS3399)->rc = _M0L11_2anew__cntS4204;
    moonbit_incref(_M0L8_2afieldS3714);
  } else if (_M0L6_2acntS4202 == 1) {
    struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error* _M0L8_2afieldS4203 =
      _M0L6_2atmpS3399->$0;
    moonbit_decref(_M0L8_2afieldS4203);
    #line 228 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    moonbit_free(_M0L6_2atmpS3399);
  }
  _M0L5namesS3398 = _M0L8_2afieldS3714;
  _M0L6_2atmpS3386
  = (struct _M0TPB6ToJson){
    _M0FP0157moonbitlang_2fcore_2fbuiltin_2fArray_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L5namesS3398
  };
  #line 229 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3397
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_41.data);
  _M0L6_2atmpS3396 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3396[0] = _M0L6_2atmpS3397;
  _M0L6_2atmpS3395
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3395)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3395->$0 = _M0L6_2atmpS3396;
  _M0L6_2atmpS3395->$1 = 1;
  #line 229 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3394 = _M0MPC14json4Json5array(_M0L6_2atmpS3395);
  _M0L6_2atmpS3387 = _M0L6_2atmpS3394;
  _M0L6_2atmpS3390 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3391 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3392 = 0;
  _M0L6_2atmpS3393 = 0;
  _M0L6_2atmpS3389 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3389[0] = _M0L6_2atmpS3390;
  _M0L6_2atmpS3389[1] = _M0L6_2atmpS3391;
  _M0L6_2atmpS3389[2] = _M0L6_2atmpS3392;
  _M0L6_2atmpS3389[3] = _M0L6_2atmpS3393;
  _M0L6_2atmpS3388
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3388)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3388->$0 = _M0L6_2atmpS3389;
  _M0L6_2atmpS3388->$1 = 4;
  #line 227 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3386, _M0L6_2atmpS3387, (moonbit_string_t)moonbit_string_literal_44.data, _M0L6_2atmpS3388);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal5httpx33____test__726f757465722e6d6274__0C3416l210(
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error* _M0L6_2aenvS3417,
  struct _M0TP48clawteam8clawteam8internal5httpx13RequestReader* _M0L12_2adiscard__S1387,
  struct _M0TP48clawteam8clawteam8internal5httpx14ResponseWriter* _M0L12_2adiscard__S1388,
  struct _M0TWiEu* _M0L7_2acontS1389,
  struct _M0TWRPC15error5ErrorEu* _M0L12_2aerr__contS1390
) {
  int32_t _M0L6_2atmpS3419;
  int32_t _M0L6_2atmpS3418;
  struct moonbit_result_0 _result_4477;
  #line 210 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  moonbit_decref(_M0L12_2aerr__contS1390);
  moonbit_decref(_M0L7_2acontS1389);
  moonbit_decref(_M0L12_2adiscard__S1388);
  moonbit_decref(_M0L12_2adiscard__S1387);
  moonbit_decref(_M0L6_2aenvS3417);
  _M0L6_2atmpS3419 = 0;
  _M0L6_2atmpS3418 = _M0L6_2atmpS3419;
  _result_4477.tag = 1;
  _result_4477.data.ok = _M0L6_2atmpS3418;
  return _result_4477;
}

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0MP48clawteam8clawteam8internal5httpx9MethodMap7op__getGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L4selfS1382,
  int32_t _M0L8method__S1383
) {
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L5valueS1380;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L7_2abindS1381;
  #line 30 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  #line 31 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  _M0L7_2abindS1381
  = _M0MP48clawteam8clawteam8internal5httpx9MethodMap3getGRP48clawteam8clawteam8internal5httpx5RouteE(_M0L4selfS1382, _M0L8method__S1383);
  if (_M0L7_2abindS1381 == 0) {
    moonbit_string_t _M0L6_2atmpS3261;
    moonbit_string_t _M0L6_2atmpS3736;
    moonbit_string_t _M0L6_2atmpS3260;
    if (_M0L7_2abindS1381) {
      moonbit_decref(_M0L7_2abindS1381);
    }
    #line 32 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
    _M0L6_2atmpS3261
    = _M0IP48clawteam8clawteam8internal5httpx6MethodPB4Show10to__string(_M0L8method__S1383);
    #line 32 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
    _M0L6_2atmpS3736
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_45.data, _M0L6_2atmpS3261);
    moonbit_decref(_M0L6_2atmpS3261);
    _M0L6_2atmpS3260 = _M0L6_2atmpS3736;
    #line 32 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
    return _M0FPB5abortGRP48clawteam8clawteam8internal5httpx5RouteE(_M0L6_2atmpS3260, (moonbit_string_t)moonbit_string_literal_46.data);
  } else {
    struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L7_2aSomeS1384 =
      _M0L7_2abindS1381;
    struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2avalueS1385 =
      _M0L7_2aSomeS1384;
    _M0L5valueS1380 = _M0L8_2avalueS1385;
    goto join_1379;
  }
  join_1379:;
  return _M0L5valueS1380;
}

moonbit_string_t _M0IP48clawteam8clawteam8internal5httpx6MethodPB4Show10to__string(
  int32_t _M0L4selfS1378
) {
  #line 16 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
  switch (_M0L4selfS1378) {
    case 0: {
      return (moonbit_string_t)moonbit_string_literal_47.data;
      break;
    }
    
    case 1: {
      return (moonbit_string_t)moonbit_string_literal_48.data;
      break;
    }
    
    case 2: {
      return (moonbit_string_t)moonbit_string_literal_49.data;
      break;
    }
    
    case 3: {
      return (moonbit_string_t)moonbit_string_literal_50.data;
      break;
    }
    
    case 4: {
      return (moonbit_string_t)moonbit_string_literal_51.data;
      break;
    }
    
    case 5: {
      return (moonbit_string_t)moonbit_string_literal_52.data;
      break;
    }
    
    case 6: {
      return (moonbit_string_t)moonbit_string_literal_53.data;
      break;
    }
    
    case 7: {
      return (moonbit_string_t)moonbit_string_literal_54.data;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_55.data;
      break;
    }
  }
}

struct _M0TWEOi* _M0MP48clawteam8clawteam8internal5httpx9MethodMap4keysGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L4selfS1376
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS1373;
  struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__* _closure_4479;
  struct _M0TWEOi* _M0L6_2atmpS3250;
  #line 72 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  _M0L1iS1373
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1373)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1373->$0 = 0;
  _closure_4479
  = (struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__*)moonbit_malloc(sizeof(struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__));
  Moonbit_object_header(_closure_4479)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__, $0) >> 2, 2, 0);
  _closure_4479->code
  = &_M0MP48clawteam8clawteam8internal5httpx9MethodMap4keysGRP48clawteam8clawteam8internal5httpx5RouteEC3251l74;
  _closure_4479->$0 = _M0L4selfS1376;
  _closure_4479->$1 = _M0L1iS1373;
  _M0L6_2atmpS3250 = (struct _M0TWEOi*)_closure_4479;
  #line 74 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  return _M0MPB4Iter3newGRP48clawteam8clawteam8internal5httpx6MethodE(_M0L6_2atmpS3250);
}

int64_t _M0MP48clawteam8clawteam8internal5httpx9MethodMap4keysGRP48clawteam8clawteam8internal5httpx5RouteEC3251l74(
  struct _M0TWEOi* _M0L6_2aenvS3252
) {
  struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__* _M0L14_2acasted__envS3253;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3739;
  struct _M0TPC13ref3RefGiE* _M0L1iS1373;
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS3738;
  int32_t _M0L6_2acntS4205;
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L4selfS1376;
  #line 74 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  _M0L14_2acasted__envS3253
  = (struct _M0R142_40clawteam_2fclawteam_2finternal_2fhttpx_2eMethodMap_3a_3akeys_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fRoute_5d_7c_2eanon__u3251__l74__*)_M0L6_2aenvS3252;
  _M0L8_2afieldS3739 = _M0L14_2acasted__envS3253->$1;
  _M0L1iS1373 = _M0L8_2afieldS3739;
  _M0L8_2afieldS3738 = _M0L14_2acasted__envS3253->$0;
  _M0L6_2acntS4205 = Moonbit_object_header(_M0L14_2acasted__envS3253)->rc;
  if (_M0L6_2acntS4205 > 1) {
    int32_t _M0L11_2anew__cntS4206 = _M0L6_2acntS4205 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3253)->rc
    = _M0L11_2anew__cntS4206;
    moonbit_incref(_M0L1iS1373);
    moonbit_incref(_M0L8_2afieldS3738);
  } else if (_M0L6_2acntS4205 == 1) {
    #line 74 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
    moonbit_free(_M0L14_2acasted__envS3253);
  }
  _M0L4selfS1376 = _M0L8_2afieldS3738;
  while (1) {
    int32_t _M0L3valS3254 = _M0L1iS1373->$0;
    int32_t _M0L6_2atmpS3255;
    moonbit_incref(_M0FP48clawteam8clawteam8internal5httpx7methods);
    #line 75 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
    _M0L6_2atmpS3255
    = _M0MPC15array5Array6lengthGRP48clawteam8clawteam8internal5httpx6MethodE(_M0FP48clawteam8clawteam8internal5httpx7methods);
    if (_M0L3valS3254 < _M0L6_2atmpS3255) {
      int32_t _M0L3valS3259 = _M0L1iS1373->$0;
      int32_t _M0L8method__S1374;
      int32_t _M0L3valS3257;
      int32_t _M0L6_2atmpS3256;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L7_2abindS1375;
      int32_t _M0L6_2atmpS3737;
      moonbit_incref(_M0FP48clawteam8clawteam8internal5httpx7methods);
      #line 76 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
      _M0L8method__S1374
      = _M0MPC15array5Array2atGRP48clawteam8clawteam8internal5httpx6MethodE(_M0FP48clawteam8clawteam8internal5httpx7methods, _M0L3valS3259);
      _M0L3valS3257 = _M0L1iS1373->$0;
      _M0L6_2atmpS3256 = _M0L3valS3257 + 1;
      _M0L1iS1373->$0 = _M0L6_2atmpS3256;
      moonbit_incref(_M0L4selfS1376);
      #line 78 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
      _M0L7_2abindS1375
      = _M0MP48clawteam8clawteam8internal5httpx9MethodMap3getGRP48clawteam8clawteam8internal5httpx5RouteE(_M0L4selfS1376, _M0L8method__S1374);
      _M0L6_2atmpS3737 = _M0L7_2abindS1375 == 0;
      if (_M0L7_2abindS1375) {
        moonbit_decref(_M0L7_2abindS1375);
      }
      if (_M0L6_2atmpS3737) {
        
      } else {
        int64_t _M0L6_2atmpS3258;
        moonbit_decref(_M0L4selfS1376);
        moonbit_decref(_M0L1iS1373);
        _M0L6_2atmpS3258 = (int64_t)_M0L8method__S1374;
        return _M0L6_2atmpS3258;
      }
      continue;
    } else {
      moonbit_decref(_M0L4selfS1376);
      moonbit_decref(_M0L1iS1373);
      return 4294967296ll;
    }
    break;
  }
}

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0MP48clawteam8clawteam8internal5httpx9MethodMap3getGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L4selfS1372,
  int32_t _M0L8method__S1371
) {
  #line 15 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  switch (_M0L8method__S1371) {
    case 0: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3740 =
        _M0L4selfS1372->$0;
      int32_t _M0L6_2acntS4207 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4207 > 1) {
        int32_t _M0L11_2anew__cntS4216 = _M0L6_2acntS4207 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4216;
        if (_M0L8_2afieldS3740) {
          moonbit_incref(_M0L8_2afieldS3740);
        }
      } else if (_M0L6_2acntS4207 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4215 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4214;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4213;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4212;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4211;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4210;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4209;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4208;
        if (_M0L8_2afieldS4215) {
          moonbit_decref(_M0L8_2afieldS4215);
        }
        _M0L8_2afieldS4214 = _M0L4selfS1372->$7;
        if (_M0L8_2afieldS4214) {
          moonbit_decref(_M0L8_2afieldS4214);
        }
        _M0L8_2afieldS4213 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4213) {
          moonbit_decref(_M0L8_2afieldS4213);
        }
        _M0L8_2afieldS4212 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4212) {
          moonbit_decref(_M0L8_2afieldS4212);
        }
        _M0L8_2afieldS4211 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4211) {
          moonbit_decref(_M0L8_2afieldS4211);
        }
        _M0L8_2afieldS4210 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4210) {
          moonbit_decref(_M0L8_2afieldS4210);
        }
        _M0L8_2afieldS4209 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4209) {
          moonbit_decref(_M0L8_2afieldS4209);
        }
        _M0L8_2afieldS4208 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4208) {
          moonbit_decref(_M0L8_2afieldS4208);
        }
        #line 17 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3740;
      break;
    }
    
    case 1: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3741 =
        _M0L4selfS1372->$1;
      int32_t _M0L6_2acntS4217 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4217 > 1) {
        int32_t _M0L11_2anew__cntS4226 = _M0L6_2acntS4217 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4226;
        if (_M0L8_2afieldS3741) {
          moonbit_incref(_M0L8_2afieldS3741);
        }
      } else if (_M0L6_2acntS4217 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4225 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4224;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4223;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4222;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4221;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4220;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4219;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4218;
        if (_M0L8_2afieldS4225) {
          moonbit_decref(_M0L8_2afieldS4225);
        }
        _M0L8_2afieldS4224 = _M0L4selfS1372->$7;
        if (_M0L8_2afieldS4224) {
          moonbit_decref(_M0L8_2afieldS4224);
        }
        _M0L8_2afieldS4223 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4223) {
          moonbit_decref(_M0L8_2afieldS4223);
        }
        _M0L8_2afieldS4222 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4222) {
          moonbit_decref(_M0L8_2afieldS4222);
        }
        _M0L8_2afieldS4221 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4221) {
          moonbit_decref(_M0L8_2afieldS4221);
        }
        _M0L8_2afieldS4220 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4220) {
          moonbit_decref(_M0L8_2afieldS4220);
        }
        _M0L8_2afieldS4219 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4219) {
          moonbit_decref(_M0L8_2afieldS4219);
        }
        _M0L8_2afieldS4218 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4218) {
          moonbit_decref(_M0L8_2afieldS4218);
        }
        #line 18 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3741;
      break;
    }
    
    case 2: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3742 =
        _M0L4selfS1372->$2;
      int32_t _M0L6_2acntS4227 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4227 > 1) {
        int32_t _M0L11_2anew__cntS4236 = _M0L6_2acntS4227 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4236;
        if (_M0L8_2afieldS3742) {
          moonbit_incref(_M0L8_2afieldS3742);
        }
      } else if (_M0L6_2acntS4227 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4235 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4234;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4233;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4232;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4231;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4230;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4229;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4228;
        if (_M0L8_2afieldS4235) {
          moonbit_decref(_M0L8_2afieldS4235);
        }
        _M0L8_2afieldS4234 = _M0L4selfS1372->$7;
        if (_M0L8_2afieldS4234) {
          moonbit_decref(_M0L8_2afieldS4234);
        }
        _M0L8_2afieldS4233 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4233) {
          moonbit_decref(_M0L8_2afieldS4233);
        }
        _M0L8_2afieldS4232 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4232) {
          moonbit_decref(_M0L8_2afieldS4232);
        }
        _M0L8_2afieldS4231 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4231) {
          moonbit_decref(_M0L8_2afieldS4231);
        }
        _M0L8_2afieldS4230 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4230) {
          moonbit_decref(_M0L8_2afieldS4230);
        }
        _M0L8_2afieldS4229 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4229) {
          moonbit_decref(_M0L8_2afieldS4229);
        }
        _M0L8_2afieldS4228 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4228) {
          moonbit_decref(_M0L8_2afieldS4228);
        }
        #line 19 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3742;
      break;
    }
    
    case 3: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3743 =
        _M0L4selfS1372->$3;
      int32_t _M0L6_2acntS4237 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4237 > 1) {
        int32_t _M0L11_2anew__cntS4246 = _M0L6_2acntS4237 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4246;
        if (_M0L8_2afieldS3743) {
          moonbit_incref(_M0L8_2afieldS3743);
        }
      } else if (_M0L6_2acntS4237 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4245 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4244;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4243;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4242;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4241;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4240;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4239;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4238;
        if (_M0L8_2afieldS4245) {
          moonbit_decref(_M0L8_2afieldS4245);
        }
        _M0L8_2afieldS4244 = _M0L4selfS1372->$7;
        if (_M0L8_2afieldS4244) {
          moonbit_decref(_M0L8_2afieldS4244);
        }
        _M0L8_2afieldS4243 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4243) {
          moonbit_decref(_M0L8_2afieldS4243);
        }
        _M0L8_2afieldS4242 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4242) {
          moonbit_decref(_M0L8_2afieldS4242);
        }
        _M0L8_2afieldS4241 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4241) {
          moonbit_decref(_M0L8_2afieldS4241);
        }
        _M0L8_2afieldS4240 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4240) {
          moonbit_decref(_M0L8_2afieldS4240);
        }
        _M0L8_2afieldS4239 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4239) {
          moonbit_decref(_M0L8_2afieldS4239);
        }
        _M0L8_2afieldS4238 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4238) {
          moonbit_decref(_M0L8_2afieldS4238);
        }
        #line 20 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3743;
      break;
    }
    
    case 4: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3744 =
        _M0L4selfS1372->$4;
      int32_t _M0L6_2acntS4247 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4247 > 1) {
        int32_t _M0L11_2anew__cntS4256 = _M0L6_2acntS4247 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4256;
        if (_M0L8_2afieldS3744) {
          moonbit_incref(_M0L8_2afieldS3744);
        }
      } else if (_M0L6_2acntS4247 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4255 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4254;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4253;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4252;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4251;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4250;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4249;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4248;
        if (_M0L8_2afieldS4255) {
          moonbit_decref(_M0L8_2afieldS4255);
        }
        _M0L8_2afieldS4254 = _M0L4selfS1372->$7;
        if (_M0L8_2afieldS4254) {
          moonbit_decref(_M0L8_2afieldS4254);
        }
        _M0L8_2afieldS4253 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4253) {
          moonbit_decref(_M0L8_2afieldS4253);
        }
        _M0L8_2afieldS4252 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4252) {
          moonbit_decref(_M0L8_2afieldS4252);
        }
        _M0L8_2afieldS4251 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4251) {
          moonbit_decref(_M0L8_2afieldS4251);
        }
        _M0L8_2afieldS4250 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4250) {
          moonbit_decref(_M0L8_2afieldS4250);
        }
        _M0L8_2afieldS4249 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4249) {
          moonbit_decref(_M0L8_2afieldS4249);
        }
        _M0L8_2afieldS4248 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4248) {
          moonbit_decref(_M0L8_2afieldS4248);
        }
        #line 21 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3744;
      break;
    }
    
    case 5: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3745 =
        _M0L4selfS1372->$5;
      int32_t _M0L6_2acntS4257 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4257 > 1) {
        int32_t _M0L11_2anew__cntS4266 = _M0L6_2acntS4257 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4266;
        if (_M0L8_2afieldS3745) {
          moonbit_incref(_M0L8_2afieldS3745);
        }
      } else if (_M0L6_2acntS4257 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4265 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4264;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4263;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4262;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4261;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4260;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4259;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4258;
        if (_M0L8_2afieldS4265) {
          moonbit_decref(_M0L8_2afieldS4265);
        }
        _M0L8_2afieldS4264 = _M0L4selfS1372->$7;
        if (_M0L8_2afieldS4264) {
          moonbit_decref(_M0L8_2afieldS4264);
        }
        _M0L8_2afieldS4263 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4263) {
          moonbit_decref(_M0L8_2afieldS4263);
        }
        _M0L8_2afieldS4262 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4262) {
          moonbit_decref(_M0L8_2afieldS4262);
        }
        _M0L8_2afieldS4261 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4261) {
          moonbit_decref(_M0L8_2afieldS4261);
        }
        _M0L8_2afieldS4260 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4260) {
          moonbit_decref(_M0L8_2afieldS4260);
        }
        _M0L8_2afieldS4259 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4259) {
          moonbit_decref(_M0L8_2afieldS4259);
        }
        _M0L8_2afieldS4258 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4258) {
          moonbit_decref(_M0L8_2afieldS4258);
        }
        #line 22 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3745;
      break;
    }
    
    case 6: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3746 =
        _M0L4selfS1372->$6;
      int32_t _M0L6_2acntS4267 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4267 > 1) {
        int32_t _M0L11_2anew__cntS4276 = _M0L6_2acntS4267 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4276;
        if (_M0L8_2afieldS3746) {
          moonbit_incref(_M0L8_2afieldS3746);
        }
      } else if (_M0L6_2acntS4267 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4275 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4274;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4273;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4272;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4271;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4270;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4269;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4268;
        if (_M0L8_2afieldS4275) {
          moonbit_decref(_M0L8_2afieldS4275);
        }
        _M0L8_2afieldS4274 = _M0L4selfS1372->$7;
        if (_M0L8_2afieldS4274) {
          moonbit_decref(_M0L8_2afieldS4274);
        }
        _M0L8_2afieldS4273 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4273) {
          moonbit_decref(_M0L8_2afieldS4273);
        }
        _M0L8_2afieldS4272 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4272) {
          moonbit_decref(_M0L8_2afieldS4272);
        }
        _M0L8_2afieldS4271 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4271) {
          moonbit_decref(_M0L8_2afieldS4271);
        }
        _M0L8_2afieldS4270 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4270) {
          moonbit_decref(_M0L8_2afieldS4270);
        }
        _M0L8_2afieldS4269 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4269) {
          moonbit_decref(_M0L8_2afieldS4269);
        }
        _M0L8_2afieldS4268 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4268) {
          moonbit_decref(_M0L8_2afieldS4268);
        }
        #line 23 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3746;
      break;
    }
    
    case 7: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3747 =
        _M0L4selfS1372->$7;
      int32_t _M0L6_2acntS4277 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4277 > 1) {
        int32_t _M0L11_2anew__cntS4286 = _M0L6_2acntS4277 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4286;
        if (_M0L8_2afieldS3747) {
          moonbit_incref(_M0L8_2afieldS3747);
        }
      } else if (_M0L6_2acntS4277 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4285 =
          _M0L4selfS1372->$8;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4284;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4283;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4282;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4281;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4280;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4279;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4278;
        if (_M0L8_2afieldS4285) {
          moonbit_decref(_M0L8_2afieldS4285);
        }
        _M0L8_2afieldS4284 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4284) {
          moonbit_decref(_M0L8_2afieldS4284);
        }
        _M0L8_2afieldS4283 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4283) {
          moonbit_decref(_M0L8_2afieldS4283);
        }
        _M0L8_2afieldS4282 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4282) {
          moonbit_decref(_M0L8_2afieldS4282);
        }
        _M0L8_2afieldS4281 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4281) {
          moonbit_decref(_M0L8_2afieldS4281);
        }
        _M0L8_2afieldS4280 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4280) {
          moonbit_decref(_M0L8_2afieldS4280);
        }
        _M0L8_2afieldS4279 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4279) {
          moonbit_decref(_M0L8_2afieldS4279);
        }
        _M0L8_2afieldS4278 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4278) {
          moonbit_decref(_M0L8_2afieldS4278);
        }
        #line 24 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3747;
      break;
    }
    default: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS3748 =
        _M0L4selfS1372->$8;
      int32_t _M0L6_2acntS4287 = Moonbit_object_header(_M0L4selfS1372)->rc;
      if (_M0L6_2acntS4287 > 1) {
        int32_t _M0L11_2anew__cntS4296 = _M0L6_2acntS4287 - 1;
        Moonbit_object_header(_M0L4selfS1372)->rc = _M0L11_2anew__cntS4296;
        if (_M0L8_2afieldS3748) {
          moonbit_incref(_M0L8_2afieldS3748);
        }
      } else if (_M0L6_2acntS4287 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4295 =
          _M0L4selfS1372->$7;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4294;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4293;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4292;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4291;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4290;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4289;
        struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L8_2afieldS4288;
        if (_M0L8_2afieldS4295) {
          moonbit_decref(_M0L8_2afieldS4295);
        }
        _M0L8_2afieldS4294 = _M0L4selfS1372->$6;
        if (_M0L8_2afieldS4294) {
          moonbit_decref(_M0L8_2afieldS4294);
        }
        _M0L8_2afieldS4293 = _M0L4selfS1372->$5;
        if (_M0L8_2afieldS4293) {
          moonbit_decref(_M0L8_2afieldS4293);
        }
        _M0L8_2afieldS4292 = _M0L4selfS1372->$4;
        if (_M0L8_2afieldS4292) {
          moonbit_decref(_M0L8_2afieldS4292);
        }
        _M0L8_2afieldS4291 = _M0L4selfS1372->$3;
        if (_M0L8_2afieldS4291) {
          moonbit_decref(_M0L8_2afieldS4291);
        }
        _M0L8_2afieldS4290 = _M0L4selfS1372->$2;
        if (_M0L8_2afieldS4290) {
          moonbit_decref(_M0L8_2afieldS4290);
        }
        _M0L8_2afieldS4289 = _M0L4selfS1372->$1;
        if (_M0L8_2afieldS4289) {
          moonbit_decref(_M0L8_2afieldS4289);
        }
        _M0L8_2afieldS4288 = _M0L4selfS1372->$0;
        if (_M0L8_2afieldS4288) {
          moonbit_decref(_M0L8_2afieldS4288);
        }
        #line 25 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
        moonbit_free(_M0L4selfS1372);
      }
      return _M0L8_2afieldS3748;
      break;
    }
  }
}

struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE* _M0MP48clawteam8clawteam8internal5httpx5Layer5route(
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L4selfS1340,
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8segmentsS1370
) {
  struct _M0TPC16string10StringView* _M0L6_2atmpS3249;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3248;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L8_2atupleS3247;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L6_2atmpS3246;
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L6_2atmpS3245;
  struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE* _M0L4currS1339;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L6_2atmpS3244;
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L6_2atmpS3243;
  struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE* _M0L4nextS1341;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8_2aparamS1342;
  #line 79 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3249
  = (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
  _M0L6_2atmpS3248
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2atmpS3248)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3248->$0 = _M0L6_2atmpS3249;
  _M0L6_2atmpS3248->$1 = 0;
  _M0L8_2atupleS3247
  = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE));
  Moonbit_object_header(_M0L8_2atupleS3247)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3247->$0 = _M0L4selfS1340;
  _M0L8_2atupleS3247->$1 = _M0L6_2atmpS3248;
  _M0L6_2atmpS3246
  = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3246[0] = _M0L8_2atupleS3247;
  _M0L6_2atmpS3245
  = (struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE));
  Moonbit_object_header(_M0L6_2atmpS3245)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3245->$0 = _M0L6_2atmpS3246;
  _M0L6_2atmpS3245->$1 = 1;
  _M0L4currS1339
  = (struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE));
  Moonbit_object_header(_M0L4currS1339)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE, $0) >> 2, 1, 0);
  _M0L4currS1339->$0 = _M0L6_2atmpS3245;
  _M0L6_2atmpS3244
  = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3243
  = (struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE));
  Moonbit_object_header(_M0L6_2atmpS3243)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3243->$0 = _M0L6_2atmpS3244;
  _M0L6_2atmpS3243->$1 = 0;
  _M0L4nextS1341
  = (struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE));
  Moonbit_object_header(_M0L4nextS1341)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGRPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE, $0) >> 2, 1, 0);
  _M0L4nextS1341->$0 = _M0L6_2atmpS3243;
  _M0L8_2aparamS1342 = _M0L8segmentsS1370;
  while (1) {
    struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8segmentsS1344;
    struct _M0TPC16string10StringView _M0L7segmentS1345;
    int32_t _M0L3endS3229 = _M0L8_2aparamS1342.$2;
    int32_t _M0L5startS3230 = _M0L8_2aparamS1342.$1;
    int32_t _M0L6_2atmpS3228 = _M0L3endS3229 - _M0L5startS3230;
    struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L8_2afieldS3762;
    struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L7_2abindS1346;
    int32_t _M0L7_2abindS1347;
    int32_t _M0L2__S1348;
    struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L8_2afieldS3751;
    struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L3valS3225;
    struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L6_2aoldS3750;
    struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L6_2atmpS3227;
    struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L6_2atmpS3226;
    struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L6_2aoldS3749;
    if (_M0L6_2atmpS3228 == 0) {
      struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L8_2afieldS3768;
      int32_t _M0L6_2acntS4303;
      struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L7_2abindS1363;
      int32_t _M0L7_2abindS1364;
      int32_t _M0L2__S1365;
      moonbit_decref(_M0L8_2aparamS1342.$0);
      moonbit_decref(_M0L4nextS1341);
      _M0L8_2afieldS3768 = _M0L4currS1339->$0;
      _M0L6_2acntS4303 = Moonbit_object_header(_M0L4currS1339)->rc;
      if (_M0L6_2acntS4303 > 1) {
        int32_t _M0L11_2anew__cntS4304 = _M0L6_2acntS4303 - 1;
        Moonbit_object_header(_M0L4currS1339)->rc = _M0L11_2anew__cntS4304;
        moonbit_incref(_M0L8_2afieldS3768);
      } else if (_M0L6_2acntS4303 == 1) {
        #line 85 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
        moonbit_free(_M0L4currS1339);
      }
      _M0L7_2abindS1363 = _M0L8_2afieldS3768;
      _M0L7_2abindS1364 = _M0L7_2abindS1363->$1;
      _M0L2__S1365 = 0;
      while (1) {
        if (_M0L2__S1365 < _M0L7_2abindS1364) {
          struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L8_2afieldS3767 =
            _M0L7_2abindS1363->$0;
          int32_t _M0L6_2acntS4305 =
            Moonbit_object_header(_M0L7_2abindS1363)->rc;
          struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L3bufS3236;
          struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2atmpS3766;
          struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L1rS1366;
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3765;
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3235;
          struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS3764;
          struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L6routesS3233;
          struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS3763;
          int32_t _M0L6_2acntS4307;
          struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3234;
          struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE* _M0L8_2atupleS3232;
          struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE* _M0L6_2atmpS3231;
          if (_M0L6_2acntS4305 > 1) {
            int32_t _M0L11_2anew__cntS4306 = _M0L6_2acntS4305 - 1;
            Moonbit_object_header(_M0L7_2abindS1363)->rc
            = _M0L11_2anew__cntS4306;
            moonbit_incref(_M0L8_2afieldS3767);
          } else if (_M0L6_2acntS4305 == 1) {
            #line 87 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
            moonbit_free(_M0L7_2abindS1363);
          }
          _M0L3bufS3236 = _M0L8_2afieldS3767;
          _M0L6_2atmpS3766
          = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)_M0L3bufS3236[
              _M0L2__S1365
            ];
          if (_M0L6_2atmpS3766) {
            moonbit_incref(_M0L6_2atmpS3766);
          }
          moonbit_decref(_M0L3bufS3236);
          _M0L1rS1366 = _M0L6_2atmpS3766;
          _M0L8_2afieldS3765 = _M0L1rS1366->$0;
          _M0L6_2atmpS3235 = _M0L8_2afieldS3765;
          _M0L8_2afieldS3764 = _M0L6_2atmpS3235->$0;
          _M0L6routesS3233 = _M0L8_2afieldS3764;
          _M0L8_2afieldS3763 = _M0L1rS1366->$1;
          moonbit_incref(_M0L6routesS3233);
          _M0L6_2acntS4307 = Moonbit_object_header(_M0L1rS1366)->rc;
          if (_M0L6_2acntS4307 > 1) {
            int32_t _M0L11_2anew__cntS4309 = _M0L6_2acntS4307 - 1;
            Moonbit_object_header(_M0L1rS1366)->rc = _M0L11_2anew__cntS4309;
            moonbit_incref(_M0L8_2afieldS3763);
          } else if (_M0L6_2acntS4307 == 1) {
            struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4308 =
              _M0L1rS1366->$0;
            moonbit_decref(_M0L8_2afieldS4308);
            #line 88 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
            moonbit_free(_M0L1rS1366);
          }
          _M0L6_2atmpS3234 = _M0L8_2afieldS3763;
          _M0L8_2atupleS3232
          = (struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE));
          Moonbit_object_header(_M0L8_2atupleS3232)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TURP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteERPB5ArrayGRPC16string10StringViewEE, $0) >> 2, 2, 0);
          _M0L8_2atupleS3232->$0 = _M0L6routesS3233;
          _M0L8_2atupleS3232->$1 = _M0L6_2atmpS3234;
          _M0L6_2atmpS3231 = _M0L8_2atupleS3232;
          return _M0L6_2atmpS3231;
        } else {
          moonbit_decref(_M0L7_2abindS1363);
          return 0;
        }
        break;
      }
    } else {
      struct _M0TPC16string10StringView* _M0L8_2afieldS3772 =
        _M0L8_2aparamS1342.$0;
      struct _M0TPC16string10StringView* _M0L3bufS3241 = _M0L8_2afieldS3772;
      int32_t _M0L5startS3242 = _M0L8_2aparamS1342.$1;
      struct _M0TPC16string10StringView _M0L6_2atmpS3771 =
        _M0L3bufS3241[_M0L5startS3242];
      struct _M0TPC16string10StringView _M0L10_2asegmentS1368 =
        _M0L6_2atmpS3771;
      struct _M0TPC16string10StringView* _M0L8_2afieldS3770 =
        _M0L8_2aparamS1342.$0;
      struct _M0TPC16string10StringView* _M0L3bufS3237 = _M0L8_2afieldS3770;
      int32_t _M0L5startS3240 = _M0L8_2aparamS1342.$1;
      int32_t _M0L6_2atmpS3238 = 1 + _M0L5startS3240;
      int32_t _M0L8_2afieldS3769 = _M0L8_2aparamS1342.$2;
      int32_t _M0L3endS3239;
      struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4_2axS1369;
      moonbit_incref(_M0L10_2asegmentS1368.$0);
      _M0L3endS3239 = _M0L8_2afieldS3769;
      _M0L4_2axS1369
      = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
        _M0L6_2atmpS3238, _M0L3endS3239, _M0L3bufS3237
      };
      _M0L8segmentsS1344 = _M0L4_2axS1369;
      _M0L7segmentS1345 = _M0L10_2asegmentS1368;
      goto join_1343;
    }
    join_1343:;
    _M0L8_2afieldS3762 = _M0L4currS1339->$0;
    _M0L7_2abindS1346 = _M0L8_2afieldS3762;
    _M0L7_2abindS1347 = _M0L7_2abindS1346->$1;
    moonbit_incref(_M0L7_2abindS1346);
    _M0L2__S1348 = 0;
    while (1) {
      if (_M0L2__S1348 < _M0L7_2abindS1347) {
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L8_2afieldS3761 =
          _M0L7_2abindS1346->$0;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L3bufS3224 =
          _M0L8_2afieldS3761;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2atmpS3760 =
          (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)_M0L3bufS3224[
            _M0L2__S1348
          ];
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L1rS1349 =
          _M0L6_2atmpS3760;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5childS1351;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3759 =
          _M0L1rS1349->$0;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3222 =
          _M0L8_2afieldS3759;
        struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3758 =
          _M0L6_2atmpS3222->$1;
        struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3221 =
          _M0L8_2afieldS3758;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2abindS1352;
        struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L8_2afieldS3753;
        struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L3valS3215;
        struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS3752;
        int32_t _M0L6_2acntS4297;
        struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3217;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L8_2atupleS3216;
        int32_t _M0L6_2atmpS3223;
        moonbit_incref(_M0L7matchesS3221);
        moonbit_incref(_M0L1rS1349);
        moonbit_incref(_M0L7segmentS1345.$0);
        #line 94 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
        _M0L7_2abindS1352
        = _M0MPB3Map3getGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3221, _M0L7segmentS1345);
        if (_M0L7_2abindS1352 == 0) {
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5childS1356;
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3757;
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3220;
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3756;
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2abindS1358;
          struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS3755;
          int32_t _M0L6_2acntS4300;
          struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6valuesS1357;
          struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L8_2afieldS3754;
          struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L3valS3218;
          struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L8_2atupleS3219;
          if (_M0L7_2abindS1352) {
            moonbit_decref(_M0L7_2abindS1352);
          }
          _M0L8_2afieldS3757 = _M0L1rS1349->$0;
          _M0L6_2atmpS3220 = _M0L8_2afieldS3757;
          _M0L8_2afieldS3756 = _M0L6_2atmpS3220->$2;
          _M0L7_2abindS1358 = _M0L8_2afieldS3756;
          if (_M0L7_2abindS1358 == 0) {
            moonbit_decref(_M0L1rS1349);
          } else {
            struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2aSomeS1359 =
              _M0L7_2abindS1358;
            struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2achildS1360 =
              _M0L7_2aSomeS1359;
            moonbit_incref(_M0L8_2achildS1360);
            _M0L5childS1356 = _M0L8_2achildS1360;
            goto join_1355;
          }
          goto joinlet_4486;
          join_1355:;
          _M0L8_2afieldS3755 = _M0L1rS1349->$1;
          _M0L6_2acntS4300 = Moonbit_object_header(_M0L1rS1349)->rc;
          if (_M0L6_2acntS4300 > 1) {
            int32_t _M0L11_2anew__cntS4302 = _M0L6_2acntS4300 - 1;
            Moonbit_object_header(_M0L1rS1349)->rc = _M0L11_2anew__cntS4302;
            moonbit_incref(_M0L8_2afieldS3755);
          } else if (_M0L6_2acntS4300 == 1) {
            struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4301 =
              _M0L1rS1349->$0;
            moonbit_decref(_M0L8_2afieldS4301);
            #line 97 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
            moonbit_free(_M0L1rS1349);
          }
          _M0L6valuesS1357 = _M0L8_2afieldS3755;
          moonbit_incref(_M0L6valuesS1357);
          moonbit_incref(_M0L7segmentS1345.$0);
          #line 98 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
          _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L6valuesS1357, _M0L7segmentS1345);
          _M0L8_2afieldS3754 = _M0L4nextS1341->$0;
          _M0L3valS3218 = _M0L8_2afieldS3754;
          _M0L8_2atupleS3219
          = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE));
          Moonbit_object_header(_M0L8_2atupleS3219)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE, $0) >> 2, 2, 0);
          _M0L8_2atupleS3219->$0 = _M0L5childS1356;
          _M0L8_2atupleS3219->$1 = _M0L6valuesS1357;
          moonbit_incref(_M0L3valS3218);
          #line 99 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
          _M0MPC15array5Array4pushGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(_M0L3valS3218, _M0L8_2atupleS3219);
          joinlet_4486:;
        } else {
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2aSomeS1353 =
            _M0L7_2abindS1352;
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2achildS1354 =
            _M0L7_2aSomeS1353;
          _M0L5childS1351 = _M0L8_2achildS1354;
          goto join_1350;
        }
        goto joinlet_4485;
        join_1350:;
        _M0L8_2afieldS3753 = _M0L4nextS1341->$0;
        _M0L3valS3215 = _M0L8_2afieldS3753;
        _M0L8_2afieldS3752 = _M0L1rS1349->$1;
        moonbit_incref(_M0L3valS3215);
        _M0L6_2acntS4297 = Moonbit_object_header(_M0L1rS1349)->rc;
        if (_M0L6_2acntS4297 > 1) {
          int32_t _M0L11_2anew__cntS4299 = _M0L6_2acntS4297 - 1;
          Moonbit_object_header(_M0L1rS1349)->rc = _M0L11_2anew__cntS4299;
          moonbit_incref(_M0L8_2afieldS3752);
        } else if (_M0L6_2acntS4297 == 1) {
          struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4298 =
            _M0L1rS1349->$0;
          moonbit_decref(_M0L8_2afieldS4298);
          #line 95 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
          moonbit_free(_M0L1rS1349);
        }
        _M0L6_2atmpS3217 = _M0L8_2afieldS3752;
        _M0L8_2atupleS3216
        = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE));
        Moonbit_object_header(_M0L8_2atupleS3216)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE, $0) >> 2, 2, 0);
        _M0L8_2atupleS3216->$0 = _M0L5childS1351;
        _M0L8_2atupleS3216->$1 = _M0L6_2atmpS3217;
        #line 95 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
        _M0MPC15array5Array4pushGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(_M0L3valS3215, _M0L8_2atupleS3216);
        joinlet_4485:;
        _M0L6_2atmpS3223 = _M0L2__S1348 + 1;
        _M0L2__S1348 = _M0L6_2atmpS3223;
        continue;
      } else {
        moonbit_decref(_M0L7_2abindS1346);
        moonbit_decref(_M0L7segmentS1345.$0);
      }
      break;
    }
    _M0L8_2afieldS3751 = _M0L4nextS1341->$0;
    _M0L3valS3225 = _M0L8_2afieldS3751;
    _M0L6_2aoldS3750 = _M0L4currS1339->$0;
    moonbit_incref(_M0L3valS3225);
    moonbit_decref(_M0L6_2aoldS3750);
    _M0L4currS1339->$0 = _M0L3valS3225;
    _M0L6_2atmpS3227
    = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3226
    = (struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE));
    Moonbit_object_header(_M0L6_2atmpS3226)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE, $0) >> 2, 1, 0);
    _M0L6_2atmpS3226->$0 = _M0L6_2atmpS3227;
    _M0L6_2atmpS3226->$1 = 0;
    _M0L6_2aoldS3749 = _M0L4nextS1341->$0;
    moonbit_decref(_M0L6_2aoldS3749);
    _M0L4nextS1341->$0 = _M0L6_2atmpS3226;
    _M0L8_2aparamS1342 = _M0L8segmentsS1344;
    continue;
    break;
  }
}

int32_t _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L4selfS1335,
  int32_t _M0L8method__S1336,
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8segmentsS1337,
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L5routeS1338
) {
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L4selfS1295;
  int32_t _M0L8method__S1296;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8segmentsS1297;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L5routeS1298;
  #line 45 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L4selfS1295 = _M0L4selfS1335;
  _M0L8method__S1296 = _M0L8method__S1336;
  _M0L8segmentsS1297 = _M0L8segmentsS1337;
  _M0L5routeS1298 = _M0L5routeS1338;
  while (1) {
    struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8segmentsS1300;
    struct _M0TPC16string10StringView _M0L5valueS1301;
    struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8segmentsS1310;
    struct _M0TPC16string10StringView _M0L4nameS1311;
    int32_t _M0L3endS3164 = _M0L8segmentsS1297.$2;
    int32_t _M0L5startS3165 = _M0L8segmentsS1297.$1;
    int32_t _M0L6_2atmpS3163 = _M0L3endS3164 - _M0L5startS3165;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS3779;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L5namesS3160;
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5childS1313;
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3778;
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2abindS1314;
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3776;
    int32_t _M0L6_2acntS4314;
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3162;
    struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L5_2aitS1318;
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5childS1303;
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3774;
    struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3159;
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2abindS1305;
    int32_t _tmp_4496;
    struct _M0TP48clawteam8clawteam8internal5httpx5Route* _tmp_4497;
    if (_M0L6_2atmpS3163 == 0) {
      struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS3780;
      int32_t _M0L6_2acntS4321;
      struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L6routesS3166;
      moonbit_decref(_M0L8segmentsS1297.$0);
      _M0L8_2afieldS3780 = _M0L4selfS1295->$0;
      _M0L6_2acntS4321 = Moonbit_object_header(_M0L4selfS1295)->rc;
      if (_M0L6_2acntS4321 > 1) {
        int32_t _M0L11_2anew__cntS4324 = _M0L6_2acntS4321 - 1;
        Moonbit_object_header(_M0L4selfS1295)->rc = _M0L11_2anew__cntS4324;
        moonbit_incref(_M0L8_2afieldS3780);
      } else if (_M0L6_2acntS4321 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4323 =
          _M0L4selfS1295->$2;
        struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4322;
        if (_M0L8_2afieldS4323) {
          moonbit_decref(_M0L8_2afieldS4323);
        }
        _M0L8_2afieldS4322 = _M0L4selfS1295->$1;
        moonbit_decref(_M0L8_2afieldS4322);
        #line 52 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
        moonbit_free(_M0L4selfS1295);
      }
      _M0L6routesS3166 = _M0L8_2afieldS3780;
      #line 52 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      _M0MP48clawteam8clawteam8internal5httpx9MethodMap7op__setGRP48clawteam8clawteam8internal5httpx5RouteE(_M0L6routesS3166, _M0L8method__S1296, _M0L5routeS1298);
    } else {
      struct _M0TPC16string10StringView* _M0L8_2afieldS3799 =
        _M0L8segmentsS1297.$0;
      struct _M0TPC16string10StringView* _M0L3bufS3213 = _M0L8_2afieldS3799;
      int32_t _M0L5startS3214 = _M0L8segmentsS1297.$1;
      struct _M0TPC16string10StringView _M0L6_2atmpS3798 =
        _M0L3bufS3213[_M0L5startS3214];
      struct _M0TPC16string10StringView _M0L4_2axS1326 = _M0L6_2atmpS3798;
      moonbit_string_t _M0L8_2afieldS3797 = _M0L4_2axS1326.$0;
      moonbit_string_t _M0L3strS3167 = _M0L8_2afieldS3797;
      int32_t _M0L5startS3168 = _M0L4_2axS1326.$1;
      int32_t _M0L3endS3170 = _M0L4_2axS1326.$2;
      int64_t _M0L6_2atmpS3169 = (int64_t)_M0L3endS3170;
      moonbit_incref(_M0L3strS3167);
      moonbit_incref(_M0L4_2axS1326.$0);
      #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      if (
        _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3167, 2, _M0L5startS3168, _M0L6_2atmpS3169)
      ) {
        moonbit_string_t _M0L8_2afieldS3794 = _M0L4_2axS1326.$0;
        moonbit_string_t _M0L3strS3202 = _M0L8_2afieldS3794;
        moonbit_string_t _M0L8_2afieldS3793 = _M0L4_2axS1326.$0;
        moonbit_string_t _M0L3strS3205 = _M0L8_2afieldS3793;
        int32_t _M0L5startS3206 = _M0L4_2axS1326.$1;
        int32_t _M0L3endS3208 = _M0L4_2axS1326.$2;
        int64_t _M0L6_2atmpS3207 = (int64_t)_M0L3endS3208;
        int64_t _M0L6_2atmpS3204;
        int32_t _M0L6_2atmpS3203;
        int32_t _M0L4_2axS1327;
        moonbit_incref(_M0L3strS3205);
        moonbit_incref(_M0L3strS3202);
        #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
        _M0L6_2atmpS3204
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3205, 0, _M0L5startS3206, _M0L6_2atmpS3207);
        _M0L6_2atmpS3203 = (int32_t)_M0L6_2atmpS3204;
        #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
        _M0L4_2axS1327
        = _M0MPC16string6String16unsafe__char__at(_M0L3strS3202, _M0L6_2atmpS3203);
        if (_M0L4_2axS1327 == 123) {
          moonbit_string_t _M0L8_2afieldS3790 = _M0L4_2axS1326.$0;
          moonbit_string_t _M0L3strS3195 = _M0L8_2afieldS3790;
          moonbit_string_t _M0L8_2afieldS3789 = _M0L4_2axS1326.$0;
          moonbit_string_t _M0L3strS3198 = _M0L8_2afieldS3789;
          int32_t _M0L5startS3199 = _M0L4_2axS1326.$1;
          int32_t _M0L3endS3201 = _M0L4_2axS1326.$2;
          int64_t _M0L6_2atmpS3200 = (int64_t)_M0L3endS3201;
          int64_t _M0L6_2atmpS3197;
          int32_t _M0L6_2atmpS3196;
          int32_t _M0L4_2axS1328;
          moonbit_incref(_M0L3strS3198);
          moonbit_incref(_M0L3strS3195);
          #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
          _M0L6_2atmpS3197
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3198, -1, _M0L5startS3199, _M0L6_2atmpS3200);
          _M0L6_2atmpS3196 = (int32_t)_M0L6_2atmpS3197;
          #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
          _M0L4_2axS1328
          = _M0MPC16string6String16unsafe__char__at(_M0L3strS3195, _M0L6_2atmpS3196);
          if (_M0L4_2axS1328 == 125) {
            moonbit_string_t _M0L8_2afieldS3786 = _M0L4_2axS1326.$0;
            moonbit_string_t _M0L3strS3183 = _M0L8_2afieldS3786;
            moonbit_string_t _M0L8_2afieldS3785 = _M0L4_2axS1326.$0;
            moonbit_string_t _M0L3strS3191 = _M0L8_2afieldS3785;
            int32_t _M0L5startS3192 = _M0L4_2axS1326.$1;
            int32_t _M0L3endS3194 = _M0L4_2axS1326.$2;
            int64_t _M0L6_2atmpS3193 = (int64_t)_M0L3endS3194;
            int64_t _M0L7_2abindS1594;
            int32_t _M0L6_2atmpS3184;
            moonbit_string_t _M0L8_2afieldS3784;
            moonbit_string_t _M0L3strS3187;
            int32_t _M0L5startS3188;
            int32_t _M0L8_2afieldS3783;
            int32_t _M0L3endS3190;
            int64_t _M0L6_2atmpS3189;
            int64_t _M0L6_2atmpS3186;
            int32_t _M0L6_2atmpS3185;
            struct _M0TPC16string10StringView _M0L4_2axS1329;
            struct _M0TPC16string10StringView* _M0L8_2afieldS3782;
            struct _M0TPC16string10StringView* _M0L3bufS3179;
            int32_t _M0L5startS3182;
            int32_t _M0L6_2atmpS3180;
            int32_t _M0L8_2afieldS3781;
            int32_t _M0L3endS3181;
            struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4_2axS1331;
            moonbit_incref(_M0L3strS3191);
            moonbit_incref(_M0L3strS3183);
            #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
            _M0L7_2abindS1594
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3191, 1, _M0L5startS3192, _M0L6_2atmpS3193);
            if (_M0L7_2abindS1594 == 4294967296ll) {
              _M0L6_2atmpS3184 = _M0L4_2axS1326.$2;
            } else {
              int64_t _M0L7_2aSomeS1330 = _M0L7_2abindS1594;
              _M0L6_2atmpS3184 = (int32_t)_M0L7_2aSomeS1330;
            }
            _M0L8_2afieldS3784 = _M0L4_2axS1326.$0;
            _M0L3strS3187 = _M0L8_2afieldS3784;
            _M0L5startS3188 = _M0L4_2axS1326.$1;
            _M0L8_2afieldS3783 = _M0L4_2axS1326.$2;
            _M0L3endS3190 = _M0L8_2afieldS3783;
            _M0L6_2atmpS3189 = (int64_t)_M0L3endS3190;
            #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
            _M0L6_2atmpS3186
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3187, -1, _M0L5startS3188, _M0L6_2atmpS3189);
            _M0L6_2atmpS3185 = (int32_t)_M0L6_2atmpS3186;
            _M0L4_2axS1329
            = (struct _M0TPC16string10StringView){
              _M0L6_2atmpS3184, _M0L6_2atmpS3185, _M0L3strS3183
            };
            _M0L8_2afieldS3782 = _M0L8segmentsS1297.$0;
            _M0L3bufS3179 = _M0L8_2afieldS3782;
            _M0L5startS3182 = _M0L8segmentsS1297.$1;
            _M0L6_2atmpS3180 = 1 + _M0L5startS3182;
            _M0L8_2afieldS3781 = _M0L8segmentsS1297.$2;
            _M0L3endS3181 = _M0L8_2afieldS3781;
            _M0L4_2axS1331
            = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
              _M0L6_2atmpS3180, _M0L3endS3181, _M0L3bufS3179
            };
            _M0L8segmentsS1310 = _M0L4_2axS1331;
            _M0L4nameS1311 = _M0L4_2axS1329;
            goto join_1309;
          } else {
            struct _M0TPC16string10StringView* _M0L8_2afieldS3788 =
              _M0L8segmentsS1297.$0;
            struct _M0TPC16string10StringView* _M0L3bufS3175 =
              _M0L8_2afieldS3788;
            int32_t _M0L5startS3178 = _M0L8segmentsS1297.$1;
            int32_t _M0L6_2atmpS3176 = 1 + _M0L5startS3178;
            int32_t _M0L8_2afieldS3787 = _M0L8segmentsS1297.$2;
            int32_t _M0L3endS3177 = _M0L8_2afieldS3787;
            struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4_2axS1332 =
              (struct _M0TPB9ArrayViewGRPC16string10StringViewE){_M0L6_2atmpS3176,
                                                                   _M0L3endS3177,
                                                                   _M0L3bufS3175};
            _M0L8segmentsS1300 = _M0L4_2axS1332;
            _M0L5valueS1301 = _M0L4_2axS1326;
            goto join_1299;
          }
        } else {
          struct _M0TPC16string10StringView* _M0L8_2afieldS3792 =
            _M0L8segmentsS1297.$0;
          struct _M0TPC16string10StringView* _M0L3bufS3171 =
            _M0L8_2afieldS3792;
          int32_t _M0L5startS3174 = _M0L8segmentsS1297.$1;
          int32_t _M0L6_2atmpS3172 = 1 + _M0L5startS3174;
          int32_t _M0L8_2afieldS3791 = _M0L8segmentsS1297.$2;
          int32_t _M0L3endS3173 = _M0L8_2afieldS3791;
          struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4_2axS1333 =
            (struct _M0TPB9ArrayViewGRPC16string10StringViewE){_M0L6_2atmpS3172,
                                                                 _M0L3endS3173,
                                                                 _M0L3bufS3171};
          _M0L8segmentsS1300 = _M0L4_2axS1333;
          _M0L5valueS1301 = _M0L4_2axS1326;
          goto join_1299;
        }
      } else {
        struct _M0TPC16string10StringView* _M0L8_2afieldS3796 =
          _M0L8segmentsS1297.$0;
        struct _M0TPC16string10StringView* _M0L3bufS3209 = _M0L8_2afieldS3796;
        int32_t _M0L5startS3212 = _M0L8segmentsS1297.$1;
        int32_t _M0L6_2atmpS3210 = 1 + _M0L5startS3212;
        int32_t _M0L8_2afieldS3795 = _M0L8segmentsS1297.$2;
        int32_t _M0L3endS3211 = _M0L8_2afieldS3795;
        struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4_2axS1334 =
          (struct _M0TPB9ArrayViewGRPC16string10StringViewE){_M0L6_2atmpS3210,
                                                               _M0L3endS3211,
                                                               _M0L3bufS3209};
        _M0L8segmentsS1300 = _M0L4_2axS1334;
        _M0L5valueS1301 = _M0L4_2axS1326;
        goto join_1299;
      }
    }
    goto joinlet_4489;
    join_1309:;
    _M0L8_2afieldS3779 = _M0L5routeS1298->$1;
    _M0L5namesS3160 = _M0L8_2afieldS3779;
    moonbit_incref(_M0L5namesS3160);
    #line 54 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L5namesS3160, _M0L4nameS1311);
    _M0L8_2afieldS3778 = _M0L4selfS1295->$2;
    _M0L7_2abindS1314 = _M0L8_2afieldS3778;
    if (_M0L7_2abindS1314 == 0) {
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5childS1317;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3161;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2aoldS3777;
      #line 58 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      _M0L5childS1317 = _M0MP48clawteam8clawteam8internal5httpx5Layer3new();
      moonbit_incref(_M0L5childS1317);
      _M0L6_2atmpS3161 = _M0L5childS1317;
      _M0L6_2aoldS3777 = _M0L4selfS1295->$2;
      if (_M0L6_2aoldS3777) {
        moonbit_decref(_M0L6_2aoldS3777);
      }
      _M0L4selfS1295->$2 = _M0L6_2atmpS3161;
      moonbit_incref(_M0L8segmentsS1310.$0);
      moonbit_incref(_M0L5routeS1298);
      #line 60 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(_M0L5childS1317, _M0L8method__S1296, _M0L8segmentsS1310, _M0L5routeS1298);
    } else {
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2aSomeS1315 =
        _M0L7_2abindS1314;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2achildS1316 =
        _M0L7_2aSomeS1315;
      moonbit_incref(_M0L8_2achildS1316);
      _M0L5childS1313 = _M0L8_2achildS1316;
      goto join_1312;
    }
    goto joinlet_4490;
    join_1312:;
    moonbit_incref(_M0L8segmentsS1310.$0);
    moonbit_incref(_M0L5routeS1298);
    #line 56 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(_M0L5childS1313, _M0L8method__S1296, _M0L8segmentsS1310, _M0L5routeS1298);
    joinlet_4490:;
    _M0L8_2afieldS3776 = _M0L4selfS1295->$1;
    _M0L6_2acntS4314 = Moonbit_object_header(_M0L4selfS1295)->rc;
    if (_M0L6_2acntS4314 > 1) {
      int32_t _M0L11_2anew__cntS4317 = _M0L6_2acntS4314 - 1;
      Moonbit_object_header(_M0L4selfS1295)->rc = _M0L11_2anew__cntS4317;
      moonbit_incref(_M0L8_2afieldS3776);
    } else if (_M0L6_2acntS4314 == 1) {
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4316 =
        _M0L4selfS1295->$2;
      struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4315;
      if (_M0L8_2afieldS4316) {
        moonbit_decref(_M0L8_2afieldS4316);
      }
      _M0L8_2afieldS4315 = _M0L4selfS1295->$0;
      moonbit_decref(_M0L8_2afieldS4315);
      #line 62 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      moonbit_free(_M0L4selfS1295);
    }
    _M0L7matchesS3162 = _M0L8_2afieldS3776;
    #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    _M0L5_2aitS1318
    = _M0MPB3Map5iter2GRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3162);
    while (1) {
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5valueS1320;
      struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS1322;
      moonbit_incref(_M0L5_2aitS1318);
      #line 62 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      _M0L7_2abindS1322
      = _M0MPB5Iter24nextGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L5_2aitS1318);
      if (_M0L7_2abindS1322 == 0) {
        if (_M0L7_2abindS1322) {
          moonbit_decref(_M0L7_2abindS1322);
        }
        moonbit_decref(_M0L5_2aitS1318);
        moonbit_decref(_M0L8segmentsS1310.$0);
        moonbit_decref(_M0L5routeS1298);
      } else {
        struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS1323 =
          _M0L7_2abindS1322;
        struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4_2axS1324 =
          _M0L7_2aSomeS1323;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3775 =
          _M0L4_2axS1324->$1;
        int32_t _M0L6_2acntS4318 = Moonbit_object_header(_M0L4_2axS1324)->rc;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2avalueS1325;
        if (_M0L6_2acntS4318 > 1) {
          int32_t _M0L11_2anew__cntS4320 = _M0L6_2acntS4318 - 1;
          Moonbit_object_header(_M0L4_2axS1324)->rc = _M0L11_2anew__cntS4320;
          moonbit_incref(_M0L8_2afieldS3775);
        } else if (_M0L6_2acntS4318 == 1) {
          struct _M0TPC16string10StringView _M0L8_2afieldS4319 =
            (struct _M0TPC16string10StringView){_M0L4_2axS1324->$0_1,
                                                  _M0L4_2axS1324->$0_2,
                                                  _M0L4_2axS1324->$0_0};
          moonbit_decref(_M0L8_2afieldS4319.$0);
          #line 62 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
          moonbit_free(_M0L4_2axS1324);
        }
        _M0L8_2avalueS1325 = _M0L8_2afieldS3775;
        _M0L5valueS1320 = _M0L8_2avalueS1325;
        goto join_1319;
      }
      goto joinlet_4492;
      join_1319:;
      moonbit_incref(_M0L8segmentsS1310.$0);
      moonbit_incref(_M0L5routeS1298);
      #line 63 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      _M0MP48clawteam8clawteam8internal5httpx5Layer10add__route(_M0L5valueS1320, _M0L8method__S1296, _M0L8segmentsS1310, _M0L5routeS1298);
      continue;
      joinlet_4492:;
      break;
    }
    joinlet_4489:;
    goto joinlet_4488;
    join_1299:;
    _M0L8_2afieldS3774 = _M0L4selfS1295->$1;
    _M0L7matchesS3159 = _M0L8_2afieldS3774;
    moonbit_incref(_M0L7matchesS3159);
    moonbit_incref(_M0L5valueS1301.$0);
    #line 67 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
    _M0L7_2abindS1305
    = _M0MPB3Map3getGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3159, _M0L5valueS1301);
    if (_M0L7_2abindS1305 == 0) {
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5childS1308;
      struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3773;
      int32_t _M0L6_2acntS4310;
      struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7matchesS3158;
      int32_t _tmp_4494;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _tmp_4495;
      if (_M0L7_2abindS1305) {
        moonbit_decref(_M0L7_2abindS1305);
      }
      #line 70 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      _M0L5childS1308 = _M0MP48clawteam8clawteam8internal5httpx5Layer3new();
      _M0L8_2afieldS3773 = _M0L4selfS1295->$1;
      _M0L6_2acntS4310 = Moonbit_object_header(_M0L4selfS1295)->rc;
      if (_M0L6_2acntS4310 > 1) {
        int32_t _M0L11_2anew__cntS4313 = _M0L6_2acntS4310 - 1;
        Moonbit_object_header(_M0L4selfS1295)->rc = _M0L11_2anew__cntS4313;
        moonbit_incref(_M0L8_2afieldS3773);
      } else if (_M0L6_2acntS4310 == 1) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4312 =
          _M0L4selfS1295->$2;
        struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L8_2afieldS4311;
        if (_M0L8_2afieldS4312) {
          moonbit_decref(_M0L8_2afieldS4312);
        }
        _M0L8_2afieldS4311 = _M0L4selfS1295->$0;
        moonbit_decref(_M0L8_2afieldS4311);
        #line 71 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
        moonbit_free(_M0L4selfS1295);
      }
      _M0L7matchesS3158 = _M0L8_2afieldS3773;
      moonbit_incref(_M0L5childS1308);
      #line 71 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
      _M0MPB3Map3setGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L7matchesS3158, _M0L5valueS1301, _M0L5childS1308);
      _tmp_4494 = _M0L8method__S1296;
      _tmp_4495 = _M0L5routeS1298;
      _M0L4selfS1295 = _M0L5childS1308;
      _M0L8method__S1296 = _tmp_4494;
      _M0L8segmentsS1297 = _M0L8segmentsS1300;
      _M0L5routeS1298 = _tmp_4495;
      continue;
    } else {
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2aSomeS1306;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2achildS1307;
      moonbit_decref(_M0L5valueS1301.$0);
      moonbit_decref(_M0L4selfS1295);
      _M0L7_2aSomeS1306 = _M0L7_2abindS1305;
      _M0L8_2achildS1307 = _M0L7_2aSomeS1306;
      _M0L5childS1303 = _M0L8_2achildS1307;
      goto join_1302;
    }
    goto joinlet_4493;
    join_1302:;
    _tmp_4496 = _M0L8method__S1296;
    _tmp_4497 = _M0L5routeS1298;
    _M0L4selfS1295 = _M0L5childS1303;
    _M0L8method__S1296 = _tmp_4496;
    _M0L8segmentsS1297 = _M0L8segmentsS1300;
    _M0L5routeS1298 = _tmp_4497;
    continue;
    joinlet_4493:;
    joinlet_4488:;
    break;
  }
  return 0;
}

int32_t _M0MP48clawteam8clawteam8internal5httpx9MethodMap7op__setGRP48clawteam8clawteam8internal5httpx5RouteE(
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L4selfS1293,
  int32_t _M0L8method__S1292,
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L5valueS1294
) {
  #line 38 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  switch (_M0L8method__S1292) {
    case 0: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3149 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3800 =
        _M0L4selfS1293->$0;
      if (_M0L6_2aoldS3800) {
        moonbit_decref(_M0L6_2aoldS3800);
      }
      _M0L4selfS1293->$0 = _M0L6_2atmpS3149;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    
    case 1: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3150 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3801 =
        _M0L4selfS1293->$1;
      if (_M0L6_2aoldS3801) {
        moonbit_decref(_M0L6_2aoldS3801);
      }
      _M0L4selfS1293->$1 = _M0L6_2atmpS3150;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    
    case 2: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3151 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3802 =
        _M0L4selfS1293->$2;
      if (_M0L6_2aoldS3802) {
        moonbit_decref(_M0L6_2aoldS3802);
      }
      _M0L4selfS1293->$2 = _M0L6_2atmpS3151;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    
    case 3: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3152 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3803 =
        _M0L4selfS1293->$3;
      if (_M0L6_2aoldS3803) {
        moonbit_decref(_M0L6_2aoldS3803);
      }
      _M0L4selfS1293->$3 = _M0L6_2atmpS3152;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    
    case 4: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3153 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3804 =
        _M0L4selfS1293->$4;
      if (_M0L6_2aoldS3804) {
        moonbit_decref(_M0L6_2aoldS3804);
      }
      _M0L4selfS1293->$4 = _M0L6_2atmpS3153;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    
    case 5: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3154 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3805 =
        _M0L4selfS1293->$5;
      if (_M0L6_2aoldS3805) {
        moonbit_decref(_M0L6_2aoldS3805);
      }
      _M0L4selfS1293->$5 = _M0L6_2atmpS3154;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    
    case 6: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3155 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3806 =
        _M0L4selfS1293->$6;
      if (_M0L6_2aoldS3806) {
        moonbit_decref(_M0L6_2aoldS3806);
      }
      _M0L4selfS1293->$6 = _M0L6_2atmpS3155;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    
    case 7: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3156 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3807 =
        _M0L4selfS1293->$7;
      if (_M0L6_2aoldS3807) {
        moonbit_decref(_M0L6_2aoldS3807);
      }
      _M0L4selfS1293->$7 = _M0L6_2atmpS3156;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
    default: {
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3157 =
        _M0L5valueS1294;
      struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2aoldS3808 =
        _M0L4selfS1293->$8;
      if (_M0L6_2aoldS3808) {
        moonbit_decref(_M0L6_2aoldS3808);
      }
      _M0L4selfS1293->$8 = _M0L6_2atmpS3157;
      moonbit_decref(_M0L4selfS1293);
      break;
    }
  }
  return 0;
}

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MP48clawteam8clawteam8internal5httpx5Layer3new(
  
) {
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0L6_2atmpS3144;
  struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7_2abindS1291;
  struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L6_2atmpS3148;
  struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE _M0L6_2atmpS3147;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS3145;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS3146;
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _block_4498;
  #line 40 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  #line 41 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3144
  = _M0MP48clawteam8clawteam8internal5httpx9MethodMap3newGRP48clawteam8clawteam8internal5httpx5RouteE();
  _M0L7_2abindS1291
  = (struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3148 = _M0L7_2abindS1291;
  _M0L6_2atmpS3147
  = (struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE){
    0, 0, _M0L6_2atmpS3148
  };
  #line 41 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3145
  = _M0MPB3Map11from__arrayGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L6_2atmpS3147);
  _M0L6_2atmpS3146 = 0;
  _block_4498
  = (struct _M0TP48clawteam8clawteam8internal5httpx5Layer*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal5httpx5Layer));
  Moonbit_object_header(_block_4498)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal5httpx5Layer, $0) >> 2, 3, 0);
  _block_4498->$0 = _M0L6_2atmpS3144;
  _block_4498->$1 = _M0L6_2atmpS3145;
  _block_4498->$2 = _M0L6_2atmpS3146;
  return _block_4498;
}

struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _M0MP48clawteam8clawteam8internal5httpx9MethodMap3newGRP48clawteam8clawteam8internal5httpx5RouteE(
  
) {
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3135;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3136;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3137;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3138;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3139;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3140;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3141;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3142;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0L6_2atmpS3143;
  struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE* _block_4499;
  #line 57 "E:\\moonbit\\clawteam\\internal\\httpx\\method_map.mbt"
  _M0L6_2atmpS3135 = 0;
  _M0L6_2atmpS3136 = 0;
  _M0L6_2atmpS3137 = 0;
  _M0L6_2atmpS3138 = 0;
  _M0L6_2atmpS3139 = 0;
  _M0L6_2atmpS3140 = 0;
  _M0L6_2atmpS3141 = 0;
  _M0L6_2atmpS3142 = 0;
  _M0L6_2atmpS3143 = 0;
  _block_4499
  = (struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE));
  Moonbit_object_header(_block_4499)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal5httpx9MethodMapGRP48clawteam8clawteam8internal5httpx5RouteE, $0) >> 2, 9, 0);
  _block_4499->$0 = _M0L6_2atmpS3135;
  _block_4499->$1 = _M0L6_2atmpS3136;
  _block_4499->$2 = _M0L6_2atmpS3137;
  _block_4499->$3 = _M0L6_2atmpS3138;
  _block_4499->$4 = _M0L6_2atmpS3139;
  _block_4499->$5 = _M0L6_2atmpS3140;
  _block_4499->$6 = _M0L6_2atmpS3141;
  _block_4499->$7 = _M0L6_2atmpS3142;
  _block_4499->$8 = _M0L6_2atmpS3143;
  return _block_4499;
}

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0MP48clawteam8clawteam8internal5httpx5Route3new(
  struct _M0TWRP48clawteam8clawteam8internal5httpx13RequestReaderRP48clawteam8clawteam8internal5httpx14ResponseWriterWuEuWRPC15error5ErrorEuEOuQRPC15error5Error* _M0L6handleS1290
) {
  struct _M0TPC16string10StringView* _M0L6_2atmpS3134;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3133;
  struct _M0TP48clawteam8clawteam8internal5httpx5Route* _block_4500;
  #line 28 "E:\\moonbit\\clawteam\\internal\\httpx\\router.mbt"
  _M0L6_2atmpS3134
  = (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
  _M0L6_2atmpS3133
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2atmpS3133)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3133->$0 = _M0L6_2atmpS3134;
  _M0L6_2atmpS3133->$1 = 0;
  _block_4500
  = (struct _M0TP48clawteam8clawteam8internal5httpx5Route*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal5httpx5Route));
  Moonbit_object_header(_block_4500)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal5httpx5Route, $0) >> 2, 2, 0);
  _block_4500->$0 = _M0L6handleS1290;
  _block_4500->$1 = _M0L6_2atmpS3133;
  return _block_4500;
}

void* _M0IP48clawteam8clawteam8internal5httpx6MethodPB6ToJson8to__json(
  int32_t _M0L9_2ax__292S1289
) {
  #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
  switch (_M0L9_2ax__292S1289) {
    case 0: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_37.data);
      break;
    }
    
    case 1: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_56.data);
      break;
    }
    
    case 2: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_57.data);
      break;
    }
    
    case 3: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_58.data);
      break;
    }
    
    case 4: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_59.data);
      break;
    }
    
    case 5: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_60.data);
      break;
    }
    
    case 6: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_61.data);
      break;
    }
    
    case 7: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_62.data);
      break;
    }
    default: {
      #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\method.mbt"
      return _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_63.data);
      break;
    }
  }
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1284,
  void* _M0L7contentS1286,
  moonbit_string_t _M0L3locS1280,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1282
) {
  moonbit_string_t _M0L3locS1279;
  moonbit_string_t _M0L9args__locS1281;
  void* _M0L6_2atmpS3131;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3132;
  moonbit_string_t _M0L6actualS1283;
  moonbit_string_t _M0L4wantS1285;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1279 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1280);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1281 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1282);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3131 = _M0L3objS1284.$0->$method_0(_M0L3objS1284.$1);
  _M0L6_2atmpS3132 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1283
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3131, 0, 0, _M0L6_2atmpS3132);
  if (_M0L7contentS1286 == 0) {
    void* _M0L6_2atmpS3128;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3129;
    if (_M0L7contentS1286) {
      moonbit_decref(_M0L7contentS1286);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3128
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS3129 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1285
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3128, 0, 0, _M0L6_2atmpS3129);
  } else {
    void* _M0L7_2aSomeS1287 = _M0L7contentS1286;
    void* _M0L4_2axS1288 = _M0L7_2aSomeS1287;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3130 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1285
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1288, 0, 0, _M0L6_2atmpS3130);
  }
  moonbit_incref(_M0L4wantS1285);
  moonbit_incref(_M0L6actualS1283);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1283, _M0L4wantS1285)
  ) {
    moonbit_string_t _M0L6_2atmpS3126;
    moonbit_string_t _M0L6_2atmpS3816;
    moonbit_string_t _M0L6_2atmpS3125;
    moonbit_string_t _M0L6_2atmpS3815;
    moonbit_string_t _M0L6_2atmpS3123;
    moonbit_string_t _M0L6_2atmpS3124;
    moonbit_string_t _M0L6_2atmpS3814;
    moonbit_string_t _M0L6_2atmpS3122;
    moonbit_string_t _M0L6_2atmpS3813;
    moonbit_string_t _M0L6_2atmpS3119;
    moonbit_string_t _M0L6_2atmpS3121;
    moonbit_string_t _M0L6_2atmpS3120;
    moonbit_string_t _M0L6_2atmpS3812;
    moonbit_string_t _M0L6_2atmpS3118;
    moonbit_string_t _M0L6_2atmpS3811;
    moonbit_string_t _M0L6_2atmpS3115;
    moonbit_string_t _M0L6_2atmpS3117;
    moonbit_string_t _M0L6_2atmpS3116;
    moonbit_string_t _M0L6_2atmpS3810;
    moonbit_string_t _M0L6_2atmpS3114;
    moonbit_string_t _M0L6_2atmpS3809;
    moonbit_string_t _M0L6_2atmpS3113;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3112;
    struct moonbit_result_0 _result_4501;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3126
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1279);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3816
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_64.data, _M0L6_2atmpS3126);
    moonbit_decref(_M0L6_2atmpS3126);
    _M0L6_2atmpS3125 = _M0L6_2atmpS3816;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3815
    = moonbit_add_string(_M0L6_2atmpS3125, (moonbit_string_t)moonbit_string_literal_65.data);
    moonbit_decref(_M0L6_2atmpS3125);
    _M0L6_2atmpS3123 = _M0L6_2atmpS3815;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3124
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1281);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3814 = moonbit_add_string(_M0L6_2atmpS3123, _M0L6_2atmpS3124);
    moonbit_decref(_M0L6_2atmpS3123);
    moonbit_decref(_M0L6_2atmpS3124);
    _M0L6_2atmpS3122 = _M0L6_2atmpS3814;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3813
    = moonbit_add_string(_M0L6_2atmpS3122, (moonbit_string_t)moonbit_string_literal_66.data);
    moonbit_decref(_M0L6_2atmpS3122);
    _M0L6_2atmpS3119 = _M0L6_2atmpS3813;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3121 = _M0MPC16string6String6escape(_M0L4wantS1285);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3120
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3121);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3812 = moonbit_add_string(_M0L6_2atmpS3119, _M0L6_2atmpS3120);
    moonbit_decref(_M0L6_2atmpS3119);
    moonbit_decref(_M0L6_2atmpS3120);
    _M0L6_2atmpS3118 = _M0L6_2atmpS3812;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3811
    = moonbit_add_string(_M0L6_2atmpS3118, (moonbit_string_t)moonbit_string_literal_67.data);
    moonbit_decref(_M0L6_2atmpS3118);
    _M0L6_2atmpS3115 = _M0L6_2atmpS3811;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3117 = _M0MPC16string6String6escape(_M0L6actualS1283);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3116
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3117);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3810 = moonbit_add_string(_M0L6_2atmpS3115, _M0L6_2atmpS3116);
    moonbit_decref(_M0L6_2atmpS3115);
    moonbit_decref(_M0L6_2atmpS3116);
    _M0L6_2atmpS3114 = _M0L6_2atmpS3810;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3809
    = moonbit_add_string(_M0L6_2atmpS3114, (moonbit_string_t)moonbit_string_literal_68.data);
    moonbit_decref(_M0L6_2atmpS3114);
    _M0L6_2atmpS3113 = _M0L6_2atmpS3809;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3112
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3112)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3112)->$0
    = _M0L6_2atmpS3113;
    _result_4501.tag = 0;
    _result_4501.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3112;
    return _result_4501;
  } else {
    int32_t _M0L6_2atmpS3127;
    struct moonbit_result_0 _result_4502;
    moonbit_decref(_M0L4wantS1285);
    moonbit_decref(_M0L6actualS1283);
    moonbit_decref(_M0L9args__locS1281);
    moonbit_decref(_M0L3locS1279);
    _M0L6_2atmpS3127 = 0;
    _result_4502.tag = 1;
    _result_4502.data.ok = _M0L6_2atmpS3127;
    return _result_4502;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1278,
  int32_t _M0L13escape__slashS1250,
  int32_t _M0L6indentS1245,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1271
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1237;
  void** _M0L6_2atmpS3111;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1238;
  int32_t _M0Lm5depthS1239;
  void* _M0L6_2atmpS3110;
  void* _M0L8_2aparamS1240;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1237 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS3111 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1238
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1238)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1238->$0 = _M0L6_2atmpS3111;
  _M0L5stackS1238->$1 = 0;
  _M0Lm5depthS1239 = 0;
  _M0L6_2atmpS3110 = _M0L4selfS1278;
  _M0L8_2aparamS1240 = _M0L6_2atmpS3110;
  _2aloop_1256:;
  while (1) {
    if (_M0L8_2aparamS1240 == 0) {
      int32_t _M0L3lenS3072;
      if (_M0L8_2aparamS1240) {
        moonbit_decref(_M0L8_2aparamS1240);
      }
      _M0L3lenS3072 = _M0L5stackS1238->$1;
      if (_M0L3lenS3072 == 0) {
        if (_M0L8replacerS1271) {
          moonbit_decref(_M0L8replacerS1271);
        }
        moonbit_decref(_M0L5stackS1238);
        break;
      } else {
        void** _M0L8_2afieldS3824 = _M0L5stackS1238->$0;
        void** _M0L3bufS3096 = _M0L8_2afieldS3824;
        int32_t _M0L3lenS3098 = _M0L5stackS1238->$1;
        int32_t _M0L6_2atmpS3097 = _M0L3lenS3098 - 1;
        void* _M0L6_2atmpS3823 = (void*)_M0L3bufS3096[_M0L6_2atmpS3097];
        void* _M0L4_2axS1257 = _M0L6_2atmpS3823;
        switch (Moonbit_object_tag(_M0L4_2axS1257)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1258 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1257;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3819 =
              _M0L8_2aArrayS1258->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1259 =
              _M0L8_2afieldS3819;
            int32_t _M0L4_2aiS1260 = _M0L8_2aArrayS1258->$1;
            int32_t _M0L3lenS3084 = _M0L6_2aarrS1259->$1;
            if (_M0L4_2aiS1260 < _M0L3lenS3084) {
              int32_t _if__result_4504;
              void** _M0L8_2afieldS3818;
              void** _M0L3bufS3090;
              void* _M0L6_2atmpS3817;
              void* _M0L7elementS1261;
              int32_t _M0L6_2atmpS3085;
              void* _M0L6_2atmpS3088;
              if (_M0L4_2aiS1260 < 0) {
                _if__result_4504 = 1;
              } else {
                int32_t _M0L3lenS3089 = _M0L6_2aarrS1259->$1;
                _if__result_4504 = _M0L4_2aiS1260 >= _M0L3lenS3089;
              }
              if (_if__result_4504) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3818 = _M0L6_2aarrS1259->$0;
              _M0L3bufS3090 = _M0L8_2afieldS3818;
              _M0L6_2atmpS3817 = (void*)_M0L3bufS3090[_M0L4_2aiS1260];
              _M0L7elementS1261 = _M0L6_2atmpS3817;
              _M0L6_2atmpS3085 = _M0L4_2aiS1260 + 1;
              _M0L8_2aArrayS1258->$1 = _M0L6_2atmpS3085;
              if (_M0L4_2aiS1260 > 0) {
                int32_t _M0L6_2atmpS3087;
                moonbit_string_t _M0L6_2atmpS3086;
                moonbit_incref(_M0L7elementS1261);
                moonbit_incref(_M0L3bufS1237);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 44);
                _M0L6_2atmpS3087 = _M0Lm5depthS1239;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3086
                = _M0FPC14json11indent__str(_M0L6_2atmpS3087, _M0L6indentS1245);
                moonbit_incref(_M0L3bufS1237);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3086);
              } else {
                moonbit_incref(_M0L7elementS1261);
              }
              _M0L6_2atmpS3088 = _M0L7elementS1261;
              _M0L8_2aparamS1240 = _M0L6_2atmpS3088;
              goto _2aloop_1256;
            } else {
              int32_t _M0L6_2atmpS3091 = _M0Lm5depthS1239;
              void* _M0L6_2atmpS3092;
              int32_t _M0L6_2atmpS3094;
              moonbit_string_t _M0L6_2atmpS3093;
              void* _M0L6_2atmpS3095;
              _M0Lm5depthS1239 = _M0L6_2atmpS3091 - 1;
              moonbit_incref(_M0L5stackS1238);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3092
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1238);
              if (_M0L6_2atmpS3092) {
                moonbit_decref(_M0L6_2atmpS3092);
              }
              _M0L6_2atmpS3094 = _M0Lm5depthS1239;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3093
              = _M0FPC14json11indent__str(_M0L6_2atmpS3094, _M0L6indentS1245);
              moonbit_incref(_M0L3bufS1237);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3093);
              moonbit_incref(_M0L3bufS1237);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 93);
              _M0L6_2atmpS3095 = 0;
              _M0L8_2aparamS1240 = _M0L6_2atmpS3095;
              goto _2aloop_1256;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1262 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1257;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3822 =
              _M0L9_2aObjectS1262->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1263 =
              _M0L8_2afieldS3822;
            int32_t _M0L8_2afirstS1264 = _M0L9_2aObjectS1262->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1265;
            moonbit_incref(_M0L11_2aiteratorS1263);
            moonbit_incref(_M0L9_2aObjectS1262);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1265
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1263);
            if (_M0L7_2abindS1265 == 0) {
              int32_t _M0L6_2atmpS3073;
              void* _M0L6_2atmpS3074;
              int32_t _M0L6_2atmpS3076;
              moonbit_string_t _M0L6_2atmpS3075;
              void* _M0L6_2atmpS3077;
              if (_M0L7_2abindS1265) {
                moonbit_decref(_M0L7_2abindS1265);
              }
              moonbit_decref(_M0L9_2aObjectS1262);
              _M0L6_2atmpS3073 = _M0Lm5depthS1239;
              _M0Lm5depthS1239 = _M0L6_2atmpS3073 - 1;
              moonbit_incref(_M0L5stackS1238);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3074
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1238);
              if (_M0L6_2atmpS3074) {
                moonbit_decref(_M0L6_2atmpS3074);
              }
              _M0L6_2atmpS3076 = _M0Lm5depthS1239;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3075
              = _M0FPC14json11indent__str(_M0L6_2atmpS3076, _M0L6indentS1245);
              moonbit_incref(_M0L3bufS1237);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3075);
              moonbit_incref(_M0L3bufS1237);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 125);
              _M0L6_2atmpS3077 = 0;
              _M0L8_2aparamS1240 = _M0L6_2atmpS3077;
              goto _2aloop_1256;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1266 = _M0L7_2abindS1265;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1267 = _M0L7_2aSomeS1266;
              moonbit_string_t _M0L8_2afieldS3821 = _M0L4_2axS1267->$0;
              moonbit_string_t _M0L4_2akS1268 = _M0L8_2afieldS3821;
              void* _M0L8_2afieldS3820 = _M0L4_2axS1267->$1;
              int32_t _M0L6_2acntS4325 =
                Moonbit_object_header(_M0L4_2axS1267)->rc;
              void* _M0L4_2avS1269;
              void* _M0Lm2v2S1270;
              moonbit_string_t _M0L6_2atmpS3081;
              void* _M0L6_2atmpS3083;
              void* _M0L6_2atmpS3082;
              if (_M0L6_2acntS4325 > 1) {
                int32_t _M0L11_2anew__cntS4326 = _M0L6_2acntS4325 - 1;
                Moonbit_object_header(_M0L4_2axS1267)->rc
                = _M0L11_2anew__cntS4326;
                moonbit_incref(_M0L8_2afieldS3820);
                moonbit_incref(_M0L4_2akS1268);
              } else if (_M0L6_2acntS4325 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1267);
              }
              _M0L4_2avS1269 = _M0L8_2afieldS3820;
              _M0Lm2v2S1270 = _M0L4_2avS1269;
              if (_M0L8replacerS1271 == 0) {
                moonbit_incref(_M0Lm2v2S1270);
                moonbit_decref(_M0L4_2avS1269);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1272 =
                  _M0L8replacerS1271;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1273 =
                  _M0L7_2aSomeS1272;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1274 =
                  _M0L11_2areplacerS1273;
                void* _M0L7_2abindS1275;
                moonbit_incref(_M0L7_2afuncS1274);
                moonbit_incref(_M0L4_2akS1268);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1275
                = _M0L7_2afuncS1274->code(_M0L7_2afuncS1274, _M0L4_2akS1268, _M0L4_2avS1269);
                if (_M0L7_2abindS1275 == 0) {
                  void* _M0L6_2atmpS3078;
                  if (_M0L7_2abindS1275) {
                    moonbit_decref(_M0L7_2abindS1275);
                  }
                  moonbit_decref(_M0L4_2akS1268);
                  moonbit_decref(_M0L9_2aObjectS1262);
                  _M0L6_2atmpS3078 = 0;
                  _M0L8_2aparamS1240 = _M0L6_2atmpS3078;
                  goto _2aloop_1256;
                } else {
                  void* _M0L7_2aSomeS1276 = _M0L7_2abindS1275;
                  void* _M0L4_2avS1277 = _M0L7_2aSomeS1276;
                  _M0Lm2v2S1270 = _M0L4_2avS1277;
                }
              }
              if (!_M0L8_2afirstS1264) {
                int32_t _M0L6_2atmpS3080;
                moonbit_string_t _M0L6_2atmpS3079;
                moonbit_incref(_M0L3bufS1237);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 44);
                _M0L6_2atmpS3080 = _M0Lm5depthS1239;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3079
                = _M0FPC14json11indent__str(_M0L6_2atmpS3080, _M0L6indentS1245);
                moonbit_incref(_M0L3bufS1237);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3079);
              }
              moonbit_incref(_M0L3bufS1237);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3081
              = _M0FPC14json6escape(_M0L4_2akS1268, _M0L13escape__slashS1250);
              moonbit_incref(_M0L3bufS1237);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3081);
              moonbit_incref(_M0L3bufS1237);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 34);
              moonbit_incref(_M0L3bufS1237);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 58);
              if (_M0L6indentS1245 > 0) {
                moonbit_incref(_M0L3bufS1237);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 32);
              }
              _M0L9_2aObjectS1262->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1262);
              _M0L6_2atmpS3083 = _M0Lm2v2S1270;
              _M0L6_2atmpS3082 = _M0L6_2atmpS3083;
              _M0L8_2aparamS1240 = _M0L6_2atmpS3082;
              goto _2aloop_1256;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1241 = _M0L8_2aparamS1240;
      void* _M0L8_2avalueS1242 = _M0L7_2aSomeS1241;
      void* _M0L6_2atmpS3109;
      switch (Moonbit_object_tag(_M0L8_2avalueS1242)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1243 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1242;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3825 =
            _M0L9_2aObjectS1243->$0;
          int32_t _M0L6_2acntS4327 =
            Moonbit_object_header(_M0L9_2aObjectS1243)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1244;
          if (_M0L6_2acntS4327 > 1) {
            int32_t _M0L11_2anew__cntS4328 = _M0L6_2acntS4327 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1243)->rc
            = _M0L11_2anew__cntS4328;
            moonbit_incref(_M0L8_2afieldS3825);
          } else if (_M0L6_2acntS4327 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1243);
          }
          _M0L10_2amembersS1244 = _M0L8_2afieldS3825;
          moonbit_incref(_M0L10_2amembersS1244);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1244)) {
            moonbit_decref(_M0L10_2amembersS1244);
            moonbit_incref(_M0L3bufS1237);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, (moonbit_string_t)moonbit_string_literal_69.data);
          } else {
            int32_t _M0L6_2atmpS3104 = _M0Lm5depthS1239;
            int32_t _M0L6_2atmpS3106;
            moonbit_string_t _M0L6_2atmpS3105;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS3108;
            void* _M0L6ObjectS3107;
            _M0Lm5depthS1239 = _M0L6_2atmpS3104 + 1;
            moonbit_incref(_M0L3bufS1237);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 123);
            _M0L6_2atmpS3106 = _M0Lm5depthS1239;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3105
            = _M0FPC14json11indent__str(_M0L6_2atmpS3106, _M0L6indentS1245);
            moonbit_incref(_M0L3bufS1237);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3105);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3108
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1244);
            _M0L6ObjectS3107
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS3107)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3107)->$0
            = _M0L6_2atmpS3108;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3107)->$1
            = 1;
            moonbit_incref(_M0L5stackS1238);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1238, _M0L6ObjectS3107);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1246 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1242;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3826 =
            _M0L8_2aArrayS1246->$0;
          int32_t _M0L6_2acntS4329 =
            Moonbit_object_header(_M0L8_2aArrayS1246)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1247;
          if (_M0L6_2acntS4329 > 1) {
            int32_t _M0L11_2anew__cntS4330 = _M0L6_2acntS4329 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1246)->rc
            = _M0L11_2anew__cntS4330;
            moonbit_incref(_M0L8_2afieldS3826);
          } else if (_M0L6_2acntS4329 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1246);
          }
          _M0L6_2aarrS1247 = _M0L8_2afieldS3826;
          moonbit_incref(_M0L6_2aarrS1247);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1247)) {
            moonbit_decref(_M0L6_2aarrS1247);
            moonbit_incref(_M0L3bufS1237);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, (moonbit_string_t)moonbit_string_literal_70.data);
          } else {
            int32_t _M0L6_2atmpS3100 = _M0Lm5depthS1239;
            int32_t _M0L6_2atmpS3102;
            moonbit_string_t _M0L6_2atmpS3101;
            void* _M0L5ArrayS3103;
            _M0Lm5depthS1239 = _M0L6_2atmpS3100 + 1;
            moonbit_incref(_M0L3bufS1237);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 91);
            _M0L6_2atmpS3102 = _M0Lm5depthS1239;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3101
            = _M0FPC14json11indent__str(_M0L6_2atmpS3102, _M0L6indentS1245);
            moonbit_incref(_M0L3bufS1237);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3101);
            _M0L5ArrayS3103
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS3103)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3103)->$0
            = _M0L6_2aarrS1247;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3103)->$1
            = 0;
            moonbit_incref(_M0L5stackS1238);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1238, _M0L5ArrayS3103);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1248 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1242;
          moonbit_string_t _M0L8_2afieldS3827 = _M0L9_2aStringS1248->$0;
          int32_t _M0L6_2acntS4331 =
            Moonbit_object_header(_M0L9_2aStringS1248)->rc;
          moonbit_string_t _M0L4_2asS1249;
          moonbit_string_t _M0L6_2atmpS3099;
          if (_M0L6_2acntS4331 > 1) {
            int32_t _M0L11_2anew__cntS4332 = _M0L6_2acntS4331 - 1;
            Moonbit_object_header(_M0L9_2aStringS1248)->rc
            = _M0L11_2anew__cntS4332;
            moonbit_incref(_M0L8_2afieldS3827);
          } else if (_M0L6_2acntS4331 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1248);
          }
          _M0L4_2asS1249 = _M0L8_2afieldS3827;
          moonbit_incref(_M0L3bufS1237);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3099
          = _M0FPC14json6escape(_M0L4_2asS1249, _M0L13escape__slashS1250);
          moonbit_incref(_M0L3bufS1237);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L6_2atmpS3099);
          moonbit_incref(_M0L3bufS1237);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1237, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1251 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1242;
          double _M0L4_2anS1252 = _M0L9_2aNumberS1251->$0;
          moonbit_string_t _M0L8_2afieldS3828 = _M0L9_2aNumberS1251->$1;
          int32_t _M0L6_2acntS4333 =
            Moonbit_object_header(_M0L9_2aNumberS1251)->rc;
          moonbit_string_t _M0L7_2areprS1253;
          if (_M0L6_2acntS4333 > 1) {
            int32_t _M0L11_2anew__cntS4334 = _M0L6_2acntS4333 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1251)->rc
            = _M0L11_2anew__cntS4334;
            if (_M0L8_2afieldS3828) {
              moonbit_incref(_M0L8_2afieldS3828);
            }
          } else if (_M0L6_2acntS4333 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1251);
          }
          _M0L7_2areprS1253 = _M0L8_2afieldS3828;
          if (_M0L7_2areprS1253 == 0) {
            if (_M0L7_2areprS1253) {
              moonbit_decref(_M0L7_2areprS1253);
            }
            moonbit_incref(_M0L3bufS1237);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1237, _M0L4_2anS1252);
          } else {
            moonbit_string_t _M0L7_2aSomeS1254 = _M0L7_2areprS1253;
            moonbit_string_t _M0L4_2arS1255 = _M0L7_2aSomeS1254;
            moonbit_incref(_M0L3bufS1237);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, _M0L4_2arS1255);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1237);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, (moonbit_string_t)moonbit_string_literal_71.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1237);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, (moonbit_string_t)moonbit_string_literal_72.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1242);
          moonbit_incref(_M0L3bufS1237);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1237, (moonbit_string_t)moonbit_string_literal_73.data);
          break;
        }
      }
      _M0L6_2atmpS3109 = 0;
      _M0L8_2aparamS1240 = _M0L6_2atmpS3109;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1237);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1236,
  int32_t _M0L6indentS1234
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1234 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1235 = _M0L6indentS1234 * _M0L5levelS1236;
    switch (_M0L6spacesS1235) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_74.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_75.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_76.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_77.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_78.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_79.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_80.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_81.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_82.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS3071;
        moonbit_string_t _M0L6_2atmpS3829;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3071
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_83.data, _M0L6spacesS1235);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3829
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_74.data, _M0L6_2atmpS3071);
        moonbit_decref(_M0L6_2atmpS3071);
        return _M0L6_2atmpS3829;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1226,
  int32_t _M0L13escape__slashS1231
) {
  int32_t _M0L6_2atmpS3070;
  struct _M0TPB13StringBuilder* _M0L3bufS1225;
  struct _M0TWEOc* _M0L5_2aitS1227;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3070 = Moonbit_array_length(_M0L3strS1226);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1225 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3070);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1227 = _M0MPC16string6String4iter(_M0L3strS1226);
  while (1) {
    int32_t _M0L7_2abindS1228;
    moonbit_incref(_M0L5_2aitS1227);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1228 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1227);
    if (_M0L7_2abindS1228 == -1) {
      moonbit_decref(_M0L5_2aitS1227);
    } else {
      int32_t _M0L7_2aSomeS1229 = _M0L7_2abindS1228;
      int32_t _M0L4_2acS1230 = _M0L7_2aSomeS1229;
      if (_M0L4_2acS1230 == 34) {
        moonbit_incref(_M0L3bufS1225);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_84.data);
      } else if (_M0L4_2acS1230 == 92) {
        moonbit_incref(_M0L3bufS1225);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_85.data);
      } else if (_M0L4_2acS1230 == 47) {
        if (_M0L13escape__slashS1231) {
          moonbit_incref(_M0L3bufS1225);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_86.data);
        } else {
          moonbit_incref(_M0L3bufS1225);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1225, _M0L4_2acS1230);
        }
      } else if (_M0L4_2acS1230 == 10) {
        moonbit_incref(_M0L3bufS1225);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_87.data);
      } else if (_M0L4_2acS1230 == 13) {
        moonbit_incref(_M0L3bufS1225);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_88.data);
      } else if (_M0L4_2acS1230 == 8) {
        moonbit_incref(_M0L3bufS1225);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_89.data);
      } else if (_M0L4_2acS1230 == 9) {
        moonbit_incref(_M0L3bufS1225);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_90.data);
      } else {
        int32_t _M0L4codeS1232 = _M0L4_2acS1230;
        if (_M0L4codeS1232 == 12) {
          moonbit_incref(_M0L3bufS1225);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_91.data);
        } else if (_M0L4codeS1232 < 32) {
          int32_t _M0L6_2atmpS3069;
          moonbit_string_t _M0L6_2atmpS3068;
          moonbit_incref(_M0L3bufS1225);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, (moonbit_string_t)moonbit_string_literal_92.data);
          _M0L6_2atmpS3069 = _M0L4codeS1232 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3068 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS3069);
          moonbit_incref(_M0L3bufS1225);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1225, _M0L6_2atmpS3068);
        } else {
          moonbit_incref(_M0L3bufS1225);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1225, _M0L4_2acS1230);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1225);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1224
) {
  int32_t _M0L8_2afieldS3830;
  int32_t _M0L3lenS3067;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3830 = _M0L4selfS1224->$1;
  moonbit_decref(_M0L4selfS1224);
  _M0L3lenS3067 = _M0L8_2afieldS3830;
  return _M0L3lenS3067 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1221
) {
  int32_t _M0L3lenS1220;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1220 = _M0L4selfS1221->$1;
  if (_M0L3lenS1220 == 0) {
    moonbit_decref(_M0L4selfS1221);
    return 0;
  } else {
    int32_t _M0L5indexS1222 = _M0L3lenS1220 - 1;
    void** _M0L8_2afieldS3834 = _M0L4selfS1221->$0;
    void** _M0L3bufS3066 = _M0L8_2afieldS3834;
    void* _M0L6_2atmpS3833 = (void*)_M0L3bufS3066[_M0L5indexS1222];
    void* _M0L1vS1223 = _M0L6_2atmpS3833;
    void** _M0L8_2afieldS3832 = _M0L4selfS1221->$0;
    void** _M0L3bufS3065 = _M0L8_2afieldS3832;
    void* _M0L6_2aoldS3831;
    if (
      _M0L5indexS1222 < 0
      || _M0L5indexS1222 >= Moonbit_array_length(_M0L3bufS3065)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3831 = (void*)_M0L3bufS3065[_M0L5indexS1222];
    moonbit_incref(_M0L1vS1223);
    moonbit_decref(_M0L6_2aoldS3831);
    if (
      _M0L5indexS1222 < 0
      || _M0L5indexS1222 >= Moonbit_array_length(_M0L3bufS3065)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3065[_M0L5indexS1222]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1221->$1 = _M0L5indexS1222;
    moonbit_decref(_M0L4selfS1221);
    return _M0L1vS1223;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1218,
  struct _M0TPB6Logger _M0L6loggerS1219
) {
  moonbit_string_t _M0L6_2atmpS3064;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS3063;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3064 = _M0L4selfS1218;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3063 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS3064);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS3063, _M0L6loggerS1219);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1195,
  struct _M0TPB6Logger _M0L6loggerS1217
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3843;
  struct _M0TPC16string10StringView _M0L3pkgS1194;
  moonbit_string_t _M0L7_2adataS1196;
  int32_t _M0L8_2astartS1197;
  int32_t _M0L6_2atmpS3062;
  int32_t _M0L6_2aendS1198;
  int32_t _M0Lm9_2acursorS1199;
  int32_t _M0Lm13accept__stateS1200;
  int32_t _M0Lm10match__endS1201;
  int32_t _M0Lm20match__tag__saver__0S1202;
  int32_t _M0Lm6tag__0S1203;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1204;
  struct _M0TPC16string10StringView _M0L8_2afieldS3842;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1213;
  void* _M0L8_2afieldS3841;
  int32_t _M0L6_2acntS4335;
  void* _M0L16_2apackage__nameS1214;
  struct _M0TPC16string10StringView _M0L8_2afieldS3839;
  struct _M0TPC16string10StringView _M0L8filenameS3039;
  struct _M0TPC16string10StringView _M0L8_2afieldS3838;
  struct _M0TPC16string10StringView _M0L11start__lineS3040;
  struct _M0TPC16string10StringView _M0L8_2afieldS3837;
  struct _M0TPC16string10StringView _M0L13start__columnS3041;
  struct _M0TPC16string10StringView _M0L8_2afieldS3836;
  struct _M0TPC16string10StringView _M0L9end__lineS3042;
  struct _M0TPC16string10StringView _M0L8_2afieldS3835;
  int32_t _M0L6_2acntS4339;
  struct _M0TPC16string10StringView _M0L11end__columnS3043;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3843
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1195->$0_1, _M0L4selfS1195->$0_2, _M0L4selfS1195->$0_0
  };
  _M0L3pkgS1194 = _M0L8_2afieldS3843;
  moonbit_incref(_M0L3pkgS1194.$0);
  moonbit_incref(_M0L3pkgS1194.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1196 = _M0MPC16string10StringView4data(_M0L3pkgS1194);
  moonbit_incref(_M0L3pkgS1194.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1197
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1194);
  moonbit_incref(_M0L3pkgS1194.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3062 = _M0MPC16string10StringView6length(_M0L3pkgS1194);
  _M0L6_2aendS1198 = _M0L8_2astartS1197 + _M0L6_2atmpS3062;
  _M0Lm9_2acursorS1199 = _M0L8_2astartS1197;
  _M0Lm13accept__stateS1200 = -1;
  _M0Lm10match__endS1201 = -1;
  _M0Lm20match__tag__saver__0S1202 = -1;
  _M0Lm6tag__0S1203 = -1;
  while (1) {
    int32_t _M0L6_2atmpS3054 = _M0Lm9_2acursorS1199;
    if (_M0L6_2atmpS3054 < _M0L6_2aendS1198) {
      int32_t _M0L6_2atmpS3061 = _M0Lm9_2acursorS1199;
      int32_t _M0L10next__charS1208;
      int32_t _M0L6_2atmpS3055;
      moonbit_incref(_M0L7_2adataS1196);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1208
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1196, _M0L6_2atmpS3061);
      _M0L6_2atmpS3055 = _M0Lm9_2acursorS1199;
      _M0Lm9_2acursorS1199 = _M0L6_2atmpS3055 + 1;
      if (_M0L10next__charS1208 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS3056;
          _M0Lm6tag__0S1203 = _M0Lm9_2acursorS1199;
          _M0L6_2atmpS3056 = _M0Lm9_2acursorS1199;
          if (_M0L6_2atmpS3056 < _M0L6_2aendS1198) {
            int32_t _M0L6_2atmpS3060 = _M0Lm9_2acursorS1199;
            int32_t _M0L10next__charS1209;
            int32_t _M0L6_2atmpS3057;
            moonbit_incref(_M0L7_2adataS1196);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1209
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1196, _M0L6_2atmpS3060);
            _M0L6_2atmpS3057 = _M0Lm9_2acursorS1199;
            _M0Lm9_2acursorS1199 = _M0L6_2atmpS3057 + 1;
            if (_M0L10next__charS1209 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS3058 = _M0Lm9_2acursorS1199;
                if (_M0L6_2atmpS3058 < _M0L6_2aendS1198) {
                  int32_t _M0L6_2atmpS3059 = _M0Lm9_2acursorS1199;
                  _M0Lm9_2acursorS1199 = _M0L6_2atmpS3059 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1202 = _M0Lm6tag__0S1203;
                  _M0Lm13accept__stateS1200 = 0;
                  _M0Lm10match__endS1201 = _M0Lm9_2acursorS1199;
                  goto join_1205;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1205;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1205;
    }
    break;
  }
  goto joinlet_4506;
  join_1205:;
  switch (_M0Lm13accept__stateS1200) {
    case 0: {
      int32_t _M0L6_2atmpS3052;
      int32_t _M0L6_2atmpS3051;
      int64_t _M0L6_2atmpS3048;
      int32_t _M0L6_2atmpS3050;
      int64_t _M0L6_2atmpS3049;
      struct _M0TPC16string10StringView _M0L13package__nameS1206;
      int64_t _M0L6_2atmpS3045;
      int32_t _M0L6_2atmpS3047;
      int64_t _M0L6_2atmpS3046;
      struct _M0TPC16string10StringView _M0L12module__nameS1207;
      void* _M0L4SomeS3044;
      moonbit_decref(_M0L3pkgS1194.$0);
      _M0L6_2atmpS3052 = _M0Lm20match__tag__saver__0S1202;
      _M0L6_2atmpS3051 = _M0L6_2atmpS3052 + 1;
      _M0L6_2atmpS3048 = (int64_t)_M0L6_2atmpS3051;
      _M0L6_2atmpS3050 = _M0Lm10match__endS1201;
      _M0L6_2atmpS3049 = (int64_t)_M0L6_2atmpS3050;
      moonbit_incref(_M0L7_2adataS1196);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1206
      = _M0MPC16string6String4view(_M0L7_2adataS1196, _M0L6_2atmpS3048, _M0L6_2atmpS3049);
      _M0L6_2atmpS3045 = (int64_t)_M0L8_2astartS1197;
      _M0L6_2atmpS3047 = _M0Lm20match__tag__saver__0S1202;
      _M0L6_2atmpS3046 = (int64_t)_M0L6_2atmpS3047;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1207
      = _M0MPC16string6String4view(_M0L7_2adataS1196, _M0L6_2atmpS3045, _M0L6_2atmpS3046);
      _M0L4SomeS3044
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS3044)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3044)->$0_0
      = _M0L13package__nameS1206.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3044)->$0_1
      = _M0L13package__nameS1206.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3044)->$0_2
      = _M0L13package__nameS1206.$2;
      _M0L7_2abindS1204
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1204)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1204->$0_0 = _M0L12module__nameS1207.$0;
      _M0L7_2abindS1204->$0_1 = _M0L12module__nameS1207.$1;
      _M0L7_2abindS1204->$0_2 = _M0L12module__nameS1207.$2;
      _M0L7_2abindS1204->$1 = _M0L4SomeS3044;
      break;
    }
    default: {
      void* _M0L4NoneS3053;
      moonbit_decref(_M0L7_2adataS1196);
      _M0L4NoneS3053
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1204
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1204)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1204->$0_0 = _M0L3pkgS1194.$0;
      _M0L7_2abindS1204->$0_1 = _M0L3pkgS1194.$1;
      _M0L7_2abindS1204->$0_2 = _M0L3pkgS1194.$2;
      _M0L7_2abindS1204->$1 = _M0L4NoneS3053;
      break;
    }
  }
  joinlet_4506:;
  _M0L8_2afieldS3842
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1204->$0_1, _M0L7_2abindS1204->$0_2, _M0L7_2abindS1204->$0_0
  };
  _M0L15_2amodule__nameS1213 = _M0L8_2afieldS3842;
  _M0L8_2afieldS3841 = _M0L7_2abindS1204->$1;
  _M0L6_2acntS4335 = Moonbit_object_header(_M0L7_2abindS1204)->rc;
  if (_M0L6_2acntS4335 > 1) {
    int32_t _M0L11_2anew__cntS4336 = _M0L6_2acntS4335 - 1;
    Moonbit_object_header(_M0L7_2abindS1204)->rc = _M0L11_2anew__cntS4336;
    moonbit_incref(_M0L8_2afieldS3841);
    moonbit_incref(_M0L15_2amodule__nameS1213.$0);
  } else if (_M0L6_2acntS4335 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1204);
  }
  _M0L16_2apackage__nameS1214 = _M0L8_2afieldS3841;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1214)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1215 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1214;
      struct _M0TPC16string10StringView _M0L8_2afieldS3840 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1215->$0_1,
                                              _M0L7_2aSomeS1215->$0_2,
                                              _M0L7_2aSomeS1215->$0_0};
      int32_t _M0L6_2acntS4337 = Moonbit_object_header(_M0L7_2aSomeS1215)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1216;
      if (_M0L6_2acntS4337 > 1) {
        int32_t _M0L11_2anew__cntS4338 = _M0L6_2acntS4337 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1215)->rc = _M0L11_2anew__cntS4338;
        moonbit_incref(_M0L8_2afieldS3840.$0);
      } else if (_M0L6_2acntS4337 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1215);
      }
      _M0L12_2apkg__nameS1216 = _M0L8_2afieldS3840;
      if (_M0L6loggerS1217.$1) {
        moonbit_incref(_M0L6loggerS1217.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1217.$0->$method_2(_M0L6loggerS1217.$1, _M0L12_2apkg__nameS1216);
      if (_M0L6loggerS1217.$1) {
        moonbit_incref(_M0L6loggerS1217.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1217.$0->$method_3(_M0L6loggerS1217.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1214);
      break;
    }
  }
  _M0L8_2afieldS3839
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1195->$1_1, _M0L4selfS1195->$1_2, _M0L4selfS1195->$1_0
  };
  _M0L8filenameS3039 = _M0L8_2afieldS3839;
  moonbit_incref(_M0L8filenameS3039.$0);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_2(_M0L6loggerS1217.$1, _M0L8filenameS3039);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_3(_M0L6loggerS1217.$1, 58);
  _M0L8_2afieldS3838
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1195->$2_1, _M0L4selfS1195->$2_2, _M0L4selfS1195->$2_0
  };
  _M0L11start__lineS3040 = _M0L8_2afieldS3838;
  moonbit_incref(_M0L11start__lineS3040.$0);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_2(_M0L6loggerS1217.$1, _M0L11start__lineS3040);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_3(_M0L6loggerS1217.$1, 58);
  _M0L8_2afieldS3837
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1195->$3_1, _M0L4selfS1195->$3_2, _M0L4selfS1195->$3_0
  };
  _M0L13start__columnS3041 = _M0L8_2afieldS3837;
  moonbit_incref(_M0L13start__columnS3041.$0);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_2(_M0L6loggerS1217.$1, _M0L13start__columnS3041);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_3(_M0L6loggerS1217.$1, 45);
  _M0L8_2afieldS3836
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1195->$4_1, _M0L4selfS1195->$4_2, _M0L4selfS1195->$4_0
  };
  _M0L9end__lineS3042 = _M0L8_2afieldS3836;
  moonbit_incref(_M0L9end__lineS3042.$0);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_2(_M0L6loggerS1217.$1, _M0L9end__lineS3042);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_3(_M0L6loggerS1217.$1, 58);
  _M0L8_2afieldS3835
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1195->$5_1, _M0L4selfS1195->$5_2, _M0L4selfS1195->$5_0
  };
  _M0L6_2acntS4339 = Moonbit_object_header(_M0L4selfS1195)->rc;
  if (_M0L6_2acntS4339 > 1) {
    int32_t _M0L11_2anew__cntS4345 = _M0L6_2acntS4339 - 1;
    Moonbit_object_header(_M0L4selfS1195)->rc = _M0L11_2anew__cntS4345;
    moonbit_incref(_M0L8_2afieldS3835.$0);
  } else if (_M0L6_2acntS4339 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4344 =
      (struct _M0TPC16string10StringView){_M0L4selfS1195->$4_1,
                                            _M0L4selfS1195->$4_2,
                                            _M0L4selfS1195->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4343;
    struct _M0TPC16string10StringView _M0L8_2afieldS4342;
    struct _M0TPC16string10StringView _M0L8_2afieldS4341;
    struct _M0TPC16string10StringView _M0L8_2afieldS4340;
    moonbit_decref(_M0L8_2afieldS4344.$0);
    _M0L8_2afieldS4343
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1195->$3_1, _M0L4selfS1195->$3_2, _M0L4selfS1195->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4343.$0);
    _M0L8_2afieldS4342
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1195->$2_1, _M0L4selfS1195->$2_2, _M0L4selfS1195->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4342.$0);
    _M0L8_2afieldS4341
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1195->$1_1, _M0L4selfS1195->$1_2, _M0L4selfS1195->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4341.$0);
    _M0L8_2afieldS4340
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1195->$0_1, _M0L4selfS1195->$0_2, _M0L4selfS1195->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4340.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1195);
  }
  _M0L11end__columnS3043 = _M0L8_2afieldS3835;
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_2(_M0L6loggerS1217.$1, _M0L11end__columnS3043);
  if (_M0L6loggerS1217.$1) {
    moonbit_incref(_M0L6loggerS1217.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_3(_M0L6loggerS1217.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1217.$0->$method_2(_M0L6loggerS1217.$1, _M0L15_2amodule__nameS1213);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1193) {
  moonbit_string_t _M0L6_2atmpS3038;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS3038
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1193);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS3038);
  moonbit_decref(_M0L6_2atmpS3038);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1192,
  struct _M0TPB6Logger _M0L6loggerS1191
) {
  moonbit_string_t _M0L6_2atmpS3037;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS3037 = _M0MPC16double6Double10to__string(_M0L4selfS1192);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1191.$0->$method_0(_M0L6loggerS1191.$1, _M0L6_2atmpS3037);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1190) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1190);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1177) {
  uint64_t _M0L4bitsS1178;
  uint64_t _M0L6_2atmpS3036;
  uint64_t _M0L6_2atmpS3035;
  int32_t _M0L8ieeeSignS1179;
  uint64_t _M0L12ieeeMantissaS1180;
  uint64_t _M0L6_2atmpS3034;
  uint64_t _M0L6_2atmpS3033;
  int32_t _M0L12ieeeExponentS1181;
  int32_t _if__result_4510;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1182;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1183;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3032;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1177 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_17.data;
  }
  _M0L4bitsS1178 = *(int64_t*)&_M0L3valS1177;
  _M0L6_2atmpS3036 = _M0L4bitsS1178 >> 63;
  _M0L6_2atmpS3035 = _M0L6_2atmpS3036 & 1ull;
  _M0L8ieeeSignS1179 = _M0L6_2atmpS3035 != 0ull;
  _M0L12ieeeMantissaS1180 = _M0L4bitsS1178 & 4503599627370495ull;
  _M0L6_2atmpS3034 = _M0L4bitsS1178 >> 52;
  _M0L6_2atmpS3033 = _M0L6_2atmpS3034 & 2047ull;
  _M0L12ieeeExponentS1181 = (int32_t)_M0L6_2atmpS3033;
  if (_M0L12ieeeExponentS1181 == 2047) {
    _if__result_4510 = 1;
  } else if (_M0L12ieeeExponentS1181 == 0) {
    _if__result_4510 = _M0L12ieeeMantissaS1180 == 0ull;
  } else {
    _if__result_4510 = 0;
  }
  if (_if__result_4510) {
    int32_t _M0L6_2atmpS3021 = _M0L12ieeeExponentS1181 != 0;
    int32_t _M0L6_2atmpS3022 = _M0L12ieeeMantissaS1180 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1179, _M0L6_2atmpS3021, _M0L6_2atmpS3022);
  }
  _M0Lm1vS1182 = _M0FPB31ryu__to__string_2erecord_2f1176;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1183
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1180, _M0L12ieeeExponentS1181);
  if (_M0L5smallS1183 == 0) {
    uint32_t _M0L6_2atmpS3023;
    if (_M0L5smallS1183) {
      moonbit_decref(_M0L5smallS1183);
    }
    _M0L6_2atmpS3023 = *(uint32_t*)&_M0L12ieeeExponentS1181;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1182 = _M0FPB3d2d(_M0L12ieeeMantissaS1180, _M0L6_2atmpS3023);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1184 = _M0L5smallS1183;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1185 = _M0L7_2aSomeS1184;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1186 = _M0L4_2afS1185;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3031 = _M0Lm1xS1186;
      uint64_t _M0L8_2afieldS3846 = _M0L6_2atmpS3031->$0;
      uint64_t _M0L8mantissaS3030 = _M0L8_2afieldS3846;
      uint64_t _M0L1qS1187 = _M0L8mantissaS3030 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3029 = _M0Lm1xS1186;
      uint64_t _M0L8_2afieldS3845 = _M0L6_2atmpS3029->$0;
      uint64_t _M0L8mantissaS3027 = _M0L8_2afieldS3845;
      uint64_t _M0L6_2atmpS3028 = 10ull * _M0L1qS1187;
      uint64_t _M0L1rS1188 = _M0L8mantissaS3027 - _M0L6_2atmpS3028;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3026;
      int32_t _M0L8_2afieldS3844;
      int32_t _M0L8exponentS3025;
      int32_t _M0L6_2atmpS3024;
      if (_M0L1rS1188 != 0ull) {
        break;
      }
      _M0L6_2atmpS3026 = _M0Lm1xS1186;
      _M0L8_2afieldS3844 = _M0L6_2atmpS3026->$1;
      moonbit_decref(_M0L6_2atmpS3026);
      _M0L8exponentS3025 = _M0L8_2afieldS3844;
      _M0L6_2atmpS3024 = _M0L8exponentS3025 + 1;
      _M0Lm1xS1186
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1186)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1186->$0 = _M0L1qS1187;
      _M0Lm1xS1186->$1 = _M0L6_2atmpS3024;
      continue;
      break;
    }
    _M0Lm1vS1182 = _M0Lm1xS1186;
  }
  _M0L6_2atmpS3032 = _M0Lm1vS1182;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS3032, _M0L8ieeeSignS1179);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1171,
  int32_t _M0L12ieeeExponentS1173
) {
  uint64_t _M0L2m2S1170;
  int32_t _M0L6_2atmpS3020;
  int32_t _M0L2e2S1172;
  int32_t _M0L6_2atmpS3019;
  uint64_t _M0L6_2atmpS3018;
  uint64_t _M0L4maskS1174;
  uint64_t _M0L8fractionS1175;
  int32_t _M0L6_2atmpS3017;
  uint64_t _M0L6_2atmpS3016;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3015;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1170 = 4503599627370496ull | _M0L12ieeeMantissaS1171;
  _M0L6_2atmpS3020 = _M0L12ieeeExponentS1173 - 1023;
  _M0L2e2S1172 = _M0L6_2atmpS3020 - 52;
  if (_M0L2e2S1172 > 0) {
    return 0;
  }
  if (_M0L2e2S1172 < -52) {
    return 0;
  }
  _M0L6_2atmpS3019 = -_M0L2e2S1172;
  _M0L6_2atmpS3018 = 1ull << (_M0L6_2atmpS3019 & 63);
  _M0L4maskS1174 = _M0L6_2atmpS3018 - 1ull;
  _M0L8fractionS1175 = _M0L2m2S1170 & _M0L4maskS1174;
  if (_M0L8fractionS1175 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3017 = -_M0L2e2S1172;
  _M0L6_2atmpS3016 = _M0L2m2S1170 >> (_M0L6_2atmpS3017 & 63);
  _M0L6_2atmpS3015
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3015)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS3015->$0 = _M0L6_2atmpS3016;
  _M0L6_2atmpS3015->$1 = 0;
  return _M0L6_2atmpS3015;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1144,
  int32_t _M0L4signS1142
) {
  int32_t _M0L6_2atmpS3014;
  moonbit_bytes_t _M0L6resultS1140;
  int32_t _M0Lm5indexS1141;
  uint64_t _M0Lm6outputS1143;
  uint64_t _M0L6_2atmpS3013;
  int32_t _M0L7olengthS1145;
  int32_t _M0L8_2afieldS3847;
  int32_t _M0L8exponentS3012;
  int32_t _M0L6_2atmpS3011;
  int32_t _M0Lm3expS1146;
  int32_t _M0L6_2atmpS3010;
  int32_t _M0L6_2atmpS3008;
  int32_t _M0L18scientificNotationS1147;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3014 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1140
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3014);
  _M0Lm5indexS1141 = 0;
  if (_M0L4signS1142) {
    int32_t _M0L6_2atmpS2883 = _M0Lm5indexS1141;
    int32_t _M0L6_2atmpS2884;
    if (
      _M0L6_2atmpS2883 < 0
      || _M0L6_2atmpS2883 >= Moonbit_array_length(_M0L6resultS1140)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1140[_M0L6_2atmpS2883] = 45;
    _M0L6_2atmpS2884 = _M0Lm5indexS1141;
    _M0Lm5indexS1141 = _M0L6_2atmpS2884 + 1;
  }
  _M0Lm6outputS1143 = _M0L1vS1144->$0;
  _M0L6_2atmpS3013 = _M0Lm6outputS1143;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1145 = _M0FPB17decimal__length17(_M0L6_2atmpS3013);
  _M0L8_2afieldS3847 = _M0L1vS1144->$1;
  moonbit_decref(_M0L1vS1144);
  _M0L8exponentS3012 = _M0L8_2afieldS3847;
  _M0L6_2atmpS3011 = _M0L8exponentS3012 + _M0L7olengthS1145;
  _M0Lm3expS1146 = _M0L6_2atmpS3011 - 1;
  _M0L6_2atmpS3010 = _M0Lm3expS1146;
  if (_M0L6_2atmpS3010 >= -6) {
    int32_t _M0L6_2atmpS3009 = _M0Lm3expS1146;
    _M0L6_2atmpS3008 = _M0L6_2atmpS3009 < 21;
  } else {
    _M0L6_2atmpS3008 = 0;
  }
  _M0L18scientificNotationS1147 = !_M0L6_2atmpS3008;
  if (_M0L18scientificNotationS1147) {
    int32_t _M0L7_2abindS1148 = _M0L7olengthS1145 - 1;
    int32_t _M0L1iS1149 = 0;
    int32_t _M0L6_2atmpS2894;
    uint64_t _M0L6_2atmpS2899;
    int32_t _M0L6_2atmpS2898;
    int32_t _M0L6_2atmpS2897;
    int32_t _M0L6_2atmpS2896;
    int32_t _M0L6_2atmpS2895;
    int32_t _M0L6_2atmpS2903;
    int32_t _M0L6_2atmpS2904;
    int32_t _M0L6_2atmpS2905;
    int32_t _M0L6_2atmpS2906;
    int32_t _M0L6_2atmpS2907;
    int32_t _M0L6_2atmpS2913;
    int32_t _M0L6_2atmpS2946;
    while (1) {
      if (_M0L1iS1149 < _M0L7_2abindS1148) {
        uint64_t _M0L6_2atmpS2892 = _M0Lm6outputS1143;
        uint64_t _M0L1cS1150 = _M0L6_2atmpS2892 % 10ull;
        uint64_t _M0L6_2atmpS2885 = _M0Lm6outputS1143;
        int32_t _M0L6_2atmpS2891;
        int32_t _M0L6_2atmpS2890;
        int32_t _M0L6_2atmpS2886;
        int32_t _M0L6_2atmpS2889;
        int32_t _M0L6_2atmpS2888;
        int32_t _M0L6_2atmpS2887;
        int32_t _M0L6_2atmpS2893;
        _M0Lm6outputS1143 = _M0L6_2atmpS2885 / 10ull;
        _M0L6_2atmpS2891 = _M0Lm5indexS1141;
        _M0L6_2atmpS2890 = _M0L6_2atmpS2891 + _M0L7olengthS1145;
        _M0L6_2atmpS2886 = _M0L6_2atmpS2890 - _M0L1iS1149;
        _M0L6_2atmpS2889 = (int32_t)_M0L1cS1150;
        _M0L6_2atmpS2888 = 48 + _M0L6_2atmpS2889;
        _M0L6_2atmpS2887 = _M0L6_2atmpS2888 & 0xff;
        if (
          _M0L6_2atmpS2886 < 0
          || _M0L6_2atmpS2886 >= Moonbit_array_length(_M0L6resultS1140)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1140[_M0L6_2atmpS2886] = _M0L6_2atmpS2887;
        _M0L6_2atmpS2893 = _M0L1iS1149 + 1;
        _M0L1iS1149 = _M0L6_2atmpS2893;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2894 = _M0Lm5indexS1141;
    _M0L6_2atmpS2899 = _M0Lm6outputS1143;
    _M0L6_2atmpS2898 = (int32_t)_M0L6_2atmpS2899;
    _M0L6_2atmpS2897 = _M0L6_2atmpS2898 % 10;
    _M0L6_2atmpS2896 = 48 + _M0L6_2atmpS2897;
    _M0L6_2atmpS2895 = _M0L6_2atmpS2896 & 0xff;
    if (
      _M0L6_2atmpS2894 < 0
      || _M0L6_2atmpS2894 >= Moonbit_array_length(_M0L6resultS1140)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1140[_M0L6_2atmpS2894] = _M0L6_2atmpS2895;
    if (_M0L7olengthS1145 > 1) {
      int32_t _M0L6_2atmpS2901 = _M0Lm5indexS1141;
      int32_t _M0L6_2atmpS2900 = _M0L6_2atmpS2901 + 1;
      if (
        _M0L6_2atmpS2900 < 0
        || _M0L6_2atmpS2900 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2900] = 46;
    } else {
      int32_t _M0L6_2atmpS2902 = _M0Lm5indexS1141;
      _M0Lm5indexS1141 = _M0L6_2atmpS2902 - 1;
    }
    _M0L6_2atmpS2903 = _M0Lm5indexS1141;
    _M0L6_2atmpS2904 = _M0L7olengthS1145 + 1;
    _M0Lm5indexS1141 = _M0L6_2atmpS2903 + _M0L6_2atmpS2904;
    _M0L6_2atmpS2905 = _M0Lm5indexS1141;
    if (
      _M0L6_2atmpS2905 < 0
      || _M0L6_2atmpS2905 >= Moonbit_array_length(_M0L6resultS1140)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1140[_M0L6_2atmpS2905] = 101;
    _M0L6_2atmpS2906 = _M0Lm5indexS1141;
    _M0Lm5indexS1141 = _M0L6_2atmpS2906 + 1;
    _M0L6_2atmpS2907 = _M0Lm3expS1146;
    if (_M0L6_2atmpS2907 < 0) {
      int32_t _M0L6_2atmpS2908 = _M0Lm5indexS1141;
      int32_t _M0L6_2atmpS2909;
      int32_t _M0L6_2atmpS2910;
      if (
        _M0L6_2atmpS2908 < 0
        || _M0L6_2atmpS2908 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2908] = 45;
      _M0L6_2atmpS2909 = _M0Lm5indexS1141;
      _M0Lm5indexS1141 = _M0L6_2atmpS2909 + 1;
      _M0L6_2atmpS2910 = _M0Lm3expS1146;
      _M0Lm3expS1146 = -_M0L6_2atmpS2910;
    } else {
      int32_t _M0L6_2atmpS2911 = _M0Lm5indexS1141;
      int32_t _M0L6_2atmpS2912;
      if (
        _M0L6_2atmpS2911 < 0
        || _M0L6_2atmpS2911 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2911] = 43;
      _M0L6_2atmpS2912 = _M0Lm5indexS1141;
      _M0Lm5indexS1141 = _M0L6_2atmpS2912 + 1;
    }
    _M0L6_2atmpS2913 = _M0Lm3expS1146;
    if (_M0L6_2atmpS2913 >= 100) {
      int32_t _M0L6_2atmpS2929 = _M0Lm3expS1146;
      int32_t _M0L1aS1152 = _M0L6_2atmpS2929 / 100;
      int32_t _M0L6_2atmpS2928 = _M0Lm3expS1146;
      int32_t _M0L6_2atmpS2927 = _M0L6_2atmpS2928 / 10;
      int32_t _M0L1bS1153 = _M0L6_2atmpS2927 % 10;
      int32_t _M0L6_2atmpS2926 = _M0Lm3expS1146;
      int32_t _M0L1cS1154 = _M0L6_2atmpS2926 % 10;
      int32_t _M0L6_2atmpS2914 = _M0Lm5indexS1141;
      int32_t _M0L6_2atmpS2916 = 48 + _M0L1aS1152;
      int32_t _M0L6_2atmpS2915 = _M0L6_2atmpS2916 & 0xff;
      int32_t _M0L6_2atmpS2920;
      int32_t _M0L6_2atmpS2917;
      int32_t _M0L6_2atmpS2919;
      int32_t _M0L6_2atmpS2918;
      int32_t _M0L6_2atmpS2924;
      int32_t _M0L6_2atmpS2921;
      int32_t _M0L6_2atmpS2923;
      int32_t _M0L6_2atmpS2922;
      int32_t _M0L6_2atmpS2925;
      if (
        _M0L6_2atmpS2914 < 0
        || _M0L6_2atmpS2914 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2914] = _M0L6_2atmpS2915;
      _M0L6_2atmpS2920 = _M0Lm5indexS1141;
      _M0L6_2atmpS2917 = _M0L6_2atmpS2920 + 1;
      _M0L6_2atmpS2919 = 48 + _M0L1bS1153;
      _M0L6_2atmpS2918 = _M0L6_2atmpS2919 & 0xff;
      if (
        _M0L6_2atmpS2917 < 0
        || _M0L6_2atmpS2917 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2917] = _M0L6_2atmpS2918;
      _M0L6_2atmpS2924 = _M0Lm5indexS1141;
      _M0L6_2atmpS2921 = _M0L6_2atmpS2924 + 2;
      _M0L6_2atmpS2923 = 48 + _M0L1cS1154;
      _M0L6_2atmpS2922 = _M0L6_2atmpS2923 & 0xff;
      if (
        _M0L6_2atmpS2921 < 0
        || _M0L6_2atmpS2921 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2921] = _M0L6_2atmpS2922;
      _M0L6_2atmpS2925 = _M0Lm5indexS1141;
      _M0Lm5indexS1141 = _M0L6_2atmpS2925 + 3;
    } else {
      int32_t _M0L6_2atmpS2930 = _M0Lm3expS1146;
      if (_M0L6_2atmpS2930 >= 10) {
        int32_t _M0L6_2atmpS2940 = _M0Lm3expS1146;
        int32_t _M0L1aS1155 = _M0L6_2atmpS2940 / 10;
        int32_t _M0L6_2atmpS2939 = _M0Lm3expS1146;
        int32_t _M0L1bS1156 = _M0L6_2atmpS2939 % 10;
        int32_t _M0L6_2atmpS2931 = _M0Lm5indexS1141;
        int32_t _M0L6_2atmpS2933 = 48 + _M0L1aS1155;
        int32_t _M0L6_2atmpS2932 = _M0L6_2atmpS2933 & 0xff;
        int32_t _M0L6_2atmpS2937;
        int32_t _M0L6_2atmpS2934;
        int32_t _M0L6_2atmpS2936;
        int32_t _M0L6_2atmpS2935;
        int32_t _M0L6_2atmpS2938;
        if (
          _M0L6_2atmpS2931 < 0
          || _M0L6_2atmpS2931 >= Moonbit_array_length(_M0L6resultS1140)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1140[_M0L6_2atmpS2931] = _M0L6_2atmpS2932;
        _M0L6_2atmpS2937 = _M0Lm5indexS1141;
        _M0L6_2atmpS2934 = _M0L6_2atmpS2937 + 1;
        _M0L6_2atmpS2936 = 48 + _M0L1bS1156;
        _M0L6_2atmpS2935 = _M0L6_2atmpS2936 & 0xff;
        if (
          _M0L6_2atmpS2934 < 0
          || _M0L6_2atmpS2934 >= Moonbit_array_length(_M0L6resultS1140)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1140[_M0L6_2atmpS2934] = _M0L6_2atmpS2935;
        _M0L6_2atmpS2938 = _M0Lm5indexS1141;
        _M0Lm5indexS1141 = _M0L6_2atmpS2938 + 2;
      } else {
        int32_t _M0L6_2atmpS2941 = _M0Lm5indexS1141;
        int32_t _M0L6_2atmpS2944 = _M0Lm3expS1146;
        int32_t _M0L6_2atmpS2943 = 48 + _M0L6_2atmpS2944;
        int32_t _M0L6_2atmpS2942 = _M0L6_2atmpS2943 & 0xff;
        int32_t _M0L6_2atmpS2945;
        if (
          _M0L6_2atmpS2941 < 0
          || _M0L6_2atmpS2941 >= Moonbit_array_length(_M0L6resultS1140)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1140[_M0L6_2atmpS2941] = _M0L6_2atmpS2942;
        _M0L6_2atmpS2945 = _M0Lm5indexS1141;
        _M0Lm5indexS1141 = _M0L6_2atmpS2945 + 1;
      }
    }
    _M0L6_2atmpS2946 = _M0Lm5indexS1141;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1140, 0, _M0L6_2atmpS2946);
  } else {
    int32_t _M0L6_2atmpS2947 = _M0Lm3expS1146;
    int32_t _M0L6_2atmpS3007;
    if (_M0L6_2atmpS2947 < 0) {
      int32_t _M0L6_2atmpS2948 = _M0Lm5indexS1141;
      int32_t _M0L6_2atmpS2949;
      int32_t _M0L6_2atmpS2950;
      int32_t _M0L6_2atmpS2951;
      int32_t _M0L1iS1157;
      int32_t _M0L7currentS1159;
      int32_t _M0L1iS1160;
      if (
        _M0L6_2atmpS2948 < 0
        || _M0L6_2atmpS2948 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2948] = 48;
      _M0L6_2atmpS2949 = _M0Lm5indexS1141;
      _M0Lm5indexS1141 = _M0L6_2atmpS2949 + 1;
      _M0L6_2atmpS2950 = _M0Lm5indexS1141;
      if (
        _M0L6_2atmpS2950 < 0
        || _M0L6_2atmpS2950 >= Moonbit_array_length(_M0L6resultS1140)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1140[_M0L6_2atmpS2950] = 46;
      _M0L6_2atmpS2951 = _M0Lm5indexS1141;
      _M0Lm5indexS1141 = _M0L6_2atmpS2951 + 1;
      _M0L1iS1157 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2952 = _M0Lm3expS1146;
        if (_M0L1iS1157 > _M0L6_2atmpS2952) {
          int32_t _M0L6_2atmpS2953 = _M0Lm5indexS1141;
          int32_t _M0L6_2atmpS2954;
          int32_t _M0L6_2atmpS2955;
          if (
            _M0L6_2atmpS2953 < 0
            || _M0L6_2atmpS2953 >= Moonbit_array_length(_M0L6resultS1140)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1140[_M0L6_2atmpS2953] = 48;
          _M0L6_2atmpS2954 = _M0Lm5indexS1141;
          _M0Lm5indexS1141 = _M0L6_2atmpS2954 + 1;
          _M0L6_2atmpS2955 = _M0L1iS1157 - 1;
          _M0L1iS1157 = _M0L6_2atmpS2955;
          continue;
        }
        break;
      }
      _M0L7currentS1159 = _M0Lm5indexS1141;
      _M0L1iS1160 = 0;
      while (1) {
        if (_M0L1iS1160 < _M0L7olengthS1145) {
          int32_t _M0L6_2atmpS2963 = _M0L7currentS1159 + _M0L7olengthS1145;
          int32_t _M0L6_2atmpS2962 = _M0L6_2atmpS2963 - _M0L1iS1160;
          int32_t _M0L6_2atmpS2956 = _M0L6_2atmpS2962 - 1;
          uint64_t _M0L6_2atmpS2961 = _M0Lm6outputS1143;
          uint64_t _M0L6_2atmpS2960 = _M0L6_2atmpS2961 % 10ull;
          int32_t _M0L6_2atmpS2959 = (int32_t)_M0L6_2atmpS2960;
          int32_t _M0L6_2atmpS2958 = 48 + _M0L6_2atmpS2959;
          int32_t _M0L6_2atmpS2957 = _M0L6_2atmpS2958 & 0xff;
          uint64_t _M0L6_2atmpS2964;
          int32_t _M0L6_2atmpS2965;
          int32_t _M0L6_2atmpS2966;
          if (
            _M0L6_2atmpS2956 < 0
            || _M0L6_2atmpS2956 >= Moonbit_array_length(_M0L6resultS1140)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1140[_M0L6_2atmpS2956] = _M0L6_2atmpS2957;
          _M0L6_2atmpS2964 = _M0Lm6outputS1143;
          _M0Lm6outputS1143 = _M0L6_2atmpS2964 / 10ull;
          _M0L6_2atmpS2965 = _M0Lm5indexS1141;
          _M0Lm5indexS1141 = _M0L6_2atmpS2965 + 1;
          _M0L6_2atmpS2966 = _M0L1iS1160 + 1;
          _M0L1iS1160 = _M0L6_2atmpS2966;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2968 = _M0Lm3expS1146;
      int32_t _M0L6_2atmpS2967 = _M0L6_2atmpS2968 + 1;
      if (_M0L6_2atmpS2967 >= _M0L7olengthS1145) {
        int32_t _M0L1iS1162 = 0;
        int32_t _M0L6_2atmpS2980;
        int32_t _M0L6_2atmpS2984;
        int32_t _M0L7_2abindS1164;
        int32_t _M0L2__S1165;
        while (1) {
          if (_M0L1iS1162 < _M0L7olengthS1145) {
            int32_t _M0L6_2atmpS2977 = _M0Lm5indexS1141;
            int32_t _M0L6_2atmpS2976 = _M0L6_2atmpS2977 + _M0L7olengthS1145;
            int32_t _M0L6_2atmpS2975 = _M0L6_2atmpS2976 - _M0L1iS1162;
            int32_t _M0L6_2atmpS2969 = _M0L6_2atmpS2975 - 1;
            uint64_t _M0L6_2atmpS2974 = _M0Lm6outputS1143;
            uint64_t _M0L6_2atmpS2973 = _M0L6_2atmpS2974 % 10ull;
            int32_t _M0L6_2atmpS2972 = (int32_t)_M0L6_2atmpS2973;
            int32_t _M0L6_2atmpS2971 = 48 + _M0L6_2atmpS2972;
            int32_t _M0L6_2atmpS2970 = _M0L6_2atmpS2971 & 0xff;
            uint64_t _M0L6_2atmpS2978;
            int32_t _M0L6_2atmpS2979;
            if (
              _M0L6_2atmpS2969 < 0
              || _M0L6_2atmpS2969 >= Moonbit_array_length(_M0L6resultS1140)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1140[_M0L6_2atmpS2969] = _M0L6_2atmpS2970;
            _M0L6_2atmpS2978 = _M0Lm6outputS1143;
            _M0Lm6outputS1143 = _M0L6_2atmpS2978 / 10ull;
            _M0L6_2atmpS2979 = _M0L1iS1162 + 1;
            _M0L1iS1162 = _M0L6_2atmpS2979;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2980 = _M0Lm5indexS1141;
        _M0Lm5indexS1141 = _M0L6_2atmpS2980 + _M0L7olengthS1145;
        _M0L6_2atmpS2984 = _M0Lm3expS1146;
        _M0L7_2abindS1164 = _M0L6_2atmpS2984 + 1;
        _M0L2__S1165 = _M0L7olengthS1145;
        while (1) {
          if (_M0L2__S1165 < _M0L7_2abindS1164) {
            int32_t _M0L6_2atmpS2981 = _M0Lm5indexS1141;
            int32_t _M0L6_2atmpS2982;
            int32_t _M0L6_2atmpS2983;
            if (
              _M0L6_2atmpS2981 < 0
              || _M0L6_2atmpS2981 >= Moonbit_array_length(_M0L6resultS1140)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1140[_M0L6_2atmpS2981] = 48;
            _M0L6_2atmpS2982 = _M0Lm5indexS1141;
            _M0Lm5indexS1141 = _M0L6_2atmpS2982 + 1;
            _M0L6_2atmpS2983 = _M0L2__S1165 + 1;
            _M0L2__S1165 = _M0L6_2atmpS2983;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS3006 = _M0Lm5indexS1141;
        int32_t _M0Lm7currentS1167 = _M0L6_2atmpS3006 + 1;
        int32_t _M0L1iS1168 = 0;
        int32_t _M0L6_2atmpS3004;
        int32_t _M0L6_2atmpS3005;
        while (1) {
          if (_M0L1iS1168 < _M0L7olengthS1145) {
            int32_t _M0L6_2atmpS2987 = _M0L7olengthS1145 - _M0L1iS1168;
            int32_t _M0L6_2atmpS2985 = _M0L6_2atmpS2987 - 1;
            int32_t _M0L6_2atmpS2986 = _M0Lm3expS1146;
            int32_t _M0L6_2atmpS3001;
            int32_t _M0L6_2atmpS3000;
            int32_t _M0L6_2atmpS2999;
            int32_t _M0L6_2atmpS2993;
            uint64_t _M0L6_2atmpS2998;
            uint64_t _M0L6_2atmpS2997;
            int32_t _M0L6_2atmpS2996;
            int32_t _M0L6_2atmpS2995;
            int32_t _M0L6_2atmpS2994;
            uint64_t _M0L6_2atmpS3002;
            int32_t _M0L6_2atmpS3003;
            if (_M0L6_2atmpS2985 == _M0L6_2atmpS2986) {
              int32_t _M0L6_2atmpS2991 = _M0Lm7currentS1167;
              int32_t _M0L6_2atmpS2990 = _M0L6_2atmpS2991 + _M0L7olengthS1145;
              int32_t _M0L6_2atmpS2989 = _M0L6_2atmpS2990 - _M0L1iS1168;
              int32_t _M0L6_2atmpS2988 = _M0L6_2atmpS2989 - 1;
              int32_t _M0L6_2atmpS2992;
              if (
                _M0L6_2atmpS2988 < 0
                || _M0L6_2atmpS2988 >= Moonbit_array_length(_M0L6resultS1140)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1140[_M0L6_2atmpS2988] = 46;
              _M0L6_2atmpS2992 = _M0Lm7currentS1167;
              _M0Lm7currentS1167 = _M0L6_2atmpS2992 - 1;
            }
            _M0L6_2atmpS3001 = _M0Lm7currentS1167;
            _M0L6_2atmpS3000 = _M0L6_2atmpS3001 + _M0L7olengthS1145;
            _M0L6_2atmpS2999 = _M0L6_2atmpS3000 - _M0L1iS1168;
            _M0L6_2atmpS2993 = _M0L6_2atmpS2999 - 1;
            _M0L6_2atmpS2998 = _M0Lm6outputS1143;
            _M0L6_2atmpS2997 = _M0L6_2atmpS2998 % 10ull;
            _M0L6_2atmpS2996 = (int32_t)_M0L6_2atmpS2997;
            _M0L6_2atmpS2995 = 48 + _M0L6_2atmpS2996;
            _M0L6_2atmpS2994 = _M0L6_2atmpS2995 & 0xff;
            if (
              _M0L6_2atmpS2993 < 0
              || _M0L6_2atmpS2993 >= Moonbit_array_length(_M0L6resultS1140)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1140[_M0L6_2atmpS2993] = _M0L6_2atmpS2994;
            _M0L6_2atmpS3002 = _M0Lm6outputS1143;
            _M0Lm6outputS1143 = _M0L6_2atmpS3002 / 10ull;
            _M0L6_2atmpS3003 = _M0L1iS1168 + 1;
            _M0L1iS1168 = _M0L6_2atmpS3003;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3004 = _M0Lm5indexS1141;
        _M0L6_2atmpS3005 = _M0L7olengthS1145 + 1;
        _M0Lm5indexS1141 = _M0L6_2atmpS3004 + _M0L6_2atmpS3005;
      }
    }
    _M0L6_2atmpS3007 = _M0Lm5indexS1141;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1140, 0, _M0L6_2atmpS3007);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1086,
  uint32_t _M0L12ieeeExponentS1085
) {
  int32_t _M0Lm2e2S1083;
  uint64_t _M0Lm2m2S1084;
  uint64_t _M0L6_2atmpS2882;
  uint64_t _M0L6_2atmpS2881;
  int32_t _M0L4evenS1087;
  uint64_t _M0L6_2atmpS2880;
  uint64_t _M0L2mvS1088;
  int32_t _M0L7mmShiftS1089;
  uint64_t _M0Lm2vrS1090;
  uint64_t _M0Lm2vpS1091;
  uint64_t _M0Lm2vmS1092;
  int32_t _M0Lm3e10S1093;
  int32_t _M0Lm17vmIsTrailingZerosS1094;
  int32_t _M0Lm17vrIsTrailingZerosS1095;
  int32_t _M0L6_2atmpS2782;
  int32_t _M0Lm7removedS1114;
  int32_t _M0Lm16lastRemovedDigitS1115;
  uint64_t _M0Lm6outputS1116;
  int32_t _M0L6_2atmpS2878;
  int32_t _M0L6_2atmpS2879;
  int32_t _M0L3expS1139;
  uint64_t _M0L6_2atmpS2877;
  struct _M0TPB17FloatingDecimal64* _block_4523;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S1083 = 0;
  _M0Lm2m2S1084 = 0ull;
  if (_M0L12ieeeExponentS1085 == 0u) {
    _M0Lm2e2S1083 = -1076;
    _M0Lm2m2S1084 = _M0L12ieeeMantissaS1086;
  } else {
    int32_t _M0L6_2atmpS2781 = *(int32_t*)&_M0L12ieeeExponentS1085;
    int32_t _M0L6_2atmpS2780 = _M0L6_2atmpS2781 - 1023;
    int32_t _M0L6_2atmpS2779 = _M0L6_2atmpS2780 - 52;
    _M0Lm2e2S1083 = _M0L6_2atmpS2779 - 2;
    _M0Lm2m2S1084 = 4503599627370496ull | _M0L12ieeeMantissaS1086;
  }
  _M0L6_2atmpS2882 = _M0Lm2m2S1084;
  _M0L6_2atmpS2881 = _M0L6_2atmpS2882 & 1ull;
  _M0L4evenS1087 = _M0L6_2atmpS2881 == 0ull;
  _M0L6_2atmpS2880 = _M0Lm2m2S1084;
  _M0L2mvS1088 = 4ull * _M0L6_2atmpS2880;
  if (_M0L12ieeeMantissaS1086 != 0ull) {
    _M0L7mmShiftS1089 = 1;
  } else {
    _M0L7mmShiftS1089 = _M0L12ieeeExponentS1085 <= 1u;
  }
  _M0Lm2vrS1090 = 0ull;
  _M0Lm2vpS1091 = 0ull;
  _M0Lm2vmS1092 = 0ull;
  _M0Lm3e10S1093 = 0;
  _M0Lm17vmIsTrailingZerosS1094 = 0;
  _M0Lm17vrIsTrailingZerosS1095 = 0;
  _M0L6_2atmpS2782 = _M0Lm2e2S1083;
  if (_M0L6_2atmpS2782 >= 0) {
    int32_t _M0L6_2atmpS2804 = _M0Lm2e2S1083;
    int32_t _M0L6_2atmpS2800;
    int32_t _M0L6_2atmpS2803;
    int32_t _M0L6_2atmpS2802;
    int32_t _M0L6_2atmpS2801;
    int32_t _M0L1qS1096;
    int32_t _M0L6_2atmpS2799;
    int32_t _M0L6_2atmpS2798;
    int32_t _M0L1kS1097;
    int32_t _M0L6_2atmpS2797;
    int32_t _M0L6_2atmpS2796;
    int32_t _M0L6_2atmpS2795;
    int32_t _M0L1iS1098;
    struct _M0TPB8Pow5Pair _M0L4pow5S1099;
    uint64_t _M0L6_2atmpS2794;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1100;
    uint64_t _M0L8_2avrOutS1101;
    uint64_t _M0L8_2avpOutS1102;
    uint64_t _M0L8_2avmOutS1103;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2800 = _M0FPB9log10Pow2(_M0L6_2atmpS2804);
    _M0L6_2atmpS2803 = _M0Lm2e2S1083;
    _M0L6_2atmpS2802 = _M0L6_2atmpS2803 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2801 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2802);
    _M0L1qS1096 = _M0L6_2atmpS2800 - _M0L6_2atmpS2801;
    _M0Lm3e10S1093 = _M0L1qS1096;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2799 = _M0FPB8pow5bits(_M0L1qS1096);
    _M0L6_2atmpS2798 = 125 + _M0L6_2atmpS2799;
    _M0L1kS1097 = _M0L6_2atmpS2798 - 1;
    _M0L6_2atmpS2797 = _M0Lm2e2S1083;
    _M0L6_2atmpS2796 = -_M0L6_2atmpS2797;
    _M0L6_2atmpS2795 = _M0L6_2atmpS2796 + _M0L1qS1096;
    _M0L1iS1098 = _M0L6_2atmpS2795 + _M0L1kS1097;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1099 = _M0FPB22double__computeInvPow5(_M0L1qS1096);
    _M0L6_2atmpS2794 = _M0Lm2m2S1084;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1100
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2794, _M0L4pow5S1099, _M0L1iS1098, _M0L7mmShiftS1089);
    _M0L8_2avrOutS1101 = _M0L7_2abindS1100.$0;
    _M0L8_2avpOutS1102 = _M0L7_2abindS1100.$1;
    _M0L8_2avmOutS1103 = _M0L7_2abindS1100.$2;
    _M0Lm2vrS1090 = _M0L8_2avrOutS1101;
    _M0Lm2vpS1091 = _M0L8_2avpOutS1102;
    _M0Lm2vmS1092 = _M0L8_2avmOutS1103;
    if (_M0L1qS1096 <= 21) {
      int32_t _M0L6_2atmpS2790 = (int32_t)_M0L2mvS1088;
      uint64_t _M0L6_2atmpS2793 = _M0L2mvS1088 / 5ull;
      int32_t _M0L6_2atmpS2792 = (int32_t)_M0L6_2atmpS2793;
      int32_t _M0L6_2atmpS2791 = 5 * _M0L6_2atmpS2792;
      int32_t _M0L6mvMod5S1104 = _M0L6_2atmpS2790 - _M0L6_2atmpS2791;
      if (_M0L6mvMod5S1104 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1095
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1088, _M0L1qS1096);
      } else if (_M0L4evenS1087) {
        uint64_t _M0L6_2atmpS2784 = _M0L2mvS1088 - 1ull;
        uint64_t _M0L6_2atmpS2785;
        uint64_t _M0L6_2atmpS2783;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2785 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1089);
        _M0L6_2atmpS2783 = _M0L6_2atmpS2784 - _M0L6_2atmpS2785;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1094
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2783, _M0L1qS1096);
      } else {
        uint64_t _M0L6_2atmpS2786 = _M0Lm2vpS1091;
        uint64_t _M0L6_2atmpS2789 = _M0L2mvS1088 + 2ull;
        int32_t _M0L6_2atmpS2788;
        uint64_t _M0L6_2atmpS2787;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2788
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2789, _M0L1qS1096);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2787 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2788);
        _M0Lm2vpS1091 = _M0L6_2atmpS2786 - _M0L6_2atmpS2787;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2818 = _M0Lm2e2S1083;
    int32_t _M0L6_2atmpS2817 = -_M0L6_2atmpS2818;
    int32_t _M0L6_2atmpS2812;
    int32_t _M0L6_2atmpS2816;
    int32_t _M0L6_2atmpS2815;
    int32_t _M0L6_2atmpS2814;
    int32_t _M0L6_2atmpS2813;
    int32_t _M0L1qS1105;
    int32_t _M0L6_2atmpS2805;
    int32_t _M0L6_2atmpS2811;
    int32_t _M0L6_2atmpS2810;
    int32_t _M0L1iS1106;
    int32_t _M0L6_2atmpS2809;
    int32_t _M0L1kS1107;
    int32_t _M0L1jS1108;
    struct _M0TPB8Pow5Pair _M0L4pow5S1109;
    uint64_t _M0L6_2atmpS2808;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1110;
    uint64_t _M0L8_2avrOutS1111;
    uint64_t _M0L8_2avpOutS1112;
    uint64_t _M0L8_2avmOutS1113;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2812 = _M0FPB9log10Pow5(_M0L6_2atmpS2817);
    _M0L6_2atmpS2816 = _M0Lm2e2S1083;
    _M0L6_2atmpS2815 = -_M0L6_2atmpS2816;
    _M0L6_2atmpS2814 = _M0L6_2atmpS2815 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2813 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2814);
    _M0L1qS1105 = _M0L6_2atmpS2812 - _M0L6_2atmpS2813;
    _M0L6_2atmpS2805 = _M0Lm2e2S1083;
    _M0Lm3e10S1093 = _M0L1qS1105 + _M0L6_2atmpS2805;
    _M0L6_2atmpS2811 = _M0Lm2e2S1083;
    _M0L6_2atmpS2810 = -_M0L6_2atmpS2811;
    _M0L1iS1106 = _M0L6_2atmpS2810 - _M0L1qS1105;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2809 = _M0FPB8pow5bits(_M0L1iS1106);
    _M0L1kS1107 = _M0L6_2atmpS2809 - 125;
    _M0L1jS1108 = _M0L1qS1105 - _M0L1kS1107;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1109 = _M0FPB19double__computePow5(_M0L1iS1106);
    _M0L6_2atmpS2808 = _M0Lm2m2S1084;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1110
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2808, _M0L4pow5S1109, _M0L1jS1108, _M0L7mmShiftS1089);
    _M0L8_2avrOutS1111 = _M0L7_2abindS1110.$0;
    _M0L8_2avpOutS1112 = _M0L7_2abindS1110.$1;
    _M0L8_2avmOutS1113 = _M0L7_2abindS1110.$2;
    _M0Lm2vrS1090 = _M0L8_2avrOutS1111;
    _M0Lm2vpS1091 = _M0L8_2avpOutS1112;
    _M0Lm2vmS1092 = _M0L8_2avmOutS1113;
    if (_M0L1qS1105 <= 1) {
      _M0Lm17vrIsTrailingZerosS1095 = 1;
      if (_M0L4evenS1087) {
        int32_t _M0L6_2atmpS2806;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2806 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1089);
        _M0Lm17vmIsTrailingZerosS1094 = _M0L6_2atmpS2806 == 1;
      } else {
        uint64_t _M0L6_2atmpS2807 = _M0Lm2vpS1091;
        _M0Lm2vpS1091 = _M0L6_2atmpS2807 - 1ull;
      }
    } else if (_M0L1qS1105 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1095
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1088, _M0L1qS1105);
    }
  }
  _M0Lm7removedS1114 = 0;
  _M0Lm16lastRemovedDigitS1115 = 0;
  _M0Lm6outputS1116 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1094 || _M0Lm17vrIsTrailingZerosS1095) {
    int32_t _if__result_4520;
    uint64_t _M0L6_2atmpS2848;
    uint64_t _M0L6_2atmpS2854;
    uint64_t _M0L6_2atmpS2855;
    int32_t _if__result_4521;
    int32_t _M0L6_2atmpS2851;
    int64_t _M0L6_2atmpS2850;
    uint64_t _M0L6_2atmpS2849;
    while (1) {
      uint64_t _M0L6_2atmpS2831 = _M0Lm2vpS1091;
      uint64_t _M0L7vpDiv10S1117 = _M0L6_2atmpS2831 / 10ull;
      uint64_t _M0L6_2atmpS2830 = _M0Lm2vmS1092;
      uint64_t _M0L7vmDiv10S1118 = _M0L6_2atmpS2830 / 10ull;
      uint64_t _M0L6_2atmpS2829;
      int32_t _M0L6_2atmpS2826;
      int32_t _M0L6_2atmpS2828;
      int32_t _M0L6_2atmpS2827;
      int32_t _M0L7vmMod10S1120;
      uint64_t _M0L6_2atmpS2825;
      uint64_t _M0L7vrDiv10S1121;
      uint64_t _M0L6_2atmpS2824;
      int32_t _M0L6_2atmpS2821;
      int32_t _M0L6_2atmpS2823;
      int32_t _M0L6_2atmpS2822;
      int32_t _M0L7vrMod10S1122;
      int32_t _M0L6_2atmpS2820;
      if (_M0L7vpDiv10S1117 <= _M0L7vmDiv10S1118) {
        break;
      }
      _M0L6_2atmpS2829 = _M0Lm2vmS1092;
      _M0L6_2atmpS2826 = (int32_t)_M0L6_2atmpS2829;
      _M0L6_2atmpS2828 = (int32_t)_M0L7vmDiv10S1118;
      _M0L6_2atmpS2827 = 10 * _M0L6_2atmpS2828;
      _M0L7vmMod10S1120 = _M0L6_2atmpS2826 - _M0L6_2atmpS2827;
      _M0L6_2atmpS2825 = _M0Lm2vrS1090;
      _M0L7vrDiv10S1121 = _M0L6_2atmpS2825 / 10ull;
      _M0L6_2atmpS2824 = _M0Lm2vrS1090;
      _M0L6_2atmpS2821 = (int32_t)_M0L6_2atmpS2824;
      _M0L6_2atmpS2823 = (int32_t)_M0L7vrDiv10S1121;
      _M0L6_2atmpS2822 = 10 * _M0L6_2atmpS2823;
      _M0L7vrMod10S1122 = _M0L6_2atmpS2821 - _M0L6_2atmpS2822;
      if (_M0Lm17vmIsTrailingZerosS1094) {
        _M0Lm17vmIsTrailingZerosS1094 = _M0L7vmMod10S1120 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1094 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1095) {
        int32_t _M0L6_2atmpS2819 = _M0Lm16lastRemovedDigitS1115;
        _M0Lm17vrIsTrailingZerosS1095 = _M0L6_2atmpS2819 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1095 = 0;
      }
      _M0Lm16lastRemovedDigitS1115 = _M0L7vrMod10S1122;
      _M0Lm2vrS1090 = _M0L7vrDiv10S1121;
      _M0Lm2vpS1091 = _M0L7vpDiv10S1117;
      _M0Lm2vmS1092 = _M0L7vmDiv10S1118;
      _M0L6_2atmpS2820 = _M0Lm7removedS1114;
      _M0Lm7removedS1114 = _M0L6_2atmpS2820 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1094) {
      while (1) {
        uint64_t _M0L6_2atmpS2844 = _M0Lm2vmS1092;
        uint64_t _M0L7vmDiv10S1123 = _M0L6_2atmpS2844 / 10ull;
        uint64_t _M0L6_2atmpS2843 = _M0Lm2vmS1092;
        int32_t _M0L6_2atmpS2840 = (int32_t)_M0L6_2atmpS2843;
        int32_t _M0L6_2atmpS2842 = (int32_t)_M0L7vmDiv10S1123;
        int32_t _M0L6_2atmpS2841 = 10 * _M0L6_2atmpS2842;
        int32_t _M0L7vmMod10S1124 = _M0L6_2atmpS2840 - _M0L6_2atmpS2841;
        uint64_t _M0L6_2atmpS2839;
        uint64_t _M0L7vpDiv10S1126;
        uint64_t _M0L6_2atmpS2838;
        uint64_t _M0L7vrDiv10S1127;
        uint64_t _M0L6_2atmpS2837;
        int32_t _M0L6_2atmpS2834;
        int32_t _M0L6_2atmpS2836;
        int32_t _M0L6_2atmpS2835;
        int32_t _M0L7vrMod10S1128;
        int32_t _M0L6_2atmpS2833;
        if (_M0L7vmMod10S1124 != 0) {
          break;
        }
        _M0L6_2atmpS2839 = _M0Lm2vpS1091;
        _M0L7vpDiv10S1126 = _M0L6_2atmpS2839 / 10ull;
        _M0L6_2atmpS2838 = _M0Lm2vrS1090;
        _M0L7vrDiv10S1127 = _M0L6_2atmpS2838 / 10ull;
        _M0L6_2atmpS2837 = _M0Lm2vrS1090;
        _M0L6_2atmpS2834 = (int32_t)_M0L6_2atmpS2837;
        _M0L6_2atmpS2836 = (int32_t)_M0L7vrDiv10S1127;
        _M0L6_2atmpS2835 = 10 * _M0L6_2atmpS2836;
        _M0L7vrMod10S1128 = _M0L6_2atmpS2834 - _M0L6_2atmpS2835;
        if (_M0Lm17vrIsTrailingZerosS1095) {
          int32_t _M0L6_2atmpS2832 = _M0Lm16lastRemovedDigitS1115;
          _M0Lm17vrIsTrailingZerosS1095 = _M0L6_2atmpS2832 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1095 = 0;
        }
        _M0Lm16lastRemovedDigitS1115 = _M0L7vrMod10S1128;
        _M0Lm2vrS1090 = _M0L7vrDiv10S1127;
        _M0Lm2vpS1091 = _M0L7vpDiv10S1126;
        _M0Lm2vmS1092 = _M0L7vmDiv10S1123;
        _M0L6_2atmpS2833 = _M0Lm7removedS1114;
        _M0Lm7removedS1114 = _M0L6_2atmpS2833 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1095) {
      int32_t _M0L6_2atmpS2847 = _M0Lm16lastRemovedDigitS1115;
      if (_M0L6_2atmpS2847 == 5) {
        uint64_t _M0L6_2atmpS2846 = _M0Lm2vrS1090;
        uint64_t _M0L6_2atmpS2845 = _M0L6_2atmpS2846 % 2ull;
        _if__result_4520 = _M0L6_2atmpS2845 == 0ull;
      } else {
        _if__result_4520 = 0;
      }
    } else {
      _if__result_4520 = 0;
    }
    if (_if__result_4520) {
      _M0Lm16lastRemovedDigitS1115 = 4;
    }
    _M0L6_2atmpS2848 = _M0Lm2vrS1090;
    _M0L6_2atmpS2854 = _M0Lm2vrS1090;
    _M0L6_2atmpS2855 = _M0Lm2vmS1092;
    if (_M0L6_2atmpS2854 == _M0L6_2atmpS2855) {
      if (!_M0L4evenS1087) {
        _if__result_4521 = 1;
      } else {
        int32_t _M0L6_2atmpS2853 = _M0Lm17vmIsTrailingZerosS1094;
        _if__result_4521 = !_M0L6_2atmpS2853;
      }
    } else {
      _if__result_4521 = 0;
    }
    if (_if__result_4521) {
      _M0L6_2atmpS2851 = 1;
    } else {
      int32_t _M0L6_2atmpS2852 = _M0Lm16lastRemovedDigitS1115;
      _M0L6_2atmpS2851 = _M0L6_2atmpS2852 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2850 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2851);
    _M0L6_2atmpS2849 = *(uint64_t*)&_M0L6_2atmpS2850;
    _M0Lm6outputS1116 = _M0L6_2atmpS2848 + _M0L6_2atmpS2849;
  } else {
    int32_t _M0Lm7roundUpS1129 = 0;
    uint64_t _M0L6_2atmpS2876 = _M0Lm2vpS1091;
    uint64_t _M0L8vpDiv100S1130 = _M0L6_2atmpS2876 / 100ull;
    uint64_t _M0L6_2atmpS2875 = _M0Lm2vmS1092;
    uint64_t _M0L8vmDiv100S1131 = _M0L6_2atmpS2875 / 100ull;
    uint64_t _M0L6_2atmpS2870;
    uint64_t _M0L6_2atmpS2873;
    uint64_t _M0L6_2atmpS2874;
    int32_t _M0L6_2atmpS2872;
    uint64_t _M0L6_2atmpS2871;
    if (_M0L8vpDiv100S1130 > _M0L8vmDiv100S1131) {
      uint64_t _M0L6_2atmpS2861 = _M0Lm2vrS1090;
      uint64_t _M0L8vrDiv100S1132 = _M0L6_2atmpS2861 / 100ull;
      uint64_t _M0L6_2atmpS2860 = _M0Lm2vrS1090;
      int32_t _M0L6_2atmpS2857 = (int32_t)_M0L6_2atmpS2860;
      int32_t _M0L6_2atmpS2859 = (int32_t)_M0L8vrDiv100S1132;
      int32_t _M0L6_2atmpS2858 = 100 * _M0L6_2atmpS2859;
      int32_t _M0L8vrMod100S1133 = _M0L6_2atmpS2857 - _M0L6_2atmpS2858;
      int32_t _M0L6_2atmpS2856;
      _M0Lm7roundUpS1129 = _M0L8vrMod100S1133 >= 50;
      _M0Lm2vrS1090 = _M0L8vrDiv100S1132;
      _M0Lm2vpS1091 = _M0L8vpDiv100S1130;
      _M0Lm2vmS1092 = _M0L8vmDiv100S1131;
      _M0L6_2atmpS2856 = _M0Lm7removedS1114;
      _M0Lm7removedS1114 = _M0L6_2atmpS2856 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2869 = _M0Lm2vpS1091;
      uint64_t _M0L7vpDiv10S1134 = _M0L6_2atmpS2869 / 10ull;
      uint64_t _M0L6_2atmpS2868 = _M0Lm2vmS1092;
      uint64_t _M0L7vmDiv10S1135 = _M0L6_2atmpS2868 / 10ull;
      uint64_t _M0L6_2atmpS2867;
      uint64_t _M0L7vrDiv10S1137;
      uint64_t _M0L6_2atmpS2866;
      int32_t _M0L6_2atmpS2863;
      int32_t _M0L6_2atmpS2865;
      int32_t _M0L6_2atmpS2864;
      int32_t _M0L7vrMod10S1138;
      int32_t _M0L6_2atmpS2862;
      if (_M0L7vpDiv10S1134 <= _M0L7vmDiv10S1135) {
        break;
      }
      _M0L6_2atmpS2867 = _M0Lm2vrS1090;
      _M0L7vrDiv10S1137 = _M0L6_2atmpS2867 / 10ull;
      _M0L6_2atmpS2866 = _M0Lm2vrS1090;
      _M0L6_2atmpS2863 = (int32_t)_M0L6_2atmpS2866;
      _M0L6_2atmpS2865 = (int32_t)_M0L7vrDiv10S1137;
      _M0L6_2atmpS2864 = 10 * _M0L6_2atmpS2865;
      _M0L7vrMod10S1138 = _M0L6_2atmpS2863 - _M0L6_2atmpS2864;
      _M0Lm7roundUpS1129 = _M0L7vrMod10S1138 >= 5;
      _M0Lm2vrS1090 = _M0L7vrDiv10S1137;
      _M0Lm2vpS1091 = _M0L7vpDiv10S1134;
      _M0Lm2vmS1092 = _M0L7vmDiv10S1135;
      _M0L6_2atmpS2862 = _M0Lm7removedS1114;
      _M0Lm7removedS1114 = _M0L6_2atmpS2862 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2870 = _M0Lm2vrS1090;
    _M0L6_2atmpS2873 = _M0Lm2vrS1090;
    _M0L6_2atmpS2874 = _M0Lm2vmS1092;
    _M0L6_2atmpS2872
    = _M0L6_2atmpS2873 == _M0L6_2atmpS2874 || _M0Lm7roundUpS1129;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2871 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2872);
    _M0Lm6outputS1116 = _M0L6_2atmpS2870 + _M0L6_2atmpS2871;
  }
  _M0L6_2atmpS2878 = _M0Lm3e10S1093;
  _M0L6_2atmpS2879 = _M0Lm7removedS1114;
  _M0L3expS1139 = _M0L6_2atmpS2878 + _M0L6_2atmpS2879;
  _M0L6_2atmpS2877 = _M0Lm6outputS1116;
  _block_4523
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4523)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4523->$0 = _M0L6_2atmpS2877;
  _block_4523->$1 = _M0L3expS1139;
  return _block_4523;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1082) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1082) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1081) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1081) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1080) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1080) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1079) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS1079 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1079 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1079 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1079 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1079 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1079 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1079 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1079 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1079 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1079 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1079 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1079 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1079 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1079 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1079 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1079 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1062) {
  int32_t _M0L6_2atmpS2778;
  int32_t _M0L6_2atmpS2777;
  int32_t _M0L4baseS1061;
  int32_t _M0L5base2S1063;
  int32_t _M0L6offsetS1064;
  int32_t _M0L6_2atmpS2776;
  uint64_t _M0L4mul0S1065;
  int32_t _M0L6_2atmpS2775;
  int32_t _M0L6_2atmpS2774;
  uint64_t _M0L4mul1S1066;
  uint64_t _M0L1mS1067;
  struct _M0TPB7Umul128 _M0L7_2abindS1068;
  uint64_t _M0L7_2alow1S1069;
  uint64_t _M0L8_2ahigh1S1070;
  struct _M0TPB7Umul128 _M0L7_2abindS1071;
  uint64_t _M0L7_2alow0S1072;
  uint64_t _M0L8_2ahigh0S1073;
  uint64_t _M0L3sumS1074;
  uint64_t _M0Lm5high1S1075;
  int32_t _M0L6_2atmpS2772;
  int32_t _M0L6_2atmpS2773;
  int32_t _M0L5deltaS1076;
  uint64_t _M0L6_2atmpS2771;
  uint64_t _M0L6_2atmpS2763;
  int32_t _M0L6_2atmpS2770;
  uint32_t _M0L6_2atmpS2767;
  int32_t _M0L6_2atmpS2769;
  int32_t _M0L6_2atmpS2768;
  uint32_t _M0L6_2atmpS2766;
  uint32_t _M0L6_2atmpS2765;
  uint64_t _M0L6_2atmpS2764;
  uint64_t _M0L1aS1077;
  uint64_t _M0L6_2atmpS2762;
  uint64_t _M0L1bS1078;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2778 = _M0L1iS1062 + 26;
  _M0L6_2atmpS2777 = _M0L6_2atmpS2778 - 1;
  _M0L4baseS1061 = _M0L6_2atmpS2777 / 26;
  _M0L5base2S1063 = _M0L4baseS1061 * 26;
  _M0L6offsetS1064 = _M0L5base2S1063 - _M0L1iS1062;
  _M0L6_2atmpS2776 = _M0L4baseS1061 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1065
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2776);
  _M0L6_2atmpS2775 = _M0L4baseS1061 * 2;
  _M0L6_2atmpS2774 = _M0L6_2atmpS2775 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1066
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2774);
  if (_M0L6offsetS1064 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1065, _M0L4mul1S1066};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1067
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1064);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1068 = _M0FPB7umul128(_M0L1mS1067, _M0L4mul1S1066);
  _M0L7_2alow1S1069 = _M0L7_2abindS1068.$0;
  _M0L8_2ahigh1S1070 = _M0L7_2abindS1068.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1071 = _M0FPB7umul128(_M0L1mS1067, _M0L4mul0S1065);
  _M0L7_2alow0S1072 = _M0L7_2abindS1071.$0;
  _M0L8_2ahigh0S1073 = _M0L7_2abindS1071.$1;
  _M0L3sumS1074 = _M0L8_2ahigh0S1073 + _M0L7_2alow1S1069;
  _M0Lm5high1S1075 = _M0L8_2ahigh1S1070;
  if (_M0L3sumS1074 < _M0L8_2ahigh0S1073) {
    uint64_t _M0L6_2atmpS2761 = _M0Lm5high1S1075;
    _M0Lm5high1S1075 = _M0L6_2atmpS2761 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2772 = _M0FPB8pow5bits(_M0L5base2S1063);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2773 = _M0FPB8pow5bits(_M0L1iS1062);
  _M0L5deltaS1076 = _M0L6_2atmpS2772 - _M0L6_2atmpS2773;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2771
  = _M0FPB13shiftright128(_M0L7_2alow0S1072, _M0L3sumS1074, _M0L5deltaS1076);
  _M0L6_2atmpS2763 = _M0L6_2atmpS2771 + 1ull;
  _M0L6_2atmpS2770 = _M0L1iS1062 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2767
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2770);
  _M0L6_2atmpS2769 = _M0L1iS1062 % 16;
  _M0L6_2atmpS2768 = _M0L6_2atmpS2769 << 1;
  _M0L6_2atmpS2766 = _M0L6_2atmpS2767 >> (_M0L6_2atmpS2768 & 31);
  _M0L6_2atmpS2765 = _M0L6_2atmpS2766 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2764 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2765);
  _M0L1aS1077 = _M0L6_2atmpS2763 + _M0L6_2atmpS2764;
  _M0L6_2atmpS2762 = _M0Lm5high1S1075;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1078
  = _M0FPB13shiftright128(_M0L3sumS1074, _M0L6_2atmpS2762, _M0L5deltaS1076);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1077, _M0L1bS1078};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1044) {
  int32_t _M0L4baseS1043;
  int32_t _M0L5base2S1045;
  int32_t _M0L6offsetS1046;
  int32_t _M0L6_2atmpS2760;
  uint64_t _M0L4mul0S1047;
  int32_t _M0L6_2atmpS2759;
  int32_t _M0L6_2atmpS2758;
  uint64_t _M0L4mul1S1048;
  uint64_t _M0L1mS1049;
  struct _M0TPB7Umul128 _M0L7_2abindS1050;
  uint64_t _M0L7_2alow1S1051;
  uint64_t _M0L8_2ahigh1S1052;
  struct _M0TPB7Umul128 _M0L7_2abindS1053;
  uint64_t _M0L7_2alow0S1054;
  uint64_t _M0L8_2ahigh0S1055;
  uint64_t _M0L3sumS1056;
  uint64_t _M0Lm5high1S1057;
  int32_t _M0L6_2atmpS2756;
  int32_t _M0L6_2atmpS2757;
  int32_t _M0L5deltaS1058;
  uint64_t _M0L6_2atmpS2748;
  int32_t _M0L6_2atmpS2755;
  uint32_t _M0L6_2atmpS2752;
  int32_t _M0L6_2atmpS2754;
  int32_t _M0L6_2atmpS2753;
  uint32_t _M0L6_2atmpS2751;
  uint32_t _M0L6_2atmpS2750;
  uint64_t _M0L6_2atmpS2749;
  uint64_t _M0L1aS1059;
  uint64_t _M0L6_2atmpS2747;
  uint64_t _M0L1bS1060;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS1043 = _M0L1iS1044 / 26;
  _M0L5base2S1045 = _M0L4baseS1043 * 26;
  _M0L6offsetS1046 = _M0L1iS1044 - _M0L5base2S1045;
  _M0L6_2atmpS2760 = _M0L4baseS1043 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1047
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2760);
  _M0L6_2atmpS2759 = _M0L4baseS1043 * 2;
  _M0L6_2atmpS2758 = _M0L6_2atmpS2759 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1048
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2758);
  if (_M0L6offsetS1046 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1047, _M0L4mul1S1048};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1049
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1046);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1050 = _M0FPB7umul128(_M0L1mS1049, _M0L4mul1S1048);
  _M0L7_2alow1S1051 = _M0L7_2abindS1050.$0;
  _M0L8_2ahigh1S1052 = _M0L7_2abindS1050.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1053 = _M0FPB7umul128(_M0L1mS1049, _M0L4mul0S1047);
  _M0L7_2alow0S1054 = _M0L7_2abindS1053.$0;
  _M0L8_2ahigh0S1055 = _M0L7_2abindS1053.$1;
  _M0L3sumS1056 = _M0L8_2ahigh0S1055 + _M0L7_2alow1S1051;
  _M0Lm5high1S1057 = _M0L8_2ahigh1S1052;
  if (_M0L3sumS1056 < _M0L8_2ahigh0S1055) {
    uint64_t _M0L6_2atmpS2746 = _M0Lm5high1S1057;
    _M0Lm5high1S1057 = _M0L6_2atmpS2746 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2756 = _M0FPB8pow5bits(_M0L1iS1044);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2757 = _M0FPB8pow5bits(_M0L5base2S1045);
  _M0L5deltaS1058 = _M0L6_2atmpS2756 - _M0L6_2atmpS2757;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2748
  = _M0FPB13shiftright128(_M0L7_2alow0S1054, _M0L3sumS1056, _M0L5deltaS1058);
  _M0L6_2atmpS2755 = _M0L1iS1044 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2752
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2755);
  _M0L6_2atmpS2754 = _M0L1iS1044 % 16;
  _M0L6_2atmpS2753 = _M0L6_2atmpS2754 << 1;
  _M0L6_2atmpS2751 = _M0L6_2atmpS2752 >> (_M0L6_2atmpS2753 & 31);
  _M0L6_2atmpS2750 = _M0L6_2atmpS2751 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2749 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2750);
  _M0L1aS1059 = _M0L6_2atmpS2748 + _M0L6_2atmpS2749;
  _M0L6_2atmpS2747 = _M0Lm5high1S1057;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1060
  = _M0FPB13shiftright128(_M0L3sumS1056, _M0L6_2atmpS2747, _M0L5deltaS1058);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1059, _M0L1bS1060};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1017,
  struct _M0TPB8Pow5Pair _M0L3mulS1014,
  int32_t _M0L1jS1030,
  int32_t _M0L7mmShiftS1032
) {
  uint64_t _M0L7_2amul0S1013;
  uint64_t _M0L7_2amul1S1015;
  uint64_t _M0L1mS1016;
  struct _M0TPB7Umul128 _M0L7_2abindS1018;
  uint64_t _M0L5_2aloS1019;
  uint64_t _M0L6_2atmpS1020;
  struct _M0TPB7Umul128 _M0L7_2abindS1021;
  uint64_t _M0L6_2alo2S1022;
  uint64_t _M0L6_2ahi2S1023;
  uint64_t _M0L3midS1024;
  uint64_t _M0L6_2atmpS2745;
  uint64_t _M0L2hiS1025;
  uint64_t _M0L3lo2S1026;
  uint64_t _M0L6_2atmpS2743;
  uint64_t _M0L6_2atmpS2744;
  uint64_t _M0L4mid2S1027;
  uint64_t _M0L6_2atmpS2742;
  uint64_t _M0L3hi2S1028;
  int32_t _M0L6_2atmpS2741;
  int32_t _M0L6_2atmpS2740;
  uint64_t _M0L2vpS1029;
  uint64_t _M0Lm2vmS1031;
  int32_t _M0L6_2atmpS2739;
  int32_t _M0L6_2atmpS2738;
  uint64_t _M0L2vrS1042;
  uint64_t _M0L6_2atmpS2737;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S1013 = _M0L3mulS1014.$0;
  _M0L7_2amul1S1015 = _M0L3mulS1014.$1;
  _M0L1mS1016 = _M0L1mS1017 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1018 = _M0FPB7umul128(_M0L1mS1016, _M0L7_2amul0S1013);
  _M0L5_2aloS1019 = _M0L7_2abindS1018.$0;
  _M0L6_2atmpS1020 = _M0L7_2abindS1018.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1021 = _M0FPB7umul128(_M0L1mS1016, _M0L7_2amul1S1015);
  _M0L6_2alo2S1022 = _M0L7_2abindS1021.$0;
  _M0L6_2ahi2S1023 = _M0L7_2abindS1021.$1;
  _M0L3midS1024 = _M0L6_2atmpS1020 + _M0L6_2alo2S1022;
  if (_M0L3midS1024 < _M0L6_2atmpS1020) {
    _M0L6_2atmpS2745 = 1ull;
  } else {
    _M0L6_2atmpS2745 = 0ull;
  }
  _M0L2hiS1025 = _M0L6_2ahi2S1023 + _M0L6_2atmpS2745;
  _M0L3lo2S1026 = _M0L5_2aloS1019 + _M0L7_2amul0S1013;
  _M0L6_2atmpS2743 = _M0L3midS1024 + _M0L7_2amul1S1015;
  if (_M0L3lo2S1026 < _M0L5_2aloS1019) {
    _M0L6_2atmpS2744 = 1ull;
  } else {
    _M0L6_2atmpS2744 = 0ull;
  }
  _M0L4mid2S1027 = _M0L6_2atmpS2743 + _M0L6_2atmpS2744;
  if (_M0L4mid2S1027 < _M0L3midS1024) {
    _M0L6_2atmpS2742 = 1ull;
  } else {
    _M0L6_2atmpS2742 = 0ull;
  }
  _M0L3hi2S1028 = _M0L2hiS1025 + _M0L6_2atmpS2742;
  _M0L6_2atmpS2741 = _M0L1jS1030 - 64;
  _M0L6_2atmpS2740 = _M0L6_2atmpS2741 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS1029
  = _M0FPB13shiftright128(_M0L4mid2S1027, _M0L3hi2S1028, _M0L6_2atmpS2740);
  _M0Lm2vmS1031 = 0ull;
  if (_M0L7mmShiftS1032) {
    uint64_t _M0L3lo3S1033 = _M0L5_2aloS1019 - _M0L7_2amul0S1013;
    uint64_t _M0L6_2atmpS2727 = _M0L3midS1024 - _M0L7_2amul1S1015;
    uint64_t _M0L6_2atmpS2728;
    uint64_t _M0L4mid3S1034;
    uint64_t _M0L6_2atmpS2726;
    uint64_t _M0L3hi3S1035;
    int32_t _M0L6_2atmpS2725;
    int32_t _M0L6_2atmpS2724;
    if (_M0L5_2aloS1019 < _M0L3lo3S1033) {
      _M0L6_2atmpS2728 = 1ull;
    } else {
      _M0L6_2atmpS2728 = 0ull;
    }
    _M0L4mid3S1034 = _M0L6_2atmpS2727 - _M0L6_2atmpS2728;
    if (_M0L3midS1024 < _M0L4mid3S1034) {
      _M0L6_2atmpS2726 = 1ull;
    } else {
      _M0L6_2atmpS2726 = 0ull;
    }
    _M0L3hi3S1035 = _M0L2hiS1025 - _M0L6_2atmpS2726;
    _M0L6_2atmpS2725 = _M0L1jS1030 - 64;
    _M0L6_2atmpS2724 = _M0L6_2atmpS2725 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS1031
    = _M0FPB13shiftright128(_M0L4mid3S1034, _M0L3hi3S1035, _M0L6_2atmpS2724);
  } else {
    uint64_t _M0L3lo3S1036 = _M0L5_2aloS1019 + _M0L5_2aloS1019;
    uint64_t _M0L6_2atmpS2735 = _M0L3midS1024 + _M0L3midS1024;
    uint64_t _M0L6_2atmpS2736;
    uint64_t _M0L4mid3S1037;
    uint64_t _M0L6_2atmpS2733;
    uint64_t _M0L6_2atmpS2734;
    uint64_t _M0L3hi3S1038;
    uint64_t _M0L3lo4S1039;
    uint64_t _M0L6_2atmpS2731;
    uint64_t _M0L6_2atmpS2732;
    uint64_t _M0L4mid4S1040;
    uint64_t _M0L6_2atmpS2730;
    uint64_t _M0L3hi4S1041;
    int32_t _M0L6_2atmpS2729;
    if (_M0L3lo3S1036 < _M0L5_2aloS1019) {
      _M0L6_2atmpS2736 = 1ull;
    } else {
      _M0L6_2atmpS2736 = 0ull;
    }
    _M0L4mid3S1037 = _M0L6_2atmpS2735 + _M0L6_2atmpS2736;
    _M0L6_2atmpS2733 = _M0L2hiS1025 + _M0L2hiS1025;
    if (_M0L4mid3S1037 < _M0L3midS1024) {
      _M0L6_2atmpS2734 = 1ull;
    } else {
      _M0L6_2atmpS2734 = 0ull;
    }
    _M0L3hi3S1038 = _M0L6_2atmpS2733 + _M0L6_2atmpS2734;
    _M0L3lo4S1039 = _M0L3lo3S1036 - _M0L7_2amul0S1013;
    _M0L6_2atmpS2731 = _M0L4mid3S1037 - _M0L7_2amul1S1015;
    if (_M0L3lo3S1036 < _M0L3lo4S1039) {
      _M0L6_2atmpS2732 = 1ull;
    } else {
      _M0L6_2atmpS2732 = 0ull;
    }
    _M0L4mid4S1040 = _M0L6_2atmpS2731 - _M0L6_2atmpS2732;
    if (_M0L4mid3S1037 < _M0L4mid4S1040) {
      _M0L6_2atmpS2730 = 1ull;
    } else {
      _M0L6_2atmpS2730 = 0ull;
    }
    _M0L3hi4S1041 = _M0L3hi3S1038 - _M0L6_2atmpS2730;
    _M0L6_2atmpS2729 = _M0L1jS1030 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS1031
    = _M0FPB13shiftright128(_M0L4mid4S1040, _M0L3hi4S1041, _M0L6_2atmpS2729);
  }
  _M0L6_2atmpS2739 = _M0L1jS1030 - 64;
  _M0L6_2atmpS2738 = _M0L6_2atmpS2739 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS1042
  = _M0FPB13shiftright128(_M0L3midS1024, _M0L2hiS1025, _M0L6_2atmpS2738);
  _M0L6_2atmpS2737 = _M0Lm2vmS1031;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS1042,
                                                _M0L2vpS1029,
                                                _M0L6_2atmpS2737};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1011,
  int32_t _M0L1pS1012
) {
  uint64_t _M0L6_2atmpS2723;
  uint64_t _M0L6_2atmpS2722;
  uint64_t _M0L6_2atmpS2721;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2723 = 1ull << (_M0L1pS1012 & 63);
  _M0L6_2atmpS2722 = _M0L6_2atmpS2723 - 1ull;
  _M0L6_2atmpS2721 = _M0L5valueS1011 & _M0L6_2atmpS2722;
  return _M0L6_2atmpS2721 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1009,
  int32_t _M0L1pS1010
) {
  int32_t _M0L6_2atmpS2720;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2720 = _M0FPB10pow5Factor(_M0L5valueS1009);
  return _M0L6_2atmpS2720 >= _M0L1pS1010;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1005) {
  uint64_t _M0L6_2atmpS2708;
  uint64_t _M0L6_2atmpS2709;
  uint64_t _M0L6_2atmpS2710;
  uint64_t _M0L6_2atmpS2711;
  int32_t _M0Lm5countS1006;
  uint64_t _M0Lm5valueS1007;
  uint64_t _M0L6_2atmpS2719;
  moonbit_string_t _M0L6_2atmpS2718;
  moonbit_string_t _M0L6_2atmpS3848;
  moonbit_string_t _M0L6_2atmpS2717;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2708 = _M0L5valueS1005 % 5ull;
  if (_M0L6_2atmpS2708 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2709 = _M0L5valueS1005 % 25ull;
  if (_M0L6_2atmpS2709 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2710 = _M0L5valueS1005 % 125ull;
  if (_M0L6_2atmpS2710 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2711 = _M0L5valueS1005 % 625ull;
  if (_M0L6_2atmpS2711 != 0ull) {
    return 3;
  }
  _M0Lm5countS1006 = 4;
  _M0Lm5valueS1007 = _M0L5valueS1005 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2712 = _M0Lm5valueS1007;
    if (_M0L6_2atmpS2712 > 0ull) {
      uint64_t _M0L6_2atmpS2714 = _M0Lm5valueS1007;
      uint64_t _M0L6_2atmpS2713 = _M0L6_2atmpS2714 % 5ull;
      uint64_t _M0L6_2atmpS2715;
      int32_t _M0L6_2atmpS2716;
      if (_M0L6_2atmpS2713 != 0ull) {
        return _M0Lm5countS1006;
      }
      _M0L6_2atmpS2715 = _M0Lm5valueS1007;
      _M0Lm5valueS1007 = _M0L6_2atmpS2715 / 5ull;
      _M0L6_2atmpS2716 = _M0Lm5countS1006;
      _M0Lm5countS1006 = _M0L6_2atmpS2716 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2719 = _M0Lm5valueS1007;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2718
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2719);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3848
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_93.data, _M0L6_2atmpS2718);
  moonbit_decref(_M0L6_2atmpS2718);
  _M0L6_2atmpS2717 = _M0L6_2atmpS3848;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2717, (moonbit_string_t)moonbit_string_literal_94.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1004,
  uint64_t _M0L2hiS1002,
  int32_t _M0L4distS1003
) {
  int32_t _M0L6_2atmpS2707;
  uint64_t _M0L6_2atmpS2705;
  uint64_t _M0L6_2atmpS2706;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2707 = 64 - _M0L4distS1003;
  _M0L6_2atmpS2705 = _M0L2hiS1002 << (_M0L6_2atmpS2707 & 63);
  _M0L6_2atmpS2706 = _M0L2loS1004 >> (_M0L4distS1003 & 63);
  return _M0L6_2atmpS2705 | _M0L6_2atmpS2706;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS992,
  uint64_t _M0L1bS995
) {
  uint64_t _M0L3aLoS991;
  uint64_t _M0L3aHiS993;
  uint64_t _M0L3bLoS994;
  uint64_t _M0L3bHiS996;
  uint64_t _M0L1xS997;
  uint64_t _M0L6_2atmpS2703;
  uint64_t _M0L6_2atmpS2704;
  uint64_t _M0L1yS998;
  uint64_t _M0L6_2atmpS2701;
  uint64_t _M0L6_2atmpS2702;
  uint64_t _M0L1zS999;
  uint64_t _M0L6_2atmpS2699;
  uint64_t _M0L6_2atmpS2700;
  uint64_t _M0L6_2atmpS2697;
  uint64_t _M0L6_2atmpS2698;
  uint64_t _M0L1wS1000;
  uint64_t _M0L2loS1001;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS991 = _M0L1aS992 & 4294967295ull;
  _M0L3aHiS993 = _M0L1aS992 >> 32;
  _M0L3bLoS994 = _M0L1bS995 & 4294967295ull;
  _M0L3bHiS996 = _M0L1bS995 >> 32;
  _M0L1xS997 = _M0L3aLoS991 * _M0L3bLoS994;
  _M0L6_2atmpS2703 = _M0L3aHiS993 * _M0L3bLoS994;
  _M0L6_2atmpS2704 = _M0L1xS997 >> 32;
  _M0L1yS998 = _M0L6_2atmpS2703 + _M0L6_2atmpS2704;
  _M0L6_2atmpS2701 = _M0L3aLoS991 * _M0L3bHiS996;
  _M0L6_2atmpS2702 = _M0L1yS998 & 4294967295ull;
  _M0L1zS999 = _M0L6_2atmpS2701 + _M0L6_2atmpS2702;
  _M0L6_2atmpS2699 = _M0L3aHiS993 * _M0L3bHiS996;
  _M0L6_2atmpS2700 = _M0L1yS998 >> 32;
  _M0L6_2atmpS2697 = _M0L6_2atmpS2699 + _M0L6_2atmpS2700;
  _M0L6_2atmpS2698 = _M0L1zS999 >> 32;
  _M0L1wS1000 = _M0L6_2atmpS2697 + _M0L6_2atmpS2698;
  _M0L2loS1001 = _M0L1aS992 * _M0L1bS995;
  return (struct _M0TPB7Umul128){_M0L2loS1001, _M0L1wS1000};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS986,
  int32_t _M0L4fromS990,
  int32_t _M0L2toS988
) {
  int32_t _M0L6_2atmpS2696;
  struct _M0TPB13StringBuilder* _M0L3bufS985;
  int32_t _M0L1iS987;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2696 = Moonbit_array_length(_M0L5bytesS986);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS985 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2696);
  _M0L1iS987 = _M0L4fromS990;
  while (1) {
    if (_M0L1iS987 < _M0L2toS988) {
      int32_t _M0L6_2atmpS2694;
      int32_t _M0L6_2atmpS2693;
      int32_t _M0L6_2atmpS2695;
      if (
        _M0L1iS987 < 0 || _M0L1iS987 >= Moonbit_array_length(_M0L5bytesS986)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2694 = (int32_t)_M0L5bytesS986[_M0L1iS987];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2693 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2694);
      moonbit_incref(_M0L3bufS985);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS985, _M0L6_2atmpS2693);
      _M0L6_2atmpS2695 = _M0L1iS987 + 1;
      _M0L1iS987 = _M0L6_2atmpS2695;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS986);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS985);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS984) {
  int32_t _M0L6_2atmpS2692;
  uint32_t _M0L6_2atmpS2691;
  uint32_t _M0L6_2atmpS2690;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2692 = _M0L1eS984 * 78913;
  _M0L6_2atmpS2691 = *(uint32_t*)&_M0L6_2atmpS2692;
  _M0L6_2atmpS2690 = _M0L6_2atmpS2691 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2690;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS983) {
  int32_t _M0L6_2atmpS2689;
  uint32_t _M0L6_2atmpS2688;
  uint32_t _M0L6_2atmpS2687;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2689 = _M0L1eS983 * 732923;
  _M0L6_2atmpS2688 = *(uint32_t*)&_M0L6_2atmpS2689;
  _M0L6_2atmpS2687 = _M0L6_2atmpS2688 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2687;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS981,
  int32_t _M0L8exponentS982,
  int32_t _M0L8mantissaS979
) {
  moonbit_string_t _M0L1sS980;
  moonbit_string_t _M0L6_2atmpS3849;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS979) {
    return (moonbit_string_t)moonbit_string_literal_95.data;
  }
  if (_M0L4signS981) {
    _M0L1sS980 = (moonbit_string_t)moonbit_string_literal_96.data;
  } else {
    _M0L1sS980 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS982) {
    moonbit_string_t _M0L6_2atmpS3850;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3850
    = moonbit_add_string(_M0L1sS980, (moonbit_string_t)moonbit_string_literal_97.data);
    moonbit_decref(_M0L1sS980);
    return _M0L6_2atmpS3850;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3849
  = moonbit_add_string(_M0L1sS980, (moonbit_string_t)moonbit_string_literal_98.data);
  moonbit_decref(_M0L1sS980);
  return _M0L6_2atmpS3849;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS978) {
  int32_t _M0L6_2atmpS2686;
  uint32_t _M0L6_2atmpS2685;
  uint32_t _M0L6_2atmpS2684;
  int32_t _M0L6_2atmpS2683;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2686 = _M0L1eS978 * 1217359;
  _M0L6_2atmpS2685 = *(uint32_t*)&_M0L6_2atmpS2686;
  _M0L6_2atmpS2684 = _M0L6_2atmpS2685 >> 19;
  _M0L6_2atmpS2683 = *(int32_t*)&_M0L6_2atmpS2684;
  return _M0L6_2atmpS2683 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS977,
  struct _M0TPB6Hasher* _M0L6hasherS976
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS976, _M0L4selfS977);
  return 0;
}

int32_t _M0IPC16string10StringViewPB4Hash13hash__combine(
  struct _M0TPC16string10StringView _M0L4selfS970,
  struct _M0TPB6Hasher* _M0L6hasherS974
) {
  moonbit_string_t _M0L8_2afieldS3852;
  moonbit_string_t _M0L3strS969;
  int32_t _M0L7_2abindS971;
  int32_t _M0L8_2afieldS3851;
  int32_t _M0L7_2abindS972;
  int32_t _M0L1iS973;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3852 = _M0L4selfS970.$0;
  _M0L3strS969 = _M0L8_2afieldS3852;
  _M0L7_2abindS971 = _M0L4selfS970.$1;
  _M0L8_2afieldS3851 = _M0L4selfS970.$2;
  _M0L7_2abindS972 = _M0L8_2afieldS3851;
  _M0L1iS973 = _M0L7_2abindS971;
  while (1) {
    if (_M0L1iS973 < _M0L7_2abindS972) {
      int32_t _M0L6_2atmpS2681 = _M0L3strS969[_M0L1iS973];
      int32_t _M0L6_2atmpS2680 = (int32_t)_M0L6_2atmpS2681;
      uint32_t _M0L6_2atmpS2679 = *(uint32_t*)&_M0L6_2atmpS2680;
      int32_t _M0L6_2atmpS2682;
      moonbit_incref(_M0L6hasherS974);
      #line 509 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L6hasherS974, _M0L6_2atmpS2679);
      _M0L6_2atmpS2682 = _M0L1iS973 + 1;
      _M0L1iS973 = _M0L6_2atmpS2682;
      continue;
    } else {
      moonbit_decref(_M0L6hasherS974);
      moonbit_decref(_M0L3strS969);
    }
    break;
  }
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS968,
  struct _M0TPB6Hasher* _M0L6hasherS967
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS967, _M0L4selfS968);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS965,
  moonbit_string_t _M0L5valueS963
) {
  int32_t _M0L7_2abindS962;
  int32_t _M0L1iS964;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS962 = Moonbit_array_length(_M0L5valueS963);
  _M0L1iS964 = 0;
  while (1) {
    if (_M0L1iS964 < _M0L7_2abindS962) {
      int32_t _M0L6_2atmpS2677 = _M0L5valueS963[_M0L1iS964];
      int32_t _M0L6_2atmpS2676 = (int32_t)_M0L6_2atmpS2677;
      uint32_t _M0L6_2atmpS2675 = *(uint32_t*)&_M0L6_2atmpS2676;
      int32_t _M0L6_2atmpS2678;
      moonbit_incref(_M0L4selfS965);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS965, _M0L6_2atmpS2675);
      _M0L6_2atmpS2678 = _M0L1iS964 + 1;
      _M0L1iS964 = _M0L6_2atmpS2678;
      continue;
    } else {
      moonbit_decref(_M0L4selfS965);
      moonbit_decref(_M0L5valueS963);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS960,
  int32_t _M0L3idxS961
) {
  int32_t _M0L6_2atmpS3853;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3853 = _M0L4selfS960[_M0L3idxS961];
  moonbit_decref(_M0L4selfS960);
  return _M0L6_2atmpS3853;
}

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB5Iter24nextGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS959
) {
  #line 904 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  #line 905 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPB4Iter4nextGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(_M0L4selfS959);
}

void* _M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS955
) {
  struct _M0TWRPC16string10StringViewERPB4Json* _M0L6_2atmpS2667;
  struct _M0TWEORPB4Json* _M0L6_2atmpS2666;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2665;
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2667
  = (struct _M0TWRPC16string10StringViewERPB4Json*)&_M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewEC2668l78$closure.data;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2666
  = _M0MPB4Iter3mapGRPC16string10StringViewRPB4JsonE(_M0L4selfS955, _M0L6_2atmpS2667);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2665 = _M0MPB4Iter9to__arrayGRPB4JsonE(_M0L6_2atmpS2666);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPC14json4Json5array(_M0L6_2atmpS2665);
}

void* _M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TWEOi* _M0L4selfS957
) {
  struct _M0TWiERPB4Json* _M0L6_2atmpS2672;
  struct _M0TWEORPB4Json* _M0L6_2atmpS2671;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2670;
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2672
  = (struct _M0TWiERPB4Json*)&_M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodEC2673l78$closure.data;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2671
  = _M0MPB4Iter3mapGRP48clawteam8clawteam8internal5httpx6MethodRPB4JsonE(_M0L4selfS957, _M0L6_2atmpS2672);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2670 = _M0MPB4Iter9to__arrayGRPB4JsonE(_M0L6_2atmpS2671);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPC14json4Json5array(_M0L6_2atmpS2670);
}

void* _M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodEC2673l78(
  struct _M0TWiERPB4Json* _M0L6_2aenvS2674,
  int32_t _M0L1xS958
) {
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  moonbit_decref(_M0L6_2aenvS2674);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0IP48clawteam8clawteam8internal5httpx6MethodPB6ToJson8to__json(_M0L1xS958);
}

void* _M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewEC2668l78(
  struct _M0TWRPC16string10StringViewERPB4Json* _M0L6_2aenvS2669,
  struct _M0TPC16string10StringView _M0L1xS956
) {
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  moonbit_decref(_M0L6_2aenvS2669);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0IPC16string10StringViewPB6ToJson8to__json(_M0L1xS956);
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS954
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2664;
  void* _block_4528;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IPC16string10StringViewPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2664
  = _M0MPC15array5Array3mapGRPC16string10StringViewRPB4JsonE(_M0L4selfS954, _M0IPC16string10StringViewPB6ToJson14to__json_2eclo);
  _block_4528 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4528)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4528)->$0 = _M0L6_2atmpS2664;
  return _block_4528;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRPC16string10StringViewRPB4JsonE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS948,
  struct _M0TWRPC16string10StringViewERPB4Json* _M0L1fS952
) {
  int32_t _M0L3lenS2663;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS947;
  int32_t _M0L7_2abindS949;
  int32_t _M0L1iS950;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2663 = _M0L4selfS948->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS947 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2663);
  _M0L7_2abindS949 = _M0L4selfS948->$1;
  _M0L1iS950 = 0;
  while (1) {
    if (_M0L1iS950 < _M0L7_2abindS949) {
      struct _M0TPC16string10StringView* _M0L8_2afieldS3857 =
        _M0L4selfS948->$0;
      struct _M0TPC16string10StringView* _M0L3bufS2662 = _M0L8_2afieldS3857;
      struct _M0TPC16string10StringView _M0L6_2atmpS3856 =
        _M0L3bufS2662[_M0L1iS950];
      struct _M0TPC16string10StringView _M0L1vS951 = _M0L6_2atmpS3856;
      void** _M0L8_2afieldS3855 = _M0L3arrS947->$0;
      void** _M0L3bufS2659 = _M0L8_2afieldS3855;
      void* _M0L6_2atmpS2660;
      void* _M0L6_2aoldS3854;
      int32_t _M0L6_2atmpS2661;
      moonbit_incref(_M0L3bufS2659);
      moonbit_incref(_M0L1fS952);
      moonbit_incref(_M0L1vS951.$0);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2660 = _M0L1fS952->code(_M0L1fS952, _M0L1vS951);
      _M0L6_2aoldS3854 = (void*)_M0L3bufS2659[_M0L1iS950];
      moonbit_decref(_M0L6_2aoldS3854);
      _M0L3bufS2659[_M0L1iS950] = _M0L6_2atmpS2660;
      moonbit_decref(_M0L3bufS2659);
      _M0L6_2atmpS2661 = _M0L1iS950 + 1;
      _M0L1iS950 = _M0L6_2atmpS2661;
      continue;
    } else {
      moonbit_decref(_M0L1fS952);
      moonbit_decref(_M0L4selfS948);
    }
    break;
  }
  return _M0L3arrS947;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS946) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS946;
}

void* _M0IPC14bool4BoolPB6ToJson8to__json(int32_t _M0L4selfS945) {
  #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS945) {
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json7boolean(1);
  } else {
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json7boolean(0);
  }
}

void* _M0MPC14json4Json7boolean(int32_t _M0L7booleanS944) {
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L7booleanS944) {
    return (struct moonbit_object*)&moonbit_constant_constructor_1 + 1;
  } else {
    return (struct moonbit_object*)&moonbit_constant_constructor_2 + 1;
  }
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS943) {
  void* _block_4530;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4530 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4530)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4530)->$0 = _M0L6stringS943;
  return _block_4530;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS937
) {
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3858;
  int32_t _M0L6_2acntS4346;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4headS2658;
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* _M0L11curr__entryS936;
  struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__* _closure_4531;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2654;
  #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3858 = _M0L4selfS937->$5;
  _M0L6_2acntS4346 = Moonbit_object_header(_M0L4selfS937)->rc;
  if (_M0L6_2acntS4346 > 1) {
    int32_t _M0L11_2anew__cntS4348 = _M0L6_2acntS4346 - 1;
    Moonbit_object_header(_M0L4selfS937)->rc = _M0L11_2anew__cntS4348;
    if (_M0L8_2afieldS3858) {
      moonbit_incref(_M0L8_2afieldS3858);
    }
  } else if (_M0L6_2acntS4346 == 1) {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS4347 =
      _M0L4selfS937->$0;
    moonbit_decref(_M0L8_2afieldS4347);
    #line 612 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS937);
  }
  _M0L4headS2658 = _M0L8_2afieldS3858;
  _M0L11curr__entryS936
  = (struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE));
  Moonbit_object_header(_M0L11curr__entryS936)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS936->$0 = _M0L4headS2658;
  _closure_4531
  = (struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__*)moonbit_malloc(sizeof(struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__));
  Moonbit_object_header(_closure_4531)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__, $0) >> 2, 1, 0);
  _closure_4531->code
  = &_M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEC2655l613;
  _closure_4531->$0 = _M0L11curr__entryS936;
  _M0L6_2atmpS2654
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_4531;
  #line 613 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGRPC16string10StringViewE(_M0L6_2atmpS2654);
}

void* _M0MPB3Map4keysGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEC2655l613(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2656
) {
  struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__* _M0L14_2acasted__envS2657;
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* _M0L8_2afieldS3863;
  int32_t _M0L6_2acntS4349;
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* _M0L11curr__entryS936;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3862;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS938;
  #line 613 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2657
  = (struct _M0R139Map_3a_3akeys_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2655__l613__*)_M0L6_2aenvS2656;
  _M0L8_2afieldS3863 = _M0L14_2acasted__envS2657->$0;
  _M0L6_2acntS4349 = Moonbit_object_header(_M0L14_2acasted__envS2657)->rc;
  if (_M0L6_2acntS4349 > 1) {
    int32_t _M0L11_2anew__cntS4350 = _M0L6_2acntS4349 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2657)->rc
    = _M0L11_2anew__cntS4350;
    moonbit_incref(_M0L8_2afieldS3863);
  } else if (_M0L6_2acntS4349 == 1) {
    #line 613 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2657);
  }
  _M0L11curr__entryS936 = _M0L8_2afieldS3863;
  _M0L8_2afieldS3862 = _M0L11curr__entryS936->$0;
  _M0L7_2abindS938 = _M0L8_2afieldS3862;
  if (_M0L7_2abindS938 == 0) {
    moonbit_decref(_M0L11curr__entryS936);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS939 =
      _M0L7_2abindS938;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4_2axS940 =
      _M0L7_2aSomeS939;
    struct _M0TPC16string10StringView _M0L8_2afieldS3861 =
      (struct _M0TPC16string10StringView){_M0L4_2axS940->$4_1,
                                            _M0L4_2axS940->$4_2,
                                            _M0L4_2axS940->$4_0};
    struct _M0TPC16string10StringView _M0L6_2akeyS941 = _M0L8_2afieldS3861;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3860 =
      _M0L4_2axS940->$1;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2anextS942 =
      _M0L8_2afieldS3860;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aoldS3859 =
      _M0L11curr__entryS936->$0;
    void* _block_4532;
    if (_M0L7_2anextS942) {
      moonbit_incref(_M0L7_2anextS942);
    }
    moonbit_incref(_M0L6_2akeyS941.$0);
    if (_M0L6_2aoldS3859) {
      moonbit_decref(_M0L6_2aoldS3859);
    }
    _M0L11curr__entryS936->$0 = _M0L7_2anextS942;
    moonbit_decref(_M0L11curr__entryS936);
    _block_4532
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_block_4532)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4532)->$0_0
    = _M0L6_2akeyS941.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4532)->$0_1
    = _M0L6_2akeyS941.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4532)->$0_2
    = _M0L6_2akeyS941.$2;
    return _block_4532;
  }
}

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map5iter2GRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS935
) {
  #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 606 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB3Map4iterGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS935);
}

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map4iterGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS920
) {
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3864;
  int32_t _M0L6_2acntS4351;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4headS2647;
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* _M0L11curr__entryS919;
  struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__* _closure_4533;
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2642;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3864 = _M0L4selfS920->$5;
  _M0L6_2acntS4351 = Moonbit_object_header(_M0L4selfS920)->rc;
  if (_M0L6_2acntS4351 > 1) {
    int32_t _M0L11_2anew__cntS4353 = _M0L6_2acntS4351 - 1;
    Moonbit_object_header(_M0L4selfS920)->rc = _M0L11_2anew__cntS4353;
    if (_M0L8_2afieldS3864) {
      moonbit_incref(_M0L8_2afieldS3864);
    }
  } else if (_M0L6_2acntS4351 == 1) {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS4352 =
      _M0L4selfS920->$0;
    moonbit_decref(_M0L8_2afieldS4352);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS920);
  }
  _M0L4headS2647 = _M0L8_2afieldS3864;
  _M0L11curr__entryS919
  = (struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE));
  Moonbit_object_header(_M0L11curr__entryS919)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS919->$0 = _M0L4headS2647;
  _closure_4533
  = (struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__*)moonbit_malloc(sizeof(struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__));
  Moonbit_object_header(_closure_4533)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__, $0) >> 2, 1, 0);
  _closure_4533->code
  = &_M0MPB3Map4iterGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEC2643l591;
  _closure_4533->$0 = _M0L11curr__entryS919;
  _M0L6_2atmpS2642
  = (struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_closure_4533;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(_M0L6_2atmpS2642);
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS928
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3865;
  int32_t _M0L6_2acntS4354;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2653;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS927;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__* _closure_4534;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2648;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3865 = _M0L4selfS928->$5;
  _M0L6_2acntS4354 = Moonbit_object_header(_M0L4selfS928)->rc;
  if (_M0L6_2acntS4354 > 1) {
    int32_t _M0L11_2anew__cntS4356 = _M0L6_2acntS4354 - 1;
    Moonbit_object_header(_M0L4selfS928)->rc = _M0L11_2anew__cntS4356;
    if (_M0L8_2afieldS3865) {
      moonbit_incref(_M0L8_2afieldS3865);
    }
  } else if (_M0L6_2acntS4354 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4355 = _M0L4selfS928->$0;
    moonbit_decref(_M0L8_2afieldS4355);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS928);
  }
  _M0L4headS2653 = _M0L8_2afieldS3865;
  _M0L11curr__entryS927
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS927)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS927->$0 = _M0L4headS2653;
  _closure_4534
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__));
  Moonbit_object_header(_closure_4534)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__, $0) >> 2, 1, 0);
  _closure_4534->code = &_M0MPB3Map4iterGsRPB4JsonEC2649l591;
  _closure_4534->$0 = _M0L11curr__entryS927;
  _M0L6_2atmpS2648 = (struct _M0TWEOUsRPB4JsonE*)_closure_4534;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2648);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2649l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2650
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__* _M0L14_2acasted__envS2651;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3871;
  int32_t _M0L6_2acntS4357;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS927;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3870;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS929;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2651
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2649__l591__*)_M0L6_2aenvS2650;
  _M0L8_2afieldS3871 = _M0L14_2acasted__envS2651->$0;
  _M0L6_2acntS4357 = Moonbit_object_header(_M0L14_2acasted__envS2651)->rc;
  if (_M0L6_2acntS4357 > 1) {
    int32_t _M0L11_2anew__cntS4358 = _M0L6_2acntS4357 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2651)->rc
    = _M0L11_2anew__cntS4358;
    moonbit_incref(_M0L8_2afieldS3871);
  } else if (_M0L6_2acntS4357 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2651);
  }
  _M0L11curr__entryS927 = _M0L8_2afieldS3871;
  _M0L8_2afieldS3870 = _M0L11curr__entryS927->$0;
  _M0L7_2abindS929 = _M0L8_2afieldS3870;
  if (_M0L7_2abindS929 == 0) {
    moonbit_decref(_M0L11curr__entryS927);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS930 = _M0L7_2abindS929;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS931 = _M0L7_2aSomeS930;
    moonbit_string_t _M0L8_2afieldS3869 = _M0L4_2axS931->$4;
    moonbit_string_t _M0L6_2akeyS932 = _M0L8_2afieldS3869;
    void* _M0L8_2afieldS3868 = _M0L4_2axS931->$5;
    void* _M0L8_2avalueS933 = _M0L8_2afieldS3868;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3867 = _M0L4_2axS931->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS934 = _M0L8_2afieldS3867;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3866 =
      _M0L11curr__entryS927->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2652;
    if (_M0L7_2anextS934) {
      moonbit_incref(_M0L7_2anextS934);
    }
    moonbit_incref(_M0L8_2avalueS933);
    moonbit_incref(_M0L6_2akeyS932);
    if (_M0L6_2aoldS3866) {
      moonbit_decref(_M0L6_2aoldS3866);
    }
    _M0L11curr__entryS927->$0 = _M0L7_2anextS934;
    moonbit_decref(_M0L11curr__entryS927);
    _M0L8_2atupleS2652
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2652)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2652->$0 = _M0L6_2akeyS932;
    _M0L8_2atupleS2652->$1 = _M0L8_2avalueS933;
    return _M0L8_2atupleS2652;
  }
}

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map4iterGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEC2643l591(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aenvS2644
) {
  struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__* _M0L14_2acasted__envS2645;
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* _M0L8_2afieldS3877;
  int32_t _M0L6_2acntS4359;
  struct _M0TPC13ref3RefGORPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE* _M0L11curr__entryS919;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3876;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS921;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2645
  = (struct _M0R139Map_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20clawteam_2fclawteam_2finternal_2fhttpx_2fLayer_5d_7c_2eanon__u2643__l591__*)_M0L6_2aenvS2644;
  _M0L8_2afieldS3877 = _M0L14_2acasted__envS2645->$0;
  _M0L6_2acntS4359 = Moonbit_object_header(_M0L14_2acasted__envS2645)->rc;
  if (_M0L6_2acntS4359 > 1) {
    int32_t _M0L11_2anew__cntS4360 = _M0L6_2acntS4359 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2645)->rc
    = _M0L11_2anew__cntS4360;
    moonbit_incref(_M0L8_2afieldS3877);
  } else if (_M0L6_2acntS4359 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2645);
  }
  _M0L11curr__entryS919 = _M0L8_2afieldS3877;
  _M0L8_2afieldS3876 = _M0L11curr__entryS919->$0;
  _M0L7_2abindS921 = _M0L8_2afieldS3876;
  if (_M0L7_2abindS921 == 0) {
    moonbit_decref(_M0L11curr__entryS919);
    return 0;
  } else {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS922 =
      _M0L7_2abindS921;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4_2axS923 =
      _M0L7_2aSomeS922;
    struct _M0TPC16string10StringView _M0L8_2afieldS3875 =
      (struct _M0TPC16string10StringView){_M0L4_2axS923->$4_1,
                                            _M0L4_2axS923->$4_2,
                                            _M0L4_2axS923->$4_0};
    struct _M0TPC16string10StringView _M0L6_2akeyS924 = _M0L8_2afieldS3875;
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3874 =
      _M0L4_2axS923->$5;
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2avalueS925 =
      _M0L8_2afieldS3874;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3873 =
      _M0L4_2axS923->$1;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2anextS926 =
      _M0L8_2afieldS3873;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aoldS3872 =
      _M0L11curr__entryS919->$0;
    struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2atupleS2646;
    if (_M0L7_2anextS926) {
      moonbit_incref(_M0L7_2anextS926);
    }
    moonbit_incref(_M0L8_2avalueS925);
    moonbit_incref(_M0L6_2akeyS924.$0);
    if (_M0L6_2aoldS3872) {
      moonbit_decref(_M0L6_2aoldS3872);
    }
    _M0L11curr__entryS919->$0 = _M0L7_2anextS926;
    moonbit_decref(_M0L11curr__entryS919);
    _M0L8_2atupleS2646
    = (struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE));
    Moonbit_object_header(_M0L8_2atupleS2646)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE, $0_0) >> 2, 2, 0);
    _M0L8_2atupleS2646->$0_0 = _M0L6_2akeyS924.$0;
    _M0L8_2atupleS2646->$0_1 = _M0L6_2akeyS924.$1;
    _M0L8_2atupleS2646->$0_2 = _M0L6_2akeyS924.$2;
    _M0L8_2atupleS2646->$1 = _M0L8_2avalueS925;
    return _M0L8_2atupleS2646;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS918
) {
  int32_t _M0L8_2afieldS3878;
  int32_t _M0L4sizeS2641;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3878 = _M0L4selfS918->$1;
  moonbit_decref(_M0L4selfS918);
  _M0L4sizeS2641 = _M0L8_2afieldS3878;
  return _M0L4sizeS2641 == 0;
}

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MPB3Map2atGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS914,
  struct _M0TPC16string10StringView _M0L3keyS910
) {
  int32_t _M0L4hashS909;
  int32_t _M0L14capacity__maskS2640;
  int32_t _M0L6_2atmpS2639;
  int32_t _M0L1iS911;
  int32_t _M0L3idxS912;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS910.$0);
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS909
  = _M0IP016_24default__implPB4Hash4hashGRPC16string10StringViewE(_M0L3keyS910);
  _M0L14capacity__maskS2640 = _M0L4selfS914->$3;
  _M0L6_2atmpS2639 = _M0L4hashS909 & _M0L14capacity__maskS2640;
  _M0L1iS911 = 0;
  _M0L3idxS912 = _M0L6_2atmpS2639;
  while (1) {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3883 =
      _M0L4selfS914->$0;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7entriesS2638 =
      _M0L8_2afieldS3883;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS3882;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS913;
    int32_t _tmp_4537;
    int32_t _tmp_4538;
    if (
      _M0L3idxS912 < 0
      || _M0L3idxS912 >= Moonbit_array_length(_M0L7entriesS2638)
    ) {
      #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3882
    = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L7entriesS2638[
        _M0L3idxS912
      ];
    _M0L7_2abindS913 = _M0L6_2atmpS3882;
    if (_M0L7_2abindS913 == 0) {
      #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    } else {
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS915 =
        _M0L7_2abindS913;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2aentryS916 =
        _M0L7_2aSomeS915;
      int32_t _M0L4hashS2632 = _M0L8_2aentryS916->$3;
      int32_t _if__result_4536;
      int32_t _M0L8_2afieldS3879;
      int32_t _M0L3pslS2633;
      if (_M0L4hashS2632 == _M0L4hashS909) {
        struct _M0TPC16string10StringView _M0L8_2afieldS3881 =
          (struct _M0TPC16string10StringView){_M0L8_2aentryS916->$4_1,
                                                _M0L8_2aentryS916->$4_2,
                                                _M0L8_2aentryS916->$4_0};
        struct _M0TPC16string10StringView _M0L3keyS2631 = _M0L8_2afieldS3881;
        moonbit_incref(_M0L3keyS2631.$0);
        moonbit_incref(_M0L8_2aentryS916);
        moonbit_incref(_M0L3keyS910.$0);
        #line 237 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _if__result_4536
        = _M0IPC16string10StringViewPB2Eq5equal(_M0L3keyS2631, _M0L3keyS910);
      } else {
        moonbit_incref(_M0L8_2aentryS916);
        _if__result_4536 = 0;
      }
      if (_if__result_4536) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3880;
        moonbit_decref(_M0L4selfS914);
        moonbit_decref(_M0L3keyS910.$0);
        _M0L8_2afieldS3880 = _M0L8_2aentryS916->$5;
        moonbit_incref(_M0L8_2afieldS3880);
        moonbit_decref(_M0L8_2aentryS916);
        return _M0L8_2afieldS3880;
      }
      _M0L8_2afieldS3879 = _M0L8_2aentryS916->$2;
      moonbit_decref(_M0L8_2aentryS916);
      _M0L3pslS2633 = _M0L8_2afieldS3879;
      if (_M0L1iS911 <= _M0L3pslS2633) {
        int32_t _M0L6_2atmpS2634 = _M0L1iS911 + 1;
        int32_t _M0L6_2atmpS2636 = _M0L3idxS912 + 1;
        int32_t _M0L14capacity__maskS2637 = _M0L4selfS914->$3;
        int32_t _M0L6_2atmpS2635 =
          _M0L6_2atmpS2636 & _M0L14capacity__maskS2637;
        _M0L1iS911 = _M0L6_2atmpS2634;
        _M0L3idxS912 = _M0L6_2atmpS2635;
        continue;
      } else {
        #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
    }
    _tmp_4537 = _M0L1iS911;
    _tmp_4538 = _M0L3idxS912;
    _M0L1iS911 = _tmp_4537;
    _M0L3idxS912 = _tmp_4538;
    continue;
    break;
  }
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS887,
  int32_t _M0L3keyS883
) {
  int32_t _M0L4hashS882;
  int32_t _M0L14capacity__maskS2602;
  int32_t _M0L6_2atmpS2601;
  int32_t _M0L1iS884;
  int32_t _M0L3idxS885;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS882 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS883);
  _M0L14capacity__maskS2602 = _M0L4selfS887->$3;
  _M0L6_2atmpS2601 = _M0L4hashS882 & _M0L14capacity__maskS2602;
  _M0L1iS884 = 0;
  _M0L3idxS885 = _M0L6_2atmpS2601;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3887 =
      _M0L4selfS887->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2600 =
      _M0L8_2afieldS3887;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3886;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS886;
    if (
      _M0L3idxS885 < 0
      || _M0L3idxS885 >= Moonbit_array_length(_M0L7entriesS2600)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3886
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2600[
        _M0L3idxS885
      ];
    _M0L7_2abindS886 = _M0L6_2atmpS3886;
    if (_M0L7_2abindS886 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2589;
      if (_M0L7_2abindS886) {
        moonbit_incref(_M0L7_2abindS886);
      }
      moonbit_decref(_M0L4selfS887);
      if (_M0L7_2abindS886) {
        moonbit_decref(_M0L7_2abindS886);
      }
      _M0L6_2atmpS2589 = 0;
      return _M0L6_2atmpS2589;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS888 =
        _M0L7_2abindS886;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS889 =
        _M0L7_2aSomeS888;
      int32_t _M0L4hashS2591 = _M0L8_2aentryS889->$3;
      int32_t _if__result_4540;
      int32_t _M0L8_2afieldS3884;
      int32_t _M0L3pslS2594;
      int32_t _M0L6_2atmpS2596;
      int32_t _M0L6_2atmpS2598;
      int32_t _M0L14capacity__maskS2599;
      int32_t _M0L6_2atmpS2597;
      if (_M0L4hashS2591 == _M0L4hashS882) {
        int32_t _M0L3keyS2590 = _M0L8_2aentryS889->$4;
        _if__result_4540 = _M0L3keyS2590 == _M0L3keyS883;
      } else {
        _if__result_4540 = 0;
      }
      if (_if__result_4540) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3885;
        int32_t _M0L6_2acntS4361;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2593;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2592;
        moonbit_incref(_M0L8_2aentryS889);
        moonbit_decref(_M0L4selfS887);
        _M0L8_2afieldS3885 = _M0L8_2aentryS889->$5;
        _M0L6_2acntS4361 = Moonbit_object_header(_M0L8_2aentryS889)->rc;
        if (_M0L6_2acntS4361 > 1) {
          int32_t _M0L11_2anew__cntS4363 = _M0L6_2acntS4361 - 1;
          Moonbit_object_header(_M0L8_2aentryS889)->rc
          = _M0L11_2anew__cntS4363;
          moonbit_incref(_M0L8_2afieldS3885);
        } else if (_M0L6_2acntS4361 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4362 =
            _M0L8_2aentryS889->$1;
          if (_M0L8_2afieldS4362) {
            moonbit_decref(_M0L8_2afieldS4362);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS889);
        }
        _M0L5valueS2593 = _M0L8_2afieldS3885;
        _M0L6_2atmpS2592 = _M0L5valueS2593;
        return _M0L6_2atmpS2592;
      } else {
        moonbit_incref(_M0L8_2aentryS889);
      }
      _M0L8_2afieldS3884 = _M0L8_2aentryS889->$2;
      moonbit_decref(_M0L8_2aentryS889);
      _M0L3pslS2594 = _M0L8_2afieldS3884;
      if (_M0L1iS884 > _M0L3pslS2594) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2595;
        moonbit_decref(_M0L4selfS887);
        _M0L6_2atmpS2595 = 0;
        return _M0L6_2atmpS2595;
      }
      _M0L6_2atmpS2596 = _M0L1iS884 + 1;
      _M0L6_2atmpS2598 = _M0L3idxS885 + 1;
      _M0L14capacity__maskS2599 = _M0L4selfS887->$3;
      _M0L6_2atmpS2597 = _M0L6_2atmpS2598 & _M0L14capacity__maskS2599;
      _M0L1iS884 = _M0L6_2atmpS2596;
      _M0L3idxS885 = _M0L6_2atmpS2597;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS896,
  moonbit_string_t _M0L3keyS892
) {
  int32_t _M0L4hashS891;
  int32_t _M0L14capacity__maskS2616;
  int32_t _M0L6_2atmpS2615;
  int32_t _M0L1iS893;
  int32_t _M0L3idxS894;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS892);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS891 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS892);
  _M0L14capacity__maskS2616 = _M0L4selfS896->$3;
  _M0L6_2atmpS2615 = _M0L4hashS891 & _M0L14capacity__maskS2616;
  _M0L1iS893 = 0;
  _M0L3idxS894 = _M0L6_2atmpS2615;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3893 =
      _M0L4selfS896->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2614 =
      _M0L8_2afieldS3893;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3892;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS895;
    if (
      _M0L3idxS894 < 0
      || _M0L3idxS894 >= Moonbit_array_length(_M0L7entriesS2614)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3892
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2614[
        _M0L3idxS894
      ];
    _M0L7_2abindS895 = _M0L6_2atmpS3892;
    if (_M0L7_2abindS895 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2603;
      if (_M0L7_2abindS895) {
        moonbit_incref(_M0L7_2abindS895);
      }
      moonbit_decref(_M0L4selfS896);
      if (_M0L7_2abindS895) {
        moonbit_decref(_M0L7_2abindS895);
      }
      moonbit_decref(_M0L3keyS892);
      _M0L6_2atmpS2603 = 0;
      return _M0L6_2atmpS2603;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS897 =
        _M0L7_2abindS895;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS898 =
        _M0L7_2aSomeS897;
      int32_t _M0L4hashS2605 = _M0L8_2aentryS898->$3;
      int32_t _if__result_4542;
      int32_t _M0L8_2afieldS3888;
      int32_t _M0L3pslS2608;
      int32_t _M0L6_2atmpS2610;
      int32_t _M0L6_2atmpS2612;
      int32_t _M0L14capacity__maskS2613;
      int32_t _M0L6_2atmpS2611;
      if (_M0L4hashS2605 == _M0L4hashS891) {
        moonbit_string_t _M0L8_2afieldS3891 = _M0L8_2aentryS898->$4;
        moonbit_string_t _M0L3keyS2604 = _M0L8_2afieldS3891;
        int32_t _M0L6_2atmpS3890;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3890
        = moonbit_val_array_equal(_M0L3keyS2604, _M0L3keyS892);
        _if__result_4542 = _M0L6_2atmpS3890;
      } else {
        _if__result_4542 = 0;
      }
      if (_if__result_4542) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3889;
        int32_t _M0L6_2acntS4364;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2607;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2606;
        moonbit_incref(_M0L8_2aentryS898);
        moonbit_decref(_M0L4selfS896);
        moonbit_decref(_M0L3keyS892);
        _M0L8_2afieldS3889 = _M0L8_2aentryS898->$5;
        _M0L6_2acntS4364 = Moonbit_object_header(_M0L8_2aentryS898)->rc;
        if (_M0L6_2acntS4364 > 1) {
          int32_t _M0L11_2anew__cntS4367 = _M0L6_2acntS4364 - 1;
          Moonbit_object_header(_M0L8_2aentryS898)->rc
          = _M0L11_2anew__cntS4367;
          moonbit_incref(_M0L8_2afieldS3889);
        } else if (_M0L6_2acntS4364 == 1) {
          moonbit_string_t _M0L8_2afieldS4366 = _M0L8_2aentryS898->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4365;
          moonbit_decref(_M0L8_2afieldS4366);
          _M0L8_2afieldS4365 = _M0L8_2aentryS898->$1;
          if (_M0L8_2afieldS4365) {
            moonbit_decref(_M0L8_2afieldS4365);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS898);
        }
        _M0L5valueS2607 = _M0L8_2afieldS3889;
        _M0L6_2atmpS2606 = _M0L5valueS2607;
        return _M0L6_2atmpS2606;
      } else {
        moonbit_incref(_M0L8_2aentryS898);
      }
      _M0L8_2afieldS3888 = _M0L8_2aentryS898->$2;
      moonbit_decref(_M0L8_2aentryS898);
      _M0L3pslS2608 = _M0L8_2afieldS3888;
      if (_M0L1iS893 > _M0L3pslS2608) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2609;
        moonbit_decref(_M0L4selfS896);
        moonbit_decref(_M0L3keyS892);
        _M0L6_2atmpS2609 = 0;
        return _M0L6_2atmpS2609;
      }
      _M0L6_2atmpS2610 = _M0L1iS893 + 1;
      _M0L6_2atmpS2612 = _M0L3idxS894 + 1;
      _M0L14capacity__maskS2613 = _M0L4selfS896->$3;
      _M0L6_2atmpS2611 = _M0L6_2atmpS2612 & _M0L14capacity__maskS2613;
      _M0L1iS893 = _M0L6_2atmpS2610;
      _M0L3idxS894 = _M0L6_2atmpS2611;
      continue;
    }
    break;
  }
}

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MPB3Map3getGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS905,
  struct _M0TPC16string10StringView _M0L3keyS901
) {
  int32_t _M0L4hashS900;
  int32_t _M0L14capacity__maskS2630;
  int32_t _M0L6_2atmpS2629;
  int32_t _M0L1iS902;
  int32_t _M0L3idxS903;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS901.$0);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS900
  = _M0IP016_24default__implPB4Hash4hashGRPC16string10StringViewE(_M0L3keyS901);
  _M0L14capacity__maskS2630 = _M0L4selfS905->$3;
  _M0L6_2atmpS2629 = _M0L4hashS900 & _M0L14capacity__maskS2630;
  _M0L1iS902 = 0;
  _M0L3idxS903 = _M0L6_2atmpS2629;
  while (1) {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3898 =
      _M0L4selfS905->$0;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7entriesS2628 =
      _M0L8_2afieldS3898;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS3897;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS904;
    if (
      _M0L3idxS903 < 0
      || _M0L3idxS903 >= Moonbit_array_length(_M0L7entriesS2628)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3897
    = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L7entriesS2628[
        _M0L3idxS903
      ];
    _M0L7_2abindS904 = _M0L6_2atmpS3897;
    if (_M0L7_2abindS904 == 0) {
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS2617;
      if (_M0L7_2abindS904) {
        moonbit_incref(_M0L7_2abindS904);
      }
      moonbit_decref(_M0L4selfS905);
      if (_M0L7_2abindS904) {
        moonbit_decref(_M0L7_2abindS904);
      }
      moonbit_decref(_M0L3keyS901.$0);
      _M0L6_2atmpS2617 = 0;
      return _M0L6_2atmpS2617;
    } else {
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS906 =
        _M0L7_2abindS904;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2aentryS907 =
        _M0L7_2aSomeS906;
      int32_t _M0L4hashS2619 = _M0L8_2aentryS907->$3;
      int32_t _if__result_4544;
      int32_t _M0L8_2afieldS3894;
      int32_t _M0L3pslS2622;
      int32_t _M0L6_2atmpS2624;
      int32_t _M0L6_2atmpS2626;
      int32_t _M0L14capacity__maskS2627;
      int32_t _M0L6_2atmpS2625;
      if (_M0L4hashS2619 == _M0L4hashS900) {
        struct _M0TPC16string10StringView _M0L8_2afieldS3896 =
          (struct _M0TPC16string10StringView){_M0L8_2aentryS907->$4_1,
                                                _M0L8_2aentryS907->$4_2,
                                                _M0L8_2aentryS907->$4_0};
        struct _M0TPC16string10StringView _M0L3keyS2618 = _M0L8_2afieldS3896;
        moonbit_incref(_M0L3keyS2618.$0);
        moonbit_incref(_M0L8_2aentryS907);
        moonbit_incref(_M0L3keyS901.$0);
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _if__result_4544
        = _M0IPC16string10StringViewPB2Eq5equal(_M0L3keyS2618, _M0L3keyS901);
      } else {
        moonbit_incref(_M0L8_2aentryS907);
        _if__result_4544 = 0;
      }
      if (_if__result_4544) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3895;
        int32_t _M0L6_2acntS4368;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5valueS2621;
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS2620;
        moonbit_decref(_M0L4selfS905);
        moonbit_decref(_M0L3keyS901.$0);
        _M0L8_2afieldS3895 = _M0L8_2aentryS907->$5;
        _M0L6_2acntS4368 = Moonbit_object_header(_M0L8_2aentryS907)->rc;
        if (_M0L6_2acntS4368 > 1) {
          int32_t _M0L11_2anew__cntS4371 = _M0L6_2acntS4368 - 1;
          Moonbit_object_header(_M0L8_2aentryS907)->rc
          = _M0L11_2anew__cntS4371;
          moonbit_incref(_M0L8_2afieldS3895);
        } else if (_M0L6_2acntS4368 == 1) {
          struct _M0TPC16string10StringView _M0L8_2afieldS4370 =
            (struct _M0TPC16string10StringView){_M0L8_2aentryS907->$4_1,
                                                  _M0L8_2aentryS907->$4_2,
                                                  _M0L8_2aentryS907->$4_0};
          struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS4369;
          moonbit_decref(_M0L8_2afieldS4370.$0);
          _M0L8_2afieldS4369 = _M0L8_2aentryS907->$1;
          if (_M0L8_2afieldS4369) {
            moonbit_decref(_M0L8_2afieldS4369);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS907);
        }
        _M0L5valueS2621 = _M0L8_2afieldS3895;
        _M0L6_2atmpS2620 = _M0L5valueS2621;
        return _M0L6_2atmpS2620;
      }
      _M0L8_2afieldS3894 = _M0L8_2aentryS907->$2;
      moonbit_decref(_M0L8_2aentryS907);
      _M0L3pslS2622 = _M0L8_2afieldS3894;
      if (_M0L1iS902 > _M0L3pslS2622) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS2623;
        moonbit_decref(_M0L4selfS905);
        moonbit_decref(_M0L3keyS901.$0);
        _M0L6_2atmpS2623 = 0;
        return _M0L6_2atmpS2623;
      }
      _M0L6_2atmpS2624 = _M0L1iS902 + 1;
      _M0L6_2atmpS2626 = _M0L3idxS903 + 1;
      _M0L14capacity__maskS2627 = _M0L4selfS905->$3;
      _M0L6_2atmpS2625 = _M0L6_2atmpS2626 & _M0L14capacity__maskS2627;
      _M0L1iS902 = _M0L6_2atmpS2624;
      _M0L3idxS903 = _M0L6_2atmpS2625;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS859
) {
  int32_t _M0L6lengthS858;
  int32_t _M0Lm8capacityS860;
  int32_t _M0L6_2atmpS2554;
  int32_t _M0L6_2atmpS2553;
  int32_t _M0L6_2atmpS2564;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS861;
  int32_t _M0L3endS2562;
  int32_t _M0L5startS2563;
  int32_t _M0L7_2abindS862;
  int32_t _M0L2__S863;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS859.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS858
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS859);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS860 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS858);
  _M0L6_2atmpS2554 = _M0Lm8capacityS860;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2553 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2554);
  if (_M0L6lengthS858 > _M0L6_2atmpS2553) {
    int32_t _M0L6_2atmpS2555 = _M0Lm8capacityS860;
    _M0Lm8capacityS860 = _M0L6_2atmpS2555 * 2;
  }
  _M0L6_2atmpS2564 = _M0Lm8capacityS860;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS861
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2564);
  _M0L3endS2562 = _M0L3arrS859.$2;
  _M0L5startS2563 = _M0L3arrS859.$1;
  _M0L7_2abindS862 = _M0L3endS2562 - _M0L5startS2563;
  _M0L2__S863 = 0;
  while (1) {
    if (_M0L2__S863 < _M0L7_2abindS862) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3902 =
        _M0L3arrS859.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2559 =
        _M0L8_2afieldS3902;
      int32_t _M0L5startS2561 = _M0L3arrS859.$1;
      int32_t _M0L6_2atmpS2560 = _M0L5startS2561 + _M0L2__S863;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3901 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2559[
          _M0L6_2atmpS2560
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS864 =
        _M0L6_2atmpS3901;
      moonbit_string_t _M0L8_2afieldS3900 = _M0L1eS864->$0;
      moonbit_string_t _M0L6_2atmpS2556 = _M0L8_2afieldS3900;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3899 =
        _M0L1eS864->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2557 =
        _M0L8_2afieldS3899;
      int32_t _M0L6_2atmpS2558;
      moonbit_incref(_M0L6_2atmpS2557);
      moonbit_incref(_M0L6_2atmpS2556);
      moonbit_incref(_M0L1mS861);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS861, _M0L6_2atmpS2556, _M0L6_2atmpS2557);
      _M0L6_2atmpS2558 = _M0L2__S863 + 1;
      _M0L2__S863 = _M0L6_2atmpS2558;
      continue;
    } else {
      moonbit_decref(_M0L3arrS859.$0);
    }
    break;
  }
  return _M0L1mS861;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS867
) {
  int32_t _M0L6lengthS866;
  int32_t _M0Lm8capacityS868;
  int32_t _M0L6_2atmpS2566;
  int32_t _M0L6_2atmpS2565;
  int32_t _M0L6_2atmpS2576;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS869;
  int32_t _M0L3endS2574;
  int32_t _M0L5startS2575;
  int32_t _M0L7_2abindS870;
  int32_t _M0L2__S871;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS867.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS866
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS867);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS868 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS866);
  _M0L6_2atmpS2566 = _M0Lm8capacityS868;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2565 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2566);
  if (_M0L6lengthS866 > _M0L6_2atmpS2565) {
    int32_t _M0L6_2atmpS2567 = _M0Lm8capacityS868;
    _M0Lm8capacityS868 = _M0L6_2atmpS2567 * 2;
  }
  _M0L6_2atmpS2576 = _M0Lm8capacityS868;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS869
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2576);
  _M0L3endS2574 = _M0L3arrS867.$2;
  _M0L5startS2575 = _M0L3arrS867.$1;
  _M0L7_2abindS870 = _M0L3endS2574 - _M0L5startS2575;
  _M0L2__S871 = 0;
  while (1) {
    if (_M0L2__S871 < _M0L7_2abindS870) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3905 =
        _M0L3arrS867.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2571 =
        _M0L8_2afieldS3905;
      int32_t _M0L5startS2573 = _M0L3arrS867.$1;
      int32_t _M0L6_2atmpS2572 = _M0L5startS2573 + _M0L2__S871;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3904 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2571[
          _M0L6_2atmpS2572
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS872 = _M0L6_2atmpS3904;
      int32_t _M0L6_2atmpS2568 = _M0L1eS872->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3903 =
        _M0L1eS872->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2569 =
        _M0L8_2afieldS3903;
      int32_t _M0L6_2atmpS2570;
      moonbit_incref(_M0L6_2atmpS2569);
      moonbit_incref(_M0L1mS869);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS869, _M0L6_2atmpS2568, _M0L6_2atmpS2569);
      _M0L6_2atmpS2570 = _M0L2__S871 + 1;
      _M0L2__S871 = _M0L6_2atmpS2570;
      continue;
    } else {
      moonbit_decref(_M0L3arrS867.$0);
    }
    break;
  }
  return _M0L1mS869;
}

struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map11from__arrayGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE _M0L3arrS875
) {
  int32_t _M0L6lengthS874;
  int32_t _M0Lm8capacityS876;
  int32_t _M0L6_2atmpS2578;
  int32_t _M0L6_2atmpS2577;
  int32_t _M0L6_2atmpS2588;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L1mS877;
  int32_t _M0L3endS2586;
  int32_t _M0L5startS2587;
  int32_t _M0L7_2abindS878;
  int32_t _M0L2__S879;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS875.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS874
  = _M0MPC15array9ArrayView6lengthGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(_M0L3arrS875);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS876 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS874);
  _M0L6_2atmpS2578 = _M0Lm8capacityS876;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2577 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2578);
  if (_M0L6lengthS874 > _M0L6_2atmpS2577) {
    int32_t _M0L6_2atmpS2579 = _M0Lm8capacityS876;
    _M0Lm8capacityS876 = _M0L6_2atmpS2579 * 2;
  }
  _M0L6_2atmpS2588 = _M0Lm8capacityS876;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS877
  = _M0MPB3Map11new_2einnerGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L6_2atmpS2588);
  _M0L3endS2586 = _M0L3arrS875.$2;
  _M0L5startS2587 = _M0L3arrS875.$1;
  _M0L7_2abindS878 = _M0L3endS2586 - _M0L5startS2587;
  _M0L2__S879 = 0;
  while (1) {
    if (_M0L2__S879 < _M0L7_2abindS878) {
      struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3909 =
        _M0L3arrS875.$0;
      struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L3bufS2583 =
        _M0L8_2afieldS3909;
      int32_t _M0L5startS2585 = _M0L3arrS875.$1;
      int32_t _M0L6_2atmpS2584 = _M0L5startS2585 + _M0L2__S879;
      struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS3908 =
        (struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L3bufS2583[
          _M0L6_2atmpS2584
        ];
      struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L1eS880 =
        _M0L6_2atmpS3908;
      struct _M0TPC16string10StringView _M0L8_2afieldS3907 =
        (struct _M0TPC16string10StringView){_M0L1eS880->$0_1,
                                              _M0L1eS880->$0_2,
                                              _M0L1eS880->$0_0};
      struct _M0TPC16string10StringView _M0L6_2atmpS2580 = _M0L8_2afieldS3907;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3906 =
        _M0L1eS880->$1;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2atmpS2581 =
        _M0L8_2afieldS3906;
      int32_t _M0L6_2atmpS2582;
      moonbit_incref(_M0L6_2atmpS2581);
      moonbit_incref(_M0L6_2atmpS2580.$0);
      moonbit_incref(_M0L1mS877);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L1mS877, _M0L6_2atmpS2580, _M0L6_2atmpS2581);
      _M0L6_2atmpS2582 = _M0L2__S879 + 1;
      _M0L2__S879 = _M0L6_2atmpS2582;
      continue;
    } else {
      moonbit_decref(_M0L3arrS875.$0);
    }
    break;
  }
  return _M0L1mS877;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS849,
  moonbit_string_t _M0L3keyS850,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS851
) {
  int32_t _M0L6_2atmpS2550;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS850);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2550 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS850);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS849, _M0L3keyS850, _M0L5valueS851, _M0L6_2atmpS2550);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS852,
  int32_t _M0L3keyS853,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS854
) {
  int32_t _M0L6_2atmpS2551;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2551 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS853);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS852, _M0L3keyS853, _M0L5valueS854, _M0L6_2atmpS2551);
  return 0;
}

int32_t _M0MPB3Map3setGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS855,
  struct _M0TPC16string10StringView _M0L3keyS856,
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5valueS857
) {
  int32_t _M0L6_2atmpS2552;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS856.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2552
  = _M0IP016_24default__implPB4Hash4hashGRPC16string10StringViewE(_M0L3keyS856);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS855, _M0L3keyS856, _M0L5valueS857, _M0L6_2atmpS2552);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS817
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3916;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS816;
  int32_t _M0L8capacityS2535;
  int32_t _M0L13new__capacityS818;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2530;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2529;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3915;
  int32_t _M0L6_2atmpS2531;
  int32_t _M0L8capacityS2533;
  int32_t _M0L6_2atmpS2532;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2534;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3914;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS819;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3916 = _M0L4selfS817->$5;
  _M0L9old__headS816 = _M0L8_2afieldS3916;
  _M0L8capacityS2535 = _M0L4selfS817->$2;
  _M0L13new__capacityS818 = _M0L8capacityS2535 << 1;
  _M0L6_2atmpS2530 = 0;
  _M0L6_2atmpS2529
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS818, _M0L6_2atmpS2530);
  _M0L6_2aoldS3915 = _M0L4selfS817->$0;
  if (_M0L9old__headS816) {
    moonbit_incref(_M0L9old__headS816);
  }
  moonbit_decref(_M0L6_2aoldS3915);
  _M0L4selfS817->$0 = _M0L6_2atmpS2529;
  _M0L4selfS817->$2 = _M0L13new__capacityS818;
  _M0L6_2atmpS2531 = _M0L13new__capacityS818 - 1;
  _M0L4selfS817->$3 = _M0L6_2atmpS2531;
  _M0L8capacityS2533 = _M0L4selfS817->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2532 = _M0FPB21calc__grow__threshold(_M0L8capacityS2533);
  _M0L4selfS817->$4 = _M0L6_2atmpS2532;
  _M0L4selfS817->$1 = 0;
  _M0L6_2atmpS2534 = 0;
  _M0L6_2aoldS3914 = _M0L4selfS817->$5;
  if (_M0L6_2aoldS3914) {
    moonbit_decref(_M0L6_2aoldS3914);
  }
  _M0L4selfS817->$5 = _M0L6_2atmpS2534;
  _M0L4selfS817->$6 = -1;
  _M0L8_2aparamS819 = _M0L9old__headS816;
  while (1) {
    if (_M0L8_2aparamS819 == 0) {
      if (_M0L8_2aparamS819) {
        moonbit_decref(_M0L8_2aparamS819);
      }
      moonbit_decref(_M0L4selfS817);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS820 =
        _M0L8_2aparamS819;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS821 =
        _M0L7_2aSomeS820;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3913 =
        _M0L4_2axS821->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS822 =
        _M0L8_2afieldS3913;
      moonbit_string_t _M0L8_2afieldS3912 = _M0L4_2axS821->$4;
      moonbit_string_t _M0L6_2akeyS823 = _M0L8_2afieldS3912;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3911 =
        _M0L4_2axS821->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS824 =
        _M0L8_2afieldS3911;
      int32_t _M0L8_2afieldS3910 = _M0L4_2axS821->$3;
      int32_t _M0L6_2acntS4372 = Moonbit_object_header(_M0L4_2axS821)->rc;
      int32_t _M0L7_2ahashS825;
      if (_M0L6_2acntS4372 > 1) {
        int32_t _M0L11_2anew__cntS4373 = _M0L6_2acntS4372 - 1;
        Moonbit_object_header(_M0L4_2axS821)->rc = _M0L11_2anew__cntS4373;
        moonbit_incref(_M0L8_2avalueS824);
        moonbit_incref(_M0L6_2akeyS823);
        if (_M0L7_2anextS822) {
          moonbit_incref(_M0L7_2anextS822);
        }
      } else if (_M0L6_2acntS4372 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS821);
      }
      _M0L7_2ahashS825 = _M0L8_2afieldS3910;
      moonbit_incref(_M0L4selfS817);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS817, _M0L6_2akeyS823, _M0L8_2avalueS824, _M0L7_2ahashS825);
      _M0L8_2aparamS819 = _M0L7_2anextS822;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS828
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3922;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS827;
  int32_t _M0L8capacityS2542;
  int32_t _M0L13new__capacityS829;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2537;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2536;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3921;
  int32_t _M0L6_2atmpS2538;
  int32_t _M0L8capacityS2540;
  int32_t _M0L6_2atmpS2539;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2541;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3920;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS830;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3922 = _M0L4selfS828->$5;
  _M0L9old__headS827 = _M0L8_2afieldS3922;
  _M0L8capacityS2542 = _M0L4selfS828->$2;
  _M0L13new__capacityS829 = _M0L8capacityS2542 << 1;
  _M0L6_2atmpS2537 = 0;
  _M0L6_2atmpS2536
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS829, _M0L6_2atmpS2537);
  _M0L6_2aoldS3921 = _M0L4selfS828->$0;
  if (_M0L9old__headS827) {
    moonbit_incref(_M0L9old__headS827);
  }
  moonbit_decref(_M0L6_2aoldS3921);
  _M0L4selfS828->$0 = _M0L6_2atmpS2536;
  _M0L4selfS828->$2 = _M0L13new__capacityS829;
  _M0L6_2atmpS2538 = _M0L13new__capacityS829 - 1;
  _M0L4selfS828->$3 = _M0L6_2atmpS2538;
  _M0L8capacityS2540 = _M0L4selfS828->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2539 = _M0FPB21calc__grow__threshold(_M0L8capacityS2540);
  _M0L4selfS828->$4 = _M0L6_2atmpS2539;
  _M0L4selfS828->$1 = 0;
  _M0L6_2atmpS2541 = 0;
  _M0L6_2aoldS3920 = _M0L4selfS828->$5;
  if (_M0L6_2aoldS3920) {
    moonbit_decref(_M0L6_2aoldS3920);
  }
  _M0L4selfS828->$5 = _M0L6_2atmpS2541;
  _M0L4selfS828->$6 = -1;
  _M0L8_2aparamS830 = _M0L9old__headS827;
  while (1) {
    if (_M0L8_2aparamS830 == 0) {
      if (_M0L8_2aparamS830) {
        moonbit_decref(_M0L8_2aparamS830);
      }
      moonbit_decref(_M0L4selfS828);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS831 =
        _M0L8_2aparamS830;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS832 =
        _M0L7_2aSomeS831;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3919 =
        _M0L4_2axS832->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS833 =
        _M0L8_2afieldS3919;
      int32_t _M0L6_2akeyS834 = _M0L4_2axS832->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3918 =
        _M0L4_2axS832->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS835 =
        _M0L8_2afieldS3918;
      int32_t _M0L8_2afieldS3917 = _M0L4_2axS832->$3;
      int32_t _M0L6_2acntS4374 = Moonbit_object_header(_M0L4_2axS832)->rc;
      int32_t _M0L7_2ahashS836;
      if (_M0L6_2acntS4374 > 1) {
        int32_t _M0L11_2anew__cntS4375 = _M0L6_2acntS4374 - 1;
        Moonbit_object_header(_M0L4_2axS832)->rc = _M0L11_2anew__cntS4375;
        moonbit_incref(_M0L8_2avalueS835);
        if (_M0L7_2anextS833) {
          moonbit_incref(_M0L7_2anextS833);
        }
      } else if (_M0L6_2acntS4374 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS832);
      }
      _M0L7_2ahashS836 = _M0L8_2afieldS3917;
      moonbit_incref(_M0L4selfS828);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS828, _M0L6_2akeyS834, _M0L8_2avalueS835, _M0L7_2ahashS836);
      _M0L8_2aparamS830 = _M0L7_2anextS833;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS839
) {
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3929;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L9old__headS838;
  int32_t _M0L8capacityS2549;
  int32_t _M0L13new__capacityS840;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2544;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L6_2atmpS2543;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L6_2aoldS3928;
  int32_t _M0L6_2atmpS2545;
  int32_t _M0L8capacityS2547;
  int32_t _M0L6_2atmpS2546;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2548;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aoldS3927;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2aparamS841;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3929 = _M0L4selfS839->$5;
  _M0L9old__headS838 = _M0L8_2afieldS3929;
  _M0L8capacityS2549 = _M0L4selfS839->$2;
  _M0L13new__capacityS840 = _M0L8capacityS2549 << 1;
  _M0L6_2atmpS2544 = 0;
  _M0L6_2atmpS2543
  = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE**)moonbit_make_ref_array(_M0L13new__capacityS840, _M0L6_2atmpS2544);
  _M0L6_2aoldS3928 = _M0L4selfS839->$0;
  if (_M0L9old__headS838) {
    moonbit_incref(_M0L9old__headS838);
  }
  moonbit_decref(_M0L6_2aoldS3928);
  _M0L4selfS839->$0 = _M0L6_2atmpS2543;
  _M0L4selfS839->$2 = _M0L13new__capacityS840;
  _M0L6_2atmpS2545 = _M0L13new__capacityS840 - 1;
  _M0L4selfS839->$3 = _M0L6_2atmpS2545;
  _M0L8capacityS2547 = _M0L4selfS839->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2546 = _M0FPB21calc__grow__threshold(_M0L8capacityS2547);
  _M0L4selfS839->$4 = _M0L6_2atmpS2546;
  _M0L4selfS839->$1 = 0;
  _M0L6_2atmpS2548 = 0;
  _M0L6_2aoldS3927 = _M0L4selfS839->$5;
  if (_M0L6_2aoldS3927) {
    moonbit_decref(_M0L6_2aoldS3927);
  }
  _M0L4selfS839->$5 = _M0L6_2atmpS2548;
  _M0L4selfS839->$6 = -1;
  _M0L8_2aparamS841 = _M0L9old__headS838;
  while (1) {
    if (_M0L8_2aparamS841 == 0) {
      if (_M0L8_2aparamS841) {
        moonbit_decref(_M0L8_2aparamS841);
      }
      moonbit_decref(_M0L4selfS839);
    } else {
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS842 =
        _M0L8_2aparamS841;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4_2axS843 =
        _M0L7_2aSomeS842;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3926 =
        _M0L4_2axS843->$1;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2anextS844 =
        _M0L8_2afieldS3926;
      struct _M0TPC16string10StringView _M0L8_2afieldS3925 =
        (struct _M0TPC16string10StringView){_M0L4_2axS843->$4_1,
                                              _M0L4_2axS843->$4_2,
                                              _M0L4_2axS843->$4_0};
      struct _M0TPC16string10StringView _M0L6_2akeyS845 = _M0L8_2afieldS3925;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS3924 =
        _M0L4_2axS843->$5;
      struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2avalueS846 =
        _M0L8_2afieldS3924;
      int32_t _M0L8_2afieldS3923 = _M0L4_2axS843->$3;
      int32_t _M0L6_2acntS4376 = Moonbit_object_header(_M0L4_2axS843)->rc;
      int32_t _M0L7_2ahashS847;
      if (_M0L6_2acntS4376 > 1) {
        int32_t _M0L11_2anew__cntS4377 = _M0L6_2acntS4376 - 1;
        Moonbit_object_header(_M0L4_2axS843)->rc = _M0L11_2anew__cntS4377;
        moonbit_incref(_M0L8_2avalueS846);
        moonbit_incref(_M0L6_2akeyS845.$0);
        if (_M0L7_2anextS844) {
          moonbit_incref(_M0L7_2anextS844);
        }
      } else if (_M0L6_2acntS4376 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS843);
      }
      _M0L7_2ahashS847 = _M0L8_2afieldS3923;
      moonbit_incref(_M0L4selfS839);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS839, _M0L6_2akeyS845, _M0L8_2avalueS846, _M0L7_2ahashS847);
      _M0L8_2aparamS841 = _M0L7_2anextS844;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS771,
  moonbit_string_t _M0L3keyS777,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS778,
  int32_t _M0L4hashS773
) {
  int32_t _M0L14capacity__maskS2492;
  int32_t _M0L6_2atmpS2491;
  int32_t _M0L3pslS768;
  int32_t _M0L3idxS769;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2492 = _M0L4selfS771->$3;
  _M0L6_2atmpS2491 = _M0L4hashS773 & _M0L14capacity__maskS2492;
  _M0L3pslS768 = 0;
  _M0L3idxS769 = _M0L6_2atmpS2491;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3934 =
      _M0L4selfS771->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2490 =
      _M0L8_2afieldS3934;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3933;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS770;
    if (
      _M0L3idxS769 < 0
      || _M0L3idxS769 >= Moonbit_array_length(_M0L7entriesS2490)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3933
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2490[
        _M0L3idxS769
      ];
    _M0L7_2abindS770 = _M0L6_2atmpS3933;
    if (_M0L7_2abindS770 == 0) {
      int32_t _M0L4sizeS2475 = _M0L4selfS771->$1;
      int32_t _M0L8grow__atS2476 = _M0L4selfS771->$4;
      int32_t _M0L7_2abindS774;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS775;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS776;
      if (_M0L4sizeS2475 >= _M0L8grow__atS2476) {
        int32_t _M0L14capacity__maskS2478;
        int32_t _M0L6_2atmpS2477;
        moonbit_incref(_M0L4selfS771);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS771);
        _M0L14capacity__maskS2478 = _M0L4selfS771->$3;
        _M0L6_2atmpS2477 = _M0L4hashS773 & _M0L14capacity__maskS2478;
        _M0L3pslS768 = 0;
        _M0L3idxS769 = _M0L6_2atmpS2477;
        continue;
      }
      _M0L7_2abindS774 = _M0L4selfS771->$6;
      _M0L7_2abindS775 = 0;
      _M0L5entryS776
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS776)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS776->$0 = _M0L7_2abindS774;
      _M0L5entryS776->$1 = _M0L7_2abindS775;
      _M0L5entryS776->$2 = _M0L3pslS768;
      _M0L5entryS776->$3 = _M0L4hashS773;
      _M0L5entryS776->$4 = _M0L3keyS777;
      _M0L5entryS776->$5 = _M0L5valueS778;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS771, _M0L3idxS769, _M0L5entryS776);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS779 =
        _M0L7_2abindS770;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS780 =
        _M0L7_2aSomeS779;
      int32_t _M0L4hashS2480 = _M0L14_2acurr__entryS780->$3;
      int32_t _if__result_4552;
      int32_t _M0L3pslS2481;
      int32_t _M0L6_2atmpS2486;
      int32_t _M0L6_2atmpS2488;
      int32_t _M0L14capacity__maskS2489;
      int32_t _M0L6_2atmpS2487;
      if (_M0L4hashS2480 == _M0L4hashS773) {
        moonbit_string_t _M0L8_2afieldS3932 = _M0L14_2acurr__entryS780->$4;
        moonbit_string_t _M0L3keyS2479 = _M0L8_2afieldS3932;
        int32_t _M0L6_2atmpS3931;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3931
        = moonbit_val_array_equal(_M0L3keyS2479, _M0L3keyS777);
        _if__result_4552 = _M0L6_2atmpS3931;
      } else {
        _if__result_4552 = 0;
      }
      if (_if__result_4552) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3930;
        moonbit_incref(_M0L14_2acurr__entryS780);
        moonbit_decref(_M0L3keyS777);
        moonbit_decref(_M0L4selfS771);
        _M0L6_2aoldS3930 = _M0L14_2acurr__entryS780->$5;
        moonbit_decref(_M0L6_2aoldS3930);
        _M0L14_2acurr__entryS780->$5 = _M0L5valueS778;
        moonbit_decref(_M0L14_2acurr__entryS780);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS780);
      }
      _M0L3pslS2481 = _M0L14_2acurr__entryS780->$2;
      if (_M0L3pslS768 > _M0L3pslS2481) {
        int32_t _M0L4sizeS2482 = _M0L4selfS771->$1;
        int32_t _M0L8grow__atS2483 = _M0L4selfS771->$4;
        int32_t _M0L7_2abindS781;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS782;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS783;
        if (_M0L4sizeS2482 >= _M0L8grow__atS2483) {
          int32_t _M0L14capacity__maskS2485;
          int32_t _M0L6_2atmpS2484;
          moonbit_decref(_M0L14_2acurr__entryS780);
          moonbit_incref(_M0L4selfS771);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS771);
          _M0L14capacity__maskS2485 = _M0L4selfS771->$3;
          _M0L6_2atmpS2484 = _M0L4hashS773 & _M0L14capacity__maskS2485;
          _M0L3pslS768 = 0;
          _M0L3idxS769 = _M0L6_2atmpS2484;
          continue;
        }
        moonbit_incref(_M0L4selfS771);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS771, _M0L3idxS769, _M0L14_2acurr__entryS780);
        _M0L7_2abindS781 = _M0L4selfS771->$6;
        _M0L7_2abindS782 = 0;
        _M0L5entryS783
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS783)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS783->$0 = _M0L7_2abindS781;
        _M0L5entryS783->$1 = _M0L7_2abindS782;
        _M0L5entryS783->$2 = _M0L3pslS768;
        _M0L5entryS783->$3 = _M0L4hashS773;
        _M0L5entryS783->$4 = _M0L3keyS777;
        _M0L5entryS783->$5 = _M0L5valueS778;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS771, _M0L3idxS769, _M0L5entryS783);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS780);
      }
      _M0L6_2atmpS2486 = _M0L3pslS768 + 1;
      _M0L6_2atmpS2488 = _M0L3idxS769 + 1;
      _M0L14capacity__maskS2489 = _M0L4selfS771->$3;
      _M0L6_2atmpS2487 = _M0L6_2atmpS2488 & _M0L14capacity__maskS2489;
      _M0L3pslS768 = _M0L6_2atmpS2486;
      _M0L3idxS769 = _M0L6_2atmpS2487;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS787,
  int32_t _M0L3keyS793,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS794,
  int32_t _M0L4hashS789
) {
  int32_t _M0L14capacity__maskS2510;
  int32_t _M0L6_2atmpS2509;
  int32_t _M0L3pslS784;
  int32_t _M0L3idxS785;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2510 = _M0L4selfS787->$3;
  _M0L6_2atmpS2509 = _M0L4hashS789 & _M0L14capacity__maskS2510;
  _M0L3pslS784 = 0;
  _M0L3idxS785 = _M0L6_2atmpS2509;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3937 =
      _M0L4selfS787->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2508 =
      _M0L8_2afieldS3937;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3936;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS786;
    if (
      _M0L3idxS785 < 0
      || _M0L3idxS785 >= Moonbit_array_length(_M0L7entriesS2508)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3936
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2508[
        _M0L3idxS785
      ];
    _M0L7_2abindS786 = _M0L6_2atmpS3936;
    if (_M0L7_2abindS786 == 0) {
      int32_t _M0L4sizeS2493 = _M0L4selfS787->$1;
      int32_t _M0L8grow__atS2494 = _M0L4selfS787->$4;
      int32_t _M0L7_2abindS790;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS791;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS792;
      if (_M0L4sizeS2493 >= _M0L8grow__atS2494) {
        int32_t _M0L14capacity__maskS2496;
        int32_t _M0L6_2atmpS2495;
        moonbit_incref(_M0L4selfS787);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS787);
        _M0L14capacity__maskS2496 = _M0L4selfS787->$3;
        _M0L6_2atmpS2495 = _M0L4hashS789 & _M0L14capacity__maskS2496;
        _M0L3pslS784 = 0;
        _M0L3idxS785 = _M0L6_2atmpS2495;
        continue;
      }
      _M0L7_2abindS790 = _M0L4selfS787->$6;
      _M0L7_2abindS791 = 0;
      _M0L5entryS792
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS792)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS792->$0 = _M0L7_2abindS790;
      _M0L5entryS792->$1 = _M0L7_2abindS791;
      _M0L5entryS792->$2 = _M0L3pslS784;
      _M0L5entryS792->$3 = _M0L4hashS789;
      _M0L5entryS792->$4 = _M0L3keyS793;
      _M0L5entryS792->$5 = _M0L5valueS794;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS787, _M0L3idxS785, _M0L5entryS792);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS795 =
        _M0L7_2abindS786;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS796 =
        _M0L7_2aSomeS795;
      int32_t _M0L4hashS2498 = _M0L14_2acurr__entryS796->$3;
      int32_t _if__result_4554;
      int32_t _M0L3pslS2499;
      int32_t _M0L6_2atmpS2504;
      int32_t _M0L6_2atmpS2506;
      int32_t _M0L14capacity__maskS2507;
      int32_t _M0L6_2atmpS2505;
      if (_M0L4hashS2498 == _M0L4hashS789) {
        int32_t _M0L3keyS2497 = _M0L14_2acurr__entryS796->$4;
        _if__result_4554 = _M0L3keyS2497 == _M0L3keyS793;
      } else {
        _if__result_4554 = 0;
      }
      if (_if__result_4554) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3935;
        moonbit_incref(_M0L14_2acurr__entryS796);
        moonbit_decref(_M0L4selfS787);
        _M0L6_2aoldS3935 = _M0L14_2acurr__entryS796->$5;
        moonbit_decref(_M0L6_2aoldS3935);
        _M0L14_2acurr__entryS796->$5 = _M0L5valueS794;
        moonbit_decref(_M0L14_2acurr__entryS796);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS796);
      }
      _M0L3pslS2499 = _M0L14_2acurr__entryS796->$2;
      if (_M0L3pslS784 > _M0L3pslS2499) {
        int32_t _M0L4sizeS2500 = _M0L4selfS787->$1;
        int32_t _M0L8grow__atS2501 = _M0L4selfS787->$4;
        int32_t _M0L7_2abindS797;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS798;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS799;
        if (_M0L4sizeS2500 >= _M0L8grow__atS2501) {
          int32_t _M0L14capacity__maskS2503;
          int32_t _M0L6_2atmpS2502;
          moonbit_decref(_M0L14_2acurr__entryS796);
          moonbit_incref(_M0L4selfS787);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS787);
          _M0L14capacity__maskS2503 = _M0L4selfS787->$3;
          _M0L6_2atmpS2502 = _M0L4hashS789 & _M0L14capacity__maskS2503;
          _M0L3pslS784 = 0;
          _M0L3idxS785 = _M0L6_2atmpS2502;
          continue;
        }
        moonbit_incref(_M0L4selfS787);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS787, _M0L3idxS785, _M0L14_2acurr__entryS796);
        _M0L7_2abindS797 = _M0L4selfS787->$6;
        _M0L7_2abindS798 = 0;
        _M0L5entryS799
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS799)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS799->$0 = _M0L7_2abindS797;
        _M0L5entryS799->$1 = _M0L7_2abindS798;
        _M0L5entryS799->$2 = _M0L3pslS784;
        _M0L5entryS799->$3 = _M0L4hashS789;
        _M0L5entryS799->$4 = _M0L3keyS793;
        _M0L5entryS799->$5 = _M0L5valueS794;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS787, _M0L3idxS785, _M0L5entryS799);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS796);
      }
      _M0L6_2atmpS2504 = _M0L3pslS784 + 1;
      _M0L6_2atmpS2506 = _M0L3idxS785 + 1;
      _M0L14capacity__maskS2507 = _M0L4selfS787->$3;
      _M0L6_2atmpS2505 = _M0L6_2atmpS2506 & _M0L14capacity__maskS2507;
      _M0L3pslS784 = _M0L6_2atmpS2504;
      _M0L3idxS785 = _M0L6_2atmpS2505;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS803,
  struct _M0TPC16string10StringView _M0L3keyS809,
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L5valueS810,
  int32_t _M0L4hashS805
) {
  int32_t _M0L14capacity__maskS2528;
  int32_t _M0L6_2atmpS2527;
  int32_t _M0L3pslS800;
  int32_t _M0L3idxS801;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2528 = _M0L4selfS803->$3;
  _M0L6_2atmpS2527 = _M0L4hashS805 & _M0L14capacity__maskS2528;
  _M0L3pslS800 = 0;
  _M0L3idxS801 = _M0L6_2atmpS2527;
  while (1) {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3941 =
      _M0L4selfS803->$0;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7entriesS2526 =
      _M0L8_2afieldS3941;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS3940;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS802;
    if (
      _M0L3idxS801 < 0
      || _M0L3idxS801 >= Moonbit_array_length(_M0L7entriesS2526)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3940
    = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L7entriesS2526[
        _M0L3idxS801
      ];
    _M0L7_2abindS802 = _M0L6_2atmpS3940;
    if (_M0L7_2abindS802 == 0) {
      int32_t _M0L4sizeS2511 = _M0L4selfS803->$1;
      int32_t _M0L8grow__atS2512 = _M0L4selfS803->$4;
      int32_t _M0L7_2abindS806;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS807;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L5entryS808;
      if (_M0L4sizeS2511 >= _M0L8grow__atS2512) {
        int32_t _M0L14capacity__maskS2514;
        int32_t _M0L6_2atmpS2513;
        moonbit_incref(_M0L4selfS803);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS803);
        _M0L14capacity__maskS2514 = _M0L4selfS803->$3;
        _M0L6_2atmpS2513 = _M0L4hashS805 & _M0L14capacity__maskS2514;
        _M0L3pslS800 = 0;
        _M0L3idxS801 = _M0L6_2atmpS2513;
        continue;
      }
      _M0L7_2abindS806 = _M0L4selfS803->$6;
      _M0L7_2abindS807 = 0;
      _M0L5entryS808
      = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE));
      Moonbit_object_header(_M0L5entryS808)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE, $1) >> 2, 3, 0);
      _M0L5entryS808->$0 = _M0L7_2abindS806;
      _M0L5entryS808->$1 = _M0L7_2abindS807;
      _M0L5entryS808->$2 = _M0L3pslS800;
      _M0L5entryS808->$3 = _M0L4hashS805;
      _M0L5entryS808->$4_0 = _M0L3keyS809.$0;
      _M0L5entryS808->$4_1 = _M0L3keyS809.$1;
      _M0L5entryS808->$4_2 = _M0L3keyS809.$2;
      _M0L5entryS808->$5 = _M0L5valueS810;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS803, _M0L3idxS801, _M0L5entryS808);
      return 0;
    } else {
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS811 =
        _M0L7_2abindS802;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L14_2acurr__entryS812 =
        _M0L7_2aSomeS811;
      int32_t _M0L4hashS2516 = _M0L14_2acurr__entryS812->$3;
      int32_t _if__result_4556;
      int32_t _M0L3pslS2517;
      int32_t _M0L6_2atmpS2522;
      int32_t _M0L6_2atmpS2524;
      int32_t _M0L14capacity__maskS2525;
      int32_t _M0L6_2atmpS2523;
      if (_M0L4hashS2516 == _M0L4hashS805) {
        struct _M0TPC16string10StringView _M0L8_2afieldS3939 =
          (struct _M0TPC16string10StringView){_M0L14_2acurr__entryS812->$4_1,
                                                _M0L14_2acurr__entryS812->$4_2,
                                                _M0L14_2acurr__entryS812->$4_0};
        struct _M0TPC16string10StringView _M0L3keyS2515 = _M0L8_2afieldS3939;
        moonbit_incref(_M0L3keyS2515.$0);
        moonbit_incref(_M0L14_2acurr__entryS812);
        moonbit_incref(_M0L3keyS809.$0);
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _if__result_4556
        = _M0IPC16string10StringViewPB2Eq5equal(_M0L3keyS2515, _M0L3keyS809);
      } else {
        moonbit_incref(_M0L14_2acurr__entryS812);
        _if__result_4556 = 0;
      }
      if (_if__result_4556) {
        struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L6_2aoldS3938;
        moonbit_decref(_M0L3keyS809.$0);
        moonbit_decref(_M0L4selfS803);
        _M0L6_2aoldS3938 = _M0L14_2acurr__entryS812->$5;
        moonbit_decref(_M0L6_2aoldS3938);
        _M0L14_2acurr__entryS812->$5 = _M0L5valueS810;
        moonbit_decref(_M0L14_2acurr__entryS812);
        return 0;
      }
      _M0L3pslS2517 = _M0L14_2acurr__entryS812->$2;
      if (_M0L3pslS800 > _M0L3pslS2517) {
        int32_t _M0L4sizeS2518 = _M0L4selfS803->$1;
        int32_t _M0L8grow__atS2519 = _M0L4selfS803->$4;
        int32_t _M0L7_2abindS813;
        struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS814;
        struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L5entryS815;
        if (_M0L4sizeS2518 >= _M0L8grow__atS2519) {
          int32_t _M0L14capacity__maskS2521;
          int32_t _M0L6_2atmpS2520;
          moonbit_decref(_M0L14_2acurr__entryS812);
          moonbit_incref(_M0L4selfS803);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS803);
          _M0L14capacity__maskS2521 = _M0L4selfS803->$3;
          _M0L6_2atmpS2520 = _M0L4hashS805 & _M0L14capacity__maskS2521;
          _M0L3pslS800 = 0;
          _M0L3idxS801 = _M0L6_2atmpS2520;
          continue;
        }
        moonbit_incref(_M0L4selfS803);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS803, _M0L3idxS801, _M0L14_2acurr__entryS812);
        _M0L7_2abindS813 = _M0L4selfS803->$6;
        _M0L7_2abindS814 = 0;
        _M0L5entryS815
        = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE));
        Moonbit_object_header(_M0L5entryS815)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE, $1) >> 2, 3, 0);
        _M0L5entryS815->$0 = _M0L7_2abindS813;
        _M0L5entryS815->$1 = _M0L7_2abindS814;
        _M0L5entryS815->$2 = _M0L3pslS800;
        _M0L5entryS815->$3 = _M0L4hashS805;
        _M0L5entryS815->$4_0 = _M0L3keyS809.$0;
        _M0L5entryS815->$4_1 = _M0L3keyS809.$1;
        _M0L5entryS815->$4_2 = _M0L3keyS809.$2;
        _M0L5entryS815->$5 = _M0L5valueS810;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS803, _M0L3idxS801, _M0L5entryS815);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS812);
      }
      _M0L6_2atmpS2522 = _M0L3pslS800 + 1;
      _M0L6_2atmpS2524 = _M0L3idxS801 + 1;
      _M0L14capacity__maskS2525 = _M0L4selfS803->$3;
      _M0L6_2atmpS2523 = _M0L6_2atmpS2524 & _M0L14capacity__maskS2525;
      _M0L3pslS800 = _M0L6_2atmpS2522;
      _M0L3idxS801 = _M0L6_2atmpS2523;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS742,
  int32_t _M0L3idxS747,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS746
) {
  int32_t _M0L3pslS2442;
  int32_t _M0L6_2atmpS2438;
  int32_t _M0L6_2atmpS2440;
  int32_t _M0L14capacity__maskS2441;
  int32_t _M0L6_2atmpS2439;
  int32_t _M0L3pslS738;
  int32_t _M0L3idxS739;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS740;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2442 = _M0L5entryS746->$2;
  _M0L6_2atmpS2438 = _M0L3pslS2442 + 1;
  _M0L6_2atmpS2440 = _M0L3idxS747 + 1;
  _M0L14capacity__maskS2441 = _M0L4selfS742->$3;
  _M0L6_2atmpS2439 = _M0L6_2atmpS2440 & _M0L14capacity__maskS2441;
  _M0L3pslS738 = _M0L6_2atmpS2438;
  _M0L3idxS739 = _M0L6_2atmpS2439;
  _M0L5entryS740 = _M0L5entryS746;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3943 =
      _M0L4selfS742->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2437 =
      _M0L8_2afieldS3943;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3942;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS741;
    if (
      _M0L3idxS739 < 0
      || _M0L3idxS739 >= Moonbit_array_length(_M0L7entriesS2437)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3942
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2437[
        _M0L3idxS739
      ];
    _M0L7_2abindS741 = _M0L6_2atmpS3942;
    if (_M0L7_2abindS741 == 0) {
      _M0L5entryS740->$2 = _M0L3pslS738;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS742, _M0L5entryS740, _M0L3idxS739);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS744 =
        _M0L7_2abindS741;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS745 =
        _M0L7_2aSomeS744;
      int32_t _M0L3pslS2427 = _M0L14_2acurr__entryS745->$2;
      if (_M0L3pslS738 > _M0L3pslS2427) {
        int32_t _M0L3pslS2432;
        int32_t _M0L6_2atmpS2428;
        int32_t _M0L6_2atmpS2430;
        int32_t _M0L14capacity__maskS2431;
        int32_t _M0L6_2atmpS2429;
        _M0L5entryS740->$2 = _M0L3pslS738;
        moonbit_incref(_M0L14_2acurr__entryS745);
        moonbit_incref(_M0L4selfS742);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS742, _M0L5entryS740, _M0L3idxS739);
        _M0L3pslS2432 = _M0L14_2acurr__entryS745->$2;
        _M0L6_2atmpS2428 = _M0L3pslS2432 + 1;
        _M0L6_2atmpS2430 = _M0L3idxS739 + 1;
        _M0L14capacity__maskS2431 = _M0L4selfS742->$3;
        _M0L6_2atmpS2429 = _M0L6_2atmpS2430 & _M0L14capacity__maskS2431;
        _M0L3pslS738 = _M0L6_2atmpS2428;
        _M0L3idxS739 = _M0L6_2atmpS2429;
        _M0L5entryS740 = _M0L14_2acurr__entryS745;
        continue;
      } else {
        int32_t _M0L6_2atmpS2433 = _M0L3pslS738 + 1;
        int32_t _M0L6_2atmpS2435 = _M0L3idxS739 + 1;
        int32_t _M0L14capacity__maskS2436 = _M0L4selfS742->$3;
        int32_t _M0L6_2atmpS2434 =
          _M0L6_2atmpS2435 & _M0L14capacity__maskS2436;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4558 =
          _M0L5entryS740;
        _M0L3pslS738 = _M0L6_2atmpS2433;
        _M0L3idxS739 = _M0L6_2atmpS2434;
        _M0L5entryS740 = _tmp_4558;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS752,
  int32_t _M0L3idxS757,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS756
) {
  int32_t _M0L3pslS2458;
  int32_t _M0L6_2atmpS2454;
  int32_t _M0L6_2atmpS2456;
  int32_t _M0L14capacity__maskS2457;
  int32_t _M0L6_2atmpS2455;
  int32_t _M0L3pslS748;
  int32_t _M0L3idxS749;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS750;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2458 = _M0L5entryS756->$2;
  _M0L6_2atmpS2454 = _M0L3pslS2458 + 1;
  _M0L6_2atmpS2456 = _M0L3idxS757 + 1;
  _M0L14capacity__maskS2457 = _M0L4selfS752->$3;
  _M0L6_2atmpS2455 = _M0L6_2atmpS2456 & _M0L14capacity__maskS2457;
  _M0L3pslS748 = _M0L6_2atmpS2454;
  _M0L3idxS749 = _M0L6_2atmpS2455;
  _M0L5entryS750 = _M0L5entryS756;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3945 =
      _M0L4selfS752->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2453 =
      _M0L8_2afieldS3945;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3944;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS751;
    if (
      _M0L3idxS749 < 0
      || _M0L3idxS749 >= Moonbit_array_length(_M0L7entriesS2453)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3944
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2453[
        _M0L3idxS749
      ];
    _M0L7_2abindS751 = _M0L6_2atmpS3944;
    if (_M0L7_2abindS751 == 0) {
      _M0L5entryS750->$2 = _M0L3pslS748;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS752, _M0L5entryS750, _M0L3idxS749);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS754 =
        _M0L7_2abindS751;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS755 =
        _M0L7_2aSomeS754;
      int32_t _M0L3pslS2443 = _M0L14_2acurr__entryS755->$2;
      if (_M0L3pslS748 > _M0L3pslS2443) {
        int32_t _M0L3pslS2448;
        int32_t _M0L6_2atmpS2444;
        int32_t _M0L6_2atmpS2446;
        int32_t _M0L14capacity__maskS2447;
        int32_t _M0L6_2atmpS2445;
        _M0L5entryS750->$2 = _M0L3pslS748;
        moonbit_incref(_M0L14_2acurr__entryS755);
        moonbit_incref(_M0L4selfS752);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS752, _M0L5entryS750, _M0L3idxS749);
        _M0L3pslS2448 = _M0L14_2acurr__entryS755->$2;
        _M0L6_2atmpS2444 = _M0L3pslS2448 + 1;
        _M0L6_2atmpS2446 = _M0L3idxS749 + 1;
        _M0L14capacity__maskS2447 = _M0L4selfS752->$3;
        _M0L6_2atmpS2445 = _M0L6_2atmpS2446 & _M0L14capacity__maskS2447;
        _M0L3pslS748 = _M0L6_2atmpS2444;
        _M0L3idxS749 = _M0L6_2atmpS2445;
        _M0L5entryS750 = _M0L14_2acurr__entryS755;
        continue;
      } else {
        int32_t _M0L6_2atmpS2449 = _M0L3pslS748 + 1;
        int32_t _M0L6_2atmpS2451 = _M0L3idxS749 + 1;
        int32_t _M0L14capacity__maskS2452 = _M0L4selfS752->$3;
        int32_t _M0L6_2atmpS2450 =
          _M0L6_2atmpS2451 & _M0L14capacity__maskS2452;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4560 =
          _M0L5entryS750;
        _M0L3pslS748 = _M0L6_2atmpS2449;
        _M0L3idxS749 = _M0L6_2atmpS2450;
        _M0L5entryS750 = _tmp_4560;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS762,
  int32_t _M0L3idxS767,
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L5entryS766
) {
  int32_t _M0L3pslS2474;
  int32_t _M0L6_2atmpS2470;
  int32_t _M0L6_2atmpS2472;
  int32_t _M0L14capacity__maskS2473;
  int32_t _M0L6_2atmpS2471;
  int32_t _M0L3pslS758;
  int32_t _M0L3idxS759;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L5entryS760;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2474 = _M0L5entryS766->$2;
  _M0L6_2atmpS2470 = _M0L3pslS2474 + 1;
  _M0L6_2atmpS2472 = _M0L3idxS767 + 1;
  _M0L14capacity__maskS2473 = _M0L4selfS762->$3;
  _M0L6_2atmpS2471 = _M0L6_2atmpS2472 & _M0L14capacity__maskS2473;
  _M0L3pslS758 = _M0L6_2atmpS2470;
  _M0L3idxS759 = _M0L6_2atmpS2471;
  _M0L5entryS760 = _M0L5entryS766;
  while (1) {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3947 =
      _M0L4selfS762->$0;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7entriesS2469 =
      _M0L8_2afieldS3947;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS3946;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS761;
    if (
      _M0L3idxS759 < 0
      || _M0L3idxS759 >= Moonbit_array_length(_M0L7entriesS2469)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3946
    = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L7entriesS2469[
        _M0L3idxS759
      ];
    _M0L7_2abindS761 = _M0L6_2atmpS3946;
    if (_M0L7_2abindS761 == 0) {
      _M0L5entryS760->$2 = _M0L3pslS758;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS762, _M0L5entryS760, _M0L3idxS759);
      break;
    } else {
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS764 =
        _M0L7_2abindS761;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L14_2acurr__entryS765 =
        _M0L7_2aSomeS764;
      int32_t _M0L3pslS2459 = _M0L14_2acurr__entryS765->$2;
      if (_M0L3pslS758 > _M0L3pslS2459) {
        int32_t _M0L3pslS2464;
        int32_t _M0L6_2atmpS2460;
        int32_t _M0L6_2atmpS2462;
        int32_t _M0L14capacity__maskS2463;
        int32_t _M0L6_2atmpS2461;
        _M0L5entryS760->$2 = _M0L3pslS758;
        moonbit_incref(_M0L14_2acurr__entryS765);
        moonbit_incref(_M0L4selfS762);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(_M0L4selfS762, _M0L5entryS760, _M0L3idxS759);
        _M0L3pslS2464 = _M0L14_2acurr__entryS765->$2;
        _M0L6_2atmpS2460 = _M0L3pslS2464 + 1;
        _M0L6_2atmpS2462 = _M0L3idxS759 + 1;
        _M0L14capacity__maskS2463 = _M0L4selfS762->$3;
        _M0L6_2atmpS2461 = _M0L6_2atmpS2462 & _M0L14capacity__maskS2463;
        _M0L3pslS758 = _M0L6_2atmpS2460;
        _M0L3idxS759 = _M0L6_2atmpS2461;
        _M0L5entryS760 = _M0L14_2acurr__entryS765;
        continue;
      } else {
        int32_t _M0L6_2atmpS2465 = _M0L3pslS758 + 1;
        int32_t _M0L6_2atmpS2467 = _M0L3idxS759 + 1;
        int32_t _M0L14capacity__maskS2468 = _M0L4selfS762->$3;
        int32_t _M0L6_2atmpS2466 =
          _M0L6_2atmpS2467 & _M0L14capacity__maskS2468;
        struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _tmp_4562 =
          _M0L5entryS760;
        _M0L3pslS758 = _M0L6_2atmpS2465;
        _M0L3idxS759 = _M0L6_2atmpS2466;
        _M0L5entryS760 = _tmp_4562;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS720,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS722,
  int32_t _M0L8new__idxS721
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3950;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2421;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2422;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3949;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3948;
  int32_t _M0L6_2acntS4378;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS723;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3950 = _M0L4selfS720->$0;
  _M0L7entriesS2421 = _M0L8_2afieldS3950;
  moonbit_incref(_M0L5entryS722);
  _M0L6_2atmpS2422 = _M0L5entryS722;
  if (
    _M0L8new__idxS721 < 0
    || _M0L8new__idxS721 >= Moonbit_array_length(_M0L7entriesS2421)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3949
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2421[
      _M0L8new__idxS721
    ];
  if (_M0L6_2aoldS3949) {
    moonbit_decref(_M0L6_2aoldS3949);
  }
  _M0L7entriesS2421[_M0L8new__idxS721] = _M0L6_2atmpS2422;
  _M0L8_2afieldS3948 = _M0L5entryS722->$1;
  _M0L6_2acntS4378 = Moonbit_object_header(_M0L5entryS722)->rc;
  if (_M0L6_2acntS4378 > 1) {
    int32_t _M0L11_2anew__cntS4381 = _M0L6_2acntS4378 - 1;
    Moonbit_object_header(_M0L5entryS722)->rc = _M0L11_2anew__cntS4381;
    if (_M0L8_2afieldS3948) {
      moonbit_incref(_M0L8_2afieldS3948);
    }
  } else if (_M0L6_2acntS4378 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4380 =
      _M0L5entryS722->$5;
    moonbit_string_t _M0L8_2afieldS4379;
    moonbit_decref(_M0L8_2afieldS4380);
    _M0L8_2afieldS4379 = _M0L5entryS722->$4;
    moonbit_decref(_M0L8_2afieldS4379);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS722);
  }
  _M0L7_2abindS723 = _M0L8_2afieldS3948;
  if (_M0L7_2abindS723 == 0) {
    if (_M0L7_2abindS723) {
      moonbit_decref(_M0L7_2abindS723);
    }
    _M0L4selfS720->$6 = _M0L8new__idxS721;
    moonbit_decref(_M0L4selfS720);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS724;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS725;
    moonbit_decref(_M0L4selfS720);
    _M0L7_2aSomeS724 = _M0L7_2abindS723;
    _M0L7_2anextS725 = _M0L7_2aSomeS724;
    _M0L7_2anextS725->$0 = _M0L8new__idxS721;
    moonbit_decref(_M0L7_2anextS725);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS726,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS728,
  int32_t _M0L8new__idxS727
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3953;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2423;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2424;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3952;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3951;
  int32_t _M0L6_2acntS4382;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS729;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3953 = _M0L4selfS726->$0;
  _M0L7entriesS2423 = _M0L8_2afieldS3953;
  moonbit_incref(_M0L5entryS728);
  _M0L6_2atmpS2424 = _M0L5entryS728;
  if (
    _M0L8new__idxS727 < 0
    || _M0L8new__idxS727 >= Moonbit_array_length(_M0L7entriesS2423)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3952
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2423[
      _M0L8new__idxS727
    ];
  if (_M0L6_2aoldS3952) {
    moonbit_decref(_M0L6_2aoldS3952);
  }
  _M0L7entriesS2423[_M0L8new__idxS727] = _M0L6_2atmpS2424;
  _M0L8_2afieldS3951 = _M0L5entryS728->$1;
  _M0L6_2acntS4382 = Moonbit_object_header(_M0L5entryS728)->rc;
  if (_M0L6_2acntS4382 > 1) {
    int32_t _M0L11_2anew__cntS4384 = _M0L6_2acntS4382 - 1;
    Moonbit_object_header(_M0L5entryS728)->rc = _M0L11_2anew__cntS4384;
    if (_M0L8_2afieldS3951) {
      moonbit_incref(_M0L8_2afieldS3951);
    }
  } else if (_M0L6_2acntS4382 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4383 =
      _M0L5entryS728->$5;
    moonbit_decref(_M0L8_2afieldS4383);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS728);
  }
  _M0L7_2abindS729 = _M0L8_2afieldS3951;
  if (_M0L7_2abindS729 == 0) {
    if (_M0L7_2abindS729) {
      moonbit_decref(_M0L7_2abindS729);
    }
    _M0L4selfS726->$6 = _M0L8new__idxS727;
    moonbit_decref(_M0L4selfS726);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS730;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS731;
    moonbit_decref(_M0L4selfS726);
    _M0L7_2aSomeS730 = _M0L7_2abindS729;
    _M0L7_2anextS731 = _M0L7_2aSomeS730;
    _M0L7_2anextS731->$0 = _M0L8new__idxS727;
    moonbit_decref(_M0L7_2anextS731);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS732,
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L5entryS734,
  int32_t _M0L8new__idxS733
) {
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3956;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7entriesS2425;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2426;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aoldS3955;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L8_2afieldS3954;
  int32_t _M0L6_2acntS4385;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS735;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3956 = _M0L4selfS732->$0;
  _M0L7entriesS2425 = _M0L8_2afieldS3956;
  moonbit_incref(_M0L5entryS734);
  _M0L6_2atmpS2426 = _M0L5entryS734;
  if (
    _M0L8new__idxS733 < 0
    || _M0L8new__idxS733 >= Moonbit_array_length(_M0L7entriesS2425)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3955
  = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L7entriesS2425[
      _M0L8new__idxS733
    ];
  if (_M0L6_2aoldS3955) {
    moonbit_decref(_M0L6_2aoldS3955);
  }
  _M0L7entriesS2425[_M0L8new__idxS733] = _M0L6_2atmpS2426;
  _M0L8_2afieldS3954 = _M0L5entryS734->$1;
  _M0L6_2acntS4385 = Moonbit_object_header(_M0L5entryS734)->rc;
  if (_M0L6_2acntS4385 > 1) {
    int32_t _M0L11_2anew__cntS4388 = _M0L6_2acntS4385 - 1;
    Moonbit_object_header(_M0L5entryS734)->rc = _M0L11_2anew__cntS4388;
    if (_M0L8_2afieldS3954) {
      moonbit_incref(_M0L8_2afieldS3954);
    }
  } else if (_M0L6_2acntS4385 == 1) {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L8_2afieldS4387 =
      _M0L5entryS734->$5;
    struct _M0TPC16string10StringView _M0L8_2afieldS4386;
    moonbit_decref(_M0L8_2afieldS4387);
    _M0L8_2afieldS4386
    = (struct _M0TPC16string10StringView){
      _M0L5entryS734->$4_1, _M0L5entryS734->$4_2, _M0L5entryS734->$4_0
    };
    moonbit_decref(_M0L8_2afieldS4386.$0);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS734);
  }
  _M0L7_2abindS735 = _M0L8_2afieldS3954;
  if (_M0L7_2abindS735 == 0) {
    if (_M0L7_2abindS735) {
      moonbit_decref(_M0L7_2abindS735);
    }
    _M0L4selfS732->$6 = _M0L8new__idxS733;
    moonbit_decref(_M0L4selfS732);
  } else {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS736;
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2anextS737;
    moonbit_decref(_M0L4selfS732);
    _M0L7_2aSomeS736 = _M0L7_2abindS735;
    _M0L7_2anextS737 = _M0L7_2aSomeS736;
    _M0L7_2anextS737->$0 = _M0L8new__idxS733;
    moonbit_decref(_M0L7_2anextS737);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS709,
  int32_t _M0L3idxS711,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS710
) {
  int32_t _M0L7_2abindS708;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3958;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2399;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2400;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3957;
  int32_t _M0L4sizeS2402;
  int32_t _M0L6_2atmpS2401;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS708 = _M0L4selfS709->$6;
  switch (_M0L7_2abindS708) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2394;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3959;
      moonbit_incref(_M0L5entryS710);
      _M0L6_2atmpS2394 = _M0L5entryS710;
      _M0L6_2aoldS3959 = _M0L4selfS709->$5;
      if (_M0L6_2aoldS3959) {
        moonbit_decref(_M0L6_2aoldS3959);
      }
      _M0L4selfS709->$5 = _M0L6_2atmpS2394;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3962 =
        _M0L4selfS709->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2398 =
        _M0L8_2afieldS3962;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3961;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2397;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2395;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2396;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3960;
      if (
        _M0L7_2abindS708 < 0
        || _M0L7_2abindS708 >= Moonbit_array_length(_M0L7entriesS2398)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3961
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2398[
          _M0L7_2abindS708
        ];
      _M0L6_2atmpS2397 = _M0L6_2atmpS3961;
      if (_M0L6_2atmpS2397) {
        moonbit_incref(_M0L6_2atmpS2397);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2395
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2397);
      moonbit_incref(_M0L5entryS710);
      _M0L6_2atmpS2396 = _M0L5entryS710;
      _M0L6_2aoldS3960 = _M0L6_2atmpS2395->$1;
      if (_M0L6_2aoldS3960) {
        moonbit_decref(_M0L6_2aoldS3960);
      }
      _M0L6_2atmpS2395->$1 = _M0L6_2atmpS2396;
      moonbit_decref(_M0L6_2atmpS2395);
      break;
    }
  }
  _M0L4selfS709->$6 = _M0L3idxS711;
  _M0L8_2afieldS3958 = _M0L4selfS709->$0;
  _M0L7entriesS2399 = _M0L8_2afieldS3958;
  _M0L6_2atmpS2400 = _M0L5entryS710;
  if (
    _M0L3idxS711 < 0
    || _M0L3idxS711 >= Moonbit_array_length(_M0L7entriesS2399)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3957
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2399[
      _M0L3idxS711
    ];
  if (_M0L6_2aoldS3957) {
    moonbit_decref(_M0L6_2aoldS3957);
  }
  _M0L7entriesS2399[_M0L3idxS711] = _M0L6_2atmpS2400;
  _M0L4sizeS2402 = _M0L4selfS709->$1;
  _M0L6_2atmpS2401 = _M0L4sizeS2402 + 1;
  _M0L4selfS709->$1 = _M0L6_2atmpS2401;
  moonbit_decref(_M0L4selfS709);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS713,
  int32_t _M0L3idxS715,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS714
) {
  int32_t _M0L7_2abindS712;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3964;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2408;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2409;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3963;
  int32_t _M0L4sizeS2411;
  int32_t _M0L6_2atmpS2410;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS712 = _M0L4selfS713->$6;
  switch (_M0L7_2abindS712) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2403;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3965;
      moonbit_incref(_M0L5entryS714);
      _M0L6_2atmpS2403 = _M0L5entryS714;
      _M0L6_2aoldS3965 = _M0L4selfS713->$5;
      if (_M0L6_2aoldS3965) {
        moonbit_decref(_M0L6_2aoldS3965);
      }
      _M0L4selfS713->$5 = _M0L6_2atmpS2403;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3968 =
        _M0L4selfS713->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2407 =
        _M0L8_2afieldS3968;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3967;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2406;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2404;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2405;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3966;
      if (
        _M0L7_2abindS712 < 0
        || _M0L7_2abindS712 >= Moonbit_array_length(_M0L7entriesS2407)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3967
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2407[
          _M0L7_2abindS712
        ];
      _M0L6_2atmpS2406 = _M0L6_2atmpS3967;
      if (_M0L6_2atmpS2406) {
        moonbit_incref(_M0L6_2atmpS2406);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2404
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2406);
      moonbit_incref(_M0L5entryS714);
      _M0L6_2atmpS2405 = _M0L5entryS714;
      _M0L6_2aoldS3966 = _M0L6_2atmpS2404->$1;
      if (_M0L6_2aoldS3966) {
        moonbit_decref(_M0L6_2aoldS3966);
      }
      _M0L6_2atmpS2404->$1 = _M0L6_2atmpS2405;
      moonbit_decref(_M0L6_2atmpS2404);
      break;
    }
  }
  _M0L4selfS713->$6 = _M0L3idxS715;
  _M0L8_2afieldS3964 = _M0L4selfS713->$0;
  _M0L7entriesS2408 = _M0L8_2afieldS3964;
  _M0L6_2atmpS2409 = _M0L5entryS714;
  if (
    _M0L3idxS715 < 0
    || _M0L3idxS715 >= Moonbit_array_length(_M0L7entriesS2408)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3963
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2408[
      _M0L3idxS715
    ];
  if (_M0L6_2aoldS3963) {
    moonbit_decref(_M0L6_2aoldS3963);
  }
  _M0L7entriesS2408[_M0L3idxS715] = _M0L6_2atmpS2409;
  _M0L4sizeS2411 = _M0L4selfS713->$1;
  _M0L6_2atmpS2410 = _M0L4sizeS2411 + 1;
  _M0L4selfS713->$1 = _M0L6_2atmpS2410;
  moonbit_decref(_M0L4selfS713);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS717,
  int32_t _M0L3idxS719,
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L5entryS718
) {
  int32_t _M0L7_2abindS716;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3970;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7entriesS2417;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2418;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aoldS3969;
  int32_t _M0L4sizeS2420;
  int32_t _M0L6_2atmpS2419;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS716 = _M0L4selfS717->$6;
  switch (_M0L7_2abindS716) {
    case -1: {
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2412;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aoldS3971;
      moonbit_incref(_M0L5entryS718);
      _M0L6_2atmpS2412 = _M0L5entryS718;
      _M0L6_2aoldS3971 = _M0L4selfS717->$5;
      if (_M0L6_2aoldS3971) {
        moonbit_decref(_M0L6_2aoldS3971);
      }
      _M0L4selfS717->$5 = _M0L6_2atmpS2412;
      break;
    }
    default: {
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L8_2afieldS3974 =
        _M0L4selfS717->$0;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7entriesS2416 =
        _M0L8_2afieldS3974;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS3973;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2415;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2413;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2414;
      struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2aoldS3972;
      if (
        _M0L7_2abindS716 < 0
        || _M0L7_2abindS716 >= Moonbit_array_length(_M0L7entriesS2416)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3973
      = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L7entriesS2416[
          _M0L7_2abindS716
        ];
      _M0L6_2atmpS2415 = _M0L6_2atmpS3973;
      if (_M0L6_2atmpS2415) {
        moonbit_incref(_M0L6_2atmpS2415);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2413
      = _M0MPC16option6Option6unwrapGRPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(_M0L6_2atmpS2415);
      moonbit_incref(_M0L5entryS718);
      _M0L6_2atmpS2414 = _M0L5entryS718;
      _M0L6_2aoldS3972 = _M0L6_2atmpS2413->$1;
      if (_M0L6_2aoldS3972) {
        moonbit_decref(_M0L6_2aoldS3972);
      }
      _M0L6_2atmpS2413->$1 = _M0L6_2atmpS2414;
      moonbit_decref(_M0L6_2atmpS2413);
      break;
    }
  }
  _M0L4selfS717->$6 = _M0L3idxS719;
  _M0L8_2afieldS3970 = _M0L4selfS717->$0;
  _M0L7entriesS2417 = _M0L8_2afieldS3970;
  _M0L6_2atmpS2418 = _M0L5entryS718;
  if (
    _M0L3idxS719 < 0
    || _M0L3idxS719 >= Moonbit_array_length(_M0L7entriesS2417)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3969
  = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)_M0L7entriesS2417[
      _M0L3idxS719
    ];
  if (_M0L6_2aoldS3969) {
    moonbit_decref(_M0L6_2aoldS3969);
  }
  _M0L7entriesS2417[_M0L3idxS719] = _M0L6_2atmpS2418;
  _M0L4sizeS2420 = _M0L4selfS717->$1;
  _M0L6_2atmpS2419 = _M0L4sizeS2420 + 1;
  _M0L4selfS717->$1 = _M0L6_2atmpS2419;
  moonbit_decref(_M0L4selfS717);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS691
) {
  int32_t _M0L8capacityS690;
  int32_t _M0L7_2abindS692;
  int32_t _M0L7_2abindS693;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2391;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS694;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS695;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4563;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS690
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS691);
  _M0L7_2abindS692 = _M0L8capacityS690 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS693 = _M0FPB21calc__grow__threshold(_M0L8capacityS690);
  _M0L6_2atmpS2391 = 0;
  _M0L7_2abindS694
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS690, _M0L6_2atmpS2391);
  _M0L7_2abindS695 = 0;
  _block_4563
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4563)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4563->$0 = _M0L7_2abindS694;
  _block_4563->$1 = 0;
  _block_4563->$2 = _M0L8capacityS690;
  _block_4563->$3 = _M0L7_2abindS692;
  _block_4563->$4 = _M0L7_2abindS693;
  _block_4563->$5 = _M0L7_2abindS695;
  _block_4563->$6 = -1;
  return _block_4563;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS697
) {
  int32_t _M0L8capacityS696;
  int32_t _M0L7_2abindS698;
  int32_t _M0L7_2abindS699;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2392;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS700;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS701;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4564;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS696
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS697);
  _M0L7_2abindS698 = _M0L8capacityS696 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS699 = _M0FPB21calc__grow__threshold(_M0L8capacityS696);
  _M0L6_2atmpS2392 = 0;
  _M0L7_2abindS700
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS696, _M0L6_2atmpS2392);
  _M0L7_2abindS701 = 0;
  _block_4564
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4564)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4564->$0 = _M0L7_2abindS700;
  _block_4564->$1 = 0;
  _block_4564->$2 = _M0L8capacityS696;
  _block_4564->$3 = _M0L7_2abindS698;
  _block_4564->$4 = _M0L7_2abindS699;
  _block_4564->$5 = _M0L7_2abindS701;
  _block_4564->$6 = -1;
  return _block_4564;
}

struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB3Map11new_2einnerGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE(
  int32_t _M0L8capacityS703
) {
  int32_t _M0L8capacityS702;
  int32_t _M0L7_2abindS704;
  int32_t _M0L7_2abindS705;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L6_2atmpS2393;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE** _M0L7_2abindS706;
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2abindS707;
  struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _block_4565;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS702
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS703);
  _M0L7_2abindS704 = _M0L8capacityS702 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS705 = _M0FPB21calc__grow__threshold(_M0L8capacityS702);
  _M0L6_2atmpS2393 = 0;
  _M0L7_2abindS706
  = (struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE**)moonbit_make_ref_array(_M0L8capacityS702, _M0L6_2atmpS2393);
  _M0L7_2abindS707 = 0;
  _block_4565
  = (struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE*)moonbit_malloc(sizeof(struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE));
  Moonbit_object_header(_block_4565)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE, $0) >> 2, 2, 0);
  _block_4565->$0 = _M0L7_2abindS706;
  _block_4565->$1 = 0;
  _block_4565->$2 = _M0L8capacityS702;
  _block_4565->$3 = _M0L7_2abindS704;
  _block_4565->$4 = _M0L7_2abindS705;
  _block_4565->$5 = _M0L7_2abindS707;
  _block_4565->$6 = -1;
  return _block_4565;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS689) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS689 >= 0) {
    int32_t _M0L6_2atmpS2390;
    int32_t _M0L6_2atmpS2389;
    int32_t _M0L6_2atmpS2388;
    int32_t _M0L6_2atmpS2387;
    if (_M0L4selfS689 <= 1) {
      return 1;
    }
    if (_M0L4selfS689 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2390 = _M0L4selfS689 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2389 = moonbit_clz32(_M0L6_2atmpS2390);
    _M0L6_2atmpS2388 = _M0L6_2atmpS2389 - 1;
    _M0L6_2atmpS2387 = 2147483647 >> (_M0L6_2atmpS2388 & 31);
    return _M0L6_2atmpS2387 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS688) {
  int32_t _M0L6_2atmpS2386;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2386 = _M0L8capacityS688 * 13;
  return _M0L6_2atmpS2386 / 16;
}

struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0MPC16option6Option6unwrapGRP48clawteam8clawteam8internal5httpx5LayerE(
  struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L4selfS680
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS680 == 0) {
    if (_M0L4selfS680) {
      moonbit_decref(_M0L4selfS680);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TP48clawteam8clawteam8internal5httpx5Layer* _M0L7_2aSomeS681 =
      _M0L4selfS680;
    return _M0L7_2aSomeS681;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS682
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS682 == 0) {
    if (_M0L4selfS682) {
      moonbit_decref(_M0L4selfS682);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS683 =
      _M0L4selfS682;
    return _M0L7_2aSomeS683;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS684
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS684 == 0) {
    if (_M0L4selfS684) {
      moonbit_decref(_M0L4selfS684);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS685 =
      _M0L4selfS684;
    return _M0L7_2aSomeS685;
  }
}

struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPC16option6Option6unwrapGRPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS686
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS686 == 0) {
    if (_M0L4selfS686) {
      moonbit_decref(_M0L4selfS686);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGRPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2aSomeS687 =
      _M0L4selfS686;
    return _M0L7_2aSomeS687;
  }
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t _M0L3lenS679
) {
  void** _M0L6_2atmpS2385;
  struct _M0TPB5ArrayGRPB4JsonE* _block_4566;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2385
  = (void**)moonbit_make_ref_array(_M0L3lenS679, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _block_4566
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_block_4566)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _block_4566->$0 = _M0L6_2atmpS2385;
  _block_4566->$1 = _M0L3lenS679;
  return _block_4566;
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS678
) {
  moonbit_string_t* _M0L6_2atmpS2384;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2384 = _M0L4selfS678;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2384);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS674,
  int32_t _M0L5indexS675
) {
  uint64_t* _M0L6_2atmpS2382;
  uint64_t _M0L6_2atmpS3975;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2382 = _M0L4selfS674;
  if (
    _M0L5indexS675 < 0
    || _M0L5indexS675 >= Moonbit_array_length(_M0L6_2atmpS2382)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3975 = (uint64_t)_M0L6_2atmpS2382[_M0L5indexS675];
  moonbit_decref(_M0L6_2atmpS2382);
  return _M0L6_2atmpS3975;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS676,
  int32_t _M0L5indexS677
) {
  uint32_t* _M0L6_2atmpS2383;
  uint32_t _M0L6_2atmpS3976;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2383 = _M0L4selfS676;
  if (
    _M0L5indexS677 < 0
    || _M0L5indexS677 >= Moonbit_array_length(_M0L6_2atmpS2383)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3976 = (uint32_t)_M0L6_2atmpS2383[_M0L5indexS677];
  moonbit_decref(_M0L6_2atmpS2383);
  return _M0L6_2atmpS3976;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS673
) {
  moonbit_string_t* _M0L6_2atmpS2380;
  int32_t _M0L6_2atmpS3977;
  int32_t _M0L6_2atmpS2381;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2379;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS673);
  _M0L6_2atmpS2380 = _M0L4selfS673;
  _M0L6_2atmpS3977 = Moonbit_array_length(_M0L4selfS673);
  moonbit_decref(_M0L4selfS673);
  _M0L6_2atmpS2381 = _M0L6_2atmpS3977;
  _M0L6_2atmpS2379
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2381, _M0L6_2atmpS2380
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2379);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS671
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS670;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__* _closure_4567;
  struct _M0TWEOs* _M0L6_2atmpS2367;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS670
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS670)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS670->$0 = 0;
  _closure_4567
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__));
  Moonbit_object_header(_closure_4567)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__, $0_0) >> 2, 2, 0);
  _closure_4567->code = &_M0MPC15array9ArrayView4iterGsEC2368l570;
  _closure_4567->$0_0 = _M0L4selfS671.$0;
  _closure_4567->$0_1 = _M0L4selfS671.$1;
  _closure_4567->$0_2 = _M0L4selfS671.$2;
  _closure_4567->$1 = _M0L1iS670;
  _M0L6_2atmpS2367 = (struct _M0TWEOs*)_closure_4567;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2367);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2368l570(
  struct _M0TWEOs* _M0L6_2aenvS2369
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__* _M0L14_2acasted__envS2370;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3982;
  struct _M0TPC13ref3RefGiE* _M0L1iS670;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3981;
  int32_t _M0L6_2acntS4389;
  struct _M0TPB9ArrayViewGsE _M0L4selfS671;
  int32_t _M0L3valS2371;
  int32_t _M0L6_2atmpS2372;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2370
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2368__l570__*)_M0L6_2aenvS2369;
  _M0L8_2afieldS3982 = _M0L14_2acasted__envS2370->$1;
  _M0L1iS670 = _M0L8_2afieldS3982;
  _M0L8_2afieldS3981
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2370->$0_1,
      _M0L14_2acasted__envS2370->$0_2,
      _M0L14_2acasted__envS2370->$0_0
  };
  _M0L6_2acntS4389 = Moonbit_object_header(_M0L14_2acasted__envS2370)->rc;
  if (_M0L6_2acntS4389 > 1) {
    int32_t _M0L11_2anew__cntS4390 = _M0L6_2acntS4389 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2370)->rc
    = _M0L11_2anew__cntS4390;
    moonbit_incref(_M0L1iS670);
    moonbit_incref(_M0L8_2afieldS3981.$0);
  } else if (_M0L6_2acntS4389 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2370);
  }
  _M0L4selfS671 = _M0L8_2afieldS3981;
  _M0L3valS2371 = _M0L1iS670->$0;
  moonbit_incref(_M0L4selfS671.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2372 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS671);
  if (_M0L3valS2371 < _M0L6_2atmpS2372) {
    moonbit_string_t* _M0L8_2afieldS3980 = _M0L4selfS671.$0;
    moonbit_string_t* _M0L3bufS2375 = _M0L8_2afieldS3980;
    int32_t _M0L8_2afieldS3979 = _M0L4selfS671.$1;
    int32_t _M0L5startS2377 = _M0L8_2afieldS3979;
    int32_t _M0L3valS2378 = _M0L1iS670->$0;
    int32_t _M0L6_2atmpS2376 = _M0L5startS2377 + _M0L3valS2378;
    moonbit_string_t _M0L6_2atmpS3978 =
      (moonbit_string_t)_M0L3bufS2375[_M0L6_2atmpS2376];
    moonbit_string_t _M0L4elemS672;
    int32_t _M0L3valS2374;
    int32_t _M0L6_2atmpS2373;
    moonbit_incref(_M0L6_2atmpS3978);
    moonbit_decref(_M0L3bufS2375);
    _M0L4elemS672 = _M0L6_2atmpS3978;
    _M0L3valS2374 = _M0L1iS670->$0;
    _M0L6_2atmpS2373 = _M0L3valS2374 + 1;
    _M0L1iS670->$0 = _M0L6_2atmpS2373;
    moonbit_decref(_M0L1iS670);
    return _M0L4elemS672;
  } else {
    moonbit_decref(_M0L4selfS671.$0);
    moonbit_decref(_M0L1iS670);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS669
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS669;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS668,
  struct _M0TPB6Logger _M0L6loggerS667
) {
  moonbit_string_t _M0L6_2atmpS2366;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2366
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS668, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS667.$0->$method_0(_M0L6loggerS667.$1, _M0L6_2atmpS2366);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS666,
  struct _M0TPB6Logger _M0L6loggerS665
) {
  moonbit_string_t _M0L6_2atmpS2365;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2365 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS666, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS665.$0->$method_0(_M0L6loggerS665.$1, _M0L6_2atmpS2365);
  return 0;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPB4Iter9to__arrayGRPB4JsonE(
  struct _M0TWEORPB4Json* _M0L4selfS661
) {
  void** _M0L6_2atmpS2364;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6resultS659;
  #line 674 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2364 = (void**)moonbit_empty_ref_array;
  _M0L6resultS659
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6resultS659)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6resultS659->$0 = _M0L6_2atmpS2364;
  _M0L6resultS659->$1 = 0;
  while (1) {
    void* _M0L7_2abindS660;
    moonbit_incref(_M0L4selfS661);
    #line 677 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L7_2abindS660 = _M0MPB4Iter4nextGRPB4JsonE(_M0L4selfS661);
    if (_M0L7_2abindS660 == 0) {
      moonbit_decref(_M0L4selfS661);
      if (_M0L7_2abindS660) {
        moonbit_decref(_M0L7_2abindS660);
      }
    } else {
      void* _M0L7_2aSomeS662 = _M0L7_2abindS660;
      void* _M0L4_2axS663 = _M0L7_2aSomeS662;
      moonbit_incref(_M0L6resultS659);
      #line 678 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
      _M0MPC15array5Array4pushGRPB4JsonE(_M0L6resultS659, _M0L4_2axS663);
      continue;
    }
    break;
  }
  return _M0L6resultS659;
}

struct _M0TWEORPB4Json* _M0MPB4Iter3mapGRPC16string10StringViewRPB4JsonE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS650,
  struct _M0TWRPC16string10StringViewERPB4Json* _M0L1fS653
) {
  struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__* _closure_4569;
  #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _closure_4569
  = (struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__*)moonbit_malloc(sizeof(struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__));
  Moonbit_object_header(_closure_4569)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__, $0) >> 2, 2, 0);
  _closure_4569->code
  = &_M0MPB4Iter3mapGRPC16string10StringViewRPB4JsonEC2356l317;
  _closure_4569->$0 = _M0L1fS653;
  _closure_4569->$1 = _M0L4selfS650;
  return (struct _M0TWEORPB4Json*)_closure_4569;
}

struct _M0TWEORPB4Json* _M0MPB4Iter3mapGRP48clawteam8clawteam8internal5httpx6MethodRPB4JsonE(
  struct _M0TWEOi* _M0L4selfS655,
  struct _M0TWiERPB4Json* _M0L1fS658
) {
  struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__* _closure_4570;
  #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _closure_4570
  = (struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__*)moonbit_malloc(sizeof(struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__));
  Moonbit_object_header(_closure_4570)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__, $0) >> 2, 2, 0);
  _closure_4570->code
  = &_M0MPB4Iter3mapGRP48clawteam8clawteam8internal5httpx6MethodRPB4JsonEC2360l317;
  _closure_4570->$0 = _M0L1fS658;
  _closure_4570->$1 = _M0L4selfS655;
  return (struct _M0TWEORPB4Json*)_closure_4570;
}

void* _M0MPB4Iter3mapGRP48clawteam8clawteam8internal5httpx6MethodRPB4JsonEC2360l317(
  struct _M0TWEORPB4Json* _M0L6_2aenvS2361
) {
  struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__* _M0L14_2acasted__envS2362;
  struct _M0TWEOi* _M0L8_2afieldS3984;
  struct _M0TWEOi* _M0L4selfS655;
  struct _M0TWiERPB4Json* _M0L8_2afieldS3983;
  int32_t _M0L6_2acntS4391;
  struct _M0TWiERPB4Json* _M0L1fS658;
  int64_t _M0L7_2abindS654;
  #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2362
  = (struct _M0R135Iter_3a_3amap_7c_5bclawteam_2fclawteam_2finternal_2fhttpx_2fMethod_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2360__l317__*)_M0L6_2aenvS2361;
  _M0L8_2afieldS3984 = _M0L14_2acasted__envS2362->$1;
  _M0L4selfS655 = _M0L8_2afieldS3984;
  _M0L8_2afieldS3983 = _M0L14_2acasted__envS2362->$0;
  _M0L6_2acntS4391 = Moonbit_object_header(_M0L14_2acasted__envS2362)->rc;
  if (_M0L6_2acntS4391 > 1) {
    int32_t _M0L11_2anew__cntS4392 = _M0L6_2acntS4391 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2362)->rc
    = _M0L11_2anew__cntS4392;
    moonbit_incref(_M0L4selfS655);
    moonbit_incref(_M0L8_2afieldS3983);
  } else if (_M0L6_2acntS4391 == 1) {
    #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2362);
  }
  _M0L1fS658 = _M0L8_2afieldS3983;
  #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2abindS654
  = _M0MPB4Iter4nextGRP48clawteam8clawteam8internal5httpx6MethodE(_M0L4selfS655);
  if (_M0L7_2abindS654 == 4294967296ll) {
    moonbit_decref(_M0L1fS658);
    return 0;
  } else {
    int64_t _M0L7_2aSomeS656 = _M0L7_2abindS654;
    int32_t _M0L4_2axS657 = (int32_t)_M0L7_2aSomeS656;
    void* _M0L6_2atmpS2363;
    #line 319 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L6_2atmpS2363 = _M0L1fS658->code(_M0L1fS658, _M0L4_2axS657);
    return _M0L6_2atmpS2363;
  }
}

void* _M0MPB4Iter3mapGRPC16string10StringViewRPB4JsonEC2356l317(
  struct _M0TWEORPB4Json* _M0L6_2aenvS2357
) {
  struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__* _M0L14_2acasted__envS2358;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L8_2afieldS3987;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS650;
  struct _M0TWRPC16string10StringViewERPB4Json* _M0L8_2afieldS3986;
  int32_t _M0L6_2acntS4393;
  struct _M0TWRPC16string10StringViewERPB4Json* _M0L1fS653;
  void* _M0L7_2abindS649;
  #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2358
  = (struct _M0R128Iter_3a_3amap_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2356__l317__*)_M0L6_2aenvS2357;
  _M0L8_2afieldS3987 = _M0L14_2acasted__envS2358->$1;
  _M0L4selfS650 = _M0L8_2afieldS3987;
  _M0L8_2afieldS3986 = _M0L14_2acasted__envS2358->$0;
  _M0L6_2acntS4393 = Moonbit_object_header(_M0L14_2acasted__envS2358)->rc;
  if (_M0L6_2acntS4393 > 1) {
    int32_t _M0L11_2anew__cntS4394 = _M0L6_2acntS4393 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2358)->rc
    = _M0L11_2anew__cntS4394;
    moonbit_incref(_M0L4selfS650);
    moonbit_incref(_M0L8_2afieldS3986);
  } else if (_M0L6_2acntS4393 == 1) {
    #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2358);
  }
  _M0L1fS653 = _M0L8_2afieldS3986;
  #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2abindS649 = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4selfS650);
  switch (Moonbit_object_tag(_M0L7_2abindS649)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS651 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS649;
      struct _M0TPC16string10StringView _M0L8_2afieldS3985 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS651->$0_1,
                                              _M0L7_2aSomeS651->$0_2,
                                              _M0L7_2aSomeS651->$0_0};
      int32_t _M0L6_2acntS4395 = Moonbit_object_header(_M0L7_2aSomeS651)->rc;
      struct _M0TPC16string10StringView _M0L4_2axS652;
      void* _M0L6_2atmpS2359;
      if (_M0L6_2acntS4395 > 1) {
        int32_t _M0L11_2anew__cntS4396 = _M0L6_2acntS4395 - 1;
        Moonbit_object_header(_M0L7_2aSomeS651)->rc = _M0L11_2anew__cntS4396;
        moonbit_incref(_M0L8_2afieldS3985.$0);
      } else if (_M0L6_2acntS4395 == 1) {
        #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
        moonbit_free(_M0L7_2aSomeS651);
      }
      _M0L4_2axS652 = _M0L8_2afieldS3985;
      #line 319 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
      _M0L6_2atmpS2359 = _M0L1fS653->code(_M0L1fS653, _M0L4_2axS652);
      return _M0L6_2atmpS2359;
      break;
    }
    default: {
      moonbit_decref(_M0L1fS653);
      moonbit_decref(_M0L7_2abindS649);
      return 0;
      break;
    }
  }
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS644) {
  int32_t _M0L3lenS643;
  struct _M0TPC13ref3RefGiE* _M0L5indexS645;
  struct _M0R38String_3a_3aiter_2eanon__u2340__l247__* _closure_4571;
  struct _M0TWEOc* _M0L6_2atmpS2339;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS643 = Moonbit_array_length(_M0L4selfS644);
  _M0L5indexS645
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS645)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS645->$0 = 0;
  _closure_4571
  = (struct _M0R38String_3a_3aiter_2eanon__u2340__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2340__l247__));
  Moonbit_object_header(_closure_4571)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2340__l247__, $0) >> 2, 2, 0);
  _closure_4571->code = &_M0MPC16string6String4iterC2340l247;
  _closure_4571->$0 = _M0L5indexS645;
  _closure_4571->$1 = _M0L4selfS644;
  _closure_4571->$2 = _M0L3lenS643;
  _M0L6_2atmpS2339 = (struct _M0TWEOc*)_closure_4571;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2339);
}

int32_t _M0MPC16string6String4iterC2340l247(
  struct _M0TWEOc* _M0L6_2aenvS2341
) {
  struct _M0R38String_3a_3aiter_2eanon__u2340__l247__* _M0L14_2acasted__envS2342;
  int32_t _M0L3lenS643;
  moonbit_string_t _M0L8_2afieldS3990;
  moonbit_string_t _M0L4selfS644;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3989;
  int32_t _M0L6_2acntS4397;
  struct _M0TPC13ref3RefGiE* _M0L5indexS645;
  int32_t _M0L3valS2343;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2342
  = (struct _M0R38String_3a_3aiter_2eanon__u2340__l247__*)_M0L6_2aenvS2341;
  _M0L3lenS643 = _M0L14_2acasted__envS2342->$2;
  _M0L8_2afieldS3990 = _M0L14_2acasted__envS2342->$1;
  _M0L4selfS644 = _M0L8_2afieldS3990;
  _M0L8_2afieldS3989 = _M0L14_2acasted__envS2342->$0;
  _M0L6_2acntS4397 = Moonbit_object_header(_M0L14_2acasted__envS2342)->rc;
  if (_M0L6_2acntS4397 > 1) {
    int32_t _M0L11_2anew__cntS4398 = _M0L6_2acntS4397 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2342)->rc
    = _M0L11_2anew__cntS4398;
    moonbit_incref(_M0L4selfS644);
    moonbit_incref(_M0L8_2afieldS3989);
  } else if (_M0L6_2acntS4397 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2342);
  }
  _M0L5indexS645 = _M0L8_2afieldS3989;
  _M0L3valS2343 = _M0L5indexS645->$0;
  if (_M0L3valS2343 < _M0L3lenS643) {
    int32_t _M0L3valS2355 = _M0L5indexS645->$0;
    int32_t _M0L2c1S646 = _M0L4selfS644[_M0L3valS2355];
    int32_t _if__result_4572;
    int32_t _M0L3valS2353;
    int32_t _M0L6_2atmpS2352;
    int32_t _M0L6_2atmpS2354;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S646)) {
      int32_t _M0L3valS2345 = _M0L5indexS645->$0;
      int32_t _M0L6_2atmpS2344 = _M0L3valS2345 + 1;
      _if__result_4572 = _M0L6_2atmpS2344 < _M0L3lenS643;
    } else {
      _if__result_4572 = 0;
    }
    if (_if__result_4572) {
      int32_t _M0L3valS2351 = _M0L5indexS645->$0;
      int32_t _M0L6_2atmpS2350 = _M0L3valS2351 + 1;
      int32_t _M0L6_2atmpS3988 = _M0L4selfS644[_M0L6_2atmpS2350];
      int32_t _M0L2c2S647;
      moonbit_decref(_M0L4selfS644);
      _M0L2c2S647 = _M0L6_2atmpS3988;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S647)) {
        int32_t _M0L6_2atmpS2348 = (int32_t)_M0L2c1S646;
        int32_t _M0L6_2atmpS2349 = (int32_t)_M0L2c2S647;
        int32_t _M0L1cS648;
        int32_t _M0L3valS2347;
        int32_t _M0L6_2atmpS2346;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS648
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2348, _M0L6_2atmpS2349);
        _M0L3valS2347 = _M0L5indexS645->$0;
        _M0L6_2atmpS2346 = _M0L3valS2347 + 2;
        _M0L5indexS645->$0 = _M0L6_2atmpS2346;
        moonbit_decref(_M0L5indexS645);
        return _M0L1cS648;
      }
    } else {
      moonbit_decref(_M0L4selfS644);
    }
    _M0L3valS2353 = _M0L5indexS645->$0;
    _M0L6_2atmpS2352 = _M0L3valS2353 + 1;
    _M0L5indexS645->$0 = _M0L6_2atmpS2352;
    moonbit_decref(_M0L5indexS645);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2354 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S646);
    return _M0L6_2atmpS2354;
  } else {
    moonbit_decref(_M0L5indexS645);
    moonbit_decref(_M0L4selfS644);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS625,
  moonbit_string_t _M0L5valueS627
) {
  int32_t _M0L3lenS2309;
  moonbit_string_t* _M0L6_2atmpS2311;
  int32_t _M0L6_2atmpS3993;
  int32_t _M0L6_2atmpS2310;
  int32_t _M0L6lengthS626;
  moonbit_string_t* _M0L8_2afieldS3992;
  moonbit_string_t* _M0L3bufS2312;
  moonbit_string_t _M0L6_2aoldS3991;
  int32_t _M0L6_2atmpS2313;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2309 = _M0L4selfS625->$1;
  moonbit_incref(_M0L4selfS625);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2311 = _M0MPC15array5Array6bufferGsE(_M0L4selfS625);
  _M0L6_2atmpS3993 = Moonbit_array_length(_M0L6_2atmpS2311);
  moonbit_decref(_M0L6_2atmpS2311);
  _M0L6_2atmpS2310 = _M0L6_2atmpS3993;
  if (_M0L3lenS2309 == _M0L6_2atmpS2310) {
    moonbit_incref(_M0L4selfS625);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS625);
  }
  _M0L6lengthS626 = _M0L4selfS625->$1;
  _M0L8_2afieldS3992 = _M0L4selfS625->$0;
  _M0L3bufS2312 = _M0L8_2afieldS3992;
  _M0L6_2aoldS3991 = (moonbit_string_t)_M0L3bufS2312[_M0L6lengthS626];
  moonbit_decref(_M0L6_2aoldS3991);
  _M0L3bufS2312[_M0L6lengthS626] = _M0L5valueS627;
  _M0L6_2atmpS2313 = _M0L6lengthS626 + 1;
  _M0L4selfS625->$1 = _M0L6_2atmpS2313;
  moonbit_decref(_M0L4selfS625);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS628,
  struct _M0TUsiE* _M0L5valueS630
) {
  int32_t _M0L3lenS2314;
  struct _M0TUsiE** _M0L6_2atmpS2316;
  int32_t _M0L6_2atmpS3996;
  int32_t _M0L6_2atmpS2315;
  int32_t _M0L6lengthS629;
  struct _M0TUsiE** _M0L8_2afieldS3995;
  struct _M0TUsiE** _M0L3bufS2317;
  struct _M0TUsiE* _M0L6_2aoldS3994;
  int32_t _M0L6_2atmpS2318;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2314 = _M0L4selfS628->$1;
  moonbit_incref(_M0L4selfS628);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2316 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS628);
  _M0L6_2atmpS3996 = Moonbit_array_length(_M0L6_2atmpS2316);
  moonbit_decref(_M0L6_2atmpS2316);
  _M0L6_2atmpS2315 = _M0L6_2atmpS3996;
  if (_M0L3lenS2314 == _M0L6_2atmpS2315) {
    moonbit_incref(_M0L4selfS628);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS628);
  }
  _M0L6lengthS629 = _M0L4selfS628->$1;
  _M0L8_2afieldS3995 = _M0L4selfS628->$0;
  _M0L3bufS2317 = _M0L8_2afieldS3995;
  _M0L6_2aoldS3994 = (struct _M0TUsiE*)_M0L3bufS2317[_M0L6lengthS629];
  if (_M0L6_2aoldS3994) {
    moonbit_decref(_M0L6_2aoldS3994);
  }
  _M0L3bufS2317[_M0L6lengthS629] = _M0L5valueS630;
  _M0L6_2atmpS2318 = _M0L6lengthS629 + 1;
  _M0L4selfS628->$1 = _M0L6_2atmpS2318;
  moonbit_decref(_M0L4selfS628);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS631,
  struct _M0TPC16string10StringView _M0L5valueS633
) {
  int32_t _M0L3lenS2319;
  struct _M0TPC16string10StringView* _M0L6_2atmpS2321;
  int32_t _M0L6_2atmpS3999;
  int32_t _M0L6_2atmpS2320;
  int32_t _M0L6lengthS632;
  struct _M0TPC16string10StringView* _M0L8_2afieldS3998;
  struct _M0TPC16string10StringView* _M0L3bufS2322;
  struct _M0TPC16string10StringView _M0L6_2aoldS3997;
  int32_t _M0L6_2atmpS2323;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2319 = _M0L4selfS631->$1;
  moonbit_incref(_M0L4selfS631);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2321
  = _M0MPC15array5Array6bufferGRPC16string10StringViewE(_M0L4selfS631);
  _M0L6_2atmpS3999 = Moonbit_array_length(_M0L6_2atmpS2321);
  moonbit_decref(_M0L6_2atmpS2321);
  _M0L6_2atmpS2320 = _M0L6_2atmpS3999;
  if (_M0L3lenS2319 == _M0L6_2atmpS2320) {
    moonbit_incref(_M0L4selfS631);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC16string10StringViewE(_M0L4selfS631);
  }
  _M0L6lengthS632 = _M0L4selfS631->$1;
  _M0L8_2afieldS3998 = _M0L4selfS631->$0;
  _M0L3bufS2322 = _M0L8_2afieldS3998;
  _M0L6_2aoldS3997 = _M0L3bufS2322[_M0L6lengthS632];
  moonbit_decref(_M0L6_2aoldS3997.$0);
  _M0L3bufS2322[_M0L6lengthS632] = _M0L5valueS633;
  _M0L6_2atmpS2323 = _M0L6lengthS632 + 1;
  _M0L4selfS631->$1 = _M0L6_2atmpS2323;
  moonbit_decref(_M0L4selfS631);
  return 0;
}

int32_t _M0MPC15array5Array4pushGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L4selfS634,
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L5valueS636
) {
  int32_t _M0L3lenS2324;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L6_2atmpS2326;
  int32_t _M0L6_2atmpS4002;
  int32_t _M0L6_2atmpS2325;
  int32_t _M0L6lengthS635;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L8_2afieldS4001;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L3bufS2327;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2aoldS4000;
  int32_t _M0L6_2atmpS2328;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2324 = _M0L4selfS634->$1;
  moonbit_incref(_M0L4selfS634);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2326
  = _M0MPC15array5Array6bufferGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(_M0L4selfS634);
  _M0L6_2atmpS4002 = Moonbit_array_length(_M0L6_2atmpS2326);
  moonbit_decref(_M0L6_2atmpS2326);
  _M0L6_2atmpS2325 = _M0L6_2atmpS4002;
  if (_M0L3lenS2324 == _M0L6_2atmpS2325) {
    moonbit_incref(_M0L4selfS634);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(_M0L4selfS634);
  }
  _M0L6lengthS635 = _M0L4selfS634->$1;
  _M0L8_2afieldS4001 = _M0L4selfS634->$0;
  _M0L3bufS2327 = _M0L8_2afieldS4001;
  _M0L6_2aoldS4000
  = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)_M0L3bufS2327[
      _M0L6lengthS635
    ];
  if (_M0L6_2aoldS4000) {
    moonbit_decref(_M0L6_2aoldS4000);
  }
  _M0L3bufS2327[_M0L6lengthS635] = _M0L5valueS636;
  _M0L6_2atmpS2328 = _M0L6lengthS635 + 1;
  _M0L4selfS634->$1 = _M0L6_2atmpS2328;
  moonbit_decref(_M0L4selfS634);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS637,
  void* _M0L5valueS639
) {
  int32_t _M0L3lenS2329;
  void** _M0L6_2atmpS2331;
  int32_t _M0L6_2atmpS4005;
  int32_t _M0L6_2atmpS2330;
  int32_t _M0L6lengthS638;
  void** _M0L8_2afieldS4004;
  void** _M0L3bufS2332;
  void* _M0L6_2aoldS4003;
  int32_t _M0L6_2atmpS2333;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2329 = _M0L4selfS637->$1;
  moonbit_incref(_M0L4selfS637);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2331
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS637);
  _M0L6_2atmpS4005 = Moonbit_array_length(_M0L6_2atmpS2331);
  moonbit_decref(_M0L6_2atmpS2331);
  _M0L6_2atmpS2330 = _M0L6_2atmpS4005;
  if (_M0L3lenS2329 == _M0L6_2atmpS2330) {
    moonbit_incref(_M0L4selfS637);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS637);
  }
  _M0L6lengthS638 = _M0L4selfS637->$1;
  _M0L8_2afieldS4004 = _M0L4selfS637->$0;
  _M0L3bufS2332 = _M0L8_2afieldS4004;
  _M0L6_2aoldS4003 = (void*)_M0L3bufS2332[_M0L6lengthS638];
  moonbit_decref(_M0L6_2aoldS4003);
  _M0L3bufS2332[_M0L6lengthS638] = _M0L5valueS639;
  _M0L6_2atmpS2333 = _M0L6lengthS638 + 1;
  _M0L4selfS637->$1 = _M0L6_2atmpS2333;
  moonbit_decref(_M0L4selfS637);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS640,
  void* _M0L5valueS642
) {
  int32_t _M0L3lenS2334;
  void** _M0L6_2atmpS2336;
  int32_t _M0L6_2atmpS4008;
  int32_t _M0L6_2atmpS2335;
  int32_t _M0L6lengthS641;
  void** _M0L8_2afieldS4007;
  void** _M0L3bufS2337;
  void* _M0L6_2aoldS4006;
  int32_t _M0L6_2atmpS2338;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2334 = _M0L4selfS640->$1;
  moonbit_incref(_M0L4selfS640);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2336 = _M0MPC15array5Array6bufferGRPB4JsonE(_M0L4selfS640);
  _M0L6_2atmpS4008 = Moonbit_array_length(_M0L6_2atmpS2336);
  moonbit_decref(_M0L6_2atmpS2336);
  _M0L6_2atmpS2335 = _M0L6_2atmpS4008;
  if (_M0L3lenS2334 == _M0L6_2atmpS2335) {
    moonbit_incref(_M0L4selfS640);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPB4JsonE(_M0L4selfS640);
  }
  _M0L6lengthS641 = _M0L4selfS640->$1;
  _M0L8_2afieldS4007 = _M0L4selfS640->$0;
  _M0L3bufS2337 = _M0L8_2afieldS4007;
  _M0L6_2aoldS4006 = (void*)_M0L3bufS2337[_M0L6lengthS641];
  moonbit_decref(_M0L6_2aoldS4006);
  _M0L3bufS2337[_M0L6lengthS641] = _M0L5valueS642;
  _M0L6_2atmpS2338 = _M0L6lengthS641 + 1;
  _M0L4selfS640->$1 = _M0L6_2atmpS2338;
  moonbit_decref(_M0L4selfS640);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS608) {
  int32_t _M0L8old__capS607;
  int32_t _M0L8new__capS609;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS607 = _M0L4selfS608->$1;
  if (_M0L8old__capS607 == 0) {
    _M0L8new__capS609 = 8;
  } else {
    _M0L8new__capS609 = _M0L8old__capS607 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS608, _M0L8new__capS609);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS611
) {
  int32_t _M0L8old__capS610;
  int32_t _M0L8new__capS612;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS610 = _M0L4selfS611->$1;
  if (_M0L8old__capS610 == 0) {
    _M0L8new__capS612 = 8;
  } else {
    _M0L8new__capS612 = _M0L8old__capS610 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS611, _M0L8new__capS612);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS614
) {
  int32_t _M0L8old__capS613;
  int32_t _M0L8new__capS615;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS613 = _M0L4selfS614->$1;
  if (_M0L8old__capS613 == 0) {
    _M0L8new__capS615 = 8;
  } else {
    _M0L8new__capS615 = _M0L8old__capS613 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(_M0L4selfS614, _M0L8new__capS615);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L4selfS617
) {
  int32_t _M0L8old__capS616;
  int32_t _M0L8new__capS618;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS616 = _M0L4selfS617->$1;
  if (_M0L8old__capS616 == 0) {
    _M0L8new__capS618 = 8;
  } else {
    _M0L8new__capS618 = _M0L8old__capS616 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(_M0L4selfS617, _M0L8new__capS618);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS620
) {
  int32_t _M0L8old__capS619;
  int32_t _M0L8new__capS621;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS619 = _M0L4selfS620->$1;
  if (_M0L8old__capS619 == 0) {
    _M0L8new__capS621 = 8;
  } else {
    _M0L8new__capS621 = _M0L8old__capS619 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS620, _M0L8new__capS621);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS623
) {
  int32_t _M0L8old__capS622;
  int32_t _M0L8new__capS624;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS622 = _M0L4selfS623->$1;
  if (_M0L8old__capS622 == 0) {
    _M0L8new__capS624 = 8;
  } else {
    _M0L8new__capS624 = _M0L8old__capS622 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPB4JsonE(_M0L4selfS623, _M0L8new__capS624);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS574,
  int32_t _M0L13new__capacityS572
) {
  moonbit_string_t* _M0L8new__bufS571;
  moonbit_string_t* _M0L8_2afieldS4010;
  moonbit_string_t* _M0L8old__bufS573;
  int32_t _M0L8old__capS575;
  int32_t _M0L9copy__lenS576;
  moonbit_string_t* _M0L6_2aoldS4009;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS571
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS572, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4010 = _M0L4selfS574->$0;
  _M0L8old__bufS573 = _M0L8_2afieldS4010;
  _M0L8old__capS575 = Moonbit_array_length(_M0L8old__bufS573);
  if (_M0L8old__capS575 < _M0L13new__capacityS572) {
    _M0L9copy__lenS576 = _M0L8old__capS575;
  } else {
    _M0L9copy__lenS576 = _M0L13new__capacityS572;
  }
  moonbit_incref(_M0L8old__bufS573);
  moonbit_incref(_M0L8new__bufS571);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS571, 0, _M0L8old__bufS573, 0, _M0L9copy__lenS576);
  _M0L6_2aoldS4009 = _M0L4selfS574->$0;
  moonbit_decref(_M0L6_2aoldS4009);
  _M0L4selfS574->$0 = _M0L8new__bufS571;
  moonbit_decref(_M0L4selfS574);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS580,
  int32_t _M0L13new__capacityS578
) {
  struct _M0TUsiE** _M0L8new__bufS577;
  struct _M0TUsiE** _M0L8_2afieldS4012;
  struct _M0TUsiE** _M0L8old__bufS579;
  int32_t _M0L8old__capS581;
  int32_t _M0L9copy__lenS582;
  struct _M0TUsiE** _M0L6_2aoldS4011;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS577
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS578, 0);
  _M0L8_2afieldS4012 = _M0L4selfS580->$0;
  _M0L8old__bufS579 = _M0L8_2afieldS4012;
  _M0L8old__capS581 = Moonbit_array_length(_M0L8old__bufS579);
  if (_M0L8old__capS581 < _M0L13new__capacityS578) {
    _M0L9copy__lenS582 = _M0L8old__capS581;
  } else {
    _M0L9copy__lenS582 = _M0L13new__capacityS578;
  }
  moonbit_incref(_M0L8old__bufS579);
  moonbit_incref(_M0L8new__bufS577);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS577, 0, _M0L8old__bufS579, 0, _M0L9copy__lenS582);
  _M0L6_2aoldS4011 = _M0L4selfS580->$0;
  moonbit_decref(_M0L6_2aoldS4011);
  _M0L4selfS580->$0 = _M0L8new__bufS577;
  moonbit_decref(_M0L4selfS580);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS586,
  int32_t _M0L13new__capacityS584
) {
  struct _M0TPC16string10StringView* _M0L8new__bufS583;
  struct _M0TPC16string10StringView* _M0L8_2afieldS4014;
  struct _M0TPC16string10StringView* _M0L8old__bufS585;
  int32_t _M0L8old__capS587;
  int32_t _M0L9copy__lenS588;
  struct _M0TPC16string10StringView* _M0L6_2aoldS4013;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS583
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array(_M0L13new__capacityS584, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0), &(struct _M0TPC16string10StringView){0, 0, (moonbit_string_t)moonbit_string_literal_0.data});
  _M0L8_2afieldS4014 = _M0L4selfS586->$0;
  _M0L8old__bufS585 = _M0L8_2afieldS4014;
  _M0L8old__capS587 = Moonbit_array_length(_M0L8old__bufS585);
  if (_M0L8old__capS587 < _M0L13new__capacityS584) {
    _M0L9copy__lenS588 = _M0L8old__capS587;
  } else {
    _M0L9copy__lenS588 = _M0L13new__capacityS584;
  }
  moonbit_incref(_M0L8old__bufS585);
  moonbit_incref(_M0L8new__bufS583);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(_M0L8new__bufS583, 0, _M0L8old__bufS585, 0, _M0L9copy__lenS588);
  _M0L6_2aoldS4013 = _M0L4selfS586->$0;
  moonbit_decref(_M0L6_2aoldS4013);
  _M0L4selfS586->$0 = _M0L8new__bufS583;
  moonbit_decref(_M0L4selfS586);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L4selfS592,
  int32_t _M0L13new__capacityS590
) {
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L8new__bufS589;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L8_2afieldS4016;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L8old__bufS591;
  int32_t _M0L8old__capS593;
  int32_t _M0L9copy__lenS594;
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L6_2aoldS4015;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS589
  = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE**)moonbit_make_ref_array(_M0L13new__capacityS590, 0);
  _M0L8_2afieldS4016 = _M0L4selfS592->$0;
  _M0L8old__bufS591 = _M0L8_2afieldS4016;
  _M0L8old__capS593 = Moonbit_array_length(_M0L8old__bufS591);
  if (_M0L8old__capS593 < _M0L13new__capacityS590) {
    _M0L9copy__lenS594 = _M0L8old__capS593;
  } else {
    _M0L9copy__lenS594 = _M0L13new__capacityS590;
  }
  moonbit_incref(_M0L8old__bufS591);
  moonbit_incref(_M0L8new__bufS589);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(_M0L8new__bufS589, 0, _M0L8old__bufS591, 0, _M0L9copy__lenS594);
  _M0L6_2aoldS4015 = _M0L4selfS592->$0;
  moonbit_decref(_M0L6_2aoldS4015);
  _M0L4selfS592->$0 = _M0L8new__bufS589;
  moonbit_decref(_M0L4selfS592);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS598,
  int32_t _M0L13new__capacityS596
) {
  void** _M0L8new__bufS595;
  void** _M0L8_2afieldS4018;
  void** _M0L8old__bufS597;
  int32_t _M0L8old__capS599;
  int32_t _M0L9copy__lenS600;
  void** _M0L6_2aoldS4017;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS595
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS596, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4018 = _M0L4selfS598->$0;
  _M0L8old__bufS597 = _M0L8_2afieldS4018;
  _M0L8old__capS599 = Moonbit_array_length(_M0L8old__bufS597);
  if (_M0L8old__capS599 < _M0L13new__capacityS596) {
    _M0L9copy__lenS600 = _M0L8old__capS599;
  } else {
    _M0L9copy__lenS600 = _M0L13new__capacityS596;
  }
  moonbit_incref(_M0L8old__bufS597);
  moonbit_incref(_M0L8new__bufS595);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS595, 0, _M0L8old__bufS597, 0, _M0L9copy__lenS600);
  _M0L6_2aoldS4017 = _M0L4selfS598->$0;
  moonbit_decref(_M0L6_2aoldS4017);
  _M0L4selfS598->$0 = _M0L8new__bufS595;
  moonbit_decref(_M0L4selfS598);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS604,
  int32_t _M0L13new__capacityS602
) {
  void** _M0L8new__bufS601;
  void** _M0L8_2afieldS4020;
  void** _M0L8old__bufS603;
  int32_t _M0L8old__capS605;
  int32_t _M0L9copy__lenS606;
  void** _M0L6_2aoldS4019;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS601
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS602, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4020 = _M0L4selfS604->$0;
  _M0L8old__bufS603 = _M0L8_2afieldS4020;
  _M0L8old__capS605 = Moonbit_array_length(_M0L8old__bufS603);
  if (_M0L8old__capS605 < _M0L13new__capacityS602) {
    _M0L9copy__lenS606 = _M0L8old__capS605;
  } else {
    _M0L9copy__lenS606 = _M0L13new__capacityS602;
  }
  moonbit_incref(_M0L8old__bufS603);
  moonbit_incref(_M0L8new__bufS601);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPB4JsonE(_M0L8new__bufS601, 0, _M0L8old__bufS603, 0, _M0L9copy__lenS606);
  _M0L6_2aoldS4019 = _M0L4selfS604->$0;
  moonbit_decref(_M0L6_2aoldS4019);
  _M0L4selfS604->$0 = _M0L8new__bufS601;
  moonbit_decref(_M0L4selfS604);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS570
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS570 == 0) {
    moonbit_string_t* _M0L6_2atmpS2307 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4573 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4573)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4573->$0 = _M0L6_2atmpS2307;
    _block_4573->$1 = 0;
    return _block_4573;
  } else {
    moonbit_string_t* _M0L6_2atmpS2308 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS570, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4574 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4574)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4574->$0 = _M0L6_2atmpS2308;
    _block_4574->$1 = 0;
    return _block_4574;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS564,
  int32_t _M0L1nS563
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS563 <= 0) {
    moonbit_decref(_M0L4selfS564);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS563 == 1) {
    return _M0L4selfS564;
  } else {
    int32_t _M0L3lenS565 = Moonbit_array_length(_M0L4selfS564);
    int32_t _M0L6_2atmpS2306 = _M0L3lenS565 * _M0L1nS563;
    struct _M0TPB13StringBuilder* _M0L3bufS566;
    moonbit_string_t _M0L3strS567;
    int32_t _M0L2__S568;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS566 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2306);
    _M0L3strS567 = _M0L4selfS564;
    _M0L2__S568 = 0;
    while (1) {
      if (_M0L2__S568 < _M0L1nS563) {
        int32_t _M0L6_2atmpS2305;
        moonbit_incref(_M0L3strS567);
        moonbit_incref(_M0L3bufS566);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS566, _M0L3strS567);
        _M0L6_2atmpS2305 = _M0L2__S568 + 1;
        _M0L2__S568 = _M0L6_2atmpS2305;
        continue;
      } else {
        moonbit_decref(_M0L3strS567);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS566);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS561,
  struct _M0TPC16string10StringView _M0L3strS562
) {
  int32_t _M0L3lenS2293;
  int32_t _M0L6_2atmpS2295;
  int32_t _M0L6_2atmpS2294;
  int32_t _M0L6_2atmpS2292;
  moonbit_bytes_t _M0L8_2afieldS4021;
  moonbit_bytes_t _M0L4dataS2296;
  int32_t _M0L3lenS2297;
  moonbit_string_t _M0L6_2atmpS2298;
  int32_t _M0L6_2atmpS2299;
  int32_t _M0L6_2atmpS2300;
  int32_t _M0L3lenS2302;
  int32_t _M0L6_2atmpS2304;
  int32_t _M0L6_2atmpS2303;
  int32_t _M0L6_2atmpS2301;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2293 = _M0L4selfS561->$1;
  moonbit_incref(_M0L3strS562.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2295 = _M0MPC16string10StringView6length(_M0L3strS562);
  _M0L6_2atmpS2294 = _M0L6_2atmpS2295 * 2;
  _M0L6_2atmpS2292 = _M0L3lenS2293 + _M0L6_2atmpS2294;
  moonbit_incref(_M0L4selfS561);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS561, _M0L6_2atmpS2292);
  _M0L8_2afieldS4021 = _M0L4selfS561->$0;
  _M0L4dataS2296 = _M0L8_2afieldS4021;
  _M0L3lenS2297 = _M0L4selfS561->$1;
  moonbit_incref(_M0L4dataS2296);
  moonbit_incref(_M0L3strS562.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2298 = _M0MPC16string10StringView4data(_M0L3strS562);
  moonbit_incref(_M0L3strS562.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2299 = _M0MPC16string10StringView13start__offset(_M0L3strS562);
  moonbit_incref(_M0L3strS562.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2300 = _M0MPC16string10StringView6length(_M0L3strS562);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2296, _M0L3lenS2297, _M0L6_2atmpS2298, _M0L6_2atmpS2299, _M0L6_2atmpS2300);
  _M0L3lenS2302 = _M0L4selfS561->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2304 = _M0MPC16string10StringView6length(_M0L3strS562);
  _M0L6_2atmpS2303 = _M0L6_2atmpS2304 * 2;
  _M0L6_2atmpS2301 = _M0L3lenS2302 + _M0L6_2atmpS2303;
  _M0L4selfS561->$1 = _M0L6_2atmpS2301;
  moonbit_decref(_M0L4selfS561);
  return 0;
}

void* _M0IPC16string10StringViewPB6ToJson8to__json(
  struct _M0TPC16string10StringView _M0L4selfS560
) {
  moonbit_string_t _M0L6_2atmpS2291;
  #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  #line 600 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6_2atmpS2291
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L4selfS560);
  #line 600 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0IPC16string6StringPB6ToJson8to__json(_M0L6_2atmpS2291);
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS557,
  int32_t _M0L1iS558,
  int32_t _M0L13start__offsetS559,
  int64_t _M0L11end__offsetS555
) {
  int32_t _M0L11end__offsetS554;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS555 == 4294967296ll) {
    _M0L11end__offsetS554 = Moonbit_array_length(_M0L4selfS557);
  } else {
    int64_t _M0L7_2aSomeS556 = _M0L11end__offsetS555;
    _M0L11end__offsetS554 = (int32_t)_M0L7_2aSomeS556;
  }
  if (_M0L1iS558 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS557, _M0L1iS558, _M0L13start__offsetS559, _M0L11end__offsetS554);
  } else {
    int32_t _M0L6_2atmpS2290 = -_M0L1iS558;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS557, _M0L6_2atmpS2290, _M0L13start__offsetS559, _M0L11end__offsetS554);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS552,
  int32_t _M0L1nS550,
  int32_t _M0L13start__offsetS546,
  int32_t _M0L11end__offsetS547
) {
  int32_t _if__result_4576;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS546 >= 0) {
    _if__result_4576 = _M0L13start__offsetS546 <= _M0L11end__offsetS547;
  } else {
    _if__result_4576 = 0;
  }
  if (_if__result_4576) {
    int32_t _M0Lm13utf16__offsetS548 = _M0L13start__offsetS546;
    int32_t _M0Lm11char__countS549 = 0;
    int32_t _M0L6_2atmpS2288;
    int32_t _if__result_4579;
    while (1) {
      int32_t _M0L6_2atmpS2282 = _M0Lm13utf16__offsetS548;
      int32_t _if__result_4578;
      if (_M0L6_2atmpS2282 < _M0L11end__offsetS547) {
        int32_t _M0L6_2atmpS2281 = _M0Lm11char__countS549;
        _if__result_4578 = _M0L6_2atmpS2281 < _M0L1nS550;
      } else {
        _if__result_4578 = 0;
      }
      if (_if__result_4578) {
        int32_t _M0L6_2atmpS2286 = _M0Lm13utf16__offsetS548;
        int32_t _M0L1cS551 = _M0L4selfS552[_M0L6_2atmpS2286];
        int32_t _M0L6_2atmpS2285;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS551)) {
          int32_t _M0L6_2atmpS2283 = _M0Lm13utf16__offsetS548;
          _M0Lm13utf16__offsetS548 = _M0L6_2atmpS2283 + 2;
        } else {
          int32_t _M0L6_2atmpS2284 = _M0Lm13utf16__offsetS548;
          _M0Lm13utf16__offsetS548 = _M0L6_2atmpS2284 + 1;
        }
        _M0L6_2atmpS2285 = _M0Lm11char__countS549;
        _M0Lm11char__countS549 = _M0L6_2atmpS2285 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS552);
      }
      break;
    }
    _M0L6_2atmpS2288 = _M0Lm11char__countS549;
    if (_M0L6_2atmpS2288 < _M0L1nS550) {
      _if__result_4579 = 1;
    } else {
      int32_t _M0L6_2atmpS2287 = _M0Lm13utf16__offsetS548;
      _if__result_4579 = _M0L6_2atmpS2287 >= _M0L11end__offsetS547;
    }
    if (_if__result_4579) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2289 = _M0Lm13utf16__offsetS548;
      return (int64_t)_M0L6_2atmpS2289;
    }
  } else {
    moonbit_decref(_M0L4selfS552);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_99.data, (moonbit_string_t)moonbit_string_literal_100.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS544,
  int32_t _M0L1nS542,
  int32_t _M0L13start__offsetS541,
  int32_t _M0L11end__offsetS540
) {
  int32_t _M0Lm11char__countS538;
  int32_t _M0Lm13utf16__offsetS539;
  int32_t _M0L6_2atmpS2279;
  int32_t _if__result_4582;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS538 = 0;
  _M0Lm13utf16__offsetS539 = _M0L11end__offsetS540;
  while (1) {
    int32_t _M0L6_2atmpS2272 = _M0Lm13utf16__offsetS539;
    int32_t _M0L6_2atmpS2271 = _M0L6_2atmpS2272 - 1;
    int32_t _if__result_4581;
    if (_M0L6_2atmpS2271 >= _M0L13start__offsetS541) {
      int32_t _M0L6_2atmpS2270 = _M0Lm11char__countS538;
      _if__result_4581 = _M0L6_2atmpS2270 < _M0L1nS542;
    } else {
      _if__result_4581 = 0;
    }
    if (_if__result_4581) {
      int32_t _M0L6_2atmpS2277 = _M0Lm13utf16__offsetS539;
      int32_t _M0L6_2atmpS2276 = _M0L6_2atmpS2277 - 1;
      int32_t _M0L1cS543 = _M0L4selfS544[_M0L6_2atmpS2276];
      int32_t _M0L6_2atmpS2275;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS543)) {
        int32_t _M0L6_2atmpS2273 = _M0Lm13utf16__offsetS539;
        _M0Lm13utf16__offsetS539 = _M0L6_2atmpS2273 - 2;
      } else {
        int32_t _M0L6_2atmpS2274 = _M0Lm13utf16__offsetS539;
        _M0Lm13utf16__offsetS539 = _M0L6_2atmpS2274 - 1;
      }
      _M0L6_2atmpS2275 = _M0Lm11char__countS538;
      _M0Lm11char__countS538 = _M0L6_2atmpS2275 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS544);
    }
    break;
  }
  _M0L6_2atmpS2279 = _M0Lm11char__countS538;
  if (_M0L6_2atmpS2279 < _M0L1nS542) {
    _if__result_4582 = 1;
  } else {
    int32_t _M0L6_2atmpS2278 = _M0Lm13utf16__offsetS539;
    _if__result_4582 = _M0L6_2atmpS2278 < _M0L13start__offsetS541;
  }
  if (_if__result_4582) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2280 = _M0Lm13utf16__offsetS539;
    return (int64_t)_M0L6_2atmpS2280;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS530,
  int32_t _M0L3lenS533,
  int32_t _M0L13start__offsetS537,
  int64_t _M0L11end__offsetS528
) {
  int32_t _M0L11end__offsetS527;
  int32_t _M0L5indexS531;
  int32_t _M0L5countS532;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS528 == 4294967296ll) {
    _M0L11end__offsetS527 = Moonbit_array_length(_M0L4selfS530);
  } else {
    int64_t _M0L7_2aSomeS529 = _M0L11end__offsetS528;
    _M0L11end__offsetS527 = (int32_t)_M0L7_2aSomeS529;
  }
  _M0L5indexS531 = _M0L13start__offsetS537;
  _M0L5countS532 = 0;
  while (1) {
    int32_t _if__result_4584;
    if (_M0L5indexS531 < _M0L11end__offsetS527) {
      _if__result_4584 = _M0L5countS532 < _M0L3lenS533;
    } else {
      _if__result_4584 = 0;
    }
    if (_if__result_4584) {
      int32_t _M0L2c1S534 = _M0L4selfS530[_M0L5indexS531];
      int32_t _if__result_4585;
      int32_t _M0L6_2atmpS2268;
      int32_t _M0L6_2atmpS2269;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S534)) {
        int32_t _M0L6_2atmpS2264 = _M0L5indexS531 + 1;
        _if__result_4585 = _M0L6_2atmpS2264 < _M0L11end__offsetS527;
      } else {
        _if__result_4585 = 0;
      }
      if (_if__result_4585) {
        int32_t _M0L6_2atmpS2267 = _M0L5indexS531 + 1;
        int32_t _M0L2c2S535 = _M0L4selfS530[_M0L6_2atmpS2267];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S535)) {
          int32_t _M0L6_2atmpS2265 = _M0L5indexS531 + 2;
          int32_t _M0L6_2atmpS2266 = _M0L5countS532 + 1;
          _M0L5indexS531 = _M0L6_2atmpS2265;
          _M0L5countS532 = _M0L6_2atmpS2266;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_101.data, (moonbit_string_t)moonbit_string_literal_102.data);
        }
      }
      _M0L6_2atmpS2268 = _M0L5indexS531 + 1;
      _M0L6_2atmpS2269 = _M0L5countS532 + 1;
      _M0L5indexS531 = _M0L6_2atmpS2268;
      _M0L5countS532 = _M0L6_2atmpS2269;
      continue;
    } else {
      moonbit_decref(_M0L4selfS530);
      return _M0L5countS532 >= _M0L3lenS533;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS523
) {
  int32_t _M0L3endS2256;
  int32_t _M0L8_2afieldS4022;
  int32_t _M0L5startS2257;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2256 = _M0L4selfS523.$2;
  _M0L8_2afieldS4022 = _M0L4selfS523.$1;
  moonbit_decref(_M0L4selfS523.$0);
  _M0L5startS2257 = _M0L8_2afieldS4022;
  return _M0L3endS2256 - _M0L5startS2257;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS524
) {
  int32_t _M0L3endS2258;
  int32_t _M0L8_2afieldS4023;
  int32_t _M0L5startS2259;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2258 = _M0L4selfS524.$2;
  _M0L8_2afieldS4023 = _M0L4selfS524.$1;
  moonbit_decref(_M0L4selfS524.$0);
  _M0L5startS2259 = _M0L8_2afieldS4023;
  return _M0L3endS2258 - _M0L5startS2259;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS525
) {
  int32_t _M0L3endS2260;
  int32_t _M0L8_2afieldS4024;
  int32_t _M0L5startS2261;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2260 = _M0L4selfS525.$2;
  _M0L8_2afieldS4024 = _M0L4selfS525.$1;
  moonbit_decref(_M0L4selfS525.$0);
  _M0L5startS2261 = _M0L8_2afieldS4024;
  return _M0L3endS2260 - _M0L5startS2261;
}

int32_t _M0MPC15array9ArrayView6lengthGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TPB9ArrayViewGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE _M0L4selfS526
) {
  int32_t _M0L3endS2262;
  int32_t _M0L8_2afieldS4025;
  int32_t _M0L5startS2263;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2262 = _M0L4selfS526.$2;
  _M0L8_2afieldS4025 = _M0L4selfS526.$1;
  moonbit_decref(_M0L4selfS526.$0);
  _M0L5startS2263 = _M0L8_2afieldS4025;
  return _M0L3endS2262 - _M0L5startS2263;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS521,
  int64_t _M0L19start__offset_2eoptS519,
  int64_t _M0L11end__offsetS522
) {
  int32_t _M0L13start__offsetS518;
  if (_M0L19start__offset_2eoptS519 == 4294967296ll) {
    _M0L13start__offsetS518 = 0;
  } else {
    int64_t _M0L7_2aSomeS520 = _M0L19start__offset_2eoptS519;
    _M0L13start__offsetS518 = (int32_t)_M0L7_2aSomeS520;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS521, _M0L13start__offsetS518, _M0L11end__offsetS522);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS516,
  int32_t _M0L13start__offsetS517,
  int64_t _M0L11end__offsetS514
) {
  int32_t _M0L11end__offsetS513;
  int32_t _if__result_4586;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS514 == 4294967296ll) {
    _M0L11end__offsetS513 = Moonbit_array_length(_M0L4selfS516);
  } else {
    int64_t _M0L7_2aSomeS515 = _M0L11end__offsetS514;
    _M0L11end__offsetS513 = (int32_t)_M0L7_2aSomeS515;
  }
  if (_M0L13start__offsetS517 >= 0) {
    if (_M0L13start__offsetS517 <= _M0L11end__offsetS513) {
      int32_t _M0L6_2atmpS2255 = Moonbit_array_length(_M0L4selfS516);
      _if__result_4586 = _M0L11end__offsetS513 <= _M0L6_2atmpS2255;
    } else {
      _if__result_4586 = 0;
    }
  } else {
    _if__result_4586 = 0;
  }
  if (_if__result_4586) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS517,
                                                 _M0L11end__offsetS513,
                                                 _M0L4selfS516};
  } else {
    moonbit_decref(_M0L4selfS516);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_103.data, (moonbit_string_t)moonbit_string_literal_104.data);
  }
}

int32_t _M0IPC16string10StringViewPB2Eq5equal(
  struct _M0TPC16string10StringView _M0L4selfS509,
  struct _M0TPC16string10StringView _M0L5otherS510
) {
  int32_t _M0L3lenS508;
  int32_t _M0L6_2atmpS2241;
  #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  moonbit_incref(_M0L4selfS509.$0);
  #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS508 = _M0MPC16string10StringView6length(_M0L4selfS509);
  moonbit_incref(_M0L5otherS510.$0);
  #line 271 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6_2atmpS2241 = _M0MPC16string10StringView6length(_M0L5otherS510);
  if (_M0L3lenS508 == _M0L6_2atmpS2241) {
    moonbit_string_t _M0L8_2afieldS4032 = _M0L4selfS509.$0;
    moonbit_string_t _M0L3strS2244 = _M0L8_2afieldS4032;
    moonbit_string_t _M0L8_2afieldS4031 = _M0L5otherS510.$0;
    moonbit_string_t _M0L3strS2245 = _M0L8_2afieldS4031;
    int32_t _M0L6_2atmpS4030 = _M0L3strS2244 == _M0L3strS2245;
    int32_t _if__result_4587;
    int32_t _M0L1iS511;
    if (_M0L6_2atmpS4030) {
      int32_t _M0L5startS2242 = _M0L4selfS509.$1;
      int32_t _M0L5startS2243 = _M0L5otherS510.$1;
      _if__result_4587 = _M0L5startS2242 == _M0L5startS2243;
    } else {
      _if__result_4587 = 0;
    }
    if (_if__result_4587) {
      moonbit_decref(_M0L5otherS510.$0);
      moonbit_decref(_M0L4selfS509.$0);
      return 1;
    }
    _M0L1iS511 = 0;
    while (1) {
      if (_M0L1iS511 < _M0L3lenS508) {
        moonbit_string_t _M0L8_2afieldS4029 = _M0L4selfS509.$0;
        moonbit_string_t _M0L3strS2251 = _M0L8_2afieldS4029;
        int32_t _M0L5startS2253 = _M0L4selfS509.$1;
        int32_t _M0L6_2atmpS2252 = _M0L5startS2253 + _M0L1iS511;
        int32_t _M0L6_2atmpS4028 = _M0L3strS2251[_M0L6_2atmpS2252];
        int32_t _M0L6_2atmpS2246 = _M0L6_2atmpS4028;
        moonbit_string_t _M0L8_2afieldS4027 = _M0L5otherS510.$0;
        moonbit_string_t _M0L3strS2248 = _M0L8_2afieldS4027;
        int32_t _M0L5startS2250 = _M0L5otherS510.$1;
        int32_t _M0L6_2atmpS2249 = _M0L5startS2250 + _M0L1iS511;
        int32_t _M0L6_2atmpS4026 = _M0L3strS2248[_M0L6_2atmpS2249];
        int32_t _M0L6_2atmpS2247 = _M0L6_2atmpS4026;
        int32_t _M0L6_2atmpS2254;
        if (_M0L6_2atmpS2246 == _M0L6_2atmpS2247) {
          
        } else {
          moonbit_decref(_M0L5otherS510.$0);
          moonbit_decref(_M0L4selfS509.$0);
          return 0;
        }
        _M0L6_2atmpS2254 = _M0L1iS511 + 1;
        _M0L1iS511 = _M0L6_2atmpS2254;
        continue;
      } else {
        moonbit_decref(_M0L5otherS510.$0);
        moonbit_decref(_M0L4selfS509.$0);
      }
      break;
    }
    return 1;
  } else {
    moonbit_decref(_M0L5otherS510.$0);
    moonbit_decref(_M0L4selfS509.$0);
    return 0;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS507
) {
  moonbit_string_t _M0L8_2afieldS4034;
  moonbit_string_t _M0L3strS2238;
  int32_t _M0L5startS2239;
  int32_t _M0L8_2afieldS4033;
  int32_t _M0L3endS2240;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4034 = _M0L4selfS507.$0;
  _M0L3strS2238 = _M0L8_2afieldS4034;
  _M0L5startS2239 = _M0L4selfS507.$1;
  _M0L8_2afieldS4033 = _M0L4selfS507.$2;
  _M0L3endS2240 = _M0L8_2afieldS4033;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2238, _M0L5startS2239, _M0L3endS2240);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS505,
  struct _M0TPB6Logger _M0L6loggerS506
) {
  moonbit_string_t _M0L8_2afieldS4036;
  moonbit_string_t _M0L3strS2235;
  int32_t _M0L5startS2236;
  int32_t _M0L8_2afieldS4035;
  int32_t _M0L3endS2237;
  moonbit_string_t _M0L6substrS504;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4036 = _M0L4selfS505.$0;
  _M0L3strS2235 = _M0L8_2afieldS4036;
  _M0L5startS2236 = _M0L4selfS505.$1;
  _M0L8_2afieldS4035 = _M0L4selfS505.$2;
  _M0L3endS2237 = _M0L8_2afieldS4035;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS504
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2235, _M0L5startS2236, _M0L3endS2237);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS504, _M0L6loggerS506);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS496,
  struct _M0TPB6Logger _M0L6loggerS494
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS495;
  int32_t _M0L3lenS497;
  int32_t _M0L1iS498;
  int32_t _M0L3segS499;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS494.$1) {
    moonbit_incref(_M0L6loggerS494.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS494.$0->$method_3(_M0L6loggerS494.$1, 34);
  moonbit_incref(_M0L4selfS496);
  if (_M0L6loggerS494.$1) {
    moonbit_incref(_M0L6loggerS494.$1);
  }
  _M0L6_2aenvS495
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS495)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS495->$0 = _M0L4selfS496;
  _M0L6_2aenvS495->$1_0 = _M0L6loggerS494.$0;
  _M0L6_2aenvS495->$1_1 = _M0L6loggerS494.$1;
  _M0L3lenS497 = Moonbit_array_length(_M0L4selfS496);
  _M0L1iS498 = 0;
  _M0L3segS499 = 0;
  _2afor_500:;
  while (1) {
    int32_t _M0L4codeS501;
    int32_t _M0L1cS503;
    int32_t _M0L6_2atmpS2219;
    int32_t _M0L6_2atmpS2220;
    int32_t _M0L6_2atmpS2221;
    int32_t _tmp_4592;
    int32_t _tmp_4593;
    if (_M0L1iS498 >= _M0L3lenS497) {
      moonbit_decref(_M0L4selfS496);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS495, _M0L3segS499, _M0L1iS498);
      break;
    }
    _M0L4codeS501 = _M0L4selfS496[_M0L1iS498];
    switch (_M0L4codeS501) {
      case 34: {
        _M0L1cS503 = _M0L4codeS501;
        goto join_502;
        break;
      }
      
      case 92: {
        _M0L1cS503 = _M0L4codeS501;
        goto join_502;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2222;
        int32_t _M0L6_2atmpS2223;
        moonbit_incref(_M0L6_2aenvS495);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS495, _M0L3segS499, _M0L1iS498);
        if (_M0L6loggerS494.$1) {
          moonbit_incref(_M0L6loggerS494.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS494.$0->$method_0(_M0L6loggerS494.$1, (moonbit_string_t)moonbit_string_literal_87.data);
        _M0L6_2atmpS2222 = _M0L1iS498 + 1;
        _M0L6_2atmpS2223 = _M0L1iS498 + 1;
        _M0L1iS498 = _M0L6_2atmpS2222;
        _M0L3segS499 = _M0L6_2atmpS2223;
        goto _2afor_500;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2224;
        int32_t _M0L6_2atmpS2225;
        moonbit_incref(_M0L6_2aenvS495);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS495, _M0L3segS499, _M0L1iS498);
        if (_M0L6loggerS494.$1) {
          moonbit_incref(_M0L6loggerS494.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS494.$0->$method_0(_M0L6loggerS494.$1, (moonbit_string_t)moonbit_string_literal_88.data);
        _M0L6_2atmpS2224 = _M0L1iS498 + 1;
        _M0L6_2atmpS2225 = _M0L1iS498 + 1;
        _M0L1iS498 = _M0L6_2atmpS2224;
        _M0L3segS499 = _M0L6_2atmpS2225;
        goto _2afor_500;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2226;
        int32_t _M0L6_2atmpS2227;
        moonbit_incref(_M0L6_2aenvS495);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS495, _M0L3segS499, _M0L1iS498);
        if (_M0L6loggerS494.$1) {
          moonbit_incref(_M0L6loggerS494.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS494.$0->$method_0(_M0L6loggerS494.$1, (moonbit_string_t)moonbit_string_literal_89.data);
        _M0L6_2atmpS2226 = _M0L1iS498 + 1;
        _M0L6_2atmpS2227 = _M0L1iS498 + 1;
        _M0L1iS498 = _M0L6_2atmpS2226;
        _M0L3segS499 = _M0L6_2atmpS2227;
        goto _2afor_500;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2228;
        int32_t _M0L6_2atmpS2229;
        moonbit_incref(_M0L6_2aenvS495);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS495, _M0L3segS499, _M0L1iS498);
        if (_M0L6loggerS494.$1) {
          moonbit_incref(_M0L6loggerS494.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS494.$0->$method_0(_M0L6loggerS494.$1, (moonbit_string_t)moonbit_string_literal_90.data);
        _M0L6_2atmpS2228 = _M0L1iS498 + 1;
        _M0L6_2atmpS2229 = _M0L1iS498 + 1;
        _M0L1iS498 = _M0L6_2atmpS2228;
        _M0L3segS499 = _M0L6_2atmpS2229;
        goto _2afor_500;
        break;
      }
      default: {
        if (_M0L4codeS501 < 32) {
          int32_t _M0L6_2atmpS2231;
          moonbit_string_t _M0L6_2atmpS2230;
          int32_t _M0L6_2atmpS2232;
          int32_t _M0L6_2atmpS2233;
          moonbit_incref(_M0L6_2aenvS495);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS495, _M0L3segS499, _M0L1iS498);
          if (_M0L6loggerS494.$1) {
            moonbit_incref(_M0L6loggerS494.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS494.$0->$method_0(_M0L6loggerS494.$1, (moonbit_string_t)moonbit_string_literal_105.data);
          _M0L6_2atmpS2231 = _M0L4codeS501 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2230 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2231);
          if (_M0L6loggerS494.$1) {
            moonbit_incref(_M0L6loggerS494.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS494.$0->$method_0(_M0L6loggerS494.$1, _M0L6_2atmpS2230);
          if (_M0L6loggerS494.$1) {
            moonbit_incref(_M0L6loggerS494.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS494.$0->$method_3(_M0L6loggerS494.$1, 125);
          _M0L6_2atmpS2232 = _M0L1iS498 + 1;
          _M0L6_2atmpS2233 = _M0L1iS498 + 1;
          _M0L1iS498 = _M0L6_2atmpS2232;
          _M0L3segS499 = _M0L6_2atmpS2233;
          goto _2afor_500;
        } else {
          int32_t _M0L6_2atmpS2234 = _M0L1iS498 + 1;
          int32_t _tmp_4591 = _M0L3segS499;
          _M0L1iS498 = _M0L6_2atmpS2234;
          _M0L3segS499 = _tmp_4591;
          goto _2afor_500;
        }
        break;
      }
    }
    goto joinlet_4590;
    join_502:;
    moonbit_incref(_M0L6_2aenvS495);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS495, _M0L3segS499, _M0L1iS498);
    if (_M0L6loggerS494.$1) {
      moonbit_incref(_M0L6loggerS494.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS494.$0->$method_3(_M0L6loggerS494.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2219 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS503);
    if (_M0L6loggerS494.$1) {
      moonbit_incref(_M0L6loggerS494.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS494.$0->$method_3(_M0L6loggerS494.$1, _M0L6_2atmpS2219);
    _M0L6_2atmpS2220 = _M0L1iS498 + 1;
    _M0L6_2atmpS2221 = _M0L1iS498 + 1;
    _M0L1iS498 = _M0L6_2atmpS2220;
    _M0L3segS499 = _M0L6_2atmpS2221;
    continue;
    joinlet_4590:;
    _tmp_4592 = _M0L1iS498;
    _tmp_4593 = _M0L3segS499;
    _M0L1iS498 = _tmp_4592;
    _M0L3segS499 = _tmp_4593;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS494.$0->$method_3(_M0L6loggerS494.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS490,
  int32_t _M0L3segS493,
  int32_t _M0L1iS492
) {
  struct _M0TPB6Logger _M0L8_2afieldS4038;
  struct _M0TPB6Logger _M0L6loggerS489;
  moonbit_string_t _M0L8_2afieldS4037;
  int32_t _M0L6_2acntS4399;
  moonbit_string_t _M0L4selfS491;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4038
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS490->$1_0, _M0L6_2aenvS490->$1_1
  };
  _M0L6loggerS489 = _M0L8_2afieldS4038;
  _M0L8_2afieldS4037 = _M0L6_2aenvS490->$0;
  _M0L6_2acntS4399 = Moonbit_object_header(_M0L6_2aenvS490)->rc;
  if (_M0L6_2acntS4399 > 1) {
    int32_t _M0L11_2anew__cntS4400 = _M0L6_2acntS4399 - 1;
    Moonbit_object_header(_M0L6_2aenvS490)->rc = _M0L11_2anew__cntS4400;
    if (_M0L6loggerS489.$1) {
      moonbit_incref(_M0L6loggerS489.$1);
    }
    moonbit_incref(_M0L8_2afieldS4037);
  } else if (_M0L6_2acntS4399 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS490);
  }
  _M0L4selfS491 = _M0L8_2afieldS4037;
  if (_M0L1iS492 > _M0L3segS493) {
    int32_t _M0L6_2atmpS2218 = _M0L1iS492 - _M0L3segS493;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS489.$0->$method_1(_M0L6loggerS489.$1, _M0L4selfS491, _M0L3segS493, _M0L6_2atmpS2218);
  } else {
    moonbit_decref(_M0L4selfS491);
    if (_M0L6loggerS489.$1) {
      moonbit_decref(_M0L6loggerS489.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS488) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS487;
  int32_t _M0L6_2atmpS2215;
  int32_t _M0L6_2atmpS2214;
  int32_t _M0L6_2atmpS2217;
  int32_t _M0L6_2atmpS2216;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2213;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS487 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2215 = _M0IPC14byte4BytePB3Div3div(_M0L1bS488, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2214
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2215);
  moonbit_incref(_M0L7_2aselfS487);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS487, _M0L6_2atmpS2214);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2217 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS488, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2216
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2217);
  moonbit_incref(_M0L7_2aselfS487);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS487, _M0L6_2atmpS2216);
  _M0L6_2atmpS2213 = _M0L7_2aselfS487;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2213);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS486) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS486 < 10) {
    int32_t _M0L6_2atmpS2210;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2210 = _M0IPC14byte4BytePB3Add3add(_M0L1iS486, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2210);
  } else {
    int32_t _M0L6_2atmpS2212;
    int32_t _M0L6_2atmpS2211;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2212 = _M0IPC14byte4BytePB3Add3add(_M0L1iS486, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2211 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2212, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2211);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS484,
  int32_t _M0L4thatS485
) {
  int32_t _M0L6_2atmpS2208;
  int32_t _M0L6_2atmpS2209;
  int32_t _M0L6_2atmpS2207;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2208 = (int32_t)_M0L4selfS484;
  _M0L6_2atmpS2209 = (int32_t)_M0L4thatS485;
  _M0L6_2atmpS2207 = _M0L6_2atmpS2208 - _M0L6_2atmpS2209;
  return _M0L6_2atmpS2207 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS482,
  int32_t _M0L4thatS483
) {
  int32_t _M0L6_2atmpS2205;
  int32_t _M0L6_2atmpS2206;
  int32_t _M0L6_2atmpS2204;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2205 = (int32_t)_M0L4selfS482;
  _M0L6_2atmpS2206 = (int32_t)_M0L4thatS483;
  _M0L6_2atmpS2204 = _M0L6_2atmpS2205 % _M0L6_2atmpS2206;
  return _M0L6_2atmpS2204 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS480,
  int32_t _M0L4thatS481
) {
  int32_t _M0L6_2atmpS2202;
  int32_t _M0L6_2atmpS2203;
  int32_t _M0L6_2atmpS2201;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2202 = (int32_t)_M0L4selfS480;
  _M0L6_2atmpS2203 = (int32_t)_M0L4thatS481;
  _M0L6_2atmpS2201 = _M0L6_2atmpS2202 / _M0L6_2atmpS2203;
  return _M0L6_2atmpS2201 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS478,
  int32_t _M0L4thatS479
) {
  int32_t _M0L6_2atmpS2199;
  int32_t _M0L6_2atmpS2200;
  int32_t _M0L6_2atmpS2198;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2199 = (int32_t)_M0L4selfS478;
  _M0L6_2atmpS2200 = (int32_t)_M0L4thatS479;
  _M0L6_2atmpS2198 = _M0L6_2atmpS2199 + _M0L6_2atmpS2200;
  return _M0L6_2atmpS2198 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS475,
  int32_t _M0L5startS473,
  int32_t _M0L3endS474
) {
  int32_t _if__result_4594;
  int32_t _M0L3lenS476;
  int32_t _M0L6_2atmpS2196;
  int32_t _M0L6_2atmpS2197;
  moonbit_bytes_t _M0L5bytesS477;
  moonbit_bytes_t _M0L6_2atmpS2195;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS473 == 0) {
    int32_t _M0L6_2atmpS2194 = Moonbit_array_length(_M0L3strS475);
    _if__result_4594 = _M0L3endS474 == _M0L6_2atmpS2194;
  } else {
    _if__result_4594 = 0;
  }
  if (_if__result_4594) {
    return _M0L3strS475;
  }
  _M0L3lenS476 = _M0L3endS474 - _M0L5startS473;
  _M0L6_2atmpS2196 = _M0L3lenS476 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2197 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS477
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2196, _M0L6_2atmpS2197);
  moonbit_incref(_M0L5bytesS477);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS477, 0, _M0L3strS475, _M0L5startS473, _M0L3lenS476);
  _M0L6_2atmpS2195 = _M0L5bytesS477;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2195, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS467) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS467;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L1fS468
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS468;
}

struct _M0TWEOi* _M0MPB4Iter3newGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TWEOi* _M0L1fS469
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS469;
}

struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB4Iter3newGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L1fS470
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS470;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS471
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS471;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS472) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS472;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS459,
  int32_t _M0L5radixS458
) {
  int32_t _if__result_4595;
  uint16_t* _M0L6bufferS460;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS458 < 2) {
    _if__result_4595 = 1;
  } else {
    _if__result_4595 = _M0L5radixS458 > 36;
  }
  if (_if__result_4595) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_106.data, (moonbit_string_t)moonbit_string_literal_107.data);
  }
  if (_M0L4selfS459 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_17.data;
  }
  switch (_M0L5radixS458) {
    case 10: {
      int32_t _M0L3lenS461;
      uint16_t* _M0L6bufferS462;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS461 = _M0FPB12dec__count64(_M0L4selfS459);
      _M0L6bufferS462 = (uint16_t*)moonbit_make_string(_M0L3lenS461, 0);
      moonbit_incref(_M0L6bufferS462);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS462, _M0L4selfS459, 0, _M0L3lenS461);
      _M0L6bufferS460 = _M0L6bufferS462;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS463;
      uint16_t* _M0L6bufferS464;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS463 = _M0FPB12hex__count64(_M0L4selfS459);
      _M0L6bufferS464 = (uint16_t*)moonbit_make_string(_M0L3lenS463, 0);
      moonbit_incref(_M0L6bufferS464);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS464, _M0L4selfS459, 0, _M0L3lenS463);
      _M0L6bufferS460 = _M0L6bufferS464;
      break;
    }
    default: {
      int32_t _M0L3lenS465;
      uint16_t* _M0L6bufferS466;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS465 = _M0FPB14radix__count64(_M0L4selfS459, _M0L5radixS458);
      _M0L6bufferS466 = (uint16_t*)moonbit_make_string(_M0L3lenS465, 0);
      moonbit_incref(_M0L6bufferS466);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS466, _M0L4selfS459, 0, _M0L3lenS465, _M0L5radixS458);
      _M0L6bufferS460 = _M0L6bufferS466;
      break;
    }
  }
  return _M0L6bufferS460;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS448,
  uint64_t _M0L3numS436,
  int32_t _M0L12digit__startS439,
  int32_t _M0L10total__lenS438
) {
  uint64_t _M0Lm3numS435;
  int32_t _M0Lm6offsetS437;
  uint64_t _M0L6_2atmpS2193;
  int32_t _M0Lm9remainingS450;
  int32_t _M0L6_2atmpS2174;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS435 = _M0L3numS436;
  _M0Lm6offsetS437 = _M0L10total__lenS438 - _M0L12digit__startS439;
  while (1) {
    uint64_t _M0L6_2atmpS2137 = _M0Lm3numS435;
    if (_M0L6_2atmpS2137 >= 10000ull) {
      uint64_t _M0L6_2atmpS2160 = _M0Lm3numS435;
      uint64_t _M0L1tS440 = _M0L6_2atmpS2160 / 10000ull;
      uint64_t _M0L6_2atmpS2159 = _M0Lm3numS435;
      uint64_t _M0L6_2atmpS2158 = _M0L6_2atmpS2159 % 10000ull;
      int32_t _M0L1rS441 = (int32_t)_M0L6_2atmpS2158;
      int32_t _M0L2d1S442;
      int32_t _M0L2d2S443;
      int32_t _M0L6_2atmpS2138;
      int32_t _M0L6_2atmpS2157;
      int32_t _M0L6_2atmpS2156;
      int32_t _M0L6d1__hiS444;
      int32_t _M0L6_2atmpS2155;
      int32_t _M0L6_2atmpS2154;
      int32_t _M0L6d1__loS445;
      int32_t _M0L6_2atmpS2153;
      int32_t _M0L6_2atmpS2152;
      int32_t _M0L6d2__hiS446;
      int32_t _M0L6_2atmpS2151;
      int32_t _M0L6_2atmpS2150;
      int32_t _M0L6d2__loS447;
      int32_t _M0L6_2atmpS2140;
      int32_t _M0L6_2atmpS2139;
      int32_t _M0L6_2atmpS2143;
      int32_t _M0L6_2atmpS2142;
      int32_t _M0L6_2atmpS2141;
      int32_t _M0L6_2atmpS2146;
      int32_t _M0L6_2atmpS2145;
      int32_t _M0L6_2atmpS2144;
      int32_t _M0L6_2atmpS2149;
      int32_t _M0L6_2atmpS2148;
      int32_t _M0L6_2atmpS2147;
      _M0Lm3numS435 = _M0L1tS440;
      _M0L2d1S442 = _M0L1rS441 / 100;
      _M0L2d2S443 = _M0L1rS441 % 100;
      _M0L6_2atmpS2138 = _M0Lm6offsetS437;
      _M0Lm6offsetS437 = _M0L6_2atmpS2138 - 4;
      _M0L6_2atmpS2157 = _M0L2d1S442 / 10;
      _M0L6_2atmpS2156 = 48 + _M0L6_2atmpS2157;
      _M0L6d1__hiS444 = (uint16_t)_M0L6_2atmpS2156;
      _M0L6_2atmpS2155 = _M0L2d1S442 % 10;
      _M0L6_2atmpS2154 = 48 + _M0L6_2atmpS2155;
      _M0L6d1__loS445 = (uint16_t)_M0L6_2atmpS2154;
      _M0L6_2atmpS2153 = _M0L2d2S443 / 10;
      _M0L6_2atmpS2152 = 48 + _M0L6_2atmpS2153;
      _M0L6d2__hiS446 = (uint16_t)_M0L6_2atmpS2152;
      _M0L6_2atmpS2151 = _M0L2d2S443 % 10;
      _M0L6_2atmpS2150 = 48 + _M0L6_2atmpS2151;
      _M0L6d2__loS447 = (uint16_t)_M0L6_2atmpS2150;
      _M0L6_2atmpS2140 = _M0Lm6offsetS437;
      _M0L6_2atmpS2139 = _M0L12digit__startS439 + _M0L6_2atmpS2140;
      _M0L6bufferS448[_M0L6_2atmpS2139] = _M0L6d1__hiS444;
      _M0L6_2atmpS2143 = _M0Lm6offsetS437;
      _M0L6_2atmpS2142 = _M0L12digit__startS439 + _M0L6_2atmpS2143;
      _M0L6_2atmpS2141 = _M0L6_2atmpS2142 + 1;
      _M0L6bufferS448[_M0L6_2atmpS2141] = _M0L6d1__loS445;
      _M0L6_2atmpS2146 = _M0Lm6offsetS437;
      _M0L6_2atmpS2145 = _M0L12digit__startS439 + _M0L6_2atmpS2146;
      _M0L6_2atmpS2144 = _M0L6_2atmpS2145 + 2;
      _M0L6bufferS448[_M0L6_2atmpS2144] = _M0L6d2__hiS446;
      _M0L6_2atmpS2149 = _M0Lm6offsetS437;
      _M0L6_2atmpS2148 = _M0L12digit__startS439 + _M0L6_2atmpS2149;
      _M0L6_2atmpS2147 = _M0L6_2atmpS2148 + 3;
      _M0L6bufferS448[_M0L6_2atmpS2147] = _M0L6d2__loS447;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2193 = _M0Lm3numS435;
  _M0Lm9remainingS450 = (int32_t)_M0L6_2atmpS2193;
  while (1) {
    int32_t _M0L6_2atmpS2161 = _M0Lm9remainingS450;
    if (_M0L6_2atmpS2161 >= 100) {
      int32_t _M0L6_2atmpS2173 = _M0Lm9remainingS450;
      int32_t _M0L1tS451 = _M0L6_2atmpS2173 / 100;
      int32_t _M0L6_2atmpS2172 = _M0Lm9remainingS450;
      int32_t _M0L1dS452 = _M0L6_2atmpS2172 % 100;
      int32_t _M0L6_2atmpS2162;
      int32_t _M0L6_2atmpS2171;
      int32_t _M0L6_2atmpS2170;
      int32_t _M0L5d__hiS453;
      int32_t _M0L6_2atmpS2169;
      int32_t _M0L6_2atmpS2168;
      int32_t _M0L5d__loS454;
      int32_t _M0L6_2atmpS2164;
      int32_t _M0L6_2atmpS2163;
      int32_t _M0L6_2atmpS2167;
      int32_t _M0L6_2atmpS2166;
      int32_t _M0L6_2atmpS2165;
      _M0Lm9remainingS450 = _M0L1tS451;
      _M0L6_2atmpS2162 = _M0Lm6offsetS437;
      _M0Lm6offsetS437 = _M0L6_2atmpS2162 - 2;
      _M0L6_2atmpS2171 = _M0L1dS452 / 10;
      _M0L6_2atmpS2170 = 48 + _M0L6_2atmpS2171;
      _M0L5d__hiS453 = (uint16_t)_M0L6_2atmpS2170;
      _M0L6_2atmpS2169 = _M0L1dS452 % 10;
      _M0L6_2atmpS2168 = 48 + _M0L6_2atmpS2169;
      _M0L5d__loS454 = (uint16_t)_M0L6_2atmpS2168;
      _M0L6_2atmpS2164 = _M0Lm6offsetS437;
      _M0L6_2atmpS2163 = _M0L12digit__startS439 + _M0L6_2atmpS2164;
      _M0L6bufferS448[_M0L6_2atmpS2163] = _M0L5d__hiS453;
      _M0L6_2atmpS2167 = _M0Lm6offsetS437;
      _M0L6_2atmpS2166 = _M0L12digit__startS439 + _M0L6_2atmpS2167;
      _M0L6_2atmpS2165 = _M0L6_2atmpS2166 + 1;
      _M0L6bufferS448[_M0L6_2atmpS2165] = _M0L5d__loS454;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2174 = _M0Lm9remainingS450;
  if (_M0L6_2atmpS2174 >= 10) {
    int32_t _M0L6_2atmpS2175 = _M0Lm6offsetS437;
    int32_t _M0L6_2atmpS2186;
    int32_t _M0L6_2atmpS2185;
    int32_t _M0L6_2atmpS2184;
    int32_t _M0L5d__hiS456;
    int32_t _M0L6_2atmpS2183;
    int32_t _M0L6_2atmpS2182;
    int32_t _M0L6_2atmpS2181;
    int32_t _M0L5d__loS457;
    int32_t _M0L6_2atmpS2177;
    int32_t _M0L6_2atmpS2176;
    int32_t _M0L6_2atmpS2180;
    int32_t _M0L6_2atmpS2179;
    int32_t _M0L6_2atmpS2178;
    _M0Lm6offsetS437 = _M0L6_2atmpS2175 - 2;
    _M0L6_2atmpS2186 = _M0Lm9remainingS450;
    _M0L6_2atmpS2185 = _M0L6_2atmpS2186 / 10;
    _M0L6_2atmpS2184 = 48 + _M0L6_2atmpS2185;
    _M0L5d__hiS456 = (uint16_t)_M0L6_2atmpS2184;
    _M0L6_2atmpS2183 = _M0Lm9remainingS450;
    _M0L6_2atmpS2182 = _M0L6_2atmpS2183 % 10;
    _M0L6_2atmpS2181 = 48 + _M0L6_2atmpS2182;
    _M0L5d__loS457 = (uint16_t)_M0L6_2atmpS2181;
    _M0L6_2atmpS2177 = _M0Lm6offsetS437;
    _M0L6_2atmpS2176 = _M0L12digit__startS439 + _M0L6_2atmpS2177;
    _M0L6bufferS448[_M0L6_2atmpS2176] = _M0L5d__hiS456;
    _M0L6_2atmpS2180 = _M0Lm6offsetS437;
    _M0L6_2atmpS2179 = _M0L12digit__startS439 + _M0L6_2atmpS2180;
    _M0L6_2atmpS2178 = _M0L6_2atmpS2179 + 1;
    _M0L6bufferS448[_M0L6_2atmpS2178] = _M0L5d__loS457;
    moonbit_decref(_M0L6bufferS448);
  } else {
    int32_t _M0L6_2atmpS2187 = _M0Lm6offsetS437;
    int32_t _M0L6_2atmpS2192;
    int32_t _M0L6_2atmpS2188;
    int32_t _M0L6_2atmpS2191;
    int32_t _M0L6_2atmpS2190;
    int32_t _M0L6_2atmpS2189;
    _M0Lm6offsetS437 = _M0L6_2atmpS2187 - 1;
    _M0L6_2atmpS2192 = _M0Lm6offsetS437;
    _M0L6_2atmpS2188 = _M0L12digit__startS439 + _M0L6_2atmpS2192;
    _M0L6_2atmpS2191 = _M0Lm9remainingS450;
    _M0L6_2atmpS2190 = 48 + _M0L6_2atmpS2191;
    _M0L6_2atmpS2189 = (uint16_t)_M0L6_2atmpS2190;
    _M0L6bufferS448[_M0L6_2atmpS2188] = _M0L6_2atmpS2189;
    moonbit_decref(_M0L6bufferS448);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS430,
  uint64_t _M0L3numS424,
  int32_t _M0L12digit__startS422,
  int32_t _M0L10total__lenS421,
  int32_t _M0L5radixS426
) {
  int32_t _M0Lm6offsetS420;
  uint64_t _M0Lm1nS423;
  uint64_t _M0L4baseS425;
  int32_t _M0L6_2atmpS2119;
  int32_t _M0L6_2atmpS2118;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS420 = _M0L10total__lenS421 - _M0L12digit__startS422;
  _M0Lm1nS423 = _M0L3numS424;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS425 = _M0MPC13int3Int10to__uint64(_M0L5radixS426);
  _M0L6_2atmpS2119 = _M0L5radixS426 - 1;
  _M0L6_2atmpS2118 = _M0L5radixS426 & _M0L6_2atmpS2119;
  if (_M0L6_2atmpS2118 == 0) {
    int32_t _M0L5shiftS427;
    uint64_t _M0L4maskS428;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS427 = moonbit_ctz32(_M0L5radixS426);
    _M0L4maskS428 = _M0L4baseS425 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS2120 = _M0Lm1nS423;
      if (_M0L6_2atmpS2120 > 0ull) {
        int32_t _M0L6_2atmpS2121 = _M0Lm6offsetS420;
        uint64_t _M0L6_2atmpS2127;
        uint64_t _M0L6_2atmpS2126;
        int32_t _M0L5digitS429;
        int32_t _M0L6_2atmpS2124;
        int32_t _M0L6_2atmpS2122;
        int32_t _M0L6_2atmpS2123;
        uint64_t _M0L6_2atmpS2125;
        _M0Lm6offsetS420 = _M0L6_2atmpS2121 - 1;
        _M0L6_2atmpS2127 = _M0Lm1nS423;
        _M0L6_2atmpS2126 = _M0L6_2atmpS2127 & _M0L4maskS428;
        _M0L5digitS429 = (int32_t)_M0L6_2atmpS2126;
        _M0L6_2atmpS2124 = _M0Lm6offsetS420;
        _M0L6_2atmpS2122 = _M0L12digit__startS422 + _M0L6_2atmpS2124;
        _M0L6_2atmpS2123
        = ((moonbit_string_t)moonbit_string_literal_108.data)[
          _M0L5digitS429
        ];
        _M0L6bufferS430[_M0L6_2atmpS2122] = _M0L6_2atmpS2123;
        _M0L6_2atmpS2125 = _M0Lm1nS423;
        _M0Lm1nS423 = _M0L6_2atmpS2125 >> (_M0L5shiftS427 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS430);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS2128 = _M0Lm1nS423;
      if (_M0L6_2atmpS2128 > 0ull) {
        int32_t _M0L6_2atmpS2129 = _M0Lm6offsetS420;
        uint64_t _M0L6_2atmpS2136;
        uint64_t _M0L1qS432;
        uint64_t _M0L6_2atmpS2134;
        uint64_t _M0L6_2atmpS2135;
        uint64_t _M0L6_2atmpS2133;
        int32_t _M0L5digitS433;
        int32_t _M0L6_2atmpS2132;
        int32_t _M0L6_2atmpS2130;
        int32_t _M0L6_2atmpS2131;
        _M0Lm6offsetS420 = _M0L6_2atmpS2129 - 1;
        _M0L6_2atmpS2136 = _M0Lm1nS423;
        _M0L1qS432 = _M0L6_2atmpS2136 / _M0L4baseS425;
        _M0L6_2atmpS2134 = _M0Lm1nS423;
        _M0L6_2atmpS2135 = _M0L1qS432 * _M0L4baseS425;
        _M0L6_2atmpS2133 = _M0L6_2atmpS2134 - _M0L6_2atmpS2135;
        _M0L5digitS433 = (int32_t)_M0L6_2atmpS2133;
        _M0L6_2atmpS2132 = _M0Lm6offsetS420;
        _M0L6_2atmpS2130 = _M0L12digit__startS422 + _M0L6_2atmpS2132;
        _M0L6_2atmpS2131
        = ((moonbit_string_t)moonbit_string_literal_108.data)[
          _M0L5digitS433
        ];
        _M0L6bufferS430[_M0L6_2atmpS2130] = _M0L6_2atmpS2131;
        _M0Lm1nS423 = _M0L1qS432;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS430);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS417,
  uint64_t _M0L3numS413,
  int32_t _M0L12digit__startS411,
  int32_t _M0L10total__lenS410
) {
  int32_t _M0Lm6offsetS409;
  uint64_t _M0Lm1nS412;
  int32_t _M0L6_2atmpS2114;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS409 = _M0L10total__lenS410 - _M0L12digit__startS411;
  _M0Lm1nS412 = _M0L3numS413;
  while (1) {
    int32_t _M0L6_2atmpS2102 = _M0Lm6offsetS409;
    if (_M0L6_2atmpS2102 >= 2) {
      int32_t _M0L6_2atmpS2103 = _M0Lm6offsetS409;
      uint64_t _M0L6_2atmpS2113;
      uint64_t _M0L6_2atmpS2112;
      int32_t _M0L9byte__valS414;
      int32_t _M0L2hiS415;
      int32_t _M0L2loS416;
      int32_t _M0L6_2atmpS2106;
      int32_t _M0L6_2atmpS2104;
      int32_t _M0L6_2atmpS2105;
      int32_t _M0L6_2atmpS2110;
      int32_t _M0L6_2atmpS2109;
      int32_t _M0L6_2atmpS2107;
      int32_t _M0L6_2atmpS2108;
      uint64_t _M0L6_2atmpS2111;
      _M0Lm6offsetS409 = _M0L6_2atmpS2103 - 2;
      _M0L6_2atmpS2113 = _M0Lm1nS412;
      _M0L6_2atmpS2112 = _M0L6_2atmpS2113 & 255ull;
      _M0L9byte__valS414 = (int32_t)_M0L6_2atmpS2112;
      _M0L2hiS415 = _M0L9byte__valS414 / 16;
      _M0L2loS416 = _M0L9byte__valS414 % 16;
      _M0L6_2atmpS2106 = _M0Lm6offsetS409;
      _M0L6_2atmpS2104 = _M0L12digit__startS411 + _M0L6_2atmpS2106;
      _M0L6_2atmpS2105
      = ((moonbit_string_t)moonbit_string_literal_108.data)[
        _M0L2hiS415
      ];
      _M0L6bufferS417[_M0L6_2atmpS2104] = _M0L6_2atmpS2105;
      _M0L6_2atmpS2110 = _M0Lm6offsetS409;
      _M0L6_2atmpS2109 = _M0L12digit__startS411 + _M0L6_2atmpS2110;
      _M0L6_2atmpS2107 = _M0L6_2atmpS2109 + 1;
      _M0L6_2atmpS2108
      = ((moonbit_string_t)moonbit_string_literal_108.data)[
        _M0L2loS416
      ];
      _M0L6bufferS417[_M0L6_2atmpS2107] = _M0L6_2atmpS2108;
      _M0L6_2atmpS2111 = _M0Lm1nS412;
      _M0Lm1nS412 = _M0L6_2atmpS2111 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2114 = _M0Lm6offsetS409;
  if (_M0L6_2atmpS2114 == 1) {
    uint64_t _M0L6_2atmpS2117 = _M0Lm1nS412;
    uint64_t _M0L6_2atmpS2116 = _M0L6_2atmpS2117 & 15ull;
    int32_t _M0L6nibbleS419 = (int32_t)_M0L6_2atmpS2116;
    int32_t _M0L6_2atmpS2115 =
      ((moonbit_string_t)moonbit_string_literal_108.data)[_M0L6nibbleS419];
    _M0L6bufferS417[_M0L12digit__startS411] = _M0L6_2atmpS2115;
    moonbit_decref(_M0L6bufferS417);
  } else {
    moonbit_decref(_M0L6bufferS417);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS403,
  int32_t _M0L5radixS406
) {
  uint64_t _M0Lm3numS404;
  uint64_t _M0L4baseS405;
  int32_t _M0Lm5countS407;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS403 == 0ull) {
    return 1;
  }
  _M0Lm3numS404 = _M0L5valueS403;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS405 = _M0MPC13int3Int10to__uint64(_M0L5radixS406);
  _M0Lm5countS407 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS2099 = _M0Lm3numS404;
    if (_M0L6_2atmpS2099 > 0ull) {
      int32_t _M0L6_2atmpS2100 = _M0Lm5countS407;
      uint64_t _M0L6_2atmpS2101;
      _M0Lm5countS407 = _M0L6_2atmpS2100 + 1;
      _M0L6_2atmpS2101 = _M0Lm3numS404;
      _M0Lm3numS404 = _M0L6_2atmpS2101 / _M0L4baseS405;
      continue;
    }
    break;
  }
  return _M0Lm5countS407;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS401) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS401 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS402;
    int32_t _M0L6_2atmpS2098;
    int32_t _M0L6_2atmpS2097;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS402 = moonbit_clz64(_M0L5valueS401);
    _M0L6_2atmpS2098 = 63 - _M0L14leading__zerosS402;
    _M0L6_2atmpS2097 = _M0L6_2atmpS2098 / 4;
    return _M0L6_2atmpS2097 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS400) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS400 >= 10000000000ull) {
    if (_M0L5valueS400 >= 100000000000000ull) {
      if (_M0L5valueS400 >= 10000000000000000ull) {
        if (_M0L5valueS400 >= 1000000000000000000ull) {
          if (_M0L5valueS400 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS400 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS400 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS400 >= 1000000000000ull) {
      if (_M0L5valueS400 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS400 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS400 >= 100000ull) {
    if (_M0L5valueS400 >= 10000000ull) {
      if (_M0L5valueS400 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS400 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS400 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS400 >= 1000ull) {
    if (_M0L5valueS400 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS400 >= 100ull) {
    return 3;
  } else if (_M0L5valueS400 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS384,
  int32_t _M0L5radixS383
) {
  int32_t _if__result_4602;
  int32_t _M0L12is__negativeS385;
  uint32_t _M0L3numS386;
  uint16_t* _M0L6bufferS387;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS383 < 2) {
    _if__result_4602 = 1;
  } else {
    _if__result_4602 = _M0L5radixS383 > 36;
  }
  if (_if__result_4602) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_106.data, (moonbit_string_t)moonbit_string_literal_109.data);
  }
  if (_M0L4selfS384 == 0) {
    return (moonbit_string_t)moonbit_string_literal_17.data;
  }
  _M0L12is__negativeS385 = _M0L4selfS384 < 0;
  if (_M0L12is__negativeS385) {
    int32_t _M0L6_2atmpS2096 = -_M0L4selfS384;
    _M0L3numS386 = *(uint32_t*)&_M0L6_2atmpS2096;
  } else {
    _M0L3numS386 = *(uint32_t*)&_M0L4selfS384;
  }
  switch (_M0L5radixS383) {
    case 10: {
      int32_t _M0L10digit__lenS388;
      int32_t _M0L6_2atmpS2093;
      int32_t _M0L10total__lenS389;
      uint16_t* _M0L6bufferS390;
      int32_t _M0L12digit__startS391;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS388 = _M0FPB12dec__count32(_M0L3numS386);
      if (_M0L12is__negativeS385) {
        _M0L6_2atmpS2093 = 1;
      } else {
        _M0L6_2atmpS2093 = 0;
      }
      _M0L10total__lenS389 = _M0L10digit__lenS388 + _M0L6_2atmpS2093;
      _M0L6bufferS390
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS389, 0);
      if (_M0L12is__negativeS385) {
        _M0L12digit__startS391 = 1;
      } else {
        _M0L12digit__startS391 = 0;
      }
      moonbit_incref(_M0L6bufferS390);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS390, _M0L3numS386, _M0L12digit__startS391, _M0L10total__lenS389);
      _M0L6bufferS387 = _M0L6bufferS390;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS392;
      int32_t _M0L6_2atmpS2094;
      int32_t _M0L10total__lenS393;
      uint16_t* _M0L6bufferS394;
      int32_t _M0L12digit__startS395;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS392 = _M0FPB12hex__count32(_M0L3numS386);
      if (_M0L12is__negativeS385) {
        _M0L6_2atmpS2094 = 1;
      } else {
        _M0L6_2atmpS2094 = 0;
      }
      _M0L10total__lenS393 = _M0L10digit__lenS392 + _M0L6_2atmpS2094;
      _M0L6bufferS394
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS393, 0);
      if (_M0L12is__negativeS385) {
        _M0L12digit__startS395 = 1;
      } else {
        _M0L12digit__startS395 = 0;
      }
      moonbit_incref(_M0L6bufferS394);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS394, _M0L3numS386, _M0L12digit__startS395, _M0L10total__lenS393);
      _M0L6bufferS387 = _M0L6bufferS394;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS396;
      int32_t _M0L6_2atmpS2095;
      int32_t _M0L10total__lenS397;
      uint16_t* _M0L6bufferS398;
      int32_t _M0L12digit__startS399;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS396
      = _M0FPB14radix__count32(_M0L3numS386, _M0L5radixS383);
      if (_M0L12is__negativeS385) {
        _M0L6_2atmpS2095 = 1;
      } else {
        _M0L6_2atmpS2095 = 0;
      }
      _M0L10total__lenS397 = _M0L10digit__lenS396 + _M0L6_2atmpS2095;
      _M0L6bufferS398
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS397, 0);
      if (_M0L12is__negativeS385) {
        _M0L12digit__startS399 = 1;
      } else {
        _M0L12digit__startS399 = 0;
      }
      moonbit_incref(_M0L6bufferS398);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS398, _M0L3numS386, _M0L12digit__startS399, _M0L10total__lenS397, _M0L5radixS383);
      _M0L6bufferS387 = _M0L6bufferS398;
      break;
    }
  }
  if (_M0L12is__negativeS385) {
    _M0L6bufferS387[0] = 45;
  }
  return _M0L6bufferS387;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS377,
  int32_t _M0L5radixS380
) {
  uint32_t _M0Lm3numS378;
  uint32_t _M0L4baseS379;
  int32_t _M0Lm5countS381;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS377 == 0u) {
    return 1;
  }
  _M0Lm3numS378 = _M0L5valueS377;
  _M0L4baseS379 = *(uint32_t*)&_M0L5radixS380;
  _M0Lm5countS381 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS2090 = _M0Lm3numS378;
    if (_M0L6_2atmpS2090 > 0u) {
      int32_t _M0L6_2atmpS2091 = _M0Lm5countS381;
      uint32_t _M0L6_2atmpS2092;
      _M0Lm5countS381 = _M0L6_2atmpS2091 + 1;
      _M0L6_2atmpS2092 = _M0Lm3numS378;
      _M0Lm3numS378 = _M0L6_2atmpS2092 / _M0L4baseS379;
      continue;
    }
    break;
  }
  return _M0Lm5countS381;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS375) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS375 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS376;
    int32_t _M0L6_2atmpS2089;
    int32_t _M0L6_2atmpS2088;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS376 = moonbit_clz32(_M0L5valueS375);
    _M0L6_2atmpS2089 = 31 - _M0L14leading__zerosS376;
    _M0L6_2atmpS2088 = _M0L6_2atmpS2089 / 4;
    return _M0L6_2atmpS2088 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS374) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS374 >= 100000u) {
    if (_M0L5valueS374 >= 10000000u) {
      if (_M0L5valueS374 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS374 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS374 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS374 >= 1000u) {
    if (_M0L5valueS374 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS374 >= 100u) {
    return 3;
  } else if (_M0L5valueS374 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS364,
  uint32_t _M0L3numS352,
  int32_t _M0L12digit__startS355,
  int32_t _M0L10total__lenS354
) {
  uint32_t _M0Lm3numS351;
  int32_t _M0Lm6offsetS353;
  uint32_t _M0L6_2atmpS2087;
  int32_t _M0Lm9remainingS366;
  int32_t _M0L6_2atmpS2068;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS351 = _M0L3numS352;
  _M0Lm6offsetS353 = _M0L10total__lenS354 - _M0L12digit__startS355;
  while (1) {
    uint32_t _M0L6_2atmpS2031 = _M0Lm3numS351;
    if (_M0L6_2atmpS2031 >= 10000u) {
      uint32_t _M0L6_2atmpS2054 = _M0Lm3numS351;
      uint32_t _M0L1tS356 = _M0L6_2atmpS2054 / 10000u;
      uint32_t _M0L6_2atmpS2053 = _M0Lm3numS351;
      uint32_t _M0L6_2atmpS2052 = _M0L6_2atmpS2053 % 10000u;
      int32_t _M0L1rS357 = *(int32_t*)&_M0L6_2atmpS2052;
      int32_t _M0L2d1S358;
      int32_t _M0L2d2S359;
      int32_t _M0L6_2atmpS2032;
      int32_t _M0L6_2atmpS2051;
      int32_t _M0L6_2atmpS2050;
      int32_t _M0L6d1__hiS360;
      int32_t _M0L6_2atmpS2049;
      int32_t _M0L6_2atmpS2048;
      int32_t _M0L6d1__loS361;
      int32_t _M0L6_2atmpS2047;
      int32_t _M0L6_2atmpS2046;
      int32_t _M0L6d2__hiS362;
      int32_t _M0L6_2atmpS2045;
      int32_t _M0L6_2atmpS2044;
      int32_t _M0L6d2__loS363;
      int32_t _M0L6_2atmpS2034;
      int32_t _M0L6_2atmpS2033;
      int32_t _M0L6_2atmpS2037;
      int32_t _M0L6_2atmpS2036;
      int32_t _M0L6_2atmpS2035;
      int32_t _M0L6_2atmpS2040;
      int32_t _M0L6_2atmpS2039;
      int32_t _M0L6_2atmpS2038;
      int32_t _M0L6_2atmpS2043;
      int32_t _M0L6_2atmpS2042;
      int32_t _M0L6_2atmpS2041;
      _M0Lm3numS351 = _M0L1tS356;
      _M0L2d1S358 = _M0L1rS357 / 100;
      _M0L2d2S359 = _M0L1rS357 % 100;
      _M0L6_2atmpS2032 = _M0Lm6offsetS353;
      _M0Lm6offsetS353 = _M0L6_2atmpS2032 - 4;
      _M0L6_2atmpS2051 = _M0L2d1S358 / 10;
      _M0L6_2atmpS2050 = 48 + _M0L6_2atmpS2051;
      _M0L6d1__hiS360 = (uint16_t)_M0L6_2atmpS2050;
      _M0L6_2atmpS2049 = _M0L2d1S358 % 10;
      _M0L6_2atmpS2048 = 48 + _M0L6_2atmpS2049;
      _M0L6d1__loS361 = (uint16_t)_M0L6_2atmpS2048;
      _M0L6_2atmpS2047 = _M0L2d2S359 / 10;
      _M0L6_2atmpS2046 = 48 + _M0L6_2atmpS2047;
      _M0L6d2__hiS362 = (uint16_t)_M0L6_2atmpS2046;
      _M0L6_2atmpS2045 = _M0L2d2S359 % 10;
      _M0L6_2atmpS2044 = 48 + _M0L6_2atmpS2045;
      _M0L6d2__loS363 = (uint16_t)_M0L6_2atmpS2044;
      _M0L6_2atmpS2034 = _M0Lm6offsetS353;
      _M0L6_2atmpS2033 = _M0L12digit__startS355 + _M0L6_2atmpS2034;
      _M0L6bufferS364[_M0L6_2atmpS2033] = _M0L6d1__hiS360;
      _M0L6_2atmpS2037 = _M0Lm6offsetS353;
      _M0L6_2atmpS2036 = _M0L12digit__startS355 + _M0L6_2atmpS2037;
      _M0L6_2atmpS2035 = _M0L6_2atmpS2036 + 1;
      _M0L6bufferS364[_M0L6_2atmpS2035] = _M0L6d1__loS361;
      _M0L6_2atmpS2040 = _M0Lm6offsetS353;
      _M0L6_2atmpS2039 = _M0L12digit__startS355 + _M0L6_2atmpS2040;
      _M0L6_2atmpS2038 = _M0L6_2atmpS2039 + 2;
      _M0L6bufferS364[_M0L6_2atmpS2038] = _M0L6d2__hiS362;
      _M0L6_2atmpS2043 = _M0Lm6offsetS353;
      _M0L6_2atmpS2042 = _M0L12digit__startS355 + _M0L6_2atmpS2043;
      _M0L6_2atmpS2041 = _M0L6_2atmpS2042 + 3;
      _M0L6bufferS364[_M0L6_2atmpS2041] = _M0L6d2__loS363;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2087 = _M0Lm3numS351;
  _M0Lm9remainingS366 = *(int32_t*)&_M0L6_2atmpS2087;
  while (1) {
    int32_t _M0L6_2atmpS2055 = _M0Lm9remainingS366;
    if (_M0L6_2atmpS2055 >= 100) {
      int32_t _M0L6_2atmpS2067 = _M0Lm9remainingS366;
      int32_t _M0L1tS367 = _M0L6_2atmpS2067 / 100;
      int32_t _M0L6_2atmpS2066 = _M0Lm9remainingS366;
      int32_t _M0L1dS368 = _M0L6_2atmpS2066 % 100;
      int32_t _M0L6_2atmpS2056;
      int32_t _M0L6_2atmpS2065;
      int32_t _M0L6_2atmpS2064;
      int32_t _M0L5d__hiS369;
      int32_t _M0L6_2atmpS2063;
      int32_t _M0L6_2atmpS2062;
      int32_t _M0L5d__loS370;
      int32_t _M0L6_2atmpS2058;
      int32_t _M0L6_2atmpS2057;
      int32_t _M0L6_2atmpS2061;
      int32_t _M0L6_2atmpS2060;
      int32_t _M0L6_2atmpS2059;
      _M0Lm9remainingS366 = _M0L1tS367;
      _M0L6_2atmpS2056 = _M0Lm6offsetS353;
      _M0Lm6offsetS353 = _M0L6_2atmpS2056 - 2;
      _M0L6_2atmpS2065 = _M0L1dS368 / 10;
      _M0L6_2atmpS2064 = 48 + _M0L6_2atmpS2065;
      _M0L5d__hiS369 = (uint16_t)_M0L6_2atmpS2064;
      _M0L6_2atmpS2063 = _M0L1dS368 % 10;
      _M0L6_2atmpS2062 = 48 + _M0L6_2atmpS2063;
      _M0L5d__loS370 = (uint16_t)_M0L6_2atmpS2062;
      _M0L6_2atmpS2058 = _M0Lm6offsetS353;
      _M0L6_2atmpS2057 = _M0L12digit__startS355 + _M0L6_2atmpS2058;
      _M0L6bufferS364[_M0L6_2atmpS2057] = _M0L5d__hiS369;
      _M0L6_2atmpS2061 = _M0Lm6offsetS353;
      _M0L6_2atmpS2060 = _M0L12digit__startS355 + _M0L6_2atmpS2061;
      _M0L6_2atmpS2059 = _M0L6_2atmpS2060 + 1;
      _M0L6bufferS364[_M0L6_2atmpS2059] = _M0L5d__loS370;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2068 = _M0Lm9remainingS366;
  if (_M0L6_2atmpS2068 >= 10) {
    int32_t _M0L6_2atmpS2069 = _M0Lm6offsetS353;
    int32_t _M0L6_2atmpS2080;
    int32_t _M0L6_2atmpS2079;
    int32_t _M0L6_2atmpS2078;
    int32_t _M0L5d__hiS372;
    int32_t _M0L6_2atmpS2077;
    int32_t _M0L6_2atmpS2076;
    int32_t _M0L6_2atmpS2075;
    int32_t _M0L5d__loS373;
    int32_t _M0L6_2atmpS2071;
    int32_t _M0L6_2atmpS2070;
    int32_t _M0L6_2atmpS2074;
    int32_t _M0L6_2atmpS2073;
    int32_t _M0L6_2atmpS2072;
    _M0Lm6offsetS353 = _M0L6_2atmpS2069 - 2;
    _M0L6_2atmpS2080 = _M0Lm9remainingS366;
    _M0L6_2atmpS2079 = _M0L6_2atmpS2080 / 10;
    _M0L6_2atmpS2078 = 48 + _M0L6_2atmpS2079;
    _M0L5d__hiS372 = (uint16_t)_M0L6_2atmpS2078;
    _M0L6_2atmpS2077 = _M0Lm9remainingS366;
    _M0L6_2atmpS2076 = _M0L6_2atmpS2077 % 10;
    _M0L6_2atmpS2075 = 48 + _M0L6_2atmpS2076;
    _M0L5d__loS373 = (uint16_t)_M0L6_2atmpS2075;
    _M0L6_2atmpS2071 = _M0Lm6offsetS353;
    _M0L6_2atmpS2070 = _M0L12digit__startS355 + _M0L6_2atmpS2071;
    _M0L6bufferS364[_M0L6_2atmpS2070] = _M0L5d__hiS372;
    _M0L6_2atmpS2074 = _M0Lm6offsetS353;
    _M0L6_2atmpS2073 = _M0L12digit__startS355 + _M0L6_2atmpS2074;
    _M0L6_2atmpS2072 = _M0L6_2atmpS2073 + 1;
    _M0L6bufferS364[_M0L6_2atmpS2072] = _M0L5d__loS373;
    moonbit_decref(_M0L6bufferS364);
  } else {
    int32_t _M0L6_2atmpS2081 = _M0Lm6offsetS353;
    int32_t _M0L6_2atmpS2086;
    int32_t _M0L6_2atmpS2082;
    int32_t _M0L6_2atmpS2085;
    int32_t _M0L6_2atmpS2084;
    int32_t _M0L6_2atmpS2083;
    _M0Lm6offsetS353 = _M0L6_2atmpS2081 - 1;
    _M0L6_2atmpS2086 = _M0Lm6offsetS353;
    _M0L6_2atmpS2082 = _M0L12digit__startS355 + _M0L6_2atmpS2086;
    _M0L6_2atmpS2085 = _M0Lm9remainingS366;
    _M0L6_2atmpS2084 = 48 + _M0L6_2atmpS2085;
    _M0L6_2atmpS2083 = (uint16_t)_M0L6_2atmpS2084;
    _M0L6bufferS364[_M0L6_2atmpS2082] = _M0L6_2atmpS2083;
    moonbit_decref(_M0L6bufferS364);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS346,
  uint32_t _M0L3numS340,
  int32_t _M0L12digit__startS338,
  int32_t _M0L10total__lenS337,
  int32_t _M0L5radixS342
) {
  int32_t _M0Lm6offsetS336;
  uint32_t _M0Lm1nS339;
  uint32_t _M0L4baseS341;
  int32_t _M0L6_2atmpS2013;
  int32_t _M0L6_2atmpS2012;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS336 = _M0L10total__lenS337 - _M0L12digit__startS338;
  _M0Lm1nS339 = _M0L3numS340;
  _M0L4baseS341 = *(uint32_t*)&_M0L5radixS342;
  _M0L6_2atmpS2013 = _M0L5radixS342 - 1;
  _M0L6_2atmpS2012 = _M0L5radixS342 & _M0L6_2atmpS2013;
  if (_M0L6_2atmpS2012 == 0) {
    int32_t _M0L5shiftS343;
    uint32_t _M0L4maskS344;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS343 = moonbit_ctz32(_M0L5radixS342);
    _M0L4maskS344 = _M0L4baseS341 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS2014 = _M0Lm1nS339;
      if (_M0L6_2atmpS2014 > 0u) {
        int32_t _M0L6_2atmpS2015 = _M0Lm6offsetS336;
        uint32_t _M0L6_2atmpS2021;
        uint32_t _M0L6_2atmpS2020;
        int32_t _M0L5digitS345;
        int32_t _M0L6_2atmpS2018;
        int32_t _M0L6_2atmpS2016;
        int32_t _M0L6_2atmpS2017;
        uint32_t _M0L6_2atmpS2019;
        _M0Lm6offsetS336 = _M0L6_2atmpS2015 - 1;
        _M0L6_2atmpS2021 = _M0Lm1nS339;
        _M0L6_2atmpS2020 = _M0L6_2atmpS2021 & _M0L4maskS344;
        _M0L5digitS345 = *(int32_t*)&_M0L6_2atmpS2020;
        _M0L6_2atmpS2018 = _M0Lm6offsetS336;
        _M0L6_2atmpS2016 = _M0L12digit__startS338 + _M0L6_2atmpS2018;
        _M0L6_2atmpS2017
        = ((moonbit_string_t)moonbit_string_literal_108.data)[
          _M0L5digitS345
        ];
        _M0L6bufferS346[_M0L6_2atmpS2016] = _M0L6_2atmpS2017;
        _M0L6_2atmpS2019 = _M0Lm1nS339;
        _M0Lm1nS339 = _M0L6_2atmpS2019 >> (_M0L5shiftS343 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS346);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS2022 = _M0Lm1nS339;
      if (_M0L6_2atmpS2022 > 0u) {
        int32_t _M0L6_2atmpS2023 = _M0Lm6offsetS336;
        uint32_t _M0L6_2atmpS2030;
        uint32_t _M0L1qS348;
        uint32_t _M0L6_2atmpS2028;
        uint32_t _M0L6_2atmpS2029;
        uint32_t _M0L6_2atmpS2027;
        int32_t _M0L5digitS349;
        int32_t _M0L6_2atmpS2026;
        int32_t _M0L6_2atmpS2024;
        int32_t _M0L6_2atmpS2025;
        _M0Lm6offsetS336 = _M0L6_2atmpS2023 - 1;
        _M0L6_2atmpS2030 = _M0Lm1nS339;
        _M0L1qS348 = _M0L6_2atmpS2030 / _M0L4baseS341;
        _M0L6_2atmpS2028 = _M0Lm1nS339;
        _M0L6_2atmpS2029 = _M0L1qS348 * _M0L4baseS341;
        _M0L6_2atmpS2027 = _M0L6_2atmpS2028 - _M0L6_2atmpS2029;
        _M0L5digitS349 = *(int32_t*)&_M0L6_2atmpS2027;
        _M0L6_2atmpS2026 = _M0Lm6offsetS336;
        _M0L6_2atmpS2024 = _M0L12digit__startS338 + _M0L6_2atmpS2026;
        _M0L6_2atmpS2025
        = ((moonbit_string_t)moonbit_string_literal_108.data)[
          _M0L5digitS349
        ];
        _M0L6bufferS346[_M0L6_2atmpS2024] = _M0L6_2atmpS2025;
        _M0Lm1nS339 = _M0L1qS348;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS346);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS333,
  uint32_t _M0L3numS329,
  int32_t _M0L12digit__startS327,
  int32_t _M0L10total__lenS326
) {
  int32_t _M0Lm6offsetS325;
  uint32_t _M0Lm1nS328;
  int32_t _M0L6_2atmpS2008;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS325 = _M0L10total__lenS326 - _M0L12digit__startS327;
  _M0Lm1nS328 = _M0L3numS329;
  while (1) {
    int32_t _M0L6_2atmpS1996 = _M0Lm6offsetS325;
    if (_M0L6_2atmpS1996 >= 2) {
      int32_t _M0L6_2atmpS1997 = _M0Lm6offsetS325;
      uint32_t _M0L6_2atmpS2007;
      uint32_t _M0L6_2atmpS2006;
      int32_t _M0L9byte__valS330;
      int32_t _M0L2hiS331;
      int32_t _M0L2loS332;
      int32_t _M0L6_2atmpS2000;
      int32_t _M0L6_2atmpS1998;
      int32_t _M0L6_2atmpS1999;
      int32_t _M0L6_2atmpS2004;
      int32_t _M0L6_2atmpS2003;
      int32_t _M0L6_2atmpS2001;
      int32_t _M0L6_2atmpS2002;
      uint32_t _M0L6_2atmpS2005;
      _M0Lm6offsetS325 = _M0L6_2atmpS1997 - 2;
      _M0L6_2atmpS2007 = _M0Lm1nS328;
      _M0L6_2atmpS2006 = _M0L6_2atmpS2007 & 255u;
      _M0L9byte__valS330 = *(int32_t*)&_M0L6_2atmpS2006;
      _M0L2hiS331 = _M0L9byte__valS330 / 16;
      _M0L2loS332 = _M0L9byte__valS330 % 16;
      _M0L6_2atmpS2000 = _M0Lm6offsetS325;
      _M0L6_2atmpS1998 = _M0L12digit__startS327 + _M0L6_2atmpS2000;
      _M0L6_2atmpS1999
      = ((moonbit_string_t)moonbit_string_literal_108.data)[
        _M0L2hiS331
      ];
      _M0L6bufferS333[_M0L6_2atmpS1998] = _M0L6_2atmpS1999;
      _M0L6_2atmpS2004 = _M0Lm6offsetS325;
      _M0L6_2atmpS2003 = _M0L12digit__startS327 + _M0L6_2atmpS2004;
      _M0L6_2atmpS2001 = _M0L6_2atmpS2003 + 1;
      _M0L6_2atmpS2002
      = ((moonbit_string_t)moonbit_string_literal_108.data)[
        _M0L2loS332
      ];
      _M0L6bufferS333[_M0L6_2atmpS2001] = _M0L6_2atmpS2002;
      _M0L6_2atmpS2005 = _M0Lm1nS328;
      _M0Lm1nS328 = _M0L6_2atmpS2005 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2008 = _M0Lm6offsetS325;
  if (_M0L6_2atmpS2008 == 1) {
    uint32_t _M0L6_2atmpS2011 = _M0Lm1nS328;
    uint32_t _M0L6_2atmpS2010 = _M0L6_2atmpS2011 & 15u;
    int32_t _M0L6nibbleS335 = *(int32_t*)&_M0L6_2atmpS2010;
    int32_t _M0L6_2atmpS2009 =
      ((moonbit_string_t)moonbit_string_literal_108.data)[_M0L6nibbleS335];
    _M0L6bufferS333[_M0L12digit__startS327] = _M0L6_2atmpS2009;
    moonbit_decref(_M0L6bufferS333);
  } else {
    moonbit_decref(_M0L6bufferS333);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS312) {
  struct _M0TWEOs* _M0L7_2afuncS311;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS311 = _M0L4selfS312;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS311->code(_M0L7_2afuncS311);
}

struct _M0TURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0MPB4Iter4nextGURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerEE(
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L4selfS314
) {
  struct _M0TWEOURPC16string10StringViewRP48clawteam8clawteam8internal5httpx5LayerE* _M0L7_2afuncS313;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS313 = _M0L4selfS314;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS313->code(_M0L7_2afuncS313);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS316
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS315;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS315 = _M0L4selfS316;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS315->code(_M0L7_2afuncS315);
}

void* _M0MPB4Iter4nextGRPB4JsonE(struct _M0TWEORPB4Json* _M0L4selfS318) {
  struct _M0TWEORPB4Json* _M0L7_2afuncS317;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS317 = _M0L4selfS318;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS317->code(_M0L7_2afuncS317);
}

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS320
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2afuncS319;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS319 = _M0L4selfS320;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS319->code(_M0L7_2afuncS319);
}

int64_t _M0MPB4Iter4nextGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TWEOi* _M0L4selfS322
) {
  struct _M0TWEOi* _M0L7_2afuncS321;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS321 = _M0L4selfS322;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS321->code(_M0L7_2afuncS321);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS324) {
  struct _M0TWEOc* _M0L7_2afuncS323;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS323 = _M0L4selfS324;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS323->code(_M0L7_2afuncS323);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS304
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS303;
  struct _M0TPB6Logger _M0L6_2atmpS1992;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS303 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS303);
  _M0L6_2atmpS1992
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS303
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS304, _M0L6_2atmpS1992);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS303);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS306
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS305;
  struct _M0TPB6Logger _M0L6_2atmpS1993;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS305 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS305);
  _M0L6_2atmpS1993
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS305
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS306, _M0L6_2atmpS1993);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS305);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS308
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS307;
  struct _M0TPB6Logger _M0L6_2atmpS1994;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS307 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS307);
  _M0L6_2atmpS1994
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS307
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS308, _M0L6_2atmpS1994);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS307);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS310
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS309;
  struct _M0TPB6Logger _M0L6_2atmpS1995;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS309 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS309);
  _M0L6_2atmpS1995
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS309
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS310, _M0L6_2atmpS1995);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS309);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS302
) {
  int32_t _M0L8_2afieldS4039;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4039 = _M0L4selfS302.$1;
  moonbit_decref(_M0L4selfS302.$0);
  return _M0L8_2afieldS4039;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS301
) {
  int32_t _M0L3endS1990;
  int32_t _M0L8_2afieldS4040;
  int32_t _M0L5startS1991;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1990 = _M0L4selfS301.$2;
  _M0L8_2afieldS4040 = _M0L4selfS301.$1;
  moonbit_decref(_M0L4selfS301.$0);
  _M0L5startS1991 = _M0L8_2afieldS4040;
  return _M0L3endS1990 - _M0L5startS1991;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS300
) {
  moonbit_string_t _M0L8_2afieldS4041;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4041 = _M0L4selfS300.$0;
  return _M0L8_2afieldS4041;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS296,
  moonbit_string_t _M0L5valueS297,
  int32_t _M0L5startS298,
  int32_t _M0L3lenS299
) {
  int32_t _M0L6_2atmpS1989;
  int64_t _M0L6_2atmpS1988;
  struct _M0TPC16string10StringView _M0L6_2atmpS1987;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1989 = _M0L5startS298 + _M0L3lenS299;
  _M0L6_2atmpS1988 = (int64_t)_M0L6_2atmpS1989;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1987
  = _M0MPC16string6String11sub_2einner(_M0L5valueS297, _M0L5startS298, _M0L6_2atmpS1988);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS296, _M0L6_2atmpS1987);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS289,
  int32_t _M0L5startS295,
  int64_t _M0L3endS291
) {
  int32_t _M0L3lenS288;
  int32_t _M0L3endS290;
  int32_t _M0L5startS294;
  int32_t _if__result_4609;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS288 = Moonbit_array_length(_M0L4selfS289);
  if (_M0L3endS291 == 4294967296ll) {
    _M0L3endS290 = _M0L3lenS288;
  } else {
    int64_t _M0L7_2aSomeS292 = _M0L3endS291;
    int32_t _M0L6_2aendS293 = (int32_t)_M0L7_2aSomeS292;
    if (_M0L6_2aendS293 < 0) {
      _M0L3endS290 = _M0L3lenS288 + _M0L6_2aendS293;
    } else {
      _M0L3endS290 = _M0L6_2aendS293;
    }
  }
  if (_M0L5startS295 < 0) {
    _M0L5startS294 = _M0L3lenS288 + _M0L5startS295;
  } else {
    _M0L5startS294 = _M0L5startS295;
  }
  if (_M0L5startS294 >= 0) {
    if (_M0L5startS294 <= _M0L3endS290) {
      _if__result_4609 = _M0L3endS290 <= _M0L3lenS288;
    } else {
      _if__result_4609 = 0;
    }
  } else {
    _if__result_4609 = 0;
  }
  if (_if__result_4609) {
    if (_M0L5startS294 < _M0L3lenS288) {
      int32_t _M0L6_2atmpS1984 = _M0L4selfS289[_M0L5startS294];
      int32_t _M0L6_2atmpS1983;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1983
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1984);
      if (!_M0L6_2atmpS1983) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS290 < _M0L3lenS288) {
      int32_t _M0L6_2atmpS1986 = _M0L4selfS289[_M0L3endS290];
      int32_t _M0L6_2atmpS1985;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1985
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1986);
      if (!_M0L6_2atmpS1985) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS294,
                                                 _M0L3endS290,
                                                 _M0L4selfS289};
  } else {
    moonbit_decref(_M0L4selfS289);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS283) {
  struct _M0TPB6Hasher* _M0L1hS282;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS282 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS282);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS282, _M0L4selfS283);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS282);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS285
) {
  struct _M0TPB6Hasher* _M0L1hS284;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS284 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS284);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS284, _M0L4selfS285);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS284);
}

int32_t _M0IP016_24default__implPB4Hash4hashGRPC16string10StringViewE(
  struct _M0TPC16string10StringView _M0L4selfS287
) {
  struct _M0TPB6Hasher* _M0L1hS286;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS286 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS286);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGRPC16string10StringViewE(_M0L1hS286, _M0L4selfS287);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS286);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS280) {
  int32_t _M0L4seedS279;
  if (_M0L10seed_2eoptS280 == 4294967296ll) {
    _M0L4seedS279 = 0;
  } else {
    int64_t _M0L7_2aSomeS281 = _M0L10seed_2eoptS280;
    _M0L4seedS279 = (int32_t)_M0L7_2aSomeS281;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS279);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS278) {
  uint32_t _M0L6_2atmpS1982;
  uint32_t _M0L6_2atmpS1981;
  struct _M0TPB6Hasher* _block_4610;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1982 = *(uint32_t*)&_M0L4seedS278;
  _M0L6_2atmpS1981 = _M0L6_2atmpS1982 + 374761393u;
  _block_4610
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4610)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4610->$0 = _M0L6_2atmpS1981;
  return _block_4610;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS277) {
  uint32_t _M0L6_2atmpS1980;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1980 = _M0MPB6Hasher9avalanche(_M0L4selfS277);
  return *(int32_t*)&_M0L6_2atmpS1980;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS276) {
  uint32_t _M0L8_2afieldS4042;
  uint32_t _M0Lm3accS275;
  uint32_t _M0L6_2atmpS1969;
  uint32_t _M0L6_2atmpS1971;
  uint32_t _M0L6_2atmpS1970;
  uint32_t _M0L6_2atmpS1972;
  uint32_t _M0L6_2atmpS1973;
  uint32_t _M0L6_2atmpS1975;
  uint32_t _M0L6_2atmpS1974;
  uint32_t _M0L6_2atmpS1976;
  uint32_t _M0L6_2atmpS1977;
  uint32_t _M0L6_2atmpS1979;
  uint32_t _M0L6_2atmpS1978;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4042 = _M0L4selfS276->$0;
  moonbit_decref(_M0L4selfS276);
  _M0Lm3accS275 = _M0L8_2afieldS4042;
  _M0L6_2atmpS1969 = _M0Lm3accS275;
  _M0L6_2atmpS1971 = _M0Lm3accS275;
  _M0L6_2atmpS1970 = _M0L6_2atmpS1971 >> 15;
  _M0Lm3accS275 = _M0L6_2atmpS1969 ^ _M0L6_2atmpS1970;
  _M0L6_2atmpS1972 = _M0Lm3accS275;
  _M0Lm3accS275 = _M0L6_2atmpS1972 * 2246822519u;
  _M0L6_2atmpS1973 = _M0Lm3accS275;
  _M0L6_2atmpS1975 = _M0Lm3accS275;
  _M0L6_2atmpS1974 = _M0L6_2atmpS1975 >> 13;
  _M0Lm3accS275 = _M0L6_2atmpS1973 ^ _M0L6_2atmpS1974;
  _M0L6_2atmpS1976 = _M0Lm3accS275;
  _M0Lm3accS275 = _M0L6_2atmpS1976 * 3266489917u;
  _M0L6_2atmpS1977 = _M0Lm3accS275;
  _M0L6_2atmpS1979 = _M0Lm3accS275;
  _M0L6_2atmpS1978 = _M0L6_2atmpS1979 >> 16;
  _M0Lm3accS275 = _M0L6_2atmpS1977 ^ _M0L6_2atmpS1978;
  return _M0Lm3accS275;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS273,
  moonbit_string_t _M0L1yS274
) {
  int32_t _M0L6_2atmpS4043;
  int32_t _M0L6_2atmpS1968;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4043 = moonbit_val_array_equal(_M0L1xS273, _M0L1yS274);
  moonbit_decref(_M0L1xS273);
  moonbit_decref(_M0L1yS274);
  _M0L6_2atmpS1968 = _M0L6_2atmpS4043;
  return !_M0L6_2atmpS1968;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS268,
  int32_t _M0L5valueS267
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS267, _M0L4selfS268);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS270,
  moonbit_string_t _M0L5valueS269
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS269, _M0L4selfS270);
  return 0;
}

int32_t _M0MPB6Hasher7combineGRPC16string10StringViewE(
  struct _M0TPB6Hasher* _M0L4selfS272,
  struct _M0TPC16string10StringView _M0L5valueS271
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string10StringViewPB4Hash13hash__combine(_M0L5valueS271, _M0L4selfS272);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS266) {
  int64_t _M0L6_2atmpS1967;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1967 = (int64_t)_M0L4selfS266;
  return *(uint64_t*)&_M0L6_2atmpS1967;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS264,
  int32_t _M0L5valueS265
) {
  uint32_t _M0L6_2atmpS1966;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1966 = *(uint32_t*)&_M0L5valueS265;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS264, _M0L6_2atmpS1966);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS257
) {
  struct _M0TPB13StringBuilder* _M0L3bufS255;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS256;
  int32_t _M0L7_2abindS258;
  int32_t _M0L1iS259;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS255 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS256 = _M0L4selfS257;
  moonbit_incref(_M0L3bufS255);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS255, 91);
  _M0L7_2abindS258 = _M0L7_2aselfS256->$1;
  _M0L1iS259 = 0;
  while (1) {
    if (_M0L1iS259 < _M0L7_2abindS258) {
      int32_t _if__result_4612;
      moonbit_string_t* _M0L8_2afieldS4045;
      moonbit_string_t* _M0L3bufS1964;
      moonbit_string_t _M0L6_2atmpS4044;
      moonbit_string_t _M0L4itemS260;
      int32_t _M0L6_2atmpS1965;
      if (_M0L1iS259 != 0) {
        moonbit_incref(_M0L3bufS255);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS255, (moonbit_string_t)moonbit_string_literal_110.data);
      }
      if (_M0L1iS259 < 0) {
        _if__result_4612 = 1;
      } else {
        int32_t _M0L3lenS1963 = _M0L7_2aselfS256->$1;
        _if__result_4612 = _M0L1iS259 >= _M0L3lenS1963;
      }
      if (_if__result_4612) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS4045 = _M0L7_2aselfS256->$0;
      _M0L3bufS1964 = _M0L8_2afieldS4045;
      _M0L6_2atmpS4044 = (moonbit_string_t)_M0L3bufS1964[_M0L1iS259];
      _M0L4itemS260 = _M0L6_2atmpS4044;
      if (_M0L4itemS260 == 0) {
        moonbit_incref(_M0L3bufS255);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS255, (moonbit_string_t)moonbit_string_literal_73.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS261 = _M0L4itemS260;
        moonbit_string_t _M0L6_2alocS262 = _M0L7_2aSomeS261;
        moonbit_string_t _M0L6_2atmpS1962;
        moonbit_incref(_M0L6_2alocS262);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1962
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS262);
        moonbit_incref(_M0L3bufS255);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS255, _M0L6_2atmpS1962);
      }
      _M0L6_2atmpS1965 = _M0L1iS259 + 1;
      _M0L1iS259 = _M0L6_2atmpS1965;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS256);
    }
    break;
  }
  moonbit_incref(_M0L3bufS255);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS255, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS255);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS254
) {
  moonbit_string_t _M0L6_2atmpS1961;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1960;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1961 = _M0L4selfS254;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1960 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1961);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1960);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS253
) {
  struct _M0TPB13StringBuilder* _M0L2sbS252;
  struct _M0TPC16string10StringView _M0L8_2afieldS4058;
  struct _M0TPC16string10StringView _M0L3pkgS1945;
  moonbit_string_t _M0L6_2atmpS1944;
  moonbit_string_t _M0L6_2atmpS4057;
  moonbit_string_t _M0L6_2atmpS1943;
  moonbit_string_t _M0L6_2atmpS4056;
  moonbit_string_t _M0L6_2atmpS1942;
  struct _M0TPC16string10StringView _M0L8_2afieldS4055;
  struct _M0TPC16string10StringView _M0L8filenameS1946;
  struct _M0TPC16string10StringView _M0L8_2afieldS4054;
  struct _M0TPC16string10StringView _M0L11start__lineS1949;
  moonbit_string_t _M0L6_2atmpS1948;
  moonbit_string_t _M0L6_2atmpS4053;
  moonbit_string_t _M0L6_2atmpS1947;
  struct _M0TPC16string10StringView _M0L8_2afieldS4052;
  struct _M0TPC16string10StringView _M0L13start__columnS1952;
  moonbit_string_t _M0L6_2atmpS1951;
  moonbit_string_t _M0L6_2atmpS4051;
  moonbit_string_t _M0L6_2atmpS1950;
  struct _M0TPC16string10StringView _M0L8_2afieldS4050;
  struct _M0TPC16string10StringView _M0L9end__lineS1955;
  moonbit_string_t _M0L6_2atmpS1954;
  moonbit_string_t _M0L6_2atmpS4049;
  moonbit_string_t _M0L6_2atmpS1953;
  struct _M0TPC16string10StringView _M0L8_2afieldS4048;
  int32_t _M0L6_2acntS4401;
  struct _M0TPC16string10StringView _M0L11end__columnS1959;
  moonbit_string_t _M0L6_2atmpS1958;
  moonbit_string_t _M0L6_2atmpS4047;
  moonbit_string_t _M0L6_2atmpS1957;
  moonbit_string_t _M0L6_2atmpS4046;
  moonbit_string_t _M0L6_2atmpS1956;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS252 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS4058
  = (struct _M0TPC16string10StringView){
    _M0L4selfS253->$0_1, _M0L4selfS253->$0_2, _M0L4selfS253->$0_0
  };
  _M0L3pkgS1945 = _M0L8_2afieldS4058;
  moonbit_incref(_M0L3pkgS1945.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1944
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1945);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4057
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_111.data, _M0L6_2atmpS1944);
  moonbit_decref(_M0L6_2atmpS1944);
  _M0L6_2atmpS1943 = _M0L6_2atmpS4057;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4056
  = moonbit_add_string(_M0L6_2atmpS1943, (moonbit_string_t)moonbit_string_literal_112.data);
  moonbit_decref(_M0L6_2atmpS1943);
  _M0L6_2atmpS1942 = _M0L6_2atmpS4056;
  moonbit_incref(_M0L2sbS252);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS252, _M0L6_2atmpS1942);
  moonbit_incref(_M0L2sbS252);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS252, (moonbit_string_t)moonbit_string_literal_113.data);
  _M0L8_2afieldS4055
  = (struct _M0TPC16string10StringView){
    _M0L4selfS253->$1_1, _M0L4selfS253->$1_2, _M0L4selfS253->$1_0
  };
  _M0L8filenameS1946 = _M0L8_2afieldS4055;
  moonbit_incref(_M0L8filenameS1946.$0);
  moonbit_incref(_M0L2sbS252);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS252, _M0L8filenameS1946);
  _M0L8_2afieldS4054
  = (struct _M0TPC16string10StringView){
    _M0L4selfS253->$2_1, _M0L4selfS253->$2_2, _M0L4selfS253->$2_0
  };
  _M0L11start__lineS1949 = _M0L8_2afieldS4054;
  moonbit_incref(_M0L11start__lineS1949.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1948
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1949);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4053
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_114.data, _M0L6_2atmpS1948);
  moonbit_decref(_M0L6_2atmpS1948);
  _M0L6_2atmpS1947 = _M0L6_2atmpS4053;
  moonbit_incref(_M0L2sbS252);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS252, _M0L6_2atmpS1947);
  _M0L8_2afieldS4052
  = (struct _M0TPC16string10StringView){
    _M0L4selfS253->$3_1, _M0L4selfS253->$3_2, _M0L4selfS253->$3_0
  };
  _M0L13start__columnS1952 = _M0L8_2afieldS4052;
  moonbit_incref(_M0L13start__columnS1952.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1951
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1952);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4051
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_115.data, _M0L6_2atmpS1951);
  moonbit_decref(_M0L6_2atmpS1951);
  _M0L6_2atmpS1950 = _M0L6_2atmpS4051;
  moonbit_incref(_M0L2sbS252);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS252, _M0L6_2atmpS1950);
  _M0L8_2afieldS4050
  = (struct _M0TPC16string10StringView){
    _M0L4selfS253->$4_1, _M0L4selfS253->$4_2, _M0L4selfS253->$4_0
  };
  _M0L9end__lineS1955 = _M0L8_2afieldS4050;
  moonbit_incref(_M0L9end__lineS1955.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1954
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1955);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4049
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_116.data, _M0L6_2atmpS1954);
  moonbit_decref(_M0L6_2atmpS1954);
  _M0L6_2atmpS1953 = _M0L6_2atmpS4049;
  moonbit_incref(_M0L2sbS252);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS252, _M0L6_2atmpS1953);
  _M0L8_2afieldS4048
  = (struct _M0TPC16string10StringView){
    _M0L4selfS253->$5_1, _M0L4selfS253->$5_2, _M0L4selfS253->$5_0
  };
  _M0L6_2acntS4401 = Moonbit_object_header(_M0L4selfS253)->rc;
  if (_M0L6_2acntS4401 > 1) {
    int32_t _M0L11_2anew__cntS4407 = _M0L6_2acntS4401 - 1;
    Moonbit_object_header(_M0L4selfS253)->rc = _M0L11_2anew__cntS4407;
    moonbit_incref(_M0L8_2afieldS4048.$0);
  } else if (_M0L6_2acntS4401 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4406 =
      (struct _M0TPC16string10StringView){_M0L4selfS253->$4_1,
                                            _M0L4selfS253->$4_2,
                                            _M0L4selfS253->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4405;
    struct _M0TPC16string10StringView _M0L8_2afieldS4404;
    struct _M0TPC16string10StringView _M0L8_2afieldS4403;
    struct _M0TPC16string10StringView _M0L8_2afieldS4402;
    moonbit_decref(_M0L8_2afieldS4406.$0);
    _M0L8_2afieldS4405
    = (struct _M0TPC16string10StringView){
      _M0L4selfS253->$3_1, _M0L4selfS253->$3_2, _M0L4selfS253->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4405.$0);
    _M0L8_2afieldS4404
    = (struct _M0TPC16string10StringView){
      _M0L4selfS253->$2_1, _M0L4selfS253->$2_2, _M0L4selfS253->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4404.$0);
    _M0L8_2afieldS4403
    = (struct _M0TPC16string10StringView){
      _M0L4selfS253->$1_1, _M0L4selfS253->$1_2, _M0L4selfS253->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4403.$0);
    _M0L8_2afieldS4402
    = (struct _M0TPC16string10StringView){
      _M0L4selfS253->$0_1, _M0L4selfS253->$0_2, _M0L4selfS253->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4402.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS253);
  }
  _M0L11end__columnS1959 = _M0L8_2afieldS4048;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1958
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1959);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4047
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_117.data, _M0L6_2atmpS1958);
  moonbit_decref(_M0L6_2atmpS1958);
  _M0L6_2atmpS1957 = _M0L6_2atmpS4047;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4046
  = moonbit_add_string(_M0L6_2atmpS1957, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1957);
  _M0L6_2atmpS1956 = _M0L6_2atmpS4046;
  moonbit_incref(_M0L2sbS252);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS252, _M0L6_2atmpS1956);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS252);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS250,
  moonbit_string_t _M0L3strS251
) {
  int32_t _M0L3lenS1932;
  int32_t _M0L6_2atmpS1934;
  int32_t _M0L6_2atmpS1933;
  int32_t _M0L6_2atmpS1931;
  moonbit_bytes_t _M0L8_2afieldS4060;
  moonbit_bytes_t _M0L4dataS1935;
  int32_t _M0L3lenS1936;
  int32_t _M0L6_2atmpS1937;
  int32_t _M0L3lenS1939;
  int32_t _M0L6_2atmpS4059;
  int32_t _M0L6_2atmpS1941;
  int32_t _M0L6_2atmpS1940;
  int32_t _M0L6_2atmpS1938;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1932 = _M0L4selfS250->$1;
  _M0L6_2atmpS1934 = Moonbit_array_length(_M0L3strS251);
  _M0L6_2atmpS1933 = _M0L6_2atmpS1934 * 2;
  _M0L6_2atmpS1931 = _M0L3lenS1932 + _M0L6_2atmpS1933;
  moonbit_incref(_M0L4selfS250);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS250, _M0L6_2atmpS1931);
  _M0L8_2afieldS4060 = _M0L4selfS250->$0;
  _M0L4dataS1935 = _M0L8_2afieldS4060;
  _M0L3lenS1936 = _M0L4selfS250->$1;
  _M0L6_2atmpS1937 = Moonbit_array_length(_M0L3strS251);
  moonbit_incref(_M0L4dataS1935);
  moonbit_incref(_M0L3strS251);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1935, _M0L3lenS1936, _M0L3strS251, 0, _M0L6_2atmpS1937);
  _M0L3lenS1939 = _M0L4selfS250->$1;
  _M0L6_2atmpS4059 = Moonbit_array_length(_M0L3strS251);
  moonbit_decref(_M0L3strS251);
  _M0L6_2atmpS1941 = _M0L6_2atmpS4059;
  _M0L6_2atmpS1940 = _M0L6_2atmpS1941 * 2;
  _M0L6_2atmpS1938 = _M0L3lenS1939 + _M0L6_2atmpS1940;
  _M0L4selfS250->$1 = _M0L6_2atmpS1938;
  moonbit_decref(_M0L4selfS250);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS242,
  int32_t _M0L13bytes__offsetS237,
  moonbit_string_t _M0L3strS244,
  int32_t _M0L11str__offsetS240,
  int32_t _M0L6lengthS238
) {
  int32_t _M0L6_2atmpS1930;
  int32_t _M0L6_2atmpS1929;
  int32_t _M0L2e1S236;
  int32_t _M0L6_2atmpS1928;
  int32_t _M0L2e2S239;
  int32_t _M0L4len1S241;
  int32_t _M0L4len2S243;
  int32_t _if__result_4613;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1930 = _M0L6lengthS238 * 2;
  _M0L6_2atmpS1929 = _M0L13bytes__offsetS237 + _M0L6_2atmpS1930;
  _M0L2e1S236 = _M0L6_2atmpS1929 - 1;
  _M0L6_2atmpS1928 = _M0L11str__offsetS240 + _M0L6lengthS238;
  _M0L2e2S239 = _M0L6_2atmpS1928 - 1;
  _M0L4len1S241 = Moonbit_array_length(_M0L4selfS242);
  _M0L4len2S243 = Moonbit_array_length(_M0L3strS244);
  if (_M0L6lengthS238 >= 0) {
    if (_M0L13bytes__offsetS237 >= 0) {
      if (_M0L2e1S236 < _M0L4len1S241) {
        if (_M0L11str__offsetS240 >= 0) {
          _if__result_4613 = _M0L2e2S239 < _M0L4len2S243;
        } else {
          _if__result_4613 = 0;
        }
      } else {
        _if__result_4613 = 0;
      }
    } else {
      _if__result_4613 = 0;
    }
  } else {
    _if__result_4613 = 0;
  }
  if (_if__result_4613) {
    int32_t _M0L16end__str__offsetS245 =
      _M0L11str__offsetS240 + _M0L6lengthS238;
    int32_t _M0L1iS246 = _M0L11str__offsetS240;
    int32_t _M0L1jS247 = _M0L13bytes__offsetS237;
    while (1) {
      if (_M0L1iS246 < _M0L16end__str__offsetS245) {
        int32_t _M0L6_2atmpS1925 = _M0L3strS244[_M0L1iS246];
        int32_t _M0L6_2atmpS1924 = (int32_t)_M0L6_2atmpS1925;
        uint32_t _M0L1cS248 = *(uint32_t*)&_M0L6_2atmpS1924;
        uint32_t _M0L6_2atmpS1920 = _M0L1cS248 & 255u;
        int32_t _M0L6_2atmpS1919;
        int32_t _M0L6_2atmpS1921;
        uint32_t _M0L6_2atmpS1923;
        int32_t _M0L6_2atmpS1922;
        int32_t _M0L6_2atmpS1926;
        int32_t _M0L6_2atmpS1927;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1919 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1920);
        if (
          _M0L1jS247 < 0 || _M0L1jS247 >= Moonbit_array_length(_M0L4selfS242)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS242[_M0L1jS247] = _M0L6_2atmpS1919;
        _M0L6_2atmpS1921 = _M0L1jS247 + 1;
        _M0L6_2atmpS1923 = _M0L1cS248 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1922 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1923);
        if (
          _M0L6_2atmpS1921 < 0
          || _M0L6_2atmpS1921 >= Moonbit_array_length(_M0L4selfS242)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS242[_M0L6_2atmpS1921] = _M0L6_2atmpS1922;
        _M0L6_2atmpS1926 = _M0L1iS246 + 1;
        _M0L6_2atmpS1927 = _M0L1jS247 + 2;
        _M0L1iS246 = _M0L6_2atmpS1926;
        _M0L1jS247 = _M0L6_2atmpS1927;
        continue;
      } else {
        moonbit_decref(_M0L3strS244);
        moonbit_decref(_M0L4selfS242);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS244);
    moonbit_decref(_M0L4selfS242);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS233,
  double _M0L3objS232
) {
  struct _M0TPB6Logger _M0L6_2atmpS1917;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1917
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS233
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS232, _M0L6_2atmpS1917);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS235,
  struct _M0TPC16string10StringView _M0L3objS234
) {
  struct _M0TPB6Logger _M0L6_2atmpS1918;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1918
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS235
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS234, _M0L6_2atmpS1918);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS178
) {
  int32_t _M0L6_2atmpS1916;
  struct _M0TPC16string10StringView _M0L7_2abindS177;
  moonbit_string_t _M0L7_2adataS179;
  int32_t _M0L8_2astartS180;
  int32_t _M0L6_2atmpS1915;
  int32_t _M0L6_2aendS181;
  int32_t _M0Lm9_2acursorS182;
  int32_t _M0Lm13accept__stateS183;
  int32_t _M0Lm10match__endS184;
  int32_t _M0Lm20match__tag__saver__0S185;
  int32_t _M0Lm20match__tag__saver__1S186;
  int32_t _M0Lm20match__tag__saver__2S187;
  int32_t _M0Lm20match__tag__saver__3S188;
  int32_t _M0Lm20match__tag__saver__4S189;
  int32_t _M0Lm6tag__0S190;
  int32_t _M0Lm6tag__1S191;
  int32_t _M0Lm9tag__1__1S192;
  int32_t _M0Lm9tag__1__2S193;
  int32_t _M0Lm6tag__3S194;
  int32_t _M0Lm6tag__2S195;
  int32_t _M0Lm9tag__2__1S196;
  int32_t _M0Lm6tag__4S197;
  int32_t _M0L6_2atmpS1873;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1916 = Moonbit_array_length(_M0L4reprS178);
  _M0L7_2abindS177
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1916, _M0L4reprS178
  };
  moonbit_incref(_M0L7_2abindS177.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS179 = _M0MPC16string10StringView4data(_M0L7_2abindS177);
  moonbit_incref(_M0L7_2abindS177.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS180
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS177);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1915 = _M0MPC16string10StringView6length(_M0L7_2abindS177);
  _M0L6_2aendS181 = _M0L8_2astartS180 + _M0L6_2atmpS1915;
  _M0Lm9_2acursorS182 = _M0L8_2astartS180;
  _M0Lm13accept__stateS183 = -1;
  _M0Lm10match__endS184 = -1;
  _M0Lm20match__tag__saver__0S185 = -1;
  _M0Lm20match__tag__saver__1S186 = -1;
  _M0Lm20match__tag__saver__2S187 = -1;
  _M0Lm20match__tag__saver__3S188 = -1;
  _M0Lm20match__tag__saver__4S189 = -1;
  _M0Lm6tag__0S190 = -1;
  _M0Lm6tag__1S191 = -1;
  _M0Lm9tag__1__1S192 = -1;
  _M0Lm9tag__1__2S193 = -1;
  _M0Lm6tag__3S194 = -1;
  _M0Lm6tag__2S195 = -1;
  _M0Lm9tag__2__1S196 = -1;
  _M0Lm6tag__4S197 = -1;
  _M0L6_2atmpS1873 = _M0Lm9_2acursorS182;
  if (_M0L6_2atmpS1873 < _M0L6_2aendS181) {
    int32_t _M0L6_2atmpS1875 = _M0Lm9_2acursorS182;
    int32_t _M0L6_2atmpS1874;
    moonbit_incref(_M0L7_2adataS179);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1874
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1875);
    if (_M0L6_2atmpS1874 == 64) {
      int32_t _M0L6_2atmpS1876 = _M0Lm9_2acursorS182;
      _M0Lm9_2acursorS182 = _M0L6_2atmpS1876 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1877;
        _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
        _M0L6_2atmpS1877 = _M0Lm9_2acursorS182;
        if (_M0L6_2atmpS1877 < _M0L6_2aendS181) {
          int32_t _M0L6_2atmpS1914 = _M0Lm9_2acursorS182;
          int32_t _M0L10next__charS205;
          int32_t _M0L6_2atmpS1878;
          moonbit_incref(_M0L7_2adataS179);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS205
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1914);
          _M0L6_2atmpS1878 = _M0Lm9_2acursorS182;
          _M0Lm9_2acursorS182 = _M0L6_2atmpS1878 + 1;
          if (_M0L10next__charS205 == 58) {
            int32_t _M0L6_2atmpS1879 = _M0Lm9_2acursorS182;
            if (_M0L6_2atmpS1879 < _M0L6_2aendS181) {
              int32_t _M0L6_2atmpS1880 = _M0Lm9_2acursorS182;
              int32_t _M0L12dispatch__15S206;
              _M0Lm9_2acursorS182 = _M0L6_2atmpS1880 + 1;
              _M0L12dispatch__15S206 = 0;
              loop__label__15_209:;
              while (1) {
                int32_t _M0L6_2atmpS1881;
                switch (_M0L12dispatch__15S206) {
                  case 3: {
                    int32_t _M0L6_2atmpS1884;
                    _M0Lm9tag__1__2S193 = _M0Lm9tag__1__1S192;
                    _M0Lm9tag__1__1S192 = _M0Lm6tag__1S191;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1884 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1884 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1889 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS213;
                      int32_t _M0L6_2atmpS1885;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS213
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1889);
                      _M0L6_2atmpS1885 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1885 + 1;
                      if (_M0L10next__charS213 < 58) {
                        if (_M0L10next__charS213 < 48) {
                          goto join_212;
                        } else {
                          int32_t _M0L6_2atmpS1886;
                          _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                          _M0Lm9tag__2__1S196 = _M0Lm6tag__2S195;
                          _M0Lm6tag__2S195 = _M0Lm9_2acursorS182;
                          _M0Lm6tag__3S194 = _M0Lm9_2acursorS182;
                          _M0L6_2atmpS1886 = _M0Lm9_2acursorS182;
                          if (_M0L6_2atmpS1886 < _M0L6_2aendS181) {
                            int32_t _M0L6_2atmpS1888 = _M0Lm9_2acursorS182;
                            int32_t _M0L10next__charS215;
                            int32_t _M0L6_2atmpS1887;
                            moonbit_incref(_M0L7_2adataS179);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS215
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1888);
                            _M0L6_2atmpS1887 = _M0Lm9_2acursorS182;
                            _M0Lm9_2acursorS182 = _M0L6_2atmpS1887 + 1;
                            if (_M0L10next__charS215 < 48) {
                              if (_M0L10next__charS215 == 45) {
                                goto join_207;
                              } else {
                                goto join_214;
                              }
                            } else if (_M0L10next__charS215 > 57) {
                              if (_M0L10next__charS215 < 59) {
                                _M0L12dispatch__15S206 = 3;
                                goto loop__label__15_209;
                              } else {
                                goto join_214;
                              }
                            } else {
                              _M0L12dispatch__15S206 = 6;
                              goto loop__label__15_209;
                            }
                            join_214:;
                            _M0L12dispatch__15S206 = 0;
                            goto loop__label__15_209;
                          } else {
                            goto join_198;
                          }
                        }
                      } else if (_M0L10next__charS213 > 58) {
                        goto join_212;
                      } else {
                        _M0L12dispatch__15S206 = 1;
                        goto loop__label__15_209;
                      }
                      join_212:;
                      _M0L12dispatch__15S206 = 0;
                      goto loop__label__15_209;
                    } else {
                      goto join_198;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1890;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                    _M0Lm6tag__2S195 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1890 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1890 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1892 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS217;
                      int32_t _M0L6_2atmpS1891;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS217
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1892);
                      _M0L6_2atmpS1891 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1891 + 1;
                      if (_M0L10next__charS217 < 58) {
                        if (_M0L10next__charS217 < 48) {
                          goto join_216;
                        } else {
                          _M0L12dispatch__15S206 = 2;
                          goto loop__label__15_209;
                        }
                      } else if (_M0L10next__charS217 > 58) {
                        goto join_216;
                      } else {
                        _M0L12dispatch__15S206 = 3;
                        goto loop__label__15_209;
                      }
                      join_216:;
                      _M0L12dispatch__15S206 = 0;
                      goto loop__label__15_209;
                    } else {
                      goto join_198;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1893;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1893 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1893 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1895 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS218;
                      int32_t _M0L6_2atmpS1894;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS218
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1895);
                      _M0L6_2atmpS1894 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1894 + 1;
                      if (_M0L10next__charS218 == 58) {
                        _M0L12dispatch__15S206 = 1;
                        goto loop__label__15_209;
                      } else {
                        _M0L12dispatch__15S206 = 0;
                        goto loop__label__15_209;
                      }
                    } else {
                      goto join_198;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1896;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                    _M0Lm6tag__4S197 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1896 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1896 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1904 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS220;
                      int32_t _M0L6_2atmpS1897;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS220
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1904);
                      _M0L6_2atmpS1897 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1897 + 1;
                      if (_M0L10next__charS220 < 58) {
                        if (_M0L10next__charS220 < 48) {
                          goto join_219;
                        } else {
                          _M0L12dispatch__15S206 = 4;
                          goto loop__label__15_209;
                        }
                      } else if (_M0L10next__charS220 > 58) {
                        goto join_219;
                      } else {
                        int32_t _M0L6_2atmpS1898;
                        _M0Lm9tag__1__2S193 = _M0Lm9tag__1__1S192;
                        _M0Lm9tag__1__1S192 = _M0Lm6tag__1S191;
                        _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                        _M0L6_2atmpS1898 = _M0Lm9_2acursorS182;
                        if (_M0L6_2atmpS1898 < _M0L6_2aendS181) {
                          int32_t _M0L6_2atmpS1903 = _M0Lm9_2acursorS182;
                          int32_t _M0L10next__charS222;
                          int32_t _M0L6_2atmpS1899;
                          moonbit_incref(_M0L7_2adataS179);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS222
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1903);
                          _M0L6_2atmpS1899 = _M0Lm9_2acursorS182;
                          _M0Lm9_2acursorS182 = _M0L6_2atmpS1899 + 1;
                          if (_M0L10next__charS222 < 58) {
                            if (_M0L10next__charS222 < 48) {
                              goto join_221;
                            } else {
                              int32_t _M0L6_2atmpS1900;
                              _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                              _M0Lm9tag__2__1S196 = _M0Lm6tag__2S195;
                              _M0Lm6tag__2S195 = _M0Lm9_2acursorS182;
                              _M0L6_2atmpS1900 = _M0Lm9_2acursorS182;
                              if (_M0L6_2atmpS1900 < _M0L6_2aendS181) {
                                int32_t _M0L6_2atmpS1902 =
                                  _M0Lm9_2acursorS182;
                                int32_t _M0L10next__charS224;
                                int32_t _M0L6_2atmpS1901;
                                moonbit_incref(_M0L7_2adataS179);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS224
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1902);
                                _M0L6_2atmpS1901 = _M0Lm9_2acursorS182;
                                _M0Lm9_2acursorS182 = _M0L6_2atmpS1901 + 1;
                                if (_M0L10next__charS224 < 58) {
                                  if (_M0L10next__charS224 < 48) {
                                    goto join_223;
                                  } else {
                                    _M0L12dispatch__15S206 = 5;
                                    goto loop__label__15_209;
                                  }
                                } else if (_M0L10next__charS224 > 58) {
                                  goto join_223;
                                } else {
                                  _M0L12dispatch__15S206 = 3;
                                  goto loop__label__15_209;
                                }
                                join_223:;
                                _M0L12dispatch__15S206 = 0;
                                goto loop__label__15_209;
                              } else {
                                goto join_211;
                              }
                            }
                          } else if (_M0L10next__charS222 > 58) {
                            goto join_221;
                          } else {
                            _M0L12dispatch__15S206 = 1;
                            goto loop__label__15_209;
                          }
                          join_221:;
                          _M0L12dispatch__15S206 = 0;
                          goto loop__label__15_209;
                        } else {
                          goto join_198;
                        }
                      }
                      join_219:;
                      _M0L12dispatch__15S206 = 0;
                      goto loop__label__15_209;
                    } else {
                      goto join_198;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1905;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                    _M0Lm6tag__2S195 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1905 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1905 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1907 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS226;
                      int32_t _M0L6_2atmpS1906;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS226
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1907);
                      _M0L6_2atmpS1906 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1906 + 1;
                      if (_M0L10next__charS226 < 58) {
                        if (_M0L10next__charS226 < 48) {
                          goto join_225;
                        } else {
                          _M0L12dispatch__15S206 = 5;
                          goto loop__label__15_209;
                        }
                      } else if (_M0L10next__charS226 > 58) {
                        goto join_225;
                      } else {
                        _M0L12dispatch__15S206 = 3;
                        goto loop__label__15_209;
                      }
                      join_225:;
                      _M0L12dispatch__15S206 = 0;
                      goto loop__label__15_209;
                    } else {
                      goto join_211;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1908;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                    _M0Lm6tag__2S195 = _M0Lm9_2acursorS182;
                    _M0Lm6tag__3S194 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1908 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1908 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1910 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS228;
                      int32_t _M0L6_2atmpS1909;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS228
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1910);
                      _M0L6_2atmpS1909 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1909 + 1;
                      if (_M0L10next__charS228 < 48) {
                        if (_M0L10next__charS228 == 45) {
                          goto join_207;
                        } else {
                          goto join_227;
                        }
                      } else if (_M0L10next__charS228 > 57) {
                        if (_M0L10next__charS228 < 59) {
                          _M0L12dispatch__15S206 = 3;
                          goto loop__label__15_209;
                        } else {
                          goto join_227;
                        }
                      } else {
                        _M0L12dispatch__15S206 = 6;
                        goto loop__label__15_209;
                      }
                      join_227:;
                      _M0L12dispatch__15S206 = 0;
                      goto loop__label__15_209;
                    } else {
                      goto join_198;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1911;
                    _M0Lm9tag__1__1S192 = _M0Lm6tag__1S191;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1911 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1911 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1913 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS230;
                      int32_t _M0L6_2atmpS1912;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS230
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1913);
                      _M0L6_2atmpS1912 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1912 + 1;
                      if (_M0L10next__charS230 < 58) {
                        if (_M0L10next__charS230 < 48) {
                          goto join_229;
                        } else {
                          _M0L12dispatch__15S206 = 2;
                          goto loop__label__15_209;
                        }
                      } else if (_M0L10next__charS230 > 58) {
                        goto join_229;
                      } else {
                        _M0L12dispatch__15S206 = 1;
                        goto loop__label__15_209;
                      }
                      join_229:;
                      _M0L12dispatch__15S206 = 0;
                      goto loop__label__15_209;
                    } else {
                      goto join_198;
                    }
                    break;
                  }
                  default: {
                    goto join_198;
                    break;
                  }
                }
                join_211:;
                _M0Lm6tag__1S191 = _M0Lm9tag__1__2S193;
                _M0Lm6tag__2S195 = _M0Lm9tag__2__1S196;
                _M0Lm20match__tag__saver__0S185 = _M0Lm6tag__0S190;
                _M0Lm20match__tag__saver__1S186 = _M0Lm6tag__1S191;
                _M0Lm20match__tag__saver__2S187 = _M0Lm6tag__2S195;
                _M0Lm20match__tag__saver__3S188 = _M0Lm6tag__3S194;
                _M0Lm20match__tag__saver__4S189 = _M0Lm6tag__4S197;
                _M0Lm13accept__stateS183 = 0;
                _M0Lm10match__endS184 = _M0Lm9_2acursorS182;
                goto join_198;
                join_207:;
                _M0Lm9tag__1__1S192 = _M0Lm9tag__1__2S193;
                _M0Lm6tag__1S191 = _M0Lm9_2acursorS182;
                _M0Lm6tag__2S195 = _M0Lm9tag__2__1S196;
                _M0L6_2atmpS1881 = _M0Lm9_2acursorS182;
                if (_M0L6_2atmpS1881 < _M0L6_2aendS181) {
                  int32_t _M0L6_2atmpS1883 = _M0Lm9_2acursorS182;
                  int32_t _M0L10next__charS210;
                  int32_t _M0L6_2atmpS1882;
                  moonbit_incref(_M0L7_2adataS179);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS210
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1883);
                  _M0L6_2atmpS1882 = _M0Lm9_2acursorS182;
                  _M0Lm9_2acursorS182 = _M0L6_2atmpS1882 + 1;
                  if (_M0L10next__charS210 < 58) {
                    if (_M0L10next__charS210 < 48) {
                      goto join_208;
                    } else {
                      _M0L12dispatch__15S206 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS210 > 58) {
                    goto join_208;
                  } else {
                    _M0L12dispatch__15S206 = 1;
                    continue;
                  }
                  join_208:;
                  _M0L12dispatch__15S206 = 0;
                  continue;
                } else {
                  goto join_198;
                }
                break;
              }
            } else {
              goto join_198;
            }
          } else {
            continue;
          }
        } else {
          goto join_198;
        }
        break;
      }
    } else {
      goto join_198;
    }
  } else {
    goto join_198;
  }
  join_198:;
  switch (_M0Lm13accept__stateS183) {
    case 0: {
      int32_t _M0L6_2atmpS1872 = _M0Lm20match__tag__saver__1S186;
      int32_t _M0L6_2atmpS1871 = _M0L6_2atmpS1872 + 1;
      int64_t _M0L6_2atmpS1868 = (int64_t)_M0L6_2atmpS1871;
      int32_t _M0L6_2atmpS1870 = _M0Lm20match__tag__saver__2S187;
      int64_t _M0L6_2atmpS1869 = (int64_t)_M0L6_2atmpS1870;
      struct _M0TPC16string10StringView _M0L11start__lineS199;
      int32_t _M0L6_2atmpS1867;
      int32_t _M0L6_2atmpS1866;
      int64_t _M0L6_2atmpS1863;
      int32_t _M0L6_2atmpS1865;
      int64_t _M0L6_2atmpS1864;
      struct _M0TPC16string10StringView _M0L13start__columnS200;
      int32_t _M0L6_2atmpS1862;
      int64_t _M0L6_2atmpS1859;
      int32_t _M0L6_2atmpS1861;
      int64_t _M0L6_2atmpS1860;
      struct _M0TPC16string10StringView _M0L3pkgS201;
      int32_t _M0L6_2atmpS1858;
      int32_t _M0L6_2atmpS1857;
      int64_t _M0L6_2atmpS1854;
      int32_t _M0L6_2atmpS1856;
      int64_t _M0L6_2atmpS1855;
      struct _M0TPC16string10StringView _M0L8filenameS202;
      int32_t _M0L6_2atmpS1853;
      int32_t _M0L6_2atmpS1852;
      int64_t _M0L6_2atmpS1849;
      int32_t _M0L6_2atmpS1851;
      int64_t _M0L6_2atmpS1850;
      struct _M0TPC16string10StringView _M0L9end__lineS203;
      int32_t _M0L6_2atmpS1848;
      int32_t _M0L6_2atmpS1847;
      int64_t _M0L6_2atmpS1844;
      int32_t _M0L6_2atmpS1846;
      int64_t _M0L6_2atmpS1845;
      struct _M0TPC16string10StringView _M0L11end__columnS204;
      struct _M0TPB13SourceLocRepr* _block_4630;
      moonbit_incref(_M0L7_2adataS179);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS199
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1868, _M0L6_2atmpS1869);
      _M0L6_2atmpS1867 = _M0Lm20match__tag__saver__2S187;
      _M0L6_2atmpS1866 = _M0L6_2atmpS1867 + 1;
      _M0L6_2atmpS1863 = (int64_t)_M0L6_2atmpS1866;
      _M0L6_2atmpS1865 = _M0Lm20match__tag__saver__3S188;
      _M0L6_2atmpS1864 = (int64_t)_M0L6_2atmpS1865;
      moonbit_incref(_M0L7_2adataS179);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS200
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1863, _M0L6_2atmpS1864);
      _M0L6_2atmpS1862 = _M0L8_2astartS180 + 1;
      _M0L6_2atmpS1859 = (int64_t)_M0L6_2atmpS1862;
      _M0L6_2atmpS1861 = _M0Lm20match__tag__saver__0S185;
      _M0L6_2atmpS1860 = (int64_t)_M0L6_2atmpS1861;
      moonbit_incref(_M0L7_2adataS179);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS201
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1859, _M0L6_2atmpS1860);
      _M0L6_2atmpS1858 = _M0Lm20match__tag__saver__0S185;
      _M0L6_2atmpS1857 = _M0L6_2atmpS1858 + 1;
      _M0L6_2atmpS1854 = (int64_t)_M0L6_2atmpS1857;
      _M0L6_2atmpS1856 = _M0Lm20match__tag__saver__1S186;
      _M0L6_2atmpS1855 = (int64_t)_M0L6_2atmpS1856;
      moonbit_incref(_M0L7_2adataS179);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS202
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1854, _M0L6_2atmpS1855);
      _M0L6_2atmpS1853 = _M0Lm20match__tag__saver__3S188;
      _M0L6_2atmpS1852 = _M0L6_2atmpS1853 + 1;
      _M0L6_2atmpS1849 = (int64_t)_M0L6_2atmpS1852;
      _M0L6_2atmpS1851 = _M0Lm20match__tag__saver__4S189;
      _M0L6_2atmpS1850 = (int64_t)_M0L6_2atmpS1851;
      moonbit_incref(_M0L7_2adataS179);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS203
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1849, _M0L6_2atmpS1850);
      _M0L6_2atmpS1848 = _M0Lm20match__tag__saver__4S189;
      _M0L6_2atmpS1847 = _M0L6_2atmpS1848 + 1;
      _M0L6_2atmpS1844 = (int64_t)_M0L6_2atmpS1847;
      _M0L6_2atmpS1846 = _M0Lm10match__endS184;
      _M0L6_2atmpS1845 = (int64_t)_M0L6_2atmpS1846;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS204
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1844, _M0L6_2atmpS1845);
      _block_4630
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4630)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4630->$0_0 = _M0L3pkgS201.$0;
      _block_4630->$0_1 = _M0L3pkgS201.$1;
      _block_4630->$0_2 = _M0L3pkgS201.$2;
      _block_4630->$1_0 = _M0L8filenameS202.$0;
      _block_4630->$1_1 = _M0L8filenameS202.$1;
      _block_4630->$1_2 = _M0L8filenameS202.$2;
      _block_4630->$2_0 = _M0L11start__lineS199.$0;
      _block_4630->$2_1 = _M0L11start__lineS199.$1;
      _block_4630->$2_2 = _M0L11start__lineS199.$2;
      _block_4630->$3_0 = _M0L13start__columnS200.$0;
      _block_4630->$3_1 = _M0L13start__columnS200.$1;
      _block_4630->$3_2 = _M0L13start__columnS200.$2;
      _block_4630->$4_0 = _M0L9end__lineS203.$0;
      _block_4630->$4_1 = _M0L9end__lineS203.$1;
      _block_4630->$4_2 = _M0L9end__lineS203.$2;
      _block_4630->$5_0 = _M0L11end__columnS204.$0;
      _block_4630->$5_1 = _M0L11end__columnS204.$1;
      _block_4630->$5_2 = _M0L11end__columnS204.$2;
      return _block_4630;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS179);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS172,
  int32_t _M0L5indexS173
) {
  int32_t _M0L3lenS171;
  int32_t _if__result_4631;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS171 = _M0L4selfS172->$1;
  if (_M0L5indexS173 >= 0) {
    _if__result_4631 = _M0L5indexS173 < _M0L3lenS171;
  } else {
    _if__result_4631 = 0;
  }
  if (_if__result_4631) {
    moonbit_string_t* _M0L6_2atmpS1842;
    moonbit_string_t _M0L6_2atmpS4061;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1842 = _M0MPC15array5Array6bufferGsE(_M0L4selfS172);
    if (
      _M0L5indexS173 < 0
      || _M0L5indexS173 >= Moonbit_array_length(_M0L6_2atmpS1842)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4061 = (moonbit_string_t)_M0L6_2atmpS1842[_M0L5indexS173];
    moonbit_incref(_M0L6_2atmpS4061);
    moonbit_decref(_M0L6_2atmpS1842);
    return _M0L6_2atmpS4061;
  } else {
    moonbit_decref(_M0L4selfS172);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array5Array2atGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE* _M0L4selfS175,
  int32_t _M0L5indexS176
) {
  int32_t _M0L3lenS174;
  int32_t _if__result_4632;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS174 = _M0L4selfS175->$1;
  if (_M0L5indexS176 >= 0) {
    _if__result_4632 = _M0L5indexS176 < _M0L3lenS174;
  } else {
    _if__result_4632 = 0;
  }
  if (_if__result_4632) {
    int32_t* _M0L6_2atmpS1843;
    int32_t _M0L6_2atmpS4062;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1843
    = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal5httpx6MethodE(_M0L4selfS175);
    if (
      _M0L5indexS176 < 0
      || _M0L5indexS176 >= Moonbit_array_length(_M0L6_2atmpS1843)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4062 = (int32_t)_M0L6_2atmpS1843[_M0L5indexS176];
    moonbit_decref(_M0L6_2atmpS1843);
    return _M0L6_2atmpS4062;
  } else {
    moonbit_decref(_M0L4selfS175);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array5Array6lengthGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE* _M0L4selfS170
) {
  int32_t _M0L8_2afieldS4063;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4063 = _M0L4selfS170->$1;
  moonbit_decref(_M0L4selfS170);
  return _M0L8_2afieldS4063;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS163
) {
  moonbit_string_t* _M0L8_2afieldS4064;
  int32_t _M0L6_2acntS4408;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4064 = _M0L4selfS163->$0;
  _M0L6_2acntS4408 = Moonbit_object_header(_M0L4selfS163)->rc;
  if (_M0L6_2acntS4408 > 1) {
    int32_t _M0L11_2anew__cntS4409 = _M0L6_2acntS4408 - 1;
    Moonbit_object_header(_M0L4selfS163)->rc = _M0L11_2anew__cntS4409;
    moonbit_incref(_M0L8_2afieldS4064);
  } else if (_M0L6_2acntS4408 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS163);
  }
  return _M0L8_2afieldS4064;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS164
) {
  struct _M0TUsiE** _M0L8_2afieldS4065;
  int32_t _M0L6_2acntS4410;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4065 = _M0L4selfS164->$0;
  _M0L6_2acntS4410 = Moonbit_object_header(_M0L4selfS164)->rc;
  if (_M0L6_2acntS4410 > 1) {
    int32_t _M0L11_2anew__cntS4411 = _M0L6_2acntS4410 - 1;
    Moonbit_object_header(_M0L4selfS164)->rc = _M0L11_2anew__cntS4411;
    moonbit_incref(_M0L8_2afieldS4065);
  } else if (_M0L6_2acntS4410 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS164);
  }
  return _M0L8_2afieldS4065;
}

struct _M0TPC16string10StringView* _M0MPC15array5Array6bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS165
) {
  struct _M0TPC16string10StringView* _M0L8_2afieldS4066;
  int32_t _M0L6_2acntS4412;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4066 = _M0L4selfS165->$0;
  _M0L6_2acntS4412 = Moonbit_object_header(_M0L4selfS165)->rc;
  if (_M0L6_2acntS4412 > 1) {
    int32_t _M0L11_2anew__cntS4413 = _M0L6_2acntS4412 - 1;
    Moonbit_object_header(_M0L4selfS165)->rc = _M0L11_2anew__cntS4413;
    moonbit_incref(_M0L8_2afieldS4066);
  } else if (_M0L6_2acntS4412 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS165);
  }
  return _M0L8_2afieldS4066;
}

int32_t* _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal5httpx6MethodE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE* _M0L4selfS166
) {
  int32_t* _M0L8_2afieldS4067;
  int32_t _M0L6_2acntS4414;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4067 = _M0L4selfS166->$0;
  _M0L6_2acntS4414 = Moonbit_object_header(_M0L4selfS166)->rc;
  if (_M0L6_2acntS4414 > 1) {
    int32_t _M0L11_2anew__cntS4415 = _M0L6_2acntS4414 - 1;
    Moonbit_object_header(_M0L4selfS166)->rc = _M0L11_2anew__cntS4415;
    moonbit_incref(_M0L8_2afieldS4067);
  } else if (_M0L6_2acntS4414 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS166);
  }
  return _M0L8_2afieldS4067;
}

struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0MPC15array5Array6bufferGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TPB5ArrayGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE* _M0L4selfS167
) {
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L8_2afieldS4068;
  int32_t _M0L6_2acntS4416;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4068 = _M0L4selfS167->$0;
  _M0L6_2acntS4416 = Moonbit_object_header(_M0L4selfS167)->rc;
  if (_M0L6_2acntS4416 > 1) {
    int32_t _M0L11_2anew__cntS4417 = _M0L6_2acntS4416 - 1;
    Moonbit_object_header(_M0L4selfS167)->rc = _M0L11_2anew__cntS4417;
    moonbit_incref(_M0L8_2afieldS4068);
  } else if (_M0L6_2acntS4416 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS167);
  }
  return _M0L8_2afieldS4068;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS168
) {
  void** _M0L8_2afieldS4069;
  int32_t _M0L6_2acntS4418;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4069 = _M0L4selfS168->$0;
  _M0L6_2acntS4418 = Moonbit_object_header(_M0L4selfS168)->rc;
  if (_M0L6_2acntS4418 > 1) {
    int32_t _M0L11_2anew__cntS4419 = _M0L6_2acntS4418 - 1;
    Moonbit_object_header(_M0L4selfS168)->rc = _M0L11_2anew__cntS4419;
    moonbit_incref(_M0L8_2afieldS4069);
  } else if (_M0L6_2acntS4418 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS168);
  }
  return _M0L8_2afieldS4069;
}

void** _M0MPC15array5Array6bufferGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS169
) {
  void** _M0L8_2afieldS4070;
  int32_t _M0L6_2acntS4420;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4070 = _M0L4selfS169->$0;
  _M0L6_2acntS4420 = Moonbit_object_header(_M0L4selfS169)->rc;
  if (_M0L6_2acntS4420 > 1) {
    int32_t _M0L11_2anew__cntS4421 = _M0L6_2acntS4420 - 1;
    Moonbit_object_header(_M0L4selfS169)->rc = _M0L11_2anew__cntS4421;
    moonbit_incref(_M0L8_2afieldS4070);
  } else if (_M0L6_2acntS4420 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS169);
  }
  return _M0L8_2afieldS4070;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS162) {
  struct _M0TPB13StringBuilder* _M0L3bufS161;
  struct _M0TPB6Logger _M0L6_2atmpS1841;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS161 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS161);
  _M0L6_2atmpS1841
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS161
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS162, _M0L6_2atmpS1841);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS161);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS158,
  int32_t _M0L5indexS159
) {
  int32_t _M0L2c1S157;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S157 = _M0L4selfS158[_M0L5indexS159];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S157)) {
    int32_t _M0L6_2atmpS1840 = _M0L5indexS159 + 1;
    int32_t _M0L6_2atmpS4071 = _M0L4selfS158[_M0L6_2atmpS1840];
    int32_t _M0L2c2S160;
    int32_t _M0L6_2atmpS1838;
    int32_t _M0L6_2atmpS1839;
    moonbit_decref(_M0L4selfS158);
    _M0L2c2S160 = _M0L6_2atmpS4071;
    _M0L6_2atmpS1838 = (int32_t)_M0L2c1S157;
    _M0L6_2atmpS1839 = (int32_t)_M0L2c2S160;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1838, _M0L6_2atmpS1839);
  } else {
    moonbit_decref(_M0L4selfS158);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S157);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS156) {
  int32_t _M0L6_2atmpS1837;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1837 = (int32_t)_M0L4selfS156;
  return _M0L6_2atmpS1837;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS154,
  int32_t _M0L8trailingS155
) {
  int32_t _M0L6_2atmpS1836;
  int32_t _M0L6_2atmpS1835;
  int32_t _M0L6_2atmpS1834;
  int32_t _M0L6_2atmpS1833;
  int32_t _M0L6_2atmpS1832;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1836 = _M0L7leadingS154 - 55296;
  _M0L6_2atmpS1835 = _M0L6_2atmpS1836 * 1024;
  _M0L6_2atmpS1834 = _M0L6_2atmpS1835 + _M0L8trailingS155;
  _M0L6_2atmpS1833 = _M0L6_2atmpS1834 - 56320;
  _M0L6_2atmpS1832 = _M0L6_2atmpS1833 + 65536;
  return _M0L6_2atmpS1832;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS153) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS153 >= 56320) {
    return _M0L4selfS153 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS152) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS152 >= 55296) {
    return _M0L4selfS152 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS149,
  int32_t _M0L2chS151
) {
  int32_t _M0L3lenS1827;
  int32_t _M0L6_2atmpS1826;
  moonbit_bytes_t _M0L8_2afieldS4072;
  moonbit_bytes_t _M0L4dataS1830;
  int32_t _M0L3lenS1831;
  int32_t _M0L3incS150;
  int32_t _M0L3lenS1829;
  int32_t _M0L6_2atmpS1828;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1827 = _M0L4selfS149->$1;
  _M0L6_2atmpS1826 = _M0L3lenS1827 + 4;
  moonbit_incref(_M0L4selfS149);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS149, _M0L6_2atmpS1826);
  _M0L8_2afieldS4072 = _M0L4selfS149->$0;
  _M0L4dataS1830 = _M0L8_2afieldS4072;
  _M0L3lenS1831 = _M0L4selfS149->$1;
  moonbit_incref(_M0L4dataS1830);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS150
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1830, _M0L3lenS1831, _M0L2chS151);
  _M0L3lenS1829 = _M0L4selfS149->$1;
  _M0L6_2atmpS1828 = _M0L3lenS1829 + _M0L3incS150;
  _M0L4selfS149->$1 = _M0L6_2atmpS1828;
  moonbit_decref(_M0L4selfS149);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS144,
  int32_t _M0L8requiredS145
) {
  moonbit_bytes_t _M0L8_2afieldS4076;
  moonbit_bytes_t _M0L4dataS1825;
  int32_t _M0L6_2atmpS4075;
  int32_t _M0L12current__lenS143;
  int32_t _M0Lm13enough__spaceS146;
  int32_t _M0L6_2atmpS1823;
  int32_t _M0L6_2atmpS1824;
  moonbit_bytes_t _M0L9new__dataS148;
  moonbit_bytes_t _M0L8_2afieldS4074;
  moonbit_bytes_t _M0L4dataS1821;
  int32_t _M0L3lenS1822;
  moonbit_bytes_t _M0L6_2aoldS4073;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4076 = _M0L4selfS144->$0;
  _M0L4dataS1825 = _M0L8_2afieldS4076;
  _M0L6_2atmpS4075 = Moonbit_array_length(_M0L4dataS1825);
  _M0L12current__lenS143 = _M0L6_2atmpS4075;
  if (_M0L8requiredS145 <= _M0L12current__lenS143) {
    moonbit_decref(_M0L4selfS144);
    return 0;
  }
  _M0Lm13enough__spaceS146 = _M0L12current__lenS143;
  while (1) {
    int32_t _M0L6_2atmpS1819 = _M0Lm13enough__spaceS146;
    if (_M0L6_2atmpS1819 < _M0L8requiredS145) {
      int32_t _M0L6_2atmpS1820 = _M0Lm13enough__spaceS146;
      _M0Lm13enough__spaceS146 = _M0L6_2atmpS1820 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1823 = _M0Lm13enough__spaceS146;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1824 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS148
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1823, _M0L6_2atmpS1824);
  _M0L8_2afieldS4074 = _M0L4selfS144->$0;
  _M0L4dataS1821 = _M0L8_2afieldS4074;
  _M0L3lenS1822 = _M0L4selfS144->$1;
  moonbit_incref(_M0L4dataS1821);
  moonbit_incref(_M0L9new__dataS148);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS148, 0, _M0L4dataS1821, 0, _M0L3lenS1822);
  _M0L6_2aoldS4073 = _M0L4selfS144->$0;
  moonbit_decref(_M0L6_2aoldS4073);
  _M0L4selfS144->$0 = _M0L9new__dataS148;
  moonbit_decref(_M0L4selfS144);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS138,
  int32_t _M0L6offsetS139,
  int32_t _M0L5valueS137
) {
  uint32_t _M0L4codeS136;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS136 = _M0MPC14char4Char8to__uint(_M0L5valueS137);
  if (_M0L4codeS136 < 65536u) {
    uint32_t _M0L6_2atmpS1802 = _M0L4codeS136 & 255u;
    int32_t _M0L6_2atmpS1801;
    int32_t _M0L6_2atmpS1803;
    uint32_t _M0L6_2atmpS1805;
    int32_t _M0L6_2atmpS1804;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1801 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1802);
    if (
      _M0L6offsetS139 < 0
      || _M0L6offsetS139 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6offsetS139] = _M0L6_2atmpS1801;
    _M0L6_2atmpS1803 = _M0L6offsetS139 + 1;
    _M0L6_2atmpS1805 = _M0L4codeS136 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1804 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1805);
    if (
      _M0L6_2atmpS1803 < 0
      || _M0L6_2atmpS1803 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1803] = _M0L6_2atmpS1804;
    moonbit_decref(_M0L4selfS138);
    return 2;
  } else if (_M0L4codeS136 < 1114112u) {
    uint32_t _M0L2hiS140 = _M0L4codeS136 - 65536u;
    uint32_t _M0L6_2atmpS1818 = _M0L2hiS140 >> 10;
    uint32_t _M0L2loS141 = _M0L6_2atmpS1818 | 55296u;
    uint32_t _M0L6_2atmpS1817 = _M0L2hiS140 & 1023u;
    uint32_t _M0L2hiS142 = _M0L6_2atmpS1817 | 56320u;
    uint32_t _M0L6_2atmpS1807 = _M0L2loS141 & 255u;
    int32_t _M0L6_2atmpS1806;
    int32_t _M0L6_2atmpS1808;
    uint32_t _M0L6_2atmpS1810;
    int32_t _M0L6_2atmpS1809;
    int32_t _M0L6_2atmpS1811;
    uint32_t _M0L6_2atmpS1813;
    int32_t _M0L6_2atmpS1812;
    int32_t _M0L6_2atmpS1814;
    uint32_t _M0L6_2atmpS1816;
    int32_t _M0L6_2atmpS1815;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1806 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1807);
    if (
      _M0L6offsetS139 < 0
      || _M0L6offsetS139 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6offsetS139] = _M0L6_2atmpS1806;
    _M0L6_2atmpS1808 = _M0L6offsetS139 + 1;
    _M0L6_2atmpS1810 = _M0L2loS141 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1809 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1810);
    if (
      _M0L6_2atmpS1808 < 0
      || _M0L6_2atmpS1808 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1808] = _M0L6_2atmpS1809;
    _M0L6_2atmpS1811 = _M0L6offsetS139 + 2;
    _M0L6_2atmpS1813 = _M0L2hiS142 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1812 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1813);
    if (
      _M0L6_2atmpS1811 < 0
      || _M0L6_2atmpS1811 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1811] = _M0L6_2atmpS1812;
    _M0L6_2atmpS1814 = _M0L6offsetS139 + 3;
    _M0L6_2atmpS1816 = _M0L2hiS142 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1815 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1816);
    if (
      _M0L6_2atmpS1814 < 0
      || _M0L6_2atmpS1814 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1814] = _M0L6_2atmpS1815;
    moonbit_decref(_M0L4selfS138);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS138);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_118.data, (moonbit_string_t)moonbit_string_literal_119.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS135) {
  int32_t _M0L6_2atmpS1800;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1800 = *(int32_t*)&_M0L4selfS135;
  return _M0L6_2atmpS1800 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS134) {
  int32_t _M0L6_2atmpS1799;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1799 = _M0L4selfS134;
  return *(uint32_t*)&_M0L6_2atmpS1799;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS133
) {
  moonbit_bytes_t _M0L8_2afieldS4078;
  moonbit_bytes_t _M0L4dataS1798;
  moonbit_bytes_t _M0L6_2atmpS1795;
  int32_t _M0L8_2afieldS4077;
  int32_t _M0L3lenS1797;
  int64_t _M0L6_2atmpS1796;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4078 = _M0L4selfS133->$0;
  _M0L4dataS1798 = _M0L8_2afieldS4078;
  moonbit_incref(_M0L4dataS1798);
  _M0L6_2atmpS1795 = _M0L4dataS1798;
  _M0L8_2afieldS4077 = _M0L4selfS133->$1;
  moonbit_decref(_M0L4selfS133);
  _M0L3lenS1797 = _M0L8_2afieldS4077;
  _M0L6_2atmpS1796 = (int64_t)_M0L3lenS1797;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1795, 0, _M0L6_2atmpS1796);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS128,
  int32_t _M0L6offsetS132,
  int64_t _M0L6lengthS130
) {
  int32_t _M0L3lenS127;
  int32_t _M0L6lengthS129;
  int32_t _if__result_4634;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS127 = Moonbit_array_length(_M0L4selfS128);
  if (_M0L6lengthS130 == 4294967296ll) {
    _M0L6lengthS129 = _M0L3lenS127 - _M0L6offsetS132;
  } else {
    int64_t _M0L7_2aSomeS131 = _M0L6lengthS130;
    _M0L6lengthS129 = (int32_t)_M0L7_2aSomeS131;
  }
  if (_M0L6offsetS132 >= 0) {
    if (_M0L6lengthS129 >= 0) {
      int32_t _M0L6_2atmpS1794 = _M0L6offsetS132 + _M0L6lengthS129;
      _if__result_4634 = _M0L6_2atmpS1794 <= _M0L3lenS127;
    } else {
      _if__result_4634 = 0;
    }
  } else {
    _if__result_4634 = 0;
  }
  if (_if__result_4634) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS128, _M0L6offsetS132, _M0L6lengthS129);
  } else {
    moonbit_decref(_M0L4selfS128);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS125
) {
  int32_t _M0L7initialS124;
  moonbit_bytes_t _M0L4dataS126;
  struct _M0TPB13StringBuilder* _block_4635;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS125 < 1) {
    _M0L7initialS124 = 1;
  } else {
    _M0L7initialS124 = _M0L10size__hintS125;
  }
  _M0L4dataS126 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS124, 0);
  _block_4635
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4635)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4635->$0 = _M0L4dataS126;
  _block_4635->$1 = 0;
  return _block_4635;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS123) {
  int32_t _M0L6_2atmpS1793;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1793 = (int32_t)_M0L4selfS123;
  return _M0L6_2atmpS1793;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS93,
  int32_t _M0L11dst__offsetS94,
  moonbit_string_t* _M0L3srcS95,
  int32_t _M0L11src__offsetS96,
  int32_t _M0L3lenS97
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS93, _M0L11dst__offsetS94, _M0L3srcS95, _M0L11src__offsetS96, _M0L3lenS97);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS98,
  int32_t _M0L11dst__offsetS99,
  struct _M0TUsiE** _M0L3srcS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L3lenS102
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS98, _M0L11dst__offsetS99, _M0L3srcS100, _M0L11src__offsetS101, _M0L3lenS102);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(
  struct _M0TPC16string10StringView* _M0L3dstS103,
  int32_t _M0L11dst__offsetS104,
  struct _M0TPC16string10StringView* _M0L3srcS105,
  int32_t _M0L11src__offsetS106,
  int32_t _M0L3lenS107
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(_M0L3dstS103, _M0L11dst__offsetS104, _M0L3srcS105, _M0L11src__offsetS106, _M0L3lenS107);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEE(
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L3dstS108,
  int32_t _M0L11dst__offsetS109,
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L3srcS110,
  int32_t _M0L11src__offsetS111,
  int32_t _M0L3lenS112
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE(_M0L3dstS108, _M0L11dst__offsetS109, _M0L3srcS110, _M0L11src__offsetS111, _M0L3lenS112);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS113,
  int32_t _M0L11dst__offsetS114,
  void** _M0L3srcS115,
  int32_t _M0L11src__offsetS116,
  int32_t _M0L3lenS117
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS113, _M0L11dst__offsetS114, _M0L3srcS115, _M0L11src__offsetS116, _M0L3lenS117);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPB4JsonE(
  void** _M0L3dstS118,
  int32_t _M0L11dst__offsetS119,
  void** _M0L3srcS120,
  int32_t _M0L11src__offsetS121,
  int32_t _M0L3lenS122
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPB4JsonEE(_M0L3dstS118, _M0L11dst__offsetS119, _M0L3srcS120, _M0L11src__offsetS121, _M0L3lenS122);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS30,
  int32_t _M0L11dst__offsetS32,
  moonbit_bytes_t _M0L3srcS31,
  int32_t _M0L11src__offsetS33,
  int32_t _M0L3lenS35
) {
  int32_t _if__result_4636;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS30 == _M0L3srcS31) {
    _if__result_4636 = _M0L11dst__offsetS32 < _M0L11src__offsetS33;
  } else {
    _if__result_4636 = 0;
  }
  if (_if__result_4636) {
    int32_t _M0L1iS34 = 0;
    while (1) {
      if (_M0L1iS34 < _M0L3lenS35) {
        int32_t _M0L6_2atmpS1730 = _M0L11dst__offsetS32 + _M0L1iS34;
        int32_t _M0L6_2atmpS1732 = _M0L11src__offsetS33 + _M0L1iS34;
        int32_t _M0L6_2atmpS1731;
        int32_t _M0L6_2atmpS1733;
        if (
          _M0L6_2atmpS1732 < 0
          || _M0L6_2atmpS1732 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1731 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1732];
        if (
          _M0L6_2atmpS1730 < 0
          || _M0L6_2atmpS1730 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1730] = _M0L6_2atmpS1731;
        _M0L6_2atmpS1733 = _M0L1iS34 + 1;
        _M0L1iS34 = _M0L6_2atmpS1733;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1738 = _M0L3lenS35 - 1;
    int32_t _M0L1iS37 = _M0L6_2atmpS1738;
    while (1) {
      if (_M0L1iS37 >= 0) {
        int32_t _M0L6_2atmpS1734 = _M0L11dst__offsetS32 + _M0L1iS37;
        int32_t _M0L6_2atmpS1736 = _M0L11src__offsetS33 + _M0L1iS37;
        int32_t _M0L6_2atmpS1735;
        int32_t _M0L6_2atmpS1737;
        if (
          _M0L6_2atmpS1736 < 0
          || _M0L6_2atmpS1736 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1735 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1736];
        if (
          _M0L6_2atmpS1734 < 0
          || _M0L6_2atmpS1734 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1734] = _M0L6_2atmpS1735;
        _M0L6_2atmpS1737 = _M0L1iS37 - 1;
        _M0L1iS37 = _M0L6_2atmpS1737;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS39,
  int32_t _M0L11dst__offsetS41,
  moonbit_string_t* _M0L3srcS40,
  int32_t _M0L11src__offsetS42,
  int32_t _M0L3lenS44
) {
  int32_t _if__result_4639;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS39 == _M0L3srcS40) {
    _if__result_4639 = _M0L11dst__offsetS41 < _M0L11src__offsetS42;
  } else {
    _if__result_4639 = 0;
  }
  if (_if__result_4639) {
    int32_t _M0L1iS43 = 0;
    while (1) {
      if (_M0L1iS43 < _M0L3lenS44) {
        int32_t _M0L6_2atmpS1739 = _M0L11dst__offsetS41 + _M0L1iS43;
        int32_t _M0L6_2atmpS1741 = _M0L11src__offsetS42 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS4080;
        moonbit_string_t _M0L6_2atmpS1740;
        moonbit_string_t _M0L6_2aoldS4079;
        int32_t _M0L6_2atmpS1742;
        if (
          _M0L6_2atmpS1741 < 0
          || _M0L6_2atmpS1741 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4080 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1741];
        _M0L6_2atmpS1740 = _M0L6_2atmpS4080;
        if (
          _M0L6_2atmpS1739 < 0
          || _M0L6_2atmpS1739 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4079 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1739];
        moonbit_incref(_M0L6_2atmpS1740);
        moonbit_decref(_M0L6_2aoldS4079);
        _M0L3dstS39[_M0L6_2atmpS1739] = _M0L6_2atmpS1740;
        _M0L6_2atmpS1742 = _M0L1iS43 + 1;
        _M0L1iS43 = _M0L6_2atmpS1742;
        continue;
      } else {
        moonbit_decref(_M0L3srcS40);
        moonbit_decref(_M0L3dstS39);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1747 = _M0L3lenS44 - 1;
    int32_t _M0L1iS46 = _M0L6_2atmpS1747;
    while (1) {
      if (_M0L1iS46 >= 0) {
        int32_t _M0L6_2atmpS1743 = _M0L11dst__offsetS41 + _M0L1iS46;
        int32_t _M0L6_2atmpS1745 = _M0L11src__offsetS42 + _M0L1iS46;
        moonbit_string_t _M0L6_2atmpS4082;
        moonbit_string_t _M0L6_2atmpS1744;
        moonbit_string_t _M0L6_2aoldS4081;
        int32_t _M0L6_2atmpS1746;
        if (
          _M0L6_2atmpS1745 < 0
          || _M0L6_2atmpS1745 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4082 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1745];
        _M0L6_2atmpS1744 = _M0L6_2atmpS4082;
        if (
          _M0L6_2atmpS1743 < 0
          || _M0L6_2atmpS1743 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4081 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1743];
        moonbit_incref(_M0L6_2atmpS1744);
        moonbit_decref(_M0L6_2aoldS4081);
        _M0L3dstS39[_M0L6_2atmpS1743] = _M0L6_2atmpS1744;
        _M0L6_2atmpS1746 = _M0L1iS46 - 1;
        _M0L1iS46 = _M0L6_2atmpS1746;
        continue;
      } else {
        moonbit_decref(_M0L3srcS40);
        moonbit_decref(_M0L3dstS39);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS48,
  int32_t _M0L11dst__offsetS50,
  struct _M0TUsiE** _M0L3srcS49,
  int32_t _M0L11src__offsetS51,
  int32_t _M0L3lenS53
) {
  int32_t _if__result_4642;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS48 == _M0L3srcS49) {
    _if__result_4642 = _M0L11dst__offsetS50 < _M0L11src__offsetS51;
  } else {
    _if__result_4642 = 0;
  }
  if (_if__result_4642) {
    int32_t _M0L1iS52 = 0;
    while (1) {
      if (_M0L1iS52 < _M0L3lenS53) {
        int32_t _M0L6_2atmpS1748 = _M0L11dst__offsetS50 + _M0L1iS52;
        int32_t _M0L6_2atmpS1750 = _M0L11src__offsetS51 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS4084;
        struct _M0TUsiE* _M0L6_2atmpS1749;
        struct _M0TUsiE* _M0L6_2aoldS4083;
        int32_t _M0L6_2atmpS1751;
        if (
          _M0L6_2atmpS1750 < 0
          || _M0L6_2atmpS1750 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4084 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1750];
        _M0L6_2atmpS1749 = _M0L6_2atmpS4084;
        if (
          _M0L6_2atmpS1748 < 0
          || _M0L6_2atmpS1748 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4083 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1748];
        if (_M0L6_2atmpS1749) {
          moonbit_incref(_M0L6_2atmpS1749);
        }
        if (_M0L6_2aoldS4083) {
          moonbit_decref(_M0L6_2aoldS4083);
        }
        _M0L3dstS48[_M0L6_2atmpS1748] = _M0L6_2atmpS1749;
        _M0L6_2atmpS1751 = _M0L1iS52 + 1;
        _M0L1iS52 = _M0L6_2atmpS1751;
        continue;
      } else {
        moonbit_decref(_M0L3srcS49);
        moonbit_decref(_M0L3dstS48);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1756 = _M0L3lenS53 - 1;
    int32_t _M0L1iS55 = _M0L6_2atmpS1756;
    while (1) {
      if (_M0L1iS55 >= 0) {
        int32_t _M0L6_2atmpS1752 = _M0L11dst__offsetS50 + _M0L1iS55;
        int32_t _M0L6_2atmpS1754 = _M0L11src__offsetS51 + _M0L1iS55;
        struct _M0TUsiE* _M0L6_2atmpS4086;
        struct _M0TUsiE* _M0L6_2atmpS1753;
        struct _M0TUsiE* _M0L6_2aoldS4085;
        int32_t _M0L6_2atmpS1755;
        if (
          _M0L6_2atmpS1754 < 0
          || _M0L6_2atmpS1754 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4086 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1754];
        _M0L6_2atmpS1753 = _M0L6_2atmpS4086;
        if (
          _M0L6_2atmpS1752 < 0
          || _M0L6_2atmpS1752 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4085 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1752];
        if (_M0L6_2atmpS1753) {
          moonbit_incref(_M0L6_2atmpS1753);
        }
        if (_M0L6_2aoldS4085) {
          moonbit_decref(_M0L6_2aoldS4085);
        }
        _M0L3dstS48[_M0L6_2atmpS1752] = _M0L6_2atmpS1753;
        _M0L6_2atmpS1755 = _M0L1iS55 - 1;
        _M0L1iS55 = _M0L6_2atmpS1755;
        continue;
      } else {
        moonbit_decref(_M0L3srcS49);
        moonbit_decref(_M0L3dstS48);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(
  struct _M0TPC16string10StringView* _M0L3dstS57,
  int32_t _M0L11dst__offsetS59,
  struct _M0TPC16string10StringView* _M0L3srcS58,
  int32_t _M0L11src__offsetS60,
  int32_t _M0L3lenS62
) {
  int32_t _if__result_4645;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS57 == _M0L3srcS58) {
    _if__result_4645 = _M0L11dst__offsetS59 < _M0L11src__offsetS60;
  } else {
    _if__result_4645 = 0;
  }
  if (_if__result_4645) {
    int32_t _M0L1iS61 = 0;
    while (1) {
      if (_M0L1iS61 < _M0L3lenS62) {
        int32_t _M0L6_2atmpS1757 = _M0L11dst__offsetS59 + _M0L1iS61;
        int32_t _M0L6_2atmpS1759 = _M0L11src__offsetS60 + _M0L1iS61;
        struct _M0TPC16string10StringView _M0L6_2atmpS4088;
        struct _M0TPC16string10StringView _M0L6_2atmpS1758;
        struct _M0TPC16string10StringView _M0L6_2aoldS4087;
        int32_t _M0L6_2atmpS1760;
        if (
          _M0L6_2atmpS1759 < 0
          || _M0L6_2atmpS1759 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4088 = _M0L3srcS58[_M0L6_2atmpS1759];
        _M0L6_2atmpS1758 = _M0L6_2atmpS4088;
        if (
          _M0L6_2atmpS1757 < 0
          || _M0L6_2atmpS1757 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4087 = _M0L3dstS57[_M0L6_2atmpS1757];
        moonbit_incref(_M0L6_2atmpS1758.$0);
        moonbit_decref(_M0L6_2aoldS4087.$0);
        _M0L3dstS57[_M0L6_2atmpS1757] = _M0L6_2atmpS1758;
        _M0L6_2atmpS1760 = _M0L1iS61 + 1;
        _M0L1iS61 = _M0L6_2atmpS1760;
        continue;
      } else {
        moonbit_decref(_M0L3srcS58);
        moonbit_decref(_M0L3dstS57);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1765 = _M0L3lenS62 - 1;
    int32_t _M0L1iS64 = _M0L6_2atmpS1765;
    while (1) {
      if (_M0L1iS64 >= 0) {
        int32_t _M0L6_2atmpS1761 = _M0L11dst__offsetS59 + _M0L1iS64;
        int32_t _M0L6_2atmpS1763 = _M0L11src__offsetS60 + _M0L1iS64;
        struct _M0TPC16string10StringView _M0L6_2atmpS4090;
        struct _M0TPC16string10StringView _M0L6_2atmpS1762;
        struct _M0TPC16string10StringView _M0L6_2aoldS4089;
        int32_t _M0L6_2atmpS1764;
        if (
          _M0L6_2atmpS1763 < 0
          || _M0L6_2atmpS1763 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4090 = _M0L3srcS58[_M0L6_2atmpS1763];
        _M0L6_2atmpS1762 = _M0L6_2atmpS4090;
        if (
          _M0L6_2atmpS1761 < 0
          || _M0L6_2atmpS1761 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4089 = _M0L3dstS57[_M0L6_2atmpS1761];
        moonbit_incref(_M0L6_2atmpS1762.$0);
        moonbit_decref(_M0L6_2aoldS4089.$0);
        _M0L3dstS57[_M0L6_2atmpS1761] = _M0L6_2atmpS1762;
        _M0L6_2atmpS1764 = _M0L1iS64 - 1;
        _M0L1iS64 = _M0L6_2atmpS1764;
        continue;
      } else {
        moonbit_decref(_M0L3srcS58);
        moonbit_decref(_M0L3dstS57);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEEEE(
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L3dstS66,
  int32_t _M0L11dst__offsetS68,
  struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE** _M0L3srcS67,
  int32_t _M0L11src__offsetS69,
  int32_t _M0L3lenS71
) {
  int32_t _if__result_4648;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS66 == _M0L3srcS67) {
    _if__result_4648 = _M0L11dst__offsetS68 < _M0L11src__offsetS69;
  } else {
    _if__result_4648 = 0;
  }
  if (_if__result_4648) {
    int32_t _M0L1iS70 = 0;
    while (1) {
      if (_M0L1iS70 < _M0L3lenS71) {
        int32_t _M0L6_2atmpS1766 = _M0L11dst__offsetS68 + _M0L1iS70;
        int32_t _M0L6_2atmpS1768 = _M0L11src__offsetS69 + _M0L1iS70;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2atmpS4092;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2atmpS1767;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2aoldS4091;
        int32_t _M0L6_2atmpS1769;
        if (
          _M0L6_2atmpS1768 < 0
          || _M0L6_2atmpS1768 >= Moonbit_array_length(_M0L3srcS67)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4092
        = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)_M0L3srcS67[
            _M0L6_2atmpS1768
          ];
        _M0L6_2atmpS1767 = _M0L6_2atmpS4092;
        if (
          _M0L6_2atmpS1766 < 0
          || _M0L6_2atmpS1766 >= Moonbit_array_length(_M0L3dstS66)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4091
        = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)_M0L3dstS66[
            _M0L6_2atmpS1766
          ];
        if (_M0L6_2atmpS1767) {
          moonbit_incref(_M0L6_2atmpS1767);
        }
        if (_M0L6_2aoldS4091) {
          moonbit_decref(_M0L6_2aoldS4091);
        }
        _M0L3dstS66[_M0L6_2atmpS1766] = _M0L6_2atmpS1767;
        _M0L6_2atmpS1769 = _M0L1iS70 + 1;
        _M0L1iS70 = _M0L6_2atmpS1769;
        continue;
      } else {
        moonbit_decref(_M0L3srcS67);
        moonbit_decref(_M0L3dstS66);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1774 = _M0L3lenS71 - 1;
    int32_t _M0L1iS73 = _M0L6_2atmpS1774;
    while (1) {
      if (_M0L1iS73 >= 0) {
        int32_t _M0L6_2atmpS1770 = _M0L11dst__offsetS68 + _M0L1iS73;
        int32_t _M0L6_2atmpS1772 = _M0L11src__offsetS69 + _M0L1iS73;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2atmpS4094;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2atmpS1771;
        struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE* _M0L6_2aoldS4093;
        int32_t _M0L6_2atmpS1773;
        if (
          _M0L6_2atmpS1772 < 0
          || _M0L6_2atmpS1772 >= Moonbit_array_length(_M0L3srcS67)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4094
        = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)_M0L3srcS67[
            _M0L6_2atmpS1772
          ];
        _M0L6_2atmpS1771 = _M0L6_2atmpS4094;
        if (
          _M0L6_2atmpS1770 < 0
          || _M0L6_2atmpS1770 >= Moonbit_array_length(_M0L3dstS66)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4093
        = (struct _M0TURP48clawteam8clawteam8internal5httpx5LayerRPB5ArrayGRPC16string10StringViewEE*)_M0L3dstS66[
            _M0L6_2atmpS1770
          ];
        if (_M0L6_2atmpS1771) {
          moonbit_incref(_M0L6_2atmpS1771);
        }
        if (_M0L6_2aoldS4093) {
          moonbit_decref(_M0L6_2aoldS4093);
        }
        _M0L3dstS66[_M0L6_2atmpS1770] = _M0L6_2atmpS1771;
        _M0L6_2atmpS1773 = _M0L1iS73 - 1;
        _M0L1iS73 = _M0L6_2atmpS1773;
        continue;
      } else {
        moonbit_decref(_M0L3srcS67);
        moonbit_decref(_M0L3dstS66);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS75,
  int32_t _M0L11dst__offsetS77,
  void** _M0L3srcS76,
  int32_t _M0L11src__offsetS78,
  int32_t _M0L3lenS80
) {
  int32_t _if__result_4651;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS75 == _M0L3srcS76) {
    _if__result_4651 = _M0L11dst__offsetS77 < _M0L11src__offsetS78;
  } else {
    _if__result_4651 = 0;
  }
  if (_if__result_4651) {
    int32_t _M0L1iS79 = 0;
    while (1) {
      if (_M0L1iS79 < _M0L3lenS80) {
        int32_t _M0L6_2atmpS1775 = _M0L11dst__offsetS77 + _M0L1iS79;
        int32_t _M0L6_2atmpS1777 = _M0L11src__offsetS78 + _M0L1iS79;
        void* _M0L6_2atmpS4096;
        void* _M0L6_2atmpS1776;
        void* _M0L6_2aoldS4095;
        int32_t _M0L6_2atmpS1778;
        if (
          _M0L6_2atmpS1777 < 0
          || _M0L6_2atmpS1777 >= Moonbit_array_length(_M0L3srcS76)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4096 = (void*)_M0L3srcS76[_M0L6_2atmpS1777];
        _M0L6_2atmpS1776 = _M0L6_2atmpS4096;
        if (
          _M0L6_2atmpS1775 < 0
          || _M0L6_2atmpS1775 >= Moonbit_array_length(_M0L3dstS75)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4095 = (void*)_M0L3dstS75[_M0L6_2atmpS1775];
        moonbit_incref(_M0L6_2atmpS1776);
        moonbit_decref(_M0L6_2aoldS4095);
        _M0L3dstS75[_M0L6_2atmpS1775] = _M0L6_2atmpS1776;
        _M0L6_2atmpS1778 = _M0L1iS79 + 1;
        _M0L1iS79 = _M0L6_2atmpS1778;
        continue;
      } else {
        moonbit_decref(_M0L3srcS76);
        moonbit_decref(_M0L3dstS75);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1783 = _M0L3lenS80 - 1;
    int32_t _M0L1iS82 = _M0L6_2atmpS1783;
    while (1) {
      if (_M0L1iS82 >= 0) {
        int32_t _M0L6_2atmpS1779 = _M0L11dst__offsetS77 + _M0L1iS82;
        int32_t _M0L6_2atmpS1781 = _M0L11src__offsetS78 + _M0L1iS82;
        void* _M0L6_2atmpS4098;
        void* _M0L6_2atmpS1780;
        void* _M0L6_2aoldS4097;
        int32_t _M0L6_2atmpS1782;
        if (
          _M0L6_2atmpS1781 < 0
          || _M0L6_2atmpS1781 >= Moonbit_array_length(_M0L3srcS76)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4098 = (void*)_M0L3srcS76[_M0L6_2atmpS1781];
        _M0L6_2atmpS1780 = _M0L6_2atmpS4098;
        if (
          _M0L6_2atmpS1779 < 0
          || _M0L6_2atmpS1779 >= Moonbit_array_length(_M0L3dstS75)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4097 = (void*)_M0L3dstS75[_M0L6_2atmpS1779];
        moonbit_incref(_M0L6_2atmpS1780);
        moonbit_decref(_M0L6_2aoldS4097);
        _M0L3dstS75[_M0L6_2atmpS1779] = _M0L6_2atmpS1780;
        _M0L6_2atmpS1782 = _M0L1iS82 - 1;
        _M0L1iS82 = _M0L6_2atmpS1782;
        continue;
      } else {
        moonbit_decref(_M0L3srcS76);
        moonbit_decref(_M0L3dstS75);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPB4JsonEE(
  void** _M0L3dstS84,
  int32_t _M0L11dst__offsetS86,
  void** _M0L3srcS85,
  int32_t _M0L11src__offsetS87,
  int32_t _M0L3lenS89
) {
  int32_t _if__result_4654;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS84 == _M0L3srcS85) {
    _if__result_4654 = _M0L11dst__offsetS86 < _M0L11src__offsetS87;
  } else {
    _if__result_4654 = 0;
  }
  if (_if__result_4654) {
    int32_t _M0L1iS88 = 0;
    while (1) {
      if (_M0L1iS88 < _M0L3lenS89) {
        int32_t _M0L6_2atmpS1784 = _M0L11dst__offsetS86 + _M0L1iS88;
        int32_t _M0L6_2atmpS1786 = _M0L11src__offsetS87 + _M0L1iS88;
        void* _M0L6_2atmpS4100;
        void* _M0L6_2atmpS1785;
        void* _M0L6_2aoldS4099;
        int32_t _M0L6_2atmpS1787;
        if (
          _M0L6_2atmpS1786 < 0
          || _M0L6_2atmpS1786 >= Moonbit_array_length(_M0L3srcS85)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4100 = (void*)_M0L3srcS85[_M0L6_2atmpS1786];
        _M0L6_2atmpS1785 = _M0L6_2atmpS4100;
        if (
          _M0L6_2atmpS1784 < 0
          || _M0L6_2atmpS1784 >= Moonbit_array_length(_M0L3dstS84)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4099 = (void*)_M0L3dstS84[_M0L6_2atmpS1784];
        moonbit_incref(_M0L6_2atmpS1785);
        moonbit_decref(_M0L6_2aoldS4099);
        _M0L3dstS84[_M0L6_2atmpS1784] = _M0L6_2atmpS1785;
        _M0L6_2atmpS1787 = _M0L1iS88 + 1;
        _M0L1iS88 = _M0L6_2atmpS1787;
        continue;
      } else {
        moonbit_decref(_M0L3srcS85);
        moonbit_decref(_M0L3dstS84);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1792 = _M0L3lenS89 - 1;
    int32_t _M0L1iS91 = _M0L6_2atmpS1792;
    while (1) {
      if (_M0L1iS91 >= 0) {
        int32_t _M0L6_2atmpS1788 = _M0L11dst__offsetS86 + _M0L1iS91;
        int32_t _M0L6_2atmpS1790 = _M0L11src__offsetS87 + _M0L1iS91;
        void* _M0L6_2atmpS4102;
        void* _M0L6_2atmpS1789;
        void* _M0L6_2aoldS4101;
        int32_t _M0L6_2atmpS1791;
        if (
          _M0L6_2atmpS1790 < 0
          || _M0L6_2atmpS1790 >= Moonbit_array_length(_M0L3srcS85)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4102 = (void*)_M0L3srcS85[_M0L6_2atmpS1790];
        _M0L6_2atmpS1789 = _M0L6_2atmpS4102;
        if (
          _M0L6_2atmpS1788 < 0
          || _M0L6_2atmpS1788 >= Moonbit_array_length(_M0L3dstS84)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4101 = (void*)_M0L3dstS84[_M0L6_2atmpS1788];
        moonbit_incref(_M0L6_2atmpS1789);
        moonbit_decref(_M0L6_2aoldS4101);
        _M0L3dstS84[_M0L6_2atmpS1788] = _M0L6_2atmpS1789;
        _M0L6_2atmpS1791 = _M0L1iS91 - 1;
        _M0L1iS91 = _M0L6_2atmpS1791;
        continue;
      } else {
        moonbit_decref(_M0L3srcS85);
        moonbit_decref(_M0L3dstS84);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1709;
  moonbit_string_t _M0L6_2atmpS4105;
  moonbit_string_t _M0L6_2atmpS1707;
  moonbit_string_t _M0L6_2atmpS1708;
  moonbit_string_t _M0L6_2atmpS4104;
  moonbit_string_t _M0L6_2atmpS1706;
  moonbit_string_t _M0L6_2atmpS4103;
  moonbit_string_t _M0L6_2atmpS1705;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1709 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4105
  = moonbit_add_string(_M0L6_2atmpS1709, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1709);
  _M0L6_2atmpS1707 = _M0L6_2atmpS4105;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1708
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4104 = moonbit_add_string(_M0L6_2atmpS1707, _M0L6_2atmpS1708);
  moonbit_decref(_M0L6_2atmpS1707);
  moonbit_decref(_M0L6_2atmpS1708);
  _M0L6_2atmpS1706 = _M0L6_2atmpS4104;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4103
  = moonbit_add_string(_M0L6_2atmpS1706, (moonbit_string_t)moonbit_string_literal_74.data);
  moonbit_decref(_M0L6_2atmpS1706);
  _M0L6_2atmpS1705 = _M0L6_2atmpS4103;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1705);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1714;
  moonbit_string_t _M0L6_2atmpS4108;
  moonbit_string_t _M0L6_2atmpS1712;
  moonbit_string_t _M0L6_2atmpS1713;
  moonbit_string_t _M0L6_2atmpS4107;
  moonbit_string_t _M0L6_2atmpS1711;
  moonbit_string_t _M0L6_2atmpS4106;
  moonbit_string_t _M0L6_2atmpS1710;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1714 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4108
  = moonbit_add_string(_M0L6_2atmpS1714, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1714);
  _M0L6_2atmpS1712 = _M0L6_2atmpS4108;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1713
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4107 = moonbit_add_string(_M0L6_2atmpS1712, _M0L6_2atmpS1713);
  moonbit_decref(_M0L6_2atmpS1712);
  moonbit_decref(_M0L6_2atmpS1713);
  _M0L6_2atmpS1711 = _M0L6_2atmpS4107;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4106
  = moonbit_add_string(_M0L6_2atmpS1711, (moonbit_string_t)moonbit_string_literal_74.data);
  moonbit_decref(_M0L6_2atmpS1711);
  _M0L6_2atmpS1710 = _M0L6_2atmpS4106;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1710);
  return 0;
}

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0FPB5abortGRP48clawteam8clawteam8internal5httpx5RouteE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1719;
  moonbit_string_t _M0L6_2atmpS4111;
  moonbit_string_t _M0L6_2atmpS1717;
  moonbit_string_t _M0L6_2atmpS1718;
  moonbit_string_t _M0L6_2atmpS4110;
  moonbit_string_t _M0L6_2atmpS1716;
  moonbit_string_t _M0L6_2atmpS4109;
  moonbit_string_t _M0L6_2atmpS1715;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1719 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4111
  = moonbit_add_string(_M0L6_2atmpS1719, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1719);
  _M0L6_2atmpS1717 = _M0L6_2atmpS4111;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1718
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4110 = moonbit_add_string(_M0L6_2atmpS1717, _M0L6_2atmpS1718);
  moonbit_decref(_M0L6_2atmpS1717);
  moonbit_decref(_M0L6_2atmpS1718);
  _M0L6_2atmpS1716 = _M0L6_2atmpS4110;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4109
  = moonbit_add_string(_M0L6_2atmpS1716, (moonbit_string_t)moonbit_string_literal_74.data);
  moonbit_decref(_M0L6_2atmpS1716);
  _M0L6_2atmpS1715 = _M0L6_2atmpS4109;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRP48clawteam8clawteam8internal5httpx5RouteE(_M0L6_2atmpS1715);
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS26,
  moonbit_string_t _M0L3locS27
) {
  moonbit_string_t _M0L6_2atmpS1724;
  moonbit_string_t _M0L6_2atmpS4114;
  moonbit_string_t _M0L6_2atmpS1722;
  moonbit_string_t _M0L6_2atmpS1723;
  moonbit_string_t _M0L6_2atmpS4113;
  moonbit_string_t _M0L6_2atmpS1721;
  moonbit_string_t _M0L6_2atmpS4112;
  moonbit_string_t _M0L6_2atmpS1720;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1724 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4114
  = moonbit_add_string(_M0L6_2atmpS1724, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1724);
  _M0L6_2atmpS1722 = _M0L6_2atmpS4114;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1723
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS27);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4113 = moonbit_add_string(_M0L6_2atmpS1722, _M0L6_2atmpS1723);
  moonbit_decref(_M0L6_2atmpS1722);
  moonbit_decref(_M0L6_2atmpS1723);
  _M0L6_2atmpS1721 = _M0L6_2atmpS4113;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4112
  = moonbit_add_string(_M0L6_2atmpS1721, (moonbit_string_t)moonbit_string_literal_74.data);
  moonbit_decref(_M0L6_2atmpS1721);
  _M0L6_2atmpS1720 = _M0L6_2atmpS4112;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1720);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS28,
  moonbit_string_t _M0L3locS29
) {
  moonbit_string_t _M0L6_2atmpS1729;
  moonbit_string_t _M0L6_2atmpS4117;
  moonbit_string_t _M0L6_2atmpS1727;
  moonbit_string_t _M0L6_2atmpS1728;
  moonbit_string_t _M0L6_2atmpS4116;
  moonbit_string_t _M0L6_2atmpS1726;
  moonbit_string_t _M0L6_2atmpS4115;
  moonbit_string_t _M0L6_2atmpS1725;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1729 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS28);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4117
  = moonbit_add_string(_M0L6_2atmpS1729, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1729);
  _M0L6_2atmpS1727 = _M0L6_2atmpS4117;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1728
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS29);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4116 = moonbit_add_string(_M0L6_2atmpS1727, _M0L6_2atmpS1728);
  moonbit_decref(_M0L6_2atmpS1727);
  moonbit_decref(_M0L6_2atmpS1728);
  _M0L6_2atmpS1726 = _M0L6_2atmpS4116;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4115
  = moonbit_add_string(_M0L6_2atmpS1726, (moonbit_string_t)moonbit_string_literal_74.data);
  moonbit_decref(_M0L6_2atmpS1726);
  _M0L6_2atmpS1725 = _M0L6_2atmpS4115;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1725);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS18,
  uint32_t _M0L5valueS19
) {
  uint32_t _M0L3accS1704;
  uint32_t _M0L6_2atmpS1703;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1704 = _M0L4selfS18->$0;
  _M0L6_2atmpS1703 = _M0L3accS1704 + 4u;
  _M0L4selfS18->$0 = _M0L6_2atmpS1703;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS18, _M0L5valueS19);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5inputS17
) {
  uint32_t _M0L3accS1701;
  uint32_t _M0L6_2atmpS1702;
  uint32_t _M0L6_2atmpS1700;
  uint32_t _M0L6_2atmpS1699;
  uint32_t _M0L6_2atmpS1698;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1701 = _M0L4selfS16->$0;
  _M0L6_2atmpS1702 = _M0L5inputS17 * 3266489917u;
  _M0L6_2atmpS1700 = _M0L3accS1701 + _M0L6_2atmpS1702;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1699 = _M0FPB4rotl(_M0L6_2atmpS1700, 17);
  _M0L6_2atmpS1698 = _M0L6_2atmpS1699 * 668265263u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1698;
  moonbit_decref(_M0L4selfS16);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS14, int32_t _M0L1rS15) {
  uint32_t _M0L6_2atmpS1695;
  int32_t _M0L6_2atmpS1697;
  uint32_t _M0L6_2atmpS1696;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1695 = _M0L1xS14 << (_M0L1rS15 & 31);
  _M0L6_2atmpS1697 = 32 - _M0L1rS15;
  _M0L6_2atmpS1696 = _M0L1xS14 >> (_M0L6_2atmpS1697 & 31);
  return _M0L6_2atmpS1695 | _M0L6_2atmpS1696;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S10,
  struct _M0TPB6Logger _M0L10_2ax__4934S13
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS11;
  moonbit_string_t _M0L8_2afieldS4118;
  int32_t _M0L6_2acntS4422;
  moonbit_string_t _M0L15_2a_2aarg__4935S12;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS11
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S10;
  _M0L8_2afieldS4118 = _M0L10_2aFailureS11->$0;
  _M0L6_2acntS4422 = Moonbit_object_header(_M0L10_2aFailureS11)->rc;
  if (_M0L6_2acntS4422 > 1) {
    int32_t _M0L11_2anew__cntS4423 = _M0L6_2acntS4422 - 1;
    Moonbit_object_header(_M0L10_2aFailureS11)->rc = _M0L11_2anew__cntS4423;
    moonbit_incref(_M0L8_2afieldS4118);
  } else if (_M0L6_2acntS4422 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS11);
  }
  _M0L15_2a_2aarg__4935S12 = _M0L8_2afieldS4118;
  if (_M0L10_2ax__4934S13.$1) {
    moonbit_incref(_M0L10_2ax__4934S13.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S13.$0->$method_0(_M0L10_2ax__4934S13.$1, (moonbit_string_t)moonbit_string_literal_121.data);
  if (_M0L10_2ax__4934S13.$1) {
    moonbit_incref(_M0L10_2ax__4934S13.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S13, _M0L15_2a_2aarg__4935S12);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S13.$0->$method_0(_M0L10_2ax__4934S13.$1, (moonbit_string_t)moonbit_string_literal_122.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS9) {
  void* _block_4657;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4657 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4657)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4657)->$0 = _M0L4selfS9;
  return _block_4657;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS8) {
  void* _block_4658;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4658 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4658)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4658)->$0 = _M0L5arrayS8;
  return _block_4658;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS7,
  moonbit_string_t _M0L3objS6
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS6, _M0L4selfS7);
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

struct _M0TP48clawteam8clawteam8internal5httpx5Route* _M0FPC15abort5abortGRP48clawteam8clawteam8internal5httpx5RouteE(
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

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS5
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS5);
  moonbit_decref(_M0L3msgS5);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1593) {
  switch (Moonbit_object_tag(_M0L4_2aeS1593)) {
    case 2: {
      moonbit_decref(_M0L4_2aeS1593);
      return (moonbit_string_t)moonbit_string_literal_123.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1593);
      return (moonbit_string_t)moonbit_string_literal_124.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1593);
      return (moonbit_string_t)moonbit_string_literal_125.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1593);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1593);
      return (moonbit_string_t)moonbit_string_literal_126.data;
      break;
    }
  }
}

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPC16string10StringViewE(
  void* _M0L11_2aobj__ptrS1620
) {
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L7_2aselfS1619 =
    (struct _M0TPB5ArrayGRPC16string10StringViewE*)_M0L11_2aobj__ptrS1620;
  return _M0IPC15array5ArrayPB6ToJson8to__jsonGRPC16string10StringViewE(_M0L7_2aselfS1619);
}

void* _M0IPB4IterPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal5httpx6MethodE(
  void* _M0L11_2aobj__ptrS1618
) {
  struct _M0TWEOi* _M0L7_2aselfS1617 =
    (struct _M0TWEOi*)_M0L11_2aobj__ptrS1618;
  return _M0IPB4IterPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5httpx6MethodE(_M0L7_2aselfS1617);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1616,
  int32_t _M0L8_2aparamS1615
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1614 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1616;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1614, _M0L8_2aparamS1615);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1613,
  struct _M0TPC16string10StringView _M0L8_2aparamS1612
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1611 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1613;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1611, _M0L8_2aparamS1612);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1610,
  moonbit_string_t _M0L8_2aparamS1607,
  int32_t _M0L8_2aparamS1608,
  int32_t _M0L8_2aparamS1609
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1606 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1610;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1606, _M0L8_2aparamS1607, _M0L8_2aparamS1608, _M0L8_2aparamS1609);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1605,
  moonbit_string_t _M0L8_2aparamS1604
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1603 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1605;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1603, _M0L8_2aparamS1604);
  return 0;
}

void* _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1601
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1602 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1601;
  int32_t _M0L8_2afieldS4119 = _M0L14_2aboxed__selfS1602->$0;
  int32_t _M0L7_2aselfS1600;
  moonbit_decref(_M0L14_2aboxed__selfS1602);
  _M0L7_2aselfS1600 = _M0L8_2afieldS4119;
  return _M0IPC14bool4BoolPB6ToJson8to__json(_M0L7_2aselfS1600);
}

void* _M0IPB4IterPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRPC16string10StringViewE(
  void* _M0L11_2aobj__ptrS1599
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2aselfS1598 =
    (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_M0L11_2aobj__ptrS1599;
  return _M0IPB4IterPB6ToJson8to__jsonGRPC16string10StringViewE(_M0L7_2aselfS1598);
}

void moonbit_init() {
  int32_t* _M0L6_2atmpS1626 = (int32_t*)moonbit_make_int32_array_raw(9);
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1506;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1694;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1693;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1692;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1629;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1507;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1691;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1690;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1689;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1630;
  moonbit_string_t* _M0L6_2atmpS1688;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1687;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1683;
  moonbit_string_t* _M0L6_2atmpS1686;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1685;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1684;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1508;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1682;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1681;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1680;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1631;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1509;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1679;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1678;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1677;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1632;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1510;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1676;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1675;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1674;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1633;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1511;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1673;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1672;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1671;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1634;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1512;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1670;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1669;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1668;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1635;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1513;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1667;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1666;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1665;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1636;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1514;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1664;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1663;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1662;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1637;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1515;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1661;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1660;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1659;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1638;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1516;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1658;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1657;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1656;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1639;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1517;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1655;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1654;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1653;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1640;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1518;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1652;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1651;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1650;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1641;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1519;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1649;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1648;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1647;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1642;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1520;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1646;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1645;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1644;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1643;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1505;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1628;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1627;
  _M0L6_2atmpS1626[0] = 0;
  _M0L6_2atmpS1626[1] = 1;
  _M0L6_2atmpS1626[2] = 2;
  _M0L6_2atmpS1626[3] = 3;
  _M0L6_2atmpS1626[4] = 4;
  _M0L6_2atmpS1626[5] = 5;
  _M0L6_2atmpS1626[6] = 6;
  _M0L6_2atmpS1626[7] = 7;
  _M0L6_2atmpS1626[8] = 8;
  _M0FP48clawteam8clawteam8internal5httpx7methods
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE));
  Moonbit_object_header(_M0FP48clawteam8clawteam8internal5httpx7methods)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal5httpx6MethodE, $0) >> 2, 1, 0);
  _M0FP48clawteam8clawteam8internal5httpx7methods->$0 = _M0L6_2atmpS1626;
  _M0FP48clawteam8clawteam8internal5httpx7methods->$1 = 9;
  _M0L7_2abindS1506
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1694 = _M0L7_2abindS1506;
  _M0L6_2atmpS1693
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1694
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1692
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1693);
  _M0L8_2atupleS1629
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1629)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1629->$0 = (moonbit_string_t)moonbit_string_literal_127.data;
  _M0L8_2atupleS1629->$1 = _M0L6_2atmpS1692;
  _M0L7_2abindS1507
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1691 = _M0L7_2abindS1507;
  _M0L6_2atmpS1690
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1691
  };
  #line 400 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1689
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1690);
  _M0L8_2atupleS1630
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1630)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1630->$0 = (moonbit_string_t)moonbit_string_literal_128.data;
  _M0L8_2atupleS1630->$1 = _M0L6_2atmpS1689;
  _M0L6_2atmpS1688 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1688[0] = (moonbit_string_t)moonbit_string_literal_129.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal5httpx39____test__726f757465722e6d6274__0_2eclo);
  _M0L8_2atupleS1687
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1687)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1687->$0
  = _M0FP48clawteam8clawteam8internal5httpx39____test__726f757465722e6d6274__0_2eclo;
  _M0L8_2atupleS1687->$1 = _M0L6_2atmpS1688;
  _M0L8_2atupleS1683
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1683)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1683->$0 = 0;
  _M0L8_2atupleS1683->$1 = _M0L8_2atupleS1687;
  _M0L6_2atmpS1686 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1686[0] = (moonbit_string_t)moonbit_string_literal_130.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal5httpx39____test__726f757465722e6d6274__1_2eclo);
  _M0L8_2atupleS1685
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1685)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1685->$0
  = _M0FP48clawteam8clawteam8internal5httpx39____test__726f757465722e6d6274__1_2eclo;
  _M0L8_2atupleS1685->$1 = _M0L6_2atmpS1686;
  _M0L8_2atupleS1684
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1684)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1684->$0 = 1;
  _M0L8_2atupleS1684->$1 = _M0L8_2atupleS1685;
  _M0L7_2abindS1508
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1508[0] = _M0L8_2atupleS1683;
  _M0L7_2abindS1508[1] = _M0L8_2atupleS1684;
  _M0L6_2atmpS1682 = _M0L7_2abindS1508;
  _M0L6_2atmpS1681
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 2, _M0L6_2atmpS1682
  };
  #line 402 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1680
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1681);
  _M0L8_2atupleS1631
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1631)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1631->$0 = (moonbit_string_t)moonbit_string_literal_131.data;
  _M0L8_2atupleS1631->$1 = _M0L6_2atmpS1680;
  _M0L7_2abindS1509
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1679 = _M0L7_2abindS1509;
  _M0L6_2atmpS1678
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1679
  };
  #line 406 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1677
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1678);
  _M0L8_2atupleS1632
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1632)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1632->$0 = (moonbit_string_t)moonbit_string_literal_132.data;
  _M0L8_2atupleS1632->$1 = _M0L6_2atmpS1677;
  _M0L7_2abindS1510
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1676 = _M0L7_2abindS1510;
  _M0L6_2atmpS1675
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1676
  };
  #line 408 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1674
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1675);
  _M0L8_2atupleS1633
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1633)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1633->$0 = (moonbit_string_t)moonbit_string_literal_133.data;
  _M0L8_2atupleS1633->$1 = _M0L6_2atmpS1674;
  _M0L7_2abindS1511
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1673 = _M0L7_2abindS1511;
  _M0L6_2atmpS1672
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1673
  };
  #line 410 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1671
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1672);
  _M0L8_2atupleS1634
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1634)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1634->$0 = (moonbit_string_t)moonbit_string_literal_134.data;
  _M0L8_2atupleS1634->$1 = _M0L6_2atmpS1671;
  _M0L7_2abindS1512
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1670 = _M0L7_2abindS1512;
  _M0L6_2atmpS1669
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1670
  };
  #line 412 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1668
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1669);
  _M0L8_2atupleS1635
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1635)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1635->$0 = (moonbit_string_t)moonbit_string_literal_135.data;
  _M0L8_2atupleS1635->$1 = _M0L6_2atmpS1668;
  _M0L7_2abindS1513
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1667 = _M0L7_2abindS1513;
  _M0L6_2atmpS1666
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1667
  };
  #line 414 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1665
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1666);
  _M0L8_2atupleS1636
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1636)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1636->$0 = (moonbit_string_t)moonbit_string_literal_136.data;
  _M0L8_2atupleS1636->$1 = _M0L6_2atmpS1665;
  _M0L7_2abindS1514
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1664 = _M0L7_2abindS1514;
  _M0L6_2atmpS1663
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1664
  };
  #line 416 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1662
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1663);
  _M0L8_2atupleS1637
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1637)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1637->$0 = (moonbit_string_t)moonbit_string_literal_137.data;
  _M0L8_2atupleS1637->$1 = _M0L6_2atmpS1662;
  _M0L7_2abindS1515
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1661 = _M0L7_2abindS1515;
  _M0L6_2atmpS1660
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1661
  };
  #line 418 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1659
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1660);
  _M0L8_2atupleS1638
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1638)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1638->$0 = (moonbit_string_t)moonbit_string_literal_138.data;
  _M0L8_2atupleS1638->$1 = _M0L6_2atmpS1659;
  _M0L7_2abindS1516
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1658 = _M0L7_2abindS1516;
  _M0L6_2atmpS1657
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1658
  };
  #line 420 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1656
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1657);
  _M0L8_2atupleS1639
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1639)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1639->$0 = (moonbit_string_t)moonbit_string_literal_139.data;
  _M0L8_2atupleS1639->$1 = _M0L6_2atmpS1656;
  _M0L7_2abindS1517
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1655 = _M0L7_2abindS1517;
  _M0L6_2atmpS1654
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1655
  };
  #line 422 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1653
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1654);
  _M0L8_2atupleS1640
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1640)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1640->$0 = (moonbit_string_t)moonbit_string_literal_140.data;
  _M0L8_2atupleS1640->$1 = _M0L6_2atmpS1653;
  _M0L7_2abindS1518
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1652 = _M0L7_2abindS1518;
  _M0L6_2atmpS1651
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1652
  };
  #line 424 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1650
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1651);
  _M0L8_2atupleS1641
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1641)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1641->$0 = (moonbit_string_t)moonbit_string_literal_141.data;
  _M0L8_2atupleS1641->$1 = _M0L6_2atmpS1650;
  _M0L7_2abindS1519
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1649 = _M0L7_2abindS1519;
  _M0L6_2atmpS1648
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1649
  };
  #line 426 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1647
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1648);
  _M0L8_2atupleS1642
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1642)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1642->$0 = (moonbit_string_t)moonbit_string_literal_142.data;
  _M0L8_2atupleS1642->$1 = _M0L6_2atmpS1647;
  _M0L7_2abindS1520
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1646 = _M0L7_2abindS1520;
  _M0L6_2atmpS1645
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1646
  };
  #line 428 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1644
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1645);
  _M0L8_2atupleS1643
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1643)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1643->$0 = (moonbit_string_t)moonbit_string_literal_143.data;
  _M0L8_2atupleS1643->$1 = _M0L6_2atmpS1644;
  _M0L7_2abindS1505
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(15);
  _M0L7_2abindS1505[0] = _M0L8_2atupleS1629;
  _M0L7_2abindS1505[1] = _M0L8_2atupleS1630;
  _M0L7_2abindS1505[2] = _M0L8_2atupleS1631;
  _M0L7_2abindS1505[3] = _M0L8_2atupleS1632;
  _M0L7_2abindS1505[4] = _M0L8_2atupleS1633;
  _M0L7_2abindS1505[5] = _M0L8_2atupleS1634;
  _M0L7_2abindS1505[6] = _M0L8_2atupleS1635;
  _M0L7_2abindS1505[7] = _M0L8_2atupleS1636;
  _M0L7_2abindS1505[8] = _M0L8_2atupleS1637;
  _M0L7_2abindS1505[9] = _M0L8_2atupleS1638;
  _M0L7_2abindS1505[10] = _M0L8_2atupleS1639;
  _M0L7_2abindS1505[11] = _M0L8_2atupleS1640;
  _M0L7_2abindS1505[12] = _M0L8_2atupleS1641;
  _M0L7_2abindS1505[13] = _M0L8_2atupleS1642;
  _M0L7_2abindS1505[14] = _M0L8_2atupleS1643;
  _M0L6_2atmpS1628 = _M0L7_2abindS1505;
  _M0L6_2atmpS1627
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 15, _M0L6_2atmpS1628
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal5httpx48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1627);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1625;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1587;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1588;
  int32_t _M0L7_2abindS1589;
  int32_t _M0L2__S1590;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1625
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1587
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1587)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1587->$0 = _M0L6_2atmpS1625;
  _M0L12async__testsS1587->$1 = 0;
  #line 467 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1588
  = _M0FP48clawteam8clawteam8internal5httpx52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1589 = _M0L7_2abindS1588->$1;
  _M0L2__S1590 = 0;
  while (1) {
    if (_M0L2__S1590 < _M0L7_2abindS1589) {
      struct _M0TUsiE** _M0L8_2afieldS4123 = _M0L7_2abindS1588->$0;
      struct _M0TUsiE** _M0L3bufS1624 = _M0L8_2afieldS4123;
      struct _M0TUsiE* _M0L6_2atmpS4122 =
        (struct _M0TUsiE*)_M0L3bufS1624[_M0L2__S1590];
      struct _M0TUsiE* _M0L3argS1591 = _M0L6_2atmpS4122;
      moonbit_string_t _M0L8_2afieldS4121 = _M0L3argS1591->$0;
      moonbit_string_t _M0L6_2atmpS1621 = _M0L8_2afieldS4121;
      int32_t _M0L8_2afieldS4120 = _M0L3argS1591->$1;
      int32_t _M0L6_2atmpS1622 = _M0L8_2afieldS4120;
      int32_t _M0L6_2atmpS1623;
      moonbit_incref(_M0L6_2atmpS1621);
      moonbit_incref(_M0L12async__testsS1587);
      #line 468 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam8internal5httpx44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1587, _M0L6_2atmpS1621, _M0L6_2atmpS1622);
      _M0L6_2atmpS1623 = _M0L2__S1590 + 1;
      _M0L2__S1590 = _M0L6_2atmpS1623;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1588);
    }
    break;
  }
  #line 470 "E:\\moonbit\\clawteam\\internal\\httpx\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal5httpx28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal5httpx34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1587);
  return 0;
}