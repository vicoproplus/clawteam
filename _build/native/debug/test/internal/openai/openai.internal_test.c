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
struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPB4Json5Array;

struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TWERPC16option6OptionGRPC16string10StringViewE;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0DTPC16result6ResultGsRPB7NoErrorE3Err;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__;

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0DTPC16result6ResultGOsRPB7NoErrorE3Err;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWEuQRPC15error5Error;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWRPC16string10StringViewEOs;

struct _M0TWEOUsRPB4JsonE;

struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__;

struct _M0R38String_3a_3aiter_2eanon__u2269__l247__;

struct _M0TWRPC16string10StringViewEs;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGOsRPB7NoErrorE2Ok;

struct _M0TPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__;

struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0KTPB6ToJsonTPC16option6OptionGsE;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TWcERPC16string10StringView;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0TPC13ref3RefGORPC16string10StringViewE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPB9ArrayViewGsE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0DTPC16result6ResultGsRPB7NoErrorE2Ok;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__;

struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

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

struct _M0DTPB4Json5Array {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* $0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  
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

struct _M0DTPC16result6ResultGsRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $2_0;
  
};

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TWcERPC16string10StringView* $0;
  struct _M0TWEOc* $1;
  
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

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0DTPC16result6ResultGOsRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TWRPC16string10StringViewEOs {
  moonbit_string_t(* code)(
    struct _M0TWRPC16string10StringViewEOs*,
    struct _M0TPC16string10StringView
  );
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* $0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2269__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0DTPC16result6ResultGOsRPB7NoErrorE2Ok {
  moonbit_string_t $0;
  
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

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
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

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0TPC13ref3RefGORPC16string10StringViewE {
  void* $0;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPC13ref3RefGiE {
  int32_t $0;
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0DTPC16result6ResultGsRPB7NoErrorE2Ok {
  moonbit_string_t $0;
  
};

struct _M0TUWEuQRPC15error5ErrorNsE {
  struct _M0TWEuQRPC15error5Error* $0;
  moonbit_string_t* $1;
  
};

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  int32_t $1_1;
  int32_t $1_2;
  int32_t $2;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* $0;
  moonbit_string_t $1_0;
  
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai35____test__61692e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1325(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1316(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3145l515(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3141l516(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal6openai45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1205(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1200(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1187(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal6openai28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6openai34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai25____test__61692e6d6274__0(
  
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai27extract__first__json__block(
  moonbit_string_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2973l720(
  struct _M0TWRPC16string10StringViewEOs*,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2977l725(
  struct _M0TWRPC16string10StringViewEs*,
  struct _M0TPC16string10StringView
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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter4dropGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*,
  int32_t
);

void* _M0MPB4Iter4dropGRPC16string10StringViewEC2510l560(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter4takeGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*,
  int32_t
);

void* _M0MPB4Iter4takeGRPC16string10StringViewEC2504l481(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

void* _M0IPC16option6OptionPB6ToJson8to__jsonGsE(moonbit_string_t);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2496l591(
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

moonbit_string_t _M0MPC16option6Option4bindGRPC16string10StringViewsE(
  void*,
  struct _M0TWRPC16string10StringViewEOs*
);

moonbit_string_t _M0MPC16option6Option3mapGRPC16string10StringViewsE(
  void*,
  struct _M0TWRPC16string10StringViewEs*
);

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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2311l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

void* _M0MPC16string10StringView5splitC2296l1073(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2292l1070(
  struct _M0TWcERPC16string10StringView*,
  int32_t
);

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t);

moonbit_string_t _M0FPB16char__to__string(int32_t);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TWEOc*,
  struct _M0TWcERPC16string10StringView*
);

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2285l317(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2269l247(struct _M0TWEOc*);

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

int32_t _M0MPC16string10StringView4iterC2091l198(struct _M0TWEOc*);

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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

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

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

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

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 97, 105, 46, 109, 98, 116, 58, 55, 52, 
    52, 58, 49, 54, 45, 55, 52, 52, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 112, 101, 110, 
    97, 105, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 97, 108, 108, 111, 119, 101, 100, 95, 116, 111, 111, 108, 
    115, 95, 109, 111, 100, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 26), 
    99, 111, 109, 112, 108, 101, 116, 105, 111, 110, 95, 117, 115, 97, 
    103, 101, 95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 40), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 102, 117, 110, 99, 116, 105, 111, 110, 95, 116, 111, 111, 
    108, 95, 99, 104, 111, 105, 99, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_46 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 40), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 104, 111, 105, 99, 101, 95, 102, 105, 110, 105, 115, 
    104, 95, 114, 101, 97, 115, 111, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 50, 54, 58, 57, 45, 
    52, 50, 54, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 104, 117, 110, 107, 95, 99, 104, 111, 105, 99, 101, 
    95, 100, 101, 108, 116, 97, 95, 116, 111, 111, 108, 95, 99, 97, 108, 
    108, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 104, 117, 110, 107, 95, 99, 104, 111, 105, 99, 101, 
    95, 100, 101, 108, 116, 97, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    96, 96, 96, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_78 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_36 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    106, 115, 111, 110, 95, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    99, 111, 109, 112, 108, 101, 116, 105, 111, 110, 95, 117, 115, 97, 
    103, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    112, 114, 111, 109, 112, 116, 95, 116, 111, 107, 101, 110, 115, 95, 
    100, 101, 116, 97, 105, 108, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 116, 111, 111, 108, 95, 112, 97, 114, 97, 109, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    114, 101, 115, 112, 111, 110, 115, 101, 95, 102, 111, 114, 109, 97, 
    116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 104, 117, 110, 107, 95, 99, 104, 111, 105, 99, 101, 
    95, 100, 101, 108, 116, 97, 95, 116, 111, 111, 108, 95, 99, 97, 108, 
    108, 95, 102, 117, 110, 99, 116, 105, 111, 110, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    106, 115, 111, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 109, 101, 115, 115, 97, 103, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_50 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[117]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 116), 
    72, 101, 114, 101, 32, 105, 115, 32, 115, 111, 109, 101, 32, 116, 
    101, 120, 116, 46, 10, 96, 96, 96, 106, 115, 111, 110, 10, 123, 10, 
    32, 32, 34, 107, 101, 121, 34, 58, 32, 34, 118, 97, 108, 117, 101, 
    34, 10, 125, 10, 96, 96, 96, 10, 83, 111, 109, 101, 32, 109, 111, 
    114, 101, 32, 116, 101, 120, 116, 46, 10, 96, 96, 96, 106, 115, 111, 
    110, 10, 123, 10, 32, 32, 34, 97, 110, 111, 116, 104, 101, 114, 95, 
    107, 101, 121, 34, 58, 32, 34, 97, 110, 111, 116, 104, 101, 114, 
    95, 118, 97, 108, 117, 101, 34, 10, 125, 10, 96, 96, 96, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    101, 120, 116, 114, 97, 99, 116, 95, 102, 105, 114, 115, 116, 95, 
    106, 115, 111, 110, 95, 98, 108, 111, 99, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[34]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 33), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 109, 101, 115, 115, 97, 103, 101, 95, 112, 97, 114, 97, 
    109, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    99, 111, 115, 116, 95, 100, 101, 116, 97, 105, 108, 115, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[47]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 46), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 109, 101, 115, 115, 97, 103, 101, 95, 116, 111, 111, 108, 
    95, 99, 97, 108, 108, 95, 102, 117, 110, 99, 116, 105, 111, 110, 
    46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 97, 105, 46, 109, 98, 116, 58, 55, 52, 
    52, 58, 51, 45, 55, 52, 54, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 114, 101, 97, 115, 111, 110, 105, 110, 103, 95, 101, 102, 
    102, 111, 114, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 116, 111, 111, 108, 95, 109, 101, 115, 115, 97, 103, 101, 
    95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    97, 110, 116, 104, 114, 111, 112, 105, 99, 95, 116, 121, 112, 101, 
    115, 95, 106, 115, 111, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    111, 112, 101, 110, 95, 114, 111, 117, 116, 101, 114, 95, 101, 114, 
    114, 111, 114, 95, 114, 101, 115, 112, 111, 110, 115, 101, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 97, 105, 46, 109, 98, 116, 58, 55, 52, 
    52, 58, 53, 57, 45, 55, 52, 54, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 115, 101, 114, 118, 105, 99, 101, 95, 116, 105, 101, 114, 
    46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    117, 114, 105, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 104, 117, 110, 107, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    98, 117, 105, 108, 100, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[100]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 99), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 112, 
    101, 110, 97, 105, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 
    49, 51, 58, 53, 45, 49, 49, 51, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[102]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 101), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 112, 
    101, 110, 97, 105, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 114, 111, 108, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 26), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 104, 111, 105, 99, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    96, 96, 96, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 109, 101, 115, 115, 97, 103, 101, 95, 114, 111, 108, 101, 
    46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_27 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[34]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 33), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 97, 108, 108, 111, 119, 101, 100, 95, 116, 111, 111, 108, 
    115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 111, 110, 116, 101, 110, 116, 95, 112, 97, 114, 116, 
    95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    102, 117, 110, 99, 116, 105, 111, 110, 95, 100, 101, 102, 105, 110, 
    105, 116, 105, 111, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 29), 
    99, 111, 109, 112, 108, 101, 116, 105, 111, 110, 95, 116, 111, 107, 
    101, 110, 115, 95, 100, 101, 116, 97, 105, 108, 115, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 104, 117, 110, 107, 95, 99, 104, 111, 105, 99, 101, 
    46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_77 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    97, 105, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    114, 101, 113, 117, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 40), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 115, 116, 114, 101, 97, 109, 95, 111, 112, 116, 105, 111, 
    110, 115, 95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 40), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 115, 121, 115, 116, 101, 109, 95, 109, 101, 115, 115, 97, 
    103, 101, 95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 117, 115, 116, 111, 109, 95, 116, 111, 111, 108, 95, 
    99, 104, 111, 105, 99, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[38]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 37), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 109, 101, 115, 115, 97, 103, 101, 95, 116, 111, 111, 108, 
    95, 99, 97, 108, 108, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 117, 115, 101, 114, 95, 109, 101, 115, 115, 97, 103, 101, 
    95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    123, 10, 32, 32, 34, 107, 101, 121, 34, 58, 32, 34, 118, 97, 108, 
    117, 101, 34, 10, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[44]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 43), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 97, 115, 115, 105, 115, 116, 97, 110, 116, 95, 109, 101, 
    115, 115, 97, 103, 101, 95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 116, 111, 111, 108, 95, 99, 104, 111, 105, 99, 101, 46, 
    109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_68 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    32, 13, 10, 9, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[44]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 43), 
    99, 104, 97, 116, 95, 99, 111, 109, 112, 108, 101, 116, 105, 111, 
    110, 95, 99, 111, 110, 116, 101, 110, 116, 95, 112, 97, 114, 116, 
    95, 116, 101, 120, 116, 95, 112, 97, 114, 97, 109, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    99, 97, 99, 104, 101, 95, 99, 111, 110, 116, 114, 111, 108, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    97, 110, 116, 104, 114, 111, 112, 105, 99, 95, 116, 121, 112, 101, 
    115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRPC16string10StringViewEOs data;
  
} const _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2973l720$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2973l720
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRPC16string10StringViewEs data;
  
} const _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2977l725$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2977l725
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWcERPC16string10StringView data;
  
} const _M0MPC16string10StringView5splitC2292l1070$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MPC16string10StringView5splitC2292l1070
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal6openai35____test__61692e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai35____test__61692e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1325$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1325
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal6openai31____test__61692e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal6openai35____test__61692e6d6274__0_2edyncall$closure.data;

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
} _M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE}
  };

struct _M0BTPB6ToJson* _M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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

int64_t _M0FPB33brute__force__find_2econstr_2f529 = 0ll;

int64_t _M0FPB43boyer__moore__horspool__find_2econstr_2f515 = 0ll;

struct { int32_t rc; uint32_t meta; struct _M0TPB17FloatingDecimal64 data; 
} _M0FPB31ryu__to__string_2erecord_2f1046$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1046 =
  &_M0FPB31ryu__to__string_2erecord_2f1046$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai35____test__61692e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3176
) {
  return _M0FP48clawteam8clawteam8internal6openai25____test__61692e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1346,
  moonbit_string_t _M0L8filenameS1321,
  int32_t _M0L5indexS1324
) {
  struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316* _closure_3552;
  struct _M0TWssbEu* _M0L14handle__resultS1316;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1325;
  void* _M0L11_2atry__errS1340;
  struct moonbit_result_0 _tmp_3554;
  int32_t _handle__error__result_3555;
  int32_t _M0L6_2atmpS3164;
  void* _M0L3errS1341;
  moonbit_string_t _M0L4nameS1343;
  struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1344;
  moonbit_string_t _M0L8_2afieldS3177;
  int32_t _M0L6_2acntS3455;
  moonbit_string_t _M0L7_2anameS1345;
  #line 614 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1321);
  _closure_3552
  = (struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316*)moonbit_malloc(sizeof(struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316));
  Moonbit_object_header(_closure_3552)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316, $1) >> 2, 1, 0);
  _closure_3552->code
  = &_M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1316;
  _closure_3552->$0 = _M0L5indexS1324;
  _closure_3552->$1 = _M0L8filenameS1321;
  _M0L14handle__resultS1316 = (struct _M0TWssbEu*)_closure_3552;
  _M0L17error__to__stringS1325
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1325$closure.data;
  moonbit_incref(_M0L12async__testsS1346);
  moonbit_incref(_M0L17error__to__stringS1325);
  moonbit_incref(_M0L8filenameS1321);
  moonbit_incref(_M0L14handle__resultS1316);
  #line 648 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _tmp_3554
  = _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__test(_M0L12async__testsS1346, _M0L8filenameS1321, _M0L5indexS1324, _M0L14handle__resultS1316, _M0L17error__to__stringS1325);
  if (_tmp_3554.tag) {
    int32_t const _M0L5_2aokS3173 = _tmp_3554.data.ok;
    _handle__error__result_3555 = _M0L5_2aokS3173;
  } else {
    void* const _M0L6_2aerrS3174 = _tmp_3554.data.err;
    moonbit_decref(_M0L12async__testsS1346);
    moonbit_decref(_M0L17error__to__stringS1325);
    moonbit_decref(_M0L8filenameS1321);
    _M0L11_2atry__errS1340 = _M0L6_2aerrS3174;
    goto join_1339;
  }
  if (_handle__error__result_3555) {
    moonbit_decref(_M0L12async__testsS1346);
    moonbit_decref(_M0L17error__to__stringS1325);
    moonbit_decref(_M0L8filenameS1321);
    _M0L6_2atmpS3164 = 1;
  } else {
    struct moonbit_result_0 _tmp_3556;
    int32_t _handle__error__result_3557;
    moonbit_incref(_M0L12async__testsS1346);
    moonbit_incref(_M0L17error__to__stringS1325);
    moonbit_incref(_M0L8filenameS1321);
    moonbit_incref(_M0L14handle__resultS1316);
    #line 651 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    _tmp_3556
    = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1346, _M0L8filenameS1321, _M0L5indexS1324, _M0L14handle__resultS1316, _M0L17error__to__stringS1325);
    if (_tmp_3556.tag) {
      int32_t const _M0L5_2aokS3171 = _tmp_3556.data.ok;
      _handle__error__result_3557 = _M0L5_2aokS3171;
    } else {
      void* const _M0L6_2aerrS3172 = _tmp_3556.data.err;
      moonbit_decref(_M0L12async__testsS1346);
      moonbit_decref(_M0L17error__to__stringS1325);
      moonbit_decref(_M0L8filenameS1321);
      _M0L11_2atry__errS1340 = _M0L6_2aerrS3172;
      goto join_1339;
    }
    if (_handle__error__result_3557) {
      moonbit_decref(_M0L12async__testsS1346);
      moonbit_decref(_M0L17error__to__stringS1325);
      moonbit_decref(_M0L8filenameS1321);
      _M0L6_2atmpS3164 = 1;
    } else {
      struct moonbit_result_0 _tmp_3558;
      int32_t _handle__error__result_3559;
      moonbit_incref(_M0L12async__testsS1346);
      moonbit_incref(_M0L17error__to__stringS1325);
      moonbit_incref(_M0L8filenameS1321);
      moonbit_incref(_M0L14handle__resultS1316);
      #line 654 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _tmp_3558
      = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1346, _M0L8filenameS1321, _M0L5indexS1324, _M0L14handle__resultS1316, _M0L17error__to__stringS1325);
      if (_tmp_3558.tag) {
        int32_t const _M0L5_2aokS3169 = _tmp_3558.data.ok;
        _handle__error__result_3559 = _M0L5_2aokS3169;
      } else {
        void* const _M0L6_2aerrS3170 = _tmp_3558.data.err;
        moonbit_decref(_M0L12async__testsS1346);
        moonbit_decref(_M0L17error__to__stringS1325);
        moonbit_decref(_M0L8filenameS1321);
        _M0L11_2atry__errS1340 = _M0L6_2aerrS3170;
        goto join_1339;
      }
      if (_handle__error__result_3559) {
        moonbit_decref(_M0L12async__testsS1346);
        moonbit_decref(_M0L17error__to__stringS1325);
        moonbit_decref(_M0L8filenameS1321);
        _M0L6_2atmpS3164 = 1;
      } else {
        struct moonbit_result_0 _tmp_3560;
        int32_t _handle__error__result_3561;
        moonbit_incref(_M0L12async__testsS1346);
        moonbit_incref(_M0L17error__to__stringS1325);
        moonbit_incref(_M0L8filenameS1321);
        moonbit_incref(_M0L14handle__resultS1316);
        #line 657 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        _tmp_3560
        = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1346, _M0L8filenameS1321, _M0L5indexS1324, _M0L14handle__resultS1316, _M0L17error__to__stringS1325);
        if (_tmp_3560.tag) {
          int32_t const _M0L5_2aokS3167 = _tmp_3560.data.ok;
          _handle__error__result_3561 = _M0L5_2aokS3167;
        } else {
          void* const _M0L6_2aerrS3168 = _tmp_3560.data.err;
          moonbit_decref(_M0L12async__testsS1346);
          moonbit_decref(_M0L17error__to__stringS1325);
          moonbit_decref(_M0L8filenameS1321);
          _M0L11_2atry__errS1340 = _M0L6_2aerrS3168;
          goto join_1339;
        }
        if (_handle__error__result_3561) {
          moonbit_decref(_M0L12async__testsS1346);
          moonbit_decref(_M0L17error__to__stringS1325);
          moonbit_decref(_M0L8filenameS1321);
          _M0L6_2atmpS3164 = 1;
        } else {
          struct moonbit_result_0 _tmp_3562;
          moonbit_incref(_M0L14handle__resultS1316);
          #line 660 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
          _tmp_3562
          = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1346, _M0L8filenameS1321, _M0L5indexS1324, _M0L14handle__resultS1316, _M0L17error__to__stringS1325);
          if (_tmp_3562.tag) {
            int32_t const _M0L5_2aokS3165 = _tmp_3562.data.ok;
            _M0L6_2atmpS3164 = _M0L5_2aokS3165;
          } else {
            void* const _M0L6_2aerrS3166 = _tmp_3562.data.err;
            _M0L11_2atry__errS1340 = _M0L6_2aerrS3166;
            goto join_1339;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3164) {
    void* _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3175 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3175)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3175)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1340
    = _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3175;
    goto join_1339;
  } else {
    moonbit_decref(_M0L14handle__resultS1316);
  }
  goto joinlet_3553;
  join_1339:;
  _M0L3errS1341 = _M0L11_2atry__errS1340;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1344
  = (struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1341;
  _M0L8_2afieldS3177 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1344->$0;
  _M0L6_2acntS3455
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1344)->rc;
  if (_M0L6_2acntS3455 > 1) {
    int32_t _M0L11_2anew__cntS3456 = _M0L6_2acntS3455 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1344)->rc
    = _M0L11_2anew__cntS3456;
    moonbit_incref(_M0L8_2afieldS3177);
  } else if (_M0L6_2acntS3455 == 1) {
    #line 667 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1344);
  }
  _M0L7_2anameS1345 = _M0L8_2afieldS3177;
  _M0L4nameS1343 = _M0L7_2anameS1345;
  goto join_1342;
  goto joinlet_3563;
  join_1342:;
  #line 668 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1316(_M0L14handle__resultS1316, _M0L4nameS1343, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3563:;
  joinlet_3553:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1325(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3163,
  void* _M0L3errS1326
) {
  void* _M0L1eS1328;
  moonbit_string_t _M0L1eS1330;
  #line 637 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3163);
  switch (Moonbit_object_tag(_M0L3errS1326)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1331 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1326;
      moonbit_string_t _M0L8_2afieldS3178 = _M0L10_2aFailureS1331->$0;
      int32_t _M0L6_2acntS3457 =
        Moonbit_object_header(_M0L10_2aFailureS1331)->rc;
      moonbit_string_t _M0L4_2aeS1332;
      if (_M0L6_2acntS3457 > 1) {
        int32_t _M0L11_2anew__cntS3458 = _M0L6_2acntS3457 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1331)->rc
        = _M0L11_2anew__cntS3458;
        moonbit_incref(_M0L8_2afieldS3178);
      } else if (_M0L6_2acntS3457 == 1) {
        #line 638 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1331);
      }
      _M0L4_2aeS1332 = _M0L8_2afieldS3178;
      _M0L1eS1330 = _M0L4_2aeS1332;
      goto join_1329;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1333 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1326;
      moonbit_string_t _M0L8_2afieldS3179 = _M0L15_2aInspectErrorS1333->$0;
      int32_t _M0L6_2acntS3459 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1333)->rc;
      moonbit_string_t _M0L4_2aeS1334;
      if (_M0L6_2acntS3459 > 1) {
        int32_t _M0L11_2anew__cntS3460 = _M0L6_2acntS3459 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1333)->rc
        = _M0L11_2anew__cntS3460;
        moonbit_incref(_M0L8_2afieldS3179);
      } else if (_M0L6_2acntS3459 == 1) {
        #line 638 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1333);
      }
      _M0L4_2aeS1334 = _M0L8_2afieldS3179;
      _M0L1eS1330 = _M0L4_2aeS1334;
      goto join_1329;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1335 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1326;
      moonbit_string_t _M0L8_2afieldS3180 = _M0L16_2aSnapshotErrorS1335->$0;
      int32_t _M0L6_2acntS3461 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1335)->rc;
      moonbit_string_t _M0L4_2aeS1336;
      if (_M0L6_2acntS3461 > 1) {
        int32_t _M0L11_2anew__cntS3462 = _M0L6_2acntS3461 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1335)->rc
        = _M0L11_2anew__cntS3462;
        moonbit_incref(_M0L8_2afieldS3180);
      } else if (_M0L6_2acntS3461 == 1) {
        #line 638 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1335);
      }
      _M0L4_2aeS1336 = _M0L8_2afieldS3180;
      _M0L1eS1330 = _M0L4_2aeS1336;
      goto join_1329;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1337 =
        (struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1326;
      moonbit_string_t _M0L8_2afieldS3181 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1337->$0;
      int32_t _M0L6_2acntS3463 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1337)->rc;
      moonbit_string_t _M0L4_2aeS1338;
      if (_M0L6_2acntS3463 > 1) {
        int32_t _M0L11_2anew__cntS3464 = _M0L6_2acntS3463 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1337)->rc
        = _M0L11_2anew__cntS3464;
        moonbit_incref(_M0L8_2afieldS3181);
      } else if (_M0L6_2acntS3463 == 1) {
        #line 638 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1337);
      }
      _M0L4_2aeS1338 = _M0L8_2afieldS3181;
      _M0L1eS1330 = _M0L4_2aeS1338;
      goto join_1329;
      break;
    }
    default: {
      _M0L1eS1328 = _M0L3errS1326;
      goto join_1327;
      break;
    }
  }
  join_1329:;
  return _M0L1eS1330;
  join_1327:;
  #line 643 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1328);
}

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1316(
  struct _M0TWssbEu* _M0L6_2aenvS3149,
  moonbit_string_t _M0L8testnameS1317,
  moonbit_string_t _M0L7messageS1318,
  int32_t _M0L7skippedS1319
) {
  struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316* _M0L14_2acasted__envS3150;
  moonbit_string_t _M0L8_2afieldS3191;
  moonbit_string_t _M0L8filenameS1321;
  int32_t _M0L8_2afieldS3190;
  int32_t _M0L6_2acntS3465;
  int32_t _M0L5indexS1324;
  int32_t _if__result_3566;
  moonbit_string_t _M0L10file__nameS1320;
  moonbit_string_t _M0L10test__nameS1322;
  moonbit_string_t _M0L7messageS1323;
  moonbit_string_t _M0L6_2atmpS3162;
  moonbit_string_t _M0L6_2atmpS3189;
  moonbit_string_t _M0L6_2atmpS3161;
  moonbit_string_t _M0L6_2atmpS3188;
  moonbit_string_t _M0L6_2atmpS3159;
  moonbit_string_t _M0L6_2atmpS3160;
  moonbit_string_t _M0L6_2atmpS3187;
  moonbit_string_t _M0L6_2atmpS3158;
  moonbit_string_t _M0L6_2atmpS3186;
  moonbit_string_t _M0L6_2atmpS3156;
  moonbit_string_t _M0L6_2atmpS3157;
  moonbit_string_t _M0L6_2atmpS3185;
  moonbit_string_t _M0L6_2atmpS3155;
  moonbit_string_t _M0L6_2atmpS3184;
  moonbit_string_t _M0L6_2atmpS3153;
  moonbit_string_t _M0L6_2atmpS3154;
  moonbit_string_t _M0L6_2atmpS3183;
  moonbit_string_t _M0L6_2atmpS3152;
  moonbit_string_t _M0L6_2atmpS3182;
  moonbit_string_t _M0L6_2atmpS3151;
  #line 621 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3150
  = (struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1316*)_M0L6_2aenvS3149;
  _M0L8_2afieldS3191 = _M0L14_2acasted__envS3150->$1;
  _M0L8filenameS1321 = _M0L8_2afieldS3191;
  _M0L8_2afieldS3190 = _M0L14_2acasted__envS3150->$0;
  _M0L6_2acntS3465 = Moonbit_object_header(_M0L14_2acasted__envS3150)->rc;
  if (_M0L6_2acntS3465 > 1) {
    int32_t _M0L11_2anew__cntS3466 = _M0L6_2acntS3465 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3150)->rc
    = _M0L11_2anew__cntS3466;
    moonbit_incref(_M0L8filenameS1321);
  } else if (_M0L6_2acntS3465 == 1) {
    #line 621 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3150);
  }
  _M0L5indexS1324 = _M0L8_2afieldS3190;
  if (!_M0L7skippedS1319) {
    _if__result_3566 = 1;
  } else {
    _if__result_3566 = 0;
  }
  if (_if__result_3566) {
    
  }
  #line 627 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1320 = _M0MPC16string6String6escape(_M0L8filenameS1321);
  #line 628 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1322 = _M0MPC16string6String6escape(_M0L8testnameS1317);
  #line 629 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1323 = _M0MPC16string6String6escape(_M0L7messageS1318);
  #line 630 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 632 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3162
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1320);
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3189
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3162);
  moonbit_decref(_M0L6_2atmpS3162);
  _M0L6_2atmpS3161 = _M0L6_2atmpS3189;
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3188
  = moonbit_add_string(_M0L6_2atmpS3161, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3161);
  _M0L6_2atmpS3159 = _M0L6_2atmpS3188;
  #line 632 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3160
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1324);
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3187 = moonbit_add_string(_M0L6_2atmpS3159, _M0L6_2atmpS3160);
  moonbit_decref(_M0L6_2atmpS3159);
  moonbit_decref(_M0L6_2atmpS3160);
  _M0L6_2atmpS3158 = _M0L6_2atmpS3187;
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3186
  = moonbit_add_string(_M0L6_2atmpS3158, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3158);
  _M0L6_2atmpS3156 = _M0L6_2atmpS3186;
  #line 632 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3157
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1322);
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3185 = moonbit_add_string(_M0L6_2atmpS3156, _M0L6_2atmpS3157);
  moonbit_decref(_M0L6_2atmpS3156);
  moonbit_decref(_M0L6_2atmpS3157);
  _M0L6_2atmpS3155 = _M0L6_2atmpS3185;
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3184
  = moonbit_add_string(_M0L6_2atmpS3155, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3155);
  _M0L6_2atmpS3153 = _M0L6_2atmpS3184;
  #line 632 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3154
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1323);
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3183 = moonbit_add_string(_M0L6_2atmpS3153, _M0L6_2atmpS3154);
  moonbit_decref(_M0L6_2atmpS3153);
  moonbit_decref(_M0L6_2atmpS3154);
  _M0L6_2atmpS3152 = _M0L6_2atmpS3183;
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3182
  = moonbit_add_string(_M0L6_2atmpS3152, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3152);
  _M0L6_2atmpS3151 = _M0L6_2atmpS3182;
  #line 631 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3151);
  #line 634 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1315,
  moonbit_string_t _M0L8filenameS1312,
  int32_t _M0L5indexS1306,
  struct _M0TWssbEu* _M0L14handle__resultS1302,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1304
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1282;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1311;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1284;
  moonbit_string_t* _M0L5attrsS1285;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1305;
  moonbit_string_t _M0L4nameS1288;
  moonbit_string_t _M0L4nameS1286;
  int32_t _M0L6_2atmpS3148;
  struct _M0TWEOs* _M0L5_2aitS1290;
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__* _closure_3575;
  struct _M0TWEOc* _M0L6_2atmpS3139;
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__* _closure_3576;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3140;
  struct moonbit_result_0 _result_3577;
  #line 495 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1315);
  moonbit_incref(_M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests);
  #line 502 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1311
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1312);
  if (_M0L7_2abindS1311 == 0) {
    struct moonbit_result_0 _result_3568;
    if (_M0L7_2abindS1311) {
      moonbit_decref(_M0L7_2abindS1311);
    }
    moonbit_decref(_M0L17error__to__stringS1304);
    moonbit_decref(_M0L14handle__resultS1302);
    _result_3568.tag = 1;
    _result_3568.data.ok = 0;
    return _result_3568;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1313 =
      _M0L7_2abindS1311;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1314 =
      _M0L7_2aSomeS1313;
    _M0L10index__mapS1282 = _M0L13_2aindex__mapS1314;
    goto join_1281;
  }
  join_1281:;
  #line 504 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1305
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1282, _M0L5indexS1306);
  if (_M0L7_2abindS1305 == 0) {
    struct moonbit_result_0 _result_3570;
    if (_M0L7_2abindS1305) {
      moonbit_decref(_M0L7_2abindS1305);
    }
    moonbit_decref(_M0L17error__to__stringS1304);
    moonbit_decref(_M0L14handle__resultS1302);
    _result_3570.tag = 1;
    _result_3570.data.ok = 0;
    return _result_3570;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1307 =
      _M0L7_2abindS1305;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1308 = _M0L7_2aSomeS1307;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3195 = _M0L4_2axS1308->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1309 = _M0L8_2afieldS3195;
    moonbit_string_t* _M0L8_2afieldS3194 = _M0L4_2axS1308->$1;
    int32_t _M0L6_2acntS3467 = Moonbit_object_header(_M0L4_2axS1308)->rc;
    moonbit_string_t* _M0L8_2aattrsS1310;
    if (_M0L6_2acntS3467 > 1) {
      int32_t _M0L11_2anew__cntS3468 = _M0L6_2acntS3467 - 1;
      Moonbit_object_header(_M0L4_2axS1308)->rc = _M0L11_2anew__cntS3468;
      moonbit_incref(_M0L8_2afieldS3194);
      moonbit_incref(_M0L4_2afS1309);
    } else if (_M0L6_2acntS3467 == 1) {
      #line 502 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1308);
    }
    _M0L8_2aattrsS1310 = _M0L8_2afieldS3194;
    _M0L1fS1284 = _M0L4_2afS1309;
    _M0L5attrsS1285 = _M0L8_2aattrsS1310;
    goto join_1283;
  }
  join_1283:;
  _M0L6_2atmpS3148 = Moonbit_array_length(_M0L5attrsS1285);
  if (_M0L6_2atmpS3148 >= 1) {
    moonbit_string_t _M0L6_2atmpS3193 = (moonbit_string_t)_M0L5attrsS1285[0];
    moonbit_string_t _M0L7_2anameS1289 = _M0L6_2atmpS3193;
    moonbit_incref(_M0L7_2anameS1289);
    _M0L4nameS1288 = _M0L7_2anameS1289;
    goto join_1287;
  } else {
    _M0L4nameS1286 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3571;
  join_1287:;
  _M0L4nameS1286 = _M0L4nameS1288;
  joinlet_3571:;
  #line 505 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1290 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1285);
  while (1) {
    moonbit_string_t _M0L4attrS1292;
    moonbit_string_t _M0L7_2abindS1299;
    int32_t _M0L6_2atmpS3132;
    int64_t _M0L6_2atmpS3131;
    moonbit_incref(_M0L5_2aitS1290);
    #line 507 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1299 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1290);
    if (_M0L7_2abindS1299 == 0) {
      if (_M0L7_2abindS1299) {
        moonbit_decref(_M0L7_2abindS1299);
      }
      moonbit_decref(_M0L5_2aitS1290);
    } else {
      moonbit_string_t _M0L7_2aSomeS1300 = _M0L7_2abindS1299;
      moonbit_string_t _M0L7_2aattrS1301 = _M0L7_2aSomeS1300;
      _M0L4attrS1292 = _M0L7_2aattrS1301;
      goto join_1291;
    }
    goto joinlet_3573;
    join_1291:;
    _M0L6_2atmpS3132 = Moonbit_array_length(_M0L4attrS1292);
    _M0L6_2atmpS3131 = (int64_t)_M0L6_2atmpS3132;
    moonbit_incref(_M0L4attrS1292);
    #line 508 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1292, 5, 0, _M0L6_2atmpS3131)
    ) {
      int32_t _M0L6_2atmpS3138 = _M0L4attrS1292[0];
      int32_t _M0L4_2axS1293 = _M0L6_2atmpS3138;
      if (_M0L4_2axS1293 == 112) {
        int32_t _M0L6_2atmpS3137 = _M0L4attrS1292[1];
        int32_t _M0L4_2axS1294 = _M0L6_2atmpS3137;
        if (_M0L4_2axS1294 == 97) {
          int32_t _M0L6_2atmpS3136 = _M0L4attrS1292[2];
          int32_t _M0L4_2axS1295 = _M0L6_2atmpS3136;
          if (_M0L4_2axS1295 == 110) {
            int32_t _M0L6_2atmpS3135 = _M0L4attrS1292[3];
            int32_t _M0L4_2axS1296 = _M0L6_2atmpS3135;
            if (_M0L4_2axS1296 == 105) {
              int32_t _M0L6_2atmpS3192 = _M0L4attrS1292[4];
              int32_t _M0L6_2atmpS3134;
              int32_t _M0L4_2axS1297;
              moonbit_decref(_M0L4attrS1292);
              _M0L6_2atmpS3134 = _M0L6_2atmpS3192;
              _M0L4_2axS1297 = _M0L6_2atmpS3134;
              if (_M0L4_2axS1297 == 99) {
                void* _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3133;
                struct moonbit_result_0 _result_3574;
                moonbit_decref(_M0L17error__to__stringS1304);
                moonbit_decref(_M0L14handle__resultS1302);
                moonbit_decref(_M0L5_2aitS1290);
                moonbit_decref(_M0L1fS1284);
                _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3133
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3133)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3133)->$0
                = _M0L4nameS1286;
                _result_3574.tag = 0;
                _result_3574.data.err
                = _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3133;
                return _result_3574;
              }
            } else {
              moonbit_decref(_M0L4attrS1292);
            }
          } else {
            moonbit_decref(_M0L4attrS1292);
          }
        } else {
          moonbit_decref(_M0L4attrS1292);
        }
      } else {
        moonbit_decref(_M0L4attrS1292);
      }
    } else {
      moonbit_decref(_M0L4attrS1292);
    }
    continue;
    joinlet_3573:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1302);
  moonbit_incref(_M0L4nameS1286);
  _closure_3575
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__*)moonbit_malloc(sizeof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__));
  Moonbit_object_header(_closure_3575)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__, $0) >> 2, 2, 0);
  _closure_3575->code
  = &_M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3145l515;
  _closure_3575->$0 = _M0L14handle__resultS1302;
  _closure_3575->$1 = _M0L4nameS1286;
  _M0L6_2atmpS3139 = (struct _M0TWEOc*)_closure_3575;
  _closure_3576
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__*)moonbit_malloc(sizeof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__));
  Moonbit_object_header(_closure_3576)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__, $0) >> 2, 3, 0);
  _closure_3576->code
  = &_M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3141l516;
  _closure_3576->$0 = _M0L17error__to__stringS1304;
  _closure_3576->$1 = _M0L14handle__resultS1302;
  _closure_3576->$2 = _M0L4nameS1286;
  _M0L6_2atmpS3140 = (struct _M0TWRPC15error5ErrorEu*)_closure_3576;
  #line 513 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal6openai45moonbit__test__driver__internal__catch__error(_M0L1fS1284, _M0L6_2atmpS3139, _M0L6_2atmpS3140);
  _result_3577.tag = 1;
  _result_3577.data.ok = 1;
  return _result_3577;
}

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3145l515(
  struct _M0TWEOc* _M0L6_2aenvS3146
) {
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__* _M0L14_2acasted__envS3147;
  moonbit_string_t _M0L8_2afieldS3197;
  moonbit_string_t _M0L4nameS1286;
  struct _M0TWssbEu* _M0L8_2afieldS3196;
  int32_t _M0L6_2acntS3469;
  struct _M0TWssbEu* _M0L14handle__resultS1302;
  #line 515 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3147
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3145__l515__*)_M0L6_2aenvS3146;
  _M0L8_2afieldS3197 = _M0L14_2acasted__envS3147->$1;
  _M0L4nameS1286 = _M0L8_2afieldS3197;
  _M0L8_2afieldS3196 = _M0L14_2acasted__envS3147->$0;
  _M0L6_2acntS3469 = Moonbit_object_header(_M0L14_2acasted__envS3147)->rc;
  if (_M0L6_2acntS3469 > 1) {
    int32_t _M0L11_2anew__cntS3470 = _M0L6_2acntS3469 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3147)->rc
    = _M0L11_2anew__cntS3470;
    moonbit_incref(_M0L4nameS1286);
    moonbit_incref(_M0L8_2afieldS3196);
  } else if (_M0L6_2acntS3469 == 1) {
    #line 515 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3147);
  }
  _M0L14handle__resultS1302 = _M0L8_2afieldS3196;
  #line 515 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1302->code(_M0L14handle__resultS1302, _M0L4nameS1286, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3141l516(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3142,
  void* _M0L3errS1303
) {
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__* _M0L14_2acasted__envS3143;
  moonbit_string_t _M0L8_2afieldS3200;
  moonbit_string_t _M0L4nameS1286;
  struct _M0TWssbEu* _M0L8_2afieldS3199;
  struct _M0TWssbEu* _M0L14handle__resultS1302;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3198;
  int32_t _M0L6_2acntS3471;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1304;
  moonbit_string_t _M0L6_2atmpS3144;
  #line 516 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3143
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3141__l516__*)_M0L6_2aenvS3142;
  _M0L8_2afieldS3200 = _M0L14_2acasted__envS3143->$2;
  _M0L4nameS1286 = _M0L8_2afieldS3200;
  _M0L8_2afieldS3199 = _M0L14_2acasted__envS3143->$1;
  _M0L14handle__resultS1302 = _M0L8_2afieldS3199;
  _M0L8_2afieldS3198 = _M0L14_2acasted__envS3143->$0;
  _M0L6_2acntS3471 = Moonbit_object_header(_M0L14_2acasted__envS3143)->rc;
  if (_M0L6_2acntS3471 > 1) {
    int32_t _M0L11_2anew__cntS3472 = _M0L6_2acntS3471 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3143)->rc
    = _M0L11_2anew__cntS3472;
    moonbit_incref(_M0L4nameS1286);
    moonbit_incref(_M0L14handle__resultS1302);
    moonbit_incref(_M0L8_2afieldS3198);
  } else if (_M0L6_2acntS3471 == 1) {
    #line 516 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3143);
  }
  _M0L17error__to__stringS1304 = _M0L8_2afieldS3198;
  #line 516 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3144
  = _M0L17error__to__stringS1304->code(_M0L17error__to__stringS1304, _M0L3errS1303);
  #line 516 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1302->code(_M0L14handle__resultS1302, _M0L4nameS1286, _M0L6_2atmpS3144, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal6openai45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1232,
  struct _M0TWEOc* _M0L6on__okS1233,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1230
) {
  void* _M0L11_2atry__errS1228;
  struct moonbit_result_0 _tmp_3579;
  void* _M0L3errS1229;
  #line 375 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _tmp_3579 = _M0L1fS1232->code(_M0L1fS1232);
  if (_tmp_3579.tag) {
    int32_t const _M0L5_2aokS3129 = _tmp_3579.data.ok;
    moonbit_decref(_M0L7on__errS1230);
  } else {
    void* const _M0L6_2aerrS3130 = _tmp_3579.data.err;
    moonbit_decref(_M0L6on__okS1233);
    _M0L11_2atry__errS1228 = _M0L6_2aerrS3130;
    goto join_1227;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1233->code(_M0L6on__okS1233);
  goto joinlet_3578;
  join_1227:;
  _M0L3errS1229 = _M0L11_2atry__errS1228;
  #line 383 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1230->code(_M0L7on__errS1230, _M0L3errS1229);
  joinlet_3578:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1187;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1200;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1205;
  struct _M0TUsiE** _M0L6_2atmpS3128;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1212;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1213;
  moonbit_string_t _M0L6_2atmpS3127;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1214;
  int32_t _M0L7_2abindS1215;
  int32_t _M0L2__S1216;
  #line 193 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1187 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1200
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1205 = 0;
  _M0L6_2atmpS3128 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1212
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1212)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1212->$0 = _M0L6_2atmpS3128;
  _M0L16file__and__indexS1212->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1213
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1200(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1200);
  #line 284 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3127 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1213, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1214
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1205(_M0L51moonbit__test__driver__internal__split__mbt__stringS1205, _M0L6_2atmpS3127, 47);
  _M0L7_2abindS1215 = _M0L10test__argsS1214->$1;
  _M0L2__S1216 = 0;
  while (1) {
    if (_M0L2__S1216 < _M0L7_2abindS1215) {
      moonbit_string_t* _M0L8_2afieldS3202 = _M0L10test__argsS1214->$0;
      moonbit_string_t* _M0L3bufS3126 = _M0L8_2afieldS3202;
      moonbit_string_t _M0L6_2atmpS3201 =
        (moonbit_string_t)_M0L3bufS3126[_M0L2__S1216];
      moonbit_string_t _M0L3argS1217 = _M0L6_2atmpS3201;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1218;
      moonbit_string_t _M0L4fileS1219;
      moonbit_string_t _M0L5rangeS1220;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1221;
      moonbit_string_t _M0L6_2atmpS3124;
      int32_t _M0L5startS1222;
      moonbit_string_t _M0L6_2atmpS3123;
      int32_t _M0L3endS1223;
      int32_t _M0L1iS1224;
      int32_t _M0L6_2atmpS3125;
      moonbit_incref(_M0L3argS1217);
      #line 288 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1218
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1205(_M0L51moonbit__test__driver__internal__split__mbt__stringS1205, _M0L3argS1217, 58);
      moonbit_incref(_M0L16file__and__rangeS1218);
      #line 289 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1219
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1218, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1220
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1218, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1221
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1205(_M0L51moonbit__test__driver__internal__split__mbt__stringS1205, _M0L5rangeS1220, 45);
      moonbit_incref(_M0L15start__and__endS1221);
      #line 294 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3124
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1221, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1222
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1187(_M0L45moonbit__test__driver__internal__parse__int__S1187, _M0L6_2atmpS3124);
      #line 295 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3123
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1221, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1223
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1187(_M0L45moonbit__test__driver__internal__parse__int__S1187, _M0L6_2atmpS3123);
      _M0L1iS1224 = _M0L5startS1222;
      while (1) {
        if (_M0L1iS1224 < _M0L3endS1223) {
          struct _M0TUsiE* _M0L8_2atupleS3121;
          int32_t _M0L6_2atmpS3122;
          moonbit_incref(_M0L4fileS1219);
          _M0L8_2atupleS3121
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3121)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3121->$0 = _M0L4fileS1219;
          _M0L8_2atupleS3121->$1 = _M0L1iS1224;
          moonbit_incref(_M0L16file__and__indexS1212);
          #line 297 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1212, _M0L8_2atupleS3121);
          _M0L6_2atmpS3122 = _M0L1iS1224 + 1;
          _M0L1iS1224 = _M0L6_2atmpS3122;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1219);
        }
        break;
      }
      _M0L6_2atmpS3125 = _M0L2__S1216 + 1;
      _M0L2__S1216 = _M0L6_2atmpS3125;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1214);
    }
    break;
  }
  return _M0L16file__and__indexS1212;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1205(
  int32_t _M0L6_2aenvS3102,
  moonbit_string_t _M0L1sS1206,
  int32_t _M0L3sepS1207
) {
  moonbit_string_t* _M0L6_2atmpS3120;
  struct _M0TPB5ArrayGsE* _M0L3resS1208;
  struct _M0TPC13ref3RefGiE* _M0L1iS1209;
  struct _M0TPC13ref3RefGiE* _M0L5startS1210;
  int32_t _M0L3valS3115;
  int32_t _M0L6_2atmpS3116;
  #line 261 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3120 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1208
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1208)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1208->$0 = _M0L6_2atmpS3120;
  _M0L3resS1208->$1 = 0;
  _M0L1iS1209
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1209)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1209->$0 = 0;
  _M0L5startS1210
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1210)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1210->$0 = 0;
  while (1) {
    int32_t _M0L3valS3103 = _M0L1iS1209->$0;
    int32_t _M0L6_2atmpS3104 = Moonbit_array_length(_M0L1sS1206);
    if (_M0L3valS3103 < _M0L6_2atmpS3104) {
      int32_t _M0L3valS3107 = _M0L1iS1209->$0;
      int32_t _M0L6_2atmpS3106;
      int32_t _M0L6_2atmpS3105;
      int32_t _M0L3valS3114;
      int32_t _M0L6_2atmpS3113;
      if (
        _M0L3valS3107 < 0
        || _M0L3valS3107 >= Moonbit_array_length(_M0L1sS1206)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3106 = _M0L1sS1206[_M0L3valS3107];
      _M0L6_2atmpS3105 = _M0L6_2atmpS3106;
      if (_M0L6_2atmpS3105 == _M0L3sepS1207) {
        int32_t _M0L3valS3109 = _M0L5startS1210->$0;
        int32_t _M0L3valS3110 = _M0L1iS1209->$0;
        moonbit_string_t _M0L6_2atmpS3108;
        int32_t _M0L3valS3112;
        int32_t _M0L6_2atmpS3111;
        moonbit_incref(_M0L1sS1206);
        #line 270 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3108
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1206, _M0L3valS3109, _M0L3valS3110);
        moonbit_incref(_M0L3resS1208);
        #line 270 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1208, _M0L6_2atmpS3108);
        _M0L3valS3112 = _M0L1iS1209->$0;
        _M0L6_2atmpS3111 = _M0L3valS3112 + 1;
        _M0L5startS1210->$0 = _M0L6_2atmpS3111;
      }
      _M0L3valS3114 = _M0L1iS1209->$0;
      _M0L6_2atmpS3113 = _M0L3valS3114 + 1;
      _M0L1iS1209->$0 = _M0L6_2atmpS3113;
      continue;
    } else {
      moonbit_decref(_M0L1iS1209);
    }
    break;
  }
  _M0L3valS3115 = _M0L5startS1210->$0;
  _M0L6_2atmpS3116 = Moonbit_array_length(_M0L1sS1206);
  if (_M0L3valS3115 < _M0L6_2atmpS3116) {
    int32_t _M0L8_2afieldS3203 = _M0L5startS1210->$0;
    int32_t _M0L3valS3118;
    int32_t _M0L6_2atmpS3119;
    moonbit_string_t _M0L6_2atmpS3117;
    moonbit_decref(_M0L5startS1210);
    _M0L3valS3118 = _M0L8_2afieldS3203;
    _M0L6_2atmpS3119 = Moonbit_array_length(_M0L1sS1206);
    #line 276 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3117
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1206, _M0L3valS3118, _M0L6_2atmpS3119);
    moonbit_incref(_M0L3resS1208);
    #line 276 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1208, _M0L6_2atmpS3117);
  } else {
    moonbit_decref(_M0L5startS1210);
    moonbit_decref(_M0L1sS1206);
  }
  return _M0L3resS1208;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1200(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193
) {
  moonbit_bytes_t* _M0L3tmpS1201;
  int32_t _M0L6_2atmpS3101;
  struct _M0TPB5ArrayGsE* _M0L3resS1202;
  int32_t _M0L1iS1203;
  #line 250 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1201
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3101 = Moonbit_array_length(_M0L3tmpS1201);
  #line 254 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1202 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3101);
  _M0L1iS1203 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3097 = Moonbit_array_length(_M0L3tmpS1201);
    if (_M0L1iS1203 < _M0L6_2atmpS3097) {
      moonbit_bytes_t _M0L6_2atmpS3204;
      moonbit_bytes_t _M0L6_2atmpS3099;
      moonbit_string_t _M0L6_2atmpS3098;
      int32_t _M0L6_2atmpS3100;
      if (
        _M0L1iS1203 < 0 || _M0L1iS1203 >= Moonbit_array_length(_M0L3tmpS1201)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3204 = (moonbit_bytes_t)_M0L3tmpS1201[_M0L1iS1203];
      _M0L6_2atmpS3099 = _M0L6_2atmpS3204;
      moonbit_incref(_M0L6_2atmpS3099);
      #line 256 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3098
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193, _M0L6_2atmpS3099);
      moonbit_incref(_M0L3resS1202);
      #line 256 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1202, _M0L6_2atmpS3098);
      _M0L6_2atmpS3100 = _M0L1iS1203 + 1;
      _M0L1iS1203 = _M0L6_2atmpS3100;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1201);
    }
    break;
  }
  return _M0L3resS1202;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1193(
  int32_t _M0L6_2aenvS3011,
  moonbit_bytes_t _M0L5bytesS1194
) {
  struct _M0TPB13StringBuilder* _M0L3resS1195;
  int32_t _M0L3lenS1196;
  struct _M0TPC13ref3RefGiE* _M0L1iS1197;
  #line 206 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1195 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1196 = Moonbit_array_length(_M0L5bytesS1194);
  _M0L1iS1197
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1197)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1197->$0 = 0;
  while (1) {
    int32_t _M0L3valS3012 = _M0L1iS1197->$0;
    if (_M0L3valS3012 < _M0L3lenS1196) {
      int32_t _M0L3valS3096 = _M0L1iS1197->$0;
      int32_t _M0L6_2atmpS3095;
      int32_t _M0L6_2atmpS3094;
      struct _M0TPC13ref3RefGiE* _M0L1cS1198;
      int32_t _M0L3valS3013;
      if (
        _M0L3valS3096 < 0
        || _M0L3valS3096 >= Moonbit_array_length(_M0L5bytesS1194)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3095 = _M0L5bytesS1194[_M0L3valS3096];
      _M0L6_2atmpS3094 = (int32_t)_M0L6_2atmpS3095;
      _M0L1cS1198
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1198)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1198->$0 = _M0L6_2atmpS3094;
      _M0L3valS3013 = _M0L1cS1198->$0;
      if (_M0L3valS3013 < 128) {
        int32_t _M0L8_2afieldS3205 = _M0L1cS1198->$0;
        int32_t _M0L3valS3015;
        int32_t _M0L6_2atmpS3014;
        int32_t _M0L3valS3017;
        int32_t _M0L6_2atmpS3016;
        moonbit_decref(_M0L1cS1198);
        _M0L3valS3015 = _M0L8_2afieldS3205;
        _M0L6_2atmpS3014 = _M0L3valS3015;
        moonbit_incref(_M0L3resS1195);
        #line 215 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1195, _M0L6_2atmpS3014);
        _M0L3valS3017 = _M0L1iS1197->$0;
        _M0L6_2atmpS3016 = _M0L3valS3017 + 1;
        _M0L1iS1197->$0 = _M0L6_2atmpS3016;
      } else {
        int32_t _M0L3valS3018 = _M0L1cS1198->$0;
        if (_M0L3valS3018 < 224) {
          int32_t _M0L3valS3020 = _M0L1iS1197->$0;
          int32_t _M0L6_2atmpS3019 = _M0L3valS3020 + 1;
          int32_t _M0L3valS3029;
          int32_t _M0L6_2atmpS3028;
          int32_t _M0L6_2atmpS3022;
          int32_t _M0L3valS3027;
          int32_t _M0L6_2atmpS3026;
          int32_t _M0L6_2atmpS3025;
          int32_t _M0L6_2atmpS3024;
          int32_t _M0L6_2atmpS3023;
          int32_t _M0L6_2atmpS3021;
          int32_t _M0L8_2afieldS3206;
          int32_t _M0L3valS3031;
          int32_t _M0L6_2atmpS3030;
          int32_t _M0L3valS3033;
          int32_t _M0L6_2atmpS3032;
          if (_M0L6_2atmpS3019 >= _M0L3lenS1196) {
            moonbit_decref(_M0L1cS1198);
            moonbit_decref(_M0L1iS1197);
            moonbit_decref(_M0L5bytesS1194);
            break;
          }
          _M0L3valS3029 = _M0L1cS1198->$0;
          _M0L6_2atmpS3028 = _M0L3valS3029 & 31;
          _M0L6_2atmpS3022 = _M0L6_2atmpS3028 << 6;
          _M0L3valS3027 = _M0L1iS1197->$0;
          _M0L6_2atmpS3026 = _M0L3valS3027 + 1;
          if (
            _M0L6_2atmpS3026 < 0
            || _M0L6_2atmpS3026 >= Moonbit_array_length(_M0L5bytesS1194)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3025 = _M0L5bytesS1194[_M0L6_2atmpS3026];
          _M0L6_2atmpS3024 = (int32_t)_M0L6_2atmpS3025;
          _M0L6_2atmpS3023 = _M0L6_2atmpS3024 & 63;
          _M0L6_2atmpS3021 = _M0L6_2atmpS3022 | _M0L6_2atmpS3023;
          _M0L1cS1198->$0 = _M0L6_2atmpS3021;
          _M0L8_2afieldS3206 = _M0L1cS1198->$0;
          moonbit_decref(_M0L1cS1198);
          _M0L3valS3031 = _M0L8_2afieldS3206;
          _M0L6_2atmpS3030 = _M0L3valS3031;
          moonbit_incref(_M0L3resS1195);
          #line 222 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1195, _M0L6_2atmpS3030);
          _M0L3valS3033 = _M0L1iS1197->$0;
          _M0L6_2atmpS3032 = _M0L3valS3033 + 2;
          _M0L1iS1197->$0 = _M0L6_2atmpS3032;
        } else {
          int32_t _M0L3valS3034 = _M0L1cS1198->$0;
          if (_M0L3valS3034 < 240) {
            int32_t _M0L3valS3036 = _M0L1iS1197->$0;
            int32_t _M0L6_2atmpS3035 = _M0L3valS3036 + 2;
            int32_t _M0L3valS3052;
            int32_t _M0L6_2atmpS3051;
            int32_t _M0L6_2atmpS3044;
            int32_t _M0L3valS3050;
            int32_t _M0L6_2atmpS3049;
            int32_t _M0L6_2atmpS3048;
            int32_t _M0L6_2atmpS3047;
            int32_t _M0L6_2atmpS3046;
            int32_t _M0L6_2atmpS3045;
            int32_t _M0L6_2atmpS3038;
            int32_t _M0L3valS3043;
            int32_t _M0L6_2atmpS3042;
            int32_t _M0L6_2atmpS3041;
            int32_t _M0L6_2atmpS3040;
            int32_t _M0L6_2atmpS3039;
            int32_t _M0L6_2atmpS3037;
            int32_t _M0L8_2afieldS3207;
            int32_t _M0L3valS3054;
            int32_t _M0L6_2atmpS3053;
            int32_t _M0L3valS3056;
            int32_t _M0L6_2atmpS3055;
            if (_M0L6_2atmpS3035 >= _M0L3lenS1196) {
              moonbit_decref(_M0L1cS1198);
              moonbit_decref(_M0L1iS1197);
              moonbit_decref(_M0L5bytesS1194);
              break;
            }
            _M0L3valS3052 = _M0L1cS1198->$0;
            _M0L6_2atmpS3051 = _M0L3valS3052 & 15;
            _M0L6_2atmpS3044 = _M0L6_2atmpS3051 << 12;
            _M0L3valS3050 = _M0L1iS1197->$0;
            _M0L6_2atmpS3049 = _M0L3valS3050 + 1;
            if (
              _M0L6_2atmpS3049 < 0
              || _M0L6_2atmpS3049 >= Moonbit_array_length(_M0L5bytesS1194)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3048 = _M0L5bytesS1194[_M0L6_2atmpS3049];
            _M0L6_2atmpS3047 = (int32_t)_M0L6_2atmpS3048;
            _M0L6_2atmpS3046 = _M0L6_2atmpS3047 & 63;
            _M0L6_2atmpS3045 = _M0L6_2atmpS3046 << 6;
            _M0L6_2atmpS3038 = _M0L6_2atmpS3044 | _M0L6_2atmpS3045;
            _M0L3valS3043 = _M0L1iS1197->$0;
            _M0L6_2atmpS3042 = _M0L3valS3043 + 2;
            if (
              _M0L6_2atmpS3042 < 0
              || _M0L6_2atmpS3042 >= Moonbit_array_length(_M0L5bytesS1194)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3041 = _M0L5bytesS1194[_M0L6_2atmpS3042];
            _M0L6_2atmpS3040 = (int32_t)_M0L6_2atmpS3041;
            _M0L6_2atmpS3039 = _M0L6_2atmpS3040 & 63;
            _M0L6_2atmpS3037 = _M0L6_2atmpS3038 | _M0L6_2atmpS3039;
            _M0L1cS1198->$0 = _M0L6_2atmpS3037;
            _M0L8_2afieldS3207 = _M0L1cS1198->$0;
            moonbit_decref(_M0L1cS1198);
            _M0L3valS3054 = _M0L8_2afieldS3207;
            _M0L6_2atmpS3053 = _M0L3valS3054;
            moonbit_incref(_M0L3resS1195);
            #line 231 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1195, _M0L6_2atmpS3053);
            _M0L3valS3056 = _M0L1iS1197->$0;
            _M0L6_2atmpS3055 = _M0L3valS3056 + 3;
            _M0L1iS1197->$0 = _M0L6_2atmpS3055;
          } else {
            int32_t _M0L3valS3058 = _M0L1iS1197->$0;
            int32_t _M0L6_2atmpS3057 = _M0L3valS3058 + 3;
            int32_t _M0L3valS3081;
            int32_t _M0L6_2atmpS3080;
            int32_t _M0L6_2atmpS3073;
            int32_t _M0L3valS3079;
            int32_t _M0L6_2atmpS3078;
            int32_t _M0L6_2atmpS3077;
            int32_t _M0L6_2atmpS3076;
            int32_t _M0L6_2atmpS3075;
            int32_t _M0L6_2atmpS3074;
            int32_t _M0L6_2atmpS3066;
            int32_t _M0L3valS3072;
            int32_t _M0L6_2atmpS3071;
            int32_t _M0L6_2atmpS3070;
            int32_t _M0L6_2atmpS3069;
            int32_t _M0L6_2atmpS3068;
            int32_t _M0L6_2atmpS3067;
            int32_t _M0L6_2atmpS3060;
            int32_t _M0L3valS3065;
            int32_t _M0L6_2atmpS3064;
            int32_t _M0L6_2atmpS3063;
            int32_t _M0L6_2atmpS3062;
            int32_t _M0L6_2atmpS3061;
            int32_t _M0L6_2atmpS3059;
            int32_t _M0L3valS3083;
            int32_t _M0L6_2atmpS3082;
            int32_t _M0L3valS3087;
            int32_t _M0L6_2atmpS3086;
            int32_t _M0L6_2atmpS3085;
            int32_t _M0L6_2atmpS3084;
            int32_t _M0L8_2afieldS3208;
            int32_t _M0L3valS3091;
            int32_t _M0L6_2atmpS3090;
            int32_t _M0L6_2atmpS3089;
            int32_t _M0L6_2atmpS3088;
            int32_t _M0L3valS3093;
            int32_t _M0L6_2atmpS3092;
            if (_M0L6_2atmpS3057 >= _M0L3lenS1196) {
              moonbit_decref(_M0L1cS1198);
              moonbit_decref(_M0L1iS1197);
              moonbit_decref(_M0L5bytesS1194);
              break;
            }
            _M0L3valS3081 = _M0L1cS1198->$0;
            _M0L6_2atmpS3080 = _M0L3valS3081 & 7;
            _M0L6_2atmpS3073 = _M0L6_2atmpS3080 << 18;
            _M0L3valS3079 = _M0L1iS1197->$0;
            _M0L6_2atmpS3078 = _M0L3valS3079 + 1;
            if (
              _M0L6_2atmpS3078 < 0
              || _M0L6_2atmpS3078 >= Moonbit_array_length(_M0L5bytesS1194)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3077 = _M0L5bytesS1194[_M0L6_2atmpS3078];
            _M0L6_2atmpS3076 = (int32_t)_M0L6_2atmpS3077;
            _M0L6_2atmpS3075 = _M0L6_2atmpS3076 & 63;
            _M0L6_2atmpS3074 = _M0L6_2atmpS3075 << 12;
            _M0L6_2atmpS3066 = _M0L6_2atmpS3073 | _M0L6_2atmpS3074;
            _M0L3valS3072 = _M0L1iS1197->$0;
            _M0L6_2atmpS3071 = _M0L3valS3072 + 2;
            if (
              _M0L6_2atmpS3071 < 0
              || _M0L6_2atmpS3071 >= Moonbit_array_length(_M0L5bytesS1194)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3070 = _M0L5bytesS1194[_M0L6_2atmpS3071];
            _M0L6_2atmpS3069 = (int32_t)_M0L6_2atmpS3070;
            _M0L6_2atmpS3068 = _M0L6_2atmpS3069 & 63;
            _M0L6_2atmpS3067 = _M0L6_2atmpS3068 << 6;
            _M0L6_2atmpS3060 = _M0L6_2atmpS3066 | _M0L6_2atmpS3067;
            _M0L3valS3065 = _M0L1iS1197->$0;
            _M0L6_2atmpS3064 = _M0L3valS3065 + 3;
            if (
              _M0L6_2atmpS3064 < 0
              || _M0L6_2atmpS3064 >= Moonbit_array_length(_M0L5bytesS1194)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3063 = _M0L5bytesS1194[_M0L6_2atmpS3064];
            _M0L6_2atmpS3062 = (int32_t)_M0L6_2atmpS3063;
            _M0L6_2atmpS3061 = _M0L6_2atmpS3062 & 63;
            _M0L6_2atmpS3059 = _M0L6_2atmpS3060 | _M0L6_2atmpS3061;
            _M0L1cS1198->$0 = _M0L6_2atmpS3059;
            _M0L3valS3083 = _M0L1cS1198->$0;
            _M0L6_2atmpS3082 = _M0L3valS3083 - 65536;
            _M0L1cS1198->$0 = _M0L6_2atmpS3082;
            _M0L3valS3087 = _M0L1cS1198->$0;
            _M0L6_2atmpS3086 = _M0L3valS3087 >> 10;
            _M0L6_2atmpS3085 = _M0L6_2atmpS3086 + 55296;
            _M0L6_2atmpS3084 = _M0L6_2atmpS3085;
            moonbit_incref(_M0L3resS1195);
            #line 242 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1195, _M0L6_2atmpS3084);
            _M0L8_2afieldS3208 = _M0L1cS1198->$0;
            moonbit_decref(_M0L1cS1198);
            _M0L3valS3091 = _M0L8_2afieldS3208;
            _M0L6_2atmpS3090 = _M0L3valS3091 & 1023;
            _M0L6_2atmpS3089 = _M0L6_2atmpS3090 + 56320;
            _M0L6_2atmpS3088 = _M0L6_2atmpS3089;
            moonbit_incref(_M0L3resS1195);
            #line 243 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1195, _M0L6_2atmpS3088);
            _M0L3valS3093 = _M0L1iS1197->$0;
            _M0L6_2atmpS3092 = _M0L3valS3093 + 4;
            _M0L1iS1197->$0 = _M0L6_2atmpS3092;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1197);
      moonbit_decref(_M0L5bytesS1194);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1195);
}

int32_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1187(
  int32_t _M0L6_2aenvS3004,
  moonbit_string_t _M0L1sS1188
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1189;
  int32_t _M0L3lenS1190;
  int32_t _M0L1iS1191;
  int32_t _M0L8_2afieldS3209;
  #line 197 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1189
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1189)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1189->$0 = 0;
  _M0L3lenS1190 = Moonbit_array_length(_M0L1sS1188);
  _M0L1iS1191 = 0;
  while (1) {
    if (_M0L1iS1191 < _M0L3lenS1190) {
      int32_t _M0L3valS3009 = _M0L3resS1189->$0;
      int32_t _M0L6_2atmpS3006 = _M0L3valS3009 * 10;
      int32_t _M0L6_2atmpS3008;
      int32_t _M0L6_2atmpS3007;
      int32_t _M0L6_2atmpS3005;
      int32_t _M0L6_2atmpS3010;
      if (
        _M0L1iS1191 < 0 || _M0L1iS1191 >= Moonbit_array_length(_M0L1sS1188)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3008 = _M0L1sS1188[_M0L1iS1191];
      _M0L6_2atmpS3007 = _M0L6_2atmpS3008 - 48;
      _M0L6_2atmpS3005 = _M0L6_2atmpS3006 + _M0L6_2atmpS3007;
      _M0L3resS1189->$0 = _M0L6_2atmpS3005;
      _M0L6_2atmpS3010 = _M0L1iS1191 + 1;
      _M0L1iS1191 = _M0L6_2atmpS3010;
      continue;
    } else {
      moonbit_decref(_M0L1sS1188);
    }
    break;
  }
  _M0L8_2afieldS3209 = _M0L3resS1189->$0;
  moonbit_decref(_M0L3resS1189);
  return _M0L8_2afieldS3209;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1167,
  moonbit_string_t _M0L12_2adiscard__S1168,
  int32_t _M0L12_2adiscard__S1169,
  struct _M0TWssbEu* _M0L12_2adiscard__S1170,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1171
) {
  struct moonbit_result_0 _result_3586;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1171);
  moonbit_decref(_M0L12_2adiscard__S1170);
  moonbit_decref(_M0L12_2adiscard__S1168);
  moonbit_decref(_M0L12_2adiscard__S1167);
  _result_3586.tag = 1;
  _result_3586.data.ok = 0;
  return _result_3586;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1172,
  moonbit_string_t _M0L12_2adiscard__S1173,
  int32_t _M0L12_2adiscard__S1174,
  struct _M0TWssbEu* _M0L12_2adiscard__S1175,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1176
) {
  struct moonbit_result_0 _result_3587;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1176);
  moonbit_decref(_M0L12_2adiscard__S1175);
  moonbit_decref(_M0L12_2adiscard__S1173);
  moonbit_decref(_M0L12_2adiscard__S1172);
  _result_3587.tag = 1;
  _result_3587.data.ok = 0;
  return _result_3587;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1177,
  moonbit_string_t _M0L12_2adiscard__S1178,
  int32_t _M0L12_2adiscard__S1179,
  struct _M0TWssbEu* _M0L12_2adiscard__S1180,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1181
) {
  struct moonbit_result_0 _result_3588;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1181);
  moonbit_decref(_M0L12_2adiscard__S1180);
  moonbit_decref(_M0L12_2adiscard__S1178);
  moonbit_decref(_M0L12_2adiscard__S1177);
  _result_3588.tag = 1;
  _result_3588.data.ok = 0;
  return _result_3588;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1182,
  moonbit_string_t _M0L12_2adiscard__S1183,
  int32_t _M0L12_2adiscard__S1184,
  struct _M0TWssbEu* _M0L12_2adiscard__S1185,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1186
) {
  struct moonbit_result_0 _result_3589;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1186);
  moonbit_decref(_M0L12_2adiscard__S1185);
  moonbit_decref(_M0L12_2adiscard__S1183);
  moonbit_decref(_M0L12_2adiscard__S1182);
  _result_3589.tag = 1;
  _result_3589.data.ok = 0;
  return _result_3589;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal6openai28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6openai34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1166
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1166);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai25____test__61692e6d6274__0(
  
) {
  moonbit_string_t _M0L7contentS1165;
  moonbit_string_t _M0L6_2atmpS3003;
  struct _M0TPB6ToJson _M0L6_2atmpS2991;
  void* _M0L6_2atmpS3002;
  void** _M0L6_2atmpS3001;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3000;
  void* _M0L6_2atmpS2999;
  void* _M0L6_2atmpS2992;
  moonbit_string_t _M0L6_2atmpS2995;
  moonbit_string_t _M0L6_2atmpS2996;
  moonbit_string_t _M0L6_2atmpS2997;
  moonbit_string_t _M0L6_2atmpS2998;
  moonbit_string_t* _M0L6_2atmpS2994;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2993;
  #line 730 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L7contentS1165 = (moonbit_string_t)moonbit_string_literal_9.data;
  #line 744 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS3003
  = _M0FP48clawteam8clawteam8internal6openai27extract__first__json__block(_M0L7contentS1165);
  _M0L6_2atmpS2991
  = (struct _M0TPB6ToJson){
    _M0FP0123moonbitlang_2fcore_2foption_2fOption_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3003
  };
  #line 745 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS3002
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L6_2atmpS3001 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3001[0] = _M0L6_2atmpS3002;
  _M0L6_2atmpS3000
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3000)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3000->$0 = _M0L6_2atmpS3001;
  _M0L6_2atmpS3000->$1 = 1;
  #line 744 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2999 = _M0MPC14json4Json5array(_M0L6_2atmpS3000);
  _M0L6_2atmpS2992 = _M0L6_2atmpS2999;
  _M0L6_2atmpS2995 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS2996 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS2997 = 0;
  _M0L6_2atmpS2998 = 0;
  _M0L6_2atmpS2994 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2994[0] = _M0L6_2atmpS2995;
  _M0L6_2atmpS2994[1] = _M0L6_2atmpS2996;
  _M0L6_2atmpS2994[2] = _M0L6_2atmpS2997;
  _M0L6_2atmpS2994[3] = _M0L6_2atmpS2998;
  _M0L6_2atmpS2993
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2993)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2993->$0 = _M0L6_2atmpS2994;
  _M0L6_2atmpS2993->$1 = 4;
  #line 744 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2991, _M0L6_2atmpS2992, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2993);
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai27extract__first__json__block(
  moonbit_string_t _M0L7contentS1159
) {
  moonbit_string_t _M0L7_2abindS1160;
  int32_t _M0L6_2atmpS2990;
  struct _M0TPC16string10StringView _M0L6_2atmpS2989;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2988;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2987;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2986;
  void* _M0L6_2atmpS2971;
  struct _M0TWRPC16string10StringViewEOs* _M0L6_2atmpS2972;
  #line 714 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L7_2abindS1160 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS2990 = Moonbit_array_length(_M0L7_2abindS1160);
  _M0L6_2atmpS2989
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2990, _M0L7_2abindS1160
  };
  #line 715 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2988
  = _M0MPC16string6String5split(_M0L7contentS1159, _M0L6_2atmpS2989);
  #line 715 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2987
  = _M0MPB4Iter4dropGRPC16string10StringViewE(_M0L6_2atmpS2988, 1);
  #line 715 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2986
  = _M0MPB4Iter4takeGRPC16string10StringViewE(_M0L6_2atmpS2987, 1);
  #line 715 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2971
  = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L6_2atmpS2986);
  _M0L6_2atmpS2972
  = (struct _M0TWRPC16string10StringViewEOs*)&_M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2973l720$closure.data;
  #line 715 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  return _M0MPC16option6Option4bindGRPC16string10StringViewsE(_M0L6_2atmpS2971, _M0L6_2atmpS2972);
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2973l720(
  struct _M0TWRPC16string10StringViewEOs* _M0L6_2aenvS2974,
  struct _M0TPC16string10StringView _M0L5blockS1161
) {
  moonbit_string_t _M0L7_2abindS1162;
  int32_t _M0L6_2atmpS2985;
  struct _M0TPC16string10StringView _M0L6_2atmpS2984;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2983;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2982;
  void* _M0L6_2atmpS2975;
  struct _M0TWRPC16string10StringViewEs* _M0L6_2atmpS2976;
  #line 720 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  moonbit_decref(_M0L6_2aenvS2974);
  _M0L7_2abindS1162 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS2985 = Moonbit_array_length(_M0L7_2abindS1162);
  _M0L6_2atmpS2984
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2985, _M0L7_2abindS1162
  };
  #line 721 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2983
  = _M0MPC16string10StringView5split(_M0L5blockS1161, _M0L6_2atmpS2984);
  #line 721 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2982
  = _M0MPB4Iter4takeGRPC16string10StringViewE(_M0L6_2atmpS2983, 1);
  #line 721 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2975
  = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L6_2atmpS2982);
  _M0L6_2atmpS2976
  = (struct _M0TWRPC16string10StringViewEs*)&_M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2977l725$closure.data;
  #line 721 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  return _M0MPC16option6Option3mapGRPC16string10StringViewsE(_M0L6_2atmpS2975, _M0L6_2atmpS2976);
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai27extract__first__json__blockC2977l725(
  struct _M0TWRPC16string10StringViewEs* _M0L6_2aenvS2978,
  struct _M0TPC16string10StringView _M0L1sS1163
) {
  moonbit_string_t _M0L7_2abindS1164;
  int32_t _M0L6_2atmpS2981;
  struct _M0TPC16string10StringView _M0L6_2atmpS2980;
  struct _M0TPC16string10StringView _M0L6_2atmpS2979;
  #line 725 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  moonbit_decref(_M0L6_2aenvS2978);
  _M0L7_2abindS1164 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS2981 = Moonbit_array_length(_M0L7_2abindS1164);
  _M0L6_2atmpS2980
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2981, _M0L7_2abindS1164
  };
  #line 725 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  _M0L6_2atmpS2979
  = _M0MPC16string10StringView12trim_2einner(_M0L1sS1163, _M0L6_2atmpS2980);
  #line 725 "E:\\moonbit\\clawteam\\internal\\openai\\ai.mbt"
  return _M0IPC16string10StringViewPB4Show10to__string(_M0L6_2atmpS2979);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1154,
  void* _M0L7contentS1156,
  moonbit_string_t _M0L3locS1150,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1152
) {
  moonbit_string_t _M0L3locS1149;
  moonbit_string_t _M0L9args__locS1151;
  void* _M0L6_2atmpS2969;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2970;
  moonbit_string_t _M0L6actualS1153;
  moonbit_string_t _M0L4wantS1155;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1149 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1150);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1151 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1152);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2969 = _M0L3objS1154.$0->$method_0(_M0L3objS1154.$1);
  _M0L6_2atmpS2970 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1153
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2969, 0, 0, _M0L6_2atmpS2970);
  if (_M0L7contentS1156 == 0) {
    void* _M0L6_2atmpS2966;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2967;
    if (_M0L7contentS1156) {
      moonbit_decref(_M0L7contentS1156);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2966
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2967 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1155
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2966, 0, 0, _M0L6_2atmpS2967);
  } else {
    void* _M0L7_2aSomeS1157 = _M0L7contentS1156;
    void* _M0L4_2axS1158 = _M0L7_2aSomeS1157;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2968 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1155
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1158, 0, 0, _M0L6_2atmpS2968);
  }
  moonbit_incref(_M0L4wantS1155);
  moonbit_incref(_M0L6actualS1153);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1153, _M0L4wantS1155)
  ) {
    moonbit_string_t _M0L6_2atmpS2964;
    moonbit_string_t _M0L6_2atmpS3217;
    moonbit_string_t _M0L6_2atmpS2963;
    moonbit_string_t _M0L6_2atmpS3216;
    moonbit_string_t _M0L6_2atmpS2961;
    moonbit_string_t _M0L6_2atmpS2962;
    moonbit_string_t _M0L6_2atmpS3215;
    moonbit_string_t _M0L6_2atmpS2960;
    moonbit_string_t _M0L6_2atmpS3214;
    moonbit_string_t _M0L6_2atmpS2957;
    moonbit_string_t _M0L6_2atmpS2959;
    moonbit_string_t _M0L6_2atmpS2958;
    moonbit_string_t _M0L6_2atmpS3213;
    moonbit_string_t _M0L6_2atmpS2956;
    moonbit_string_t _M0L6_2atmpS3212;
    moonbit_string_t _M0L6_2atmpS2953;
    moonbit_string_t _M0L6_2atmpS2955;
    moonbit_string_t _M0L6_2atmpS2954;
    moonbit_string_t _M0L6_2atmpS3211;
    moonbit_string_t _M0L6_2atmpS2952;
    moonbit_string_t _M0L6_2atmpS3210;
    moonbit_string_t _M0L6_2atmpS2951;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2950;
    struct moonbit_result_0 _result_3590;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2964
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1149);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3217
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS2964);
    moonbit_decref(_M0L6_2atmpS2964);
    _M0L6_2atmpS2963 = _M0L6_2atmpS3217;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3216
    = moonbit_add_string(_M0L6_2atmpS2963, (moonbit_string_t)moonbit_string_literal_18.data);
    moonbit_decref(_M0L6_2atmpS2963);
    _M0L6_2atmpS2961 = _M0L6_2atmpS3216;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2962
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1151);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3215 = moonbit_add_string(_M0L6_2atmpS2961, _M0L6_2atmpS2962);
    moonbit_decref(_M0L6_2atmpS2961);
    moonbit_decref(_M0L6_2atmpS2962);
    _M0L6_2atmpS2960 = _M0L6_2atmpS3215;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3214
    = moonbit_add_string(_M0L6_2atmpS2960, (moonbit_string_t)moonbit_string_literal_19.data);
    moonbit_decref(_M0L6_2atmpS2960);
    _M0L6_2atmpS2957 = _M0L6_2atmpS3214;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2959 = _M0MPC16string6String6escape(_M0L4wantS1155);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2958
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2959);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3213 = moonbit_add_string(_M0L6_2atmpS2957, _M0L6_2atmpS2958);
    moonbit_decref(_M0L6_2atmpS2957);
    moonbit_decref(_M0L6_2atmpS2958);
    _M0L6_2atmpS2956 = _M0L6_2atmpS3213;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3212
    = moonbit_add_string(_M0L6_2atmpS2956, (moonbit_string_t)moonbit_string_literal_20.data);
    moonbit_decref(_M0L6_2atmpS2956);
    _M0L6_2atmpS2953 = _M0L6_2atmpS3212;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2955 = _M0MPC16string6String6escape(_M0L6actualS1153);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2954
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2955);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3211 = moonbit_add_string(_M0L6_2atmpS2953, _M0L6_2atmpS2954);
    moonbit_decref(_M0L6_2atmpS2953);
    moonbit_decref(_M0L6_2atmpS2954);
    _M0L6_2atmpS2952 = _M0L6_2atmpS3211;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3210
    = moonbit_add_string(_M0L6_2atmpS2952, (moonbit_string_t)moonbit_string_literal_21.data);
    moonbit_decref(_M0L6_2atmpS2952);
    _M0L6_2atmpS2951 = _M0L6_2atmpS3210;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2950
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2950)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2950)->$0
    = _M0L6_2atmpS2951;
    _result_3590.tag = 0;
    _result_3590.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2950;
    return _result_3590;
  } else {
    int32_t _M0L6_2atmpS2965;
    struct moonbit_result_0 _result_3591;
    moonbit_decref(_M0L4wantS1155);
    moonbit_decref(_M0L6actualS1153);
    moonbit_decref(_M0L9args__locS1151);
    moonbit_decref(_M0L3locS1149);
    _M0L6_2atmpS2965 = 0;
    _result_3591.tag = 1;
    _result_3591.data.ok = _M0L6_2atmpS2965;
    return _result_3591;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1148,
  int32_t _M0L13escape__slashS1120,
  int32_t _M0L6indentS1115,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1141
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1107;
  void** _M0L6_2atmpS2949;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1108;
  int32_t _M0Lm5depthS1109;
  void* _M0L6_2atmpS2948;
  void* _M0L8_2aparamS1110;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1107 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2949 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1108
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1108)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1108->$0 = _M0L6_2atmpS2949;
  _M0L5stackS1108->$1 = 0;
  _M0Lm5depthS1109 = 0;
  _M0L6_2atmpS2948 = _M0L4selfS1148;
  _M0L8_2aparamS1110 = _M0L6_2atmpS2948;
  _2aloop_1126:;
  while (1) {
    if (_M0L8_2aparamS1110 == 0) {
      int32_t _M0L3lenS2910;
      if (_M0L8_2aparamS1110) {
        moonbit_decref(_M0L8_2aparamS1110);
      }
      _M0L3lenS2910 = _M0L5stackS1108->$1;
      if (_M0L3lenS2910 == 0) {
        if (_M0L8replacerS1141) {
          moonbit_decref(_M0L8replacerS1141);
        }
        moonbit_decref(_M0L5stackS1108);
        break;
      } else {
        void** _M0L8_2afieldS3225 = _M0L5stackS1108->$0;
        void** _M0L3bufS2934 = _M0L8_2afieldS3225;
        int32_t _M0L3lenS2936 = _M0L5stackS1108->$1;
        int32_t _M0L6_2atmpS2935 = _M0L3lenS2936 - 1;
        void* _M0L6_2atmpS3224 = (void*)_M0L3bufS2934[_M0L6_2atmpS2935];
        void* _M0L4_2axS1127 = _M0L6_2atmpS3224;
        switch (Moonbit_object_tag(_M0L4_2axS1127)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1128 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1127;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3220 =
              _M0L8_2aArrayS1128->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1129 =
              _M0L8_2afieldS3220;
            int32_t _M0L4_2aiS1130 = _M0L8_2aArrayS1128->$1;
            int32_t _M0L3lenS2922 = _M0L6_2aarrS1129->$1;
            if (_M0L4_2aiS1130 < _M0L3lenS2922) {
              int32_t _if__result_3593;
              void** _M0L8_2afieldS3219;
              void** _M0L3bufS2928;
              void* _M0L6_2atmpS3218;
              void* _M0L7elementS1131;
              int32_t _M0L6_2atmpS2923;
              void* _M0L6_2atmpS2926;
              if (_M0L4_2aiS1130 < 0) {
                _if__result_3593 = 1;
              } else {
                int32_t _M0L3lenS2927 = _M0L6_2aarrS1129->$1;
                _if__result_3593 = _M0L4_2aiS1130 >= _M0L3lenS2927;
              }
              if (_if__result_3593) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3219 = _M0L6_2aarrS1129->$0;
              _M0L3bufS2928 = _M0L8_2afieldS3219;
              _M0L6_2atmpS3218 = (void*)_M0L3bufS2928[_M0L4_2aiS1130];
              _M0L7elementS1131 = _M0L6_2atmpS3218;
              _M0L6_2atmpS2923 = _M0L4_2aiS1130 + 1;
              _M0L8_2aArrayS1128->$1 = _M0L6_2atmpS2923;
              if (_M0L4_2aiS1130 > 0) {
                int32_t _M0L6_2atmpS2925;
                moonbit_string_t _M0L6_2atmpS2924;
                moonbit_incref(_M0L7elementS1131);
                moonbit_incref(_M0L3bufS1107);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 44);
                _M0L6_2atmpS2925 = _M0Lm5depthS1109;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2924
                = _M0FPC14json11indent__str(_M0L6_2atmpS2925, _M0L6indentS1115);
                moonbit_incref(_M0L3bufS1107);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2924);
              } else {
                moonbit_incref(_M0L7elementS1131);
              }
              _M0L6_2atmpS2926 = _M0L7elementS1131;
              _M0L8_2aparamS1110 = _M0L6_2atmpS2926;
              goto _2aloop_1126;
            } else {
              int32_t _M0L6_2atmpS2929 = _M0Lm5depthS1109;
              void* _M0L6_2atmpS2930;
              int32_t _M0L6_2atmpS2932;
              moonbit_string_t _M0L6_2atmpS2931;
              void* _M0L6_2atmpS2933;
              _M0Lm5depthS1109 = _M0L6_2atmpS2929 - 1;
              moonbit_incref(_M0L5stackS1108);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2930
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1108);
              if (_M0L6_2atmpS2930) {
                moonbit_decref(_M0L6_2atmpS2930);
              }
              _M0L6_2atmpS2932 = _M0Lm5depthS1109;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2931
              = _M0FPC14json11indent__str(_M0L6_2atmpS2932, _M0L6indentS1115);
              moonbit_incref(_M0L3bufS1107);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2931);
              moonbit_incref(_M0L3bufS1107);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 93);
              _M0L6_2atmpS2933 = 0;
              _M0L8_2aparamS1110 = _M0L6_2atmpS2933;
              goto _2aloop_1126;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1132 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1127;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3223 =
              _M0L9_2aObjectS1132->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1133 =
              _M0L8_2afieldS3223;
            int32_t _M0L8_2afirstS1134 = _M0L9_2aObjectS1132->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1135;
            moonbit_incref(_M0L11_2aiteratorS1133);
            moonbit_incref(_M0L9_2aObjectS1132);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1135
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1133);
            if (_M0L7_2abindS1135 == 0) {
              int32_t _M0L6_2atmpS2911;
              void* _M0L6_2atmpS2912;
              int32_t _M0L6_2atmpS2914;
              moonbit_string_t _M0L6_2atmpS2913;
              void* _M0L6_2atmpS2915;
              if (_M0L7_2abindS1135) {
                moonbit_decref(_M0L7_2abindS1135);
              }
              moonbit_decref(_M0L9_2aObjectS1132);
              _M0L6_2atmpS2911 = _M0Lm5depthS1109;
              _M0Lm5depthS1109 = _M0L6_2atmpS2911 - 1;
              moonbit_incref(_M0L5stackS1108);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2912
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1108);
              if (_M0L6_2atmpS2912) {
                moonbit_decref(_M0L6_2atmpS2912);
              }
              _M0L6_2atmpS2914 = _M0Lm5depthS1109;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2913
              = _M0FPC14json11indent__str(_M0L6_2atmpS2914, _M0L6indentS1115);
              moonbit_incref(_M0L3bufS1107);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2913);
              moonbit_incref(_M0L3bufS1107);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 125);
              _M0L6_2atmpS2915 = 0;
              _M0L8_2aparamS1110 = _M0L6_2atmpS2915;
              goto _2aloop_1126;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1136 = _M0L7_2abindS1135;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1137 = _M0L7_2aSomeS1136;
              moonbit_string_t _M0L8_2afieldS3222 = _M0L4_2axS1137->$0;
              moonbit_string_t _M0L4_2akS1138 = _M0L8_2afieldS3222;
              void* _M0L8_2afieldS3221 = _M0L4_2axS1137->$1;
              int32_t _M0L6_2acntS3473 =
                Moonbit_object_header(_M0L4_2axS1137)->rc;
              void* _M0L4_2avS1139;
              void* _M0Lm2v2S1140;
              moonbit_string_t _M0L6_2atmpS2919;
              void* _M0L6_2atmpS2921;
              void* _M0L6_2atmpS2920;
              if (_M0L6_2acntS3473 > 1) {
                int32_t _M0L11_2anew__cntS3474 = _M0L6_2acntS3473 - 1;
                Moonbit_object_header(_M0L4_2axS1137)->rc
                = _M0L11_2anew__cntS3474;
                moonbit_incref(_M0L8_2afieldS3221);
                moonbit_incref(_M0L4_2akS1138);
              } else if (_M0L6_2acntS3473 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1137);
              }
              _M0L4_2avS1139 = _M0L8_2afieldS3221;
              _M0Lm2v2S1140 = _M0L4_2avS1139;
              if (_M0L8replacerS1141 == 0) {
                moonbit_incref(_M0Lm2v2S1140);
                moonbit_decref(_M0L4_2avS1139);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1142 =
                  _M0L8replacerS1141;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1143 =
                  _M0L7_2aSomeS1142;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1144 =
                  _M0L11_2areplacerS1143;
                void* _M0L7_2abindS1145;
                moonbit_incref(_M0L7_2afuncS1144);
                moonbit_incref(_M0L4_2akS1138);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1145
                = _M0L7_2afuncS1144->code(_M0L7_2afuncS1144, _M0L4_2akS1138, _M0L4_2avS1139);
                if (_M0L7_2abindS1145 == 0) {
                  void* _M0L6_2atmpS2916;
                  if (_M0L7_2abindS1145) {
                    moonbit_decref(_M0L7_2abindS1145);
                  }
                  moonbit_decref(_M0L4_2akS1138);
                  moonbit_decref(_M0L9_2aObjectS1132);
                  _M0L6_2atmpS2916 = 0;
                  _M0L8_2aparamS1110 = _M0L6_2atmpS2916;
                  goto _2aloop_1126;
                } else {
                  void* _M0L7_2aSomeS1146 = _M0L7_2abindS1145;
                  void* _M0L4_2avS1147 = _M0L7_2aSomeS1146;
                  _M0Lm2v2S1140 = _M0L4_2avS1147;
                }
              }
              if (!_M0L8_2afirstS1134) {
                int32_t _M0L6_2atmpS2918;
                moonbit_string_t _M0L6_2atmpS2917;
                moonbit_incref(_M0L3bufS1107);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 44);
                _M0L6_2atmpS2918 = _M0Lm5depthS1109;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2917
                = _M0FPC14json11indent__str(_M0L6_2atmpS2918, _M0L6indentS1115);
                moonbit_incref(_M0L3bufS1107);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2917);
              }
              moonbit_incref(_M0L3bufS1107);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2919
              = _M0FPC14json6escape(_M0L4_2akS1138, _M0L13escape__slashS1120);
              moonbit_incref(_M0L3bufS1107);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2919);
              moonbit_incref(_M0L3bufS1107);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 34);
              moonbit_incref(_M0L3bufS1107);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 58);
              if (_M0L6indentS1115 > 0) {
                moonbit_incref(_M0L3bufS1107);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 32);
              }
              _M0L9_2aObjectS1132->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1132);
              _M0L6_2atmpS2921 = _M0Lm2v2S1140;
              _M0L6_2atmpS2920 = _M0L6_2atmpS2921;
              _M0L8_2aparamS1110 = _M0L6_2atmpS2920;
              goto _2aloop_1126;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1111 = _M0L8_2aparamS1110;
      void* _M0L8_2avalueS1112 = _M0L7_2aSomeS1111;
      void* _M0L6_2atmpS2947;
      switch (Moonbit_object_tag(_M0L8_2avalueS1112)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1113 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1112;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3226 =
            _M0L9_2aObjectS1113->$0;
          int32_t _M0L6_2acntS3475 =
            Moonbit_object_header(_M0L9_2aObjectS1113)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1114;
          if (_M0L6_2acntS3475 > 1) {
            int32_t _M0L11_2anew__cntS3476 = _M0L6_2acntS3475 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1113)->rc
            = _M0L11_2anew__cntS3476;
            moonbit_incref(_M0L8_2afieldS3226);
          } else if (_M0L6_2acntS3475 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1113);
          }
          _M0L10_2amembersS1114 = _M0L8_2afieldS3226;
          moonbit_incref(_M0L10_2amembersS1114);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1114)) {
            moonbit_decref(_M0L10_2amembersS1114);
            moonbit_incref(_M0L3bufS1107);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, (moonbit_string_t)moonbit_string_literal_22.data);
          } else {
            int32_t _M0L6_2atmpS2942 = _M0Lm5depthS1109;
            int32_t _M0L6_2atmpS2944;
            moonbit_string_t _M0L6_2atmpS2943;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2946;
            void* _M0L6ObjectS2945;
            _M0Lm5depthS1109 = _M0L6_2atmpS2942 + 1;
            moonbit_incref(_M0L3bufS1107);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 123);
            _M0L6_2atmpS2944 = _M0Lm5depthS1109;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2943
            = _M0FPC14json11indent__str(_M0L6_2atmpS2944, _M0L6indentS1115);
            moonbit_incref(_M0L3bufS1107);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2943);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2946
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1114);
            _M0L6ObjectS2945
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2945)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2945)->$0
            = _M0L6_2atmpS2946;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2945)->$1
            = 1;
            moonbit_incref(_M0L5stackS1108);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1108, _M0L6ObjectS2945);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1116 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1112;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3227 =
            _M0L8_2aArrayS1116->$0;
          int32_t _M0L6_2acntS3477 =
            Moonbit_object_header(_M0L8_2aArrayS1116)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1117;
          if (_M0L6_2acntS3477 > 1) {
            int32_t _M0L11_2anew__cntS3478 = _M0L6_2acntS3477 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1116)->rc
            = _M0L11_2anew__cntS3478;
            moonbit_incref(_M0L8_2afieldS3227);
          } else if (_M0L6_2acntS3477 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1116);
          }
          _M0L6_2aarrS1117 = _M0L8_2afieldS3227;
          moonbit_incref(_M0L6_2aarrS1117);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1117)) {
            moonbit_decref(_M0L6_2aarrS1117);
            moonbit_incref(_M0L3bufS1107);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, (moonbit_string_t)moonbit_string_literal_23.data);
          } else {
            int32_t _M0L6_2atmpS2938 = _M0Lm5depthS1109;
            int32_t _M0L6_2atmpS2940;
            moonbit_string_t _M0L6_2atmpS2939;
            void* _M0L5ArrayS2941;
            _M0Lm5depthS1109 = _M0L6_2atmpS2938 + 1;
            moonbit_incref(_M0L3bufS1107);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 91);
            _M0L6_2atmpS2940 = _M0Lm5depthS1109;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2939
            = _M0FPC14json11indent__str(_M0L6_2atmpS2940, _M0L6indentS1115);
            moonbit_incref(_M0L3bufS1107);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2939);
            _M0L5ArrayS2941
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2941)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2941)->$0
            = _M0L6_2aarrS1117;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2941)->$1
            = 0;
            moonbit_incref(_M0L5stackS1108);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1108, _M0L5ArrayS2941);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1118 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1112;
          moonbit_string_t _M0L8_2afieldS3228 = _M0L9_2aStringS1118->$0;
          int32_t _M0L6_2acntS3479 =
            Moonbit_object_header(_M0L9_2aStringS1118)->rc;
          moonbit_string_t _M0L4_2asS1119;
          moonbit_string_t _M0L6_2atmpS2937;
          if (_M0L6_2acntS3479 > 1) {
            int32_t _M0L11_2anew__cntS3480 = _M0L6_2acntS3479 - 1;
            Moonbit_object_header(_M0L9_2aStringS1118)->rc
            = _M0L11_2anew__cntS3480;
            moonbit_incref(_M0L8_2afieldS3228);
          } else if (_M0L6_2acntS3479 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1118);
          }
          _M0L4_2asS1119 = _M0L8_2afieldS3228;
          moonbit_incref(_M0L3bufS1107);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2937
          = _M0FPC14json6escape(_M0L4_2asS1119, _M0L13escape__slashS1120);
          moonbit_incref(_M0L3bufS1107);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L6_2atmpS2937);
          moonbit_incref(_M0L3bufS1107);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1107, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1121 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1112;
          double _M0L4_2anS1122 = _M0L9_2aNumberS1121->$0;
          moonbit_string_t _M0L8_2afieldS3229 = _M0L9_2aNumberS1121->$1;
          int32_t _M0L6_2acntS3481 =
            Moonbit_object_header(_M0L9_2aNumberS1121)->rc;
          moonbit_string_t _M0L7_2areprS1123;
          if (_M0L6_2acntS3481 > 1) {
            int32_t _M0L11_2anew__cntS3482 = _M0L6_2acntS3481 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1121)->rc
            = _M0L11_2anew__cntS3482;
            if (_M0L8_2afieldS3229) {
              moonbit_incref(_M0L8_2afieldS3229);
            }
          } else if (_M0L6_2acntS3481 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1121);
          }
          _M0L7_2areprS1123 = _M0L8_2afieldS3229;
          if (_M0L7_2areprS1123 == 0) {
            if (_M0L7_2areprS1123) {
              moonbit_decref(_M0L7_2areprS1123);
            }
            moonbit_incref(_M0L3bufS1107);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1107, _M0L4_2anS1122);
          } else {
            moonbit_string_t _M0L7_2aSomeS1124 = _M0L7_2areprS1123;
            moonbit_string_t _M0L4_2arS1125 = _M0L7_2aSomeS1124;
            moonbit_incref(_M0L3bufS1107);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, _M0L4_2arS1125);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1107);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, (moonbit_string_t)moonbit_string_literal_24.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1107);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, (moonbit_string_t)moonbit_string_literal_25.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1112);
          moonbit_incref(_M0L3bufS1107);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1107, (moonbit_string_t)moonbit_string_literal_26.data);
          break;
        }
      }
      _M0L6_2atmpS2947 = 0;
      _M0L8_2aparamS1110 = _M0L6_2atmpS2947;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1107);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1106,
  int32_t _M0L6indentS1104
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1104 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1105 = _M0L6indentS1104 * _M0L5levelS1106;
    switch (_M0L6spacesS1105) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_27.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_28.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_29.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_30.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_31.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_32.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_33.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_34.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_35.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2909;
        moonbit_string_t _M0L6_2atmpS3230;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2909
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_36.data, _M0L6spacesS1105);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3230
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS2909);
        moonbit_decref(_M0L6_2atmpS2909);
        return _M0L6_2atmpS3230;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1096,
  int32_t _M0L13escape__slashS1101
) {
  int32_t _M0L6_2atmpS2908;
  struct _M0TPB13StringBuilder* _M0L3bufS1095;
  struct _M0TWEOc* _M0L5_2aitS1097;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2908 = Moonbit_array_length(_M0L3strS1096);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1095 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2908);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1097 = _M0MPC16string6String4iter(_M0L3strS1096);
  while (1) {
    int32_t _M0L7_2abindS1098;
    moonbit_incref(_M0L5_2aitS1097);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1098 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1097);
    if (_M0L7_2abindS1098 == -1) {
      moonbit_decref(_M0L5_2aitS1097);
    } else {
      int32_t _M0L7_2aSomeS1099 = _M0L7_2abindS1098;
      int32_t _M0L4_2acS1100 = _M0L7_2aSomeS1099;
      if (_M0L4_2acS1100 == 34) {
        moonbit_incref(_M0L3bufS1095);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_37.data);
      } else if (_M0L4_2acS1100 == 92) {
        moonbit_incref(_M0L3bufS1095);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_38.data);
      } else if (_M0L4_2acS1100 == 47) {
        if (_M0L13escape__slashS1101) {
          moonbit_incref(_M0L3bufS1095);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_39.data);
        } else {
          moonbit_incref(_M0L3bufS1095);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1095, _M0L4_2acS1100);
        }
      } else if (_M0L4_2acS1100 == 10) {
        moonbit_incref(_M0L3bufS1095);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_40.data);
      } else if (_M0L4_2acS1100 == 13) {
        moonbit_incref(_M0L3bufS1095);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_41.data);
      } else if (_M0L4_2acS1100 == 8) {
        moonbit_incref(_M0L3bufS1095);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_42.data);
      } else if (_M0L4_2acS1100 == 9) {
        moonbit_incref(_M0L3bufS1095);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_43.data);
      } else {
        int32_t _M0L4codeS1102 = _M0L4_2acS1100;
        if (_M0L4codeS1102 == 12) {
          moonbit_incref(_M0L3bufS1095);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_44.data);
        } else if (_M0L4codeS1102 < 32) {
          int32_t _M0L6_2atmpS2907;
          moonbit_string_t _M0L6_2atmpS2906;
          moonbit_incref(_M0L3bufS1095);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, (moonbit_string_t)moonbit_string_literal_45.data);
          _M0L6_2atmpS2907 = _M0L4codeS1102 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2906 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2907);
          moonbit_incref(_M0L3bufS1095);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1095, _M0L6_2atmpS2906);
        } else {
          moonbit_incref(_M0L3bufS1095);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1095, _M0L4_2acS1100);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1095);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1094
) {
  int32_t _M0L8_2afieldS3231;
  int32_t _M0L3lenS2905;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3231 = _M0L4selfS1094->$1;
  moonbit_decref(_M0L4selfS1094);
  _M0L3lenS2905 = _M0L8_2afieldS3231;
  return _M0L3lenS2905 == 0;
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
    void** _M0L8_2afieldS3235 = _M0L4selfS1091->$0;
    void** _M0L3bufS2904 = _M0L8_2afieldS3235;
    void* _M0L6_2atmpS3234 = (void*)_M0L3bufS2904[_M0L5indexS1092];
    void* _M0L1vS1093 = _M0L6_2atmpS3234;
    void** _M0L8_2afieldS3233 = _M0L4selfS1091->$0;
    void** _M0L3bufS2903 = _M0L8_2afieldS3233;
    void* _M0L6_2aoldS3232;
    if (
      _M0L5indexS1092 < 0
      || _M0L5indexS1092 >= Moonbit_array_length(_M0L3bufS2903)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3232 = (void*)_M0L3bufS2903[_M0L5indexS1092];
    moonbit_incref(_M0L1vS1093);
    moonbit_decref(_M0L6_2aoldS3232);
    if (
      _M0L5indexS1092 < 0
      || _M0L5indexS1092 >= Moonbit_array_length(_M0L3bufS2903)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2903[_M0L5indexS1092]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1091->$1 = _M0L5indexS1092;
    moonbit_decref(_M0L4selfS1091);
    return _M0L1vS1093;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1088,
  struct _M0TPB6Logger _M0L6loggerS1089
) {
  moonbit_string_t _M0L6_2atmpS2902;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2901;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2902 = _M0L4selfS1088;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2901 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2902);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2901, _M0L6loggerS1089);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1065,
  struct _M0TPB6Logger _M0L6loggerS1087
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3244;
  struct _M0TPC16string10StringView _M0L3pkgS1064;
  moonbit_string_t _M0L7_2adataS1066;
  int32_t _M0L8_2astartS1067;
  int32_t _M0L6_2atmpS2900;
  int32_t _M0L6_2aendS1068;
  int32_t _M0Lm9_2acursorS1069;
  int32_t _M0Lm13accept__stateS1070;
  int32_t _M0Lm10match__endS1071;
  int32_t _M0Lm20match__tag__saver__0S1072;
  int32_t _M0Lm6tag__0S1073;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1074;
  struct _M0TPC16string10StringView _M0L8_2afieldS3243;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1083;
  void* _M0L8_2afieldS3242;
  int32_t _M0L6_2acntS3483;
  void* _M0L16_2apackage__nameS1084;
  struct _M0TPC16string10StringView _M0L8_2afieldS3240;
  struct _M0TPC16string10StringView _M0L8filenameS2877;
  struct _M0TPC16string10StringView _M0L8_2afieldS3239;
  struct _M0TPC16string10StringView _M0L11start__lineS2878;
  struct _M0TPC16string10StringView _M0L8_2afieldS3238;
  struct _M0TPC16string10StringView _M0L13start__columnS2879;
  struct _M0TPC16string10StringView _M0L8_2afieldS3237;
  struct _M0TPC16string10StringView _M0L9end__lineS2880;
  struct _M0TPC16string10StringView _M0L8_2afieldS3236;
  int32_t _M0L6_2acntS3487;
  struct _M0TPC16string10StringView _M0L11end__columnS2881;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3244
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1065->$0_1, _M0L4selfS1065->$0_2, _M0L4selfS1065->$0_0
  };
  _M0L3pkgS1064 = _M0L8_2afieldS3244;
  moonbit_incref(_M0L3pkgS1064.$0);
  moonbit_incref(_M0L3pkgS1064.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1066 = _M0MPC16string10StringView4data(_M0L3pkgS1064);
  moonbit_incref(_M0L3pkgS1064.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1067
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1064);
  moonbit_incref(_M0L3pkgS1064.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2900 = _M0MPC16string10StringView6length(_M0L3pkgS1064);
  _M0L6_2aendS1068 = _M0L8_2astartS1067 + _M0L6_2atmpS2900;
  _M0Lm9_2acursorS1069 = _M0L8_2astartS1067;
  _M0Lm13accept__stateS1070 = -1;
  _M0Lm10match__endS1071 = -1;
  _M0Lm20match__tag__saver__0S1072 = -1;
  _M0Lm6tag__0S1073 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2892 = _M0Lm9_2acursorS1069;
    if (_M0L6_2atmpS2892 < _M0L6_2aendS1068) {
      int32_t _M0L6_2atmpS2899 = _M0Lm9_2acursorS1069;
      int32_t _M0L10next__charS1078;
      int32_t _M0L6_2atmpS2893;
      moonbit_incref(_M0L7_2adataS1066);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1078
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1066, _M0L6_2atmpS2899);
      _M0L6_2atmpS2893 = _M0Lm9_2acursorS1069;
      _M0Lm9_2acursorS1069 = _M0L6_2atmpS2893 + 1;
      if (_M0L10next__charS1078 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2894;
          _M0Lm6tag__0S1073 = _M0Lm9_2acursorS1069;
          _M0L6_2atmpS2894 = _M0Lm9_2acursorS1069;
          if (_M0L6_2atmpS2894 < _M0L6_2aendS1068) {
            int32_t _M0L6_2atmpS2898 = _M0Lm9_2acursorS1069;
            int32_t _M0L10next__charS1079;
            int32_t _M0L6_2atmpS2895;
            moonbit_incref(_M0L7_2adataS1066);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1079
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1066, _M0L6_2atmpS2898);
            _M0L6_2atmpS2895 = _M0Lm9_2acursorS1069;
            _M0Lm9_2acursorS1069 = _M0L6_2atmpS2895 + 1;
            if (_M0L10next__charS1079 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2896 = _M0Lm9_2acursorS1069;
                if (_M0L6_2atmpS2896 < _M0L6_2aendS1068) {
                  int32_t _M0L6_2atmpS2897 = _M0Lm9_2acursorS1069;
                  _M0Lm9_2acursorS1069 = _M0L6_2atmpS2897 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1072 = _M0Lm6tag__0S1073;
                  _M0Lm13accept__stateS1070 = 0;
                  _M0Lm10match__endS1071 = _M0Lm9_2acursorS1069;
                  goto join_1075;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1075;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1075;
    }
    break;
  }
  goto joinlet_3595;
  join_1075:;
  switch (_M0Lm13accept__stateS1070) {
    case 0: {
      int32_t _M0L6_2atmpS2890;
      int32_t _M0L6_2atmpS2889;
      int64_t _M0L6_2atmpS2886;
      int32_t _M0L6_2atmpS2888;
      int64_t _M0L6_2atmpS2887;
      struct _M0TPC16string10StringView _M0L13package__nameS1076;
      int64_t _M0L6_2atmpS2883;
      int32_t _M0L6_2atmpS2885;
      int64_t _M0L6_2atmpS2884;
      struct _M0TPC16string10StringView _M0L12module__nameS1077;
      void* _M0L4SomeS2882;
      moonbit_decref(_M0L3pkgS1064.$0);
      _M0L6_2atmpS2890 = _M0Lm20match__tag__saver__0S1072;
      _M0L6_2atmpS2889 = _M0L6_2atmpS2890 + 1;
      _M0L6_2atmpS2886 = (int64_t)_M0L6_2atmpS2889;
      _M0L6_2atmpS2888 = _M0Lm10match__endS1071;
      _M0L6_2atmpS2887 = (int64_t)_M0L6_2atmpS2888;
      moonbit_incref(_M0L7_2adataS1066);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1076
      = _M0MPC16string6String4view(_M0L7_2adataS1066, _M0L6_2atmpS2886, _M0L6_2atmpS2887);
      _M0L6_2atmpS2883 = (int64_t)_M0L8_2astartS1067;
      _M0L6_2atmpS2885 = _M0Lm20match__tag__saver__0S1072;
      _M0L6_2atmpS2884 = (int64_t)_M0L6_2atmpS2885;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1077
      = _M0MPC16string6String4view(_M0L7_2adataS1066, _M0L6_2atmpS2883, _M0L6_2atmpS2884);
      _M0L4SomeS2882
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2882)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2882)->$0_0
      = _M0L13package__nameS1076.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2882)->$0_1
      = _M0L13package__nameS1076.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2882)->$0_2
      = _M0L13package__nameS1076.$2;
      _M0L7_2abindS1074
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1074)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1074->$0_0 = _M0L12module__nameS1077.$0;
      _M0L7_2abindS1074->$0_1 = _M0L12module__nameS1077.$1;
      _M0L7_2abindS1074->$0_2 = _M0L12module__nameS1077.$2;
      _M0L7_2abindS1074->$1 = _M0L4SomeS2882;
      break;
    }
    default: {
      void* _M0L4NoneS2891;
      moonbit_decref(_M0L7_2adataS1066);
      _M0L4NoneS2891
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1074
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1074)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1074->$0_0 = _M0L3pkgS1064.$0;
      _M0L7_2abindS1074->$0_1 = _M0L3pkgS1064.$1;
      _M0L7_2abindS1074->$0_2 = _M0L3pkgS1064.$2;
      _M0L7_2abindS1074->$1 = _M0L4NoneS2891;
      break;
    }
  }
  joinlet_3595:;
  _M0L8_2afieldS3243
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1074->$0_1, _M0L7_2abindS1074->$0_2, _M0L7_2abindS1074->$0_0
  };
  _M0L15_2amodule__nameS1083 = _M0L8_2afieldS3243;
  _M0L8_2afieldS3242 = _M0L7_2abindS1074->$1;
  _M0L6_2acntS3483 = Moonbit_object_header(_M0L7_2abindS1074)->rc;
  if (_M0L6_2acntS3483 > 1) {
    int32_t _M0L11_2anew__cntS3484 = _M0L6_2acntS3483 - 1;
    Moonbit_object_header(_M0L7_2abindS1074)->rc = _M0L11_2anew__cntS3484;
    moonbit_incref(_M0L8_2afieldS3242);
    moonbit_incref(_M0L15_2amodule__nameS1083.$0);
  } else if (_M0L6_2acntS3483 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1074);
  }
  _M0L16_2apackage__nameS1084 = _M0L8_2afieldS3242;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1084)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1085 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1084;
      struct _M0TPC16string10StringView _M0L8_2afieldS3241 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1085->$0_1,
                                              _M0L7_2aSomeS1085->$0_2,
                                              _M0L7_2aSomeS1085->$0_0};
      int32_t _M0L6_2acntS3485 = Moonbit_object_header(_M0L7_2aSomeS1085)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1086;
      if (_M0L6_2acntS3485 > 1) {
        int32_t _M0L11_2anew__cntS3486 = _M0L6_2acntS3485 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1085)->rc = _M0L11_2anew__cntS3486;
        moonbit_incref(_M0L8_2afieldS3241.$0);
      } else if (_M0L6_2acntS3485 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1085);
      }
      _M0L12_2apkg__nameS1086 = _M0L8_2afieldS3241;
      if (_M0L6loggerS1087.$1) {
        moonbit_incref(_M0L6loggerS1087.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1087.$0->$method_2(_M0L6loggerS1087.$1, _M0L12_2apkg__nameS1086);
      if (_M0L6loggerS1087.$1) {
        moonbit_incref(_M0L6loggerS1087.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1087.$0->$method_3(_M0L6loggerS1087.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1084);
      break;
    }
  }
  _M0L8_2afieldS3240
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1065->$1_1, _M0L4selfS1065->$1_2, _M0L4selfS1065->$1_0
  };
  _M0L8filenameS2877 = _M0L8_2afieldS3240;
  moonbit_incref(_M0L8filenameS2877.$0);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_2(_M0L6loggerS1087.$1, _M0L8filenameS2877);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_3(_M0L6loggerS1087.$1, 58);
  _M0L8_2afieldS3239
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1065->$2_1, _M0L4selfS1065->$2_2, _M0L4selfS1065->$2_0
  };
  _M0L11start__lineS2878 = _M0L8_2afieldS3239;
  moonbit_incref(_M0L11start__lineS2878.$0);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_2(_M0L6loggerS1087.$1, _M0L11start__lineS2878);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_3(_M0L6loggerS1087.$1, 58);
  _M0L8_2afieldS3238
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1065->$3_1, _M0L4selfS1065->$3_2, _M0L4selfS1065->$3_0
  };
  _M0L13start__columnS2879 = _M0L8_2afieldS3238;
  moonbit_incref(_M0L13start__columnS2879.$0);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_2(_M0L6loggerS1087.$1, _M0L13start__columnS2879);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_3(_M0L6loggerS1087.$1, 45);
  _M0L8_2afieldS3237
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1065->$4_1, _M0L4selfS1065->$4_2, _M0L4selfS1065->$4_0
  };
  _M0L9end__lineS2880 = _M0L8_2afieldS3237;
  moonbit_incref(_M0L9end__lineS2880.$0);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_2(_M0L6loggerS1087.$1, _M0L9end__lineS2880);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_3(_M0L6loggerS1087.$1, 58);
  _M0L8_2afieldS3236
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1065->$5_1, _M0L4selfS1065->$5_2, _M0L4selfS1065->$5_0
  };
  _M0L6_2acntS3487 = Moonbit_object_header(_M0L4selfS1065)->rc;
  if (_M0L6_2acntS3487 > 1) {
    int32_t _M0L11_2anew__cntS3493 = _M0L6_2acntS3487 - 1;
    Moonbit_object_header(_M0L4selfS1065)->rc = _M0L11_2anew__cntS3493;
    moonbit_incref(_M0L8_2afieldS3236.$0);
  } else if (_M0L6_2acntS3487 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3492 =
      (struct _M0TPC16string10StringView){_M0L4selfS1065->$4_1,
                                            _M0L4selfS1065->$4_2,
                                            _M0L4selfS1065->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3491;
    struct _M0TPC16string10StringView _M0L8_2afieldS3490;
    struct _M0TPC16string10StringView _M0L8_2afieldS3489;
    struct _M0TPC16string10StringView _M0L8_2afieldS3488;
    moonbit_decref(_M0L8_2afieldS3492.$0);
    _M0L8_2afieldS3491
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1065->$3_1, _M0L4selfS1065->$3_2, _M0L4selfS1065->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3491.$0);
    _M0L8_2afieldS3490
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1065->$2_1, _M0L4selfS1065->$2_2, _M0L4selfS1065->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3490.$0);
    _M0L8_2afieldS3489
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1065->$1_1, _M0L4selfS1065->$1_2, _M0L4selfS1065->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3489.$0);
    _M0L8_2afieldS3488
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1065->$0_1, _M0L4selfS1065->$0_2, _M0L4selfS1065->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3488.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1065);
  }
  _M0L11end__columnS2881 = _M0L8_2afieldS3236;
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_2(_M0L6loggerS1087.$1, _M0L11end__columnS2881);
  if (_M0L6loggerS1087.$1) {
    moonbit_incref(_M0L6loggerS1087.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_3(_M0L6loggerS1087.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1087.$0->$method_2(_M0L6loggerS1087.$1, _M0L15_2amodule__nameS1083);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1063) {
  moonbit_string_t _M0L6_2atmpS2876;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2876
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1063);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2876);
  moonbit_decref(_M0L6_2atmpS2876);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1062,
  struct _M0TPB6Logger _M0L6loggerS1061
) {
  moonbit_string_t _M0L6_2atmpS2875;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2875 = _M0MPC16double6Double10to__string(_M0L4selfS1062);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1061.$0->$method_0(_M0L6loggerS1061.$1, _M0L6_2atmpS2875);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1060) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1060);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1047) {
  uint64_t _M0L4bitsS1048;
  uint64_t _M0L6_2atmpS2874;
  uint64_t _M0L6_2atmpS2873;
  int32_t _M0L8ieeeSignS1049;
  uint64_t _M0L12ieeeMantissaS1050;
  uint64_t _M0L6_2atmpS2872;
  uint64_t _M0L6_2atmpS2871;
  int32_t _M0L12ieeeExponentS1051;
  int32_t _if__result_3599;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1052;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1053;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2870;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1047 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_46.data;
  }
  _M0L4bitsS1048 = *(int64_t*)&_M0L3valS1047;
  _M0L6_2atmpS2874 = _M0L4bitsS1048 >> 63;
  _M0L6_2atmpS2873 = _M0L6_2atmpS2874 & 1ull;
  _M0L8ieeeSignS1049 = _M0L6_2atmpS2873 != 0ull;
  _M0L12ieeeMantissaS1050 = _M0L4bitsS1048 & 4503599627370495ull;
  _M0L6_2atmpS2872 = _M0L4bitsS1048 >> 52;
  _M0L6_2atmpS2871 = _M0L6_2atmpS2872 & 2047ull;
  _M0L12ieeeExponentS1051 = (int32_t)_M0L6_2atmpS2871;
  if (_M0L12ieeeExponentS1051 == 2047) {
    _if__result_3599 = 1;
  } else if (_M0L12ieeeExponentS1051 == 0) {
    _if__result_3599 = _M0L12ieeeMantissaS1050 == 0ull;
  } else {
    _if__result_3599 = 0;
  }
  if (_if__result_3599) {
    int32_t _M0L6_2atmpS2859 = _M0L12ieeeExponentS1051 != 0;
    int32_t _M0L6_2atmpS2860 = _M0L12ieeeMantissaS1050 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1049, _M0L6_2atmpS2859, _M0L6_2atmpS2860);
  }
  _M0Lm1vS1052 = _M0FPB31ryu__to__string_2erecord_2f1046;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1053
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1050, _M0L12ieeeExponentS1051);
  if (_M0L5smallS1053 == 0) {
    uint32_t _M0L6_2atmpS2861;
    if (_M0L5smallS1053) {
      moonbit_decref(_M0L5smallS1053);
    }
    _M0L6_2atmpS2861 = *(uint32_t*)&_M0L12ieeeExponentS1051;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1052 = _M0FPB3d2d(_M0L12ieeeMantissaS1050, _M0L6_2atmpS2861);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1054 = _M0L5smallS1053;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1055 = _M0L7_2aSomeS1054;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1056 = _M0L4_2afS1055;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2869 = _M0Lm1xS1056;
      uint64_t _M0L8_2afieldS3247 = _M0L6_2atmpS2869->$0;
      uint64_t _M0L8mantissaS2868 = _M0L8_2afieldS3247;
      uint64_t _M0L1qS1057 = _M0L8mantissaS2868 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2867 = _M0Lm1xS1056;
      uint64_t _M0L8_2afieldS3246 = _M0L6_2atmpS2867->$0;
      uint64_t _M0L8mantissaS2865 = _M0L8_2afieldS3246;
      uint64_t _M0L6_2atmpS2866 = 10ull * _M0L1qS1057;
      uint64_t _M0L1rS1058 = _M0L8mantissaS2865 - _M0L6_2atmpS2866;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2864;
      int32_t _M0L8_2afieldS3245;
      int32_t _M0L8exponentS2863;
      int32_t _M0L6_2atmpS2862;
      if (_M0L1rS1058 != 0ull) {
        break;
      }
      _M0L6_2atmpS2864 = _M0Lm1xS1056;
      _M0L8_2afieldS3245 = _M0L6_2atmpS2864->$1;
      moonbit_decref(_M0L6_2atmpS2864);
      _M0L8exponentS2863 = _M0L8_2afieldS3245;
      _M0L6_2atmpS2862 = _M0L8exponentS2863 + 1;
      _M0Lm1xS1056
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1056)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1056->$0 = _M0L1qS1057;
      _M0Lm1xS1056->$1 = _M0L6_2atmpS2862;
      continue;
      break;
    }
    _M0Lm1vS1052 = _M0Lm1xS1056;
  }
  _M0L6_2atmpS2870 = _M0Lm1vS1052;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2870, _M0L8ieeeSignS1049);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1041,
  int32_t _M0L12ieeeExponentS1043
) {
  uint64_t _M0L2m2S1040;
  int32_t _M0L6_2atmpS2858;
  int32_t _M0L2e2S1042;
  int32_t _M0L6_2atmpS2857;
  uint64_t _M0L6_2atmpS2856;
  uint64_t _M0L4maskS1044;
  uint64_t _M0L8fractionS1045;
  int32_t _M0L6_2atmpS2855;
  uint64_t _M0L6_2atmpS2854;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2853;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1040 = 4503599627370496ull | _M0L12ieeeMantissaS1041;
  _M0L6_2atmpS2858 = _M0L12ieeeExponentS1043 - 1023;
  _M0L2e2S1042 = _M0L6_2atmpS2858 - 52;
  if (_M0L2e2S1042 > 0) {
    return 0;
  }
  if (_M0L2e2S1042 < -52) {
    return 0;
  }
  _M0L6_2atmpS2857 = -_M0L2e2S1042;
  _M0L6_2atmpS2856 = 1ull << (_M0L6_2atmpS2857 & 63);
  _M0L4maskS1044 = _M0L6_2atmpS2856 - 1ull;
  _M0L8fractionS1045 = _M0L2m2S1040 & _M0L4maskS1044;
  if (_M0L8fractionS1045 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2855 = -_M0L2e2S1042;
  _M0L6_2atmpS2854 = _M0L2m2S1040 >> (_M0L6_2atmpS2855 & 63);
  _M0L6_2atmpS2853
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2853)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2853->$0 = _M0L6_2atmpS2854;
  _M0L6_2atmpS2853->$1 = 0;
  return _M0L6_2atmpS2853;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1014,
  int32_t _M0L4signS1012
) {
  int32_t _M0L6_2atmpS2852;
  moonbit_bytes_t _M0L6resultS1010;
  int32_t _M0Lm5indexS1011;
  uint64_t _M0Lm6outputS1013;
  uint64_t _M0L6_2atmpS2851;
  int32_t _M0L7olengthS1015;
  int32_t _M0L8_2afieldS3248;
  int32_t _M0L8exponentS2850;
  int32_t _M0L6_2atmpS2849;
  int32_t _M0Lm3expS1016;
  int32_t _M0L6_2atmpS2848;
  int32_t _M0L6_2atmpS2846;
  int32_t _M0L18scientificNotationS1017;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2852 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1010
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2852);
  _M0Lm5indexS1011 = 0;
  if (_M0L4signS1012) {
    int32_t _M0L6_2atmpS2721 = _M0Lm5indexS1011;
    int32_t _M0L6_2atmpS2722;
    if (
      _M0L6_2atmpS2721 < 0
      || _M0L6_2atmpS2721 >= Moonbit_array_length(_M0L6resultS1010)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1010[_M0L6_2atmpS2721] = 45;
    _M0L6_2atmpS2722 = _M0Lm5indexS1011;
    _M0Lm5indexS1011 = _M0L6_2atmpS2722 + 1;
  }
  _M0Lm6outputS1013 = _M0L1vS1014->$0;
  _M0L6_2atmpS2851 = _M0Lm6outputS1013;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1015 = _M0FPB17decimal__length17(_M0L6_2atmpS2851);
  _M0L8_2afieldS3248 = _M0L1vS1014->$1;
  moonbit_decref(_M0L1vS1014);
  _M0L8exponentS2850 = _M0L8_2afieldS3248;
  _M0L6_2atmpS2849 = _M0L8exponentS2850 + _M0L7olengthS1015;
  _M0Lm3expS1016 = _M0L6_2atmpS2849 - 1;
  _M0L6_2atmpS2848 = _M0Lm3expS1016;
  if (_M0L6_2atmpS2848 >= -6) {
    int32_t _M0L6_2atmpS2847 = _M0Lm3expS1016;
    _M0L6_2atmpS2846 = _M0L6_2atmpS2847 < 21;
  } else {
    _M0L6_2atmpS2846 = 0;
  }
  _M0L18scientificNotationS1017 = !_M0L6_2atmpS2846;
  if (_M0L18scientificNotationS1017) {
    int32_t _M0L7_2abindS1018 = _M0L7olengthS1015 - 1;
    int32_t _M0L1iS1019 = 0;
    int32_t _M0L6_2atmpS2732;
    uint64_t _M0L6_2atmpS2737;
    int32_t _M0L6_2atmpS2736;
    int32_t _M0L6_2atmpS2735;
    int32_t _M0L6_2atmpS2734;
    int32_t _M0L6_2atmpS2733;
    int32_t _M0L6_2atmpS2741;
    int32_t _M0L6_2atmpS2742;
    int32_t _M0L6_2atmpS2743;
    int32_t _M0L6_2atmpS2744;
    int32_t _M0L6_2atmpS2745;
    int32_t _M0L6_2atmpS2751;
    int32_t _M0L6_2atmpS2784;
    while (1) {
      if (_M0L1iS1019 < _M0L7_2abindS1018) {
        uint64_t _M0L6_2atmpS2730 = _M0Lm6outputS1013;
        uint64_t _M0L1cS1020 = _M0L6_2atmpS2730 % 10ull;
        uint64_t _M0L6_2atmpS2723 = _M0Lm6outputS1013;
        int32_t _M0L6_2atmpS2729;
        int32_t _M0L6_2atmpS2728;
        int32_t _M0L6_2atmpS2724;
        int32_t _M0L6_2atmpS2727;
        int32_t _M0L6_2atmpS2726;
        int32_t _M0L6_2atmpS2725;
        int32_t _M0L6_2atmpS2731;
        _M0Lm6outputS1013 = _M0L6_2atmpS2723 / 10ull;
        _M0L6_2atmpS2729 = _M0Lm5indexS1011;
        _M0L6_2atmpS2728 = _M0L6_2atmpS2729 + _M0L7olengthS1015;
        _M0L6_2atmpS2724 = _M0L6_2atmpS2728 - _M0L1iS1019;
        _M0L6_2atmpS2727 = (int32_t)_M0L1cS1020;
        _M0L6_2atmpS2726 = 48 + _M0L6_2atmpS2727;
        _M0L6_2atmpS2725 = _M0L6_2atmpS2726 & 0xff;
        if (
          _M0L6_2atmpS2724 < 0
          || _M0L6_2atmpS2724 >= Moonbit_array_length(_M0L6resultS1010)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1010[_M0L6_2atmpS2724] = _M0L6_2atmpS2725;
        _M0L6_2atmpS2731 = _M0L1iS1019 + 1;
        _M0L1iS1019 = _M0L6_2atmpS2731;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2732 = _M0Lm5indexS1011;
    _M0L6_2atmpS2737 = _M0Lm6outputS1013;
    _M0L6_2atmpS2736 = (int32_t)_M0L6_2atmpS2737;
    _M0L6_2atmpS2735 = _M0L6_2atmpS2736 % 10;
    _M0L6_2atmpS2734 = 48 + _M0L6_2atmpS2735;
    _M0L6_2atmpS2733 = _M0L6_2atmpS2734 & 0xff;
    if (
      _M0L6_2atmpS2732 < 0
      || _M0L6_2atmpS2732 >= Moonbit_array_length(_M0L6resultS1010)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1010[_M0L6_2atmpS2732] = _M0L6_2atmpS2733;
    if (_M0L7olengthS1015 > 1) {
      int32_t _M0L6_2atmpS2739 = _M0Lm5indexS1011;
      int32_t _M0L6_2atmpS2738 = _M0L6_2atmpS2739 + 1;
      if (
        _M0L6_2atmpS2738 < 0
        || _M0L6_2atmpS2738 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2738] = 46;
    } else {
      int32_t _M0L6_2atmpS2740 = _M0Lm5indexS1011;
      _M0Lm5indexS1011 = _M0L6_2atmpS2740 - 1;
    }
    _M0L6_2atmpS2741 = _M0Lm5indexS1011;
    _M0L6_2atmpS2742 = _M0L7olengthS1015 + 1;
    _M0Lm5indexS1011 = _M0L6_2atmpS2741 + _M0L6_2atmpS2742;
    _M0L6_2atmpS2743 = _M0Lm5indexS1011;
    if (
      _M0L6_2atmpS2743 < 0
      || _M0L6_2atmpS2743 >= Moonbit_array_length(_M0L6resultS1010)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1010[_M0L6_2atmpS2743] = 101;
    _M0L6_2atmpS2744 = _M0Lm5indexS1011;
    _M0Lm5indexS1011 = _M0L6_2atmpS2744 + 1;
    _M0L6_2atmpS2745 = _M0Lm3expS1016;
    if (_M0L6_2atmpS2745 < 0) {
      int32_t _M0L6_2atmpS2746 = _M0Lm5indexS1011;
      int32_t _M0L6_2atmpS2747;
      int32_t _M0L6_2atmpS2748;
      if (
        _M0L6_2atmpS2746 < 0
        || _M0L6_2atmpS2746 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2746] = 45;
      _M0L6_2atmpS2747 = _M0Lm5indexS1011;
      _M0Lm5indexS1011 = _M0L6_2atmpS2747 + 1;
      _M0L6_2atmpS2748 = _M0Lm3expS1016;
      _M0Lm3expS1016 = -_M0L6_2atmpS2748;
    } else {
      int32_t _M0L6_2atmpS2749 = _M0Lm5indexS1011;
      int32_t _M0L6_2atmpS2750;
      if (
        _M0L6_2atmpS2749 < 0
        || _M0L6_2atmpS2749 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2749] = 43;
      _M0L6_2atmpS2750 = _M0Lm5indexS1011;
      _M0Lm5indexS1011 = _M0L6_2atmpS2750 + 1;
    }
    _M0L6_2atmpS2751 = _M0Lm3expS1016;
    if (_M0L6_2atmpS2751 >= 100) {
      int32_t _M0L6_2atmpS2767 = _M0Lm3expS1016;
      int32_t _M0L1aS1022 = _M0L6_2atmpS2767 / 100;
      int32_t _M0L6_2atmpS2766 = _M0Lm3expS1016;
      int32_t _M0L6_2atmpS2765 = _M0L6_2atmpS2766 / 10;
      int32_t _M0L1bS1023 = _M0L6_2atmpS2765 % 10;
      int32_t _M0L6_2atmpS2764 = _M0Lm3expS1016;
      int32_t _M0L1cS1024 = _M0L6_2atmpS2764 % 10;
      int32_t _M0L6_2atmpS2752 = _M0Lm5indexS1011;
      int32_t _M0L6_2atmpS2754 = 48 + _M0L1aS1022;
      int32_t _M0L6_2atmpS2753 = _M0L6_2atmpS2754 & 0xff;
      int32_t _M0L6_2atmpS2758;
      int32_t _M0L6_2atmpS2755;
      int32_t _M0L6_2atmpS2757;
      int32_t _M0L6_2atmpS2756;
      int32_t _M0L6_2atmpS2762;
      int32_t _M0L6_2atmpS2759;
      int32_t _M0L6_2atmpS2761;
      int32_t _M0L6_2atmpS2760;
      int32_t _M0L6_2atmpS2763;
      if (
        _M0L6_2atmpS2752 < 0
        || _M0L6_2atmpS2752 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2752] = _M0L6_2atmpS2753;
      _M0L6_2atmpS2758 = _M0Lm5indexS1011;
      _M0L6_2atmpS2755 = _M0L6_2atmpS2758 + 1;
      _M0L6_2atmpS2757 = 48 + _M0L1bS1023;
      _M0L6_2atmpS2756 = _M0L6_2atmpS2757 & 0xff;
      if (
        _M0L6_2atmpS2755 < 0
        || _M0L6_2atmpS2755 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2755] = _M0L6_2atmpS2756;
      _M0L6_2atmpS2762 = _M0Lm5indexS1011;
      _M0L6_2atmpS2759 = _M0L6_2atmpS2762 + 2;
      _M0L6_2atmpS2761 = 48 + _M0L1cS1024;
      _M0L6_2atmpS2760 = _M0L6_2atmpS2761 & 0xff;
      if (
        _M0L6_2atmpS2759 < 0
        || _M0L6_2atmpS2759 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2759] = _M0L6_2atmpS2760;
      _M0L6_2atmpS2763 = _M0Lm5indexS1011;
      _M0Lm5indexS1011 = _M0L6_2atmpS2763 + 3;
    } else {
      int32_t _M0L6_2atmpS2768 = _M0Lm3expS1016;
      if (_M0L6_2atmpS2768 >= 10) {
        int32_t _M0L6_2atmpS2778 = _M0Lm3expS1016;
        int32_t _M0L1aS1025 = _M0L6_2atmpS2778 / 10;
        int32_t _M0L6_2atmpS2777 = _M0Lm3expS1016;
        int32_t _M0L1bS1026 = _M0L6_2atmpS2777 % 10;
        int32_t _M0L6_2atmpS2769 = _M0Lm5indexS1011;
        int32_t _M0L6_2atmpS2771 = 48 + _M0L1aS1025;
        int32_t _M0L6_2atmpS2770 = _M0L6_2atmpS2771 & 0xff;
        int32_t _M0L6_2atmpS2775;
        int32_t _M0L6_2atmpS2772;
        int32_t _M0L6_2atmpS2774;
        int32_t _M0L6_2atmpS2773;
        int32_t _M0L6_2atmpS2776;
        if (
          _M0L6_2atmpS2769 < 0
          || _M0L6_2atmpS2769 >= Moonbit_array_length(_M0L6resultS1010)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1010[_M0L6_2atmpS2769] = _M0L6_2atmpS2770;
        _M0L6_2atmpS2775 = _M0Lm5indexS1011;
        _M0L6_2atmpS2772 = _M0L6_2atmpS2775 + 1;
        _M0L6_2atmpS2774 = 48 + _M0L1bS1026;
        _M0L6_2atmpS2773 = _M0L6_2atmpS2774 & 0xff;
        if (
          _M0L6_2atmpS2772 < 0
          || _M0L6_2atmpS2772 >= Moonbit_array_length(_M0L6resultS1010)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1010[_M0L6_2atmpS2772] = _M0L6_2atmpS2773;
        _M0L6_2atmpS2776 = _M0Lm5indexS1011;
        _M0Lm5indexS1011 = _M0L6_2atmpS2776 + 2;
      } else {
        int32_t _M0L6_2atmpS2779 = _M0Lm5indexS1011;
        int32_t _M0L6_2atmpS2782 = _M0Lm3expS1016;
        int32_t _M0L6_2atmpS2781 = 48 + _M0L6_2atmpS2782;
        int32_t _M0L6_2atmpS2780 = _M0L6_2atmpS2781 & 0xff;
        int32_t _M0L6_2atmpS2783;
        if (
          _M0L6_2atmpS2779 < 0
          || _M0L6_2atmpS2779 >= Moonbit_array_length(_M0L6resultS1010)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1010[_M0L6_2atmpS2779] = _M0L6_2atmpS2780;
        _M0L6_2atmpS2783 = _M0Lm5indexS1011;
        _M0Lm5indexS1011 = _M0L6_2atmpS2783 + 1;
      }
    }
    _M0L6_2atmpS2784 = _M0Lm5indexS1011;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1010, 0, _M0L6_2atmpS2784);
  } else {
    int32_t _M0L6_2atmpS2785 = _M0Lm3expS1016;
    int32_t _M0L6_2atmpS2845;
    if (_M0L6_2atmpS2785 < 0) {
      int32_t _M0L6_2atmpS2786 = _M0Lm5indexS1011;
      int32_t _M0L6_2atmpS2787;
      int32_t _M0L6_2atmpS2788;
      int32_t _M0L6_2atmpS2789;
      int32_t _M0L1iS1027;
      int32_t _M0L7currentS1029;
      int32_t _M0L1iS1030;
      if (
        _M0L6_2atmpS2786 < 0
        || _M0L6_2atmpS2786 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2786] = 48;
      _M0L6_2atmpS2787 = _M0Lm5indexS1011;
      _M0Lm5indexS1011 = _M0L6_2atmpS2787 + 1;
      _M0L6_2atmpS2788 = _M0Lm5indexS1011;
      if (
        _M0L6_2atmpS2788 < 0
        || _M0L6_2atmpS2788 >= Moonbit_array_length(_M0L6resultS1010)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1010[_M0L6_2atmpS2788] = 46;
      _M0L6_2atmpS2789 = _M0Lm5indexS1011;
      _M0Lm5indexS1011 = _M0L6_2atmpS2789 + 1;
      _M0L1iS1027 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2790 = _M0Lm3expS1016;
        if (_M0L1iS1027 > _M0L6_2atmpS2790) {
          int32_t _M0L6_2atmpS2791 = _M0Lm5indexS1011;
          int32_t _M0L6_2atmpS2792;
          int32_t _M0L6_2atmpS2793;
          if (
            _M0L6_2atmpS2791 < 0
            || _M0L6_2atmpS2791 >= Moonbit_array_length(_M0L6resultS1010)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1010[_M0L6_2atmpS2791] = 48;
          _M0L6_2atmpS2792 = _M0Lm5indexS1011;
          _M0Lm5indexS1011 = _M0L6_2atmpS2792 + 1;
          _M0L6_2atmpS2793 = _M0L1iS1027 - 1;
          _M0L1iS1027 = _M0L6_2atmpS2793;
          continue;
        }
        break;
      }
      _M0L7currentS1029 = _M0Lm5indexS1011;
      _M0L1iS1030 = 0;
      while (1) {
        if (_M0L1iS1030 < _M0L7olengthS1015) {
          int32_t _M0L6_2atmpS2801 = _M0L7currentS1029 + _M0L7olengthS1015;
          int32_t _M0L6_2atmpS2800 = _M0L6_2atmpS2801 - _M0L1iS1030;
          int32_t _M0L6_2atmpS2794 = _M0L6_2atmpS2800 - 1;
          uint64_t _M0L6_2atmpS2799 = _M0Lm6outputS1013;
          uint64_t _M0L6_2atmpS2798 = _M0L6_2atmpS2799 % 10ull;
          int32_t _M0L6_2atmpS2797 = (int32_t)_M0L6_2atmpS2798;
          int32_t _M0L6_2atmpS2796 = 48 + _M0L6_2atmpS2797;
          int32_t _M0L6_2atmpS2795 = _M0L6_2atmpS2796 & 0xff;
          uint64_t _M0L6_2atmpS2802;
          int32_t _M0L6_2atmpS2803;
          int32_t _M0L6_2atmpS2804;
          if (
            _M0L6_2atmpS2794 < 0
            || _M0L6_2atmpS2794 >= Moonbit_array_length(_M0L6resultS1010)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1010[_M0L6_2atmpS2794] = _M0L6_2atmpS2795;
          _M0L6_2atmpS2802 = _M0Lm6outputS1013;
          _M0Lm6outputS1013 = _M0L6_2atmpS2802 / 10ull;
          _M0L6_2atmpS2803 = _M0Lm5indexS1011;
          _M0Lm5indexS1011 = _M0L6_2atmpS2803 + 1;
          _M0L6_2atmpS2804 = _M0L1iS1030 + 1;
          _M0L1iS1030 = _M0L6_2atmpS2804;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2806 = _M0Lm3expS1016;
      int32_t _M0L6_2atmpS2805 = _M0L6_2atmpS2806 + 1;
      if (_M0L6_2atmpS2805 >= _M0L7olengthS1015) {
        int32_t _M0L1iS1032 = 0;
        int32_t _M0L6_2atmpS2818;
        int32_t _M0L6_2atmpS2822;
        int32_t _M0L7_2abindS1034;
        int32_t _M0L2__S1035;
        while (1) {
          if (_M0L1iS1032 < _M0L7olengthS1015) {
            int32_t _M0L6_2atmpS2815 = _M0Lm5indexS1011;
            int32_t _M0L6_2atmpS2814 = _M0L6_2atmpS2815 + _M0L7olengthS1015;
            int32_t _M0L6_2atmpS2813 = _M0L6_2atmpS2814 - _M0L1iS1032;
            int32_t _M0L6_2atmpS2807 = _M0L6_2atmpS2813 - 1;
            uint64_t _M0L6_2atmpS2812 = _M0Lm6outputS1013;
            uint64_t _M0L6_2atmpS2811 = _M0L6_2atmpS2812 % 10ull;
            int32_t _M0L6_2atmpS2810 = (int32_t)_M0L6_2atmpS2811;
            int32_t _M0L6_2atmpS2809 = 48 + _M0L6_2atmpS2810;
            int32_t _M0L6_2atmpS2808 = _M0L6_2atmpS2809 & 0xff;
            uint64_t _M0L6_2atmpS2816;
            int32_t _M0L6_2atmpS2817;
            if (
              _M0L6_2atmpS2807 < 0
              || _M0L6_2atmpS2807 >= Moonbit_array_length(_M0L6resultS1010)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1010[_M0L6_2atmpS2807] = _M0L6_2atmpS2808;
            _M0L6_2atmpS2816 = _M0Lm6outputS1013;
            _M0Lm6outputS1013 = _M0L6_2atmpS2816 / 10ull;
            _M0L6_2atmpS2817 = _M0L1iS1032 + 1;
            _M0L1iS1032 = _M0L6_2atmpS2817;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2818 = _M0Lm5indexS1011;
        _M0Lm5indexS1011 = _M0L6_2atmpS2818 + _M0L7olengthS1015;
        _M0L6_2atmpS2822 = _M0Lm3expS1016;
        _M0L7_2abindS1034 = _M0L6_2atmpS2822 + 1;
        _M0L2__S1035 = _M0L7olengthS1015;
        while (1) {
          if (_M0L2__S1035 < _M0L7_2abindS1034) {
            int32_t _M0L6_2atmpS2819 = _M0Lm5indexS1011;
            int32_t _M0L6_2atmpS2820;
            int32_t _M0L6_2atmpS2821;
            if (
              _M0L6_2atmpS2819 < 0
              || _M0L6_2atmpS2819 >= Moonbit_array_length(_M0L6resultS1010)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1010[_M0L6_2atmpS2819] = 48;
            _M0L6_2atmpS2820 = _M0Lm5indexS1011;
            _M0Lm5indexS1011 = _M0L6_2atmpS2820 + 1;
            _M0L6_2atmpS2821 = _M0L2__S1035 + 1;
            _M0L2__S1035 = _M0L6_2atmpS2821;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2844 = _M0Lm5indexS1011;
        int32_t _M0Lm7currentS1037 = _M0L6_2atmpS2844 + 1;
        int32_t _M0L1iS1038 = 0;
        int32_t _M0L6_2atmpS2842;
        int32_t _M0L6_2atmpS2843;
        while (1) {
          if (_M0L1iS1038 < _M0L7olengthS1015) {
            int32_t _M0L6_2atmpS2825 = _M0L7olengthS1015 - _M0L1iS1038;
            int32_t _M0L6_2atmpS2823 = _M0L6_2atmpS2825 - 1;
            int32_t _M0L6_2atmpS2824 = _M0Lm3expS1016;
            int32_t _M0L6_2atmpS2839;
            int32_t _M0L6_2atmpS2838;
            int32_t _M0L6_2atmpS2837;
            int32_t _M0L6_2atmpS2831;
            uint64_t _M0L6_2atmpS2836;
            uint64_t _M0L6_2atmpS2835;
            int32_t _M0L6_2atmpS2834;
            int32_t _M0L6_2atmpS2833;
            int32_t _M0L6_2atmpS2832;
            uint64_t _M0L6_2atmpS2840;
            int32_t _M0L6_2atmpS2841;
            if (_M0L6_2atmpS2823 == _M0L6_2atmpS2824) {
              int32_t _M0L6_2atmpS2829 = _M0Lm7currentS1037;
              int32_t _M0L6_2atmpS2828 = _M0L6_2atmpS2829 + _M0L7olengthS1015;
              int32_t _M0L6_2atmpS2827 = _M0L6_2atmpS2828 - _M0L1iS1038;
              int32_t _M0L6_2atmpS2826 = _M0L6_2atmpS2827 - 1;
              int32_t _M0L6_2atmpS2830;
              if (
                _M0L6_2atmpS2826 < 0
                || _M0L6_2atmpS2826 >= Moonbit_array_length(_M0L6resultS1010)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1010[_M0L6_2atmpS2826] = 46;
              _M0L6_2atmpS2830 = _M0Lm7currentS1037;
              _M0Lm7currentS1037 = _M0L6_2atmpS2830 - 1;
            }
            _M0L6_2atmpS2839 = _M0Lm7currentS1037;
            _M0L6_2atmpS2838 = _M0L6_2atmpS2839 + _M0L7olengthS1015;
            _M0L6_2atmpS2837 = _M0L6_2atmpS2838 - _M0L1iS1038;
            _M0L6_2atmpS2831 = _M0L6_2atmpS2837 - 1;
            _M0L6_2atmpS2836 = _M0Lm6outputS1013;
            _M0L6_2atmpS2835 = _M0L6_2atmpS2836 % 10ull;
            _M0L6_2atmpS2834 = (int32_t)_M0L6_2atmpS2835;
            _M0L6_2atmpS2833 = 48 + _M0L6_2atmpS2834;
            _M0L6_2atmpS2832 = _M0L6_2atmpS2833 & 0xff;
            if (
              _M0L6_2atmpS2831 < 0
              || _M0L6_2atmpS2831 >= Moonbit_array_length(_M0L6resultS1010)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1010[_M0L6_2atmpS2831] = _M0L6_2atmpS2832;
            _M0L6_2atmpS2840 = _M0Lm6outputS1013;
            _M0Lm6outputS1013 = _M0L6_2atmpS2840 / 10ull;
            _M0L6_2atmpS2841 = _M0L1iS1038 + 1;
            _M0L1iS1038 = _M0L6_2atmpS2841;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2842 = _M0Lm5indexS1011;
        _M0L6_2atmpS2843 = _M0L7olengthS1015 + 1;
        _M0Lm5indexS1011 = _M0L6_2atmpS2842 + _M0L6_2atmpS2843;
      }
    }
    _M0L6_2atmpS2845 = _M0Lm5indexS1011;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1010, 0, _M0L6_2atmpS2845);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS956,
  uint32_t _M0L12ieeeExponentS955
) {
  int32_t _M0Lm2e2S953;
  uint64_t _M0Lm2m2S954;
  uint64_t _M0L6_2atmpS2720;
  uint64_t _M0L6_2atmpS2719;
  int32_t _M0L4evenS957;
  uint64_t _M0L6_2atmpS2718;
  uint64_t _M0L2mvS958;
  int32_t _M0L7mmShiftS959;
  uint64_t _M0Lm2vrS960;
  uint64_t _M0Lm2vpS961;
  uint64_t _M0Lm2vmS962;
  int32_t _M0Lm3e10S963;
  int32_t _M0Lm17vmIsTrailingZerosS964;
  int32_t _M0Lm17vrIsTrailingZerosS965;
  int32_t _M0L6_2atmpS2620;
  int32_t _M0Lm7removedS984;
  int32_t _M0Lm16lastRemovedDigitS985;
  uint64_t _M0Lm6outputS986;
  int32_t _M0L6_2atmpS2716;
  int32_t _M0L6_2atmpS2717;
  int32_t _M0L3expS1009;
  uint64_t _M0L6_2atmpS2715;
  struct _M0TPB17FloatingDecimal64* _block_3612;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S953 = 0;
  _M0Lm2m2S954 = 0ull;
  if (_M0L12ieeeExponentS955 == 0u) {
    _M0Lm2e2S953 = -1076;
    _M0Lm2m2S954 = _M0L12ieeeMantissaS956;
  } else {
    int32_t _M0L6_2atmpS2619 = *(int32_t*)&_M0L12ieeeExponentS955;
    int32_t _M0L6_2atmpS2618 = _M0L6_2atmpS2619 - 1023;
    int32_t _M0L6_2atmpS2617 = _M0L6_2atmpS2618 - 52;
    _M0Lm2e2S953 = _M0L6_2atmpS2617 - 2;
    _M0Lm2m2S954 = 4503599627370496ull | _M0L12ieeeMantissaS956;
  }
  _M0L6_2atmpS2720 = _M0Lm2m2S954;
  _M0L6_2atmpS2719 = _M0L6_2atmpS2720 & 1ull;
  _M0L4evenS957 = _M0L6_2atmpS2719 == 0ull;
  _M0L6_2atmpS2718 = _M0Lm2m2S954;
  _M0L2mvS958 = 4ull * _M0L6_2atmpS2718;
  if (_M0L12ieeeMantissaS956 != 0ull) {
    _M0L7mmShiftS959 = 1;
  } else {
    _M0L7mmShiftS959 = _M0L12ieeeExponentS955 <= 1u;
  }
  _M0Lm2vrS960 = 0ull;
  _M0Lm2vpS961 = 0ull;
  _M0Lm2vmS962 = 0ull;
  _M0Lm3e10S963 = 0;
  _M0Lm17vmIsTrailingZerosS964 = 0;
  _M0Lm17vrIsTrailingZerosS965 = 0;
  _M0L6_2atmpS2620 = _M0Lm2e2S953;
  if (_M0L6_2atmpS2620 >= 0) {
    int32_t _M0L6_2atmpS2642 = _M0Lm2e2S953;
    int32_t _M0L6_2atmpS2638;
    int32_t _M0L6_2atmpS2641;
    int32_t _M0L6_2atmpS2640;
    int32_t _M0L6_2atmpS2639;
    int32_t _M0L1qS966;
    int32_t _M0L6_2atmpS2637;
    int32_t _M0L6_2atmpS2636;
    int32_t _M0L1kS967;
    int32_t _M0L6_2atmpS2635;
    int32_t _M0L6_2atmpS2634;
    int32_t _M0L6_2atmpS2633;
    int32_t _M0L1iS968;
    struct _M0TPB8Pow5Pair _M0L4pow5S969;
    uint64_t _M0L6_2atmpS2632;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS970;
    uint64_t _M0L8_2avrOutS971;
    uint64_t _M0L8_2avpOutS972;
    uint64_t _M0L8_2avmOutS973;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2638 = _M0FPB9log10Pow2(_M0L6_2atmpS2642);
    _M0L6_2atmpS2641 = _M0Lm2e2S953;
    _M0L6_2atmpS2640 = _M0L6_2atmpS2641 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2639 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2640);
    _M0L1qS966 = _M0L6_2atmpS2638 - _M0L6_2atmpS2639;
    _M0Lm3e10S963 = _M0L1qS966;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2637 = _M0FPB8pow5bits(_M0L1qS966);
    _M0L6_2atmpS2636 = 125 + _M0L6_2atmpS2637;
    _M0L1kS967 = _M0L6_2atmpS2636 - 1;
    _M0L6_2atmpS2635 = _M0Lm2e2S953;
    _M0L6_2atmpS2634 = -_M0L6_2atmpS2635;
    _M0L6_2atmpS2633 = _M0L6_2atmpS2634 + _M0L1qS966;
    _M0L1iS968 = _M0L6_2atmpS2633 + _M0L1kS967;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S969 = _M0FPB22double__computeInvPow5(_M0L1qS966);
    _M0L6_2atmpS2632 = _M0Lm2m2S954;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS970
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2632, _M0L4pow5S969, _M0L1iS968, _M0L7mmShiftS959);
    _M0L8_2avrOutS971 = _M0L7_2abindS970.$0;
    _M0L8_2avpOutS972 = _M0L7_2abindS970.$1;
    _M0L8_2avmOutS973 = _M0L7_2abindS970.$2;
    _M0Lm2vrS960 = _M0L8_2avrOutS971;
    _M0Lm2vpS961 = _M0L8_2avpOutS972;
    _M0Lm2vmS962 = _M0L8_2avmOutS973;
    if (_M0L1qS966 <= 21) {
      int32_t _M0L6_2atmpS2628 = (int32_t)_M0L2mvS958;
      uint64_t _M0L6_2atmpS2631 = _M0L2mvS958 / 5ull;
      int32_t _M0L6_2atmpS2630 = (int32_t)_M0L6_2atmpS2631;
      int32_t _M0L6_2atmpS2629 = 5 * _M0L6_2atmpS2630;
      int32_t _M0L6mvMod5S974 = _M0L6_2atmpS2628 - _M0L6_2atmpS2629;
      if (_M0L6mvMod5S974 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS965
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS958, _M0L1qS966);
      } else if (_M0L4evenS957) {
        uint64_t _M0L6_2atmpS2622 = _M0L2mvS958 - 1ull;
        uint64_t _M0L6_2atmpS2623;
        uint64_t _M0L6_2atmpS2621;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2623 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS959);
        _M0L6_2atmpS2621 = _M0L6_2atmpS2622 - _M0L6_2atmpS2623;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS964
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2621, _M0L1qS966);
      } else {
        uint64_t _M0L6_2atmpS2624 = _M0Lm2vpS961;
        uint64_t _M0L6_2atmpS2627 = _M0L2mvS958 + 2ull;
        int32_t _M0L6_2atmpS2626;
        uint64_t _M0L6_2atmpS2625;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2626
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2627, _M0L1qS966);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2625 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2626);
        _M0Lm2vpS961 = _M0L6_2atmpS2624 - _M0L6_2atmpS2625;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2656 = _M0Lm2e2S953;
    int32_t _M0L6_2atmpS2655 = -_M0L6_2atmpS2656;
    int32_t _M0L6_2atmpS2650;
    int32_t _M0L6_2atmpS2654;
    int32_t _M0L6_2atmpS2653;
    int32_t _M0L6_2atmpS2652;
    int32_t _M0L6_2atmpS2651;
    int32_t _M0L1qS975;
    int32_t _M0L6_2atmpS2643;
    int32_t _M0L6_2atmpS2649;
    int32_t _M0L6_2atmpS2648;
    int32_t _M0L1iS976;
    int32_t _M0L6_2atmpS2647;
    int32_t _M0L1kS977;
    int32_t _M0L1jS978;
    struct _M0TPB8Pow5Pair _M0L4pow5S979;
    uint64_t _M0L6_2atmpS2646;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS980;
    uint64_t _M0L8_2avrOutS981;
    uint64_t _M0L8_2avpOutS982;
    uint64_t _M0L8_2avmOutS983;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2650 = _M0FPB9log10Pow5(_M0L6_2atmpS2655);
    _M0L6_2atmpS2654 = _M0Lm2e2S953;
    _M0L6_2atmpS2653 = -_M0L6_2atmpS2654;
    _M0L6_2atmpS2652 = _M0L6_2atmpS2653 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2651 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2652);
    _M0L1qS975 = _M0L6_2atmpS2650 - _M0L6_2atmpS2651;
    _M0L6_2atmpS2643 = _M0Lm2e2S953;
    _M0Lm3e10S963 = _M0L1qS975 + _M0L6_2atmpS2643;
    _M0L6_2atmpS2649 = _M0Lm2e2S953;
    _M0L6_2atmpS2648 = -_M0L6_2atmpS2649;
    _M0L1iS976 = _M0L6_2atmpS2648 - _M0L1qS975;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2647 = _M0FPB8pow5bits(_M0L1iS976);
    _M0L1kS977 = _M0L6_2atmpS2647 - 125;
    _M0L1jS978 = _M0L1qS975 - _M0L1kS977;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S979 = _M0FPB19double__computePow5(_M0L1iS976);
    _M0L6_2atmpS2646 = _M0Lm2m2S954;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS980
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2646, _M0L4pow5S979, _M0L1jS978, _M0L7mmShiftS959);
    _M0L8_2avrOutS981 = _M0L7_2abindS980.$0;
    _M0L8_2avpOutS982 = _M0L7_2abindS980.$1;
    _M0L8_2avmOutS983 = _M0L7_2abindS980.$2;
    _M0Lm2vrS960 = _M0L8_2avrOutS981;
    _M0Lm2vpS961 = _M0L8_2avpOutS982;
    _M0Lm2vmS962 = _M0L8_2avmOutS983;
    if (_M0L1qS975 <= 1) {
      _M0Lm17vrIsTrailingZerosS965 = 1;
      if (_M0L4evenS957) {
        int32_t _M0L6_2atmpS2644;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2644 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS959);
        _M0Lm17vmIsTrailingZerosS964 = _M0L6_2atmpS2644 == 1;
      } else {
        uint64_t _M0L6_2atmpS2645 = _M0Lm2vpS961;
        _M0Lm2vpS961 = _M0L6_2atmpS2645 - 1ull;
      }
    } else if (_M0L1qS975 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS965
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS958, _M0L1qS975);
    }
  }
  _M0Lm7removedS984 = 0;
  _M0Lm16lastRemovedDigitS985 = 0;
  _M0Lm6outputS986 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS964 || _M0Lm17vrIsTrailingZerosS965) {
    int32_t _if__result_3609;
    uint64_t _M0L6_2atmpS2686;
    uint64_t _M0L6_2atmpS2692;
    uint64_t _M0L6_2atmpS2693;
    int32_t _if__result_3610;
    int32_t _M0L6_2atmpS2689;
    int64_t _M0L6_2atmpS2688;
    uint64_t _M0L6_2atmpS2687;
    while (1) {
      uint64_t _M0L6_2atmpS2669 = _M0Lm2vpS961;
      uint64_t _M0L7vpDiv10S987 = _M0L6_2atmpS2669 / 10ull;
      uint64_t _M0L6_2atmpS2668 = _M0Lm2vmS962;
      uint64_t _M0L7vmDiv10S988 = _M0L6_2atmpS2668 / 10ull;
      uint64_t _M0L6_2atmpS2667;
      int32_t _M0L6_2atmpS2664;
      int32_t _M0L6_2atmpS2666;
      int32_t _M0L6_2atmpS2665;
      int32_t _M0L7vmMod10S990;
      uint64_t _M0L6_2atmpS2663;
      uint64_t _M0L7vrDiv10S991;
      uint64_t _M0L6_2atmpS2662;
      int32_t _M0L6_2atmpS2659;
      int32_t _M0L6_2atmpS2661;
      int32_t _M0L6_2atmpS2660;
      int32_t _M0L7vrMod10S992;
      int32_t _M0L6_2atmpS2658;
      if (_M0L7vpDiv10S987 <= _M0L7vmDiv10S988) {
        break;
      }
      _M0L6_2atmpS2667 = _M0Lm2vmS962;
      _M0L6_2atmpS2664 = (int32_t)_M0L6_2atmpS2667;
      _M0L6_2atmpS2666 = (int32_t)_M0L7vmDiv10S988;
      _M0L6_2atmpS2665 = 10 * _M0L6_2atmpS2666;
      _M0L7vmMod10S990 = _M0L6_2atmpS2664 - _M0L6_2atmpS2665;
      _M0L6_2atmpS2663 = _M0Lm2vrS960;
      _M0L7vrDiv10S991 = _M0L6_2atmpS2663 / 10ull;
      _M0L6_2atmpS2662 = _M0Lm2vrS960;
      _M0L6_2atmpS2659 = (int32_t)_M0L6_2atmpS2662;
      _M0L6_2atmpS2661 = (int32_t)_M0L7vrDiv10S991;
      _M0L6_2atmpS2660 = 10 * _M0L6_2atmpS2661;
      _M0L7vrMod10S992 = _M0L6_2atmpS2659 - _M0L6_2atmpS2660;
      if (_M0Lm17vmIsTrailingZerosS964) {
        _M0Lm17vmIsTrailingZerosS964 = _M0L7vmMod10S990 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS964 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS965) {
        int32_t _M0L6_2atmpS2657 = _M0Lm16lastRemovedDigitS985;
        _M0Lm17vrIsTrailingZerosS965 = _M0L6_2atmpS2657 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS965 = 0;
      }
      _M0Lm16lastRemovedDigitS985 = _M0L7vrMod10S992;
      _M0Lm2vrS960 = _M0L7vrDiv10S991;
      _M0Lm2vpS961 = _M0L7vpDiv10S987;
      _M0Lm2vmS962 = _M0L7vmDiv10S988;
      _M0L6_2atmpS2658 = _M0Lm7removedS984;
      _M0Lm7removedS984 = _M0L6_2atmpS2658 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS964) {
      while (1) {
        uint64_t _M0L6_2atmpS2682 = _M0Lm2vmS962;
        uint64_t _M0L7vmDiv10S993 = _M0L6_2atmpS2682 / 10ull;
        uint64_t _M0L6_2atmpS2681 = _M0Lm2vmS962;
        int32_t _M0L6_2atmpS2678 = (int32_t)_M0L6_2atmpS2681;
        int32_t _M0L6_2atmpS2680 = (int32_t)_M0L7vmDiv10S993;
        int32_t _M0L6_2atmpS2679 = 10 * _M0L6_2atmpS2680;
        int32_t _M0L7vmMod10S994 = _M0L6_2atmpS2678 - _M0L6_2atmpS2679;
        uint64_t _M0L6_2atmpS2677;
        uint64_t _M0L7vpDiv10S996;
        uint64_t _M0L6_2atmpS2676;
        uint64_t _M0L7vrDiv10S997;
        uint64_t _M0L6_2atmpS2675;
        int32_t _M0L6_2atmpS2672;
        int32_t _M0L6_2atmpS2674;
        int32_t _M0L6_2atmpS2673;
        int32_t _M0L7vrMod10S998;
        int32_t _M0L6_2atmpS2671;
        if (_M0L7vmMod10S994 != 0) {
          break;
        }
        _M0L6_2atmpS2677 = _M0Lm2vpS961;
        _M0L7vpDiv10S996 = _M0L6_2atmpS2677 / 10ull;
        _M0L6_2atmpS2676 = _M0Lm2vrS960;
        _M0L7vrDiv10S997 = _M0L6_2atmpS2676 / 10ull;
        _M0L6_2atmpS2675 = _M0Lm2vrS960;
        _M0L6_2atmpS2672 = (int32_t)_M0L6_2atmpS2675;
        _M0L6_2atmpS2674 = (int32_t)_M0L7vrDiv10S997;
        _M0L6_2atmpS2673 = 10 * _M0L6_2atmpS2674;
        _M0L7vrMod10S998 = _M0L6_2atmpS2672 - _M0L6_2atmpS2673;
        if (_M0Lm17vrIsTrailingZerosS965) {
          int32_t _M0L6_2atmpS2670 = _M0Lm16lastRemovedDigitS985;
          _M0Lm17vrIsTrailingZerosS965 = _M0L6_2atmpS2670 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS965 = 0;
        }
        _M0Lm16lastRemovedDigitS985 = _M0L7vrMod10S998;
        _M0Lm2vrS960 = _M0L7vrDiv10S997;
        _M0Lm2vpS961 = _M0L7vpDiv10S996;
        _M0Lm2vmS962 = _M0L7vmDiv10S993;
        _M0L6_2atmpS2671 = _M0Lm7removedS984;
        _M0Lm7removedS984 = _M0L6_2atmpS2671 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS965) {
      int32_t _M0L6_2atmpS2685 = _M0Lm16lastRemovedDigitS985;
      if (_M0L6_2atmpS2685 == 5) {
        uint64_t _M0L6_2atmpS2684 = _M0Lm2vrS960;
        uint64_t _M0L6_2atmpS2683 = _M0L6_2atmpS2684 % 2ull;
        _if__result_3609 = _M0L6_2atmpS2683 == 0ull;
      } else {
        _if__result_3609 = 0;
      }
    } else {
      _if__result_3609 = 0;
    }
    if (_if__result_3609) {
      _M0Lm16lastRemovedDigitS985 = 4;
    }
    _M0L6_2atmpS2686 = _M0Lm2vrS960;
    _M0L6_2atmpS2692 = _M0Lm2vrS960;
    _M0L6_2atmpS2693 = _M0Lm2vmS962;
    if (_M0L6_2atmpS2692 == _M0L6_2atmpS2693) {
      if (!_M0L4evenS957) {
        _if__result_3610 = 1;
      } else {
        int32_t _M0L6_2atmpS2691 = _M0Lm17vmIsTrailingZerosS964;
        _if__result_3610 = !_M0L6_2atmpS2691;
      }
    } else {
      _if__result_3610 = 0;
    }
    if (_if__result_3610) {
      _M0L6_2atmpS2689 = 1;
    } else {
      int32_t _M0L6_2atmpS2690 = _M0Lm16lastRemovedDigitS985;
      _M0L6_2atmpS2689 = _M0L6_2atmpS2690 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2688 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2689);
    _M0L6_2atmpS2687 = *(uint64_t*)&_M0L6_2atmpS2688;
    _M0Lm6outputS986 = _M0L6_2atmpS2686 + _M0L6_2atmpS2687;
  } else {
    int32_t _M0Lm7roundUpS999 = 0;
    uint64_t _M0L6_2atmpS2714 = _M0Lm2vpS961;
    uint64_t _M0L8vpDiv100S1000 = _M0L6_2atmpS2714 / 100ull;
    uint64_t _M0L6_2atmpS2713 = _M0Lm2vmS962;
    uint64_t _M0L8vmDiv100S1001 = _M0L6_2atmpS2713 / 100ull;
    uint64_t _M0L6_2atmpS2708;
    uint64_t _M0L6_2atmpS2711;
    uint64_t _M0L6_2atmpS2712;
    int32_t _M0L6_2atmpS2710;
    uint64_t _M0L6_2atmpS2709;
    if (_M0L8vpDiv100S1000 > _M0L8vmDiv100S1001) {
      uint64_t _M0L6_2atmpS2699 = _M0Lm2vrS960;
      uint64_t _M0L8vrDiv100S1002 = _M0L6_2atmpS2699 / 100ull;
      uint64_t _M0L6_2atmpS2698 = _M0Lm2vrS960;
      int32_t _M0L6_2atmpS2695 = (int32_t)_M0L6_2atmpS2698;
      int32_t _M0L6_2atmpS2697 = (int32_t)_M0L8vrDiv100S1002;
      int32_t _M0L6_2atmpS2696 = 100 * _M0L6_2atmpS2697;
      int32_t _M0L8vrMod100S1003 = _M0L6_2atmpS2695 - _M0L6_2atmpS2696;
      int32_t _M0L6_2atmpS2694;
      _M0Lm7roundUpS999 = _M0L8vrMod100S1003 >= 50;
      _M0Lm2vrS960 = _M0L8vrDiv100S1002;
      _M0Lm2vpS961 = _M0L8vpDiv100S1000;
      _M0Lm2vmS962 = _M0L8vmDiv100S1001;
      _M0L6_2atmpS2694 = _M0Lm7removedS984;
      _M0Lm7removedS984 = _M0L6_2atmpS2694 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2707 = _M0Lm2vpS961;
      uint64_t _M0L7vpDiv10S1004 = _M0L6_2atmpS2707 / 10ull;
      uint64_t _M0L6_2atmpS2706 = _M0Lm2vmS962;
      uint64_t _M0L7vmDiv10S1005 = _M0L6_2atmpS2706 / 10ull;
      uint64_t _M0L6_2atmpS2705;
      uint64_t _M0L7vrDiv10S1007;
      uint64_t _M0L6_2atmpS2704;
      int32_t _M0L6_2atmpS2701;
      int32_t _M0L6_2atmpS2703;
      int32_t _M0L6_2atmpS2702;
      int32_t _M0L7vrMod10S1008;
      int32_t _M0L6_2atmpS2700;
      if (_M0L7vpDiv10S1004 <= _M0L7vmDiv10S1005) {
        break;
      }
      _M0L6_2atmpS2705 = _M0Lm2vrS960;
      _M0L7vrDiv10S1007 = _M0L6_2atmpS2705 / 10ull;
      _M0L6_2atmpS2704 = _M0Lm2vrS960;
      _M0L6_2atmpS2701 = (int32_t)_M0L6_2atmpS2704;
      _M0L6_2atmpS2703 = (int32_t)_M0L7vrDiv10S1007;
      _M0L6_2atmpS2702 = 10 * _M0L6_2atmpS2703;
      _M0L7vrMod10S1008 = _M0L6_2atmpS2701 - _M0L6_2atmpS2702;
      _M0Lm7roundUpS999 = _M0L7vrMod10S1008 >= 5;
      _M0Lm2vrS960 = _M0L7vrDiv10S1007;
      _M0Lm2vpS961 = _M0L7vpDiv10S1004;
      _M0Lm2vmS962 = _M0L7vmDiv10S1005;
      _M0L6_2atmpS2700 = _M0Lm7removedS984;
      _M0Lm7removedS984 = _M0L6_2atmpS2700 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2708 = _M0Lm2vrS960;
    _M0L6_2atmpS2711 = _M0Lm2vrS960;
    _M0L6_2atmpS2712 = _M0Lm2vmS962;
    _M0L6_2atmpS2710
    = _M0L6_2atmpS2711 == _M0L6_2atmpS2712 || _M0Lm7roundUpS999;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2709 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2710);
    _M0Lm6outputS986 = _M0L6_2atmpS2708 + _M0L6_2atmpS2709;
  }
  _M0L6_2atmpS2716 = _M0Lm3e10S963;
  _M0L6_2atmpS2717 = _M0Lm7removedS984;
  _M0L3expS1009 = _M0L6_2atmpS2716 + _M0L6_2atmpS2717;
  _M0L6_2atmpS2715 = _M0Lm6outputS986;
  _block_3612
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3612)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3612->$0 = _M0L6_2atmpS2715;
  _block_3612->$1 = _M0L3expS1009;
  return _block_3612;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS952) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS952) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS951) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS951) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS950) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS950) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS949) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS949 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS949 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS949 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS949 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS949 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS949 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS949 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS949 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS949 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS949 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS949 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS949 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS949 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS949 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS949 >= 100ull) {
    return 3;
  }
  if (_M0L1vS949 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS932) {
  int32_t _M0L6_2atmpS2616;
  int32_t _M0L6_2atmpS2615;
  int32_t _M0L4baseS931;
  int32_t _M0L5base2S933;
  int32_t _M0L6offsetS934;
  int32_t _M0L6_2atmpS2614;
  uint64_t _M0L4mul0S935;
  int32_t _M0L6_2atmpS2613;
  int32_t _M0L6_2atmpS2612;
  uint64_t _M0L4mul1S936;
  uint64_t _M0L1mS937;
  struct _M0TPB7Umul128 _M0L7_2abindS938;
  uint64_t _M0L7_2alow1S939;
  uint64_t _M0L8_2ahigh1S940;
  struct _M0TPB7Umul128 _M0L7_2abindS941;
  uint64_t _M0L7_2alow0S942;
  uint64_t _M0L8_2ahigh0S943;
  uint64_t _M0L3sumS944;
  uint64_t _M0Lm5high1S945;
  int32_t _M0L6_2atmpS2610;
  int32_t _M0L6_2atmpS2611;
  int32_t _M0L5deltaS946;
  uint64_t _M0L6_2atmpS2609;
  uint64_t _M0L6_2atmpS2601;
  int32_t _M0L6_2atmpS2608;
  uint32_t _M0L6_2atmpS2605;
  int32_t _M0L6_2atmpS2607;
  int32_t _M0L6_2atmpS2606;
  uint32_t _M0L6_2atmpS2604;
  uint32_t _M0L6_2atmpS2603;
  uint64_t _M0L6_2atmpS2602;
  uint64_t _M0L1aS947;
  uint64_t _M0L6_2atmpS2600;
  uint64_t _M0L1bS948;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2616 = _M0L1iS932 + 26;
  _M0L6_2atmpS2615 = _M0L6_2atmpS2616 - 1;
  _M0L4baseS931 = _M0L6_2atmpS2615 / 26;
  _M0L5base2S933 = _M0L4baseS931 * 26;
  _M0L6offsetS934 = _M0L5base2S933 - _M0L1iS932;
  _M0L6_2atmpS2614 = _M0L4baseS931 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S935
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2614);
  _M0L6_2atmpS2613 = _M0L4baseS931 * 2;
  _M0L6_2atmpS2612 = _M0L6_2atmpS2613 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S936
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2612);
  if (_M0L6offsetS934 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S935, _M0L4mul1S936};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS937
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS934);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS938 = _M0FPB7umul128(_M0L1mS937, _M0L4mul1S936);
  _M0L7_2alow1S939 = _M0L7_2abindS938.$0;
  _M0L8_2ahigh1S940 = _M0L7_2abindS938.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS941 = _M0FPB7umul128(_M0L1mS937, _M0L4mul0S935);
  _M0L7_2alow0S942 = _M0L7_2abindS941.$0;
  _M0L8_2ahigh0S943 = _M0L7_2abindS941.$1;
  _M0L3sumS944 = _M0L8_2ahigh0S943 + _M0L7_2alow1S939;
  _M0Lm5high1S945 = _M0L8_2ahigh1S940;
  if (_M0L3sumS944 < _M0L8_2ahigh0S943) {
    uint64_t _M0L6_2atmpS2599 = _M0Lm5high1S945;
    _M0Lm5high1S945 = _M0L6_2atmpS2599 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2610 = _M0FPB8pow5bits(_M0L5base2S933);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2611 = _M0FPB8pow5bits(_M0L1iS932);
  _M0L5deltaS946 = _M0L6_2atmpS2610 - _M0L6_2atmpS2611;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2609
  = _M0FPB13shiftright128(_M0L7_2alow0S942, _M0L3sumS944, _M0L5deltaS946);
  _M0L6_2atmpS2601 = _M0L6_2atmpS2609 + 1ull;
  _M0L6_2atmpS2608 = _M0L1iS932 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2605
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2608);
  _M0L6_2atmpS2607 = _M0L1iS932 % 16;
  _M0L6_2atmpS2606 = _M0L6_2atmpS2607 << 1;
  _M0L6_2atmpS2604 = _M0L6_2atmpS2605 >> (_M0L6_2atmpS2606 & 31);
  _M0L6_2atmpS2603 = _M0L6_2atmpS2604 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2602 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2603);
  _M0L1aS947 = _M0L6_2atmpS2601 + _M0L6_2atmpS2602;
  _M0L6_2atmpS2600 = _M0Lm5high1S945;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS948
  = _M0FPB13shiftright128(_M0L3sumS944, _M0L6_2atmpS2600, _M0L5deltaS946);
  return (struct _M0TPB8Pow5Pair){_M0L1aS947, _M0L1bS948};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS914) {
  int32_t _M0L4baseS913;
  int32_t _M0L5base2S915;
  int32_t _M0L6offsetS916;
  int32_t _M0L6_2atmpS2598;
  uint64_t _M0L4mul0S917;
  int32_t _M0L6_2atmpS2597;
  int32_t _M0L6_2atmpS2596;
  uint64_t _M0L4mul1S918;
  uint64_t _M0L1mS919;
  struct _M0TPB7Umul128 _M0L7_2abindS920;
  uint64_t _M0L7_2alow1S921;
  uint64_t _M0L8_2ahigh1S922;
  struct _M0TPB7Umul128 _M0L7_2abindS923;
  uint64_t _M0L7_2alow0S924;
  uint64_t _M0L8_2ahigh0S925;
  uint64_t _M0L3sumS926;
  uint64_t _M0Lm5high1S927;
  int32_t _M0L6_2atmpS2594;
  int32_t _M0L6_2atmpS2595;
  int32_t _M0L5deltaS928;
  uint64_t _M0L6_2atmpS2586;
  int32_t _M0L6_2atmpS2593;
  uint32_t _M0L6_2atmpS2590;
  int32_t _M0L6_2atmpS2592;
  int32_t _M0L6_2atmpS2591;
  uint32_t _M0L6_2atmpS2589;
  uint32_t _M0L6_2atmpS2588;
  uint64_t _M0L6_2atmpS2587;
  uint64_t _M0L1aS929;
  uint64_t _M0L6_2atmpS2585;
  uint64_t _M0L1bS930;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS913 = _M0L1iS914 / 26;
  _M0L5base2S915 = _M0L4baseS913 * 26;
  _M0L6offsetS916 = _M0L1iS914 - _M0L5base2S915;
  _M0L6_2atmpS2598 = _M0L4baseS913 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S917
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2598);
  _M0L6_2atmpS2597 = _M0L4baseS913 * 2;
  _M0L6_2atmpS2596 = _M0L6_2atmpS2597 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S918
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2596);
  if (_M0L6offsetS916 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S917, _M0L4mul1S918};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS919
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS916);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS920 = _M0FPB7umul128(_M0L1mS919, _M0L4mul1S918);
  _M0L7_2alow1S921 = _M0L7_2abindS920.$0;
  _M0L8_2ahigh1S922 = _M0L7_2abindS920.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS923 = _M0FPB7umul128(_M0L1mS919, _M0L4mul0S917);
  _M0L7_2alow0S924 = _M0L7_2abindS923.$0;
  _M0L8_2ahigh0S925 = _M0L7_2abindS923.$1;
  _M0L3sumS926 = _M0L8_2ahigh0S925 + _M0L7_2alow1S921;
  _M0Lm5high1S927 = _M0L8_2ahigh1S922;
  if (_M0L3sumS926 < _M0L8_2ahigh0S925) {
    uint64_t _M0L6_2atmpS2584 = _M0Lm5high1S927;
    _M0Lm5high1S927 = _M0L6_2atmpS2584 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2594 = _M0FPB8pow5bits(_M0L1iS914);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2595 = _M0FPB8pow5bits(_M0L5base2S915);
  _M0L5deltaS928 = _M0L6_2atmpS2594 - _M0L6_2atmpS2595;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2586
  = _M0FPB13shiftright128(_M0L7_2alow0S924, _M0L3sumS926, _M0L5deltaS928);
  _M0L6_2atmpS2593 = _M0L1iS914 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2590
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2593);
  _M0L6_2atmpS2592 = _M0L1iS914 % 16;
  _M0L6_2atmpS2591 = _M0L6_2atmpS2592 << 1;
  _M0L6_2atmpS2589 = _M0L6_2atmpS2590 >> (_M0L6_2atmpS2591 & 31);
  _M0L6_2atmpS2588 = _M0L6_2atmpS2589 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2587 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2588);
  _M0L1aS929 = _M0L6_2atmpS2586 + _M0L6_2atmpS2587;
  _M0L6_2atmpS2585 = _M0Lm5high1S927;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS930
  = _M0FPB13shiftright128(_M0L3sumS926, _M0L6_2atmpS2585, _M0L5deltaS928);
  return (struct _M0TPB8Pow5Pair){_M0L1aS929, _M0L1bS930};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS887,
  struct _M0TPB8Pow5Pair _M0L3mulS884,
  int32_t _M0L1jS900,
  int32_t _M0L7mmShiftS902
) {
  uint64_t _M0L7_2amul0S883;
  uint64_t _M0L7_2amul1S885;
  uint64_t _M0L1mS886;
  struct _M0TPB7Umul128 _M0L7_2abindS888;
  uint64_t _M0L5_2aloS889;
  uint64_t _M0L6_2atmpS890;
  struct _M0TPB7Umul128 _M0L7_2abindS891;
  uint64_t _M0L6_2alo2S892;
  uint64_t _M0L6_2ahi2S893;
  uint64_t _M0L3midS894;
  uint64_t _M0L6_2atmpS2583;
  uint64_t _M0L2hiS895;
  uint64_t _M0L3lo2S896;
  uint64_t _M0L6_2atmpS2581;
  uint64_t _M0L6_2atmpS2582;
  uint64_t _M0L4mid2S897;
  uint64_t _M0L6_2atmpS2580;
  uint64_t _M0L3hi2S898;
  int32_t _M0L6_2atmpS2579;
  int32_t _M0L6_2atmpS2578;
  uint64_t _M0L2vpS899;
  uint64_t _M0Lm2vmS901;
  int32_t _M0L6_2atmpS2577;
  int32_t _M0L6_2atmpS2576;
  uint64_t _M0L2vrS912;
  uint64_t _M0L6_2atmpS2575;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S883 = _M0L3mulS884.$0;
  _M0L7_2amul1S885 = _M0L3mulS884.$1;
  _M0L1mS886 = _M0L1mS887 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS888 = _M0FPB7umul128(_M0L1mS886, _M0L7_2amul0S883);
  _M0L5_2aloS889 = _M0L7_2abindS888.$0;
  _M0L6_2atmpS890 = _M0L7_2abindS888.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS891 = _M0FPB7umul128(_M0L1mS886, _M0L7_2amul1S885);
  _M0L6_2alo2S892 = _M0L7_2abindS891.$0;
  _M0L6_2ahi2S893 = _M0L7_2abindS891.$1;
  _M0L3midS894 = _M0L6_2atmpS890 + _M0L6_2alo2S892;
  if (_M0L3midS894 < _M0L6_2atmpS890) {
    _M0L6_2atmpS2583 = 1ull;
  } else {
    _M0L6_2atmpS2583 = 0ull;
  }
  _M0L2hiS895 = _M0L6_2ahi2S893 + _M0L6_2atmpS2583;
  _M0L3lo2S896 = _M0L5_2aloS889 + _M0L7_2amul0S883;
  _M0L6_2atmpS2581 = _M0L3midS894 + _M0L7_2amul1S885;
  if (_M0L3lo2S896 < _M0L5_2aloS889) {
    _M0L6_2atmpS2582 = 1ull;
  } else {
    _M0L6_2atmpS2582 = 0ull;
  }
  _M0L4mid2S897 = _M0L6_2atmpS2581 + _M0L6_2atmpS2582;
  if (_M0L4mid2S897 < _M0L3midS894) {
    _M0L6_2atmpS2580 = 1ull;
  } else {
    _M0L6_2atmpS2580 = 0ull;
  }
  _M0L3hi2S898 = _M0L2hiS895 + _M0L6_2atmpS2580;
  _M0L6_2atmpS2579 = _M0L1jS900 - 64;
  _M0L6_2atmpS2578 = _M0L6_2atmpS2579 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS899
  = _M0FPB13shiftright128(_M0L4mid2S897, _M0L3hi2S898, _M0L6_2atmpS2578);
  _M0Lm2vmS901 = 0ull;
  if (_M0L7mmShiftS902) {
    uint64_t _M0L3lo3S903 = _M0L5_2aloS889 - _M0L7_2amul0S883;
    uint64_t _M0L6_2atmpS2565 = _M0L3midS894 - _M0L7_2amul1S885;
    uint64_t _M0L6_2atmpS2566;
    uint64_t _M0L4mid3S904;
    uint64_t _M0L6_2atmpS2564;
    uint64_t _M0L3hi3S905;
    int32_t _M0L6_2atmpS2563;
    int32_t _M0L6_2atmpS2562;
    if (_M0L5_2aloS889 < _M0L3lo3S903) {
      _M0L6_2atmpS2566 = 1ull;
    } else {
      _M0L6_2atmpS2566 = 0ull;
    }
    _M0L4mid3S904 = _M0L6_2atmpS2565 - _M0L6_2atmpS2566;
    if (_M0L3midS894 < _M0L4mid3S904) {
      _M0L6_2atmpS2564 = 1ull;
    } else {
      _M0L6_2atmpS2564 = 0ull;
    }
    _M0L3hi3S905 = _M0L2hiS895 - _M0L6_2atmpS2564;
    _M0L6_2atmpS2563 = _M0L1jS900 - 64;
    _M0L6_2atmpS2562 = _M0L6_2atmpS2563 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS901
    = _M0FPB13shiftright128(_M0L4mid3S904, _M0L3hi3S905, _M0L6_2atmpS2562);
  } else {
    uint64_t _M0L3lo3S906 = _M0L5_2aloS889 + _M0L5_2aloS889;
    uint64_t _M0L6_2atmpS2573 = _M0L3midS894 + _M0L3midS894;
    uint64_t _M0L6_2atmpS2574;
    uint64_t _M0L4mid3S907;
    uint64_t _M0L6_2atmpS2571;
    uint64_t _M0L6_2atmpS2572;
    uint64_t _M0L3hi3S908;
    uint64_t _M0L3lo4S909;
    uint64_t _M0L6_2atmpS2569;
    uint64_t _M0L6_2atmpS2570;
    uint64_t _M0L4mid4S910;
    uint64_t _M0L6_2atmpS2568;
    uint64_t _M0L3hi4S911;
    int32_t _M0L6_2atmpS2567;
    if (_M0L3lo3S906 < _M0L5_2aloS889) {
      _M0L6_2atmpS2574 = 1ull;
    } else {
      _M0L6_2atmpS2574 = 0ull;
    }
    _M0L4mid3S907 = _M0L6_2atmpS2573 + _M0L6_2atmpS2574;
    _M0L6_2atmpS2571 = _M0L2hiS895 + _M0L2hiS895;
    if (_M0L4mid3S907 < _M0L3midS894) {
      _M0L6_2atmpS2572 = 1ull;
    } else {
      _M0L6_2atmpS2572 = 0ull;
    }
    _M0L3hi3S908 = _M0L6_2atmpS2571 + _M0L6_2atmpS2572;
    _M0L3lo4S909 = _M0L3lo3S906 - _M0L7_2amul0S883;
    _M0L6_2atmpS2569 = _M0L4mid3S907 - _M0L7_2amul1S885;
    if (_M0L3lo3S906 < _M0L3lo4S909) {
      _M0L6_2atmpS2570 = 1ull;
    } else {
      _M0L6_2atmpS2570 = 0ull;
    }
    _M0L4mid4S910 = _M0L6_2atmpS2569 - _M0L6_2atmpS2570;
    if (_M0L4mid3S907 < _M0L4mid4S910) {
      _M0L6_2atmpS2568 = 1ull;
    } else {
      _M0L6_2atmpS2568 = 0ull;
    }
    _M0L3hi4S911 = _M0L3hi3S908 - _M0L6_2atmpS2568;
    _M0L6_2atmpS2567 = _M0L1jS900 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS901
    = _M0FPB13shiftright128(_M0L4mid4S910, _M0L3hi4S911, _M0L6_2atmpS2567);
  }
  _M0L6_2atmpS2577 = _M0L1jS900 - 64;
  _M0L6_2atmpS2576 = _M0L6_2atmpS2577 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS912
  = _M0FPB13shiftright128(_M0L3midS894, _M0L2hiS895, _M0L6_2atmpS2576);
  _M0L6_2atmpS2575 = _M0Lm2vmS901;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS912,
                                                _M0L2vpS899,
                                                _M0L6_2atmpS2575};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS881,
  int32_t _M0L1pS882
) {
  uint64_t _M0L6_2atmpS2561;
  uint64_t _M0L6_2atmpS2560;
  uint64_t _M0L6_2atmpS2559;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2561 = 1ull << (_M0L1pS882 & 63);
  _M0L6_2atmpS2560 = _M0L6_2atmpS2561 - 1ull;
  _M0L6_2atmpS2559 = _M0L5valueS881 & _M0L6_2atmpS2560;
  return _M0L6_2atmpS2559 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS879,
  int32_t _M0L1pS880
) {
  int32_t _M0L6_2atmpS2558;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2558 = _M0FPB10pow5Factor(_M0L5valueS879);
  return _M0L6_2atmpS2558 >= _M0L1pS880;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS875) {
  uint64_t _M0L6_2atmpS2546;
  uint64_t _M0L6_2atmpS2547;
  uint64_t _M0L6_2atmpS2548;
  uint64_t _M0L6_2atmpS2549;
  int32_t _M0Lm5countS876;
  uint64_t _M0Lm5valueS877;
  uint64_t _M0L6_2atmpS2557;
  moonbit_string_t _M0L6_2atmpS2556;
  moonbit_string_t _M0L6_2atmpS3249;
  moonbit_string_t _M0L6_2atmpS2555;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2546 = _M0L5valueS875 % 5ull;
  if (_M0L6_2atmpS2546 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2547 = _M0L5valueS875 % 25ull;
  if (_M0L6_2atmpS2547 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2548 = _M0L5valueS875 % 125ull;
  if (_M0L6_2atmpS2548 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2549 = _M0L5valueS875 % 625ull;
  if (_M0L6_2atmpS2549 != 0ull) {
    return 3;
  }
  _M0Lm5countS876 = 4;
  _M0Lm5valueS877 = _M0L5valueS875 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2550 = _M0Lm5valueS877;
    if (_M0L6_2atmpS2550 > 0ull) {
      uint64_t _M0L6_2atmpS2552 = _M0Lm5valueS877;
      uint64_t _M0L6_2atmpS2551 = _M0L6_2atmpS2552 % 5ull;
      uint64_t _M0L6_2atmpS2553;
      int32_t _M0L6_2atmpS2554;
      if (_M0L6_2atmpS2551 != 0ull) {
        return _M0Lm5countS876;
      }
      _M0L6_2atmpS2553 = _M0Lm5valueS877;
      _M0Lm5valueS877 = _M0L6_2atmpS2553 / 5ull;
      _M0L6_2atmpS2554 = _M0Lm5countS876;
      _M0Lm5countS876 = _M0L6_2atmpS2554 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2557 = _M0Lm5valueS877;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2556
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2557);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3249
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS2556);
  moonbit_decref(_M0L6_2atmpS2556);
  _M0L6_2atmpS2555 = _M0L6_2atmpS3249;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2555, (moonbit_string_t)moonbit_string_literal_48.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS874,
  uint64_t _M0L2hiS872,
  int32_t _M0L4distS873
) {
  int32_t _M0L6_2atmpS2545;
  uint64_t _M0L6_2atmpS2543;
  uint64_t _M0L6_2atmpS2544;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2545 = 64 - _M0L4distS873;
  _M0L6_2atmpS2543 = _M0L2hiS872 << (_M0L6_2atmpS2545 & 63);
  _M0L6_2atmpS2544 = _M0L2loS874 >> (_M0L4distS873 & 63);
  return _M0L6_2atmpS2543 | _M0L6_2atmpS2544;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS862,
  uint64_t _M0L1bS865
) {
  uint64_t _M0L3aLoS861;
  uint64_t _M0L3aHiS863;
  uint64_t _M0L3bLoS864;
  uint64_t _M0L3bHiS866;
  uint64_t _M0L1xS867;
  uint64_t _M0L6_2atmpS2541;
  uint64_t _M0L6_2atmpS2542;
  uint64_t _M0L1yS868;
  uint64_t _M0L6_2atmpS2539;
  uint64_t _M0L6_2atmpS2540;
  uint64_t _M0L1zS869;
  uint64_t _M0L6_2atmpS2537;
  uint64_t _M0L6_2atmpS2538;
  uint64_t _M0L6_2atmpS2535;
  uint64_t _M0L6_2atmpS2536;
  uint64_t _M0L1wS870;
  uint64_t _M0L2loS871;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS861 = _M0L1aS862 & 4294967295ull;
  _M0L3aHiS863 = _M0L1aS862 >> 32;
  _M0L3bLoS864 = _M0L1bS865 & 4294967295ull;
  _M0L3bHiS866 = _M0L1bS865 >> 32;
  _M0L1xS867 = _M0L3aLoS861 * _M0L3bLoS864;
  _M0L6_2atmpS2541 = _M0L3aHiS863 * _M0L3bLoS864;
  _M0L6_2atmpS2542 = _M0L1xS867 >> 32;
  _M0L1yS868 = _M0L6_2atmpS2541 + _M0L6_2atmpS2542;
  _M0L6_2atmpS2539 = _M0L3aLoS861 * _M0L3bHiS866;
  _M0L6_2atmpS2540 = _M0L1yS868 & 4294967295ull;
  _M0L1zS869 = _M0L6_2atmpS2539 + _M0L6_2atmpS2540;
  _M0L6_2atmpS2537 = _M0L3aHiS863 * _M0L3bHiS866;
  _M0L6_2atmpS2538 = _M0L1yS868 >> 32;
  _M0L6_2atmpS2535 = _M0L6_2atmpS2537 + _M0L6_2atmpS2538;
  _M0L6_2atmpS2536 = _M0L1zS869 >> 32;
  _M0L1wS870 = _M0L6_2atmpS2535 + _M0L6_2atmpS2536;
  _M0L2loS871 = _M0L1aS862 * _M0L1bS865;
  return (struct _M0TPB7Umul128){_M0L2loS871, _M0L1wS870};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS856,
  int32_t _M0L4fromS860,
  int32_t _M0L2toS858
) {
  int32_t _M0L6_2atmpS2534;
  struct _M0TPB13StringBuilder* _M0L3bufS855;
  int32_t _M0L1iS857;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2534 = Moonbit_array_length(_M0L5bytesS856);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS855 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2534);
  _M0L1iS857 = _M0L4fromS860;
  while (1) {
    if (_M0L1iS857 < _M0L2toS858) {
      int32_t _M0L6_2atmpS2532;
      int32_t _M0L6_2atmpS2531;
      int32_t _M0L6_2atmpS2533;
      if (
        _M0L1iS857 < 0 || _M0L1iS857 >= Moonbit_array_length(_M0L5bytesS856)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2532 = (int32_t)_M0L5bytesS856[_M0L1iS857];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2531 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2532);
      moonbit_incref(_M0L3bufS855);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS855, _M0L6_2atmpS2531);
      _M0L6_2atmpS2533 = _M0L1iS857 + 1;
      _M0L1iS857 = _M0L6_2atmpS2533;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS856);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS855);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS854) {
  int32_t _M0L6_2atmpS2530;
  uint32_t _M0L6_2atmpS2529;
  uint32_t _M0L6_2atmpS2528;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2530 = _M0L1eS854 * 78913;
  _M0L6_2atmpS2529 = *(uint32_t*)&_M0L6_2atmpS2530;
  _M0L6_2atmpS2528 = _M0L6_2atmpS2529 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2528;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS853) {
  int32_t _M0L6_2atmpS2527;
  uint32_t _M0L6_2atmpS2526;
  uint32_t _M0L6_2atmpS2525;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2527 = _M0L1eS853 * 732923;
  _M0L6_2atmpS2526 = *(uint32_t*)&_M0L6_2atmpS2527;
  _M0L6_2atmpS2525 = _M0L6_2atmpS2526 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2525;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS851,
  int32_t _M0L8exponentS852,
  int32_t _M0L8mantissaS849
) {
  moonbit_string_t _M0L1sS850;
  moonbit_string_t _M0L6_2atmpS3250;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS849) {
    return (moonbit_string_t)moonbit_string_literal_49.data;
  }
  if (_M0L4signS851) {
    _M0L1sS850 = (moonbit_string_t)moonbit_string_literal_50.data;
  } else {
    _M0L1sS850 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS852) {
    moonbit_string_t _M0L6_2atmpS3251;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3251
    = moonbit_add_string(_M0L1sS850, (moonbit_string_t)moonbit_string_literal_51.data);
    moonbit_decref(_M0L1sS850);
    return _M0L6_2atmpS3251;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3250
  = moonbit_add_string(_M0L1sS850, (moonbit_string_t)moonbit_string_literal_52.data);
  moonbit_decref(_M0L1sS850);
  return _M0L6_2atmpS3250;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS848) {
  int32_t _M0L6_2atmpS2524;
  uint32_t _M0L6_2atmpS2523;
  uint32_t _M0L6_2atmpS2522;
  int32_t _M0L6_2atmpS2521;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2524 = _M0L1eS848 * 1217359;
  _M0L6_2atmpS2523 = *(uint32_t*)&_M0L6_2atmpS2524;
  _M0L6_2atmpS2522 = _M0L6_2atmpS2523 >> 19;
  _M0L6_2atmpS2521 = *(int32_t*)&_M0L6_2atmpS2522;
  return _M0L6_2atmpS2521 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS847,
  struct _M0TPB6Hasher* _M0L6hasherS846
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS846, _M0L4selfS847);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS845,
  struct _M0TPB6Hasher* _M0L6hasherS844
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS844, _M0L4selfS845);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS842,
  moonbit_string_t _M0L5valueS840
) {
  int32_t _M0L7_2abindS839;
  int32_t _M0L1iS841;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS839 = Moonbit_array_length(_M0L5valueS840);
  _M0L1iS841 = 0;
  while (1) {
    if (_M0L1iS841 < _M0L7_2abindS839) {
      int32_t _M0L6_2atmpS2519 = _M0L5valueS840[_M0L1iS841];
      int32_t _M0L6_2atmpS2518 = (int32_t)_M0L6_2atmpS2519;
      uint32_t _M0L6_2atmpS2517 = *(uint32_t*)&_M0L6_2atmpS2518;
      int32_t _M0L6_2atmpS2520;
      moonbit_incref(_M0L4selfS842);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS842, _M0L6_2atmpS2517);
      _M0L6_2atmpS2520 = _M0L1iS841 + 1;
      _M0L1iS841 = _M0L6_2atmpS2520;
      continue;
    } else {
      moonbit_decref(_M0L4selfS842);
      moonbit_decref(_M0L5valueS840);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS837,
  int32_t _M0L3idxS838
) {
  int32_t _M0L6_2atmpS3252;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3252 = _M0L4selfS837[_M0L3idxS838];
  moonbit_decref(_M0L4selfS837);
  return _M0L6_2atmpS3252;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter4dropGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS835,
  int32_t _M0L1nS833
) {
  struct _M0TPC13ref3RefGiE* _M0L9remainingS832;
  struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__* _closure_3616;
  #line 558 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L9remainingS832
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L9remainingS832)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L9remainingS832->$0 = _M0L1nS833;
  _closure_3616
  = (struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__*)moonbit_malloc(sizeof(struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__));
  Moonbit_object_header(_closure_3616)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__, $0) >> 2, 2, 0);
  _closure_3616->code = &_M0MPB4Iter4dropGRPC16string10StringViewEC2510l560;
  _closure_3616->$0 = _M0L4selfS835;
  _closure_3616->$1 = _M0L9remainingS832;
  return (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_3616;
}

void* _M0MPB4Iter4dropGRPC16string10StringViewEC2510l560(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2511
) {
  struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__* _M0L14_2acasted__envS2512;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3254;
  struct _M0TPC13ref3RefGiE* _M0L9remainingS832;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L8_2afieldS3253;
  int32_t _M0L6_2acntS3494;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS835;
  #line 560 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2512
  = (struct _M0R88Iter_3a_3adrop_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2510__l560__*)_M0L6_2aenvS2511;
  _M0L8_2afieldS3254 = _M0L14_2acasted__envS2512->$1;
  _M0L9remainingS832 = _M0L8_2afieldS3254;
  _M0L8_2afieldS3253 = _M0L14_2acasted__envS2512->$0;
  _M0L6_2acntS3494 = Moonbit_object_header(_M0L14_2acasted__envS2512)->rc;
  if (_M0L6_2acntS3494 > 1) {
    int32_t _M0L11_2anew__cntS3495 = _M0L6_2acntS3494 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2512)->rc
    = _M0L11_2anew__cntS3495;
    moonbit_incref(_M0L9remainingS832);
    moonbit_incref(_M0L8_2afieldS3253);
  } else if (_M0L6_2acntS3494 == 1) {
    #line 560 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2512);
  }
  _M0L4selfS835 = _M0L8_2afieldS3253;
  while (1) {
    int32_t _M0L3valS2513 = _M0L9remainingS832->$0;
    if (_M0L3valS2513 > 0) {
      void* _M0L7_2abindS834;
      moonbit_incref(_M0L4selfS835);
      #line 562 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
      _M0L7_2abindS834
      = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4selfS835);
      switch (Moonbit_object_tag(_M0L7_2abindS834)) {
        case 1: {
          int32_t _M0L3valS2516;
          int32_t _M0L6_2atmpS2515;
          moonbit_decref(_M0L7_2abindS834);
          _M0L3valS2516 = _M0L9remainingS832->$0;
          _M0L6_2atmpS2515 = _M0L3valS2516 - 1;
          _M0L9remainingS832->$0 = _M0L6_2atmpS2515;
          break;
        }
        default: {
          void* _M0L4NoneS2514;
          moonbit_decref(_M0L4selfS835);
          moonbit_decref(_M0L7_2abindS834);
          moonbit_decref(_M0L9remainingS832);
          _M0L4NoneS2514
          = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
          return _M0L4NoneS2514;
          break;
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L9remainingS832);
      #line 565 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
      return _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4selfS835);
    }
    break;
  }
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter4takeGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS831,
  int32_t _M0L1nS829
) {
  struct _M0TPC13ref3RefGiE* _M0L9remainingS828;
  struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__* _closure_3618;
  #line 479 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L9remainingS828
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L9remainingS828)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L9remainingS828->$0 = _M0L1nS829;
  _closure_3618
  = (struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__*)moonbit_malloc(sizeof(struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__));
  Moonbit_object_header(_closure_3618)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__, $0) >> 2, 2, 0);
  _closure_3618->code = &_M0MPB4Iter4takeGRPC16string10StringViewEC2504l481;
  _closure_3618->$0 = _M0L4selfS831;
  _closure_3618->$1 = _M0L9remainingS828;
  return (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_3618;
}

void* _M0MPB4Iter4takeGRPC16string10StringViewEC2504l481(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2505
) {
  struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__* _M0L14_2acasted__envS2506;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3256;
  struct _M0TPC13ref3RefGiE* _M0L9remainingS828;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L8_2afieldS3255;
  int32_t _M0L6_2acntS3496;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS831;
  int32_t _M0L3valS2507;
  #line 481 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2506
  = (struct _M0R88Iter_3a_3atake_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2504__l481__*)_M0L6_2aenvS2505;
  _M0L8_2afieldS3256 = _M0L14_2acasted__envS2506->$1;
  _M0L9remainingS828 = _M0L8_2afieldS3256;
  _M0L8_2afieldS3255 = _M0L14_2acasted__envS2506->$0;
  _M0L6_2acntS3496 = Moonbit_object_header(_M0L14_2acasted__envS2506)->rc;
  if (_M0L6_2acntS3496 > 1) {
    int32_t _M0L11_2anew__cntS3497 = _M0L6_2acntS3496 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2506)->rc
    = _M0L11_2anew__cntS3497;
    moonbit_incref(_M0L9remainingS828);
    moonbit_incref(_M0L8_2afieldS3255);
  } else if (_M0L6_2acntS3496 == 1) {
    #line 481 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2506);
  }
  _M0L4selfS831 = _M0L8_2afieldS3255;
  _M0L3valS2507 = _M0L9remainingS828->$0;
  if (_M0L3valS2507 > 0) {
    void* _M0L6resultS830;
    #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L6resultS830
    = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4selfS831);
    switch (Moonbit_object_tag(_M0L6resultS830)) {
      case 1: {
        int32_t _M0L3valS2509 = _M0L9remainingS828->$0;
        int32_t _M0L6_2atmpS2508 = _M0L3valS2509 - 1;
        _M0L9remainingS828->$0 = _M0L6_2atmpS2508;
        moonbit_decref(_M0L9remainingS828);
        break;
      }
      default: {
        moonbit_decref(_M0L9remainingS828);
        break;
      }
    }
    return _M0L6resultS830;
  } else {
    moonbit_decref(_M0L4selfS831);
    moonbit_decref(_M0L9remainingS828);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

void* _M0IPC16option6OptionPB6ToJson8to__jsonGsE(
  moonbit_string_t _M0L4selfS825
) {
  #line 287 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS825 == 0) {
    if (_M0L4selfS825) {
      moonbit_decref(_M0L4selfS825);
    }
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    moonbit_string_t _M0L7_2aSomeS826 = _M0L4selfS825;
    moonbit_string_t _M0L8_2avalueS827 = _M0L7_2aSomeS826;
    void* _M0L6_2atmpS2503;
    void** _M0L6_2atmpS2502;
    struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2501;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L6_2atmpS2503
    = _M0IPC16string6StringPB6ToJson8to__json(_M0L8_2avalueS827);
    _M0L6_2atmpS2502 = (void**)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS2502[0] = _M0L6_2atmpS2503;
    _M0L6_2atmpS2501
    = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
    Moonbit_object_header(_M0L6_2atmpS2501)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2501->$0 = _M0L6_2atmpS2502;
    _M0L6_2atmpS2501->$1 = 1;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json5array(_M0L6_2atmpS2501);
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS824) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS824;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS823) {
  void* _block_3619;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3619 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3619)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3619)->$0 = _M0L6stringS823;
  return _block_3619;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS816
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3257;
  int32_t _M0L6_2acntS3498;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2500;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS815;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__* _closure_3620;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2495;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3257 = _M0L4selfS816->$5;
  _M0L6_2acntS3498 = Moonbit_object_header(_M0L4selfS816)->rc;
  if (_M0L6_2acntS3498 > 1) {
    int32_t _M0L11_2anew__cntS3500 = _M0L6_2acntS3498 - 1;
    Moonbit_object_header(_M0L4selfS816)->rc = _M0L11_2anew__cntS3500;
    if (_M0L8_2afieldS3257) {
      moonbit_incref(_M0L8_2afieldS3257);
    }
  } else if (_M0L6_2acntS3498 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3499 = _M0L4selfS816->$0;
    moonbit_decref(_M0L8_2afieldS3499);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS816);
  }
  _M0L4headS2500 = _M0L8_2afieldS3257;
  _M0L11curr__entryS815
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS815)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS815->$0 = _M0L4headS2500;
  _closure_3620
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__));
  Moonbit_object_header(_closure_3620)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__, $0) >> 2, 1, 0);
  _closure_3620->code = &_M0MPB3Map4iterGsRPB4JsonEC2496l591;
  _closure_3620->$0 = _M0L11curr__entryS815;
  _M0L6_2atmpS2495 = (struct _M0TWEOUsRPB4JsonE*)_closure_3620;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2495);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2496l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2497
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__* _M0L14_2acasted__envS2498;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3263;
  int32_t _M0L6_2acntS3501;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS815;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3262;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS817;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2498
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2496__l591__*)_M0L6_2aenvS2497;
  _M0L8_2afieldS3263 = _M0L14_2acasted__envS2498->$0;
  _M0L6_2acntS3501 = Moonbit_object_header(_M0L14_2acasted__envS2498)->rc;
  if (_M0L6_2acntS3501 > 1) {
    int32_t _M0L11_2anew__cntS3502 = _M0L6_2acntS3501 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2498)->rc
    = _M0L11_2anew__cntS3502;
    moonbit_incref(_M0L8_2afieldS3263);
  } else if (_M0L6_2acntS3501 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2498);
  }
  _M0L11curr__entryS815 = _M0L8_2afieldS3263;
  _M0L8_2afieldS3262 = _M0L11curr__entryS815->$0;
  _M0L7_2abindS817 = _M0L8_2afieldS3262;
  if (_M0L7_2abindS817 == 0) {
    moonbit_decref(_M0L11curr__entryS815);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS818 = _M0L7_2abindS817;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS819 = _M0L7_2aSomeS818;
    moonbit_string_t _M0L8_2afieldS3261 = _M0L4_2axS819->$4;
    moonbit_string_t _M0L6_2akeyS820 = _M0L8_2afieldS3261;
    void* _M0L8_2afieldS3260 = _M0L4_2axS819->$5;
    void* _M0L8_2avalueS821 = _M0L8_2afieldS3260;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3259 = _M0L4_2axS819->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS822 = _M0L8_2afieldS3259;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3258 =
      _M0L11curr__entryS815->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2499;
    if (_M0L7_2anextS822) {
      moonbit_incref(_M0L7_2anextS822);
    }
    moonbit_incref(_M0L8_2avalueS821);
    moonbit_incref(_M0L6_2akeyS820);
    if (_M0L6_2aoldS3258) {
      moonbit_decref(_M0L6_2aoldS3258);
    }
    _M0L11curr__entryS815->$0 = _M0L7_2anextS822;
    moonbit_decref(_M0L11curr__entryS815);
    _M0L8_2atupleS2499
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2499)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2499->$0 = _M0L6_2akeyS820;
    _M0L8_2atupleS2499->$1 = _M0L8_2avalueS821;
    return _M0L8_2atupleS2499;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS814
) {
  int32_t _M0L8_2afieldS3264;
  int32_t _M0L4sizeS2494;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3264 = _M0L4selfS814->$1;
  moonbit_decref(_M0L4selfS814);
  _M0L4sizeS2494 = _M0L8_2afieldS3264;
  return _M0L4sizeS2494 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS801,
  int32_t _M0L3keyS797
) {
  int32_t _M0L4hashS796;
  int32_t _M0L14capacity__maskS2479;
  int32_t _M0L6_2atmpS2478;
  int32_t _M0L1iS798;
  int32_t _M0L3idxS799;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS796 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS797);
  _M0L14capacity__maskS2479 = _M0L4selfS801->$3;
  _M0L6_2atmpS2478 = _M0L4hashS796 & _M0L14capacity__maskS2479;
  _M0L1iS798 = 0;
  _M0L3idxS799 = _M0L6_2atmpS2478;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3268 =
      _M0L4selfS801->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2477 =
      _M0L8_2afieldS3268;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3267;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS800;
    if (
      _M0L3idxS799 < 0
      || _M0L3idxS799 >= Moonbit_array_length(_M0L7entriesS2477)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3267
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2477[
        _M0L3idxS799
      ];
    _M0L7_2abindS800 = _M0L6_2atmpS3267;
    if (_M0L7_2abindS800 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2466;
      if (_M0L7_2abindS800) {
        moonbit_incref(_M0L7_2abindS800);
      }
      moonbit_decref(_M0L4selfS801);
      if (_M0L7_2abindS800) {
        moonbit_decref(_M0L7_2abindS800);
      }
      _M0L6_2atmpS2466 = 0;
      return _M0L6_2atmpS2466;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS802 =
        _M0L7_2abindS800;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS803 =
        _M0L7_2aSomeS802;
      int32_t _M0L4hashS2468 = _M0L8_2aentryS803->$3;
      int32_t _if__result_3622;
      int32_t _M0L8_2afieldS3265;
      int32_t _M0L3pslS2471;
      int32_t _M0L6_2atmpS2473;
      int32_t _M0L6_2atmpS2475;
      int32_t _M0L14capacity__maskS2476;
      int32_t _M0L6_2atmpS2474;
      if (_M0L4hashS2468 == _M0L4hashS796) {
        int32_t _M0L3keyS2467 = _M0L8_2aentryS803->$4;
        _if__result_3622 = _M0L3keyS2467 == _M0L3keyS797;
      } else {
        _if__result_3622 = 0;
      }
      if (_if__result_3622) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3266;
        int32_t _M0L6_2acntS3503;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2470;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2469;
        moonbit_incref(_M0L8_2aentryS803);
        moonbit_decref(_M0L4selfS801);
        _M0L8_2afieldS3266 = _M0L8_2aentryS803->$5;
        _M0L6_2acntS3503 = Moonbit_object_header(_M0L8_2aentryS803)->rc;
        if (_M0L6_2acntS3503 > 1) {
          int32_t _M0L11_2anew__cntS3505 = _M0L6_2acntS3503 - 1;
          Moonbit_object_header(_M0L8_2aentryS803)->rc
          = _M0L11_2anew__cntS3505;
          moonbit_incref(_M0L8_2afieldS3266);
        } else if (_M0L6_2acntS3503 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3504 =
            _M0L8_2aentryS803->$1;
          if (_M0L8_2afieldS3504) {
            moonbit_decref(_M0L8_2afieldS3504);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS803);
        }
        _M0L5valueS2470 = _M0L8_2afieldS3266;
        _M0L6_2atmpS2469 = _M0L5valueS2470;
        return _M0L6_2atmpS2469;
      } else {
        moonbit_incref(_M0L8_2aentryS803);
      }
      _M0L8_2afieldS3265 = _M0L8_2aentryS803->$2;
      moonbit_decref(_M0L8_2aentryS803);
      _M0L3pslS2471 = _M0L8_2afieldS3265;
      if (_M0L1iS798 > _M0L3pslS2471) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2472;
        moonbit_decref(_M0L4selfS801);
        _M0L6_2atmpS2472 = 0;
        return _M0L6_2atmpS2472;
      }
      _M0L6_2atmpS2473 = _M0L1iS798 + 1;
      _M0L6_2atmpS2475 = _M0L3idxS799 + 1;
      _M0L14capacity__maskS2476 = _M0L4selfS801->$3;
      _M0L6_2atmpS2474 = _M0L6_2atmpS2475 & _M0L14capacity__maskS2476;
      _M0L1iS798 = _M0L6_2atmpS2473;
      _M0L3idxS799 = _M0L6_2atmpS2474;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS810,
  moonbit_string_t _M0L3keyS806
) {
  int32_t _M0L4hashS805;
  int32_t _M0L14capacity__maskS2493;
  int32_t _M0L6_2atmpS2492;
  int32_t _M0L1iS807;
  int32_t _M0L3idxS808;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS806);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS805 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS806);
  _M0L14capacity__maskS2493 = _M0L4selfS810->$3;
  _M0L6_2atmpS2492 = _M0L4hashS805 & _M0L14capacity__maskS2493;
  _M0L1iS807 = 0;
  _M0L3idxS808 = _M0L6_2atmpS2492;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3274 =
      _M0L4selfS810->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2491 =
      _M0L8_2afieldS3274;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3273;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS809;
    if (
      _M0L3idxS808 < 0
      || _M0L3idxS808 >= Moonbit_array_length(_M0L7entriesS2491)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3273
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2491[
        _M0L3idxS808
      ];
    _M0L7_2abindS809 = _M0L6_2atmpS3273;
    if (_M0L7_2abindS809 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2480;
      if (_M0L7_2abindS809) {
        moonbit_incref(_M0L7_2abindS809);
      }
      moonbit_decref(_M0L4selfS810);
      if (_M0L7_2abindS809) {
        moonbit_decref(_M0L7_2abindS809);
      }
      moonbit_decref(_M0L3keyS806);
      _M0L6_2atmpS2480 = 0;
      return _M0L6_2atmpS2480;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS811 =
        _M0L7_2abindS809;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS812 =
        _M0L7_2aSomeS811;
      int32_t _M0L4hashS2482 = _M0L8_2aentryS812->$3;
      int32_t _if__result_3624;
      int32_t _M0L8_2afieldS3269;
      int32_t _M0L3pslS2485;
      int32_t _M0L6_2atmpS2487;
      int32_t _M0L6_2atmpS2489;
      int32_t _M0L14capacity__maskS2490;
      int32_t _M0L6_2atmpS2488;
      if (_M0L4hashS2482 == _M0L4hashS805) {
        moonbit_string_t _M0L8_2afieldS3272 = _M0L8_2aentryS812->$4;
        moonbit_string_t _M0L3keyS2481 = _M0L8_2afieldS3272;
        int32_t _M0L6_2atmpS3271;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3271
        = moonbit_val_array_equal(_M0L3keyS2481, _M0L3keyS806);
        _if__result_3624 = _M0L6_2atmpS3271;
      } else {
        _if__result_3624 = 0;
      }
      if (_if__result_3624) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3270;
        int32_t _M0L6_2acntS3506;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2484;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2483;
        moonbit_incref(_M0L8_2aentryS812);
        moonbit_decref(_M0L4selfS810);
        moonbit_decref(_M0L3keyS806);
        _M0L8_2afieldS3270 = _M0L8_2aentryS812->$5;
        _M0L6_2acntS3506 = Moonbit_object_header(_M0L8_2aentryS812)->rc;
        if (_M0L6_2acntS3506 > 1) {
          int32_t _M0L11_2anew__cntS3509 = _M0L6_2acntS3506 - 1;
          Moonbit_object_header(_M0L8_2aentryS812)->rc
          = _M0L11_2anew__cntS3509;
          moonbit_incref(_M0L8_2afieldS3270);
        } else if (_M0L6_2acntS3506 == 1) {
          moonbit_string_t _M0L8_2afieldS3508 = _M0L8_2aentryS812->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3507;
          moonbit_decref(_M0L8_2afieldS3508);
          _M0L8_2afieldS3507 = _M0L8_2aentryS812->$1;
          if (_M0L8_2afieldS3507) {
            moonbit_decref(_M0L8_2afieldS3507);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS812);
        }
        _M0L5valueS2484 = _M0L8_2afieldS3270;
        _M0L6_2atmpS2483 = _M0L5valueS2484;
        return _M0L6_2atmpS2483;
      } else {
        moonbit_incref(_M0L8_2aentryS812);
      }
      _M0L8_2afieldS3269 = _M0L8_2aentryS812->$2;
      moonbit_decref(_M0L8_2aentryS812);
      _M0L3pslS2485 = _M0L8_2afieldS3269;
      if (_M0L1iS807 > _M0L3pslS2485) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2486;
        moonbit_decref(_M0L4selfS810);
        moonbit_decref(_M0L3keyS806);
        _M0L6_2atmpS2486 = 0;
        return _M0L6_2atmpS2486;
      }
      _M0L6_2atmpS2487 = _M0L1iS807 + 1;
      _M0L6_2atmpS2489 = _M0L3idxS808 + 1;
      _M0L14capacity__maskS2490 = _M0L4selfS810->$3;
      _M0L6_2atmpS2488 = _M0L6_2atmpS2489 & _M0L14capacity__maskS2490;
      _M0L1iS807 = _M0L6_2atmpS2487;
      _M0L3idxS808 = _M0L6_2atmpS2488;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS781
) {
  int32_t _M0L6lengthS780;
  int32_t _M0Lm8capacityS782;
  int32_t _M0L6_2atmpS2443;
  int32_t _M0L6_2atmpS2442;
  int32_t _M0L6_2atmpS2453;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS783;
  int32_t _M0L3endS2451;
  int32_t _M0L5startS2452;
  int32_t _M0L7_2abindS784;
  int32_t _M0L2__S785;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS781.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS780
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS781);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS782 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS780);
  _M0L6_2atmpS2443 = _M0Lm8capacityS782;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2442 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2443);
  if (_M0L6lengthS780 > _M0L6_2atmpS2442) {
    int32_t _M0L6_2atmpS2444 = _M0Lm8capacityS782;
    _M0Lm8capacityS782 = _M0L6_2atmpS2444 * 2;
  }
  _M0L6_2atmpS2453 = _M0Lm8capacityS782;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS783
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2453);
  _M0L3endS2451 = _M0L3arrS781.$2;
  _M0L5startS2452 = _M0L3arrS781.$1;
  _M0L7_2abindS784 = _M0L3endS2451 - _M0L5startS2452;
  _M0L2__S785 = 0;
  while (1) {
    if (_M0L2__S785 < _M0L7_2abindS784) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3278 =
        _M0L3arrS781.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2448 =
        _M0L8_2afieldS3278;
      int32_t _M0L5startS2450 = _M0L3arrS781.$1;
      int32_t _M0L6_2atmpS2449 = _M0L5startS2450 + _M0L2__S785;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3277 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2448[
          _M0L6_2atmpS2449
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS786 =
        _M0L6_2atmpS3277;
      moonbit_string_t _M0L8_2afieldS3276 = _M0L1eS786->$0;
      moonbit_string_t _M0L6_2atmpS2445 = _M0L8_2afieldS3276;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3275 =
        _M0L1eS786->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2446 =
        _M0L8_2afieldS3275;
      int32_t _M0L6_2atmpS2447;
      moonbit_incref(_M0L6_2atmpS2446);
      moonbit_incref(_M0L6_2atmpS2445);
      moonbit_incref(_M0L1mS783);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS783, _M0L6_2atmpS2445, _M0L6_2atmpS2446);
      _M0L6_2atmpS2447 = _M0L2__S785 + 1;
      _M0L2__S785 = _M0L6_2atmpS2447;
      continue;
    } else {
      moonbit_decref(_M0L3arrS781.$0);
    }
    break;
  }
  return _M0L1mS783;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS789
) {
  int32_t _M0L6lengthS788;
  int32_t _M0Lm8capacityS790;
  int32_t _M0L6_2atmpS2455;
  int32_t _M0L6_2atmpS2454;
  int32_t _M0L6_2atmpS2465;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS791;
  int32_t _M0L3endS2463;
  int32_t _M0L5startS2464;
  int32_t _M0L7_2abindS792;
  int32_t _M0L2__S793;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS789.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS788
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS789);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS790 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS788);
  _M0L6_2atmpS2455 = _M0Lm8capacityS790;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2454 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2455);
  if (_M0L6lengthS788 > _M0L6_2atmpS2454) {
    int32_t _M0L6_2atmpS2456 = _M0Lm8capacityS790;
    _M0Lm8capacityS790 = _M0L6_2atmpS2456 * 2;
  }
  _M0L6_2atmpS2465 = _M0Lm8capacityS790;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS791
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2465);
  _M0L3endS2463 = _M0L3arrS789.$2;
  _M0L5startS2464 = _M0L3arrS789.$1;
  _M0L7_2abindS792 = _M0L3endS2463 - _M0L5startS2464;
  _M0L2__S793 = 0;
  while (1) {
    if (_M0L2__S793 < _M0L7_2abindS792) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3281 =
        _M0L3arrS789.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2460 =
        _M0L8_2afieldS3281;
      int32_t _M0L5startS2462 = _M0L3arrS789.$1;
      int32_t _M0L6_2atmpS2461 = _M0L5startS2462 + _M0L2__S793;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3280 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2460[
          _M0L6_2atmpS2461
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS794 = _M0L6_2atmpS3280;
      int32_t _M0L6_2atmpS2457 = _M0L1eS794->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3279 =
        _M0L1eS794->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2458 =
        _M0L8_2afieldS3279;
      int32_t _M0L6_2atmpS2459;
      moonbit_incref(_M0L6_2atmpS2458);
      moonbit_incref(_M0L1mS791);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS791, _M0L6_2atmpS2457, _M0L6_2atmpS2458);
      _M0L6_2atmpS2459 = _M0L2__S793 + 1;
      _M0L2__S793 = _M0L6_2atmpS2459;
      continue;
    } else {
      moonbit_decref(_M0L3arrS789.$0);
    }
    break;
  }
  return _M0L1mS791;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS774,
  moonbit_string_t _M0L3keyS775,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS776
) {
  int32_t _M0L6_2atmpS2440;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS775);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2440 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS775);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS774, _M0L3keyS775, _M0L5valueS776, _M0L6_2atmpS2440);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS777,
  int32_t _M0L3keyS778,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS779
) {
  int32_t _M0L6_2atmpS2441;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2441 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS778);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS777, _M0L3keyS778, _M0L5valueS779, _M0L6_2atmpS2441);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS753
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3288;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS752;
  int32_t _M0L8capacityS2432;
  int32_t _M0L13new__capacityS754;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2427;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2426;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3287;
  int32_t _M0L6_2atmpS2428;
  int32_t _M0L8capacityS2430;
  int32_t _M0L6_2atmpS2429;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2431;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3286;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS755;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3288 = _M0L4selfS753->$5;
  _M0L9old__headS752 = _M0L8_2afieldS3288;
  _M0L8capacityS2432 = _M0L4selfS753->$2;
  _M0L13new__capacityS754 = _M0L8capacityS2432 << 1;
  _M0L6_2atmpS2427 = 0;
  _M0L6_2atmpS2426
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS754, _M0L6_2atmpS2427);
  _M0L6_2aoldS3287 = _M0L4selfS753->$0;
  if (_M0L9old__headS752) {
    moonbit_incref(_M0L9old__headS752);
  }
  moonbit_decref(_M0L6_2aoldS3287);
  _M0L4selfS753->$0 = _M0L6_2atmpS2426;
  _M0L4selfS753->$2 = _M0L13new__capacityS754;
  _M0L6_2atmpS2428 = _M0L13new__capacityS754 - 1;
  _M0L4selfS753->$3 = _M0L6_2atmpS2428;
  _M0L8capacityS2430 = _M0L4selfS753->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2429 = _M0FPB21calc__grow__threshold(_M0L8capacityS2430);
  _M0L4selfS753->$4 = _M0L6_2atmpS2429;
  _M0L4selfS753->$1 = 0;
  _M0L6_2atmpS2431 = 0;
  _M0L6_2aoldS3286 = _M0L4selfS753->$5;
  if (_M0L6_2aoldS3286) {
    moonbit_decref(_M0L6_2aoldS3286);
  }
  _M0L4selfS753->$5 = _M0L6_2atmpS2431;
  _M0L4selfS753->$6 = -1;
  _M0L8_2aparamS755 = _M0L9old__headS752;
  while (1) {
    if (_M0L8_2aparamS755 == 0) {
      if (_M0L8_2aparamS755) {
        moonbit_decref(_M0L8_2aparamS755);
      }
      moonbit_decref(_M0L4selfS753);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS756 =
        _M0L8_2aparamS755;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS757 =
        _M0L7_2aSomeS756;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3285 =
        _M0L4_2axS757->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS758 =
        _M0L8_2afieldS3285;
      moonbit_string_t _M0L8_2afieldS3284 = _M0L4_2axS757->$4;
      moonbit_string_t _M0L6_2akeyS759 = _M0L8_2afieldS3284;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3283 =
        _M0L4_2axS757->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS760 =
        _M0L8_2afieldS3283;
      int32_t _M0L8_2afieldS3282 = _M0L4_2axS757->$3;
      int32_t _M0L6_2acntS3510 = Moonbit_object_header(_M0L4_2axS757)->rc;
      int32_t _M0L7_2ahashS761;
      if (_M0L6_2acntS3510 > 1) {
        int32_t _M0L11_2anew__cntS3511 = _M0L6_2acntS3510 - 1;
        Moonbit_object_header(_M0L4_2axS757)->rc = _M0L11_2anew__cntS3511;
        moonbit_incref(_M0L8_2avalueS760);
        moonbit_incref(_M0L6_2akeyS759);
        if (_M0L7_2anextS758) {
          moonbit_incref(_M0L7_2anextS758);
        }
      } else if (_M0L6_2acntS3510 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS757);
      }
      _M0L7_2ahashS761 = _M0L8_2afieldS3282;
      moonbit_incref(_M0L4selfS753);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS753, _M0L6_2akeyS759, _M0L8_2avalueS760, _M0L7_2ahashS761);
      _M0L8_2aparamS755 = _M0L7_2anextS758;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS764
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3294;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS763;
  int32_t _M0L8capacityS2439;
  int32_t _M0L13new__capacityS765;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2434;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2433;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3293;
  int32_t _M0L6_2atmpS2435;
  int32_t _M0L8capacityS2437;
  int32_t _M0L6_2atmpS2436;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2438;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3292;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS766;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3294 = _M0L4selfS764->$5;
  _M0L9old__headS763 = _M0L8_2afieldS3294;
  _M0L8capacityS2439 = _M0L4selfS764->$2;
  _M0L13new__capacityS765 = _M0L8capacityS2439 << 1;
  _M0L6_2atmpS2434 = 0;
  _M0L6_2atmpS2433
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS765, _M0L6_2atmpS2434);
  _M0L6_2aoldS3293 = _M0L4selfS764->$0;
  if (_M0L9old__headS763) {
    moonbit_incref(_M0L9old__headS763);
  }
  moonbit_decref(_M0L6_2aoldS3293);
  _M0L4selfS764->$0 = _M0L6_2atmpS2433;
  _M0L4selfS764->$2 = _M0L13new__capacityS765;
  _M0L6_2atmpS2435 = _M0L13new__capacityS765 - 1;
  _M0L4selfS764->$3 = _M0L6_2atmpS2435;
  _M0L8capacityS2437 = _M0L4selfS764->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2436 = _M0FPB21calc__grow__threshold(_M0L8capacityS2437);
  _M0L4selfS764->$4 = _M0L6_2atmpS2436;
  _M0L4selfS764->$1 = 0;
  _M0L6_2atmpS2438 = 0;
  _M0L6_2aoldS3292 = _M0L4selfS764->$5;
  if (_M0L6_2aoldS3292) {
    moonbit_decref(_M0L6_2aoldS3292);
  }
  _M0L4selfS764->$5 = _M0L6_2atmpS2438;
  _M0L4selfS764->$6 = -1;
  _M0L8_2aparamS766 = _M0L9old__headS763;
  while (1) {
    if (_M0L8_2aparamS766 == 0) {
      if (_M0L8_2aparamS766) {
        moonbit_decref(_M0L8_2aparamS766);
      }
      moonbit_decref(_M0L4selfS764);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS767 =
        _M0L8_2aparamS766;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS768 =
        _M0L7_2aSomeS767;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3291 =
        _M0L4_2axS768->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS769 =
        _M0L8_2afieldS3291;
      int32_t _M0L6_2akeyS770 = _M0L4_2axS768->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3290 =
        _M0L4_2axS768->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS771 =
        _M0L8_2afieldS3290;
      int32_t _M0L8_2afieldS3289 = _M0L4_2axS768->$3;
      int32_t _M0L6_2acntS3512 = Moonbit_object_header(_M0L4_2axS768)->rc;
      int32_t _M0L7_2ahashS772;
      if (_M0L6_2acntS3512 > 1) {
        int32_t _M0L11_2anew__cntS3513 = _M0L6_2acntS3512 - 1;
        Moonbit_object_header(_M0L4_2axS768)->rc = _M0L11_2anew__cntS3513;
        moonbit_incref(_M0L8_2avalueS771);
        if (_M0L7_2anextS769) {
          moonbit_incref(_M0L7_2anextS769);
        }
      } else if (_M0L6_2acntS3512 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS768);
      }
      _M0L7_2ahashS772 = _M0L8_2afieldS3289;
      moonbit_incref(_M0L4selfS764);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS764, _M0L6_2akeyS770, _M0L8_2avalueS771, _M0L7_2ahashS772);
      _M0L8_2aparamS766 = _M0L7_2anextS769;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS723,
  moonbit_string_t _M0L3keyS729,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS730,
  int32_t _M0L4hashS725
) {
  int32_t _M0L14capacity__maskS2407;
  int32_t _M0L6_2atmpS2406;
  int32_t _M0L3pslS720;
  int32_t _M0L3idxS721;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2407 = _M0L4selfS723->$3;
  _M0L6_2atmpS2406 = _M0L4hashS725 & _M0L14capacity__maskS2407;
  _M0L3pslS720 = 0;
  _M0L3idxS721 = _M0L6_2atmpS2406;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3299 =
      _M0L4selfS723->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2405 =
      _M0L8_2afieldS3299;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3298;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS722;
    if (
      _M0L3idxS721 < 0
      || _M0L3idxS721 >= Moonbit_array_length(_M0L7entriesS2405)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3298
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2405[
        _M0L3idxS721
      ];
    _M0L7_2abindS722 = _M0L6_2atmpS3298;
    if (_M0L7_2abindS722 == 0) {
      int32_t _M0L4sizeS2390 = _M0L4selfS723->$1;
      int32_t _M0L8grow__atS2391 = _M0L4selfS723->$4;
      int32_t _M0L7_2abindS726;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS727;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS728;
      if (_M0L4sizeS2390 >= _M0L8grow__atS2391) {
        int32_t _M0L14capacity__maskS2393;
        int32_t _M0L6_2atmpS2392;
        moonbit_incref(_M0L4selfS723);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS723);
        _M0L14capacity__maskS2393 = _M0L4selfS723->$3;
        _M0L6_2atmpS2392 = _M0L4hashS725 & _M0L14capacity__maskS2393;
        _M0L3pslS720 = 0;
        _M0L3idxS721 = _M0L6_2atmpS2392;
        continue;
      }
      _M0L7_2abindS726 = _M0L4selfS723->$6;
      _M0L7_2abindS727 = 0;
      _M0L5entryS728
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS728)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS728->$0 = _M0L7_2abindS726;
      _M0L5entryS728->$1 = _M0L7_2abindS727;
      _M0L5entryS728->$2 = _M0L3pslS720;
      _M0L5entryS728->$3 = _M0L4hashS725;
      _M0L5entryS728->$4 = _M0L3keyS729;
      _M0L5entryS728->$5 = _M0L5valueS730;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS723, _M0L3idxS721, _M0L5entryS728);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS731 =
        _M0L7_2abindS722;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS732 =
        _M0L7_2aSomeS731;
      int32_t _M0L4hashS2395 = _M0L14_2acurr__entryS732->$3;
      int32_t _if__result_3630;
      int32_t _M0L3pslS2396;
      int32_t _M0L6_2atmpS2401;
      int32_t _M0L6_2atmpS2403;
      int32_t _M0L14capacity__maskS2404;
      int32_t _M0L6_2atmpS2402;
      if (_M0L4hashS2395 == _M0L4hashS725) {
        moonbit_string_t _M0L8_2afieldS3297 = _M0L14_2acurr__entryS732->$4;
        moonbit_string_t _M0L3keyS2394 = _M0L8_2afieldS3297;
        int32_t _M0L6_2atmpS3296;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3296
        = moonbit_val_array_equal(_M0L3keyS2394, _M0L3keyS729);
        _if__result_3630 = _M0L6_2atmpS3296;
      } else {
        _if__result_3630 = 0;
      }
      if (_if__result_3630) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3295;
        moonbit_incref(_M0L14_2acurr__entryS732);
        moonbit_decref(_M0L3keyS729);
        moonbit_decref(_M0L4selfS723);
        _M0L6_2aoldS3295 = _M0L14_2acurr__entryS732->$5;
        moonbit_decref(_M0L6_2aoldS3295);
        _M0L14_2acurr__entryS732->$5 = _M0L5valueS730;
        moonbit_decref(_M0L14_2acurr__entryS732);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS732);
      }
      _M0L3pslS2396 = _M0L14_2acurr__entryS732->$2;
      if (_M0L3pslS720 > _M0L3pslS2396) {
        int32_t _M0L4sizeS2397 = _M0L4selfS723->$1;
        int32_t _M0L8grow__atS2398 = _M0L4selfS723->$4;
        int32_t _M0L7_2abindS733;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS734;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS735;
        if (_M0L4sizeS2397 >= _M0L8grow__atS2398) {
          int32_t _M0L14capacity__maskS2400;
          int32_t _M0L6_2atmpS2399;
          moonbit_decref(_M0L14_2acurr__entryS732);
          moonbit_incref(_M0L4selfS723);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS723);
          _M0L14capacity__maskS2400 = _M0L4selfS723->$3;
          _M0L6_2atmpS2399 = _M0L4hashS725 & _M0L14capacity__maskS2400;
          _M0L3pslS720 = 0;
          _M0L3idxS721 = _M0L6_2atmpS2399;
          continue;
        }
        moonbit_incref(_M0L4selfS723);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS723, _M0L3idxS721, _M0L14_2acurr__entryS732);
        _M0L7_2abindS733 = _M0L4selfS723->$6;
        _M0L7_2abindS734 = 0;
        _M0L5entryS735
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS735)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS735->$0 = _M0L7_2abindS733;
        _M0L5entryS735->$1 = _M0L7_2abindS734;
        _M0L5entryS735->$2 = _M0L3pslS720;
        _M0L5entryS735->$3 = _M0L4hashS725;
        _M0L5entryS735->$4 = _M0L3keyS729;
        _M0L5entryS735->$5 = _M0L5valueS730;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS723, _M0L3idxS721, _M0L5entryS735);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS732);
      }
      _M0L6_2atmpS2401 = _M0L3pslS720 + 1;
      _M0L6_2atmpS2403 = _M0L3idxS721 + 1;
      _M0L14capacity__maskS2404 = _M0L4selfS723->$3;
      _M0L6_2atmpS2402 = _M0L6_2atmpS2403 & _M0L14capacity__maskS2404;
      _M0L3pslS720 = _M0L6_2atmpS2401;
      _M0L3idxS721 = _M0L6_2atmpS2402;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS739,
  int32_t _M0L3keyS745,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS746,
  int32_t _M0L4hashS741
) {
  int32_t _M0L14capacity__maskS2425;
  int32_t _M0L6_2atmpS2424;
  int32_t _M0L3pslS736;
  int32_t _M0L3idxS737;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2425 = _M0L4selfS739->$3;
  _M0L6_2atmpS2424 = _M0L4hashS741 & _M0L14capacity__maskS2425;
  _M0L3pslS736 = 0;
  _M0L3idxS737 = _M0L6_2atmpS2424;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3302 =
      _M0L4selfS739->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2423 =
      _M0L8_2afieldS3302;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3301;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS738;
    if (
      _M0L3idxS737 < 0
      || _M0L3idxS737 >= Moonbit_array_length(_M0L7entriesS2423)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3301
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2423[
        _M0L3idxS737
      ];
    _M0L7_2abindS738 = _M0L6_2atmpS3301;
    if (_M0L7_2abindS738 == 0) {
      int32_t _M0L4sizeS2408 = _M0L4selfS739->$1;
      int32_t _M0L8grow__atS2409 = _M0L4selfS739->$4;
      int32_t _M0L7_2abindS742;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS743;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS744;
      if (_M0L4sizeS2408 >= _M0L8grow__atS2409) {
        int32_t _M0L14capacity__maskS2411;
        int32_t _M0L6_2atmpS2410;
        moonbit_incref(_M0L4selfS739);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS739);
        _M0L14capacity__maskS2411 = _M0L4selfS739->$3;
        _M0L6_2atmpS2410 = _M0L4hashS741 & _M0L14capacity__maskS2411;
        _M0L3pslS736 = 0;
        _M0L3idxS737 = _M0L6_2atmpS2410;
        continue;
      }
      _M0L7_2abindS742 = _M0L4selfS739->$6;
      _M0L7_2abindS743 = 0;
      _M0L5entryS744
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS744)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS744->$0 = _M0L7_2abindS742;
      _M0L5entryS744->$1 = _M0L7_2abindS743;
      _M0L5entryS744->$2 = _M0L3pslS736;
      _M0L5entryS744->$3 = _M0L4hashS741;
      _M0L5entryS744->$4 = _M0L3keyS745;
      _M0L5entryS744->$5 = _M0L5valueS746;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS739, _M0L3idxS737, _M0L5entryS744);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS747 =
        _M0L7_2abindS738;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS748 =
        _M0L7_2aSomeS747;
      int32_t _M0L4hashS2413 = _M0L14_2acurr__entryS748->$3;
      int32_t _if__result_3632;
      int32_t _M0L3pslS2414;
      int32_t _M0L6_2atmpS2419;
      int32_t _M0L6_2atmpS2421;
      int32_t _M0L14capacity__maskS2422;
      int32_t _M0L6_2atmpS2420;
      if (_M0L4hashS2413 == _M0L4hashS741) {
        int32_t _M0L3keyS2412 = _M0L14_2acurr__entryS748->$4;
        _if__result_3632 = _M0L3keyS2412 == _M0L3keyS745;
      } else {
        _if__result_3632 = 0;
      }
      if (_if__result_3632) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3300;
        moonbit_incref(_M0L14_2acurr__entryS748);
        moonbit_decref(_M0L4selfS739);
        _M0L6_2aoldS3300 = _M0L14_2acurr__entryS748->$5;
        moonbit_decref(_M0L6_2aoldS3300);
        _M0L14_2acurr__entryS748->$5 = _M0L5valueS746;
        moonbit_decref(_M0L14_2acurr__entryS748);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS748);
      }
      _M0L3pslS2414 = _M0L14_2acurr__entryS748->$2;
      if (_M0L3pslS736 > _M0L3pslS2414) {
        int32_t _M0L4sizeS2415 = _M0L4selfS739->$1;
        int32_t _M0L8grow__atS2416 = _M0L4selfS739->$4;
        int32_t _M0L7_2abindS749;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS750;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS751;
        if (_M0L4sizeS2415 >= _M0L8grow__atS2416) {
          int32_t _M0L14capacity__maskS2418;
          int32_t _M0L6_2atmpS2417;
          moonbit_decref(_M0L14_2acurr__entryS748);
          moonbit_incref(_M0L4selfS739);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS739);
          _M0L14capacity__maskS2418 = _M0L4selfS739->$3;
          _M0L6_2atmpS2417 = _M0L4hashS741 & _M0L14capacity__maskS2418;
          _M0L3pslS736 = 0;
          _M0L3idxS737 = _M0L6_2atmpS2417;
          continue;
        }
        moonbit_incref(_M0L4selfS739);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS739, _M0L3idxS737, _M0L14_2acurr__entryS748);
        _M0L7_2abindS749 = _M0L4selfS739->$6;
        _M0L7_2abindS750 = 0;
        _M0L5entryS751
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS751)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS751->$0 = _M0L7_2abindS749;
        _M0L5entryS751->$1 = _M0L7_2abindS750;
        _M0L5entryS751->$2 = _M0L3pslS736;
        _M0L5entryS751->$3 = _M0L4hashS741;
        _M0L5entryS751->$4 = _M0L3keyS745;
        _M0L5entryS751->$5 = _M0L5valueS746;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS739, _M0L3idxS737, _M0L5entryS751);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS748);
      }
      _M0L6_2atmpS2419 = _M0L3pslS736 + 1;
      _M0L6_2atmpS2421 = _M0L3idxS737 + 1;
      _M0L14capacity__maskS2422 = _M0L4selfS739->$3;
      _M0L6_2atmpS2420 = _M0L6_2atmpS2421 & _M0L14capacity__maskS2422;
      _M0L3pslS736 = _M0L6_2atmpS2419;
      _M0L3idxS737 = _M0L6_2atmpS2420;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS704,
  int32_t _M0L3idxS709,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS708
) {
  int32_t _M0L3pslS2373;
  int32_t _M0L6_2atmpS2369;
  int32_t _M0L6_2atmpS2371;
  int32_t _M0L14capacity__maskS2372;
  int32_t _M0L6_2atmpS2370;
  int32_t _M0L3pslS700;
  int32_t _M0L3idxS701;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS702;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2373 = _M0L5entryS708->$2;
  _M0L6_2atmpS2369 = _M0L3pslS2373 + 1;
  _M0L6_2atmpS2371 = _M0L3idxS709 + 1;
  _M0L14capacity__maskS2372 = _M0L4selfS704->$3;
  _M0L6_2atmpS2370 = _M0L6_2atmpS2371 & _M0L14capacity__maskS2372;
  _M0L3pslS700 = _M0L6_2atmpS2369;
  _M0L3idxS701 = _M0L6_2atmpS2370;
  _M0L5entryS702 = _M0L5entryS708;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3304 =
      _M0L4selfS704->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2368 =
      _M0L8_2afieldS3304;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3303;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS703;
    if (
      _M0L3idxS701 < 0
      || _M0L3idxS701 >= Moonbit_array_length(_M0L7entriesS2368)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3303
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2368[
        _M0L3idxS701
      ];
    _M0L7_2abindS703 = _M0L6_2atmpS3303;
    if (_M0L7_2abindS703 == 0) {
      _M0L5entryS702->$2 = _M0L3pslS700;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS704, _M0L5entryS702, _M0L3idxS701);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS706 =
        _M0L7_2abindS703;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS707 =
        _M0L7_2aSomeS706;
      int32_t _M0L3pslS2358 = _M0L14_2acurr__entryS707->$2;
      if (_M0L3pslS700 > _M0L3pslS2358) {
        int32_t _M0L3pslS2363;
        int32_t _M0L6_2atmpS2359;
        int32_t _M0L6_2atmpS2361;
        int32_t _M0L14capacity__maskS2362;
        int32_t _M0L6_2atmpS2360;
        _M0L5entryS702->$2 = _M0L3pslS700;
        moonbit_incref(_M0L14_2acurr__entryS707);
        moonbit_incref(_M0L4selfS704);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS704, _M0L5entryS702, _M0L3idxS701);
        _M0L3pslS2363 = _M0L14_2acurr__entryS707->$2;
        _M0L6_2atmpS2359 = _M0L3pslS2363 + 1;
        _M0L6_2atmpS2361 = _M0L3idxS701 + 1;
        _M0L14capacity__maskS2362 = _M0L4selfS704->$3;
        _M0L6_2atmpS2360 = _M0L6_2atmpS2361 & _M0L14capacity__maskS2362;
        _M0L3pslS700 = _M0L6_2atmpS2359;
        _M0L3idxS701 = _M0L6_2atmpS2360;
        _M0L5entryS702 = _M0L14_2acurr__entryS707;
        continue;
      } else {
        int32_t _M0L6_2atmpS2364 = _M0L3pslS700 + 1;
        int32_t _M0L6_2atmpS2366 = _M0L3idxS701 + 1;
        int32_t _M0L14capacity__maskS2367 = _M0L4selfS704->$3;
        int32_t _M0L6_2atmpS2365 =
          _M0L6_2atmpS2366 & _M0L14capacity__maskS2367;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3634 =
          _M0L5entryS702;
        _M0L3pslS700 = _M0L6_2atmpS2364;
        _M0L3idxS701 = _M0L6_2atmpS2365;
        _M0L5entryS702 = _tmp_3634;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS714,
  int32_t _M0L3idxS719,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS718
) {
  int32_t _M0L3pslS2389;
  int32_t _M0L6_2atmpS2385;
  int32_t _M0L6_2atmpS2387;
  int32_t _M0L14capacity__maskS2388;
  int32_t _M0L6_2atmpS2386;
  int32_t _M0L3pslS710;
  int32_t _M0L3idxS711;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS712;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2389 = _M0L5entryS718->$2;
  _M0L6_2atmpS2385 = _M0L3pslS2389 + 1;
  _M0L6_2atmpS2387 = _M0L3idxS719 + 1;
  _M0L14capacity__maskS2388 = _M0L4selfS714->$3;
  _M0L6_2atmpS2386 = _M0L6_2atmpS2387 & _M0L14capacity__maskS2388;
  _M0L3pslS710 = _M0L6_2atmpS2385;
  _M0L3idxS711 = _M0L6_2atmpS2386;
  _M0L5entryS712 = _M0L5entryS718;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3306 =
      _M0L4selfS714->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2384 =
      _M0L8_2afieldS3306;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3305;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS713;
    if (
      _M0L3idxS711 < 0
      || _M0L3idxS711 >= Moonbit_array_length(_M0L7entriesS2384)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3305
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2384[
        _M0L3idxS711
      ];
    _M0L7_2abindS713 = _M0L6_2atmpS3305;
    if (_M0L7_2abindS713 == 0) {
      _M0L5entryS712->$2 = _M0L3pslS710;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS714, _M0L5entryS712, _M0L3idxS711);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS716 =
        _M0L7_2abindS713;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS717 =
        _M0L7_2aSomeS716;
      int32_t _M0L3pslS2374 = _M0L14_2acurr__entryS717->$2;
      if (_M0L3pslS710 > _M0L3pslS2374) {
        int32_t _M0L3pslS2379;
        int32_t _M0L6_2atmpS2375;
        int32_t _M0L6_2atmpS2377;
        int32_t _M0L14capacity__maskS2378;
        int32_t _M0L6_2atmpS2376;
        _M0L5entryS712->$2 = _M0L3pslS710;
        moonbit_incref(_M0L14_2acurr__entryS717);
        moonbit_incref(_M0L4selfS714);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS714, _M0L5entryS712, _M0L3idxS711);
        _M0L3pslS2379 = _M0L14_2acurr__entryS717->$2;
        _M0L6_2atmpS2375 = _M0L3pslS2379 + 1;
        _M0L6_2atmpS2377 = _M0L3idxS711 + 1;
        _M0L14capacity__maskS2378 = _M0L4selfS714->$3;
        _M0L6_2atmpS2376 = _M0L6_2atmpS2377 & _M0L14capacity__maskS2378;
        _M0L3pslS710 = _M0L6_2atmpS2375;
        _M0L3idxS711 = _M0L6_2atmpS2376;
        _M0L5entryS712 = _M0L14_2acurr__entryS717;
        continue;
      } else {
        int32_t _M0L6_2atmpS2380 = _M0L3pslS710 + 1;
        int32_t _M0L6_2atmpS2382 = _M0L3idxS711 + 1;
        int32_t _M0L14capacity__maskS2383 = _M0L4selfS714->$3;
        int32_t _M0L6_2atmpS2381 =
          _M0L6_2atmpS2382 & _M0L14capacity__maskS2383;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3636 =
          _M0L5entryS712;
        _M0L3pslS710 = _M0L6_2atmpS2380;
        _M0L3idxS711 = _M0L6_2atmpS2381;
        _M0L5entryS712 = _tmp_3636;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS688,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS690,
  int32_t _M0L8new__idxS689
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3309;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2354;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2355;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3308;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3307;
  int32_t _M0L6_2acntS3514;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS691;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3309 = _M0L4selfS688->$0;
  _M0L7entriesS2354 = _M0L8_2afieldS3309;
  moonbit_incref(_M0L5entryS690);
  _M0L6_2atmpS2355 = _M0L5entryS690;
  if (
    _M0L8new__idxS689 < 0
    || _M0L8new__idxS689 >= Moonbit_array_length(_M0L7entriesS2354)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3308
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2354[
      _M0L8new__idxS689
    ];
  if (_M0L6_2aoldS3308) {
    moonbit_decref(_M0L6_2aoldS3308);
  }
  _M0L7entriesS2354[_M0L8new__idxS689] = _M0L6_2atmpS2355;
  _M0L8_2afieldS3307 = _M0L5entryS690->$1;
  _M0L6_2acntS3514 = Moonbit_object_header(_M0L5entryS690)->rc;
  if (_M0L6_2acntS3514 > 1) {
    int32_t _M0L11_2anew__cntS3517 = _M0L6_2acntS3514 - 1;
    Moonbit_object_header(_M0L5entryS690)->rc = _M0L11_2anew__cntS3517;
    if (_M0L8_2afieldS3307) {
      moonbit_incref(_M0L8_2afieldS3307);
    }
  } else if (_M0L6_2acntS3514 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3516 =
      _M0L5entryS690->$5;
    moonbit_string_t _M0L8_2afieldS3515;
    moonbit_decref(_M0L8_2afieldS3516);
    _M0L8_2afieldS3515 = _M0L5entryS690->$4;
    moonbit_decref(_M0L8_2afieldS3515);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS690);
  }
  _M0L7_2abindS691 = _M0L8_2afieldS3307;
  if (_M0L7_2abindS691 == 0) {
    if (_M0L7_2abindS691) {
      moonbit_decref(_M0L7_2abindS691);
    }
    _M0L4selfS688->$6 = _M0L8new__idxS689;
    moonbit_decref(_M0L4selfS688);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS692;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS693;
    moonbit_decref(_M0L4selfS688);
    _M0L7_2aSomeS692 = _M0L7_2abindS691;
    _M0L7_2anextS693 = _M0L7_2aSomeS692;
    _M0L7_2anextS693->$0 = _M0L8new__idxS689;
    moonbit_decref(_M0L7_2anextS693);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS694,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS696,
  int32_t _M0L8new__idxS695
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3312;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2356;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2357;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3311;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3310;
  int32_t _M0L6_2acntS3518;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS697;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3312 = _M0L4selfS694->$0;
  _M0L7entriesS2356 = _M0L8_2afieldS3312;
  moonbit_incref(_M0L5entryS696);
  _M0L6_2atmpS2357 = _M0L5entryS696;
  if (
    _M0L8new__idxS695 < 0
    || _M0L8new__idxS695 >= Moonbit_array_length(_M0L7entriesS2356)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3311
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2356[
      _M0L8new__idxS695
    ];
  if (_M0L6_2aoldS3311) {
    moonbit_decref(_M0L6_2aoldS3311);
  }
  _M0L7entriesS2356[_M0L8new__idxS695] = _M0L6_2atmpS2357;
  _M0L8_2afieldS3310 = _M0L5entryS696->$1;
  _M0L6_2acntS3518 = Moonbit_object_header(_M0L5entryS696)->rc;
  if (_M0L6_2acntS3518 > 1) {
    int32_t _M0L11_2anew__cntS3520 = _M0L6_2acntS3518 - 1;
    Moonbit_object_header(_M0L5entryS696)->rc = _M0L11_2anew__cntS3520;
    if (_M0L8_2afieldS3310) {
      moonbit_incref(_M0L8_2afieldS3310);
    }
  } else if (_M0L6_2acntS3518 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3519 =
      _M0L5entryS696->$5;
    moonbit_decref(_M0L8_2afieldS3519);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS696);
  }
  _M0L7_2abindS697 = _M0L8_2afieldS3310;
  if (_M0L7_2abindS697 == 0) {
    if (_M0L7_2abindS697) {
      moonbit_decref(_M0L7_2abindS697);
    }
    _M0L4selfS694->$6 = _M0L8new__idxS695;
    moonbit_decref(_M0L4selfS694);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS698;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS699;
    moonbit_decref(_M0L4selfS694);
    _M0L7_2aSomeS698 = _M0L7_2abindS697;
    _M0L7_2anextS699 = _M0L7_2aSomeS698;
    _M0L7_2anextS699->$0 = _M0L8new__idxS695;
    moonbit_decref(_M0L7_2anextS699);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS681,
  int32_t _M0L3idxS683,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS682
) {
  int32_t _M0L7_2abindS680;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3314;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2341;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2342;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3313;
  int32_t _M0L4sizeS2344;
  int32_t _M0L6_2atmpS2343;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS680 = _M0L4selfS681->$6;
  switch (_M0L7_2abindS680) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2336;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3315;
      moonbit_incref(_M0L5entryS682);
      _M0L6_2atmpS2336 = _M0L5entryS682;
      _M0L6_2aoldS3315 = _M0L4selfS681->$5;
      if (_M0L6_2aoldS3315) {
        moonbit_decref(_M0L6_2aoldS3315);
      }
      _M0L4selfS681->$5 = _M0L6_2atmpS2336;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3318 =
        _M0L4selfS681->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2340 =
        _M0L8_2afieldS3318;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3317;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2339;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2337;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2338;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3316;
      if (
        _M0L7_2abindS680 < 0
        || _M0L7_2abindS680 >= Moonbit_array_length(_M0L7entriesS2340)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3317
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2340[
          _M0L7_2abindS680
        ];
      _M0L6_2atmpS2339 = _M0L6_2atmpS3317;
      if (_M0L6_2atmpS2339) {
        moonbit_incref(_M0L6_2atmpS2339);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2337
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2339);
      moonbit_incref(_M0L5entryS682);
      _M0L6_2atmpS2338 = _M0L5entryS682;
      _M0L6_2aoldS3316 = _M0L6_2atmpS2337->$1;
      if (_M0L6_2aoldS3316) {
        moonbit_decref(_M0L6_2aoldS3316);
      }
      _M0L6_2atmpS2337->$1 = _M0L6_2atmpS2338;
      moonbit_decref(_M0L6_2atmpS2337);
      break;
    }
  }
  _M0L4selfS681->$6 = _M0L3idxS683;
  _M0L8_2afieldS3314 = _M0L4selfS681->$0;
  _M0L7entriesS2341 = _M0L8_2afieldS3314;
  _M0L6_2atmpS2342 = _M0L5entryS682;
  if (
    _M0L3idxS683 < 0
    || _M0L3idxS683 >= Moonbit_array_length(_M0L7entriesS2341)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3313
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2341[
      _M0L3idxS683
    ];
  if (_M0L6_2aoldS3313) {
    moonbit_decref(_M0L6_2aoldS3313);
  }
  _M0L7entriesS2341[_M0L3idxS683] = _M0L6_2atmpS2342;
  _M0L4sizeS2344 = _M0L4selfS681->$1;
  _M0L6_2atmpS2343 = _M0L4sizeS2344 + 1;
  _M0L4selfS681->$1 = _M0L6_2atmpS2343;
  moonbit_decref(_M0L4selfS681);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS685,
  int32_t _M0L3idxS687,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS686
) {
  int32_t _M0L7_2abindS684;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3320;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2350;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2351;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3319;
  int32_t _M0L4sizeS2353;
  int32_t _M0L6_2atmpS2352;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS684 = _M0L4selfS685->$6;
  switch (_M0L7_2abindS684) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2345;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3321;
      moonbit_incref(_M0L5entryS686);
      _M0L6_2atmpS2345 = _M0L5entryS686;
      _M0L6_2aoldS3321 = _M0L4selfS685->$5;
      if (_M0L6_2aoldS3321) {
        moonbit_decref(_M0L6_2aoldS3321);
      }
      _M0L4selfS685->$5 = _M0L6_2atmpS2345;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3324 =
        _M0L4selfS685->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2349 =
        _M0L8_2afieldS3324;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3323;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2348;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2346;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2347;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3322;
      if (
        _M0L7_2abindS684 < 0
        || _M0L7_2abindS684 >= Moonbit_array_length(_M0L7entriesS2349)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3323
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2349[
          _M0L7_2abindS684
        ];
      _M0L6_2atmpS2348 = _M0L6_2atmpS3323;
      if (_M0L6_2atmpS2348) {
        moonbit_incref(_M0L6_2atmpS2348);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2346
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2348);
      moonbit_incref(_M0L5entryS686);
      _M0L6_2atmpS2347 = _M0L5entryS686;
      _M0L6_2aoldS3322 = _M0L6_2atmpS2346->$1;
      if (_M0L6_2aoldS3322) {
        moonbit_decref(_M0L6_2aoldS3322);
      }
      _M0L6_2atmpS2346->$1 = _M0L6_2atmpS2347;
      moonbit_decref(_M0L6_2atmpS2346);
      break;
    }
  }
  _M0L4selfS685->$6 = _M0L3idxS687;
  _M0L8_2afieldS3320 = _M0L4selfS685->$0;
  _M0L7entriesS2350 = _M0L8_2afieldS3320;
  _M0L6_2atmpS2351 = _M0L5entryS686;
  if (
    _M0L3idxS687 < 0
    || _M0L3idxS687 >= Moonbit_array_length(_M0L7entriesS2350)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3319
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2350[
      _M0L3idxS687
    ];
  if (_M0L6_2aoldS3319) {
    moonbit_decref(_M0L6_2aoldS3319);
  }
  _M0L7entriesS2350[_M0L3idxS687] = _M0L6_2atmpS2351;
  _M0L4sizeS2353 = _M0L4selfS685->$1;
  _M0L6_2atmpS2352 = _M0L4sizeS2353 + 1;
  _M0L4selfS685->$1 = _M0L6_2atmpS2352;
  moonbit_decref(_M0L4selfS685);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS669
) {
  int32_t _M0L8capacityS668;
  int32_t _M0L7_2abindS670;
  int32_t _M0L7_2abindS671;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2334;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS672;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS673;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3637;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS668
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS669);
  _M0L7_2abindS670 = _M0L8capacityS668 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS671 = _M0FPB21calc__grow__threshold(_M0L8capacityS668);
  _M0L6_2atmpS2334 = 0;
  _M0L7_2abindS672
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS668, _M0L6_2atmpS2334);
  _M0L7_2abindS673 = 0;
  _block_3637
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3637)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3637->$0 = _M0L7_2abindS672;
  _block_3637->$1 = 0;
  _block_3637->$2 = _M0L8capacityS668;
  _block_3637->$3 = _M0L7_2abindS670;
  _block_3637->$4 = _M0L7_2abindS671;
  _block_3637->$5 = _M0L7_2abindS673;
  _block_3637->$6 = -1;
  return _block_3637;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS675
) {
  int32_t _M0L8capacityS674;
  int32_t _M0L7_2abindS676;
  int32_t _M0L7_2abindS677;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2335;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS678;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS679;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3638;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS674
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS675);
  _M0L7_2abindS676 = _M0L8capacityS674 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS677 = _M0FPB21calc__grow__threshold(_M0L8capacityS674);
  _M0L6_2atmpS2335 = 0;
  _M0L7_2abindS678
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS674, _M0L6_2atmpS2335);
  _M0L7_2abindS679 = 0;
  _block_3638
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3638)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3638->$0 = _M0L7_2abindS678;
  _block_3638->$1 = 0;
  _block_3638->$2 = _M0L8capacityS674;
  _block_3638->$3 = _M0L7_2abindS676;
  _block_3638->$4 = _M0L7_2abindS677;
  _block_3638->$5 = _M0L7_2abindS679;
  _block_3638->$6 = -1;
  return _block_3638;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS667) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS667 >= 0) {
    int32_t _M0L6_2atmpS2333;
    int32_t _M0L6_2atmpS2332;
    int32_t _M0L6_2atmpS2331;
    int32_t _M0L6_2atmpS2330;
    if (_M0L4selfS667 <= 1) {
      return 1;
    }
    if (_M0L4selfS667 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2333 = _M0L4selfS667 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2332 = moonbit_clz32(_M0L6_2atmpS2333);
    _M0L6_2atmpS2331 = _M0L6_2atmpS2332 - 1;
    _M0L6_2atmpS2330 = 2147483647 >> (_M0L6_2atmpS2331 & 31);
    return _M0L6_2atmpS2330 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS666) {
  int32_t _M0L6_2atmpS2329;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2329 = _M0L8capacityS666 * 13;
  return _M0L6_2atmpS2329 / 16;
}

moonbit_string_t _M0MPC16option6Option4bindGRPC16string10StringViewsE(
  void* _M0L4selfS662,
  struct _M0TWRPC16string10StringViewEOs* _M0L1fS665
) {
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  switch (Moonbit_object_tag(_M0L4selfS662)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS663 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4selfS662;
      struct _M0TPC16string10StringView _M0L8_2afieldS3325 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS663->$0_1,
                                              _M0L7_2aSomeS663->$0_2,
                                              _M0L7_2aSomeS663->$0_0};
      int32_t _M0L6_2acntS3521 = Moonbit_object_header(_M0L7_2aSomeS663)->rc;
      struct _M0TPC16string10StringView _M0L4_2atS664;
      if (_M0L6_2acntS3521 > 1) {
        int32_t _M0L11_2anew__cntS3522 = _M0L6_2acntS3521 - 1;
        Moonbit_object_header(_M0L7_2aSomeS663)->rc = _M0L11_2anew__cntS3522;
        moonbit_incref(_M0L8_2afieldS3325.$0);
      } else if (_M0L6_2acntS3521 == 1) {
        #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
        moonbit_free(_M0L7_2aSomeS663);
      }
      _M0L4_2atS664 = _M0L8_2afieldS3325;
      #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
      return _M0L1fS665->code(_M0L1fS665, _M0L4_2atS664);
      break;
    }
    default: {
      moonbit_decref(_M0L1fS665);
      moonbit_decref(_M0L4selfS662);
      return 0;
      break;
    }
  }
}

moonbit_string_t _M0MPC16option6Option3mapGRPC16string10StringViewsE(
  void* _M0L4selfS658,
  struct _M0TWRPC16string10StringViewEs* _M0L1fS661
) {
  #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  switch (Moonbit_object_tag(_M0L4selfS658)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS659 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4selfS658;
      struct _M0TPC16string10StringView _M0L8_2afieldS3326 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS659->$0_1,
                                              _M0L7_2aSomeS659->$0_2,
                                              _M0L7_2aSomeS659->$0_0};
      int32_t _M0L6_2acntS3523 = Moonbit_object_header(_M0L7_2aSomeS659)->rc;
      struct _M0TPC16string10StringView _M0L4_2atS660;
      moonbit_string_t _M0L6_2atmpS2328;
      if (_M0L6_2acntS3523 > 1) {
        int32_t _M0L11_2anew__cntS3524 = _M0L6_2acntS3523 - 1;
        Moonbit_object_header(_M0L7_2aSomeS659)->rc = _M0L11_2anew__cntS3524;
        moonbit_incref(_M0L8_2afieldS3326.$0);
      } else if (_M0L6_2acntS3523 == 1) {
        #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
        moonbit_free(_M0L7_2aSomeS659);
      }
      _M0L4_2atS660 = _M0L8_2afieldS3326;
      #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
      _M0L6_2atmpS2328 = _M0L1fS661->code(_M0L1fS661, _M0L4_2atS660);
      return _M0L6_2atmpS2328;
      break;
    }
    default: {
      moonbit_decref(_M0L1fS661);
      moonbit_decref(_M0L4selfS658);
      return 0;
      break;
    }
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS654
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS654 == 0) {
    if (_M0L4selfS654) {
      moonbit_decref(_M0L4selfS654);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS655 =
      _M0L4selfS654;
    return _M0L7_2aSomeS655;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS656
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS656 == 0) {
    if (_M0L4selfS656) {
      moonbit_decref(_M0L4selfS656);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS657 =
      _M0L4selfS656;
    return _M0L7_2aSomeS657;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS653
) {
  moonbit_string_t* _M0L6_2atmpS2327;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2327 = _M0L4selfS653;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2327);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS649,
  int32_t _M0L5indexS650
) {
  uint64_t* _M0L6_2atmpS2325;
  uint64_t _M0L6_2atmpS3327;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2325 = _M0L4selfS649;
  if (
    _M0L5indexS650 < 0
    || _M0L5indexS650 >= Moonbit_array_length(_M0L6_2atmpS2325)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3327 = (uint64_t)_M0L6_2atmpS2325[_M0L5indexS650];
  moonbit_decref(_M0L6_2atmpS2325);
  return _M0L6_2atmpS3327;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS651,
  int32_t _M0L5indexS652
) {
  uint32_t* _M0L6_2atmpS2326;
  uint32_t _M0L6_2atmpS3328;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2326 = _M0L4selfS651;
  if (
    _M0L5indexS652 < 0
    || _M0L5indexS652 >= Moonbit_array_length(_M0L6_2atmpS2326)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3328 = (uint32_t)_M0L6_2atmpS2326[_M0L5indexS652];
  moonbit_decref(_M0L6_2atmpS2326);
  return _M0L6_2atmpS3328;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS648
) {
  moonbit_string_t* _M0L6_2atmpS2323;
  int32_t _M0L6_2atmpS3329;
  int32_t _M0L6_2atmpS2324;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2322;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS648);
  _M0L6_2atmpS2323 = _M0L4selfS648;
  _M0L6_2atmpS3329 = Moonbit_array_length(_M0L4selfS648);
  moonbit_decref(_M0L4selfS648);
  _M0L6_2atmpS2324 = _M0L6_2atmpS3329;
  _M0L6_2atmpS2322
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2324, _M0L6_2atmpS2323
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2322);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS646
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS645;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__* _closure_3639;
  struct _M0TWEOs* _M0L6_2atmpS2310;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS645
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS645)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS645->$0 = 0;
  _closure_3639
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__));
  Moonbit_object_header(_closure_3639)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__, $0_0) >> 2, 2, 0);
  _closure_3639->code = &_M0MPC15array9ArrayView4iterGsEC2311l570;
  _closure_3639->$0_0 = _M0L4selfS646.$0;
  _closure_3639->$0_1 = _M0L4selfS646.$1;
  _closure_3639->$0_2 = _M0L4selfS646.$2;
  _closure_3639->$1 = _M0L1iS645;
  _M0L6_2atmpS2310 = (struct _M0TWEOs*)_closure_3639;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2310);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2311l570(
  struct _M0TWEOs* _M0L6_2aenvS2312
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__* _M0L14_2acasted__envS2313;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3334;
  struct _M0TPC13ref3RefGiE* _M0L1iS645;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3333;
  int32_t _M0L6_2acntS3525;
  struct _M0TPB9ArrayViewGsE _M0L4selfS646;
  int32_t _M0L3valS2314;
  int32_t _M0L6_2atmpS2315;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2313
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2311__l570__*)_M0L6_2aenvS2312;
  _M0L8_2afieldS3334 = _M0L14_2acasted__envS2313->$1;
  _M0L1iS645 = _M0L8_2afieldS3334;
  _M0L8_2afieldS3333
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2313->$0_1,
      _M0L14_2acasted__envS2313->$0_2,
      _M0L14_2acasted__envS2313->$0_0
  };
  _M0L6_2acntS3525 = Moonbit_object_header(_M0L14_2acasted__envS2313)->rc;
  if (_M0L6_2acntS3525 > 1) {
    int32_t _M0L11_2anew__cntS3526 = _M0L6_2acntS3525 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2313)->rc
    = _M0L11_2anew__cntS3526;
    moonbit_incref(_M0L1iS645);
    moonbit_incref(_M0L8_2afieldS3333.$0);
  } else if (_M0L6_2acntS3525 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2313);
  }
  _M0L4selfS646 = _M0L8_2afieldS3333;
  _M0L3valS2314 = _M0L1iS645->$0;
  moonbit_incref(_M0L4selfS646.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2315 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS646);
  if (_M0L3valS2314 < _M0L6_2atmpS2315) {
    moonbit_string_t* _M0L8_2afieldS3332 = _M0L4selfS646.$0;
    moonbit_string_t* _M0L3bufS2318 = _M0L8_2afieldS3332;
    int32_t _M0L8_2afieldS3331 = _M0L4selfS646.$1;
    int32_t _M0L5startS2320 = _M0L8_2afieldS3331;
    int32_t _M0L3valS2321 = _M0L1iS645->$0;
    int32_t _M0L6_2atmpS2319 = _M0L5startS2320 + _M0L3valS2321;
    moonbit_string_t _M0L6_2atmpS3330 =
      (moonbit_string_t)_M0L3bufS2318[_M0L6_2atmpS2319];
    moonbit_string_t _M0L4elemS647;
    int32_t _M0L3valS2317;
    int32_t _M0L6_2atmpS2316;
    moonbit_incref(_M0L6_2atmpS3330);
    moonbit_decref(_M0L3bufS2318);
    _M0L4elemS647 = _M0L6_2atmpS3330;
    _M0L3valS2317 = _M0L1iS645->$0;
    _M0L6_2atmpS2316 = _M0L3valS2317 + 1;
    _M0L1iS645->$0 = _M0L6_2atmpS2316;
    moonbit_decref(_M0L1iS645);
    return _M0L4elemS647;
  } else {
    moonbit_decref(_M0L4selfS646.$0);
    moonbit_decref(_M0L1iS645);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS644
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS644;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS643,
  struct _M0TPB6Logger _M0L6loggerS642
) {
  moonbit_string_t _M0L6_2atmpS2309;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2309
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS643, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS642.$0->$method_0(_M0L6loggerS642.$1, _M0L6_2atmpS2309);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS641,
  struct _M0TPB6Logger _M0L6loggerS640
) {
  moonbit_string_t _M0L6_2atmpS2308;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2308 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS641, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS640.$0->$method_0(_M0L6loggerS640.$1, _M0L6_2atmpS2308);
  return 0;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t _M0L4selfS638,
  struct _M0TPC16string10StringView _M0L3sepS639
) {
  int32_t _M0L6_2atmpS2307;
  struct _M0TPC16string10StringView _M0L6_2atmpS2306;
  #line 1093 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2307 = Moonbit_array_length(_M0L4selfS638);
  _M0L6_2atmpS2306
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2307, _M0L4selfS638
  };
  #line 1094 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView5split(_M0L6_2atmpS2306, _M0L3sepS639);
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView _M0L4selfS629,
  struct _M0TPC16string10StringView _M0L3sepS628
) {
  int32_t _M0L8sep__lenS627;
  void* _M0L4SomeS2305;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* _M0L9remainingS631;
  struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__* _closure_3640;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2295;
  #line 1064 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L3sepS628.$0);
  #line 1068 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8sep__lenS627 = _M0MPC16string10StringView6length(_M0L3sepS628);
  if (_M0L8sep__lenS627 == 0) {
    struct _M0TWEOc* _M0L6_2atmpS2290;
    struct _M0TWcERPC16string10StringView* _M0L6_2atmpS2291;
    moonbit_decref(_M0L3sepS628.$0);
    #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L6_2atmpS2290 = _M0MPC16string10StringView4iter(_M0L4selfS629);
    _M0L6_2atmpS2291
    = (struct _M0TWcERPC16string10StringView*)&_M0MPC16string10StringView5splitC2292l1070$closure.data;
    #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB4Iter3mapGcRPC16string10StringViewE(_M0L6_2atmpS2290, _M0L6_2atmpS2291);
  }
  _M0L4SomeS2305
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS2305)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2305)->$0_0
  = _M0L4selfS629.$0;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2305)->$0_1
  = _M0L4selfS629.$1;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2305)->$0_2
  = _M0L4selfS629.$2;
  _M0L9remainingS631
  = (struct _M0TPC13ref3RefGORPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPC16string10StringViewE));
  Moonbit_object_header(_M0L9remainingS631)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L9remainingS631->$0 = _M0L4SomeS2305;
  _closure_3640
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__*)moonbit_malloc(sizeof(struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__));
  Moonbit_object_header(_closure_3640)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__, $0) >> 2, 2, 0);
  _closure_3640->code = &_M0MPC16string10StringView5splitC2296l1073;
  _closure_3640->$0 = _M0L9remainingS631;
  _closure_3640->$1_0 = _M0L3sepS628.$0;
  _closure_3640->$1_1 = _M0L3sepS628.$1;
  _closure_3640->$1_2 = _M0L3sepS628.$2;
  _closure_3640->$2 = _M0L8sep__lenS627;
  _M0L6_2atmpS2295
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_3640;
  #line 1073 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPB4Iter3newGRPC16string10StringViewE(_M0L6_2atmpS2295);
}

void* _M0MPC16string10StringView5splitC2296l1073(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2297
) {
  struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__* _M0L14_2acasted__envS2298;
  int32_t _M0L8sep__lenS627;
  struct _M0TPC16string10StringView _M0L8_2afieldS3340;
  struct _M0TPC16string10StringView _M0L3sepS628;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* _M0L8_2afieldS3339;
  int32_t _M0L6_2acntS3527;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* _M0L9remainingS631;
  void* _M0L8_2afieldS3338;
  void* _M0L7_2abindS632;
  #line 1073 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L14_2acasted__envS2298
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2296__l1073__*)_M0L6_2aenvS2297;
  _M0L8sep__lenS627 = _M0L14_2acasted__envS2298->$2;
  _M0L8_2afieldS3340
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS2298->$1_1,
      _M0L14_2acasted__envS2298->$1_2,
      _M0L14_2acasted__envS2298->$1_0
  };
  _M0L3sepS628 = _M0L8_2afieldS3340;
  _M0L8_2afieldS3339 = _M0L14_2acasted__envS2298->$0;
  _M0L6_2acntS3527 = Moonbit_object_header(_M0L14_2acasted__envS2298)->rc;
  if (_M0L6_2acntS3527 > 1) {
    int32_t _M0L11_2anew__cntS3528 = _M0L6_2acntS3527 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2298)->rc
    = _M0L11_2anew__cntS3528;
    moonbit_incref(_M0L3sepS628.$0);
    moonbit_incref(_M0L8_2afieldS3339);
  } else if (_M0L6_2acntS3527 == 1) {
    #line 1073 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    moonbit_free(_M0L14_2acasted__envS2298);
  }
  _M0L9remainingS631 = _M0L8_2afieldS3339;
  _M0L8_2afieldS3338 = _M0L9remainingS631->$0;
  _M0L7_2abindS632 = _M0L8_2afieldS3338;
  switch (Moonbit_object_tag(_M0L7_2abindS632)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS633 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS632;
      struct _M0TPC16string10StringView _M0L8_2afieldS3337 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS633->$0_1,
                                              _M0L7_2aSomeS633->$0_2,
                                              _M0L7_2aSomeS633->$0_0};
      struct _M0TPC16string10StringView _M0L7_2aviewS634 = _M0L8_2afieldS3337;
      int64_t _M0L7_2abindS635;
      moonbit_incref(_M0L7_2aviewS634.$0);
      moonbit_incref(_M0L7_2aviewS634.$0);
      #line 1075 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L7_2abindS635
      = _M0MPC16string10StringView4find(_M0L7_2aviewS634, _M0L3sepS628);
      if (_M0L7_2abindS635 == 4294967296ll) {
        void* _M0L4NoneS2299 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        void* _M0L6_2aoldS3335 = _M0L9remainingS631->$0;
        void* _block_3641;
        moonbit_decref(_M0L6_2aoldS3335);
        _M0L9remainingS631->$0 = _M0L4NoneS2299;
        moonbit_decref(_M0L9remainingS631);
        _block_3641
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_3641)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3641)->$0_0
        = _M0L7_2aviewS634.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3641)->$0_1
        = _M0L7_2aviewS634.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3641)->$0_2
        = _M0L7_2aviewS634.$2;
        return _block_3641;
      } else {
        int64_t _M0L7_2aSomeS636 = _M0L7_2abindS635;
        int32_t _M0L6_2aendS637 = (int32_t)_M0L7_2aSomeS636;
        int32_t _M0L6_2atmpS2302 = _M0L6_2aendS637 + _M0L8sep__lenS627;
        struct _M0TPC16string10StringView _M0L6_2atmpS2301;
        void* _M0L4SomeS2300;
        void* _M0L6_2aoldS3336;
        int64_t _M0L6_2atmpS2304;
        struct _M0TPC16string10StringView _M0L6_2atmpS2303;
        void* _block_3642;
        moonbit_incref(_M0L7_2aviewS634.$0);
        #line 1079 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0L6_2atmpS2301
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS634, _M0L6_2atmpS2302, 4294967296ll);
        _M0L4SomeS2300
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_M0L4SomeS2300)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2300)->$0_0
        = _M0L6_2atmpS2301.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2300)->$0_1
        = _M0L6_2atmpS2301.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2300)->$0_2
        = _M0L6_2atmpS2301.$2;
        _M0L6_2aoldS3336 = _M0L9remainingS631->$0;
        moonbit_decref(_M0L6_2aoldS3336);
        _M0L9remainingS631->$0 = _M0L4SomeS2300;
        moonbit_decref(_M0L9remainingS631);
        _M0L6_2atmpS2304 = (int64_t)_M0L6_2aendS637;
        #line 1080 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0L6_2atmpS2303
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS634, 0, _M0L6_2atmpS2304);
        _block_3642
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_3642)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3642)->$0_0
        = _M0L6_2atmpS2303.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3642)->$0_1
        = _M0L6_2atmpS2303.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3642)->$0_2
        = _M0L6_2atmpS2303.$2;
        return _block_3642;
      }
      break;
    }
    default: {
      moonbit_decref(_M0L9remainingS631);
      moonbit_decref(_M0L3sepS628.$0);
      return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      break;
    }
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2292l1070(
  struct _M0TWcERPC16string10StringView* _M0L6_2aenvS2293,
  int32_t _M0L1cS630
) {
  moonbit_string_t _M0L6_2atmpS2294;
  #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_decref(_M0L6_2aenvS2293);
  #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2294 = _M0IPC14char4CharPB4Show10to__string(_M0L1cS630);
  #line 1070 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string6String12view_2einner(_M0L6_2atmpS2294, 0, 4294967296ll);
}

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t _M0L4selfS626) {
  #line 435 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  return _M0FPB16char__to__string(_M0L4selfS626);
}

moonbit_string_t _M0FPB16char__to__string(int32_t _M0L4charS625) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS624;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2289;
  #line 441 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L7_2aselfS624 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L7_2aselfS624);
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS624, _M0L4charS625);
  _M0L6_2atmpS2289 = _M0L7_2aselfS624;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2289);
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TWEOc* _M0L4selfS620,
  struct _M0TWcERPC16string10StringView* _M0L1fS623
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__* _closure_3643;
  #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _closure_3643
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__*)moonbit_malloc(sizeof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__));
  Moonbit_object_header(_closure_3643)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__, $0) >> 2, 2, 0);
  _closure_3643->code = &_M0MPB4Iter3mapGcRPC16string10StringViewEC2285l317;
  _closure_3643->$0 = _M0L1fS623;
  _closure_3643->$1 = _M0L4selfS620;
  return (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_3643;
}

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2285l317(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2286
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__* _M0L14_2acasted__envS2287;
  struct _M0TWEOc* _M0L8_2afieldS3342;
  struct _M0TWEOc* _M0L4selfS620;
  struct _M0TWcERPC16string10StringView* _M0L8_2afieldS3341;
  int32_t _M0L6_2acntS3529;
  struct _M0TWcERPC16string10StringView* _M0L1fS623;
  int32_t _M0L7_2abindS619;
  #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2287
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2285__l317__*)_M0L6_2aenvS2286;
  _M0L8_2afieldS3342 = _M0L14_2acasted__envS2287->$1;
  _M0L4selfS620 = _M0L8_2afieldS3342;
  _M0L8_2afieldS3341 = _M0L14_2acasted__envS2287->$0;
  _M0L6_2acntS3529 = Moonbit_object_header(_M0L14_2acasted__envS2287)->rc;
  if (_M0L6_2acntS3529 > 1) {
    int32_t _M0L11_2anew__cntS3530 = _M0L6_2acntS3529 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2287)->rc
    = _M0L11_2anew__cntS3530;
    moonbit_incref(_M0L4selfS620);
    moonbit_incref(_M0L8_2afieldS3341);
  } else if (_M0L6_2acntS3529 == 1) {
    #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2287);
  }
  _M0L1fS623 = _M0L8_2afieldS3341;
  #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2abindS619 = _M0MPB4Iter4nextGcE(_M0L4selfS620);
  if (_M0L7_2abindS619 == -1) {
    moonbit_decref(_M0L1fS623);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    int32_t _M0L7_2aSomeS621 = _M0L7_2abindS619;
    int32_t _M0L4_2axS622 = _M0L7_2aSomeS621;
    struct _M0TPC16string10StringView _M0L6_2atmpS2288;
    void* _block_3644;
    #line 319 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L6_2atmpS2288 = _M0L1fS623->code(_M0L1fS623, _M0L4_2axS622);
    _block_3644
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_block_3644)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3644)->$0_0
    = _M0L6_2atmpS2288.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3644)->$0_1
    = _M0L6_2atmpS2288.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_3644)->$0_2
    = _M0L6_2atmpS2288.$2;
    return _block_3644;
  }
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS614) {
  int32_t _M0L3lenS613;
  struct _M0TPC13ref3RefGiE* _M0L5indexS615;
  struct _M0R38String_3a_3aiter_2eanon__u2269__l247__* _closure_3645;
  struct _M0TWEOc* _M0L6_2atmpS2268;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS613 = Moonbit_array_length(_M0L4selfS614);
  _M0L5indexS615
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS615)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS615->$0 = 0;
  _closure_3645
  = (struct _M0R38String_3a_3aiter_2eanon__u2269__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2269__l247__));
  Moonbit_object_header(_closure_3645)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2269__l247__, $0) >> 2, 2, 0);
  _closure_3645->code = &_M0MPC16string6String4iterC2269l247;
  _closure_3645->$0 = _M0L5indexS615;
  _closure_3645->$1 = _M0L4selfS614;
  _closure_3645->$2 = _M0L3lenS613;
  _M0L6_2atmpS2268 = (struct _M0TWEOc*)_closure_3645;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2268);
}

int32_t _M0MPC16string6String4iterC2269l247(
  struct _M0TWEOc* _M0L6_2aenvS2270
) {
  struct _M0R38String_3a_3aiter_2eanon__u2269__l247__* _M0L14_2acasted__envS2271;
  int32_t _M0L3lenS613;
  moonbit_string_t _M0L8_2afieldS3345;
  moonbit_string_t _M0L4selfS614;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3344;
  int32_t _M0L6_2acntS3531;
  struct _M0TPC13ref3RefGiE* _M0L5indexS615;
  int32_t _M0L3valS2272;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2271
  = (struct _M0R38String_3a_3aiter_2eanon__u2269__l247__*)_M0L6_2aenvS2270;
  _M0L3lenS613 = _M0L14_2acasted__envS2271->$2;
  _M0L8_2afieldS3345 = _M0L14_2acasted__envS2271->$1;
  _M0L4selfS614 = _M0L8_2afieldS3345;
  _M0L8_2afieldS3344 = _M0L14_2acasted__envS2271->$0;
  _M0L6_2acntS3531 = Moonbit_object_header(_M0L14_2acasted__envS2271)->rc;
  if (_M0L6_2acntS3531 > 1) {
    int32_t _M0L11_2anew__cntS3532 = _M0L6_2acntS3531 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2271)->rc
    = _M0L11_2anew__cntS3532;
    moonbit_incref(_M0L4selfS614);
    moonbit_incref(_M0L8_2afieldS3344);
  } else if (_M0L6_2acntS3531 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2271);
  }
  _M0L5indexS615 = _M0L8_2afieldS3344;
  _M0L3valS2272 = _M0L5indexS615->$0;
  if (_M0L3valS2272 < _M0L3lenS613) {
    int32_t _M0L3valS2284 = _M0L5indexS615->$0;
    int32_t _M0L2c1S616 = _M0L4selfS614[_M0L3valS2284];
    int32_t _if__result_3646;
    int32_t _M0L3valS2282;
    int32_t _M0L6_2atmpS2281;
    int32_t _M0L6_2atmpS2283;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S616)) {
      int32_t _M0L3valS2274 = _M0L5indexS615->$0;
      int32_t _M0L6_2atmpS2273 = _M0L3valS2274 + 1;
      _if__result_3646 = _M0L6_2atmpS2273 < _M0L3lenS613;
    } else {
      _if__result_3646 = 0;
    }
    if (_if__result_3646) {
      int32_t _M0L3valS2280 = _M0L5indexS615->$0;
      int32_t _M0L6_2atmpS2279 = _M0L3valS2280 + 1;
      int32_t _M0L6_2atmpS3343 = _M0L4selfS614[_M0L6_2atmpS2279];
      int32_t _M0L2c2S617;
      moonbit_decref(_M0L4selfS614);
      _M0L2c2S617 = _M0L6_2atmpS3343;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S617)) {
        int32_t _M0L6_2atmpS2277 = (int32_t)_M0L2c1S616;
        int32_t _M0L6_2atmpS2278 = (int32_t)_M0L2c2S617;
        int32_t _M0L1cS618;
        int32_t _M0L3valS2276;
        int32_t _M0L6_2atmpS2275;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS618
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2277, _M0L6_2atmpS2278);
        _M0L3valS2276 = _M0L5indexS615->$0;
        _M0L6_2atmpS2275 = _M0L3valS2276 + 2;
        _M0L5indexS615->$0 = _M0L6_2atmpS2275;
        moonbit_decref(_M0L5indexS615);
        return _M0L1cS618;
      }
    } else {
      moonbit_decref(_M0L4selfS614);
    }
    _M0L3valS2282 = _M0L5indexS615->$0;
    _M0L6_2atmpS2281 = _M0L3valS2282 + 1;
    _M0L5indexS615->$0 = _M0L6_2atmpS2281;
    moonbit_decref(_M0L5indexS615);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2283 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S616);
    return _M0L6_2atmpS2283;
  } else {
    moonbit_decref(_M0L5indexS615);
    moonbit_decref(_M0L4selfS614);
    return -1;
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12trim_2einner(
  struct _M0TPC16string10StringView _M0L4selfS611,
  struct _M0TPC16string10StringView _M0L5charsS612
) {
  struct _M0TPC16string10StringView _M0L6_2atmpS2267;
  #line 731 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L5charsS612.$0);
  #line 736 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2267
  = _M0MPC16string10StringView19trim__start_2einner(_M0L4selfS611, _M0L5charsS612);
  #line 736 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView17trim__end_2einner(_M0L6_2atmpS2267, _M0L5charsS612);
}

struct _M0TPC16string10StringView _M0MPC16string10StringView17trim__end_2einner(
  struct _M0TPC16string10StringView _M0L4selfS610,
  struct _M0TPC16string10StringView _M0L5charsS608
) {
  struct _M0TPC16string10StringView _M0L8_2aparamS605;
  #line 689 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8_2aparamS605 = _M0L4selfS610;
  while (1) {
    moonbit_string_t _M0L8_2afieldS3350 = _M0L8_2aparamS605.$0;
    moonbit_string_t _M0L3strS2248 = _M0L8_2afieldS3350;
    int32_t _M0L5startS2249 = _M0L8_2aparamS605.$1;
    int32_t _M0L3endS2251 = _M0L8_2aparamS605.$2;
    int64_t _M0L6_2atmpS2250 = (int64_t)_M0L3endS2251;
    moonbit_incref(_M0L3strS2248);
    #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2248, 0, _M0L5startS2249, _M0L6_2atmpS2250)
    ) {
      moonbit_decref(_M0L5charsS608.$0);
      return _M0L8_2aparamS605;
    } else {
      moonbit_string_t _M0L8_2afieldS3349 = _M0L8_2aparamS605.$0;
      moonbit_string_t _M0L3strS2260 = _M0L8_2afieldS3349;
      moonbit_string_t _M0L8_2afieldS3348 = _M0L8_2aparamS605.$0;
      moonbit_string_t _M0L3strS2263 = _M0L8_2afieldS3348;
      int32_t _M0L5startS2264 = _M0L8_2aparamS605.$1;
      int32_t _M0L3endS2266 = _M0L8_2aparamS605.$2;
      int64_t _M0L6_2atmpS2265 = (int64_t)_M0L3endS2266;
      int64_t _M0L6_2atmpS2262;
      int32_t _M0L6_2atmpS2261;
      int32_t _M0L4_2acS606;
      moonbit_string_t _M0L8_2afieldS3347;
      moonbit_string_t _M0L3strS2252;
      int32_t _M0L5startS2253;
      moonbit_string_t _M0L8_2afieldS3346;
      moonbit_string_t _M0L3strS2256;
      int32_t _M0L5startS2257;
      int32_t _M0L3endS2259;
      int64_t _M0L6_2atmpS2258;
      int64_t _M0L6_2atmpS2255;
      int32_t _M0L6_2atmpS2254;
      struct _M0TPC16string10StringView _M0L4_2axS607;
      moonbit_incref(_M0L3strS2263);
      moonbit_incref(_M0L3strS2260);
      #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2262
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2263, -1, _M0L5startS2264, _M0L6_2atmpS2265);
      _M0L6_2atmpS2261 = (int32_t)_M0L6_2atmpS2262;
      #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L4_2acS606
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2260, _M0L6_2atmpS2261);
      _M0L8_2afieldS3347 = _M0L8_2aparamS605.$0;
      _M0L3strS2252 = _M0L8_2afieldS3347;
      _M0L5startS2253 = _M0L8_2aparamS605.$1;
      _M0L8_2afieldS3346 = _M0L8_2aparamS605.$0;
      _M0L3strS2256 = _M0L8_2afieldS3346;
      _M0L5startS2257 = _M0L8_2aparamS605.$1;
      _M0L3endS2259 = _M0L8_2aparamS605.$2;
      _M0L6_2atmpS2258 = (int64_t)_M0L3endS2259;
      moonbit_incref(_M0L3strS2256);
      moonbit_incref(_M0L3strS2252);
      #line 694 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2255
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2256, -1, _M0L5startS2257, _M0L6_2atmpS2258);
      _M0L6_2atmpS2254 = (int32_t)_M0L6_2atmpS2255;
      _M0L4_2axS607
      = (struct _M0TPC16string10StringView){
        _M0L5startS2253, _M0L6_2atmpS2254, _M0L3strS2252
      };
      moonbit_incref(_M0L5charsS608.$0);
      #line 696 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      if (
        _M0MPC16string10StringView14contains__char(_M0L5charsS608, _M0L4_2acS606)
      ) {
        moonbit_decref(_M0L8_2aparamS605.$0);
        _M0L8_2aparamS605 = _M0L4_2axS607;
        continue;
      } else {
        moonbit_decref(_M0L5charsS608.$0);
        moonbit_decref(_M0L4_2axS607.$0);
        return _M0L8_2aparamS605;
      }
    }
    break;
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView19trim__start_2einner(
  struct _M0TPC16string10StringView _M0L4selfS604,
  struct _M0TPC16string10StringView _M0L5charsS602
) {
  struct _M0TPC16string10StringView _M0L8_2aparamS598;
  #line 648 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8_2aparamS598 = _M0L4selfS604;
  while (1) {
    moonbit_string_t _M0L8_2afieldS3355 = _M0L8_2aparamS598.$0;
    moonbit_string_t _M0L3strS2230 = _M0L8_2afieldS3355;
    int32_t _M0L5startS2231 = _M0L8_2aparamS598.$1;
    int32_t _M0L3endS2233 = _M0L8_2aparamS598.$2;
    int64_t _M0L6_2atmpS2232 = (int64_t)_M0L3endS2233;
    moonbit_incref(_M0L3strS2230);
    #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2230, 0, _M0L5startS2231, _M0L6_2atmpS2232)
    ) {
      moonbit_decref(_M0L5charsS602.$0);
      return _M0L8_2aparamS598;
    } else {
      moonbit_string_t _M0L8_2afieldS3354 = _M0L8_2aparamS598.$0;
      moonbit_string_t _M0L3strS2241 = _M0L8_2afieldS3354;
      moonbit_string_t _M0L8_2afieldS3353 = _M0L8_2aparamS598.$0;
      moonbit_string_t _M0L3strS2244 = _M0L8_2afieldS3353;
      int32_t _M0L5startS2245 = _M0L8_2aparamS598.$1;
      int32_t _M0L3endS2247 = _M0L8_2aparamS598.$2;
      int64_t _M0L6_2atmpS2246 = (int64_t)_M0L3endS2247;
      int64_t _M0L6_2atmpS2243;
      int32_t _M0L6_2atmpS2242;
      int32_t _M0L4_2acS599;
      moonbit_string_t _M0L8_2afieldS3352;
      moonbit_string_t _M0L3strS2234;
      moonbit_string_t _M0L8_2afieldS3351;
      moonbit_string_t _M0L3strS2237;
      int32_t _M0L5startS2238;
      int32_t _M0L3endS2240;
      int64_t _M0L6_2atmpS2239;
      int64_t _M0L7_2abindS1354;
      int32_t _M0L6_2atmpS2235;
      int32_t _M0L3endS2236;
      struct _M0TPC16string10StringView _M0L4_2axS600;
      moonbit_incref(_M0L3strS2244);
      moonbit_incref(_M0L3strS2241);
      #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2243
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2244, 0, _M0L5startS2245, _M0L6_2atmpS2246);
      _M0L6_2atmpS2242 = (int32_t)_M0L6_2atmpS2243;
      #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L4_2acS599
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2241, _M0L6_2atmpS2242);
      _M0L8_2afieldS3352 = _M0L8_2aparamS598.$0;
      _M0L3strS2234 = _M0L8_2afieldS3352;
      _M0L8_2afieldS3351 = _M0L8_2aparamS598.$0;
      _M0L3strS2237 = _M0L8_2afieldS3351;
      _M0L5startS2238 = _M0L8_2aparamS598.$1;
      _M0L3endS2240 = _M0L8_2aparamS598.$2;
      _M0L6_2atmpS2239 = (int64_t)_M0L3endS2240;
      moonbit_incref(_M0L3strS2237);
      moonbit_incref(_M0L3strS2234);
      #line 653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L7_2abindS1354
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2237, 1, _M0L5startS2238, _M0L6_2atmpS2239);
      if (_M0L7_2abindS1354 == 4294967296ll) {
        _M0L6_2atmpS2235 = _M0L8_2aparamS598.$2;
      } else {
        int64_t _M0L7_2aSomeS601 = _M0L7_2abindS1354;
        _M0L6_2atmpS2235 = (int32_t)_M0L7_2aSomeS601;
      }
      _M0L3endS2236 = _M0L8_2aparamS598.$2;
      _M0L4_2axS600
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS2235, _M0L3endS2236, _M0L3strS2234
      };
      moonbit_incref(_M0L5charsS602.$0);
      #line 655 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      if (
        _M0MPC16string10StringView14contains__char(_M0L5charsS602, _M0L4_2acS599)
      ) {
        moonbit_decref(_M0L8_2aparamS598.$0);
        _M0L8_2aparamS598 = _M0L4_2axS600;
        continue;
      } else {
        moonbit_decref(_M0L5charsS602.$0);
        moonbit_decref(_M0L4_2axS600.$0);
        return _M0L8_2aparamS598;
      }
    }
    break;
  }
}

int32_t _M0MPC16string10StringView14contains__char(
  struct _M0TPC16string10StringView _M0L4selfS588,
  int32_t _M0L1cS590
) {
  int32_t _M0L3lenS587;
  #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L4selfS588.$0);
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L3lenS587 = _M0MPC16string10StringView6length(_M0L4selfS588);
  if (_M0L3lenS587 > 0) {
    int32_t _M0L1cS589 = _M0L1cS590;
    if (_M0L1cS589 <= 65535) {
      int32_t _M0L1iS591 = 0;
      while (1) {
        if (_M0L1iS591 < _M0L3lenS587) {
          int32_t _M0L6_2atmpS2216;
          int32_t _M0L6_2atmpS2215;
          int32_t _M0L6_2atmpS2217;
          moonbit_incref(_M0L4selfS588.$0);
          #line 598 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2216
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS588, _M0L1iS591);
          _M0L6_2atmpS2215 = (int32_t)_M0L6_2atmpS2216;
          if (_M0L6_2atmpS2215 == _M0L1cS589) {
            moonbit_decref(_M0L4selfS588.$0);
            return 1;
          }
          _M0L6_2atmpS2217 = _M0L1iS591 + 1;
          _M0L1iS591 = _M0L6_2atmpS2217;
          continue;
        } else {
          moonbit_decref(_M0L4selfS588.$0);
        }
        break;
      }
    } else if (_M0L3lenS587 >= 2) {
      int32_t _M0L3adjS593 = _M0L1cS589 - 65536;
      int32_t _M0L6_2atmpS2229 = _M0L3adjS593 >> 10;
      int32_t _M0L4highS594 = 55296 + _M0L6_2atmpS2229;
      int32_t _M0L6_2atmpS2228 = _M0L3adjS593 & 1023;
      int32_t _M0L3lowS595 = 56320 + _M0L6_2atmpS2228;
      int32_t _M0Lm1iS596 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2218 = _M0Lm1iS596;
        int32_t _M0L6_2atmpS2219 = _M0L3lenS587 - 1;
        if (_M0L6_2atmpS2218 < _M0L6_2atmpS2219) {
          int32_t _M0L6_2atmpS2222 = _M0Lm1iS596;
          int32_t _M0L6_2atmpS2221;
          int32_t _M0L6_2atmpS2220;
          int32_t _M0L6_2atmpS2227;
          moonbit_incref(_M0L4selfS588.$0);
          #line 612 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2221
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS588, _M0L6_2atmpS2222);
          _M0L6_2atmpS2220 = (int32_t)_M0L6_2atmpS2221;
          if (_M0L6_2atmpS2220 == _M0L4highS594) {
            int32_t _M0L6_2atmpS2223 = _M0Lm1iS596;
            int32_t _M0L6_2atmpS2226;
            int32_t _M0L6_2atmpS2225;
            int32_t _M0L6_2atmpS2224;
            _M0Lm1iS596 = _M0L6_2atmpS2223 + 1;
            _M0L6_2atmpS2226 = _M0Lm1iS596;
            moonbit_incref(_M0L4selfS588.$0);
            #line 614 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            _M0L6_2atmpS2225
            = _M0MPC16string10StringView11unsafe__get(_M0L4selfS588, _M0L6_2atmpS2226);
            _M0L6_2atmpS2224 = (int32_t)_M0L6_2atmpS2225;
            if (_M0L6_2atmpS2224 == _M0L3lowS595) {
              moonbit_decref(_M0L4selfS588.$0);
              return 1;
            }
          }
          _M0L6_2atmpS2227 = _M0Lm1iS596;
          _M0Lm1iS596 = _M0L6_2atmpS2227 + 1;
          continue;
        } else {
          moonbit_decref(_M0L4selfS588.$0);
        }
        break;
      }
    } else {
      moonbit_decref(_M0L4selfS588.$0);
      return 0;
    }
    return 0;
  } else {
    moonbit_decref(_M0L4selfS588.$0);
    return 0;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS578,
  moonbit_string_t _M0L5valueS580
) {
  int32_t _M0L3lenS2200;
  moonbit_string_t* _M0L6_2atmpS2202;
  int32_t _M0L6_2atmpS3358;
  int32_t _M0L6_2atmpS2201;
  int32_t _M0L6lengthS579;
  moonbit_string_t* _M0L8_2afieldS3357;
  moonbit_string_t* _M0L3bufS2203;
  moonbit_string_t _M0L6_2aoldS3356;
  int32_t _M0L6_2atmpS2204;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2200 = _M0L4selfS578->$1;
  moonbit_incref(_M0L4selfS578);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2202 = _M0MPC15array5Array6bufferGsE(_M0L4selfS578);
  _M0L6_2atmpS3358 = Moonbit_array_length(_M0L6_2atmpS2202);
  moonbit_decref(_M0L6_2atmpS2202);
  _M0L6_2atmpS2201 = _M0L6_2atmpS3358;
  if (_M0L3lenS2200 == _M0L6_2atmpS2201) {
    moonbit_incref(_M0L4selfS578);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS578);
  }
  _M0L6lengthS579 = _M0L4selfS578->$1;
  _M0L8_2afieldS3357 = _M0L4selfS578->$0;
  _M0L3bufS2203 = _M0L8_2afieldS3357;
  _M0L6_2aoldS3356 = (moonbit_string_t)_M0L3bufS2203[_M0L6lengthS579];
  moonbit_decref(_M0L6_2aoldS3356);
  _M0L3bufS2203[_M0L6lengthS579] = _M0L5valueS580;
  _M0L6_2atmpS2204 = _M0L6lengthS579 + 1;
  _M0L4selfS578->$1 = _M0L6_2atmpS2204;
  moonbit_decref(_M0L4selfS578);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS581,
  struct _M0TUsiE* _M0L5valueS583
) {
  int32_t _M0L3lenS2205;
  struct _M0TUsiE** _M0L6_2atmpS2207;
  int32_t _M0L6_2atmpS3361;
  int32_t _M0L6_2atmpS2206;
  int32_t _M0L6lengthS582;
  struct _M0TUsiE** _M0L8_2afieldS3360;
  struct _M0TUsiE** _M0L3bufS2208;
  struct _M0TUsiE* _M0L6_2aoldS3359;
  int32_t _M0L6_2atmpS2209;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2205 = _M0L4selfS581->$1;
  moonbit_incref(_M0L4selfS581);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2207 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS581);
  _M0L6_2atmpS3361 = Moonbit_array_length(_M0L6_2atmpS2207);
  moonbit_decref(_M0L6_2atmpS2207);
  _M0L6_2atmpS2206 = _M0L6_2atmpS3361;
  if (_M0L3lenS2205 == _M0L6_2atmpS2206) {
    moonbit_incref(_M0L4selfS581);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS581);
  }
  _M0L6lengthS582 = _M0L4selfS581->$1;
  _M0L8_2afieldS3360 = _M0L4selfS581->$0;
  _M0L3bufS2208 = _M0L8_2afieldS3360;
  _M0L6_2aoldS3359 = (struct _M0TUsiE*)_M0L3bufS2208[_M0L6lengthS582];
  if (_M0L6_2aoldS3359) {
    moonbit_decref(_M0L6_2aoldS3359);
  }
  _M0L3bufS2208[_M0L6lengthS582] = _M0L5valueS583;
  _M0L6_2atmpS2209 = _M0L6lengthS582 + 1;
  _M0L4selfS581->$1 = _M0L6_2atmpS2209;
  moonbit_decref(_M0L4selfS581);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS584,
  void* _M0L5valueS586
) {
  int32_t _M0L3lenS2210;
  void** _M0L6_2atmpS2212;
  int32_t _M0L6_2atmpS3364;
  int32_t _M0L6_2atmpS2211;
  int32_t _M0L6lengthS585;
  void** _M0L8_2afieldS3363;
  void** _M0L3bufS2213;
  void* _M0L6_2aoldS3362;
  int32_t _M0L6_2atmpS2214;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2210 = _M0L4selfS584->$1;
  moonbit_incref(_M0L4selfS584);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2212
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS584);
  _M0L6_2atmpS3364 = Moonbit_array_length(_M0L6_2atmpS2212);
  moonbit_decref(_M0L6_2atmpS2212);
  _M0L6_2atmpS2211 = _M0L6_2atmpS3364;
  if (_M0L3lenS2210 == _M0L6_2atmpS2211) {
    moonbit_incref(_M0L4selfS584);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS584);
  }
  _M0L6lengthS585 = _M0L4selfS584->$1;
  _M0L8_2afieldS3363 = _M0L4selfS584->$0;
  _M0L3bufS2213 = _M0L8_2afieldS3363;
  _M0L6_2aoldS3362 = (void*)_M0L3bufS2213[_M0L6lengthS585];
  moonbit_decref(_M0L6_2aoldS3362);
  _M0L3bufS2213[_M0L6lengthS585] = _M0L5valueS586;
  _M0L6_2atmpS2214 = _M0L6lengthS585 + 1;
  _M0L4selfS584->$1 = _M0L6_2atmpS2214;
  moonbit_decref(_M0L4selfS584);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS570) {
  int32_t _M0L8old__capS569;
  int32_t _M0L8new__capS571;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS569 = _M0L4selfS570->$1;
  if (_M0L8old__capS569 == 0) {
    _M0L8new__capS571 = 8;
  } else {
    _M0L8new__capS571 = _M0L8old__capS569 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS570, _M0L8new__capS571);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS573
) {
  int32_t _M0L8old__capS572;
  int32_t _M0L8new__capS574;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS572 = _M0L4selfS573->$1;
  if (_M0L8old__capS572 == 0) {
    _M0L8new__capS574 = 8;
  } else {
    _M0L8new__capS574 = _M0L8old__capS572 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS573, _M0L8new__capS574);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS576
) {
  int32_t _M0L8old__capS575;
  int32_t _M0L8new__capS577;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS575 = _M0L4selfS576->$1;
  if (_M0L8old__capS575 == 0) {
    _M0L8new__capS577 = 8;
  } else {
    _M0L8new__capS577 = _M0L8old__capS575 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS576, _M0L8new__capS577);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS554,
  int32_t _M0L13new__capacityS552
) {
  moonbit_string_t* _M0L8new__bufS551;
  moonbit_string_t* _M0L8_2afieldS3366;
  moonbit_string_t* _M0L8old__bufS553;
  int32_t _M0L8old__capS555;
  int32_t _M0L9copy__lenS556;
  moonbit_string_t* _M0L6_2aoldS3365;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS551
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS552, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3366 = _M0L4selfS554->$0;
  _M0L8old__bufS553 = _M0L8_2afieldS3366;
  _M0L8old__capS555 = Moonbit_array_length(_M0L8old__bufS553);
  if (_M0L8old__capS555 < _M0L13new__capacityS552) {
    _M0L9copy__lenS556 = _M0L8old__capS555;
  } else {
    _M0L9copy__lenS556 = _M0L13new__capacityS552;
  }
  moonbit_incref(_M0L8old__bufS553);
  moonbit_incref(_M0L8new__bufS551);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS551, 0, _M0L8old__bufS553, 0, _M0L9copy__lenS556);
  _M0L6_2aoldS3365 = _M0L4selfS554->$0;
  moonbit_decref(_M0L6_2aoldS3365);
  _M0L4selfS554->$0 = _M0L8new__bufS551;
  moonbit_decref(_M0L4selfS554);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS560,
  int32_t _M0L13new__capacityS558
) {
  struct _M0TUsiE** _M0L8new__bufS557;
  struct _M0TUsiE** _M0L8_2afieldS3368;
  struct _M0TUsiE** _M0L8old__bufS559;
  int32_t _M0L8old__capS561;
  int32_t _M0L9copy__lenS562;
  struct _M0TUsiE** _M0L6_2aoldS3367;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS557
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS558, 0);
  _M0L8_2afieldS3368 = _M0L4selfS560->$0;
  _M0L8old__bufS559 = _M0L8_2afieldS3368;
  _M0L8old__capS561 = Moonbit_array_length(_M0L8old__bufS559);
  if (_M0L8old__capS561 < _M0L13new__capacityS558) {
    _M0L9copy__lenS562 = _M0L8old__capS561;
  } else {
    _M0L9copy__lenS562 = _M0L13new__capacityS558;
  }
  moonbit_incref(_M0L8old__bufS559);
  moonbit_incref(_M0L8new__bufS557);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS557, 0, _M0L8old__bufS559, 0, _M0L9copy__lenS562);
  _M0L6_2aoldS3367 = _M0L4selfS560->$0;
  moonbit_decref(_M0L6_2aoldS3367);
  _M0L4selfS560->$0 = _M0L8new__bufS557;
  moonbit_decref(_M0L4selfS560);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS566,
  int32_t _M0L13new__capacityS564
) {
  void** _M0L8new__bufS563;
  void** _M0L8_2afieldS3370;
  void** _M0L8old__bufS565;
  int32_t _M0L8old__capS567;
  int32_t _M0L9copy__lenS568;
  void** _M0L6_2aoldS3369;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS563
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS564, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3370 = _M0L4selfS566->$0;
  _M0L8old__bufS565 = _M0L8_2afieldS3370;
  _M0L8old__capS567 = Moonbit_array_length(_M0L8old__bufS565);
  if (_M0L8old__capS567 < _M0L13new__capacityS564) {
    _M0L9copy__lenS568 = _M0L8old__capS567;
  } else {
    _M0L9copy__lenS568 = _M0L13new__capacityS564;
  }
  moonbit_incref(_M0L8old__bufS565);
  moonbit_incref(_M0L8new__bufS563);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS563, 0, _M0L8old__bufS565, 0, _M0L9copy__lenS568);
  _M0L6_2aoldS3369 = _M0L4selfS566->$0;
  moonbit_decref(_M0L6_2aoldS3369);
  _M0L4selfS566->$0 = _M0L8new__bufS563;
  moonbit_decref(_M0L4selfS566);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS550
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS550 == 0) {
    moonbit_string_t* _M0L6_2atmpS2198 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3651 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3651)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3651->$0 = _M0L6_2atmpS2198;
    _block_3651->$1 = 0;
    return _block_3651;
  } else {
    moonbit_string_t* _M0L6_2atmpS2199 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS550, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3652 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3652)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3652->$0 = _M0L6_2atmpS2199;
    _block_3652->$1 = 0;
    return _block_3652;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS544,
  int32_t _M0L1nS543
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS543 <= 0) {
    moonbit_decref(_M0L4selfS544);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS543 == 1) {
    return _M0L4selfS544;
  } else {
    int32_t _M0L3lenS545 = Moonbit_array_length(_M0L4selfS544);
    int32_t _M0L6_2atmpS2197 = _M0L3lenS545 * _M0L1nS543;
    struct _M0TPB13StringBuilder* _M0L3bufS546;
    moonbit_string_t _M0L3strS547;
    int32_t _M0L2__S548;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS546 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2197);
    _M0L3strS547 = _M0L4selfS544;
    _M0L2__S548 = 0;
    while (1) {
      if (_M0L2__S548 < _M0L1nS543) {
        int32_t _M0L6_2atmpS2196;
        moonbit_incref(_M0L3strS547);
        moonbit_incref(_M0L3bufS546);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS546, _M0L3strS547);
        _M0L6_2atmpS2196 = _M0L2__S548 + 1;
        _M0L2__S548 = _M0L6_2atmpS2196;
        continue;
      } else {
        moonbit_decref(_M0L3strS547);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS546);
  }
}

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView _M0L4selfS542,
  struct _M0TPC16string10StringView _M0L3strS541
) {
  int32_t _M0L6_2atmpS2195;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L3strS541.$0);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2195 = _M0MPC16string10StringView6length(_M0L3strS541);
  if (_M0L6_2atmpS2195 <= 4) {
    #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0FPB18brute__force__find(_M0L4selfS542, _M0L3strS541);
  } else {
    #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0FPB28boyer__moore__horspool__find(_M0L4selfS542, _M0L3strS541);
  }
}

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView _M0L8haystackS531,
  struct _M0TPC16string10StringView _M0L6needleS533
) {
  int32_t _M0L13haystack__lenS530;
  int32_t _M0L11needle__lenS532;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L8haystackS531.$0);
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L13haystack__lenS530
  = _M0MPC16string10StringView6length(_M0L8haystackS531);
  moonbit_incref(_M0L6needleS533.$0);
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L11needle__lenS532 = _M0MPC16string10StringView6length(_M0L6needleS533);
  if (_M0L11needle__lenS532 > 0) {
    if (_M0L13haystack__lenS530 >= _M0L11needle__lenS532) {
      int32_t _M0L13needle__firstS534;
      int32_t _M0L12forward__lenS535;
      int32_t _M0Lm1iS536;
      moonbit_incref(_M0L6needleS533.$0);
      #line 36 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L13needle__firstS534
      = _M0MPC16string10StringView11unsafe__get(_M0L6needleS533, 0);
      _M0L12forward__lenS535
      = _M0L13haystack__lenS530 - _M0L11needle__lenS532;
      _M0Lm1iS536 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2182 = _M0Lm1iS536;
        if (_M0L6_2atmpS2182 <= _M0L12forward__lenS535) {
          int32_t _M0L6_2atmpS2187;
          while (1) {
            int32_t _M0L6_2atmpS2185 = _M0Lm1iS536;
            int32_t _if__result_3656;
            if (_M0L6_2atmpS2185 <= _M0L12forward__lenS535) {
              int32_t _M0L6_2atmpS2184 = _M0Lm1iS536;
              int32_t _M0L6_2atmpS2183;
              moonbit_incref(_M0L8haystackS531.$0);
              #line 41 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS2183
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS531, _M0L6_2atmpS2184);
              _if__result_3656 = _M0L6_2atmpS2183 != _M0L13needle__firstS534;
            } else {
              _if__result_3656 = 0;
            }
            if (_if__result_3656) {
              int32_t _M0L6_2atmpS2186 = _M0Lm1iS536;
              _M0Lm1iS536 = _M0L6_2atmpS2186 + 1;
              continue;
            }
            break;
          }
          _M0L6_2atmpS2187 = _M0Lm1iS536;
          if (_M0L6_2atmpS2187 <= _M0L12forward__lenS535) {
            int32_t _M0L1jS538 = 1;
            int32_t _M0L6_2atmpS2194;
            while (1) {
              if (_M0L1jS538 < _M0L11needle__lenS532) {
                int32_t _M0L6_2atmpS2191 = _M0Lm1iS536;
                int32_t _M0L6_2atmpS2190 = _M0L6_2atmpS2191 + _M0L1jS538;
                int32_t _M0L6_2atmpS2188;
                int32_t _M0L6_2atmpS2189;
                int32_t _M0L6_2atmpS2192;
                moonbit_incref(_M0L8haystackS531.$0);
                #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
                _M0L6_2atmpS2188
                = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS531, _M0L6_2atmpS2190);
                moonbit_incref(_M0L6needleS533.$0);
                #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
                _M0L6_2atmpS2189
                = _M0MPC16string10StringView11unsafe__get(_M0L6needleS533, _M0L1jS538);
                if (_M0L6_2atmpS2188 != _M0L6_2atmpS2189) {
                  break;
                }
                _M0L6_2atmpS2192 = _M0L1jS538 + 1;
                _M0L1jS538 = _M0L6_2atmpS2192;
                continue;
              } else {
                int32_t _M0L6_2atmpS2193;
                moonbit_decref(_M0L6needleS533.$0);
                moonbit_decref(_M0L8haystackS531.$0);
                _M0L6_2atmpS2193 = _M0Lm1iS536;
                return (int64_t)_M0L6_2atmpS2193;
              }
              break;
            }
            _M0L6_2atmpS2194 = _M0Lm1iS536;
            _M0Lm1iS536 = _M0L6_2atmpS2194 + 1;
          }
          continue;
        } else {
          moonbit_decref(_M0L6needleS533.$0);
          moonbit_decref(_M0L8haystackS531.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS533.$0);
      moonbit_decref(_M0L8haystackS531.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS533.$0);
    moonbit_decref(_M0L8haystackS531.$0);
    return _M0FPB33brute__force__find_2econstr_2f529;
  }
}

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView _M0L8haystackS517,
  struct _M0TPC16string10StringView _M0L6needleS519
) {
  int32_t _M0L13haystack__lenS516;
  int32_t _M0L11needle__lenS518;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L8haystackS517.$0);
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L13haystack__lenS516
  = _M0MPC16string10StringView6length(_M0L8haystackS517);
  moonbit_incref(_M0L6needleS519.$0);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L11needle__lenS518 = _M0MPC16string10StringView6length(_M0L6needleS519);
  if (_M0L11needle__lenS518 > 0) {
    if (_M0L13haystack__lenS516 >= _M0L11needle__lenS518) {
      int32_t* _M0L11skip__tableS520 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS518);
      int32_t _M0L7_2abindS521 = _M0L11needle__lenS518 - 1;
      int32_t _M0L1iS522 = 0;
      int32_t _M0L1iS524;
      while (1) {
        if (_M0L1iS522 < _M0L7_2abindS521) {
          int32_t _M0L6_2atmpS2168;
          int32_t _M0L6_2atmpS2167;
          int32_t _M0L6_2atmpS2164;
          int32_t _M0L6_2atmpS2166;
          int32_t _M0L6_2atmpS2165;
          int32_t _M0L6_2atmpS2169;
          moonbit_incref(_M0L6needleS519.$0);
          #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2168
          = _M0MPC16string10StringView11unsafe__get(_M0L6needleS519, _M0L1iS522);
          _M0L6_2atmpS2167 = (int32_t)_M0L6_2atmpS2168;
          _M0L6_2atmpS2164 = _M0L6_2atmpS2167 & 255;
          _M0L6_2atmpS2166 = _M0L11needle__lenS518 - 1;
          _M0L6_2atmpS2165 = _M0L6_2atmpS2166 - _M0L1iS522;
          if (
            _M0L6_2atmpS2164 < 0
            || _M0L6_2atmpS2164
               >= Moonbit_array_length(_M0L11skip__tableS520)
          ) {
            #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS520[_M0L6_2atmpS2164] = _M0L6_2atmpS2165;
          _M0L6_2atmpS2169 = _M0L1iS522 + 1;
          _M0L1iS522 = _M0L6_2atmpS2169;
          continue;
        }
        break;
      }
      _M0L1iS524 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2170 =
          _M0L13haystack__lenS516 - _M0L11needle__lenS518;
        if (_M0L1iS524 <= _M0L6_2atmpS2170) {
          int32_t _M0L7_2abindS525 = _M0L11needle__lenS518 - 1;
          int32_t _M0L1jS526 = 0;
          int32_t _M0L6_2atmpS2181;
          int32_t _M0L6_2atmpS2180;
          int32_t _M0L6_2atmpS2179;
          int32_t _M0L6_2atmpS2178;
          int32_t _M0L6_2atmpS2177;
          int32_t _M0L6_2atmpS2176;
          int32_t _M0L6_2atmpS2175;
          while (1) {
            if (_M0L1jS526 <= _M0L7_2abindS525) {
              int32_t _M0L6_2atmpS2173 = _M0L1iS524 + _M0L1jS526;
              int32_t _M0L6_2atmpS2171;
              int32_t _M0L6_2atmpS2172;
              int32_t _M0L6_2atmpS2174;
              moonbit_incref(_M0L8haystackS517.$0);
              #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS2171
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS517, _M0L6_2atmpS2173);
              moonbit_incref(_M0L6needleS519.$0);
              #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS2172
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS519, _M0L1jS526);
              if (_M0L6_2atmpS2171 != _M0L6_2atmpS2172) {
                break;
              }
              _M0L6_2atmpS2174 = _M0L1jS526 + 1;
              _M0L1jS526 = _M0L6_2atmpS2174;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS520);
              moonbit_decref(_M0L6needleS519.$0);
              moonbit_decref(_M0L8haystackS517.$0);
              return (int64_t)_M0L1iS524;
            }
            break;
          }
          _M0L6_2atmpS2181 = _M0L1iS524 + _M0L11needle__lenS518;
          _M0L6_2atmpS2180 = _M0L6_2atmpS2181 - 1;
          moonbit_incref(_M0L8haystackS517.$0);
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2179
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS517, _M0L6_2atmpS2180);
          _M0L6_2atmpS2178 = (int32_t)_M0L6_2atmpS2179;
          _M0L6_2atmpS2177 = _M0L6_2atmpS2178 & 255;
          if (
            _M0L6_2atmpS2177 < 0
            || _M0L6_2atmpS2177
               >= Moonbit_array_length(_M0L11skip__tableS520)
          ) {
            #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2176 = (int32_t)_M0L11skip__tableS520[_M0L6_2atmpS2177];
          _M0L6_2atmpS2175 = _M0L1iS524 + _M0L6_2atmpS2176;
          _M0L1iS524 = _M0L6_2atmpS2175;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS520);
          moonbit_decref(_M0L6needleS519.$0);
          moonbit_decref(_M0L8haystackS517.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS519.$0);
      moonbit_decref(_M0L8haystackS517.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS519.$0);
    moonbit_decref(_M0L8haystackS517.$0);
    return _M0FPB43boyer__moore__horspool__find_2econstr_2f515;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS513,
  struct _M0TPC16string10StringView _M0L3strS514
) {
  int32_t _M0L3lenS2152;
  int32_t _M0L6_2atmpS2154;
  int32_t _M0L6_2atmpS2153;
  int32_t _M0L6_2atmpS2151;
  moonbit_bytes_t _M0L8_2afieldS3371;
  moonbit_bytes_t _M0L4dataS2155;
  int32_t _M0L3lenS2156;
  moonbit_string_t _M0L6_2atmpS2157;
  int32_t _M0L6_2atmpS2158;
  int32_t _M0L6_2atmpS2159;
  int32_t _M0L3lenS2161;
  int32_t _M0L6_2atmpS2163;
  int32_t _M0L6_2atmpS2162;
  int32_t _M0L6_2atmpS2160;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2152 = _M0L4selfS513->$1;
  moonbit_incref(_M0L3strS514.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2154 = _M0MPC16string10StringView6length(_M0L3strS514);
  _M0L6_2atmpS2153 = _M0L6_2atmpS2154 * 2;
  _M0L6_2atmpS2151 = _M0L3lenS2152 + _M0L6_2atmpS2153;
  moonbit_incref(_M0L4selfS513);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS513, _M0L6_2atmpS2151);
  _M0L8_2afieldS3371 = _M0L4selfS513->$0;
  _M0L4dataS2155 = _M0L8_2afieldS3371;
  _M0L3lenS2156 = _M0L4selfS513->$1;
  moonbit_incref(_M0L4dataS2155);
  moonbit_incref(_M0L3strS514.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2157 = _M0MPC16string10StringView4data(_M0L3strS514);
  moonbit_incref(_M0L3strS514.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2158 = _M0MPC16string10StringView13start__offset(_M0L3strS514);
  moonbit_incref(_M0L3strS514.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2159 = _M0MPC16string10StringView6length(_M0L3strS514);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2155, _M0L3lenS2156, _M0L6_2atmpS2157, _M0L6_2atmpS2158, _M0L6_2atmpS2159);
  _M0L3lenS2161 = _M0L4selfS513->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2163 = _M0MPC16string10StringView6length(_M0L3strS514);
  _M0L6_2atmpS2162 = _M0L6_2atmpS2163 * 2;
  _M0L6_2atmpS2160 = _M0L3lenS2161 + _M0L6_2atmpS2162;
  _M0L4selfS513->$1 = _M0L6_2atmpS2160;
  moonbit_decref(_M0L4selfS513);
  return 0;
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS510,
  int32_t _M0L1iS511,
  int32_t _M0L13start__offsetS512,
  int64_t _M0L11end__offsetS508
) {
  int32_t _M0L11end__offsetS507;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS508 == 4294967296ll) {
    _M0L11end__offsetS507 = Moonbit_array_length(_M0L4selfS510);
  } else {
    int64_t _M0L7_2aSomeS509 = _M0L11end__offsetS508;
    _M0L11end__offsetS507 = (int32_t)_M0L7_2aSomeS509;
  }
  if (_M0L1iS511 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS510, _M0L1iS511, _M0L13start__offsetS512, _M0L11end__offsetS507);
  } else {
    int32_t _M0L6_2atmpS2150 = -_M0L1iS511;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS510, _M0L6_2atmpS2150, _M0L13start__offsetS512, _M0L11end__offsetS507);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS505,
  int32_t _M0L1nS503,
  int32_t _M0L13start__offsetS499,
  int32_t _M0L11end__offsetS500
) {
  int32_t _if__result_3661;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS499 >= 0) {
    _if__result_3661 = _M0L13start__offsetS499 <= _M0L11end__offsetS500;
  } else {
    _if__result_3661 = 0;
  }
  if (_if__result_3661) {
    int32_t _M0Lm13utf16__offsetS501 = _M0L13start__offsetS499;
    int32_t _M0Lm11char__countS502 = 0;
    int32_t _M0L6_2atmpS2148;
    int32_t _if__result_3664;
    while (1) {
      int32_t _M0L6_2atmpS2142 = _M0Lm13utf16__offsetS501;
      int32_t _if__result_3663;
      if (_M0L6_2atmpS2142 < _M0L11end__offsetS500) {
        int32_t _M0L6_2atmpS2141 = _M0Lm11char__countS502;
        _if__result_3663 = _M0L6_2atmpS2141 < _M0L1nS503;
      } else {
        _if__result_3663 = 0;
      }
      if (_if__result_3663) {
        int32_t _M0L6_2atmpS2146 = _M0Lm13utf16__offsetS501;
        int32_t _M0L1cS504 = _M0L4selfS505[_M0L6_2atmpS2146];
        int32_t _M0L6_2atmpS2145;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS504)) {
          int32_t _M0L6_2atmpS2143 = _M0Lm13utf16__offsetS501;
          _M0Lm13utf16__offsetS501 = _M0L6_2atmpS2143 + 2;
        } else {
          int32_t _M0L6_2atmpS2144 = _M0Lm13utf16__offsetS501;
          _M0Lm13utf16__offsetS501 = _M0L6_2atmpS2144 + 1;
        }
        _M0L6_2atmpS2145 = _M0Lm11char__countS502;
        _M0Lm11char__countS502 = _M0L6_2atmpS2145 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS505);
      }
      break;
    }
    _M0L6_2atmpS2148 = _M0Lm11char__countS502;
    if (_M0L6_2atmpS2148 < _M0L1nS503) {
      _if__result_3664 = 1;
    } else {
      int32_t _M0L6_2atmpS2147 = _M0Lm13utf16__offsetS501;
      _if__result_3664 = _M0L6_2atmpS2147 >= _M0L11end__offsetS500;
    }
    if (_if__result_3664) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2149 = _M0Lm13utf16__offsetS501;
      return (int64_t)_M0L6_2atmpS2149;
    }
  } else {
    moonbit_decref(_M0L4selfS505);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_53.data, (moonbit_string_t)moonbit_string_literal_54.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS497,
  int32_t _M0L1nS495,
  int32_t _M0L13start__offsetS494,
  int32_t _M0L11end__offsetS493
) {
  int32_t _M0Lm11char__countS491;
  int32_t _M0Lm13utf16__offsetS492;
  int32_t _M0L6_2atmpS2139;
  int32_t _if__result_3667;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS491 = 0;
  _M0Lm13utf16__offsetS492 = _M0L11end__offsetS493;
  while (1) {
    int32_t _M0L6_2atmpS2132 = _M0Lm13utf16__offsetS492;
    int32_t _M0L6_2atmpS2131 = _M0L6_2atmpS2132 - 1;
    int32_t _if__result_3666;
    if (_M0L6_2atmpS2131 >= _M0L13start__offsetS494) {
      int32_t _M0L6_2atmpS2130 = _M0Lm11char__countS491;
      _if__result_3666 = _M0L6_2atmpS2130 < _M0L1nS495;
    } else {
      _if__result_3666 = 0;
    }
    if (_if__result_3666) {
      int32_t _M0L6_2atmpS2137 = _M0Lm13utf16__offsetS492;
      int32_t _M0L6_2atmpS2136 = _M0L6_2atmpS2137 - 1;
      int32_t _M0L1cS496 = _M0L4selfS497[_M0L6_2atmpS2136];
      int32_t _M0L6_2atmpS2135;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS496)) {
        int32_t _M0L6_2atmpS2133 = _M0Lm13utf16__offsetS492;
        _M0Lm13utf16__offsetS492 = _M0L6_2atmpS2133 - 2;
      } else {
        int32_t _M0L6_2atmpS2134 = _M0Lm13utf16__offsetS492;
        _M0Lm13utf16__offsetS492 = _M0L6_2atmpS2134 - 1;
      }
      _M0L6_2atmpS2135 = _M0Lm11char__countS491;
      _M0Lm11char__countS491 = _M0L6_2atmpS2135 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS497);
    }
    break;
  }
  _M0L6_2atmpS2139 = _M0Lm11char__countS491;
  if (_M0L6_2atmpS2139 < _M0L1nS495) {
    _if__result_3667 = 1;
  } else {
    int32_t _M0L6_2atmpS2138 = _M0Lm13utf16__offsetS492;
    _if__result_3667 = _M0L6_2atmpS2138 < _M0L13start__offsetS494;
  }
  if (_if__result_3667) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2140 = _M0Lm13utf16__offsetS492;
    return (int64_t)_M0L6_2atmpS2140;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS483,
  int32_t _M0L3lenS486,
  int32_t _M0L13start__offsetS490,
  int64_t _M0L11end__offsetS481
) {
  int32_t _M0L11end__offsetS480;
  int32_t _M0L5indexS484;
  int32_t _M0L5countS485;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS481 == 4294967296ll) {
    _M0L11end__offsetS480 = Moonbit_array_length(_M0L4selfS483);
  } else {
    int64_t _M0L7_2aSomeS482 = _M0L11end__offsetS481;
    _M0L11end__offsetS480 = (int32_t)_M0L7_2aSomeS482;
  }
  _M0L5indexS484 = _M0L13start__offsetS490;
  _M0L5countS485 = 0;
  while (1) {
    int32_t _if__result_3669;
    if (_M0L5indexS484 < _M0L11end__offsetS480) {
      _if__result_3669 = _M0L5countS485 < _M0L3lenS486;
    } else {
      _if__result_3669 = 0;
    }
    if (_if__result_3669) {
      int32_t _M0L2c1S487 = _M0L4selfS483[_M0L5indexS484];
      int32_t _if__result_3670;
      int32_t _M0L6_2atmpS2128;
      int32_t _M0L6_2atmpS2129;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S487)) {
        int32_t _M0L6_2atmpS2124 = _M0L5indexS484 + 1;
        _if__result_3670 = _M0L6_2atmpS2124 < _M0L11end__offsetS480;
      } else {
        _if__result_3670 = 0;
      }
      if (_if__result_3670) {
        int32_t _M0L6_2atmpS2127 = _M0L5indexS484 + 1;
        int32_t _M0L2c2S488 = _M0L4selfS483[_M0L6_2atmpS2127];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S488)) {
          int32_t _M0L6_2atmpS2125 = _M0L5indexS484 + 2;
          int32_t _M0L6_2atmpS2126 = _M0L5countS485 + 1;
          _M0L5indexS484 = _M0L6_2atmpS2125;
          _M0L5countS485 = _M0L6_2atmpS2126;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_56.data);
        }
      }
      _M0L6_2atmpS2128 = _M0L5indexS484 + 1;
      _M0L6_2atmpS2129 = _M0L5countS485 + 1;
      _M0L5indexS484 = _M0L6_2atmpS2128;
      _M0L5countS485 = _M0L6_2atmpS2129;
      continue;
    } else {
      moonbit_decref(_M0L4selfS483);
      return _M0L5countS485 >= _M0L3lenS486;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS472,
  int32_t _M0L3lenS475,
  int32_t _M0L13start__offsetS479,
  int64_t _M0L11end__offsetS470
) {
  int32_t _M0L11end__offsetS469;
  int32_t _M0L5indexS473;
  int32_t _M0L5countS474;
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS470 == 4294967296ll) {
    _M0L11end__offsetS469 = Moonbit_array_length(_M0L4selfS472);
  } else {
    int64_t _M0L7_2aSomeS471 = _M0L11end__offsetS470;
    _M0L11end__offsetS469 = (int32_t)_M0L7_2aSomeS471;
  }
  _M0L5indexS473 = _M0L13start__offsetS479;
  _M0L5countS474 = 0;
  while (1) {
    int32_t _if__result_3672;
    if (_M0L5indexS473 < _M0L11end__offsetS469) {
      _if__result_3672 = _M0L5countS474 < _M0L3lenS475;
    } else {
      _if__result_3672 = 0;
    }
    if (_if__result_3672) {
      int32_t _M0L2c1S476 = _M0L4selfS472[_M0L5indexS473];
      int32_t _if__result_3673;
      int32_t _M0L6_2atmpS2122;
      int32_t _M0L6_2atmpS2123;
      #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S476)) {
        int32_t _M0L6_2atmpS2118 = _M0L5indexS473 + 1;
        _if__result_3673 = _M0L6_2atmpS2118 < _M0L11end__offsetS469;
      } else {
        _if__result_3673 = 0;
      }
      if (_if__result_3673) {
        int32_t _M0L6_2atmpS2121 = _M0L5indexS473 + 1;
        int32_t _M0L2c2S477 = _M0L4selfS472[_M0L6_2atmpS2121];
        #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S477)) {
          int32_t _M0L6_2atmpS2119 = _M0L5indexS473 + 2;
          int32_t _M0L6_2atmpS2120 = _M0L5countS474 + 1;
          _M0L5indexS473 = _M0L6_2atmpS2119;
          _M0L5countS474 = _M0L6_2atmpS2120;
          continue;
        } else {
          #line 426 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_57.data);
        }
      }
      _M0L6_2atmpS2122 = _M0L5indexS473 + 1;
      _M0L6_2atmpS2123 = _M0L5countS474 + 1;
      _M0L5indexS473 = _M0L6_2atmpS2122;
      _M0L5countS474 = _M0L6_2atmpS2123;
      continue;
    } else {
      moonbit_decref(_M0L4selfS472);
      if (_M0L5countS474 == _M0L3lenS475) {
        return _M0L5indexS473 == _M0L11end__offsetS469;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS466
) {
  int32_t _M0L3endS2112;
  int32_t _M0L8_2afieldS3372;
  int32_t _M0L5startS2113;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2112 = _M0L4selfS466.$2;
  _M0L8_2afieldS3372 = _M0L4selfS466.$1;
  moonbit_decref(_M0L4selfS466.$0);
  _M0L5startS2113 = _M0L8_2afieldS3372;
  return _M0L3endS2112 - _M0L5startS2113;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS467
) {
  int32_t _M0L3endS2114;
  int32_t _M0L8_2afieldS3373;
  int32_t _M0L5startS2115;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2114 = _M0L4selfS467.$2;
  _M0L8_2afieldS3373 = _M0L4selfS467.$1;
  moonbit_decref(_M0L4selfS467.$0);
  _M0L5startS2115 = _M0L8_2afieldS3373;
  return _M0L3endS2114 - _M0L5startS2115;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS468
) {
  int32_t _M0L3endS2116;
  int32_t _M0L8_2afieldS3374;
  int32_t _M0L5startS2117;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2116 = _M0L4selfS468.$2;
  _M0L8_2afieldS3374 = _M0L4selfS468.$1;
  moonbit_decref(_M0L4selfS468.$0);
  _M0L5startS2117 = _M0L8_2afieldS3374;
  return _M0L3endS2116 - _M0L5startS2117;
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
  int32_t _if__result_3674;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS457 == 4294967296ll) {
    _M0L11end__offsetS456 = Moonbit_array_length(_M0L4selfS459);
  } else {
    int64_t _M0L7_2aSomeS458 = _M0L11end__offsetS457;
    _M0L11end__offsetS456 = (int32_t)_M0L7_2aSomeS458;
  }
  if (_M0L13start__offsetS460 >= 0) {
    if (_M0L13start__offsetS460 <= _M0L11end__offsetS456) {
      int32_t _M0L6_2atmpS2111 = Moonbit_array_length(_M0L4selfS459);
      _if__result_3674 = _M0L11end__offsetS456 <= _M0L6_2atmpS2111;
    } else {
      _if__result_3674 = 0;
    }
  } else {
    _if__result_3674 = 0;
  }
  if (_if__result_3674) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS460,
                                                 _M0L11end__offsetS456,
                                                 _M0L4selfS459};
  } else {
    moonbit_decref(_M0L4selfS459);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_58.data, (moonbit_string_t)moonbit_string_literal_59.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS451
) {
  int32_t _M0L5startS450;
  int32_t _M0L3endS452;
  struct _M0TPC13ref3RefGiE* _M0L5indexS453;
  struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__* _closure_3675;
  struct _M0TWEOc* _M0L6_2atmpS2090;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L5startS450 = _M0L4selfS451.$1;
  _M0L3endS452 = _M0L4selfS451.$2;
  _M0L5indexS453
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS453)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS453->$0 = _M0L5startS450;
  _closure_3675
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__));
  Moonbit_object_header(_closure_3675)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__, $0) >> 2, 2, 0);
  _closure_3675->code = &_M0MPC16string10StringView4iterC2091l198;
  _closure_3675->$0 = _M0L5indexS453;
  _closure_3675->$1 = _M0L3endS452;
  _closure_3675->$2_0 = _M0L4selfS451.$0;
  _closure_3675->$2_1 = _M0L4selfS451.$1;
  _closure_3675->$2_2 = _M0L4selfS451.$2;
  _M0L6_2atmpS2090 = (struct _M0TWEOc*)_closure_3675;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2090);
}

int32_t _M0MPC16string10StringView4iterC2091l198(
  struct _M0TWEOc* _M0L6_2aenvS2092
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__* _M0L14_2acasted__envS2093;
  struct _M0TPC16string10StringView _M0L8_2afieldS3380;
  struct _M0TPC16string10StringView _M0L4selfS451;
  int32_t _M0L3endS452;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3379;
  int32_t _M0L6_2acntS3533;
  struct _M0TPC13ref3RefGiE* _M0L5indexS453;
  int32_t _M0L3valS2094;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L14_2acasted__envS2093
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2091__l198__*)_M0L6_2aenvS2092;
  _M0L8_2afieldS3380
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS2093->$2_1,
      _M0L14_2acasted__envS2093->$2_2,
      _M0L14_2acasted__envS2093->$2_0
  };
  _M0L4selfS451 = _M0L8_2afieldS3380;
  _M0L3endS452 = _M0L14_2acasted__envS2093->$1;
  _M0L8_2afieldS3379 = _M0L14_2acasted__envS2093->$0;
  _M0L6_2acntS3533 = Moonbit_object_header(_M0L14_2acasted__envS2093)->rc;
  if (_M0L6_2acntS3533 > 1) {
    int32_t _M0L11_2anew__cntS3534 = _M0L6_2acntS3533 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2093)->rc
    = _M0L11_2anew__cntS3534;
    moonbit_incref(_M0L4selfS451.$0);
    moonbit_incref(_M0L8_2afieldS3379);
  } else if (_M0L6_2acntS3533 == 1) {
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS2093);
  }
  _M0L5indexS453 = _M0L8_2afieldS3379;
  _M0L3valS2094 = _M0L5indexS453->$0;
  if (_M0L3valS2094 < _M0L3endS452) {
    moonbit_string_t _M0L8_2afieldS3378 = _M0L4selfS451.$0;
    moonbit_string_t _M0L3strS2109 = _M0L8_2afieldS3378;
    int32_t _M0L3valS2110 = _M0L5indexS453->$0;
    int32_t _M0L6_2atmpS3377 = _M0L3strS2109[_M0L3valS2110];
    int32_t _M0L2c1S454 = _M0L6_2atmpS3377;
    int32_t _if__result_3676;
    int32_t _M0L3valS2107;
    int32_t _M0L6_2atmpS2106;
    int32_t _M0L6_2atmpS2108;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S454)) {
      int32_t _M0L3valS2097 = _M0L5indexS453->$0;
      int32_t _M0L6_2atmpS2095 = _M0L3valS2097 + 1;
      int32_t _M0L3endS2096 = _M0L4selfS451.$2;
      _if__result_3676 = _M0L6_2atmpS2095 < _M0L3endS2096;
    } else {
      _if__result_3676 = 0;
    }
    if (_if__result_3676) {
      moonbit_string_t _M0L8_2afieldS3376 = _M0L4selfS451.$0;
      moonbit_string_t _M0L3strS2103 = _M0L8_2afieldS3376;
      int32_t _M0L3valS2105 = _M0L5indexS453->$0;
      int32_t _M0L6_2atmpS2104 = _M0L3valS2105 + 1;
      int32_t _M0L6_2atmpS3375 = _M0L3strS2103[_M0L6_2atmpS2104];
      int32_t _M0L2c2S455;
      moonbit_decref(_M0L3strS2103);
      _M0L2c2S455 = _M0L6_2atmpS3375;
      #line 203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S455)) {
        int32_t _M0L3valS2099 = _M0L5indexS453->$0;
        int32_t _M0L6_2atmpS2098 = _M0L3valS2099 + 2;
        int32_t _M0L6_2atmpS2101;
        int32_t _M0L6_2atmpS2102;
        int32_t _M0L6_2atmpS2100;
        _M0L5indexS453->$0 = _M0L6_2atmpS2098;
        moonbit_decref(_M0L5indexS453);
        _M0L6_2atmpS2101 = (int32_t)_M0L2c1S454;
        _M0L6_2atmpS2102 = (int32_t)_M0L2c2S455;
        #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        _M0L6_2atmpS2100
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2101, _M0L6_2atmpS2102);
        return _M0L6_2atmpS2100;
      }
    } else {
      moonbit_decref(_M0L4selfS451.$0);
    }
    _M0L3valS2107 = _M0L5indexS453->$0;
    _M0L6_2atmpS2106 = _M0L3valS2107 + 1;
    _M0L5indexS453->$0 = _M0L6_2atmpS2106;
    moonbit_decref(_M0L5indexS453);
    #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L6_2atmpS2108 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S454);
    return _M0L6_2atmpS2108;
  } else {
    moonbit_decref(_M0L5indexS453);
    moonbit_decref(_M0L4selfS451.$0);
    return -1;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS449
) {
  moonbit_string_t _M0L8_2afieldS3382;
  moonbit_string_t _M0L3strS2087;
  int32_t _M0L5startS2088;
  int32_t _M0L8_2afieldS3381;
  int32_t _M0L3endS2089;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3382 = _M0L4selfS449.$0;
  _M0L3strS2087 = _M0L8_2afieldS3382;
  _M0L5startS2088 = _M0L4selfS449.$1;
  _M0L8_2afieldS3381 = _M0L4selfS449.$2;
  _M0L3endS2089 = _M0L8_2afieldS3381;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2087, _M0L5startS2088, _M0L3endS2089);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS447,
  struct _M0TPB6Logger _M0L6loggerS448
) {
  moonbit_string_t _M0L8_2afieldS3384;
  moonbit_string_t _M0L3strS2084;
  int32_t _M0L5startS2085;
  int32_t _M0L8_2afieldS3383;
  int32_t _M0L3endS2086;
  moonbit_string_t _M0L6substrS446;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3384 = _M0L4selfS447.$0;
  _M0L3strS2084 = _M0L8_2afieldS3384;
  _M0L5startS2085 = _M0L4selfS447.$1;
  _M0L8_2afieldS3383 = _M0L4selfS447.$2;
  _M0L3endS2086 = _M0L8_2afieldS3383;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS446
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2084, _M0L5startS2085, _M0L3endS2086);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS446, _M0L6loggerS448);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS438,
  struct _M0TPB6Logger _M0L6loggerS436
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS437;
  int32_t _M0L3lenS439;
  int32_t _M0L1iS440;
  int32_t _M0L3segS441;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS436.$1) {
    moonbit_incref(_M0L6loggerS436.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS436.$0->$method_3(_M0L6loggerS436.$1, 34);
  moonbit_incref(_M0L4selfS438);
  if (_M0L6loggerS436.$1) {
    moonbit_incref(_M0L6loggerS436.$1);
  }
  _M0L6_2aenvS437
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS437)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS437->$0 = _M0L4selfS438;
  _M0L6_2aenvS437->$1_0 = _M0L6loggerS436.$0;
  _M0L6_2aenvS437->$1_1 = _M0L6loggerS436.$1;
  _M0L3lenS439 = Moonbit_array_length(_M0L4selfS438);
  _M0L1iS440 = 0;
  _M0L3segS441 = 0;
  _2afor_442:;
  while (1) {
    int32_t _M0L4codeS443;
    int32_t _M0L1cS445;
    int32_t _M0L6_2atmpS2068;
    int32_t _M0L6_2atmpS2069;
    int32_t _M0L6_2atmpS2070;
    int32_t _tmp_3680;
    int32_t _tmp_3681;
    if (_M0L1iS440 >= _M0L3lenS439) {
      moonbit_decref(_M0L4selfS438);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS437, _M0L3segS441, _M0L1iS440);
      break;
    }
    _M0L4codeS443 = _M0L4selfS438[_M0L1iS440];
    switch (_M0L4codeS443) {
      case 34: {
        _M0L1cS445 = _M0L4codeS443;
        goto join_444;
        break;
      }
      
      case 92: {
        _M0L1cS445 = _M0L4codeS443;
        goto join_444;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2071;
        int32_t _M0L6_2atmpS2072;
        moonbit_incref(_M0L6_2aenvS437);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS437, _M0L3segS441, _M0L1iS440);
        if (_M0L6loggerS436.$1) {
          moonbit_incref(_M0L6loggerS436.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS436.$0->$method_0(_M0L6loggerS436.$1, (moonbit_string_t)moonbit_string_literal_40.data);
        _M0L6_2atmpS2071 = _M0L1iS440 + 1;
        _M0L6_2atmpS2072 = _M0L1iS440 + 1;
        _M0L1iS440 = _M0L6_2atmpS2071;
        _M0L3segS441 = _M0L6_2atmpS2072;
        goto _2afor_442;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2073;
        int32_t _M0L6_2atmpS2074;
        moonbit_incref(_M0L6_2aenvS437);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS437, _M0L3segS441, _M0L1iS440);
        if (_M0L6loggerS436.$1) {
          moonbit_incref(_M0L6loggerS436.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS436.$0->$method_0(_M0L6loggerS436.$1, (moonbit_string_t)moonbit_string_literal_41.data);
        _M0L6_2atmpS2073 = _M0L1iS440 + 1;
        _M0L6_2atmpS2074 = _M0L1iS440 + 1;
        _M0L1iS440 = _M0L6_2atmpS2073;
        _M0L3segS441 = _M0L6_2atmpS2074;
        goto _2afor_442;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2075;
        int32_t _M0L6_2atmpS2076;
        moonbit_incref(_M0L6_2aenvS437);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS437, _M0L3segS441, _M0L1iS440);
        if (_M0L6loggerS436.$1) {
          moonbit_incref(_M0L6loggerS436.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS436.$0->$method_0(_M0L6loggerS436.$1, (moonbit_string_t)moonbit_string_literal_42.data);
        _M0L6_2atmpS2075 = _M0L1iS440 + 1;
        _M0L6_2atmpS2076 = _M0L1iS440 + 1;
        _M0L1iS440 = _M0L6_2atmpS2075;
        _M0L3segS441 = _M0L6_2atmpS2076;
        goto _2afor_442;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2077;
        int32_t _M0L6_2atmpS2078;
        moonbit_incref(_M0L6_2aenvS437);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS437, _M0L3segS441, _M0L1iS440);
        if (_M0L6loggerS436.$1) {
          moonbit_incref(_M0L6loggerS436.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS436.$0->$method_0(_M0L6loggerS436.$1, (moonbit_string_t)moonbit_string_literal_43.data);
        _M0L6_2atmpS2077 = _M0L1iS440 + 1;
        _M0L6_2atmpS2078 = _M0L1iS440 + 1;
        _M0L1iS440 = _M0L6_2atmpS2077;
        _M0L3segS441 = _M0L6_2atmpS2078;
        goto _2afor_442;
        break;
      }
      default: {
        if (_M0L4codeS443 < 32) {
          int32_t _M0L6_2atmpS2080;
          moonbit_string_t _M0L6_2atmpS2079;
          int32_t _M0L6_2atmpS2081;
          int32_t _M0L6_2atmpS2082;
          moonbit_incref(_M0L6_2aenvS437);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS437, _M0L3segS441, _M0L1iS440);
          if (_M0L6loggerS436.$1) {
            moonbit_incref(_M0L6loggerS436.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS436.$0->$method_0(_M0L6loggerS436.$1, (moonbit_string_t)moonbit_string_literal_60.data);
          _M0L6_2atmpS2080 = _M0L4codeS443 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2079 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2080);
          if (_M0L6loggerS436.$1) {
            moonbit_incref(_M0L6loggerS436.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS436.$0->$method_0(_M0L6loggerS436.$1, _M0L6_2atmpS2079);
          if (_M0L6loggerS436.$1) {
            moonbit_incref(_M0L6loggerS436.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS436.$0->$method_3(_M0L6loggerS436.$1, 125);
          _M0L6_2atmpS2081 = _M0L1iS440 + 1;
          _M0L6_2atmpS2082 = _M0L1iS440 + 1;
          _M0L1iS440 = _M0L6_2atmpS2081;
          _M0L3segS441 = _M0L6_2atmpS2082;
          goto _2afor_442;
        } else {
          int32_t _M0L6_2atmpS2083 = _M0L1iS440 + 1;
          int32_t _tmp_3679 = _M0L3segS441;
          _M0L1iS440 = _M0L6_2atmpS2083;
          _M0L3segS441 = _tmp_3679;
          goto _2afor_442;
        }
        break;
      }
    }
    goto joinlet_3678;
    join_444:;
    moonbit_incref(_M0L6_2aenvS437);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS437, _M0L3segS441, _M0L1iS440);
    if (_M0L6loggerS436.$1) {
      moonbit_incref(_M0L6loggerS436.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS436.$0->$method_3(_M0L6loggerS436.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2068 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS445);
    if (_M0L6loggerS436.$1) {
      moonbit_incref(_M0L6loggerS436.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS436.$0->$method_3(_M0L6loggerS436.$1, _M0L6_2atmpS2068);
    _M0L6_2atmpS2069 = _M0L1iS440 + 1;
    _M0L6_2atmpS2070 = _M0L1iS440 + 1;
    _M0L1iS440 = _M0L6_2atmpS2069;
    _M0L3segS441 = _M0L6_2atmpS2070;
    continue;
    joinlet_3678:;
    _tmp_3680 = _M0L1iS440;
    _tmp_3681 = _M0L3segS441;
    _M0L1iS440 = _tmp_3680;
    _M0L3segS441 = _tmp_3681;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS436.$0->$method_3(_M0L6loggerS436.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS432,
  int32_t _M0L3segS435,
  int32_t _M0L1iS434
) {
  struct _M0TPB6Logger _M0L8_2afieldS3386;
  struct _M0TPB6Logger _M0L6loggerS431;
  moonbit_string_t _M0L8_2afieldS3385;
  int32_t _M0L6_2acntS3535;
  moonbit_string_t _M0L4selfS433;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3386
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS432->$1_0, _M0L6_2aenvS432->$1_1
  };
  _M0L6loggerS431 = _M0L8_2afieldS3386;
  _M0L8_2afieldS3385 = _M0L6_2aenvS432->$0;
  _M0L6_2acntS3535 = Moonbit_object_header(_M0L6_2aenvS432)->rc;
  if (_M0L6_2acntS3535 > 1) {
    int32_t _M0L11_2anew__cntS3536 = _M0L6_2acntS3535 - 1;
    Moonbit_object_header(_M0L6_2aenvS432)->rc = _M0L11_2anew__cntS3536;
    if (_M0L6loggerS431.$1) {
      moonbit_incref(_M0L6loggerS431.$1);
    }
    moonbit_incref(_M0L8_2afieldS3385);
  } else if (_M0L6_2acntS3535 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS432);
  }
  _M0L4selfS433 = _M0L8_2afieldS3385;
  if (_M0L1iS434 > _M0L3segS435) {
    int32_t _M0L6_2atmpS2067 = _M0L1iS434 - _M0L3segS435;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS431.$0->$method_1(_M0L6loggerS431.$1, _M0L4selfS433, _M0L3segS435, _M0L6_2atmpS2067);
  } else {
    moonbit_decref(_M0L4selfS433);
    if (_M0L6loggerS431.$1) {
      moonbit_decref(_M0L6loggerS431.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS430) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS429;
  int32_t _M0L6_2atmpS2064;
  int32_t _M0L6_2atmpS2063;
  int32_t _M0L6_2atmpS2066;
  int32_t _M0L6_2atmpS2065;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2062;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS429 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2064 = _M0IPC14byte4BytePB3Div3div(_M0L1bS430, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2063
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2064);
  moonbit_incref(_M0L7_2aselfS429);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS429, _M0L6_2atmpS2063);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2066 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS430, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2065
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2066);
  moonbit_incref(_M0L7_2aselfS429);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS429, _M0L6_2atmpS2065);
  _M0L6_2atmpS2062 = _M0L7_2aselfS429;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2062);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS428) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS428 < 10) {
    int32_t _M0L6_2atmpS2059;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2059 = _M0IPC14byte4BytePB3Add3add(_M0L1iS428, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2059);
  } else {
    int32_t _M0L6_2atmpS2061;
    int32_t _M0L6_2atmpS2060;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2061 = _M0IPC14byte4BytePB3Add3add(_M0L1iS428, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2060 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2061, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2060);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS426,
  int32_t _M0L4thatS427
) {
  int32_t _M0L6_2atmpS2057;
  int32_t _M0L6_2atmpS2058;
  int32_t _M0L6_2atmpS2056;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2057 = (int32_t)_M0L4selfS426;
  _M0L6_2atmpS2058 = (int32_t)_M0L4thatS427;
  _M0L6_2atmpS2056 = _M0L6_2atmpS2057 - _M0L6_2atmpS2058;
  return _M0L6_2atmpS2056 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS424,
  int32_t _M0L4thatS425
) {
  int32_t _M0L6_2atmpS2054;
  int32_t _M0L6_2atmpS2055;
  int32_t _M0L6_2atmpS2053;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2054 = (int32_t)_M0L4selfS424;
  _M0L6_2atmpS2055 = (int32_t)_M0L4thatS425;
  _M0L6_2atmpS2053 = _M0L6_2atmpS2054 % _M0L6_2atmpS2055;
  return _M0L6_2atmpS2053 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS422,
  int32_t _M0L4thatS423
) {
  int32_t _M0L6_2atmpS2051;
  int32_t _M0L6_2atmpS2052;
  int32_t _M0L6_2atmpS2050;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2051 = (int32_t)_M0L4selfS422;
  _M0L6_2atmpS2052 = (int32_t)_M0L4thatS423;
  _M0L6_2atmpS2050 = _M0L6_2atmpS2051 / _M0L6_2atmpS2052;
  return _M0L6_2atmpS2050 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS420,
  int32_t _M0L4thatS421
) {
  int32_t _M0L6_2atmpS2048;
  int32_t _M0L6_2atmpS2049;
  int32_t _M0L6_2atmpS2047;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2048 = (int32_t)_M0L4selfS420;
  _M0L6_2atmpS2049 = (int32_t)_M0L4thatS421;
  _M0L6_2atmpS2047 = _M0L6_2atmpS2048 + _M0L6_2atmpS2049;
  return _M0L6_2atmpS2047 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS417,
  int32_t _M0L5startS415,
  int32_t _M0L3endS416
) {
  int32_t _if__result_3682;
  int32_t _M0L3lenS418;
  int32_t _M0L6_2atmpS2045;
  int32_t _M0L6_2atmpS2046;
  moonbit_bytes_t _M0L5bytesS419;
  moonbit_bytes_t _M0L6_2atmpS2044;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS415 == 0) {
    int32_t _M0L6_2atmpS2043 = Moonbit_array_length(_M0L3strS417);
    _if__result_3682 = _M0L3endS416 == _M0L6_2atmpS2043;
  } else {
    _if__result_3682 = 0;
  }
  if (_if__result_3682) {
    return _M0L3strS417;
  }
  _M0L3lenS418 = _M0L3endS416 - _M0L5startS415;
  _M0L6_2atmpS2045 = _M0L3lenS418 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2046 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS419
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2045, _M0L6_2atmpS2046);
  moonbit_incref(_M0L5bytesS419);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS419, 0, _M0L3strS417, _M0L5startS415, _M0L3lenS418);
  _M0L6_2atmpS2044 = _M0L5bytesS419;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2044, 0, 4294967296ll);
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView _M0L4selfS413,
  int32_t _M0L13start__offsetS414,
  int64_t _M0L11end__offsetS411
) {
  int32_t _M0L11end__offsetS410;
  int32_t _if__result_3683;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS411 == 4294967296ll) {
    moonbit_incref(_M0L4selfS413.$0);
    #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L11end__offsetS410 = _M0MPC16string10StringView6length(_M0L4selfS413);
  } else {
    int64_t _M0L7_2aSomeS412 = _M0L11end__offsetS411;
    _M0L11end__offsetS410 = (int32_t)_M0L7_2aSomeS412;
  }
  if (_M0L13start__offsetS414 >= 0) {
    if (_M0L13start__offsetS414 <= _M0L11end__offsetS410) {
      int32_t _M0L6_2atmpS2037;
      moonbit_incref(_M0L4selfS413.$0);
      #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2037 = _M0MPC16string10StringView6length(_M0L4selfS413);
      _if__result_3683 = _M0L11end__offsetS410 <= _M0L6_2atmpS2037;
    } else {
      _if__result_3683 = 0;
    }
  } else {
    _if__result_3683 = 0;
  }
  if (_if__result_3683) {
    moonbit_string_t _M0L8_2afieldS3388 = _M0L4selfS413.$0;
    moonbit_string_t _M0L3strS2038 = _M0L8_2afieldS3388;
    int32_t _M0L5startS2042 = _M0L4selfS413.$1;
    int32_t _M0L6_2atmpS2039 = _M0L5startS2042 + _M0L13start__offsetS414;
    int32_t _M0L8_2afieldS3387 = _M0L4selfS413.$1;
    int32_t _M0L5startS2041 = _M0L8_2afieldS3387;
    int32_t _M0L6_2atmpS2040 = _M0L5startS2041 + _M0L11end__offsetS410;
    return (struct _M0TPC16string10StringView){_M0L6_2atmpS2039,
                                                 _M0L6_2atmpS2040,
                                                 _M0L3strS2038};
  } else {
    moonbit_decref(_M0L4selfS413.$0);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_58.data, (moonbit_string_t)moonbit_string_literal_61.data);
  }
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS406) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS406;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L1fS407
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS407;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS408
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS408;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS409) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS409;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS404,
  int32_t _M0L5indexS405
) {
  moonbit_string_t _M0L8_2afieldS3391;
  moonbit_string_t _M0L3strS2034;
  int32_t _M0L8_2afieldS3390;
  int32_t _M0L5startS2036;
  int32_t _M0L6_2atmpS2035;
  int32_t _M0L6_2atmpS3389;
  #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3391 = _M0L4selfS404.$0;
  _M0L3strS2034 = _M0L8_2afieldS3391;
  _M0L8_2afieldS3390 = _M0L4selfS404.$1;
  _M0L5startS2036 = _M0L8_2afieldS3390;
  _M0L6_2atmpS2035 = _M0L5startS2036 + _M0L5indexS405;
  _M0L6_2atmpS3389 = _M0L3strS2034[_M0L6_2atmpS2035];
  moonbit_decref(_M0L3strS2034);
  return _M0L6_2atmpS3389;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS396,
  int32_t _M0L5radixS395
) {
  int32_t _if__result_3684;
  uint16_t* _M0L6bufferS397;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS395 < 2) {
    _if__result_3684 = 1;
  } else {
    _if__result_3684 = _M0L5radixS395 > 36;
  }
  if (_if__result_3684) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_62.data, (moonbit_string_t)moonbit_string_literal_63.data);
  }
  if (_M0L4selfS396 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_46.data;
  }
  switch (_M0L5radixS395) {
    case 10: {
      int32_t _M0L3lenS398;
      uint16_t* _M0L6bufferS399;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS398 = _M0FPB12dec__count64(_M0L4selfS396);
      _M0L6bufferS399 = (uint16_t*)moonbit_make_string(_M0L3lenS398, 0);
      moonbit_incref(_M0L6bufferS399);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS399, _M0L4selfS396, 0, _M0L3lenS398);
      _M0L6bufferS397 = _M0L6bufferS399;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS400;
      uint16_t* _M0L6bufferS401;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS400 = _M0FPB12hex__count64(_M0L4selfS396);
      _M0L6bufferS401 = (uint16_t*)moonbit_make_string(_M0L3lenS400, 0);
      moonbit_incref(_M0L6bufferS401);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS401, _M0L4selfS396, 0, _M0L3lenS400);
      _M0L6bufferS397 = _M0L6bufferS401;
      break;
    }
    default: {
      int32_t _M0L3lenS402;
      uint16_t* _M0L6bufferS403;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS402 = _M0FPB14radix__count64(_M0L4selfS396, _M0L5radixS395);
      _M0L6bufferS403 = (uint16_t*)moonbit_make_string(_M0L3lenS402, 0);
      moonbit_incref(_M0L6bufferS403);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS403, _M0L4selfS396, 0, _M0L3lenS402, _M0L5radixS395);
      _M0L6bufferS397 = _M0L6bufferS403;
      break;
    }
  }
  return _M0L6bufferS397;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS385,
  uint64_t _M0L3numS373,
  int32_t _M0L12digit__startS376,
  int32_t _M0L10total__lenS375
) {
  uint64_t _M0Lm3numS372;
  int32_t _M0Lm6offsetS374;
  uint64_t _M0L6_2atmpS2033;
  int32_t _M0Lm9remainingS387;
  int32_t _M0L6_2atmpS2014;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS372 = _M0L3numS373;
  _M0Lm6offsetS374 = _M0L10total__lenS375 - _M0L12digit__startS376;
  while (1) {
    uint64_t _M0L6_2atmpS1977 = _M0Lm3numS372;
    if (_M0L6_2atmpS1977 >= 10000ull) {
      uint64_t _M0L6_2atmpS2000 = _M0Lm3numS372;
      uint64_t _M0L1tS377 = _M0L6_2atmpS2000 / 10000ull;
      uint64_t _M0L6_2atmpS1999 = _M0Lm3numS372;
      uint64_t _M0L6_2atmpS1998 = _M0L6_2atmpS1999 % 10000ull;
      int32_t _M0L1rS378 = (int32_t)_M0L6_2atmpS1998;
      int32_t _M0L2d1S379;
      int32_t _M0L2d2S380;
      int32_t _M0L6_2atmpS1978;
      int32_t _M0L6_2atmpS1997;
      int32_t _M0L6_2atmpS1996;
      int32_t _M0L6d1__hiS381;
      int32_t _M0L6_2atmpS1995;
      int32_t _M0L6_2atmpS1994;
      int32_t _M0L6d1__loS382;
      int32_t _M0L6_2atmpS1993;
      int32_t _M0L6_2atmpS1992;
      int32_t _M0L6d2__hiS383;
      int32_t _M0L6_2atmpS1991;
      int32_t _M0L6_2atmpS1990;
      int32_t _M0L6d2__loS384;
      int32_t _M0L6_2atmpS1980;
      int32_t _M0L6_2atmpS1979;
      int32_t _M0L6_2atmpS1983;
      int32_t _M0L6_2atmpS1982;
      int32_t _M0L6_2atmpS1981;
      int32_t _M0L6_2atmpS1986;
      int32_t _M0L6_2atmpS1985;
      int32_t _M0L6_2atmpS1984;
      int32_t _M0L6_2atmpS1989;
      int32_t _M0L6_2atmpS1988;
      int32_t _M0L6_2atmpS1987;
      _M0Lm3numS372 = _M0L1tS377;
      _M0L2d1S379 = _M0L1rS378 / 100;
      _M0L2d2S380 = _M0L1rS378 % 100;
      _M0L6_2atmpS1978 = _M0Lm6offsetS374;
      _M0Lm6offsetS374 = _M0L6_2atmpS1978 - 4;
      _M0L6_2atmpS1997 = _M0L2d1S379 / 10;
      _M0L6_2atmpS1996 = 48 + _M0L6_2atmpS1997;
      _M0L6d1__hiS381 = (uint16_t)_M0L6_2atmpS1996;
      _M0L6_2atmpS1995 = _M0L2d1S379 % 10;
      _M0L6_2atmpS1994 = 48 + _M0L6_2atmpS1995;
      _M0L6d1__loS382 = (uint16_t)_M0L6_2atmpS1994;
      _M0L6_2atmpS1993 = _M0L2d2S380 / 10;
      _M0L6_2atmpS1992 = 48 + _M0L6_2atmpS1993;
      _M0L6d2__hiS383 = (uint16_t)_M0L6_2atmpS1992;
      _M0L6_2atmpS1991 = _M0L2d2S380 % 10;
      _M0L6_2atmpS1990 = 48 + _M0L6_2atmpS1991;
      _M0L6d2__loS384 = (uint16_t)_M0L6_2atmpS1990;
      _M0L6_2atmpS1980 = _M0Lm6offsetS374;
      _M0L6_2atmpS1979 = _M0L12digit__startS376 + _M0L6_2atmpS1980;
      _M0L6bufferS385[_M0L6_2atmpS1979] = _M0L6d1__hiS381;
      _M0L6_2atmpS1983 = _M0Lm6offsetS374;
      _M0L6_2atmpS1982 = _M0L12digit__startS376 + _M0L6_2atmpS1983;
      _M0L6_2atmpS1981 = _M0L6_2atmpS1982 + 1;
      _M0L6bufferS385[_M0L6_2atmpS1981] = _M0L6d1__loS382;
      _M0L6_2atmpS1986 = _M0Lm6offsetS374;
      _M0L6_2atmpS1985 = _M0L12digit__startS376 + _M0L6_2atmpS1986;
      _M0L6_2atmpS1984 = _M0L6_2atmpS1985 + 2;
      _M0L6bufferS385[_M0L6_2atmpS1984] = _M0L6d2__hiS383;
      _M0L6_2atmpS1989 = _M0Lm6offsetS374;
      _M0L6_2atmpS1988 = _M0L12digit__startS376 + _M0L6_2atmpS1989;
      _M0L6_2atmpS1987 = _M0L6_2atmpS1988 + 3;
      _M0L6bufferS385[_M0L6_2atmpS1987] = _M0L6d2__loS384;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2033 = _M0Lm3numS372;
  _M0Lm9remainingS387 = (int32_t)_M0L6_2atmpS2033;
  while (1) {
    int32_t _M0L6_2atmpS2001 = _M0Lm9remainingS387;
    if (_M0L6_2atmpS2001 >= 100) {
      int32_t _M0L6_2atmpS2013 = _M0Lm9remainingS387;
      int32_t _M0L1tS388 = _M0L6_2atmpS2013 / 100;
      int32_t _M0L6_2atmpS2012 = _M0Lm9remainingS387;
      int32_t _M0L1dS389 = _M0L6_2atmpS2012 % 100;
      int32_t _M0L6_2atmpS2002;
      int32_t _M0L6_2atmpS2011;
      int32_t _M0L6_2atmpS2010;
      int32_t _M0L5d__hiS390;
      int32_t _M0L6_2atmpS2009;
      int32_t _M0L6_2atmpS2008;
      int32_t _M0L5d__loS391;
      int32_t _M0L6_2atmpS2004;
      int32_t _M0L6_2atmpS2003;
      int32_t _M0L6_2atmpS2007;
      int32_t _M0L6_2atmpS2006;
      int32_t _M0L6_2atmpS2005;
      _M0Lm9remainingS387 = _M0L1tS388;
      _M0L6_2atmpS2002 = _M0Lm6offsetS374;
      _M0Lm6offsetS374 = _M0L6_2atmpS2002 - 2;
      _M0L6_2atmpS2011 = _M0L1dS389 / 10;
      _M0L6_2atmpS2010 = 48 + _M0L6_2atmpS2011;
      _M0L5d__hiS390 = (uint16_t)_M0L6_2atmpS2010;
      _M0L6_2atmpS2009 = _M0L1dS389 % 10;
      _M0L6_2atmpS2008 = 48 + _M0L6_2atmpS2009;
      _M0L5d__loS391 = (uint16_t)_M0L6_2atmpS2008;
      _M0L6_2atmpS2004 = _M0Lm6offsetS374;
      _M0L6_2atmpS2003 = _M0L12digit__startS376 + _M0L6_2atmpS2004;
      _M0L6bufferS385[_M0L6_2atmpS2003] = _M0L5d__hiS390;
      _M0L6_2atmpS2007 = _M0Lm6offsetS374;
      _M0L6_2atmpS2006 = _M0L12digit__startS376 + _M0L6_2atmpS2007;
      _M0L6_2atmpS2005 = _M0L6_2atmpS2006 + 1;
      _M0L6bufferS385[_M0L6_2atmpS2005] = _M0L5d__loS391;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2014 = _M0Lm9remainingS387;
  if (_M0L6_2atmpS2014 >= 10) {
    int32_t _M0L6_2atmpS2015 = _M0Lm6offsetS374;
    int32_t _M0L6_2atmpS2026;
    int32_t _M0L6_2atmpS2025;
    int32_t _M0L6_2atmpS2024;
    int32_t _M0L5d__hiS393;
    int32_t _M0L6_2atmpS2023;
    int32_t _M0L6_2atmpS2022;
    int32_t _M0L6_2atmpS2021;
    int32_t _M0L5d__loS394;
    int32_t _M0L6_2atmpS2017;
    int32_t _M0L6_2atmpS2016;
    int32_t _M0L6_2atmpS2020;
    int32_t _M0L6_2atmpS2019;
    int32_t _M0L6_2atmpS2018;
    _M0Lm6offsetS374 = _M0L6_2atmpS2015 - 2;
    _M0L6_2atmpS2026 = _M0Lm9remainingS387;
    _M0L6_2atmpS2025 = _M0L6_2atmpS2026 / 10;
    _M0L6_2atmpS2024 = 48 + _M0L6_2atmpS2025;
    _M0L5d__hiS393 = (uint16_t)_M0L6_2atmpS2024;
    _M0L6_2atmpS2023 = _M0Lm9remainingS387;
    _M0L6_2atmpS2022 = _M0L6_2atmpS2023 % 10;
    _M0L6_2atmpS2021 = 48 + _M0L6_2atmpS2022;
    _M0L5d__loS394 = (uint16_t)_M0L6_2atmpS2021;
    _M0L6_2atmpS2017 = _M0Lm6offsetS374;
    _M0L6_2atmpS2016 = _M0L12digit__startS376 + _M0L6_2atmpS2017;
    _M0L6bufferS385[_M0L6_2atmpS2016] = _M0L5d__hiS393;
    _M0L6_2atmpS2020 = _M0Lm6offsetS374;
    _M0L6_2atmpS2019 = _M0L12digit__startS376 + _M0L6_2atmpS2020;
    _M0L6_2atmpS2018 = _M0L6_2atmpS2019 + 1;
    _M0L6bufferS385[_M0L6_2atmpS2018] = _M0L5d__loS394;
    moonbit_decref(_M0L6bufferS385);
  } else {
    int32_t _M0L6_2atmpS2027 = _M0Lm6offsetS374;
    int32_t _M0L6_2atmpS2032;
    int32_t _M0L6_2atmpS2028;
    int32_t _M0L6_2atmpS2031;
    int32_t _M0L6_2atmpS2030;
    int32_t _M0L6_2atmpS2029;
    _M0Lm6offsetS374 = _M0L6_2atmpS2027 - 1;
    _M0L6_2atmpS2032 = _M0Lm6offsetS374;
    _M0L6_2atmpS2028 = _M0L12digit__startS376 + _M0L6_2atmpS2032;
    _M0L6_2atmpS2031 = _M0Lm9remainingS387;
    _M0L6_2atmpS2030 = 48 + _M0L6_2atmpS2031;
    _M0L6_2atmpS2029 = (uint16_t)_M0L6_2atmpS2030;
    _M0L6bufferS385[_M0L6_2atmpS2028] = _M0L6_2atmpS2029;
    moonbit_decref(_M0L6bufferS385);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS367,
  uint64_t _M0L3numS361,
  int32_t _M0L12digit__startS359,
  int32_t _M0L10total__lenS358,
  int32_t _M0L5radixS363
) {
  int32_t _M0Lm6offsetS357;
  uint64_t _M0Lm1nS360;
  uint64_t _M0L4baseS362;
  int32_t _M0L6_2atmpS1959;
  int32_t _M0L6_2atmpS1958;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS357 = _M0L10total__lenS358 - _M0L12digit__startS359;
  _M0Lm1nS360 = _M0L3numS361;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS362 = _M0MPC13int3Int10to__uint64(_M0L5radixS363);
  _M0L6_2atmpS1959 = _M0L5radixS363 - 1;
  _M0L6_2atmpS1958 = _M0L5radixS363 & _M0L6_2atmpS1959;
  if (_M0L6_2atmpS1958 == 0) {
    int32_t _M0L5shiftS364;
    uint64_t _M0L4maskS365;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS364 = moonbit_ctz32(_M0L5radixS363);
    _M0L4maskS365 = _M0L4baseS362 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1960 = _M0Lm1nS360;
      if (_M0L6_2atmpS1960 > 0ull) {
        int32_t _M0L6_2atmpS1961 = _M0Lm6offsetS357;
        uint64_t _M0L6_2atmpS1967;
        uint64_t _M0L6_2atmpS1966;
        int32_t _M0L5digitS366;
        int32_t _M0L6_2atmpS1964;
        int32_t _M0L6_2atmpS1962;
        int32_t _M0L6_2atmpS1963;
        uint64_t _M0L6_2atmpS1965;
        _M0Lm6offsetS357 = _M0L6_2atmpS1961 - 1;
        _M0L6_2atmpS1967 = _M0Lm1nS360;
        _M0L6_2atmpS1966 = _M0L6_2atmpS1967 & _M0L4maskS365;
        _M0L5digitS366 = (int32_t)_M0L6_2atmpS1966;
        _M0L6_2atmpS1964 = _M0Lm6offsetS357;
        _M0L6_2atmpS1962 = _M0L12digit__startS359 + _M0L6_2atmpS1964;
        _M0L6_2atmpS1963
        = ((moonbit_string_t)moonbit_string_literal_64.data)[
          _M0L5digitS366
        ];
        _M0L6bufferS367[_M0L6_2atmpS1962] = _M0L6_2atmpS1963;
        _M0L6_2atmpS1965 = _M0Lm1nS360;
        _M0Lm1nS360 = _M0L6_2atmpS1965 >> (_M0L5shiftS364 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS367);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1968 = _M0Lm1nS360;
      if (_M0L6_2atmpS1968 > 0ull) {
        int32_t _M0L6_2atmpS1969 = _M0Lm6offsetS357;
        uint64_t _M0L6_2atmpS1976;
        uint64_t _M0L1qS369;
        uint64_t _M0L6_2atmpS1974;
        uint64_t _M0L6_2atmpS1975;
        uint64_t _M0L6_2atmpS1973;
        int32_t _M0L5digitS370;
        int32_t _M0L6_2atmpS1972;
        int32_t _M0L6_2atmpS1970;
        int32_t _M0L6_2atmpS1971;
        _M0Lm6offsetS357 = _M0L6_2atmpS1969 - 1;
        _M0L6_2atmpS1976 = _M0Lm1nS360;
        _M0L1qS369 = _M0L6_2atmpS1976 / _M0L4baseS362;
        _M0L6_2atmpS1974 = _M0Lm1nS360;
        _M0L6_2atmpS1975 = _M0L1qS369 * _M0L4baseS362;
        _M0L6_2atmpS1973 = _M0L6_2atmpS1974 - _M0L6_2atmpS1975;
        _M0L5digitS370 = (int32_t)_M0L6_2atmpS1973;
        _M0L6_2atmpS1972 = _M0Lm6offsetS357;
        _M0L6_2atmpS1970 = _M0L12digit__startS359 + _M0L6_2atmpS1972;
        _M0L6_2atmpS1971
        = ((moonbit_string_t)moonbit_string_literal_64.data)[
          _M0L5digitS370
        ];
        _M0L6bufferS367[_M0L6_2atmpS1970] = _M0L6_2atmpS1971;
        _M0Lm1nS360 = _M0L1qS369;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS367);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS354,
  uint64_t _M0L3numS350,
  int32_t _M0L12digit__startS348,
  int32_t _M0L10total__lenS347
) {
  int32_t _M0Lm6offsetS346;
  uint64_t _M0Lm1nS349;
  int32_t _M0L6_2atmpS1954;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS346 = _M0L10total__lenS347 - _M0L12digit__startS348;
  _M0Lm1nS349 = _M0L3numS350;
  while (1) {
    int32_t _M0L6_2atmpS1942 = _M0Lm6offsetS346;
    if (_M0L6_2atmpS1942 >= 2) {
      int32_t _M0L6_2atmpS1943 = _M0Lm6offsetS346;
      uint64_t _M0L6_2atmpS1953;
      uint64_t _M0L6_2atmpS1952;
      int32_t _M0L9byte__valS351;
      int32_t _M0L2hiS352;
      int32_t _M0L2loS353;
      int32_t _M0L6_2atmpS1946;
      int32_t _M0L6_2atmpS1944;
      int32_t _M0L6_2atmpS1945;
      int32_t _M0L6_2atmpS1950;
      int32_t _M0L6_2atmpS1949;
      int32_t _M0L6_2atmpS1947;
      int32_t _M0L6_2atmpS1948;
      uint64_t _M0L6_2atmpS1951;
      _M0Lm6offsetS346 = _M0L6_2atmpS1943 - 2;
      _M0L6_2atmpS1953 = _M0Lm1nS349;
      _M0L6_2atmpS1952 = _M0L6_2atmpS1953 & 255ull;
      _M0L9byte__valS351 = (int32_t)_M0L6_2atmpS1952;
      _M0L2hiS352 = _M0L9byte__valS351 / 16;
      _M0L2loS353 = _M0L9byte__valS351 % 16;
      _M0L6_2atmpS1946 = _M0Lm6offsetS346;
      _M0L6_2atmpS1944 = _M0L12digit__startS348 + _M0L6_2atmpS1946;
      _M0L6_2atmpS1945
      = ((moonbit_string_t)moonbit_string_literal_64.data)[
        _M0L2hiS352
      ];
      _M0L6bufferS354[_M0L6_2atmpS1944] = _M0L6_2atmpS1945;
      _M0L6_2atmpS1950 = _M0Lm6offsetS346;
      _M0L6_2atmpS1949 = _M0L12digit__startS348 + _M0L6_2atmpS1950;
      _M0L6_2atmpS1947 = _M0L6_2atmpS1949 + 1;
      _M0L6_2atmpS1948
      = ((moonbit_string_t)moonbit_string_literal_64.data)[
        _M0L2loS353
      ];
      _M0L6bufferS354[_M0L6_2atmpS1947] = _M0L6_2atmpS1948;
      _M0L6_2atmpS1951 = _M0Lm1nS349;
      _M0Lm1nS349 = _M0L6_2atmpS1951 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1954 = _M0Lm6offsetS346;
  if (_M0L6_2atmpS1954 == 1) {
    uint64_t _M0L6_2atmpS1957 = _M0Lm1nS349;
    uint64_t _M0L6_2atmpS1956 = _M0L6_2atmpS1957 & 15ull;
    int32_t _M0L6nibbleS356 = (int32_t)_M0L6_2atmpS1956;
    int32_t _M0L6_2atmpS1955 =
      ((moonbit_string_t)moonbit_string_literal_64.data)[_M0L6nibbleS356];
    _M0L6bufferS354[_M0L12digit__startS348] = _M0L6_2atmpS1955;
    moonbit_decref(_M0L6bufferS354);
  } else {
    moonbit_decref(_M0L6bufferS354);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS340,
  int32_t _M0L5radixS343
) {
  uint64_t _M0Lm3numS341;
  uint64_t _M0L4baseS342;
  int32_t _M0Lm5countS344;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS340 == 0ull) {
    return 1;
  }
  _M0Lm3numS341 = _M0L5valueS340;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS342 = _M0MPC13int3Int10to__uint64(_M0L5radixS343);
  _M0Lm5countS344 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1939 = _M0Lm3numS341;
    if (_M0L6_2atmpS1939 > 0ull) {
      int32_t _M0L6_2atmpS1940 = _M0Lm5countS344;
      uint64_t _M0L6_2atmpS1941;
      _M0Lm5countS344 = _M0L6_2atmpS1940 + 1;
      _M0L6_2atmpS1941 = _M0Lm3numS341;
      _M0Lm3numS341 = _M0L6_2atmpS1941 / _M0L4baseS342;
      continue;
    }
    break;
  }
  return _M0Lm5countS344;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS338) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS338 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS339;
    int32_t _M0L6_2atmpS1938;
    int32_t _M0L6_2atmpS1937;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS339 = moonbit_clz64(_M0L5valueS338);
    _M0L6_2atmpS1938 = 63 - _M0L14leading__zerosS339;
    _M0L6_2atmpS1937 = _M0L6_2atmpS1938 / 4;
    return _M0L6_2atmpS1937 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS337) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS337 >= 10000000000ull) {
    if (_M0L5valueS337 >= 100000000000000ull) {
      if (_M0L5valueS337 >= 10000000000000000ull) {
        if (_M0L5valueS337 >= 1000000000000000000ull) {
          if (_M0L5valueS337 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS337 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS337 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS337 >= 1000000000000ull) {
      if (_M0L5valueS337 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS337 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS337 >= 100000ull) {
    if (_M0L5valueS337 >= 10000000ull) {
      if (_M0L5valueS337 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS337 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS337 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS337 >= 1000ull) {
    if (_M0L5valueS337 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS337 >= 100ull) {
    return 3;
  } else if (_M0L5valueS337 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS321,
  int32_t _M0L5radixS320
) {
  int32_t _if__result_3691;
  int32_t _M0L12is__negativeS322;
  uint32_t _M0L3numS323;
  uint16_t* _M0L6bufferS324;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS320 < 2) {
    _if__result_3691 = 1;
  } else {
    _if__result_3691 = _M0L5radixS320 > 36;
  }
  if (_if__result_3691) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_62.data, (moonbit_string_t)moonbit_string_literal_65.data);
  }
  if (_M0L4selfS321 == 0) {
    return (moonbit_string_t)moonbit_string_literal_46.data;
  }
  _M0L12is__negativeS322 = _M0L4selfS321 < 0;
  if (_M0L12is__negativeS322) {
    int32_t _M0L6_2atmpS1936 = -_M0L4selfS321;
    _M0L3numS323 = *(uint32_t*)&_M0L6_2atmpS1936;
  } else {
    _M0L3numS323 = *(uint32_t*)&_M0L4selfS321;
  }
  switch (_M0L5radixS320) {
    case 10: {
      int32_t _M0L10digit__lenS325;
      int32_t _M0L6_2atmpS1933;
      int32_t _M0L10total__lenS326;
      uint16_t* _M0L6bufferS327;
      int32_t _M0L12digit__startS328;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS325 = _M0FPB12dec__count32(_M0L3numS323);
      if (_M0L12is__negativeS322) {
        _M0L6_2atmpS1933 = 1;
      } else {
        _M0L6_2atmpS1933 = 0;
      }
      _M0L10total__lenS326 = _M0L10digit__lenS325 + _M0L6_2atmpS1933;
      _M0L6bufferS327
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS326, 0);
      if (_M0L12is__negativeS322) {
        _M0L12digit__startS328 = 1;
      } else {
        _M0L12digit__startS328 = 0;
      }
      moonbit_incref(_M0L6bufferS327);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS327, _M0L3numS323, _M0L12digit__startS328, _M0L10total__lenS326);
      _M0L6bufferS324 = _M0L6bufferS327;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS329;
      int32_t _M0L6_2atmpS1934;
      int32_t _M0L10total__lenS330;
      uint16_t* _M0L6bufferS331;
      int32_t _M0L12digit__startS332;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS329 = _M0FPB12hex__count32(_M0L3numS323);
      if (_M0L12is__negativeS322) {
        _M0L6_2atmpS1934 = 1;
      } else {
        _M0L6_2atmpS1934 = 0;
      }
      _M0L10total__lenS330 = _M0L10digit__lenS329 + _M0L6_2atmpS1934;
      _M0L6bufferS331
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS330, 0);
      if (_M0L12is__negativeS322) {
        _M0L12digit__startS332 = 1;
      } else {
        _M0L12digit__startS332 = 0;
      }
      moonbit_incref(_M0L6bufferS331);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS331, _M0L3numS323, _M0L12digit__startS332, _M0L10total__lenS330);
      _M0L6bufferS324 = _M0L6bufferS331;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS333;
      int32_t _M0L6_2atmpS1935;
      int32_t _M0L10total__lenS334;
      uint16_t* _M0L6bufferS335;
      int32_t _M0L12digit__startS336;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS333
      = _M0FPB14radix__count32(_M0L3numS323, _M0L5radixS320);
      if (_M0L12is__negativeS322) {
        _M0L6_2atmpS1935 = 1;
      } else {
        _M0L6_2atmpS1935 = 0;
      }
      _M0L10total__lenS334 = _M0L10digit__lenS333 + _M0L6_2atmpS1935;
      _M0L6bufferS335
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS334, 0);
      if (_M0L12is__negativeS322) {
        _M0L12digit__startS336 = 1;
      } else {
        _M0L12digit__startS336 = 0;
      }
      moonbit_incref(_M0L6bufferS335);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS335, _M0L3numS323, _M0L12digit__startS336, _M0L10total__lenS334, _M0L5radixS320);
      _M0L6bufferS324 = _M0L6bufferS335;
      break;
    }
  }
  if (_M0L12is__negativeS322) {
    _M0L6bufferS324[0] = 45;
  }
  return _M0L6bufferS324;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS314,
  int32_t _M0L5radixS317
) {
  uint32_t _M0Lm3numS315;
  uint32_t _M0L4baseS316;
  int32_t _M0Lm5countS318;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS314 == 0u) {
    return 1;
  }
  _M0Lm3numS315 = _M0L5valueS314;
  _M0L4baseS316 = *(uint32_t*)&_M0L5radixS317;
  _M0Lm5countS318 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1930 = _M0Lm3numS315;
    if (_M0L6_2atmpS1930 > 0u) {
      int32_t _M0L6_2atmpS1931 = _M0Lm5countS318;
      uint32_t _M0L6_2atmpS1932;
      _M0Lm5countS318 = _M0L6_2atmpS1931 + 1;
      _M0L6_2atmpS1932 = _M0Lm3numS315;
      _M0Lm3numS315 = _M0L6_2atmpS1932 / _M0L4baseS316;
      continue;
    }
    break;
  }
  return _M0Lm5countS318;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS312) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS312 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS313;
    int32_t _M0L6_2atmpS1929;
    int32_t _M0L6_2atmpS1928;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS313 = moonbit_clz32(_M0L5valueS312);
    _M0L6_2atmpS1929 = 31 - _M0L14leading__zerosS313;
    _M0L6_2atmpS1928 = _M0L6_2atmpS1929 / 4;
    return _M0L6_2atmpS1928 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS311) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS311 >= 100000u) {
    if (_M0L5valueS311 >= 10000000u) {
      if (_M0L5valueS311 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS311 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS311 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS311 >= 1000u) {
    if (_M0L5valueS311 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS311 >= 100u) {
    return 3;
  } else if (_M0L5valueS311 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS301,
  uint32_t _M0L3numS289,
  int32_t _M0L12digit__startS292,
  int32_t _M0L10total__lenS291
) {
  uint32_t _M0Lm3numS288;
  int32_t _M0Lm6offsetS290;
  uint32_t _M0L6_2atmpS1927;
  int32_t _M0Lm9remainingS303;
  int32_t _M0L6_2atmpS1908;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS288 = _M0L3numS289;
  _M0Lm6offsetS290 = _M0L10total__lenS291 - _M0L12digit__startS292;
  while (1) {
    uint32_t _M0L6_2atmpS1871 = _M0Lm3numS288;
    if (_M0L6_2atmpS1871 >= 10000u) {
      uint32_t _M0L6_2atmpS1894 = _M0Lm3numS288;
      uint32_t _M0L1tS293 = _M0L6_2atmpS1894 / 10000u;
      uint32_t _M0L6_2atmpS1893 = _M0Lm3numS288;
      uint32_t _M0L6_2atmpS1892 = _M0L6_2atmpS1893 % 10000u;
      int32_t _M0L1rS294 = *(int32_t*)&_M0L6_2atmpS1892;
      int32_t _M0L2d1S295;
      int32_t _M0L2d2S296;
      int32_t _M0L6_2atmpS1872;
      int32_t _M0L6_2atmpS1891;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6d1__hiS297;
      int32_t _M0L6_2atmpS1889;
      int32_t _M0L6_2atmpS1888;
      int32_t _M0L6d1__loS298;
      int32_t _M0L6_2atmpS1887;
      int32_t _M0L6_2atmpS1886;
      int32_t _M0L6d2__hiS299;
      int32_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1884;
      int32_t _M0L6d2__loS300;
      int32_t _M0L6_2atmpS1874;
      int32_t _M0L6_2atmpS1873;
      int32_t _M0L6_2atmpS1877;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6_2atmpS1875;
      int32_t _M0L6_2atmpS1880;
      int32_t _M0L6_2atmpS1879;
      int32_t _M0L6_2atmpS1878;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L6_2atmpS1881;
      _M0Lm3numS288 = _M0L1tS293;
      _M0L2d1S295 = _M0L1rS294 / 100;
      _M0L2d2S296 = _M0L1rS294 % 100;
      _M0L6_2atmpS1872 = _M0Lm6offsetS290;
      _M0Lm6offsetS290 = _M0L6_2atmpS1872 - 4;
      _M0L6_2atmpS1891 = _M0L2d1S295 / 10;
      _M0L6_2atmpS1890 = 48 + _M0L6_2atmpS1891;
      _M0L6d1__hiS297 = (uint16_t)_M0L6_2atmpS1890;
      _M0L6_2atmpS1889 = _M0L2d1S295 % 10;
      _M0L6_2atmpS1888 = 48 + _M0L6_2atmpS1889;
      _M0L6d1__loS298 = (uint16_t)_M0L6_2atmpS1888;
      _M0L6_2atmpS1887 = _M0L2d2S296 / 10;
      _M0L6_2atmpS1886 = 48 + _M0L6_2atmpS1887;
      _M0L6d2__hiS299 = (uint16_t)_M0L6_2atmpS1886;
      _M0L6_2atmpS1885 = _M0L2d2S296 % 10;
      _M0L6_2atmpS1884 = 48 + _M0L6_2atmpS1885;
      _M0L6d2__loS300 = (uint16_t)_M0L6_2atmpS1884;
      _M0L6_2atmpS1874 = _M0Lm6offsetS290;
      _M0L6_2atmpS1873 = _M0L12digit__startS292 + _M0L6_2atmpS1874;
      _M0L6bufferS301[_M0L6_2atmpS1873] = _M0L6d1__hiS297;
      _M0L6_2atmpS1877 = _M0Lm6offsetS290;
      _M0L6_2atmpS1876 = _M0L12digit__startS292 + _M0L6_2atmpS1877;
      _M0L6_2atmpS1875 = _M0L6_2atmpS1876 + 1;
      _M0L6bufferS301[_M0L6_2atmpS1875] = _M0L6d1__loS298;
      _M0L6_2atmpS1880 = _M0Lm6offsetS290;
      _M0L6_2atmpS1879 = _M0L12digit__startS292 + _M0L6_2atmpS1880;
      _M0L6_2atmpS1878 = _M0L6_2atmpS1879 + 2;
      _M0L6bufferS301[_M0L6_2atmpS1878] = _M0L6d2__hiS299;
      _M0L6_2atmpS1883 = _M0Lm6offsetS290;
      _M0L6_2atmpS1882 = _M0L12digit__startS292 + _M0L6_2atmpS1883;
      _M0L6_2atmpS1881 = _M0L6_2atmpS1882 + 3;
      _M0L6bufferS301[_M0L6_2atmpS1881] = _M0L6d2__loS300;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1927 = _M0Lm3numS288;
  _M0Lm9remainingS303 = *(int32_t*)&_M0L6_2atmpS1927;
  while (1) {
    int32_t _M0L6_2atmpS1895 = _M0Lm9remainingS303;
    if (_M0L6_2atmpS1895 >= 100) {
      int32_t _M0L6_2atmpS1907 = _M0Lm9remainingS303;
      int32_t _M0L1tS304 = _M0L6_2atmpS1907 / 100;
      int32_t _M0L6_2atmpS1906 = _M0Lm9remainingS303;
      int32_t _M0L1dS305 = _M0L6_2atmpS1906 % 100;
      int32_t _M0L6_2atmpS1896;
      int32_t _M0L6_2atmpS1905;
      int32_t _M0L6_2atmpS1904;
      int32_t _M0L5d__hiS306;
      int32_t _M0L6_2atmpS1903;
      int32_t _M0L6_2atmpS1902;
      int32_t _M0L5d__loS307;
      int32_t _M0L6_2atmpS1898;
      int32_t _M0L6_2atmpS1897;
      int32_t _M0L6_2atmpS1901;
      int32_t _M0L6_2atmpS1900;
      int32_t _M0L6_2atmpS1899;
      _M0Lm9remainingS303 = _M0L1tS304;
      _M0L6_2atmpS1896 = _M0Lm6offsetS290;
      _M0Lm6offsetS290 = _M0L6_2atmpS1896 - 2;
      _M0L6_2atmpS1905 = _M0L1dS305 / 10;
      _M0L6_2atmpS1904 = 48 + _M0L6_2atmpS1905;
      _M0L5d__hiS306 = (uint16_t)_M0L6_2atmpS1904;
      _M0L6_2atmpS1903 = _M0L1dS305 % 10;
      _M0L6_2atmpS1902 = 48 + _M0L6_2atmpS1903;
      _M0L5d__loS307 = (uint16_t)_M0L6_2atmpS1902;
      _M0L6_2atmpS1898 = _M0Lm6offsetS290;
      _M0L6_2atmpS1897 = _M0L12digit__startS292 + _M0L6_2atmpS1898;
      _M0L6bufferS301[_M0L6_2atmpS1897] = _M0L5d__hiS306;
      _M0L6_2atmpS1901 = _M0Lm6offsetS290;
      _M0L6_2atmpS1900 = _M0L12digit__startS292 + _M0L6_2atmpS1901;
      _M0L6_2atmpS1899 = _M0L6_2atmpS1900 + 1;
      _M0L6bufferS301[_M0L6_2atmpS1899] = _M0L5d__loS307;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1908 = _M0Lm9remainingS303;
  if (_M0L6_2atmpS1908 >= 10) {
    int32_t _M0L6_2atmpS1909 = _M0Lm6offsetS290;
    int32_t _M0L6_2atmpS1920;
    int32_t _M0L6_2atmpS1919;
    int32_t _M0L6_2atmpS1918;
    int32_t _M0L5d__hiS309;
    int32_t _M0L6_2atmpS1917;
    int32_t _M0L6_2atmpS1916;
    int32_t _M0L6_2atmpS1915;
    int32_t _M0L5d__loS310;
    int32_t _M0L6_2atmpS1911;
    int32_t _M0L6_2atmpS1910;
    int32_t _M0L6_2atmpS1914;
    int32_t _M0L6_2atmpS1913;
    int32_t _M0L6_2atmpS1912;
    _M0Lm6offsetS290 = _M0L6_2atmpS1909 - 2;
    _M0L6_2atmpS1920 = _M0Lm9remainingS303;
    _M0L6_2atmpS1919 = _M0L6_2atmpS1920 / 10;
    _M0L6_2atmpS1918 = 48 + _M0L6_2atmpS1919;
    _M0L5d__hiS309 = (uint16_t)_M0L6_2atmpS1918;
    _M0L6_2atmpS1917 = _M0Lm9remainingS303;
    _M0L6_2atmpS1916 = _M0L6_2atmpS1917 % 10;
    _M0L6_2atmpS1915 = 48 + _M0L6_2atmpS1916;
    _M0L5d__loS310 = (uint16_t)_M0L6_2atmpS1915;
    _M0L6_2atmpS1911 = _M0Lm6offsetS290;
    _M0L6_2atmpS1910 = _M0L12digit__startS292 + _M0L6_2atmpS1911;
    _M0L6bufferS301[_M0L6_2atmpS1910] = _M0L5d__hiS309;
    _M0L6_2atmpS1914 = _M0Lm6offsetS290;
    _M0L6_2atmpS1913 = _M0L12digit__startS292 + _M0L6_2atmpS1914;
    _M0L6_2atmpS1912 = _M0L6_2atmpS1913 + 1;
    _M0L6bufferS301[_M0L6_2atmpS1912] = _M0L5d__loS310;
    moonbit_decref(_M0L6bufferS301);
  } else {
    int32_t _M0L6_2atmpS1921 = _M0Lm6offsetS290;
    int32_t _M0L6_2atmpS1926;
    int32_t _M0L6_2atmpS1922;
    int32_t _M0L6_2atmpS1925;
    int32_t _M0L6_2atmpS1924;
    int32_t _M0L6_2atmpS1923;
    _M0Lm6offsetS290 = _M0L6_2atmpS1921 - 1;
    _M0L6_2atmpS1926 = _M0Lm6offsetS290;
    _M0L6_2atmpS1922 = _M0L12digit__startS292 + _M0L6_2atmpS1926;
    _M0L6_2atmpS1925 = _M0Lm9remainingS303;
    _M0L6_2atmpS1924 = 48 + _M0L6_2atmpS1925;
    _M0L6_2atmpS1923 = (uint16_t)_M0L6_2atmpS1924;
    _M0L6bufferS301[_M0L6_2atmpS1922] = _M0L6_2atmpS1923;
    moonbit_decref(_M0L6bufferS301);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS283,
  uint32_t _M0L3numS277,
  int32_t _M0L12digit__startS275,
  int32_t _M0L10total__lenS274,
  int32_t _M0L5radixS279
) {
  int32_t _M0Lm6offsetS273;
  uint32_t _M0Lm1nS276;
  uint32_t _M0L4baseS278;
  int32_t _M0L6_2atmpS1853;
  int32_t _M0L6_2atmpS1852;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS273 = _M0L10total__lenS274 - _M0L12digit__startS275;
  _M0Lm1nS276 = _M0L3numS277;
  _M0L4baseS278 = *(uint32_t*)&_M0L5radixS279;
  _M0L6_2atmpS1853 = _M0L5radixS279 - 1;
  _M0L6_2atmpS1852 = _M0L5radixS279 & _M0L6_2atmpS1853;
  if (_M0L6_2atmpS1852 == 0) {
    int32_t _M0L5shiftS280;
    uint32_t _M0L4maskS281;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS280 = moonbit_ctz32(_M0L5radixS279);
    _M0L4maskS281 = _M0L4baseS278 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1854 = _M0Lm1nS276;
      if (_M0L6_2atmpS1854 > 0u) {
        int32_t _M0L6_2atmpS1855 = _M0Lm6offsetS273;
        uint32_t _M0L6_2atmpS1861;
        uint32_t _M0L6_2atmpS1860;
        int32_t _M0L5digitS282;
        int32_t _M0L6_2atmpS1858;
        int32_t _M0L6_2atmpS1856;
        int32_t _M0L6_2atmpS1857;
        uint32_t _M0L6_2atmpS1859;
        _M0Lm6offsetS273 = _M0L6_2atmpS1855 - 1;
        _M0L6_2atmpS1861 = _M0Lm1nS276;
        _M0L6_2atmpS1860 = _M0L6_2atmpS1861 & _M0L4maskS281;
        _M0L5digitS282 = *(int32_t*)&_M0L6_2atmpS1860;
        _M0L6_2atmpS1858 = _M0Lm6offsetS273;
        _M0L6_2atmpS1856 = _M0L12digit__startS275 + _M0L6_2atmpS1858;
        _M0L6_2atmpS1857
        = ((moonbit_string_t)moonbit_string_literal_64.data)[
          _M0L5digitS282
        ];
        _M0L6bufferS283[_M0L6_2atmpS1856] = _M0L6_2atmpS1857;
        _M0L6_2atmpS1859 = _M0Lm1nS276;
        _M0Lm1nS276 = _M0L6_2atmpS1859 >> (_M0L5shiftS280 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS283);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1862 = _M0Lm1nS276;
      if (_M0L6_2atmpS1862 > 0u) {
        int32_t _M0L6_2atmpS1863 = _M0Lm6offsetS273;
        uint32_t _M0L6_2atmpS1870;
        uint32_t _M0L1qS285;
        uint32_t _M0L6_2atmpS1868;
        uint32_t _M0L6_2atmpS1869;
        uint32_t _M0L6_2atmpS1867;
        int32_t _M0L5digitS286;
        int32_t _M0L6_2atmpS1866;
        int32_t _M0L6_2atmpS1864;
        int32_t _M0L6_2atmpS1865;
        _M0Lm6offsetS273 = _M0L6_2atmpS1863 - 1;
        _M0L6_2atmpS1870 = _M0Lm1nS276;
        _M0L1qS285 = _M0L6_2atmpS1870 / _M0L4baseS278;
        _M0L6_2atmpS1868 = _M0Lm1nS276;
        _M0L6_2atmpS1869 = _M0L1qS285 * _M0L4baseS278;
        _M0L6_2atmpS1867 = _M0L6_2atmpS1868 - _M0L6_2atmpS1869;
        _M0L5digitS286 = *(int32_t*)&_M0L6_2atmpS1867;
        _M0L6_2atmpS1866 = _M0Lm6offsetS273;
        _M0L6_2atmpS1864 = _M0L12digit__startS275 + _M0L6_2atmpS1866;
        _M0L6_2atmpS1865
        = ((moonbit_string_t)moonbit_string_literal_64.data)[
          _M0L5digitS286
        ];
        _M0L6bufferS283[_M0L6_2atmpS1864] = _M0L6_2atmpS1865;
        _M0Lm1nS276 = _M0L1qS285;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS283);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS270,
  uint32_t _M0L3numS266,
  int32_t _M0L12digit__startS264,
  int32_t _M0L10total__lenS263
) {
  int32_t _M0Lm6offsetS262;
  uint32_t _M0Lm1nS265;
  int32_t _M0L6_2atmpS1848;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS262 = _M0L10total__lenS263 - _M0L12digit__startS264;
  _M0Lm1nS265 = _M0L3numS266;
  while (1) {
    int32_t _M0L6_2atmpS1836 = _M0Lm6offsetS262;
    if (_M0L6_2atmpS1836 >= 2) {
      int32_t _M0L6_2atmpS1837 = _M0Lm6offsetS262;
      uint32_t _M0L6_2atmpS1847;
      uint32_t _M0L6_2atmpS1846;
      int32_t _M0L9byte__valS267;
      int32_t _M0L2hiS268;
      int32_t _M0L2loS269;
      int32_t _M0L6_2atmpS1840;
      int32_t _M0L6_2atmpS1838;
      int32_t _M0L6_2atmpS1839;
      int32_t _M0L6_2atmpS1844;
      int32_t _M0L6_2atmpS1843;
      int32_t _M0L6_2atmpS1841;
      int32_t _M0L6_2atmpS1842;
      uint32_t _M0L6_2atmpS1845;
      _M0Lm6offsetS262 = _M0L6_2atmpS1837 - 2;
      _M0L6_2atmpS1847 = _M0Lm1nS265;
      _M0L6_2atmpS1846 = _M0L6_2atmpS1847 & 255u;
      _M0L9byte__valS267 = *(int32_t*)&_M0L6_2atmpS1846;
      _M0L2hiS268 = _M0L9byte__valS267 / 16;
      _M0L2loS269 = _M0L9byte__valS267 % 16;
      _M0L6_2atmpS1840 = _M0Lm6offsetS262;
      _M0L6_2atmpS1838 = _M0L12digit__startS264 + _M0L6_2atmpS1840;
      _M0L6_2atmpS1839
      = ((moonbit_string_t)moonbit_string_literal_64.data)[
        _M0L2hiS268
      ];
      _M0L6bufferS270[_M0L6_2atmpS1838] = _M0L6_2atmpS1839;
      _M0L6_2atmpS1844 = _M0Lm6offsetS262;
      _M0L6_2atmpS1843 = _M0L12digit__startS264 + _M0L6_2atmpS1844;
      _M0L6_2atmpS1841 = _M0L6_2atmpS1843 + 1;
      _M0L6_2atmpS1842
      = ((moonbit_string_t)moonbit_string_literal_64.data)[
        _M0L2loS269
      ];
      _M0L6bufferS270[_M0L6_2atmpS1841] = _M0L6_2atmpS1842;
      _M0L6_2atmpS1845 = _M0Lm1nS265;
      _M0Lm1nS265 = _M0L6_2atmpS1845 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1848 = _M0Lm6offsetS262;
  if (_M0L6_2atmpS1848 == 1) {
    uint32_t _M0L6_2atmpS1851 = _M0Lm1nS265;
    uint32_t _M0L6_2atmpS1850 = _M0L6_2atmpS1851 & 15u;
    int32_t _M0L6nibbleS272 = *(int32_t*)&_M0L6_2atmpS1850;
    int32_t _M0L6_2atmpS1849 =
      ((moonbit_string_t)moonbit_string_literal_64.data)[_M0L6nibbleS272];
    _M0L6bufferS270[_M0L12digit__startS264] = _M0L6_2atmpS1849;
    moonbit_decref(_M0L6bufferS270);
  } else {
    moonbit_decref(_M0L6bufferS270);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS255) {
  struct _M0TWEOs* _M0L7_2afuncS254;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS254 = _M0L4selfS255;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS254->code(_M0L7_2afuncS254);
}

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS257
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2afuncS256;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS256 = _M0L4selfS257;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS256->code(_M0L7_2afuncS256);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS259
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS258;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS258 = _M0L4selfS259;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS258->code(_M0L7_2afuncS258);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS261) {
  struct _M0TWEOc* _M0L7_2afuncS260;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS260 = _M0L4selfS261;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS260->code(_M0L7_2afuncS260);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS247
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS246;
  struct _M0TPB6Logger _M0L6_2atmpS1832;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS246 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS246);
  _M0L6_2atmpS1832
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS246
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS247, _M0L6_2atmpS1832);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS246);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS249
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS248;
  struct _M0TPB6Logger _M0L6_2atmpS1833;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS248 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS248);
  _M0L6_2atmpS1833
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS248
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS249, _M0L6_2atmpS1833);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS248);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS251
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS250;
  struct _M0TPB6Logger _M0L6_2atmpS1834;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS250 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS250);
  _M0L6_2atmpS1834
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS250
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS251, _M0L6_2atmpS1834);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS250);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS253
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS252;
  struct _M0TPB6Logger _M0L6_2atmpS1835;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS252 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS252);
  _M0L6_2atmpS1835
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS252
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS253, _M0L6_2atmpS1835);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS252);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS245
) {
  int32_t _M0L8_2afieldS3392;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3392 = _M0L4selfS245.$1;
  moonbit_decref(_M0L4selfS245.$0);
  return _M0L8_2afieldS3392;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS244
) {
  int32_t _M0L3endS1830;
  int32_t _M0L8_2afieldS3393;
  int32_t _M0L5startS1831;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1830 = _M0L4selfS244.$2;
  _M0L8_2afieldS3393 = _M0L4selfS244.$1;
  moonbit_decref(_M0L4selfS244.$0);
  _M0L5startS1831 = _M0L8_2afieldS3393;
  return _M0L3endS1830 - _M0L5startS1831;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS243
) {
  moonbit_string_t _M0L8_2afieldS3394;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3394 = _M0L4selfS243.$0;
  return _M0L8_2afieldS3394;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS239,
  moonbit_string_t _M0L5valueS240,
  int32_t _M0L5startS241,
  int32_t _M0L3lenS242
) {
  int32_t _M0L6_2atmpS1829;
  int64_t _M0L6_2atmpS1828;
  struct _M0TPC16string10StringView _M0L6_2atmpS1827;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1829 = _M0L5startS241 + _M0L3lenS242;
  _M0L6_2atmpS1828 = (int64_t)_M0L6_2atmpS1829;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1827
  = _M0MPC16string6String11sub_2einner(_M0L5valueS240, _M0L5startS241, _M0L6_2atmpS1828);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS239, _M0L6_2atmpS1827);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS232,
  int32_t _M0L5startS238,
  int64_t _M0L3endS234
) {
  int32_t _M0L3lenS231;
  int32_t _M0L3endS233;
  int32_t _M0L5startS237;
  int32_t _if__result_3698;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS231 = Moonbit_array_length(_M0L4selfS232);
  if (_M0L3endS234 == 4294967296ll) {
    _M0L3endS233 = _M0L3lenS231;
  } else {
    int64_t _M0L7_2aSomeS235 = _M0L3endS234;
    int32_t _M0L6_2aendS236 = (int32_t)_M0L7_2aSomeS235;
    if (_M0L6_2aendS236 < 0) {
      _M0L3endS233 = _M0L3lenS231 + _M0L6_2aendS236;
    } else {
      _M0L3endS233 = _M0L6_2aendS236;
    }
  }
  if (_M0L5startS238 < 0) {
    _M0L5startS237 = _M0L3lenS231 + _M0L5startS238;
  } else {
    _M0L5startS237 = _M0L5startS238;
  }
  if (_M0L5startS237 >= 0) {
    if (_M0L5startS237 <= _M0L3endS233) {
      _if__result_3698 = _M0L3endS233 <= _M0L3lenS231;
    } else {
      _if__result_3698 = 0;
    }
  } else {
    _if__result_3698 = 0;
  }
  if (_if__result_3698) {
    if (_M0L5startS237 < _M0L3lenS231) {
      int32_t _M0L6_2atmpS1824 = _M0L4selfS232[_M0L5startS237];
      int32_t _M0L6_2atmpS1823;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1823
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1824);
      if (!_M0L6_2atmpS1823) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS233 < _M0L3lenS231) {
      int32_t _M0L6_2atmpS1826 = _M0L4selfS232[_M0L3endS233];
      int32_t _M0L6_2atmpS1825;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1825
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1826);
      if (!_M0L6_2atmpS1825) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS237,
                                                 _M0L3endS233,
                                                 _M0L4selfS232};
  } else {
    moonbit_decref(_M0L4selfS232);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS228) {
  struct _M0TPB6Hasher* _M0L1hS227;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS227 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS227);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS227, _M0L4selfS228);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS227);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS230
) {
  struct _M0TPB6Hasher* _M0L1hS229;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS229 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS229);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS229, _M0L4selfS230);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS229);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS225) {
  int32_t _M0L4seedS224;
  if (_M0L10seed_2eoptS225 == 4294967296ll) {
    _M0L4seedS224 = 0;
  } else {
    int64_t _M0L7_2aSomeS226 = _M0L10seed_2eoptS225;
    _M0L4seedS224 = (int32_t)_M0L7_2aSomeS226;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS224);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS223) {
  uint32_t _M0L6_2atmpS1822;
  uint32_t _M0L6_2atmpS1821;
  struct _M0TPB6Hasher* _block_3699;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1822 = *(uint32_t*)&_M0L4seedS223;
  _M0L6_2atmpS1821 = _M0L6_2atmpS1822 + 374761393u;
  _block_3699
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3699)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3699->$0 = _M0L6_2atmpS1821;
  return _block_3699;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS222) {
  uint32_t _M0L6_2atmpS1820;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1820 = _M0MPB6Hasher9avalanche(_M0L4selfS222);
  return *(int32_t*)&_M0L6_2atmpS1820;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS221) {
  uint32_t _M0L8_2afieldS3395;
  uint32_t _M0Lm3accS220;
  uint32_t _M0L6_2atmpS1809;
  uint32_t _M0L6_2atmpS1811;
  uint32_t _M0L6_2atmpS1810;
  uint32_t _M0L6_2atmpS1812;
  uint32_t _M0L6_2atmpS1813;
  uint32_t _M0L6_2atmpS1815;
  uint32_t _M0L6_2atmpS1814;
  uint32_t _M0L6_2atmpS1816;
  uint32_t _M0L6_2atmpS1817;
  uint32_t _M0L6_2atmpS1819;
  uint32_t _M0L6_2atmpS1818;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3395 = _M0L4selfS221->$0;
  moonbit_decref(_M0L4selfS221);
  _M0Lm3accS220 = _M0L8_2afieldS3395;
  _M0L6_2atmpS1809 = _M0Lm3accS220;
  _M0L6_2atmpS1811 = _M0Lm3accS220;
  _M0L6_2atmpS1810 = _M0L6_2atmpS1811 >> 15;
  _M0Lm3accS220 = _M0L6_2atmpS1809 ^ _M0L6_2atmpS1810;
  _M0L6_2atmpS1812 = _M0Lm3accS220;
  _M0Lm3accS220 = _M0L6_2atmpS1812 * 2246822519u;
  _M0L6_2atmpS1813 = _M0Lm3accS220;
  _M0L6_2atmpS1815 = _M0Lm3accS220;
  _M0L6_2atmpS1814 = _M0L6_2atmpS1815 >> 13;
  _M0Lm3accS220 = _M0L6_2atmpS1813 ^ _M0L6_2atmpS1814;
  _M0L6_2atmpS1816 = _M0Lm3accS220;
  _M0Lm3accS220 = _M0L6_2atmpS1816 * 3266489917u;
  _M0L6_2atmpS1817 = _M0Lm3accS220;
  _M0L6_2atmpS1819 = _M0Lm3accS220;
  _M0L6_2atmpS1818 = _M0L6_2atmpS1819 >> 16;
  _M0Lm3accS220 = _M0L6_2atmpS1817 ^ _M0L6_2atmpS1818;
  return _M0Lm3accS220;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS218,
  moonbit_string_t _M0L1yS219
) {
  int32_t _M0L6_2atmpS3396;
  int32_t _M0L6_2atmpS1808;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3396 = moonbit_val_array_equal(_M0L1xS218, _M0L1yS219);
  moonbit_decref(_M0L1xS218);
  moonbit_decref(_M0L1yS219);
  _M0L6_2atmpS1808 = _M0L6_2atmpS3396;
  return !_M0L6_2atmpS1808;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS215,
  int32_t _M0L5valueS214
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS214, _M0L4selfS215);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS217,
  moonbit_string_t _M0L5valueS216
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS216, _M0L4selfS217);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS213) {
  int64_t _M0L6_2atmpS1807;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1807 = (int64_t)_M0L4selfS213;
  return *(uint64_t*)&_M0L6_2atmpS1807;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS211,
  int32_t _M0L5valueS212
) {
  uint32_t _M0L6_2atmpS1806;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1806 = *(uint32_t*)&_M0L5valueS212;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS211, _M0L6_2atmpS1806);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS204
) {
  struct _M0TPB13StringBuilder* _M0L3bufS202;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS203;
  int32_t _M0L7_2abindS205;
  int32_t _M0L1iS206;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS202 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS203 = _M0L4selfS204;
  moonbit_incref(_M0L3bufS202);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS202, 91);
  _M0L7_2abindS205 = _M0L7_2aselfS203->$1;
  _M0L1iS206 = 0;
  while (1) {
    if (_M0L1iS206 < _M0L7_2abindS205) {
      int32_t _if__result_3701;
      moonbit_string_t* _M0L8_2afieldS3398;
      moonbit_string_t* _M0L3bufS1804;
      moonbit_string_t _M0L6_2atmpS3397;
      moonbit_string_t _M0L4itemS207;
      int32_t _M0L6_2atmpS1805;
      if (_M0L1iS206 != 0) {
        moonbit_incref(_M0L3bufS202);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS202, (moonbit_string_t)moonbit_string_literal_66.data);
      }
      if (_M0L1iS206 < 0) {
        _if__result_3701 = 1;
      } else {
        int32_t _M0L3lenS1803 = _M0L7_2aselfS203->$1;
        _if__result_3701 = _M0L1iS206 >= _M0L3lenS1803;
      }
      if (_if__result_3701) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3398 = _M0L7_2aselfS203->$0;
      _M0L3bufS1804 = _M0L8_2afieldS3398;
      _M0L6_2atmpS3397 = (moonbit_string_t)_M0L3bufS1804[_M0L1iS206];
      _M0L4itemS207 = _M0L6_2atmpS3397;
      if (_M0L4itemS207 == 0) {
        moonbit_incref(_M0L3bufS202);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS202, (moonbit_string_t)moonbit_string_literal_26.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS208 = _M0L4itemS207;
        moonbit_string_t _M0L6_2alocS209 = _M0L7_2aSomeS208;
        moonbit_string_t _M0L6_2atmpS1802;
        moonbit_incref(_M0L6_2alocS209);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1802
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS209);
        moonbit_incref(_M0L3bufS202);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS202, _M0L6_2atmpS1802);
      }
      _M0L6_2atmpS1805 = _M0L1iS206 + 1;
      _M0L1iS206 = _M0L6_2atmpS1805;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS203);
    }
    break;
  }
  moonbit_incref(_M0L3bufS202);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS202, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS202);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS201
) {
  moonbit_string_t _M0L6_2atmpS1801;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1800;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1801 = _M0L4selfS201;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1800 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1801);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1800);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS200
) {
  struct _M0TPB13StringBuilder* _M0L2sbS199;
  struct _M0TPC16string10StringView _M0L8_2afieldS3411;
  struct _M0TPC16string10StringView _M0L3pkgS1785;
  moonbit_string_t _M0L6_2atmpS1784;
  moonbit_string_t _M0L6_2atmpS3410;
  moonbit_string_t _M0L6_2atmpS1783;
  moonbit_string_t _M0L6_2atmpS3409;
  moonbit_string_t _M0L6_2atmpS1782;
  struct _M0TPC16string10StringView _M0L8_2afieldS3408;
  struct _M0TPC16string10StringView _M0L8filenameS1786;
  struct _M0TPC16string10StringView _M0L8_2afieldS3407;
  struct _M0TPC16string10StringView _M0L11start__lineS1789;
  moonbit_string_t _M0L6_2atmpS1788;
  moonbit_string_t _M0L6_2atmpS3406;
  moonbit_string_t _M0L6_2atmpS1787;
  struct _M0TPC16string10StringView _M0L8_2afieldS3405;
  struct _M0TPC16string10StringView _M0L13start__columnS1792;
  moonbit_string_t _M0L6_2atmpS1791;
  moonbit_string_t _M0L6_2atmpS3404;
  moonbit_string_t _M0L6_2atmpS1790;
  struct _M0TPC16string10StringView _M0L8_2afieldS3403;
  struct _M0TPC16string10StringView _M0L9end__lineS1795;
  moonbit_string_t _M0L6_2atmpS1794;
  moonbit_string_t _M0L6_2atmpS3402;
  moonbit_string_t _M0L6_2atmpS1793;
  struct _M0TPC16string10StringView _M0L8_2afieldS3401;
  int32_t _M0L6_2acntS3537;
  struct _M0TPC16string10StringView _M0L11end__columnS1799;
  moonbit_string_t _M0L6_2atmpS1798;
  moonbit_string_t _M0L6_2atmpS3400;
  moonbit_string_t _M0L6_2atmpS1797;
  moonbit_string_t _M0L6_2atmpS3399;
  moonbit_string_t _M0L6_2atmpS1796;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS199 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3411
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$0_1, _M0L4selfS200->$0_2, _M0L4selfS200->$0_0
  };
  _M0L3pkgS1785 = _M0L8_2afieldS3411;
  moonbit_incref(_M0L3pkgS1785.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1784
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1785);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3410
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_67.data, _M0L6_2atmpS1784);
  moonbit_decref(_M0L6_2atmpS1784);
  _M0L6_2atmpS1783 = _M0L6_2atmpS3410;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3409
  = moonbit_add_string(_M0L6_2atmpS1783, (moonbit_string_t)moonbit_string_literal_68.data);
  moonbit_decref(_M0L6_2atmpS1783);
  _M0L6_2atmpS1782 = _M0L6_2atmpS3409;
  moonbit_incref(_M0L2sbS199);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1782);
  moonbit_incref(_M0L2sbS199);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, (moonbit_string_t)moonbit_string_literal_69.data);
  _M0L8_2afieldS3408
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$1_1, _M0L4selfS200->$1_2, _M0L4selfS200->$1_0
  };
  _M0L8filenameS1786 = _M0L8_2afieldS3408;
  moonbit_incref(_M0L8filenameS1786.$0);
  moonbit_incref(_M0L2sbS199);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS199, _M0L8filenameS1786);
  _M0L8_2afieldS3407
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$2_1, _M0L4selfS200->$2_2, _M0L4selfS200->$2_0
  };
  _M0L11start__lineS1789 = _M0L8_2afieldS3407;
  moonbit_incref(_M0L11start__lineS1789.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1788
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1789);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3406
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_70.data, _M0L6_2atmpS1788);
  moonbit_decref(_M0L6_2atmpS1788);
  _M0L6_2atmpS1787 = _M0L6_2atmpS3406;
  moonbit_incref(_M0L2sbS199);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1787);
  _M0L8_2afieldS3405
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$3_1, _M0L4selfS200->$3_2, _M0L4selfS200->$3_0
  };
  _M0L13start__columnS1792 = _M0L8_2afieldS3405;
  moonbit_incref(_M0L13start__columnS1792.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1791
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1792);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3404
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_71.data, _M0L6_2atmpS1791);
  moonbit_decref(_M0L6_2atmpS1791);
  _M0L6_2atmpS1790 = _M0L6_2atmpS3404;
  moonbit_incref(_M0L2sbS199);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1790);
  _M0L8_2afieldS3403
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$4_1, _M0L4selfS200->$4_2, _M0L4selfS200->$4_0
  };
  _M0L9end__lineS1795 = _M0L8_2afieldS3403;
  moonbit_incref(_M0L9end__lineS1795.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1794
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1795);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3402
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_72.data, _M0L6_2atmpS1794);
  moonbit_decref(_M0L6_2atmpS1794);
  _M0L6_2atmpS1793 = _M0L6_2atmpS3402;
  moonbit_incref(_M0L2sbS199);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1793);
  _M0L8_2afieldS3401
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$5_1, _M0L4selfS200->$5_2, _M0L4selfS200->$5_0
  };
  _M0L6_2acntS3537 = Moonbit_object_header(_M0L4selfS200)->rc;
  if (_M0L6_2acntS3537 > 1) {
    int32_t _M0L11_2anew__cntS3543 = _M0L6_2acntS3537 - 1;
    Moonbit_object_header(_M0L4selfS200)->rc = _M0L11_2anew__cntS3543;
    moonbit_incref(_M0L8_2afieldS3401.$0);
  } else if (_M0L6_2acntS3537 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3542 =
      (struct _M0TPC16string10StringView){_M0L4selfS200->$4_1,
                                            _M0L4selfS200->$4_2,
                                            _M0L4selfS200->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3541;
    struct _M0TPC16string10StringView _M0L8_2afieldS3540;
    struct _M0TPC16string10StringView _M0L8_2afieldS3539;
    struct _M0TPC16string10StringView _M0L8_2afieldS3538;
    moonbit_decref(_M0L8_2afieldS3542.$0);
    _M0L8_2afieldS3541
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$3_1, _M0L4selfS200->$3_2, _M0L4selfS200->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3541.$0);
    _M0L8_2afieldS3540
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$2_1, _M0L4selfS200->$2_2, _M0L4selfS200->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3540.$0);
    _M0L8_2afieldS3539
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$1_1, _M0L4selfS200->$1_2, _M0L4selfS200->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3539.$0);
    _M0L8_2afieldS3538
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$0_1, _M0L4selfS200->$0_2, _M0L4selfS200->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3538.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS200);
  }
  _M0L11end__columnS1799 = _M0L8_2afieldS3401;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1798
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1799);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3400
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_73.data, _M0L6_2atmpS1798);
  moonbit_decref(_M0L6_2atmpS1798);
  _M0L6_2atmpS1797 = _M0L6_2atmpS3400;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3399
  = moonbit_add_string(_M0L6_2atmpS1797, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1797);
  _M0L6_2atmpS1796 = _M0L6_2atmpS3399;
  moonbit_incref(_M0L2sbS199);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1796);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS199);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS197,
  moonbit_string_t _M0L3strS198
) {
  int32_t _M0L3lenS1772;
  int32_t _M0L6_2atmpS1774;
  int32_t _M0L6_2atmpS1773;
  int32_t _M0L6_2atmpS1771;
  moonbit_bytes_t _M0L8_2afieldS3413;
  moonbit_bytes_t _M0L4dataS1775;
  int32_t _M0L3lenS1776;
  int32_t _M0L6_2atmpS1777;
  int32_t _M0L3lenS1779;
  int32_t _M0L6_2atmpS3412;
  int32_t _M0L6_2atmpS1781;
  int32_t _M0L6_2atmpS1780;
  int32_t _M0L6_2atmpS1778;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1772 = _M0L4selfS197->$1;
  _M0L6_2atmpS1774 = Moonbit_array_length(_M0L3strS198);
  _M0L6_2atmpS1773 = _M0L6_2atmpS1774 * 2;
  _M0L6_2atmpS1771 = _M0L3lenS1772 + _M0L6_2atmpS1773;
  moonbit_incref(_M0L4selfS197);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS197, _M0L6_2atmpS1771);
  _M0L8_2afieldS3413 = _M0L4selfS197->$0;
  _M0L4dataS1775 = _M0L8_2afieldS3413;
  _M0L3lenS1776 = _M0L4selfS197->$1;
  _M0L6_2atmpS1777 = Moonbit_array_length(_M0L3strS198);
  moonbit_incref(_M0L4dataS1775);
  moonbit_incref(_M0L3strS198);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1775, _M0L3lenS1776, _M0L3strS198, 0, _M0L6_2atmpS1777);
  _M0L3lenS1779 = _M0L4selfS197->$1;
  _M0L6_2atmpS3412 = Moonbit_array_length(_M0L3strS198);
  moonbit_decref(_M0L3strS198);
  _M0L6_2atmpS1781 = _M0L6_2atmpS3412;
  _M0L6_2atmpS1780 = _M0L6_2atmpS1781 * 2;
  _M0L6_2atmpS1778 = _M0L3lenS1779 + _M0L6_2atmpS1780;
  _M0L4selfS197->$1 = _M0L6_2atmpS1778;
  moonbit_decref(_M0L4selfS197);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS189,
  int32_t _M0L13bytes__offsetS184,
  moonbit_string_t _M0L3strS191,
  int32_t _M0L11str__offsetS187,
  int32_t _M0L6lengthS185
) {
  int32_t _M0L6_2atmpS1770;
  int32_t _M0L6_2atmpS1769;
  int32_t _M0L2e1S183;
  int32_t _M0L6_2atmpS1768;
  int32_t _M0L2e2S186;
  int32_t _M0L4len1S188;
  int32_t _M0L4len2S190;
  int32_t _if__result_3702;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1770 = _M0L6lengthS185 * 2;
  _M0L6_2atmpS1769 = _M0L13bytes__offsetS184 + _M0L6_2atmpS1770;
  _M0L2e1S183 = _M0L6_2atmpS1769 - 1;
  _M0L6_2atmpS1768 = _M0L11str__offsetS187 + _M0L6lengthS185;
  _M0L2e2S186 = _M0L6_2atmpS1768 - 1;
  _M0L4len1S188 = Moonbit_array_length(_M0L4selfS189);
  _M0L4len2S190 = Moonbit_array_length(_M0L3strS191);
  if (_M0L6lengthS185 >= 0) {
    if (_M0L13bytes__offsetS184 >= 0) {
      if (_M0L2e1S183 < _M0L4len1S188) {
        if (_M0L11str__offsetS187 >= 0) {
          _if__result_3702 = _M0L2e2S186 < _M0L4len2S190;
        } else {
          _if__result_3702 = 0;
        }
      } else {
        _if__result_3702 = 0;
      }
    } else {
      _if__result_3702 = 0;
    }
  } else {
    _if__result_3702 = 0;
  }
  if (_if__result_3702) {
    int32_t _M0L16end__str__offsetS192 =
      _M0L11str__offsetS187 + _M0L6lengthS185;
    int32_t _M0L1iS193 = _M0L11str__offsetS187;
    int32_t _M0L1jS194 = _M0L13bytes__offsetS184;
    while (1) {
      if (_M0L1iS193 < _M0L16end__str__offsetS192) {
        int32_t _M0L6_2atmpS1765 = _M0L3strS191[_M0L1iS193];
        int32_t _M0L6_2atmpS1764 = (int32_t)_M0L6_2atmpS1765;
        uint32_t _M0L1cS195 = *(uint32_t*)&_M0L6_2atmpS1764;
        uint32_t _M0L6_2atmpS1760 = _M0L1cS195 & 255u;
        int32_t _M0L6_2atmpS1759;
        int32_t _M0L6_2atmpS1761;
        uint32_t _M0L6_2atmpS1763;
        int32_t _M0L6_2atmpS1762;
        int32_t _M0L6_2atmpS1766;
        int32_t _M0L6_2atmpS1767;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1759 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1760);
        if (
          _M0L1jS194 < 0 || _M0L1jS194 >= Moonbit_array_length(_M0L4selfS189)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS189[_M0L1jS194] = _M0L6_2atmpS1759;
        _M0L6_2atmpS1761 = _M0L1jS194 + 1;
        _M0L6_2atmpS1763 = _M0L1cS195 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1762 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1763);
        if (
          _M0L6_2atmpS1761 < 0
          || _M0L6_2atmpS1761 >= Moonbit_array_length(_M0L4selfS189)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS189[_M0L6_2atmpS1761] = _M0L6_2atmpS1762;
        _M0L6_2atmpS1766 = _M0L1iS193 + 1;
        _M0L6_2atmpS1767 = _M0L1jS194 + 2;
        _M0L1iS193 = _M0L6_2atmpS1766;
        _M0L1jS194 = _M0L6_2atmpS1767;
        continue;
      } else {
        moonbit_decref(_M0L3strS191);
        moonbit_decref(_M0L4selfS189);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS191);
    moonbit_decref(_M0L4selfS189);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS180,
  double _M0L3objS179
) {
  struct _M0TPB6Logger _M0L6_2atmpS1757;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1757
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS180
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS179, _M0L6_2atmpS1757);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS182,
  struct _M0TPC16string10StringView _M0L3objS181
) {
  struct _M0TPB6Logger _M0L6_2atmpS1758;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1758
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS182
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS181, _M0L6_2atmpS1758);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS125
) {
  int32_t _M0L6_2atmpS1756;
  struct _M0TPC16string10StringView _M0L7_2abindS124;
  moonbit_string_t _M0L7_2adataS126;
  int32_t _M0L8_2astartS127;
  int32_t _M0L6_2atmpS1755;
  int32_t _M0L6_2aendS128;
  int32_t _M0Lm9_2acursorS129;
  int32_t _M0Lm13accept__stateS130;
  int32_t _M0Lm10match__endS131;
  int32_t _M0Lm20match__tag__saver__0S132;
  int32_t _M0Lm20match__tag__saver__1S133;
  int32_t _M0Lm20match__tag__saver__2S134;
  int32_t _M0Lm20match__tag__saver__3S135;
  int32_t _M0Lm20match__tag__saver__4S136;
  int32_t _M0Lm6tag__0S137;
  int32_t _M0Lm6tag__1S138;
  int32_t _M0Lm9tag__1__1S139;
  int32_t _M0Lm9tag__1__2S140;
  int32_t _M0Lm6tag__3S141;
  int32_t _M0Lm6tag__2S142;
  int32_t _M0Lm9tag__2__1S143;
  int32_t _M0Lm6tag__4S144;
  int32_t _M0L6_2atmpS1713;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1756 = Moonbit_array_length(_M0L4reprS125);
  _M0L7_2abindS124
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1756, _M0L4reprS125
  };
  moonbit_incref(_M0L7_2abindS124.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS126 = _M0MPC16string10StringView4data(_M0L7_2abindS124);
  moonbit_incref(_M0L7_2abindS124.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS127
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS124);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1755 = _M0MPC16string10StringView6length(_M0L7_2abindS124);
  _M0L6_2aendS128 = _M0L8_2astartS127 + _M0L6_2atmpS1755;
  _M0Lm9_2acursorS129 = _M0L8_2astartS127;
  _M0Lm13accept__stateS130 = -1;
  _M0Lm10match__endS131 = -1;
  _M0Lm20match__tag__saver__0S132 = -1;
  _M0Lm20match__tag__saver__1S133 = -1;
  _M0Lm20match__tag__saver__2S134 = -1;
  _M0Lm20match__tag__saver__3S135 = -1;
  _M0Lm20match__tag__saver__4S136 = -1;
  _M0Lm6tag__0S137 = -1;
  _M0Lm6tag__1S138 = -1;
  _M0Lm9tag__1__1S139 = -1;
  _M0Lm9tag__1__2S140 = -1;
  _M0Lm6tag__3S141 = -1;
  _M0Lm6tag__2S142 = -1;
  _M0Lm9tag__2__1S143 = -1;
  _M0Lm6tag__4S144 = -1;
  _M0L6_2atmpS1713 = _M0Lm9_2acursorS129;
  if (_M0L6_2atmpS1713 < _M0L6_2aendS128) {
    int32_t _M0L6_2atmpS1715 = _M0Lm9_2acursorS129;
    int32_t _M0L6_2atmpS1714;
    moonbit_incref(_M0L7_2adataS126);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1714
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1715);
    if (_M0L6_2atmpS1714 == 64) {
      int32_t _M0L6_2atmpS1716 = _M0Lm9_2acursorS129;
      _M0Lm9_2acursorS129 = _M0L6_2atmpS1716 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1717;
        _M0Lm6tag__0S137 = _M0Lm9_2acursorS129;
        _M0L6_2atmpS1717 = _M0Lm9_2acursorS129;
        if (_M0L6_2atmpS1717 < _M0L6_2aendS128) {
          int32_t _M0L6_2atmpS1754 = _M0Lm9_2acursorS129;
          int32_t _M0L10next__charS152;
          int32_t _M0L6_2atmpS1718;
          moonbit_incref(_M0L7_2adataS126);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS152
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1754);
          _M0L6_2atmpS1718 = _M0Lm9_2acursorS129;
          _M0Lm9_2acursorS129 = _M0L6_2atmpS1718 + 1;
          if (_M0L10next__charS152 == 58) {
            int32_t _M0L6_2atmpS1719 = _M0Lm9_2acursorS129;
            if (_M0L6_2atmpS1719 < _M0L6_2aendS128) {
              int32_t _M0L6_2atmpS1720 = _M0Lm9_2acursorS129;
              int32_t _M0L12dispatch__15S153;
              _M0Lm9_2acursorS129 = _M0L6_2atmpS1720 + 1;
              _M0L12dispatch__15S153 = 0;
              loop__label__15_156:;
              while (1) {
                int32_t _M0L6_2atmpS1721;
                switch (_M0L12dispatch__15S153) {
                  case 3: {
                    int32_t _M0L6_2atmpS1724;
                    _M0Lm9tag__1__2S140 = _M0Lm9tag__1__1S139;
                    _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1724 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1724 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1729 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1725;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1729);
                      _M0L6_2atmpS1725 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1725 + 1;
                      if (_M0L10next__charS160 < 58) {
                        if (_M0L10next__charS160 < 48) {
                          goto join_159;
                        } else {
                          int32_t _M0L6_2atmpS1726;
                          _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                          _M0Lm9tag__2__1S143 = _M0Lm6tag__2S142;
                          _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                          _M0Lm6tag__3S141 = _M0Lm9_2acursorS129;
                          _M0L6_2atmpS1726 = _M0Lm9_2acursorS129;
                          if (_M0L6_2atmpS1726 < _M0L6_2aendS128) {
                            int32_t _M0L6_2atmpS1728 = _M0Lm9_2acursorS129;
                            int32_t _M0L10next__charS162;
                            int32_t _M0L6_2atmpS1727;
                            moonbit_incref(_M0L7_2adataS126);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS162
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1728);
                            _M0L6_2atmpS1727 = _M0Lm9_2acursorS129;
                            _M0Lm9_2acursorS129 = _M0L6_2atmpS1727 + 1;
                            if (_M0L10next__charS162 < 48) {
                              if (_M0L10next__charS162 == 45) {
                                goto join_154;
                              } else {
                                goto join_161;
                              }
                            } else if (_M0L10next__charS162 > 57) {
                              if (_M0L10next__charS162 < 59) {
                                _M0L12dispatch__15S153 = 3;
                                goto loop__label__15_156;
                              } else {
                                goto join_161;
                              }
                            } else {
                              _M0L12dispatch__15S153 = 6;
                              goto loop__label__15_156;
                            }
                            join_161:;
                            _M0L12dispatch__15S153 = 0;
                            goto loop__label__15_156;
                          } else {
                            goto join_145;
                          }
                        }
                      } else if (_M0L10next__charS160 > 58) {
                        goto join_159;
                      } else {
                        _M0L12dispatch__15S153 = 1;
                        goto loop__label__15_156;
                      }
                      join_159:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1730;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1730 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1730 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1732 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS164;
                      int32_t _M0L6_2atmpS1731;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS164
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1732);
                      _M0L6_2atmpS1731 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1731 + 1;
                      if (_M0L10next__charS164 < 58) {
                        if (_M0L10next__charS164 < 48) {
                          goto join_163;
                        } else {
                          _M0L12dispatch__15S153 = 2;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS164 > 58) {
                        goto join_163;
                      } else {
                        _M0L12dispatch__15S153 = 3;
                        goto loop__label__15_156;
                      }
                      join_163:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1733;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1733 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1733 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1735 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS165;
                      int32_t _M0L6_2atmpS1734;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS165
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1735);
                      _M0L6_2atmpS1734 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1734 + 1;
                      if (_M0L10next__charS165 == 58) {
                        _M0L12dispatch__15S153 = 1;
                        goto loop__label__15_156;
                      } else {
                        _M0L12dispatch__15S153 = 0;
                        goto loop__label__15_156;
                      }
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1736;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__4S144 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1736 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1736 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1744 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS167;
                      int32_t _M0L6_2atmpS1737;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS167
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1744);
                      _M0L6_2atmpS1737 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1737 + 1;
                      if (_M0L10next__charS167 < 58) {
                        if (_M0L10next__charS167 < 48) {
                          goto join_166;
                        } else {
                          _M0L12dispatch__15S153 = 4;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS167 > 58) {
                        goto join_166;
                      } else {
                        int32_t _M0L6_2atmpS1738;
                        _M0Lm9tag__1__2S140 = _M0Lm9tag__1__1S139;
                        _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                        _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                        _M0L6_2atmpS1738 = _M0Lm9_2acursorS129;
                        if (_M0L6_2atmpS1738 < _M0L6_2aendS128) {
                          int32_t _M0L6_2atmpS1743 = _M0Lm9_2acursorS129;
                          int32_t _M0L10next__charS169;
                          int32_t _M0L6_2atmpS1739;
                          moonbit_incref(_M0L7_2adataS126);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS169
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1743);
                          _M0L6_2atmpS1739 = _M0Lm9_2acursorS129;
                          _M0Lm9_2acursorS129 = _M0L6_2atmpS1739 + 1;
                          if (_M0L10next__charS169 < 58) {
                            if (_M0L10next__charS169 < 48) {
                              goto join_168;
                            } else {
                              int32_t _M0L6_2atmpS1740;
                              _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                              _M0Lm9tag__2__1S143 = _M0Lm6tag__2S142;
                              _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                              _M0L6_2atmpS1740 = _M0Lm9_2acursorS129;
                              if (_M0L6_2atmpS1740 < _M0L6_2aendS128) {
                                int32_t _M0L6_2atmpS1742 =
                                  _M0Lm9_2acursorS129;
                                int32_t _M0L10next__charS171;
                                int32_t _M0L6_2atmpS1741;
                                moonbit_incref(_M0L7_2adataS126);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS171
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1742);
                                _M0L6_2atmpS1741 = _M0Lm9_2acursorS129;
                                _M0Lm9_2acursorS129 = _M0L6_2atmpS1741 + 1;
                                if (_M0L10next__charS171 < 58) {
                                  if (_M0L10next__charS171 < 48) {
                                    goto join_170;
                                  } else {
                                    _M0L12dispatch__15S153 = 5;
                                    goto loop__label__15_156;
                                  }
                                } else if (_M0L10next__charS171 > 58) {
                                  goto join_170;
                                } else {
                                  _M0L12dispatch__15S153 = 3;
                                  goto loop__label__15_156;
                                }
                                join_170:;
                                _M0L12dispatch__15S153 = 0;
                                goto loop__label__15_156;
                              } else {
                                goto join_158;
                              }
                            }
                          } else if (_M0L10next__charS169 > 58) {
                            goto join_168;
                          } else {
                            _M0L12dispatch__15S153 = 1;
                            goto loop__label__15_156;
                          }
                          join_168:;
                          _M0L12dispatch__15S153 = 0;
                          goto loop__label__15_156;
                        } else {
                          goto join_145;
                        }
                      }
                      join_166:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1745;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1745 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1745 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1747 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS173;
                      int32_t _M0L6_2atmpS1746;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS173
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1747);
                      _M0L6_2atmpS1746 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1746 + 1;
                      if (_M0L10next__charS173 < 58) {
                        if (_M0L10next__charS173 < 48) {
                          goto join_172;
                        } else {
                          _M0L12dispatch__15S153 = 5;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS173 > 58) {
                        goto join_172;
                      } else {
                        _M0L12dispatch__15S153 = 3;
                        goto loop__label__15_156;
                      }
                      join_172:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_158;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1748;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__3S141 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1748 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1748 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1750 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS175;
                      int32_t _M0L6_2atmpS1749;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS175
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1750);
                      _M0L6_2atmpS1749 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1749 + 1;
                      if (_M0L10next__charS175 < 48) {
                        if (_M0L10next__charS175 == 45) {
                          goto join_154;
                        } else {
                          goto join_174;
                        }
                      } else if (_M0L10next__charS175 > 57) {
                        if (_M0L10next__charS175 < 59) {
                          _M0L12dispatch__15S153 = 3;
                          goto loop__label__15_156;
                        } else {
                          goto join_174;
                        }
                      } else {
                        _M0L12dispatch__15S153 = 6;
                        goto loop__label__15_156;
                      }
                      join_174:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1751;
                    _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1751 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1751 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1753 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS177;
                      int32_t _M0L6_2atmpS1752;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS177
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1753);
                      _M0L6_2atmpS1752 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1752 + 1;
                      if (_M0L10next__charS177 < 58) {
                        if (_M0L10next__charS177 < 48) {
                          goto join_176;
                        } else {
                          _M0L12dispatch__15S153 = 2;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS177 > 58) {
                        goto join_176;
                      } else {
                        _M0L12dispatch__15S153 = 1;
                        goto loop__label__15_156;
                      }
                      join_176:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  default: {
                    goto join_145;
                    break;
                  }
                }
                join_158:;
                _M0Lm6tag__1S138 = _M0Lm9tag__1__2S140;
                _M0Lm6tag__2S142 = _M0Lm9tag__2__1S143;
                _M0Lm20match__tag__saver__0S132 = _M0Lm6tag__0S137;
                _M0Lm20match__tag__saver__1S133 = _M0Lm6tag__1S138;
                _M0Lm20match__tag__saver__2S134 = _M0Lm6tag__2S142;
                _M0Lm20match__tag__saver__3S135 = _M0Lm6tag__3S141;
                _M0Lm20match__tag__saver__4S136 = _M0Lm6tag__4S144;
                _M0Lm13accept__stateS130 = 0;
                _M0Lm10match__endS131 = _M0Lm9_2acursorS129;
                goto join_145;
                join_154:;
                _M0Lm9tag__1__1S139 = _M0Lm9tag__1__2S140;
                _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                _M0Lm6tag__2S142 = _M0Lm9tag__2__1S143;
                _M0L6_2atmpS1721 = _M0Lm9_2acursorS129;
                if (_M0L6_2atmpS1721 < _M0L6_2aendS128) {
                  int32_t _M0L6_2atmpS1723 = _M0Lm9_2acursorS129;
                  int32_t _M0L10next__charS157;
                  int32_t _M0L6_2atmpS1722;
                  moonbit_incref(_M0L7_2adataS126);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS157
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1723);
                  _M0L6_2atmpS1722 = _M0Lm9_2acursorS129;
                  _M0Lm9_2acursorS129 = _M0L6_2atmpS1722 + 1;
                  if (_M0L10next__charS157 < 58) {
                    if (_M0L10next__charS157 < 48) {
                      goto join_155;
                    } else {
                      _M0L12dispatch__15S153 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS157 > 58) {
                    goto join_155;
                  } else {
                    _M0L12dispatch__15S153 = 1;
                    continue;
                  }
                  join_155:;
                  _M0L12dispatch__15S153 = 0;
                  continue;
                } else {
                  goto join_145;
                }
                break;
              }
            } else {
              goto join_145;
            }
          } else {
            continue;
          }
        } else {
          goto join_145;
        }
        break;
      }
    } else {
      goto join_145;
    }
  } else {
    goto join_145;
  }
  join_145:;
  switch (_M0Lm13accept__stateS130) {
    case 0: {
      int32_t _M0L6_2atmpS1712 = _M0Lm20match__tag__saver__1S133;
      int32_t _M0L6_2atmpS1711 = _M0L6_2atmpS1712 + 1;
      int64_t _M0L6_2atmpS1708 = (int64_t)_M0L6_2atmpS1711;
      int32_t _M0L6_2atmpS1710 = _M0Lm20match__tag__saver__2S134;
      int64_t _M0L6_2atmpS1709 = (int64_t)_M0L6_2atmpS1710;
      struct _M0TPC16string10StringView _M0L11start__lineS146;
      int32_t _M0L6_2atmpS1707;
      int32_t _M0L6_2atmpS1706;
      int64_t _M0L6_2atmpS1703;
      int32_t _M0L6_2atmpS1705;
      int64_t _M0L6_2atmpS1704;
      struct _M0TPC16string10StringView _M0L13start__columnS147;
      int32_t _M0L6_2atmpS1702;
      int64_t _M0L6_2atmpS1699;
      int32_t _M0L6_2atmpS1701;
      int64_t _M0L6_2atmpS1700;
      struct _M0TPC16string10StringView _M0L3pkgS148;
      int32_t _M0L6_2atmpS1698;
      int32_t _M0L6_2atmpS1697;
      int64_t _M0L6_2atmpS1694;
      int32_t _M0L6_2atmpS1696;
      int64_t _M0L6_2atmpS1695;
      struct _M0TPC16string10StringView _M0L8filenameS149;
      int32_t _M0L6_2atmpS1693;
      int32_t _M0L6_2atmpS1692;
      int64_t _M0L6_2atmpS1689;
      int32_t _M0L6_2atmpS1691;
      int64_t _M0L6_2atmpS1690;
      struct _M0TPC16string10StringView _M0L9end__lineS150;
      int32_t _M0L6_2atmpS1688;
      int32_t _M0L6_2atmpS1687;
      int64_t _M0L6_2atmpS1684;
      int32_t _M0L6_2atmpS1686;
      int64_t _M0L6_2atmpS1685;
      struct _M0TPC16string10StringView _M0L11end__columnS151;
      struct _M0TPB13SourceLocRepr* _block_3719;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS146
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1708, _M0L6_2atmpS1709);
      _M0L6_2atmpS1707 = _M0Lm20match__tag__saver__2S134;
      _M0L6_2atmpS1706 = _M0L6_2atmpS1707 + 1;
      _M0L6_2atmpS1703 = (int64_t)_M0L6_2atmpS1706;
      _M0L6_2atmpS1705 = _M0Lm20match__tag__saver__3S135;
      _M0L6_2atmpS1704 = (int64_t)_M0L6_2atmpS1705;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS147
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1703, _M0L6_2atmpS1704);
      _M0L6_2atmpS1702 = _M0L8_2astartS127 + 1;
      _M0L6_2atmpS1699 = (int64_t)_M0L6_2atmpS1702;
      _M0L6_2atmpS1701 = _M0Lm20match__tag__saver__0S132;
      _M0L6_2atmpS1700 = (int64_t)_M0L6_2atmpS1701;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS148
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1699, _M0L6_2atmpS1700);
      _M0L6_2atmpS1698 = _M0Lm20match__tag__saver__0S132;
      _M0L6_2atmpS1697 = _M0L6_2atmpS1698 + 1;
      _M0L6_2atmpS1694 = (int64_t)_M0L6_2atmpS1697;
      _M0L6_2atmpS1696 = _M0Lm20match__tag__saver__1S133;
      _M0L6_2atmpS1695 = (int64_t)_M0L6_2atmpS1696;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS149
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1694, _M0L6_2atmpS1695);
      _M0L6_2atmpS1693 = _M0Lm20match__tag__saver__3S135;
      _M0L6_2atmpS1692 = _M0L6_2atmpS1693 + 1;
      _M0L6_2atmpS1689 = (int64_t)_M0L6_2atmpS1692;
      _M0L6_2atmpS1691 = _M0Lm20match__tag__saver__4S136;
      _M0L6_2atmpS1690 = (int64_t)_M0L6_2atmpS1691;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS150
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1689, _M0L6_2atmpS1690);
      _M0L6_2atmpS1688 = _M0Lm20match__tag__saver__4S136;
      _M0L6_2atmpS1687 = _M0L6_2atmpS1688 + 1;
      _M0L6_2atmpS1684 = (int64_t)_M0L6_2atmpS1687;
      _M0L6_2atmpS1686 = _M0Lm10match__endS131;
      _M0L6_2atmpS1685 = (int64_t)_M0L6_2atmpS1686;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS151
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1684, _M0L6_2atmpS1685);
      _block_3719
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3719)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3719->$0_0 = _M0L3pkgS148.$0;
      _block_3719->$0_1 = _M0L3pkgS148.$1;
      _block_3719->$0_2 = _M0L3pkgS148.$2;
      _block_3719->$1_0 = _M0L8filenameS149.$0;
      _block_3719->$1_1 = _M0L8filenameS149.$1;
      _block_3719->$1_2 = _M0L8filenameS149.$2;
      _block_3719->$2_0 = _M0L11start__lineS146.$0;
      _block_3719->$2_1 = _M0L11start__lineS146.$1;
      _block_3719->$2_2 = _M0L11start__lineS146.$2;
      _block_3719->$3_0 = _M0L13start__columnS147.$0;
      _block_3719->$3_1 = _M0L13start__columnS147.$1;
      _block_3719->$3_2 = _M0L13start__columnS147.$2;
      _block_3719->$4_0 = _M0L9end__lineS150.$0;
      _block_3719->$4_1 = _M0L9end__lineS150.$1;
      _block_3719->$4_2 = _M0L9end__lineS150.$2;
      _block_3719->$5_0 = _M0L11end__columnS151.$0;
      _block_3719->$5_1 = _M0L11end__columnS151.$1;
      _block_3719->$5_2 = _M0L11end__columnS151.$2;
      return _block_3719;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS126);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS122,
  int32_t _M0L5indexS123
) {
  int32_t _M0L3lenS121;
  int32_t _if__result_3720;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS121 = _M0L4selfS122->$1;
  if (_M0L5indexS123 >= 0) {
    _if__result_3720 = _M0L5indexS123 < _M0L3lenS121;
  } else {
    _if__result_3720 = 0;
  }
  if (_if__result_3720) {
    moonbit_string_t* _M0L6_2atmpS1683;
    moonbit_string_t _M0L6_2atmpS3414;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1683 = _M0MPC15array5Array6bufferGsE(_M0L4selfS122);
    if (
      _M0L5indexS123 < 0
      || _M0L5indexS123 >= Moonbit_array_length(_M0L6_2atmpS1683)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3414 = (moonbit_string_t)_M0L6_2atmpS1683[_M0L5indexS123];
    moonbit_incref(_M0L6_2atmpS3414);
    moonbit_decref(_M0L6_2atmpS1683);
    return _M0L6_2atmpS3414;
  } else {
    moonbit_decref(_M0L4selfS122);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS118
) {
  moonbit_string_t* _M0L8_2afieldS3415;
  int32_t _M0L6_2acntS3544;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3415 = _M0L4selfS118->$0;
  _M0L6_2acntS3544 = Moonbit_object_header(_M0L4selfS118)->rc;
  if (_M0L6_2acntS3544 > 1) {
    int32_t _M0L11_2anew__cntS3545 = _M0L6_2acntS3544 - 1;
    Moonbit_object_header(_M0L4selfS118)->rc = _M0L11_2anew__cntS3545;
    moonbit_incref(_M0L8_2afieldS3415);
  } else if (_M0L6_2acntS3544 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS118);
  }
  return _M0L8_2afieldS3415;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS119
) {
  struct _M0TUsiE** _M0L8_2afieldS3416;
  int32_t _M0L6_2acntS3546;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3416 = _M0L4selfS119->$0;
  _M0L6_2acntS3546 = Moonbit_object_header(_M0L4selfS119)->rc;
  if (_M0L6_2acntS3546 > 1) {
    int32_t _M0L11_2anew__cntS3547 = _M0L6_2acntS3546 - 1;
    Moonbit_object_header(_M0L4selfS119)->rc = _M0L11_2anew__cntS3547;
    moonbit_incref(_M0L8_2afieldS3416);
  } else if (_M0L6_2acntS3546 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS119);
  }
  return _M0L8_2afieldS3416;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS120
) {
  void** _M0L8_2afieldS3417;
  int32_t _M0L6_2acntS3548;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3417 = _M0L4selfS120->$0;
  _M0L6_2acntS3548 = Moonbit_object_header(_M0L4selfS120)->rc;
  if (_M0L6_2acntS3548 > 1) {
    int32_t _M0L11_2anew__cntS3549 = _M0L6_2acntS3548 - 1;
    Moonbit_object_header(_M0L4selfS120)->rc = _M0L11_2anew__cntS3549;
    moonbit_incref(_M0L8_2afieldS3417);
  } else if (_M0L6_2acntS3548 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS120);
  }
  return _M0L8_2afieldS3417;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS117) {
  struct _M0TPB13StringBuilder* _M0L3bufS116;
  struct _M0TPB6Logger _M0L6_2atmpS1682;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS116 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS116);
  _M0L6_2atmpS1682
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS116
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS117, _M0L6_2atmpS1682);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS116);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS113,
  int32_t _M0L5indexS114
) {
  int32_t _M0L2c1S112;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S112 = _M0L4selfS113[_M0L5indexS114];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S112)) {
    int32_t _M0L6_2atmpS1681 = _M0L5indexS114 + 1;
    int32_t _M0L6_2atmpS3418 = _M0L4selfS113[_M0L6_2atmpS1681];
    int32_t _M0L2c2S115;
    int32_t _M0L6_2atmpS1679;
    int32_t _M0L6_2atmpS1680;
    moonbit_decref(_M0L4selfS113);
    _M0L2c2S115 = _M0L6_2atmpS3418;
    _M0L6_2atmpS1679 = (int32_t)_M0L2c1S112;
    _M0L6_2atmpS1680 = (int32_t)_M0L2c2S115;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1679, _M0L6_2atmpS1680);
  } else {
    moonbit_decref(_M0L4selfS113);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S112);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS111) {
  int32_t _M0L6_2atmpS1678;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1678 = (int32_t)_M0L4selfS111;
  return _M0L6_2atmpS1678;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS109,
  int32_t _M0L8trailingS110
) {
  int32_t _M0L6_2atmpS1677;
  int32_t _M0L6_2atmpS1676;
  int32_t _M0L6_2atmpS1675;
  int32_t _M0L6_2atmpS1674;
  int32_t _M0L6_2atmpS1673;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1677 = _M0L7leadingS109 - 55296;
  _M0L6_2atmpS1676 = _M0L6_2atmpS1677 * 1024;
  _M0L6_2atmpS1675 = _M0L6_2atmpS1676 + _M0L8trailingS110;
  _M0L6_2atmpS1674 = _M0L6_2atmpS1675 - 56320;
  _M0L6_2atmpS1673 = _M0L6_2atmpS1674 + 65536;
  return _M0L6_2atmpS1673;
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
  int32_t _M0L3lenS1668;
  int32_t _M0L6_2atmpS1667;
  moonbit_bytes_t _M0L8_2afieldS3419;
  moonbit_bytes_t _M0L4dataS1671;
  int32_t _M0L3lenS1672;
  int32_t _M0L3incS105;
  int32_t _M0L3lenS1670;
  int32_t _M0L6_2atmpS1669;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1668 = _M0L4selfS104->$1;
  _M0L6_2atmpS1667 = _M0L3lenS1668 + 4;
  moonbit_incref(_M0L4selfS104);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS104, _M0L6_2atmpS1667);
  _M0L8_2afieldS3419 = _M0L4selfS104->$0;
  _M0L4dataS1671 = _M0L8_2afieldS3419;
  _M0L3lenS1672 = _M0L4selfS104->$1;
  moonbit_incref(_M0L4dataS1671);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS105
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1671, _M0L3lenS1672, _M0L2chS106);
  _M0L3lenS1670 = _M0L4selfS104->$1;
  _M0L6_2atmpS1669 = _M0L3lenS1670 + _M0L3incS105;
  _M0L4selfS104->$1 = _M0L6_2atmpS1669;
  moonbit_decref(_M0L4selfS104);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS99,
  int32_t _M0L8requiredS100
) {
  moonbit_bytes_t _M0L8_2afieldS3423;
  moonbit_bytes_t _M0L4dataS1666;
  int32_t _M0L6_2atmpS3422;
  int32_t _M0L12current__lenS98;
  int32_t _M0Lm13enough__spaceS101;
  int32_t _M0L6_2atmpS1664;
  int32_t _M0L6_2atmpS1665;
  moonbit_bytes_t _M0L9new__dataS103;
  moonbit_bytes_t _M0L8_2afieldS3421;
  moonbit_bytes_t _M0L4dataS1662;
  int32_t _M0L3lenS1663;
  moonbit_bytes_t _M0L6_2aoldS3420;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3423 = _M0L4selfS99->$0;
  _M0L4dataS1666 = _M0L8_2afieldS3423;
  _M0L6_2atmpS3422 = Moonbit_array_length(_M0L4dataS1666);
  _M0L12current__lenS98 = _M0L6_2atmpS3422;
  if (_M0L8requiredS100 <= _M0L12current__lenS98) {
    moonbit_decref(_M0L4selfS99);
    return 0;
  }
  _M0Lm13enough__spaceS101 = _M0L12current__lenS98;
  while (1) {
    int32_t _M0L6_2atmpS1660 = _M0Lm13enough__spaceS101;
    if (_M0L6_2atmpS1660 < _M0L8requiredS100) {
      int32_t _M0L6_2atmpS1661 = _M0Lm13enough__spaceS101;
      _M0Lm13enough__spaceS101 = _M0L6_2atmpS1661 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1664 = _M0Lm13enough__spaceS101;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1665 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS103
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1664, _M0L6_2atmpS1665);
  _M0L8_2afieldS3421 = _M0L4selfS99->$0;
  _M0L4dataS1662 = _M0L8_2afieldS3421;
  _M0L3lenS1663 = _M0L4selfS99->$1;
  moonbit_incref(_M0L4dataS1662);
  moonbit_incref(_M0L9new__dataS103);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS103, 0, _M0L4dataS1662, 0, _M0L3lenS1663);
  _M0L6_2aoldS3420 = _M0L4selfS99->$0;
  moonbit_decref(_M0L6_2aoldS3420);
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
    uint32_t _M0L6_2atmpS1643 = _M0L4codeS91 & 255u;
    int32_t _M0L6_2atmpS1642;
    int32_t _M0L6_2atmpS1644;
    uint32_t _M0L6_2atmpS1646;
    int32_t _M0L6_2atmpS1645;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1642 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1643);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1642;
    _M0L6_2atmpS1644 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1646 = _M0L4codeS91 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1645 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1646);
    if (
      _M0L6_2atmpS1644 < 0
      || _M0L6_2atmpS1644 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1644] = _M0L6_2atmpS1645;
    moonbit_decref(_M0L4selfS93);
    return 2;
  } else if (_M0L4codeS91 < 1114112u) {
    uint32_t _M0L2hiS95 = _M0L4codeS91 - 65536u;
    uint32_t _M0L6_2atmpS1659 = _M0L2hiS95 >> 10;
    uint32_t _M0L2loS96 = _M0L6_2atmpS1659 | 55296u;
    uint32_t _M0L6_2atmpS1658 = _M0L2hiS95 & 1023u;
    uint32_t _M0L2hiS97 = _M0L6_2atmpS1658 | 56320u;
    uint32_t _M0L6_2atmpS1648 = _M0L2loS96 & 255u;
    int32_t _M0L6_2atmpS1647;
    int32_t _M0L6_2atmpS1649;
    uint32_t _M0L6_2atmpS1651;
    int32_t _M0L6_2atmpS1650;
    int32_t _M0L6_2atmpS1652;
    uint32_t _M0L6_2atmpS1654;
    int32_t _M0L6_2atmpS1653;
    int32_t _M0L6_2atmpS1655;
    uint32_t _M0L6_2atmpS1657;
    int32_t _M0L6_2atmpS1656;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1647 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1648);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1647;
    _M0L6_2atmpS1649 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1651 = _M0L2loS96 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1650 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1651);
    if (
      _M0L6_2atmpS1649 < 0
      || _M0L6_2atmpS1649 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1649] = _M0L6_2atmpS1650;
    _M0L6_2atmpS1652 = _M0L6offsetS94 + 2;
    _M0L6_2atmpS1654 = _M0L2hiS97 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1653 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1654);
    if (
      _M0L6_2atmpS1652 < 0
      || _M0L6_2atmpS1652 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1652] = _M0L6_2atmpS1653;
    _M0L6_2atmpS1655 = _M0L6offsetS94 + 3;
    _M0L6_2atmpS1657 = _M0L2hiS97 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1656 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1657);
    if (
      _M0L6_2atmpS1655 < 0
      || _M0L6_2atmpS1655 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1655] = _M0L6_2atmpS1656;
    moonbit_decref(_M0L4selfS93);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS93);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_74.data, (moonbit_string_t)moonbit_string_literal_75.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS90) {
  int32_t _M0L6_2atmpS1641;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1641 = *(int32_t*)&_M0L4selfS90;
  return _M0L6_2atmpS1641 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1640;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1640 = _M0L4selfS89;
  return *(uint32_t*)&_M0L6_2atmpS1640;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS88
) {
  moonbit_bytes_t _M0L8_2afieldS3425;
  moonbit_bytes_t _M0L4dataS1639;
  moonbit_bytes_t _M0L6_2atmpS1636;
  int32_t _M0L8_2afieldS3424;
  int32_t _M0L3lenS1638;
  int64_t _M0L6_2atmpS1637;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3425 = _M0L4selfS88->$0;
  _M0L4dataS1639 = _M0L8_2afieldS3425;
  moonbit_incref(_M0L4dataS1639);
  _M0L6_2atmpS1636 = _M0L4dataS1639;
  _M0L8_2afieldS3424 = _M0L4selfS88->$1;
  moonbit_decref(_M0L4selfS88);
  _M0L3lenS1638 = _M0L8_2afieldS3424;
  _M0L6_2atmpS1637 = (int64_t)_M0L3lenS1638;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1636, 0, _M0L6_2atmpS1637);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS83,
  int32_t _M0L6offsetS87,
  int64_t _M0L6lengthS85
) {
  int32_t _M0L3lenS82;
  int32_t _M0L6lengthS84;
  int32_t _if__result_3722;
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
      int32_t _M0L6_2atmpS1635 = _M0L6offsetS87 + _M0L6lengthS84;
      _if__result_3722 = _M0L6_2atmpS1635 <= _M0L3lenS82;
    } else {
      _if__result_3722 = 0;
    }
  } else {
    _if__result_3722 = 0;
  }
  if (_if__result_3722) {
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
  struct _M0TPB13StringBuilder* _block_3723;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS80 < 1) {
    _M0L7initialS79 = 1;
  } else {
    _M0L7initialS79 = _M0L10size__hintS80;
  }
  _M0L4dataS81 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS79, 0);
  _block_3723
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3723)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3723->$0 = _M0L4dataS81;
  _block_3723->$1 = 0;
  return _block_3723;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS78) {
  int32_t _M0L6_2atmpS1634;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1634 = (int32_t)_M0L4selfS78;
  return _M0L6_2atmpS1634;
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
  int32_t _if__result_3724;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS27 == _M0L3srcS28) {
    _if__result_3724 = _M0L11dst__offsetS29 < _M0L11src__offsetS30;
  } else {
    _if__result_3724 = 0;
  }
  if (_if__result_3724) {
    int32_t _M0L1iS31 = 0;
    while (1) {
      if (_M0L1iS31 < _M0L3lenS32) {
        int32_t _M0L6_2atmpS1598 = _M0L11dst__offsetS29 + _M0L1iS31;
        int32_t _M0L6_2atmpS1600 = _M0L11src__offsetS30 + _M0L1iS31;
        int32_t _M0L6_2atmpS1599;
        int32_t _M0L6_2atmpS1601;
        if (
          _M0L6_2atmpS1600 < 0
          || _M0L6_2atmpS1600 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1599 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1600];
        if (
          _M0L6_2atmpS1598 < 0
          || _M0L6_2atmpS1598 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1598] = _M0L6_2atmpS1599;
        _M0L6_2atmpS1601 = _M0L1iS31 + 1;
        _M0L1iS31 = _M0L6_2atmpS1601;
        continue;
      } else {
        moonbit_decref(_M0L3srcS28);
        moonbit_decref(_M0L3dstS27);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1606 = _M0L3lenS32 - 1;
    int32_t _M0L1iS34 = _M0L6_2atmpS1606;
    while (1) {
      if (_M0L1iS34 >= 0) {
        int32_t _M0L6_2atmpS1602 = _M0L11dst__offsetS29 + _M0L1iS34;
        int32_t _M0L6_2atmpS1604 = _M0L11src__offsetS30 + _M0L1iS34;
        int32_t _M0L6_2atmpS1603;
        int32_t _M0L6_2atmpS1605;
        if (
          _M0L6_2atmpS1604 < 0
          || _M0L6_2atmpS1604 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1603 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1604];
        if (
          _M0L6_2atmpS1602 < 0
          || _M0L6_2atmpS1602 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1602] = _M0L6_2atmpS1603;
        _M0L6_2atmpS1605 = _M0L1iS34 - 1;
        _M0L1iS34 = _M0L6_2atmpS1605;
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
  int32_t _if__result_3727;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS36 == _M0L3srcS37) {
    _if__result_3727 = _M0L11dst__offsetS38 < _M0L11src__offsetS39;
  } else {
    _if__result_3727 = 0;
  }
  if (_if__result_3727) {
    int32_t _M0L1iS40 = 0;
    while (1) {
      if (_M0L1iS40 < _M0L3lenS41) {
        int32_t _M0L6_2atmpS1607 = _M0L11dst__offsetS38 + _M0L1iS40;
        int32_t _M0L6_2atmpS1609 = _M0L11src__offsetS39 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS3427;
        moonbit_string_t _M0L6_2atmpS1608;
        moonbit_string_t _M0L6_2aoldS3426;
        int32_t _M0L6_2atmpS1610;
        if (
          _M0L6_2atmpS1609 < 0
          || _M0L6_2atmpS1609 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3427 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1609];
        _M0L6_2atmpS1608 = _M0L6_2atmpS3427;
        if (
          _M0L6_2atmpS1607 < 0
          || _M0L6_2atmpS1607 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3426 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1607];
        moonbit_incref(_M0L6_2atmpS1608);
        moonbit_decref(_M0L6_2aoldS3426);
        _M0L3dstS36[_M0L6_2atmpS1607] = _M0L6_2atmpS1608;
        _M0L6_2atmpS1610 = _M0L1iS40 + 1;
        _M0L1iS40 = _M0L6_2atmpS1610;
        continue;
      } else {
        moonbit_decref(_M0L3srcS37);
        moonbit_decref(_M0L3dstS36);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1615 = _M0L3lenS41 - 1;
    int32_t _M0L1iS43 = _M0L6_2atmpS1615;
    while (1) {
      if (_M0L1iS43 >= 0) {
        int32_t _M0L6_2atmpS1611 = _M0L11dst__offsetS38 + _M0L1iS43;
        int32_t _M0L6_2atmpS1613 = _M0L11src__offsetS39 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS3429;
        moonbit_string_t _M0L6_2atmpS1612;
        moonbit_string_t _M0L6_2aoldS3428;
        int32_t _M0L6_2atmpS1614;
        if (
          _M0L6_2atmpS1613 < 0
          || _M0L6_2atmpS1613 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3429 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1613];
        _M0L6_2atmpS1612 = _M0L6_2atmpS3429;
        if (
          _M0L6_2atmpS1611 < 0
          || _M0L6_2atmpS1611 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3428 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1611];
        moonbit_incref(_M0L6_2atmpS1612);
        moonbit_decref(_M0L6_2aoldS3428);
        _M0L3dstS36[_M0L6_2atmpS1611] = _M0L6_2atmpS1612;
        _M0L6_2atmpS1614 = _M0L1iS43 - 1;
        _M0L1iS43 = _M0L6_2atmpS1614;
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
  int32_t _if__result_3730;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS45 == _M0L3srcS46) {
    _if__result_3730 = _M0L11dst__offsetS47 < _M0L11src__offsetS48;
  } else {
    _if__result_3730 = 0;
  }
  if (_if__result_3730) {
    int32_t _M0L1iS49 = 0;
    while (1) {
      if (_M0L1iS49 < _M0L3lenS50) {
        int32_t _M0L6_2atmpS1616 = _M0L11dst__offsetS47 + _M0L1iS49;
        int32_t _M0L6_2atmpS1618 = _M0L11src__offsetS48 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS3431;
        struct _M0TUsiE* _M0L6_2atmpS1617;
        struct _M0TUsiE* _M0L6_2aoldS3430;
        int32_t _M0L6_2atmpS1619;
        if (
          _M0L6_2atmpS1618 < 0
          || _M0L6_2atmpS1618 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3431 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1618];
        _M0L6_2atmpS1617 = _M0L6_2atmpS3431;
        if (
          _M0L6_2atmpS1616 < 0
          || _M0L6_2atmpS1616 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3430 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1616];
        if (_M0L6_2atmpS1617) {
          moonbit_incref(_M0L6_2atmpS1617);
        }
        if (_M0L6_2aoldS3430) {
          moonbit_decref(_M0L6_2aoldS3430);
        }
        _M0L3dstS45[_M0L6_2atmpS1616] = _M0L6_2atmpS1617;
        _M0L6_2atmpS1619 = _M0L1iS49 + 1;
        _M0L1iS49 = _M0L6_2atmpS1619;
        continue;
      } else {
        moonbit_decref(_M0L3srcS46);
        moonbit_decref(_M0L3dstS45);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1624 = _M0L3lenS50 - 1;
    int32_t _M0L1iS52 = _M0L6_2atmpS1624;
    while (1) {
      if (_M0L1iS52 >= 0) {
        int32_t _M0L6_2atmpS1620 = _M0L11dst__offsetS47 + _M0L1iS52;
        int32_t _M0L6_2atmpS1622 = _M0L11src__offsetS48 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS3433;
        struct _M0TUsiE* _M0L6_2atmpS1621;
        struct _M0TUsiE* _M0L6_2aoldS3432;
        int32_t _M0L6_2atmpS1623;
        if (
          _M0L6_2atmpS1622 < 0
          || _M0L6_2atmpS1622 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3433 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1622];
        _M0L6_2atmpS1621 = _M0L6_2atmpS3433;
        if (
          _M0L6_2atmpS1620 < 0
          || _M0L6_2atmpS1620 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3432 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1620];
        if (_M0L6_2atmpS1621) {
          moonbit_incref(_M0L6_2atmpS1621);
        }
        if (_M0L6_2aoldS3432) {
          moonbit_decref(_M0L6_2aoldS3432);
        }
        _M0L3dstS45[_M0L6_2atmpS1620] = _M0L6_2atmpS1621;
        _M0L6_2atmpS1623 = _M0L1iS52 - 1;
        _M0L1iS52 = _M0L6_2atmpS1623;
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
  int32_t _if__result_3733;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS54 == _M0L3srcS55) {
    _if__result_3733 = _M0L11dst__offsetS56 < _M0L11src__offsetS57;
  } else {
    _if__result_3733 = 0;
  }
  if (_if__result_3733) {
    int32_t _M0L1iS58 = 0;
    while (1) {
      if (_M0L1iS58 < _M0L3lenS59) {
        int32_t _M0L6_2atmpS1625 = _M0L11dst__offsetS56 + _M0L1iS58;
        int32_t _M0L6_2atmpS1627 = _M0L11src__offsetS57 + _M0L1iS58;
        void* _M0L6_2atmpS3435;
        void* _M0L6_2atmpS1626;
        void* _M0L6_2aoldS3434;
        int32_t _M0L6_2atmpS1628;
        if (
          _M0L6_2atmpS1627 < 0
          || _M0L6_2atmpS1627 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3435 = (void*)_M0L3srcS55[_M0L6_2atmpS1627];
        _M0L6_2atmpS1626 = _M0L6_2atmpS3435;
        if (
          _M0L6_2atmpS1625 < 0
          || _M0L6_2atmpS1625 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3434 = (void*)_M0L3dstS54[_M0L6_2atmpS1625];
        moonbit_incref(_M0L6_2atmpS1626);
        moonbit_decref(_M0L6_2aoldS3434);
        _M0L3dstS54[_M0L6_2atmpS1625] = _M0L6_2atmpS1626;
        _M0L6_2atmpS1628 = _M0L1iS58 + 1;
        _M0L1iS58 = _M0L6_2atmpS1628;
        continue;
      } else {
        moonbit_decref(_M0L3srcS55);
        moonbit_decref(_M0L3dstS54);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1633 = _M0L3lenS59 - 1;
    int32_t _M0L1iS61 = _M0L6_2atmpS1633;
    while (1) {
      if (_M0L1iS61 >= 0) {
        int32_t _M0L6_2atmpS1629 = _M0L11dst__offsetS56 + _M0L1iS61;
        int32_t _M0L6_2atmpS1631 = _M0L11src__offsetS57 + _M0L1iS61;
        void* _M0L6_2atmpS3437;
        void* _M0L6_2atmpS1630;
        void* _M0L6_2aoldS3436;
        int32_t _M0L6_2atmpS1632;
        if (
          _M0L6_2atmpS1631 < 0
          || _M0L6_2atmpS1631 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3437 = (void*)_M0L3srcS55[_M0L6_2atmpS1631];
        _M0L6_2atmpS1630 = _M0L6_2atmpS3437;
        if (
          _M0L6_2atmpS1629 < 0
          || _M0L6_2atmpS1629 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3436 = (void*)_M0L3dstS54[_M0L6_2atmpS1629];
        moonbit_incref(_M0L6_2atmpS1630);
        moonbit_decref(_M0L6_2aoldS3436);
        _M0L3dstS54[_M0L6_2atmpS1629] = _M0L6_2atmpS1630;
        _M0L6_2atmpS1632 = _M0L1iS61 - 1;
        _M0L1iS61 = _M0L6_2atmpS1632;
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
  moonbit_string_t _M0L6_2atmpS1582;
  moonbit_string_t _M0L6_2atmpS3440;
  moonbit_string_t _M0L6_2atmpS1580;
  moonbit_string_t _M0L6_2atmpS1581;
  moonbit_string_t _M0L6_2atmpS3439;
  moonbit_string_t _M0L6_2atmpS1579;
  moonbit_string_t _M0L6_2atmpS3438;
  moonbit_string_t _M0L6_2atmpS1578;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1582 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3440
  = moonbit_add_string(_M0L6_2atmpS1582, (moonbit_string_t)moonbit_string_literal_76.data);
  moonbit_decref(_M0L6_2atmpS1582);
  _M0L6_2atmpS1580 = _M0L6_2atmpS3440;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1581
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3439 = moonbit_add_string(_M0L6_2atmpS1580, _M0L6_2atmpS1581);
  moonbit_decref(_M0L6_2atmpS1580);
  moonbit_decref(_M0L6_2atmpS1581);
  _M0L6_2atmpS1579 = _M0L6_2atmpS3439;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3438
  = moonbit_add_string(_M0L6_2atmpS1579, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS1579);
  _M0L6_2atmpS1578 = _M0L6_2atmpS3438;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1578);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1587;
  moonbit_string_t _M0L6_2atmpS3443;
  moonbit_string_t _M0L6_2atmpS1585;
  moonbit_string_t _M0L6_2atmpS1586;
  moonbit_string_t _M0L6_2atmpS3442;
  moonbit_string_t _M0L6_2atmpS1584;
  moonbit_string_t _M0L6_2atmpS3441;
  moonbit_string_t _M0L6_2atmpS1583;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1587 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3443
  = moonbit_add_string(_M0L6_2atmpS1587, (moonbit_string_t)moonbit_string_literal_76.data);
  moonbit_decref(_M0L6_2atmpS1587);
  _M0L6_2atmpS1585 = _M0L6_2atmpS3443;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1586
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3442 = moonbit_add_string(_M0L6_2atmpS1585, _M0L6_2atmpS1586);
  moonbit_decref(_M0L6_2atmpS1585);
  moonbit_decref(_M0L6_2atmpS1586);
  _M0L6_2atmpS1584 = _M0L6_2atmpS3442;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3441
  = moonbit_add_string(_M0L6_2atmpS1584, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS1584);
  _M0L6_2atmpS1583 = _M0L6_2atmpS3441;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1583);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS1592;
  moonbit_string_t _M0L6_2atmpS3446;
  moonbit_string_t _M0L6_2atmpS1590;
  moonbit_string_t _M0L6_2atmpS1591;
  moonbit_string_t _M0L6_2atmpS3445;
  moonbit_string_t _M0L6_2atmpS1589;
  moonbit_string_t _M0L6_2atmpS3444;
  moonbit_string_t _M0L6_2atmpS1588;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1592 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3446
  = moonbit_add_string(_M0L6_2atmpS1592, (moonbit_string_t)moonbit_string_literal_76.data);
  moonbit_decref(_M0L6_2atmpS1592);
  _M0L6_2atmpS1590 = _M0L6_2atmpS3446;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1591
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3445 = moonbit_add_string(_M0L6_2atmpS1590, _M0L6_2atmpS1591);
  moonbit_decref(_M0L6_2atmpS1590);
  moonbit_decref(_M0L6_2atmpS1591);
  _M0L6_2atmpS1589 = _M0L6_2atmpS3445;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3444
  = moonbit_add_string(_M0L6_2atmpS1589, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS1589);
  _M0L6_2atmpS1588 = _M0L6_2atmpS3444;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1588);
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS25,
  moonbit_string_t _M0L3locS26
) {
  moonbit_string_t _M0L6_2atmpS1597;
  moonbit_string_t _M0L6_2atmpS3449;
  moonbit_string_t _M0L6_2atmpS1595;
  moonbit_string_t _M0L6_2atmpS1596;
  moonbit_string_t _M0L6_2atmpS3448;
  moonbit_string_t _M0L6_2atmpS1594;
  moonbit_string_t _M0L6_2atmpS3447;
  moonbit_string_t _M0L6_2atmpS1593;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1597 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3449
  = moonbit_add_string(_M0L6_2atmpS1597, (moonbit_string_t)moonbit_string_literal_76.data);
  moonbit_decref(_M0L6_2atmpS1597);
  _M0L6_2atmpS1595 = _M0L6_2atmpS3449;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1596
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3448 = moonbit_add_string(_M0L6_2atmpS1595, _M0L6_2atmpS1596);
  moonbit_decref(_M0L6_2atmpS1595);
  moonbit_decref(_M0L6_2atmpS1596);
  _M0L6_2atmpS1594 = _M0L6_2atmpS3448;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3447
  = moonbit_add_string(_M0L6_2atmpS1594, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS1594);
  _M0L6_2atmpS1593 = _M0L6_2atmpS3447;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1593);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS17,
  uint32_t _M0L5valueS18
) {
  uint32_t _M0L3accS1577;
  uint32_t _M0L6_2atmpS1576;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1577 = _M0L4selfS17->$0;
  _M0L6_2atmpS1576 = _M0L3accS1577 + 4u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1576;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS17, _M0L5valueS18);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5inputS16
) {
  uint32_t _M0L3accS1574;
  uint32_t _M0L6_2atmpS1575;
  uint32_t _M0L6_2atmpS1573;
  uint32_t _M0L6_2atmpS1572;
  uint32_t _M0L6_2atmpS1571;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1574 = _M0L4selfS15->$0;
  _M0L6_2atmpS1575 = _M0L5inputS16 * 3266489917u;
  _M0L6_2atmpS1573 = _M0L3accS1574 + _M0L6_2atmpS1575;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1572 = _M0FPB4rotl(_M0L6_2atmpS1573, 17);
  _M0L6_2atmpS1571 = _M0L6_2atmpS1572 * 668265263u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1571;
  moonbit_decref(_M0L4selfS15);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS13, int32_t _M0L1rS14) {
  uint32_t _M0L6_2atmpS1568;
  int32_t _M0L6_2atmpS1570;
  uint32_t _M0L6_2atmpS1569;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1568 = _M0L1xS13 << (_M0L1rS14 & 31);
  _M0L6_2atmpS1570 = 32 - _M0L1rS14;
  _M0L6_2atmpS1569 = _M0L1xS13 >> (_M0L6_2atmpS1570 & 31);
  return _M0L6_2atmpS1568 | _M0L6_2atmpS1569;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S9,
  struct _M0TPB6Logger _M0L10_2ax__4934S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS3450;
  int32_t _M0L6_2acntS3550;
  moonbit_string_t _M0L15_2a_2aarg__4935S11;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S9;
  _M0L8_2afieldS3450 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS3550 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS3550 > 1) {
    int32_t _M0L11_2anew__cntS3551 = _M0L6_2acntS3550 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS3551;
    moonbit_incref(_M0L8_2afieldS3450);
  } else if (_M0L6_2acntS3550 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__4935S11 = _M0L8_2afieldS3450;
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_77.data);
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S12, _M0L15_2a_2aarg__4935S11);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_78.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS8) {
  void* _block_3736;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3736 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3736)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3736)->$0 = _M0L4selfS8;
  return _block_3736;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS7) {
  void* _block_3737;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3737 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3737)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3737)->$0 = _M0L5arrayS7;
  return _block_3737;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1353) {
  switch (Moonbit_object_tag(_M0L4_2aeS1353)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1353);
      return (moonbit_string_t)moonbit_string_literal_79.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1353);
      return (moonbit_string_t)moonbit_string_literal_80.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1353);
      return (moonbit_string_t)moonbit_string_literal_81.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1353);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1353);
      return (moonbit_string_t)moonbit_string_literal_82.data;
      break;
    }
  }
}

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE(
  void* _M0L11_2aobj__ptrS1373
) {
  moonbit_string_t _M0L7_2aselfS1372 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1373;
  return _M0IPC16option6OptionPB6ToJson8to__jsonGsE(_M0L7_2aselfS1372);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1371,
  int32_t _M0L8_2aparamS1370
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1369 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1371;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1369, _M0L8_2aparamS1370);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1368,
  struct _M0TPC16string10StringView _M0L8_2aparamS1367
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1366 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1368;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1366, _M0L8_2aparamS1367);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1365,
  moonbit_string_t _M0L8_2aparamS1362,
  int32_t _M0L8_2aparamS1363,
  int32_t _M0L8_2aparamS1364
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1361 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1365;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1361, _M0L8_2aparamS1362, _M0L8_2aparamS1363, _M0L8_2aparamS1364);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1360,
  moonbit_string_t _M0L8_2aparamS1359
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1358 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1360;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1358, _M0L8_2aparamS1359);
  return 0;
}

void moonbit_init() {
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1235 =
    (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1567 =
    _M0L7_2abindS1235;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1566 =
    (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){0,
                                                             0,
                                                             _M0L6_2atmpS1567};
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1565;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1381;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1236;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1564;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1563;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1562;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1382;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1237;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1561;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1560;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1559;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1383;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1238;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1558;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1557;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1556;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1384;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1239;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1555;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1554;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1553;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1385;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1240;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1552;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1551;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1550;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1386;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1241;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1549;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1548;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1547;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1387;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1242;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1546;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1545;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1544;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1388;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1243;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1543;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1542;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1541;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1389;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1244;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1540;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1539;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1538;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1390;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1245;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1537;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1536;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1535;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1391;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1246;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1534;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1533;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1532;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1392;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1247;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1531;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1530;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1529;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1393;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1248;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1528;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1527;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1526;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1394;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1249;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1525;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1524;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1523;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1395;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1250;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1522;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1521;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1520;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1396;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1251;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1519;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1518;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1517;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1397;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1252;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1516;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1515;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1514;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1398;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1253;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1513;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1512;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1511;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1399;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1254;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1510;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1509;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1508;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1400;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1255;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1507;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1506;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1505;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1401;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1256;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1504;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1503;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1502;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1402;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1257;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1501;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1500;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1499;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1403;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1258;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1498;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1497;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1496;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1404;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1259;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1495;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1494;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1493;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1405;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1260;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1492;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1491;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1490;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1406;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1261;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1489;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1488;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1487;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1407;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1262;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1486;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1485;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1484;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1408;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1263;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1483;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1482;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1481;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1409;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1264;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1480;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1479;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1478;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1410;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1265;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1477;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1476;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1475;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1411;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1266;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1474;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1473;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1472;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1412;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1267;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1471;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1470;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1469;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1413;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1268;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1468;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1467;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1466;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1414;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1269;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1465;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1464;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1463;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1415;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1270;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1462;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1461;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1460;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1416;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1271;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1459;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1458;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1457;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1417;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1272;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1456;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1455;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1454;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1418;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1273;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1453;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1452;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1451;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1419;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1274;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1450;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1449;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1448;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1420;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1275;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1447;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1446;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1445;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1421;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1276;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1444;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1443;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1442;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1422;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1277;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1441;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1440;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1439;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1423;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1278;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1438;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1437;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1436;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1424;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1279;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1435;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1434;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1433;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1425;
  moonbit_string_t* _M0L6_2atmpS1432;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1431;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1430;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1280;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1429;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1428;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1427;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1426;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1234;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1380;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1379;
  #line 398 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1565
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1566);
  _M0L8_2atupleS1381
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1381)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1381->$0 = (moonbit_string_t)moonbit_string_literal_83.data;
  _M0L8_2atupleS1381->$1 = _M0L6_2atmpS1565;
  _M0L7_2abindS1236
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1564 = _M0L7_2abindS1236;
  _M0L6_2atmpS1563
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1564
  };
  #line 400 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1562
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1563);
  _M0L8_2atupleS1382
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1382)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1382->$0 = (moonbit_string_t)moonbit_string_literal_84.data;
  _M0L8_2atupleS1382->$1 = _M0L6_2atmpS1562;
  _M0L7_2abindS1237
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1561 = _M0L7_2abindS1237;
  _M0L6_2atmpS1560
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1561
  };
  #line 402 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1559
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1560);
  _M0L8_2atupleS1383
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1383)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1383->$0 = (moonbit_string_t)moonbit_string_literal_85.data;
  _M0L8_2atupleS1383->$1 = _M0L6_2atmpS1559;
  _M0L7_2abindS1238
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1558 = _M0L7_2abindS1238;
  _M0L6_2atmpS1557
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1558
  };
  #line 404 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1556
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1557);
  _M0L8_2atupleS1384
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1384)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1384->$0 = (moonbit_string_t)moonbit_string_literal_86.data;
  _M0L8_2atupleS1384->$1 = _M0L6_2atmpS1556;
  _M0L7_2abindS1239
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1555 = _M0L7_2abindS1239;
  _M0L6_2atmpS1554
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1555
  };
  #line 406 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1553
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1554);
  _M0L8_2atupleS1385
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1385)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1385->$0 = (moonbit_string_t)moonbit_string_literal_87.data;
  _M0L8_2atupleS1385->$1 = _M0L6_2atmpS1553;
  _M0L7_2abindS1240
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1552 = _M0L7_2abindS1240;
  _M0L6_2atmpS1551
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1552
  };
  #line 408 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1550
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1551);
  _M0L8_2atupleS1386
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1386)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1386->$0 = (moonbit_string_t)moonbit_string_literal_88.data;
  _M0L8_2atupleS1386->$1 = _M0L6_2atmpS1550;
  _M0L7_2abindS1241
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1549 = _M0L7_2abindS1241;
  _M0L6_2atmpS1548
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1549
  };
  #line 410 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1547
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1548);
  _M0L8_2atupleS1387
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1387)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1387->$0 = (moonbit_string_t)moonbit_string_literal_89.data;
  _M0L8_2atupleS1387->$1 = _M0L6_2atmpS1547;
  _M0L7_2abindS1242
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1546 = _M0L7_2abindS1242;
  _M0L6_2atmpS1545
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1546
  };
  #line 412 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1544
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1545);
  _M0L8_2atupleS1388
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1388)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1388->$0 = (moonbit_string_t)moonbit_string_literal_90.data;
  _M0L8_2atupleS1388->$1 = _M0L6_2atmpS1544;
  _M0L7_2abindS1243
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1543 = _M0L7_2abindS1243;
  _M0L6_2atmpS1542
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1543
  };
  #line 414 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1541
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1542);
  _M0L8_2atupleS1389
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1389)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1389->$0 = (moonbit_string_t)moonbit_string_literal_91.data;
  _M0L8_2atupleS1389->$1 = _M0L6_2atmpS1541;
  _M0L7_2abindS1244
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1540 = _M0L7_2abindS1244;
  _M0L6_2atmpS1539
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1540
  };
  #line 416 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1538
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1539);
  _M0L8_2atupleS1390
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1390)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1390->$0 = (moonbit_string_t)moonbit_string_literal_92.data;
  _M0L8_2atupleS1390->$1 = _M0L6_2atmpS1538;
  _M0L7_2abindS1245
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1537 = _M0L7_2abindS1245;
  _M0L6_2atmpS1536
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1537
  };
  #line 418 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1535
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1536);
  _M0L8_2atupleS1391
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1391)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1391->$0 = (moonbit_string_t)moonbit_string_literal_93.data;
  _M0L8_2atupleS1391->$1 = _M0L6_2atmpS1535;
  _M0L7_2abindS1246
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1534 = _M0L7_2abindS1246;
  _M0L6_2atmpS1533
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1534
  };
  #line 420 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1532
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1533);
  _M0L8_2atupleS1392
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1392)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1392->$0 = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L8_2atupleS1392->$1 = _M0L6_2atmpS1532;
  _M0L7_2abindS1247
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1531 = _M0L7_2abindS1247;
  _M0L6_2atmpS1530
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1531
  };
  #line 422 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1529
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1530);
  _M0L8_2atupleS1393
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1393)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1393->$0 = (moonbit_string_t)moonbit_string_literal_95.data;
  _M0L8_2atupleS1393->$1 = _M0L6_2atmpS1529;
  _M0L7_2abindS1248
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1528 = _M0L7_2abindS1248;
  _M0L6_2atmpS1527
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1528
  };
  #line 424 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1526
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1527);
  _M0L8_2atupleS1394
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1394)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1394->$0 = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L8_2atupleS1394->$1 = _M0L6_2atmpS1526;
  _M0L7_2abindS1249
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1525 = _M0L7_2abindS1249;
  _M0L6_2atmpS1524
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1525
  };
  #line 426 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1523
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1524);
  _M0L8_2atupleS1395
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1395)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1395->$0 = (moonbit_string_t)moonbit_string_literal_97.data;
  _M0L8_2atupleS1395->$1 = _M0L6_2atmpS1523;
  _M0L7_2abindS1250
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1522 = _M0L7_2abindS1250;
  _M0L6_2atmpS1521
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1522
  };
  #line 428 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1520
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1521);
  _M0L8_2atupleS1396
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1396)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1396->$0 = (moonbit_string_t)moonbit_string_literal_98.data;
  _M0L8_2atupleS1396->$1 = _M0L6_2atmpS1520;
  _M0L7_2abindS1251
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1519 = _M0L7_2abindS1251;
  _M0L6_2atmpS1518
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1519
  };
  #line 430 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1517
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1518);
  _M0L8_2atupleS1397
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1397)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1397->$0 = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L8_2atupleS1397->$1 = _M0L6_2atmpS1517;
  _M0L7_2abindS1252
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1516 = _M0L7_2abindS1252;
  _M0L6_2atmpS1515
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1516
  };
  #line 432 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1514
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1515);
  _M0L8_2atupleS1398
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1398)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1398->$0 = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L8_2atupleS1398->$1 = _M0L6_2atmpS1514;
  _M0L7_2abindS1253
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1513 = _M0L7_2abindS1253;
  _M0L6_2atmpS1512
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1513
  };
  #line 434 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1511
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1512);
  _M0L8_2atupleS1399
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1399)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1399->$0 = (moonbit_string_t)moonbit_string_literal_101.data;
  _M0L8_2atupleS1399->$1 = _M0L6_2atmpS1511;
  _M0L7_2abindS1254
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1510 = _M0L7_2abindS1254;
  _M0L6_2atmpS1509
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1510
  };
  #line 436 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1508
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1509);
  _M0L8_2atupleS1400
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1400)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1400->$0 = (moonbit_string_t)moonbit_string_literal_102.data;
  _M0L8_2atupleS1400->$1 = _M0L6_2atmpS1508;
  _M0L7_2abindS1255
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1507 = _M0L7_2abindS1255;
  _M0L6_2atmpS1506
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1507
  };
  #line 438 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1505
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1506);
  _M0L8_2atupleS1401
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1401)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1401->$0 = (moonbit_string_t)moonbit_string_literal_103.data;
  _M0L8_2atupleS1401->$1 = _M0L6_2atmpS1505;
  _M0L7_2abindS1256
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1504 = _M0L7_2abindS1256;
  _M0L6_2atmpS1503
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1504
  };
  #line 440 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1502
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1503);
  _M0L8_2atupleS1402
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1402)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1402->$0 = (moonbit_string_t)moonbit_string_literal_104.data;
  _M0L8_2atupleS1402->$1 = _M0L6_2atmpS1502;
  _M0L7_2abindS1257
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1501 = _M0L7_2abindS1257;
  _M0L6_2atmpS1500
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1501
  };
  #line 442 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1499
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1500);
  _M0L8_2atupleS1403
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1403)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1403->$0 = (moonbit_string_t)moonbit_string_literal_105.data;
  _M0L8_2atupleS1403->$1 = _M0L6_2atmpS1499;
  _M0L7_2abindS1258
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1498 = _M0L7_2abindS1258;
  _M0L6_2atmpS1497
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1498
  };
  #line 444 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1496
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1497);
  _M0L8_2atupleS1404
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1404)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1404->$0 = (moonbit_string_t)moonbit_string_literal_106.data;
  _M0L8_2atupleS1404->$1 = _M0L6_2atmpS1496;
  _M0L7_2abindS1259
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1495 = _M0L7_2abindS1259;
  _M0L6_2atmpS1494
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1495
  };
  #line 446 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1493
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1494);
  _M0L8_2atupleS1405
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1405)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1405->$0 = (moonbit_string_t)moonbit_string_literal_107.data;
  _M0L8_2atupleS1405->$1 = _M0L6_2atmpS1493;
  _M0L7_2abindS1260
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1492 = _M0L7_2abindS1260;
  _M0L6_2atmpS1491
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1492
  };
  #line 448 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1490
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1491);
  _M0L8_2atupleS1406
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1406)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1406->$0 = (moonbit_string_t)moonbit_string_literal_108.data;
  _M0L8_2atupleS1406->$1 = _M0L6_2atmpS1490;
  _M0L7_2abindS1261
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1489 = _M0L7_2abindS1261;
  _M0L6_2atmpS1488
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1489
  };
  #line 450 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1487
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1488);
  _M0L8_2atupleS1407
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1407)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1407->$0 = (moonbit_string_t)moonbit_string_literal_109.data;
  _M0L8_2atupleS1407->$1 = _M0L6_2atmpS1487;
  _M0L7_2abindS1262
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1486 = _M0L7_2abindS1262;
  _M0L6_2atmpS1485
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1486
  };
  #line 452 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1484
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1485);
  _M0L8_2atupleS1408
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1408)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1408->$0 = (moonbit_string_t)moonbit_string_literal_110.data;
  _M0L8_2atupleS1408->$1 = _M0L6_2atmpS1484;
  _M0L7_2abindS1263
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1483 = _M0L7_2abindS1263;
  _M0L6_2atmpS1482
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1483
  };
  #line 454 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1481
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1482);
  _M0L8_2atupleS1409
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1409)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1409->$0 = (moonbit_string_t)moonbit_string_literal_111.data;
  _M0L8_2atupleS1409->$1 = _M0L6_2atmpS1481;
  _M0L7_2abindS1264
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1480 = _M0L7_2abindS1264;
  _M0L6_2atmpS1479
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1480
  };
  #line 456 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1478
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1479);
  _M0L8_2atupleS1410
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1410)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1410->$0 = (moonbit_string_t)moonbit_string_literal_112.data;
  _M0L8_2atupleS1410->$1 = _M0L6_2atmpS1478;
  _M0L7_2abindS1265
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1477 = _M0L7_2abindS1265;
  _M0L6_2atmpS1476
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1477
  };
  #line 458 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1475
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1476);
  _M0L8_2atupleS1411
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1411)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1411->$0 = (moonbit_string_t)moonbit_string_literal_113.data;
  _M0L8_2atupleS1411->$1 = _M0L6_2atmpS1475;
  _M0L7_2abindS1266
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1474 = _M0L7_2abindS1266;
  _M0L6_2atmpS1473
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1474
  };
  #line 460 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1472
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1473);
  _M0L8_2atupleS1412
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1412)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1412->$0 = (moonbit_string_t)moonbit_string_literal_114.data;
  _M0L8_2atupleS1412->$1 = _M0L6_2atmpS1472;
  _M0L7_2abindS1267
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1471 = _M0L7_2abindS1267;
  _M0L6_2atmpS1470
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1471
  };
  #line 462 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1469
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1470);
  _M0L8_2atupleS1413
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1413)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1413->$0 = (moonbit_string_t)moonbit_string_literal_115.data;
  _M0L8_2atupleS1413->$1 = _M0L6_2atmpS1469;
  _M0L7_2abindS1268
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1468 = _M0L7_2abindS1268;
  _M0L6_2atmpS1467
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1468
  };
  #line 464 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1466
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1467);
  _M0L8_2atupleS1414
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1414)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1414->$0 = (moonbit_string_t)moonbit_string_literal_116.data;
  _M0L8_2atupleS1414->$1 = _M0L6_2atmpS1466;
  _M0L7_2abindS1269
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1465 = _M0L7_2abindS1269;
  _M0L6_2atmpS1464
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1465
  };
  #line 466 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1463
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1464);
  _M0L8_2atupleS1415
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1415)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1415->$0 = (moonbit_string_t)moonbit_string_literal_117.data;
  _M0L8_2atupleS1415->$1 = _M0L6_2atmpS1463;
  _M0L7_2abindS1270
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1462 = _M0L7_2abindS1270;
  _M0L6_2atmpS1461
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1462
  };
  #line 468 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1460
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1461);
  _M0L8_2atupleS1416
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1416)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1416->$0 = (moonbit_string_t)moonbit_string_literal_118.data;
  _M0L8_2atupleS1416->$1 = _M0L6_2atmpS1460;
  _M0L7_2abindS1271
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1459 = _M0L7_2abindS1271;
  _M0L6_2atmpS1458
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1459
  };
  #line 470 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1457
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1458);
  _M0L8_2atupleS1417
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1417)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1417->$0 = (moonbit_string_t)moonbit_string_literal_119.data;
  _M0L8_2atupleS1417->$1 = _M0L6_2atmpS1457;
  _M0L7_2abindS1272
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1456 = _M0L7_2abindS1272;
  _M0L6_2atmpS1455
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1456
  };
  #line 472 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1454
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1455);
  _M0L8_2atupleS1418
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1418)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1418->$0 = (moonbit_string_t)moonbit_string_literal_120.data;
  _M0L8_2atupleS1418->$1 = _M0L6_2atmpS1454;
  _M0L7_2abindS1273
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1453 = _M0L7_2abindS1273;
  _M0L6_2atmpS1452
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1453
  };
  #line 474 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1451
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1452);
  _M0L8_2atupleS1419
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1419)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1419->$0 = (moonbit_string_t)moonbit_string_literal_121.data;
  _M0L8_2atupleS1419->$1 = _M0L6_2atmpS1451;
  _M0L7_2abindS1274
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1450 = _M0L7_2abindS1274;
  _M0L6_2atmpS1449
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1450
  };
  #line 476 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1448
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1449);
  _M0L8_2atupleS1420
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1420)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1420->$0 = (moonbit_string_t)moonbit_string_literal_122.data;
  _M0L8_2atupleS1420->$1 = _M0L6_2atmpS1448;
  _M0L7_2abindS1275
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1447 = _M0L7_2abindS1275;
  _M0L6_2atmpS1446
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1447
  };
  #line 478 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1445
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1446);
  _M0L8_2atupleS1421
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1421)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1421->$0 = (moonbit_string_t)moonbit_string_literal_123.data;
  _M0L8_2atupleS1421->$1 = _M0L6_2atmpS1445;
  _M0L7_2abindS1276
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1444 = _M0L7_2abindS1276;
  _M0L6_2atmpS1443
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1444
  };
  #line 480 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1442
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1443);
  _M0L8_2atupleS1422
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1422)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1422->$0 = (moonbit_string_t)moonbit_string_literal_124.data;
  _M0L8_2atupleS1422->$1 = _M0L6_2atmpS1442;
  _M0L7_2abindS1277
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1441 = _M0L7_2abindS1277;
  _M0L6_2atmpS1440
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1441
  };
  #line 482 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1439
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1440);
  _M0L8_2atupleS1423
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1423)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1423->$0 = (moonbit_string_t)moonbit_string_literal_125.data;
  _M0L8_2atupleS1423->$1 = _M0L6_2atmpS1439;
  _M0L7_2abindS1278
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1438 = _M0L7_2abindS1278;
  _M0L6_2atmpS1437
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1438
  };
  #line 484 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1436
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1437);
  _M0L8_2atupleS1424
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1424)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1424->$0 = (moonbit_string_t)moonbit_string_literal_126.data;
  _M0L8_2atupleS1424->$1 = _M0L6_2atmpS1436;
  _M0L7_2abindS1279
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1435 = _M0L7_2abindS1279;
  _M0L6_2atmpS1434
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1435
  };
  #line 486 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1433
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1434);
  _M0L8_2atupleS1425
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1425)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1425->$0 = (moonbit_string_t)moonbit_string_literal_127.data;
  _M0L8_2atupleS1425->$1 = _M0L6_2atmpS1433;
  _M0L6_2atmpS1432 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1432[0] = (moonbit_string_t)moonbit_string_literal_128.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal6openai31____test__61692e6d6274__0_2eclo);
  _M0L8_2atupleS1431
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1431)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1431->$0
  = _M0FP48clawteam8clawteam8internal6openai31____test__61692e6d6274__0_2eclo;
  _M0L8_2atupleS1431->$1 = _M0L6_2atmpS1432;
  _M0L8_2atupleS1430
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1430)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1430->$0 = 0;
  _M0L8_2atupleS1430->$1 = _M0L8_2atupleS1431;
  _M0L7_2abindS1280
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1280[0] = _M0L8_2atupleS1430;
  _M0L6_2atmpS1429 = _M0L7_2abindS1280;
  _M0L6_2atmpS1428
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1429
  };
  #line 488 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1427
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1428);
  _M0L8_2atupleS1426
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1426)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1426->$0 = (moonbit_string_t)moonbit_string_literal_129.data;
  _M0L8_2atupleS1426->$1 = _M0L6_2atmpS1427;
  _M0L7_2abindS1234
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(46);
  _M0L7_2abindS1234[0] = _M0L8_2atupleS1381;
  _M0L7_2abindS1234[1] = _M0L8_2atupleS1382;
  _M0L7_2abindS1234[2] = _M0L8_2atupleS1383;
  _M0L7_2abindS1234[3] = _M0L8_2atupleS1384;
  _M0L7_2abindS1234[4] = _M0L8_2atupleS1385;
  _M0L7_2abindS1234[5] = _M0L8_2atupleS1386;
  _M0L7_2abindS1234[6] = _M0L8_2atupleS1387;
  _M0L7_2abindS1234[7] = _M0L8_2atupleS1388;
  _M0L7_2abindS1234[8] = _M0L8_2atupleS1389;
  _M0L7_2abindS1234[9] = _M0L8_2atupleS1390;
  _M0L7_2abindS1234[10] = _M0L8_2atupleS1391;
  _M0L7_2abindS1234[11] = _M0L8_2atupleS1392;
  _M0L7_2abindS1234[12] = _M0L8_2atupleS1393;
  _M0L7_2abindS1234[13] = _M0L8_2atupleS1394;
  _M0L7_2abindS1234[14] = _M0L8_2atupleS1395;
  _M0L7_2abindS1234[15] = _M0L8_2atupleS1396;
  _M0L7_2abindS1234[16] = _M0L8_2atupleS1397;
  _M0L7_2abindS1234[17] = _M0L8_2atupleS1398;
  _M0L7_2abindS1234[18] = _M0L8_2atupleS1399;
  _M0L7_2abindS1234[19] = _M0L8_2atupleS1400;
  _M0L7_2abindS1234[20] = _M0L8_2atupleS1401;
  _M0L7_2abindS1234[21] = _M0L8_2atupleS1402;
  _M0L7_2abindS1234[22] = _M0L8_2atupleS1403;
  _M0L7_2abindS1234[23] = _M0L8_2atupleS1404;
  _M0L7_2abindS1234[24] = _M0L8_2atupleS1405;
  _M0L7_2abindS1234[25] = _M0L8_2atupleS1406;
  _M0L7_2abindS1234[26] = _M0L8_2atupleS1407;
  _M0L7_2abindS1234[27] = _M0L8_2atupleS1408;
  _M0L7_2abindS1234[28] = _M0L8_2atupleS1409;
  _M0L7_2abindS1234[29] = _M0L8_2atupleS1410;
  _M0L7_2abindS1234[30] = _M0L8_2atupleS1411;
  _M0L7_2abindS1234[31] = _M0L8_2atupleS1412;
  _M0L7_2abindS1234[32] = _M0L8_2atupleS1413;
  _M0L7_2abindS1234[33] = _M0L8_2atupleS1414;
  _M0L7_2abindS1234[34] = _M0L8_2atupleS1415;
  _M0L7_2abindS1234[35] = _M0L8_2atupleS1416;
  _M0L7_2abindS1234[36] = _M0L8_2atupleS1417;
  _M0L7_2abindS1234[37] = _M0L8_2atupleS1418;
  _M0L7_2abindS1234[38] = _M0L8_2atupleS1419;
  _M0L7_2abindS1234[39] = _M0L8_2atupleS1420;
  _M0L7_2abindS1234[40] = _M0L8_2atupleS1421;
  _M0L7_2abindS1234[41] = _M0L8_2atupleS1422;
  _M0L7_2abindS1234[42] = _M0L8_2atupleS1423;
  _M0L7_2abindS1234[43] = _M0L8_2atupleS1424;
  _M0L7_2abindS1234[44] = _M0L8_2atupleS1425;
  _M0L7_2abindS1234[45] = _M0L8_2atupleS1426;
  _M0L6_2atmpS1380 = _M0L7_2abindS1234;
  _M0L6_2atmpS1379
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 46, _M0L6_2atmpS1380
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1379);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1378;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1347;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1348;
  int32_t _M0L7_2abindS1349;
  int32_t _M0L2__S1350;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1378
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1347
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1347)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1347->$0 = _M0L6_2atmpS1378;
  _M0L12async__testsS1347->$1 = 0;
  #line 528 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1348
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1349 = _M0L7_2abindS1348->$1;
  _M0L2__S1350 = 0;
  while (1) {
    if (_M0L2__S1350 < _M0L7_2abindS1349) {
      struct _M0TUsiE** _M0L8_2afieldS3454 = _M0L7_2abindS1348->$0;
      struct _M0TUsiE** _M0L3bufS1377 = _M0L8_2afieldS3454;
      struct _M0TUsiE* _M0L6_2atmpS3453 =
        (struct _M0TUsiE*)_M0L3bufS1377[_M0L2__S1350];
      struct _M0TUsiE* _M0L3argS1351 = _M0L6_2atmpS3453;
      moonbit_string_t _M0L8_2afieldS3452 = _M0L3argS1351->$0;
      moonbit_string_t _M0L6_2atmpS1374 = _M0L8_2afieldS3452;
      int32_t _M0L8_2afieldS3451 = _M0L3argS1351->$1;
      int32_t _M0L6_2atmpS1375 = _M0L8_2afieldS3451;
      int32_t _M0L6_2atmpS1376;
      moonbit_incref(_M0L6_2atmpS1374);
      moonbit_incref(_M0L12async__testsS1347);
      #line 529 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1347, _M0L6_2atmpS1374, _M0L6_2atmpS1375);
      _M0L6_2atmpS1376 = _M0L2__S1350 + 1;
      _M0L2__S1350 = _M0L6_2atmpS1376;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1348);
    }
    break;
  }
  #line 531 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal6openai28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6openai34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1347);
  return 0;
}