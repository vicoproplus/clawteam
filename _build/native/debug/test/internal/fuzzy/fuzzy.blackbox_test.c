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
struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0DTPC16result6ResultGbRPB7NoErrorE2Ok;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWERPC16option6OptionGRPC16string10StringViewE;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__;

struct _M0TPB6Logger;

struct _M0KTPB6ToJsonTPC16option6OptionGRP48clawteam8clawteam8internal5fuzzy11MatchResultE;

struct _M0TPB19MulShiftAll64Result;

struct _M0TPB5ArrayGRPC16string10StringViewE;

struct _M0TWEOUsRPB4JsonE;

struct _M0TWRPC16string10StringViewEs;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGRPB5ArrayGsERPB7NoErrorE3Err;

struct _M0DTPC16result6ResultGRPB5ArrayGsERPB7NoErrorE2Ok;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPC13ref3RefGORPC16string10StringViewE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21fuzzy__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGsRPB7NoErrorE2Ok;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC16result6ResultGsRPB7NoErrorE3Err;

struct _M0TWsEb;

struct _M0TPB5ArrayGUsiEE;

struct _M0DTPC16result6ResultGbRPB7NoErrorE3Err;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__;

struct _M0TPB9ArrayViewGRPC16string10StringViewE;

struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__;

struct _M0DTPB4Json6Object;

struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__;

struct _M0TPC13ref3RefGbE;

struct _M0R38String_3a_3aiter_2eanon__u2286__l247__;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TWcERPC16string10StringView;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21fuzzy__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

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

struct _M0DTPC16result6ResultGbRPB7NoErrorE2Ok {
  int32_t $0;
  
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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  
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

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TWcERPC16string10StringView* $0;
  struct _M0TWEOc* $1;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0KTPB6ToJsonTPC16option6OptionGRP48clawteam8clawteam8internal5fuzzy11MatchResultE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TPB5ArrayGRPC16string10StringViewE {
  int32_t $1;
  struct _M0TPC16string10StringView* $0;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0TWRPC16string10StringViewEs {
  moonbit_string_t(* code)(
    struct _M0TWRPC16string10StringViewEs*,
    struct _M0TPC16string10StringView
  );
  
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

struct _M0DTPC16result6ResultGRPB5ArrayGsERPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGsERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGsE* $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
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

struct _M0TPC13ref3RefGORPC16string10StringViewE {
  void* $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21fuzzy__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGsRPB7NoErrorE2Ok {
  moonbit_string_t $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
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

struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGsRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TWsEb {
  int32_t(* code)(struct _M0TWsEb*, moonbit_string_t);
  
};

struct _M0TPB5ArrayGUsiEE {
  int32_t $1;
  struct _M0TUsiE** $0;
  
};

struct _M0DTPC16result6ResultGbRPB7NoErrorE3Err {
  int32_t $0;
  
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

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
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

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPB4Json6String {
  moonbit_string_t $0;
  
};

struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult {
  int32_t $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  
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

struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB9ArrayViewGRPC16string10StringViewE {
  int32_t $1;
  int32_t $2;
  struct _M0TPC16string10StringView* $0;
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $2_0;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  int32_t $1_1;
  int32_t $1_2;
  int32_t $2;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* $0;
  moonbit_string_t $1_0;
  
};

struct _M0TPC13ref3RefGbE {
  int32_t $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2286__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0TWcERPC16string10StringView {
  struct _M0TPC16string10StringView(* code)(
    struct _M0TWcERPC16string10StringView*,
    int32_t
  );
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21fuzzy__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1486(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1477(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testC3436l432(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testC3432l433(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1409(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1404(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1391(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal21fuzzy__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__3(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__0(
  
);

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy11find__match(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__match(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3150l41(
  struct _M0TWRPC16string10StringViewEs*,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3144l44(
  struct _M0TWRPC16string10StringViewEs*,
  struct _M0TPC16string10StringView
);

int32_t _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3141l49(
  struct _M0TWsEb*,
  moonbit_string_t
);

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchN24fuzzy__match__from__lineS1323(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  int32_t,
  int32_t
);

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy17try__exact__match(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0IP48clawteam8clawteam8internal5fuzzy11MatchResultPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult*
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

moonbit_string_t _M0MPC15array5Array4joinGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array6filterGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TWsEb*
);

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

moonbit_string_t _M0MPC15array9ArrayView4joinGRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGRPC16string10StringViewE,
  struct _M0TPC16string10StringView
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

void* _M0IPC16option6OptionPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5fuzzy11MatchResultE(
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult*
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array3mapGRPC16string10StringViewsE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TWRPC16string10StringViewEs*
);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2580l591(
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

struct _M0TPB5ArrayGsE* _M0MPC15array5Array12make__uninitGsE(int32_t);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2329l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TPC16string10StringView _M0IPC16string10StringViewPB12ToStringView16to__string__view(
  struct _M0TPC16string10StringView
);

struct _M0TPB5ArrayGRPC16string10StringViewE* _M0MPB4Iter9to__arrayGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

void* _M0MPC16string10StringView5splitC2313l1073(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2309l1070(
  struct _M0TWcERPC16string10StringView*,
  int32_t
);

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t);

moonbit_string_t _M0FPB16char__to__string(int32_t);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TWEOc*,
  struct _M0TWcERPC16string10StringView*
);

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2302l317(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2286l247(struct _M0TWEOc*);

struct _M0TPC16string10StringView _M0MPC16string10StringView12trim_2einner(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

struct _M0TPC16string10StringView _M0MPC16string10StringView17trim__end_2einner(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

struct _M0TPC16string10StringView _M0MPC16string10StringView19trim__start_2einner(
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

int32_t _M0MPC15array5Array4pushGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

int32_t _M0MPC15array5Array7reallocGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
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

int32_t _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(int32_t);

moonbit_string_t _M0MPC16string6String6repeat(moonbit_string_t, int32_t);

int64_t _M0MPC16string6String4find(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

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

int32_t _M0MPC16string10StringView4iterC2099l198(struct _M0TWEOc*);

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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

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

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

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

struct _M0TPC16string10StringView _M0MPC15array5Array2atGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
);

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

struct _M0TPC16string10StringView* _M0MPC15array5Array6bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(
  struct _M0TPC16string10StringView*,
  int32_t,
  struct _M0TPC16string10StringView*,
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(
  struct _M0TPC16string10StringView*,
  int32_t,
  struct _M0TPC16string10StringView*,
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

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal5fuzzy11MatchResultE(
  void*
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

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 54, 55, 58, 51, 45, 54, 55, 58, 54, 54, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[115]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 114), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 117, 
    122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 
    115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 56, 58, 54, 49, 45, 53, 48, 58, 52, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_106 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    102, 105, 110, 100, 95, 109, 97, 116, 99, 104, 32, 110, 111, 32, 
    109, 97, 116, 99, 104, 32, 102, 111, 117, 110, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[43]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 42), 
    102, 105, 110, 100, 95, 109, 97, 116, 99, 104, 47, 108, 105, 110, 
    101, 45, 98, 121, 45, 108, 105, 110, 101, 47, 105, 103, 110, 111, 
    114, 101, 45, 101, 109, 112, 116, 121, 45, 108, 105, 110, 101, 115, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 54, 55, 58, 54, 49, 45, 54, 55, 58, 54, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 
    49, 51, 58, 53, 45, 49, 49, 51, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_72 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    112, 111, 115, 105, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[113]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 112), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 117, 
    122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 
    115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 50, 54, 58, 57, 45, 
    52, 50, 54, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100, 10, 84, 104, 
    105, 115, 32, 105, 115, 32, 97, 32, 116, 101, 115, 116, 10, 69, 110, 
    100, 32, 111, 102, 32, 102, 105, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 50, 58, 54, 49, 45, 50, 52, 58, 52, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    108, 105, 110, 101, 32, 50, 32, 32, 10, 32, 32, 108, 105, 110, 101, 
    32, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 50, 58, 51, 45, 50, 52, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 54, 48, 58, 54, 49, 45, 54, 48, 58, 54, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 54, 55, 58, 49, 54, 45, 54, 55, 58, 53, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_53 =
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
} const moonbit_string_literal_42 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 51, 51, 58, 51, 45, 51, 53, 58, 53, 0
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
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    102, 105, 110, 100, 95, 109, 97, 116, 99, 104, 32, 101, 120, 97, 
    99, 116, 32, 109, 97, 116, 99, 104, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 54, 48, 58, 49, 54, 45, 54, 48, 58, 53, 
    49, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[40]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 39), 
    102, 105, 110, 100, 95, 109, 97, 116, 99, 104, 32, 108, 105, 110, 
    101, 45, 98, 121, 45, 108, 105, 110, 101, 32, 119, 105, 116, 104, 
    32, 119, 104, 105, 116, 101, 115, 112, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 51, 51, 58, 54, 49, 45, 51, 53, 58, 52, 
    0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_76 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    108, 105, 110, 101, 32, 49, 10, 108, 105, 110, 101, 32, 50, 10, 10, 
    108, 105, 110, 101, 32, 51, 10, 108, 105, 110, 101, 32, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    108, 105, 110, 101, 32, 50, 10, 108, 105, 110, 101, 32, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    108, 105, 110, 101, 32, 49, 10, 10, 108, 105, 110, 101, 32, 50, 10, 
    108, 105, 110, 101, 32, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    84, 104, 105, 115, 32, 105, 115, 32, 97, 32, 116, 101, 115, 116, 
    0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    32, 32, 108, 105, 110, 101, 32, 49, 32, 32, 10, 32, 32, 108, 105, 
    110, 101, 32, 50, 32, 32, 10, 32, 32, 108, 105, 110, 101, 32, 51, 
    32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    108, 101, 110, 103, 116, 104, 0
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
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    32, 9, 13, 10, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 54, 48, 58, 51, 45, 54, 48, 58, 54, 54, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    101, 110, 100, 95, 108, 105, 110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 101, 97, 114, 99, 104, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 56, 58, 49, 54, 45, 56, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    108, 105, 110, 101, 32, 49, 10, 108, 105, 110, 101, 32, 50, 10, 108, 
    105, 110, 101, 32, 51, 10, 108, 105, 110, 101, 32, 52, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[73]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 72), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 56, 58, 51, 45, 49, 48, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 51, 51, 58, 49, 54, 45, 51, 51, 58, 53, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 56, 58, 51, 45, 53, 48, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 56, 58, 49, 54, 45, 52, 56, 58, 53, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 56, 58, 54, 49, 45, 49, 48, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    78, 111, 116, 32, 102, 111, 117, 110, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 117, 122, 122, 
    121, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 102, 
    117, 122, 122, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 115, 101, 97, 114, 99, 104, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 50, 58, 49, 54, 45, 50, 50, 58, 53, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    116, 121, 112, 101, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    108, 105, 110, 101, 32, 49, 10, 108, 105, 110, 101, 32, 50, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWcERPC16string10StringView data;
  
} const _M0MPC16string10StringView5splitC2309l1070$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MPC16string10StringView5splitC2309l1070
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__0_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRPC16string10StringViewEs data;
  
} const _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3150l41$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3150l41
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__3_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRPC16string10StringViewEs data;
  
} const _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3144l44$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3144l44
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWsEb data; 
} const _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3141l49$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3141l49
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1486$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1486
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__0_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal5fuzzy11MatchResultE}
  };

struct _M0BTPB6ToJson* _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB31ryu__to__string_2erecord_2f1148$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1148 =
  &_M0FPB31ryu__to__string_2erecord_2f1148$object.data;

int64_t _M0FPB33brute__force__find_2econstr_2f552 = 0ll;

int64_t _M0FPB43boyer__moore__horspool__find_2econstr_2f538 = 0ll;

void* _M0FPC17prelude4null;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3470
) {
  return _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3469
) {
  return _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__3();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3468
) {
  return _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test53____test__7365617263685f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3467
) {
  return _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__2();
}

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1507,
  moonbit_string_t _M0L8filenameS1482,
  int32_t _M0L5indexS1485
) {
  struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477* _closure_3908;
  struct _M0TWssbEu* _M0L14handle__resultS1477;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1486;
  void* _M0L11_2atry__errS1501;
  struct moonbit_result_0 _tmp_3910;
  int32_t _handle__error__result_3911;
  int32_t _M0L6_2atmpS3455;
  void* _M0L3errS1502;
  moonbit_string_t _M0L4nameS1504;
  struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1505;
  moonbit_string_t _M0L8_2afieldS3471;
  int32_t _M0L6_2acntS3807;
  moonbit_string_t _M0L7_2anameS1506;
  #line 531 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1482);
  _closure_3908
  = (struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477*)moonbit_malloc(sizeof(struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477));
  Moonbit_object_header(_closure_3908)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477, $1) >> 2, 1, 0);
  _closure_3908->code
  = &_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1477;
  _closure_3908->$0 = _M0L5indexS1485;
  _closure_3908->$1 = _M0L8filenameS1482;
  _M0L14handle__resultS1477 = (struct _M0TWssbEu*)_closure_3908;
  _M0L17error__to__stringS1486
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1486$closure.data;
  moonbit_incref(_M0L12async__testsS1507);
  moonbit_incref(_M0L17error__to__stringS1486);
  moonbit_incref(_M0L8filenameS1482);
  moonbit_incref(_M0L14handle__resultS1477);
  #line 565 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3910
  = _M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1507, _M0L8filenameS1482, _M0L5indexS1485, _M0L14handle__resultS1477, _M0L17error__to__stringS1486);
  if (_tmp_3910.tag) {
    int32_t const _M0L5_2aokS3464 = _tmp_3910.data.ok;
    _handle__error__result_3911 = _M0L5_2aokS3464;
  } else {
    void* const _M0L6_2aerrS3465 = _tmp_3910.data.err;
    moonbit_decref(_M0L12async__testsS1507);
    moonbit_decref(_M0L17error__to__stringS1486);
    moonbit_decref(_M0L8filenameS1482);
    _M0L11_2atry__errS1501 = _M0L6_2aerrS3465;
    goto join_1500;
  }
  if (_handle__error__result_3911) {
    moonbit_decref(_M0L12async__testsS1507);
    moonbit_decref(_M0L17error__to__stringS1486);
    moonbit_decref(_M0L8filenameS1482);
    _M0L6_2atmpS3455 = 1;
  } else {
    struct moonbit_result_0 _tmp_3912;
    int32_t _handle__error__result_3913;
    moonbit_incref(_M0L12async__testsS1507);
    moonbit_incref(_M0L17error__to__stringS1486);
    moonbit_incref(_M0L8filenameS1482);
    moonbit_incref(_M0L14handle__resultS1477);
    #line 568 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    _tmp_3912
    = _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1507, _M0L8filenameS1482, _M0L5indexS1485, _M0L14handle__resultS1477, _M0L17error__to__stringS1486);
    if (_tmp_3912.tag) {
      int32_t const _M0L5_2aokS3462 = _tmp_3912.data.ok;
      _handle__error__result_3913 = _M0L5_2aokS3462;
    } else {
      void* const _M0L6_2aerrS3463 = _tmp_3912.data.err;
      moonbit_decref(_M0L12async__testsS1507);
      moonbit_decref(_M0L17error__to__stringS1486);
      moonbit_decref(_M0L8filenameS1482);
      _M0L11_2atry__errS1501 = _M0L6_2aerrS3463;
      goto join_1500;
    }
    if (_handle__error__result_3913) {
      moonbit_decref(_M0L12async__testsS1507);
      moonbit_decref(_M0L17error__to__stringS1486);
      moonbit_decref(_M0L8filenameS1482);
      _M0L6_2atmpS3455 = 1;
    } else {
      struct moonbit_result_0 _tmp_3914;
      int32_t _handle__error__result_3915;
      moonbit_incref(_M0L12async__testsS1507);
      moonbit_incref(_M0L17error__to__stringS1486);
      moonbit_incref(_M0L8filenameS1482);
      moonbit_incref(_M0L14handle__resultS1477);
      #line 571 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _tmp_3914
      = _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1507, _M0L8filenameS1482, _M0L5indexS1485, _M0L14handle__resultS1477, _M0L17error__to__stringS1486);
      if (_tmp_3914.tag) {
        int32_t const _M0L5_2aokS3460 = _tmp_3914.data.ok;
        _handle__error__result_3915 = _M0L5_2aokS3460;
      } else {
        void* const _M0L6_2aerrS3461 = _tmp_3914.data.err;
        moonbit_decref(_M0L12async__testsS1507);
        moonbit_decref(_M0L17error__to__stringS1486);
        moonbit_decref(_M0L8filenameS1482);
        _M0L11_2atry__errS1501 = _M0L6_2aerrS3461;
        goto join_1500;
      }
      if (_handle__error__result_3915) {
        moonbit_decref(_M0L12async__testsS1507);
        moonbit_decref(_M0L17error__to__stringS1486);
        moonbit_decref(_M0L8filenameS1482);
        _M0L6_2atmpS3455 = 1;
      } else {
        struct moonbit_result_0 _tmp_3916;
        int32_t _handle__error__result_3917;
        moonbit_incref(_M0L12async__testsS1507);
        moonbit_incref(_M0L17error__to__stringS1486);
        moonbit_incref(_M0L8filenameS1482);
        moonbit_incref(_M0L14handle__resultS1477);
        #line 574 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        _tmp_3916
        = _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1507, _M0L8filenameS1482, _M0L5indexS1485, _M0L14handle__resultS1477, _M0L17error__to__stringS1486);
        if (_tmp_3916.tag) {
          int32_t const _M0L5_2aokS3458 = _tmp_3916.data.ok;
          _handle__error__result_3917 = _M0L5_2aokS3458;
        } else {
          void* const _M0L6_2aerrS3459 = _tmp_3916.data.err;
          moonbit_decref(_M0L12async__testsS1507);
          moonbit_decref(_M0L17error__to__stringS1486);
          moonbit_decref(_M0L8filenameS1482);
          _M0L11_2atry__errS1501 = _M0L6_2aerrS3459;
          goto join_1500;
        }
        if (_handle__error__result_3917) {
          moonbit_decref(_M0L12async__testsS1507);
          moonbit_decref(_M0L17error__to__stringS1486);
          moonbit_decref(_M0L8filenameS1482);
          _M0L6_2atmpS3455 = 1;
        } else {
          struct moonbit_result_0 _tmp_3918;
          moonbit_incref(_M0L14handle__resultS1477);
          #line 577 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
          _tmp_3918
          = _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1507, _M0L8filenameS1482, _M0L5indexS1485, _M0L14handle__resultS1477, _M0L17error__to__stringS1486);
          if (_tmp_3918.tag) {
            int32_t const _M0L5_2aokS3456 = _tmp_3918.data.ok;
            _M0L6_2atmpS3455 = _M0L5_2aokS3456;
          } else {
            void* const _M0L6_2aerrS3457 = _tmp_3918.data.err;
            _M0L11_2atry__errS1501 = _M0L6_2aerrS3457;
            goto join_1500;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3455) {
    void* _M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3466 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3466)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3466)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1501
    = _M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3466;
    goto join_1500;
  } else {
    moonbit_decref(_M0L14handle__resultS1477);
  }
  goto joinlet_3909;
  join_1500:;
  _M0L3errS1502 = _M0L11_2atry__errS1501;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1505
  = (struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1502;
  _M0L8_2afieldS3471 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1505->$0;
  _M0L6_2acntS3807
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1505)->rc;
  if (_M0L6_2acntS3807 > 1) {
    int32_t _M0L11_2anew__cntS3808 = _M0L6_2acntS3807 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1505)->rc
    = _M0L11_2anew__cntS3808;
    moonbit_incref(_M0L8_2afieldS3471);
  } else if (_M0L6_2acntS3807 == 1) {
    #line 584 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1505);
  }
  _M0L7_2anameS1506 = _M0L8_2afieldS3471;
  _M0L4nameS1504 = _M0L7_2anameS1506;
  goto join_1503;
  goto joinlet_3919;
  join_1503:;
  #line 585 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1477(_M0L14handle__resultS1477, _M0L4nameS1504, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3919:;
  joinlet_3909:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1486(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3454,
  void* _M0L3errS1487
) {
  void* _M0L1eS1489;
  moonbit_string_t _M0L1eS1491;
  #line 554 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3454);
  switch (Moonbit_object_tag(_M0L3errS1487)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1492 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1487;
      moonbit_string_t _M0L8_2afieldS3472 = _M0L10_2aFailureS1492->$0;
      int32_t _M0L6_2acntS3809 =
        Moonbit_object_header(_M0L10_2aFailureS1492)->rc;
      moonbit_string_t _M0L4_2aeS1493;
      if (_M0L6_2acntS3809 > 1) {
        int32_t _M0L11_2anew__cntS3810 = _M0L6_2acntS3809 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1492)->rc
        = _M0L11_2anew__cntS3810;
        moonbit_incref(_M0L8_2afieldS3472);
      } else if (_M0L6_2acntS3809 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1492);
      }
      _M0L4_2aeS1493 = _M0L8_2afieldS3472;
      _M0L1eS1491 = _M0L4_2aeS1493;
      goto join_1490;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1494 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1487;
      moonbit_string_t _M0L8_2afieldS3473 = _M0L15_2aInspectErrorS1494->$0;
      int32_t _M0L6_2acntS3811 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1494)->rc;
      moonbit_string_t _M0L4_2aeS1495;
      if (_M0L6_2acntS3811 > 1) {
        int32_t _M0L11_2anew__cntS3812 = _M0L6_2acntS3811 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1494)->rc
        = _M0L11_2anew__cntS3812;
        moonbit_incref(_M0L8_2afieldS3473);
      } else if (_M0L6_2acntS3811 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1494);
      }
      _M0L4_2aeS1495 = _M0L8_2afieldS3473;
      _M0L1eS1491 = _M0L4_2aeS1495;
      goto join_1490;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1496 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1487;
      moonbit_string_t _M0L8_2afieldS3474 = _M0L16_2aSnapshotErrorS1496->$0;
      int32_t _M0L6_2acntS3813 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1496)->rc;
      moonbit_string_t _M0L4_2aeS1497;
      if (_M0L6_2acntS3813 > 1) {
        int32_t _M0L11_2anew__cntS3814 = _M0L6_2acntS3813 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1496)->rc
        = _M0L11_2anew__cntS3814;
        moonbit_incref(_M0L8_2afieldS3474);
      } else if (_M0L6_2acntS3813 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1496);
      }
      _M0L4_2aeS1497 = _M0L8_2afieldS3474;
      _M0L1eS1491 = _M0L4_2aeS1497;
      goto join_1490;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1498 =
        (struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1487;
      moonbit_string_t _M0L8_2afieldS3475 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1498->$0;
      int32_t _M0L6_2acntS3815 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1498)->rc;
      moonbit_string_t _M0L4_2aeS1499;
      if (_M0L6_2acntS3815 > 1) {
        int32_t _M0L11_2anew__cntS3816 = _M0L6_2acntS3815 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1498)->rc
        = _M0L11_2anew__cntS3816;
        moonbit_incref(_M0L8_2afieldS3475);
      } else if (_M0L6_2acntS3815 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1498);
      }
      _M0L4_2aeS1499 = _M0L8_2afieldS3475;
      _M0L1eS1491 = _M0L4_2aeS1499;
      goto join_1490;
      break;
    }
    default: {
      _M0L1eS1489 = _M0L3errS1487;
      goto join_1488;
      break;
    }
  }
  join_1490:;
  return _M0L1eS1491;
  join_1488:;
  #line 560 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1489);
}

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1477(
  struct _M0TWssbEu* _M0L6_2aenvS3440,
  moonbit_string_t _M0L8testnameS1478,
  moonbit_string_t _M0L7messageS1479,
  int32_t _M0L7skippedS1480
) {
  struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477* _M0L14_2acasted__envS3441;
  moonbit_string_t _M0L8_2afieldS3485;
  moonbit_string_t _M0L8filenameS1482;
  int32_t _M0L8_2afieldS3484;
  int32_t _M0L6_2acntS3817;
  int32_t _M0L5indexS1485;
  int32_t _if__result_3922;
  moonbit_string_t _M0L10file__nameS1481;
  moonbit_string_t _M0L10test__nameS1483;
  moonbit_string_t _M0L7messageS1484;
  moonbit_string_t _M0L6_2atmpS3453;
  moonbit_string_t _M0L6_2atmpS3483;
  moonbit_string_t _M0L6_2atmpS3452;
  moonbit_string_t _M0L6_2atmpS3482;
  moonbit_string_t _M0L6_2atmpS3450;
  moonbit_string_t _M0L6_2atmpS3451;
  moonbit_string_t _M0L6_2atmpS3481;
  moonbit_string_t _M0L6_2atmpS3449;
  moonbit_string_t _M0L6_2atmpS3480;
  moonbit_string_t _M0L6_2atmpS3447;
  moonbit_string_t _M0L6_2atmpS3448;
  moonbit_string_t _M0L6_2atmpS3479;
  moonbit_string_t _M0L6_2atmpS3446;
  moonbit_string_t _M0L6_2atmpS3478;
  moonbit_string_t _M0L6_2atmpS3444;
  moonbit_string_t _M0L6_2atmpS3445;
  moonbit_string_t _M0L6_2atmpS3477;
  moonbit_string_t _M0L6_2atmpS3443;
  moonbit_string_t _M0L6_2atmpS3476;
  moonbit_string_t _M0L6_2atmpS3442;
  #line 538 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3441
  = (struct _M0R128_24clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1477*)_M0L6_2aenvS3440;
  _M0L8_2afieldS3485 = _M0L14_2acasted__envS3441->$1;
  _M0L8filenameS1482 = _M0L8_2afieldS3485;
  _M0L8_2afieldS3484 = _M0L14_2acasted__envS3441->$0;
  _M0L6_2acntS3817 = Moonbit_object_header(_M0L14_2acasted__envS3441)->rc;
  if (_M0L6_2acntS3817 > 1) {
    int32_t _M0L11_2anew__cntS3818 = _M0L6_2acntS3817 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3441)->rc
    = _M0L11_2anew__cntS3818;
    moonbit_incref(_M0L8filenameS1482);
  } else if (_M0L6_2acntS3817 == 1) {
    #line 538 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3441);
  }
  _M0L5indexS1485 = _M0L8_2afieldS3484;
  if (!_M0L7skippedS1480) {
    _if__result_3922 = 1;
  } else {
    _if__result_3922 = 0;
  }
  if (_if__result_3922) {
    
  }
  #line 544 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1481 = _M0MPC16string6String6escape(_M0L8filenameS1482);
  #line 545 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1483 = _M0MPC16string6String6escape(_M0L8testnameS1478);
  #line 546 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1484 = _M0MPC16string6String6escape(_M0L7messageS1479);
  #line 547 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 549 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3453
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1481);
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3483
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3453);
  moonbit_decref(_M0L6_2atmpS3453);
  _M0L6_2atmpS3452 = _M0L6_2atmpS3483;
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3482
  = moonbit_add_string(_M0L6_2atmpS3452, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3452);
  _M0L6_2atmpS3450 = _M0L6_2atmpS3482;
  #line 549 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3451
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1485);
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3481 = moonbit_add_string(_M0L6_2atmpS3450, _M0L6_2atmpS3451);
  moonbit_decref(_M0L6_2atmpS3450);
  moonbit_decref(_M0L6_2atmpS3451);
  _M0L6_2atmpS3449 = _M0L6_2atmpS3481;
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3480
  = moonbit_add_string(_M0L6_2atmpS3449, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3449);
  _M0L6_2atmpS3447 = _M0L6_2atmpS3480;
  #line 549 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3448
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1483);
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3479 = moonbit_add_string(_M0L6_2atmpS3447, _M0L6_2atmpS3448);
  moonbit_decref(_M0L6_2atmpS3447);
  moonbit_decref(_M0L6_2atmpS3448);
  _M0L6_2atmpS3446 = _M0L6_2atmpS3479;
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3478
  = moonbit_add_string(_M0L6_2atmpS3446, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3446);
  _M0L6_2atmpS3444 = _M0L6_2atmpS3478;
  #line 549 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3445
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1484);
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3477 = moonbit_add_string(_M0L6_2atmpS3444, _M0L6_2atmpS3445);
  moonbit_decref(_M0L6_2atmpS3444);
  moonbit_decref(_M0L6_2atmpS3445);
  _M0L6_2atmpS3443 = _M0L6_2atmpS3477;
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3476
  = moonbit_add_string(_M0L6_2atmpS3443, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3443);
  _M0L6_2atmpS3442 = _M0L6_2atmpS3476;
  #line 548 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3442);
  #line 551 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1476,
  moonbit_string_t _M0L8filenameS1473,
  int32_t _M0L5indexS1467,
  struct _M0TWssbEu* _M0L14handle__resultS1463,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1465
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1443;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1472;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1445;
  moonbit_string_t* _M0L5attrsS1446;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1466;
  moonbit_string_t _M0L4nameS1449;
  moonbit_string_t _M0L4nameS1447;
  int32_t _M0L6_2atmpS3439;
  struct _M0TWEOs* _M0L5_2aitS1451;
  struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__* _closure_3931;
  struct _M0TWEOc* _M0L6_2atmpS3430;
  struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__* _closure_3932;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3431;
  struct moonbit_result_0 _result_3933;
  #line 412 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1476);
  moonbit_incref(_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 419 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1472
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1473);
  if (_M0L7_2abindS1472 == 0) {
    struct moonbit_result_0 _result_3924;
    if (_M0L7_2abindS1472) {
      moonbit_decref(_M0L7_2abindS1472);
    }
    moonbit_decref(_M0L17error__to__stringS1465);
    moonbit_decref(_M0L14handle__resultS1463);
    _result_3924.tag = 1;
    _result_3924.data.ok = 0;
    return _result_3924;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1474 =
      _M0L7_2abindS1472;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1475 =
      _M0L7_2aSomeS1474;
    _M0L10index__mapS1443 = _M0L13_2aindex__mapS1475;
    goto join_1442;
  }
  join_1442:;
  #line 421 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1466
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1443, _M0L5indexS1467);
  if (_M0L7_2abindS1466 == 0) {
    struct moonbit_result_0 _result_3926;
    if (_M0L7_2abindS1466) {
      moonbit_decref(_M0L7_2abindS1466);
    }
    moonbit_decref(_M0L17error__to__stringS1465);
    moonbit_decref(_M0L14handle__resultS1463);
    _result_3926.tag = 1;
    _result_3926.data.ok = 0;
    return _result_3926;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1468 =
      _M0L7_2abindS1466;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1469 = _M0L7_2aSomeS1468;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3489 = _M0L4_2axS1469->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1470 = _M0L8_2afieldS3489;
    moonbit_string_t* _M0L8_2afieldS3488 = _M0L4_2axS1469->$1;
    int32_t _M0L6_2acntS3819 = Moonbit_object_header(_M0L4_2axS1469)->rc;
    moonbit_string_t* _M0L8_2aattrsS1471;
    if (_M0L6_2acntS3819 > 1) {
      int32_t _M0L11_2anew__cntS3820 = _M0L6_2acntS3819 - 1;
      Moonbit_object_header(_M0L4_2axS1469)->rc = _M0L11_2anew__cntS3820;
      moonbit_incref(_M0L8_2afieldS3488);
      moonbit_incref(_M0L4_2afS1470);
    } else if (_M0L6_2acntS3819 == 1) {
      #line 419 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1469);
    }
    _M0L8_2aattrsS1471 = _M0L8_2afieldS3488;
    _M0L1fS1445 = _M0L4_2afS1470;
    _M0L5attrsS1446 = _M0L8_2aattrsS1471;
    goto join_1444;
  }
  join_1444:;
  _M0L6_2atmpS3439 = Moonbit_array_length(_M0L5attrsS1446);
  if (_M0L6_2atmpS3439 >= 1) {
    moonbit_string_t _M0L6_2atmpS3487 = (moonbit_string_t)_M0L5attrsS1446[0];
    moonbit_string_t _M0L7_2anameS1450 = _M0L6_2atmpS3487;
    moonbit_incref(_M0L7_2anameS1450);
    _M0L4nameS1449 = _M0L7_2anameS1450;
    goto join_1448;
  } else {
    _M0L4nameS1447 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3927;
  join_1448:;
  _M0L4nameS1447 = _M0L4nameS1449;
  joinlet_3927:;
  #line 422 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1451 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1446);
  while (1) {
    moonbit_string_t _M0L4attrS1453;
    moonbit_string_t _M0L7_2abindS1460;
    int32_t _M0L6_2atmpS3423;
    int64_t _M0L6_2atmpS3422;
    moonbit_incref(_M0L5_2aitS1451);
    #line 424 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1460 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1451);
    if (_M0L7_2abindS1460 == 0) {
      if (_M0L7_2abindS1460) {
        moonbit_decref(_M0L7_2abindS1460);
      }
      moonbit_decref(_M0L5_2aitS1451);
    } else {
      moonbit_string_t _M0L7_2aSomeS1461 = _M0L7_2abindS1460;
      moonbit_string_t _M0L7_2aattrS1462 = _M0L7_2aSomeS1461;
      _M0L4attrS1453 = _M0L7_2aattrS1462;
      goto join_1452;
    }
    goto joinlet_3929;
    join_1452:;
    _M0L6_2atmpS3423 = Moonbit_array_length(_M0L4attrS1453);
    _M0L6_2atmpS3422 = (int64_t)_M0L6_2atmpS3423;
    moonbit_incref(_M0L4attrS1453);
    #line 425 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1453, 5, 0, _M0L6_2atmpS3422)
    ) {
      int32_t _M0L6_2atmpS3429 = _M0L4attrS1453[0];
      int32_t _M0L4_2axS1454 = _M0L6_2atmpS3429;
      if (_M0L4_2axS1454 == 112) {
        int32_t _M0L6_2atmpS3428 = _M0L4attrS1453[1];
        int32_t _M0L4_2axS1455 = _M0L6_2atmpS3428;
        if (_M0L4_2axS1455 == 97) {
          int32_t _M0L6_2atmpS3427 = _M0L4attrS1453[2];
          int32_t _M0L4_2axS1456 = _M0L6_2atmpS3427;
          if (_M0L4_2axS1456 == 110) {
            int32_t _M0L6_2atmpS3426 = _M0L4attrS1453[3];
            int32_t _M0L4_2axS1457 = _M0L6_2atmpS3426;
            if (_M0L4_2axS1457 == 105) {
              int32_t _M0L6_2atmpS3486 = _M0L4attrS1453[4];
              int32_t _M0L6_2atmpS3425;
              int32_t _M0L4_2axS1458;
              moonbit_decref(_M0L4attrS1453);
              _M0L6_2atmpS3425 = _M0L6_2atmpS3486;
              _M0L4_2axS1458 = _M0L6_2atmpS3425;
              if (_M0L4_2axS1458 == 99) {
                void* _M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3424;
                struct moonbit_result_0 _result_3930;
                moonbit_decref(_M0L17error__to__stringS1465);
                moonbit_decref(_M0L14handle__resultS1463);
                moonbit_decref(_M0L5_2aitS1451);
                moonbit_decref(_M0L1fS1445);
                _M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3424
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3424)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3424)->$0
                = _M0L4nameS1447;
                _result_3930.tag = 0;
                _result_3930.data.err
                = _M0L126clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3424;
                return _result_3930;
              }
            } else {
              moonbit_decref(_M0L4attrS1453);
            }
          } else {
            moonbit_decref(_M0L4attrS1453);
          }
        } else {
          moonbit_decref(_M0L4attrS1453);
        }
      } else {
        moonbit_decref(_M0L4attrS1453);
      }
    } else {
      moonbit_decref(_M0L4attrS1453);
    }
    continue;
    joinlet_3929:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1463);
  moonbit_incref(_M0L4nameS1447);
  _closure_3931
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__*)moonbit_malloc(sizeof(struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__));
  Moonbit_object_header(_closure_3931)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__, $0) >> 2, 2, 0);
  _closure_3931->code
  = &_M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testC3436l432;
  _closure_3931->$0 = _M0L14handle__resultS1463;
  _closure_3931->$1 = _M0L4nameS1447;
  _M0L6_2atmpS3430 = (struct _M0TWEOc*)_closure_3931;
  _closure_3932
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__*)moonbit_malloc(sizeof(struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__));
  Moonbit_object_header(_closure_3932)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__, $0) >> 2, 3, 0);
  _closure_3932->code
  = &_M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testC3432l433;
  _closure_3932->$0 = _M0L17error__to__stringS1465;
  _closure_3932->$1 = _M0L14handle__resultS1463;
  _closure_3932->$2 = _M0L4nameS1447;
  _M0L6_2atmpS3431 = (struct _M0TWRPC15error5ErrorEu*)_closure_3932;
  #line 430 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1445, _M0L6_2atmpS3430, _M0L6_2atmpS3431);
  _result_3933.tag = 1;
  _result_3933.data.ok = 1;
  return _result_3933;
}

int32_t _M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testC3436l432(
  struct _M0TWEOc* _M0L6_2aenvS3437
) {
  struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__* _M0L14_2acasted__envS3438;
  moonbit_string_t _M0L8_2afieldS3491;
  moonbit_string_t _M0L4nameS1447;
  struct _M0TWssbEu* _M0L8_2afieldS3490;
  int32_t _M0L6_2acntS3821;
  struct _M0TWssbEu* _M0L14handle__resultS1463;
  #line 432 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3438
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3436__l432__*)_M0L6_2aenvS3437;
  _M0L8_2afieldS3491 = _M0L14_2acasted__envS3438->$1;
  _M0L4nameS1447 = _M0L8_2afieldS3491;
  _M0L8_2afieldS3490 = _M0L14_2acasted__envS3438->$0;
  _M0L6_2acntS3821 = Moonbit_object_header(_M0L14_2acasted__envS3438)->rc;
  if (_M0L6_2acntS3821 > 1) {
    int32_t _M0L11_2anew__cntS3822 = _M0L6_2acntS3821 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3438)->rc
    = _M0L11_2anew__cntS3822;
    moonbit_incref(_M0L4nameS1447);
    moonbit_incref(_M0L8_2afieldS3490);
  } else if (_M0L6_2acntS3821 == 1) {
    #line 432 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3438);
  }
  _M0L14handle__resultS1463 = _M0L8_2afieldS3490;
  #line 432 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1463->code(_M0L14handle__resultS1463, _M0L4nameS1447, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal21fuzzy__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testC3432l433(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3433,
  void* _M0L3errS1464
) {
  struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__* _M0L14_2acasted__envS3434;
  moonbit_string_t _M0L8_2afieldS3494;
  moonbit_string_t _M0L4nameS1447;
  struct _M0TWssbEu* _M0L8_2afieldS3493;
  struct _M0TWssbEu* _M0L14handle__resultS1463;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3492;
  int32_t _M0L6_2acntS3823;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1465;
  moonbit_string_t _M0L6_2atmpS3435;
  #line 433 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3434
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ffuzzy__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3432__l433__*)_M0L6_2aenvS3433;
  _M0L8_2afieldS3494 = _M0L14_2acasted__envS3434->$2;
  _M0L4nameS1447 = _M0L8_2afieldS3494;
  _M0L8_2afieldS3493 = _M0L14_2acasted__envS3434->$1;
  _M0L14handle__resultS1463 = _M0L8_2afieldS3493;
  _M0L8_2afieldS3492 = _M0L14_2acasted__envS3434->$0;
  _M0L6_2acntS3823 = Moonbit_object_header(_M0L14_2acasted__envS3434)->rc;
  if (_M0L6_2acntS3823 > 1) {
    int32_t _M0L11_2anew__cntS3824 = _M0L6_2acntS3823 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3434)->rc
    = _M0L11_2anew__cntS3824;
    moonbit_incref(_M0L4nameS1447);
    moonbit_incref(_M0L14handle__resultS1463);
    moonbit_incref(_M0L8_2afieldS3492);
  } else if (_M0L6_2acntS3823 == 1) {
    #line 433 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3434);
  }
  _M0L17error__to__stringS1465 = _M0L8_2afieldS3492;
  #line 433 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3435
  = _M0L17error__to__stringS1465->code(_M0L17error__to__stringS1465, _M0L3errS1464);
  #line 433 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1463->code(_M0L14handle__resultS1463, _M0L4nameS1447, _M0L6_2atmpS3435, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1436,
  struct _M0TWEOc* _M0L6on__okS1437,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1434
) {
  void* _M0L11_2atry__errS1432;
  struct moonbit_result_0 _tmp_3935;
  void* _M0L3errS1433;
  #line 375 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3935 = _M0L1fS1436->code(_M0L1fS1436);
  if (_tmp_3935.tag) {
    int32_t const _M0L5_2aokS3420 = _tmp_3935.data.ok;
    moonbit_decref(_M0L7on__errS1434);
  } else {
    void* const _M0L6_2aerrS3421 = _tmp_3935.data.err;
    moonbit_decref(_M0L6on__okS1437);
    _M0L11_2atry__errS1432 = _M0L6_2aerrS3421;
    goto join_1431;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1437->code(_M0L6on__okS1437);
  goto joinlet_3934;
  join_1431:;
  _M0L3errS1433 = _M0L11_2atry__errS1432;
  #line 383 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1434->code(_M0L7on__errS1434, _M0L3errS1433);
  joinlet_3934:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1391;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1404;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1409;
  struct _M0TUsiE** _M0L6_2atmpS3419;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1416;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1417;
  moonbit_string_t _M0L6_2atmpS3418;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1418;
  int32_t _M0L7_2abindS1419;
  int32_t _M0L2__S1420;
  #line 193 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1391 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1404
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1409 = 0;
  _M0L6_2atmpS3419 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1416
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1416)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1416->$0 = _M0L6_2atmpS3419;
  _M0L16file__and__indexS1416->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1417
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1404(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1404);
  #line 284 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3418 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1417, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1418
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1409(_M0L51moonbit__test__driver__internal__split__mbt__stringS1409, _M0L6_2atmpS3418, 47);
  _M0L7_2abindS1419 = _M0L10test__argsS1418->$1;
  _M0L2__S1420 = 0;
  while (1) {
    if (_M0L2__S1420 < _M0L7_2abindS1419) {
      moonbit_string_t* _M0L8_2afieldS3496 = _M0L10test__argsS1418->$0;
      moonbit_string_t* _M0L3bufS3417 = _M0L8_2afieldS3496;
      moonbit_string_t _M0L6_2atmpS3495 =
        (moonbit_string_t)_M0L3bufS3417[_M0L2__S1420];
      moonbit_string_t _M0L3argS1421 = _M0L6_2atmpS3495;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1422;
      moonbit_string_t _M0L4fileS1423;
      moonbit_string_t _M0L5rangeS1424;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1425;
      moonbit_string_t _M0L6_2atmpS3415;
      int32_t _M0L5startS1426;
      moonbit_string_t _M0L6_2atmpS3414;
      int32_t _M0L3endS1427;
      int32_t _M0L1iS1428;
      int32_t _M0L6_2atmpS3416;
      moonbit_incref(_M0L3argS1421);
      #line 288 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1422
      = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1409(_M0L51moonbit__test__driver__internal__split__mbt__stringS1409, _M0L3argS1421, 58);
      moonbit_incref(_M0L16file__and__rangeS1422);
      #line 289 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1423
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1422, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1424
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1422, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1425
      = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1409(_M0L51moonbit__test__driver__internal__split__mbt__stringS1409, _M0L5rangeS1424, 45);
      moonbit_incref(_M0L15start__and__endS1425);
      #line 294 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3415
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1425, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1426
      = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1391(_M0L45moonbit__test__driver__internal__parse__int__S1391, _M0L6_2atmpS3415);
      #line 295 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3414
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1425, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1427
      = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1391(_M0L45moonbit__test__driver__internal__parse__int__S1391, _M0L6_2atmpS3414);
      _M0L1iS1428 = _M0L5startS1426;
      while (1) {
        if (_M0L1iS1428 < _M0L3endS1427) {
          struct _M0TUsiE* _M0L8_2atupleS3412;
          int32_t _M0L6_2atmpS3413;
          moonbit_incref(_M0L4fileS1423);
          _M0L8_2atupleS3412
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3412)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3412->$0 = _M0L4fileS1423;
          _M0L8_2atupleS3412->$1 = _M0L1iS1428;
          moonbit_incref(_M0L16file__and__indexS1416);
          #line 297 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1416, _M0L8_2atupleS3412);
          _M0L6_2atmpS3413 = _M0L1iS1428 + 1;
          _M0L1iS1428 = _M0L6_2atmpS3413;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1423);
        }
        break;
      }
      _M0L6_2atmpS3416 = _M0L2__S1420 + 1;
      _M0L2__S1420 = _M0L6_2atmpS3416;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1418);
    }
    break;
  }
  return _M0L16file__and__indexS1416;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1409(
  int32_t _M0L6_2aenvS3393,
  moonbit_string_t _M0L1sS1410,
  int32_t _M0L3sepS1411
) {
  moonbit_string_t* _M0L6_2atmpS3411;
  struct _M0TPB5ArrayGsE* _M0L3resS1412;
  struct _M0TPC13ref3RefGiE* _M0L1iS1413;
  struct _M0TPC13ref3RefGiE* _M0L5startS1414;
  int32_t _M0L3valS3406;
  int32_t _M0L6_2atmpS3407;
  #line 261 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3411 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1412
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1412)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1412->$0 = _M0L6_2atmpS3411;
  _M0L3resS1412->$1 = 0;
  _M0L1iS1413
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1413)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1413->$0 = 0;
  _M0L5startS1414
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1414)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1414->$0 = 0;
  while (1) {
    int32_t _M0L3valS3394 = _M0L1iS1413->$0;
    int32_t _M0L6_2atmpS3395 = Moonbit_array_length(_M0L1sS1410);
    if (_M0L3valS3394 < _M0L6_2atmpS3395) {
      int32_t _M0L3valS3398 = _M0L1iS1413->$0;
      int32_t _M0L6_2atmpS3397;
      int32_t _M0L6_2atmpS3396;
      int32_t _M0L3valS3405;
      int32_t _M0L6_2atmpS3404;
      if (
        _M0L3valS3398 < 0
        || _M0L3valS3398 >= Moonbit_array_length(_M0L1sS1410)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3397 = _M0L1sS1410[_M0L3valS3398];
      _M0L6_2atmpS3396 = _M0L6_2atmpS3397;
      if (_M0L6_2atmpS3396 == _M0L3sepS1411) {
        int32_t _M0L3valS3400 = _M0L5startS1414->$0;
        int32_t _M0L3valS3401 = _M0L1iS1413->$0;
        moonbit_string_t _M0L6_2atmpS3399;
        int32_t _M0L3valS3403;
        int32_t _M0L6_2atmpS3402;
        moonbit_incref(_M0L1sS1410);
        #line 270 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS3399
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1410, _M0L3valS3400, _M0L3valS3401);
        moonbit_incref(_M0L3resS1412);
        #line 270 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1412, _M0L6_2atmpS3399);
        _M0L3valS3403 = _M0L1iS1413->$0;
        _M0L6_2atmpS3402 = _M0L3valS3403 + 1;
        _M0L5startS1414->$0 = _M0L6_2atmpS3402;
      }
      _M0L3valS3405 = _M0L1iS1413->$0;
      _M0L6_2atmpS3404 = _M0L3valS3405 + 1;
      _M0L1iS1413->$0 = _M0L6_2atmpS3404;
      continue;
    } else {
      moonbit_decref(_M0L1iS1413);
    }
    break;
  }
  _M0L3valS3406 = _M0L5startS1414->$0;
  _M0L6_2atmpS3407 = Moonbit_array_length(_M0L1sS1410);
  if (_M0L3valS3406 < _M0L6_2atmpS3407) {
    int32_t _M0L8_2afieldS3497 = _M0L5startS1414->$0;
    int32_t _M0L3valS3409;
    int32_t _M0L6_2atmpS3410;
    moonbit_string_t _M0L6_2atmpS3408;
    moonbit_decref(_M0L5startS1414);
    _M0L3valS3409 = _M0L8_2afieldS3497;
    _M0L6_2atmpS3410 = Moonbit_array_length(_M0L1sS1410);
    #line 276 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS3408
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1410, _M0L3valS3409, _M0L6_2atmpS3410);
    moonbit_incref(_M0L3resS1412);
    #line 276 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1412, _M0L6_2atmpS3408);
  } else {
    moonbit_decref(_M0L5startS1414);
    moonbit_decref(_M0L1sS1410);
  }
  return _M0L3resS1412;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1404(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397
) {
  moonbit_bytes_t* _M0L3tmpS1405;
  int32_t _M0L6_2atmpS3392;
  struct _M0TPB5ArrayGsE* _M0L3resS1406;
  int32_t _M0L1iS1407;
  #line 250 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1405
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3392 = Moonbit_array_length(_M0L3tmpS1405);
  #line 254 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1406 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3392);
  _M0L1iS1407 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3388 = Moonbit_array_length(_M0L3tmpS1405);
    if (_M0L1iS1407 < _M0L6_2atmpS3388) {
      moonbit_bytes_t _M0L6_2atmpS3498;
      moonbit_bytes_t _M0L6_2atmpS3390;
      moonbit_string_t _M0L6_2atmpS3389;
      int32_t _M0L6_2atmpS3391;
      if (
        _M0L1iS1407 < 0 || _M0L1iS1407 >= Moonbit_array_length(_M0L3tmpS1405)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3498 = (moonbit_bytes_t)_M0L3tmpS1405[_M0L1iS1407];
      _M0L6_2atmpS3390 = _M0L6_2atmpS3498;
      moonbit_incref(_M0L6_2atmpS3390);
      #line 256 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3389
      = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397, _M0L6_2atmpS3390);
      moonbit_incref(_M0L3resS1406);
      #line 256 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1406, _M0L6_2atmpS3389);
      _M0L6_2atmpS3391 = _M0L1iS1407 + 1;
      _M0L1iS1407 = _M0L6_2atmpS3391;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1405);
    }
    break;
  }
  return _M0L3resS1406;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1397(
  int32_t _M0L6_2aenvS3302,
  moonbit_bytes_t _M0L5bytesS1398
) {
  struct _M0TPB13StringBuilder* _M0L3resS1399;
  int32_t _M0L3lenS1400;
  struct _M0TPC13ref3RefGiE* _M0L1iS1401;
  #line 206 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1399 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1400 = Moonbit_array_length(_M0L5bytesS1398);
  _M0L1iS1401
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1401)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1401->$0 = 0;
  while (1) {
    int32_t _M0L3valS3303 = _M0L1iS1401->$0;
    if (_M0L3valS3303 < _M0L3lenS1400) {
      int32_t _M0L3valS3387 = _M0L1iS1401->$0;
      int32_t _M0L6_2atmpS3386;
      int32_t _M0L6_2atmpS3385;
      struct _M0TPC13ref3RefGiE* _M0L1cS1402;
      int32_t _M0L3valS3304;
      if (
        _M0L3valS3387 < 0
        || _M0L3valS3387 >= Moonbit_array_length(_M0L5bytesS1398)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3386 = _M0L5bytesS1398[_M0L3valS3387];
      _M0L6_2atmpS3385 = (int32_t)_M0L6_2atmpS3386;
      _M0L1cS1402
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1402)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1402->$0 = _M0L6_2atmpS3385;
      _M0L3valS3304 = _M0L1cS1402->$0;
      if (_M0L3valS3304 < 128) {
        int32_t _M0L8_2afieldS3499 = _M0L1cS1402->$0;
        int32_t _M0L3valS3306;
        int32_t _M0L6_2atmpS3305;
        int32_t _M0L3valS3308;
        int32_t _M0L6_2atmpS3307;
        moonbit_decref(_M0L1cS1402);
        _M0L3valS3306 = _M0L8_2afieldS3499;
        _M0L6_2atmpS3305 = _M0L3valS3306;
        moonbit_incref(_M0L3resS1399);
        #line 215 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1399, _M0L6_2atmpS3305);
        _M0L3valS3308 = _M0L1iS1401->$0;
        _M0L6_2atmpS3307 = _M0L3valS3308 + 1;
        _M0L1iS1401->$0 = _M0L6_2atmpS3307;
      } else {
        int32_t _M0L3valS3309 = _M0L1cS1402->$0;
        if (_M0L3valS3309 < 224) {
          int32_t _M0L3valS3311 = _M0L1iS1401->$0;
          int32_t _M0L6_2atmpS3310 = _M0L3valS3311 + 1;
          int32_t _M0L3valS3320;
          int32_t _M0L6_2atmpS3319;
          int32_t _M0L6_2atmpS3313;
          int32_t _M0L3valS3318;
          int32_t _M0L6_2atmpS3317;
          int32_t _M0L6_2atmpS3316;
          int32_t _M0L6_2atmpS3315;
          int32_t _M0L6_2atmpS3314;
          int32_t _M0L6_2atmpS3312;
          int32_t _M0L8_2afieldS3500;
          int32_t _M0L3valS3322;
          int32_t _M0L6_2atmpS3321;
          int32_t _M0L3valS3324;
          int32_t _M0L6_2atmpS3323;
          if (_M0L6_2atmpS3310 >= _M0L3lenS1400) {
            moonbit_decref(_M0L1cS1402);
            moonbit_decref(_M0L1iS1401);
            moonbit_decref(_M0L5bytesS1398);
            break;
          }
          _M0L3valS3320 = _M0L1cS1402->$0;
          _M0L6_2atmpS3319 = _M0L3valS3320 & 31;
          _M0L6_2atmpS3313 = _M0L6_2atmpS3319 << 6;
          _M0L3valS3318 = _M0L1iS1401->$0;
          _M0L6_2atmpS3317 = _M0L3valS3318 + 1;
          if (
            _M0L6_2atmpS3317 < 0
            || _M0L6_2atmpS3317 >= Moonbit_array_length(_M0L5bytesS1398)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3316 = _M0L5bytesS1398[_M0L6_2atmpS3317];
          _M0L6_2atmpS3315 = (int32_t)_M0L6_2atmpS3316;
          _M0L6_2atmpS3314 = _M0L6_2atmpS3315 & 63;
          _M0L6_2atmpS3312 = _M0L6_2atmpS3313 | _M0L6_2atmpS3314;
          _M0L1cS1402->$0 = _M0L6_2atmpS3312;
          _M0L8_2afieldS3500 = _M0L1cS1402->$0;
          moonbit_decref(_M0L1cS1402);
          _M0L3valS3322 = _M0L8_2afieldS3500;
          _M0L6_2atmpS3321 = _M0L3valS3322;
          moonbit_incref(_M0L3resS1399);
          #line 222 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1399, _M0L6_2atmpS3321);
          _M0L3valS3324 = _M0L1iS1401->$0;
          _M0L6_2atmpS3323 = _M0L3valS3324 + 2;
          _M0L1iS1401->$0 = _M0L6_2atmpS3323;
        } else {
          int32_t _M0L3valS3325 = _M0L1cS1402->$0;
          if (_M0L3valS3325 < 240) {
            int32_t _M0L3valS3327 = _M0L1iS1401->$0;
            int32_t _M0L6_2atmpS3326 = _M0L3valS3327 + 2;
            int32_t _M0L3valS3343;
            int32_t _M0L6_2atmpS3342;
            int32_t _M0L6_2atmpS3335;
            int32_t _M0L3valS3341;
            int32_t _M0L6_2atmpS3340;
            int32_t _M0L6_2atmpS3339;
            int32_t _M0L6_2atmpS3338;
            int32_t _M0L6_2atmpS3337;
            int32_t _M0L6_2atmpS3336;
            int32_t _M0L6_2atmpS3329;
            int32_t _M0L3valS3334;
            int32_t _M0L6_2atmpS3333;
            int32_t _M0L6_2atmpS3332;
            int32_t _M0L6_2atmpS3331;
            int32_t _M0L6_2atmpS3330;
            int32_t _M0L6_2atmpS3328;
            int32_t _M0L8_2afieldS3501;
            int32_t _M0L3valS3345;
            int32_t _M0L6_2atmpS3344;
            int32_t _M0L3valS3347;
            int32_t _M0L6_2atmpS3346;
            if (_M0L6_2atmpS3326 >= _M0L3lenS1400) {
              moonbit_decref(_M0L1cS1402);
              moonbit_decref(_M0L1iS1401);
              moonbit_decref(_M0L5bytesS1398);
              break;
            }
            _M0L3valS3343 = _M0L1cS1402->$0;
            _M0L6_2atmpS3342 = _M0L3valS3343 & 15;
            _M0L6_2atmpS3335 = _M0L6_2atmpS3342 << 12;
            _M0L3valS3341 = _M0L1iS1401->$0;
            _M0L6_2atmpS3340 = _M0L3valS3341 + 1;
            if (
              _M0L6_2atmpS3340 < 0
              || _M0L6_2atmpS3340 >= Moonbit_array_length(_M0L5bytesS1398)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3339 = _M0L5bytesS1398[_M0L6_2atmpS3340];
            _M0L6_2atmpS3338 = (int32_t)_M0L6_2atmpS3339;
            _M0L6_2atmpS3337 = _M0L6_2atmpS3338 & 63;
            _M0L6_2atmpS3336 = _M0L6_2atmpS3337 << 6;
            _M0L6_2atmpS3329 = _M0L6_2atmpS3335 | _M0L6_2atmpS3336;
            _M0L3valS3334 = _M0L1iS1401->$0;
            _M0L6_2atmpS3333 = _M0L3valS3334 + 2;
            if (
              _M0L6_2atmpS3333 < 0
              || _M0L6_2atmpS3333 >= Moonbit_array_length(_M0L5bytesS1398)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3332 = _M0L5bytesS1398[_M0L6_2atmpS3333];
            _M0L6_2atmpS3331 = (int32_t)_M0L6_2atmpS3332;
            _M0L6_2atmpS3330 = _M0L6_2atmpS3331 & 63;
            _M0L6_2atmpS3328 = _M0L6_2atmpS3329 | _M0L6_2atmpS3330;
            _M0L1cS1402->$0 = _M0L6_2atmpS3328;
            _M0L8_2afieldS3501 = _M0L1cS1402->$0;
            moonbit_decref(_M0L1cS1402);
            _M0L3valS3345 = _M0L8_2afieldS3501;
            _M0L6_2atmpS3344 = _M0L3valS3345;
            moonbit_incref(_M0L3resS1399);
            #line 231 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1399, _M0L6_2atmpS3344);
            _M0L3valS3347 = _M0L1iS1401->$0;
            _M0L6_2atmpS3346 = _M0L3valS3347 + 3;
            _M0L1iS1401->$0 = _M0L6_2atmpS3346;
          } else {
            int32_t _M0L3valS3349 = _M0L1iS1401->$0;
            int32_t _M0L6_2atmpS3348 = _M0L3valS3349 + 3;
            int32_t _M0L3valS3372;
            int32_t _M0L6_2atmpS3371;
            int32_t _M0L6_2atmpS3364;
            int32_t _M0L3valS3370;
            int32_t _M0L6_2atmpS3369;
            int32_t _M0L6_2atmpS3368;
            int32_t _M0L6_2atmpS3367;
            int32_t _M0L6_2atmpS3366;
            int32_t _M0L6_2atmpS3365;
            int32_t _M0L6_2atmpS3357;
            int32_t _M0L3valS3363;
            int32_t _M0L6_2atmpS3362;
            int32_t _M0L6_2atmpS3361;
            int32_t _M0L6_2atmpS3360;
            int32_t _M0L6_2atmpS3359;
            int32_t _M0L6_2atmpS3358;
            int32_t _M0L6_2atmpS3351;
            int32_t _M0L3valS3356;
            int32_t _M0L6_2atmpS3355;
            int32_t _M0L6_2atmpS3354;
            int32_t _M0L6_2atmpS3353;
            int32_t _M0L6_2atmpS3352;
            int32_t _M0L6_2atmpS3350;
            int32_t _M0L3valS3374;
            int32_t _M0L6_2atmpS3373;
            int32_t _M0L3valS3378;
            int32_t _M0L6_2atmpS3377;
            int32_t _M0L6_2atmpS3376;
            int32_t _M0L6_2atmpS3375;
            int32_t _M0L8_2afieldS3502;
            int32_t _M0L3valS3382;
            int32_t _M0L6_2atmpS3381;
            int32_t _M0L6_2atmpS3380;
            int32_t _M0L6_2atmpS3379;
            int32_t _M0L3valS3384;
            int32_t _M0L6_2atmpS3383;
            if (_M0L6_2atmpS3348 >= _M0L3lenS1400) {
              moonbit_decref(_M0L1cS1402);
              moonbit_decref(_M0L1iS1401);
              moonbit_decref(_M0L5bytesS1398);
              break;
            }
            _M0L3valS3372 = _M0L1cS1402->$0;
            _M0L6_2atmpS3371 = _M0L3valS3372 & 7;
            _M0L6_2atmpS3364 = _M0L6_2atmpS3371 << 18;
            _M0L3valS3370 = _M0L1iS1401->$0;
            _M0L6_2atmpS3369 = _M0L3valS3370 + 1;
            if (
              _M0L6_2atmpS3369 < 0
              || _M0L6_2atmpS3369 >= Moonbit_array_length(_M0L5bytesS1398)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3368 = _M0L5bytesS1398[_M0L6_2atmpS3369];
            _M0L6_2atmpS3367 = (int32_t)_M0L6_2atmpS3368;
            _M0L6_2atmpS3366 = _M0L6_2atmpS3367 & 63;
            _M0L6_2atmpS3365 = _M0L6_2atmpS3366 << 12;
            _M0L6_2atmpS3357 = _M0L6_2atmpS3364 | _M0L6_2atmpS3365;
            _M0L3valS3363 = _M0L1iS1401->$0;
            _M0L6_2atmpS3362 = _M0L3valS3363 + 2;
            if (
              _M0L6_2atmpS3362 < 0
              || _M0L6_2atmpS3362 >= Moonbit_array_length(_M0L5bytesS1398)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3361 = _M0L5bytesS1398[_M0L6_2atmpS3362];
            _M0L6_2atmpS3360 = (int32_t)_M0L6_2atmpS3361;
            _M0L6_2atmpS3359 = _M0L6_2atmpS3360 & 63;
            _M0L6_2atmpS3358 = _M0L6_2atmpS3359 << 6;
            _M0L6_2atmpS3351 = _M0L6_2atmpS3357 | _M0L6_2atmpS3358;
            _M0L3valS3356 = _M0L1iS1401->$0;
            _M0L6_2atmpS3355 = _M0L3valS3356 + 3;
            if (
              _M0L6_2atmpS3355 < 0
              || _M0L6_2atmpS3355 >= Moonbit_array_length(_M0L5bytesS1398)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3354 = _M0L5bytesS1398[_M0L6_2atmpS3355];
            _M0L6_2atmpS3353 = (int32_t)_M0L6_2atmpS3354;
            _M0L6_2atmpS3352 = _M0L6_2atmpS3353 & 63;
            _M0L6_2atmpS3350 = _M0L6_2atmpS3351 | _M0L6_2atmpS3352;
            _M0L1cS1402->$0 = _M0L6_2atmpS3350;
            _M0L3valS3374 = _M0L1cS1402->$0;
            _M0L6_2atmpS3373 = _M0L3valS3374 - 65536;
            _M0L1cS1402->$0 = _M0L6_2atmpS3373;
            _M0L3valS3378 = _M0L1cS1402->$0;
            _M0L6_2atmpS3377 = _M0L3valS3378 >> 10;
            _M0L6_2atmpS3376 = _M0L6_2atmpS3377 + 55296;
            _M0L6_2atmpS3375 = _M0L6_2atmpS3376;
            moonbit_incref(_M0L3resS1399);
            #line 242 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1399, _M0L6_2atmpS3375);
            _M0L8_2afieldS3502 = _M0L1cS1402->$0;
            moonbit_decref(_M0L1cS1402);
            _M0L3valS3382 = _M0L8_2afieldS3502;
            _M0L6_2atmpS3381 = _M0L3valS3382 & 1023;
            _M0L6_2atmpS3380 = _M0L6_2atmpS3381 + 56320;
            _M0L6_2atmpS3379 = _M0L6_2atmpS3380;
            moonbit_incref(_M0L3resS1399);
            #line 243 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1399, _M0L6_2atmpS3379);
            _M0L3valS3384 = _M0L1iS1401->$0;
            _M0L6_2atmpS3383 = _M0L3valS3384 + 4;
            _M0L1iS1401->$0 = _M0L6_2atmpS3383;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1401);
      moonbit_decref(_M0L5bytesS1398);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1399);
}

int32_t _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1391(
  int32_t _M0L6_2aenvS3295,
  moonbit_string_t _M0L1sS1392
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1393;
  int32_t _M0L3lenS1394;
  int32_t _M0L1iS1395;
  int32_t _M0L8_2afieldS3503;
  #line 197 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1393
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1393)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1393->$0 = 0;
  _M0L3lenS1394 = Moonbit_array_length(_M0L1sS1392);
  _M0L1iS1395 = 0;
  while (1) {
    if (_M0L1iS1395 < _M0L3lenS1394) {
      int32_t _M0L3valS3300 = _M0L3resS1393->$0;
      int32_t _M0L6_2atmpS3297 = _M0L3valS3300 * 10;
      int32_t _M0L6_2atmpS3299;
      int32_t _M0L6_2atmpS3298;
      int32_t _M0L6_2atmpS3296;
      int32_t _M0L6_2atmpS3301;
      if (
        _M0L1iS1395 < 0 || _M0L1iS1395 >= Moonbit_array_length(_M0L1sS1392)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3299 = _M0L1sS1392[_M0L1iS1395];
      _M0L6_2atmpS3298 = _M0L6_2atmpS3299 - 48;
      _M0L6_2atmpS3296 = _M0L6_2atmpS3297 + _M0L6_2atmpS3298;
      _M0L3resS1393->$0 = _M0L6_2atmpS3296;
      _M0L6_2atmpS3301 = _M0L1iS1395 + 1;
      _M0L1iS1395 = _M0L6_2atmpS3301;
      continue;
    } else {
      moonbit_decref(_M0L1sS1392);
    }
    break;
  }
  _M0L8_2afieldS3503 = _M0L3resS1393->$0;
  moonbit_decref(_M0L3resS1393);
  return _M0L8_2afieldS3503;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1371,
  moonbit_string_t _M0L12_2adiscard__S1372,
  int32_t _M0L12_2adiscard__S1373,
  struct _M0TWssbEu* _M0L12_2adiscard__S1374,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1375
) {
  struct moonbit_result_0 _result_3942;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1375);
  moonbit_decref(_M0L12_2adiscard__S1374);
  moonbit_decref(_M0L12_2adiscard__S1372);
  moonbit_decref(_M0L12_2adiscard__S1371);
  _result_3942.tag = 1;
  _result_3942.data.ok = 0;
  return _result_3942;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1376,
  moonbit_string_t _M0L12_2adiscard__S1377,
  int32_t _M0L12_2adiscard__S1378,
  struct _M0TWssbEu* _M0L12_2adiscard__S1379,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1380
) {
  struct moonbit_result_0 _result_3943;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1380);
  moonbit_decref(_M0L12_2adiscard__S1379);
  moonbit_decref(_M0L12_2adiscard__S1377);
  moonbit_decref(_M0L12_2adiscard__S1376);
  _result_3943.tag = 1;
  _result_3943.data.ok = 0;
  return _result_3943;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1381,
  moonbit_string_t _M0L12_2adiscard__S1382,
  int32_t _M0L12_2adiscard__S1383,
  struct _M0TWssbEu* _M0L12_2adiscard__S1384,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1385
) {
  struct moonbit_result_0 _result_3944;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1385);
  moonbit_decref(_M0L12_2adiscard__S1384);
  moonbit_decref(_M0L12_2adiscard__S1382);
  moonbit_decref(_M0L12_2adiscard__S1381);
  _result_3944.tag = 1;
  _result_3944.data.ok = 0;
  return _result_3944;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21fuzzy__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1386,
  moonbit_string_t _M0L12_2adiscard__S1387,
  int32_t _M0L12_2adiscard__S1388,
  struct _M0TWssbEu* _M0L12_2adiscard__S1389,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1390
) {
  struct moonbit_result_0 _result_3945;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1390);
  moonbit_decref(_M0L12_2adiscard__S1389);
  moonbit_decref(_M0L12_2adiscard__S1387);
  moonbit_decref(_M0L12_2adiscard__S1386);
  _result_3945.tag = 1;
  _result_3945.data.ok = 0;
  return _result_3945;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal21fuzzy__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1370
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1370);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__3(
  
) {
  moonbit_string_t _M0L8haystackS1368;
  moonbit_string_t _M0L6needleS1369;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3294;
  struct _M0TPB6ToJson _M0L6_2atmpS3286;
  void* _M0L6_2atmpS3287;
  moonbit_string_t _M0L6_2atmpS3290;
  moonbit_string_t _M0L6_2atmpS3291;
  moonbit_string_t _M0L6_2atmpS3292;
  moonbit_string_t _M0L6_2atmpS3293;
  moonbit_string_t* _M0L6_2atmpS3289;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3288;
  #line 64 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L8haystackS1368 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6needleS1369 = (moonbit_string_t)moonbit_string_literal_10.data;
  #line 67 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3294
  = _M0FP48clawteam8clawteam8internal5fuzzy11find__match(_M0L8haystackS1368, _M0L6needleS1369);
  _M0L6_2atmpS3286
  = (struct _M0TPB6ToJson){
    _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3294
  };
  moonbit_incref(_M0FPC17prelude4null);
  _M0L6_2atmpS3287 = _M0FPC17prelude4null;
  _M0L6_2atmpS3290 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3291 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3292 = 0;
  _M0L6_2atmpS3293 = 0;
  _M0L6_2atmpS3289 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3289[0] = _M0L6_2atmpS3290;
  _M0L6_2atmpS3289[1] = _M0L6_2atmpS3291;
  _M0L6_2atmpS3289[2] = _M0L6_2atmpS3292;
  _M0L6_2atmpS3289[3] = _M0L6_2atmpS3293;
  _M0L6_2atmpS3288
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3288)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3288->$0 = _M0L6_2atmpS3289;
  _M0L6_2atmpS3288->$1 = 4;
  #line 67 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3286, _M0L6_2atmpS3287, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS3288);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__2(
  
) {
  moonbit_string_t _M0L8haystackS1363;
  moonbit_string_t _M0L6needleS1364;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3274;
  struct _M0TPB6ToJson _M0L6_2atmpS3247;
  moonbit_string_t _M0L6_2atmpS3273;
  void* _M0L6_2atmpS3272;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3262;
  moonbit_string_t _M0L6_2atmpS3271;
  void* _M0L6_2atmpS3270;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3263;
  moonbit_string_t _M0L6_2atmpS3269;
  void* _M0L6_2atmpS3268;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3264;
  moonbit_string_t _M0L6_2atmpS3267;
  void* _M0L6_2atmpS3266;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3265;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1365;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3261;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3260;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3259;
  void* _M0L6_2atmpS3258;
  void** _M0L6_2atmpS3257;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3256;
  void* _M0L6_2atmpS3255;
  void* _M0L6_2atmpS3248;
  moonbit_string_t _M0L6_2atmpS3251;
  moonbit_string_t _M0L6_2atmpS3252;
  moonbit_string_t _M0L6_2atmpS3253;
  moonbit_string_t _M0L6_2atmpS3254;
  moonbit_string_t* _M0L6_2atmpS3250;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3249;
  struct moonbit_result_0 _tmp_3946;
  moonbit_string_t _M0L8haystackS1366;
  moonbit_string_t _M0L6needleS1367;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3285;
  struct _M0TPB6ToJson _M0L6_2atmpS3277;
  void* _M0L6_2atmpS3278;
  moonbit_string_t _M0L6_2atmpS3281;
  moonbit_string_t _M0L6_2atmpS3282;
  moonbit_string_t _M0L6_2atmpS3283;
  moonbit_string_t _M0L6_2atmpS3284;
  moonbit_string_t* _M0L6_2atmpS3280;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3279;
  #line 39 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L8haystackS1363 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6needleS1364 = (moonbit_string_t)moonbit_string_literal_15.data;
  #line 48 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3274
  = _M0FP48clawteam8clawteam8internal5fuzzy11find__match(_M0L8haystackS1363, _M0L6needleS1364);
  _M0L6_2atmpS3247
  = (struct _M0TPB6ToJson){
    _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3274
  };
  _M0L6_2atmpS3273 = 0;
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3272 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3273);
  _M0L8_2atupleS3262
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3262)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3262->$0 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L8_2atupleS3262->$1 = _M0L6_2atmpS3272;
  _M0L6_2atmpS3271 = 0;
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3270 = _M0MPC14json4Json6number(0x1.cp+3, _M0L6_2atmpS3271);
  _M0L8_2atupleS3263
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3263)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3263->$0 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L8_2atupleS3263->$1 = _M0L6_2atmpS3270;
  _M0L6_2atmpS3269 = 0;
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3268 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3269);
  _M0L8_2atupleS3264
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3264)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3264->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3264->$1 = _M0L6_2atmpS3268;
  _M0L6_2atmpS3267 = 0;
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3266 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS3267);
  _M0L8_2atupleS3265
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3265)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3265->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3265->$1 = _M0L6_2atmpS3266;
  _M0L7_2abindS1365 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1365[0] = _M0L8_2atupleS3262;
  _M0L7_2abindS1365[1] = _M0L8_2atupleS3263;
  _M0L7_2abindS1365[2] = _M0L8_2atupleS3264;
  _M0L7_2abindS1365[3] = _M0L8_2atupleS3265;
  _M0L6_2atmpS3261 = _M0L7_2abindS1365;
  _M0L6_2atmpS3260
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 4, _M0L6_2atmpS3261
  };
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3259 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3260);
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3258 = _M0MPC14json4Json6object(_M0L6_2atmpS3259);
  _M0L6_2atmpS3257 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3257[0] = _M0L6_2atmpS3258;
  _M0L6_2atmpS3256
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3256)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3256->$0 = _M0L6_2atmpS3257;
  _M0L6_2atmpS3256->$1 = 1;
  #line 48 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3255 = _M0MPC14json4Json5array(_M0L6_2atmpS3256);
  _M0L6_2atmpS3248 = _M0L6_2atmpS3255;
  _M0L6_2atmpS3251 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS3252 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3253 = 0;
  _M0L6_2atmpS3254 = 0;
  _M0L6_2atmpS3250 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3250[0] = _M0L6_2atmpS3251;
  _M0L6_2atmpS3250[1] = _M0L6_2atmpS3252;
  _M0L6_2atmpS3250[2] = _M0L6_2atmpS3253;
  _M0L6_2atmpS3250[3] = _M0L6_2atmpS3254;
  _M0L6_2atmpS3249
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3249)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3249->$0 = _M0L6_2atmpS3250;
  _M0L6_2atmpS3249->$1 = 4;
  #line 48 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _tmp_3946
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3247, _M0L6_2atmpS3248, (moonbit_string_t)moonbit_string_literal_22.data, _M0L6_2atmpS3249);
  if (_tmp_3946.tag) {
    int32_t const _M0L5_2aokS3275 = _tmp_3946.data.ok;
  } else {
    void* const _M0L6_2aerrS3276 = _tmp_3946.data.err;
    struct moonbit_result_0 _result_3947;
    _result_3947.tag = 0;
    _result_3947.data.err = _M0L6_2aerrS3276;
    return _result_3947;
  }
  _M0L8haystackS1366 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6needleS1367 = (moonbit_string_t)moonbit_string_literal_24.data;
  #line 60 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3285
  = _M0FP48clawteam8clawteam8internal5fuzzy11find__match(_M0L8haystackS1366, _M0L6needleS1367);
  _M0L6_2atmpS3277
  = (struct _M0TPB6ToJson){
    _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3285
  };
  moonbit_incref(_M0FPC17prelude4null);
  _M0L6_2atmpS3278 = _M0FPC17prelude4null;
  _M0L6_2atmpS3281 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3282 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3283 = 0;
  _M0L6_2atmpS3284 = 0;
  _M0L6_2atmpS3280 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3280[0] = _M0L6_2atmpS3281;
  _M0L6_2atmpS3280[1] = _M0L6_2atmpS3282;
  _M0L6_2atmpS3280[2] = _M0L6_2atmpS3283;
  _M0L6_2atmpS3280[3] = _M0L6_2atmpS3284;
  _M0L6_2atmpS3279
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3279)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3279->$0 = _M0L6_2atmpS3280;
  _M0L6_2atmpS3279->$1 = 4;
  #line 60 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3277, _M0L6_2atmpS3278, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS3279);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__1(
  
) {
  moonbit_string_t _M0L8haystackS1357;
  moonbit_string_t _M0L6needleS1358;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3216;
  struct _M0TPB6ToJson _M0L6_2atmpS3189;
  moonbit_string_t _M0L6_2atmpS3215;
  void* _M0L6_2atmpS3214;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3204;
  moonbit_string_t _M0L6_2atmpS3213;
  void* _M0L6_2atmpS3212;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3205;
  moonbit_string_t _M0L6_2atmpS3211;
  void* _M0L6_2atmpS3210;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3206;
  moonbit_string_t _M0L6_2atmpS3209;
  void* _M0L6_2atmpS3208;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3207;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1359;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3203;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3202;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3201;
  void* _M0L6_2atmpS3200;
  void** _M0L6_2atmpS3199;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3198;
  void* _M0L6_2atmpS3197;
  void* _M0L6_2atmpS3190;
  moonbit_string_t _M0L6_2atmpS3193;
  moonbit_string_t _M0L6_2atmpS3194;
  moonbit_string_t _M0L6_2atmpS3195;
  moonbit_string_t _M0L6_2atmpS3196;
  moonbit_string_t* _M0L6_2atmpS3192;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3191;
  struct moonbit_result_0 _tmp_3948;
  moonbit_string_t _M0L8haystackS1360;
  moonbit_string_t _M0L6needleS1361;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3246;
  struct _M0TPB6ToJson _M0L6_2atmpS3219;
  moonbit_string_t _M0L6_2atmpS3245;
  void* _M0L6_2atmpS3244;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3234;
  moonbit_string_t _M0L6_2atmpS3243;
  void* _M0L6_2atmpS3242;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3235;
  moonbit_string_t _M0L6_2atmpS3241;
  void* _M0L6_2atmpS3240;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3236;
  moonbit_string_t _M0L6_2atmpS3239;
  void* _M0L6_2atmpS3238;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3237;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1362;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3233;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3232;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3231;
  void* _M0L6_2atmpS3230;
  void** _M0L6_2atmpS3229;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3228;
  void* _M0L6_2atmpS3227;
  void* _M0L6_2atmpS3220;
  moonbit_string_t _M0L6_2atmpS3223;
  moonbit_string_t _M0L6_2atmpS3224;
  moonbit_string_t _M0L6_2atmpS3225;
  moonbit_string_t _M0L6_2atmpS3226;
  moonbit_string_t* _M0L6_2atmpS3222;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3221;
  #line 14 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L8haystackS1357 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6needleS1358 = (moonbit_string_t)moonbit_string_literal_15.data;
  #line 22 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3216
  = _M0FP48clawteam8clawteam8internal5fuzzy11find__match(_M0L8haystackS1357, _M0L6needleS1358);
  _M0L6_2atmpS3189
  = (struct _M0TPB6ToJson){
    _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3216
  };
  _M0L6_2atmpS3215 = 0;
  #line 23 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3214 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3215);
  _M0L8_2atupleS3204
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3204)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3204->$0 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L8_2atupleS3204->$1 = _M0L6_2atmpS3214;
  _M0L6_2atmpS3213 = 0;
  #line 23 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3212 = _M0MPC14json4Json6number(0x1.5p+4, _M0L6_2atmpS3213);
  _M0L8_2atupleS3205
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3205)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3205->$0 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L8_2atupleS3205->$1 = _M0L6_2atmpS3212;
  _M0L6_2atmpS3211 = 0;
  #line 23 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3210 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3211);
  _M0L8_2atupleS3206
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3206)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3206->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3206->$1 = _M0L6_2atmpS3210;
  _M0L6_2atmpS3209 = 0;
  #line 23 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3208 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS3209);
  _M0L8_2atupleS3207
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3207)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3207->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3207->$1 = _M0L6_2atmpS3208;
  _M0L7_2abindS1359 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1359[0] = _M0L8_2atupleS3204;
  _M0L7_2abindS1359[1] = _M0L8_2atupleS3205;
  _M0L7_2abindS1359[2] = _M0L8_2atupleS3206;
  _M0L7_2abindS1359[3] = _M0L8_2atupleS3207;
  _M0L6_2atmpS3203 = _M0L7_2abindS1359;
  _M0L6_2atmpS3202
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 4, _M0L6_2atmpS3203
  };
  #line 23 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3201 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3202);
  #line 23 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3200 = _M0MPC14json4Json6object(_M0L6_2atmpS3201);
  _M0L6_2atmpS3199 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3199[0] = _M0L6_2atmpS3200;
  _M0L6_2atmpS3198
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3198)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3198->$0 = _M0L6_2atmpS3199;
  _M0L6_2atmpS3198->$1 = 1;
  #line 22 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3197 = _M0MPC14json4Json5array(_M0L6_2atmpS3198);
  _M0L6_2atmpS3190 = _M0L6_2atmpS3197;
  _M0L6_2atmpS3193 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3194 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3195 = 0;
  _M0L6_2atmpS3196 = 0;
  _M0L6_2atmpS3192 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3192[0] = _M0L6_2atmpS3193;
  _M0L6_2atmpS3192[1] = _M0L6_2atmpS3194;
  _M0L6_2atmpS3192[2] = _M0L6_2atmpS3195;
  _M0L6_2atmpS3192[3] = _M0L6_2atmpS3196;
  _M0L6_2atmpS3191
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3191)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3191->$0 = _M0L6_2atmpS3192;
  _M0L6_2atmpS3191->$1 = 4;
  #line 22 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _tmp_3948
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3189, _M0L6_2atmpS3190, (moonbit_string_t)moonbit_string_literal_31.data, _M0L6_2atmpS3191);
  if (_tmp_3948.tag) {
    int32_t const _M0L5_2aokS3217 = _tmp_3948.data.ok;
  } else {
    void* const _M0L6_2aerrS3218 = _tmp_3948.data.err;
    struct moonbit_result_0 _result_3949;
    _result_3949.tag = 0;
    _result_3949.data.err = _M0L6_2aerrS3218;
    return _result_3949;
  }
  _M0L8haystackS1360 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6needleS1361 = (moonbit_string_t)moonbit_string_literal_33.data;
  #line 33 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3246
  = _M0FP48clawteam8clawteam8internal5fuzzy11find__match(_M0L8haystackS1360, _M0L6needleS1361);
  _M0L6_2atmpS3219
  = (struct _M0TPB6ToJson){
    _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3246
  };
  _M0L6_2atmpS3245 = 0;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3244 = _M0MPC14json4Json6number(0x1.cp+2, _M0L6_2atmpS3245);
  _M0L8_2atupleS3234
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3234)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3234->$0 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L8_2atupleS3234->$1 = _M0L6_2atmpS3244;
  _M0L6_2atmpS3243 = 0;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3242 = _M0MPC14json4Json6number(0x1.ap+3, _M0L6_2atmpS3243);
  _M0L8_2atupleS3235
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3235)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3235->$0 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L8_2atupleS3235->$1 = _M0L6_2atmpS3242;
  _M0L6_2atmpS3241 = 0;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3240 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS3241);
  _M0L8_2atupleS3236
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3236)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3236->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3236->$1 = _M0L6_2atmpS3240;
  _M0L6_2atmpS3239 = 0;
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3238 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS3239);
  _M0L8_2atupleS3237
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3237)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3237->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3237->$1 = _M0L6_2atmpS3238;
  _M0L7_2abindS1362 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1362[0] = _M0L8_2atupleS3234;
  _M0L7_2abindS1362[1] = _M0L8_2atupleS3235;
  _M0L7_2abindS1362[2] = _M0L8_2atupleS3236;
  _M0L7_2abindS1362[3] = _M0L8_2atupleS3237;
  _M0L6_2atmpS3233 = _M0L7_2abindS1362;
  _M0L6_2atmpS3232
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 4, _M0L6_2atmpS3233
  };
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3231 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3232);
  #line 34 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3230 = _M0MPC14json4Json6object(_M0L6_2atmpS3231);
  _M0L6_2atmpS3229 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3229[0] = _M0L6_2atmpS3230;
  _M0L6_2atmpS3228
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3228)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3228->$0 = _M0L6_2atmpS3229;
  _M0L6_2atmpS3228->$1 = 1;
  #line 33 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3227 = _M0MPC14json4Json5array(_M0L6_2atmpS3228);
  _M0L6_2atmpS3220 = _M0L6_2atmpS3227;
  _M0L6_2atmpS3223 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3224 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS3225 = 0;
  _M0L6_2atmpS3226 = 0;
  _M0L6_2atmpS3222 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3222[0] = _M0L6_2atmpS3223;
  _M0L6_2atmpS3222[1] = _M0L6_2atmpS3224;
  _M0L6_2atmpS3222[2] = _M0L6_2atmpS3225;
  _M0L6_2atmpS3222[3] = _M0L6_2atmpS3226;
  _M0L6_2atmpS3221
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3221)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3221->$0 = _M0L6_2atmpS3222;
  _M0L6_2atmpS3221->$1 = 4;
  #line 33 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3219, _M0L6_2atmpS3220, (moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS3221);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test43____test__7365617263685f746573742e6d6274__0(
  
) {
  moonbit_string_t _M0L8haystackS1354;
  moonbit_string_t _M0L6needleS1355;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3188;
  struct _M0TPB6ToJson _M0L6_2atmpS3161;
  moonbit_string_t _M0L6_2atmpS3187;
  void* _M0L6_2atmpS3186;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3176;
  moonbit_string_t _M0L6_2atmpS3185;
  void* _M0L6_2atmpS3184;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3177;
  moonbit_string_t _M0L6_2atmpS3183;
  void* _M0L6_2atmpS3182;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3178;
  moonbit_string_t _M0L6_2atmpS3181;
  void* _M0L6_2atmpS3180;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3179;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1356;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3175;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3174;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3173;
  void* _M0L6_2atmpS3172;
  void** _M0L6_2atmpS3171;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3170;
  void* _M0L6_2atmpS3169;
  void* _M0L6_2atmpS3162;
  moonbit_string_t _M0L6_2atmpS3165;
  moonbit_string_t _M0L6_2atmpS3166;
  moonbit_string_t _M0L6_2atmpS3167;
  moonbit_string_t _M0L6_2atmpS3168;
  moonbit_string_t* _M0L6_2atmpS3164;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3163;
  #line 2 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L8haystackS1354 = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6needleS1355 = (moonbit_string_t)moonbit_string_literal_38.data;
  #line 8 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3188
  = _M0FP48clawteam8clawteam8internal5fuzzy11find__match(_M0L8haystackS1354, _M0L6needleS1355);
  _M0L6_2atmpS3161
  = (struct _M0TPB6ToJson){
    _M0FP0169moonbitlang_2fcore_2foption_2fOption_5bclawteam_2fclawteam_2finternal_2ffuzzy_2fMatchResult_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3188
  };
  _M0L6_2atmpS3187 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3186 = _M0MPC14json4Json6number(0x1.8p+3, _M0L6_2atmpS3187);
  _M0L8_2atupleS3176
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3176)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3176->$0 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L8_2atupleS3176->$1 = _M0L6_2atmpS3186;
  _M0L6_2atmpS3185 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3184 = _M0MPC14json4Json6number(0x1.cp+3, _M0L6_2atmpS3185);
  _M0L8_2atupleS3177
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3177)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3177->$0 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L8_2atupleS3177->$1 = _M0L6_2atmpS3184;
  _M0L6_2atmpS3183 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3182 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS3183);
  _M0L8_2atupleS3178
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3178)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3178->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3178->$1 = _M0L6_2atmpS3182;
  _M0L6_2atmpS3181 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3180 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS3181);
  _M0L8_2atupleS3179
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3179)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3179->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3179->$1 = _M0L6_2atmpS3180;
  _M0L7_2abindS1356 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1356[0] = _M0L8_2atupleS3176;
  _M0L7_2abindS1356[1] = _M0L8_2atupleS3177;
  _M0L7_2abindS1356[2] = _M0L8_2atupleS3178;
  _M0L7_2abindS1356[3] = _M0L8_2atupleS3179;
  _M0L6_2atmpS3175 = _M0L7_2abindS1356;
  _M0L6_2atmpS3174
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 4, _M0L6_2atmpS3175
  };
  #line 9 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3173 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3174);
  #line 9 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3172 = _M0MPC14json4Json6object(_M0L6_2atmpS3173);
  _M0L6_2atmpS3171 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3171[0] = _M0L6_2atmpS3172;
  _M0L6_2atmpS3170
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3170)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3170->$0 = _M0L6_2atmpS3171;
  _M0L6_2atmpS3170->$1 = 1;
  #line 8 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  _M0L6_2atmpS3169 = _M0MPC14json4Json5array(_M0L6_2atmpS3170);
  _M0L6_2atmpS3162 = _M0L6_2atmpS3169;
  _M0L6_2atmpS3165 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3166 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3167 = 0;
  _M0L6_2atmpS3168 = 0;
  _M0L6_2atmpS3164 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3164[0] = _M0L6_2atmpS3165;
  _M0L6_2atmpS3164[1] = _M0L6_2atmpS3166;
  _M0L6_2atmpS3164[2] = _M0L6_2atmpS3167;
  _M0L6_2atmpS3164[3] = _M0L6_2atmpS3168;
  _M0L6_2atmpS3163
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3163)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3163->$0 = _M0L6_2atmpS3164;
  _M0L6_2atmpS3163->$1 = 4;
  #line 8 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3161, _M0L6_2atmpS3162, (moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS3163);
}

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy11find__match(
  moonbit_string_t _M0L8haystackS1350,
  moonbit_string_t _M0L6needleS1351
) {
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6resultS1348;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L7_2abindS1349;
  #line 140 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  moonbit_incref(_M0L6needleS1351);
  moonbit_incref(_M0L8haystackS1350);
  #line 142 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L7_2abindS1349
  = _M0FP48clawteam8clawteam8internal5fuzzy17try__exact__match(_M0L8haystackS1350, _M0L6needleS1351);
  if (_M0L7_2abindS1349 == 0) {
    if (_M0L7_2abindS1349) {
      moonbit_decref(_M0L7_2abindS1349);
    }
  } else {
    struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L7_2aSomeS1352;
    struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L9_2aresultS1353;
    moonbit_decref(_M0L6needleS1351);
    moonbit_decref(_M0L8haystackS1350);
    _M0L7_2aSomeS1352 = _M0L7_2abindS1349;
    _M0L9_2aresultS1353 = _M0L7_2aSomeS1352;
    _M0L6resultS1348 = _M0L9_2aresultS1353;
    goto join_1347;
  }
  goto joinlet_3950;
  join_1347:;
  return _M0L6resultS1348;
  joinlet_3950:;
  #line 147 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  return _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__match(_M0L8haystackS1350, _M0L6needleS1351);
}

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__match(
  moonbit_string_t _M0L8haystackS1310,
  moonbit_string_t _M0L6needleS1313
) {
  moonbit_string_t _M0L7_2abindS1311;
  int32_t _M0L6_2atmpS3160;
  struct _M0TPC16string10StringView _M0L6_2atmpS3159;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS3158;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L15haystack__linesS1309;
  moonbit_string_t _M0L7_2abindS1314;
  int32_t _M0L6_2atmpS3157;
  struct _M0TPC16string10StringView _M0L6_2atmpS3156;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS3155;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L13needle__linesS1312;
  struct _M0TWRPC16string10StringViewEs* _M0L6_2atmpS3149;
  struct _M0TPB5ArrayGsE* _M0L24trimmed__haystack__linesS1315;
  struct _M0TWRPC16string10StringViewEs* _M0L6_2atmpS3143;
  struct _M0TPB5ArrayGsE* _M0L22trimmed__needle__linesS1318;
  struct _M0TWsEb* _M0L6_2atmpS3140;
  struct _M0TPB5ArrayGsE* _M0L25non__empty__needle__linesS1321;
  int32_t _M0L6_2atmpS3104;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L24fuzzy__match__from__lineS1323;
  int32_t _M0L1iS1338;
  #line 38 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L7_2abindS1311 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3160 = Moonbit_array_length(_M0L7_2abindS1311);
  _M0L6_2atmpS3159
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3160, _M0L7_2abindS1311
  };
  #line 39 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3158
  = _M0MPC16string6String5split(_M0L8haystackS1310, _M0L6_2atmpS3159);
  #line 39 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L15haystack__linesS1309
  = _M0MPB4Iter9to__arrayGRPC16string10StringViewE(_M0L6_2atmpS3158);
  _M0L7_2abindS1314 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3157 = Moonbit_array_length(_M0L7_2abindS1314);
  _M0L6_2atmpS3156
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3157, _M0L7_2abindS1314
  };
  #line 40 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3155
  = _M0MPC16string6String5split(_M0L6needleS1313, _M0L6_2atmpS3156);
  #line 40 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L13needle__linesS1312
  = _M0MPB4Iter9to__arrayGRPC16string10StringViewE(_M0L6_2atmpS3155);
  _M0L6_2atmpS3149
  = (struct _M0TWRPC16string10StringViewEs*)&_M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3150l41$closure.data;
  moonbit_incref(_M0L15haystack__linesS1309);
  #line 41 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L24trimmed__haystack__linesS1315
  = _M0MPC15array5Array3mapGRPC16string10StringViewsE(_M0L15haystack__linesS1309, _M0L6_2atmpS3149);
  _M0L6_2atmpS3143
  = (struct _M0TWRPC16string10StringViewEs*)&_M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3144l44$closure.data;
  #line 44 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L22trimmed__needle__linesS1318
  = _M0MPC15array5Array3mapGRPC16string10StringViewsE(_M0L13needle__linesS1312, _M0L6_2atmpS3143);
  _M0L6_2atmpS3140
  = (struct _M0TWsEb*)&_M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3141l49$closure.data;
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L25non__empty__needle__linesS1321
  = _M0MPC15array5Array6filterGsE(_M0L22trimmed__needle__linesS1318, _M0L6_2atmpS3140);
  moonbit_incref(_M0L25non__empty__needle__linesS1321);
  #line 54 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3104
  = _M0MPC15array5Array6lengthGsE(_M0L25non__empty__needle__linesS1321);
  if (_M0L6_2atmpS3104 == 0) {
    moonbit_decref(_M0L25non__empty__needle__linesS1321);
    moonbit_decref(_M0L24trimmed__haystack__linesS1315);
    moonbit_decref(_M0L15haystack__linesS1309);
    return 0;
  }
  _M0L24fuzzy__match__from__lineS1323 = _M0L15haystack__linesS1309;
  moonbit_incref(_M0L24fuzzy__match__from__lineS1323);
  _M0L1iS1338 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3118;
    moonbit_incref(_M0L15haystack__linesS1309);
    #line 94 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
    _M0L6_2atmpS3118
    = _M0MPC15array5Array6lengthGRPC16string10StringViewE(_M0L15haystack__linesS1309);
    if (_M0L1iS1338 < _M0L6_2atmpS3118) {
      moonbit_string_t* _M0L6_2atmpS3138 =
        (moonbit_string_t*)moonbit_empty_ref_array;
      struct _M0TPB5ArrayGsE* _M0L28non__empty__haystack__windowS1339 =
        (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      struct _M0TPC13ref3RefGiE* _M0L8end__idxS1340;
      struct _M0TPC13ref3RefGiE* _M0L1jS1341;
      int32_t _M0L6_2atmpS3130;
      int32_t _M0L6_2atmpS3131;
      int32_t _M0L6_2atmpS3139;
      Moonbit_object_header(_M0L28non__empty__haystack__windowS1339)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
      _M0L28non__empty__haystack__windowS1339->$0 = _M0L6_2atmpS3138;
      _M0L28non__empty__haystack__windowS1339->$1 = 0;
      _M0L8end__idxS1340
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L8end__idxS1340)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L8end__idxS1340->$0 = _M0L1iS1338;
      _M0L1jS1341
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1jS1341)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1jS1341->$0 = _M0L1iS1338;
      while (1) {
        int32_t _M0L6_2atmpS3121;
        int32_t _M0L6_2atmpS3122;
        int32_t _if__result_3953;
        moonbit_incref(_M0L28non__empty__haystack__windowS1339);
        #line 101 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
        _M0L6_2atmpS3121
        = _M0MPC15array5Array6lengthGsE(_M0L28non__empty__haystack__windowS1339);
        moonbit_incref(_M0L25non__empty__needle__linesS1321);
        #line 101 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
        _M0L6_2atmpS3122
        = _M0MPC15array5Array6lengthGsE(_M0L25non__empty__needle__linesS1321);
        if (_M0L6_2atmpS3121 < _M0L6_2atmpS3122) {
          int32_t _M0L3valS3119 = _M0L1jS1341->$0;
          int32_t _M0L6_2atmpS3120;
          moonbit_incref(_M0L15haystack__linesS1309);
          #line 102 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
          _M0L6_2atmpS3120
          = _M0MPC15array5Array6lengthGRPC16string10StringViewE(_M0L15haystack__linesS1309);
          _if__result_3953 = _M0L3valS3119 < _M0L6_2atmpS3120;
        } else {
          _if__result_3953 = 0;
        }
        if (_if__result_3953) {
          int32_t _M0L3valS3124 = _M0L1jS1341->$0;
          moonbit_string_t _M0L6_2atmpS3123;
          int32_t _M0L3valS3127;
          int32_t _M0L3valS3129;
          int32_t _M0L6_2atmpS3128;
          moonbit_incref(_M0L24trimmed__haystack__linesS1315);
          #line 103 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
          _M0L6_2atmpS3123
          = _M0MPC15array5Array2atGsE(_M0L24trimmed__haystack__linesS1315, _M0L3valS3124);
          #line 103 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
          if (
            _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6_2atmpS3123, (moonbit_string_t)moonbit_string_literal_0.data)
          ) {
            int32_t _M0L3valS3126 = _M0L1jS1341->$0;
            moonbit_string_t _M0L6_2atmpS3125;
            moonbit_incref(_M0L24trimmed__haystack__linesS1315);
            #line 104 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
            _M0L6_2atmpS3125
            = _M0MPC15array5Array2atGsE(_M0L24trimmed__haystack__linesS1315, _M0L3valS3126);
            moonbit_incref(_M0L28non__empty__haystack__windowS1339);
            #line 104 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
            _M0MPC15array5Array4pushGsE(_M0L28non__empty__haystack__windowS1339, _M0L6_2atmpS3125);
          }
          _M0L3valS3127 = _M0L1jS1341->$0;
          _M0L8end__idxS1340->$0 = _M0L3valS3127;
          _M0L3valS3129 = _M0L1jS1341->$0;
          _M0L6_2atmpS3128 = _M0L3valS3129 + 1;
          _M0L1jS1341->$0 = _M0L6_2atmpS3128;
          continue;
        } else {
          moonbit_decref(_M0L1jS1341);
        }
        break;
      }
      moonbit_incref(_M0L28non__empty__haystack__windowS1339);
      #line 111 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
      _M0L6_2atmpS3130
      = _M0MPC15array5Array6lengthGsE(_M0L28non__empty__haystack__windowS1339);
      moonbit_incref(_M0L25non__empty__needle__linesS1321);
      #line 111 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
      _M0L6_2atmpS3131
      = _M0MPC15array5Array6lengthGsE(_M0L25non__empty__needle__linesS1321);
      if (_M0L6_2atmpS3130 == _M0L6_2atmpS3131) {
        struct _M0TPC13ref3RefGbE* _M0L13line__matchesS1343 =
          (struct _M0TPC13ref3RefGbE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGbE));
        int32_t _M0L1kS1344;
        int32_t _M0L8_2afieldS3505;
        Moonbit_object_header(_M0L13line__matchesS1343)->meta
        = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGbE) >> 2, 0, 0);
        _M0L13line__matchesS1343->$0 = 1;
        _M0L1kS1344 = 0;
        while (1) {
          int32_t _M0L6_2atmpS3132;
          moonbit_incref(_M0L25non__empty__needle__linesS1321);
          #line 113 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
          _M0L6_2atmpS3132
          = _M0MPC15array5Array6lengthGsE(_M0L25non__empty__needle__linesS1321);
          if (_M0L1kS1344 < _M0L6_2atmpS3132) {
            moonbit_string_t _M0L6_2atmpS3133;
            moonbit_string_t _M0L6_2atmpS3134;
            int32_t _M0L6_2atmpS3135;
            moonbit_incref(_M0L28non__empty__haystack__windowS1339);
            #line 114 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
            _M0L6_2atmpS3133
            = _M0MPC15array5Array2atGsE(_M0L28non__empty__haystack__windowS1339, _M0L1kS1344);
            moonbit_incref(_M0L25non__empty__needle__linesS1321);
            #line 114 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
            _M0L6_2atmpS3134
            = _M0MPC15array5Array2atGsE(_M0L25non__empty__needle__linesS1321, _M0L1kS1344);
            #line 114 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
            if (
              _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6_2atmpS3133, _M0L6_2atmpS3134)
            ) {
              moonbit_decref(_M0L28non__empty__haystack__windowS1339);
              _M0L13line__matchesS1343->$0 = 0;
              break;
            }
            _M0L6_2atmpS3135 = _M0L1kS1344 + 1;
            _M0L1kS1344 = _M0L6_2atmpS3135;
            continue;
          } else {
            moonbit_decref(_M0L28non__empty__haystack__windowS1339);
          }
          break;
        }
        _M0L8_2afieldS3505 = _M0L13line__matchesS1343->$0;
        moonbit_decref(_M0L13line__matchesS1343);
        if (_M0L8_2afieldS3505) {
          int32_t _M0L8_2afieldS3504;
          int32_t _M0L3valS3137;
          struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3136;
          moonbit_decref(_M0L25non__empty__needle__linesS1321);
          moonbit_decref(_M0L24trimmed__haystack__linesS1315);
          moonbit_decref(_M0L15haystack__linesS1309);
          _M0L8_2afieldS3504 = _M0L8end__idxS1340->$0;
          moonbit_decref(_M0L8end__idxS1340);
          _M0L3valS3137 = _M0L8_2afieldS3504;
          #line 120 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
          _M0L6_2atmpS3136
          = _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchN24fuzzy__match__from__lineS1323(_M0L24fuzzy__match__from__lineS1323, _M0L1iS1338, _M0L3valS3137);
          return _M0L6_2atmpS3136;
        } else {
          moonbit_decref(_M0L8end__idxS1340);
        }
      } else {
        moonbit_decref(_M0L8end__idxS1340);
        moonbit_decref(_M0L28non__empty__haystack__windowS1339);
      }
      _M0L6_2atmpS3139 = _M0L1iS1338 + 1;
      _M0L1iS1338 = _M0L6_2atmpS3139;
      continue;
    } else {
      moonbit_decref(_M0L24fuzzy__match__from__lineS1323);
      moonbit_decref(_M0L25non__empty__needle__linesS1321);
      moonbit_decref(_M0L24trimmed__haystack__linesS1315);
      moonbit_decref(_M0L15haystack__linesS1309);
    }
    break;
  }
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3150l41(
  struct _M0TWRPC16string10StringViewEs* _M0L6_2aenvS3151,
  struct _M0TPC16string10StringView _M0L4lineS1316
) {
  moonbit_string_t _M0L7_2abindS1317;
  int32_t _M0L6_2atmpS3154;
  struct _M0TPC16string10StringView _M0L6_2atmpS3153;
  struct _M0TPC16string10StringView _M0L6_2atmpS3152;
  #line 41 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  moonbit_decref(_M0L6_2aenvS3151);
  _M0L7_2abindS1317 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3154 = Moonbit_array_length(_M0L7_2abindS1317);
  _M0L6_2atmpS3153
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3154, _M0L7_2abindS1317
  };
  #line 42 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3152
  = _M0MPC16string10StringView12trim_2einner(_M0L4lineS1316, _M0L6_2atmpS3153);
  #line 42 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  return _M0IPC16string10StringViewPB4Show10to__string(_M0L6_2atmpS3152);
}

moonbit_string_t _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3144l44(
  struct _M0TWRPC16string10StringViewEs* _M0L6_2aenvS3145,
  struct _M0TPC16string10StringView _M0L4lineS1319
) {
  moonbit_string_t _M0L7_2abindS1320;
  int32_t _M0L6_2atmpS3148;
  struct _M0TPC16string10StringView _M0L6_2atmpS3147;
  struct _M0TPC16string10StringView _M0L6_2atmpS3146;
  #line 44 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  moonbit_decref(_M0L6_2aenvS3145);
  _M0L7_2abindS1320 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3148 = Moonbit_array_length(_M0L7_2abindS1320);
  _M0L6_2atmpS3147
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3148, _M0L7_2abindS1320
  };
  #line 45 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3146
  = _M0MPC16string10StringView12trim_2einner(_M0L4lineS1319, _M0L6_2atmpS3147);
  #line 45 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  return _M0IPC16string10StringViewPB4Show10to__string(_M0L6_2atmpS3146);
}

int32_t _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchC3141l49(
  struct _M0TWsEb* _M0L6_2aenvS3142,
  moonbit_string_t _M0L4lineS1322
) {
  #line 49 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  moonbit_decref(_M0L6_2aenvS3142);
  #line 50 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  return _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L4lineS1322, (moonbit_string_t)moonbit_string_literal_0.data);
}

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy26try__line__by__line__matchN24fuzzy__match__from__lineS1323(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L15haystack__linesS1309,
  int32_t _M0L5startS1324,
  int32_t _M0L3endS1325
) {
  struct _M0TPC16string10StringView* _M0L6_2atmpS3117;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L14matched__linesS1326;
  int32_t _M0L1iS1327;
  moonbit_string_t _M0L7_2abindS1330;
  int32_t _M0L6_2atmpS3116;
  struct _M0TPC16string10StringView _M0L6_2atmpS3115;
  moonbit_string_t _M0L16matched__contentS1329;
  int32_t _M0L8positionS1331;
  int32_t _M0L11start__lineS1336;
  int32_t _M0L9end__lineS1337;
  int32_t _M0L6_2atmpS3506;
  int32_t _M0L6_2atmpS3107;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _block_3957;
  #line 67 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3117
  = (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
  _M0L14matched__linesS1326
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L14matched__linesS1326)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L14matched__linesS1326->$0 = _M0L6_2atmpS3117;
  _M0L14matched__linesS1326->$1 = 0;
  _M0L1iS1327 = _M0L5startS1324;
  while (1) {
    if (_M0L1iS1327 <= _M0L3endS1325) {
      struct _M0TPC16string10StringView _M0L6_2atmpS3105;
      int32_t _M0L6_2atmpS3106;
      moonbit_incref(_M0L15haystack__linesS1309);
      #line 70 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
      _M0L6_2atmpS3105
      = _M0MPC15array5Array2atGRPC16string10StringViewE(_M0L15haystack__linesS1309, _M0L1iS1327);
      moonbit_incref(_M0L14matched__linesS1326);
      #line 70 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
      _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L14matched__linesS1326, _M0L6_2atmpS3105);
      _M0L6_2atmpS3106 = _M0L1iS1327 + 1;
      _M0L1iS1327 = _M0L6_2atmpS3106;
      continue;
    }
    break;
  }
  _M0L7_2abindS1330 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3116 = Moonbit_array_length(_M0L7_2abindS1330);
  _M0L6_2atmpS3115
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3116, _M0L7_2abindS1330
  };
  #line 72 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L16matched__contentS1329
  = _M0MPC15array5Array4joinGRPC16string10StringViewE(_M0L14matched__linesS1326, _M0L6_2atmpS3115);
  if (_M0L5startS1324 > 0) {
    struct _M0TPC16string10StringView* _M0L6_2atmpS3114 =
      (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L13before__linesS1332 =
      (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
    int32_t _M0L1iS1333;
    moonbit_string_t _M0L7_2abindS1335;
    int32_t _M0L6_2atmpS3113;
    struct _M0TPC16string10StringView _M0L6_2atmpS3112;
    moonbit_string_t _M0L6_2atmpS3111;
    int32_t _M0L6_2atmpS3507;
    int32_t _M0L6_2atmpS3110;
    Moonbit_object_header(_M0L13before__linesS1332)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
    _M0L13before__linesS1332->$0 = _M0L6_2atmpS3114;
    _M0L13before__linesS1332->$1 = 0;
    _M0L1iS1333 = 0;
    while (1) {
      if (_M0L1iS1333 < _M0L5startS1324) {
        struct _M0TPC16string10StringView _M0L6_2atmpS3108;
        int32_t _M0L6_2atmpS3109;
        moonbit_incref(_M0L15haystack__linesS1309);
        #line 76 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
        _M0L6_2atmpS3108
        = _M0MPC15array5Array2atGRPC16string10StringViewE(_M0L15haystack__linesS1309, _M0L1iS1333);
        moonbit_incref(_M0L13before__linesS1332);
        #line 76 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
        _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L13before__linesS1332, _M0L6_2atmpS3108);
        _M0L6_2atmpS3109 = _M0L1iS1333 + 1;
        _M0L1iS1333 = _M0L6_2atmpS3109;
        continue;
      } else {
        moonbit_decref(_M0L15haystack__linesS1309);
      }
      break;
    }
    _M0L7_2abindS1335 = (moonbit_string_t)moonbit_string_literal_42.data;
    _M0L6_2atmpS3113 = Moonbit_array_length(_M0L7_2abindS1335);
    _M0L6_2atmpS3112
    = (struct _M0TPC16string10StringView){
      0, _M0L6_2atmpS3113, _M0L7_2abindS1335
    };
    #line 78 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
    _M0L6_2atmpS3111
    = _M0MPC15array5Array4joinGRPC16string10StringViewE(_M0L13before__linesS1332, _M0L6_2atmpS3112);
    _M0L6_2atmpS3507 = Moonbit_array_length(_M0L6_2atmpS3111);
    moonbit_decref(_M0L6_2atmpS3111);
    _M0L6_2atmpS3110 = _M0L6_2atmpS3507;
    _M0L8positionS1331 = _M0L6_2atmpS3110 + 1;
  } else {
    moonbit_decref(_M0L15haystack__linesS1309);
    _M0L8positionS1331 = 0;
  }
  _M0L11start__lineS1336 = _M0L5startS1324;
  _M0L9end__lineS1337 = _M0L3endS1325;
  _M0L6_2atmpS3506 = Moonbit_array_length(_M0L16matched__contentS1329);
  moonbit_decref(_M0L16matched__contentS1329);
  _M0L6_2atmpS3107 = _M0L6_2atmpS3506;
  _block_3957
  = (struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult));
  Moonbit_object_header(_block_3957)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult) >> 2, 0, 0);
  _block_3957->$0 = _M0L8positionS1331;
  _block_3957->$1 = _M0L6_2atmpS3107;
  _block_3957->$2 = _M0L11start__lineS1336;
  _block_3957->$3 = _M0L9end__lineS1337;
  return _block_3957;
}

struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0FP48clawteam8clawteam8internal5fuzzy17try__exact__match(
  moonbit_string_t _M0L8haystackS1299,
  moonbit_string_t _M0L6needleS1303
) {
  int32_t _M0L8positionS1297;
  int32_t _M0L6_2atmpS3103;
  struct _M0TPC16string10StringView _M0L6_2atmpS3102;
  int64_t _M0L7_2abindS1306;
  int64_t _M0L6_2atmpS3101;
  struct _M0TPC16string10StringView _M0L6_2atmpS3100;
  moonbit_string_t _M0L13before__matchS1298;
  moonbit_string_t _M0L7_2abindS1301;
  int32_t _M0L6_2atmpS3099;
  struct _M0TPC16string10StringView _M0L6_2atmpS3098;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS3097;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3096;
  int32_t _M0L6_2atmpS3095;
  int32_t _M0L11start__lineS1300;
  moonbit_string_t _M0L7_2abindS1304;
  int32_t _M0L6_2atmpS3094;
  struct _M0TPC16string10StringView _M0L6_2atmpS3093;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS3092;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3091;
  int32_t _M0L13needle__linesS1302;
  int32_t _M0L6_2atmpS3090;
  int32_t _M0L9end__lineS1305;
  int32_t _M0L6_2atmpS3508;
  int32_t _M0L6_2atmpS3089;
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L6_2atmpS3088;
  #line 10 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3103 = Moonbit_array_length(_M0L6needleS1303);
  moonbit_incref(_M0L6needleS1303);
  _M0L6_2atmpS3102
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3103, _M0L6needleS1303
  };
  moonbit_incref(_M0L8haystackS1299);
  #line 11 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L7_2abindS1306
  = _M0MPC16string6String4find(_M0L8haystackS1299, _M0L6_2atmpS3102);
  if (_M0L7_2abindS1306 == 4294967296ll) {
    moonbit_decref(_M0L6needleS1303);
    moonbit_decref(_M0L8haystackS1299);
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1307 = _M0L7_2abindS1306;
    int32_t _M0L11_2apositionS1308 = (int32_t)_M0L7_2aSomeS1307;
    _M0L8positionS1297 = _M0L11_2apositionS1308;
    goto join_1296;
  }
  join_1296:;
  _M0L6_2atmpS3101 = (int64_t)_M0L8positionS1297;
  #line 14 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3100
  = _M0MPC16string6String11sub_2einner(_M0L8haystackS1299, 0, _M0L6_2atmpS3101);
  #line 14 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L13before__matchS1298
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L6_2atmpS3100);
  _M0L7_2abindS1301 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3099 = Moonbit_array_length(_M0L7_2abindS1301);
  _M0L6_2atmpS3098
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3099, _M0L7_2abindS1301
  };
  #line 15 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3097
  = _M0MPC16string6String5split(_M0L13before__matchS1298, _M0L6_2atmpS3098);
  #line 15 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3096
  = _M0MPB4Iter9to__arrayGRPC16string10StringViewE(_M0L6_2atmpS3097);
  #line 15 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3095
  = _M0MPC15array5Array6lengthGRPC16string10StringViewE(_M0L6_2atmpS3096);
  _M0L11start__lineS1300 = _M0L6_2atmpS3095 - 1;
  _M0L7_2abindS1304 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3094 = Moonbit_array_length(_M0L7_2abindS1304);
  _M0L6_2atmpS3093
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3094, _M0L7_2abindS1304
  };
  moonbit_incref(_M0L6needleS1303);
  #line 16 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3092
  = _M0MPC16string6String5split(_M0L6needleS1303, _M0L6_2atmpS3093);
  #line 16 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L6_2atmpS3091
  = _M0MPB4Iter9to__arrayGRPC16string10StringViewE(_M0L6_2atmpS3092);
  #line 16 "E:\\moonbit\\clawteam\\internal\\fuzzy\\search.mbt"
  _M0L13needle__linesS1302
  = _M0MPC15array5Array6lengthGRPC16string10StringViewE(_M0L6_2atmpS3091);
  _M0L6_2atmpS3090 = _M0L11start__lineS1300 + _M0L13needle__linesS1302;
  _M0L9end__lineS1305 = _M0L6_2atmpS3090 - 1;
  _M0L6_2atmpS3508 = Moonbit_array_length(_M0L6needleS1303);
  moonbit_decref(_M0L6needleS1303);
  _M0L6_2atmpS3089 = _M0L6_2atmpS3508;
  _M0L6_2atmpS3088
  = (struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult));
  Moonbit_object_header(_M0L6_2atmpS3088)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult) >> 2, 0, 0);
  _M0L6_2atmpS3088->$0 = _M0L8positionS1297;
  _M0L6_2atmpS3088->$1 = _M0L6_2atmpS3089;
  _M0L6_2atmpS3088->$2 = _M0L11start__lineS1300;
  _M0L6_2atmpS3088->$3 = _M0L9end__lineS1305;
  return _M0L6_2atmpS3088;
}

void* _M0IP48clawteam8clawteam8internal5fuzzy11MatchResultPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L8_2ax__38S1295
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1294;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3087;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3086;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1293;
  int32_t _M0L8positionS3079;
  void* _M0L6_2atmpS3078;
  int32_t _M0L6lengthS3081;
  void* _M0L6_2atmpS3080;
  int32_t _M0L11start__lineS3083;
  void* _M0L6_2atmpS3082;
  int32_t _M0L8_2afieldS3509;
  int32_t _M0L9end__lineS3085;
  void* _M0L6_2atmpS3084;
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0L7_2abindS1294 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3087 = _M0L7_2abindS1294;
  _M0L6_2atmpS3086
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3087
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0L6_24mapS1293 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3086);
  _M0L8positionS3079 = _M0L8_2ax__38S1295->$0;
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0L6_2atmpS3078 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8positionS3079);
  moonbit_incref(_M0L6_24mapS1293);
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1293, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS3078);
  _M0L6lengthS3081 = _M0L8_2ax__38S1295->$1;
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0L6_2atmpS3080 = _M0IPC13int3IntPB6ToJson8to__json(_M0L6lengthS3081);
  moonbit_incref(_M0L6_24mapS1293);
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1293, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS3080);
  _M0L11start__lineS3083 = _M0L8_2ax__38S1295->$2;
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0L6_2atmpS3082
  = _M0IPC13int3IntPB6ToJson8to__json(_M0L11start__lineS3083);
  moonbit_incref(_M0L6_24mapS1293);
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1293, (moonbit_string_t)moonbit_string_literal_18.data, _M0L6_2atmpS3082);
  _M0L8_2afieldS3509 = _M0L8_2ax__38S1295->$3;
  moonbit_decref(_M0L8_2ax__38S1295);
  _M0L9end__lineS3085 = _M0L8_2afieldS3509;
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0L6_2atmpS3084 = _M0IPC13int3IntPB6ToJson8to__json(_M0L9end__lineS3085);
  moonbit_incref(_M0L6_24mapS1293);
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1293, (moonbit_string_t)moonbit_string_literal_19.data, _M0L6_2atmpS3084);
  #line 3 "E:\\moonbit\\clawteam\\internal\\fuzzy\\types.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1293);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1288,
  void* _M0L7contentS1290,
  moonbit_string_t _M0L3locS1284,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1286
) {
  moonbit_string_t _M0L3locS1283;
  moonbit_string_t _M0L9args__locS1285;
  void* _M0L6_2atmpS3076;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3077;
  moonbit_string_t _M0L6actualS1287;
  moonbit_string_t _M0L4wantS1289;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1283 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1284);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1285 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1286);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3076 = _M0L3objS1288.$0->$method_0(_M0L3objS1288.$1);
  _M0L6_2atmpS3077 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1287
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3076, 0, 0, _M0L6_2atmpS3077);
  if (_M0L7contentS1290 == 0) {
    void* _M0L6_2atmpS3073;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3074;
    if (_M0L7contentS1290) {
      moonbit_decref(_M0L7contentS1290);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3073
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS3074 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1289
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3073, 0, 0, _M0L6_2atmpS3074);
  } else {
    void* _M0L7_2aSomeS1291 = _M0L7contentS1290;
    void* _M0L4_2axS1292 = _M0L7_2aSomeS1291;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3075 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1289
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1292, 0, 0, _M0L6_2atmpS3075);
  }
  moonbit_incref(_M0L4wantS1289);
  moonbit_incref(_M0L6actualS1287);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1287, _M0L4wantS1289)
  ) {
    moonbit_string_t _M0L6_2atmpS3071;
    moonbit_string_t _M0L6_2atmpS3517;
    moonbit_string_t _M0L6_2atmpS3070;
    moonbit_string_t _M0L6_2atmpS3516;
    moonbit_string_t _M0L6_2atmpS3068;
    moonbit_string_t _M0L6_2atmpS3069;
    moonbit_string_t _M0L6_2atmpS3515;
    moonbit_string_t _M0L6_2atmpS3067;
    moonbit_string_t _M0L6_2atmpS3514;
    moonbit_string_t _M0L6_2atmpS3064;
    moonbit_string_t _M0L6_2atmpS3066;
    moonbit_string_t _M0L6_2atmpS3065;
    moonbit_string_t _M0L6_2atmpS3513;
    moonbit_string_t _M0L6_2atmpS3063;
    moonbit_string_t _M0L6_2atmpS3512;
    moonbit_string_t _M0L6_2atmpS3060;
    moonbit_string_t _M0L6_2atmpS3062;
    moonbit_string_t _M0L6_2atmpS3061;
    moonbit_string_t _M0L6_2atmpS3511;
    moonbit_string_t _M0L6_2atmpS3059;
    moonbit_string_t _M0L6_2atmpS3510;
    moonbit_string_t _M0L6_2atmpS3058;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3057;
    struct moonbit_result_0 _result_3959;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3071
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1283);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3517
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_44.data, _M0L6_2atmpS3071);
    moonbit_decref(_M0L6_2atmpS3071);
    _M0L6_2atmpS3070 = _M0L6_2atmpS3517;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3516
    = moonbit_add_string(_M0L6_2atmpS3070, (moonbit_string_t)moonbit_string_literal_45.data);
    moonbit_decref(_M0L6_2atmpS3070);
    _M0L6_2atmpS3068 = _M0L6_2atmpS3516;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3069
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1285);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3515 = moonbit_add_string(_M0L6_2atmpS3068, _M0L6_2atmpS3069);
    moonbit_decref(_M0L6_2atmpS3068);
    moonbit_decref(_M0L6_2atmpS3069);
    _M0L6_2atmpS3067 = _M0L6_2atmpS3515;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3514
    = moonbit_add_string(_M0L6_2atmpS3067, (moonbit_string_t)moonbit_string_literal_46.data);
    moonbit_decref(_M0L6_2atmpS3067);
    _M0L6_2atmpS3064 = _M0L6_2atmpS3514;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3066 = _M0MPC16string6String6escape(_M0L4wantS1289);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3065
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3066);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3513 = moonbit_add_string(_M0L6_2atmpS3064, _M0L6_2atmpS3065);
    moonbit_decref(_M0L6_2atmpS3064);
    moonbit_decref(_M0L6_2atmpS3065);
    _M0L6_2atmpS3063 = _M0L6_2atmpS3513;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3512
    = moonbit_add_string(_M0L6_2atmpS3063, (moonbit_string_t)moonbit_string_literal_47.data);
    moonbit_decref(_M0L6_2atmpS3063);
    _M0L6_2atmpS3060 = _M0L6_2atmpS3512;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3062 = _M0MPC16string6String6escape(_M0L6actualS1287);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3061
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3062);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3511 = moonbit_add_string(_M0L6_2atmpS3060, _M0L6_2atmpS3061);
    moonbit_decref(_M0L6_2atmpS3060);
    moonbit_decref(_M0L6_2atmpS3061);
    _M0L6_2atmpS3059 = _M0L6_2atmpS3511;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3510
    = moonbit_add_string(_M0L6_2atmpS3059, (moonbit_string_t)moonbit_string_literal_48.data);
    moonbit_decref(_M0L6_2atmpS3059);
    _M0L6_2atmpS3058 = _M0L6_2atmpS3510;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3057
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3057)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3057)->$0
    = _M0L6_2atmpS3058;
    _result_3959.tag = 0;
    _result_3959.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3057;
    return _result_3959;
  } else {
    int32_t _M0L6_2atmpS3072;
    struct moonbit_result_0 _result_3960;
    moonbit_decref(_M0L4wantS1289);
    moonbit_decref(_M0L6actualS1287);
    moonbit_decref(_M0L9args__locS1285);
    moonbit_decref(_M0L3locS1283);
    _M0L6_2atmpS3072 = 0;
    _result_3960.tag = 1;
    _result_3960.data.ok = _M0L6_2atmpS3072;
    return _result_3960;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1282,
  int32_t _M0L13escape__slashS1254,
  int32_t _M0L6indentS1249,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1275
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1241;
  void** _M0L6_2atmpS3056;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1242;
  int32_t _M0Lm5depthS1243;
  void* _M0L6_2atmpS3055;
  void* _M0L8_2aparamS1244;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1241 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS3056 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1242
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1242)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1242->$0 = _M0L6_2atmpS3056;
  _M0L5stackS1242->$1 = 0;
  _M0Lm5depthS1243 = 0;
  _M0L6_2atmpS3055 = _M0L4selfS1282;
  _M0L8_2aparamS1244 = _M0L6_2atmpS3055;
  _2aloop_1260:;
  while (1) {
    if (_M0L8_2aparamS1244 == 0) {
      int32_t _M0L3lenS3017;
      if (_M0L8_2aparamS1244) {
        moonbit_decref(_M0L8_2aparamS1244);
      }
      _M0L3lenS3017 = _M0L5stackS1242->$1;
      if (_M0L3lenS3017 == 0) {
        if (_M0L8replacerS1275) {
          moonbit_decref(_M0L8replacerS1275);
        }
        moonbit_decref(_M0L5stackS1242);
        break;
      } else {
        void** _M0L8_2afieldS3525 = _M0L5stackS1242->$0;
        void** _M0L3bufS3041 = _M0L8_2afieldS3525;
        int32_t _M0L3lenS3043 = _M0L5stackS1242->$1;
        int32_t _M0L6_2atmpS3042 = _M0L3lenS3043 - 1;
        void* _M0L6_2atmpS3524 = (void*)_M0L3bufS3041[_M0L6_2atmpS3042];
        void* _M0L4_2axS1261 = _M0L6_2atmpS3524;
        switch (Moonbit_object_tag(_M0L4_2axS1261)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1262 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1261;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3520 =
              _M0L8_2aArrayS1262->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1263 =
              _M0L8_2afieldS3520;
            int32_t _M0L4_2aiS1264 = _M0L8_2aArrayS1262->$1;
            int32_t _M0L3lenS3029 = _M0L6_2aarrS1263->$1;
            if (_M0L4_2aiS1264 < _M0L3lenS3029) {
              int32_t _if__result_3962;
              void** _M0L8_2afieldS3519;
              void** _M0L3bufS3035;
              void* _M0L6_2atmpS3518;
              void* _M0L7elementS1265;
              int32_t _M0L6_2atmpS3030;
              void* _M0L6_2atmpS3033;
              if (_M0L4_2aiS1264 < 0) {
                _if__result_3962 = 1;
              } else {
                int32_t _M0L3lenS3034 = _M0L6_2aarrS1263->$1;
                _if__result_3962 = _M0L4_2aiS1264 >= _M0L3lenS3034;
              }
              if (_if__result_3962) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3519 = _M0L6_2aarrS1263->$0;
              _M0L3bufS3035 = _M0L8_2afieldS3519;
              _M0L6_2atmpS3518 = (void*)_M0L3bufS3035[_M0L4_2aiS1264];
              _M0L7elementS1265 = _M0L6_2atmpS3518;
              _M0L6_2atmpS3030 = _M0L4_2aiS1264 + 1;
              _M0L8_2aArrayS1262->$1 = _M0L6_2atmpS3030;
              if (_M0L4_2aiS1264 > 0) {
                int32_t _M0L6_2atmpS3032;
                moonbit_string_t _M0L6_2atmpS3031;
                moonbit_incref(_M0L7elementS1265);
                moonbit_incref(_M0L3bufS1241);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 44);
                _M0L6_2atmpS3032 = _M0Lm5depthS1243;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3031
                = _M0FPC14json11indent__str(_M0L6_2atmpS3032, _M0L6indentS1249);
                moonbit_incref(_M0L3bufS1241);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3031);
              } else {
                moonbit_incref(_M0L7elementS1265);
              }
              _M0L6_2atmpS3033 = _M0L7elementS1265;
              _M0L8_2aparamS1244 = _M0L6_2atmpS3033;
              goto _2aloop_1260;
            } else {
              int32_t _M0L6_2atmpS3036 = _M0Lm5depthS1243;
              void* _M0L6_2atmpS3037;
              int32_t _M0L6_2atmpS3039;
              moonbit_string_t _M0L6_2atmpS3038;
              void* _M0L6_2atmpS3040;
              _M0Lm5depthS1243 = _M0L6_2atmpS3036 - 1;
              moonbit_incref(_M0L5stackS1242);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3037
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1242);
              if (_M0L6_2atmpS3037) {
                moonbit_decref(_M0L6_2atmpS3037);
              }
              _M0L6_2atmpS3039 = _M0Lm5depthS1243;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3038
              = _M0FPC14json11indent__str(_M0L6_2atmpS3039, _M0L6indentS1249);
              moonbit_incref(_M0L3bufS1241);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3038);
              moonbit_incref(_M0L3bufS1241);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 93);
              _M0L6_2atmpS3040 = 0;
              _M0L8_2aparamS1244 = _M0L6_2atmpS3040;
              goto _2aloop_1260;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1266 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1261;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3523 =
              _M0L9_2aObjectS1266->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1267 =
              _M0L8_2afieldS3523;
            int32_t _M0L8_2afirstS1268 = _M0L9_2aObjectS1266->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1269;
            moonbit_incref(_M0L11_2aiteratorS1267);
            moonbit_incref(_M0L9_2aObjectS1266);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1269
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1267);
            if (_M0L7_2abindS1269 == 0) {
              int32_t _M0L6_2atmpS3018;
              void* _M0L6_2atmpS3019;
              int32_t _M0L6_2atmpS3021;
              moonbit_string_t _M0L6_2atmpS3020;
              void* _M0L6_2atmpS3022;
              if (_M0L7_2abindS1269) {
                moonbit_decref(_M0L7_2abindS1269);
              }
              moonbit_decref(_M0L9_2aObjectS1266);
              _M0L6_2atmpS3018 = _M0Lm5depthS1243;
              _M0Lm5depthS1243 = _M0L6_2atmpS3018 - 1;
              moonbit_incref(_M0L5stackS1242);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3019
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1242);
              if (_M0L6_2atmpS3019) {
                moonbit_decref(_M0L6_2atmpS3019);
              }
              _M0L6_2atmpS3021 = _M0Lm5depthS1243;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3020
              = _M0FPC14json11indent__str(_M0L6_2atmpS3021, _M0L6indentS1249);
              moonbit_incref(_M0L3bufS1241);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3020);
              moonbit_incref(_M0L3bufS1241);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 125);
              _M0L6_2atmpS3022 = 0;
              _M0L8_2aparamS1244 = _M0L6_2atmpS3022;
              goto _2aloop_1260;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1270 = _M0L7_2abindS1269;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1271 = _M0L7_2aSomeS1270;
              moonbit_string_t _M0L8_2afieldS3522 = _M0L4_2axS1271->$0;
              moonbit_string_t _M0L4_2akS1272 = _M0L8_2afieldS3522;
              void* _M0L8_2afieldS3521 = _M0L4_2axS1271->$1;
              int32_t _M0L6_2acntS3825 =
                Moonbit_object_header(_M0L4_2axS1271)->rc;
              void* _M0L4_2avS1273;
              void* _M0Lm2v2S1274;
              moonbit_string_t _M0L6_2atmpS3026;
              void* _M0L6_2atmpS3028;
              void* _M0L6_2atmpS3027;
              if (_M0L6_2acntS3825 > 1) {
                int32_t _M0L11_2anew__cntS3826 = _M0L6_2acntS3825 - 1;
                Moonbit_object_header(_M0L4_2axS1271)->rc
                = _M0L11_2anew__cntS3826;
                moonbit_incref(_M0L8_2afieldS3521);
                moonbit_incref(_M0L4_2akS1272);
              } else if (_M0L6_2acntS3825 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1271);
              }
              _M0L4_2avS1273 = _M0L8_2afieldS3521;
              _M0Lm2v2S1274 = _M0L4_2avS1273;
              if (_M0L8replacerS1275 == 0) {
                moonbit_incref(_M0Lm2v2S1274);
                moonbit_decref(_M0L4_2avS1273);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1276 =
                  _M0L8replacerS1275;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1277 =
                  _M0L7_2aSomeS1276;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1278 =
                  _M0L11_2areplacerS1277;
                void* _M0L7_2abindS1279;
                moonbit_incref(_M0L7_2afuncS1278);
                moonbit_incref(_M0L4_2akS1272);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1279
                = _M0L7_2afuncS1278->code(_M0L7_2afuncS1278, _M0L4_2akS1272, _M0L4_2avS1273);
                if (_M0L7_2abindS1279 == 0) {
                  void* _M0L6_2atmpS3023;
                  if (_M0L7_2abindS1279) {
                    moonbit_decref(_M0L7_2abindS1279);
                  }
                  moonbit_decref(_M0L4_2akS1272);
                  moonbit_decref(_M0L9_2aObjectS1266);
                  _M0L6_2atmpS3023 = 0;
                  _M0L8_2aparamS1244 = _M0L6_2atmpS3023;
                  goto _2aloop_1260;
                } else {
                  void* _M0L7_2aSomeS1280 = _M0L7_2abindS1279;
                  void* _M0L4_2avS1281 = _M0L7_2aSomeS1280;
                  _M0Lm2v2S1274 = _M0L4_2avS1281;
                }
              }
              if (!_M0L8_2afirstS1268) {
                int32_t _M0L6_2atmpS3025;
                moonbit_string_t _M0L6_2atmpS3024;
                moonbit_incref(_M0L3bufS1241);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 44);
                _M0L6_2atmpS3025 = _M0Lm5depthS1243;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3024
                = _M0FPC14json11indent__str(_M0L6_2atmpS3025, _M0L6indentS1249);
                moonbit_incref(_M0L3bufS1241);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3024);
              }
              moonbit_incref(_M0L3bufS1241);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3026
              = _M0FPC14json6escape(_M0L4_2akS1272, _M0L13escape__slashS1254);
              moonbit_incref(_M0L3bufS1241);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3026);
              moonbit_incref(_M0L3bufS1241);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 34);
              moonbit_incref(_M0L3bufS1241);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 58);
              if (_M0L6indentS1249 > 0) {
                moonbit_incref(_M0L3bufS1241);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 32);
              }
              _M0L9_2aObjectS1266->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1266);
              _M0L6_2atmpS3028 = _M0Lm2v2S1274;
              _M0L6_2atmpS3027 = _M0L6_2atmpS3028;
              _M0L8_2aparamS1244 = _M0L6_2atmpS3027;
              goto _2aloop_1260;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1245 = _M0L8_2aparamS1244;
      void* _M0L8_2avalueS1246 = _M0L7_2aSomeS1245;
      void* _M0L6_2atmpS3054;
      switch (Moonbit_object_tag(_M0L8_2avalueS1246)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1247 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1246;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3526 =
            _M0L9_2aObjectS1247->$0;
          int32_t _M0L6_2acntS3827 =
            Moonbit_object_header(_M0L9_2aObjectS1247)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1248;
          if (_M0L6_2acntS3827 > 1) {
            int32_t _M0L11_2anew__cntS3828 = _M0L6_2acntS3827 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1247)->rc
            = _M0L11_2anew__cntS3828;
            moonbit_incref(_M0L8_2afieldS3526);
          } else if (_M0L6_2acntS3827 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1247);
          }
          _M0L10_2amembersS1248 = _M0L8_2afieldS3526;
          moonbit_incref(_M0L10_2amembersS1248);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1248)) {
            moonbit_decref(_M0L10_2amembersS1248);
            moonbit_incref(_M0L3bufS1241);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, (moonbit_string_t)moonbit_string_literal_49.data);
          } else {
            int32_t _M0L6_2atmpS3049 = _M0Lm5depthS1243;
            int32_t _M0L6_2atmpS3051;
            moonbit_string_t _M0L6_2atmpS3050;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS3053;
            void* _M0L6ObjectS3052;
            _M0Lm5depthS1243 = _M0L6_2atmpS3049 + 1;
            moonbit_incref(_M0L3bufS1241);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 123);
            _M0L6_2atmpS3051 = _M0Lm5depthS1243;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3050
            = _M0FPC14json11indent__str(_M0L6_2atmpS3051, _M0L6indentS1249);
            moonbit_incref(_M0L3bufS1241);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3050);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3053
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1248);
            _M0L6ObjectS3052
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS3052)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3052)->$0
            = _M0L6_2atmpS3053;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3052)->$1
            = 1;
            moonbit_incref(_M0L5stackS1242);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1242, _M0L6ObjectS3052);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1250 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1246;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3527 =
            _M0L8_2aArrayS1250->$0;
          int32_t _M0L6_2acntS3829 =
            Moonbit_object_header(_M0L8_2aArrayS1250)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1251;
          if (_M0L6_2acntS3829 > 1) {
            int32_t _M0L11_2anew__cntS3830 = _M0L6_2acntS3829 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1250)->rc
            = _M0L11_2anew__cntS3830;
            moonbit_incref(_M0L8_2afieldS3527);
          } else if (_M0L6_2acntS3829 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1250);
          }
          _M0L6_2aarrS1251 = _M0L8_2afieldS3527;
          moonbit_incref(_M0L6_2aarrS1251);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1251)) {
            moonbit_decref(_M0L6_2aarrS1251);
            moonbit_incref(_M0L3bufS1241);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, (moonbit_string_t)moonbit_string_literal_50.data);
          } else {
            int32_t _M0L6_2atmpS3045 = _M0Lm5depthS1243;
            int32_t _M0L6_2atmpS3047;
            moonbit_string_t _M0L6_2atmpS3046;
            void* _M0L5ArrayS3048;
            _M0Lm5depthS1243 = _M0L6_2atmpS3045 + 1;
            moonbit_incref(_M0L3bufS1241);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 91);
            _M0L6_2atmpS3047 = _M0Lm5depthS1243;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3046
            = _M0FPC14json11indent__str(_M0L6_2atmpS3047, _M0L6indentS1249);
            moonbit_incref(_M0L3bufS1241);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3046);
            _M0L5ArrayS3048
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS3048)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3048)->$0
            = _M0L6_2aarrS1251;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3048)->$1
            = 0;
            moonbit_incref(_M0L5stackS1242);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1242, _M0L5ArrayS3048);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1252 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1246;
          moonbit_string_t _M0L8_2afieldS3528 = _M0L9_2aStringS1252->$0;
          int32_t _M0L6_2acntS3831 =
            Moonbit_object_header(_M0L9_2aStringS1252)->rc;
          moonbit_string_t _M0L4_2asS1253;
          moonbit_string_t _M0L6_2atmpS3044;
          if (_M0L6_2acntS3831 > 1) {
            int32_t _M0L11_2anew__cntS3832 = _M0L6_2acntS3831 - 1;
            Moonbit_object_header(_M0L9_2aStringS1252)->rc
            = _M0L11_2anew__cntS3832;
            moonbit_incref(_M0L8_2afieldS3528);
          } else if (_M0L6_2acntS3831 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1252);
          }
          _M0L4_2asS1253 = _M0L8_2afieldS3528;
          moonbit_incref(_M0L3bufS1241);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3044
          = _M0FPC14json6escape(_M0L4_2asS1253, _M0L13escape__slashS1254);
          moonbit_incref(_M0L3bufS1241);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L6_2atmpS3044);
          moonbit_incref(_M0L3bufS1241);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1241, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1255 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1246;
          double _M0L4_2anS1256 = _M0L9_2aNumberS1255->$0;
          moonbit_string_t _M0L8_2afieldS3529 = _M0L9_2aNumberS1255->$1;
          int32_t _M0L6_2acntS3833 =
            Moonbit_object_header(_M0L9_2aNumberS1255)->rc;
          moonbit_string_t _M0L7_2areprS1257;
          if (_M0L6_2acntS3833 > 1) {
            int32_t _M0L11_2anew__cntS3834 = _M0L6_2acntS3833 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1255)->rc
            = _M0L11_2anew__cntS3834;
            if (_M0L8_2afieldS3529) {
              moonbit_incref(_M0L8_2afieldS3529);
            }
          } else if (_M0L6_2acntS3833 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1255);
          }
          _M0L7_2areprS1257 = _M0L8_2afieldS3529;
          if (_M0L7_2areprS1257 == 0) {
            if (_M0L7_2areprS1257) {
              moonbit_decref(_M0L7_2areprS1257);
            }
            moonbit_incref(_M0L3bufS1241);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1241, _M0L4_2anS1256);
          } else {
            moonbit_string_t _M0L7_2aSomeS1258 = _M0L7_2areprS1257;
            moonbit_string_t _M0L4_2arS1259 = _M0L7_2aSomeS1258;
            moonbit_incref(_M0L3bufS1241);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, _M0L4_2arS1259);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1241);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, (moonbit_string_t)moonbit_string_literal_51.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1241);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, (moonbit_string_t)moonbit_string_literal_52.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1246);
          moonbit_incref(_M0L3bufS1241);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1241, (moonbit_string_t)moonbit_string_literal_53.data);
          break;
        }
      }
      _M0L6_2atmpS3054 = 0;
      _M0L8_2aparamS1244 = _M0L6_2atmpS3054;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1241);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1240,
  int32_t _M0L6indentS1238
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1238 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1239 = _M0L6indentS1238 * _M0L5levelS1240;
    switch (_M0L6spacesS1239) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_42.data;
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
        moonbit_string_t _M0L6_2atmpS3016;
        moonbit_string_t _M0L6_2atmpS3530;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3016
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_62.data, _M0L6spacesS1239);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3530
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_42.data, _M0L6_2atmpS3016);
        moonbit_decref(_M0L6_2atmpS3016);
        return _M0L6_2atmpS3530;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1230,
  int32_t _M0L13escape__slashS1235
) {
  int32_t _M0L6_2atmpS3015;
  struct _M0TPB13StringBuilder* _M0L3bufS1229;
  struct _M0TWEOc* _M0L5_2aitS1231;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3015 = Moonbit_array_length(_M0L3strS1230);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1229 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3015);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1231 = _M0MPC16string6String4iter(_M0L3strS1230);
  while (1) {
    int32_t _M0L7_2abindS1232;
    moonbit_incref(_M0L5_2aitS1231);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1232 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1231);
    if (_M0L7_2abindS1232 == -1) {
      moonbit_decref(_M0L5_2aitS1231);
    } else {
      int32_t _M0L7_2aSomeS1233 = _M0L7_2abindS1232;
      int32_t _M0L4_2acS1234 = _M0L7_2aSomeS1233;
      if (_M0L4_2acS1234 == 34) {
        moonbit_incref(_M0L3bufS1229);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_63.data);
      } else if (_M0L4_2acS1234 == 92) {
        moonbit_incref(_M0L3bufS1229);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_64.data);
      } else if (_M0L4_2acS1234 == 47) {
        if (_M0L13escape__slashS1235) {
          moonbit_incref(_M0L3bufS1229);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_65.data);
        } else {
          moonbit_incref(_M0L3bufS1229);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1229, _M0L4_2acS1234);
        }
      } else if (_M0L4_2acS1234 == 10) {
        moonbit_incref(_M0L3bufS1229);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_66.data);
      } else if (_M0L4_2acS1234 == 13) {
        moonbit_incref(_M0L3bufS1229);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_67.data);
      } else if (_M0L4_2acS1234 == 8) {
        moonbit_incref(_M0L3bufS1229);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_68.data);
      } else if (_M0L4_2acS1234 == 9) {
        moonbit_incref(_M0L3bufS1229);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_69.data);
      } else {
        int32_t _M0L4codeS1236 = _M0L4_2acS1234;
        if (_M0L4codeS1236 == 12) {
          moonbit_incref(_M0L3bufS1229);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_70.data);
        } else if (_M0L4codeS1236 < 32) {
          int32_t _M0L6_2atmpS3014;
          moonbit_string_t _M0L6_2atmpS3013;
          moonbit_incref(_M0L3bufS1229);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, (moonbit_string_t)moonbit_string_literal_71.data);
          _M0L6_2atmpS3014 = _M0L4codeS1236 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3013 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS3014);
          moonbit_incref(_M0L3bufS1229);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1229, _M0L6_2atmpS3013);
        } else {
          moonbit_incref(_M0L3bufS1229);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1229, _M0L4_2acS1234);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1229);
}

moonbit_string_t _M0MPC15array5Array4joinGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS1227,
  struct _M0TPC16string10StringView _M0L9separatorS1228
) {
  struct _M0TPC16string10StringView* _M0L8_2afieldS3532;
  struct _M0TPC16string10StringView* _M0L3bufS3011;
  int32_t _M0L8_2afieldS3531;
  int32_t _M0L6_2acntS3835;
  int32_t _M0L3lenS3012;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS3010;
  #line 2065 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3532 = _M0L4selfS1227->$0;
  _M0L3bufS3011 = _M0L8_2afieldS3532;
  _M0L8_2afieldS3531 = _M0L4selfS1227->$1;
  _M0L6_2acntS3835 = Moonbit_object_header(_M0L4selfS1227)->rc;
  if (_M0L6_2acntS3835 > 1) {
    int32_t _M0L11_2anew__cntS3836 = _M0L6_2acntS3835 - 1;
    Moonbit_object_header(_M0L4selfS1227)->rc = _M0L11_2anew__cntS3836;
    moonbit_incref(_M0L3bufS3011);
  } else if (_M0L6_2acntS3835 == 1) {
    #line 2069 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_free(_M0L4selfS1227);
  }
  _M0L3lenS3012 = _M0L8_2afieldS3531;
  _M0L6_2atmpS3010
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, _M0L3lenS3012, _M0L3bufS3011
  };
  #line 2069 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  return _M0MPC15array9ArrayView4joinGRPC16string10StringViewE(_M0L6_2atmpS3010, _M0L9separatorS1228);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1226
) {
  int32_t _M0L8_2afieldS3533;
  int32_t _M0L3lenS3009;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3533 = _M0L4selfS1226->$1;
  moonbit_decref(_M0L4selfS1226);
  _M0L3lenS3009 = _M0L8_2afieldS3533;
  return _M0L3lenS3009 == 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array6filterGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1221,
  struct _M0TWsEb* _M0L1fS1224
) {
  moonbit_string_t* _M0L6_2atmpS3008;
  struct _M0TPB5ArrayGsE* _M0L3arrS1219;
  int32_t _M0L7_2abindS1220;
  int32_t _M0L2__S1222;
  #line 684 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L6_2atmpS3008 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1219
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1219)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3arrS1219->$0 = _M0L6_2atmpS3008;
  _M0L3arrS1219->$1 = 0;
  _M0L7_2abindS1220 = _M0L4selfS1221->$1;
  _M0L2__S1222 = 0;
  while (1) {
    if (_M0L2__S1222 < _M0L7_2abindS1220) {
      moonbit_string_t* _M0L8_2afieldS3535 = _M0L4selfS1221->$0;
      moonbit_string_t* _M0L3bufS3007 = _M0L8_2afieldS3535;
      moonbit_string_t _M0L6_2atmpS3534 =
        (moonbit_string_t)_M0L3bufS3007[_M0L2__S1222];
      moonbit_string_t _M0L1vS1223 = _M0L6_2atmpS3534;
      int32_t _M0L6_2atmpS3006;
      moonbit_incref(_M0L1vS1223);
      moonbit_incref(_M0L1fS1224);
      moonbit_incref(_M0L1vS1223);
      #line 691 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      if (_M0L1fS1224->code(_M0L1fS1224, _M0L1vS1223)) {
        moonbit_incref(_M0L3arrS1219);
        #line 692 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3arrS1219, _M0L1vS1223);
      } else {
        moonbit_decref(_M0L1vS1223);
      }
      _M0L6_2atmpS3006 = _M0L2__S1222 + 1;
      _M0L2__S1222 = _M0L6_2atmpS3006;
      continue;
    } else {
      moonbit_decref(_M0L1fS1224);
      moonbit_decref(_M0L4selfS1221);
    }
    break;
  }
  return _M0L3arrS1219;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1216
) {
  int32_t _M0L3lenS1215;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1215 = _M0L4selfS1216->$1;
  if (_M0L3lenS1215 == 0) {
    moonbit_decref(_M0L4selfS1216);
    return 0;
  } else {
    int32_t _M0L5indexS1217 = _M0L3lenS1215 - 1;
    void** _M0L8_2afieldS3539 = _M0L4selfS1216->$0;
    void** _M0L3bufS3005 = _M0L8_2afieldS3539;
    void* _M0L6_2atmpS3538 = (void*)_M0L3bufS3005[_M0L5indexS1217];
    void* _M0L1vS1218 = _M0L6_2atmpS3538;
    void** _M0L8_2afieldS3537 = _M0L4selfS1216->$0;
    void** _M0L3bufS3004 = _M0L8_2afieldS3537;
    void* _M0L6_2aoldS3536;
    if (
      _M0L5indexS1217 < 0
      || _M0L5indexS1217 >= Moonbit_array_length(_M0L3bufS3004)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3536 = (void*)_M0L3bufS3004[_M0L5indexS1217];
    moonbit_incref(_M0L1vS1218);
    moonbit_decref(_M0L6_2aoldS3536);
    if (
      _M0L5indexS1217 < 0
      || _M0L5indexS1217 >= Moonbit_array_length(_M0L3bufS3004)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3004[_M0L5indexS1217]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1216->$1 = _M0L5indexS1217;
    moonbit_decref(_M0L4selfS1216);
    return _M0L1vS1218;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4selfS1192,
  struct _M0TPC16string10StringView _M0L9separatorS1202
) {
  int32_t _M0L3endS2982;
  int32_t _M0L5startS2983;
  int32_t _M0L6_2atmpS2981;
  #line 1099 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2982 = _M0L4selfS1192.$2;
  _M0L5startS2983 = _M0L4selfS1192.$1;
  _M0L6_2atmpS2981 = _M0L3endS2982 - _M0L5startS2983;
  if (_M0L6_2atmpS2981 == 0) {
    moonbit_decref(_M0L9separatorS1202.$0);
    moonbit_decref(_M0L4selfS1192.$0);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    struct _M0TPC16string10StringView* _M0L8_2afieldS3547 = _M0L4selfS1192.$0;
    struct _M0TPC16string10StringView* _M0L3bufS3002 = _M0L8_2afieldS3547;
    int32_t _M0L5startS3003 = _M0L4selfS1192.$1;
    struct _M0TPC16string10StringView _M0L6_2atmpS3546 =
      _M0L3bufS3002[_M0L5startS3003];
    struct _M0TPC16string10StringView _M0L5_2ahdS1193 = _M0L6_2atmpS3546;
    struct _M0TPC16string10StringView* _M0L8_2afieldS3545 = _M0L4selfS1192.$0;
    struct _M0TPC16string10StringView* _M0L9_2ax__bufS1194 =
      _M0L8_2afieldS3545;
    int32_t _M0L5startS3001 = _M0L4selfS1192.$1;
    int32_t _M0L11_2ax__startS1195 = 1 + _M0L5startS3001;
    int32_t _M0L8_2afieldS3544 = _M0L4selfS1192.$2;
    int32_t _M0L9_2ax__endS1196;
    struct _M0TPC16string10StringView _M0L2hdS1197;
    int32_t _M0Lm10size__hintS1198;
    int32_t _M0L7_2abindS1199;
    int32_t _M0L2__S1200;
    int32_t _M0L6_2atmpS2991;
    int32_t _M0L6_2atmpS3000;
    struct _M0TPB13StringBuilder* _M0L3bufS1204;
    moonbit_string_t _M0L8_2afieldS3542;
    moonbit_string_t _M0L3strS2992;
    int32_t _M0L5startS2993;
    int32_t _M0L3endS2995;
    int64_t _M0L6_2atmpS2994;
    moonbit_incref(_M0L5_2ahdS1193.$0);
    _M0L9_2ax__endS1196 = _M0L8_2afieldS3544;
    #line 1106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L2hdS1197
    = _M0IPC16string10StringViewPB12ToStringView16to__string__view(_M0L5_2ahdS1193);
    moonbit_incref(_M0L2hdS1197.$0);
    #line 1107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0Lm10size__hintS1198 = _M0MPC16string10StringView6length(_M0L2hdS1197);
    _M0L7_2abindS1199 = _M0L9_2ax__endS1196 - _M0L11_2ax__startS1195;
    _M0L2__S1200 = 0;
    while (1) {
      if (_M0L2__S1200 < _M0L7_2abindS1199) {
        int32_t _M0L6_2atmpS2990 = _M0L11_2ax__startS1195 + _M0L2__S1200;
        struct _M0TPC16string10StringView _M0L6_2atmpS3543 =
          _M0L9_2ax__bufS1194[_M0L6_2atmpS2990];
        struct _M0TPC16string10StringView _M0L1sS1201 = _M0L6_2atmpS3543;
        int32_t _M0L6_2atmpS2984 = _M0Lm10size__hintS1198;
        struct _M0TPC16string10StringView _M0L6_2atmpS2988;
        int32_t _M0L6_2atmpS2986;
        int32_t _M0L6_2atmpS2987;
        int32_t _M0L6_2atmpS2985;
        int32_t _M0L6_2atmpS2989;
        moonbit_incref(_M0L1sS1201.$0);
        #line 1109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
        _M0L6_2atmpS2988
        = _M0IPC16string10StringViewPB12ToStringView16to__string__view(_M0L1sS1201);
        #line 1109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
        _M0L6_2atmpS2986
        = _M0MPC16string10StringView6length(_M0L6_2atmpS2988);
        moonbit_incref(_M0L9separatorS1202.$0);
        #line 1109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
        _M0L6_2atmpS2987
        = _M0MPC16string10StringView6length(_M0L9separatorS1202);
        _M0L6_2atmpS2985 = _M0L6_2atmpS2986 + _M0L6_2atmpS2987;
        _M0Lm10size__hintS1198 = _M0L6_2atmpS2984 + _M0L6_2atmpS2985;
        _M0L6_2atmpS2989 = _M0L2__S1200 + 1;
        _M0L2__S1200 = _M0L6_2atmpS2989;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2991 = _M0Lm10size__hintS1198;
    _M0Lm10size__hintS1198 = _M0L6_2atmpS2991 << 1;
    _M0L6_2atmpS3000 = _M0Lm10size__hintS1198;
    #line 1112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L3bufS1204 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3000);
    moonbit_incref(_M0L3bufS1204);
    #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS1204, _M0L2hdS1197);
    _M0L8_2afieldS3542 = _M0L9separatorS1202.$0;
    _M0L3strS2992 = _M0L8_2afieldS3542;
    _M0L5startS2993 = _M0L9separatorS1202.$1;
    _M0L3endS2995 = _M0L9separatorS1202.$2;
    _M0L6_2atmpS2994 = (int64_t)_M0L3endS2995;
    moonbit_incref(_M0L3strS2992);
    #line 1115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2992, 0, _M0L5startS2993, _M0L6_2atmpS2994)
    ) {
      int32_t _M0L7_2abindS1205;
      int32_t _M0L2__S1206;
      moonbit_decref(_M0L9separatorS1202.$0);
      _M0L7_2abindS1205 = _M0L9_2ax__endS1196 - _M0L11_2ax__startS1195;
      _M0L2__S1206 = 0;
      while (1) {
        if (_M0L2__S1206 < _M0L7_2abindS1205) {
          int32_t _M0L6_2atmpS2997 = _M0L11_2ax__startS1195 + _M0L2__S1206;
          struct _M0TPC16string10StringView _M0L6_2atmpS3540 =
            _M0L9_2ax__bufS1194[_M0L6_2atmpS2997];
          struct _M0TPC16string10StringView _M0L1sS1207 = _M0L6_2atmpS3540;
          struct _M0TPC16string10StringView _M0L1sS1208;
          int32_t _M0L6_2atmpS2996;
          moonbit_incref(_M0L1sS1207.$0);
          #line 1118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
          _M0L1sS1208
          = _M0IPC16string10StringViewPB12ToStringView16to__string__view(_M0L1sS1207);
          moonbit_incref(_M0L3bufS1204);
          #line 1119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS1204, _M0L1sS1208);
          _M0L6_2atmpS2996 = _M0L2__S1206 + 1;
          _M0L2__S1206 = _M0L6_2atmpS2996;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS1194);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS1210 =
        _M0L9_2ax__endS1196 - _M0L11_2ax__startS1195;
      int32_t _M0L2__S1211 = 0;
      while (1) {
        if (_M0L2__S1211 < _M0L7_2abindS1210) {
          int32_t _M0L6_2atmpS2999 = _M0L11_2ax__startS1195 + _M0L2__S1211;
          struct _M0TPC16string10StringView _M0L6_2atmpS3541 =
            _M0L9_2ax__bufS1194[_M0L6_2atmpS2999];
          struct _M0TPC16string10StringView _M0L1sS1212 = _M0L6_2atmpS3541;
          struct _M0TPC16string10StringView _M0L1sS1213;
          int32_t _M0L6_2atmpS2998;
          moonbit_incref(_M0L1sS1212.$0);
          #line 1123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
          _M0L1sS1213
          = _M0IPC16string10StringViewPB12ToStringView16to__string__view(_M0L1sS1212);
          moonbit_incref(_M0L3bufS1204);
          moonbit_incref(_M0L9separatorS1202.$0);
          #line 1124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS1204, _M0L9separatorS1202);
          moonbit_incref(_M0L3bufS1204);
          #line 1126 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS1204, _M0L1sS1213);
          _M0L6_2atmpS2998 = _M0L2__S1211 + 1;
          _M0L2__S1211 = _M0L6_2atmpS2998;
          continue;
        } else {
          moonbit_decref(_M0L9separatorS1202.$0);
          moonbit_decref(_M0L9_2ax__bufS1194);
        }
        break;
      }
    }
    #line 1129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS1204);
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1190,
  struct _M0TPB6Logger _M0L6loggerS1191
) {
  moonbit_string_t _M0L6_2atmpS2980;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2979;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2980 = _M0L4selfS1190;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2979 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2980);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2979, _M0L6loggerS1191);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1167,
  struct _M0TPB6Logger _M0L6loggerS1189
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3556;
  struct _M0TPC16string10StringView _M0L3pkgS1166;
  moonbit_string_t _M0L7_2adataS1168;
  int32_t _M0L8_2astartS1169;
  int32_t _M0L6_2atmpS2978;
  int32_t _M0L6_2aendS1170;
  int32_t _M0Lm9_2acursorS1171;
  int32_t _M0Lm13accept__stateS1172;
  int32_t _M0Lm10match__endS1173;
  int32_t _M0Lm20match__tag__saver__0S1174;
  int32_t _M0Lm6tag__0S1175;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1176;
  struct _M0TPC16string10StringView _M0L8_2afieldS3555;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1185;
  void* _M0L8_2afieldS3554;
  int32_t _M0L6_2acntS3837;
  void* _M0L16_2apackage__nameS1186;
  struct _M0TPC16string10StringView _M0L8_2afieldS3552;
  struct _M0TPC16string10StringView _M0L8filenameS2955;
  struct _M0TPC16string10StringView _M0L8_2afieldS3551;
  struct _M0TPC16string10StringView _M0L11start__lineS2956;
  struct _M0TPC16string10StringView _M0L8_2afieldS3550;
  struct _M0TPC16string10StringView _M0L13start__columnS2957;
  struct _M0TPC16string10StringView _M0L8_2afieldS3549;
  struct _M0TPC16string10StringView _M0L9end__lineS2958;
  struct _M0TPC16string10StringView _M0L8_2afieldS3548;
  int32_t _M0L6_2acntS3841;
  struct _M0TPC16string10StringView _M0L11end__columnS2959;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3556
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1167->$0_1, _M0L4selfS1167->$0_2, _M0L4selfS1167->$0_0
  };
  _M0L3pkgS1166 = _M0L8_2afieldS3556;
  moonbit_incref(_M0L3pkgS1166.$0);
  moonbit_incref(_M0L3pkgS1166.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1168 = _M0MPC16string10StringView4data(_M0L3pkgS1166);
  moonbit_incref(_M0L3pkgS1166.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1169
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1166);
  moonbit_incref(_M0L3pkgS1166.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2978 = _M0MPC16string10StringView6length(_M0L3pkgS1166);
  _M0L6_2aendS1170 = _M0L8_2astartS1169 + _M0L6_2atmpS2978;
  _M0Lm9_2acursorS1171 = _M0L8_2astartS1169;
  _M0Lm13accept__stateS1172 = -1;
  _M0Lm10match__endS1173 = -1;
  _M0Lm20match__tag__saver__0S1174 = -1;
  _M0Lm6tag__0S1175 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2970 = _M0Lm9_2acursorS1171;
    if (_M0L6_2atmpS2970 < _M0L6_2aendS1170) {
      int32_t _M0L6_2atmpS2977 = _M0Lm9_2acursorS1171;
      int32_t _M0L10next__charS1180;
      int32_t _M0L6_2atmpS2971;
      moonbit_incref(_M0L7_2adataS1168);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1180
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1168, _M0L6_2atmpS2977);
      _M0L6_2atmpS2971 = _M0Lm9_2acursorS1171;
      _M0Lm9_2acursorS1171 = _M0L6_2atmpS2971 + 1;
      if (_M0L10next__charS1180 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2972;
          _M0Lm6tag__0S1175 = _M0Lm9_2acursorS1171;
          _M0L6_2atmpS2972 = _M0Lm9_2acursorS1171;
          if (_M0L6_2atmpS2972 < _M0L6_2aendS1170) {
            int32_t _M0L6_2atmpS2976 = _M0Lm9_2acursorS1171;
            int32_t _M0L10next__charS1181;
            int32_t _M0L6_2atmpS2973;
            moonbit_incref(_M0L7_2adataS1168);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1181
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1168, _M0L6_2atmpS2976);
            _M0L6_2atmpS2973 = _M0Lm9_2acursorS1171;
            _M0Lm9_2acursorS1171 = _M0L6_2atmpS2973 + 1;
            if (_M0L10next__charS1181 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2974 = _M0Lm9_2acursorS1171;
                if (_M0L6_2atmpS2974 < _M0L6_2aendS1170) {
                  int32_t _M0L6_2atmpS2975 = _M0Lm9_2acursorS1171;
                  _M0Lm9_2acursorS1171 = _M0L6_2atmpS2975 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1174 = _M0Lm6tag__0S1175;
                  _M0Lm13accept__stateS1172 = 0;
                  _M0Lm10match__endS1173 = _M0Lm9_2acursorS1171;
                  goto join_1177;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1177;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1177;
    }
    break;
  }
  goto joinlet_3968;
  join_1177:;
  switch (_M0Lm13accept__stateS1172) {
    case 0: {
      int32_t _M0L6_2atmpS2968;
      int32_t _M0L6_2atmpS2967;
      int64_t _M0L6_2atmpS2964;
      int32_t _M0L6_2atmpS2966;
      int64_t _M0L6_2atmpS2965;
      struct _M0TPC16string10StringView _M0L13package__nameS1178;
      int64_t _M0L6_2atmpS2961;
      int32_t _M0L6_2atmpS2963;
      int64_t _M0L6_2atmpS2962;
      struct _M0TPC16string10StringView _M0L12module__nameS1179;
      void* _M0L4SomeS2960;
      moonbit_decref(_M0L3pkgS1166.$0);
      _M0L6_2atmpS2968 = _M0Lm20match__tag__saver__0S1174;
      _M0L6_2atmpS2967 = _M0L6_2atmpS2968 + 1;
      _M0L6_2atmpS2964 = (int64_t)_M0L6_2atmpS2967;
      _M0L6_2atmpS2966 = _M0Lm10match__endS1173;
      _M0L6_2atmpS2965 = (int64_t)_M0L6_2atmpS2966;
      moonbit_incref(_M0L7_2adataS1168);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1178
      = _M0MPC16string6String4view(_M0L7_2adataS1168, _M0L6_2atmpS2964, _M0L6_2atmpS2965);
      _M0L6_2atmpS2961 = (int64_t)_M0L8_2astartS1169;
      _M0L6_2atmpS2963 = _M0Lm20match__tag__saver__0S1174;
      _M0L6_2atmpS2962 = (int64_t)_M0L6_2atmpS2963;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1179
      = _M0MPC16string6String4view(_M0L7_2adataS1168, _M0L6_2atmpS2961, _M0L6_2atmpS2962);
      _M0L4SomeS2960
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2960)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2960)->$0_0
      = _M0L13package__nameS1178.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2960)->$0_1
      = _M0L13package__nameS1178.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2960)->$0_2
      = _M0L13package__nameS1178.$2;
      _M0L7_2abindS1176
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1176)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1176->$0_0 = _M0L12module__nameS1179.$0;
      _M0L7_2abindS1176->$0_1 = _M0L12module__nameS1179.$1;
      _M0L7_2abindS1176->$0_2 = _M0L12module__nameS1179.$2;
      _M0L7_2abindS1176->$1 = _M0L4SomeS2960;
      break;
    }
    default: {
      void* _M0L4NoneS2969;
      moonbit_decref(_M0L7_2adataS1168);
      _M0L4NoneS2969
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1176
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1176)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1176->$0_0 = _M0L3pkgS1166.$0;
      _M0L7_2abindS1176->$0_1 = _M0L3pkgS1166.$1;
      _M0L7_2abindS1176->$0_2 = _M0L3pkgS1166.$2;
      _M0L7_2abindS1176->$1 = _M0L4NoneS2969;
      break;
    }
  }
  joinlet_3968:;
  _M0L8_2afieldS3555
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1176->$0_1, _M0L7_2abindS1176->$0_2, _M0L7_2abindS1176->$0_0
  };
  _M0L15_2amodule__nameS1185 = _M0L8_2afieldS3555;
  _M0L8_2afieldS3554 = _M0L7_2abindS1176->$1;
  _M0L6_2acntS3837 = Moonbit_object_header(_M0L7_2abindS1176)->rc;
  if (_M0L6_2acntS3837 > 1) {
    int32_t _M0L11_2anew__cntS3838 = _M0L6_2acntS3837 - 1;
    Moonbit_object_header(_M0L7_2abindS1176)->rc = _M0L11_2anew__cntS3838;
    moonbit_incref(_M0L8_2afieldS3554);
    moonbit_incref(_M0L15_2amodule__nameS1185.$0);
  } else if (_M0L6_2acntS3837 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1176);
  }
  _M0L16_2apackage__nameS1186 = _M0L8_2afieldS3554;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1186)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1187 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1186;
      struct _M0TPC16string10StringView _M0L8_2afieldS3553 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1187->$0_1,
                                              _M0L7_2aSomeS1187->$0_2,
                                              _M0L7_2aSomeS1187->$0_0};
      int32_t _M0L6_2acntS3839 = Moonbit_object_header(_M0L7_2aSomeS1187)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1188;
      if (_M0L6_2acntS3839 > 1) {
        int32_t _M0L11_2anew__cntS3840 = _M0L6_2acntS3839 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1187)->rc = _M0L11_2anew__cntS3840;
        moonbit_incref(_M0L8_2afieldS3553.$0);
      } else if (_M0L6_2acntS3839 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1187);
      }
      _M0L12_2apkg__nameS1188 = _M0L8_2afieldS3553;
      if (_M0L6loggerS1189.$1) {
        moonbit_incref(_M0L6loggerS1189.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1189.$0->$method_2(_M0L6loggerS1189.$1, _M0L12_2apkg__nameS1188);
      if (_M0L6loggerS1189.$1) {
        moonbit_incref(_M0L6loggerS1189.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1189.$0->$method_3(_M0L6loggerS1189.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1186);
      break;
    }
  }
  _M0L8_2afieldS3552
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1167->$1_1, _M0L4selfS1167->$1_2, _M0L4selfS1167->$1_0
  };
  _M0L8filenameS2955 = _M0L8_2afieldS3552;
  moonbit_incref(_M0L8filenameS2955.$0);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_2(_M0L6loggerS1189.$1, _M0L8filenameS2955);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_3(_M0L6loggerS1189.$1, 58);
  _M0L8_2afieldS3551
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1167->$2_1, _M0L4selfS1167->$2_2, _M0L4selfS1167->$2_0
  };
  _M0L11start__lineS2956 = _M0L8_2afieldS3551;
  moonbit_incref(_M0L11start__lineS2956.$0);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_2(_M0L6loggerS1189.$1, _M0L11start__lineS2956);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_3(_M0L6loggerS1189.$1, 58);
  _M0L8_2afieldS3550
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1167->$3_1, _M0L4selfS1167->$3_2, _M0L4selfS1167->$3_0
  };
  _M0L13start__columnS2957 = _M0L8_2afieldS3550;
  moonbit_incref(_M0L13start__columnS2957.$0);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_2(_M0L6loggerS1189.$1, _M0L13start__columnS2957);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_3(_M0L6loggerS1189.$1, 45);
  _M0L8_2afieldS3549
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1167->$4_1, _M0L4selfS1167->$4_2, _M0L4selfS1167->$4_0
  };
  _M0L9end__lineS2958 = _M0L8_2afieldS3549;
  moonbit_incref(_M0L9end__lineS2958.$0);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_2(_M0L6loggerS1189.$1, _M0L9end__lineS2958);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_3(_M0L6loggerS1189.$1, 58);
  _M0L8_2afieldS3548
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1167->$5_1, _M0L4selfS1167->$5_2, _M0L4selfS1167->$5_0
  };
  _M0L6_2acntS3841 = Moonbit_object_header(_M0L4selfS1167)->rc;
  if (_M0L6_2acntS3841 > 1) {
    int32_t _M0L11_2anew__cntS3847 = _M0L6_2acntS3841 - 1;
    Moonbit_object_header(_M0L4selfS1167)->rc = _M0L11_2anew__cntS3847;
    moonbit_incref(_M0L8_2afieldS3548.$0);
  } else if (_M0L6_2acntS3841 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3846 =
      (struct _M0TPC16string10StringView){_M0L4selfS1167->$4_1,
                                            _M0L4selfS1167->$4_2,
                                            _M0L4selfS1167->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3845;
    struct _M0TPC16string10StringView _M0L8_2afieldS3844;
    struct _M0TPC16string10StringView _M0L8_2afieldS3843;
    struct _M0TPC16string10StringView _M0L8_2afieldS3842;
    moonbit_decref(_M0L8_2afieldS3846.$0);
    _M0L8_2afieldS3845
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1167->$3_1, _M0L4selfS1167->$3_2, _M0L4selfS1167->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3845.$0);
    _M0L8_2afieldS3844
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1167->$2_1, _M0L4selfS1167->$2_2, _M0L4selfS1167->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3844.$0);
    _M0L8_2afieldS3843
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1167->$1_1, _M0L4selfS1167->$1_2, _M0L4selfS1167->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3843.$0);
    _M0L8_2afieldS3842
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1167->$0_1, _M0L4selfS1167->$0_2, _M0L4selfS1167->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3842.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1167);
  }
  _M0L11end__columnS2959 = _M0L8_2afieldS3548;
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_2(_M0L6loggerS1189.$1, _M0L11end__columnS2959);
  if (_M0L6loggerS1189.$1) {
    moonbit_incref(_M0L6loggerS1189.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_3(_M0L6loggerS1189.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1189.$0->$method_2(_M0L6loggerS1189.$1, _M0L15_2amodule__nameS1185);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1165) {
  moonbit_string_t _M0L6_2atmpS2954;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2954
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1165);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2954);
  moonbit_decref(_M0L6_2atmpS2954);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1164,
  struct _M0TPB6Logger _M0L6loggerS1163
) {
  moonbit_string_t _M0L6_2atmpS2953;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2953 = _M0MPC16double6Double10to__string(_M0L4selfS1164);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1163.$0->$method_0(_M0L6loggerS1163.$1, _M0L6_2atmpS2953);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1162) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1162);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1149) {
  uint64_t _M0L4bitsS1150;
  uint64_t _M0L6_2atmpS2952;
  uint64_t _M0L6_2atmpS2951;
  int32_t _M0L8ieeeSignS1151;
  uint64_t _M0L12ieeeMantissaS1152;
  uint64_t _M0L6_2atmpS2950;
  uint64_t _M0L6_2atmpS2949;
  int32_t _M0L12ieeeExponentS1153;
  int32_t _if__result_3972;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1154;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1155;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2948;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1149 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_72.data;
  }
  _M0L4bitsS1150 = *(int64_t*)&_M0L3valS1149;
  _M0L6_2atmpS2952 = _M0L4bitsS1150 >> 63;
  _M0L6_2atmpS2951 = _M0L6_2atmpS2952 & 1ull;
  _M0L8ieeeSignS1151 = _M0L6_2atmpS2951 != 0ull;
  _M0L12ieeeMantissaS1152 = _M0L4bitsS1150 & 4503599627370495ull;
  _M0L6_2atmpS2950 = _M0L4bitsS1150 >> 52;
  _M0L6_2atmpS2949 = _M0L6_2atmpS2950 & 2047ull;
  _M0L12ieeeExponentS1153 = (int32_t)_M0L6_2atmpS2949;
  if (_M0L12ieeeExponentS1153 == 2047) {
    _if__result_3972 = 1;
  } else if (_M0L12ieeeExponentS1153 == 0) {
    _if__result_3972 = _M0L12ieeeMantissaS1152 == 0ull;
  } else {
    _if__result_3972 = 0;
  }
  if (_if__result_3972) {
    int32_t _M0L6_2atmpS2937 = _M0L12ieeeExponentS1153 != 0;
    int32_t _M0L6_2atmpS2938 = _M0L12ieeeMantissaS1152 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1151, _M0L6_2atmpS2937, _M0L6_2atmpS2938);
  }
  _M0Lm1vS1154 = _M0FPB31ryu__to__string_2erecord_2f1148;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1155
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1152, _M0L12ieeeExponentS1153);
  if (_M0L5smallS1155 == 0) {
    uint32_t _M0L6_2atmpS2939;
    if (_M0L5smallS1155) {
      moonbit_decref(_M0L5smallS1155);
    }
    _M0L6_2atmpS2939 = *(uint32_t*)&_M0L12ieeeExponentS1153;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1154 = _M0FPB3d2d(_M0L12ieeeMantissaS1152, _M0L6_2atmpS2939);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1156 = _M0L5smallS1155;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1157 = _M0L7_2aSomeS1156;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1158 = _M0L4_2afS1157;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2947 = _M0Lm1xS1158;
      uint64_t _M0L8_2afieldS3559 = _M0L6_2atmpS2947->$0;
      uint64_t _M0L8mantissaS2946 = _M0L8_2afieldS3559;
      uint64_t _M0L1qS1159 = _M0L8mantissaS2946 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2945 = _M0Lm1xS1158;
      uint64_t _M0L8_2afieldS3558 = _M0L6_2atmpS2945->$0;
      uint64_t _M0L8mantissaS2943 = _M0L8_2afieldS3558;
      uint64_t _M0L6_2atmpS2944 = 10ull * _M0L1qS1159;
      uint64_t _M0L1rS1160 = _M0L8mantissaS2943 - _M0L6_2atmpS2944;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2942;
      int32_t _M0L8_2afieldS3557;
      int32_t _M0L8exponentS2941;
      int32_t _M0L6_2atmpS2940;
      if (_M0L1rS1160 != 0ull) {
        break;
      }
      _M0L6_2atmpS2942 = _M0Lm1xS1158;
      _M0L8_2afieldS3557 = _M0L6_2atmpS2942->$1;
      moonbit_decref(_M0L6_2atmpS2942);
      _M0L8exponentS2941 = _M0L8_2afieldS3557;
      _M0L6_2atmpS2940 = _M0L8exponentS2941 + 1;
      _M0Lm1xS1158
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1158)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1158->$0 = _M0L1qS1159;
      _M0Lm1xS1158->$1 = _M0L6_2atmpS2940;
      continue;
      break;
    }
    _M0Lm1vS1154 = _M0Lm1xS1158;
  }
  _M0L6_2atmpS2948 = _M0Lm1vS1154;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2948, _M0L8ieeeSignS1151);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1143,
  int32_t _M0L12ieeeExponentS1145
) {
  uint64_t _M0L2m2S1142;
  int32_t _M0L6_2atmpS2936;
  int32_t _M0L2e2S1144;
  int32_t _M0L6_2atmpS2935;
  uint64_t _M0L6_2atmpS2934;
  uint64_t _M0L4maskS1146;
  uint64_t _M0L8fractionS1147;
  int32_t _M0L6_2atmpS2933;
  uint64_t _M0L6_2atmpS2932;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2931;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1142 = 4503599627370496ull | _M0L12ieeeMantissaS1143;
  _M0L6_2atmpS2936 = _M0L12ieeeExponentS1145 - 1023;
  _M0L2e2S1144 = _M0L6_2atmpS2936 - 52;
  if (_M0L2e2S1144 > 0) {
    return 0;
  }
  if (_M0L2e2S1144 < -52) {
    return 0;
  }
  _M0L6_2atmpS2935 = -_M0L2e2S1144;
  _M0L6_2atmpS2934 = 1ull << (_M0L6_2atmpS2935 & 63);
  _M0L4maskS1146 = _M0L6_2atmpS2934 - 1ull;
  _M0L8fractionS1147 = _M0L2m2S1142 & _M0L4maskS1146;
  if (_M0L8fractionS1147 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2933 = -_M0L2e2S1144;
  _M0L6_2atmpS2932 = _M0L2m2S1142 >> (_M0L6_2atmpS2933 & 63);
  _M0L6_2atmpS2931
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2931)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2931->$0 = _M0L6_2atmpS2932;
  _M0L6_2atmpS2931->$1 = 0;
  return _M0L6_2atmpS2931;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1116,
  int32_t _M0L4signS1114
) {
  int32_t _M0L6_2atmpS2930;
  moonbit_bytes_t _M0L6resultS1112;
  int32_t _M0Lm5indexS1113;
  uint64_t _M0Lm6outputS1115;
  uint64_t _M0L6_2atmpS2929;
  int32_t _M0L7olengthS1117;
  int32_t _M0L8_2afieldS3560;
  int32_t _M0L8exponentS2928;
  int32_t _M0L6_2atmpS2927;
  int32_t _M0Lm3expS1118;
  int32_t _M0L6_2atmpS2926;
  int32_t _M0L6_2atmpS2924;
  int32_t _M0L18scientificNotationS1119;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2930 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1112
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2930);
  _M0Lm5indexS1113 = 0;
  if (_M0L4signS1114) {
    int32_t _M0L6_2atmpS2799 = _M0Lm5indexS1113;
    int32_t _M0L6_2atmpS2800;
    if (
      _M0L6_2atmpS2799 < 0
      || _M0L6_2atmpS2799 >= Moonbit_array_length(_M0L6resultS1112)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1112[_M0L6_2atmpS2799] = 45;
    _M0L6_2atmpS2800 = _M0Lm5indexS1113;
    _M0Lm5indexS1113 = _M0L6_2atmpS2800 + 1;
  }
  _M0Lm6outputS1115 = _M0L1vS1116->$0;
  _M0L6_2atmpS2929 = _M0Lm6outputS1115;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1117 = _M0FPB17decimal__length17(_M0L6_2atmpS2929);
  _M0L8_2afieldS3560 = _M0L1vS1116->$1;
  moonbit_decref(_M0L1vS1116);
  _M0L8exponentS2928 = _M0L8_2afieldS3560;
  _M0L6_2atmpS2927 = _M0L8exponentS2928 + _M0L7olengthS1117;
  _M0Lm3expS1118 = _M0L6_2atmpS2927 - 1;
  _M0L6_2atmpS2926 = _M0Lm3expS1118;
  if (_M0L6_2atmpS2926 >= -6) {
    int32_t _M0L6_2atmpS2925 = _M0Lm3expS1118;
    _M0L6_2atmpS2924 = _M0L6_2atmpS2925 < 21;
  } else {
    _M0L6_2atmpS2924 = 0;
  }
  _M0L18scientificNotationS1119 = !_M0L6_2atmpS2924;
  if (_M0L18scientificNotationS1119) {
    int32_t _M0L7_2abindS1120 = _M0L7olengthS1117 - 1;
    int32_t _M0L1iS1121 = 0;
    int32_t _M0L6_2atmpS2810;
    uint64_t _M0L6_2atmpS2815;
    int32_t _M0L6_2atmpS2814;
    int32_t _M0L6_2atmpS2813;
    int32_t _M0L6_2atmpS2812;
    int32_t _M0L6_2atmpS2811;
    int32_t _M0L6_2atmpS2819;
    int32_t _M0L6_2atmpS2820;
    int32_t _M0L6_2atmpS2821;
    int32_t _M0L6_2atmpS2822;
    int32_t _M0L6_2atmpS2823;
    int32_t _M0L6_2atmpS2829;
    int32_t _M0L6_2atmpS2862;
    while (1) {
      if (_M0L1iS1121 < _M0L7_2abindS1120) {
        uint64_t _M0L6_2atmpS2808 = _M0Lm6outputS1115;
        uint64_t _M0L1cS1122 = _M0L6_2atmpS2808 % 10ull;
        uint64_t _M0L6_2atmpS2801 = _M0Lm6outputS1115;
        int32_t _M0L6_2atmpS2807;
        int32_t _M0L6_2atmpS2806;
        int32_t _M0L6_2atmpS2802;
        int32_t _M0L6_2atmpS2805;
        int32_t _M0L6_2atmpS2804;
        int32_t _M0L6_2atmpS2803;
        int32_t _M0L6_2atmpS2809;
        _M0Lm6outputS1115 = _M0L6_2atmpS2801 / 10ull;
        _M0L6_2atmpS2807 = _M0Lm5indexS1113;
        _M0L6_2atmpS2806 = _M0L6_2atmpS2807 + _M0L7olengthS1117;
        _M0L6_2atmpS2802 = _M0L6_2atmpS2806 - _M0L1iS1121;
        _M0L6_2atmpS2805 = (int32_t)_M0L1cS1122;
        _M0L6_2atmpS2804 = 48 + _M0L6_2atmpS2805;
        _M0L6_2atmpS2803 = _M0L6_2atmpS2804 & 0xff;
        if (
          _M0L6_2atmpS2802 < 0
          || _M0L6_2atmpS2802 >= Moonbit_array_length(_M0L6resultS1112)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1112[_M0L6_2atmpS2802] = _M0L6_2atmpS2803;
        _M0L6_2atmpS2809 = _M0L1iS1121 + 1;
        _M0L1iS1121 = _M0L6_2atmpS2809;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2810 = _M0Lm5indexS1113;
    _M0L6_2atmpS2815 = _M0Lm6outputS1115;
    _M0L6_2atmpS2814 = (int32_t)_M0L6_2atmpS2815;
    _M0L6_2atmpS2813 = _M0L6_2atmpS2814 % 10;
    _M0L6_2atmpS2812 = 48 + _M0L6_2atmpS2813;
    _M0L6_2atmpS2811 = _M0L6_2atmpS2812 & 0xff;
    if (
      _M0L6_2atmpS2810 < 0
      || _M0L6_2atmpS2810 >= Moonbit_array_length(_M0L6resultS1112)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1112[_M0L6_2atmpS2810] = _M0L6_2atmpS2811;
    if (_M0L7olengthS1117 > 1) {
      int32_t _M0L6_2atmpS2817 = _M0Lm5indexS1113;
      int32_t _M0L6_2atmpS2816 = _M0L6_2atmpS2817 + 1;
      if (
        _M0L6_2atmpS2816 < 0
        || _M0L6_2atmpS2816 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2816] = 46;
    } else {
      int32_t _M0L6_2atmpS2818 = _M0Lm5indexS1113;
      _M0Lm5indexS1113 = _M0L6_2atmpS2818 - 1;
    }
    _M0L6_2atmpS2819 = _M0Lm5indexS1113;
    _M0L6_2atmpS2820 = _M0L7olengthS1117 + 1;
    _M0Lm5indexS1113 = _M0L6_2atmpS2819 + _M0L6_2atmpS2820;
    _M0L6_2atmpS2821 = _M0Lm5indexS1113;
    if (
      _M0L6_2atmpS2821 < 0
      || _M0L6_2atmpS2821 >= Moonbit_array_length(_M0L6resultS1112)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1112[_M0L6_2atmpS2821] = 101;
    _M0L6_2atmpS2822 = _M0Lm5indexS1113;
    _M0Lm5indexS1113 = _M0L6_2atmpS2822 + 1;
    _M0L6_2atmpS2823 = _M0Lm3expS1118;
    if (_M0L6_2atmpS2823 < 0) {
      int32_t _M0L6_2atmpS2824 = _M0Lm5indexS1113;
      int32_t _M0L6_2atmpS2825;
      int32_t _M0L6_2atmpS2826;
      if (
        _M0L6_2atmpS2824 < 0
        || _M0L6_2atmpS2824 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2824] = 45;
      _M0L6_2atmpS2825 = _M0Lm5indexS1113;
      _M0Lm5indexS1113 = _M0L6_2atmpS2825 + 1;
      _M0L6_2atmpS2826 = _M0Lm3expS1118;
      _M0Lm3expS1118 = -_M0L6_2atmpS2826;
    } else {
      int32_t _M0L6_2atmpS2827 = _M0Lm5indexS1113;
      int32_t _M0L6_2atmpS2828;
      if (
        _M0L6_2atmpS2827 < 0
        || _M0L6_2atmpS2827 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2827] = 43;
      _M0L6_2atmpS2828 = _M0Lm5indexS1113;
      _M0Lm5indexS1113 = _M0L6_2atmpS2828 + 1;
    }
    _M0L6_2atmpS2829 = _M0Lm3expS1118;
    if (_M0L6_2atmpS2829 >= 100) {
      int32_t _M0L6_2atmpS2845 = _M0Lm3expS1118;
      int32_t _M0L1aS1124 = _M0L6_2atmpS2845 / 100;
      int32_t _M0L6_2atmpS2844 = _M0Lm3expS1118;
      int32_t _M0L6_2atmpS2843 = _M0L6_2atmpS2844 / 10;
      int32_t _M0L1bS1125 = _M0L6_2atmpS2843 % 10;
      int32_t _M0L6_2atmpS2842 = _M0Lm3expS1118;
      int32_t _M0L1cS1126 = _M0L6_2atmpS2842 % 10;
      int32_t _M0L6_2atmpS2830 = _M0Lm5indexS1113;
      int32_t _M0L6_2atmpS2832 = 48 + _M0L1aS1124;
      int32_t _M0L6_2atmpS2831 = _M0L6_2atmpS2832 & 0xff;
      int32_t _M0L6_2atmpS2836;
      int32_t _M0L6_2atmpS2833;
      int32_t _M0L6_2atmpS2835;
      int32_t _M0L6_2atmpS2834;
      int32_t _M0L6_2atmpS2840;
      int32_t _M0L6_2atmpS2837;
      int32_t _M0L6_2atmpS2839;
      int32_t _M0L6_2atmpS2838;
      int32_t _M0L6_2atmpS2841;
      if (
        _M0L6_2atmpS2830 < 0
        || _M0L6_2atmpS2830 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2830] = _M0L6_2atmpS2831;
      _M0L6_2atmpS2836 = _M0Lm5indexS1113;
      _M0L6_2atmpS2833 = _M0L6_2atmpS2836 + 1;
      _M0L6_2atmpS2835 = 48 + _M0L1bS1125;
      _M0L6_2atmpS2834 = _M0L6_2atmpS2835 & 0xff;
      if (
        _M0L6_2atmpS2833 < 0
        || _M0L6_2atmpS2833 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2833] = _M0L6_2atmpS2834;
      _M0L6_2atmpS2840 = _M0Lm5indexS1113;
      _M0L6_2atmpS2837 = _M0L6_2atmpS2840 + 2;
      _M0L6_2atmpS2839 = 48 + _M0L1cS1126;
      _M0L6_2atmpS2838 = _M0L6_2atmpS2839 & 0xff;
      if (
        _M0L6_2atmpS2837 < 0
        || _M0L6_2atmpS2837 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2837] = _M0L6_2atmpS2838;
      _M0L6_2atmpS2841 = _M0Lm5indexS1113;
      _M0Lm5indexS1113 = _M0L6_2atmpS2841 + 3;
    } else {
      int32_t _M0L6_2atmpS2846 = _M0Lm3expS1118;
      if (_M0L6_2atmpS2846 >= 10) {
        int32_t _M0L6_2atmpS2856 = _M0Lm3expS1118;
        int32_t _M0L1aS1127 = _M0L6_2atmpS2856 / 10;
        int32_t _M0L6_2atmpS2855 = _M0Lm3expS1118;
        int32_t _M0L1bS1128 = _M0L6_2atmpS2855 % 10;
        int32_t _M0L6_2atmpS2847 = _M0Lm5indexS1113;
        int32_t _M0L6_2atmpS2849 = 48 + _M0L1aS1127;
        int32_t _M0L6_2atmpS2848 = _M0L6_2atmpS2849 & 0xff;
        int32_t _M0L6_2atmpS2853;
        int32_t _M0L6_2atmpS2850;
        int32_t _M0L6_2atmpS2852;
        int32_t _M0L6_2atmpS2851;
        int32_t _M0L6_2atmpS2854;
        if (
          _M0L6_2atmpS2847 < 0
          || _M0L6_2atmpS2847 >= Moonbit_array_length(_M0L6resultS1112)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1112[_M0L6_2atmpS2847] = _M0L6_2atmpS2848;
        _M0L6_2atmpS2853 = _M0Lm5indexS1113;
        _M0L6_2atmpS2850 = _M0L6_2atmpS2853 + 1;
        _M0L6_2atmpS2852 = 48 + _M0L1bS1128;
        _M0L6_2atmpS2851 = _M0L6_2atmpS2852 & 0xff;
        if (
          _M0L6_2atmpS2850 < 0
          || _M0L6_2atmpS2850 >= Moonbit_array_length(_M0L6resultS1112)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1112[_M0L6_2atmpS2850] = _M0L6_2atmpS2851;
        _M0L6_2atmpS2854 = _M0Lm5indexS1113;
        _M0Lm5indexS1113 = _M0L6_2atmpS2854 + 2;
      } else {
        int32_t _M0L6_2atmpS2857 = _M0Lm5indexS1113;
        int32_t _M0L6_2atmpS2860 = _M0Lm3expS1118;
        int32_t _M0L6_2atmpS2859 = 48 + _M0L6_2atmpS2860;
        int32_t _M0L6_2atmpS2858 = _M0L6_2atmpS2859 & 0xff;
        int32_t _M0L6_2atmpS2861;
        if (
          _M0L6_2atmpS2857 < 0
          || _M0L6_2atmpS2857 >= Moonbit_array_length(_M0L6resultS1112)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1112[_M0L6_2atmpS2857] = _M0L6_2atmpS2858;
        _M0L6_2atmpS2861 = _M0Lm5indexS1113;
        _M0Lm5indexS1113 = _M0L6_2atmpS2861 + 1;
      }
    }
    _M0L6_2atmpS2862 = _M0Lm5indexS1113;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1112, 0, _M0L6_2atmpS2862);
  } else {
    int32_t _M0L6_2atmpS2863 = _M0Lm3expS1118;
    int32_t _M0L6_2atmpS2923;
    if (_M0L6_2atmpS2863 < 0) {
      int32_t _M0L6_2atmpS2864 = _M0Lm5indexS1113;
      int32_t _M0L6_2atmpS2865;
      int32_t _M0L6_2atmpS2866;
      int32_t _M0L6_2atmpS2867;
      int32_t _M0L1iS1129;
      int32_t _M0L7currentS1131;
      int32_t _M0L1iS1132;
      if (
        _M0L6_2atmpS2864 < 0
        || _M0L6_2atmpS2864 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2864] = 48;
      _M0L6_2atmpS2865 = _M0Lm5indexS1113;
      _M0Lm5indexS1113 = _M0L6_2atmpS2865 + 1;
      _M0L6_2atmpS2866 = _M0Lm5indexS1113;
      if (
        _M0L6_2atmpS2866 < 0
        || _M0L6_2atmpS2866 >= Moonbit_array_length(_M0L6resultS1112)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1112[_M0L6_2atmpS2866] = 46;
      _M0L6_2atmpS2867 = _M0Lm5indexS1113;
      _M0Lm5indexS1113 = _M0L6_2atmpS2867 + 1;
      _M0L1iS1129 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2868 = _M0Lm3expS1118;
        if (_M0L1iS1129 > _M0L6_2atmpS2868) {
          int32_t _M0L6_2atmpS2869 = _M0Lm5indexS1113;
          int32_t _M0L6_2atmpS2870;
          int32_t _M0L6_2atmpS2871;
          if (
            _M0L6_2atmpS2869 < 0
            || _M0L6_2atmpS2869 >= Moonbit_array_length(_M0L6resultS1112)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1112[_M0L6_2atmpS2869] = 48;
          _M0L6_2atmpS2870 = _M0Lm5indexS1113;
          _M0Lm5indexS1113 = _M0L6_2atmpS2870 + 1;
          _M0L6_2atmpS2871 = _M0L1iS1129 - 1;
          _M0L1iS1129 = _M0L6_2atmpS2871;
          continue;
        }
        break;
      }
      _M0L7currentS1131 = _M0Lm5indexS1113;
      _M0L1iS1132 = 0;
      while (1) {
        if (_M0L1iS1132 < _M0L7olengthS1117) {
          int32_t _M0L6_2atmpS2879 = _M0L7currentS1131 + _M0L7olengthS1117;
          int32_t _M0L6_2atmpS2878 = _M0L6_2atmpS2879 - _M0L1iS1132;
          int32_t _M0L6_2atmpS2872 = _M0L6_2atmpS2878 - 1;
          uint64_t _M0L6_2atmpS2877 = _M0Lm6outputS1115;
          uint64_t _M0L6_2atmpS2876 = _M0L6_2atmpS2877 % 10ull;
          int32_t _M0L6_2atmpS2875 = (int32_t)_M0L6_2atmpS2876;
          int32_t _M0L6_2atmpS2874 = 48 + _M0L6_2atmpS2875;
          int32_t _M0L6_2atmpS2873 = _M0L6_2atmpS2874 & 0xff;
          uint64_t _M0L6_2atmpS2880;
          int32_t _M0L6_2atmpS2881;
          int32_t _M0L6_2atmpS2882;
          if (
            _M0L6_2atmpS2872 < 0
            || _M0L6_2atmpS2872 >= Moonbit_array_length(_M0L6resultS1112)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1112[_M0L6_2atmpS2872] = _M0L6_2atmpS2873;
          _M0L6_2atmpS2880 = _M0Lm6outputS1115;
          _M0Lm6outputS1115 = _M0L6_2atmpS2880 / 10ull;
          _M0L6_2atmpS2881 = _M0Lm5indexS1113;
          _M0Lm5indexS1113 = _M0L6_2atmpS2881 + 1;
          _M0L6_2atmpS2882 = _M0L1iS1132 + 1;
          _M0L1iS1132 = _M0L6_2atmpS2882;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2884 = _M0Lm3expS1118;
      int32_t _M0L6_2atmpS2883 = _M0L6_2atmpS2884 + 1;
      if (_M0L6_2atmpS2883 >= _M0L7olengthS1117) {
        int32_t _M0L1iS1134 = 0;
        int32_t _M0L6_2atmpS2896;
        int32_t _M0L6_2atmpS2900;
        int32_t _M0L7_2abindS1136;
        int32_t _M0L2__S1137;
        while (1) {
          if (_M0L1iS1134 < _M0L7olengthS1117) {
            int32_t _M0L6_2atmpS2893 = _M0Lm5indexS1113;
            int32_t _M0L6_2atmpS2892 = _M0L6_2atmpS2893 + _M0L7olengthS1117;
            int32_t _M0L6_2atmpS2891 = _M0L6_2atmpS2892 - _M0L1iS1134;
            int32_t _M0L6_2atmpS2885 = _M0L6_2atmpS2891 - 1;
            uint64_t _M0L6_2atmpS2890 = _M0Lm6outputS1115;
            uint64_t _M0L6_2atmpS2889 = _M0L6_2atmpS2890 % 10ull;
            int32_t _M0L6_2atmpS2888 = (int32_t)_M0L6_2atmpS2889;
            int32_t _M0L6_2atmpS2887 = 48 + _M0L6_2atmpS2888;
            int32_t _M0L6_2atmpS2886 = _M0L6_2atmpS2887 & 0xff;
            uint64_t _M0L6_2atmpS2894;
            int32_t _M0L6_2atmpS2895;
            if (
              _M0L6_2atmpS2885 < 0
              || _M0L6_2atmpS2885 >= Moonbit_array_length(_M0L6resultS1112)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1112[_M0L6_2atmpS2885] = _M0L6_2atmpS2886;
            _M0L6_2atmpS2894 = _M0Lm6outputS1115;
            _M0Lm6outputS1115 = _M0L6_2atmpS2894 / 10ull;
            _M0L6_2atmpS2895 = _M0L1iS1134 + 1;
            _M0L1iS1134 = _M0L6_2atmpS2895;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2896 = _M0Lm5indexS1113;
        _M0Lm5indexS1113 = _M0L6_2atmpS2896 + _M0L7olengthS1117;
        _M0L6_2atmpS2900 = _M0Lm3expS1118;
        _M0L7_2abindS1136 = _M0L6_2atmpS2900 + 1;
        _M0L2__S1137 = _M0L7olengthS1117;
        while (1) {
          if (_M0L2__S1137 < _M0L7_2abindS1136) {
            int32_t _M0L6_2atmpS2897 = _M0Lm5indexS1113;
            int32_t _M0L6_2atmpS2898;
            int32_t _M0L6_2atmpS2899;
            if (
              _M0L6_2atmpS2897 < 0
              || _M0L6_2atmpS2897 >= Moonbit_array_length(_M0L6resultS1112)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1112[_M0L6_2atmpS2897] = 48;
            _M0L6_2atmpS2898 = _M0Lm5indexS1113;
            _M0Lm5indexS1113 = _M0L6_2atmpS2898 + 1;
            _M0L6_2atmpS2899 = _M0L2__S1137 + 1;
            _M0L2__S1137 = _M0L6_2atmpS2899;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2922 = _M0Lm5indexS1113;
        int32_t _M0Lm7currentS1139 = _M0L6_2atmpS2922 + 1;
        int32_t _M0L1iS1140 = 0;
        int32_t _M0L6_2atmpS2920;
        int32_t _M0L6_2atmpS2921;
        while (1) {
          if (_M0L1iS1140 < _M0L7olengthS1117) {
            int32_t _M0L6_2atmpS2903 = _M0L7olengthS1117 - _M0L1iS1140;
            int32_t _M0L6_2atmpS2901 = _M0L6_2atmpS2903 - 1;
            int32_t _M0L6_2atmpS2902 = _M0Lm3expS1118;
            int32_t _M0L6_2atmpS2917;
            int32_t _M0L6_2atmpS2916;
            int32_t _M0L6_2atmpS2915;
            int32_t _M0L6_2atmpS2909;
            uint64_t _M0L6_2atmpS2914;
            uint64_t _M0L6_2atmpS2913;
            int32_t _M0L6_2atmpS2912;
            int32_t _M0L6_2atmpS2911;
            int32_t _M0L6_2atmpS2910;
            uint64_t _M0L6_2atmpS2918;
            int32_t _M0L6_2atmpS2919;
            if (_M0L6_2atmpS2901 == _M0L6_2atmpS2902) {
              int32_t _M0L6_2atmpS2907 = _M0Lm7currentS1139;
              int32_t _M0L6_2atmpS2906 = _M0L6_2atmpS2907 + _M0L7olengthS1117;
              int32_t _M0L6_2atmpS2905 = _M0L6_2atmpS2906 - _M0L1iS1140;
              int32_t _M0L6_2atmpS2904 = _M0L6_2atmpS2905 - 1;
              int32_t _M0L6_2atmpS2908;
              if (
                _M0L6_2atmpS2904 < 0
                || _M0L6_2atmpS2904 >= Moonbit_array_length(_M0L6resultS1112)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1112[_M0L6_2atmpS2904] = 46;
              _M0L6_2atmpS2908 = _M0Lm7currentS1139;
              _M0Lm7currentS1139 = _M0L6_2atmpS2908 - 1;
            }
            _M0L6_2atmpS2917 = _M0Lm7currentS1139;
            _M0L6_2atmpS2916 = _M0L6_2atmpS2917 + _M0L7olengthS1117;
            _M0L6_2atmpS2915 = _M0L6_2atmpS2916 - _M0L1iS1140;
            _M0L6_2atmpS2909 = _M0L6_2atmpS2915 - 1;
            _M0L6_2atmpS2914 = _M0Lm6outputS1115;
            _M0L6_2atmpS2913 = _M0L6_2atmpS2914 % 10ull;
            _M0L6_2atmpS2912 = (int32_t)_M0L6_2atmpS2913;
            _M0L6_2atmpS2911 = 48 + _M0L6_2atmpS2912;
            _M0L6_2atmpS2910 = _M0L6_2atmpS2911 & 0xff;
            if (
              _M0L6_2atmpS2909 < 0
              || _M0L6_2atmpS2909 >= Moonbit_array_length(_M0L6resultS1112)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1112[_M0L6_2atmpS2909] = _M0L6_2atmpS2910;
            _M0L6_2atmpS2918 = _M0Lm6outputS1115;
            _M0Lm6outputS1115 = _M0L6_2atmpS2918 / 10ull;
            _M0L6_2atmpS2919 = _M0L1iS1140 + 1;
            _M0L1iS1140 = _M0L6_2atmpS2919;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2920 = _M0Lm5indexS1113;
        _M0L6_2atmpS2921 = _M0L7olengthS1117 + 1;
        _M0Lm5indexS1113 = _M0L6_2atmpS2920 + _M0L6_2atmpS2921;
      }
    }
    _M0L6_2atmpS2923 = _M0Lm5indexS1113;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1112, 0, _M0L6_2atmpS2923);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1058,
  uint32_t _M0L12ieeeExponentS1057
) {
  int32_t _M0Lm2e2S1055;
  uint64_t _M0Lm2m2S1056;
  uint64_t _M0L6_2atmpS2798;
  uint64_t _M0L6_2atmpS2797;
  int32_t _M0L4evenS1059;
  uint64_t _M0L6_2atmpS2796;
  uint64_t _M0L2mvS1060;
  int32_t _M0L7mmShiftS1061;
  uint64_t _M0Lm2vrS1062;
  uint64_t _M0Lm2vpS1063;
  uint64_t _M0Lm2vmS1064;
  int32_t _M0Lm3e10S1065;
  int32_t _M0Lm17vmIsTrailingZerosS1066;
  int32_t _M0Lm17vrIsTrailingZerosS1067;
  int32_t _M0L6_2atmpS2698;
  int32_t _M0Lm7removedS1086;
  int32_t _M0Lm16lastRemovedDigitS1087;
  uint64_t _M0Lm6outputS1088;
  int32_t _M0L6_2atmpS2794;
  int32_t _M0L6_2atmpS2795;
  int32_t _M0L3expS1111;
  uint64_t _M0L6_2atmpS2793;
  struct _M0TPB17FloatingDecimal64* _block_3985;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S1055 = 0;
  _M0Lm2m2S1056 = 0ull;
  if (_M0L12ieeeExponentS1057 == 0u) {
    _M0Lm2e2S1055 = -1076;
    _M0Lm2m2S1056 = _M0L12ieeeMantissaS1058;
  } else {
    int32_t _M0L6_2atmpS2697 = *(int32_t*)&_M0L12ieeeExponentS1057;
    int32_t _M0L6_2atmpS2696 = _M0L6_2atmpS2697 - 1023;
    int32_t _M0L6_2atmpS2695 = _M0L6_2atmpS2696 - 52;
    _M0Lm2e2S1055 = _M0L6_2atmpS2695 - 2;
    _M0Lm2m2S1056 = 4503599627370496ull | _M0L12ieeeMantissaS1058;
  }
  _M0L6_2atmpS2798 = _M0Lm2m2S1056;
  _M0L6_2atmpS2797 = _M0L6_2atmpS2798 & 1ull;
  _M0L4evenS1059 = _M0L6_2atmpS2797 == 0ull;
  _M0L6_2atmpS2796 = _M0Lm2m2S1056;
  _M0L2mvS1060 = 4ull * _M0L6_2atmpS2796;
  if (_M0L12ieeeMantissaS1058 != 0ull) {
    _M0L7mmShiftS1061 = 1;
  } else {
    _M0L7mmShiftS1061 = _M0L12ieeeExponentS1057 <= 1u;
  }
  _M0Lm2vrS1062 = 0ull;
  _M0Lm2vpS1063 = 0ull;
  _M0Lm2vmS1064 = 0ull;
  _M0Lm3e10S1065 = 0;
  _M0Lm17vmIsTrailingZerosS1066 = 0;
  _M0Lm17vrIsTrailingZerosS1067 = 0;
  _M0L6_2atmpS2698 = _M0Lm2e2S1055;
  if (_M0L6_2atmpS2698 >= 0) {
    int32_t _M0L6_2atmpS2720 = _M0Lm2e2S1055;
    int32_t _M0L6_2atmpS2716;
    int32_t _M0L6_2atmpS2719;
    int32_t _M0L6_2atmpS2718;
    int32_t _M0L6_2atmpS2717;
    int32_t _M0L1qS1068;
    int32_t _M0L6_2atmpS2715;
    int32_t _M0L6_2atmpS2714;
    int32_t _M0L1kS1069;
    int32_t _M0L6_2atmpS2713;
    int32_t _M0L6_2atmpS2712;
    int32_t _M0L6_2atmpS2711;
    int32_t _M0L1iS1070;
    struct _M0TPB8Pow5Pair _M0L4pow5S1071;
    uint64_t _M0L6_2atmpS2710;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1072;
    uint64_t _M0L8_2avrOutS1073;
    uint64_t _M0L8_2avpOutS1074;
    uint64_t _M0L8_2avmOutS1075;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2716 = _M0FPB9log10Pow2(_M0L6_2atmpS2720);
    _M0L6_2atmpS2719 = _M0Lm2e2S1055;
    _M0L6_2atmpS2718 = _M0L6_2atmpS2719 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2717 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2718);
    _M0L1qS1068 = _M0L6_2atmpS2716 - _M0L6_2atmpS2717;
    _M0Lm3e10S1065 = _M0L1qS1068;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2715 = _M0FPB8pow5bits(_M0L1qS1068);
    _M0L6_2atmpS2714 = 125 + _M0L6_2atmpS2715;
    _M0L1kS1069 = _M0L6_2atmpS2714 - 1;
    _M0L6_2atmpS2713 = _M0Lm2e2S1055;
    _M0L6_2atmpS2712 = -_M0L6_2atmpS2713;
    _M0L6_2atmpS2711 = _M0L6_2atmpS2712 + _M0L1qS1068;
    _M0L1iS1070 = _M0L6_2atmpS2711 + _M0L1kS1069;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1071 = _M0FPB22double__computeInvPow5(_M0L1qS1068);
    _M0L6_2atmpS2710 = _M0Lm2m2S1056;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1072
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2710, _M0L4pow5S1071, _M0L1iS1070, _M0L7mmShiftS1061);
    _M0L8_2avrOutS1073 = _M0L7_2abindS1072.$0;
    _M0L8_2avpOutS1074 = _M0L7_2abindS1072.$1;
    _M0L8_2avmOutS1075 = _M0L7_2abindS1072.$2;
    _M0Lm2vrS1062 = _M0L8_2avrOutS1073;
    _M0Lm2vpS1063 = _M0L8_2avpOutS1074;
    _M0Lm2vmS1064 = _M0L8_2avmOutS1075;
    if (_M0L1qS1068 <= 21) {
      int32_t _M0L6_2atmpS2706 = (int32_t)_M0L2mvS1060;
      uint64_t _M0L6_2atmpS2709 = _M0L2mvS1060 / 5ull;
      int32_t _M0L6_2atmpS2708 = (int32_t)_M0L6_2atmpS2709;
      int32_t _M0L6_2atmpS2707 = 5 * _M0L6_2atmpS2708;
      int32_t _M0L6mvMod5S1076 = _M0L6_2atmpS2706 - _M0L6_2atmpS2707;
      if (_M0L6mvMod5S1076 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1067
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1060, _M0L1qS1068);
      } else if (_M0L4evenS1059) {
        uint64_t _M0L6_2atmpS2700 = _M0L2mvS1060 - 1ull;
        uint64_t _M0L6_2atmpS2701;
        uint64_t _M0L6_2atmpS2699;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2701 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1061);
        _M0L6_2atmpS2699 = _M0L6_2atmpS2700 - _M0L6_2atmpS2701;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1066
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2699, _M0L1qS1068);
      } else {
        uint64_t _M0L6_2atmpS2702 = _M0Lm2vpS1063;
        uint64_t _M0L6_2atmpS2705 = _M0L2mvS1060 + 2ull;
        int32_t _M0L6_2atmpS2704;
        uint64_t _M0L6_2atmpS2703;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2704
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2705, _M0L1qS1068);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2703 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2704);
        _M0Lm2vpS1063 = _M0L6_2atmpS2702 - _M0L6_2atmpS2703;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2734 = _M0Lm2e2S1055;
    int32_t _M0L6_2atmpS2733 = -_M0L6_2atmpS2734;
    int32_t _M0L6_2atmpS2728;
    int32_t _M0L6_2atmpS2732;
    int32_t _M0L6_2atmpS2731;
    int32_t _M0L6_2atmpS2730;
    int32_t _M0L6_2atmpS2729;
    int32_t _M0L1qS1077;
    int32_t _M0L6_2atmpS2721;
    int32_t _M0L6_2atmpS2727;
    int32_t _M0L6_2atmpS2726;
    int32_t _M0L1iS1078;
    int32_t _M0L6_2atmpS2725;
    int32_t _M0L1kS1079;
    int32_t _M0L1jS1080;
    struct _M0TPB8Pow5Pair _M0L4pow5S1081;
    uint64_t _M0L6_2atmpS2724;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1082;
    uint64_t _M0L8_2avrOutS1083;
    uint64_t _M0L8_2avpOutS1084;
    uint64_t _M0L8_2avmOutS1085;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2728 = _M0FPB9log10Pow5(_M0L6_2atmpS2733);
    _M0L6_2atmpS2732 = _M0Lm2e2S1055;
    _M0L6_2atmpS2731 = -_M0L6_2atmpS2732;
    _M0L6_2atmpS2730 = _M0L6_2atmpS2731 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2729 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2730);
    _M0L1qS1077 = _M0L6_2atmpS2728 - _M0L6_2atmpS2729;
    _M0L6_2atmpS2721 = _M0Lm2e2S1055;
    _M0Lm3e10S1065 = _M0L1qS1077 + _M0L6_2atmpS2721;
    _M0L6_2atmpS2727 = _M0Lm2e2S1055;
    _M0L6_2atmpS2726 = -_M0L6_2atmpS2727;
    _M0L1iS1078 = _M0L6_2atmpS2726 - _M0L1qS1077;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2725 = _M0FPB8pow5bits(_M0L1iS1078);
    _M0L1kS1079 = _M0L6_2atmpS2725 - 125;
    _M0L1jS1080 = _M0L1qS1077 - _M0L1kS1079;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1081 = _M0FPB19double__computePow5(_M0L1iS1078);
    _M0L6_2atmpS2724 = _M0Lm2m2S1056;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1082
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2724, _M0L4pow5S1081, _M0L1jS1080, _M0L7mmShiftS1061);
    _M0L8_2avrOutS1083 = _M0L7_2abindS1082.$0;
    _M0L8_2avpOutS1084 = _M0L7_2abindS1082.$1;
    _M0L8_2avmOutS1085 = _M0L7_2abindS1082.$2;
    _M0Lm2vrS1062 = _M0L8_2avrOutS1083;
    _M0Lm2vpS1063 = _M0L8_2avpOutS1084;
    _M0Lm2vmS1064 = _M0L8_2avmOutS1085;
    if (_M0L1qS1077 <= 1) {
      _M0Lm17vrIsTrailingZerosS1067 = 1;
      if (_M0L4evenS1059) {
        int32_t _M0L6_2atmpS2722;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2722 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1061);
        _M0Lm17vmIsTrailingZerosS1066 = _M0L6_2atmpS2722 == 1;
      } else {
        uint64_t _M0L6_2atmpS2723 = _M0Lm2vpS1063;
        _M0Lm2vpS1063 = _M0L6_2atmpS2723 - 1ull;
      }
    } else if (_M0L1qS1077 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1067
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1060, _M0L1qS1077);
    }
  }
  _M0Lm7removedS1086 = 0;
  _M0Lm16lastRemovedDigitS1087 = 0;
  _M0Lm6outputS1088 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1066 || _M0Lm17vrIsTrailingZerosS1067) {
    int32_t _if__result_3982;
    uint64_t _M0L6_2atmpS2764;
    uint64_t _M0L6_2atmpS2770;
    uint64_t _M0L6_2atmpS2771;
    int32_t _if__result_3983;
    int32_t _M0L6_2atmpS2767;
    int64_t _M0L6_2atmpS2766;
    uint64_t _M0L6_2atmpS2765;
    while (1) {
      uint64_t _M0L6_2atmpS2747 = _M0Lm2vpS1063;
      uint64_t _M0L7vpDiv10S1089 = _M0L6_2atmpS2747 / 10ull;
      uint64_t _M0L6_2atmpS2746 = _M0Lm2vmS1064;
      uint64_t _M0L7vmDiv10S1090 = _M0L6_2atmpS2746 / 10ull;
      uint64_t _M0L6_2atmpS2745;
      int32_t _M0L6_2atmpS2742;
      int32_t _M0L6_2atmpS2744;
      int32_t _M0L6_2atmpS2743;
      int32_t _M0L7vmMod10S1092;
      uint64_t _M0L6_2atmpS2741;
      uint64_t _M0L7vrDiv10S1093;
      uint64_t _M0L6_2atmpS2740;
      int32_t _M0L6_2atmpS2737;
      int32_t _M0L6_2atmpS2739;
      int32_t _M0L6_2atmpS2738;
      int32_t _M0L7vrMod10S1094;
      int32_t _M0L6_2atmpS2736;
      if (_M0L7vpDiv10S1089 <= _M0L7vmDiv10S1090) {
        break;
      }
      _M0L6_2atmpS2745 = _M0Lm2vmS1064;
      _M0L6_2atmpS2742 = (int32_t)_M0L6_2atmpS2745;
      _M0L6_2atmpS2744 = (int32_t)_M0L7vmDiv10S1090;
      _M0L6_2atmpS2743 = 10 * _M0L6_2atmpS2744;
      _M0L7vmMod10S1092 = _M0L6_2atmpS2742 - _M0L6_2atmpS2743;
      _M0L6_2atmpS2741 = _M0Lm2vrS1062;
      _M0L7vrDiv10S1093 = _M0L6_2atmpS2741 / 10ull;
      _M0L6_2atmpS2740 = _M0Lm2vrS1062;
      _M0L6_2atmpS2737 = (int32_t)_M0L6_2atmpS2740;
      _M0L6_2atmpS2739 = (int32_t)_M0L7vrDiv10S1093;
      _M0L6_2atmpS2738 = 10 * _M0L6_2atmpS2739;
      _M0L7vrMod10S1094 = _M0L6_2atmpS2737 - _M0L6_2atmpS2738;
      if (_M0Lm17vmIsTrailingZerosS1066) {
        _M0Lm17vmIsTrailingZerosS1066 = _M0L7vmMod10S1092 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1066 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1067) {
        int32_t _M0L6_2atmpS2735 = _M0Lm16lastRemovedDigitS1087;
        _M0Lm17vrIsTrailingZerosS1067 = _M0L6_2atmpS2735 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1067 = 0;
      }
      _M0Lm16lastRemovedDigitS1087 = _M0L7vrMod10S1094;
      _M0Lm2vrS1062 = _M0L7vrDiv10S1093;
      _M0Lm2vpS1063 = _M0L7vpDiv10S1089;
      _M0Lm2vmS1064 = _M0L7vmDiv10S1090;
      _M0L6_2atmpS2736 = _M0Lm7removedS1086;
      _M0Lm7removedS1086 = _M0L6_2atmpS2736 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1066) {
      while (1) {
        uint64_t _M0L6_2atmpS2760 = _M0Lm2vmS1064;
        uint64_t _M0L7vmDiv10S1095 = _M0L6_2atmpS2760 / 10ull;
        uint64_t _M0L6_2atmpS2759 = _M0Lm2vmS1064;
        int32_t _M0L6_2atmpS2756 = (int32_t)_M0L6_2atmpS2759;
        int32_t _M0L6_2atmpS2758 = (int32_t)_M0L7vmDiv10S1095;
        int32_t _M0L6_2atmpS2757 = 10 * _M0L6_2atmpS2758;
        int32_t _M0L7vmMod10S1096 = _M0L6_2atmpS2756 - _M0L6_2atmpS2757;
        uint64_t _M0L6_2atmpS2755;
        uint64_t _M0L7vpDiv10S1098;
        uint64_t _M0L6_2atmpS2754;
        uint64_t _M0L7vrDiv10S1099;
        uint64_t _M0L6_2atmpS2753;
        int32_t _M0L6_2atmpS2750;
        int32_t _M0L6_2atmpS2752;
        int32_t _M0L6_2atmpS2751;
        int32_t _M0L7vrMod10S1100;
        int32_t _M0L6_2atmpS2749;
        if (_M0L7vmMod10S1096 != 0) {
          break;
        }
        _M0L6_2atmpS2755 = _M0Lm2vpS1063;
        _M0L7vpDiv10S1098 = _M0L6_2atmpS2755 / 10ull;
        _M0L6_2atmpS2754 = _M0Lm2vrS1062;
        _M0L7vrDiv10S1099 = _M0L6_2atmpS2754 / 10ull;
        _M0L6_2atmpS2753 = _M0Lm2vrS1062;
        _M0L6_2atmpS2750 = (int32_t)_M0L6_2atmpS2753;
        _M0L6_2atmpS2752 = (int32_t)_M0L7vrDiv10S1099;
        _M0L6_2atmpS2751 = 10 * _M0L6_2atmpS2752;
        _M0L7vrMod10S1100 = _M0L6_2atmpS2750 - _M0L6_2atmpS2751;
        if (_M0Lm17vrIsTrailingZerosS1067) {
          int32_t _M0L6_2atmpS2748 = _M0Lm16lastRemovedDigitS1087;
          _M0Lm17vrIsTrailingZerosS1067 = _M0L6_2atmpS2748 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1067 = 0;
        }
        _M0Lm16lastRemovedDigitS1087 = _M0L7vrMod10S1100;
        _M0Lm2vrS1062 = _M0L7vrDiv10S1099;
        _M0Lm2vpS1063 = _M0L7vpDiv10S1098;
        _M0Lm2vmS1064 = _M0L7vmDiv10S1095;
        _M0L6_2atmpS2749 = _M0Lm7removedS1086;
        _M0Lm7removedS1086 = _M0L6_2atmpS2749 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1067) {
      int32_t _M0L6_2atmpS2763 = _M0Lm16lastRemovedDigitS1087;
      if (_M0L6_2atmpS2763 == 5) {
        uint64_t _M0L6_2atmpS2762 = _M0Lm2vrS1062;
        uint64_t _M0L6_2atmpS2761 = _M0L6_2atmpS2762 % 2ull;
        _if__result_3982 = _M0L6_2atmpS2761 == 0ull;
      } else {
        _if__result_3982 = 0;
      }
    } else {
      _if__result_3982 = 0;
    }
    if (_if__result_3982) {
      _M0Lm16lastRemovedDigitS1087 = 4;
    }
    _M0L6_2atmpS2764 = _M0Lm2vrS1062;
    _M0L6_2atmpS2770 = _M0Lm2vrS1062;
    _M0L6_2atmpS2771 = _M0Lm2vmS1064;
    if (_M0L6_2atmpS2770 == _M0L6_2atmpS2771) {
      if (!_M0L4evenS1059) {
        _if__result_3983 = 1;
      } else {
        int32_t _M0L6_2atmpS2769 = _M0Lm17vmIsTrailingZerosS1066;
        _if__result_3983 = !_M0L6_2atmpS2769;
      }
    } else {
      _if__result_3983 = 0;
    }
    if (_if__result_3983) {
      _M0L6_2atmpS2767 = 1;
    } else {
      int32_t _M0L6_2atmpS2768 = _M0Lm16lastRemovedDigitS1087;
      _M0L6_2atmpS2767 = _M0L6_2atmpS2768 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2766 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2767);
    _M0L6_2atmpS2765 = *(uint64_t*)&_M0L6_2atmpS2766;
    _M0Lm6outputS1088 = _M0L6_2atmpS2764 + _M0L6_2atmpS2765;
  } else {
    int32_t _M0Lm7roundUpS1101 = 0;
    uint64_t _M0L6_2atmpS2792 = _M0Lm2vpS1063;
    uint64_t _M0L8vpDiv100S1102 = _M0L6_2atmpS2792 / 100ull;
    uint64_t _M0L6_2atmpS2791 = _M0Lm2vmS1064;
    uint64_t _M0L8vmDiv100S1103 = _M0L6_2atmpS2791 / 100ull;
    uint64_t _M0L6_2atmpS2786;
    uint64_t _M0L6_2atmpS2789;
    uint64_t _M0L6_2atmpS2790;
    int32_t _M0L6_2atmpS2788;
    uint64_t _M0L6_2atmpS2787;
    if (_M0L8vpDiv100S1102 > _M0L8vmDiv100S1103) {
      uint64_t _M0L6_2atmpS2777 = _M0Lm2vrS1062;
      uint64_t _M0L8vrDiv100S1104 = _M0L6_2atmpS2777 / 100ull;
      uint64_t _M0L6_2atmpS2776 = _M0Lm2vrS1062;
      int32_t _M0L6_2atmpS2773 = (int32_t)_M0L6_2atmpS2776;
      int32_t _M0L6_2atmpS2775 = (int32_t)_M0L8vrDiv100S1104;
      int32_t _M0L6_2atmpS2774 = 100 * _M0L6_2atmpS2775;
      int32_t _M0L8vrMod100S1105 = _M0L6_2atmpS2773 - _M0L6_2atmpS2774;
      int32_t _M0L6_2atmpS2772;
      _M0Lm7roundUpS1101 = _M0L8vrMod100S1105 >= 50;
      _M0Lm2vrS1062 = _M0L8vrDiv100S1104;
      _M0Lm2vpS1063 = _M0L8vpDiv100S1102;
      _M0Lm2vmS1064 = _M0L8vmDiv100S1103;
      _M0L6_2atmpS2772 = _M0Lm7removedS1086;
      _M0Lm7removedS1086 = _M0L6_2atmpS2772 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2785 = _M0Lm2vpS1063;
      uint64_t _M0L7vpDiv10S1106 = _M0L6_2atmpS2785 / 10ull;
      uint64_t _M0L6_2atmpS2784 = _M0Lm2vmS1064;
      uint64_t _M0L7vmDiv10S1107 = _M0L6_2atmpS2784 / 10ull;
      uint64_t _M0L6_2atmpS2783;
      uint64_t _M0L7vrDiv10S1109;
      uint64_t _M0L6_2atmpS2782;
      int32_t _M0L6_2atmpS2779;
      int32_t _M0L6_2atmpS2781;
      int32_t _M0L6_2atmpS2780;
      int32_t _M0L7vrMod10S1110;
      int32_t _M0L6_2atmpS2778;
      if (_M0L7vpDiv10S1106 <= _M0L7vmDiv10S1107) {
        break;
      }
      _M0L6_2atmpS2783 = _M0Lm2vrS1062;
      _M0L7vrDiv10S1109 = _M0L6_2atmpS2783 / 10ull;
      _M0L6_2atmpS2782 = _M0Lm2vrS1062;
      _M0L6_2atmpS2779 = (int32_t)_M0L6_2atmpS2782;
      _M0L6_2atmpS2781 = (int32_t)_M0L7vrDiv10S1109;
      _M0L6_2atmpS2780 = 10 * _M0L6_2atmpS2781;
      _M0L7vrMod10S1110 = _M0L6_2atmpS2779 - _M0L6_2atmpS2780;
      _M0Lm7roundUpS1101 = _M0L7vrMod10S1110 >= 5;
      _M0Lm2vrS1062 = _M0L7vrDiv10S1109;
      _M0Lm2vpS1063 = _M0L7vpDiv10S1106;
      _M0Lm2vmS1064 = _M0L7vmDiv10S1107;
      _M0L6_2atmpS2778 = _M0Lm7removedS1086;
      _M0Lm7removedS1086 = _M0L6_2atmpS2778 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2786 = _M0Lm2vrS1062;
    _M0L6_2atmpS2789 = _M0Lm2vrS1062;
    _M0L6_2atmpS2790 = _M0Lm2vmS1064;
    _M0L6_2atmpS2788
    = _M0L6_2atmpS2789 == _M0L6_2atmpS2790 || _M0Lm7roundUpS1101;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2787 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2788);
    _M0Lm6outputS1088 = _M0L6_2atmpS2786 + _M0L6_2atmpS2787;
  }
  _M0L6_2atmpS2794 = _M0Lm3e10S1065;
  _M0L6_2atmpS2795 = _M0Lm7removedS1086;
  _M0L3expS1111 = _M0L6_2atmpS2794 + _M0L6_2atmpS2795;
  _M0L6_2atmpS2793 = _M0Lm6outputS1088;
  _block_3985
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3985)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3985->$0 = _M0L6_2atmpS2793;
  _block_3985->$1 = _M0L3expS1111;
  return _block_3985;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1054) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1054) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1053) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1053) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1052) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1052) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1051) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS1051 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1051 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1051 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1051 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1051 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1051 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1051 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1051 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1051 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1051 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1051 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1051 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1051 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1051 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1051 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1051 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1034) {
  int32_t _M0L6_2atmpS2694;
  int32_t _M0L6_2atmpS2693;
  int32_t _M0L4baseS1033;
  int32_t _M0L5base2S1035;
  int32_t _M0L6offsetS1036;
  int32_t _M0L6_2atmpS2692;
  uint64_t _M0L4mul0S1037;
  int32_t _M0L6_2atmpS2691;
  int32_t _M0L6_2atmpS2690;
  uint64_t _M0L4mul1S1038;
  uint64_t _M0L1mS1039;
  struct _M0TPB7Umul128 _M0L7_2abindS1040;
  uint64_t _M0L7_2alow1S1041;
  uint64_t _M0L8_2ahigh1S1042;
  struct _M0TPB7Umul128 _M0L7_2abindS1043;
  uint64_t _M0L7_2alow0S1044;
  uint64_t _M0L8_2ahigh0S1045;
  uint64_t _M0L3sumS1046;
  uint64_t _M0Lm5high1S1047;
  int32_t _M0L6_2atmpS2688;
  int32_t _M0L6_2atmpS2689;
  int32_t _M0L5deltaS1048;
  uint64_t _M0L6_2atmpS2687;
  uint64_t _M0L6_2atmpS2679;
  int32_t _M0L6_2atmpS2686;
  uint32_t _M0L6_2atmpS2683;
  int32_t _M0L6_2atmpS2685;
  int32_t _M0L6_2atmpS2684;
  uint32_t _M0L6_2atmpS2682;
  uint32_t _M0L6_2atmpS2681;
  uint64_t _M0L6_2atmpS2680;
  uint64_t _M0L1aS1049;
  uint64_t _M0L6_2atmpS2678;
  uint64_t _M0L1bS1050;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2694 = _M0L1iS1034 + 26;
  _M0L6_2atmpS2693 = _M0L6_2atmpS2694 - 1;
  _M0L4baseS1033 = _M0L6_2atmpS2693 / 26;
  _M0L5base2S1035 = _M0L4baseS1033 * 26;
  _M0L6offsetS1036 = _M0L5base2S1035 - _M0L1iS1034;
  _M0L6_2atmpS2692 = _M0L4baseS1033 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1037
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2692);
  _M0L6_2atmpS2691 = _M0L4baseS1033 * 2;
  _M0L6_2atmpS2690 = _M0L6_2atmpS2691 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1038
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2690);
  if (_M0L6offsetS1036 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1037, _M0L4mul1S1038};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1039
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1036);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1040 = _M0FPB7umul128(_M0L1mS1039, _M0L4mul1S1038);
  _M0L7_2alow1S1041 = _M0L7_2abindS1040.$0;
  _M0L8_2ahigh1S1042 = _M0L7_2abindS1040.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1043 = _M0FPB7umul128(_M0L1mS1039, _M0L4mul0S1037);
  _M0L7_2alow0S1044 = _M0L7_2abindS1043.$0;
  _M0L8_2ahigh0S1045 = _M0L7_2abindS1043.$1;
  _M0L3sumS1046 = _M0L8_2ahigh0S1045 + _M0L7_2alow1S1041;
  _M0Lm5high1S1047 = _M0L8_2ahigh1S1042;
  if (_M0L3sumS1046 < _M0L8_2ahigh0S1045) {
    uint64_t _M0L6_2atmpS2677 = _M0Lm5high1S1047;
    _M0Lm5high1S1047 = _M0L6_2atmpS2677 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2688 = _M0FPB8pow5bits(_M0L5base2S1035);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2689 = _M0FPB8pow5bits(_M0L1iS1034);
  _M0L5deltaS1048 = _M0L6_2atmpS2688 - _M0L6_2atmpS2689;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2687
  = _M0FPB13shiftright128(_M0L7_2alow0S1044, _M0L3sumS1046, _M0L5deltaS1048);
  _M0L6_2atmpS2679 = _M0L6_2atmpS2687 + 1ull;
  _M0L6_2atmpS2686 = _M0L1iS1034 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2683
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2686);
  _M0L6_2atmpS2685 = _M0L1iS1034 % 16;
  _M0L6_2atmpS2684 = _M0L6_2atmpS2685 << 1;
  _M0L6_2atmpS2682 = _M0L6_2atmpS2683 >> (_M0L6_2atmpS2684 & 31);
  _M0L6_2atmpS2681 = _M0L6_2atmpS2682 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2680 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2681);
  _M0L1aS1049 = _M0L6_2atmpS2679 + _M0L6_2atmpS2680;
  _M0L6_2atmpS2678 = _M0Lm5high1S1047;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1050
  = _M0FPB13shiftright128(_M0L3sumS1046, _M0L6_2atmpS2678, _M0L5deltaS1048);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1049, _M0L1bS1050};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1016) {
  int32_t _M0L4baseS1015;
  int32_t _M0L5base2S1017;
  int32_t _M0L6offsetS1018;
  int32_t _M0L6_2atmpS2676;
  uint64_t _M0L4mul0S1019;
  int32_t _M0L6_2atmpS2675;
  int32_t _M0L6_2atmpS2674;
  uint64_t _M0L4mul1S1020;
  uint64_t _M0L1mS1021;
  struct _M0TPB7Umul128 _M0L7_2abindS1022;
  uint64_t _M0L7_2alow1S1023;
  uint64_t _M0L8_2ahigh1S1024;
  struct _M0TPB7Umul128 _M0L7_2abindS1025;
  uint64_t _M0L7_2alow0S1026;
  uint64_t _M0L8_2ahigh0S1027;
  uint64_t _M0L3sumS1028;
  uint64_t _M0Lm5high1S1029;
  int32_t _M0L6_2atmpS2672;
  int32_t _M0L6_2atmpS2673;
  int32_t _M0L5deltaS1030;
  uint64_t _M0L6_2atmpS2664;
  int32_t _M0L6_2atmpS2671;
  uint32_t _M0L6_2atmpS2668;
  int32_t _M0L6_2atmpS2670;
  int32_t _M0L6_2atmpS2669;
  uint32_t _M0L6_2atmpS2667;
  uint32_t _M0L6_2atmpS2666;
  uint64_t _M0L6_2atmpS2665;
  uint64_t _M0L1aS1031;
  uint64_t _M0L6_2atmpS2663;
  uint64_t _M0L1bS1032;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS1015 = _M0L1iS1016 / 26;
  _M0L5base2S1017 = _M0L4baseS1015 * 26;
  _M0L6offsetS1018 = _M0L1iS1016 - _M0L5base2S1017;
  _M0L6_2atmpS2676 = _M0L4baseS1015 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1019
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2676);
  _M0L6_2atmpS2675 = _M0L4baseS1015 * 2;
  _M0L6_2atmpS2674 = _M0L6_2atmpS2675 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1020
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2674);
  if (_M0L6offsetS1018 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1019, _M0L4mul1S1020};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1021
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1018);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1022 = _M0FPB7umul128(_M0L1mS1021, _M0L4mul1S1020);
  _M0L7_2alow1S1023 = _M0L7_2abindS1022.$0;
  _M0L8_2ahigh1S1024 = _M0L7_2abindS1022.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1025 = _M0FPB7umul128(_M0L1mS1021, _M0L4mul0S1019);
  _M0L7_2alow0S1026 = _M0L7_2abindS1025.$0;
  _M0L8_2ahigh0S1027 = _M0L7_2abindS1025.$1;
  _M0L3sumS1028 = _M0L8_2ahigh0S1027 + _M0L7_2alow1S1023;
  _M0Lm5high1S1029 = _M0L8_2ahigh1S1024;
  if (_M0L3sumS1028 < _M0L8_2ahigh0S1027) {
    uint64_t _M0L6_2atmpS2662 = _M0Lm5high1S1029;
    _M0Lm5high1S1029 = _M0L6_2atmpS2662 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2672 = _M0FPB8pow5bits(_M0L1iS1016);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2673 = _M0FPB8pow5bits(_M0L5base2S1017);
  _M0L5deltaS1030 = _M0L6_2atmpS2672 - _M0L6_2atmpS2673;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2664
  = _M0FPB13shiftright128(_M0L7_2alow0S1026, _M0L3sumS1028, _M0L5deltaS1030);
  _M0L6_2atmpS2671 = _M0L1iS1016 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2668
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2671);
  _M0L6_2atmpS2670 = _M0L1iS1016 % 16;
  _M0L6_2atmpS2669 = _M0L6_2atmpS2670 << 1;
  _M0L6_2atmpS2667 = _M0L6_2atmpS2668 >> (_M0L6_2atmpS2669 & 31);
  _M0L6_2atmpS2666 = _M0L6_2atmpS2667 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2665 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2666);
  _M0L1aS1031 = _M0L6_2atmpS2664 + _M0L6_2atmpS2665;
  _M0L6_2atmpS2663 = _M0Lm5high1S1029;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1032
  = _M0FPB13shiftright128(_M0L3sumS1028, _M0L6_2atmpS2663, _M0L5deltaS1030);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1031, _M0L1bS1032};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS989,
  struct _M0TPB8Pow5Pair _M0L3mulS986,
  int32_t _M0L1jS1002,
  int32_t _M0L7mmShiftS1004
) {
  uint64_t _M0L7_2amul0S985;
  uint64_t _M0L7_2amul1S987;
  uint64_t _M0L1mS988;
  struct _M0TPB7Umul128 _M0L7_2abindS990;
  uint64_t _M0L5_2aloS991;
  uint64_t _M0L6_2atmpS992;
  struct _M0TPB7Umul128 _M0L7_2abindS993;
  uint64_t _M0L6_2alo2S994;
  uint64_t _M0L6_2ahi2S995;
  uint64_t _M0L3midS996;
  uint64_t _M0L6_2atmpS2661;
  uint64_t _M0L2hiS997;
  uint64_t _M0L3lo2S998;
  uint64_t _M0L6_2atmpS2659;
  uint64_t _M0L6_2atmpS2660;
  uint64_t _M0L4mid2S999;
  uint64_t _M0L6_2atmpS2658;
  uint64_t _M0L3hi2S1000;
  int32_t _M0L6_2atmpS2657;
  int32_t _M0L6_2atmpS2656;
  uint64_t _M0L2vpS1001;
  uint64_t _M0Lm2vmS1003;
  int32_t _M0L6_2atmpS2655;
  int32_t _M0L6_2atmpS2654;
  uint64_t _M0L2vrS1014;
  uint64_t _M0L6_2atmpS2653;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S985 = _M0L3mulS986.$0;
  _M0L7_2amul1S987 = _M0L3mulS986.$1;
  _M0L1mS988 = _M0L1mS989 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS990 = _M0FPB7umul128(_M0L1mS988, _M0L7_2amul0S985);
  _M0L5_2aloS991 = _M0L7_2abindS990.$0;
  _M0L6_2atmpS992 = _M0L7_2abindS990.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS993 = _M0FPB7umul128(_M0L1mS988, _M0L7_2amul1S987);
  _M0L6_2alo2S994 = _M0L7_2abindS993.$0;
  _M0L6_2ahi2S995 = _M0L7_2abindS993.$1;
  _M0L3midS996 = _M0L6_2atmpS992 + _M0L6_2alo2S994;
  if (_M0L3midS996 < _M0L6_2atmpS992) {
    _M0L6_2atmpS2661 = 1ull;
  } else {
    _M0L6_2atmpS2661 = 0ull;
  }
  _M0L2hiS997 = _M0L6_2ahi2S995 + _M0L6_2atmpS2661;
  _M0L3lo2S998 = _M0L5_2aloS991 + _M0L7_2amul0S985;
  _M0L6_2atmpS2659 = _M0L3midS996 + _M0L7_2amul1S987;
  if (_M0L3lo2S998 < _M0L5_2aloS991) {
    _M0L6_2atmpS2660 = 1ull;
  } else {
    _M0L6_2atmpS2660 = 0ull;
  }
  _M0L4mid2S999 = _M0L6_2atmpS2659 + _M0L6_2atmpS2660;
  if (_M0L4mid2S999 < _M0L3midS996) {
    _M0L6_2atmpS2658 = 1ull;
  } else {
    _M0L6_2atmpS2658 = 0ull;
  }
  _M0L3hi2S1000 = _M0L2hiS997 + _M0L6_2atmpS2658;
  _M0L6_2atmpS2657 = _M0L1jS1002 - 64;
  _M0L6_2atmpS2656 = _M0L6_2atmpS2657 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS1001
  = _M0FPB13shiftright128(_M0L4mid2S999, _M0L3hi2S1000, _M0L6_2atmpS2656);
  _M0Lm2vmS1003 = 0ull;
  if (_M0L7mmShiftS1004) {
    uint64_t _M0L3lo3S1005 = _M0L5_2aloS991 - _M0L7_2amul0S985;
    uint64_t _M0L6_2atmpS2643 = _M0L3midS996 - _M0L7_2amul1S987;
    uint64_t _M0L6_2atmpS2644;
    uint64_t _M0L4mid3S1006;
    uint64_t _M0L6_2atmpS2642;
    uint64_t _M0L3hi3S1007;
    int32_t _M0L6_2atmpS2641;
    int32_t _M0L6_2atmpS2640;
    if (_M0L5_2aloS991 < _M0L3lo3S1005) {
      _M0L6_2atmpS2644 = 1ull;
    } else {
      _M0L6_2atmpS2644 = 0ull;
    }
    _M0L4mid3S1006 = _M0L6_2atmpS2643 - _M0L6_2atmpS2644;
    if (_M0L3midS996 < _M0L4mid3S1006) {
      _M0L6_2atmpS2642 = 1ull;
    } else {
      _M0L6_2atmpS2642 = 0ull;
    }
    _M0L3hi3S1007 = _M0L2hiS997 - _M0L6_2atmpS2642;
    _M0L6_2atmpS2641 = _M0L1jS1002 - 64;
    _M0L6_2atmpS2640 = _M0L6_2atmpS2641 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS1003
    = _M0FPB13shiftright128(_M0L4mid3S1006, _M0L3hi3S1007, _M0L6_2atmpS2640);
  } else {
    uint64_t _M0L3lo3S1008 = _M0L5_2aloS991 + _M0L5_2aloS991;
    uint64_t _M0L6_2atmpS2651 = _M0L3midS996 + _M0L3midS996;
    uint64_t _M0L6_2atmpS2652;
    uint64_t _M0L4mid3S1009;
    uint64_t _M0L6_2atmpS2649;
    uint64_t _M0L6_2atmpS2650;
    uint64_t _M0L3hi3S1010;
    uint64_t _M0L3lo4S1011;
    uint64_t _M0L6_2atmpS2647;
    uint64_t _M0L6_2atmpS2648;
    uint64_t _M0L4mid4S1012;
    uint64_t _M0L6_2atmpS2646;
    uint64_t _M0L3hi4S1013;
    int32_t _M0L6_2atmpS2645;
    if (_M0L3lo3S1008 < _M0L5_2aloS991) {
      _M0L6_2atmpS2652 = 1ull;
    } else {
      _M0L6_2atmpS2652 = 0ull;
    }
    _M0L4mid3S1009 = _M0L6_2atmpS2651 + _M0L6_2atmpS2652;
    _M0L6_2atmpS2649 = _M0L2hiS997 + _M0L2hiS997;
    if (_M0L4mid3S1009 < _M0L3midS996) {
      _M0L6_2atmpS2650 = 1ull;
    } else {
      _M0L6_2atmpS2650 = 0ull;
    }
    _M0L3hi3S1010 = _M0L6_2atmpS2649 + _M0L6_2atmpS2650;
    _M0L3lo4S1011 = _M0L3lo3S1008 - _M0L7_2amul0S985;
    _M0L6_2atmpS2647 = _M0L4mid3S1009 - _M0L7_2amul1S987;
    if (_M0L3lo3S1008 < _M0L3lo4S1011) {
      _M0L6_2atmpS2648 = 1ull;
    } else {
      _M0L6_2atmpS2648 = 0ull;
    }
    _M0L4mid4S1012 = _M0L6_2atmpS2647 - _M0L6_2atmpS2648;
    if (_M0L4mid3S1009 < _M0L4mid4S1012) {
      _M0L6_2atmpS2646 = 1ull;
    } else {
      _M0L6_2atmpS2646 = 0ull;
    }
    _M0L3hi4S1013 = _M0L3hi3S1010 - _M0L6_2atmpS2646;
    _M0L6_2atmpS2645 = _M0L1jS1002 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS1003
    = _M0FPB13shiftright128(_M0L4mid4S1012, _M0L3hi4S1013, _M0L6_2atmpS2645);
  }
  _M0L6_2atmpS2655 = _M0L1jS1002 - 64;
  _M0L6_2atmpS2654 = _M0L6_2atmpS2655 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS1014
  = _M0FPB13shiftright128(_M0L3midS996, _M0L2hiS997, _M0L6_2atmpS2654);
  _M0L6_2atmpS2653 = _M0Lm2vmS1003;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS1014,
                                                _M0L2vpS1001,
                                                _M0L6_2atmpS2653};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS983,
  int32_t _M0L1pS984
) {
  uint64_t _M0L6_2atmpS2639;
  uint64_t _M0L6_2atmpS2638;
  uint64_t _M0L6_2atmpS2637;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2639 = 1ull << (_M0L1pS984 & 63);
  _M0L6_2atmpS2638 = _M0L6_2atmpS2639 - 1ull;
  _M0L6_2atmpS2637 = _M0L5valueS983 & _M0L6_2atmpS2638;
  return _M0L6_2atmpS2637 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS981,
  int32_t _M0L1pS982
) {
  int32_t _M0L6_2atmpS2636;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2636 = _M0FPB10pow5Factor(_M0L5valueS981);
  return _M0L6_2atmpS2636 >= _M0L1pS982;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS977) {
  uint64_t _M0L6_2atmpS2624;
  uint64_t _M0L6_2atmpS2625;
  uint64_t _M0L6_2atmpS2626;
  uint64_t _M0L6_2atmpS2627;
  int32_t _M0Lm5countS978;
  uint64_t _M0Lm5valueS979;
  uint64_t _M0L6_2atmpS2635;
  moonbit_string_t _M0L6_2atmpS2634;
  moonbit_string_t _M0L6_2atmpS3561;
  moonbit_string_t _M0L6_2atmpS2633;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2624 = _M0L5valueS977 % 5ull;
  if (_M0L6_2atmpS2624 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2625 = _M0L5valueS977 % 25ull;
  if (_M0L6_2atmpS2625 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2626 = _M0L5valueS977 % 125ull;
  if (_M0L6_2atmpS2626 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2627 = _M0L5valueS977 % 625ull;
  if (_M0L6_2atmpS2627 != 0ull) {
    return 3;
  }
  _M0Lm5countS978 = 4;
  _M0Lm5valueS979 = _M0L5valueS977 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2628 = _M0Lm5valueS979;
    if (_M0L6_2atmpS2628 > 0ull) {
      uint64_t _M0L6_2atmpS2630 = _M0Lm5valueS979;
      uint64_t _M0L6_2atmpS2629 = _M0L6_2atmpS2630 % 5ull;
      uint64_t _M0L6_2atmpS2631;
      int32_t _M0L6_2atmpS2632;
      if (_M0L6_2atmpS2629 != 0ull) {
        return _M0Lm5countS978;
      }
      _M0L6_2atmpS2631 = _M0Lm5valueS979;
      _M0Lm5valueS979 = _M0L6_2atmpS2631 / 5ull;
      _M0L6_2atmpS2632 = _M0Lm5countS978;
      _M0Lm5countS978 = _M0L6_2atmpS2632 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2635 = _M0Lm5valueS979;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2634
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2635);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3561
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_73.data, _M0L6_2atmpS2634);
  moonbit_decref(_M0L6_2atmpS2634);
  _M0L6_2atmpS2633 = _M0L6_2atmpS3561;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2633, (moonbit_string_t)moonbit_string_literal_74.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS976,
  uint64_t _M0L2hiS974,
  int32_t _M0L4distS975
) {
  int32_t _M0L6_2atmpS2623;
  uint64_t _M0L6_2atmpS2621;
  uint64_t _M0L6_2atmpS2622;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2623 = 64 - _M0L4distS975;
  _M0L6_2atmpS2621 = _M0L2hiS974 << (_M0L6_2atmpS2623 & 63);
  _M0L6_2atmpS2622 = _M0L2loS976 >> (_M0L4distS975 & 63);
  return _M0L6_2atmpS2621 | _M0L6_2atmpS2622;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS964,
  uint64_t _M0L1bS967
) {
  uint64_t _M0L3aLoS963;
  uint64_t _M0L3aHiS965;
  uint64_t _M0L3bLoS966;
  uint64_t _M0L3bHiS968;
  uint64_t _M0L1xS969;
  uint64_t _M0L6_2atmpS2619;
  uint64_t _M0L6_2atmpS2620;
  uint64_t _M0L1yS970;
  uint64_t _M0L6_2atmpS2617;
  uint64_t _M0L6_2atmpS2618;
  uint64_t _M0L1zS971;
  uint64_t _M0L6_2atmpS2615;
  uint64_t _M0L6_2atmpS2616;
  uint64_t _M0L6_2atmpS2613;
  uint64_t _M0L6_2atmpS2614;
  uint64_t _M0L1wS972;
  uint64_t _M0L2loS973;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS963 = _M0L1aS964 & 4294967295ull;
  _M0L3aHiS965 = _M0L1aS964 >> 32;
  _M0L3bLoS966 = _M0L1bS967 & 4294967295ull;
  _M0L3bHiS968 = _M0L1bS967 >> 32;
  _M0L1xS969 = _M0L3aLoS963 * _M0L3bLoS966;
  _M0L6_2atmpS2619 = _M0L3aHiS965 * _M0L3bLoS966;
  _M0L6_2atmpS2620 = _M0L1xS969 >> 32;
  _M0L1yS970 = _M0L6_2atmpS2619 + _M0L6_2atmpS2620;
  _M0L6_2atmpS2617 = _M0L3aLoS963 * _M0L3bHiS968;
  _M0L6_2atmpS2618 = _M0L1yS970 & 4294967295ull;
  _M0L1zS971 = _M0L6_2atmpS2617 + _M0L6_2atmpS2618;
  _M0L6_2atmpS2615 = _M0L3aHiS965 * _M0L3bHiS968;
  _M0L6_2atmpS2616 = _M0L1yS970 >> 32;
  _M0L6_2atmpS2613 = _M0L6_2atmpS2615 + _M0L6_2atmpS2616;
  _M0L6_2atmpS2614 = _M0L1zS971 >> 32;
  _M0L1wS972 = _M0L6_2atmpS2613 + _M0L6_2atmpS2614;
  _M0L2loS973 = _M0L1aS964 * _M0L1bS967;
  return (struct _M0TPB7Umul128){_M0L2loS973, _M0L1wS972};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS958,
  int32_t _M0L4fromS962,
  int32_t _M0L2toS960
) {
  int32_t _M0L6_2atmpS2612;
  struct _M0TPB13StringBuilder* _M0L3bufS957;
  int32_t _M0L1iS959;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2612 = Moonbit_array_length(_M0L5bytesS958);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS957 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2612);
  _M0L1iS959 = _M0L4fromS962;
  while (1) {
    if (_M0L1iS959 < _M0L2toS960) {
      int32_t _M0L6_2atmpS2610;
      int32_t _M0L6_2atmpS2609;
      int32_t _M0L6_2atmpS2611;
      if (
        _M0L1iS959 < 0 || _M0L1iS959 >= Moonbit_array_length(_M0L5bytesS958)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2610 = (int32_t)_M0L5bytesS958[_M0L1iS959];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2609 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2610);
      moonbit_incref(_M0L3bufS957);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS957, _M0L6_2atmpS2609);
      _M0L6_2atmpS2611 = _M0L1iS959 + 1;
      _M0L1iS959 = _M0L6_2atmpS2611;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS958);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS957);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS956) {
  int32_t _M0L6_2atmpS2608;
  uint32_t _M0L6_2atmpS2607;
  uint32_t _M0L6_2atmpS2606;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2608 = _M0L1eS956 * 78913;
  _M0L6_2atmpS2607 = *(uint32_t*)&_M0L6_2atmpS2608;
  _M0L6_2atmpS2606 = _M0L6_2atmpS2607 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2606;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS955) {
  int32_t _M0L6_2atmpS2605;
  uint32_t _M0L6_2atmpS2604;
  uint32_t _M0L6_2atmpS2603;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2605 = _M0L1eS955 * 732923;
  _M0L6_2atmpS2604 = *(uint32_t*)&_M0L6_2atmpS2605;
  _M0L6_2atmpS2603 = _M0L6_2atmpS2604 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2603;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS953,
  int32_t _M0L8exponentS954,
  int32_t _M0L8mantissaS951
) {
  moonbit_string_t _M0L1sS952;
  moonbit_string_t _M0L6_2atmpS3562;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS951) {
    return (moonbit_string_t)moonbit_string_literal_75.data;
  }
  if (_M0L4signS953) {
    _M0L1sS952 = (moonbit_string_t)moonbit_string_literal_76.data;
  } else {
    _M0L1sS952 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS954) {
    moonbit_string_t _M0L6_2atmpS3563;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3563
    = moonbit_add_string(_M0L1sS952, (moonbit_string_t)moonbit_string_literal_77.data);
    moonbit_decref(_M0L1sS952);
    return _M0L6_2atmpS3563;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3562
  = moonbit_add_string(_M0L1sS952, (moonbit_string_t)moonbit_string_literal_78.data);
  moonbit_decref(_M0L1sS952);
  return _M0L6_2atmpS3562;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS950) {
  int32_t _M0L6_2atmpS2602;
  uint32_t _M0L6_2atmpS2601;
  uint32_t _M0L6_2atmpS2600;
  int32_t _M0L6_2atmpS2599;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2602 = _M0L1eS950 * 1217359;
  _M0L6_2atmpS2601 = *(uint32_t*)&_M0L6_2atmpS2602;
  _M0L6_2atmpS2600 = _M0L6_2atmpS2601 >> 19;
  _M0L6_2atmpS2599 = *(int32_t*)&_M0L6_2atmpS2600;
  return _M0L6_2atmpS2599 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS949,
  struct _M0TPB6Hasher* _M0L6hasherS948
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS948, _M0L4selfS949);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS947,
  struct _M0TPB6Hasher* _M0L6hasherS946
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS946, _M0L4selfS947);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS944,
  moonbit_string_t _M0L5valueS942
) {
  int32_t _M0L7_2abindS941;
  int32_t _M0L1iS943;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS941 = Moonbit_array_length(_M0L5valueS942);
  _M0L1iS943 = 0;
  while (1) {
    if (_M0L1iS943 < _M0L7_2abindS941) {
      int32_t _M0L6_2atmpS2597 = _M0L5valueS942[_M0L1iS943];
      int32_t _M0L6_2atmpS2596 = (int32_t)_M0L6_2atmpS2597;
      uint32_t _M0L6_2atmpS2595 = *(uint32_t*)&_M0L6_2atmpS2596;
      int32_t _M0L6_2atmpS2598;
      moonbit_incref(_M0L4selfS944);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS944, _M0L6_2atmpS2595);
      _M0L6_2atmpS2598 = _M0L1iS943 + 1;
      _M0L1iS943 = _M0L6_2atmpS2598;
      continue;
    } else {
      moonbit_decref(_M0L4selfS944);
      moonbit_decref(_M0L5valueS942);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS939,
  int32_t _M0L3idxS940
) {
  int32_t _M0L6_2atmpS3564;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3564 = _M0L4selfS939[_M0L3idxS940];
  moonbit_decref(_M0L4selfS939);
  return _M0L6_2atmpS3564;
}

void* _M0IPC16option6OptionPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5fuzzy11MatchResultE(
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L4selfS936
) {
  #line 287 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS936 == 0) {
    if (_M0L4selfS936) {
      moonbit_decref(_M0L4selfS936);
    }
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L7_2aSomeS937 =
      _M0L4selfS936;
    struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L8_2avalueS938 =
      _M0L7_2aSomeS937;
    void* _M0L6_2atmpS2594;
    void** _M0L6_2atmpS2593;
    struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2592;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L6_2atmpS2594
    = _M0IP48clawteam8clawteam8internal5fuzzy11MatchResultPB6ToJson8to__json(_M0L8_2avalueS938);
    _M0L6_2atmpS2593 = (void**)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS2593[0] = _M0L6_2atmpS2594;
    _M0L6_2atmpS2592
    = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
    Moonbit_object_header(_M0L6_2atmpS2592)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2592->$0 = _M0L6_2atmpS2593;
    _M0L6_2atmpS2592->$1 = 1;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json5array(_M0L6_2atmpS2592);
  }
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array3mapGRPC16string10StringViewsE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS930,
  struct _M0TWRPC16string10StringViewEs* _M0L1fS934
) {
  int32_t _M0L3lenS2591;
  struct _M0TPB5ArrayGsE* _M0L3arrS929;
  int32_t _M0L7_2abindS931;
  int32_t _M0L1iS932;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2591 = _M0L4selfS930->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS929 = _M0MPC15array5Array12make__uninitGsE(_M0L3lenS2591);
  _M0L7_2abindS931 = _M0L4selfS930->$1;
  _M0L1iS932 = 0;
  while (1) {
    if (_M0L1iS932 < _M0L7_2abindS931) {
      struct _M0TPC16string10StringView* _M0L8_2afieldS3568 =
        _M0L4selfS930->$0;
      struct _M0TPC16string10StringView* _M0L3bufS2590 = _M0L8_2afieldS3568;
      struct _M0TPC16string10StringView _M0L6_2atmpS3567 =
        _M0L3bufS2590[_M0L1iS932];
      struct _M0TPC16string10StringView _M0L1vS933 = _M0L6_2atmpS3567;
      moonbit_string_t* _M0L8_2afieldS3566 = _M0L3arrS929->$0;
      moonbit_string_t* _M0L3bufS2587 = _M0L8_2afieldS3566;
      moonbit_string_t _M0L6_2atmpS2588;
      moonbit_string_t _M0L6_2aoldS3565;
      int32_t _M0L6_2atmpS2589;
      moonbit_incref(_M0L3bufS2587);
      moonbit_incref(_M0L1fS934);
      moonbit_incref(_M0L1vS933.$0);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2588 = _M0L1fS934->code(_M0L1fS934, _M0L1vS933);
      _M0L6_2aoldS3565 = (moonbit_string_t)_M0L3bufS2587[_M0L1iS932];
      moonbit_decref(_M0L6_2aoldS3565);
      _M0L3bufS2587[_M0L1iS932] = _M0L6_2atmpS2588;
      moonbit_decref(_M0L3bufS2587);
      _M0L6_2atmpS2589 = _M0L1iS932 + 1;
      _M0L1iS932 = _M0L6_2atmpS2589;
      continue;
    } else {
      moonbit_decref(_M0L1fS934);
      moonbit_decref(_M0L4selfS930);
    }
    break;
  }
  return _M0L3arrS929;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS928) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS928;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS927) {
  double _M0L6_2atmpS2585;
  moonbit_string_t _M0L6_2atmpS2586;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2585 = (double)_M0L4selfS927;
  _M0L6_2atmpS2586 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2585, _M0L6_2atmpS2586);
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS926) {
  void* _block_3990;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3990 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3990)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3990)->$0 = _M0L6objectS926;
  return _block_3990;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS919
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3569;
  int32_t _M0L6_2acntS3848;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2584;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS918;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__* _closure_3991;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2579;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3569 = _M0L4selfS919->$5;
  _M0L6_2acntS3848 = Moonbit_object_header(_M0L4selfS919)->rc;
  if (_M0L6_2acntS3848 > 1) {
    int32_t _M0L11_2anew__cntS3850 = _M0L6_2acntS3848 - 1;
    Moonbit_object_header(_M0L4selfS919)->rc = _M0L11_2anew__cntS3850;
    if (_M0L8_2afieldS3569) {
      moonbit_incref(_M0L8_2afieldS3569);
    }
  } else if (_M0L6_2acntS3848 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3849 = _M0L4selfS919->$0;
    moonbit_decref(_M0L8_2afieldS3849);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS919);
  }
  _M0L4headS2584 = _M0L8_2afieldS3569;
  _M0L11curr__entryS918
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS918)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS918->$0 = _M0L4headS2584;
  _closure_3991
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__));
  Moonbit_object_header(_closure_3991)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__, $0) >> 2, 1, 0);
  _closure_3991->code = &_M0MPB3Map4iterGsRPB4JsonEC2580l591;
  _closure_3991->$0 = _M0L11curr__entryS918;
  _M0L6_2atmpS2579 = (struct _M0TWEOUsRPB4JsonE*)_closure_3991;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2579);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2580l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2581
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__* _M0L14_2acasted__envS2582;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3575;
  int32_t _M0L6_2acntS3851;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS918;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3574;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS920;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2582
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2580__l591__*)_M0L6_2aenvS2581;
  _M0L8_2afieldS3575 = _M0L14_2acasted__envS2582->$0;
  _M0L6_2acntS3851 = Moonbit_object_header(_M0L14_2acasted__envS2582)->rc;
  if (_M0L6_2acntS3851 > 1) {
    int32_t _M0L11_2anew__cntS3852 = _M0L6_2acntS3851 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2582)->rc
    = _M0L11_2anew__cntS3852;
    moonbit_incref(_M0L8_2afieldS3575);
  } else if (_M0L6_2acntS3851 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2582);
  }
  _M0L11curr__entryS918 = _M0L8_2afieldS3575;
  _M0L8_2afieldS3574 = _M0L11curr__entryS918->$0;
  _M0L7_2abindS920 = _M0L8_2afieldS3574;
  if (_M0L7_2abindS920 == 0) {
    moonbit_decref(_M0L11curr__entryS918);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS921 = _M0L7_2abindS920;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS922 = _M0L7_2aSomeS921;
    moonbit_string_t _M0L8_2afieldS3573 = _M0L4_2axS922->$4;
    moonbit_string_t _M0L6_2akeyS923 = _M0L8_2afieldS3573;
    void* _M0L8_2afieldS3572 = _M0L4_2axS922->$5;
    void* _M0L8_2avalueS924 = _M0L8_2afieldS3572;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3571 = _M0L4_2axS922->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS925 = _M0L8_2afieldS3571;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3570 =
      _M0L11curr__entryS918->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2583;
    if (_M0L7_2anextS925) {
      moonbit_incref(_M0L7_2anextS925);
    }
    moonbit_incref(_M0L8_2avalueS924);
    moonbit_incref(_M0L6_2akeyS923);
    if (_M0L6_2aoldS3570) {
      moonbit_decref(_M0L6_2aoldS3570);
    }
    _M0L11curr__entryS918->$0 = _M0L7_2anextS925;
    moonbit_decref(_M0L11curr__entryS918);
    _M0L8_2atupleS2583
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2583)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2583->$0 = _M0L6_2akeyS923;
    _M0L8_2atupleS2583->$1 = _M0L8_2avalueS924;
    return _M0L8_2atupleS2583;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS917
) {
  int32_t _M0L8_2afieldS3576;
  int32_t _M0L4sizeS2578;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3576 = _M0L4selfS917->$1;
  moonbit_decref(_M0L4selfS917);
  _M0L4sizeS2578 = _M0L8_2afieldS3576;
  return _M0L4sizeS2578 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS904,
  int32_t _M0L3keyS900
) {
  int32_t _M0L4hashS899;
  int32_t _M0L14capacity__maskS2563;
  int32_t _M0L6_2atmpS2562;
  int32_t _M0L1iS901;
  int32_t _M0L3idxS902;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS899 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS900);
  _M0L14capacity__maskS2563 = _M0L4selfS904->$3;
  _M0L6_2atmpS2562 = _M0L4hashS899 & _M0L14capacity__maskS2563;
  _M0L1iS901 = 0;
  _M0L3idxS902 = _M0L6_2atmpS2562;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3580 =
      _M0L4selfS904->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2561 =
      _M0L8_2afieldS3580;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3579;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS903;
    if (
      _M0L3idxS902 < 0
      || _M0L3idxS902 >= Moonbit_array_length(_M0L7entriesS2561)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3579
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2561[
        _M0L3idxS902
      ];
    _M0L7_2abindS903 = _M0L6_2atmpS3579;
    if (_M0L7_2abindS903 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2550;
      if (_M0L7_2abindS903) {
        moonbit_incref(_M0L7_2abindS903);
      }
      moonbit_decref(_M0L4selfS904);
      if (_M0L7_2abindS903) {
        moonbit_decref(_M0L7_2abindS903);
      }
      _M0L6_2atmpS2550 = 0;
      return _M0L6_2atmpS2550;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS905 =
        _M0L7_2abindS903;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS906 =
        _M0L7_2aSomeS905;
      int32_t _M0L4hashS2552 = _M0L8_2aentryS906->$3;
      int32_t _if__result_3993;
      int32_t _M0L8_2afieldS3577;
      int32_t _M0L3pslS2555;
      int32_t _M0L6_2atmpS2557;
      int32_t _M0L6_2atmpS2559;
      int32_t _M0L14capacity__maskS2560;
      int32_t _M0L6_2atmpS2558;
      if (_M0L4hashS2552 == _M0L4hashS899) {
        int32_t _M0L3keyS2551 = _M0L8_2aentryS906->$4;
        _if__result_3993 = _M0L3keyS2551 == _M0L3keyS900;
      } else {
        _if__result_3993 = 0;
      }
      if (_if__result_3993) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3578;
        int32_t _M0L6_2acntS3853;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2554;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2553;
        moonbit_incref(_M0L8_2aentryS906);
        moonbit_decref(_M0L4selfS904);
        _M0L8_2afieldS3578 = _M0L8_2aentryS906->$5;
        _M0L6_2acntS3853 = Moonbit_object_header(_M0L8_2aentryS906)->rc;
        if (_M0L6_2acntS3853 > 1) {
          int32_t _M0L11_2anew__cntS3855 = _M0L6_2acntS3853 - 1;
          Moonbit_object_header(_M0L8_2aentryS906)->rc
          = _M0L11_2anew__cntS3855;
          moonbit_incref(_M0L8_2afieldS3578);
        } else if (_M0L6_2acntS3853 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3854 =
            _M0L8_2aentryS906->$1;
          if (_M0L8_2afieldS3854) {
            moonbit_decref(_M0L8_2afieldS3854);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS906);
        }
        _M0L5valueS2554 = _M0L8_2afieldS3578;
        _M0L6_2atmpS2553 = _M0L5valueS2554;
        return _M0L6_2atmpS2553;
      } else {
        moonbit_incref(_M0L8_2aentryS906);
      }
      _M0L8_2afieldS3577 = _M0L8_2aentryS906->$2;
      moonbit_decref(_M0L8_2aentryS906);
      _M0L3pslS2555 = _M0L8_2afieldS3577;
      if (_M0L1iS901 > _M0L3pslS2555) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2556;
        moonbit_decref(_M0L4selfS904);
        _M0L6_2atmpS2556 = 0;
        return _M0L6_2atmpS2556;
      }
      _M0L6_2atmpS2557 = _M0L1iS901 + 1;
      _M0L6_2atmpS2559 = _M0L3idxS902 + 1;
      _M0L14capacity__maskS2560 = _M0L4selfS904->$3;
      _M0L6_2atmpS2558 = _M0L6_2atmpS2559 & _M0L14capacity__maskS2560;
      _M0L1iS901 = _M0L6_2atmpS2557;
      _M0L3idxS902 = _M0L6_2atmpS2558;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS913,
  moonbit_string_t _M0L3keyS909
) {
  int32_t _M0L4hashS908;
  int32_t _M0L14capacity__maskS2577;
  int32_t _M0L6_2atmpS2576;
  int32_t _M0L1iS910;
  int32_t _M0L3idxS911;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS909);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS908 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS909);
  _M0L14capacity__maskS2577 = _M0L4selfS913->$3;
  _M0L6_2atmpS2576 = _M0L4hashS908 & _M0L14capacity__maskS2577;
  _M0L1iS910 = 0;
  _M0L3idxS911 = _M0L6_2atmpS2576;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3586 =
      _M0L4selfS913->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2575 =
      _M0L8_2afieldS3586;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3585;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS912;
    if (
      _M0L3idxS911 < 0
      || _M0L3idxS911 >= Moonbit_array_length(_M0L7entriesS2575)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3585
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2575[
        _M0L3idxS911
      ];
    _M0L7_2abindS912 = _M0L6_2atmpS3585;
    if (_M0L7_2abindS912 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2564;
      if (_M0L7_2abindS912) {
        moonbit_incref(_M0L7_2abindS912);
      }
      moonbit_decref(_M0L4selfS913);
      if (_M0L7_2abindS912) {
        moonbit_decref(_M0L7_2abindS912);
      }
      moonbit_decref(_M0L3keyS909);
      _M0L6_2atmpS2564 = 0;
      return _M0L6_2atmpS2564;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS914 =
        _M0L7_2abindS912;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS915 =
        _M0L7_2aSomeS914;
      int32_t _M0L4hashS2566 = _M0L8_2aentryS915->$3;
      int32_t _if__result_3995;
      int32_t _M0L8_2afieldS3581;
      int32_t _M0L3pslS2569;
      int32_t _M0L6_2atmpS2571;
      int32_t _M0L6_2atmpS2573;
      int32_t _M0L14capacity__maskS2574;
      int32_t _M0L6_2atmpS2572;
      if (_M0L4hashS2566 == _M0L4hashS908) {
        moonbit_string_t _M0L8_2afieldS3584 = _M0L8_2aentryS915->$4;
        moonbit_string_t _M0L3keyS2565 = _M0L8_2afieldS3584;
        int32_t _M0L6_2atmpS3583;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3583
        = moonbit_val_array_equal(_M0L3keyS2565, _M0L3keyS909);
        _if__result_3995 = _M0L6_2atmpS3583;
      } else {
        _if__result_3995 = 0;
      }
      if (_if__result_3995) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3582;
        int32_t _M0L6_2acntS3856;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2568;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2567;
        moonbit_incref(_M0L8_2aentryS915);
        moonbit_decref(_M0L4selfS913);
        moonbit_decref(_M0L3keyS909);
        _M0L8_2afieldS3582 = _M0L8_2aentryS915->$5;
        _M0L6_2acntS3856 = Moonbit_object_header(_M0L8_2aentryS915)->rc;
        if (_M0L6_2acntS3856 > 1) {
          int32_t _M0L11_2anew__cntS3859 = _M0L6_2acntS3856 - 1;
          Moonbit_object_header(_M0L8_2aentryS915)->rc
          = _M0L11_2anew__cntS3859;
          moonbit_incref(_M0L8_2afieldS3582);
        } else if (_M0L6_2acntS3856 == 1) {
          moonbit_string_t _M0L8_2afieldS3858 = _M0L8_2aentryS915->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3857;
          moonbit_decref(_M0L8_2afieldS3858);
          _M0L8_2afieldS3857 = _M0L8_2aentryS915->$1;
          if (_M0L8_2afieldS3857) {
            moonbit_decref(_M0L8_2afieldS3857);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS915);
        }
        _M0L5valueS2568 = _M0L8_2afieldS3582;
        _M0L6_2atmpS2567 = _M0L5valueS2568;
        return _M0L6_2atmpS2567;
      } else {
        moonbit_incref(_M0L8_2aentryS915);
      }
      _M0L8_2afieldS3581 = _M0L8_2aentryS915->$2;
      moonbit_decref(_M0L8_2aentryS915);
      _M0L3pslS2569 = _M0L8_2afieldS3581;
      if (_M0L1iS910 > _M0L3pslS2569) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2570;
        moonbit_decref(_M0L4selfS913);
        moonbit_decref(_M0L3keyS909);
        _M0L6_2atmpS2570 = 0;
        return _M0L6_2atmpS2570;
      }
      _M0L6_2atmpS2571 = _M0L1iS910 + 1;
      _M0L6_2atmpS2573 = _M0L3idxS911 + 1;
      _M0L14capacity__maskS2574 = _M0L4selfS913->$3;
      _M0L6_2atmpS2572 = _M0L6_2atmpS2573 & _M0L14capacity__maskS2574;
      _M0L1iS910 = _M0L6_2atmpS2571;
      _M0L3idxS911 = _M0L6_2atmpS2572;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS876
) {
  int32_t _M0L6lengthS875;
  int32_t _M0Lm8capacityS877;
  int32_t _M0L6_2atmpS2515;
  int32_t _M0L6_2atmpS2514;
  int32_t _M0L6_2atmpS2525;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS878;
  int32_t _M0L3endS2523;
  int32_t _M0L5startS2524;
  int32_t _M0L7_2abindS879;
  int32_t _M0L2__S880;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS876.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS875
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS876);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS877 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS875);
  _M0L6_2atmpS2515 = _M0Lm8capacityS877;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2514 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2515);
  if (_M0L6lengthS875 > _M0L6_2atmpS2514) {
    int32_t _M0L6_2atmpS2516 = _M0Lm8capacityS877;
    _M0Lm8capacityS877 = _M0L6_2atmpS2516 * 2;
  }
  _M0L6_2atmpS2525 = _M0Lm8capacityS877;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS878
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2525);
  _M0L3endS2523 = _M0L3arrS876.$2;
  _M0L5startS2524 = _M0L3arrS876.$1;
  _M0L7_2abindS879 = _M0L3endS2523 - _M0L5startS2524;
  _M0L2__S880 = 0;
  while (1) {
    if (_M0L2__S880 < _M0L7_2abindS879) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3590 =
        _M0L3arrS876.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2520 =
        _M0L8_2afieldS3590;
      int32_t _M0L5startS2522 = _M0L3arrS876.$1;
      int32_t _M0L6_2atmpS2521 = _M0L5startS2522 + _M0L2__S880;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3589 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2520[
          _M0L6_2atmpS2521
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS881 =
        _M0L6_2atmpS3589;
      moonbit_string_t _M0L8_2afieldS3588 = _M0L1eS881->$0;
      moonbit_string_t _M0L6_2atmpS2517 = _M0L8_2afieldS3588;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3587 =
        _M0L1eS881->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2518 =
        _M0L8_2afieldS3587;
      int32_t _M0L6_2atmpS2519;
      moonbit_incref(_M0L6_2atmpS2518);
      moonbit_incref(_M0L6_2atmpS2517);
      moonbit_incref(_M0L1mS878);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS878, _M0L6_2atmpS2517, _M0L6_2atmpS2518);
      _M0L6_2atmpS2519 = _M0L2__S880 + 1;
      _M0L2__S880 = _M0L6_2atmpS2519;
      continue;
    } else {
      moonbit_decref(_M0L3arrS876.$0);
    }
    break;
  }
  return _M0L1mS878;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS884
) {
  int32_t _M0L6lengthS883;
  int32_t _M0Lm8capacityS885;
  int32_t _M0L6_2atmpS2527;
  int32_t _M0L6_2atmpS2526;
  int32_t _M0L6_2atmpS2537;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS886;
  int32_t _M0L3endS2535;
  int32_t _M0L5startS2536;
  int32_t _M0L7_2abindS887;
  int32_t _M0L2__S888;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS884.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS883
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS884);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS885 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS883);
  _M0L6_2atmpS2527 = _M0Lm8capacityS885;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2526 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2527);
  if (_M0L6lengthS883 > _M0L6_2atmpS2526) {
    int32_t _M0L6_2atmpS2528 = _M0Lm8capacityS885;
    _M0Lm8capacityS885 = _M0L6_2atmpS2528 * 2;
  }
  _M0L6_2atmpS2537 = _M0Lm8capacityS885;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS886
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2537);
  _M0L3endS2535 = _M0L3arrS884.$2;
  _M0L5startS2536 = _M0L3arrS884.$1;
  _M0L7_2abindS887 = _M0L3endS2535 - _M0L5startS2536;
  _M0L2__S888 = 0;
  while (1) {
    if (_M0L2__S888 < _M0L7_2abindS887) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3593 =
        _M0L3arrS884.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2532 =
        _M0L8_2afieldS3593;
      int32_t _M0L5startS2534 = _M0L3arrS884.$1;
      int32_t _M0L6_2atmpS2533 = _M0L5startS2534 + _M0L2__S888;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3592 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2532[
          _M0L6_2atmpS2533
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS889 = _M0L6_2atmpS3592;
      int32_t _M0L6_2atmpS2529 = _M0L1eS889->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3591 =
        _M0L1eS889->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2530 =
        _M0L8_2afieldS3591;
      int32_t _M0L6_2atmpS2531;
      moonbit_incref(_M0L6_2atmpS2530);
      moonbit_incref(_M0L1mS886);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS886, _M0L6_2atmpS2529, _M0L6_2atmpS2530);
      _M0L6_2atmpS2531 = _M0L2__S888 + 1;
      _M0L2__S888 = _M0L6_2atmpS2531;
      continue;
    } else {
      moonbit_decref(_M0L3arrS884.$0);
    }
    break;
  }
  return _M0L1mS886;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS892
) {
  int32_t _M0L6lengthS891;
  int32_t _M0Lm8capacityS893;
  int32_t _M0L6_2atmpS2539;
  int32_t _M0L6_2atmpS2538;
  int32_t _M0L6_2atmpS2549;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS894;
  int32_t _M0L3endS2547;
  int32_t _M0L5startS2548;
  int32_t _M0L7_2abindS895;
  int32_t _M0L2__S896;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS892.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS891 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS892);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS893 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS891);
  _M0L6_2atmpS2539 = _M0Lm8capacityS893;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2538 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2539);
  if (_M0L6lengthS891 > _M0L6_2atmpS2538) {
    int32_t _M0L6_2atmpS2540 = _M0Lm8capacityS893;
    _M0Lm8capacityS893 = _M0L6_2atmpS2540 * 2;
  }
  _M0L6_2atmpS2549 = _M0Lm8capacityS893;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS894 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2549);
  _M0L3endS2547 = _M0L3arrS892.$2;
  _M0L5startS2548 = _M0L3arrS892.$1;
  _M0L7_2abindS895 = _M0L3endS2547 - _M0L5startS2548;
  _M0L2__S896 = 0;
  while (1) {
    if (_M0L2__S896 < _M0L7_2abindS895) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS3597 = _M0L3arrS892.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2544 = _M0L8_2afieldS3597;
      int32_t _M0L5startS2546 = _M0L3arrS892.$1;
      int32_t _M0L6_2atmpS2545 = _M0L5startS2546 + _M0L2__S896;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS3596 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2544[_M0L6_2atmpS2545];
      struct _M0TUsRPB4JsonE* _M0L1eS897 = _M0L6_2atmpS3596;
      moonbit_string_t _M0L8_2afieldS3595 = _M0L1eS897->$0;
      moonbit_string_t _M0L6_2atmpS2541 = _M0L8_2afieldS3595;
      void* _M0L8_2afieldS3594 = _M0L1eS897->$1;
      void* _M0L6_2atmpS2542 = _M0L8_2afieldS3594;
      int32_t _M0L6_2atmpS2543;
      moonbit_incref(_M0L6_2atmpS2542);
      moonbit_incref(_M0L6_2atmpS2541);
      moonbit_incref(_M0L1mS894);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS894, _M0L6_2atmpS2541, _M0L6_2atmpS2542);
      _M0L6_2atmpS2543 = _M0L2__S896 + 1;
      _M0L2__S896 = _M0L6_2atmpS2543;
      continue;
    } else {
      moonbit_decref(_M0L3arrS892.$0);
    }
    break;
  }
  return _M0L1mS894;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS866,
  moonbit_string_t _M0L3keyS867,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS868
) {
  int32_t _M0L6_2atmpS2511;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS867);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2511 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS867);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS866, _M0L3keyS867, _M0L5valueS868, _M0L6_2atmpS2511);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS869,
  int32_t _M0L3keyS870,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS871
) {
  int32_t _M0L6_2atmpS2512;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2512 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS870);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS869, _M0L3keyS870, _M0L5valueS871, _M0L6_2atmpS2512);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS872,
  moonbit_string_t _M0L3keyS873,
  void* _M0L5valueS874
) {
  int32_t _M0L6_2atmpS2513;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS873);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2513 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS873);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS872, _M0L3keyS873, _M0L5valueS874, _M0L6_2atmpS2513);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS834
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3604;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS833;
  int32_t _M0L8capacityS2496;
  int32_t _M0L13new__capacityS835;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2491;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2490;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3603;
  int32_t _M0L6_2atmpS2492;
  int32_t _M0L8capacityS2494;
  int32_t _M0L6_2atmpS2493;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2495;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3602;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS836;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3604 = _M0L4selfS834->$5;
  _M0L9old__headS833 = _M0L8_2afieldS3604;
  _M0L8capacityS2496 = _M0L4selfS834->$2;
  _M0L13new__capacityS835 = _M0L8capacityS2496 << 1;
  _M0L6_2atmpS2491 = 0;
  _M0L6_2atmpS2490
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS835, _M0L6_2atmpS2491);
  _M0L6_2aoldS3603 = _M0L4selfS834->$0;
  if (_M0L9old__headS833) {
    moonbit_incref(_M0L9old__headS833);
  }
  moonbit_decref(_M0L6_2aoldS3603);
  _M0L4selfS834->$0 = _M0L6_2atmpS2490;
  _M0L4selfS834->$2 = _M0L13new__capacityS835;
  _M0L6_2atmpS2492 = _M0L13new__capacityS835 - 1;
  _M0L4selfS834->$3 = _M0L6_2atmpS2492;
  _M0L8capacityS2494 = _M0L4selfS834->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2493 = _M0FPB21calc__grow__threshold(_M0L8capacityS2494);
  _M0L4selfS834->$4 = _M0L6_2atmpS2493;
  _M0L4selfS834->$1 = 0;
  _M0L6_2atmpS2495 = 0;
  _M0L6_2aoldS3602 = _M0L4selfS834->$5;
  if (_M0L6_2aoldS3602) {
    moonbit_decref(_M0L6_2aoldS3602);
  }
  _M0L4selfS834->$5 = _M0L6_2atmpS2495;
  _M0L4selfS834->$6 = -1;
  _M0L8_2aparamS836 = _M0L9old__headS833;
  while (1) {
    if (_M0L8_2aparamS836 == 0) {
      if (_M0L8_2aparamS836) {
        moonbit_decref(_M0L8_2aparamS836);
      }
      moonbit_decref(_M0L4selfS834);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS837 =
        _M0L8_2aparamS836;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS838 =
        _M0L7_2aSomeS837;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3601 =
        _M0L4_2axS838->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS839 =
        _M0L8_2afieldS3601;
      moonbit_string_t _M0L8_2afieldS3600 = _M0L4_2axS838->$4;
      moonbit_string_t _M0L6_2akeyS840 = _M0L8_2afieldS3600;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3599 =
        _M0L4_2axS838->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS841 =
        _M0L8_2afieldS3599;
      int32_t _M0L8_2afieldS3598 = _M0L4_2axS838->$3;
      int32_t _M0L6_2acntS3860 = Moonbit_object_header(_M0L4_2axS838)->rc;
      int32_t _M0L7_2ahashS842;
      if (_M0L6_2acntS3860 > 1) {
        int32_t _M0L11_2anew__cntS3861 = _M0L6_2acntS3860 - 1;
        Moonbit_object_header(_M0L4_2axS838)->rc = _M0L11_2anew__cntS3861;
        moonbit_incref(_M0L8_2avalueS841);
        moonbit_incref(_M0L6_2akeyS840);
        if (_M0L7_2anextS839) {
          moonbit_incref(_M0L7_2anextS839);
        }
      } else if (_M0L6_2acntS3860 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS838);
      }
      _M0L7_2ahashS842 = _M0L8_2afieldS3598;
      moonbit_incref(_M0L4selfS834);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS834, _M0L6_2akeyS840, _M0L8_2avalueS841, _M0L7_2ahashS842);
      _M0L8_2aparamS836 = _M0L7_2anextS839;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS845
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3610;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS844;
  int32_t _M0L8capacityS2503;
  int32_t _M0L13new__capacityS846;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2498;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2497;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3609;
  int32_t _M0L6_2atmpS2499;
  int32_t _M0L8capacityS2501;
  int32_t _M0L6_2atmpS2500;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2502;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3608;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS847;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3610 = _M0L4selfS845->$5;
  _M0L9old__headS844 = _M0L8_2afieldS3610;
  _M0L8capacityS2503 = _M0L4selfS845->$2;
  _M0L13new__capacityS846 = _M0L8capacityS2503 << 1;
  _M0L6_2atmpS2498 = 0;
  _M0L6_2atmpS2497
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS846, _M0L6_2atmpS2498);
  _M0L6_2aoldS3609 = _M0L4selfS845->$0;
  if (_M0L9old__headS844) {
    moonbit_incref(_M0L9old__headS844);
  }
  moonbit_decref(_M0L6_2aoldS3609);
  _M0L4selfS845->$0 = _M0L6_2atmpS2497;
  _M0L4selfS845->$2 = _M0L13new__capacityS846;
  _M0L6_2atmpS2499 = _M0L13new__capacityS846 - 1;
  _M0L4selfS845->$3 = _M0L6_2atmpS2499;
  _M0L8capacityS2501 = _M0L4selfS845->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2500 = _M0FPB21calc__grow__threshold(_M0L8capacityS2501);
  _M0L4selfS845->$4 = _M0L6_2atmpS2500;
  _M0L4selfS845->$1 = 0;
  _M0L6_2atmpS2502 = 0;
  _M0L6_2aoldS3608 = _M0L4selfS845->$5;
  if (_M0L6_2aoldS3608) {
    moonbit_decref(_M0L6_2aoldS3608);
  }
  _M0L4selfS845->$5 = _M0L6_2atmpS2502;
  _M0L4selfS845->$6 = -1;
  _M0L8_2aparamS847 = _M0L9old__headS844;
  while (1) {
    if (_M0L8_2aparamS847 == 0) {
      if (_M0L8_2aparamS847) {
        moonbit_decref(_M0L8_2aparamS847);
      }
      moonbit_decref(_M0L4selfS845);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS848 =
        _M0L8_2aparamS847;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS849 =
        _M0L7_2aSomeS848;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3607 =
        _M0L4_2axS849->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS850 =
        _M0L8_2afieldS3607;
      int32_t _M0L6_2akeyS851 = _M0L4_2axS849->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3606 =
        _M0L4_2axS849->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS852 =
        _M0L8_2afieldS3606;
      int32_t _M0L8_2afieldS3605 = _M0L4_2axS849->$3;
      int32_t _M0L6_2acntS3862 = Moonbit_object_header(_M0L4_2axS849)->rc;
      int32_t _M0L7_2ahashS853;
      if (_M0L6_2acntS3862 > 1) {
        int32_t _M0L11_2anew__cntS3863 = _M0L6_2acntS3862 - 1;
        Moonbit_object_header(_M0L4_2axS849)->rc = _M0L11_2anew__cntS3863;
        moonbit_incref(_M0L8_2avalueS852);
        if (_M0L7_2anextS850) {
          moonbit_incref(_M0L7_2anextS850);
        }
      } else if (_M0L6_2acntS3862 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS849);
      }
      _M0L7_2ahashS853 = _M0L8_2afieldS3605;
      moonbit_incref(_M0L4selfS845);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS845, _M0L6_2akeyS851, _M0L8_2avalueS852, _M0L7_2ahashS853);
      _M0L8_2aparamS847 = _M0L7_2anextS850;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS856
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3617;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS855;
  int32_t _M0L8capacityS2510;
  int32_t _M0L13new__capacityS857;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2505;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2504;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS3616;
  int32_t _M0L6_2atmpS2506;
  int32_t _M0L8capacityS2508;
  int32_t _M0L6_2atmpS2507;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2509;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3615;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS858;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3617 = _M0L4selfS856->$5;
  _M0L9old__headS855 = _M0L8_2afieldS3617;
  _M0L8capacityS2510 = _M0L4selfS856->$2;
  _M0L13new__capacityS857 = _M0L8capacityS2510 << 1;
  _M0L6_2atmpS2505 = 0;
  _M0L6_2atmpS2504
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS857, _M0L6_2atmpS2505);
  _M0L6_2aoldS3616 = _M0L4selfS856->$0;
  if (_M0L9old__headS855) {
    moonbit_incref(_M0L9old__headS855);
  }
  moonbit_decref(_M0L6_2aoldS3616);
  _M0L4selfS856->$0 = _M0L6_2atmpS2504;
  _M0L4selfS856->$2 = _M0L13new__capacityS857;
  _M0L6_2atmpS2506 = _M0L13new__capacityS857 - 1;
  _M0L4selfS856->$3 = _M0L6_2atmpS2506;
  _M0L8capacityS2508 = _M0L4selfS856->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2507 = _M0FPB21calc__grow__threshold(_M0L8capacityS2508);
  _M0L4selfS856->$4 = _M0L6_2atmpS2507;
  _M0L4selfS856->$1 = 0;
  _M0L6_2atmpS2509 = 0;
  _M0L6_2aoldS3615 = _M0L4selfS856->$5;
  if (_M0L6_2aoldS3615) {
    moonbit_decref(_M0L6_2aoldS3615);
  }
  _M0L4selfS856->$5 = _M0L6_2atmpS2509;
  _M0L4selfS856->$6 = -1;
  _M0L8_2aparamS858 = _M0L9old__headS855;
  while (1) {
    if (_M0L8_2aparamS858 == 0) {
      if (_M0L8_2aparamS858) {
        moonbit_decref(_M0L8_2aparamS858);
      }
      moonbit_decref(_M0L4selfS856);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS859 = _M0L8_2aparamS858;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS860 = _M0L7_2aSomeS859;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3614 = _M0L4_2axS860->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS861 = _M0L8_2afieldS3614;
      moonbit_string_t _M0L8_2afieldS3613 = _M0L4_2axS860->$4;
      moonbit_string_t _M0L6_2akeyS862 = _M0L8_2afieldS3613;
      void* _M0L8_2afieldS3612 = _M0L4_2axS860->$5;
      void* _M0L8_2avalueS863 = _M0L8_2afieldS3612;
      int32_t _M0L8_2afieldS3611 = _M0L4_2axS860->$3;
      int32_t _M0L6_2acntS3864 = Moonbit_object_header(_M0L4_2axS860)->rc;
      int32_t _M0L7_2ahashS864;
      if (_M0L6_2acntS3864 > 1) {
        int32_t _M0L11_2anew__cntS3865 = _M0L6_2acntS3864 - 1;
        Moonbit_object_header(_M0L4_2axS860)->rc = _M0L11_2anew__cntS3865;
        moonbit_incref(_M0L8_2avalueS863);
        moonbit_incref(_M0L6_2akeyS862);
        if (_M0L7_2anextS861) {
          moonbit_incref(_M0L7_2anextS861);
        }
      } else if (_M0L6_2acntS3864 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS860);
      }
      _M0L7_2ahashS864 = _M0L8_2afieldS3611;
      moonbit_incref(_M0L4selfS856);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS856, _M0L6_2akeyS862, _M0L8_2avalueS863, _M0L7_2ahashS864);
      _M0L8_2aparamS858 = _M0L7_2anextS861;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS788,
  moonbit_string_t _M0L3keyS794,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS795,
  int32_t _M0L4hashS790
) {
  int32_t _M0L14capacity__maskS2453;
  int32_t _M0L6_2atmpS2452;
  int32_t _M0L3pslS785;
  int32_t _M0L3idxS786;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2453 = _M0L4selfS788->$3;
  _M0L6_2atmpS2452 = _M0L4hashS790 & _M0L14capacity__maskS2453;
  _M0L3pslS785 = 0;
  _M0L3idxS786 = _M0L6_2atmpS2452;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3622 =
      _M0L4selfS788->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2451 =
      _M0L8_2afieldS3622;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3621;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS787;
    if (
      _M0L3idxS786 < 0
      || _M0L3idxS786 >= Moonbit_array_length(_M0L7entriesS2451)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3621
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2451[
        _M0L3idxS786
      ];
    _M0L7_2abindS787 = _M0L6_2atmpS3621;
    if (_M0L7_2abindS787 == 0) {
      int32_t _M0L4sizeS2436 = _M0L4selfS788->$1;
      int32_t _M0L8grow__atS2437 = _M0L4selfS788->$4;
      int32_t _M0L7_2abindS791;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS792;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS793;
      if (_M0L4sizeS2436 >= _M0L8grow__atS2437) {
        int32_t _M0L14capacity__maskS2439;
        int32_t _M0L6_2atmpS2438;
        moonbit_incref(_M0L4selfS788);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS788);
        _M0L14capacity__maskS2439 = _M0L4selfS788->$3;
        _M0L6_2atmpS2438 = _M0L4hashS790 & _M0L14capacity__maskS2439;
        _M0L3pslS785 = 0;
        _M0L3idxS786 = _M0L6_2atmpS2438;
        continue;
      }
      _M0L7_2abindS791 = _M0L4selfS788->$6;
      _M0L7_2abindS792 = 0;
      _M0L5entryS793
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS793)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS793->$0 = _M0L7_2abindS791;
      _M0L5entryS793->$1 = _M0L7_2abindS792;
      _M0L5entryS793->$2 = _M0L3pslS785;
      _M0L5entryS793->$3 = _M0L4hashS790;
      _M0L5entryS793->$4 = _M0L3keyS794;
      _M0L5entryS793->$5 = _M0L5valueS795;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS788, _M0L3idxS786, _M0L5entryS793);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS796 =
        _M0L7_2abindS787;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS797 =
        _M0L7_2aSomeS796;
      int32_t _M0L4hashS2441 = _M0L14_2acurr__entryS797->$3;
      int32_t _if__result_4003;
      int32_t _M0L3pslS2442;
      int32_t _M0L6_2atmpS2447;
      int32_t _M0L6_2atmpS2449;
      int32_t _M0L14capacity__maskS2450;
      int32_t _M0L6_2atmpS2448;
      if (_M0L4hashS2441 == _M0L4hashS790) {
        moonbit_string_t _M0L8_2afieldS3620 = _M0L14_2acurr__entryS797->$4;
        moonbit_string_t _M0L3keyS2440 = _M0L8_2afieldS3620;
        int32_t _M0L6_2atmpS3619;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3619
        = moonbit_val_array_equal(_M0L3keyS2440, _M0L3keyS794);
        _if__result_4003 = _M0L6_2atmpS3619;
      } else {
        _if__result_4003 = 0;
      }
      if (_if__result_4003) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3618;
        moonbit_incref(_M0L14_2acurr__entryS797);
        moonbit_decref(_M0L3keyS794);
        moonbit_decref(_M0L4selfS788);
        _M0L6_2aoldS3618 = _M0L14_2acurr__entryS797->$5;
        moonbit_decref(_M0L6_2aoldS3618);
        _M0L14_2acurr__entryS797->$5 = _M0L5valueS795;
        moonbit_decref(_M0L14_2acurr__entryS797);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS797);
      }
      _M0L3pslS2442 = _M0L14_2acurr__entryS797->$2;
      if (_M0L3pslS785 > _M0L3pslS2442) {
        int32_t _M0L4sizeS2443 = _M0L4selfS788->$1;
        int32_t _M0L8grow__atS2444 = _M0L4selfS788->$4;
        int32_t _M0L7_2abindS798;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS799;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS800;
        if (_M0L4sizeS2443 >= _M0L8grow__atS2444) {
          int32_t _M0L14capacity__maskS2446;
          int32_t _M0L6_2atmpS2445;
          moonbit_decref(_M0L14_2acurr__entryS797);
          moonbit_incref(_M0L4selfS788);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS788);
          _M0L14capacity__maskS2446 = _M0L4selfS788->$3;
          _M0L6_2atmpS2445 = _M0L4hashS790 & _M0L14capacity__maskS2446;
          _M0L3pslS785 = 0;
          _M0L3idxS786 = _M0L6_2atmpS2445;
          continue;
        }
        moonbit_incref(_M0L4selfS788);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS788, _M0L3idxS786, _M0L14_2acurr__entryS797);
        _M0L7_2abindS798 = _M0L4selfS788->$6;
        _M0L7_2abindS799 = 0;
        _M0L5entryS800
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS800)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS800->$0 = _M0L7_2abindS798;
        _M0L5entryS800->$1 = _M0L7_2abindS799;
        _M0L5entryS800->$2 = _M0L3pslS785;
        _M0L5entryS800->$3 = _M0L4hashS790;
        _M0L5entryS800->$4 = _M0L3keyS794;
        _M0L5entryS800->$5 = _M0L5valueS795;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS788, _M0L3idxS786, _M0L5entryS800);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS797);
      }
      _M0L6_2atmpS2447 = _M0L3pslS785 + 1;
      _M0L6_2atmpS2449 = _M0L3idxS786 + 1;
      _M0L14capacity__maskS2450 = _M0L4selfS788->$3;
      _M0L6_2atmpS2448 = _M0L6_2atmpS2449 & _M0L14capacity__maskS2450;
      _M0L3pslS785 = _M0L6_2atmpS2447;
      _M0L3idxS786 = _M0L6_2atmpS2448;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS804,
  int32_t _M0L3keyS810,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS811,
  int32_t _M0L4hashS806
) {
  int32_t _M0L14capacity__maskS2471;
  int32_t _M0L6_2atmpS2470;
  int32_t _M0L3pslS801;
  int32_t _M0L3idxS802;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2471 = _M0L4selfS804->$3;
  _M0L6_2atmpS2470 = _M0L4hashS806 & _M0L14capacity__maskS2471;
  _M0L3pslS801 = 0;
  _M0L3idxS802 = _M0L6_2atmpS2470;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3625 =
      _M0L4selfS804->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2469 =
      _M0L8_2afieldS3625;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3624;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS803;
    if (
      _M0L3idxS802 < 0
      || _M0L3idxS802 >= Moonbit_array_length(_M0L7entriesS2469)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3624
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2469[
        _M0L3idxS802
      ];
    _M0L7_2abindS803 = _M0L6_2atmpS3624;
    if (_M0L7_2abindS803 == 0) {
      int32_t _M0L4sizeS2454 = _M0L4selfS804->$1;
      int32_t _M0L8grow__atS2455 = _M0L4selfS804->$4;
      int32_t _M0L7_2abindS807;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS808;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS809;
      if (_M0L4sizeS2454 >= _M0L8grow__atS2455) {
        int32_t _M0L14capacity__maskS2457;
        int32_t _M0L6_2atmpS2456;
        moonbit_incref(_M0L4selfS804);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS804);
        _M0L14capacity__maskS2457 = _M0L4selfS804->$3;
        _M0L6_2atmpS2456 = _M0L4hashS806 & _M0L14capacity__maskS2457;
        _M0L3pslS801 = 0;
        _M0L3idxS802 = _M0L6_2atmpS2456;
        continue;
      }
      _M0L7_2abindS807 = _M0L4selfS804->$6;
      _M0L7_2abindS808 = 0;
      _M0L5entryS809
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS809)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS809->$0 = _M0L7_2abindS807;
      _M0L5entryS809->$1 = _M0L7_2abindS808;
      _M0L5entryS809->$2 = _M0L3pslS801;
      _M0L5entryS809->$3 = _M0L4hashS806;
      _M0L5entryS809->$4 = _M0L3keyS810;
      _M0L5entryS809->$5 = _M0L5valueS811;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS804, _M0L3idxS802, _M0L5entryS809);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS812 =
        _M0L7_2abindS803;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS813 =
        _M0L7_2aSomeS812;
      int32_t _M0L4hashS2459 = _M0L14_2acurr__entryS813->$3;
      int32_t _if__result_4005;
      int32_t _M0L3pslS2460;
      int32_t _M0L6_2atmpS2465;
      int32_t _M0L6_2atmpS2467;
      int32_t _M0L14capacity__maskS2468;
      int32_t _M0L6_2atmpS2466;
      if (_M0L4hashS2459 == _M0L4hashS806) {
        int32_t _M0L3keyS2458 = _M0L14_2acurr__entryS813->$4;
        _if__result_4005 = _M0L3keyS2458 == _M0L3keyS810;
      } else {
        _if__result_4005 = 0;
      }
      if (_if__result_4005) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3623;
        moonbit_incref(_M0L14_2acurr__entryS813);
        moonbit_decref(_M0L4selfS804);
        _M0L6_2aoldS3623 = _M0L14_2acurr__entryS813->$5;
        moonbit_decref(_M0L6_2aoldS3623);
        _M0L14_2acurr__entryS813->$5 = _M0L5valueS811;
        moonbit_decref(_M0L14_2acurr__entryS813);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS813);
      }
      _M0L3pslS2460 = _M0L14_2acurr__entryS813->$2;
      if (_M0L3pslS801 > _M0L3pslS2460) {
        int32_t _M0L4sizeS2461 = _M0L4selfS804->$1;
        int32_t _M0L8grow__atS2462 = _M0L4selfS804->$4;
        int32_t _M0L7_2abindS814;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS815;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS816;
        if (_M0L4sizeS2461 >= _M0L8grow__atS2462) {
          int32_t _M0L14capacity__maskS2464;
          int32_t _M0L6_2atmpS2463;
          moonbit_decref(_M0L14_2acurr__entryS813);
          moonbit_incref(_M0L4selfS804);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS804);
          _M0L14capacity__maskS2464 = _M0L4selfS804->$3;
          _M0L6_2atmpS2463 = _M0L4hashS806 & _M0L14capacity__maskS2464;
          _M0L3pslS801 = 0;
          _M0L3idxS802 = _M0L6_2atmpS2463;
          continue;
        }
        moonbit_incref(_M0L4selfS804);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS804, _M0L3idxS802, _M0L14_2acurr__entryS813);
        _M0L7_2abindS814 = _M0L4selfS804->$6;
        _M0L7_2abindS815 = 0;
        _M0L5entryS816
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS816)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS816->$0 = _M0L7_2abindS814;
        _M0L5entryS816->$1 = _M0L7_2abindS815;
        _M0L5entryS816->$2 = _M0L3pslS801;
        _M0L5entryS816->$3 = _M0L4hashS806;
        _M0L5entryS816->$4 = _M0L3keyS810;
        _M0L5entryS816->$5 = _M0L5valueS811;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS804, _M0L3idxS802, _M0L5entryS816);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS813);
      }
      _M0L6_2atmpS2465 = _M0L3pslS801 + 1;
      _M0L6_2atmpS2467 = _M0L3idxS802 + 1;
      _M0L14capacity__maskS2468 = _M0L4selfS804->$3;
      _M0L6_2atmpS2466 = _M0L6_2atmpS2467 & _M0L14capacity__maskS2468;
      _M0L3pslS801 = _M0L6_2atmpS2465;
      _M0L3idxS802 = _M0L6_2atmpS2466;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS820,
  moonbit_string_t _M0L3keyS826,
  void* _M0L5valueS827,
  int32_t _M0L4hashS822
) {
  int32_t _M0L14capacity__maskS2489;
  int32_t _M0L6_2atmpS2488;
  int32_t _M0L3pslS817;
  int32_t _M0L3idxS818;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2489 = _M0L4selfS820->$3;
  _M0L6_2atmpS2488 = _M0L4hashS822 & _M0L14capacity__maskS2489;
  _M0L3pslS817 = 0;
  _M0L3idxS818 = _M0L6_2atmpS2488;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3630 = _M0L4selfS820->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2487 = _M0L8_2afieldS3630;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3629;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS819;
    if (
      _M0L3idxS818 < 0
      || _M0L3idxS818 >= Moonbit_array_length(_M0L7entriesS2487)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3629
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2487[_M0L3idxS818];
    _M0L7_2abindS819 = _M0L6_2atmpS3629;
    if (_M0L7_2abindS819 == 0) {
      int32_t _M0L4sizeS2472 = _M0L4selfS820->$1;
      int32_t _M0L8grow__atS2473 = _M0L4selfS820->$4;
      int32_t _M0L7_2abindS823;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS824;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS825;
      if (_M0L4sizeS2472 >= _M0L8grow__atS2473) {
        int32_t _M0L14capacity__maskS2475;
        int32_t _M0L6_2atmpS2474;
        moonbit_incref(_M0L4selfS820);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS820);
        _M0L14capacity__maskS2475 = _M0L4selfS820->$3;
        _M0L6_2atmpS2474 = _M0L4hashS822 & _M0L14capacity__maskS2475;
        _M0L3pslS817 = 0;
        _M0L3idxS818 = _M0L6_2atmpS2474;
        continue;
      }
      _M0L7_2abindS823 = _M0L4selfS820->$6;
      _M0L7_2abindS824 = 0;
      _M0L5entryS825
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS825)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS825->$0 = _M0L7_2abindS823;
      _M0L5entryS825->$1 = _M0L7_2abindS824;
      _M0L5entryS825->$2 = _M0L3pslS817;
      _M0L5entryS825->$3 = _M0L4hashS822;
      _M0L5entryS825->$4 = _M0L3keyS826;
      _M0L5entryS825->$5 = _M0L5valueS827;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS820, _M0L3idxS818, _M0L5entryS825);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS828 = _M0L7_2abindS819;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS829 =
        _M0L7_2aSomeS828;
      int32_t _M0L4hashS2477 = _M0L14_2acurr__entryS829->$3;
      int32_t _if__result_4007;
      int32_t _M0L3pslS2478;
      int32_t _M0L6_2atmpS2483;
      int32_t _M0L6_2atmpS2485;
      int32_t _M0L14capacity__maskS2486;
      int32_t _M0L6_2atmpS2484;
      if (_M0L4hashS2477 == _M0L4hashS822) {
        moonbit_string_t _M0L8_2afieldS3628 = _M0L14_2acurr__entryS829->$4;
        moonbit_string_t _M0L3keyS2476 = _M0L8_2afieldS3628;
        int32_t _M0L6_2atmpS3627;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3627
        = moonbit_val_array_equal(_M0L3keyS2476, _M0L3keyS826);
        _if__result_4007 = _M0L6_2atmpS3627;
      } else {
        _if__result_4007 = 0;
      }
      if (_if__result_4007) {
        void* _M0L6_2aoldS3626;
        moonbit_incref(_M0L14_2acurr__entryS829);
        moonbit_decref(_M0L3keyS826);
        moonbit_decref(_M0L4selfS820);
        _M0L6_2aoldS3626 = _M0L14_2acurr__entryS829->$5;
        moonbit_decref(_M0L6_2aoldS3626);
        _M0L14_2acurr__entryS829->$5 = _M0L5valueS827;
        moonbit_decref(_M0L14_2acurr__entryS829);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS829);
      }
      _M0L3pslS2478 = _M0L14_2acurr__entryS829->$2;
      if (_M0L3pslS817 > _M0L3pslS2478) {
        int32_t _M0L4sizeS2479 = _M0L4selfS820->$1;
        int32_t _M0L8grow__atS2480 = _M0L4selfS820->$4;
        int32_t _M0L7_2abindS830;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS831;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS832;
        if (_M0L4sizeS2479 >= _M0L8grow__atS2480) {
          int32_t _M0L14capacity__maskS2482;
          int32_t _M0L6_2atmpS2481;
          moonbit_decref(_M0L14_2acurr__entryS829);
          moonbit_incref(_M0L4selfS820);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS820);
          _M0L14capacity__maskS2482 = _M0L4selfS820->$3;
          _M0L6_2atmpS2481 = _M0L4hashS822 & _M0L14capacity__maskS2482;
          _M0L3pslS817 = 0;
          _M0L3idxS818 = _M0L6_2atmpS2481;
          continue;
        }
        moonbit_incref(_M0L4selfS820);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS820, _M0L3idxS818, _M0L14_2acurr__entryS829);
        _M0L7_2abindS830 = _M0L4selfS820->$6;
        _M0L7_2abindS831 = 0;
        _M0L5entryS832
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS832)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS832->$0 = _M0L7_2abindS830;
        _M0L5entryS832->$1 = _M0L7_2abindS831;
        _M0L5entryS832->$2 = _M0L3pslS817;
        _M0L5entryS832->$3 = _M0L4hashS822;
        _M0L5entryS832->$4 = _M0L3keyS826;
        _M0L5entryS832->$5 = _M0L5valueS827;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS820, _M0L3idxS818, _M0L5entryS832);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS829);
      }
      _M0L6_2atmpS2483 = _M0L3pslS817 + 1;
      _M0L6_2atmpS2485 = _M0L3idxS818 + 1;
      _M0L14capacity__maskS2486 = _M0L4selfS820->$3;
      _M0L6_2atmpS2484 = _M0L6_2atmpS2485 & _M0L14capacity__maskS2486;
      _M0L3pslS817 = _M0L6_2atmpS2483;
      _M0L3idxS818 = _M0L6_2atmpS2484;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS759,
  int32_t _M0L3idxS764,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS763
) {
  int32_t _M0L3pslS2403;
  int32_t _M0L6_2atmpS2399;
  int32_t _M0L6_2atmpS2401;
  int32_t _M0L14capacity__maskS2402;
  int32_t _M0L6_2atmpS2400;
  int32_t _M0L3pslS755;
  int32_t _M0L3idxS756;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS757;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2403 = _M0L5entryS763->$2;
  _M0L6_2atmpS2399 = _M0L3pslS2403 + 1;
  _M0L6_2atmpS2401 = _M0L3idxS764 + 1;
  _M0L14capacity__maskS2402 = _M0L4selfS759->$3;
  _M0L6_2atmpS2400 = _M0L6_2atmpS2401 & _M0L14capacity__maskS2402;
  _M0L3pslS755 = _M0L6_2atmpS2399;
  _M0L3idxS756 = _M0L6_2atmpS2400;
  _M0L5entryS757 = _M0L5entryS763;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3632 =
      _M0L4selfS759->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2398 =
      _M0L8_2afieldS3632;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3631;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS758;
    if (
      _M0L3idxS756 < 0
      || _M0L3idxS756 >= Moonbit_array_length(_M0L7entriesS2398)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3631
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2398[
        _M0L3idxS756
      ];
    _M0L7_2abindS758 = _M0L6_2atmpS3631;
    if (_M0L7_2abindS758 == 0) {
      _M0L5entryS757->$2 = _M0L3pslS755;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS759, _M0L5entryS757, _M0L3idxS756);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS761 =
        _M0L7_2abindS758;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS762 =
        _M0L7_2aSomeS761;
      int32_t _M0L3pslS2388 = _M0L14_2acurr__entryS762->$2;
      if (_M0L3pslS755 > _M0L3pslS2388) {
        int32_t _M0L3pslS2393;
        int32_t _M0L6_2atmpS2389;
        int32_t _M0L6_2atmpS2391;
        int32_t _M0L14capacity__maskS2392;
        int32_t _M0L6_2atmpS2390;
        _M0L5entryS757->$2 = _M0L3pslS755;
        moonbit_incref(_M0L14_2acurr__entryS762);
        moonbit_incref(_M0L4selfS759);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS759, _M0L5entryS757, _M0L3idxS756);
        _M0L3pslS2393 = _M0L14_2acurr__entryS762->$2;
        _M0L6_2atmpS2389 = _M0L3pslS2393 + 1;
        _M0L6_2atmpS2391 = _M0L3idxS756 + 1;
        _M0L14capacity__maskS2392 = _M0L4selfS759->$3;
        _M0L6_2atmpS2390 = _M0L6_2atmpS2391 & _M0L14capacity__maskS2392;
        _M0L3pslS755 = _M0L6_2atmpS2389;
        _M0L3idxS756 = _M0L6_2atmpS2390;
        _M0L5entryS757 = _M0L14_2acurr__entryS762;
        continue;
      } else {
        int32_t _M0L6_2atmpS2394 = _M0L3pslS755 + 1;
        int32_t _M0L6_2atmpS2396 = _M0L3idxS756 + 1;
        int32_t _M0L14capacity__maskS2397 = _M0L4selfS759->$3;
        int32_t _M0L6_2atmpS2395 =
          _M0L6_2atmpS2396 & _M0L14capacity__maskS2397;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4009 =
          _M0L5entryS757;
        _M0L3pslS755 = _M0L6_2atmpS2394;
        _M0L3idxS756 = _M0L6_2atmpS2395;
        _M0L5entryS757 = _tmp_4009;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS769,
  int32_t _M0L3idxS774,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS773
) {
  int32_t _M0L3pslS2419;
  int32_t _M0L6_2atmpS2415;
  int32_t _M0L6_2atmpS2417;
  int32_t _M0L14capacity__maskS2418;
  int32_t _M0L6_2atmpS2416;
  int32_t _M0L3pslS765;
  int32_t _M0L3idxS766;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS767;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2419 = _M0L5entryS773->$2;
  _M0L6_2atmpS2415 = _M0L3pslS2419 + 1;
  _M0L6_2atmpS2417 = _M0L3idxS774 + 1;
  _M0L14capacity__maskS2418 = _M0L4selfS769->$3;
  _M0L6_2atmpS2416 = _M0L6_2atmpS2417 & _M0L14capacity__maskS2418;
  _M0L3pslS765 = _M0L6_2atmpS2415;
  _M0L3idxS766 = _M0L6_2atmpS2416;
  _M0L5entryS767 = _M0L5entryS773;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3634 =
      _M0L4selfS769->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2414 =
      _M0L8_2afieldS3634;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3633;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS768;
    if (
      _M0L3idxS766 < 0
      || _M0L3idxS766 >= Moonbit_array_length(_M0L7entriesS2414)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3633
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2414[
        _M0L3idxS766
      ];
    _M0L7_2abindS768 = _M0L6_2atmpS3633;
    if (_M0L7_2abindS768 == 0) {
      _M0L5entryS767->$2 = _M0L3pslS765;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS769, _M0L5entryS767, _M0L3idxS766);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS771 =
        _M0L7_2abindS768;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS772 =
        _M0L7_2aSomeS771;
      int32_t _M0L3pslS2404 = _M0L14_2acurr__entryS772->$2;
      if (_M0L3pslS765 > _M0L3pslS2404) {
        int32_t _M0L3pslS2409;
        int32_t _M0L6_2atmpS2405;
        int32_t _M0L6_2atmpS2407;
        int32_t _M0L14capacity__maskS2408;
        int32_t _M0L6_2atmpS2406;
        _M0L5entryS767->$2 = _M0L3pslS765;
        moonbit_incref(_M0L14_2acurr__entryS772);
        moonbit_incref(_M0L4selfS769);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS769, _M0L5entryS767, _M0L3idxS766);
        _M0L3pslS2409 = _M0L14_2acurr__entryS772->$2;
        _M0L6_2atmpS2405 = _M0L3pslS2409 + 1;
        _M0L6_2atmpS2407 = _M0L3idxS766 + 1;
        _M0L14capacity__maskS2408 = _M0L4selfS769->$3;
        _M0L6_2atmpS2406 = _M0L6_2atmpS2407 & _M0L14capacity__maskS2408;
        _M0L3pslS765 = _M0L6_2atmpS2405;
        _M0L3idxS766 = _M0L6_2atmpS2406;
        _M0L5entryS767 = _M0L14_2acurr__entryS772;
        continue;
      } else {
        int32_t _M0L6_2atmpS2410 = _M0L3pslS765 + 1;
        int32_t _M0L6_2atmpS2412 = _M0L3idxS766 + 1;
        int32_t _M0L14capacity__maskS2413 = _M0L4selfS769->$3;
        int32_t _M0L6_2atmpS2411 =
          _M0L6_2atmpS2412 & _M0L14capacity__maskS2413;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4011 =
          _M0L5entryS767;
        _M0L3pslS765 = _M0L6_2atmpS2410;
        _M0L3idxS766 = _M0L6_2atmpS2411;
        _M0L5entryS767 = _tmp_4011;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS779,
  int32_t _M0L3idxS784,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS783
) {
  int32_t _M0L3pslS2435;
  int32_t _M0L6_2atmpS2431;
  int32_t _M0L6_2atmpS2433;
  int32_t _M0L14capacity__maskS2434;
  int32_t _M0L6_2atmpS2432;
  int32_t _M0L3pslS775;
  int32_t _M0L3idxS776;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS777;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2435 = _M0L5entryS783->$2;
  _M0L6_2atmpS2431 = _M0L3pslS2435 + 1;
  _M0L6_2atmpS2433 = _M0L3idxS784 + 1;
  _M0L14capacity__maskS2434 = _M0L4selfS779->$3;
  _M0L6_2atmpS2432 = _M0L6_2atmpS2433 & _M0L14capacity__maskS2434;
  _M0L3pslS775 = _M0L6_2atmpS2431;
  _M0L3idxS776 = _M0L6_2atmpS2432;
  _M0L5entryS777 = _M0L5entryS783;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3636 = _M0L4selfS779->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2430 = _M0L8_2afieldS3636;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3635;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS778;
    if (
      _M0L3idxS776 < 0
      || _M0L3idxS776 >= Moonbit_array_length(_M0L7entriesS2430)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3635
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2430[_M0L3idxS776];
    _M0L7_2abindS778 = _M0L6_2atmpS3635;
    if (_M0L7_2abindS778 == 0) {
      _M0L5entryS777->$2 = _M0L3pslS775;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS779, _M0L5entryS777, _M0L3idxS776);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS781 = _M0L7_2abindS778;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS782 =
        _M0L7_2aSomeS781;
      int32_t _M0L3pslS2420 = _M0L14_2acurr__entryS782->$2;
      if (_M0L3pslS775 > _M0L3pslS2420) {
        int32_t _M0L3pslS2425;
        int32_t _M0L6_2atmpS2421;
        int32_t _M0L6_2atmpS2423;
        int32_t _M0L14capacity__maskS2424;
        int32_t _M0L6_2atmpS2422;
        _M0L5entryS777->$2 = _M0L3pslS775;
        moonbit_incref(_M0L14_2acurr__entryS782);
        moonbit_incref(_M0L4selfS779);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS779, _M0L5entryS777, _M0L3idxS776);
        _M0L3pslS2425 = _M0L14_2acurr__entryS782->$2;
        _M0L6_2atmpS2421 = _M0L3pslS2425 + 1;
        _M0L6_2atmpS2423 = _M0L3idxS776 + 1;
        _M0L14capacity__maskS2424 = _M0L4selfS779->$3;
        _M0L6_2atmpS2422 = _M0L6_2atmpS2423 & _M0L14capacity__maskS2424;
        _M0L3pslS775 = _M0L6_2atmpS2421;
        _M0L3idxS776 = _M0L6_2atmpS2422;
        _M0L5entryS777 = _M0L14_2acurr__entryS782;
        continue;
      } else {
        int32_t _M0L6_2atmpS2426 = _M0L3pslS775 + 1;
        int32_t _M0L6_2atmpS2428 = _M0L3idxS776 + 1;
        int32_t _M0L14capacity__maskS2429 = _M0L4selfS779->$3;
        int32_t _M0L6_2atmpS2427 =
          _M0L6_2atmpS2428 & _M0L14capacity__maskS2429;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_4013 = _M0L5entryS777;
        _M0L3pslS775 = _M0L6_2atmpS2426;
        _M0L3idxS776 = _M0L6_2atmpS2427;
        _M0L5entryS777 = _tmp_4013;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS737,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS739,
  int32_t _M0L8new__idxS738
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3639;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2382;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2383;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3638;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3637;
  int32_t _M0L6_2acntS3866;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS740;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3639 = _M0L4selfS737->$0;
  _M0L7entriesS2382 = _M0L8_2afieldS3639;
  moonbit_incref(_M0L5entryS739);
  _M0L6_2atmpS2383 = _M0L5entryS739;
  if (
    _M0L8new__idxS738 < 0
    || _M0L8new__idxS738 >= Moonbit_array_length(_M0L7entriesS2382)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3638
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2382[
      _M0L8new__idxS738
    ];
  if (_M0L6_2aoldS3638) {
    moonbit_decref(_M0L6_2aoldS3638);
  }
  _M0L7entriesS2382[_M0L8new__idxS738] = _M0L6_2atmpS2383;
  _M0L8_2afieldS3637 = _M0L5entryS739->$1;
  _M0L6_2acntS3866 = Moonbit_object_header(_M0L5entryS739)->rc;
  if (_M0L6_2acntS3866 > 1) {
    int32_t _M0L11_2anew__cntS3869 = _M0L6_2acntS3866 - 1;
    Moonbit_object_header(_M0L5entryS739)->rc = _M0L11_2anew__cntS3869;
    if (_M0L8_2afieldS3637) {
      moonbit_incref(_M0L8_2afieldS3637);
    }
  } else if (_M0L6_2acntS3866 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3868 =
      _M0L5entryS739->$5;
    moonbit_string_t _M0L8_2afieldS3867;
    moonbit_decref(_M0L8_2afieldS3868);
    _M0L8_2afieldS3867 = _M0L5entryS739->$4;
    moonbit_decref(_M0L8_2afieldS3867);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS739);
  }
  _M0L7_2abindS740 = _M0L8_2afieldS3637;
  if (_M0L7_2abindS740 == 0) {
    if (_M0L7_2abindS740) {
      moonbit_decref(_M0L7_2abindS740);
    }
    _M0L4selfS737->$6 = _M0L8new__idxS738;
    moonbit_decref(_M0L4selfS737);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS741;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS742;
    moonbit_decref(_M0L4selfS737);
    _M0L7_2aSomeS741 = _M0L7_2abindS740;
    _M0L7_2anextS742 = _M0L7_2aSomeS741;
    _M0L7_2anextS742->$0 = _M0L8new__idxS738;
    moonbit_decref(_M0L7_2anextS742);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS743,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS745,
  int32_t _M0L8new__idxS744
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3642;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2384;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2385;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3641;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3640;
  int32_t _M0L6_2acntS3870;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS746;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3642 = _M0L4selfS743->$0;
  _M0L7entriesS2384 = _M0L8_2afieldS3642;
  moonbit_incref(_M0L5entryS745);
  _M0L6_2atmpS2385 = _M0L5entryS745;
  if (
    _M0L8new__idxS744 < 0
    || _M0L8new__idxS744 >= Moonbit_array_length(_M0L7entriesS2384)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3641
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2384[
      _M0L8new__idxS744
    ];
  if (_M0L6_2aoldS3641) {
    moonbit_decref(_M0L6_2aoldS3641);
  }
  _M0L7entriesS2384[_M0L8new__idxS744] = _M0L6_2atmpS2385;
  _M0L8_2afieldS3640 = _M0L5entryS745->$1;
  _M0L6_2acntS3870 = Moonbit_object_header(_M0L5entryS745)->rc;
  if (_M0L6_2acntS3870 > 1) {
    int32_t _M0L11_2anew__cntS3872 = _M0L6_2acntS3870 - 1;
    Moonbit_object_header(_M0L5entryS745)->rc = _M0L11_2anew__cntS3872;
    if (_M0L8_2afieldS3640) {
      moonbit_incref(_M0L8_2afieldS3640);
    }
  } else if (_M0L6_2acntS3870 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3871 =
      _M0L5entryS745->$5;
    moonbit_decref(_M0L8_2afieldS3871);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS745);
  }
  _M0L7_2abindS746 = _M0L8_2afieldS3640;
  if (_M0L7_2abindS746 == 0) {
    if (_M0L7_2abindS746) {
      moonbit_decref(_M0L7_2abindS746);
    }
    _M0L4selfS743->$6 = _M0L8new__idxS744;
    moonbit_decref(_M0L4selfS743);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS747;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS748;
    moonbit_decref(_M0L4selfS743);
    _M0L7_2aSomeS747 = _M0L7_2abindS746;
    _M0L7_2anextS748 = _M0L7_2aSomeS747;
    _M0L7_2anextS748->$0 = _M0L8new__idxS744;
    moonbit_decref(_M0L7_2anextS748);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS749,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS751,
  int32_t _M0L8new__idxS750
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3645;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2386;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2387;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3644;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3643;
  int32_t _M0L6_2acntS3873;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS752;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3645 = _M0L4selfS749->$0;
  _M0L7entriesS2386 = _M0L8_2afieldS3645;
  moonbit_incref(_M0L5entryS751);
  _M0L6_2atmpS2387 = _M0L5entryS751;
  if (
    _M0L8new__idxS750 < 0
    || _M0L8new__idxS750 >= Moonbit_array_length(_M0L7entriesS2386)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3644
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2386[_M0L8new__idxS750];
  if (_M0L6_2aoldS3644) {
    moonbit_decref(_M0L6_2aoldS3644);
  }
  _M0L7entriesS2386[_M0L8new__idxS750] = _M0L6_2atmpS2387;
  _M0L8_2afieldS3643 = _M0L5entryS751->$1;
  _M0L6_2acntS3873 = Moonbit_object_header(_M0L5entryS751)->rc;
  if (_M0L6_2acntS3873 > 1) {
    int32_t _M0L11_2anew__cntS3876 = _M0L6_2acntS3873 - 1;
    Moonbit_object_header(_M0L5entryS751)->rc = _M0L11_2anew__cntS3876;
    if (_M0L8_2afieldS3643) {
      moonbit_incref(_M0L8_2afieldS3643);
    }
  } else if (_M0L6_2acntS3873 == 1) {
    void* _M0L8_2afieldS3875 = _M0L5entryS751->$5;
    moonbit_string_t _M0L8_2afieldS3874;
    moonbit_decref(_M0L8_2afieldS3875);
    _M0L8_2afieldS3874 = _M0L5entryS751->$4;
    moonbit_decref(_M0L8_2afieldS3874);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS751);
  }
  _M0L7_2abindS752 = _M0L8_2afieldS3643;
  if (_M0L7_2abindS752 == 0) {
    if (_M0L7_2abindS752) {
      moonbit_decref(_M0L7_2abindS752);
    }
    _M0L4selfS749->$6 = _M0L8new__idxS750;
    moonbit_decref(_M0L4selfS749);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS753;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS754;
    moonbit_decref(_M0L4selfS749);
    _M0L7_2aSomeS753 = _M0L7_2abindS752;
    _M0L7_2anextS754 = _M0L7_2aSomeS753;
    _M0L7_2anextS754->$0 = _M0L8new__idxS750;
    moonbit_decref(_M0L7_2anextS754);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS726,
  int32_t _M0L3idxS728,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS727
) {
  int32_t _M0L7_2abindS725;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3647;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2360;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2361;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3646;
  int32_t _M0L4sizeS2363;
  int32_t _M0L6_2atmpS2362;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS725 = _M0L4selfS726->$6;
  switch (_M0L7_2abindS725) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2355;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3648;
      moonbit_incref(_M0L5entryS727);
      _M0L6_2atmpS2355 = _M0L5entryS727;
      _M0L6_2aoldS3648 = _M0L4selfS726->$5;
      if (_M0L6_2aoldS3648) {
        moonbit_decref(_M0L6_2aoldS3648);
      }
      _M0L4selfS726->$5 = _M0L6_2atmpS2355;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3651 =
        _M0L4selfS726->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2359 =
        _M0L8_2afieldS3651;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3650;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2358;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2356;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2357;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3649;
      if (
        _M0L7_2abindS725 < 0
        || _M0L7_2abindS725 >= Moonbit_array_length(_M0L7entriesS2359)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3650
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2359[
          _M0L7_2abindS725
        ];
      _M0L6_2atmpS2358 = _M0L6_2atmpS3650;
      if (_M0L6_2atmpS2358) {
        moonbit_incref(_M0L6_2atmpS2358);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2356
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2358);
      moonbit_incref(_M0L5entryS727);
      _M0L6_2atmpS2357 = _M0L5entryS727;
      _M0L6_2aoldS3649 = _M0L6_2atmpS2356->$1;
      if (_M0L6_2aoldS3649) {
        moonbit_decref(_M0L6_2aoldS3649);
      }
      _M0L6_2atmpS2356->$1 = _M0L6_2atmpS2357;
      moonbit_decref(_M0L6_2atmpS2356);
      break;
    }
  }
  _M0L4selfS726->$6 = _M0L3idxS728;
  _M0L8_2afieldS3647 = _M0L4selfS726->$0;
  _M0L7entriesS2360 = _M0L8_2afieldS3647;
  _M0L6_2atmpS2361 = _M0L5entryS727;
  if (
    _M0L3idxS728 < 0
    || _M0L3idxS728 >= Moonbit_array_length(_M0L7entriesS2360)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3646
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2360[
      _M0L3idxS728
    ];
  if (_M0L6_2aoldS3646) {
    moonbit_decref(_M0L6_2aoldS3646);
  }
  _M0L7entriesS2360[_M0L3idxS728] = _M0L6_2atmpS2361;
  _M0L4sizeS2363 = _M0L4selfS726->$1;
  _M0L6_2atmpS2362 = _M0L4sizeS2363 + 1;
  _M0L4selfS726->$1 = _M0L6_2atmpS2362;
  moonbit_decref(_M0L4selfS726);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS730,
  int32_t _M0L3idxS732,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS731
) {
  int32_t _M0L7_2abindS729;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3653;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2369;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2370;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3652;
  int32_t _M0L4sizeS2372;
  int32_t _M0L6_2atmpS2371;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS729 = _M0L4selfS730->$6;
  switch (_M0L7_2abindS729) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2364;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3654;
      moonbit_incref(_M0L5entryS731);
      _M0L6_2atmpS2364 = _M0L5entryS731;
      _M0L6_2aoldS3654 = _M0L4selfS730->$5;
      if (_M0L6_2aoldS3654) {
        moonbit_decref(_M0L6_2aoldS3654);
      }
      _M0L4selfS730->$5 = _M0L6_2atmpS2364;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3657 =
        _M0L4selfS730->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2368 =
        _M0L8_2afieldS3657;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3656;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2367;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2365;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2366;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3655;
      if (
        _M0L7_2abindS729 < 0
        || _M0L7_2abindS729 >= Moonbit_array_length(_M0L7entriesS2368)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3656
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2368[
          _M0L7_2abindS729
        ];
      _M0L6_2atmpS2367 = _M0L6_2atmpS3656;
      if (_M0L6_2atmpS2367) {
        moonbit_incref(_M0L6_2atmpS2367);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2365
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2367);
      moonbit_incref(_M0L5entryS731);
      _M0L6_2atmpS2366 = _M0L5entryS731;
      _M0L6_2aoldS3655 = _M0L6_2atmpS2365->$1;
      if (_M0L6_2aoldS3655) {
        moonbit_decref(_M0L6_2aoldS3655);
      }
      _M0L6_2atmpS2365->$1 = _M0L6_2atmpS2366;
      moonbit_decref(_M0L6_2atmpS2365);
      break;
    }
  }
  _M0L4selfS730->$6 = _M0L3idxS732;
  _M0L8_2afieldS3653 = _M0L4selfS730->$0;
  _M0L7entriesS2369 = _M0L8_2afieldS3653;
  _M0L6_2atmpS2370 = _M0L5entryS731;
  if (
    _M0L3idxS732 < 0
    || _M0L3idxS732 >= Moonbit_array_length(_M0L7entriesS2369)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3652
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2369[
      _M0L3idxS732
    ];
  if (_M0L6_2aoldS3652) {
    moonbit_decref(_M0L6_2aoldS3652);
  }
  _M0L7entriesS2369[_M0L3idxS732] = _M0L6_2atmpS2370;
  _M0L4sizeS2372 = _M0L4selfS730->$1;
  _M0L6_2atmpS2371 = _M0L4sizeS2372 + 1;
  _M0L4selfS730->$1 = _M0L6_2atmpS2371;
  moonbit_decref(_M0L4selfS730);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS734,
  int32_t _M0L3idxS736,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS735
) {
  int32_t _M0L7_2abindS733;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3659;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2378;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2379;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3658;
  int32_t _M0L4sizeS2381;
  int32_t _M0L6_2atmpS2380;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS733 = _M0L4selfS734->$6;
  switch (_M0L7_2abindS733) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2373;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3660;
      moonbit_incref(_M0L5entryS735);
      _M0L6_2atmpS2373 = _M0L5entryS735;
      _M0L6_2aoldS3660 = _M0L4selfS734->$5;
      if (_M0L6_2aoldS3660) {
        moonbit_decref(_M0L6_2aoldS3660);
      }
      _M0L4selfS734->$5 = _M0L6_2atmpS2373;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3663 = _M0L4selfS734->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2377 = _M0L8_2afieldS3663;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3662;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2376;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2374;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2375;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3661;
      if (
        _M0L7_2abindS733 < 0
        || _M0L7_2abindS733 >= Moonbit_array_length(_M0L7entriesS2377)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3662
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2377[_M0L7_2abindS733];
      _M0L6_2atmpS2376 = _M0L6_2atmpS3662;
      if (_M0L6_2atmpS2376) {
        moonbit_incref(_M0L6_2atmpS2376);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2374
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2376);
      moonbit_incref(_M0L5entryS735);
      _M0L6_2atmpS2375 = _M0L5entryS735;
      _M0L6_2aoldS3661 = _M0L6_2atmpS2374->$1;
      if (_M0L6_2aoldS3661) {
        moonbit_decref(_M0L6_2aoldS3661);
      }
      _M0L6_2atmpS2374->$1 = _M0L6_2atmpS2375;
      moonbit_decref(_M0L6_2atmpS2374);
      break;
    }
  }
  _M0L4selfS734->$6 = _M0L3idxS736;
  _M0L8_2afieldS3659 = _M0L4selfS734->$0;
  _M0L7entriesS2378 = _M0L8_2afieldS3659;
  _M0L6_2atmpS2379 = _M0L5entryS735;
  if (
    _M0L3idxS736 < 0
    || _M0L3idxS736 >= Moonbit_array_length(_M0L7entriesS2378)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3658
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2378[_M0L3idxS736];
  if (_M0L6_2aoldS3658) {
    moonbit_decref(_M0L6_2aoldS3658);
  }
  _M0L7entriesS2378[_M0L3idxS736] = _M0L6_2atmpS2379;
  _M0L4sizeS2381 = _M0L4selfS734->$1;
  _M0L6_2atmpS2380 = _M0L4sizeS2381 + 1;
  _M0L4selfS734->$1 = _M0L6_2atmpS2380;
  moonbit_decref(_M0L4selfS734);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS708
) {
  int32_t _M0L8capacityS707;
  int32_t _M0L7_2abindS709;
  int32_t _M0L7_2abindS710;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2352;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS711;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS712;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4014;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS707
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS708);
  _M0L7_2abindS709 = _M0L8capacityS707 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS710 = _M0FPB21calc__grow__threshold(_M0L8capacityS707);
  _M0L6_2atmpS2352 = 0;
  _M0L7_2abindS711
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS707, _M0L6_2atmpS2352);
  _M0L7_2abindS712 = 0;
  _block_4014
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4014)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4014->$0 = _M0L7_2abindS711;
  _block_4014->$1 = 0;
  _block_4014->$2 = _M0L8capacityS707;
  _block_4014->$3 = _M0L7_2abindS709;
  _block_4014->$4 = _M0L7_2abindS710;
  _block_4014->$5 = _M0L7_2abindS712;
  _block_4014->$6 = -1;
  return _block_4014;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS714
) {
  int32_t _M0L8capacityS713;
  int32_t _M0L7_2abindS715;
  int32_t _M0L7_2abindS716;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2353;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS717;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS718;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4015;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS713
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS714);
  _M0L7_2abindS715 = _M0L8capacityS713 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS716 = _M0FPB21calc__grow__threshold(_M0L8capacityS713);
  _M0L6_2atmpS2353 = 0;
  _M0L7_2abindS717
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS713, _M0L6_2atmpS2353);
  _M0L7_2abindS718 = 0;
  _block_4015
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4015)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4015->$0 = _M0L7_2abindS717;
  _block_4015->$1 = 0;
  _block_4015->$2 = _M0L8capacityS713;
  _block_4015->$3 = _M0L7_2abindS715;
  _block_4015->$4 = _M0L7_2abindS716;
  _block_4015->$5 = _M0L7_2abindS718;
  _block_4015->$6 = -1;
  return _block_4015;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS720
) {
  int32_t _M0L8capacityS719;
  int32_t _M0L7_2abindS721;
  int32_t _M0L7_2abindS722;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2354;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS723;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS724;
  struct _M0TPB3MapGsRPB4JsonE* _block_4016;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS719
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS720);
  _M0L7_2abindS721 = _M0L8capacityS719 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS722 = _M0FPB21calc__grow__threshold(_M0L8capacityS719);
  _M0L6_2atmpS2354 = 0;
  _M0L7_2abindS723
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS719, _M0L6_2atmpS2354);
  _M0L7_2abindS724 = 0;
  _block_4016
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_4016)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_4016->$0 = _M0L7_2abindS723;
  _block_4016->$1 = 0;
  _block_4016->$2 = _M0L8capacityS719;
  _block_4016->$3 = _M0L7_2abindS721;
  _block_4016->$4 = _M0L7_2abindS722;
  _block_4016->$5 = _M0L7_2abindS724;
  _block_4016->$6 = -1;
  return _block_4016;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS706) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS706 >= 0) {
    int32_t _M0L6_2atmpS2351;
    int32_t _M0L6_2atmpS2350;
    int32_t _M0L6_2atmpS2349;
    int32_t _M0L6_2atmpS2348;
    if (_M0L4selfS706 <= 1) {
      return 1;
    }
    if (_M0L4selfS706 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2351 = _M0L4selfS706 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2350 = moonbit_clz32(_M0L6_2atmpS2351);
    _M0L6_2atmpS2349 = _M0L6_2atmpS2350 - 1;
    _M0L6_2atmpS2348 = 2147483647 >> (_M0L6_2atmpS2349 & 31);
    return _M0L6_2atmpS2348 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS705) {
  int32_t _M0L6_2atmpS2347;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2347 = _M0L8capacityS705 * 13;
  return _M0L6_2atmpS2347 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS699
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS699 == 0) {
    if (_M0L4selfS699) {
      moonbit_decref(_M0L4selfS699);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS700 =
      _M0L4selfS699;
    return _M0L7_2aSomeS700;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS701
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS701 == 0) {
    if (_M0L4selfS701) {
      moonbit_decref(_M0L4selfS701);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS702 =
      _M0L4selfS701;
    return _M0L7_2aSomeS702;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS703
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS703 == 0) {
    if (_M0L4selfS703) {
      moonbit_decref(_M0L4selfS703);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS704 = _M0L4selfS703;
    return _M0L7_2aSomeS704;
  }
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array12make__uninitGsE(
  int32_t _M0L3lenS698
) {
  moonbit_string_t* _M0L6_2atmpS2346;
  struct _M0TPB5ArrayGsE* _block_4017;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2346
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L3lenS698, (moonbit_string_t)moonbit_string_literal_0.data);
  _block_4017
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_4017)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _block_4017->$0 = _M0L6_2atmpS2346;
  _block_4017->$1 = _M0L3lenS698;
  return _block_4017;
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS697
) {
  moonbit_string_t* _M0L6_2atmpS2345;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2345 = _M0L4selfS697;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2345);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS693,
  int32_t _M0L5indexS694
) {
  uint64_t* _M0L6_2atmpS2343;
  uint64_t _M0L6_2atmpS3664;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2343 = _M0L4selfS693;
  if (
    _M0L5indexS694 < 0
    || _M0L5indexS694 >= Moonbit_array_length(_M0L6_2atmpS2343)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3664 = (uint64_t)_M0L6_2atmpS2343[_M0L5indexS694];
  moonbit_decref(_M0L6_2atmpS2343);
  return _M0L6_2atmpS3664;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS695,
  int32_t _M0L5indexS696
) {
  uint32_t* _M0L6_2atmpS2344;
  uint32_t _M0L6_2atmpS3665;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2344 = _M0L4selfS695;
  if (
    _M0L5indexS696 < 0
    || _M0L5indexS696 >= Moonbit_array_length(_M0L6_2atmpS2344)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3665 = (uint32_t)_M0L6_2atmpS2344[_M0L5indexS696];
  moonbit_decref(_M0L6_2atmpS2344);
  return _M0L6_2atmpS3665;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS692
) {
  moonbit_string_t* _M0L6_2atmpS2341;
  int32_t _M0L6_2atmpS3666;
  int32_t _M0L6_2atmpS2342;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2340;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS692);
  _M0L6_2atmpS2341 = _M0L4selfS692;
  _M0L6_2atmpS3666 = Moonbit_array_length(_M0L4selfS692);
  moonbit_decref(_M0L4selfS692);
  _M0L6_2atmpS2342 = _M0L6_2atmpS3666;
  _M0L6_2atmpS2340
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2342, _M0L6_2atmpS2341
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2340);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS690
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS689;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__* _closure_4018;
  struct _M0TWEOs* _M0L6_2atmpS2328;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS689
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS689)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS689->$0 = 0;
  _closure_4018
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__));
  Moonbit_object_header(_closure_4018)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__, $0_0) >> 2, 2, 0);
  _closure_4018->code = &_M0MPC15array9ArrayView4iterGsEC2329l570;
  _closure_4018->$0_0 = _M0L4selfS690.$0;
  _closure_4018->$0_1 = _M0L4selfS690.$1;
  _closure_4018->$0_2 = _M0L4selfS690.$2;
  _closure_4018->$1 = _M0L1iS689;
  _M0L6_2atmpS2328 = (struct _M0TWEOs*)_closure_4018;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2328);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2329l570(
  struct _M0TWEOs* _M0L6_2aenvS2330
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__* _M0L14_2acasted__envS2331;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3671;
  struct _M0TPC13ref3RefGiE* _M0L1iS689;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3670;
  int32_t _M0L6_2acntS3877;
  struct _M0TPB9ArrayViewGsE _M0L4selfS690;
  int32_t _M0L3valS2332;
  int32_t _M0L6_2atmpS2333;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2331
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2329__l570__*)_M0L6_2aenvS2330;
  _M0L8_2afieldS3671 = _M0L14_2acasted__envS2331->$1;
  _M0L1iS689 = _M0L8_2afieldS3671;
  _M0L8_2afieldS3670
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2331->$0_1,
      _M0L14_2acasted__envS2331->$0_2,
      _M0L14_2acasted__envS2331->$0_0
  };
  _M0L6_2acntS3877 = Moonbit_object_header(_M0L14_2acasted__envS2331)->rc;
  if (_M0L6_2acntS3877 > 1) {
    int32_t _M0L11_2anew__cntS3878 = _M0L6_2acntS3877 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2331)->rc
    = _M0L11_2anew__cntS3878;
    moonbit_incref(_M0L1iS689);
    moonbit_incref(_M0L8_2afieldS3670.$0);
  } else if (_M0L6_2acntS3877 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2331);
  }
  _M0L4selfS690 = _M0L8_2afieldS3670;
  _M0L3valS2332 = _M0L1iS689->$0;
  moonbit_incref(_M0L4selfS690.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2333 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS690);
  if (_M0L3valS2332 < _M0L6_2atmpS2333) {
    moonbit_string_t* _M0L8_2afieldS3669 = _M0L4selfS690.$0;
    moonbit_string_t* _M0L3bufS2336 = _M0L8_2afieldS3669;
    int32_t _M0L8_2afieldS3668 = _M0L4selfS690.$1;
    int32_t _M0L5startS2338 = _M0L8_2afieldS3668;
    int32_t _M0L3valS2339 = _M0L1iS689->$0;
    int32_t _M0L6_2atmpS2337 = _M0L5startS2338 + _M0L3valS2339;
    moonbit_string_t _M0L6_2atmpS3667 =
      (moonbit_string_t)_M0L3bufS2336[_M0L6_2atmpS2337];
    moonbit_string_t _M0L4elemS691;
    int32_t _M0L3valS2335;
    int32_t _M0L6_2atmpS2334;
    moonbit_incref(_M0L6_2atmpS3667);
    moonbit_decref(_M0L3bufS2336);
    _M0L4elemS691 = _M0L6_2atmpS3667;
    _M0L3valS2335 = _M0L1iS689->$0;
    _M0L6_2atmpS2334 = _M0L3valS2335 + 1;
    _M0L1iS689->$0 = _M0L6_2atmpS2334;
    moonbit_decref(_M0L1iS689);
    return _M0L4elemS691;
  } else {
    moonbit_decref(_M0L4selfS690.$0);
    moonbit_decref(_M0L1iS689);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS688
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS688;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS687,
  struct _M0TPB6Logger _M0L6loggerS686
) {
  moonbit_string_t _M0L6_2atmpS2327;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2327
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS687, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS686.$0->$method_0(_M0L6loggerS686.$1, _M0L6_2atmpS2327);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS685,
  struct _M0TPB6Logger _M0L6loggerS684
) {
  moonbit_string_t _M0L6_2atmpS2326;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2326 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS685, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS684.$0->$method_0(_M0L6loggerS684.$1, _M0L6_2atmpS2326);
  return 0;
}

struct _M0TPC16string10StringView _M0IPC16string10StringViewPB12ToStringView16to__string__view(
  struct _M0TPC16string10StringView _M0L4selfS683
) {
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_like.mbt"
  return _M0L4selfS683;
}

struct _M0TPB5ArrayGRPC16string10StringViewE* _M0MPB4Iter9to__arrayGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS679
) {
  struct _M0TPC16string10StringView* _M0L6_2atmpS2325;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6resultS677;
  #line 674 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2325
  = (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
  _M0L6resultS677
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6resultS677)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6resultS677->$0 = _M0L6_2atmpS2325;
  _M0L6resultS677->$1 = 0;
  _2awhile_682:;
  while (1) {
    void* _M0L7_2abindS678;
    moonbit_incref(_M0L4selfS679);
    #line 677 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L7_2abindS678
    = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4selfS679);
    switch (Moonbit_object_tag(_M0L7_2abindS678)) {
      case 1: {
        struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS680 =
          (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS678;
        struct _M0TPC16string10StringView _M0L8_2afieldS3672 =
          (struct _M0TPC16string10StringView){_M0L7_2aSomeS680->$0_1,
                                                _M0L7_2aSomeS680->$0_2,
                                                _M0L7_2aSomeS680->$0_0};
        int32_t _M0L6_2acntS3879 =
          Moonbit_object_header(_M0L7_2aSomeS680)->rc;
        struct _M0TPC16string10StringView _M0L4_2axS681;
        if (_M0L6_2acntS3879 > 1) {
          int32_t _M0L11_2anew__cntS3880 = _M0L6_2acntS3879 - 1;
          Moonbit_object_header(_M0L7_2aSomeS680)->rc
          = _M0L11_2anew__cntS3880;
          moonbit_incref(_M0L8_2afieldS3672.$0);
        } else if (_M0L6_2acntS3879 == 1) {
          #line 677 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
          moonbit_free(_M0L7_2aSomeS680);
        }
        _M0L4_2axS681 = _M0L8_2afieldS3672;
        moonbit_incref(_M0L6resultS677);
        #line 678 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
        _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L6resultS677, _M0L4_2axS681);
        goto _2awhile_682;
        break;
      }
      default: {
        moonbit_decref(_M0L4selfS679);
        moonbit_decref(_M0L7_2abindS678);
        break;
      }
    }
    break;
  }
  return _M0L6resultS677;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t _M0L4selfS675,
  struct _M0TPC16string10StringView _M0L3sepS676
) {
  int32_t _M0L6_2atmpS2324;
  struct _M0TPC16string10StringView _M0L6_2atmpS2323;
  #line 1093 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2324 = Moonbit_array_length(_M0L4selfS675);
  _M0L6_2atmpS2323
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2324, _M0L4selfS675
  };
  #line 1094 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView5split(_M0L6_2atmpS2323, _M0L3sepS676);
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView _M0L4selfS666,
  struct _M0TPC16string10StringView _M0L3sepS665
) {
  int32_t _M0L8sep__lenS664;
  void* _M0L4SomeS2322;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* _M0L9remainingS668;
  struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__* _closure_4020;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2312;
  #line 1064 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L3sepS665.$0);
  #line 1068 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8sep__lenS664 = _M0MPC16string10StringView6length(_M0L3sepS665);
  if (_M0L8sep__lenS664 == 0) {
    struct _M0TWEOc* _M0L6_2atmpS2307;
    struct _M0TWcERPC16string10StringView* _M0L6_2atmpS2308;
    moonbit_decref(_M0L3sepS665.$0);
    #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L6_2atmpS2307 = _M0MPC16string10StringView4iter(_M0L4selfS666);
    _M0L6_2atmpS2308
    = (struct _M0TWcERPC16string10StringView*)&_M0MPC16string10StringView5splitC2309l1070$closure.data;
    #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB4Iter3mapGcRPC16string10StringViewE(_M0L6_2atmpS2307, _M0L6_2atmpS2308);
  }
  _M0L4SomeS2322
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS2322)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2322)->$0_0
  = _M0L4selfS666.$0;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2322)->$0_1
  = _M0L4selfS666.$1;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2322)->$0_2
  = _M0L4selfS666.$2;
  _M0L9remainingS668
  = (struct _M0TPC13ref3RefGORPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPC16string10StringViewE));
  Moonbit_object_header(_M0L9remainingS668)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L9remainingS668->$0 = _M0L4SomeS2322;
  _closure_4020
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__*)moonbit_malloc(sizeof(struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__));
  Moonbit_object_header(_closure_4020)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__, $0) >> 2, 2, 0);
  _closure_4020->code = &_M0MPC16string10StringView5splitC2313l1073;
  _closure_4020->$0 = _M0L9remainingS668;
  _closure_4020->$1_0 = _M0L3sepS665.$0;
  _closure_4020->$1_1 = _M0L3sepS665.$1;
  _closure_4020->$1_2 = _M0L3sepS665.$2;
  _closure_4020->$2 = _M0L8sep__lenS664;
  _M0L6_2atmpS2312
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_4020;
  #line 1073 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPB4Iter3newGRPC16string10StringViewE(_M0L6_2atmpS2312);
}

void* _M0MPC16string10StringView5splitC2313l1073(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2314
) {
  struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__* _M0L14_2acasted__envS2315;
  int32_t _M0L8sep__lenS664;
  struct _M0TPC16string10StringView _M0L8_2afieldS3678;
  struct _M0TPC16string10StringView _M0L3sepS665;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* _M0L8_2afieldS3677;
  int32_t _M0L6_2acntS3881;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* _M0L9remainingS668;
  void* _M0L8_2afieldS3676;
  void* _M0L7_2abindS669;
  #line 1073 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L14_2acasted__envS2315
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2313__l1073__*)_M0L6_2aenvS2314;
  _M0L8sep__lenS664 = _M0L14_2acasted__envS2315->$2;
  _M0L8_2afieldS3678
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS2315->$1_1,
      _M0L14_2acasted__envS2315->$1_2,
      _M0L14_2acasted__envS2315->$1_0
  };
  _M0L3sepS665 = _M0L8_2afieldS3678;
  _M0L8_2afieldS3677 = _M0L14_2acasted__envS2315->$0;
  _M0L6_2acntS3881 = Moonbit_object_header(_M0L14_2acasted__envS2315)->rc;
  if (_M0L6_2acntS3881 > 1) {
    int32_t _M0L11_2anew__cntS3882 = _M0L6_2acntS3881 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2315)->rc
    = _M0L11_2anew__cntS3882;
    moonbit_incref(_M0L3sepS665.$0);
    moonbit_incref(_M0L8_2afieldS3677);
  } else if (_M0L6_2acntS3881 == 1) {
    #line 1073 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    moonbit_free(_M0L14_2acasted__envS2315);
  }
  _M0L9remainingS668 = _M0L8_2afieldS3677;
  _M0L8_2afieldS3676 = _M0L9remainingS668->$0;
  _M0L7_2abindS669 = _M0L8_2afieldS3676;
  switch (Moonbit_object_tag(_M0L7_2abindS669)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS670 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS669;
      struct _M0TPC16string10StringView _M0L8_2afieldS3675 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS670->$0_1,
                                              _M0L7_2aSomeS670->$0_2,
                                              _M0L7_2aSomeS670->$0_0};
      struct _M0TPC16string10StringView _M0L7_2aviewS671 = _M0L8_2afieldS3675;
      int64_t _M0L7_2abindS672;
      moonbit_incref(_M0L7_2aviewS671.$0);
      moonbit_incref(_M0L7_2aviewS671.$0);
      #line 1075 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L7_2abindS672
      = _M0MPC16string10StringView4find(_M0L7_2aviewS671, _M0L3sepS665);
      if (_M0L7_2abindS672 == 4294967296ll) {
        void* _M0L4NoneS2316 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        void* _M0L6_2aoldS3673 = _M0L9remainingS668->$0;
        void* _block_4021;
        moonbit_decref(_M0L6_2aoldS3673);
        _M0L9remainingS668->$0 = _M0L4NoneS2316;
        moonbit_decref(_M0L9remainingS668);
        _block_4021
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_4021)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4021)->$0_0
        = _M0L7_2aviewS671.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4021)->$0_1
        = _M0L7_2aviewS671.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4021)->$0_2
        = _M0L7_2aviewS671.$2;
        return _block_4021;
      } else {
        int64_t _M0L7_2aSomeS673 = _M0L7_2abindS672;
        int32_t _M0L6_2aendS674 = (int32_t)_M0L7_2aSomeS673;
        int32_t _M0L6_2atmpS2319 = _M0L6_2aendS674 + _M0L8sep__lenS664;
        struct _M0TPC16string10StringView _M0L6_2atmpS2318;
        void* _M0L4SomeS2317;
        void* _M0L6_2aoldS3674;
        int64_t _M0L6_2atmpS2321;
        struct _M0TPC16string10StringView _M0L6_2atmpS2320;
        void* _block_4022;
        moonbit_incref(_M0L7_2aviewS671.$0);
        #line 1079 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0L6_2atmpS2318
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS671, _M0L6_2atmpS2319, 4294967296ll);
        _M0L4SomeS2317
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_M0L4SomeS2317)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2317)->$0_0
        = _M0L6_2atmpS2318.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2317)->$0_1
        = _M0L6_2atmpS2318.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2317)->$0_2
        = _M0L6_2atmpS2318.$2;
        _M0L6_2aoldS3674 = _M0L9remainingS668->$0;
        moonbit_decref(_M0L6_2aoldS3674);
        _M0L9remainingS668->$0 = _M0L4SomeS2317;
        moonbit_decref(_M0L9remainingS668);
        _M0L6_2atmpS2321 = (int64_t)_M0L6_2aendS674;
        #line 1080 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0L6_2atmpS2320
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS671, 0, _M0L6_2atmpS2321);
        _block_4022
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_4022)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4022)->$0_0
        = _M0L6_2atmpS2320.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4022)->$0_1
        = _M0L6_2atmpS2320.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4022)->$0_2
        = _M0L6_2atmpS2320.$2;
        return _block_4022;
      }
      break;
    }
    default: {
      moonbit_decref(_M0L9remainingS668);
      moonbit_decref(_M0L3sepS665.$0);
      return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      break;
    }
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2309l1070(
  struct _M0TWcERPC16string10StringView* _M0L6_2aenvS2310,
  int32_t _M0L1cS667
) {
  moonbit_string_t _M0L6_2atmpS2311;
  #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_decref(_M0L6_2aenvS2310);
  #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2311 = _M0IPC14char4CharPB4Show10to__string(_M0L1cS667);
  #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string6String12view_2einner(_M0L6_2atmpS2311, 0, 4294967296ll);
}

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t _M0L4selfS663) {
  #line 435 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  return _M0FPB16char__to__string(_M0L4selfS663);
}

moonbit_string_t _M0FPB16char__to__string(int32_t _M0L4charS662) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS661;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2306;
  #line 441 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L7_2aselfS661 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L7_2aselfS661);
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS661, _M0L4charS662);
  _M0L6_2atmpS2306 = _M0L7_2aselfS661;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2306);
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TWEOc* _M0L4selfS657,
  struct _M0TWcERPC16string10StringView* _M0L1fS660
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__* _closure_4023;
  #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _closure_4023
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__*)moonbit_malloc(sizeof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__));
  Moonbit_object_header(_closure_4023)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__, $0) >> 2, 2, 0);
  _closure_4023->code = &_M0MPB4Iter3mapGcRPC16string10StringViewEC2302l317;
  _closure_4023->$0 = _M0L1fS660;
  _closure_4023->$1 = _M0L4selfS657;
  return (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_4023;
}

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2302l317(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2303
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__* _M0L14_2acasted__envS2304;
  struct _M0TWEOc* _M0L8_2afieldS3680;
  struct _M0TWEOc* _M0L4selfS657;
  struct _M0TWcERPC16string10StringView* _M0L8_2afieldS3679;
  int32_t _M0L6_2acntS3883;
  struct _M0TWcERPC16string10StringView* _M0L1fS660;
  int32_t _M0L7_2abindS656;
  #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2304
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2302__l317__*)_M0L6_2aenvS2303;
  _M0L8_2afieldS3680 = _M0L14_2acasted__envS2304->$1;
  _M0L4selfS657 = _M0L8_2afieldS3680;
  _M0L8_2afieldS3679 = _M0L14_2acasted__envS2304->$0;
  _M0L6_2acntS3883 = Moonbit_object_header(_M0L14_2acasted__envS2304)->rc;
  if (_M0L6_2acntS3883 > 1) {
    int32_t _M0L11_2anew__cntS3884 = _M0L6_2acntS3883 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2304)->rc
    = _M0L11_2anew__cntS3884;
    moonbit_incref(_M0L4selfS657);
    moonbit_incref(_M0L8_2afieldS3679);
  } else if (_M0L6_2acntS3883 == 1) {
    #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2304);
  }
  _M0L1fS660 = _M0L8_2afieldS3679;
  #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2abindS656 = _M0MPB4Iter4nextGcE(_M0L4selfS657);
  if (_M0L7_2abindS656 == -1) {
    moonbit_decref(_M0L1fS660);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    int32_t _M0L7_2aSomeS658 = _M0L7_2abindS656;
    int32_t _M0L4_2axS659 = _M0L7_2aSomeS658;
    struct _M0TPC16string10StringView _M0L6_2atmpS2305;
    void* _block_4024;
    #line 319 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L6_2atmpS2305 = _M0L1fS660->code(_M0L1fS660, _M0L4_2axS659);
    _block_4024
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_block_4024)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4024)->$0_0
    = _M0L6_2atmpS2305.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4024)->$0_1
    = _M0L6_2atmpS2305.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_4024)->$0_2
    = _M0L6_2atmpS2305.$2;
    return _block_4024;
  }
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS651) {
  int32_t _M0L3lenS650;
  struct _M0TPC13ref3RefGiE* _M0L5indexS652;
  struct _M0R38String_3a_3aiter_2eanon__u2286__l247__* _closure_4025;
  struct _M0TWEOc* _M0L6_2atmpS2285;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS650 = Moonbit_array_length(_M0L4selfS651);
  _M0L5indexS652
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS652)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS652->$0 = 0;
  _closure_4025
  = (struct _M0R38String_3a_3aiter_2eanon__u2286__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2286__l247__));
  Moonbit_object_header(_closure_4025)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2286__l247__, $0) >> 2, 2, 0);
  _closure_4025->code = &_M0MPC16string6String4iterC2286l247;
  _closure_4025->$0 = _M0L5indexS652;
  _closure_4025->$1 = _M0L4selfS651;
  _closure_4025->$2 = _M0L3lenS650;
  _M0L6_2atmpS2285 = (struct _M0TWEOc*)_closure_4025;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2285);
}

int32_t _M0MPC16string6String4iterC2286l247(
  struct _M0TWEOc* _M0L6_2aenvS2287
) {
  struct _M0R38String_3a_3aiter_2eanon__u2286__l247__* _M0L14_2acasted__envS2288;
  int32_t _M0L3lenS650;
  moonbit_string_t _M0L8_2afieldS3683;
  moonbit_string_t _M0L4selfS651;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3682;
  int32_t _M0L6_2acntS3885;
  struct _M0TPC13ref3RefGiE* _M0L5indexS652;
  int32_t _M0L3valS2289;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2288
  = (struct _M0R38String_3a_3aiter_2eanon__u2286__l247__*)_M0L6_2aenvS2287;
  _M0L3lenS650 = _M0L14_2acasted__envS2288->$2;
  _M0L8_2afieldS3683 = _M0L14_2acasted__envS2288->$1;
  _M0L4selfS651 = _M0L8_2afieldS3683;
  _M0L8_2afieldS3682 = _M0L14_2acasted__envS2288->$0;
  _M0L6_2acntS3885 = Moonbit_object_header(_M0L14_2acasted__envS2288)->rc;
  if (_M0L6_2acntS3885 > 1) {
    int32_t _M0L11_2anew__cntS3886 = _M0L6_2acntS3885 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2288)->rc
    = _M0L11_2anew__cntS3886;
    moonbit_incref(_M0L4selfS651);
    moonbit_incref(_M0L8_2afieldS3682);
  } else if (_M0L6_2acntS3885 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2288);
  }
  _M0L5indexS652 = _M0L8_2afieldS3682;
  _M0L3valS2289 = _M0L5indexS652->$0;
  if (_M0L3valS2289 < _M0L3lenS650) {
    int32_t _M0L3valS2301 = _M0L5indexS652->$0;
    int32_t _M0L2c1S653 = _M0L4selfS651[_M0L3valS2301];
    int32_t _if__result_4026;
    int32_t _M0L3valS2299;
    int32_t _M0L6_2atmpS2298;
    int32_t _M0L6_2atmpS2300;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S653)) {
      int32_t _M0L3valS2291 = _M0L5indexS652->$0;
      int32_t _M0L6_2atmpS2290 = _M0L3valS2291 + 1;
      _if__result_4026 = _M0L6_2atmpS2290 < _M0L3lenS650;
    } else {
      _if__result_4026 = 0;
    }
    if (_if__result_4026) {
      int32_t _M0L3valS2297 = _M0L5indexS652->$0;
      int32_t _M0L6_2atmpS2296 = _M0L3valS2297 + 1;
      int32_t _M0L6_2atmpS3681 = _M0L4selfS651[_M0L6_2atmpS2296];
      int32_t _M0L2c2S654;
      moonbit_decref(_M0L4selfS651);
      _M0L2c2S654 = _M0L6_2atmpS3681;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S654)) {
        int32_t _M0L6_2atmpS2294 = (int32_t)_M0L2c1S653;
        int32_t _M0L6_2atmpS2295 = (int32_t)_M0L2c2S654;
        int32_t _M0L1cS655;
        int32_t _M0L3valS2293;
        int32_t _M0L6_2atmpS2292;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS655
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2294, _M0L6_2atmpS2295);
        _M0L3valS2293 = _M0L5indexS652->$0;
        _M0L6_2atmpS2292 = _M0L3valS2293 + 2;
        _M0L5indexS652->$0 = _M0L6_2atmpS2292;
        moonbit_decref(_M0L5indexS652);
        return _M0L1cS655;
      }
    } else {
      moonbit_decref(_M0L4selfS651);
    }
    _M0L3valS2299 = _M0L5indexS652->$0;
    _M0L6_2atmpS2298 = _M0L3valS2299 + 1;
    _M0L5indexS652->$0 = _M0L6_2atmpS2298;
    moonbit_decref(_M0L5indexS652);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2300 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S653);
    return _M0L6_2atmpS2300;
  } else {
    moonbit_decref(_M0L5indexS652);
    moonbit_decref(_M0L4selfS651);
    return -1;
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12trim_2einner(
  struct _M0TPC16string10StringView _M0L4selfS648,
  struct _M0TPC16string10StringView _M0L5charsS649
) {
  struct _M0TPC16string10StringView _M0L6_2atmpS2284;
  #line 731 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L5charsS649.$0);
  #line 736 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2284
  = _M0MPC16string10StringView19trim__start_2einner(_M0L4selfS648, _M0L5charsS649);
  #line 736 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView17trim__end_2einner(_M0L6_2atmpS2284, _M0L5charsS649);
}

struct _M0TPC16string10StringView _M0MPC16string10StringView17trim__end_2einner(
  struct _M0TPC16string10StringView _M0L4selfS647,
  struct _M0TPC16string10StringView _M0L5charsS645
) {
  struct _M0TPC16string10StringView _M0L8_2aparamS642;
  #line 689 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8_2aparamS642 = _M0L4selfS647;
  while (1) {
    moonbit_string_t _M0L8_2afieldS3688 = _M0L8_2aparamS642.$0;
    moonbit_string_t _M0L3strS2265 = _M0L8_2afieldS3688;
    int32_t _M0L5startS2266 = _M0L8_2aparamS642.$1;
    int32_t _M0L3endS2268 = _M0L8_2aparamS642.$2;
    int64_t _M0L6_2atmpS2267 = (int64_t)_M0L3endS2268;
    moonbit_incref(_M0L3strS2265);
    #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2265, 0, _M0L5startS2266, _M0L6_2atmpS2267)
    ) {
      moonbit_decref(_M0L5charsS645.$0);
      return _M0L8_2aparamS642;
    } else {
      moonbit_string_t _M0L8_2afieldS3687 = _M0L8_2aparamS642.$0;
      moonbit_string_t _M0L3strS2277 = _M0L8_2afieldS3687;
      moonbit_string_t _M0L8_2afieldS3686 = _M0L8_2aparamS642.$0;
      moonbit_string_t _M0L3strS2280 = _M0L8_2afieldS3686;
      int32_t _M0L5startS2281 = _M0L8_2aparamS642.$1;
      int32_t _M0L3endS2283 = _M0L8_2aparamS642.$2;
      int64_t _M0L6_2atmpS2282 = (int64_t)_M0L3endS2283;
      int64_t _M0L6_2atmpS2279;
      int32_t _M0L6_2atmpS2278;
      int32_t _M0L4_2acS643;
      moonbit_string_t _M0L8_2afieldS3685;
      moonbit_string_t _M0L3strS2269;
      int32_t _M0L5startS2270;
      moonbit_string_t _M0L8_2afieldS3684;
      moonbit_string_t _M0L3strS2273;
      int32_t _M0L5startS2274;
      int32_t _M0L3endS2276;
      int64_t _M0L6_2atmpS2275;
      int64_t _M0L6_2atmpS2272;
      int32_t _M0L6_2atmpS2271;
      struct _M0TPC16string10StringView _M0L4_2axS644;
      moonbit_incref(_M0L3strS2280);
      moonbit_incref(_M0L3strS2277);
      #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2279
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2280, -1, _M0L5startS2281, _M0L6_2atmpS2282);
      _M0L6_2atmpS2278 = (int32_t)_M0L6_2atmpS2279;
      #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L4_2acS643
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2277, _M0L6_2atmpS2278);
      _M0L8_2afieldS3685 = _M0L8_2aparamS642.$0;
      _M0L3strS2269 = _M0L8_2afieldS3685;
      _M0L5startS2270 = _M0L8_2aparamS642.$1;
      _M0L8_2afieldS3684 = _M0L8_2aparamS642.$0;
      _M0L3strS2273 = _M0L8_2afieldS3684;
      _M0L5startS2274 = _M0L8_2aparamS642.$1;
      _M0L3endS2276 = _M0L8_2aparamS642.$2;
      _M0L6_2atmpS2275 = (int64_t)_M0L3endS2276;
      moonbit_incref(_M0L3strS2273);
      moonbit_incref(_M0L3strS2269);
      #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2272
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2273, -1, _M0L5startS2274, _M0L6_2atmpS2275);
      _M0L6_2atmpS2271 = (int32_t)_M0L6_2atmpS2272;
      _M0L4_2axS644
      = (struct _M0TPC16string10StringView){
        _M0L5startS2270, _M0L6_2atmpS2271, _M0L3strS2269
      };
      moonbit_incref(_M0L5charsS645.$0);
      #line 696 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      if (
        _M0MPC16string10StringView14contains__char(_M0L5charsS645, _M0L4_2acS643)
      ) {
        moonbit_decref(_M0L8_2aparamS642.$0);
        _M0L8_2aparamS642 = _M0L4_2axS644;
        continue;
      } else {
        moonbit_decref(_M0L5charsS645.$0);
        moonbit_decref(_M0L4_2axS644.$0);
        return _M0L8_2aparamS642;
      }
    }
    break;
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView19trim__start_2einner(
  struct _M0TPC16string10StringView _M0L4selfS641,
  struct _M0TPC16string10StringView _M0L5charsS639
) {
  struct _M0TPC16string10StringView _M0L8_2aparamS635;
  #line 648 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8_2aparamS635 = _M0L4selfS641;
  while (1) {
    moonbit_string_t _M0L8_2afieldS3693 = _M0L8_2aparamS635.$0;
    moonbit_string_t _M0L3strS2247 = _M0L8_2afieldS3693;
    int32_t _M0L5startS2248 = _M0L8_2aparamS635.$1;
    int32_t _M0L3endS2250 = _M0L8_2aparamS635.$2;
    int64_t _M0L6_2atmpS2249 = (int64_t)_M0L3endS2250;
    moonbit_incref(_M0L3strS2247);
    #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2247, 0, _M0L5startS2248, _M0L6_2atmpS2249)
    ) {
      moonbit_decref(_M0L5charsS639.$0);
      return _M0L8_2aparamS635;
    } else {
      moonbit_string_t _M0L8_2afieldS3692 = _M0L8_2aparamS635.$0;
      moonbit_string_t _M0L3strS2258 = _M0L8_2afieldS3692;
      moonbit_string_t _M0L8_2afieldS3691 = _M0L8_2aparamS635.$0;
      moonbit_string_t _M0L3strS2261 = _M0L8_2afieldS3691;
      int32_t _M0L5startS2262 = _M0L8_2aparamS635.$1;
      int32_t _M0L3endS2264 = _M0L8_2aparamS635.$2;
      int64_t _M0L6_2atmpS2263 = (int64_t)_M0L3endS2264;
      int64_t _M0L6_2atmpS2260;
      int32_t _M0L6_2atmpS2259;
      int32_t _M0L4_2acS636;
      moonbit_string_t _M0L8_2afieldS3690;
      moonbit_string_t _M0L3strS2251;
      moonbit_string_t _M0L8_2afieldS3689;
      moonbit_string_t _M0L3strS2254;
      int32_t _M0L5startS2255;
      int32_t _M0L3endS2257;
      int64_t _M0L6_2atmpS2256;
      int64_t _M0L7_2abindS1515;
      int32_t _M0L6_2atmpS2252;
      int32_t _M0L3endS2253;
      struct _M0TPC16string10StringView _M0L4_2axS637;
      moonbit_incref(_M0L3strS2261);
      moonbit_incref(_M0L3strS2258);
      #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2260
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2261, 0, _M0L5startS2262, _M0L6_2atmpS2263);
      _M0L6_2atmpS2259 = (int32_t)_M0L6_2atmpS2260;
      #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L4_2acS636
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2258, _M0L6_2atmpS2259);
      _M0L8_2afieldS3690 = _M0L8_2aparamS635.$0;
      _M0L3strS2251 = _M0L8_2afieldS3690;
      _M0L8_2afieldS3689 = _M0L8_2aparamS635.$0;
      _M0L3strS2254 = _M0L8_2afieldS3689;
      _M0L5startS2255 = _M0L8_2aparamS635.$1;
      _M0L3endS2257 = _M0L8_2aparamS635.$2;
      _M0L6_2atmpS2256 = (int64_t)_M0L3endS2257;
      moonbit_incref(_M0L3strS2254);
      moonbit_incref(_M0L3strS2251);
      #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L7_2abindS1515
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2254, 1, _M0L5startS2255, _M0L6_2atmpS2256);
      if (_M0L7_2abindS1515 == 4294967296ll) {
        _M0L6_2atmpS2252 = _M0L8_2aparamS635.$2;
      } else {
        int64_t _M0L7_2aSomeS638 = _M0L7_2abindS1515;
        _M0L6_2atmpS2252 = (int32_t)_M0L7_2aSomeS638;
      }
      _M0L3endS2253 = _M0L8_2aparamS635.$2;
      _M0L4_2axS637
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS2252, _M0L3endS2253, _M0L3strS2251
      };
      moonbit_incref(_M0L5charsS639.$0);
      #line 655 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      if (
        _M0MPC16string10StringView14contains__char(_M0L5charsS639, _M0L4_2acS636)
      ) {
        moonbit_decref(_M0L8_2aparamS635.$0);
        _M0L8_2aparamS635 = _M0L4_2axS637;
        continue;
      } else {
        moonbit_decref(_M0L5charsS639.$0);
        moonbit_decref(_M0L4_2axS637.$0);
        return _M0L8_2aparamS635;
      }
    }
    break;
  }
}

int32_t _M0MPC16string10StringView14contains__char(
  struct _M0TPC16string10StringView _M0L4selfS625,
  int32_t _M0L1cS627
) {
  int32_t _M0L3lenS624;
  #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L4selfS625.$0);
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L3lenS624 = _M0MPC16string10StringView6length(_M0L4selfS625);
  if (_M0L3lenS624 > 0) {
    int32_t _M0L1cS626 = _M0L1cS627;
    if (_M0L1cS626 <= 65535) {
      int32_t _M0L1iS628 = 0;
      while (1) {
        if (_M0L1iS628 < _M0L3lenS624) {
          int32_t _M0L6_2atmpS2233;
          int32_t _M0L6_2atmpS2232;
          int32_t _M0L6_2atmpS2234;
          moonbit_incref(_M0L4selfS625.$0);
          #line 598 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2233
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS625, _M0L1iS628);
          _M0L6_2atmpS2232 = (int32_t)_M0L6_2atmpS2233;
          if (_M0L6_2atmpS2232 == _M0L1cS626) {
            moonbit_decref(_M0L4selfS625.$0);
            return 1;
          }
          _M0L6_2atmpS2234 = _M0L1iS628 + 1;
          _M0L1iS628 = _M0L6_2atmpS2234;
          continue;
        } else {
          moonbit_decref(_M0L4selfS625.$0);
        }
        break;
      }
    } else if (_M0L3lenS624 >= 2) {
      int32_t _M0L3adjS630 = _M0L1cS626 - 65536;
      int32_t _M0L6_2atmpS2246 = _M0L3adjS630 >> 10;
      int32_t _M0L4highS631 = 55296 + _M0L6_2atmpS2246;
      int32_t _M0L6_2atmpS2245 = _M0L3adjS630 & 1023;
      int32_t _M0L3lowS632 = 56320 + _M0L6_2atmpS2245;
      int32_t _M0Lm1iS633 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2235 = _M0Lm1iS633;
        int32_t _M0L6_2atmpS2236 = _M0L3lenS624 - 1;
        if (_M0L6_2atmpS2235 < _M0L6_2atmpS2236) {
          int32_t _M0L6_2atmpS2239 = _M0Lm1iS633;
          int32_t _M0L6_2atmpS2238;
          int32_t _M0L6_2atmpS2237;
          int32_t _M0L6_2atmpS2244;
          moonbit_incref(_M0L4selfS625.$0);
          #line 612 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2238
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS625, _M0L6_2atmpS2239);
          _M0L6_2atmpS2237 = (int32_t)_M0L6_2atmpS2238;
          if (_M0L6_2atmpS2237 == _M0L4highS631) {
            int32_t _M0L6_2atmpS2240 = _M0Lm1iS633;
            int32_t _M0L6_2atmpS2243;
            int32_t _M0L6_2atmpS2242;
            int32_t _M0L6_2atmpS2241;
            _M0Lm1iS633 = _M0L6_2atmpS2240 + 1;
            _M0L6_2atmpS2243 = _M0Lm1iS633;
            moonbit_incref(_M0L4selfS625.$0);
            #line 614 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            _M0L6_2atmpS2242
            = _M0MPC16string10StringView11unsafe__get(_M0L4selfS625, _M0L6_2atmpS2243);
            _M0L6_2atmpS2241 = (int32_t)_M0L6_2atmpS2242;
            if (_M0L6_2atmpS2241 == _M0L3lowS632) {
              moonbit_decref(_M0L4selfS625.$0);
              return 1;
            }
          }
          _M0L6_2atmpS2244 = _M0Lm1iS633;
          _M0Lm1iS633 = _M0L6_2atmpS2244 + 1;
          continue;
        } else {
          moonbit_decref(_M0L4selfS625.$0);
        }
        break;
      }
    } else {
      moonbit_decref(_M0L4selfS625.$0);
      return 0;
    }
    return 0;
  } else {
    moonbit_decref(_M0L4selfS625.$0);
    return 0;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS612,
  moonbit_string_t _M0L5valueS614
) {
  int32_t _M0L3lenS2212;
  moonbit_string_t* _M0L6_2atmpS2214;
  int32_t _M0L6_2atmpS3696;
  int32_t _M0L6_2atmpS2213;
  int32_t _M0L6lengthS613;
  moonbit_string_t* _M0L8_2afieldS3695;
  moonbit_string_t* _M0L3bufS2215;
  moonbit_string_t _M0L6_2aoldS3694;
  int32_t _M0L6_2atmpS2216;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2212 = _M0L4selfS612->$1;
  moonbit_incref(_M0L4selfS612);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2214 = _M0MPC15array5Array6bufferGsE(_M0L4selfS612);
  _M0L6_2atmpS3696 = Moonbit_array_length(_M0L6_2atmpS2214);
  moonbit_decref(_M0L6_2atmpS2214);
  _M0L6_2atmpS2213 = _M0L6_2atmpS3696;
  if (_M0L3lenS2212 == _M0L6_2atmpS2213) {
    moonbit_incref(_M0L4selfS612);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS612);
  }
  _M0L6lengthS613 = _M0L4selfS612->$1;
  _M0L8_2afieldS3695 = _M0L4selfS612->$0;
  _M0L3bufS2215 = _M0L8_2afieldS3695;
  _M0L6_2aoldS3694 = (moonbit_string_t)_M0L3bufS2215[_M0L6lengthS613];
  moonbit_decref(_M0L6_2aoldS3694);
  _M0L3bufS2215[_M0L6lengthS613] = _M0L5valueS614;
  _M0L6_2atmpS2216 = _M0L6lengthS613 + 1;
  _M0L4selfS612->$1 = _M0L6_2atmpS2216;
  moonbit_decref(_M0L4selfS612);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS615,
  struct _M0TUsiE* _M0L5valueS617
) {
  int32_t _M0L3lenS2217;
  struct _M0TUsiE** _M0L6_2atmpS2219;
  int32_t _M0L6_2atmpS3699;
  int32_t _M0L6_2atmpS2218;
  int32_t _M0L6lengthS616;
  struct _M0TUsiE** _M0L8_2afieldS3698;
  struct _M0TUsiE** _M0L3bufS2220;
  struct _M0TUsiE* _M0L6_2aoldS3697;
  int32_t _M0L6_2atmpS2221;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2217 = _M0L4selfS615->$1;
  moonbit_incref(_M0L4selfS615);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2219 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS615);
  _M0L6_2atmpS3699 = Moonbit_array_length(_M0L6_2atmpS2219);
  moonbit_decref(_M0L6_2atmpS2219);
  _M0L6_2atmpS2218 = _M0L6_2atmpS3699;
  if (_M0L3lenS2217 == _M0L6_2atmpS2218) {
    moonbit_incref(_M0L4selfS615);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS615);
  }
  _M0L6lengthS616 = _M0L4selfS615->$1;
  _M0L8_2afieldS3698 = _M0L4selfS615->$0;
  _M0L3bufS2220 = _M0L8_2afieldS3698;
  _M0L6_2aoldS3697 = (struct _M0TUsiE*)_M0L3bufS2220[_M0L6lengthS616];
  if (_M0L6_2aoldS3697) {
    moonbit_decref(_M0L6_2aoldS3697);
  }
  _M0L3bufS2220[_M0L6lengthS616] = _M0L5valueS617;
  _M0L6_2atmpS2221 = _M0L6lengthS616 + 1;
  _M0L4selfS615->$1 = _M0L6_2atmpS2221;
  moonbit_decref(_M0L4selfS615);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS618,
  void* _M0L5valueS620
) {
  int32_t _M0L3lenS2222;
  void** _M0L6_2atmpS2224;
  int32_t _M0L6_2atmpS3702;
  int32_t _M0L6_2atmpS2223;
  int32_t _M0L6lengthS619;
  void** _M0L8_2afieldS3701;
  void** _M0L3bufS2225;
  void* _M0L6_2aoldS3700;
  int32_t _M0L6_2atmpS2226;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2222 = _M0L4selfS618->$1;
  moonbit_incref(_M0L4selfS618);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2224
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS618);
  _M0L6_2atmpS3702 = Moonbit_array_length(_M0L6_2atmpS2224);
  moonbit_decref(_M0L6_2atmpS2224);
  _M0L6_2atmpS2223 = _M0L6_2atmpS3702;
  if (_M0L3lenS2222 == _M0L6_2atmpS2223) {
    moonbit_incref(_M0L4selfS618);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS618);
  }
  _M0L6lengthS619 = _M0L4selfS618->$1;
  _M0L8_2afieldS3701 = _M0L4selfS618->$0;
  _M0L3bufS2225 = _M0L8_2afieldS3701;
  _M0L6_2aoldS3700 = (void*)_M0L3bufS2225[_M0L6lengthS619];
  moonbit_decref(_M0L6_2aoldS3700);
  _M0L3bufS2225[_M0L6lengthS619] = _M0L5valueS620;
  _M0L6_2atmpS2226 = _M0L6lengthS619 + 1;
  _M0L4selfS618->$1 = _M0L6_2atmpS2226;
  moonbit_decref(_M0L4selfS618);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS621,
  struct _M0TPC16string10StringView _M0L5valueS623
) {
  int32_t _M0L3lenS2227;
  struct _M0TPC16string10StringView* _M0L6_2atmpS2229;
  int32_t _M0L6_2atmpS3705;
  int32_t _M0L6_2atmpS2228;
  int32_t _M0L6lengthS622;
  struct _M0TPC16string10StringView* _M0L8_2afieldS3704;
  struct _M0TPC16string10StringView* _M0L3bufS2230;
  struct _M0TPC16string10StringView _M0L6_2aoldS3703;
  int32_t _M0L6_2atmpS2231;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2227 = _M0L4selfS621->$1;
  moonbit_incref(_M0L4selfS621);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2229
  = _M0MPC15array5Array6bufferGRPC16string10StringViewE(_M0L4selfS621);
  _M0L6_2atmpS3705 = Moonbit_array_length(_M0L6_2atmpS2229);
  moonbit_decref(_M0L6_2atmpS2229);
  _M0L6_2atmpS2228 = _M0L6_2atmpS3705;
  if (_M0L3lenS2227 == _M0L6_2atmpS2228) {
    moonbit_incref(_M0L4selfS621);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC16string10StringViewE(_M0L4selfS621);
  }
  _M0L6lengthS622 = _M0L4selfS621->$1;
  _M0L8_2afieldS3704 = _M0L4selfS621->$0;
  _M0L3bufS2230 = _M0L8_2afieldS3704;
  _M0L6_2aoldS3703 = _M0L3bufS2230[_M0L6lengthS622];
  moonbit_decref(_M0L6_2aoldS3703.$0);
  _M0L3bufS2230[_M0L6lengthS622] = _M0L5valueS623;
  _M0L6_2atmpS2231 = _M0L6lengthS622 + 1;
  _M0L4selfS621->$1 = _M0L6_2atmpS2231;
  moonbit_decref(_M0L4selfS621);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS601) {
  int32_t _M0L8old__capS600;
  int32_t _M0L8new__capS602;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS600 = _M0L4selfS601->$1;
  if (_M0L8old__capS600 == 0) {
    _M0L8new__capS602 = 8;
  } else {
    _M0L8new__capS602 = _M0L8old__capS600 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS601, _M0L8new__capS602);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS604
) {
  int32_t _M0L8old__capS603;
  int32_t _M0L8new__capS605;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS603 = _M0L4selfS604->$1;
  if (_M0L8old__capS603 == 0) {
    _M0L8new__capS605 = 8;
  } else {
    _M0L8new__capS605 = _M0L8old__capS603 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS604, _M0L8new__capS605);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS607
) {
  int32_t _M0L8old__capS606;
  int32_t _M0L8new__capS608;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS606 = _M0L4selfS607->$1;
  if (_M0L8old__capS606 == 0) {
    _M0L8new__capS608 = 8;
  } else {
    _M0L8new__capS608 = _M0L8old__capS606 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS607, _M0L8new__capS608);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS610
) {
  int32_t _M0L8old__capS609;
  int32_t _M0L8new__capS611;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS609 = _M0L4selfS610->$1;
  if (_M0L8old__capS609 == 0) {
    _M0L8new__capS611 = 8;
  } else {
    _M0L8new__capS611 = _M0L8old__capS609 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(_M0L4selfS610, _M0L8new__capS611);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS579,
  int32_t _M0L13new__capacityS577
) {
  moonbit_string_t* _M0L8new__bufS576;
  moonbit_string_t* _M0L8_2afieldS3707;
  moonbit_string_t* _M0L8old__bufS578;
  int32_t _M0L8old__capS580;
  int32_t _M0L9copy__lenS581;
  moonbit_string_t* _M0L6_2aoldS3706;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS576
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS577, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3707 = _M0L4selfS579->$0;
  _M0L8old__bufS578 = _M0L8_2afieldS3707;
  _M0L8old__capS580 = Moonbit_array_length(_M0L8old__bufS578);
  if (_M0L8old__capS580 < _M0L13new__capacityS577) {
    _M0L9copy__lenS581 = _M0L8old__capS580;
  } else {
    _M0L9copy__lenS581 = _M0L13new__capacityS577;
  }
  moonbit_incref(_M0L8old__bufS578);
  moonbit_incref(_M0L8new__bufS576);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS576, 0, _M0L8old__bufS578, 0, _M0L9copy__lenS581);
  _M0L6_2aoldS3706 = _M0L4selfS579->$0;
  moonbit_decref(_M0L6_2aoldS3706);
  _M0L4selfS579->$0 = _M0L8new__bufS576;
  moonbit_decref(_M0L4selfS579);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS585,
  int32_t _M0L13new__capacityS583
) {
  struct _M0TUsiE** _M0L8new__bufS582;
  struct _M0TUsiE** _M0L8_2afieldS3709;
  struct _M0TUsiE** _M0L8old__bufS584;
  int32_t _M0L8old__capS586;
  int32_t _M0L9copy__lenS587;
  struct _M0TUsiE** _M0L6_2aoldS3708;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS582
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS583, 0);
  _M0L8_2afieldS3709 = _M0L4selfS585->$0;
  _M0L8old__bufS584 = _M0L8_2afieldS3709;
  _M0L8old__capS586 = Moonbit_array_length(_M0L8old__bufS584);
  if (_M0L8old__capS586 < _M0L13new__capacityS583) {
    _M0L9copy__lenS587 = _M0L8old__capS586;
  } else {
    _M0L9copy__lenS587 = _M0L13new__capacityS583;
  }
  moonbit_incref(_M0L8old__bufS584);
  moonbit_incref(_M0L8new__bufS582);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS582, 0, _M0L8old__bufS584, 0, _M0L9copy__lenS587);
  _M0L6_2aoldS3708 = _M0L4selfS585->$0;
  moonbit_decref(_M0L6_2aoldS3708);
  _M0L4selfS585->$0 = _M0L8new__bufS582;
  moonbit_decref(_M0L4selfS585);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS591,
  int32_t _M0L13new__capacityS589
) {
  void** _M0L8new__bufS588;
  void** _M0L8_2afieldS3711;
  void** _M0L8old__bufS590;
  int32_t _M0L8old__capS592;
  int32_t _M0L9copy__lenS593;
  void** _M0L6_2aoldS3710;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS588
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS589, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3711 = _M0L4selfS591->$0;
  _M0L8old__bufS590 = _M0L8_2afieldS3711;
  _M0L8old__capS592 = Moonbit_array_length(_M0L8old__bufS590);
  if (_M0L8old__capS592 < _M0L13new__capacityS589) {
    _M0L9copy__lenS593 = _M0L8old__capS592;
  } else {
    _M0L9copy__lenS593 = _M0L13new__capacityS589;
  }
  moonbit_incref(_M0L8old__bufS590);
  moonbit_incref(_M0L8new__bufS588);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS588, 0, _M0L8old__bufS590, 0, _M0L9copy__lenS593);
  _M0L6_2aoldS3710 = _M0L4selfS591->$0;
  moonbit_decref(_M0L6_2aoldS3710);
  _M0L4selfS591->$0 = _M0L8new__bufS588;
  moonbit_decref(_M0L4selfS591);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS597,
  int32_t _M0L13new__capacityS595
) {
  struct _M0TPC16string10StringView* _M0L8new__bufS594;
  struct _M0TPC16string10StringView* _M0L8_2afieldS3713;
  struct _M0TPC16string10StringView* _M0L8old__bufS596;
  int32_t _M0L8old__capS598;
  int32_t _M0L9copy__lenS599;
  struct _M0TPC16string10StringView* _M0L6_2aoldS3712;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS594
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array(_M0L13new__capacityS595, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0), &(struct _M0TPC16string10StringView){0, 0, (moonbit_string_t)moonbit_string_literal_0.data});
  _M0L8_2afieldS3713 = _M0L4selfS597->$0;
  _M0L8old__bufS596 = _M0L8_2afieldS3713;
  _M0L8old__capS598 = Moonbit_array_length(_M0L8old__bufS596);
  if (_M0L8old__capS598 < _M0L13new__capacityS595) {
    _M0L9copy__lenS599 = _M0L8old__capS598;
  } else {
    _M0L9copy__lenS599 = _M0L13new__capacityS595;
  }
  moonbit_incref(_M0L8old__bufS596);
  moonbit_incref(_M0L8new__bufS594);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(_M0L8new__bufS594, 0, _M0L8old__bufS596, 0, _M0L9copy__lenS599);
  _M0L6_2aoldS3712 = _M0L4selfS597->$0;
  moonbit_decref(_M0L6_2aoldS3712);
  _M0L4selfS597->$0 = _M0L8new__bufS594;
  moonbit_decref(_M0L4selfS597);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS575
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS575 == 0) {
    moonbit_string_t* _M0L6_2atmpS2210 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4031 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4031)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4031->$0 = _M0L6_2atmpS2210;
    _block_4031->$1 = 0;
    return _block_4031;
  } else {
    moonbit_string_t* _M0L6_2atmpS2211 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS575, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4032 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4032)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4032->$0 = _M0L6_2atmpS2211;
    _block_4032->$1 = 0;
    return _block_4032;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS569,
  int32_t _M0L1nS568
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS568 <= 0) {
    moonbit_decref(_M0L4selfS569);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS568 == 1) {
    return _M0L4selfS569;
  } else {
    int32_t _M0L3lenS570 = Moonbit_array_length(_M0L4selfS569);
    int32_t _M0L6_2atmpS2209 = _M0L3lenS570 * _M0L1nS568;
    struct _M0TPB13StringBuilder* _M0L3bufS571;
    moonbit_string_t _M0L3strS572;
    int32_t _M0L2__S573;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS571 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2209);
    _M0L3strS572 = _M0L4selfS569;
    _M0L2__S573 = 0;
    while (1) {
      if (_M0L2__S573 < _M0L1nS568) {
        int32_t _M0L6_2atmpS2208;
        moonbit_incref(_M0L3strS572);
        moonbit_incref(_M0L3bufS571);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS571, _M0L3strS572);
        _M0L6_2atmpS2208 = _M0L2__S573 + 1;
        _M0L2__S573 = _M0L6_2atmpS2208;
        continue;
      } else {
        moonbit_decref(_M0L3strS572);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS571);
  }
}

int64_t _M0MPC16string6String4find(
  moonbit_string_t _M0L4selfS566,
  struct _M0TPC16string10StringView _M0L3strS567
) {
  int32_t _M0L6_2atmpS2207;
  struct _M0TPC16string10StringView _M0L6_2atmpS2206;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2207 = Moonbit_array_length(_M0L4selfS566);
  _M0L6_2atmpS2206
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2207, _M0L4selfS566
  };
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView4find(_M0L6_2atmpS2206, _M0L3strS567);
}

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView _M0L4selfS565,
  struct _M0TPC16string10StringView _M0L3strS564
) {
  int32_t _M0L6_2atmpS2205;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L3strS564.$0);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2205 = _M0MPC16string10StringView6length(_M0L3strS564);
  if (_M0L6_2atmpS2205 <= 4) {
    #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0FPB18brute__force__find(_M0L4selfS565, _M0L3strS564);
  } else {
    #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0FPB28boyer__moore__horspool__find(_M0L4selfS565, _M0L3strS564);
  }
}

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView _M0L8haystackS554,
  struct _M0TPC16string10StringView _M0L6needleS556
) {
  int32_t _M0L13haystack__lenS553;
  int32_t _M0L11needle__lenS555;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L8haystackS554.$0);
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L13haystack__lenS553
  = _M0MPC16string10StringView6length(_M0L8haystackS554);
  moonbit_incref(_M0L6needleS556.$0);
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L11needle__lenS555 = _M0MPC16string10StringView6length(_M0L6needleS556);
  if (_M0L11needle__lenS555 > 0) {
    if (_M0L13haystack__lenS553 >= _M0L11needle__lenS555) {
      int32_t _M0L13needle__firstS557;
      int32_t _M0L12forward__lenS558;
      int32_t _M0Lm1iS559;
      moonbit_incref(_M0L6needleS556.$0);
      #line 36 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L13needle__firstS557
      = _M0MPC16string10StringView11unsafe__get(_M0L6needleS556, 0);
      _M0L12forward__lenS558
      = _M0L13haystack__lenS553 - _M0L11needle__lenS555;
      _M0Lm1iS559 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2192 = _M0Lm1iS559;
        if (_M0L6_2atmpS2192 <= _M0L12forward__lenS558) {
          int32_t _M0L6_2atmpS2197;
          while (1) {
            int32_t _M0L6_2atmpS2195 = _M0Lm1iS559;
            int32_t _if__result_4036;
            if (_M0L6_2atmpS2195 <= _M0L12forward__lenS558) {
              int32_t _M0L6_2atmpS2194 = _M0Lm1iS559;
              int32_t _M0L6_2atmpS2193;
              moonbit_incref(_M0L8haystackS554.$0);
              #line 41 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS2193
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS554, _M0L6_2atmpS2194);
              _if__result_4036 = _M0L6_2atmpS2193 != _M0L13needle__firstS557;
            } else {
              _if__result_4036 = 0;
            }
            if (_if__result_4036) {
              int32_t _M0L6_2atmpS2196 = _M0Lm1iS559;
              _M0Lm1iS559 = _M0L6_2atmpS2196 + 1;
              continue;
            }
            break;
          }
          _M0L6_2atmpS2197 = _M0Lm1iS559;
          if (_M0L6_2atmpS2197 <= _M0L12forward__lenS558) {
            int32_t _M0L1jS561 = 1;
            int32_t _M0L6_2atmpS2204;
            while (1) {
              if (_M0L1jS561 < _M0L11needle__lenS555) {
                int32_t _M0L6_2atmpS2201 = _M0Lm1iS559;
                int32_t _M0L6_2atmpS2200 = _M0L6_2atmpS2201 + _M0L1jS561;
                int32_t _M0L6_2atmpS2198;
                int32_t _M0L6_2atmpS2199;
                int32_t _M0L6_2atmpS2202;
                moonbit_incref(_M0L8haystackS554.$0);
                #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
                _M0L6_2atmpS2198
                = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS554, _M0L6_2atmpS2200);
                moonbit_incref(_M0L6needleS556.$0);
                #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
                _M0L6_2atmpS2199
                = _M0MPC16string10StringView11unsafe__get(_M0L6needleS556, _M0L1jS561);
                if (_M0L6_2atmpS2198 != _M0L6_2atmpS2199) {
                  break;
                }
                _M0L6_2atmpS2202 = _M0L1jS561 + 1;
                _M0L1jS561 = _M0L6_2atmpS2202;
                continue;
              } else {
                int32_t _M0L6_2atmpS2203;
                moonbit_decref(_M0L6needleS556.$0);
                moonbit_decref(_M0L8haystackS554.$0);
                _M0L6_2atmpS2203 = _M0Lm1iS559;
                return (int64_t)_M0L6_2atmpS2203;
              }
              break;
            }
            _M0L6_2atmpS2204 = _M0Lm1iS559;
            _M0Lm1iS559 = _M0L6_2atmpS2204 + 1;
          }
          continue;
        } else {
          moonbit_decref(_M0L6needleS556.$0);
          moonbit_decref(_M0L8haystackS554.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS556.$0);
      moonbit_decref(_M0L8haystackS554.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS556.$0);
    moonbit_decref(_M0L8haystackS554.$0);
    return _M0FPB33brute__force__find_2econstr_2f552;
  }
}

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView _M0L8haystackS540,
  struct _M0TPC16string10StringView _M0L6needleS542
) {
  int32_t _M0L13haystack__lenS539;
  int32_t _M0L11needle__lenS541;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L8haystackS540.$0);
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L13haystack__lenS539
  = _M0MPC16string10StringView6length(_M0L8haystackS540);
  moonbit_incref(_M0L6needleS542.$0);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L11needle__lenS541 = _M0MPC16string10StringView6length(_M0L6needleS542);
  if (_M0L11needle__lenS541 > 0) {
    if (_M0L13haystack__lenS539 >= _M0L11needle__lenS541) {
      int32_t* _M0L11skip__tableS543 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS541);
      int32_t _M0L7_2abindS544 = _M0L11needle__lenS541 - 1;
      int32_t _M0L1iS545 = 0;
      int32_t _M0L1iS547;
      while (1) {
        if (_M0L1iS545 < _M0L7_2abindS544) {
          int32_t _M0L6_2atmpS2178;
          int32_t _M0L6_2atmpS2177;
          int32_t _M0L6_2atmpS2174;
          int32_t _M0L6_2atmpS2176;
          int32_t _M0L6_2atmpS2175;
          int32_t _M0L6_2atmpS2179;
          moonbit_incref(_M0L6needleS542.$0);
          #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2178
          = _M0MPC16string10StringView11unsafe__get(_M0L6needleS542, _M0L1iS545);
          _M0L6_2atmpS2177 = (int32_t)_M0L6_2atmpS2178;
          _M0L6_2atmpS2174 = _M0L6_2atmpS2177 & 255;
          _M0L6_2atmpS2176 = _M0L11needle__lenS541 - 1;
          _M0L6_2atmpS2175 = _M0L6_2atmpS2176 - _M0L1iS545;
          if (
            _M0L6_2atmpS2174 < 0
            || _M0L6_2atmpS2174
               >= Moonbit_array_length(_M0L11skip__tableS543)
          ) {
            #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS543[_M0L6_2atmpS2174] = _M0L6_2atmpS2175;
          _M0L6_2atmpS2179 = _M0L1iS545 + 1;
          _M0L1iS545 = _M0L6_2atmpS2179;
          continue;
        }
        break;
      }
      _M0L1iS547 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2180 =
          _M0L13haystack__lenS539 - _M0L11needle__lenS541;
        if (_M0L1iS547 <= _M0L6_2atmpS2180) {
          int32_t _M0L7_2abindS548 = _M0L11needle__lenS541 - 1;
          int32_t _M0L1jS549 = 0;
          int32_t _M0L6_2atmpS2191;
          int32_t _M0L6_2atmpS2190;
          int32_t _M0L6_2atmpS2189;
          int32_t _M0L6_2atmpS2188;
          int32_t _M0L6_2atmpS2187;
          int32_t _M0L6_2atmpS2186;
          int32_t _M0L6_2atmpS2185;
          while (1) {
            if (_M0L1jS549 <= _M0L7_2abindS548) {
              int32_t _M0L6_2atmpS2183 = _M0L1iS547 + _M0L1jS549;
              int32_t _M0L6_2atmpS2181;
              int32_t _M0L6_2atmpS2182;
              int32_t _M0L6_2atmpS2184;
              moonbit_incref(_M0L8haystackS540.$0);
              #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS2181
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS540, _M0L6_2atmpS2183);
              moonbit_incref(_M0L6needleS542.$0);
              #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS2182
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS542, _M0L1jS549);
              if (_M0L6_2atmpS2181 != _M0L6_2atmpS2182) {
                break;
              }
              _M0L6_2atmpS2184 = _M0L1jS549 + 1;
              _M0L1jS549 = _M0L6_2atmpS2184;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS543);
              moonbit_decref(_M0L6needleS542.$0);
              moonbit_decref(_M0L8haystackS540.$0);
              return (int64_t)_M0L1iS547;
            }
            break;
          }
          _M0L6_2atmpS2191 = _M0L1iS547 + _M0L11needle__lenS541;
          _M0L6_2atmpS2190 = _M0L6_2atmpS2191 - 1;
          moonbit_incref(_M0L8haystackS540.$0);
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2189
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS540, _M0L6_2atmpS2190);
          _M0L6_2atmpS2188 = (int32_t)_M0L6_2atmpS2189;
          _M0L6_2atmpS2187 = _M0L6_2atmpS2188 & 255;
          if (
            _M0L6_2atmpS2187 < 0
            || _M0L6_2atmpS2187
               >= Moonbit_array_length(_M0L11skip__tableS543)
          ) {
            #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2186 = (int32_t)_M0L11skip__tableS543[_M0L6_2atmpS2187];
          _M0L6_2atmpS2185 = _M0L1iS547 + _M0L6_2atmpS2186;
          _M0L1iS547 = _M0L6_2atmpS2185;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS543);
          moonbit_decref(_M0L6needleS542.$0);
          moonbit_decref(_M0L8haystackS540.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS542.$0);
      moonbit_decref(_M0L8haystackS540.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS542.$0);
    moonbit_decref(_M0L8haystackS540.$0);
    return _M0FPB43boyer__moore__horspool__find_2econstr_2f538;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS536,
  struct _M0TPC16string10StringView _M0L3strS537
) {
  int32_t _M0L3lenS2162;
  int32_t _M0L6_2atmpS2164;
  int32_t _M0L6_2atmpS2163;
  int32_t _M0L6_2atmpS2161;
  moonbit_bytes_t _M0L8_2afieldS3714;
  moonbit_bytes_t _M0L4dataS2165;
  int32_t _M0L3lenS2166;
  moonbit_string_t _M0L6_2atmpS2167;
  int32_t _M0L6_2atmpS2168;
  int32_t _M0L6_2atmpS2169;
  int32_t _M0L3lenS2171;
  int32_t _M0L6_2atmpS2173;
  int32_t _M0L6_2atmpS2172;
  int32_t _M0L6_2atmpS2170;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2162 = _M0L4selfS536->$1;
  moonbit_incref(_M0L3strS537.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2164 = _M0MPC16string10StringView6length(_M0L3strS537);
  _M0L6_2atmpS2163 = _M0L6_2atmpS2164 * 2;
  _M0L6_2atmpS2161 = _M0L3lenS2162 + _M0L6_2atmpS2163;
  moonbit_incref(_M0L4selfS536);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS536, _M0L6_2atmpS2161);
  _M0L8_2afieldS3714 = _M0L4selfS536->$0;
  _M0L4dataS2165 = _M0L8_2afieldS3714;
  _M0L3lenS2166 = _M0L4selfS536->$1;
  moonbit_incref(_M0L4dataS2165);
  moonbit_incref(_M0L3strS537.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2167 = _M0MPC16string10StringView4data(_M0L3strS537);
  moonbit_incref(_M0L3strS537.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2168 = _M0MPC16string10StringView13start__offset(_M0L3strS537);
  moonbit_incref(_M0L3strS537.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2169 = _M0MPC16string10StringView6length(_M0L3strS537);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2165, _M0L3lenS2166, _M0L6_2atmpS2167, _M0L6_2atmpS2168, _M0L6_2atmpS2169);
  _M0L3lenS2171 = _M0L4selfS536->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2173 = _M0MPC16string10StringView6length(_M0L3strS537);
  _M0L6_2atmpS2172 = _M0L6_2atmpS2173 * 2;
  _M0L6_2atmpS2170 = _M0L3lenS2171 + _M0L6_2atmpS2172;
  _M0L4selfS536->$1 = _M0L6_2atmpS2170;
  moonbit_decref(_M0L4selfS536);
  return 0;
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS533,
  int32_t _M0L1iS534,
  int32_t _M0L13start__offsetS535,
  int64_t _M0L11end__offsetS531
) {
  int32_t _M0L11end__offsetS530;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS531 == 4294967296ll) {
    _M0L11end__offsetS530 = Moonbit_array_length(_M0L4selfS533);
  } else {
    int64_t _M0L7_2aSomeS532 = _M0L11end__offsetS531;
    _M0L11end__offsetS530 = (int32_t)_M0L7_2aSomeS532;
  }
  if (_M0L1iS534 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS533, _M0L1iS534, _M0L13start__offsetS535, _M0L11end__offsetS530);
  } else {
    int32_t _M0L6_2atmpS2160 = -_M0L1iS534;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS533, _M0L6_2atmpS2160, _M0L13start__offsetS535, _M0L11end__offsetS530);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS528,
  int32_t _M0L1nS526,
  int32_t _M0L13start__offsetS522,
  int32_t _M0L11end__offsetS523
) {
  int32_t _if__result_4041;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS522 >= 0) {
    _if__result_4041 = _M0L13start__offsetS522 <= _M0L11end__offsetS523;
  } else {
    _if__result_4041 = 0;
  }
  if (_if__result_4041) {
    int32_t _M0Lm13utf16__offsetS524 = _M0L13start__offsetS522;
    int32_t _M0Lm11char__countS525 = 0;
    int32_t _M0L6_2atmpS2158;
    int32_t _if__result_4044;
    while (1) {
      int32_t _M0L6_2atmpS2152 = _M0Lm13utf16__offsetS524;
      int32_t _if__result_4043;
      if (_M0L6_2atmpS2152 < _M0L11end__offsetS523) {
        int32_t _M0L6_2atmpS2151 = _M0Lm11char__countS525;
        _if__result_4043 = _M0L6_2atmpS2151 < _M0L1nS526;
      } else {
        _if__result_4043 = 0;
      }
      if (_if__result_4043) {
        int32_t _M0L6_2atmpS2156 = _M0Lm13utf16__offsetS524;
        int32_t _M0L1cS527 = _M0L4selfS528[_M0L6_2atmpS2156];
        int32_t _M0L6_2atmpS2155;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS527)) {
          int32_t _M0L6_2atmpS2153 = _M0Lm13utf16__offsetS524;
          _M0Lm13utf16__offsetS524 = _M0L6_2atmpS2153 + 2;
        } else {
          int32_t _M0L6_2atmpS2154 = _M0Lm13utf16__offsetS524;
          _M0Lm13utf16__offsetS524 = _M0L6_2atmpS2154 + 1;
        }
        _M0L6_2atmpS2155 = _M0Lm11char__countS525;
        _M0Lm11char__countS525 = _M0L6_2atmpS2155 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS528);
      }
      break;
    }
    _M0L6_2atmpS2158 = _M0Lm11char__countS525;
    if (_M0L6_2atmpS2158 < _M0L1nS526) {
      _if__result_4044 = 1;
    } else {
      int32_t _M0L6_2atmpS2157 = _M0Lm13utf16__offsetS524;
      _if__result_4044 = _M0L6_2atmpS2157 >= _M0L11end__offsetS523;
    }
    if (_if__result_4044) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2159 = _M0Lm13utf16__offsetS524;
      return (int64_t)_M0L6_2atmpS2159;
    }
  } else {
    moonbit_decref(_M0L4selfS528);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_79.data, (moonbit_string_t)moonbit_string_literal_80.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS520,
  int32_t _M0L1nS518,
  int32_t _M0L13start__offsetS517,
  int32_t _M0L11end__offsetS516
) {
  int32_t _M0Lm11char__countS514;
  int32_t _M0Lm13utf16__offsetS515;
  int32_t _M0L6_2atmpS2149;
  int32_t _if__result_4047;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS514 = 0;
  _M0Lm13utf16__offsetS515 = _M0L11end__offsetS516;
  while (1) {
    int32_t _M0L6_2atmpS2142 = _M0Lm13utf16__offsetS515;
    int32_t _M0L6_2atmpS2141 = _M0L6_2atmpS2142 - 1;
    int32_t _if__result_4046;
    if (_M0L6_2atmpS2141 >= _M0L13start__offsetS517) {
      int32_t _M0L6_2atmpS2140 = _M0Lm11char__countS514;
      _if__result_4046 = _M0L6_2atmpS2140 < _M0L1nS518;
    } else {
      _if__result_4046 = 0;
    }
    if (_if__result_4046) {
      int32_t _M0L6_2atmpS2147 = _M0Lm13utf16__offsetS515;
      int32_t _M0L6_2atmpS2146 = _M0L6_2atmpS2147 - 1;
      int32_t _M0L1cS519 = _M0L4selfS520[_M0L6_2atmpS2146];
      int32_t _M0L6_2atmpS2145;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS519)) {
        int32_t _M0L6_2atmpS2143 = _M0Lm13utf16__offsetS515;
        _M0Lm13utf16__offsetS515 = _M0L6_2atmpS2143 - 2;
      } else {
        int32_t _M0L6_2atmpS2144 = _M0Lm13utf16__offsetS515;
        _M0Lm13utf16__offsetS515 = _M0L6_2atmpS2144 - 1;
      }
      _M0L6_2atmpS2145 = _M0Lm11char__countS514;
      _M0Lm11char__countS514 = _M0L6_2atmpS2145 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS520);
    }
    break;
  }
  _M0L6_2atmpS2149 = _M0Lm11char__countS514;
  if (_M0L6_2atmpS2149 < _M0L1nS518) {
    _if__result_4047 = 1;
  } else {
    int32_t _M0L6_2atmpS2148 = _M0Lm13utf16__offsetS515;
    _if__result_4047 = _M0L6_2atmpS2148 < _M0L13start__offsetS517;
  }
  if (_if__result_4047) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2150 = _M0Lm13utf16__offsetS515;
    return (int64_t)_M0L6_2atmpS2150;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS506,
  int32_t _M0L3lenS509,
  int32_t _M0L13start__offsetS513,
  int64_t _M0L11end__offsetS504
) {
  int32_t _M0L11end__offsetS503;
  int32_t _M0L5indexS507;
  int32_t _M0L5countS508;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS504 == 4294967296ll) {
    _M0L11end__offsetS503 = Moonbit_array_length(_M0L4selfS506);
  } else {
    int64_t _M0L7_2aSomeS505 = _M0L11end__offsetS504;
    _M0L11end__offsetS503 = (int32_t)_M0L7_2aSomeS505;
  }
  _M0L5indexS507 = _M0L13start__offsetS513;
  _M0L5countS508 = 0;
  while (1) {
    int32_t _if__result_4049;
    if (_M0L5indexS507 < _M0L11end__offsetS503) {
      _if__result_4049 = _M0L5countS508 < _M0L3lenS509;
    } else {
      _if__result_4049 = 0;
    }
    if (_if__result_4049) {
      int32_t _M0L2c1S510 = _M0L4selfS506[_M0L5indexS507];
      int32_t _if__result_4050;
      int32_t _M0L6_2atmpS2138;
      int32_t _M0L6_2atmpS2139;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S510)) {
        int32_t _M0L6_2atmpS2134 = _M0L5indexS507 + 1;
        _if__result_4050 = _M0L6_2atmpS2134 < _M0L11end__offsetS503;
      } else {
        _if__result_4050 = 0;
      }
      if (_if__result_4050) {
        int32_t _M0L6_2atmpS2137 = _M0L5indexS507 + 1;
        int32_t _M0L2c2S511 = _M0L4selfS506[_M0L6_2atmpS2137];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S511)) {
          int32_t _M0L6_2atmpS2135 = _M0L5indexS507 + 2;
          int32_t _M0L6_2atmpS2136 = _M0L5countS508 + 1;
          _M0L5indexS507 = _M0L6_2atmpS2135;
          _M0L5countS508 = _M0L6_2atmpS2136;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_81.data, (moonbit_string_t)moonbit_string_literal_82.data);
        }
      }
      _M0L6_2atmpS2138 = _M0L5indexS507 + 1;
      _M0L6_2atmpS2139 = _M0L5countS508 + 1;
      _M0L5indexS507 = _M0L6_2atmpS2138;
      _M0L5countS508 = _M0L6_2atmpS2139;
      continue;
    } else {
      moonbit_decref(_M0L4selfS506);
      return _M0L5countS508 >= _M0L3lenS509;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS495,
  int32_t _M0L3lenS498,
  int32_t _M0L13start__offsetS502,
  int64_t _M0L11end__offsetS493
) {
  int32_t _M0L11end__offsetS492;
  int32_t _M0L5indexS496;
  int32_t _M0L5countS497;
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS493 == 4294967296ll) {
    _M0L11end__offsetS492 = Moonbit_array_length(_M0L4selfS495);
  } else {
    int64_t _M0L7_2aSomeS494 = _M0L11end__offsetS493;
    _M0L11end__offsetS492 = (int32_t)_M0L7_2aSomeS494;
  }
  _M0L5indexS496 = _M0L13start__offsetS502;
  _M0L5countS497 = 0;
  while (1) {
    int32_t _if__result_4052;
    if (_M0L5indexS496 < _M0L11end__offsetS492) {
      _if__result_4052 = _M0L5countS497 < _M0L3lenS498;
    } else {
      _if__result_4052 = 0;
    }
    if (_if__result_4052) {
      int32_t _M0L2c1S499 = _M0L4selfS495[_M0L5indexS496];
      int32_t _if__result_4053;
      int32_t _M0L6_2atmpS2132;
      int32_t _M0L6_2atmpS2133;
      #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S499)) {
        int32_t _M0L6_2atmpS2128 = _M0L5indexS496 + 1;
        _if__result_4053 = _M0L6_2atmpS2128 < _M0L11end__offsetS492;
      } else {
        _if__result_4053 = 0;
      }
      if (_if__result_4053) {
        int32_t _M0L6_2atmpS2131 = _M0L5indexS496 + 1;
        int32_t _M0L2c2S500 = _M0L4selfS495[_M0L6_2atmpS2131];
        #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S500)) {
          int32_t _M0L6_2atmpS2129 = _M0L5indexS496 + 2;
          int32_t _M0L6_2atmpS2130 = _M0L5countS497 + 1;
          _M0L5indexS496 = _M0L6_2atmpS2129;
          _M0L5countS497 = _M0L6_2atmpS2130;
          continue;
        } else {
          #line 426 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_81.data, (moonbit_string_t)moonbit_string_literal_83.data);
        }
      }
      _M0L6_2atmpS2132 = _M0L5indexS496 + 1;
      _M0L6_2atmpS2133 = _M0L5countS497 + 1;
      _M0L5indexS496 = _M0L6_2atmpS2132;
      _M0L5countS497 = _M0L6_2atmpS2133;
      continue;
    } else {
      moonbit_decref(_M0L4selfS495);
      if (_M0L5countS497 == _M0L3lenS498) {
        return _M0L5indexS496 == _M0L11end__offsetS492;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS488
) {
  int32_t _M0L3endS2120;
  int32_t _M0L8_2afieldS3715;
  int32_t _M0L5startS2121;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2120 = _M0L4selfS488.$2;
  _M0L8_2afieldS3715 = _M0L4selfS488.$1;
  moonbit_decref(_M0L4selfS488.$0);
  _M0L5startS2121 = _M0L8_2afieldS3715;
  return _M0L3endS2120 - _M0L5startS2121;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS489
) {
  int32_t _M0L3endS2122;
  int32_t _M0L8_2afieldS3716;
  int32_t _M0L5startS2123;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2122 = _M0L4selfS489.$2;
  _M0L8_2afieldS3716 = _M0L4selfS489.$1;
  moonbit_decref(_M0L4selfS489.$0);
  _M0L5startS2123 = _M0L8_2afieldS3716;
  return _M0L3endS2122 - _M0L5startS2123;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS490
) {
  int32_t _M0L3endS2124;
  int32_t _M0L8_2afieldS3717;
  int32_t _M0L5startS2125;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2124 = _M0L4selfS490.$2;
  _M0L8_2afieldS3717 = _M0L4selfS490.$1;
  moonbit_decref(_M0L4selfS490.$0);
  _M0L5startS2125 = _M0L8_2afieldS3717;
  return _M0L3endS2124 - _M0L5startS2125;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS491
) {
  int32_t _M0L3endS2126;
  int32_t _M0L8_2afieldS3718;
  int32_t _M0L5startS2127;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2126 = _M0L4selfS491.$2;
  _M0L8_2afieldS3718 = _M0L4selfS491.$1;
  moonbit_decref(_M0L4selfS491.$0);
  _M0L5startS2127 = _M0L8_2afieldS3718;
  return _M0L3endS2126 - _M0L5startS2127;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS486,
  int64_t _M0L19start__offset_2eoptS484,
  int64_t _M0L11end__offsetS487
) {
  int32_t _M0L13start__offsetS483;
  if (_M0L19start__offset_2eoptS484 == 4294967296ll) {
    _M0L13start__offsetS483 = 0;
  } else {
    int64_t _M0L7_2aSomeS485 = _M0L19start__offset_2eoptS484;
    _M0L13start__offsetS483 = (int32_t)_M0L7_2aSomeS485;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS486, _M0L13start__offsetS483, _M0L11end__offsetS487);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS481,
  int32_t _M0L13start__offsetS482,
  int64_t _M0L11end__offsetS479
) {
  int32_t _M0L11end__offsetS478;
  int32_t _if__result_4054;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS479 == 4294967296ll) {
    _M0L11end__offsetS478 = Moonbit_array_length(_M0L4selfS481);
  } else {
    int64_t _M0L7_2aSomeS480 = _M0L11end__offsetS479;
    _M0L11end__offsetS478 = (int32_t)_M0L7_2aSomeS480;
  }
  if (_M0L13start__offsetS482 >= 0) {
    if (_M0L13start__offsetS482 <= _M0L11end__offsetS478) {
      int32_t _M0L6_2atmpS2119 = Moonbit_array_length(_M0L4selfS481);
      _if__result_4054 = _M0L11end__offsetS478 <= _M0L6_2atmpS2119;
    } else {
      _if__result_4054 = 0;
    }
  } else {
    _if__result_4054 = 0;
  }
  if (_if__result_4054) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS482,
                                                 _M0L11end__offsetS478,
                                                 _M0L4selfS481};
  } else {
    moonbit_decref(_M0L4selfS481);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_84.data, (moonbit_string_t)moonbit_string_literal_85.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS473
) {
  int32_t _M0L5startS472;
  int32_t _M0L3endS474;
  struct _M0TPC13ref3RefGiE* _M0L5indexS475;
  struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__* _closure_4055;
  struct _M0TWEOc* _M0L6_2atmpS2098;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L5startS472 = _M0L4selfS473.$1;
  _M0L3endS474 = _M0L4selfS473.$2;
  _M0L5indexS475
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS475)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS475->$0 = _M0L5startS472;
  _closure_4055
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__));
  Moonbit_object_header(_closure_4055)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__, $0) >> 2, 2, 0);
  _closure_4055->code = &_M0MPC16string10StringView4iterC2099l198;
  _closure_4055->$0 = _M0L5indexS475;
  _closure_4055->$1 = _M0L3endS474;
  _closure_4055->$2_0 = _M0L4selfS473.$0;
  _closure_4055->$2_1 = _M0L4selfS473.$1;
  _closure_4055->$2_2 = _M0L4selfS473.$2;
  _M0L6_2atmpS2098 = (struct _M0TWEOc*)_closure_4055;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2098);
}

int32_t _M0MPC16string10StringView4iterC2099l198(
  struct _M0TWEOc* _M0L6_2aenvS2100
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__* _M0L14_2acasted__envS2101;
  struct _M0TPC16string10StringView _M0L8_2afieldS3724;
  struct _M0TPC16string10StringView _M0L4selfS473;
  int32_t _M0L3endS474;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3723;
  int32_t _M0L6_2acntS3887;
  struct _M0TPC13ref3RefGiE* _M0L5indexS475;
  int32_t _M0L3valS2102;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L14_2acasted__envS2101
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2099__l198__*)_M0L6_2aenvS2100;
  _M0L8_2afieldS3724
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS2101->$2_1,
      _M0L14_2acasted__envS2101->$2_2,
      _M0L14_2acasted__envS2101->$2_0
  };
  _M0L4selfS473 = _M0L8_2afieldS3724;
  _M0L3endS474 = _M0L14_2acasted__envS2101->$1;
  _M0L8_2afieldS3723 = _M0L14_2acasted__envS2101->$0;
  _M0L6_2acntS3887 = Moonbit_object_header(_M0L14_2acasted__envS2101)->rc;
  if (_M0L6_2acntS3887 > 1) {
    int32_t _M0L11_2anew__cntS3888 = _M0L6_2acntS3887 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2101)->rc
    = _M0L11_2anew__cntS3888;
    moonbit_incref(_M0L4selfS473.$0);
    moonbit_incref(_M0L8_2afieldS3723);
  } else if (_M0L6_2acntS3887 == 1) {
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS2101);
  }
  _M0L5indexS475 = _M0L8_2afieldS3723;
  _M0L3valS2102 = _M0L5indexS475->$0;
  if (_M0L3valS2102 < _M0L3endS474) {
    moonbit_string_t _M0L8_2afieldS3722 = _M0L4selfS473.$0;
    moonbit_string_t _M0L3strS2117 = _M0L8_2afieldS3722;
    int32_t _M0L3valS2118 = _M0L5indexS475->$0;
    int32_t _M0L6_2atmpS3721 = _M0L3strS2117[_M0L3valS2118];
    int32_t _M0L2c1S476 = _M0L6_2atmpS3721;
    int32_t _if__result_4056;
    int32_t _M0L3valS2115;
    int32_t _M0L6_2atmpS2114;
    int32_t _M0L6_2atmpS2116;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S476)) {
      int32_t _M0L3valS2105 = _M0L5indexS475->$0;
      int32_t _M0L6_2atmpS2103 = _M0L3valS2105 + 1;
      int32_t _M0L3endS2104 = _M0L4selfS473.$2;
      _if__result_4056 = _M0L6_2atmpS2103 < _M0L3endS2104;
    } else {
      _if__result_4056 = 0;
    }
    if (_if__result_4056) {
      moonbit_string_t _M0L8_2afieldS3720 = _M0L4selfS473.$0;
      moonbit_string_t _M0L3strS2111 = _M0L8_2afieldS3720;
      int32_t _M0L3valS2113 = _M0L5indexS475->$0;
      int32_t _M0L6_2atmpS2112 = _M0L3valS2113 + 1;
      int32_t _M0L6_2atmpS3719 = _M0L3strS2111[_M0L6_2atmpS2112];
      int32_t _M0L2c2S477;
      moonbit_decref(_M0L3strS2111);
      _M0L2c2S477 = _M0L6_2atmpS3719;
      #line 203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S477)) {
        int32_t _M0L3valS2107 = _M0L5indexS475->$0;
        int32_t _M0L6_2atmpS2106 = _M0L3valS2107 + 2;
        int32_t _M0L6_2atmpS2109;
        int32_t _M0L6_2atmpS2110;
        int32_t _M0L6_2atmpS2108;
        _M0L5indexS475->$0 = _M0L6_2atmpS2106;
        moonbit_decref(_M0L5indexS475);
        _M0L6_2atmpS2109 = (int32_t)_M0L2c1S476;
        _M0L6_2atmpS2110 = (int32_t)_M0L2c2S477;
        #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        _M0L6_2atmpS2108
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2109, _M0L6_2atmpS2110);
        return _M0L6_2atmpS2108;
      }
    } else {
      moonbit_decref(_M0L4selfS473.$0);
    }
    _M0L3valS2115 = _M0L5indexS475->$0;
    _M0L6_2atmpS2114 = _M0L3valS2115 + 1;
    _M0L5indexS475->$0 = _M0L6_2atmpS2114;
    moonbit_decref(_M0L5indexS475);
    #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L6_2atmpS2116 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S476);
    return _M0L6_2atmpS2116;
  } else {
    moonbit_decref(_M0L5indexS475);
    moonbit_decref(_M0L4selfS473.$0);
    return -1;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS471
) {
  moonbit_string_t _M0L8_2afieldS3726;
  moonbit_string_t _M0L3strS2095;
  int32_t _M0L5startS2096;
  int32_t _M0L8_2afieldS3725;
  int32_t _M0L3endS2097;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3726 = _M0L4selfS471.$0;
  _M0L3strS2095 = _M0L8_2afieldS3726;
  _M0L5startS2096 = _M0L4selfS471.$1;
  _M0L8_2afieldS3725 = _M0L4selfS471.$2;
  _M0L3endS2097 = _M0L8_2afieldS3725;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2095, _M0L5startS2096, _M0L3endS2097);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS469,
  struct _M0TPB6Logger _M0L6loggerS470
) {
  moonbit_string_t _M0L8_2afieldS3728;
  moonbit_string_t _M0L3strS2092;
  int32_t _M0L5startS2093;
  int32_t _M0L8_2afieldS3727;
  int32_t _M0L3endS2094;
  moonbit_string_t _M0L6substrS468;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3728 = _M0L4selfS469.$0;
  _M0L3strS2092 = _M0L8_2afieldS3728;
  _M0L5startS2093 = _M0L4selfS469.$1;
  _M0L8_2afieldS3727 = _M0L4selfS469.$2;
  _M0L3endS2094 = _M0L8_2afieldS3727;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS468
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2092, _M0L5startS2093, _M0L3endS2094);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS468, _M0L6loggerS470);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS460,
  struct _M0TPB6Logger _M0L6loggerS458
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS459;
  int32_t _M0L3lenS461;
  int32_t _M0L1iS462;
  int32_t _M0L3segS463;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS458.$1) {
    moonbit_incref(_M0L6loggerS458.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS458.$0->$method_3(_M0L6loggerS458.$1, 34);
  moonbit_incref(_M0L4selfS460);
  if (_M0L6loggerS458.$1) {
    moonbit_incref(_M0L6loggerS458.$1);
  }
  _M0L6_2aenvS459
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS459)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS459->$0 = _M0L4selfS460;
  _M0L6_2aenvS459->$1_0 = _M0L6loggerS458.$0;
  _M0L6_2aenvS459->$1_1 = _M0L6loggerS458.$1;
  _M0L3lenS461 = Moonbit_array_length(_M0L4selfS460);
  _M0L1iS462 = 0;
  _M0L3segS463 = 0;
  _2afor_464:;
  while (1) {
    int32_t _M0L4codeS465;
    int32_t _M0L1cS467;
    int32_t _M0L6_2atmpS2076;
    int32_t _M0L6_2atmpS2077;
    int32_t _M0L6_2atmpS2078;
    int32_t _tmp_4060;
    int32_t _tmp_4061;
    if (_M0L1iS462 >= _M0L3lenS461) {
      moonbit_decref(_M0L4selfS460);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS459, _M0L3segS463, _M0L1iS462);
      break;
    }
    _M0L4codeS465 = _M0L4selfS460[_M0L1iS462];
    switch (_M0L4codeS465) {
      case 34: {
        _M0L1cS467 = _M0L4codeS465;
        goto join_466;
        break;
      }
      
      case 92: {
        _M0L1cS467 = _M0L4codeS465;
        goto join_466;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2079;
        int32_t _M0L6_2atmpS2080;
        moonbit_incref(_M0L6_2aenvS459);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS459, _M0L3segS463, _M0L1iS462);
        if (_M0L6loggerS458.$1) {
          moonbit_incref(_M0L6loggerS458.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS458.$0->$method_0(_M0L6loggerS458.$1, (moonbit_string_t)moonbit_string_literal_66.data);
        _M0L6_2atmpS2079 = _M0L1iS462 + 1;
        _M0L6_2atmpS2080 = _M0L1iS462 + 1;
        _M0L1iS462 = _M0L6_2atmpS2079;
        _M0L3segS463 = _M0L6_2atmpS2080;
        goto _2afor_464;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2081;
        int32_t _M0L6_2atmpS2082;
        moonbit_incref(_M0L6_2aenvS459);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS459, _M0L3segS463, _M0L1iS462);
        if (_M0L6loggerS458.$1) {
          moonbit_incref(_M0L6loggerS458.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS458.$0->$method_0(_M0L6loggerS458.$1, (moonbit_string_t)moonbit_string_literal_67.data);
        _M0L6_2atmpS2081 = _M0L1iS462 + 1;
        _M0L6_2atmpS2082 = _M0L1iS462 + 1;
        _M0L1iS462 = _M0L6_2atmpS2081;
        _M0L3segS463 = _M0L6_2atmpS2082;
        goto _2afor_464;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2083;
        int32_t _M0L6_2atmpS2084;
        moonbit_incref(_M0L6_2aenvS459);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS459, _M0L3segS463, _M0L1iS462);
        if (_M0L6loggerS458.$1) {
          moonbit_incref(_M0L6loggerS458.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS458.$0->$method_0(_M0L6loggerS458.$1, (moonbit_string_t)moonbit_string_literal_68.data);
        _M0L6_2atmpS2083 = _M0L1iS462 + 1;
        _M0L6_2atmpS2084 = _M0L1iS462 + 1;
        _M0L1iS462 = _M0L6_2atmpS2083;
        _M0L3segS463 = _M0L6_2atmpS2084;
        goto _2afor_464;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2085;
        int32_t _M0L6_2atmpS2086;
        moonbit_incref(_M0L6_2aenvS459);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS459, _M0L3segS463, _M0L1iS462);
        if (_M0L6loggerS458.$1) {
          moonbit_incref(_M0L6loggerS458.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS458.$0->$method_0(_M0L6loggerS458.$1, (moonbit_string_t)moonbit_string_literal_69.data);
        _M0L6_2atmpS2085 = _M0L1iS462 + 1;
        _M0L6_2atmpS2086 = _M0L1iS462 + 1;
        _M0L1iS462 = _M0L6_2atmpS2085;
        _M0L3segS463 = _M0L6_2atmpS2086;
        goto _2afor_464;
        break;
      }
      default: {
        if (_M0L4codeS465 < 32) {
          int32_t _M0L6_2atmpS2088;
          moonbit_string_t _M0L6_2atmpS2087;
          int32_t _M0L6_2atmpS2089;
          int32_t _M0L6_2atmpS2090;
          moonbit_incref(_M0L6_2aenvS459);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS459, _M0L3segS463, _M0L1iS462);
          if (_M0L6loggerS458.$1) {
            moonbit_incref(_M0L6loggerS458.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS458.$0->$method_0(_M0L6loggerS458.$1, (moonbit_string_t)moonbit_string_literal_86.data);
          _M0L6_2atmpS2088 = _M0L4codeS465 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2087 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2088);
          if (_M0L6loggerS458.$1) {
            moonbit_incref(_M0L6loggerS458.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS458.$0->$method_0(_M0L6loggerS458.$1, _M0L6_2atmpS2087);
          if (_M0L6loggerS458.$1) {
            moonbit_incref(_M0L6loggerS458.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS458.$0->$method_3(_M0L6loggerS458.$1, 125);
          _M0L6_2atmpS2089 = _M0L1iS462 + 1;
          _M0L6_2atmpS2090 = _M0L1iS462 + 1;
          _M0L1iS462 = _M0L6_2atmpS2089;
          _M0L3segS463 = _M0L6_2atmpS2090;
          goto _2afor_464;
        } else {
          int32_t _M0L6_2atmpS2091 = _M0L1iS462 + 1;
          int32_t _tmp_4059 = _M0L3segS463;
          _M0L1iS462 = _M0L6_2atmpS2091;
          _M0L3segS463 = _tmp_4059;
          goto _2afor_464;
        }
        break;
      }
    }
    goto joinlet_4058;
    join_466:;
    moonbit_incref(_M0L6_2aenvS459);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS459, _M0L3segS463, _M0L1iS462);
    if (_M0L6loggerS458.$1) {
      moonbit_incref(_M0L6loggerS458.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS458.$0->$method_3(_M0L6loggerS458.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2076 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS467);
    if (_M0L6loggerS458.$1) {
      moonbit_incref(_M0L6loggerS458.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS458.$0->$method_3(_M0L6loggerS458.$1, _M0L6_2atmpS2076);
    _M0L6_2atmpS2077 = _M0L1iS462 + 1;
    _M0L6_2atmpS2078 = _M0L1iS462 + 1;
    _M0L1iS462 = _M0L6_2atmpS2077;
    _M0L3segS463 = _M0L6_2atmpS2078;
    continue;
    joinlet_4058:;
    _tmp_4060 = _M0L1iS462;
    _tmp_4061 = _M0L3segS463;
    _M0L1iS462 = _tmp_4060;
    _M0L3segS463 = _tmp_4061;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS458.$0->$method_3(_M0L6loggerS458.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS454,
  int32_t _M0L3segS457,
  int32_t _M0L1iS456
) {
  struct _M0TPB6Logger _M0L8_2afieldS3730;
  struct _M0TPB6Logger _M0L6loggerS453;
  moonbit_string_t _M0L8_2afieldS3729;
  int32_t _M0L6_2acntS3889;
  moonbit_string_t _M0L4selfS455;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3730
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS454->$1_0, _M0L6_2aenvS454->$1_1
  };
  _M0L6loggerS453 = _M0L8_2afieldS3730;
  _M0L8_2afieldS3729 = _M0L6_2aenvS454->$0;
  _M0L6_2acntS3889 = Moonbit_object_header(_M0L6_2aenvS454)->rc;
  if (_M0L6_2acntS3889 > 1) {
    int32_t _M0L11_2anew__cntS3890 = _M0L6_2acntS3889 - 1;
    Moonbit_object_header(_M0L6_2aenvS454)->rc = _M0L11_2anew__cntS3890;
    if (_M0L6loggerS453.$1) {
      moonbit_incref(_M0L6loggerS453.$1);
    }
    moonbit_incref(_M0L8_2afieldS3729);
  } else if (_M0L6_2acntS3889 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS454);
  }
  _M0L4selfS455 = _M0L8_2afieldS3729;
  if (_M0L1iS456 > _M0L3segS457) {
    int32_t _M0L6_2atmpS2075 = _M0L1iS456 - _M0L3segS457;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS453.$0->$method_1(_M0L6loggerS453.$1, _M0L4selfS455, _M0L3segS457, _M0L6_2atmpS2075);
  } else {
    moonbit_decref(_M0L4selfS455);
    if (_M0L6loggerS453.$1) {
      moonbit_decref(_M0L6loggerS453.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS452) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS451;
  int32_t _M0L6_2atmpS2072;
  int32_t _M0L6_2atmpS2071;
  int32_t _M0L6_2atmpS2074;
  int32_t _M0L6_2atmpS2073;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2070;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS451 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2072 = _M0IPC14byte4BytePB3Div3div(_M0L1bS452, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2071
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2072);
  moonbit_incref(_M0L7_2aselfS451);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS451, _M0L6_2atmpS2071);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2074 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS452, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2073
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2074);
  moonbit_incref(_M0L7_2aselfS451);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS451, _M0L6_2atmpS2073);
  _M0L6_2atmpS2070 = _M0L7_2aselfS451;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2070);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS450) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS450 < 10) {
    int32_t _M0L6_2atmpS2067;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2067 = _M0IPC14byte4BytePB3Add3add(_M0L1iS450, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2067);
  } else {
    int32_t _M0L6_2atmpS2069;
    int32_t _M0L6_2atmpS2068;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2069 = _M0IPC14byte4BytePB3Add3add(_M0L1iS450, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2068 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2069, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2068);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS448,
  int32_t _M0L4thatS449
) {
  int32_t _M0L6_2atmpS2065;
  int32_t _M0L6_2atmpS2066;
  int32_t _M0L6_2atmpS2064;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2065 = (int32_t)_M0L4selfS448;
  _M0L6_2atmpS2066 = (int32_t)_M0L4thatS449;
  _M0L6_2atmpS2064 = _M0L6_2atmpS2065 - _M0L6_2atmpS2066;
  return _M0L6_2atmpS2064 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS446,
  int32_t _M0L4thatS447
) {
  int32_t _M0L6_2atmpS2062;
  int32_t _M0L6_2atmpS2063;
  int32_t _M0L6_2atmpS2061;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2062 = (int32_t)_M0L4selfS446;
  _M0L6_2atmpS2063 = (int32_t)_M0L4thatS447;
  _M0L6_2atmpS2061 = _M0L6_2atmpS2062 % _M0L6_2atmpS2063;
  return _M0L6_2atmpS2061 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS444,
  int32_t _M0L4thatS445
) {
  int32_t _M0L6_2atmpS2059;
  int32_t _M0L6_2atmpS2060;
  int32_t _M0L6_2atmpS2058;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2059 = (int32_t)_M0L4selfS444;
  _M0L6_2atmpS2060 = (int32_t)_M0L4thatS445;
  _M0L6_2atmpS2058 = _M0L6_2atmpS2059 / _M0L6_2atmpS2060;
  return _M0L6_2atmpS2058 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS442,
  int32_t _M0L4thatS443
) {
  int32_t _M0L6_2atmpS2056;
  int32_t _M0L6_2atmpS2057;
  int32_t _M0L6_2atmpS2055;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2056 = (int32_t)_M0L4selfS442;
  _M0L6_2atmpS2057 = (int32_t)_M0L4thatS443;
  _M0L6_2atmpS2055 = _M0L6_2atmpS2056 + _M0L6_2atmpS2057;
  return _M0L6_2atmpS2055 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS439,
  int32_t _M0L5startS437,
  int32_t _M0L3endS438
) {
  int32_t _if__result_4062;
  int32_t _M0L3lenS440;
  int32_t _M0L6_2atmpS2053;
  int32_t _M0L6_2atmpS2054;
  moonbit_bytes_t _M0L5bytesS441;
  moonbit_bytes_t _M0L6_2atmpS2052;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS437 == 0) {
    int32_t _M0L6_2atmpS2051 = Moonbit_array_length(_M0L3strS439);
    _if__result_4062 = _M0L3endS438 == _M0L6_2atmpS2051;
  } else {
    _if__result_4062 = 0;
  }
  if (_if__result_4062) {
    return _M0L3strS439;
  }
  _M0L3lenS440 = _M0L3endS438 - _M0L5startS437;
  _M0L6_2atmpS2053 = _M0L3lenS440 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2054 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS441
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2053, _M0L6_2atmpS2054);
  moonbit_incref(_M0L5bytesS441);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS441, 0, _M0L3strS439, _M0L5startS437, _M0L3lenS440);
  _M0L6_2atmpS2052 = _M0L5bytesS441;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2052, 0, 4294967296ll);
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView _M0L4selfS435,
  int32_t _M0L13start__offsetS436,
  int64_t _M0L11end__offsetS433
) {
  int32_t _M0L11end__offsetS432;
  int32_t _if__result_4063;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS433 == 4294967296ll) {
    moonbit_incref(_M0L4selfS435.$0);
    #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L11end__offsetS432 = _M0MPC16string10StringView6length(_M0L4selfS435);
  } else {
    int64_t _M0L7_2aSomeS434 = _M0L11end__offsetS433;
    _M0L11end__offsetS432 = (int32_t)_M0L7_2aSomeS434;
  }
  if (_M0L13start__offsetS436 >= 0) {
    if (_M0L13start__offsetS436 <= _M0L11end__offsetS432) {
      int32_t _M0L6_2atmpS2045;
      moonbit_incref(_M0L4selfS435.$0);
      #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2045 = _M0MPC16string10StringView6length(_M0L4selfS435);
      _if__result_4063 = _M0L11end__offsetS432 <= _M0L6_2atmpS2045;
    } else {
      _if__result_4063 = 0;
    }
  } else {
    _if__result_4063 = 0;
  }
  if (_if__result_4063) {
    moonbit_string_t _M0L8_2afieldS3732 = _M0L4selfS435.$0;
    moonbit_string_t _M0L3strS2046 = _M0L8_2afieldS3732;
    int32_t _M0L5startS2050 = _M0L4selfS435.$1;
    int32_t _M0L6_2atmpS2047 = _M0L5startS2050 + _M0L13start__offsetS436;
    int32_t _M0L8_2afieldS3731 = _M0L4selfS435.$1;
    int32_t _M0L5startS2049 = _M0L8_2afieldS3731;
    int32_t _M0L6_2atmpS2048 = _M0L5startS2049 + _M0L11end__offsetS432;
    return (struct _M0TPC16string10StringView){_M0L6_2atmpS2047,
                                                 _M0L6_2atmpS2048,
                                                 _M0L3strS2046};
  } else {
    moonbit_decref(_M0L4selfS435.$0);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_84.data, (moonbit_string_t)moonbit_string_literal_87.data);
  }
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS428) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS428;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS429
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS429;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS430) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS430;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L1fS431
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS431;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS426,
  int32_t _M0L5indexS427
) {
  moonbit_string_t _M0L8_2afieldS3735;
  moonbit_string_t _M0L3strS2042;
  int32_t _M0L8_2afieldS3734;
  int32_t _M0L5startS2044;
  int32_t _M0L6_2atmpS2043;
  int32_t _M0L6_2atmpS3733;
  #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3735 = _M0L4selfS426.$0;
  _M0L3strS2042 = _M0L8_2afieldS3735;
  _M0L8_2afieldS3734 = _M0L4selfS426.$1;
  _M0L5startS2044 = _M0L8_2afieldS3734;
  _M0L6_2atmpS2043 = _M0L5startS2044 + _M0L5indexS427;
  _M0L6_2atmpS3733 = _M0L3strS2042[_M0L6_2atmpS2043];
  moonbit_decref(_M0L3strS2042);
  return _M0L6_2atmpS3733;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS418,
  int32_t _M0L5radixS417
) {
  int32_t _if__result_4064;
  uint16_t* _M0L6bufferS419;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS417 < 2) {
    _if__result_4064 = 1;
  } else {
    _if__result_4064 = _M0L5radixS417 > 36;
  }
  if (_if__result_4064) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_88.data, (moonbit_string_t)moonbit_string_literal_89.data);
  }
  if (_M0L4selfS418 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_72.data;
  }
  switch (_M0L5radixS417) {
    case 10: {
      int32_t _M0L3lenS420;
      uint16_t* _M0L6bufferS421;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS420 = _M0FPB12dec__count64(_M0L4selfS418);
      _M0L6bufferS421 = (uint16_t*)moonbit_make_string(_M0L3lenS420, 0);
      moonbit_incref(_M0L6bufferS421);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS421, _M0L4selfS418, 0, _M0L3lenS420);
      _M0L6bufferS419 = _M0L6bufferS421;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS422;
      uint16_t* _M0L6bufferS423;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS422 = _M0FPB12hex__count64(_M0L4selfS418);
      _M0L6bufferS423 = (uint16_t*)moonbit_make_string(_M0L3lenS422, 0);
      moonbit_incref(_M0L6bufferS423);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS423, _M0L4selfS418, 0, _M0L3lenS422);
      _M0L6bufferS419 = _M0L6bufferS423;
      break;
    }
    default: {
      int32_t _M0L3lenS424;
      uint16_t* _M0L6bufferS425;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS424 = _M0FPB14radix__count64(_M0L4selfS418, _M0L5radixS417);
      _M0L6bufferS425 = (uint16_t*)moonbit_make_string(_M0L3lenS424, 0);
      moonbit_incref(_M0L6bufferS425);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS425, _M0L4selfS418, 0, _M0L3lenS424, _M0L5radixS417);
      _M0L6bufferS419 = _M0L6bufferS425;
      break;
    }
  }
  return _M0L6bufferS419;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS407,
  uint64_t _M0L3numS395,
  int32_t _M0L12digit__startS398,
  int32_t _M0L10total__lenS397
) {
  uint64_t _M0Lm3numS394;
  int32_t _M0Lm6offsetS396;
  uint64_t _M0L6_2atmpS2041;
  int32_t _M0Lm9remainingS409;
  int32_t _M0L6_2atmpS2022;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS394 = _M0L3numS395;
  _M0Lm6offsetS396 = _M0L10total__lenS397 - _M0L12digit__startS398;
  while (1) {
    uint64_t _M0L6_2atmpS1985 = _M0Lm3numS394;
    if (_M0L6_2atmpS1985 >= 10000ull) {
      uint64_t _M0L6_2atmpS2008 = _M0Lm3numS394;
      uint64_t _M0L1tS399 = _M0L6_2atmpS2008 / 10000ull;
      uint64_t _M0L6_2atmpS2007 = _M0Lm3numS394;
      uint64_t _M0L6_2atmpS2006 = _M0L6_2atmpS2007 % 10000ull;
      int32_t _M0L1rS400 = (int32_t)_M0L6_2atmpS2006;
      int32_t _M0L2d1S401;
      int32_t _M0L2d2S402;
      int32_t _M0L6_2atmpS1986;
      int32_t _M0L6_2atmpS2005;
      int32_t _M0L6_2atmpS2004;
      int32_t _M0L6d1__hiS403;
      int32_t _M0L6_2atmpS2003;
      int32_t _M0L6_2atmpS2002;
      int32_t _M0L6d1__loS404;
      int32_t _M0L6_2atmpS2001;
      int32_t _M0L6_2atmpS2000;
      int32_t _M0L6d2__hiS405;
      int32_t _M0L6_2atmpS1999;
      int32_t _M0L6_2atmpS1998;
      int32_t _M0L6d2__loS406;
      int32_t _M0L6_2atmpS1988;
      int32_t _M0L6_2atmpS1987;
      int32_t _M0L6_2atmpS1991;
      int32_t _M0L6_2atmpS1990;
      int32_t _M0L6_2atmpS1989;
      int32_t _M0L6_2atmpS1994;
      int32_t _M0L6_2atmpS1993;
      int32_t _M0L6_2atmpS1992;
      int32_t _M0L6_2atmpS1997;
      int32_t _M0L6_2atmpS1996;
      int32_t _M0L6_2atmpS1995;
      _M0Lm3numS394 = _M0L1tS399;
      _M0L2d1S401 = _M0L1rS400 / 100;
      _M0L2d2S402 = _M0L1rS400 % 100;
      _M0L6_2atmpS1986 = _M0Lm6offsetS396;
      _M0Lm6offsetS396 = _M0L6_2atmpS1986 - 4;
      _M0L6_2atmpS2005 = _M0L2d1S401 / 10;
      _M0L6_2atmpS2004 = 48 + _M0L6_2atmpS2005;
      _M0L6d1__hiS403 = (uint16_t)_M0L6_2atmpS2004;
      _M0L6_2atmpS2003 = _M0L2d1S401 % 10;
      _M0L6_2atmpS2002 = 48 + _M0L6_2atmpS2003;
      _M0L6d1__loS404 = (uint16_t)_M0L6_2atmpS2002;
      _M0L6_2atmpS2001 = _M0L2d2S402 / 10;
      _M0L6_2atmpS2000 = 48 + _M0L6_2atmpS2001;
      _M0L6d2__hiS405 = (uint16_t)_M0L6_2atmpS2000;
      _M0L6_2atmpS1999 = _M0L2d2S402 % 10;
      _M0L6_2atmpS1998 = 48 + _M0L6_2atmpS1999;
      _M0L6d2__loS406 = (uint16_t)_M0L6_2atmpS1998;
      _M0L6_2atmpS1988 = _M0Lm6offsetS396;
      _M0L6_2atmpS1987 = _M0L12digit__startS398 + _M0L6_2atmpS1988;
      _M0L6bufferS407[_M0L6_2atmpS1987] = _M0L6d1__hiS403;
      _M0L6_2atmpS1991 = _M0Lm6offsetS396;
      _M0L6_2atmpS1990 = _M0L12digit__startS398 + _M0L6_2atmpS1991;
      _M0L6_2atmpS1989 = _M0L6_2atmpS1990 + 1;
      _M0L6bufferS407[_M0L6_2atmpS1989] = _M0L6d1__loS404;
      _M0L6_2atmpS1994 = _M0Lm6offsetS396;
      _M0L6_2atmpS1993 = _M0L12digit__startS398 + _M0L6_2atmpS1994;
      _M0L6_2atmpS1992 = _M0L6_2atmpS1993 + 2;
      _M0L6bufferS407[_M0L6_2atmpS1992] = _M0L6d2__hiS405;
      _M0L6_2atmpS1997 = _M0Lm6offsetS396;
      _M0L6_2atmpS1996 = _M0L12digit__startS398 + _M0L6_2atmpS1997;
      _M0L6_2atmpS1995 = _M0L6_2atmpS1996 + 3;
      _M0L6bufferS407[_M0L6_2atmpS1995] = _M0L6d2__loS406;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2041 = _M0Lm3numS394;
  _M0Lm9remainingS409 = (int32_t)_M0L6_2atmpS2041;
  while (1) {
    int32_t _M0L6_2atmpS2009 = _M0Lm9remainingS409;
    if (_M0L6_2atmpS2009 >= 100) {
      int32_t _M0L6_2atmpS2021 = _M0Lm9remainingS409;
      int32_t _M0L1tS410 = _M0L6_2atmpS2021 / 100;
      int32_t _M0L6_2atmpS2020 = _M0Lm9remainingS409;
      int32_t _M0L1dS411 = _M0L6_2atmpS2020 % 100;
      int32_t _M0L6_2atmpS2010;
      int32_t _M0L6_2atmpS2019;
      int32_t _M0L6_2atmpS2018;
      int32_t _M0L5d__hiS412;
      int32_t _M0L6_2atmpS2017;
      int32_t _M0L6_2atmpS2016;
      int32_t _M0L5d__loS413;
      int32_t _M0L6_2atmpS2012;
      int32_t _M0L6_2atmpS2011;
      int32_t _M0L6_2atmpS2015;
      int32_t _M0L6_2atmpS2014;
      int32_t _M0L6_2atmpS2013;
      _M0Lm9remainingS409 = _M0L1tS410;
      _M0L6_2atmpS2010 = _M0Lm6offsetS396;
      _M0Lm6offsetS396 = _M0L6_2atmpS2010 - 2;
      _M0L6_2atmpS2019 = _M0L1dS411 / 10;
      _M0L6_2atmpS2018 = 48 + _M0L6_2atmpS2019;
      _M0L5d__hiS412 = (uint16_t)_M0L6_2atmpS2018;
      _M0L6_2atmpS2017 = _M0L1dS411 % 10;
      _M0L6_2atmpS2016 = 48 + _M0L6_2atmpS2017;
      _M0L5d__loS413 = (uint16_t)_M0L6_2atmpS2016;
      _M0L6_2atmpS2012 = _M0Lm6offsetS396;
      _M0L6_2atmpS2011 = _M0L12digit__startS398 + _M0L6_2atmpS2012;
      _M0L6bufferS407[_M0L6_2atmpS2011] = _M0L5d__hiS412;
      _M0L6_2atmpS2015 = _M0Lm6offsetS396;
      _M0L6_2atmpS2014 = _M0L12digit__startS398 + _M0L6_2atmpS2015;
      _M0L6_2atmpS2013 = _M0L6_2atmpS2014 + 1;
      _M0L6bufferS407[_M0L6_2atmpS2013] = _M0L5d__loS413;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2022 = _M0Lm9remainingS409;
  if (_M0L6_2atmpS2022 >= 10) {
    int32_t _M0L6_2atmpS2023 = _M0Lm6offsetS396;
    int32_t _M0L6_2atmpS2034;
    int32_t _M0L6_2atmpS2033;
    int32_t _M0L6_2atmpS2032;
    int32_t _M0L5d__hiS415;
    int32_t _M0L6_2atmpS2031;
    int32_t _M0L6_2atmpS2030;
    int32_t _M0L6_2atmpS2029;
    int32_t _M0L5d__loS416;
    int32_t _M0L6_2atmpS2025;
    int32_t _M0L6_2atmpS2024;
    int32_t _M0L6_2atmpS2028;
    int32_t _M0L6_2atmpS2027;
    int32_t _M0L6_2atmpS2026;
    _M0Lm6offsetS396 = _M0L6_2atmpS2023 - 2;
    _M0L6_2atmpS2034 = _M0Lm9remainingS409;
    _M0L6_2atmpS2033 = _M0L6_2atmpS2034 / 10;
    _M0L6_2atmpS2032 = 48 + _M0L6_2atmpS2033;
    _M0L5d__hiS415 = (uint16_t)_M0L6_2atmpS2032;
    _M0L6_2atmpS2031 = _M0Lm9remainingS409;
    _M0L6_2atmpS2030 = _M0L6_2atmpS2031 % 10;
    _M0L6_2atmpS2029 = 48 + _M0L6_2atmpS2030;
    _M0L5d__loS416 = (uint16_t)_M0L6_2atmpS2029;
    _M0L6_2atmpS2025 = _M0Lm6offsetS396;
    _M0L6_2atmpS2024 = _M0L12digit__startS398 + _M0L6_2atmpS2025;
    _M0L6bufferS407[_M0L6_2atmpS2024] = _M0L5d__hiS415;
    _M0L6_2atmpS2028 = _M0Lm6offsetS396;
    _M0L6_2atmpS2027 = _M0L12digit__startS398 + _M0L6_2atmpS2028;
    _M0L6_2atmpS2026 = _M0L6_2atmpS2027 + 1;
    _M0L6bufferS407[_M0L6_2atmpS2026] = _M0L5d__loS416;
    moonbit_decref(_M0L6bufferS407);
  } else {
    int32_t _M0L6_2atmpS2035 = _M0Lm6offsetS396;
    int32_t _M0L6_2atmpS2040;
    int32_t _M0L6_2atmpS2036;
    int32_t _M0L6_2atmpS2039;
    int32_t _M0L6_2atmpS2038;
    int32_t _M0L6_2atmpS2037;
    _M0Lm6offsetS396 = _M0L6_2atmpS2035 - 1;
    _M0L6_2atmpS2040 = _M0Lm6offsetS396;
    _M0L6_2atmpS2036 = _M0L12digit__startS398 + _M0L6_2atmpS2040;
    _M0L6_2atmpS2039 = _M0Lm9remainingS409;
    _M0L6_2atmpS2038 = 48 + _M0L6_2atmpS2039;
    _M0L6_2atmpS2037 = (uint16_t)_M0L6_2atmpS2038;
    _M0L6bufferS407[_M0L6_2atmpS2036] = _M0L6_2atmpS2037;
    moonbit_decref(_M0L6bufferS407);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS389,
  uint64_t _M0L3numS383,
  int32_t _M0L12digit__startS381,
  int32_t _M0L10total__lenS380,
  int32_t _M0L5radixS385
) {
  int32_t _M0Lm6offsetS379;
  uint64_t _M0Lm1nS382;
  uint64_t _M0L4baseS384;
  int32_t _M0L6_2atmpS1967;
  int32_t _M0L6_2atmpS1966;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS379 = _M0L10total__lenS380 - _M0L12digit__startS381;
  _M0Lm1nS382 = _M0L3numS383;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS384 = _M0MPC13int3Int10to__uint64(_M0L5radixS385);
  _M0L6_2atmpS1967 = _M0L5radixS385 - 1;
  _M0L6_2atmpS1966 = _M0L5radixS385 & _M0L6_2atmpS1967;
  if (_M0L6_2atmpS1966 == 0) {
    int32_t _M0L5shiftS386;
    uint64_t _M0L4maskS387;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS386 = moonbit_ctz32(_M0L5radixS385);
    _M0L4maskS387 = _M0L4baseS384 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1968 = _M0Lm1nS382;
      if (_M0L6_2atmpS1968 > 0ull) {
        int32_t _M0L6_2atmpS1969 = _M0Lm6offsetS379;
        uint64_t _M0L6_2atmpS1975;
        uint64_t _M0L6_2atmpS1974;
        int32_t _M0L5digitS388;
        int32_t _M0L6_2atmpS1972;
        int32_t _M0L6_2atmpS1970;
        int32_t _M0L6_2atmpS1971;
        uint64_t _M0L6_2atmpS1973;
        _M0Lm6offsetS379 = _M0L6_2atmpS1969 - 1;
        _M0L6_2atmpS1975 = _M0Lm1nS382;
        _M0L6_2atmpS1974 = _M0L6_2atmpS1975 & _M0L4maskS387;
        _M0L5digitS388 = (int32_t)_M0L6_2atmpS1974;
        _M0L6_2atmpS1972 = _M0Lm6offsetS379;
        _M0L6_2atmpS1970 = _M0L12digit__startS381 + _M0L6_2atmpS1972;
        _M0L6_2atmpS1971
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS388
        ];
        _M0L6bufferS389[_M0L6_2atmpS1970] = _M0L6_2atmpS1971;
        _M0L6_2atmpS1973 = _M0Lm1nS382;
        _M0Lm1nS382 = _M0L6_2atmpS1973 >> (_M0L5shiftS386 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS389);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1976 = _M0Lm1nS382;
      if (_M0L6_2atmpS1976 > 0ull) {
        int32_t _M0L6_2atmpS1977 = _M0Lm6offsetS379;
        uint64_t _M0L6_2atmpS1984;
        uint64_t _M0L1qS391;
        uint64_t _M0L6_2atmpS1982;
        uint64_t _M0L6_2atmpS1983;
        uint64_t _M0L6_2atmpS1981;
        int32_t _M0L5digitS392;
        int32_t _M0L6_2atmpS1980;
        int32_t _M0L6_2atmpS1978;
        int32_t _M0L6_2atmpS1979;
        _M0Lm6offsetS379 = _M0L6_2atmpS1977 - 1;
        _M0L6_2atmpS1984 = _M0Lm1nS382;
        _M0L1qS391 = _M0L6_2atmpS1984 / _M0L4baseS384;
        _M0L6_2atmpS1982 = _M0Lm1nS382;
        _M0L6_2atmpS1983 = _M0L1qS391 * _M0L4baseS384;
        _M0L6_2atmpS1981 = _M0L6_2atmpS1982 - _M0L6_2atmpS1983;
        _M0L5digitS392 = (int32_t)_M0L6_2atmpS1981;
        _M0L6_2atmpS1980 = _M0Lm6offsetS379;
        _M0L6_2atmpS1978 = _M0L12digit__startS381 + _M0L6_2atmpS1980;
        _M0L6_2atmpS1979
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS392
        ];
        _M0L6bufferS389[_M0L6_2atmpS1978] = _M0L6_2atmpS1979;
        _M0Lm1nS382 = _M0L1qS391;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS389);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS376,
  uint64_t _M0L3numS372,
  int32_t _M0L12digit__startS370,
  int32_t _M0L10total__lenS369
) {
  int32_t _M0Lm6offsetS368;
  uint64_t _M0Lm1nS371;
  int32_t _M0L6_2atmpS1962;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS368 = _M0L10total__lenS369 - _M0L12digit__startS370;
  _M0Lm1nS371 = _M0L3numS372;
  while (1) {
    int32_t _M0L6_2atmpS1950 = _M0Lm6offsetS368;
    if (_M0L6_2atmpS1950 >= 2) {
      int32_t _M0L6_2atmpS1951 = _M0Lm6offsetS368;
      uint64_t _M0L6_2atmpS1961;
      uint64_t _M0L6_2atmpS1960;
      int32_t _M0L9byte__valS373;
      int32_t _M0L2hiS374;
      int32_t _M0L2loS375;
      int32_t _M0L6_2atmpS1954;
      int32_t _M0L6_2atmpS1952;
      int32_t _M0L6_2atmpS1953;
      int32_t _M0L6_2atmpS1958;
      int32_t _M0L6_2atmpS1957;
      int32_t _M0L6_2atmpS1955;
      int32_t _M0L6_2atmpS1956;
      uint64_t _M0L6_2atmpS1959;
      _M0Lm6offsetS368 = _M0L6_2atmpS1951 - 2;
      _M0L6_2atmpS1961 = _M0Lm1nS371;
      _M0L6_2atmpS1960 = _M0L6_2atmpS1961 & 255ull;
      _M0L9byte__valS373 = (int32_t)_M0L6_2atmpS1960;
      _M0L2hiS374 = _M0L9byte__valS373 / 16;
      _M0L2loS375 = _M0L9byte__valS373 % 16;
      _M0L6_2atmpS1954 = _M0Lm6offsetS368;
      _M0L6_2atmpS1952 = _M0L12digit__startS370 + _M0L6_2atmpS1954;
      _M0L6_2atmpS1953
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2hiS374
      ];
      _M0L6bufferS376[_M0L6_2atmpS1952] = _M0L6_2atmpS1953;
      _M0L6_2atmpS1958 = _M0Lm6offsetS368;
      _M0L6_2atmpS1957 = _M0L12digit__startS370 + _M0L6_2atmpS1958;
      _M0L6_2atmpS1955 = _M0L6_2atmpS1957 + 1;
      _M0L6_2atmpS1956
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2loS375
      ];
      _M0L6bufferS376[_M0L6_2atmpS1955] = _M0L6_2atmpS1956;
      _M0L6_2atmpS1959 = _M0Lm1nS371;
      _M0Lm1nS371 = _M0L6_2atmpS1959 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1962 = _M0Lm6offsetS368;
  if (_M0L6_2atmpS1962 == 1) {
    uint64_t _M0L6_2atmpS1965 = _M0Lm1nS371;
    uint64_t _M0L6_2atmpS1964 = _M0L6_2atmpS1965 & 15ull;
    int32_t _M0L6nibbleS378 = (int32_t)_M0L6_2atmpS1964;
    int32_t _M0L6_2atmpS1963 =
      ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L6nibbleS378];
    _M0L6bufferS376[_M0L12digit__startS370] = _M0L6_2atmpS1963;
    moonbit_decref(_M0L6bufferS376);
  } else {
    moonbit_decref(_M0L6bufferS376);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS362,
  int32_t _M0L5radixS365
) {
  uint64_t _M0Lm3numS363;
  uint64_t _M0L4baseS364;
  int32_t _M0Lm5countS366;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS362 == 0ull) {
    return 1;
  }
  _M0Lm3numS363 = _M0L5valueS362;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS364 = _M0MPC13int3Int10to__uint64(_M0L5radixS365);
  _M0Lm5countS366 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1947 = _M0Lm3numS363;
    if (_M0L6_2atmpS1947 > 0ull) {
      int32_t _M0L6_2atmpS1948 = _M0Lm5countS366;
      uint64_t _M0L6_2atmpS1949;
      _M0Lm5countS366 = _M0L6_2atmpS1948 + 1;
      _M0L6_2atmpS1949 = _M0Lm3numS363;
      _M0Lm3numS363 = _M0L6_2atmpS1949 / _M0L4baseS364;
      continue;
    }
    break;
  }
  return _M0Lm5countS366;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS360) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS360 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS361;
    int32_t _M0L6_2atmpS1946;
    int32_t _M0L6_2atmpS1945;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS361 = moonbit_clz64(_M0L5valueS360);
    _M0L6_2atmpS1946 = 63 - _M0L14leading__zerosS361;
    _M0L6_2atmpS1945 = _M0L6_2atmpS1946 / 4;
    return _M0L6_2atmpS1945 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS359) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS359 >= 10000000000ull) {
    if (_M0L5valueS359 >= 100000000000000ull) {
      if (_M0L5valueS359 >= 10000000000000000ull) {
        if (_M0L5valueS359 >= 1000000000000000000ull) {
          if (_M0L5valueS359 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS359 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS359 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS359 >= 1000000000000ull) {
      if (_M0L5valueS359 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS359 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS359 >= 100000ull) {
    if (_M0L5valueS359 >= 10000000ull) {
      if (_M0L5valueS359 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS359 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS359 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS359 >= 1000ull) {
    if (_M0L5valueS359 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS359 >= 100ull) {
    return 3;
  } else if (_M0L5valueS359 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS343,
  int32_t _M0L5radixS342
) {
  int32_t _if__result_4071;
  int32_t _M0L12is__negativeS344;
  uint32_t _M0L3numS345;
  uint16_t* _M0L6bufferS346;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS342 < 2) {
    _if__result_4071 = 1;
  } else {
    _if__result_4071 = _M0L5radixS342 > 36;
  }
  if (_if__result_4071) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_88.data, (moonbit_string_t)moonbit_string_literal_91.data);
  }
  if (_M0L4selfS343 == 0) {
    return (moonbit_string_t)moonbit_string_literal_72.data;
  }
  _M0L12is__negativeS344 = _M0L4selfS343 < 0;
  if (_M0L12is__negativeS344) {
    int32_t _M0L6_2atmpS1944 = -_M0L4selfS343;
    _M0L3numS345 = *(uint32_t*)&_M0L6_2atmpS1944;
  } else {
    _M0L3numS345 = *(uint32_t*)&_M0L4selfS343;
  }
  switch (_M0L5radixS342) {
    case 10: {
      int32_t _M0L10digit__lenS347;
      int32_t _M0L6_2atmpS1941;
      int32_t _M0L10total__lenS348;
      uint16_t* _M0L6bufferS349;
      int32_t _M0L12digit__startS350;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS347 = _M0FPB12dec__count32(_M0L3numS345);
      if (_M0L12is__negativeS344) {
        _M0L6_2atmpS1941 = 1;
      } else {
        _M0L6_2atmpS1941 = 0;
      }
      _M0L10total__lenS348 = _M0L10digit__lenS347 + _M0L6_2atmpS1941;
      _M0L6bufferS349
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS348, 0);
      if (_M0L12is__negativeS344) {
        _M0L12digit__startS350 = 1;
      } else {
        _M0L12digit__startS350 = 0;
      }
      moonbit_incref(_M0L6bufferS349);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS349, _M0L3numS345, _M0L12digit__startS350, _M0L10total__lenS348);
      _M0L6bufferS346 = _M0L6bufferS349;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS351;
      int32_t _M0L6_2atmpS1942;
      int32_t _M0L10total__lenS352;
      uint16_t* _M0L6bufferS353;
      int32_t _M0L12digit__startS354;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS351 = _M0FPB12hex__count32(_M0L3numS345);
      if (_M0L12is__negativeS344) {
        _M0L6_2atmpS1942 = 1;
      } else {
        _M0L6_2atmpS1942 = 0;
      }
      _M0L10total__lenS352 = _M0L10digit__lenS351 + _M0L6_2atmpS1942;
      _M0L6bufferS353
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS352, 0);
      if (_M0L12is__negativeS344) {
        _M0L12digit__startS354 = 1;
      } else {
        _M0L12digit__startS354 = 0;
      }
      moonbit_incref(_M0L6bufferS353);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS353, _M0L3numS345, _M0L12digit__startS354, _M0L10total__lenS352);
      _M0L6bufferS346 = _M0L6bufferS353;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS355;
      int32_t _M0L6_2atmpS1943;
      int32_t _M0L10total__lenS356;
      uint16_t* _M0L6bufferS357;
      int32_t _M0L12digit__startS358;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS355
      = _M0FPB14radix__count32(_M0L3numS345, _M0L5radixS342);
      if (_M0L12is__negativeS344) {
        _M0L6_2atmpS1943 = 1;
      } else {
        _M0L6_2atmpS1943 = 0;
      }
      _M0L10total__lenS356 = _M0L10digit__lenS355 + _M0L6_2atmpS1943;
      _M0L6bufferS357
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS356, 0);
      if (_M0L12is__negativeS344) {
        _M0L12digit__startS358 = 1;
      } else {
        _M0L12digit__startS358 = 0;
      }
      moonbit_incref(_M0L6bufferS357);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS357, _M0L3numS345, _M0L12digit__startS358, _M0L10total__lenS356, _M0L5radixS342);
      _M0L6bufferS346 = _M0L6bufferS357;
      break;
    }
  }
  if (_M0L12is__negativeS344) {
    _M0L6bufferS346[0] = 45;
  }
  return _M0L6bufferS346;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS336,
  int32_t _M0L5radixS339
) {
  uint32_t _M0Lm3numS337;
  uint32_t _M0L4baseS338;
  int32_t _M0Lm5countS340;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS336 == 0u) {
    return 1;
  }
  _M0Lm3numS337 = _M0L5valueS336;
  _M0L4baseS338 = *(uint32_t*)&_M0L5radixS339;
  _M0Lm5countS340 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1938 = _M0Lm3numS337;
    if (_M0L6_2atmpS1938 > 0u) {
      int32_t _M0L6_2atmpS1939 = _M0Lm5countS340;
      uint32_t _M0L6_2atmpS1940;
      _M0Lm5countS340 = _M0L6_2atmpS1939 + 1;
      _M0L6_2atmpS1940 = _M0Lm3numS337;
      _M0Lm3numS337 = _M0L6_2atmpS1940 / _M0L4baseS338;
      continue;
    }
    break;
  }
  return _M0Lm5countS340;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS334) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS334 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS335;
    int32_t _M0L6_2atmpS1937;
    int32_t _M0L6_2atmpS1936;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS335 = moonbit_clz32(_M0L5valueS334);
    _M0L6_2atmpS1937 = 31 - _M0L14leading__zerosS335;
    _M0L6_2atmpS1936 = _M0L6_2atmpS1937 / 4;
    return _M0L6_2atmpS1936 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS333) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS333 >= 100000u) {
    if (_M0L5valueS333 >= 10000000u) {
      if (_M0L5valueS333 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS333 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS333 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS333 >= 1000u) {
    if (_M0L5valueS333 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS333 >= 100u) {
    return 3;
  } else if (_M0L5valueS333 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS323,
  uint32_t _M0L3numS311,
  int32_t _M0L12digit__startS314,
  int32_t _M0L10total__lenS313
) {
  uint32_t _M0Lm3numS310;
  int32_t _M0Lm6offsetS312;
  uint32_t _M0L6_2atmpS1935;
  int32_t _M0Lm9remainingS325;
  int32_t _M0L6_2atmpS1916;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS310 = _M0L3numS311;
  _M0Lm6offsetS312 = _M0L10total__lenS313 - _M0L12digit__startS314;
  while (1) {
    uint32_t _M0L6_2atmpS1879 = _M0Lm3numS310;
    if (_M0L6_2atmpS1879 >= 10000u) {
      uint32_t _M0L6_2atmpS1902 = _M0Lm3numS310;
      uint32_t _M0L1tS315 = _M0L6_2atmpS1902 / 10000u;
      uint32_t _M0L6_2atmpS1901 = _M0Lm3numS310;
      uint32_t _M0L6_2atmpS1900 = _M0L6_2atmpS1901 % 10000u;
      int32_t _M0L1rS316 = *(int32_t*)&_M0L6_2atmpS1900;
      int32_t _M0L2d1S317;
      int32_t _M0L2d2S318;
      int32_t _M0L6_2atmpS1880;
      int32_t _M0L6_2atmpS1899;
      int32_t _M0L6_2atmpS1898;
      int32_t _M0L6d1__hiS319;
      int32_t _M0L6_2atmpS1897;
      int32_t _M0L6_2atmpS1896;
      int32_t _M0L6d1__loS320;
      int32_t _M0L6_2atmpS1895;
      int32_t _M0L6_2atmpS1894;
      int32_t _M0L6d2__hiS321;
      int32_t _M0L6_2atmpS1893;
      int32_t _M0L6_2atmpS1892;
      int32_t _M0L6d2__loS322;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L6_2atmpS1881;
      int32_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1884;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6_2atmpS1888;
      int32_t _M0L6_2atmpS1887;
      int32_t _M0L6_2atmpS1886;
      int32_t _M0L6_2atmpS1891;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6_2atmpS1889;
      _M0Lm3numS310 = _M0L1tS315;
      _M0L2d1S317 = _M0L1rS316 / 100;
      _M0L2d2S318 = _M0L1rS316 % 100;
      _M0L6_2atmpS1880 = _M0Lm6offsetS312;
      _M0Lm6offsetS312 = _M0L6_2atmpS1880 - 4;
      _M0L6_2atmpS1899 = _M0L2d1S317 / 10;
      _M0L6_2atmpS1898 = 48 + _M0L6_2atmpS1899;
      _M0L6d1__hiS319 = (uint16_t)_M0L6_2atmpS1898;
      _M0L6_2atmpS1897 = _M0L2d1S317 % 10;
      _M0L6_2atmpS1896 = 48 + _M0L6_2atmpS1897;
      _M0L6d1__loS320 = (uint16_t)_M0L6_2atmpS1896;
      _M0L6_2atmpS1895 = _M0L2d2S318 / 10;
      _M0L6_2atmpS1894 = 48 + _M0L6_2atmpS1895;
      _M0L6d2__hiS321 = (uint16_t)_M0L6_2atmpS1894;
      _M0L6_2atmpS1893 = _M0L2d2S318 % 10;
      _M0L6_2atmpS1892 = 48 + _M0L6_2atmpS1893;
      _M0L6d2__loS322 = (uint16_t)_M0L6_2atmpS1892;
      _M0L6_2atmpS1882 = _M0Lm6offsetS312;
      _M0L6_2atmpS1881 = _M0L12digit__startS314 + _M0L6_2atmpS1882;
      _M0L6bufferS323[_M0L6_2atmpS1881] = _M0L6d1__hiS319;
      _M0L6_2atmpS1885 = _M0Lm6offsetS312;
      _M0L6_2atmpS1884 = _M0L12digit__startS314 + _M0L6_2atmpS1885;
      _M0L6_2atmpS1883 = _M0L6_2atmpS1884 + 1;
      _M0L6bufferS323[_M0L6_2atmpS1883] = _M0L6d1__loS320;
      _M0L6_2atmpS1888 = _M0Lm6offsetS312;
      _M0L6_2atmpS1887 = _M0L12digit__startS314 + _M0L6_2atmpS1888;
      _M0L6_2atmpS1886 = _M0L6_2atmpS1887 + 2;
      _M0L6bufferS323[_M0L6_2atmpS1886] = _M0L6d2__hiS321;
      _M0L6_2atmpS1891 = _M0Lm6offsetS312;
      _M0L6_2atmpS1890 = _M0L12digit__startS314 + _M0L6_2atmpS1891;
      _M0L6_2atmpS1889 = _M0L6_2atmpS1890 + 3;
      _M0L6bufferS323[_M0L6_2atmpS1889] = _M0L6d2__loS322;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1935 = _M0Lm3numS310;
  _M0Lm9remainingS325 = *(int32_t*)&_M0L6_2atmpS1935;
  while (1) {
    int32_t _M0L6_2atmpS1903 = _M0Lm9remainingS325;
    if (_M0L6_2atmpS1903 >= 100) {
      int32_t _M0L6_2atmpS1915 = _M0Lm9remainingS325;
      int32_t _M0L1tS326 = _M0L6_2atmpS1915 / 100;
      int32_t _M0L6_2atmpS1914 = _M0Lm9remainingS325;
      int32_t _M0L1dS327 = _M0L6_2atmpS1914 % 100;
      int32_t _M0L6_2atmpS1904;
      int32_t _M0L6_2atmpS1913;
      int32_t _M0L6_2atmpS1912;
      int32_t _M0L5d__hiS328;
      int32_t _M0L6_2atmpS1911;
      int32_t _M0L6_2atmpS1910;
      int32_t _M0L5d__loS329;
      int32_t _M0L6_2atmpS1906;
      int32_t _M0L6_2atmpS1905;
      int32_t _M0L6_2atmpS1909;
      int32_t _M0L6_2atmpS1908;
      int32_t _M0L6_2atmpS1907;
      _M0Lm9remainingS325 = _M0L1tS326;
      _M0L6_2atmpS1904 = _M0Lm6offsetS312;
      _M0Lm6offsetS312 = _M0L6_2atmpS1904 - 2;
      _M0L6_2atmpS1913 = _M0L1dS327 / 10;
      _M0L6_2atmpS1912 = 48 + _M0L6_2atmpS1913;
      _M0L5d__hiS328 = (uint16_t)_M0L6_2atmpS1912;
      _M0L6_2atmpS1911 = _M0L1dS327 % 10;
      _M0L6_2atmpS1910 = 48 + _M0L6_2atmpS1911;
      _M0L5d__loS329 = (uint16_t)_M0L6_2atmpS1910;
      _M0L6_2atmpS1906 = _M0Lm6offsetS312;
      _M0L6_2atmpS1905 = _M0L12digit__startS314 + _M0L6_2atmpS1906;
      _M0L6bufferS323[_M0L6_2atmpS1905] = _M0L5d__hiS328;
      _M0L6_2atmpS1909 = _M0Lm6offsetS312;
      _M0L6_2atmpS1908 = _M0L12digit__startS314 + _M0L6_2atmpS1909;
      _M0L6_2atmpS1907 = _M0L6_2atmpS1908 + 1;
      _M0L6bufferS323[_M0L6_2atmpS1907] = _M0L5d__loS329;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1916 = _M0Lm9remainingS325;
  if (_M0L6_2atmpS1916 >= 10) {
    int32_t _M0L6_2atmpS1917 = _M0Lm6offsetS312;
    int32_t _M0L6_2atmpS1928;
    int32_t _M0L6_2atmpS1927;
    int32_t _M0L6_2atmpS1926;
    int32_t _M0L5d__hiS331;
    int32_t _M0L6_2atmpS1925;
    int32_t _M0L6_2atmpS1924;
    int32_t _M0L6_2atmpS1923;
    int32_t _M0L5d__loS332;
    int32_t _M0L6_2atmpS1919;
    int32_t _M0L6_2atmpS1918;
    int32_t _M0L6_2atmpS1922;
    int32_t _M0L6_2atmpS1921;
    int32_t _M0L6_2atmpS1920;
    _M0Lm6offsetS312 = _M0L6_2atmpS1917 - 2;
    _M0L6_2atmpS1928 = _M0Lm9remainingS325;
    _M0L6_2atmpS1927 = _M0L6_2atmpS1928 / 10;
    _M0L6_2atmpS1926 = 48 + _M0L6_2atmpS1927;
    _M0L5d__hiS331 = (uint16_t)_M0L6_2atmpS1926;
    _M0L6_2atmpS1925 = _M0Lm9remainingS325;
    _M0L6_2atmpS1924 = _M0L6_2atmpS1925 % 10;
    _M0L6_2atmpS1923 = 48 + _M0L6_2atmpS1924;
    _M0L5d__loS332 = (uint16_t)_M0L6_2atmpS1923;
    _M0L6_2atmpS1919 = _M0Lm6offsetS312;
    _M0L6_2atmpS1918 = _M0L12digit__startS314 + _M0L6_2atmpS1919;
    _M0L6bufferS323[_M0L6_2atmpS1918] = _M0L5d__hiS331;
    _M0L6_2atmpS1922 = _M0Lm6offsetS312;
    _M0L6_2atmpS1921 = _M0L12digit__startS314 + _M0L6_2atmpS1922;
    _M0L6_2atmpS1920 = _M0L6_2atmpS1921 + 1;
    _M0L6bufferS323[_M0L6_2atmpS1920] = _M0L5d__loS332;
    moonbit_decref(_M0L6bufferS323);
  } else {
    int32_t _M0L6_2atmpS1929 = _M0Lm6offsetS312;
    int32_t _M0L6_2atmpS1934;
    int32_t _M0L6_2atmpS1930;
    int32_t _M0L6_2atmpS1933;
    int32_t _M0L6_2atmpS1932;
    int32_t _M0L6_2atmpS1931;
    _M0Lm6offsetS312 = _M0L6_2atmpS1929 - 1;
    _M0L6_2atmpS1934 = _M0Lm6offsetS312;
    _M0L6_2atmpS1930 = _M0L12digit__startS314 + _M0L6_2atmpS1934;
    _M0L6_2atmpS1933 = _M0Lm9remainingS325;
    _M0L6_2atmpS1932 = 48 + _M0L6_2atmpS1933;
    _M0L6_2atmpS1931 = (uint16_t)_M0L6_2atmpS1932;
    _M0L6bufferS323[_M0L6_2atmpS1930] = _M0L6_2atmpS1931;
    moonbit_decref(_M0L6bufferS323);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS305,
  uint32_t _M0L3numS299,
  int32_t _M0L12digit__startS297,
  int32_t _M0L10total__lenS296,
  int32_t _M0L5radixS301
) {
  int32_t _M0Lm6offsetS295;
  uint32_t _M0Lm1nS298;
  uint32_t _M0L4baseS300;
  int32_t _M0L6_2atmpS1861;
  int32_t _M0L6_2atmpS1860;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS295 = _M0L10total__lenS296 - _M0L12digit__startS297;
  _M0Lm1nS298 = _M0L3numS299;
  _M0L4baseS300 = *(uint32_t*)&_M0L5radixS301;
  _M0L6_2atmpS1861 = _M0L5radixS301 - 1;
  _M0L6_2atmpS1860 = _M0L5radixS301 & _M0L6_2atmpS1861;
  if (_M0L6_2atmpS1860 == 0) {
    int32_t _M0L5shiftS302;
    uint32_t _M0L4maskS303;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS302 = moonbit_ctz32(_M0L5radixS301);
    _M0L4maskS303 = _M0L4baseS300 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1862 = _M0Lm1nS298;
      if (_M0L6_2atmpS1862 > 0u) {
        int32_t _M0L6_2atmpS1863 = _M0Lm6offsetS295;
        uint32_t _M0L6_2atmpS1869;
        uint32_t _M0L6_2atmpS1868;
        int32_t _M0L5digitS304;
        int32_t _M0L6_2atmpS1866;
        int32_t _M0L6_2atmpS1864;
        int32_t _M0L6_2atmpS1865;
        uint32_t _M0L6_2atmpS1867;
        _M0Lm6offsetS295 = _M0L6_2atmpS1863 - 1;
        _M0L6_2atmpS1869 = _M0Lm1nS298;
        _M0L6_2atmpS1868 = _M0L6_2atmpS1869 & _M0L4maskS303;
        _M0L5digitS304 = *(int32_t*)&_M0L6_2atmpS1868;
        _M0L6_2atmpS1866 = _M0Lm6offsetS295;
        _M0L6_2atmpS1864 = _M0L12digit__startS297 + _M0L6_2atmpS1866;
        _M0L6_2atmpS1865
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS304
        ];
        _M0L6bufferS305[_M0L6_2atmpS1864] = _M0L6_2atmpS1865;
        _M0L6_2atmpS1867 = _M0Lm1nS298;
        _M0Lm1nS298 = _M0L6_2atmpS1867 >> (_M0L5shiftS302 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS305);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1870 = _M0Lm1nS298;
      if (_M0L6_2atmpS1870 > 0u) {
        int32_t _M0L6_2atmpS1871 = _M0Lm6offsetS295;
        uint32_t _M0L6_2atmpS1878;
        uint32_t _M0L1qS307;
        uint32_t _M0L6_2atmpS1876;
        uint32_t _M0L6_2atmpS1877;
        uint32_t _M0L6_2atmpS1875;
        int32_t _M0L5digitS308;
        int32_t _M0L6_2atmpS1874;
        int32_t _M0L6_2atmpS1872;
        int32_t _M0L6_2atmpS1873;
        _M0Lm6offsetS295 = _M0L6_2atmpS1871 - 1;
        _M0L6_2atmpS1878 = _M0Lm1nS298;
        _M0L1qS307 = _M0L6_2atmpS1878 / _M0L4baseS300;
        _M0L6_2atmpS1876 = _M0Lm1nS298;
        _M0L6_2atmpS1877 = _M0L1qS307 * _M0L4baseS300;
        _M0L6_2atmpS1875 = _M0L6_2atmpS1876 - _M0L6_2atmpS1877;
        _M0L5digitS308 = *(int32_t*)&_M0L6_2atmpS1875;
        _M0L6_2atmpS1874 = _M0Lm6offsetS295;
        _M0L6_2atmpS1872 = _M0L12digit__startS297 + _M0L6_2atmpS1874;
        _M0L6_2atmpS1873
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS308
        ];
        _M0L6bufferS305[_M0L6_2atmpS1872] = _M0L6_2atmpS1873;
        _M0Lm1nS298 = _M0L1qS307;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS305);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS292,
  uint32_t _M0L3numS288,
  int32_t _M0L12digit__startS286,
  int32_t _M0L10total__lenS285
) {
  int32_t _M0Lm6offsetS284;
  uint32_t _M0Lm1nS287;
  int32_t _M0L6_2atmpS1856;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS284 = _M0L10total__lenS285 - _M0L12digit__startS286;
  _M0Lm1nS287 = _M0L3numS288;
  while (1) {
    int32_t _M0L6_2atmpS1844 = _M0Lm6offsetS284;
    if (_M0L6_2atmpS1844 >= 2) {
      int32_t _M0L6_2atmpS1845 = _M0Lm6offsetS284;
      uint32_t _M0L6_2atmpS1855;
      uint32_t _M0L6_2atmpS1854;
      int32_t _M0L9byte__valS289;
      int32_t _M0L2hiS290;
      int32_t _M0L2loS291;
      int32_t _M0L6_2atmpS1848;
      int32_t _M0L6_2atmpS1846;
      int32_t _M0L6_2atmpS1847;
      int32_t _M0L6_2atmpS1852;
      int32_t _M0L6_2atmpS1851;
      int32_t _M0L6_2atmpS1849;
      int32_t _M0L6_2atmpS1850;
      uint32_t _M0L6_2atmpS1853;
      _M0Lm6offsetS284 = _M0L6_2atmpS1845 - 2;
      _M0L6_2atmpS1855 = _M0Lm1nS287;
      _M0L6_2atmpS1854 = _M0L6_2atmpS1855 & 255u;
      _M0L9byte__valS289 = *(int32_t*)&_M0L6_2atmpS1854;
      _M0L2hiS290 = _M0L9byte__valS289 / 16;
      _M0L2loS291 = _M0L9byte__valS289 % 16;
      _M0L6_2atmpS1848 = _M0Lm6offsetS284;
      _M0L6_2atmpS1846 = _M0L12digit__startS286 + _M0L6_2atmpS1848;
      _M0L6_2atmpS1847
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2hiS290
      ];
      _M0L6bufferS292[_M0L6_2atmpS1846] = _M0L6_2atmpS1847;
      _M0L6_2atmpS1852 = _M0Lm6offsetS284;
      _M0L6_2atmpS1851 = _M0L12digit__startS286 + _M0L6_2atmpS1852;
      _M0L6_2atmpS1849 = _M0L6_2atmpS1851 + 1;
      _M0L6_2atmpS1850
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2loS291
      ];
      _M0L6bufferS292[_M0L6_2atmpS1849] = _M0L6_2atmpS1850;
      _M0L6_2atmpS1853 = _M0Lm1nS287;
      _M0Lm1nS287 = _M0L6_2atmpS1853 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1856 = _M0Lm6offsetS284;
  if (_M0L6_2atmpS1856 == 1) {
    uint32_t _M0L6_2atmpS1859 = _M0Lm1nS287;
    uint32_t _M0L6_2atmpS1858 = _M0L6_2atmpS1859 & 15u;
    int32_t _M0L6nibbleS294 = *(int32_t*)&_M0L6_2atmpS1858;
    int32_t _M0L6_2atmpS1857 =
      ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L6nibbleS294];
    _M0L6bufferS292[_M0L12digit__startS286] = _M0L6_2atmpS1857;
    moonbit_decref(_M0L6bufferS292);
  } else {
    moonbit_decref(_M0L6bufferS292);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS277) {
  struct _M0TWEOs* _M0L7_2afuncS276;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS276 = _M0L4selfS277;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS276->code(_M0L7_2afuncS276);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS279
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS278;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS278 = _M0L4selfS279;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS278->code(_M0L7_2afuncS278);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS281) {
  struct _M0TWEOc* _M0L7_2afuncS280;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS280 = _M0L4selfS281;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS280->code(_M0L7_2afuncS280);
}

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS283
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2afuncS282;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS282 = _M0L4selfS283;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS282->code(_M0L7_2afuncS282);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS269
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS268;
  struct _M0TPB6Logger _M0L6_2atmpS1840;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS268 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS268);
  _M0L6_2atmpS1840
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS268
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS269, _M0L6_2atmpS1840);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS268);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS271
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS270;
  struct _M0TPB6Logger _M0L6_2atmpS1841;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS270 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS270);
  _M0L6_2atmpS1841
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS270
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS271, _M0L6_2atmpS1841);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS270);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS273
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS272;
  struct _M0TPB6Logger _M0L6_2atmpS1842;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS272 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS272);
  _M0L6_2atmpS1842
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS272
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS273, _M0L6_2atmpS1842);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS272);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS275
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS274;
  struct _M0TPB6Logger _M0L6_2atmpS1843;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS274 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS274);
  _M0L6_2atmpS1843
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS274
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS275, _M0L6_2atmpS1843);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS274);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS267
) {
  int32_t _M0L8_2afieldS3736;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3736 = _M0L4selfS267.$1;
  moonbit_decref(_M0L4selfS267.$0);
  return _M0L8_2afieldS3736;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS266
) {
  int32_t _M0L3endS1838;
  int32_t _M0L8_2afieldS3737;
  int32_t _M0L5startS1839;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1838 = _M0L4selfS266.$2;
  _M0L8_2afieldS3737 = _M0L4selfS266.$1;
  moonbit_decref(_M0L4selfS266.$0);
  _M0L5startS1839 = _M0L8_2afieldS3737;
  return _M0L3endS1838 - _M0L5startS1839;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS265
) {
  moonbit_string_t _M0L8_2afieldS3738;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3738 = _M0L4selfS265.$0;
  return _M0L8_2afieldS3738;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS261,
  moonbit_string_t _M0L5valueS262,
  int32_t _M0L5startS263,
  int32_t _M0L3lenS264
) {
  int32_t _M0L6_2atmpS1837;
  int64_t _M0L6_2atmpS1836;
  struct _M0TPC16string10StringView _M0L6_2atmpS1835;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1837 = _M0L5startS263 + _M0L3lenS264;
  _M0L6_2atmpS1836 = (int64_t)_M0L6_2atmpS1837;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1835
  = _M0MPC16string6String11sub_2einner(_M0L5valueS262, _M0L5startS263, _M0L6_2atmpS1836);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS261, _M0L6_2atmpS1835);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS254,
  int32_t _M0L5startS260,
  int64_t _M0L3endS256
) {
  int32_t _M0L3lenS253;
  int32_t _M0L3endS255;
  int32_t _M0L5startS259;
  int32_t _if__result_4078;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS253 = Moonbit_array_length(_M0L4selfS254);
  if (_M0L3endS256 == 4294967296ll) {
    _M0L3endS255 = _M0L3lenS253;
  } else {
    int64_t _M0L7_2aSomeS257 = _M0L3endS256;
    int32_t _M0L6_2aendS258 = (int32_t)_M0L7_2aSomeS257;
    if (_M0L6_2aendS258 < 0) {
      _M0L3endS255 = _M0L3lenS253 + _M0L6_2aendS258;
    } else {
      _M0L3endS255 = _M0L6_2aendS258;
    }
  }
  if (_M0L5startS260 < 0) {
    _M0L5startS259 = _M0L3lenS253 + _M0L5startS260;
  } else {
    _M0L5startS259 = _M0L5startS260;
  }
  if (_M0L5startS259 >= 0) {
    if (_M0L5startS259 <= _M0L3endS255) {
      _if__result_4078 = _M0L3endS255 <= _M0L3lenS253;
    } else {
      _if__result_4078 = 0;
    }
  } else {
    _if__result_4078 = 0;
  }
  if (_if__result_4078) {
    if (_M0L5startS259 < _M0L3lenS253) {
      int32_t _M0L6_2atmpS1832 = _M0L4selfS254[_M0L5startS259];
      int32_t _M0L6_2atmpS1831;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1831
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1832);
      if (!_M0L6_2atmpS1831) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS255 < _M0L3lenS253) {
      int32_t _M0L6_2atmpS1834 = _M0L4selfS254[_M0L3endS255];
      int32_t _M0L6_2atmpS1833;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1833
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1834);
      if (!_M0L6_2atmpS1833) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS259,
                                                 _M0L3endS255,
                                                 _M0L4selfS254};
  } else {
    moonbit_decref(_M0L4selfS254);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS250) {
  struct _M0TPB6Hasher* _M0L1hS249;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS249 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS249);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS249, _M0L4selfS250);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS249);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS252
) {
  struct _M0TPB6Hasher* _M0L1hS251;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS251 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS251);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS251, _M0L4selfS252);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS251);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS247) {
  int32_t _M0L4seedS246;
  if (_M0L10seed_2eoptS247 == 4294967296ll) {
    _M0L4seedS246 = 0;
  } else {
    int64_t _M0L7_2aSomeS248 = _M0L10seed_2eoptS247;
    _M0L4seedS246 = (int32_t)_M0L7_2aSomeS248;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS246);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS245) {
  uint32_t _M0L6_2atmpS1830;
  uint32_t _M0L6_2atmpS1829;
  struct _M0TPB6Hasher* _block_4079;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1830 = *(uint32_t*)&_M0L4seedS245;
  _M0L6_2atmpS1829 = _M0L6_2atmpS1830 + 374761393u;
  _block_4079
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4079)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4079->$0 = _M0L6_2atmpS1829;
  return _block_4079;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS244) {
  uint32_t _M0L6_2atmpS1828;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1828 = _M0MPB6Hasher9avalanche(_M0L4selfS244);
  return *(int32_t*)&_M0L6_2atmpS1828;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS243) {
  uint32_t _M0L8_2afieldS3739;
  uint32_t _M0Lm3accS242;
  uint32_t _M0L6_2atmpS1817;
  uint32_t _M0L6_2atmpS1819;
  uint32_t _M0L6_2atmpS1818;
  uint32_t _M0L6_2atmpS1820;
  uint32_t _M0L6_2atmpS1821;
  uint32_t _M0L6_2atmpS1823;
  uint32_t _M0L6_2atmpS1822;
  uint32_t _M0L6_2atmpS1824;
  uint32_t _M0L6_2atmpS1825;
  uint32_t _M0L6_2atmpS1827;
  uint32_t _M0L6_2atmpS1826;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3739 = _M0L4selfS243->$0;
  moonbit_decref(_M0L4selfS243);
  _M0Lm3accS242 = _M0L8_2afieldS3739;
  _M0L6_2atmpS1817 = _M0Lm3accS242;
  _M0L6_2atmpS1819 = _M0Lm3accS242;
  _M0L6_2atmpS1818 = _M0L6_2atmpS1819 >> 15;
  _M0Lm3accS242 = _M0L6_2atmpS1817 ^ _M0L6_2atmpS1818;
  _M0L6_2atmpS1820 = _M0Lm3accS242;
  _M0Lm3accS242 = _M0L6_2atmpS1820 * 2246822519u;
  _M0L6_2atmpS1821 = _M0Lm3accS242;
  _M0L6_2atmpS1823 = _M0Lm3accS242;
  _M0L6_2atmpS1822 = _M0L6_2atmpS1823 >> 13;
  _M0Lm3accS242 = _M0L6_2atmpS1821 ^ _M0L6_2atmpS1822;
  _M0L6_2atmpS1824 = _M0Lm3accS242;
  _M0Lm3accS242 = _M0L6_2atmpS1824 * 3266489917u;
  _M0L6_2atmpS1825 = _M0Lm3accS242;
  _M0L6_2atmpS1827 = _M0Lm3accS242;
  _M0L6_2atmpS1826 = _M0L6_2atmpS1827 >> 16;
  _M0Lm3accS242 = _M0L6_2atmpS1825 ^ _M0L6_2atmpS1826;
  return _M0Lm3accS242;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS240,
  moonbit_string_t _M0L1yS241
) {
  int32_t _M0L6_2atmpS3740;
  int32_t _M0L6_2atmpS1816;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3740 = moonbit_val_array_equal(_M0L1xS240, _M0L1yS241);
  moonbit_decref(_M0L1xS240);
  moonbit_decref(_M0L1yS241);
  _M0L6_2atmpS1816 = _M0L6_2atmpS3740;
  return !_M0L6_2atmpS1816;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS237,
  int32_t _M0L5valueS236
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS236, _M0L4selfS237);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS239,
  moonbit_string_t _M0L5valueS238
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS238, _M0L4selfS239);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS235) {
  int64_t _M0L6_2atmpS1815;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1815 = (int64_t)_M0L4selfS235;
  return *(uint64_t*)&_M0L6_2atmpS1815;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS233,
  moonbit_string_t _M0L4reprS234
) {
  void* _block_4080;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4080 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_4080)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_4080)->$0 = _M0L6numberS233;
  ((struct _M0DTPB4Json6Number*)_block_4080)->$1 = _M0L4reprS234;
  return _block_4080;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS231,
  int32_t _M0L5valueS232
) {
  uint32_t _M0L6_2atmpS1814;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1814 = *(uint32_t*)&_M0L5valueS232;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS231, _M0L6_2atmpS1814);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS224
) {
  struct _M0TPB13StringBuilder* _M0L3bufS222;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS223;
  int32_t _M0L7_2abindS225;
  int32_t _M0L1iS226;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS222 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS223 = _M0L4selfS224;
  moonbit_incref(_M0L3bufS222);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS222, 91);
  _M0L7_2abindS225 = _M0L7_2aselfS223->$1;
  _M0L1iS226 = 0;
  while (1) {
    if (_M0L1iS226 < _M0L7_2abindS225) {
      int32_t _if__result_4082;
      moonbit_string_t* _M0L8_2afieldS3742;
      moonbit_string_t* _M0L3bufS1812;
      moonbit_string_t _M0L6_2atmpS3741;
      moonbit_string_t _M0L4itemS227;
      int32_t _M0L6_2atmpS1813;
      if (_M0L1iS226 != 0) {
        moonbit_incref(_M0L3bufS222);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS222, (moonbit_string_t)moonbit_string_literal_92.data);
      }
      if (_M0L1iS226 < 0) {
        _if__result_4082 = 1;
      } else {
        int32_t _M0L3lenS1811 = _M0L7_2aselfS223->$1;
        _if__result_4082 = _M0L1iS226 >= _M0L3lenS1811;
      }
      if (_if__result_4082) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3742 = _M0L7_2aselfS223->$0;
      _M0L3bufS1812 = _M0L8_2afieldS3742;
      _M0L6_2atmpS3741 = (moonbit_string_t)_M0L3bufS1812[_M0L1iS226];
      _M0L4itemS227 = _M0L6_2atmpS3741;
      if (_M0L4itemS227 == 0) {
        moonbit_incref(_M0L3bufS222);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS222, (moonbit_string_t)moonbit_string_literal_53.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS228 = _M0L4itemS227;
        moonbit_string_t _M0L6_2alocS229 = _M0L7_2aSomeS228;
        moonbit_string_t _M0L6_2atmpS1810;
        moonbit_incref(_M0L6_2alocS229);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1810
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS229);
        moonbit_incref(_M0L3bufS222);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS222, _M0L6_2atmpS1810);
      }
      _M0L6_2atmpS1813 = _M0L1iS226 + 1;
      _M0L1iS226 = _M0L6_2atmpS1813;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS223);
    }
    break;
  }
  moonbit_incref(_M0L3bufS222);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS222, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS222);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS221
) {
  moonbit_string_t _M0L6_2atmpS1809;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1808;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1809 = _M0L4selfS221;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1808 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1809);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1808);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS220
) {
  struct _M0TPB13StringBuilder* _M0L2sbS219;
  struct _M0TPC16string10StringView _M0L8_2afieldS3755;
  struct _M0TPC16string10StringView _M0L3pkgS1793;
  moonbit_string_t _M0L6_2atmpS1792;
  moonbit_string_t _M0L6_2atmpS3754;
  moonbit_string_t _M0L6_2atmpS1791;
  moonbit_string_t _M0L6_2atmpS3753;
  moonbit_string_t _M0L6_2atmpS1790;
  struct _M0TPC16string10StringView _M0L8_2afieldS3752;
  struct _M0TPC16string10StringView _M0L8filenameS1794;
  struct _M0TPC16string10StringView _M0L8_2afieldS3751;
  struct _M0TPC16string10StringView _M0L11start__lineS1797;
  moonbit_string_t _M0L6_2atmpS1796;
  moonbit_string_t _M0L6_2atmpS3750;
  moonbit_string_t _M0L6_2atmpS1795;
  struct _M0TPC16string10StringView _M0L8_2afieldS3749;
  struct _M0TPC16string10StringView _M0L13start__columnS1800;
  moonbit_string_t _M0L6_2atmpS1799;
  moonbit_string_t _M0L6_2atmpS3748;
  moonbit_string_t _M0L6_2atmpS1798;
  struct _M0TPC16string10StringView _M0L8_2afieldS3747;
  struct _M0TPC16string10StringView _M0L9end__lineS1803;
  moonbit_string_t _M0L6_2atmpS1802;
  moonbit_string_t _M0L6_2atmpS3746;
  moonbit_string_t _M0L6_2atmpS1801;
  struct _M0TPC16string10StringView _M0L8_2afieldS3745;
  int32_t _M0L6_2acntS3891;
  struct _M0TPC16string10StringView _M0L11end__columnS1807;
  moonbit_string_t _M0L6_2atmpS1806;
  moonbit_string_t _M0L6_2atmpS3744;
  moonbit_string_t _M0L6_2atmpS1805;
  moonbit_string_t _M0L6_2atmpS3743;
  moonbit_string_t _M0L6_2atmpS1804;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS219 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3755
  = (struct _M0TPC16string10StringView){
    _M0L4selfS220->$0_1, _M0L4selfS220->$0_2, _M0L4selfS220->$0_0
  };
  _M0L3pkgS1793 = _M0L8_2afieldS3755;
  moonbit_incref(_M0L3pkgS1793.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1792
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1793);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3754
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_93.data, _M0L6_2atmpS1792);
  moonbit_decref(_M0L6_2atmpS1792);
  _M0L6_2atmpS1791 = _M0L6_2atmpS3754;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3753
  = moonbit_add_string(_M0L6_2atmpS1791, (moonbit_string_t)moonbit_string_literal_94.data);
  moonbit_decref(_M0L6_2atmpS1791);
  _M0L6_2atmpS1790 = _M0L6_2atmpS3753;
  moonbit_incref(_M0L2sbS219);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS219, _M0L6_2atmpS1790);
  moonbit_incref(_M0L2sbS219);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS219, (moonbit_string_t)moonbit_string_literal_95.data);
  _M0L8_2afieldS3752
  = (struct _M0TPC16string10StringView){
    _M0L4selfS220->$1_1, _M0L4selfS220->$1_2, _M0L4selfS220->$1_0
  };
  _M0L8filenameS1794 = _M0L8_2afieldS3752;
  moonbit_incref(_M0L8filenameS1794.$0);
  moonbit_incref(_M0L2sbS219);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS219, _M0L8filenameS1794);
  _M0L8_2afieldS3751
  = (struct _M0TPC16string10StringView){
    _M0L4selfS220->$2_1, _M0L4selfS220->$2_2, _M0L4selfS220->$2_0
  };
  _M0L11start__lineS1797 = _M0L8_2afieldS3751;
  moonbit_incref(_M0L11start__lineS1797.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1796
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1797);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3750
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_96.data, _M0L6_2atmpS1796);
  moonbit_decref(_M0L6_2atmpS1796);
  _M0L6_2atmpS1795 = _M0L6_2atmpS3750;
  moonbit_incref(_M0L2sbS219);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS219, _M0L6_2atmpS1795);
  _M0L8_2afieldS3749
  = (struct _M0TPC16string10StringView){
    _M0L4selfS220->$3_1, _M0L4selfS220->$3_2, _M0L4selfS220->$3_0
  };
  _M0L13start__columnS1800 = _M0L8_2afieldS3749;
  moonbit_incref(_M0L13start__columnS1800.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1799
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1800);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3748
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_97.data, _M0L6_2atmpS1799);
  moonbit_decref(_M0L6_2atmpS1799);
  _M0L6_2atmpS1798 = _M0L6_2atmpS3748;
  moonbit_incref(_M0L2sbS219);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS219, _M0L6_2atmpS1798);
  _M0L8_2afieldS3747
  = (struct _M0TPC16string10StringView){
    _M0L4selfS220->$4_1, _M0L4selfS220->$4_2, _M0L4selfS220->$4_0
  };
  _M0L9end__lineS1803 = _M0L8_2afieldS3747;
  moonbit_incref(_M0L9end__lineS1803.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1802
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1803);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3746
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_98.data, _M0L6_2atmpS1802);
  moonbit_decref(_M0L6_2atmpS1802);
  _M0L6_2atmpS1801 = _M0L6_2atmpS3746;
  moonbit_incref(_M0L2sbS219);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS219, _M0L6_2atmpS1801);
  _M0L8_2afieldS3745
  = (struct _M0TPC16string10StringView){
    _M0L4selfS220->$5_1, _M0L4selfS220->$5_2, _M0L4selfS220->$5_0
  };
  _M0L6_2acntS3891 = Moonbit_object_header(_M0L4selfS220)->rc;
  if (_M0L6_2acntS3891 > 1) {
    int32_t _M0L11_2anew__cntS3897 = _M0L6_2acntS3891 - 1;
    Moonbit_object_header(_M0L4selfS220)->rc = _M0L11_2anew__cntS3897;
    moonbit_incref(_M0L8_2afieldS3745.$0);
  } else if (_M0L6_2acntS3891 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3896 =
      (struct _M0TPC16string10StringView){_M0L4selfS220->$4_1,
                                            _M0L4selfS220->$4_2,
                                            _M0L4selfS220->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3895;
    struct _M0TPC16string10StringView _M0L8_2afieldS3894;
    struct _M0TPC16string10StringView _M0L8_2afieldS3893;
    struct _M0TPC16string10StringView _M0L8_2afieldS3892;
    moonbit_decref(_M0L8_2afieldS3896.$0);
    _M0L8_2afieldS3895
    = (struct _M0TPC16string10StringView){
      _M0L4selfS220->$3_1, _M0L4selfS220->$3_2, _M0L4selfS220->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3895.$0);
    _M0L8_2afieldS3894
    = (struct _M0TPC16string10StringView){
      _M0L4selfS220->$2_1, _M0L4selfS220->$2_2, _M0L4selfS220->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3894.$0);
    _M0L8_2afieldS3893
    = (struct _M0TPC16string10StringView){
      _M0L4selfS220->$1_1, _M0L4selfS220->$1_2, _M0L4selfS220->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3893.$0);
    _M0L8_2afieldS3892
    = (struct _M0TPC16string10StringView){
      _M0L4selfS220->$0_1, _M0L4selfS220->$0_2, _M0L4selfS220->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3892.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS220);
  }
  _M0L11end__columnS1807 = _M0L8_2afieldS3745;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1806
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1807);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3744
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_99.data, _M0L6_2atmpS1806);
  moonbit_decref(_M0L6_2atmpS1806);
  _M0L6_2atmpS1805 = _M0L6_2atmpS3744;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3743
  = moonbit_add_string(_M0L6_2atmpS1805, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1805);
  _M0L6_2atmpS1804 = _M0L6_2atmpS3743;
  moonbit_incref(_M0L2sbS219);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS219, _M0L6_2atmpS1804);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS219);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS217,
  moonbit_string_t _M0L3strS218
) {
  int32_t _M0L3lenS1780;
  int32_t _M0L6_2atmpS1782;
  int32_t _M0L6_2atmpS1781;
  int32_t _M0L6_2atmpS1779;
  moonbit_bytes_t _M0L8_2afieldS3757;
  moonbit_bytes_t _M0L4dataS1783;
  int32_t _M0L3lenS1784;
  int32_t _M0L6_2atmpS1785;
  int32_t _M0L3lenS1787;
  int32_t _M0L6_2atmpS3756;
  int32_t _M0L6_2atmpS1789;
  int32_t _M0L6_2atmpS1788;
  int32_t _M0L6_2atmpS1786;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1780 = _M0L4selfS217->$1;
  _M0L6_2atmpS1782 = Moonbit_array_length(_M0L3strS218);
  _M0L6_2atmpS1781 = _M0L6_2atmpS1782 * 2;
  _M0L6_2atmpS1779 = _M0L3lenS1780 + _M0L6_2atmpS1781;
  moonbit_incref(_M0L4selfS217);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS217, _M0L6_2atmpS1779);
  _M0L8_2afieldS3757 = _M0L4selfS217->$0;
  _M0L4dataS1783 = _M0L8_2afieldS3757;
  _M0L3lenS1784 = _M0L4selfS217->$1;
  _M0L6_2atmpS1785 = Moonbit_array_length(_M0L3strS218);
  moonbit_incref(_M0L4dataS1783);
  moonbit_incref(_M0L3strS218);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1783, _M0L3lenS1784, _M0L3strS218, 0, _M0L6_2atmpS1785);
  _M0L3lenS1787 = _M0L4selfS217->$1;
  _M0L6_2atmpS3756 = Moonbit_array_length(_M0L3strS218);
  moonbit_decref(_M0L3strS218);
  _M0L6_2atmpS1789 = _M0L6_2atmpS3756;
  _M0L6_2atmpS1788 = _M0L6_2atmpS1789 * 2;
  _M0L6_2atmpS1786 = _M0L3lenS1787 + _M0L6_2atmpS1788;
  _M0L4selfS217->$1 = _M0L6_2atmpS1786;
  moonbit_decref(_M0L4selfS217);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS209,
  int32_t _M0L13bytes__offsetS204,
  moonbit_string_t _M0L3strS211,
  int32_t _M0L11str__offsetS207,
  int32_t _M0L6lengthS205
) {
  int32_t _M0L6_2atmpS1778;
  int32_t _M0L6_2atmpS1777;
  int32_t _M0L2e1S203;
  int32_t _M0L6_2atmpS1776;
  int32_t _M0L2e2S206;
  int32_t _M0L4len1S208;
  int32_t _M0L4len2S210;
  int32_t _if__result_4083;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1778 = _M0L6lengthS205 * 2;
  _M0L6_2atmpS1777 = _M0L13bytes__offsetS204 + _M0L6_2atmpS1778;
  _M0L2e1S203 = _M0L6_2atmpS1777 - 1;
  _M0L6_2atmpS1776 = _M0L11str__offsetS207 + _M0L6lengthS205;
  _M0L2e2S206 = _M0L6_2atmpS1776 - 1;
  _M0L4len1S208 = Moonbit_array_length(_M0L4selfS209);
  _M0L4len2S210 = Moonbit_array_length(_M0L3strS211);
  if (_M0L6lengthS205 >= 0) {
    if (_M0L13bytes__offsetS204 >= 0) {
      if (_M0L2e1S203 < _M0L4len1S208) {
        if (_M0L11str__offsetS207 >= 0) {
          _if__result_4083 = _M0L2e2S206 < _M0L4len2S210;
        } else {
          _if__result_4083 = 0;
        }
      } else {
        _if__result_4083 = 0;
      }
    } else {
      _if__result_4083 = 0;
    }
  } else {
    _if__result_4083 = 0;
  }
  if (_if__result_4083) {
    int32_t _M0L16end__str__offsetS212 =
      _M0L11str__offsetS207 + _M0L6lengthS205;
    int32_t _M0L1iS213 = _M0L11str__offsetS207;
    int32_t _M0L1jS214 = _M0L13bytes__offsetS204;
    while (1) {
      if (_M0L1iS213 < _M0L16end__str__offsetS212) {
        int32_t _M0L6_2atmpS1773 = _M0L3strS211[_M0L1iS213];
        int32_t _M0L6_2atmpS1772 = (int32_t)_M0L6_2atmpS1773;
        uint32_t _M0L1cS215 = *(uint32_t*)&_M0L6_2atmpS1772;
        uint32_t _M0L6_2atmpS1768 = _M0L1cS215 & 255u;
        int32_t _M0L6_2atmpS1767;
        int32_t _M0L6_2atmpS1769;
        uint32_t _M0L6_2atmpS1771;
        int32_t _M0L6_2atmpS1770;
        int32_t _M0L6_2atmpS1774;
        int32_t _M0L6_2atmpS1775;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1767 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1768);
        if (
          _M0L1jS214 < 0 || _M0L1jS214 >= Moonbit_array_length(_M0L4selfS209)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS209[_M0L1jS214] = _M0L6_2atmpS1767;
        _M0L6_2atmpS1769 = _M0L1jS214 + 1;
        _M0L6_2atmpS1771 = _M0L1cS215 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1770 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1771);
        if (
          _M0L6_2atmpS1769 < 0
          || _M0L6_2atmpS1769 >= Moonbit_array_length(_M0L4selfS209)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS209[_M0L6_2atmpS1769] = _M0L6_2atmpS1770;
        _M0L6_2atmpS1774 = _M0L1iS213 + 1;
        _M0L6_2atmpS1775 = _M0L1jS214 + 2;
        _M0L1iS213 = _M0L6_2atmpS1774;
        _M0L1jS214 = _M0L6_2atmpS1775;
        continue;
      } else {
        moonbit_decref(_M0L3strS211);
        moonbit_decref(_M0L4selfS209);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS211);
    moonbit_decref(_M0L4selfS209);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS200,
  double _M0L3objS199
) {
  struct _M0TPB6Logger _M0L6_2atmpS1765;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1765
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS200
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS199, _M0L6_2atmpS1765);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS202,
  struct _M0TPC16string10StringView _M0L3objS201
) {
  struct _M0TPB6Logger _M0L6_2atmpS1766;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1766
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS202
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS201, _M0L6_2atmpS1766);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS145
) {
  int32_t _M0L6_2atmpS1764;
  struct _M0TPC16string10StringView _M0L7_2abindS144;
  moonbit_string_t _M0L7_2adataS146;
  int32_t _M0L8_2astartS147;
  int32_t _M0L6_2atmpS1763;
  int32_t _M0L6_2aendS148;
  int32_t _M0Lm9_2acursorS149;
  int32_t _M0Lm13accept__stateS150;
  int32_t _M0Lm10match__endS151;
  int32_t _M0Lm20match__tag__saver__0S152;
  int32_t _M0Lm20match__tag__saver__1S153;
  int32_t _M0Lm20match__tag__saver__2S154;
  int32_t _M0Lm20match__tag__saver__3S155;
  int32_t _M0Lm20match__tag__saver__4S156;
  int32_t _M0Lm6tag__0S157;
  int32_t _M0Lm6tag__1S158;
  int32_t _M0Lm9tag__1__1S159;
  int32_t _M0Lm9tag__1__2S160;
  int32_t _M0Lm6tag__3S161;
  int32_t _M0Lm6tag__2S162;
  int32_t _M0Lm9tag__2__1S163;
  int32_t _M0Lm6tag__4S164;
  int32_t _M0L6_2atmpS1721;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1764 = Moonbit_array_length(_M0L4reprS145);
  _M0L7_2abindS144
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1764, _M0L4reprS145
  };
  moonbit_incref(_M0L7_2abindS144.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS146 = _M0MPC16string10StringView4data(_M0L7_2abindS144);
  moonbit_incref(_M0L7_2abindS144.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS147
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS144);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1763 = _M0MPC16string10StringView6length(_M0L7_2abindS144);
  _M0L6_2aendS148 = _M0L8_2astartS147 + _M0L6_2atmpS1763;
  _M0Lm9_2acursorS149 = _M0L8_2astartS147;
  _M0Lm13accept__stateS150 = -1;
  _M0Lm10match__endS151 = -1;
  _M0Lm20match__tag__saver__0S152 = -1;
  _M0Lm20match__tag__saver__1S153 = -1;
  _M0Lm20match__tag__saver__2S154 = -1;
  _M0Lm20match__tag__saver__3S155 = -1;
  _M0Lm20match__tag__saver__4S156 = -1;
  _M0Lm6tag__0S157 = -1;
  _M0Lm6tag__1S158 = -1;
  _M0Lm9tag__1__1S159 = -1;
  _M0Lm9tag__1__2S160 = -1;
  _M0Lm6tag__3S161 = -1;
  _M0Lm6tag__2S162 = -1;
  _M0Lm9tag__2__1S163 = -1;
  _M0Lm6tag__4S164 = -1;
  _M0L6_2atmpS1721 = _M0Lm9_2acursorS149;
  if (_M0L6_2atmpS1721 < _M0L6_2aendS148) {
    int32_t _M0L6_2atmpS1723 = _M0Lm9_2acursorS149;
    int32_t _M0L6_2atmpS1722;
    moonbit_incref(_M0L7_2adataS146);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1722
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1723);
    if (_M0L6_2atmpS1722 == 64) {
      int32_t _M0L6_2atmpS1724 = _M0Lm9_2acursorS149;
      _M0Lm9_2acursorS149 = _M0L6_2atmpS1724 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1725;
        _M0Lm6tag__0S157 = _M0Lm9_2acursorS149;
        _M0L6_2atmpS1725 = _M0Lm9_2acursorS149;
        if (_M0L6_2atmpS1725 < _M0L6_2aendS148) {
          int32_t _M0L6_2atmpS1762 = _M0Lm9_2acursorS149;
          int32_t _M0L10next__charS172;
          int32_t _M0L6_2atmpS1726;
          moonbit_incref(_M0L7_2adataS146);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS172
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1762);
          _M0L6_2atmpS1726 = _M0Lm9_2acursorS149;
          _M0Lm9_2acursorS149 = _M0L6_2atmpS1726 + 1;
          if (_M0L10next__charS172 == 58) {
            int32_t _M0L6_2atmpS1727 = _M0Lm9_2acursorS149;
            if (_M0L6_2atmpS1727 < _M0L6_2aendS148) {
              int32_t _M0L6_2atmpS1728 = _M0Lm9_2acursorS149;
              int32_t _M0L12dispatch__15S173;
              _M0Lm9_2acursorS149 = _M0L6_2atmpS1728 + 1;
              _M0L12dispatch__15S173 = 0;
              loop__label__15_176:;
              while (1) {
                int32_t _M0L6_2atmpS1729;
                switch (_M0L12dispatch__15S173) {
                  case 3: {
                    int32_t _M0L6_2atmpS1732;
                    _M0Lm9tag__1__2S160 = _M0Lm9tag__1__1S159;
                    _M0Lm9tag__1__1S159 = _M0Lm6tag__1S158;
                    _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                    _M0L6_2atmpS1732 = _M0Lm9_2acursorS149;
                    if (_M0L6_2atmpS1732 < _M0L6_2aendS148) {
                      int32_t _M0L6_2atmpS1737 = _M0Lm9_2acursorS149;
                      int32_t _M0L10next__charS180;
                      int32_t _M0L6_2atmpS1733;
                      moonbit_incref(_M0L7_2adataS146);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS180
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1737);
                      _M0L6_2atmpS1733 = _M0Lm9_2acursorS149;
                      _M0Lm9_2acursorS149 = _M0L6_2atmpS1733 + 1;
                      if (_M0L10next__charS180 < 58) {
                        if (_M0L10next__charS180 < 48) {
                          goto join_179;
                        } else {
                          int32_t _M0L6_2atmpS1734;
                          _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                          _M0Lm9tag__2__1S163 = _M0Lm6tag__2S162;
                          _M0Lm6tag__2S162 = _M0Lm9_2acursorS149;
                          _M0Lm6tag__3S161 = _M0Lm9_2acursorS149;
                          _M0L6_2atmpS1734 = _M0Lm9_2acursorS149;
                          if (_M0L6_2atmpS1734 < _M0L6_2aendS148) {
                            int32_t _M0L6_2atmpS1736 = _M0Lm9_2acursorS149;
                            int32_t _M0L10next__charS182;
                            int32_t _M0L6_2atmpS1735;
                            moonbit_incref(_M0L7_2adataS146);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS182
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1736);
                            _M0L6_2atmpS1735 = _M0Lm9_2acursorS149;
                            _M0Lm9_2acursorS149 = _M0L6_2atmpS1735 + 1;
                            if (_M0L10next__charS182 < 48) {
                              if (_M0L10next__charS182 == 45) {
                                goto join_174;
                              } else {
                                goto join_181;
                              }
                            } else if (_M0L10next__charS182 > 57) {
                              if (_M0L10next__charS182 < 59) {
                                _M0L12dispatch__15S173 = 3;
                                goto loop__label__15_176;
                              } else {
                                goto join_181;
                              }
                            } else {
                              _M0L12dispatch__15S173 = 6;
                              goto loop__label__15_176;
                            }
                            join_181:;
                            _M0L12dispatch__15S173 = 0;
                            goto loop__label__15_176;
                          } else {
                            goto join_165;
                          }
                        }
                      } else if (_M0L10next__charS180 > 58) {
                        goto join_179;
                      } else {
                        _M0L12dispatch__15S173 = 1;
                        goto loop__label__15_176;
                      }
                      join_179:;
                      _M0L12dispatch__15S173 = 0;
                      goto loop__label__15_176;
                    } else {
                      goto join_165;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1738;
                    _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                    _M0Lm6tag__2S162 = _M0Lm9_2acursorS149;
                    _M0L6_2atmpS1738 = _M0Lm9_2acursorS149;
                    if (_M0L6_2atmpS1738 < _M0L6_2aendS148) {
                      int32_t _M0L6_2atmpS1740 = _M0Lm9_2acursorS149;
                      int32_t _M0L10next__charS184;
                      int32_t _M0L6_2atmpS1739;
                      moonbit_incref(_M0L7_2adataS146);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS184
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1740);
                      _M0L6_2atmpS1739 = _M0Lm9_2acursorS149;
                      _M0Lm9_2acursorS149 = _M0L6_2atmpS1739 + 1;
                      if (_M0L10next__charS184 < 58) {
                        if (_M0L10next__charS184 < 48) {
                          goto join_183;
                        } else {
                          _M0L12dispatch__15S173 = 2;
                          goto loop__label__15_176;
                        }
                      } else if (_M0L10next__charS184 > 58) {
                        goto join_183;
                      } else {
                        _M0L12dispatch__15S173 = 3;
                        goto loop__label__15_176;
                      }
                      join_183:;
                      _M0L12dispatch__15S173 = 0;
                      goto loop__label__15_176;
                    } else {
                      goto join_165;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1741;
                    _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                    _M0L6_2atmpS1741 = _M0Lm9_2acursorS149;
                    if (_M0L6_2atmpS1741 < _M0L6_2aendS148) {
                      int32_t _M0L6_2atmpS1743 = _M0Lm9_2acursorS149;
                      int32_t _M0L10next__charS185;
                      int32_t _M0L6_2atmpS1742;
                      moonbit_incref(_M0L7_2adataS146);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS185
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1743);
                      _M0L6_2atmpS1742 = _M0Lm9_2acursorS149;
                      _M0Lm9_2acursorS149 = _M0L6_2atmpS1742 + 1;
                      if (_M0L10next__charS185 == 58) {
                        _M0L12dispatch__15S173 = 1;
                        goto loop__label__15_176;
                      } else {
                        _M0L12dispatch__15S173 = 0;
                        goto loop__label__15_176;
                      }
                    } else {
                      goto join_165;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1744;
                    _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                    _M0Lm6tag__4S164 = _M0Lm9_2acursorS149;
                    _M0L6_2atmpS1744 = _M0Lm9_2acursorS149;
                    if (_M0L6_2atmpS1744 < _M0L6_2aendS148) {
                      int32_t _M0L6_2atmpS1752 = _M0Lm9_2acursorS149;
                      int32_t _M0L10next__charS187;
                      int32_t _M0L6_2atmpS1745;
                      moonbit_incref(_M0L7_2adataS146);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS187
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1752);
                      _M0L6_2atmpS1745 = _M0Lm9_2acursorS149;
                      _M0Lm9_2acursorS149 = _M0L6_2atmpS1745 + 1;
                      if (_M0L10next__charS187 < 58) {
                        if (_M0L10next__charS187 < 48) {
                          goto join_186;
                        } else {
                          _M0L12dispatch__15S173 = 4;
                          goto loop__label__15_176;
                        }
                      } else if (_M0L10next__charS187 > 58) {
                        goto join_186;
                      } else {
                        int32_t _M0L6_2atmpS1746;
                        _M0Lm9tag__1__2S160 = _M0Lm9tag__1__1S159;
                        _M0Lm9tag__1__1S159 = _M0Lm6tag__1S158;
                        _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                        _M0L6_2atmpS1746 = _M0Lm9_2acursorS149;
                        if (_M0L6_2atmpS1746 < _M0L6_2aendS148) {
                          int32_t _M0L6_2atmpS1751 = _M0Lm9_2acursorS149;
                          int32_t _M0L10next__charS189;
                          int32_t _M0L6_2atmpS1747;
                          moonbit_incref(_M0L7_2adataS146);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS189
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1751);
                          _M0L6_2atmpS1747 = _M0Lm9_2acursorS149;
                          _M0Lm9_2acursorS149 = _M0L6_2atmpS1747 + 1;
                          if (_M0L10next__charS189 < 58) {
                            if (_M0L10next__charS189 < 48) {
                              goto join_188;
                            } else {
                              int32_t _M0L6_2atmpS1748;
                              _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                              _M0Lm9tag__2__1S163 = _M0Lm6tag__2S162;
                              _M0Lm6tag__2S162 = _M0Lm9_2acursorS149;
                              _M0L6_2atmpS1748 = _M0Lm9_2acursorS149;
                              if (_M0L6_2atmpS1748 < _M0L6_2aendS148) {
                                int32_t _M0L6_2atmpS1750 =
                                  _M0Lm9_2acursorS149;
                                int32_t _M0L10next__charS191;
                                int32_t _M0L6_2atmpS1749;
                                moonbit_incref(_M0L7_2adataS146);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS191
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1750);
                                _M0L6_2atmpS1749 = _M0Lm9_2acursorS149;
                                _M0Lm9_2acursorS149 = _M0L6_2atmpS1749 + 1;
                                if (_M0L10next__charS191 < 58) {
                                  if (_M0L10next__charS191 < 48) {
                                    goto join_190;
                                  } else {
                                    _M0L12dispatch__15S173 = 5;
                                    goto loop__label__15_176;
                                  }
                                } else if (_M0L10next__charS191 > 58) {
                                  goto join_190;
                                } else {
                                  _M0L12dispatch__15S173 = 3;
                                  goto loop__label__15_176;
                                }
                                join_190:;
                                _M0L12dispatch__15S173 = 0;
                                goto loop__label__15_176;
                              } else {
                                goto join_178;
                              }
                            }
                          } else if (_M0L10next__charS189 > 58) {
                            goto join_188;
                          } else {
                            _M0L12dispatch__15S173 = 1;
                            goto loop__label__15_176;
                          }
                          join_188:;
                          _M0L12dispatch__15S173 = 0;
                          goto loop__label__15_176;
                        } else {
                          goto join_165;
                        }
                      }
                      join_186:;
                      _M0L12dispatch__15S173 = 0;
                      goto loop__label__15_176;
                    } else {
                      goto join_165;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1753;
                    _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                    _M0Lm6tag__2S162 = _M0Lm9_2acursorS149;
                    _M0L6_2atmpS1753 = _M0Lm9_2acursorS149;
                    if (_M0L6_2atmpS1753 < _M0L6_2aendS148) {
                      int32_t _M0L6_2atmpS1755 = _M0Lm9_2acursorS149;
                      int32_t _M0L10next__charS193;
                      int32_t _M0L6_2atmpS1754;
                      moonbit_incref(_M0L7_2adataS146);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS193
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1755);
                      _M0L6_2atmpS1754 = _M0Lm9_2acursorS149;
                      _M0Lm9_2acursorS149 = _M0L6_2atmpS1754 + 1;
                      if (_M0L10next__charS193 < 58) {
                        if (_M0L10next__charS193 < 48) {
                          goto join_192;
                        } else {
                          _M0L12dispatch__15S173 = 5;
                          goto loop__label__15_176;
                        }
                      } else if (_M0L10next__charS193 > 58) {
                        goto join_192;
                      } else {
                        _M0L12dispatch__15S173 = 3;
                        goto loop__label__15_176;
                      }
                      join_192:;
                      _M0L12dispatch__15S173 = 0;
                      goto loop__label__15_176;
                    } else {
                      goto join_178;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1756;
                    _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                    _M0Lm6tag__2S162 = _M0Lm9_2acursorS149;
                    _M0Lm6tag__3S161 = _M0Lm9_2acursorS149;
                    _M0L6_2atmpS1756 = _M0Lm9_2acursorS149;
                    if (_M0L6_2atmpS1756 < _M0L6_2aendS148) {
                      int32_t _M0L6_2atmpS1758 = _M0Lm9_2acursorS149;
                      int32_t _M0L10next__charS195;
                      int32_t _M0L6_2atmpS1757;
                      moonbit_incref(_M0L7_2adataS146);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS195
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1758);
                      _M0L6_2atmpS1757 = _M0Lm9_2acursorS149;
                      _M0Lm9_2acursorS149 = _M0L6_2atmpS1757 + 1;
                      if (_M0L10next__charS195 < 48) {
                        if (_M0L10next__charS195 == 45) {
                          goto join_174;
                        } else {
                          goto join_194;
                        }
                      } else if (_M0L10next__charS195 > 57) {
                        if (_M0L10next__charS195 < 59) {
                          _M0L12dispatch__15S173 = 3;
                          goto loop__label__15_176;
                        } else {
                          goto join_194;
                        }
                      } else {
                        _M0L12dispatch__15S173 = 6;
                        goto loop__label__15_176;
                      }
                      join_194:;
                      _M0L12dispatch__15S173 = 0;
                      goto loop__label__15_176;
                    } else {
                      goto join_165;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1759;
                    _M0Lm9tag__1__1S159 = _M0Lm6tag__1S158;
                    _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                    _M0L6_2atmpS1759 = _M0Lm9_2acursorS149;
                    if (_M0L6_2atmpS1759 < _M0L6_2aendS148) {
                      int32_t _M0L6_2atmpS1761 = _M0Lm9_2acursorS149;
                      int32_t _M0L10next__charS197;
                      int32_t _M0L6_2atmpS1760;
                      moonbit_incref(_M0L7_2adataS146);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS197
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1761);
                      _M0L6_2atmpS1760 = _M0Lm9_2acursorS149;
                      _M0Lm9_2acursorS149 = _M0L6_2atmpS1760 + 1;
                      if (_M0L10next__charS197 < 58) {
                        if (_M0L10next__charS197 < 48) {
                          goto join_196;
                        } else {
                          _M0L12dispatch__15S173 = 2;
                          goto loop__label__15_176;
                        }
                      } else if (_M0L10next__charS197 > 58) {
                        goto join_196;
                      } else {
                        _M0L12dispatch__15S173 = 1;
                        goto loop__label__15_176;
                      }
                      join_196:;
                      _M0L12dispatch__15S173 = 0;
                      goto loop__label__15_176;
                    } else {
                      goto join_165;
                    }
                    break;
                  }
                  default: {
                    goto join_165;
                    break;
                  }
                }
                join_178:;
                _M0Lm6tag__1S158 = _M0Lm9tag__1__2S160;
                _M0Lm6tag__2S162 = _M0Lm9tag__2__1S163;
                _M0Lm20match__tag__saver__0S152 = _M0Lm6tag__0S157;
                _M0Lm20match__tag__saver__1S153 = _M0Lm6tag__1S158;
                _M0Lm20match__tag__saver__2S154 = _M0Lm6tag__2S162;
                _M0Lm20match__tag__saver__3S155 = _M0Lm6tag__3S161;
                _M0Lm20match__tag__saver__4S156 = _M0Lm6tag__4S164;
                _M0Lm13accept__stateS150 = 0;
                _M0Lm10match__endS151 = _M0Lm9_2acursorS149;
                goto join_165;
                join_174:;
                _M0Lm9tag__1__1S159 = _M0Lm9tag__1__2S160;
                _M0Lm6tag__1S158 = _M0Lm9_2acursorS149;
                _M0Lm6tag__2S162 = _M0Lm9tag__2__1S163;
                _M0L6_2atmpS1729 = _M0Lm9_2acursorS149;
                if (_M0L6_2atmpS1729 < _M0L6_2aendS148) {
                  int32_t _M0L6_2atmpS1731 = _M0Lm9_2acursorS149;
                  int32_t _M0L10next__charS177;
                  int32_t _M0L6_2atmpS1730;
                  moonbit_incref(_M0L7_2adataS146);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS177
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS146, _M0L6_2atmpS1731);
                  _M0L6_2atmpS1730 = _M0Lm9_2acursorS149;
                  _M0Lm9_2acursorS149 = _M0L6_2atmpS1730 + 1;
                  if (_M0L10next__charS177 < 58) {
                    if (_M0L10next__charS177 < 48) {
                      goto join_175;
                    } else {
                      _M0L12dispatch__15S173 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS177 > 58) {
                    goto join_175;
                  } else {
                    _M0L12dispatch__15S173 = 1;
                    continue;
                  }
                  join_175:;
                  _M0L12dispatch__15S173 = 0;
                  continue;
                } else {
                  goto join_165;
                }
                break;
              }
            } else {
              goto join_165;
            }
          } else {
            continue;
          }
        } else {
          goto join_165;
        }
        break;
      }
    } else {
      goto join_165;
    }
  } else {
    goto join_165;
  }
  join_165:;
  switch (_M0Lm13accept__stateS150) {
    case 0: {
      int32_t _M0L6_2atmpS1720 = _M0Lm20match__tag__saver__1S153;
      int32_t _M0L6_2atmpS1719 = _M0L6_2atmpS1720 + 1;
      int64_t _M0L6_2atmpS1716 = (int64_t)_M0L6_2atmpS1719;
      int32_t _M0L6_2atmpS1718 = _M0Lm20match__tag__saver__2S154;
      int64_t _M0L6_2atmpS1717 = (int64_t)_M0L6_2atmpS1718;
      struct _M0TPC16string10StringView _M0L11start__lineS166;
      int32_t _M0L6_2atmpS1715;
      int32_t _M0L6_2atmpS1714;
      int64_t _M0L6_2atmpS1711;
      int32_t _M0L6_2atmpS1713;
      int64_t _M0L6_2atmpS1712;
      struct _M0TPC16string10StringView _M0L13start__columnS167;
      int32_t _M0L6_2atmpS1710;
      int64_t _M0L6_2atmpS1707;
      int32_t _M0L6_2atmpS1709;
      int64_t _M0L6_2atmpS1708;
      struct _M0TPC16string10StringView _M0L3pkgS168;
      int32_t _M0L6_2atmpS1706;
      int32_t _M0L6_2atmpS1705;
      int64_t _M0L6_2atmpS1702;
      int32_t _M0L6_2atmpS1704;
      int64_t _M0L6_2atmpS1703;
      struct _M0TPC16string10StringView _M0L8filenameS169;
      int32_t _M0L6_2atmpS1701;
      int32_t _M0L6_2atmpS1700;
      int64_t _M0L6_2atmpS1697;
      int32_t _M0L6_2atmpS1699;
      int64_t _M0L6_2atmpS1698;
      struct _M0TPC16string10StringView _M0L9end__lineS170;
      int32_t _M0L6_2atmpS1696;
      int32_t _M0L6_2atmpS1695;
      int64_t _M0L6_2atmpS1692;
      int32_t _M0L6_2atmpS1694;
      int64_t _M0L6_2atmpS1693;
      struct _M0TPC16string10StringView _M0L11end__columnS171;
      struct _M0TPB13SourceLocRepr* _block_4100;
      moonbit_incref(_M0L7_2adataS146);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS166
      = _M0MPC16string6String4view(_M0L7_2adataS146, _M0L6_2atmpS1716, _M0L6_2atmpS1717);
      _M0L6_2atmpS1715 = _M0Lm20match__tag__saver__2S154;
      _M0L6_2atmpS1714 = _M0L6_2atmpS1715 + 1;
      _M0L6_2atmpS1711 = (int64_t)_M0L6_2atmpS1714;
      _M0L6_2atmpS1713 = _M0Lm20match__tag__saver__3S155;
      _M0L6_2atmpS1712 = (int64_t)_M0L6_2atmpS1713;
      moonbit_incref(_M0L7_2adataS146);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS167
      = _M0MPC16string6String4view(_M0L7_2adataS146, _M0L6_2atmpS1711, _M0L6_2atmpS1712);
      _M0L6_2atmpS1710 = _M0L8_2astartS147 + 1;
      _M0L6_2atmpS1707 = (int64_t)_M0L6_2atmpS1710;
      _M0L6_2atmpS1709 = _M0Lm20match__tag__saver__0S152;
      _M0L6_2atmpS1708 = (int64_t)_M0L6_2atmpS1709;
      moonbit_incref(_M0L7_2adataS146);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS168
      = _M0MPC16string6String4view(_M0L7_2adataS146, _M0L6_2atmpS1707, _M0L6_2atmpS1708);
      _M0L6_2atmpS1706 = _M0Lm20match__tag__saver__0S152;
      _M0L6_2atmpS1705 = _M0L6_2atmpS1706 + 1;
      _M0L6_2atmpS1702 = (int64_t)_M0L6_2atmpS1705;
      _M0L6_2atmpS1704 = _M0Lm20match__tag__saver__1S153;
      _M0L6_2atmpS1703 = (int64_t)_M0L6_2atmpS1704;
      moonbit_incref(_M0L7_2adataS146);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS169
      = _M0MPC16string6String4view(_M0L7_2adataS146, _M0L6_2atmpS1702, _M0L6_2atmpS1703);
      _M0L6_2atmpS1701 = _M0Lm20match__tag__saver__3S155;
      _M0L6_2atmpS1700 = _M0L6_2atmpS1701 + 1;
      _M0L6_2atmpS1697 = (int64_t)_M0L6_2atmpS1700;
      _M0L6_2atmpS1699 = _M0Lm20match__tag__saver__4S156;
      _M0L6_2atmpS1698 = (int64_t)_M0L6_2atmpS1699;
      moonbit_incref(_M0L7_2adataS146);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS170
      = _M0MPC16string6String4view(_M0L7_2adataS146, _M0L6_2atmpS1697, _M0L6_2atmpS1698);
      _M0L6_2atmpS1696 = _M0Lm20match__tag__saver__4S156;
      _M0L6_2atmpS1695 = _M0L6_2atmpS1696 + 1;
      _M0L6_2atmpS1692 = (int64_t)_M0L6_2atmpS1695;
      _M0L6_2atmpS1694 = _M0Lm10match__endS151;
      _M0L6_2atmpS1693 = (int64_t)_M0L6_2atmpS1694;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS171
      = _M0MPC16string6String4view(_M0L7_2adataS146, _M0L6_2atmpS1692, _M0L6_2atmpS1693);
      _block_4100
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4100)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4100->$0_0 = _M0L3pkgS168.$0;
      _block_4100->$0_1 = _M0L3pkgS168.$1;
      _block_4100->$0_2 = _M0L3pkgS168.$2;
      _block_4100->$1_0 = _M0L8filenameS169.$0;
      _block_4100->$1_1 = _M0L8filenameS169.$1;
      _block_4100->$1_2 = _M0L8filenameS169.$2;
      _block_4100->$2_0 = _M0L11start__lineS166.$0;
      _block_4100->$2_1 = _M0L11start__lineS166.$1;
      _block_4100->$2_2 = _M0L11start__lineS166.$2;
      _block_4100->$3_0 = _M0L13start__columnS167.$0;
      _block_4100->$3_1 = _M0L13start__columnS167.$1;
      _block_4100->$3_2 = _M0L13start__columnS167.$2;
      _block_4100->$4_0 = _M0L9end__lineS170.$0;
      _block_4100->$4_1 = _M0L9end__lineS170.$1;
      _block_4100->$4_2 = _M0L9end__lineS170.$2;
      _block_4100->$5_0 = _M0L11end__columnS171.$0;
      _block_4100->$5_1 = _M0L11end__columnS171.$1;
      _block_4100->$5_2 = _M0L11end__columnS171.$2;
      return _block_4100;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS146);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS139,
  int32_t _M0L5indexS140
) {
  int32_t _M0L3lenS138;
  int32_t _if__result_4101;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS138 = _M0L4selfS139->$1;
  if (_M0L5indexS140 >= 0) {
    _if__result_4101 = _M0L5indexS140 < _M0L3lenS138;
  } else {
    _if__result_4101 = 0;
  }
  if (_if__result_4101) {
    moonbit_string_t* _M0L6_2atmpS1690;
    moonbit_string_t _M0L6_2atmpS3758;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1690 = _M0MPC15array5Array6bufferGsE(_M0L4selfS139);
    if (
      _M0L5indexS140 < 0
      || _M0L5indexS140 >= Moonbit_array_length(_M0L6_2atmpS1690)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3758 = (moonbit_string_t)_M0L6_2atmpS1690[_M0L5indexS140];
    moonbit_incref(_M0L6_2atmpS3758);
    moonbit_decref(_M0L6_2atmpS1690);
    return _M0L6_2atmpS3758;
  } else {
    moonbit_decref(_M0L4selfS139);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

struct _M0TPC16string10StringView _M0MPC15array5Array2atGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS142,
  int32_t _M0L5indexS143
) {
  int32_t _M0L3lenS141;
  int32_t _if__result_4102;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS141 = _M0L4selfS142->$1;
  if (_M0L5indexS143 >= 0) {
    _if__result_4102 = _M0L5indexS143 < _M0L3lenS141;
  } else {
    _if__result_4102 = 0;
  }
  if (_if__result_4102) {
    struct _M0TPC16string10StringView* _M0L6_2atmpS1691;
    struct _M0TPC16string10StringView _M0L6_2atmpS3759;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1691
    = _M0MPC15array5Array6bufferGRPC16string10StringViewE(_M0L4selfS142);
    if (
      _M0L5indexS143 < 0
      || _M0L5indexS143 >= Moonbit_array_length(_M0L6_2atmpS1691)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3759 = _M0L6_2atmpS1691[_M0L5indexS143];
    moonbit_incref(_M0L6_2atmpS3759.$0);
    moonbit_decref(_M0L6_2atmpS1691);
    return _M0L6_2atmpS3759;
  } else {
    moonbit_decref(_M0L4selfS142);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array5Array6lengthGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS136
) {
  int32_t _M0L8_2afieldS3760;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3760 = _M0L4selfS136->$1;
  moonbit_decref(_M0L4selfS136);
  return _M0L8_2afieldS3760;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS137) {
  int32_t _M0L8_2afieldS3761;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3761 = _M0L4selfS137->$1;
  moonbit_decref(_M0L4selfS137);
  return _M0L8_2afieldS3761;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS132
) {
  moonbit_string_t* _M0L8_2afieldS3762;
  int32_t _M0L6_2acntS3898;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3762 = _M0L4selfS132->$0;
  _M0L6_2acntS3898 = Moonbit_object_header(_M0L4selfS132)->rc;
  if (_M0L6_2acntS3898 > 1) {
    int32_t _M0L11_2anew__cntS3899 = _M0L6_2acntS3898 - 1;
    Moonbit_object_header(_M0L4selfS132)->rc = _M0L11_2anew__cntS3899;
    moonbit_incref(_M0L8_2afieldS3762);
  } else if (_M0L6_2acntS3898 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS132);
  }
  return _M0L8_2afieldS3762;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS133
) {
  struct _M0TUsiE** _M0L8_2afieldS3763;
  int32_t _M0L6_2acntS3900;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3763 = _M0L4selfS133->$0;
  _M0L6_2acntS3900 = Moonbit_object_header(_M0L4selfS133)->rc;
  if (_M0L6_2acntS3900 > 1) {
    int32_t _M0L11_2anew__cntS3901 = _M0L6_2acntS3900 - 1;
    Moonbit_object_header(_M0L4selfS133)->rc = _M0L11_2anew__cntS3901;
    moonbit_incref(_M0L8_2afieldS3763);
  } else if (_M0L6_2acntS3900 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS133);
  }
  return _M0L8_2afieldS3763;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS134
) {
  void** _M0L8_2afieldS3764;
  int32_t _M0L6_2acntS3902;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3764 = _M0L4selfS134->$0;
  _M0L6_2acntS3902 = Moonbit_object_header(_M0L4selfS134)->rc;
  if (_M0L6_2acntS3902 > 1) {
    int32_t _M0L11_2anew__cntS3903 = _M0L6_2acntS3902 - 1;
    Moonbit_object_header(_M0L4selfS134)->rc = _M0L11_2anew__cntS3903;
    moonbit_incref(_M0L8_2afieldS3764);
  } else if (_M0L6_2acntS3902 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS134);
  }
  return _M0L8_2afieldS3764;
}

struct _M0TPC16string10StringView* _M0MPC15array5Array6bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS135
) {
  struct _M0TPC16string10StringView* _M0L8_2afieldS3765;
  int32_t _M0L6_2acntS3904;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3765 = _M0L4selfS135->$0;
  _M0L6_2acntS3904 = Moonbit_object_header(_M0L4selfS135)->rc;
  if (_M0L6_2acntS3904 > 1) {
    int32_t _M0L11_2anew__cntS3905 = _M0L6_2acntS3904 - 1;
    Moonbit_object_header(_M0L4selfS135)->rc = _M0L11_2anew__cntS3905;
    moonbit_incref(_M0L8_2afieldS3765);
  } else if (_M0L6_2acntS3904 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS135);
  }
  return _M0L8_2afieldS3765;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS131) {
  struct _M0TPB13StringBuilder* _M0L3bufS130;
  struct _M0TPB6Logger _M0L6_2atmpS1689;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS130 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS130);
  _M0L6_2atmpS1689
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS130
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS131, _M0L6_2atmpS1689);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS130);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS127,
  int32_t _M0L5indexS128
) {
  int32_t _M0L2c1S126;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S126 = _M0L4selfS127[_M0L5indexS128];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S126)) {
    int32_t _M0L6_2atmpS1688 = _M0L5indexS128 + 1;
    int32_t _M0L6_2atmpS3766 = _M0L4selfS127[_M0L6_2atmpS1688];
    int32_t _M0L2c2S129;
    int32_t _M0L6_2atmpS1686;
    int32_t _M0L6_2atmpS1687;
    moonbit_decref(_M0L4selfS127);
    _M0L2c2S129 = _M0L6_2atmpS3766;
    _M0L6_2atmpS1686 = (int32_t)_M0L2c1S126;
    _M0L6_2atmpS1687 = (int32_t)_M0L2c2S129;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1686, _M0L6_2atmpS1687);
  } else {
    moonbit_decref(_M0L4selfS127);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S126);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS125) {
  int32_t _M0L6_2atmpS1685;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1685 = (int32_t)_M0L4selfS125;
  return _M0L6_2atmpS1685;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS123,
  int32_t _M0L8trailingS124
) {
  int32_t _M0L6_2atmpS1684;
  int32_t _M0L6_2atmpS1683;
  int32_t _M0L6_2atmpS1682;
  int32_t _M0L6_2atmpS1681;
  int32_t _M0L6_2atmpS1680;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1684 = _M0L7leadingS123 - 55296;
  _M0L6_2atmpS1683 = _M0L6_2atmpS1684 * 1024;
  _M0L6_2atmpS1682 = _M0L6_2atmpS1683 + _M0L8trailingS124;
  _M0L6_2atmpS1681 = _M0L6_2atmpS1682 - 56320;
  _M0L6_2atmpS1680 = _M0L6_2atmpS1681 + 65536;
  return _M0L6_2atmpS1680;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS122) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS122 >= 56320) {
    return _M0L4selfS122 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS121) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS121 >= 55296) {
    return _M0L4selfS121 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS118,
  int32_t _M0L2chS120
) {
  int32_t _M0L3lenS1675;
  int32_t _M0L6_2atmpS1674;
  moonbit_bytes_t _M0L8_2afieldS3767;
  moonbit_bytes_t _M0L4dataS1678;
  int32_t _M0L3lenS1679;
  int32_t _M0L3incS119;
  int32_t _M0L3lenS1677;
  int32_t _M0L6_2atmpS1676;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1675 = _M0L4selfS118->$1;
  _M0L6_2atmpS1674 = _M0L3lenS1675 + 4;
  moonbit_incref(_M0L4selfS118);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS118, _M0L6_2atmpS1674);
  _M0L8_2afieldS3767 = _M0L4selfS118->$0;
  _M0L4dataS1678 = _M0L8_2afieldS3767;
  _M0L3lenS1679 = _M0L4selfS118->$1;
  moonbit_incref(_M0L4dataS1678);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS119
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1678, _M0L3lenS1679, _M0L2chS120);
  _M0L3lenS1677 = _M0L4selfS118->$1;
  _M0L6_2atmpS1676 = _M0L3lenS1677 + _M0L3incS119;
  _M0L4selfS118->$1 = _M0L6_2atmpS1676;
  moonbit_decref(_M0L4selfS118);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS113,
  int32_t _M0L8requiredS114
) {
  moonbit_bytes_t _M0L8_2afieldS3771;
  moonbit_bytes_t _M0L4dataS1673;
  int32_t _M0L6_2atmpS3770;
  int32_t _M0L12current__lenS112;
  int32_t _M0Lm13enough__spaceS115;
  int32_t _M0L6_2atmpS1671;
  int32_t _M0L6_2atmpS1672;
  moonbit_bytes_t _M0L9new__dataS117;
  moonbit_bytes_t _M0L8_2afieldS3769;
  moonbit_bytes_t _M0L4dataS1669;
  int32_t _M0L3lenS1670;
  moonbit_bytes_t _M0L6_2aoldS3768;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3771 = _M0L4selfS113->$0;
  _M0L4dataS1673 = _M0L8_2afieldS3771;
  _M0L6_2atmpS3770 = Moonbit_array_length(_M0L4dataS1673);
  _M0L12current__lenS112 = _M0L6_2atmpS3770;
  if (_M0L8requiredS114 <= _M0L12current__lenS112) {
    moonbit_decref(_M0L4selfS113);
    return 0;
  }
  _M0Lm13enough__spaceS115 = _M0L12current__lenS112;
  while (1) {
    int32_t _M0L6_2atmpS1667 = _M0Lm13enough__spaceS115;
    if (_M0L6_2atmpS1667 < _M0L8requiredS114) {
      int32_t _M0L6_2atmpS1668 = _M0Lm13enough__spaceS115;
      _M0Lm13enough__spaceS115 = _M0L6_2atmpS1668 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1671 = _M0Lm13enough__spaceS115;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1672 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS117
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1671, _M0L6_2atmpS1672);
  _M0L8_2afieldS3769 = _M0L4selfS113->$0;
  _M0L4dataS1669 = _M0L8_2afieldS3769;
  _M0L3lenS1670 = _M0L4selfS113->$1;
  moonbit_incref(_M0L4dataS1669);
  moonbit_incref(_M0L9new__dataS117);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS117, 0, _M0L4dataS1669, 0, _M0L3lenS1670);
  _M0L6_2aoldS3768 = _M0L4selfS113->$0;
  moonbit_decref(_M0L6_2aoldS3768);
  _M0L4selfS113->$0 = _M0L9new__dataS117;
  moonbit_decref(_M0L4selfS113);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS107,
  int32_t _M0L6offsetS108,
  int32_t _M0L5valueS106
) {
  uint32_t _M0L4codeS105;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS105 = _M0MPC14char4Char8to__uint(_M0L5valueS106);
  if (_M0L4codeS105 < 65536u) {
    uint32_t _M0L6_2atmpS1650 = _M0L4codeS105 & 255u;
    int32_t _M0L6_2atmpS1649;
    int32_t _M0L6_2atmpS1651;
    uint32_t _M0L6_2atmpS1653;
    int32_t _M0L6_2atmpS1652;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1649 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1650);
    if (
      _M0L6offsetS108 < 0
      || _M0L6offsetS108 >= Moonbit_array_length(_M0L4selfS107)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS107[_M0L6offsetS108] = _M0L6_2atmpS1649;
    _M0L6_2atmpS1651 = _M0L6offsetS108 + 1;
    _M0L6_2atmpS1653 = _M0L4codeS105 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1652 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1653);
    if (
      _M0L6_2atmpS1651 < 0
      || _M0L6_2atmpS1651 >= Moonbit_array_length(_M0L4selfS107)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS107[_M0L6_2atmpS1651] = _M0L6_2atmpS1652;
    moonbit_decref(_M0L4selfS107);
    return 2;
  } else if (_M0L4codeS105 < 1114112u) {
    uint32_t _M0L2hiS109 = _M0L4codeS105 - 65536u;
    uint32_t _M0L6_2atmpS1666 = _M0L2hiS109 >> 10;
    uint32_t _M0L2loS110 = _M0L6_2atmpS1666 | 55296u;
    uint32_t _M0L6_2atmpS1665 = _M0L2hiS109 & 1023u;
    uint32_t _M0L2hiS111 = _M0L6_2atmpS1665 | 56320u;
    uint32_t _M0L6_2atmpS1655 = _M0L2loS110 & 255u;
    int32_t _M0L6_2atmpS1654;
    int32_t _M0L6_2atmpS1656;
    uint32_t _M0L6_2atmpS1658;
    int32_t _M0L6_2atmpS1657;
    int32_t _M0L6_2atmpS1659;
    uint32_t _M0L6_2atmpS1661;
    int32_t _M0L6_2atmpS1660;
    int32_t _M0L6_2atmpS1662;
    uint32_t _M0L6_2atmpS1664;
    int32_t _M0L6_2atmpS1663;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1654 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1655);
    if (
      _M0L6offsetS108 < 0
      || _M0L6offsetS108 >= Moonbit_array_length(_M0L4selfS107)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS107[_M0L6offsetS108] = _M0L6_2atmpS1654;
    _M0L6_2atmpS1656 = _M0L6offsetS108 + 1;
    _M0L6_2atmpS1658 = _M0L2loS110 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1657 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1658);
    if (
      _M0L6_2atmpS1656 < 0
      || _M0L6_2atmpS1656 >= Moonbit_array_length(_M0L4selfS107)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS107[_M0L6_2atmpS1656] = _M0L6_2atmpS1657;
    _M0L6_2atmpS1659 = _M0L6offsetS108 + 2;
    _M0L6_2atmpS1661 = _M0L2hiS111 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1660 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1661);
    if (
      _M0L6_2atmpS1659 < 0
      || _M0L6_2atmpS1659 >= Moonbit_array_length(_M0L4selfS107)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS107[_M0L6_2atmpS1659] = _M0L6_2atmpS1660;
    _M0L6_2atmpS1662 = _M0L6offsetS108 + 3;
    _M0L6_2atmpS1664 = _M0L2hiS111 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1663 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1664);
    if (
      _M0L6_2atmpS1662 < 0
      || _M0L6_2atmpS1662 >= Moonbit_array_length(_M0L4selfS107)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS107[_M0L6_2atmpS1662] = _M0L6_2atmpS1663;
    moonbit_decref(_M0L4selfS107);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS107);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_100.data, (moonbit_string_t)moonbit_string_literal_101.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS104) {
  int32_t _M0L6_2atmpS1648;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1648 = *(int32_t*)&_M0L4selfS104;
  return _M0L6_2atmpS1648 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS103) {
  int32_t _M0L6_2atmpS1647;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1647 = _M0L4selfS103;
  return *(uint32_t*)&_M0L6_2atmpS1647;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS102
) {
  moonbit_bytes_t _M0L8_2afieldS3773;
  moonbit_bytes_t _M0L4dataS1646;
  moonbit_bytes_t _M0L6_2atmpS1643;
  int32_t _M0L8_2afieldS3772;
  int32_t _M0L3lenS1645;
  int64_t _M0L6_2atmpS1644;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3773 = _M0L4selfS102->$0;
  _M0L4dataS1646 = _M0L8_2afieldS3773;
  moonbit_incref(_M0L4dataS1646);
  _M0L6_2atmpS1643 = _M0L4dataS1646;
  _M0L8_2afieldS3772 = _M0L4selfS102->$1;
  moonbit_decref(_M0L4selfS102);
  _M0L3lenS1645 = _M0L8_2afieldS3772;
  _M0L6_2atmpS1644 = (int64_t)_M0L3lenS1645;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1643, 0, _M0L6_2atmpS1644);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS97,
  int32_t _M0L6offsetS101,
  int64_t _M0L6lengthS99
) {
  int32_t _M0L3lenS96;
  int32_t _M0L6lengthS98;
  int32_t _if__result_4104;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS96 = Moonbit_array_length(_M0L4selfS97);
  if (_M0L6lengthS99 == 4294967296ll) {
    _M0L6lengthS98 = _M0L3lenS96 - _M0L6offsetS101;
  } else {
    int64_t _M0L7_2aSomeS100 = _M0L6lengthS99;
    _M0L6lengthS98 = (int32_t)_M0L7_2aSomeS100;
  }
  if (_M0L6offsetS101 >= 0) {
    if (_M0L6lengthS98 >= 0) {
      int32_t _M0L6_2atmpS1642 = _M0L6offsetS101 + _M0L6lengthS98;
      _if__result_4104 = _M0L6_2atmpS1642 <= _M0L3lenS96;
    } else {
      _if__result_4104 = 0;
    }
  } else {
    _if__result_4104 = 0;
  }
  if (_if__result_4104) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS97, _M0L6offsetS101, _M0L6lengthS98);
  } else {
    moonbit_decref(_M0L4selfS97);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS94
) {
  int32_t _M0L7initialS93;
  moonbit_bytes_t _M0L4dataS95;
  struct _M0TPB13StringBuilder* _block_4105;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS94 < 1) {
    _M0L7initialS93 = 1;
  } else {
    _M0L7initialS93 = _M0L10size__hintS94;
  }
  _M0L4dataS95 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS93, 0);
  _block_4105
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4105)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4105->$0 = _M0L4dataS95;
  _block_4105->$1 = 0;
  return _block_4105;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS92) {
  int32_t _M0L6_2atmpS1641;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1641 = (int32_t)_M0L4selfS92;
  return _M0L6_2atmpS1641;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS72,
  int32_t _M0L11dst__offsetS73,
  moonbit_string_t* _M0L3srcS74,
  int32_t _M0L11src__offsetS75,
  int32_t _M0L3lenS76
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS72, _M0L11dst__offsetS73, _M0L3srcS74, _M0L11src__offsetS75, _M0L3lenS76);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS77,
  int32_t _M0L11dst__offsetS78,
  struct _M0TUsiE** _M0L3srcS79,
  int32_t _M0L11src__offsetS80,
  int32_t _M0L3lenS81
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS77, _M0L11dst__offsetS78, _M0L3srcS79, _M0L11src__offsetS80, _M0L3lenS81);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS82,
  int32_t _M0L11dst__offsetS83,
  void** _M0L3srcS84,
  int32_t _M0L11src__offsetS85,
  int32_t _M0L3lenS86
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS82, _M0L11dst__offsetS83, _M0L3srcS84, _M0L11src__offsetS85, _M0L3lenS86);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(
  struct _M0TPC16string10StringView* _M0L3dstS87,
  int32_t _M0L11dst__offsetS88,
  struct _M0TPC16string10StringView* _M0L3srcS89,
  int32_t _M0L11src__offsetS90,
  int32_t _M0L3lenS91
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(_M0L3dstS87, _M0L11dst__offsetS88, _M0L3srcS89, _M0L11src__offsetS90, _M0L3lenS91);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS27,
  int32_t _M0L11dst__offsetS29,
  moonbit_bytes_t _M0L3srcS28,
  int32_t _M0L11src__offsetS30,
  int32_t _M0L3lenS32
) {
  int32_t _if__result_4106;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS27 == _M0L3srcS28) {
    _if__result_4106 = _M0L11dst__offsetS29 < _M0L11src__offsetS30;
  } else {
    _if__result_4106 = 0;
  }
  if (_if__result_4106) {
    int32_t _M0L1iS31 = 0;
    while (1) {
      if (_M0L1iS31 < _M0L3lenS32) {
        int32_t _M0L6_2atmpS1596 = _M0L11dst__offsetS29 + _M0L1iS31;
        int32_t _M0L6_2atmpS1598 = _M0L11src__offsetS30 + _M0L1iS31;
        int32_t _M0L6_2atmpS1597;
        int32_t _M0L6_2atmpS1599;
        if (
          _M0L6_2atmpS1598 < 0
          || _M0L6_2atmpS1598 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1597 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1598];
        if (
          _M0L6_2atmpS1596 < 0
          || _M0L6_2atmpS1596 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1596] = _M0L6_2atmpS1597;
        _M0L6_2atmpS1599 = _M0L1iS31 + 1;
        _M0L1iS31 = _M0L6_2atmpS1599;
        continue;
      } else {
        moonbit_decref(_M0L3srcS28);
        moonbit_decref(_M0L3dstS27);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1604 = _M0L3lenS32 - 1;
    int32_t _M0L1iS34 = _M0L6_2atmpS1604;
    while (1) {
      if (_M0L1iS34 >= 0) {
        int32_t _M0L6_2atmpS1600 = _M0L11dst__offsetS29 + _M0L1iS34;
        int32_t _M0L6_2atmpS1602 = _M0L11src__offsetS30 + _M0L1iS34;
        int32_t _M0L6_2atmpS1601;
        int32_t _M0L6_2atmpS1603;
        if (
          _M0L6_2atmpS1602 < 0
          || _M0L6_2atmpS1602 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1601 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1602];
        if (
          _M0L6_2atmpS1600 < 0
          || _M0L6_2atmpS1600 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1600] = _M0L6_2atmpS1601;
        _M0L6_2atmpS1603 = _M0L1iS34 - 1;
        _M0L1iS34 = _M0L6_2atmpS1603;
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
  int32_t _if__result_4109;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS36 == _M0L3srcS37) {
    _if__result_4109 = _M0L11dst__offsetS38 < _M0L11src__offsetS39;
  } else {
    _if__result_4109 = 0;
  }
  if (_if__result_4109) {
    int32_t _M0L1iS40 = 0;
    while (1) {
      if (_M0L1iS40 < _M0L3lenS41) {
        int32_t _M0L6_2atmpS1605 = _M0L11dst__offsetS38 + _M0L1iS40;
        int32_t _M0L6_2atmpS1607 = _M0L11src__offsetS39 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS3775;
        moonbit_string_t _M0L6_2atmpS1606;
        moonbit_string_t _M0L6_2aoldS3774;
        int32_t _M0L6_2atmpS1608;
        if (
          _M0L6_2atmpS1607 < 0
          || _M0L6_2atmpS1607 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3775 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1607];
        _M0L6_2atmpS1606 = _M0L6_2atmpS3775;
        if (
          _M0L6_2atmpS1605 < 0
          || _M0L6_2atmpS1605 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3774 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1605];
        moonbit_incref(_M0L6_2atmpS1606);
        moonbit_decref(_M0L6_2aoldS3774);
        _M0L3dstS36[_M0L6_2atmpS1605] = _M0L6_2atmpS1606;
        _M0L6_2atmpS1608 = _M0L1iS40 + 1;
        _M0L1iS40 = _M0L6_2atmpS1608;
        continue;
      } else {
        moonbit_decref(_M0L3srcS37);
        moonbit_decref(_M0L3dstS36);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1613 = _M0L3lenS41 - 1;
    int32_t _M0L1iS43 = _M0L6_2atmpS1613;
    while (1) {
      if (_M0L1iS43 >= 0) {
        int32_t _M0L6_2atmpS1609 = _M0L11dst__offsetS38 + _M0L1iS43;
        int32_t _M0L6_2atmpS1611 = _M0L11src__offsetS39 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS3777;
        moonbit_string_t _M0L6_2atmpS1610;
        moonbit_string_t _M0L6_2aoldS3776;
        int32_t _M0L6_2atmpS1612;
        if (
          _M0L6_2atmpS1611 < 0
          || _M0L6_2atmpS1611 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3777 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1611];
        _M0L6_2atmpS1610 = _M0L6_2atmpS3777;
        if (
          _M0L6_2atmpS1609 < 0
          || _M0L6_2atmpS1609 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3776 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1609];
        moonbit_incref(_M0L6_2atmpS1610);
        moonbit_decref(_M0L6_2aoldS3776);
        _M0L3dstS36[_M0L6_2atmpS1609] = _M0L6_2atmpS1610;
        _M0L6_2atmpS1612 = _M0L1iS43 - 1;
        _M0L1iS43 = _M0L6_2atmpS1612;
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
  int32_t _if__result_4112;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS45 == _M0L3srcS46) {
    _if__result_4112 = _M0L11dst__offsetS47 < _M0L11src__offsetS48;
  } else {
    _if__result_4112 = 0;
  }
  if (_if__result_4112) {
    int32_t _M0L1iS49 = 0;
    while (1) {
      if (_M0L1iS49 < _M0L3lenS50) {
        int32_t _M0L6_2atmpS1614 = _M0L11dst__offsetS47 + _M0L1iS49;
        int32_t _M0L6_2atmpS1616 = _M0L11src__offsetS48 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS3779;
        struct _M0TUsiE* _M0L6_2atmpS1615;
        struct _M0TUsiE* _M0L6_2aoldS3778;
        int32_t _M0L6_2atmpS1617;
        if (
          _M0L6_2atmpS1616 < 0
          || _M0L6_2atmpS1616 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3779 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1616];
        _M0L6_2atmpS1615 = _M0L6_2atmpS3779;
        if (
          _M0L6_2atmpS1614 < 0
          || _M0L6_2atmpS1614 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3778 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1614];
        if (_M0L6_2atmpS1615) {
          moonbit_incref(_M0L6_2atmpS1615);
        }
        if (_M0L6_2aoldS3778) {
          moonbit_decref(_M0L6_2aoldS3778);
        }
        _M0L3dstS45[_M0L6_2atmpS1614] = _M0L6_2atmpS1615;
        _M0L6_2atmpS1617 = _M0L1iS49 + 1;
        _M0L1iS49 = _M0L6_2atmpS1617;
        continue;
      } else {
        moonbit_decref(_M0L3srcS46);
        moonbit_decref(_M0L3dstS45);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1622 = _M0L3lenS50 - 1;
    int32_t _M0L1iS52 = _M0L6_2atmpS1622;
    while (1) {
      if (_M0L1iS52 >= 0) {
        int32_t _M0L6_2atmpS1618 = _M0L11dst__offsetS47 + _M0L1iS52;
        int32_t _M0L6_2atmpS1620 = _M0L11src__offsetS48 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS3781;
        struct _M0TUsiE* _M0L6_2atmpS1619;
        struct _M0TUsiE* _M0L6_2aoldS3780;
        int32_t _M0L6_2atmpS1621;
        if (
          _M0L6_2atmpS1620 < 0
          || _M0L6_2atmpS1620 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3781 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1620];
        _M0L6_2atmpS1619 = _M0L6_2atmpS3781;
        if (
          _M0L6_2atmpS1618 < 0
          || _M0L6_2atmpS1618 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3780 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1618];
        if (_M0L6_2atmpS1619) {
          moonbit_incref(_M0L6_2atmpS1619);
        }
        if (_M0L6_2aoldS3780) {
          moonbit_decref(_M0L6_2aoldS3780);
        }
        _M0L3dstS45[_M0L6_2atmpS1618] = _M0L6_2atmpS1619;
        _M0L6_2atmpS1621 = _M0L1iS52 - 1;
        _M0L1iS52 = _M0L6_2atmpS1621;
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
  int32_t _if__result_4115;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS54 == _M0L3srcS55) {
    _if__result_4115 = _M0L11dst__offsetS56 < _M0L11src__offsetS57;
  } else {
    _if__result_4115 = 0;
  }
  if (_if__result_4115) {
    int32_t _M0L1iS58 = 0;
    while (1) {
      if (_M0L1iS58 < _M0L3lenS59) {
        int32_t _M0L6_2atmpS1623 = _M0L11dst__offsetS56 + _M0L1iS58;
        int32_t _M0L6_2atmpS1625 = _M0L11src__offsetS57 + _M0L1iS58;
        void* _M0L6_2atmpS3783;
        void* _M0L6_2atmpS1624;
        void* _M0L6_2aoldS3782;
        int32_t _M0L6_2atmpS1626;
        if (
          _M0L6_2atmpS1625 < 0
          || _M0L6_2atmpS1625 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3783 = (void*)_M0L3srcS55[_M0L6_2atmpS1625];
        _M0L6_2atmpS1624 = _M0L6_2atmpS3783;
        if (
          _M0L6_2atmpS1623 < 0
          || _M0L6_2atmpS1623 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3782 = (void*)_M0L3dstS54[_M0L6_2atmpS1623];
        moonbit_incref(_M0L6_2atmpS1624);
        moonbit_decref(_M0L6_2aoldS3782);
        _M0L3dstS54[_M0L6_2atmpS1623] = _M0L6_2atmpS1624;
        _M0L6_2atmpS1626 = _M0L1iS58 + 1;
        _M0L1iS58 = _M0L6_2atmpS1626;
        continue;
      } else {
        moonbit_decref(_M0L3srcS55);
        moonbit_decref(_M0L3dstS54);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1631 = _M0L3lenS59 - 1;
    int32_t _M0L1iS61 = _M0L6_2atmpS1631;
    while (1) {
      if (_M0L1iS61 >= 0) {
        int32_t _M0L6_2atmpS1627 = _M0L11dst__offsetS56 + _M0L1iS61;
        int32_t _M0L6_2atmpS1629 = _M0L11src__offsetS57 + _M0L1iS61;
        void* _M0L6_2atmpS3785;
        void* _M0L6_2atmpS1628;
        void* _M0L6_2aoldS3784;
        int32_t _M0L6_2atmpS1630;
        if (
          _M0L6_2atmpS1629 < 0
          || _M0L6_2atmpS1629 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3785 = (void*)_M0L3srcS55[_M0L6_2atmpS1629];
        _M0L6_2atmpS1628 = _M0L6_2atmpS3785;
        if (
          _M0L6_2atmpS1627 < 0
          || _M0L6_2atmpS1627 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3784 = (void*)_M0L3dstS54[_M0L6_2atmpS1627];
        moonbit_incref(_M0L6_2atmpS1628);
        moonbit_decref(_M0L6_2aoldS3784);
        _M0L3dstS54[_M0L6_2atmpS1627] = _M0L6_2atmpS1628;
        _M0L6_2atmpS1630 = _M0L1iS61 - 1;
        _M0L1iS61 = _M0L6_2atmpS1630;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(
  struct _M0TPC16string10StringView* _M0L3dstS63,
  int32_t _M0L11dst__offsetS65,
  struct _M0TPC16string10StringView* _M0L3srcS64,
  int32_t _M0L11src__offsetS66,
  int32_t _M0L3lenS68
) {
  int32_t _if__result_4118;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS63 == _M0L3srcS64) {
    _if__result_4118 = _M0L11dst__offsetS65 < _M0L11src__offsetS66;
  } else {
    _if__result_4118 = 0;
  }
  if (_if__result_4118) {
    int32_t _M0L1iS67 = 0;
    while (1) {
      if (_M0L1iS67 < _M0L3lenS68) {
        int32_t _M0L6_2atmpS1632 = _M0L11dst__offsetS65 + _M0L1iS67;
        int32_t _M0L6_2atmpS1634 = _M0L11src__offsetS66 + _M0L1iS67;
        struct _M0TPC16string10StringView _M0L6_2atmpS3787;
        struct _M0TPC16string10StringView _M0L6_2atmpS1633;
        struct _M0TPC16string10StringView _M0L6_2aoldS3786;
        int32_t _M0L6_2atmpS1635;
        if (
          _M0L6_2atmpS1634 < 0
          || _M0L6_2atmpS1634 >= Moonbit_array_length(_M0L3srcS64)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3787 = _M0L3srcS64[_M0L6_2atmpS1634];
        _M0L6_2atmpS1633 = _M0L6_2atmpS3787;
        if (
          _M0L6_2atmpS1632 < 0
          || _M0L6_2atmpS1632 >= Moonbit_array_length(_M0L3dstS63)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3786 = _M0L3dstS63[_M0L6_2atmpS1632];
        moonbit_incref(_M0L6_2atmpS1633.$0);
        moonbit_decref(_M0L6_2aoldS3786.$0);
        _M0L3dstS63[_M0L6_2atmpS1632] = _M0L6_2atmpS1633;
        _M0L6_2atmpS1635 = _M0L1iS67 + 1;
        _M0L1iS67 = _M0L6_2atmpS1635;
        continue;
      } else {
        moonbit_decref(_M0L3srcS64);
        moonbit_decref(_M0L3dstS63);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1640 = _M0L3lenS68 - 1;
    int32_t _M0L1iS70 = _M0L6_2atmpS1640;
    while (1) {
      if (_M0L1iS70 >= 0) {
        int32_t _M0L6_2atmpS1636 = _M0L11dst__offsetS65 + _M0L1iS70;
        int32_t _M0L6_2atmpS1638 = _M0L11src__offsetS66 + _M0L1iS70;
        struct _M0TPC16string10StringView _M0L6_2atmpS3789;
        struct _M0TPC16string10StringView _M0L6_2atmpS1637;
        struct _M0TPC16string10StringView _M0L6_2aoldS3788;
        int32_t _M0L6_2atmpS1639;
        if (
          _M0L6_2atmpS1638 < 0
          || _M0L6_2atmpS1638 >= Moonbit_array_length(_M0L3srcS64)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3789 = _M0L3srcS64[_M0L6_2atmpS1638];
        _M0L6_2atmpS1637 = _M0L6_2atmpS3789;
        if (
          _M0L6_2atmpS1636 < 0
          || _M0L6_2atmpS1636 >= Moonbit_array_length(_M0L3dstS63)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3788 = _M0L3dstS63[_M0L6_2atmpS1636];
        moonbit_incref(_M0L6_2atmpS1637.$0);
        moonbit_decref(_M0L6_2aoldS3788.$0);
        _M0L3dstS63[_M0L6_2atmpS1636] = _M0L6_2atmpS1637;
        _M0L6_2atmpS1639 = _M0L1iS70 - 1;
        _M0L1iS70 = _M0L6_2atmpS1639;
        continue;
      } else {
        moonbit_decref(_M0L3srcS64);
        moonbit_decref(_M0L3dstS63);
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
  moonbit_string_t _M0L6_2atmpS1580;
  moonbit_string_t _M0L6_2atmpS3792;
  moonbit_string_t _M0L6_2atmpS1578;
  moonbit_string_t _M0L6_2atmpS1579;
  moonbit_string_t _M0L6_2atmpS3791;
  moonbit_string_t _M0L6_2atmpS1577;
  moonbit_string_t _M0L6_2atmpS3790;
  moonbit_string_t _M0L6_2atmpS1576;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1580 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3792
  = moonbit_add_string(_M0L6_2atmpS1580, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1580);
  _M0L6_2atmpS1578 = _M0L6_2atmpS3792;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1579
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3791 = moonbit_add_string(_M0L6_2atmpS1578, _M0L6_2atmpS1579);
  moonbit_decref(_M0L6_2atmpS1578);
  moonbit_decref(_M0L6_2atmpS1579);
  _M0L6_2atmpS1577 = _M0L6_2atmpS3791;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3790
  = moonbit_add_string(_M0L6_2atmpS1577, (moonbit_string_t)moonbit_string_literal_42.data);
  moonbit_decref(_M0L6_2atmpS1577);
  _M0L6_2atmpS1576 = _M0L6_2atmpS3790;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1576);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1585;
  moonbit_string_t _M0L6_2atmpS3795;
  moonbit_string_t _M0L6_2atmpS1583;
  moonbit_string_t _M0L6_2atmpS1584;
  moonbit_string_t _M0L6_2atmpS3794;
  moonbit_string_t _M0L6_2atmpS1582;
  moonbit_string_t _M0L6_2atmpS3793;
  moonbit_string_t _M0L6_2atmpS1581;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1585 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3795
  = moonbit_add_string(_M0L6_2atmpS1585, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1585);
  _M0L6_2atmpS1583 = _M0L6_2atmpS3795;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1584
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3794 = moonbit_add_string(_M0L6_2atmpS1583, _M0L6_2atmpS1584);
  moonbit_decref(_M0L6_2atmpS1583);
  moonbit_decref(_M0L6_2atmpS1584);
  _M0L6_2atmpS1582 = _M0L6_2atmpS3794;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3793
  = moonbit_add_string(_M0L6_2atmpS1582, (moonbit_string_t)moonbit_string_literal_42.data);
  moonbit_decref(_M0L6_2atmpS1582);
  _M0L6_2atmpS1581 = _M0L6_2atmpS3793;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1581);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS1590;
  moonbit_string_t _M0L6_2atmpS3798;
  moonbit_string_t _M0L6_2atmpS1588;
  moonbit_string_t _M0L6_2atmpS1589;
  moonbit_string_t _M0L6_2atmpS3797;
  moonbit_string_t _M0L6_2atmpS1587;
  moonbit_string_t _M0L6_2atmpS3796;
  moonbit_string_t _M0L6_2atmpS1586;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1590 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3798
  = moonbit_add_string(_M0L6_2atmpS1590, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1590);
  _M0L6_2atmpS1588 = _M0L6_2atmpS3798;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1589
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3797 = moonbit_add_string(_M0L6_2atmpS1588, _M0L6_2atmpS1589);
  moonbit_decref(_M0L6_2atmpS1588);
  moonbit_decref(_M0L6_2atmpS1589);
  _M0L6_2atmpS1587 = _M0L6_2atmpS3797;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3796
  = moonbit_add_string(_M0L6_2atmpS1587, (moonbit_string_t)moonbit_string_literal_42.data);
  moonbit_decref(_M0L6_2atmpS1587);
  _M0L6_2atmpS1586 = _M0L6_2atmpS3796;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1586);
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS25,
  moonbit_string_t _M0L3locS26
) {
  moonbit_string_t _M0L6_2atmpS1595;
  moonbit_string_t _M0L6_2atmpS3801;
  moonbit_string_t _M0L6_2atmpS1593;
  moonbit_string_t _M0L6_2atmpS1594;
  moonbit_string_t _M0L6_2atmpS3800;
  moonbit_string_t _M0L6_2atmpS1592;
  moonbit_string_t _M0L6_2atmpS3799;
  moonbit_string_t _M0L6_2atmpS1591;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1595 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3801
  = moonbit_add_string(_M0L6_2atmpS1595, (moonbit_string_t)moonbit_string_literal_102.data);
  moonbit_decref(_M0L6_2atmpS1595);
  _M0L6_2atmpS1593 = _M0L6_2atmpS3801;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1594
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3800 = moonbit_add_string(_M0L6_2atmpS1593, _M0L6_2atmpS1594);
  moonbit_decref(_M0L6_2atmpS1593);
  moonbit_decref(_M0L6_2atmpS1594);
  _M0L6_2atmpS1592 = _M0L6_2atmpS3800;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3799
  = moonbit_add_string(_M0L6_2atmpS1592, (moonbit_string_t)moonbit_string_literal_42.data);
  moonbit_decref(_M0L6_2atmpS1592);
  _M0L6_2atmpS1591 = _M0L6_2atmpS3799;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1591);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS17,
  uint32_t _M0L5valueS18
) {
  uint32_t _M0L3accS1575;
  uint32_t _M0L6_2atmpS1574;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1575 = _M0L4selfS17->$0;
  _M0L6_2atmpS1574 = _M0L3accS1575 + 4u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1574;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS17, _M0L5valueS18);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5inputS16
) {
  uint32_t _M0L3accS1572;
  uint32_t _M0L6_2atmpS1573;
  uint32_t _M0L6_2atmpS1571;
  uint32_t _M0L6_2atmpS1570;
  uint32_t _M0L6_2atmpS1569;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1572 = _M0L4selfS15->$0;
  _M0L6_2atmpS1573 = _M0L5inputS16 * 3266489917u;
  _M0L6_2atmpS1571 = _M0L3accS1572 + _M0L6_2atmpS1573;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1570 = _M0FPB4rotl(_M0L6_2atmpS1571, 17);
  _M0L6_2atmpS1569 = _M0L6_2atmpS1570 * 668265263u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1569;
  moonbit_decref(_M0L4selfS15);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS13, int32_t _M0L1rS14) {
  uint32_t _M0L6_2atmpS1566;
  int32_t _M0L6_2atmpS1568;
  uint32_t _M0L6_2atmpS1567;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1566 = _M0L1xS13 << (_M0L1rS14 & 31);
  _M0L6_2atmpS1568 = 32 - _M0L1rS14;
  _M0L6_2atmpS1567 = _M0L1xS13 >> (_M0L6_2atmpS1568 & 31);
  return _M0L6_2atmpS1566 | _M0L6_2atmpS1567;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S9,
  struct _M0TPB6Logger _M0L10_2ax__4934S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS3802;
  int32_t _M0L6_2acntS3906;
  moonbit_string_t _M0L15_2a_2aarg__4935S11;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S9;
  _M0L8_2afieldS3802 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS3906 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS3906 > 1) {
    int32_t _M0L11_2anew__cntS3907 = _M0L6_2acntS3906 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS3907;
    moonbit_incref(_M0L8_2afieldS3802);
  } else if (_M0L6_2acntS3906 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__4935S11 = _M0L8_2afieldS3802;
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
  void* _block_4121;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4121 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4121)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4121)->$0 = _M0L4selfS8;
  return _block_4121;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS7) {
  void* _block_4122;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4122 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4122)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4122)->$0 = _M0L5arrayS7;
  return _block_4122;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1514) {
  switch (Moonbit_object_tag(_M0L4_2aeS1514)) {
    case 2: {
      moonbit_decref(_M0L4_2aeS1514);
      return (moonbit_string_t)moonbit_string_literal_105.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1514);
      return (moonbit_string_t)moonbit_string_literal_106.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1514);
      return (moonbit_string_t)moonbit_string_literal_107.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1514);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1514);
      return (moonbit_string_t)moonbit_string_literal_108.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1534,
  int32_t _M0L8_2aparamS1533
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1532 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1534;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1532, _M0L8_2aparamS1533);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1531,
  struct _M0TPC16string10StringView _M0L8_2aparamS1530
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1529 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1531;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1529, _M0L8_2aparamS1530);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1528,
  moonbit_string_t _M0L8_2aparamS1525,
  int32_t _M0L8_2aparamS1526,
  int32_t _M0L8_2aparamS1527
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1524 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1528;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1524, _M0L8_2aparamS1525, _M0L8_2aparamS1526, _M0L8_2aparamS1527);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1523,
  moonbit_string_t _M0L8_2aparamS1522
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1521 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1523;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1521, _M0L8_2aparamS1522);
  return 0;
}

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal5fuzzy11MatchResultE(
  void* _M0L11_2aobj__ptrS1520
) {
  struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult* _M0L7_2aselfS1519 =
    (struct _M0TP48clawteam8clawteam8internal5fuzzy11MatchResult*)_M0L11_2aobj__ptrS1520;
  return _M0IPC16option6OptionPB6ToJson8to__jsonGRP48clawteam8clawteam8internal5fuzzy11MatchResultE(_M0L7_2aselfS1519);
}

void moonbit_init() {
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1439;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1565;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1564;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1563;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1542;
  moonbit_string_t* _M0L6_2atmpS1562;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1561;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1551;
  moonbit_string_t* _M0L6_2atmpS1560;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1559;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1552;
  moonbit_string_t* _M0L6_2atmpS1558;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1557;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1553;
  moonbit_string_t* _M0L6_2atmpS1556;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1555;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1554;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1440;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1550;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1549;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1548;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1543;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1441;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1547;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1546;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1545;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1544;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1438;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1541;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1540;
  _M0FPB4null = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  moonbit_incref(_M0FPB4null);
  _M0FPC17prelude4null = _M0FPB4null;
  _M0L7_2abindS1439
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1565 = _M0L7_2abindS1439;
  _M0L6_2atmpS1564
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1565
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1563
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1564);
  _M0L8_2atupleS1542
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1542)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1542->$0 = (moonbit_string_t)moonbit_string_literal_109.data;
  _M0L8_2atupleS1542->$1 = _M0L6_2atmpS1563;
  _M0L6_2atmpS1562 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1562[0] = (moonbit_string_t)moonbit_string_literal_110.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1561
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1561)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1561->$0
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1561->$1 = _M0L6_2atmpS1562;
  _M0L8_2atupleS1551
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1551)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1551->$0 = 0;
  _M0L8_2atupleS1551->$1 = _M0L8_2atupleS1561;
  _M0L6_2atmpS1560 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1560[0] = (moonbit_string_t)moonbit_string_literal_111.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1559
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1559)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1559->$0
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1559->$1 = _M0L6_2atmpS1560;
  _M0L8_2atupleS1552
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1552)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1552->$0 = 1;
  _M0L8_2atupleS1552->$1 = _M0L8_2atupleS1559;
  _M0L6_2atmpS1558 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1558[0] = (moonbit_string_t)moonbit_string_literal_112.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1557
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1557)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1557->$0
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1557->$1 = _M0L6_2atmpS1558;
  _M0L8_2atupleS1553
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1553)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1553->$0 = 2;
  _M0L8_2atupleS1553->$1 = _M0L8_2atupleS1557;
  _M0L6_2atmpS1556 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1556[0] = (moonbit_string_t)moonbit_string_literal_113.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__3_2eclo);
  _M0L8_2atupleS1555
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1555)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1555->$0
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test49____test__7365617263685f746573742e6d6274__3_2eclo;
  _M0L8_2atupleS1555->$1 = _M0L6_2atmpS1556;
  _M0L8_2atupleS1554
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1554)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1554->$0 = 3;
  _M0L8_2atupleS1554->$1 = _M0L8_2atupleS1555;
  _M0L7_2abindS1440
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1440[0] = _M0L8_2atupleS1551;
  _M0L7_2abindS1440[1] = _M0L8_2atupleS1552;
  _M0L7_2abindS1440[2] = _M0L8_2atupleS1553;
  _M0L7_2abindS1440[3] = _M0L8_2atupleS1554;
  _M0L6_2atmpS1550 = _M0L7_2abindS1440;
  _M0L6_2atmpS1549
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 4, _M0L6_2atmpS1550
  };
  #line 400 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1548
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1549);
  _M0L8_2atupleS1543
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1543)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1543->$0 = (moonbit_string_t)moonbit_string_literal_114.data;
  _M0L8_2atupleS1543->$1 = _M0L6_2atmpS1548;
  _M0L7_2abindS1441
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1547 = _M0L7_2abindS1441;
  _M0L6_2atmpS1546
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1547
  };
  #line 406 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1545
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1546);
  _M0L8_2atupleS1544
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1544)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1544->$0 = (moonbit_string_t)moonbit_string_literal_115.data;
  _M0L8_2atupleS1544->$1 = _M0L6_2atmpS1545;
  _M0L7_2abindS1438
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1438[0] = _M0L8_2atupleS1542;
  _M0L7_2abindS1438[1] = _M0L8_2atupleS1543;
  _M0L7_2abindS1438[2] = _M0L8_2atupleS1544;
  _M0L6_2atmpS1541 = _M0L7_2abindS1438;
  _M0L6_2atmpS1540
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 3, _M0L6_2atmpS1541
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1540);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1539;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1508;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1509;
  int32_t _M0L7_2abindS1510;
  int32_t _M0L2__S1511;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1539
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1508
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1508)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1508->$0 = _M0L6_2atmpS1539;
  _M0L12async__testsS1508->$1 = 0;
  #line 445 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1509
  = _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1510 = _M0L7_2abindS1509->$1;
  _M0L2__S1511 = 0;
  while (1) {
    if (_M0L2__S1511 < _M0L7_2abindS1510) {
      struct _M0TUsiE** _M0L8_2afieldS3806 = _M0L7_2abindS1509->$0;
      struct _M0TUsiE** _M0L3bufS1538 = _M0L8_2afieldS3806;
      struct _M0TUsiE* _M0L6_2atmpS3805 =
        (struct _M0TUsiE*)_M0L3bufS1538[_M0L2__S1511];
      struct _M0TUsiE* _M0L3argS1512 = _M0L6_2atmpS3805;
      moonbit_string_t _M0L8_2afieldS3804 = _M0L3argS1512->$0;
      moonbit_string_t _M0L6_2atmpS1535 = _M0L8_2afieldS3804;
      int32_t _M0L8_2afieldS3803 = _M0L3argS1512->$1;
      int32_t _M0L6_2atmpS1536 = _M0L8_2afieldS3803;
      int32_t _M0L6_2atmpS1537;
      moonbit_incref(_M0L6_2atmpS1535);
      moonbit_incref(_M0L12async__testsS1508);
      #line 446 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal21fuzzy__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1508, _M0L6_2atmpS1535, _M0L6_2atmpS1536);
      _M0L6_2atmpS1537 = _M0L2__S1511 + 1;
      _M0L2__S1511 = _M0L6_2atmpS1537;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1509);
    }
    break;
  }
  #line 448 "E:\\moonbit\\clawteam\\internal\\fuzzy\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal21fuzzy__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal21fuzzy__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1508);
  return 0;
}