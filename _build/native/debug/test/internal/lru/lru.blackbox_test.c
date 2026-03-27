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

struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE;

struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE;

struct _M0KTPB6ToJsonTPC16option6OptionGiE;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPB4Json5Array;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0R38String_3a_3aiter_2eanon__u1994__l247__;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19lru__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__;

struct _M0TPB6Logger;

struct _M0Y5Int64;

struct _M0TWEuQRPC15error5Error;

struct _M0TPB19MulShiftAll64Result;

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE;

struct _M0TWEOUsRPB4JsonE;

struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358;

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal3lru5CacheGsiE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0KTPB6ToJsonS3Int;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__;

struct _M0TWRPC15error5ErrorEu;

struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__;

struct _M0TPB6Hasher;

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TPC13ref3RefGOUsRP48clawteam8clawteam8internal3lru5EntryGiEEE;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19lru__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPB9ArrayViewGsE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__;

struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0Y3Int {
  int32_t $0;
  
};

struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE** $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* $0;
  
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

struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE {
  int32_t $0;
  int32_t $2;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* $1;
  
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

struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TPB9ArrayViewGUsRPB4JsonEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB4JsonE** $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u1994__l247__ {
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

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE {
  moonbit_string_t $0;
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* $1;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19lru__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0Y5Int64 {
  int64_t $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* $1;
  moonbit_string_t $4;
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* $5;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal3lru5CacheGsiE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
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

struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE {
  int32_t $0;
  int32_t $1;
  
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

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
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

struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__ {
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE*(* code)(
    struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE*
  );
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE* $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE {
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE*(* code)(
    struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE*
  );
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0TPC13ref3RefGOUsRP48clawteam8clawteam8internal3lru5EntryGiEEE {
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* $0;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19lru__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** $0;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* $5;
  
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19lru__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1367(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1358(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testC3093l427(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testC3089l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1291(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1286(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1273(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal19lru__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19lru__blackbox__test41____test__63616368655f746573742e6d6274__0(
  
);

void* _M0IP48clawteam8clawteam8internal3lru5CachePB6ToJson8to__jsonGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE*
);

int32_t _M0MP48clawteam8clawteam8internal3lru5Cache3setGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MP48clawteam8clawteam8internal3lru5Cache5evictGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE*
);

int32_t _M0MP48clawteam8clawteam8internal3lru5Cache7op__getGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE*,
  moonbit_string_t
);

int64_t _M0MP48clawteam8clawteam8internal3lru5Cache3getGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE*,
  moonbit_string_t
);

struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0FP48clawteam8clawteam8internal3lru13cache_2einnerGsiE(
  int32_t
);

void* _M0IP48clawteam8clawteam8internal3lru5EntryPB6ToJson8to__jsonGiE(
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE*
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

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

void* _M0IPC16option6OptionPB6ToJson8to__jsonGiE(int64_t);

void* _M0IPB3MapPB6ToJson8to__jsonGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal3lru5EntryGiEEC2376l591(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2370l591(
  struct _M0TWEOUsRPB4JsonE*
);

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(struct _M0TPB3MapGsRPB4JsonE*);

int32_t _M0MPB3Map6lengthGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

int32_t _M0MPB3Map6removeGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  moonbit_string_t
);

int32_t _M0MPB3Map18remove__with__hashGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map11shift__backGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  int32_t
);

int32_t _M0MPB3Map13remove__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t
);

struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0MPB3Map3getGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
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

struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE
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

int32_t _M0MPB3Map3setGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  moonbit_string_t,
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE*
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

int32_t _M0MPB3Map4growGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
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

int32_t _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  moonbit_string_t,
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE*,
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

int32_t _M0MPB3Map10push__awayGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  int32_t,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
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

int32_t _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*,
  int32_t,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
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

struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map11new_2einnerGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  int32_t
);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

int32_t _M0MPC16option6Option6unwrapGiE(int64_t);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*
);

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2013l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC1994l247(struct _M0TWEOc*);

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

int32_t _M0MPC15array9ArrayView6lengthGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE
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

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB4Iter3newGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE*
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

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB4Iter4nextGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE*
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

void* _M0IP48clawteam8clawteam8internal3lru5CachePB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsiE(
  void*
);

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGiE(
  void*
);

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

struct { int32_t rc; uint32_t meta; uint16_t const data[72]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 71), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 49, 49, 58, 51, 45, 49, 49, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[73]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 72), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 49, 49, 58, 51, 49, 45, 49, 49, 58, 52, 57, 0
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
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_9 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 97, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 54, 58, 51, 54, 45, 54, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[70]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 69), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 57, 58, 51, 45, 57, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[73]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 72), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 49, 49, 58, 49, 54, 45, 49, 49, 58, 50, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_56 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 114, 117, 34, 
    44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[70]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 69), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 54, 58, 51, 45, 54, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 57, 58, 49, 54, 45, 57, 58, 50, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_36 =
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
} const moonbit_string_literal_84 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_46 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 57, 58, 51, 49, 45, 57, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_37 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[113]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 112), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 114, 
    117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 
    105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_77 =
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
} const moonbit_string_literal_17 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 99, 0};

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

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_28 =
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
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_60 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_10 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 98, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[70]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 69), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 56, 58, 51, 45, 56, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 55, 58, 52, 48, 45, 55, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 54, 58, 49, 54, 45, 54, 58, 50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[111]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 110), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 114, 
    117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 
    69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 
    101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 
    110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 55, 58, 49, 54, 45, 55, 58, 51, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_74 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    99, 97, 99, 104, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    67, 97, 99, 104, 101, 58, 58, 111, 112, 95, 115, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[70]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 69), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 55, 58, 51, 45, 55, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 56, 58, 52, 48, 45, 56, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 108, 
    114, 117, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 56, 58, 49, 54, 45, 56, 58, 51, 48, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1367$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1367
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal19lru__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal19lru__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal19lru__blackbox__test47____test__63616368655f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal19lru__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall$closure.data;

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

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0140clawteam_2fclawteam_2finternal_2flru_2fCache_5bString_2c_20Int_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam8internal3lru5CachePB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsiE}
  };

struct _M0BTPB6ToJson* _M0FP0140clawteam_2fclawteam_2finternal_2flru_2fCache_5bString_2c_20Int_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0140clawteam_2fclawteam_2finternal_2flru_2fCache_5bString_2c_20Int_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB31ryu__to__string_2erecord_2f1096$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1096 =
  &_M0FPB31ryu__to__string_2erecord_2f1096$object.data;

void* _M0FPC17prelude4null;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal19lru__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19lru__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3124
) {
  return _M0FP48clawteam8clawteam8internal19lru__blackbox__test41____test__63616368655f746573742e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1388,
  moonbit_string_t _M0L8filenameS1363,
  int32_t _M0L5indexS1366
) {
  struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358* _closure_3593;
  struct _M0TWssbEu* _M0L14handle__resultS1358;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1367;
  void* _M0L11_2atry__errS1382;
  struct moonbit_result_0 _tmp_3595;
  int32_t _handle__error__result_3596;
  int32_t _M0L6_2atmpS3112;
  void* _M0L3errS1383;
  moonbit_string_t _M0L4nameS1385;
  struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1386;
  moonbit_string_t _M0L8_2afieldS3125;
  int32_t _M0L6_2acntS3473;
  moonbit_string_t _M0L7_2anameS1387;
  #line 526 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1363);
  _closure_3593
  = (struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358*)moonbit_malloc(sizeof(struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358));
  Moonbit_object_header(_closure_3593)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358, $1) >> 2, 1, 0);
  _closure_3593->code
  = &_M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1358;
  _closure_3593->$0 = _M0L5indexS1366;
  _closure_3593->$1 = _M0L8filenameS1363;
  _M0L14handle__resultS1358 = (struct _M0TWssbEu*)_closure_3593;
  _M0L17error__to__stringS1367
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1367$closure.data;
  moonbit_incref(_M0L12async__testsS1388);
  moonbit_incref(_M0L17error__to__stringS1367);
  moonbit_incref(_M0L8filenameS1363);
  moonbit_incref(_M0L14handle__resultS1358);
  #line 560 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3595
  = _M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1388, _M0L8filenameS1363, _M0L5indexS1366, _M0L14handle__resultS1358, _M0L17error__to__stringS1367);
  if (_tmp_3595.tag) {
    int32_t const _M0L5_2aokS3121 = _tmp_3595.data.ok;
    _handle__error__result_3596 = _M0L5_2aokS3121;
  } else {
    void* const _M0L6_2aerrS3122 = _tmp_3595.data.err;
    moonbit_decref(_M0L12async__testsS1388);
    moonbit_decref(_M0L17error__to__stringS1367);
    moonbit_decref(_M0L8filenameS1363);
    _M0L11_2atry__errS1382 = _M0L6_2aerrS3122;
    goto join_1381;
  }
  if (_handle__error__result_3596) {
    moonbit_decref(_M0L12async__testsS1388);
    moonbit_decref(_M0L17error__to__stringS1367);
    moonbit_decref(_M0L8filenameS1363);
    _M0L6_2atmpS3112 = 1;
  } else {
    struct moonbit_result_0 _tmp_3597;
    int32_t _handle__error__result_3598;
    moonbit_incref(_M0L12async__testsS1388);
    moonbit_incref(_M0L17error__to__stringS1367);
    moonbit_incref(_M0L8filenameS1363);
    moonbit_incref(_M0L14handle__resultS1358);
    #line 563 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    _tmp_3597
    = _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1388, _M0L8filenameS1363, _M0L5indexS1366, _M0L14handle__resultS1358, _M0L17error__to__stringS1367);
    if (_tmp_3597.tag) {
      int32_t const _M0L5_2aokS3119 = _tmp_3597.data.ok;
      _handle__error__result_3598 = _M0L5_2aokS3119;
    } else {
      void* const _M0L6_2aerrS3120 = _tmp_3597.data.err;
      moonbit_decref(_M0L12async__testsS1388);
      moonbit_decref(_M0L17error__to__stringS1367);
      moonbit_decref(_M0L8filenameS1363);
      _M0L11_2atry__errS1382 = _M0L6_2aerrS3120;
      goto join_1381;
    }
    if (_handle__error__result_3598) {
      moonbit_decref(_M0L12async__testsS1388);
      moonbit_decref(_M0L17error__to__stringS1367);
      moonbit_decref(_M0L8filenameS1363);
      _M0L6_2atmpS3112 = 1;
    } else {
      struct moonbit_result_0 _tmp_3599;
      int32_t _handle__error__result_3600;
      moonbit_incref(_M0L12async__testsS1388);
      moonbit_incref(_M0L17error__to__stringS1367);
      moonbit_incref(_M0L8filenameS1363);
      moonbit_incref(_M0L14handle__resultS1358);
      #line 566 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _tmp_3599
      = _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1388, _M0L8filenameS1363, _M0L5indexS1366, _M0L14handle__resultS1358, _M0L17error__to__stringS1367);
      if (_tmp_3599.tag) {
        int32_t const _M0L5_2aokS3117 = _tmp_3599.data.ok;
        _handle__error__result_3600 = _M0L5_2aokS3117;
      } else {
        void* const _M0L6_2aerrS3118 = _tmp_3599.data.err;
        moonbit_decref(_M0L12async__testsS1388);
        moonbit_decref(_M0L17error__to__stringS1367);
        moonbit_decref(_M0L8filenameS1363);
        _M0L11_2atry__errS1382 = _M0L6_2aerrS3118;
        goto join_1381;
      }
      if (_handle__error__result_3600) {
        moonbit_decref(_M0L12async__testsS1388);
        moonbit_decref(_M0L17error__to__stringS1367);
        moonbit_decref(_M0L8filenameS1363);
        _M0L6_2atmpS3112 = 1;
      } else {
        struct moonbit_result_0 _tmp_3601;
        int32_t _handle__error__result_3602;
        moonbit_incref(_M0L12async__testsS1388);
        moonbit_incref(_M0L17error__to__stringS1367);
        moonbit_incref(_M0L8filenameS1363);
        moonbit_incref(_M0L14handle__resultS1358);
        #line 569 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        _tmp_3601
        = _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1388, _M0L8filenameS1363, _M0L5indexS1366, _M0L14handle__resultS1358, _M0L17error__to__stringS1367);
        if (_tmp_3601.tag) {
          int32_t const _M0L5_2aokS3115 = _tmp_3601.data.ok;
          _handle__error__result_3602 = _M0L5_2aokS3115;
        } else {
          void* const _M0L6_2aerrS3116 = _tmp_3601.data.err;
          moonbit_decref(_M0L12async__testsS1388);
          moonbit_decref(_M0L17error__to__stringS1367);
          moonbit_decref(_M0L8filenameS1363);
          _M0L11_2atry__errS1382 = _M0L6_2aerrS3116;
          goto join_1381;
        }
        if (_handle__error__result_3602) {
          moonbit_decref(_M0L12async__testsS1388);
          moonbit_decref(_M0L17error__to__stringS1367);
          moonbit_decref(_M0L8filenameS1363);
          _M0L6_2atmpS3112 = 1;
        } else {
          struct moonbit_result_0 _tmp_3603;
          moonbit_incref(_M0L14handle__resultS1358);
          #line 572 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
          _tmp_3603
          = _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1388, _M0L8filenameS1363, _M0L5indexS1366, _M0L14handle__resultS1358, _M0L17error__to__stringS1367);
          if (_tmp_3603.tag) {
            int32_t const _M0L5_2aokS3113 = _tmp_3603.data.ok;
            _M0L6_2atmpS3112 = _M0L5_2aokS3113;
          } else {
            void* const _M0L6_2aerrS3114 = _tmp_3603.data.err;
            _M0L11_2atry__errS1382 = _M0L6_2aerrS3114;
            goto join_1381;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3112) {
    void* _M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3123 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3123)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3123)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1382
    = _M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3123;
    goto join_1381;
  } else {
    moonbit_decref(_M0L14handle__resultS1358);
  }
  goto joinlet_3594;
  join_1381:;
  _M0L3errS1383 = _M0L11_2atry__errS1382;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1386
  = (struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1383;
  _M0L8_2afieldS3125 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1386->$0;
  _M0L6_2acntS3473
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1386)->rc;
  if (_M0L6_2acntS3473 > 1) {
    int32_t _M0L11_2anew__cntS3474 = _M0L6_2acntS3473 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1386)->rc
    = _M0L11_2anew__cntS3474;
    moonbit_incref(_M0L8_2afieldS3125);
  } else if (_M0L6_2acntS3473 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1386);
  }
  _M0L7_2anameS1387 = _M0L8_2afieldS3125;
  _M0L4nameS1385 = _M0L7_2anameS1387;
  goto join_1384;
  goto joinlet_3604;
  join_1384:;
  #line 580 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1358(_M0L14handle__resultS1358, _M0L4nameS1385, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3604:;
  joinlet_3594:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1367(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3111,
  void* _M0L3errS1368
) {
  void* _M0L1eS1370;
  moonbit_string_t _M0L1eS1372;
  #line 549 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3111);
  switch (Moonbit_object_tag(_M0L3errS1368)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1373 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1368;
      moonbit_string_t _M0L8_2afieldS3126 = _M0L10_2aFailureS1373->$0;
      int32_t _M0L6_2acntS3475 =
        Moonbit_object_header(_M0L10_2aFailureS1373)->rc;
      moonbit_string_t _M0L4_2aeS1374;
      if (_M0L6_2acntS3475 > 1) {
        int32_t _M0L11_2anew__cntS3476 = _M0L6_2acntS3475 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1373)->rc
        = _M0L11_2anew__cntS3476;
        moonbit_incref(_M0L8_2afieldS3126);
      } else if (_M0L6_2acntS3475 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1373);
      }
      _M0L4_2aeS1374 = _M0L8_2afieldS3126;
      _M0L1eS1372 = _M0L4_2aeS1374;
      goto join_1371;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1375 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1368;
      moonbit_string_t _M0L8_2afieldS3127 = _M0L15_2aInspectErrorS1375->$0;
      int32_t _M0L6_2acntS3477 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1375)->rc;
      moonbit_string_t _M0L4_2aeS1376;
      if (_M0L6_2acntS3477 > 1) {
        int32_t _M0L11_2anew__cntS3478 = _M0L6_2acntS3477 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1375)->rc
        = _M0L11_2anew__cntS3478;
        moonbit_incref(_M0L8_2afieldS3127);
      } else if (_M0L6_2acntS3477 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1375);
      }
      _M0L4_2aeS1376 = _M0L8_2afieldS3127;
      _M0L1eS1372 = _M0L4_2aeS1376;
      goto join_1371;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1377 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1368;
      moonbit_string_t _M0L8_2afieldS3128 = _M0L16_2aSnapshotErrorS1377->$0;
      int32_t _M0L6_2acntS3479 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1377)->rc;
      moonbit_string_t _M0L4_2aeS1378;
      if (_M0L6_2acntS3479 > 1) {
        int32_t _M0L11_2anew__cntS3480 = _M0L6_2acntS3479 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1377)->rc
        = _M0L11_2anew__cntS3480;
        moonbit_incref(_M0L8_2afieldS3128);
      } else if (_M0L6_2acntS3479 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1377);
      }
      _M0L4_2aeS1378 = _M0L8_2afieldS3128;
      _M0L1eS1372 = _M0L4_2aeS1378;
      goto join_1371;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1379 =
        (struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1368;
      moonbit_string_t _M0L8_2afieldS3129 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1379->$0;
      int32_t _M0L6_2acntS3481 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1379)->rc;
      moonbit_string_t _M0L4_2aeS1380;
      if (_M0L6_2acntS3481 > 1) {
        int32_t _M0L11_2anew__cntS3482 = _M0L6_2acntS3481 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1379)->rc
        = _M0L11_2anew__cntS3482;
        moonbit_incref(_M0L8_2afieldS3129);
      } else if (_M0L6_2acntS3481 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1379);
      }
      _M0L4_2aeS1380 = _M0L8_2afieldS3129;
      _M0L1eS1372 = _M0L4_2aeS1380;
      goto join_1371;
      break;
    }
    default: {
      _M0L1eS1370 = _M0L3errS1368;
      goto join_1369;
      break;
    }
  }
  join_1371:;
  return _M0L1eS1372;
  join_1369:;
  #line 555 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1370);
}

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1358(
  struct _M0TWssbEu* _M0L6_2aenvS3097,
  moonbit_string_t _M0L8testnameS1359,
  moonbit_string_t _M0L7messageS1360,
  int32_t _M0L7skippedS1361
) {
  struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358* _M0L14_2acasted__envS3098;
  moonbit_string_t _M0L8_2afieldS3139;
  moonbit_string_t _M0L8filenameS1363;
  int32_t _M0L8_2afieldS3138;
  int32_t _M0L6_2acntS3483;
  int32_t _M0L5indexS1366;
  int32_t _if__result_3607;
  moonbit_string_t _M0L10file__nameS1362;
  moonbit_string_t _M0L10test__nameS1364;
  moonbit_string_t _M0L7messageS1365;
  moonbit_string_t _M0L6_2atmpS3110;
  moonbit_string_t _M0L6_2atmpS3137;
  moonbit_string_t _M0L6_2atmpS3109;
  moonbit_string_t _M0L6_2atmpS3136;
  moonbit_string_t _M0L6_2atmpS3107;
  moonbit_string_t _M0L6_2atmpS3108;
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
  moonbit_string_t _M0L6_2atmpS3099;
  #line 533 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3098
  = (struct _M0R126_24clawteam_2fclawteam_2finternal_2flru__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1358*)_M0L6_2aenvS3097;
  _M0L8_2afieldS3139 = _M0L14_2acasted__envS3098->$1;
  _M0L8filenameS1363 = _M0L8_2afieldS3139;
  _M0L8_2afieldS3138 = _M0L14_2acasted__envS3098->$0;
  _M0L6_2acntS3483 = Moonbit_object_header(_M0L14_2acasted__envS3098)->rc;
  if (_M0L6_2acntS3483 > 1) {
    int32_t _M0L11_2anew__cntS3484 = _M0L6_2acntS3483 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3098)->rc
    = _M0L11_2anew__cntS3484;
    moonbit_incref(_M0L8filenameS1363);
  } else if (_M0L6_2acntS3483 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3098);
  }
  _M0L5indexS1366 = _M0L8_2afieldS3138;
  if (!_M0L7skippedS1361) {
    _if__result_3607 = 1;
  } else {
    _if__result_3607 = 0;
  }
  if (_if__result_3607) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1362 = _M0MPC16string6String6escape(_M0L8filenameS1363);
  #line 540 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1364 = _M0MPC16string6String6escape(_M0L8testnameS1359);
  #line 541 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1365 = _M0MPC16string6String6escape(_M0L7messageS1360);
  #line 542 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 544 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3110
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1362);
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3137
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3110);
  moonbit_decref(_M0L6_2atmpS3110);
  _M0L6_2atmpS3109 = _M0L6_2atmpS3137;
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3136
  = moonbit_add_string(_M0L6_2atmpS3109, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3109);
  _M0L6_2atmpS3107 = _M0L6_2atmpS3136;
  #line 544 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3108
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1366);
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3135 = moonbit_add_string(_M0L6_2atmpS3107, _M0L6_2atmpS3108);
  moonbit_decref(_M0L6_2atmpS3107);
  moonbit_decref(_M0L6_2atmpS3108);
  _M0L6_2atmpS3106 = _M0L6_2atmpS3135;
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3134
  = moonbit_add_string(_M0L6_2atmpS3106, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3106);
  _M0L6_2atmpS3104 = _M0L6_2atmpS3134;
  #line 544 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3105
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1364);
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3133 = moonbit_add_string(_M0L6_2atmpS3104, _M0L6_2atmpS3105);
  moonbit_decref(_M0L6_2atmpS3104);
  moonbit_decref(_M0L6_2atmpS3105);
  _M0L6_2atmpS3103 = _M0L6_2atmpS3133;
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3132
  = moonbit_add_string(_M0L6_2atmpS3103, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3103);
  _M0L6_2atmpS3101 = _M0L6_2atmpS3132;
  #line 544 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3102
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1365);
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3131 = moonbit_add_string(_M0L6_2atmpS3101, _M0L6_2atmpS3102);
  moonbit_decref(_M0L6_2atmpS3101);
  moonbit_decref(_M0L6_2atmpS3102);
  _M0L6_2atmpS3100 = _M0L6_2atmpS3131;
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3130
  = moonbit_add_string(_M0L6_2atmpS3100, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3100);
  _M0L6_2atmpS3099 = _M0L6_2atmpS3130;
  #line 543 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3099);
  #line 546 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1357,
  moonbit_string_t _M0L8filenameS1354,
  int32_t _M0L5indexS1348,
  struct _M0TWssbEu* _M0L14handle__resultS1344,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1346
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1324;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1353;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1326;
  moonbit_string_t* _M0L5attrsS1327;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1347;
  moonbit_string_t _M0L4nameS1330;
  moonbit_string_t _M0L4nameS1328;
  int32_t _M0L6_2atmpS3096;
  struct _M0TWEOs* _M0L5_2aitS1332;
  struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__* _closure_3616;
  struct _M0TWEOc* _M0L6_2atmpS3087;
  struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__* _closure_3617;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3088;
  struct moonbit_result_0 _result_3618;
  #line 407 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1357);
  moonbit_incref(_M0FP48clawteam8clawteam8internal19lru__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1353
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal19lru__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1354);
  if (_M0L7_2abindS1353 == 0) {
    struct moonbit_result_0 _result_3609;
    if (_M0L7_2abindS1353) {
      moonbit_decref(_M0L7_2abindS1353);
    }
    moonbit_decref(_M0L17error__to__stringS1346);
    moonbit_decref(_M0L14handle__resultS1344);
    _result_3609.tag = 1;
    _result_3609.data.ok = 0;
    return _result_3609;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1355 =
      _M0L7_2abindS1353;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1356 =
      _M0L7_2aSomeS1355;
    _M0L10index__mapS1324 = _M0L13_2aindex__mapS1356;
    goto join_1323;
  }
  join_1323:;
  #line 416 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1347
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1324, _M0L5indexS1348);
  if (_M0L7_2abindS1347 == 0) {
    struct moonbit_result_0 _result_3611;
    if (_M0L7_2abindS1347) {
      moonbit_decref(_M0L7_2abindS1347);
    }
    moonbit_decref(_M0L17error__to__stringS1346);
    moonbit_decref(_M0L14handle__resultS1344);
    _result_3611.tag = 1;
    _result_3611.data.ok = 0;
    return _result_3611;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1349 =
      _M0L7_2abindS1347;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1350 = _M0L7_2aSomeS1349;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3143 = _M0L4_2axS1350->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1351 = _M0L8_2afieldS3143;
    moonbit_string_t* _M0L8_2afieldS3142 = _M0L4_2axS1350->$1;
    int32_t _M0L6_2acntS3485 = Moonbit_object_header(_M0L4_2axS1350)->rc;
    moonbit_string_t* _M0L8_2aattrsS1352;
    if (_M0L6_2acntS3485 > 1) {
      int32_t _M0L11_2anew__cntS3486 = _M0L6_2acntS3485 - 1;
      Moonbit_object_header(_M0L4_2axS1350)->rc = _M0L11_2anew__cntS3486;
      moonbit_incref(_M0L8_2afieldS3142);
      moonbit_incref(_M0L4_2afS1351);
    } else if (_M0L6_2acntS3485 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1350);
    }
    _M0L8_2aattrsS1352 = _M0L8_2afieldS3142;
    _M0L1fS1326 = _M0L4_2afS1351;
    _M0L5attrsS1327 = _M0L8_2aattrsS1352;
    goto join_1325;
  }
  join_1325:;
  _M0L6_2atmpS3096 = Moonbit_array_length(_M0L5attrsS1327);
  if (_M0L6_2atmpS3096 >= 1) {
    moonbit_string_t _M0L6_2atmpS3141 = (moonbit_string_t)_M0L5attrsS1327[0];
    moonbit_string_t _M0L7_2anameS1331 = _M0L6_2atmpS3141;
    moonbit_incref(_M0L7_2anameS1331);
    _M0L4nameS1330 = _M0L7_2anameS1331;
    goto join_1329;
  } else {
    _M0L4nameS1328 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3612;
  join_1329:;
  _M0L4nameS1328 = _M0L4nameS1330;
  joinlet_3612:;
  #line 417 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1332 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1327);
  while (1) {
    moonbit_string_t _M0L4attrS1334;
    moonbit_string_t _M0L7_2abindS1341;
    int32_t _M0L6_2atmpS3080;
    int64_t _M0L6_2atmpS3079;
    moonbit_incref(_M0L5_2aitS1332);
    #line 419 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1341 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1332);
    if (_M0L7_2abindS1341 == 0) {
      if (_M0L7_2abindS1341) {
        moonbit_decref(_M0L7_2abindS1341);
      }
      moonbit_decref(_M0L5_2aitS1332);
    } else {
      moonbit_string_t _M0L7_2aSomeS1342 = _M0L7_2abindS1341;
      moonbit_string_t _M0L7_2aattrS1343 = _M0L7_2aSomeS1342;
      _M0L4attrS1334 = _M0L7_2aattrS1343;
      goto join_1333;
    }
    goto joinlet_3614;
    join_1333:;
    _M0L6_2atmpS3080 = Moonbit_array_length(_M0L4attrS1334);
    _M0L6_2atmpS3079 = (int64_t)_M0L6_2atmpS3080;
    moonbit_incref(_M0L4attrS1334);
    #line 420 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1334, 5, 0, _M0L6_2atmpS3079)
    ) {
      int32_t _M0L6_2atmpS3086 = _M0L4attrS1334[0];
      int32_t _M0L4_2axS1335 = _M0L6_2atmpS3086;
      if (_M0L4_2axS1335 == 112) {
        int32_t _M0L6_2atmpS3085 = _M0L4attrS1334[1];
        int32_t _M0L4_2axS1336 = _M0L6_2atmpS3085;
        if (_M0L4_2axS1336 == 97) {
          int32_t _M0L6_2atmpS3084 = _M0L4attrS1334[2];
          int32_t _M0L4_2axS1337 = _M0L6_2atmpS3084;
          if (_M0L4_2axS1337 == 110) {
            int32_t _M0L6_2atmpS3083 = _M0L4attrS1334[3];
            int32_t _M0L4_2axS1338 = _M0L6_2atmpS3083;
            if (_M0L4_2axS1338 == 105) {
              int32_t _M0L6_2atmpS3140 = _M0L4attrS1334[4];
              int32_t _M0L6_2atmpS3082;
              int32_t _M0L4_2axS1339;
              moonbit_decref(_M0L4attrS1334);
              _M0L6_2atmpS3082 = _M0L6_2atmpS3140;
              _M0L4_2axS1339 = _M0L6_2atmpS3082;
              if (_M0L4_2axS1339 == 99) {
                void* _M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3081;
                struct moonbit_result_0 _result_3615;
                moonbit_decref(_M0L17error__to__stringS1346);
                moonbit_decref(_M0L14handle__resultS1344);
                moonbit_decref(_M0L5_2aitS1332);
                moonbit_decref(_M0L1fS1326);
                _M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3081
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3081)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3081)->$0
                = _M0L4nameS1328;
                _result_3615.tag = 0;
                _result_3615.data.err
                = _M0L124clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3081;
                return _result_3615;
              }
            } else {
              moonbit_decref(_M0L4attrS1334);
            }
          } else {
            moonbit_decref(_M0L4attrS1334);
          }
        } else {
          moonbit_decref(_M0L4attrS1334);
        }
      } else {
        moonbit_decref(_M0L4attrS1334);
      }
    } else {
      moonbit_decref(_M0L4attrS1334);
    }
    continue;
    joinlet_3614:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1344);
  moonbit_incref(_M0L4nameS1328);
  _closure_3616
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__*)moonbit_malloc(sizeof(struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__));
  Moonbit_object_header(_closure_3616)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__, $0) >> 2, 2, 0);
  _closure_3616->code
  = &_M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testC3093l427;
  _closure_3616->$0 = _M0L14handle__resultS1344;
  _closure_3616->$1 = _M0L4nameS1328;
  _M0L6_2atmpS3087 = (struct _M0TWEOc*)_closure_3616;
  _closure_3617
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__*)moonbit_malloc(sizeof(struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__));
  Moonbit_object_header(_closure_3617)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__, $0) >> 2, 3, 0);
  _closure_3617->code
  = &_M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testC3089l428;
  _closure_3617->$0 = _M0L17error__to__stringS1346;
  _closure_3617->$1 = _M0L14handle__resultS1344;
  _closure_3617->$2 = _M0L4nameS1328;
  _M0L6_2atmpS3088 = (struct _M0TWRPC15error5ErrorEu*)_closure_3617;
  #line 425 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal19lru__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1326, _M0L6_2atmpS3087, _M0L6_2atmpS3088);
  _result_3618.tag = 1;
  _result_3618.data.ok = 1;
  return _result_3618;
}

int32_t _M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testC3093l427(
  struct _M0TWEOc* _M0L6_2aenvS3094
) {
  struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__* _M0L14_2acasted__envS3095;
  moonbit_string_t _M0L8_2afieldS3145;
  moonbit_string_t _M0L4nameS1328;
  struct _M0TWssbEu* _M0L8_2afieldS3144;
  int32_t _M0L6_2acntS3487;
  struct _M0TWssbEu* _M0L14handle__resultS1344;
  #line 427 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3095
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3093__l427__*)_M0L6_2aenvS3094;
  _M0L8_2afieldS3145 = _M0L14_2acasted__envS3095->$1;
  _M0L4nameS1328 = _M0L8_2afieldS3145;
  _M0L8_2afieldS3144 = _M0L14_2acasted__envS3095->$0;
  _M0L6_2acntS3487 = Moonbit_object_header(_M0L14_2acasted__envS3095)->rc;
  if (_M0L6_2acntS3487 > 1) {
    int32_t _M0L11_2anew__cntS3488 = _M0L6_2acntS3487 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3095)->rc
    = _M0L11_2anew__cntS3488;
    moonbit_incref(_M0L4nameS1328);
    moonbit_incref(_M0L8_2afieldS3144);
  } else if (_M0L6_2acntS3487 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3095);
  }
  _M0L14handle__resultS1344 = _M0L8_2afieldS3144;
  #line 427 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1344->code(_M0L14handle__resultS1344, _M0L4nameS1328, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal19lru__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testC3089l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3090,
  void* _M0L3errS1345
) {
  struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__* _M0L14_2acasted__envS3091;
  moonbit_string_t _M0L8_2afieldS3148;
  moonbit_string_t _M0L4nameS1328;
  struct _M0TWssbEu* _M0L8_2afieldS3147;
  struct _M0TWssbEu* _M0L14handle__resultS1344;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3146;
  int32_t _M0L6_2acntS3489;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1346;
  moonbit_string_t _M0L6_2atmpS3092;
  #line 428 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3091
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2flru__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3089__l428__*)_M0L6_2aenvS3090;
  _M0L8_2afieldS3148 = _M0L14_2acasted__envS3091->$2;
  _M0L4nameS1328 = _M0L8_2afieldS3148;
  _M0L8_2afieldS3147 = _M0L14_2acasted__envS3091->$1;
  _M0L14handle__resultS1344 = _M0L8_2afieldS3147;
  _M0L8_2afieldS3146 = _M0L14_2acasted__envS3091->$0;
  _M0L6_2acntS3489 = Moonbit_object_header(_M0L14_2acasted__envS3091)->rc;
  if (_M0L6_2acntS3489 > 1) {
    int32_t _M0L11_2anew__cntS3490 = _M0L6_2acntS3489 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3091)->rc
    = _M0L11_2anew__cntS3490;
    moonbit_incref(_M0L4nameS1328);
    moonbit_incref(_M0L14handle__resultS1344);
    moonbit_incref(_M0L8_2afieldS3146);
  } else if (_M0L6_2acntS3489 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3091);
  }
  _M0L17error__to__stringS1346 = _M0L8_2afieldS3146;
  #line 428 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3092
  = _M0L17error__to__stringS1346->code(_M0L17error__to__stringS1346, _M0L3errS1345);
  #line 428 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1344->code(_M0L14handle__resultS1344, _M0L4nameS1328, _M0L6_2atmpS3092, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1318,
  struct _M0TWEOc* _M0L6on__okS1319,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1316
) {
  void* _M0L11_2atry__errS1314;
  struct moonbit_result_0 _tmp_3620;
  void* _M0L3errS1315;
  #line 375 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3620 = _M0L1fS1318->code(_M0L1fS1318);
  if (_tmp_3620.tag) {
    int32_t const _M0L5_2aokS3077 = _tmp_3620.data.ok;
    moonbit_decref(_M0L7on__errS1316);
  } else {
    void* const _M0L6_2aerrS3078 = _tmp_3620.data.err;
    moonbit_decref(_M0L6on__okS1319);
    _M0L11_2atry__errS1314 = _M0L6_2aerrS3078;
    goto join_1313;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1319->code(_M0L6on__okS1319);
  goto joinlet_3619;
  join_1313:;
  _M0L3errS1315 = _M0L11_2atry__errS1314;
  #line 383 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1316->code(_M0L7on__errS1316, _M0L3errS1315);
  joinlet_3619:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1273;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1286;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1291;
  struct _M0TUsiE** _M0L6_2atmpS3076;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1298;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1299;
  moonbit_string_t _M0L6_2atmpS3075;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1300;
  int32_t _M0L7_2abindS1301;
  int32_t _M0L2__S1302;
  #line 193 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1273 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1286
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1291 = 0;
  _M0L6_2atmpS3076 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1298
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1298)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1298->$0 = _M0L6_2atmpS3076;
  _M0L16file__and__indexS1298->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1299
  = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1286(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1286);
  #line 284 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3075 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1299, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1300
  = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1291(_M0L51moonbit__test__driver__internal__split__mbt__stringS1291, _M0L6_2atmpS3075, 47);
  _M0L7_2abindS1301 = _M0L10test__argsS1300->$1;
  _M0L2__S1302 = 0;
  while (1) {
    if (_M0L2__S1302 < _M0L7_2abindS1301) {
      moonbit_string_t* _M0L8_2afieldS3150 = _M0L10test__argsS1300->$0;
      moonbit_string_t* _M0L3bufS3074 = _M0L8_2afieldS3150;
      moonbit_string_t _M0L6_2atmpS3149 =
        (moonbit_string_t)_M0L3bufS3074[_M0L2__S1302];
      moonbit_string_t _M0L3argS1303 = _M0L6_2atmpS3149;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1304;
      moonbit_string_t _M0L4fileS1305;
      moonbit_string_t _M0L5rangeS1306;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1307;
      moonbit_string_t _M0L6_2atmpS3072;
      int32_t _M0L5startS1308;
      moonbit_string_t _M0L6_2atmpS3071;
      int32_t _M0L3endS1309;
      int32_t _M0L1iS1310;
      int32_t _M0L6_2atmpS3073;
      moonbit_incref(_M0L3argS1303);
      #line 288 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1304
      = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1291(_M0L51moonbit__test__driver__internal__split__mbt__stringS1291, _M0L3argS1303, 58);
      moonbit_incref(_M0L16file__and__rangeS1304);
      #line 289 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1305
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1304, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1306
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1304, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1307
      = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1291(_M0L51moonbit__test__driver__internal__split__mbt__stringS1291, _M0L5rangeS1306, 45);
      moonbit_incref(_M0L15start__and__endS1307);
      #line 294 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3072
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1307, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1308
      = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1273(_M0L45moonbit__test__driver__internal__parse__int__S1273, _M0L6_2atmpS3072);
      #line 295 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3071
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1307, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1309
      = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1273(_M0L45moonbit__test__driver__internal__parse__int__S1273, _M0L6_2atmpS3071);
      _M0L1iS1310 = _M0L5startS1308;
      while (1) {
        if (_M0L1iS1310 < _M0L3endS1309) {
          struct _M0TUsiE* _M0L8_2atupleS3069;
          int32_t _M0L6_2atmpS3070;
          moonbit_incref(_M0L4fileS1305);
          _M0L8_2atupleS3069
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3069)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3069->$0 = _M0L4fileS1305;
          _M0L8_2atupleS3069->$1 = _M0L1iS1310;
          moonbit_incref(_M0L16file__and__indexS1298);
          #line 297 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1298, _M0L8_2atupleS3069);
          _M0L6_2atmpS3070 = _M0L1iS1310 + 1;
          _M0L1iS1310 = _M0L6_2atmpS3070;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1305);
        }
        break;
      }
      _M0L6_2atmpS3073 = _M0L2__S1302 + 1;
      _M0L2__S1302 = _M0L6_2atmpS3073;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1300);
    }
    break;
  }
  return _M0L16file__and__indexS1298;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1291(
  int32_t _M0L6_2aenvS3050,
  moonbit_string_t _M0L1sS1292,
  int32_t _M0L3sepS1293
) {
  moonbit_string_t* _M0L6_2atmpS3068;
  struct _M0TPB5ArrayGsE* _M0L3resS1294;
  struct _M0TPC13ref3RefGiE* _M0L1iS1295;
  struct _M0TPC13ref3RefGiE* _M0L5startS1296;
  int32_t _M0L3valS3063;
  int32_t _M0L6_2atmpS3064;
  #line 261 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3068 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1294
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1294)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1294->$0 = _M0L6_2atmpS3068;
  _M0L3resS1294->$1 = 0;
  _M0L1iS1295
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1295)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1295->$0 = 0;
  _M0L5startS1296
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1296)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1296->$0 = 0;
  while (1) {
    int32_t _M0L3valS3051 = _M0L1iS1295->$0;
    int32_t _M0L6_2atmpS3052 = Moonbit_array_length(_M0L1sS1292);
    if (_M0L3valS3051 < _M0L6_2atmpS3052) {
      int32_t _M0L3valS3055 = _M0L1iS1295->$0;
      int32_t _M0L6_2atmpS3054;
      int32_t _M0L6_2atmpS3053;
      int32_t _M0L3valS3062;
      int32_t _M0L6_2atmpS3061;
      if (
        _M0L3valS3055 < 0
        || _M0L3valS3055 >= Moonbit_array_length(_M0L1sS1292)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3054 = _M0L1sS1292[_M0L3valS3055];
      _M0L6_2atmpS3053 = _M0L6_2atmpS3054;
      if (_M0L6_2atmpS3053 == _M0L3sepS1293) {
        int32_t _M0L3valS3057 = _M0L5startS1296->$0;
        int32_t _M0L3valS3058 = _M0L1iS1295->$0;
        moonbit_string_t _M0L6_2atmpS3056;
        int32_t _M0L3valS3060;
        int32_t _M0L6_2atmpS3059;
        moonbit_incref(_M0L1sS1292);
        #line 270 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS3056
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1292, _M0L3valS3057, _M0L3valS3058);
        moonbit_incref(_M0L3resS1294);
        #line 270 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1294, _M0L6_2atmpS3056);
        _M0L3valS3060 = _M0L1iS1295->$0;
        _M0L6_2atmpS3059 = _M0L3valS3060 + 1;
        _M0L5startS1296->$0 = _M0L6_2atmpS3059;
      }
      _M0L3valS3062 = _M0L1iS1295->$0;
      _M0L6_2atmpS3061 = _M0L3valS3062 + 1;
      _M0L1iS1295->$0 = _M0L6_2atmpS3061;
      continue;
    } else {
      moonbit_decref(_M0L1iS1295);
    }
    break;
  }
  _M0L3valS3063 = _M0L5startS1296->$0;
  _M0L6_2atmpS3064 = Moonbit_array_length(_M0L1sS1292);
  if (_M0L3valS3063 < _M0L6_2atmpS3064) {
    int32_t _M0L8_2afieldS3151 = _M0L5startS1296->$0;
    int32_t _M0L3valS3066;
    int32_t _M0L6_2atmpS3067;
    moonbit_string_t _M0L6_2atmpS3065;
    moonbit_decref(_M0L5startS1296);
    _M0L3valS3066 = _M0L8_2afieldS3151;
    _M0L6_2atmpS3067 = Moonbit_array_length(_M0L1sS1292);
    #line 276 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS3065
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1292, _M0L3valS3066, _M0L6_2atmpS3067);
    moonbit_incref(_M0L3resS1294);
    #line 276 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1294, _M0L6_2atmpS3065);
  } else {
    moonbit_decref(_M0L5startS1296);
    moonbit_decref(_M0L1sS1292);
  }
  return _M0L3resS1294;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1286(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279
) {
  moonbit_bytes_t* _M0L3tmpS1287;
  int32_t _M0L6_2atmpS3049;
  struct _M0TPB5ArrayGsE* _M0L3resS1288;
  int32_t _M0L1iS1289;
  #line 250 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1287
  = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3049 = Moonbit_array_length(_M0L3tmpS1287);
  #line 254 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1288 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3049);
  _M0L1iS1289 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3045 = Moonbit_array_length(_M0L3tmpS1287);
    if (_M0L1iS1289 < _M0L6_2atmpS3045) {
      moonbit_bytes_t _M0L6_2atmpS3152;
      moonbit_bytes_t _M0L6_2atmpS3047;
      moonbit_string_t _M0L6_2atmpS3046;
      int32_t _M0L6_2atmpS3048;
      if (
        _M0L1iS1289 < 0 || _M0L1iS1289 >= Moonbit_array_length(_M0L3tmpS1287)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3152 = (moonbit_bytes_t)_M0L3tmpS1287[_M0L1iS1289];
      _M0L6_2atmpS3047 = _M0L6_2atmpS3152;
      moonbit_incref(_M0L6_2atmpS3047);
      #line 256 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3046
      = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279, _M0L6_2atmpS3047);
      moonbit_incref(_M0L3resS1288);
      #line 256 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1288, _M0L6_2atmpS3046);
      _M0L6_2atmpS3048 = _M0L1iS1289 + 1;
      _M0L1iS1289 = _M0L6_2atmpS3048;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1287);
    }
    break;
  }
  return _M0L3resS1288;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1279(
  int32_t _M0L6_2aenvS2959,
  moonbit_bytes_t _M0L5bytesS1280
) {
  struct _M0TPB13StringBuilder* _M0L3resS1281;
  int32_t _M0L3lenS1282;
  struct _M0TPC13ref3RefGiE* _M0L1iS1283;
  #line 206 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1281 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1282 = Moonbit_array_length(_M0L5bytesS1280);
  _M0L1iS1283
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1283)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1283->$0 = 0;
  while (1) {
    int32_t _M0L3valS2960 = _M0L1iS1283->$0;
    if (_M0L3valS2960 < _M0L3lenS1282) {
      int32_t _M0L3valS3044 = _M0L1iS1283->$0;
      int32_t _M0L6_2atmpS3043;
      int32_t _M0L6_2atmpS3042;
      struct _M0TPC13ref3RefGiE* _M0L1cS1284;
      int32_t _M0L3valS2961;
      if (
        _M0L3valS3044 < 0
        || _M0L3valS3044 >= Moonbit_array_length(_M0L5bytesS1280)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3043 = _M0L5bytesS1280[_M0L3valS3044];
      _M0L6_2atmpS3042 = (int32_t)_M0L6_2atmpS3043;
      _M0L1cS1284
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1284)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1284->$0 = _M0L6_2atmpS3042;
      _M0L3valS2961 = _M0L1cS1284->$0;
      if (_M0L3valS2961 < 128) {
        int32_t _M0L8_2afieldS3153 = _M0L1cS1284->$0;
        int32_t _M0L3valS2963;
        int32_t _M0L6_2atmpS2962;
        int32_t _M0L3valS2965;
        int32_t _M0L6_2atmpS2964;
        moonbit_decref(_M0L1cS1284);
        _M0L3valS2963 = _M0L8_2afieldS3153;
        _M0L6_2atmpS2962 = _M0L3valS2963;
        moonbit_incref(_M0L3resS1281);
        #line 215 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1281, _M0L6_2atmpS2962);
        _M0L3valS2965 = _M0L1iS1283->$0;
        _M0L6_2atmpS2964 = _M0L3valS2965 + 1;
        _M0L1iS1283->$0 = _M0L6_2atmpS2964;
      } else {
        int32_t _M0L3valS2966 = _M0L1cS1284->$0;
        if (_M0L3valS2966 < 224) {
          int32_t _M0L3valS2968 = _M0L1iS1283->$0;
          int32_t _M0L6_2atmpS2967 = _M0L3valS2968 + 1;
          int32_t _M0L3valS2977;
          int32_t _M0L6_2atmpS2976;
          int32_t _M0L6_2atmpS2970;
          int32_t _M0L3valS2975;
          int32_t _M0L6_2atmpS2974;
          int32_t _M0L6_2atmpS2973;
          int32_t _M0L6_2atmpS2972;
          int32_t _M0L6_2atmpS2971;
          int32_t _M0L6_2atmpS2969;
          int32_t _M0L8_2afieldS3154;
          int32_t _M0L3valS2979;
          int32_t _M0L6_2atmpS2978;
          int32_t _M0L3valS2981;
          int32_t _M0L6_2atmpS2980;
          if (_M0L6_2atmpS2967 >= _M0L3lenS1282) {
            moonbit_decref(_M0L1cS1284);
            moonbit_decref(_M0L1iS1283);
            moonbit_decref(_M0L5bytesS1280);
            break;
          }
          _M0L3valS2977 = _M0L1cS1284->$0;
          _M0L6_2atmpS2976 = _M0L3valS2977 & 31;
          _M0L6_2atmpS2970 = _M0L6_2atmpS2976 << 6;
          _M0L3valS2975 = _M0L1iS1283->$0;
          _M0L6_2atmpS2974 = _M0L3valS2975 + 1;
          if (
            _M0L6_2atmpS2974 < 0
            || _M0L6_2atmpS2974 >= Moonbit_array_length(_M0L5bytesS1280)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2973 = _M0L5bytesS1280[_M0L6_2atmpS2974];
          _M0L6_2atmpS2972 = (int32_t)_M0L6_2atmpS2973;
          _M0L6_2atmpS2971 = _M0L6_2atmpS2972 & 63;
          _M0L6_2atmpS2969 = _M0L6_2atmpS2970 | _M0L6_2atmpS2971;
          _M0L1cS1284->$0 = _M0L6_2atmpS2969;
          _M0L8_2afieldS3154 = _M0L1cS1284->$0;
          moonbit_decref(_M0L1cS1284);
          _M0L3valS2979 = _M0L8_2afieldS3154;
          _M0L6_2atmpS2978 = _M0L3valS2979;
          moonbit_incref(_M0L3resS1281);
          #line 222 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1281, _M0L6_2atmpS2978);
          _M0L3valS2981 = _M0L1iS1283->$0;
          _M0L6_2atmpS2980 = _M0L3valS2981 + 2;
          _M0L1iS1283->$0 = _M0L6_2atmpS2980;
        } else {
          int32_t _M0L3valS2982 = _M0L1cS1284->$0;
          if (_M0L3valS2982 < 240) {
            int32_t _M0L3valS2984 = _M0L1iS1283->$0;
            int32_t _M0L6_2atmpS2983 = _M0L3valS2984 + 2;
            int32_t _M0L3valS3000;
            int32_t _M0L6_2atmpS2999;
            int32_t _M0L6_2atmpS2992;
            int32_t _M0L3valS2998;
            int32_t _M0L6_2atmpS2997;
            int32_t _M0L6_2atmpS2996;
            int32_t _M0L6_2atmpS2995;
            int32_t _M0L6_2atmpS2994;
            int32_t _M0L6_2atmpS2993;
            int32_t _M0L6_2atmpS2986;
            int32_t _M0L3valS2991;
            int32_t _M0L6_2atmpS2990;
            int32_t _M0L6_2atmpS2989;
            int32_t _M0L6_2atmpS2988;
            int32_t _M0L6_2atmpS2987;
            int32_t _M0L6_2atmpS2985;
            int32_t _M0L8_2afieldS3155;
            int32_t _M0L3valS3002;
            int32_t _M0L6_2atmpS3001;
            int32_t _M0L3valS3004;
            int32_t _M0L6_2atmpS3003;
            if (_M0L6_2atmpS2983 >= _M0L3lenS1282) {
              moonbit_decref(_M0L1cS1284);
              moonbit_decref(_M0L1iS1283);
              moonbit_decref(_M0L5bytesS1280);
              break;
            }
            _M0L3valS3000 = _M0L1cS1284->$0;
            _M0L6_2atmpS2999 = _M0L3valS3000 & 15;
            _M0L6_2atmpS2992 = _M0L6_2atmpS2999 << 12;
            _M0L3valS2998 = _M0L1iS1283->$0;
            _M0L6_2atmpS2997 = _M0L3valS2998 + 1;
            if (
              _M0L6_2atmpS2997 < 0
              || _M0L6_2atmpS2997 >= Moonbit_array_length(_M0L5bytesS1280)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2996 = _M0L5bytesS1280[_M0L6_2atmpS2997];
            _M0L6_2atmpS2995 = (int32_t)_M0L6_2atmpS2996;
            _M0L6_2atmpS2994 = _M0L6_2atmpS2995 & 63;
            _M0L6_2atmpS2993 = _M0L6_2atmpS2994 << 6;
            _M0L6_2atmpS2986 = _M0L6_2atmpS2992 | _M0L6_2atmpS2993;
            _M0L3valS2991 = _M0L1iS1283->$0;
            _M0L6_2atmpS2990 = _M0L3valS2991 + 2;
            if (
              _M0L6_2atmpS2990 < 0
              || _M0L6_2atmpS2990 >= Moonbit_array_length(_M0L5bytesS1280)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2989 = _M0L5bytesS1280[_M0L6_2atmpS2990];
            _M0L6_2atmpS2988 = (int32_t)_M0L6_2atmpS2989;
            _M0L6_2atmpS2987 = _M0L6_2atmpS2988 & 63;
            _M0L6_2atmpS2985 = _M0L6_2atmpS2986 | _M0L6_2atmpS2987;
            _M0L1cS1284->$0 = _M0L6_2atmpS2985;
            _M0L8_2afieldS3155 = _M0L1cS1284->$0;
            moonbit_decref(_M0L1cS1284);
            _M0L3valS3002 = _M0L8_2afieldS3155;
            _M0L6_2atmpS3001 = _M0L3valS3002;
            moonbit_incref(_M0L3resS1281);
            #line 231 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1281, _M0L6_2atmpS3001);
            _M0L3valS3004 = _M0L1iS1283->$0;
            _M0L6_2atmpS3003 = _M0L3valS3004 + 3;
            _M0L1iS1283->$0 = _M0L6_2atmpS3003;
          } else {
            int32_t _M0L3valS3006 = _M0L1iS1283->$0;
            int32_t _M0L6_2atmpS3005 = _M0L3valS3006 + 3;
            int32_t _M0L3valS3029;
            int32_t _M0L6_2atmpS3028;
            int32_t _M0L6_2atmpS3021;
            int32_t _M0L3valS3027;
            int32_t _M0L6_2atmpS3026;
            int32_t _M0L6_2atmpS3025;
            int32_t _M0L6_2atmpS3024;
            int32_t _M0L6_2atmpS3023;
            int32_t _M0L6_2atmpS3022;
            int32_t _M0L6_2atmpS3014;
            int32_t _M0L3valS3020;
            int32_t _M0L6_2atmpS3019;
            int32_t _M0L6_2atmpS3018;
            int32_t _M0L6_2atmpS3017;
            int32_t _M0L6_2atmpS3016;
            int32_t _M0L6_2atmpS3015;
            int32_t _M0L6_2atmpS3008;
            int32_t _M0L3valS3013;
            int32_t _M0L6_2atmpS3012;
            int32_t _M0L6_2atmpS3011;
            int32_t _M0L6_2atmpS3010;
            int32_t _M0L6_2atmpS3009;
            int32_t _M0L6_2atmpS3007;
            int32_t _M0L3valS3031;
            int32_t _M0L6_2atmpS3030;
            int32_t _M0L3valS3035;
            int32_t _M0L6_2atmpS3034;
            int32_t _M0L6_2atmpS3033;
            int32_t _M0L6_2atmpS3032;
            int32_t _M0L8_2afieldS3156;
            int32_t _M0L3valS3039;
            int32_t _M0L6_2atmpS3038;
            int32_t _M0L6_2atmpS3037;
            int32_t _M0L6_2atmpS3036;
            int32_t _M0L3valS3041;
            int32_t _M0L6_2atmpS3040;
            if (_M0L6_2atmpS3005 >= _M0L3lenS1282) {
              moonbit_decref(_M0L1cS1284);
              moonbit_decref(_M0L1iS1283);
              moonbit_decref(_M0L5bytesS1280);
              break;
            }
            _M0L3valS3029 = _M0L1cS1284->$0;
            _M0L6_2atmpS3028 = _M0L3valS3029 & 7;
            _M0L6_2atmpS3021 = _M0L6_2atmpS3028 << 18;
            _M0L3valS3027 = _M0L1iS1283->$0;
            _M0L6_2atmpS3026 = _M0L3valS3027 + 1;
            if (
              _M0L6_2atmpS3026 < 0
              || _M0L6_2atmpS3026 >= Moonbit_array_length(_M0L5bytesS1280)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3025 = _M0L5bytesS1280[_M0L6_2atmpS3026];
            _M0L6_2atmpS3024 = (int32_t)_M0L6_2atmpS3025;
            _M0L6_2atmpS3023 = _M0L6_2atmpS3024 & 63;
            _M0L6_2atmpS3022 = _M0L6_2atmpS3023 << 12;
            _M0L6_2atmpS3014 = _M0L6_2atmpS3021 | _M0L6_2atmpS3022;
            _M0L3valS3020 = _M0L1iS1283->$0;
            _M0L6_2atmpS3019 = _M0L3valS3020 + 2;
            if (
              _M0L6_2atmpS3019 < 0
              || _M0L6_2atmpS3019 >= Moonbit_array_length(_M0L5bytesS1280)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3018 = _M0L5bytesS1280[_M0L6_2atmpS3019];
            _M0L6_2atmpS3017 = (int32_t)_M0L6_2atmpS3018;
            _M0L6_2atmpS3016 = _M0L6_2atmpS3017 & 63;
            _M0L6_2atmpS3015 = _M0L6_2atmpS3016 << 6;
            _M0L6_2atmpS3008 = _M0L6_2atmpS3014 | _M0L6_2atmpS3015;
            _M0L3valS3013 = _M0L1iS1283->$0;
            _M0L6_2atmpS3012 = _M0L3valS3013 + 3;
            if (
              _M0L6_2atmpS3012 < 0
              || _M0L6_2atmpS3012 >= Moonbit_array_length(_M0L5bytesS1280)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3011 = _M0L5bytesS1280[_M0L6_2atmpS3012];
            _M0L6_2atmpS3010 = (int32_t)_M0L6_2atmpS3011;
            _M0L6_2atmpS3009 = _M0L6_2atmpS3010 & 63;
            _M0L6_2atmpS3007 = _M0L6_2atmpS3008 | _M0L6_2atmpS3009;
            _M0L1cS1284->$0 = _M0L6_2atmpS3007;
            _M0L3valS3031 = _M0L1cS1284->$0;
            _M0L6_2atmpS3030 = _M0L3valS3031 - 65536;
            _M0L1cS1284->$0 = _M0L6_2atmpS3030;
            _M0L3valS3035 = _M0L1cS1284->$0;
            _M0L6_2atmpS3034 = _M0L3valS3035 >> 10;
            _M0L6_2atmpS3033 = _M0L6_2atmpS3034 + 55296;
            _M0L6_2atmpS3032 = _M0L6_2atmpS3033;
            moonbit_incref(_M0L3resS1281);
            #line 242 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1281, _M0L6_2atmpS3032);
            _M0L8_2afieldS3156 = _M0L1cS1284->$0;
            moonbit_decref(_M0L1cS1284);
            _M0L3valS3039 = _M0L8_2afieldS3156;
            _M0L6_2atmpS3038 = _M0L3valS3039 & 1023;
            _M0L6_2atmpS3037 = _M0L6_2atmpS3038 + 56320;
            _M0L6_2atmpS3036 = _M0L6_2atmpS3037;
            moonbit_incref(_M0L3resS1281);
            #line 243 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1281, _M0L6_2atmpS3036);
            _M0L3valS3041 = _M0L1iS1283->$0;
            _M0L6_2atmpS3040 = _M0L3valS3041 + 4;
            _M0L1iS1283->$0 = _M0L6_2atmpS3040;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1283);
      moonbit_decref(_M0L5bytesS1280);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1281);
}

int32_t _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1273(
  int32_t _M0L6_2aenvS2952,
  moonbit_string_t _M0L1sS1274
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1275;
  int32_t _M0L3lenS1276;
  int32_t _M0L1iS1277;
  int32_t _M0L8_2afieldS3157;
  #line 197 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1275
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1275)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1275->$0 = 0;
  _M0L3lenS1276 = Moonbit_array_length(_M0L1sS1274);
  _M0L1iS1277 = 0;
  while (1) {
    if (_M0L1iS1277 < _M0L3lenS1276) {
      int32_t _M0L3valS2957 = _M0L3resS1275->$0;
      int32_t _M0L6_2atmpS2954 = _M0L3valS2957 * 10;
      int32_t _M0L6_2atmpS2956;
      int32_t _M0L6_2atmpS2955;
      int32_t _M0L6_2atmpS2953;
      int32_t _M0L6_2atmpS2958;
      if (
        _M0L1iS1277 < 0 || _M0L1iS1277 >= Moonbit_array_length(_M0L1sS1274)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2956 = _M0L1sS1274[_M0L1iS1277];
      _M0L6_2atmpS2955 = _M0L6_2atmpS2956 - 48;
      _M0L6_2atmpS2953 = _M0L6_2atmpS2954 + _M0L6_2atmpS2955;
      _M0L3resS1275->$0 = _M0L6_2atmpS2953;
      _M0L6_2atmpS2958 = _M0L1iS1277 + 1;
      _M0L1iS1277 = _M0L6_2atmpS2958;
      continue;
    } else {
      moonbit_decref(_M0L1sS1274);
    }
    break;
  }
  _M0L8_2afieldS3157 = _M0L3resS1275->$0;
  moonbit_decref(_M0L3resS1275);
  return _M0L8_2afieldS3157;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1253,
  moonbit_string_t _M0L12_2adiscard__S1254,
  int32_t _M0L12_2adiscard__S1255,
  struct _M0TWssbEu* _M0L12_2adiscard__S1256,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1257
) {
  struct moonbit_result_0 _result_3627;
  #line 34 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1257);
  moonbit_decref(_M0L12_2adiscard__S1256);
  moonbit_decref(_M0L12_2adiscard__S1254);
  moonbit_decref(_M0L12_2adiscard__S1253);
  _result_3627.tag = 1;
  _result_3627.data.ok = 0;
  return _result_3627;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1258,
  moonbit_string_t _M0L12_2adiscard__S1259,
  int32_t _M0L12_2adiscard__S1260,
  struct _M0TWssbEu* _M0L12_2adiscard__S1261,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1262
) {
  struct moonbit_result_0 _result_3628;
  #line 34 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1262);
  moonbit_decref(_M0L12_2adiscard__S1261);
  moonbit_decref(_M0L12_2adiscard__S1259);
  moonbit_decref(_M0L12_2adiscard__S1258);
  _result_3628.tag = 1;
  _result_3628.data.ok = 0;
  return _result_3628;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1263,
  moonbit_string_t _M0L12_2adiscard__S1264,
  int32_t _M0L12_2adiscard__S1265,
  struct _M0TWssbEu* _M0L12_2adiscard__S1266,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1267
) {
  struct moonbit_result_0 _result_3629;
  #line 34 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1267);
  moonbit_decref(_M0L12_2adiscard__S1266);
  moonbit_decref(_M0L12_2adiscard__S1264);
  moonbit_decref(_M0L12_2adiscard__S1263);
  _result_3629.tag = 1;
  _result_3629.data.ok = 0;
  return _result_3629;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19lru__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1268,
  moonbit_string_t _M0L12_2adiscard__S1269,
  int32_t _M0L12_2adiscard__S1270,
  struct _M0TWssbEu* _M0L12_2adiscard__S1271,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1272
) {
  struct moonbit_result_0 _result_3630;
  #line 34 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1272);
  moonbit_decref(_M0L12_2adiscard__S1271);
  moonbit_decref(_M0L12_2adiscard__S1269);
  moonbit_decref(_M0L12_2adiscard__S1268);
  _result_3630.tag = 1;
  _result_3630.data.ok = 0;
  return _result_3630;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal19lru__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1252
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1252);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19lru__blackbox__test41____test__63616368655f746573742e6d6274__0(
  
) {
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0L5cacheS1249;
  int32_t _M0L6_2atmpS2881;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2882;
  struct _M0TPB6ToJson _M0L6_2atmpS2871;
  moonbit_string_t _M0L6_2atmpS2880;
  void* _M0L6_2atmpS2879;
  void* _M0L6_2atmpS2872;
  moonbit_string_t _M0L6_2atmpS2875;
  moonbit_string_t _M0L6_2atmpS2876;
  moonbit_string_t _M0L6_2atmpS2877;
  moonbit_string_t _M0L6_2atmpS2878;
  moonbit_string_t* _M0L6_2atmpS2874;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2873;
  struct moonbit_result_0 _tmp_3631;
  int64_t _M0L6_2atmpS2898;
  struct _M0Y5Int64* _M0L14_2aboxed__selfS2899;
  struct _M0TPB6ToJson _M0L6_2atmpS2885;
  moonbit_string_t _M0L6_2atmpS2897;
  void* _M0L6_2atmpS2896;
  void** _M0L6_2atmpS2895;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2894;
  void* _M0L6_2atmpS2893;
  void* _M0L6_2atmpS2886;
  moonbit_string_t _M0L6_2atmpS2889;
  moonbit_string_t _M0L6_2atmpS2890;
  moonbit_string_t _M0L6_2atmpS2891;
  moonbit_string_t _M0L6_2atmpS2892;
  moonbit_string_t* _M0L6_2atmpS2888;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2887;
  struct moonbit_result_0 _tmp_3633;
  int64_t _M0L6_2atmpS2910;
  struct _M0Y5Int64* _M0L14_2aboxed__selfS2911;
  struct _M0TPB6ToJson _M0L6_2atmpS2902;
  void* _M0L6_2atmpS2903;
  moonbit_string_t _M0L6_2atmpS2906;
  moonbit_string_t _M0L6_2atmpS2907;
  moonbit_string_t _M0L6_2atmpS2908;
  moonbit_string_t _M0L6_2atmpS2909;
  moonbit_string_t* _M0L6_2atmpS2905;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2904;
  struct moonbit_result_0 _tmp_3635;
  struct _M0TPB6ToJson _M0L6_2atmpS2914;
  moonbit_string_t _M0L6_2atmpS2931;
  void* _M0L6_2atmpS2930;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2926;
  moonbit_string_t _M0L6_2atmpS2929;
  void* _M0L6_2atmpS2928;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2927;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1250;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2925;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2924;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2923;
  void* _M0L6_2atmpS2922;
  void* _M0L6_2atmpS2915;
  moonbit_string_t _M0L6_2atmpS2918;
  moonbit_string_t _M0L6_2atmpS2919;
  moonbit_string_t _M0L6_2atmpS2920;
  moonbit_string_t _M0L6_2atmpS2921;
  moonbit_string_t* _M0L6_2atmpS2917;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2916;
  struct moonbit_result_0 _tmp_3637;
  struct _M0TPB6ToJson _M0L6_2atmpS2934;
  moonbit_string_t _M0L6_2atmpS2951;
  void* _M0L6_2atmpS2950;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2946;
  moonbit_string_t _M0L6_2atmpS2949;
  void* _M0L6_2atmpS2948;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2947;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1251;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2945;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2944;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2943;
  void* _M0L6_2atmpS2942;
  void* _M0L6_2atmpS2935;
  moonbit_string_t _M0L6_2atmpS2938;
  moonbit_string_t _M0L6_2atmpS2939;
  moonbit_string_t _M0L6_2atmpS2940;
  moonbit_string_t _M0L6_2atmpS2941;
  moonbit_string_t* _M0L6_2atmpS2937;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2936;
  #line 2 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  #line 3 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L5cacheS1249
  = _M0FP48clawteam8clawteam8internal3lru13cache_2einnerGsiE(2);
  moonbit_incref(_M0L5cacheS1249);
  #line 4 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0MP48clawteam8clawteam8internal3lru5Cache3setGsiE(_M0L5cacheS1249, (moonbit_string_t)moonbit_string_literal_9.data, 1);
  moonbit_incref(_M0L5cacheS1249);
  #line 5 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0MP48clawteam8clawteam8internal3lru5Cache3setGsiE(_M0L5cacheS1249, (moonbit_string_t)moonbit_string_literal_10.data, 2);
  moonbit_incref(_M0L5cacheS1249);
  #line 6 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2881
  = _M0MP48clawteam8clawteam8internal3lru5Cache7op__getGsiE(_M0L5cacheS1249, (moonbit_string_t)moonbit_string_literal_9.data);
  _M0L14_2aboxed__selfS2882
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2882)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2882->$0 = _M0L6_2atmpS2881;
  _M0L6_2atmpS2871
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2882
  };
  _M0L6_2atmpS2880 = 0;
  #line 6 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2879 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS2880);
  _M0L6_2atmpS2872 = _M0L6_2atmpS2879;
  _M0L6_2atmpS2875 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS2876 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS2877 = 0;
  _M0L6_2atmpS2878 = 0;
  _M0L6_2atmpS2874 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2874[0] = _M0L6_2atmpS2875;
  _M0L6_2atmpS2874[1] = _M0L6_2atmpS2876;
  _M0L6_2atmpS2874[2] = _M0L6_2atmpS2877;
  _M0L6_2atmpS2874[3] = _M0L6_2atmpS2878;
  _M0L6_2atmpS2873
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2873)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2873->$0 = _M0L6_2atmpS2874;
  _M0L6_2atmpS2873->$1 = 4;
  #line 6 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _tmp_3631
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2871, _M0L6_2atmpS2872, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2873);
  if (_tmp_3631.tag) {
    int32_t const _M0L5_2aokS2883 = _tmp_3631.data.ok;
  } else {
    void* const _M0L6_2aerrS2884 = _tmp_3631.data.err;
    struct moonbit_result_0 _result_3632;
    moonbit_decref(_M0L5cacheS1249);
    _result_3632.tag = 0;
    _result_3632.data.err = _M0L6_2aerrS2884;
    return _result_3632;
  }
  moonbit_incref(_M0L5cacheS1249);
  #line 7 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2898
  = _M0MP48clawteam8clawteam8internal3lru5Cache3getGsiE(_M0L5cacheS1249, (moonbit_string_t)moonbit_string_literal_10.data);
  _M0L14_2aboxed__selfS2899
  = (struct _M0Y5Int64*)moonbit_malloc(sizeof(struct _M0Y5Int64));
  Moonbit_object_header(_M0L14_2aboxed__selfS2899)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y5Int64) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2899->$0 = _M0L6_2atmpS2898;
  _M0L6_2atmpS2885
  = (struct _M0TPB6ToJson){
    _M0FP0120moonbitlang_2fcore_2foption_2fOption_5bInt_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2899
  };
  _M0L6_2atmpS2897 = 0;
  #line 7 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2896 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS2897);
  _M0L6_2atmpS2895 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2895[0] = _M0L6_2atmpS2896;
  _M0L6_2atmpS2894
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS2894)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2894->$0 = _M0L6_2atmpS2895;
  _M0L6_2atmpS2894->$1 = 1;
  #line 7 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2893 = _M0MPC14json4Json5array(_M0L6_2atmpS2894);
  _M0L6_2atmpS2886 = _M0L6_2atmpS2893;
  _M0L6_2atmpS2889 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS2890 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS2891 = 0;
  _M0L6_2atmpS2892 = 0;
  _M0L6_2atmpS2888 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2888[0] = _M0L6_2atmpS2889;
  _M0L6_2atmpS2888[1] = _M0L6_2atmpS2890;
  _M0L6_2atmpS2888[2] = _M0L6_2atmpS2891;
  _M0L6_2atmpS2888[3] = _M0L6_2atmpS2892;
  _M0L6_2atmpS2887
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2887)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2887->$0 = _M0L6_2atmpS2888;
  _M0L6_2atmpS2887->$1 = 4;
  #line 7 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _tmp_3633
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2885, _M0L6_2atmpS2886, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS2887);
  if (_tmp_3633.tag) {
    int32_t const _M0L5_2aokS2900 = _tmp_3633.data.ok;
  } else {
    void* const _M0L6_2aerrS2901 = _tmp_3633.data.err;
    struct moonbit_result_0 _result_3634;
    moonbit_decref(_M0L5cacheS1249);
    _result_3634.tag = 0;
    _result_3634.data.err = _M0L6_2aerrS2901;
    return _result_3634;
  }
  moonbit_incref(_M0L5cacheS1249);
  #line 8 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2910
  = _M0MP48clawteam8clawteam8internal3lru5Cache3getGsiE(_M0L5cacheS1249, (moonbit_string_t)moonbit_string_literal_17.data);
  _M0L14_2aboxed__selfS2911
  = (struct _M0Y5Int64*)moonbit_malloc(sizeof(struct _M0Y5Int64));
  Moonbit_object_header(_M0L14_2aboxed__selfS2911)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y5Int64) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2911->$0 = _M0L6_2atmpS2910;
  _M0L6_2atmpS2902
  = (struct _M0TPB6ToJson){
    _M0FP0120moonbitlang_2fcore_2foption_2fOption_5bInt_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2911
  };
  moonbit_incref(_M0FPC17prelude4null);
  _M0L6_2atmpS2903 = _M0FPC17prelude4null;
  _M0L6_2atmpS2906 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS2907 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS2908 = 0;
  _M0L6_2atmpS2909 = 0;
  _M0L6_2atmpS2905 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2905[0] = _M0L6_2atmpS2906;
  _M0L6_2atmpS2905[1] = _M0L6_2atmpS2907;
  _M0L6_2atmpS2905[2] = _M0L6_2atmpS2908;
  _M0L6_2atmpS2905[3] = _M0L6_2atmpS2909;
  _M0L6_2atmpS2904
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2904)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2904->$0 = _M0L6_2atmpS2905;
  _M0L6_2atmpS2904->$1 = 4;
  #line 8 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _tmp_3635
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2902, _M0L6_2atmpS2903, (moonbit_string_t)moonbit_string_literal_20.data, _M0L6_2atmpS2904);
  if (_tmp_3635.tag) {
    int32_t const _M0L5_2aokS2912 = _tmp_3635.data.ok;
  } else {
    void* const _M0L6_2aerrS2913 = _tmp_3635.data.err;
    struct moonbit_result_0 _result_3636;
    moonbit_decref(_M0L5cacheS1249);
    _result_3636.tag = 0;
    _result_3636.data.err = _M0L6_2aerrS2913;
    return _result_3636;
  }
  moonbit_incref(_M0L5cacheS1249);
  _M0L6_2atmpS2914
  = (struct _M0TPB6ToJson){
    _M0FP0140clawteam_2fclawteam_2finternal_2flru_2fCache_5bString_2c_20Int_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L5cacheS1249
  };
  _M0L6_2atmpS2931 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2930 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS2931);
  _M0L8_2atupleS2926
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2926)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2926->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS2926->$1 = _M0L6_2atmpS2930;
  _M0L6_2atmpS2929 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2928 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS2929);
  _M0L8_2atupleS2927
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2927)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2927->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS2927->$1 = _M0L6_2atmpS2928;
  _M0L7_2abindS1250 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1250[0] = _M0L8_2atupleS2926;
  _M0L7_2abindS1250[1] = _M0L8_2atupleS2927;
  _M0L6_2atmpS2925 = _M0L7_2abindS1250;
  _M0L6_2atmpS2924
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2925
  };
  #line 9 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2923 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2924);
  #line 9 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2922 = _M0MPC14json4Json6object(_M0L6_2atmpS2923);
  _M0L6_2atmpS2915 = _M0L6_2atmpS2922;
  _M0L6_2atmpS2918 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS2919 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS2920 = 0;
  _M0L6_2atmpS2921 = 0;
  _M0L6_2atmpS2917 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2917[0] = _M0L6_2atmpS2918;
  _M0L6_2atmpS2917[1] = _M0L6_2atmpS2919;
  _M0L6_2atmpS2917[2] = _M0L6_2atmpS2920;
  _M0L6_2atmpS2917[3] = _M0L6_2atmpS2921;
  _M0L6_2atmpS2916
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2916)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2916->$0 = _M0L6_2atmpS2917;
  _M0L6_2atmpS2916->$1 = 4;
  #line 9 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _tmp_3637
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2914, _M0L6_2atmpS2915, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS2916);
  if (_tmp_3637.tag) {
    int32_t const _M0L5_2aokS2932 = _tmp_3637.data.ok;
  } else {
    void* const _M0L6_2aerrS2933 = _tmp_3637.data.err;
    struct moonbit_result_0 _result_3638;
    moonbit_decref(_M0L5cacheS1249);
    _result_3638.tag = 0;
    _result_3638.data.err = _M0L6_2aerrS2933;
    return _result_3638;
  }
  moonbit_incref(_M0L5cacheS1249);
  #line 10 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0MP48clawteam8clawteam8internal3lru5Cache3setGsiE(_M0L5cacheS1249, (moonbit_string_t)moonbit_string_literal_17.data, 3);
  _M0L6_2atmpS2934
  = (struct _M0TPB6ToJson){
    _M0FP0140clawteam_2fclawteam_2finternal_2flru_2fCache_5bString_2c_20Int_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L5cacheS1249
  };
  _M0L6_2atmpS2951 = 0;
  #line 11 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2950 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS2951);
  _M0L8_2atupleS2946
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2946)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2946->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS2946->$1 = _M0L6_2atmpS2950;
  _M0L6_2atmpS2949 = 0;
  #line 11 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2948 = _M0MPC14json4Json6number(0x1.8p+1, _M0L6_2atmpS2949);
  _M0L8_2atupleS2947
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2947)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2947->$0 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L8_2atupleS2947->$1 = _M0L6_2atmpS2948;
  _M0L7_2abindS1251 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1251[0] = _M0L8_2atupleS2946;
  _M0L7_2abindS1251[1] = _M0L8_2atupleS2947;
  _M0L6_2atmpS2945 = _M0L7_2abindS1251;
  _M0L6_2atmpS2944
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2945
  };
  #line 11 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2943 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2944);
  #line 11 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  _M0L6_2atmpS2942 = _M0MPC14json4Json6object(_M0L6_2atmpS2943);
  _M0L6_2atmpS2935 = _M0L6_2atmpS2942;
  _M0L6_2atmpS2938 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS2939 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS2940 = 0;
  _M0L6_2atmpS2941 = 0;
  _M0L6_2atmpS2937 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2937[0] = _M0L6_2atmpS2938;
  _M0L6_2atmpS2937[1] = _M0L6_2atmpS2939;
  _M0L6_2atmpS2937[2] = _M0L6_2atmpS2940;
  _M0L6_2atmpS2937[3] = _M0L6_2atmpS2941;
  _M0L6_2atmpS2936
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2936)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2936->$0 = _M0L6_2atmpS2937;
  _M0L6_2atmpS2936->$1 = 4;
  #line 11 "E:\\moonbit\\clawteam\\internal\\lru\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2934, _M0L6_2atmpS2935, (moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS2936);
}

void* _M0IP48clawteam8clawteam8internal3lru5CachePB6ToJson8to__jsonGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0L4selfS1248
) {
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3158;
  int32_t _M0L6_2acntS3491;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5cacheS2870;
  #line 70 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L8_2afieldS3158 = _M0L4selfS1248->$1;
  _M0L6_2acntS3491 = Moonbit_object_header(_M0L4selfS1248)->rc;
  if (_M0L6_2acntS3491 > 1) {
    int32_t _M0L11_2anew__cntS3492 = _M0L6_2acntS3491 - 1;
    Moonbit_object_header(_M0L4selfS1248)->rc = _M0L11_2anew__cntS3492;
    moonbit_incref(_M0L8_2afieldS3158);
  } else if (_M0L6_2acntS3491 == 1) {
    #line 73 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
    moonbit_free(_M0L4selfS1248);
  }
  _M0L5cacheS2870 = _M0L8_2afieldS3158;
  #line 73 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  return _M0IPB3MapPB6ToJson8to__jsonGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5cacheS2870);
}

int32_t _M0MP48clawteam8clawteam8internal3lru5Cache3setGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0L4selfS1245,
  moonbit_string_t _M0L3keyS1246,
  int32_t _M0L5valueS1247
) {
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3160;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5cacheS2864;
  int32_t _M0L6_2atmpS2862;
  int32_t _M0L9max__sizeS2863;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3159;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5cacheS2865;
  int32_t _M0L9timestampS2867;
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L6_2atmpS2866;
  int32_t _M0L9timestampS2869;
  int32_t _M0L6_2atmpS2868;
  #line 42 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L8_2afieldS3160 = _M0L4selfS1245->$1;
  _M0L5cacheS2864 = _M0L8_2afieldS3160;
  moonbit_incref(_M0L5cacheS2864);
  #line 48 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L6_2atmpS2862
  = _M0MPB3Map6lengthGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5cacheS2864);
  _M0L9max__sizeS2863 = _M0L4selfS1245->$2;
  if (_M0L6_2atmpS2862 >= _M0L9max__sizeS2863) {
    moonbit_incref(_M0L4selfS1245);
    #line 49 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
    _M0MP48clawteam8clawteam8internal3lru5Cache5evictGsiE(_M0L4selfS1245);
  }
  _M0L8_2afieldS3159 = _M0L4selfS1245->$1;
  _M0L5cacheS2865 = _M0L8_2afieldS3159;
  _M0L9timestampS2867 = _M0L4selfS1245->$0;
  _M0L6_2atmpS2866
  = (struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE));
  Moonbit_object_header(_M0L6_2atmpS2866)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE) >> 2, 0, 0);
  _M0L6_2atmpS2866->$0 = _M0L9timestampS2867;
  _M0L6_2atmpS2866->$1 = _M0L5valueS1247;
  moonbit_incref(_M0L5cacheS2865);
  #line 51 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0MPB3Map3setGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5cacheS2865, _M0L3keyS1246, _M0L6_2atmpS2866);
  _M0L9timestampS2869 = _M0L4selfS1245->$0;
  _M0L6_2atmpS2868 = _M0L9timestampS2869 + 1;
  _M0L4selfS1245->$0 = _M0L6_2atmpS2868;
  moonbit_decref(_M0L4selfS1245);
  return 0;
}

int32_t _M0MP48clawteam8clawteam8internal3lru5Cache5evictGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0L4selfS1223
) {
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2861;
  struct _M0TPC13ref3RefGOUsRP48clawteam8clawteam8internal3lru5EntryGiEEE* _M0L6oldestS1221;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3171;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5cacheS2859;
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5_2aitS1222;
  moonbit_string_t _M0L1kS1240;
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3163;
  int32_t _M0L6_2acntS3497;
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS1241;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3161;
  int32_t _M0L6_2acntS3495;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5cacheS2860;
  #line 56 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L6_2atmpS2861 = 0;
  _M0L6oldestS1221
  = (struct _M0TPC13ref3RefGOUsRP48clawteam8clawteam8internal3lru5EntryGiEEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGOUsRP48clawteam8clawteam8internal3lru5EntryGiEEE));
  Moonbit_object_header(_M0L6oldestS1221)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGOUsRP48clawteam8clawteam8internal3lru5EntryGiEEE, $0) >> 2, 1, 0);
  _M0L6oldestS1221->$0 = _M0L6_2atmpS2861;
  _M0L8_2afieldS3171 = _M0L4selfS1223->$1;
  _M0L5cacheS2859 = _M0L8_2afieldS3171;
  moonbit_incref(_M0L5cacheS2859);
  #line 57 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L5_2aitS1222
  = _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5cacheS2859);
  while (1) {
    moonbit_string_t _M0L1kS1225;
    struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L1vS1226;
    struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS1234;
    struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L2ovS1228;
    struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3168;
    struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS1229;
    int32_t _M0L8accessedS2853;
    int32_t _M0L8_2afieldS3165;
    int32_t _M0L8accessedS2854;
    moonbit_incref(_M0L5_2aitS1222);
    #line 58 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
    _M0L7_2abindS1234
    = _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5_2aitS1222);
    if (_M0L7_2abindS1234 == 0) {
      if (_M0L7_2abindS1234) {
        moonbit_decref(_M0L7_2abindS1234);
      }
      moonbit_decref(_M0L5_2aitS1222);
    } else {
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS1235 =
        _M0L7_2abindS1234;
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4_2axS1236 =
        _M0L7_2aSomeS1235;
      moonbit_string_t _M0L8_2afieldS3170 = _M0L4_2axS1236->$0;
      moonbit_string_t _M0L4_2akS1237 = _M0L8_2afieldS3170;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3169 =
        _M0L4_2axS1236->$1;
      int32_t _M0L6_2acntS3493 = Moonbit_object_header(_M0L4_2axS1236)->rc;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L4_2avS1238;
      if (_M0L6_2acntS3493 > 1) {
        int32_t _M0L11_2anew__cntS3494 = _M0L6_2acntS3493 - 1;
        Moonbit_object_header(_M0L4_2axS1236)->rc = _M0L11_2anew__cntS3494;
        moonbit_incref(_M0L8_2afieldS3169);
        moonbit_incref(_M0L4_2akS1237);
      } else if (_M0L6_2acntS3493 == 1) {
        #line 58 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
        moonbit_free(_M0L4_2axS1236);
      }
      _M0L4_2avS1238 = _M0L8_2afieldS3169;
      _M0L1kS1225 = _M0L4_2akS1237;
      _M0L1vS1226 = _M0L4_2avS1238;
      goto join_1224;
    }
    goto joinlet_3640;
    join_1224:;
    _M0L8_2afieldS3168 = _M0L6oldestS1221->$0;
    _M0L7_2abindS1229 = _M0L8_2afieldS3168;
    if (_M0L7_2abindS1229 == 0) {
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2atupleS2858 =
        (struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE));
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2857;
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3166;
      Moonbit_object_header(_M0L8_2atupleS2858)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE, $0) >> 2, 2, 0);
      _M0L8_2atupleS2858->$0 = _M0L1kS1225;
      _M0L8_2atupleS2858->$1 = _M0L1vS1226;
      _M0L6_2atmpS2857 = _M0L8_2atupleS2858;
      _M0L6_2aoldS3166 = _M0L6oldestS1221->$0;
      if (_M0L6_2aoldS3166) {
        moonbit_decref(_M0L6_2aoldS3166);
      }
      _M0L6oldestS1221->$0 = _M0L6_2atmpS2857;
    } else {
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS1230 =
        _M0L7_2abindS1229;
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4_2axS1231 =
        _M0L7_2aSomeS1230;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3167 =
        _M0L4_2axS1231->$1;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L5_2aovS1232 =
        _M0L8_2afieldS3167;
      moonbit_incref(_M0L5_2aovS1232);
      _M0L2ovS1228 = _M0L5_2aovS1232;
      goto join_1227;
    }
    goto joinlet_3641;
    join_1227:;
    _M0L8accessedS2853 = _M0L1vS1226->$0;
    _M0L8_2afieldS3165 = _M0L2ovS1228->$0;
    moonbit_decref(_M0L2ovS1228);
    _M0L8accessedS2854 = _M0L8_2afieldS3165;
    if (_M0L8accessedS2853 < _M0L8accessedS2854) {
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2atupleS2856 =
        (struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE));
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2855;
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3164;
      Moonbit_object_header(_M0L8_2atupleS2856)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE, $0) >> 2, 2, 0);
      _M0L8_2atupleS2856->$0 = _M0L1kS1225;
      _M0L8_2atupleS2856->$1 = _M0L1vS1226;
      _M0L6_2atmpS2855 = _M0L8_2atupleS2856;
      _M0L6_2aoldS3164 = _M0L6oldestS1221->$0;
      if (_M0L6_2aoldS3164) {
        moonbit_decref(_M0L6_2aoldS3164);
      }
      _M0L6oldestS1221->$0 = _M0L6_2atmpS2855;
    } else {
      moonbit_decref(_M0L1vS1226);
      moonbit_decref(_M0L1kS1225);
    }
    joinlet_3641:;
    continue;
    joinlet_3640:;
    break;
  }
  _M0L8_2afieldS3163 = _M0L6oldestS1221->$0;
  _M0L6_2acntS3497 = Moonbit_object_header(_M0L6oldestS1221)->rc;
  if (_M0L6_2acntS3497 > 1) {
    int32_t _M0L11_2anew__cntS3498 = _M0L6_2acntS3497 - 1;
    Moonbit_object_header(_M0L6oldestS1221)->rc = _M0L11_2anew__cntS3498;
    if (_M0L8_2afieldS3163) {
      moonbit_incref(_M0L8_2afieldS3163);
    }
  } else if (_M0L6_2acntS3497 == 1) {
    #line 64 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
    moonbit_free(_M0L6oldestS1221);
  }
  _M0L7_2abindS1241 = _M0L8_2afieldS3163;
  if (_M0L7_2abindS1241 == 0) {
    if (_M0L7_2abindS1241) {
      moonbit_decref(_M0L7_2abindS1241);
    }
    moonbit_decref(_M0L4selfS1223);
  } else {
    struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS1242 =
      _M0L7_2abindS1241;
    struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4_2axS1243 =
      _M0L7_2aSomeS1242;
    moonbit_string_t _M0L8_2afieldS3162 = _M0L4_2axS1243->$0;
    int32_t _M0L6_2acntS3499 = Moonbit_object_header(_M0L4_2axS1243)->rc;
    moonbit_string_t _M0L4_2akS1244;
    if (_M0L6_2acntS3499 > 1) {
      int32_t _M0L11_2anew__cntS3501 = _M0L6_2acntS3499 - 1;
      Moonbit_object_header(_M0L4_2axS1243)->rc = _M0L11_2anew__cntS3501;
      moonbit_incref(_M0L8_2afieldS3162);
    } else if (_M0L6_2acntS3499 == 1) {
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3500 =
        _M0L4_2axS1243->$1;
      moonbit_decref(_M0L8_2afieldS3500);
      #line 64 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
      moonbit_free(_M0L4_2axS1243);
    }
    _M0L4_2akS1244 = _M0L8_2afieldS3162;
    _M0L1kS1240 = _M0L4_2akS1244;
    goto join_1239;
  }
  goto joinlet_3642;
  join_1239:;
  _M0L8_2afieldS3161 = _M0L4selfS1223->$1;
  _M0L6_2acntS3495 = Moonbit_object_header(_M0L4selfS1223)->rc;
  if (_M0L6_2acntS3495 > 1) {
    int32_t _M0L11_2anew__cntS3496 = _M0L6_2acntS3495 - 1;
    Moonbit_object_header(_M0L4selfS1223)->rc = _M0L11_2anew__cntS3496;
    moonbit_incref(_M0L8_2afieldS3161);
  } else if (_M0L6_2acntS3495 == 1) {
    #line 65 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
    moonbit_free(_M0L4selfS1223);
  }
  _M0L5cacheS2860 = _M0L8_2afieldS3161;
  #line 65 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0MPB3Map6removeGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5cacheS2860, _M0L1kS1240);
  joinlet_3642:;
  return 0;
}

int32_t _M0MP48clawteam8clawteam8internal3lru5Cache7op__getGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0L4selfS1219,
  moonbit_string_t _M0L3keyS1220
) {
  int64_t _M0L6_2atmpS2852;
  #line 37 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  #line 38 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L6_2atmpS2852
  = _M0MP48clawteam8clawteam8internal3lru5Cache3getGsiE(_M0L4selfS1219, _M0L3keyS1220);
  #line 38 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  return _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS2852);
}

int64_t _M0MP48clawteam8clawteam8internal3lru5Cache3getGsiE(
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0L4selfS1214,
  moonbit_string_t _M0L3keyS1216
) {
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L5entryS1213;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3173;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5cacheS2851;
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L7_2abindS1215;
  int32_t _M0L9timestampS2847;
  int32_t _M0L9timestampS2849;
  int32_t _M0L6_2atmpS2848;
  int32_t _M0L8_2afieldS3172;
  int32_t _M0L5valueS2850;
  #line 25 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L8_2afieldS3173 = _M0L4selfS1214->$1;
  _M0L5cacheS2851 = _M0L8_2afieldS3173;
  moonbit_incref(_M0L5cacheS2851);
  #line 26 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L7_2abindS1215
  = _M0MPB3Map3getGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5cacheS2851, _M0L3keyS1216);
  if (_M0L7_2abindS1215 == 0) {
    if (_M0L7_2abindS1215) {
      moonbit_decref(_M0L7_2abindS1215);
    }
    moonbit_decref(_M0L4selfS1214);
    return 4294967296ll;
  } else {
    struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L7_2aSomeS1217 =
      _M0L7_2abindS1215;
    struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2aentryS1218 =
      _M0L7_2aSomeS1217;
    _M0L5entryS1213 = _M0L8_2aentryS1218;
    goto join_1212;
  }
  join_1212:;
  _M0L9timestampS2847 = _M0L4selfS1214->$0;
  _M0L5entryS1213->$0 = _M0L9timestampS2847;
  _M0L9timestampS2849 = _M0L4selfS1214->$0;
  _M0L6_2atmpS2848 = _M0L9timestampS2849 + 1;
  _M0L4selfS1214->$0 = _M0L6_2atmpS2848;
  moonbit_decref(_M0L4selfS1214);
  _M0L8_2afieldS3172 = _M0L5entryS1213->$1;
  moonbit_decref(_M0L5entryS1213);
  _M0L5valueS2850 = _M0L8_2afieldS3172;
  return (int64_t)_M0L5valueS2850;
}

struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0FP48clawteam8clawteam8internal3lru13cache_2einnerGsiE(
  int32_t _M0L9max__sizeS1211
) {
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7_2abindS1210;
  struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L6_2atmpS2846;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE _M0L6_2atmpS2845;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2844;
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _block_3644;
  #line 20 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L7_2abindS1210
  = (struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2846 = _M0L7_2abindS1210;
  _M0L6_2atmpS2845
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE){
    0, 0, _M0L6_2atmpS2846
  };
  #line 21 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L6_2atmpS2844
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L6_2atmpS2845);
  _block_3644
  = (struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE));
  Moonbit_object_header(_block_3644)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE, $1) >> 2, 1, 0);
  _block_3644->$0 = 0;
  _block_3644->$1 = _M0L6_2atmpS2844;
  _block_3644->$2 = _M0L9max__sizeS1211;
  return _block_3644;
}

void* _M0IP48clawteam8clawteam8internal3lru5EntryPB6ToJson8to__jsonGiE(
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L4selfS1209
) {
  int32_t _M0L8_2afieldS3174;
  int32_t _M0L5valueS2843;
  #line 8 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  _M0L8_2afieldS3174 = _M0L4selfS1209->$1;
  moonbit_decref(_M0L4selfS1209);
  _M0L5valueS2843 = _M0L8_2afieldS3174;
  #line 9 "E:\\moonbit\\clawteam\\internal\\lru\\cache.mbt"
  return _M0IPC13int3IntPB6ToJson8to__json(_M0L5valueS2843);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1204,
  void* _M0L7contentS1206,
  moonbit_string_t _M0L3locS1200,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1202
) {
  moonbit_string_t _M0L3locS1199;
  moonbit_string_t _M0L9args__locS1201;
  void* _M0L6_2atmpS2841;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2842;
  moonbit_string_t _M0L6actualS1203;
  moonbit_string_t _M0L4wantS1205;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1199 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1200);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1201 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1202);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2841 = _M0L3objS1204.$0->$method_0(_M0L3objS1204.$1);
  _M0L6_2atmpS2842 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1203
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2841, 0, 0, _M0L6_2atmpS2842);
  if (_M0L7contentS1206 == 0) {
    void* _M0L6_2atmpS2838;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2839;
    if (_M0L7contentS1206) {
      moonbit_decref(_M0L7contentS1206);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2838
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2839 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1205
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2838, 0, 0, _M0L6_2atmpS2839);
  } else {
    void* _M0L7_2aSomeS1207 = _M0L7contentS1206;
    void* _M0L4_2axS1208 = _M0L7_2aSomeS1207;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2840 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1205
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1208, 0, 0, _M0L6_2atmpS2840);
  }
  moonbit_incref(_M0L4wantS1205);
  moonbit_incref(_M0L6actualS1203);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1203, _M0L4wantS1205)
  ) {
    moonbit_string_t _M0L6_2atmpS2836;
    moonbit_string_t _M0L6_2atmpS3182;
    moonbit_string_t _M0L6_2atmpS2835;
    moonbit_string_t _M0L6_2atmpS3181;
    moonbit_string_t _M0L6_2atmpS2833;
    moonbit_string_t _M0L6_2atmpS2834;
    moonbit_string_t _M0L6_2atmpS3180;
    moonbit_string_t _M0L6_2atmpS2832;
    moonbit_string_t _M0L6_2atmpS3179;
    moonbit_string_t _M0L6_2atmpS2829;
    moonbit_string_t _M0L6_2atmpS2831;
    moonbit_string_t _M0L6_2atmpS2830;
    moonbit_string_t _M0L6_2atmpS3178;
    moonbit_string_t _M0L6_2atmpS2828;
    moonbit_string_t _M0L6_2atmpS3177;
    moonbit_string_t _M0L6_2atmpS2825;
    moonbit_string_t _M0L6_2atmpS2827;
    moonbit_string_t _M0L6_2atmpS2826;
    moonbit_string_t _M0L6_2atmpS3176;
    moonbit_string_t _M0L6_2atmpS2824;
    moonbit_string_t _M0L6_2atmpS3175;
    moonbit_string_t _M0L6_2atmpS2823;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2822;
    struct moonbit_result_0 _result_3645;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2836
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1199);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3182
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS2836);
    moonbit_decref(_M0L6_2atmpS2836);
    _M0L6_2atmpS2835 = _M0L6_2atmpS3182;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3181
    = moonbit_add_string(_M0L6_2atmpS2835, (moonbit_string_t)moonbit_string_literal_28.data);
    moonbit_decref(_M0L6_2atmpS2835);
    _M0L6_2atmpS2833 = _M0L6_2atmpS3181;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2834
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1201);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3180 = moonbit_add_string(_M0L6_2atmpS2833, _M0L6_2atmpS2834);
    moonbit_decref(_M0L6_2atmpS2833);
    moonbit_decref(_M0L6_2atmpS2834);
    _M0L6_2atmpS2832 = _M0L6_2atmpS3180;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3179
    = moonbit_add_string(_M0L6_2atmpS2832, (moonbit_string_t)moonbit_string_literal_29.data);
    moonbit_decref(_M0L6_2atmpS2832);
    _M0L6_2atmpS2829 = _M0L6_2atmpS3179;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2831 = _M0MPC16string6String6escape(_M0L4wantS1205);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2830
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2831);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3178 = moonbit_add_string(_M0L6_2atmpS2829, _M0L6_2atmpS2830);
    moonbit_decref(_M0L6_2atmpS2829);
    moonbit_decref(_M0L6_2atmpS2830);
    _M0L6_2atmpS2828 = _M0L6_2atmpS3178;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3177
    = moonbit_add_string(_M0L6_2atmpS2828, (moonbit_string_t)moonbit_string_literal_30.data);
    moonbit_decref(_M0L6_2atmpS2828);
    _M0L6_2atmpS2825 = _M0L6_2atmpS3177;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2827 = _M0MPC16string6String6escape(_M0L6actualS1203);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2826
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2827);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3176 = moonbit_add_string(_M0L6_2atmpS2825, _M0L6_2atmpS2826);
    moonbit_decref(_M0L6_2atmpS2825);
    moonbit_decref(_M0L6_2atmpS2826);
    _M0L6_2atmpS2824 = _M0L6_2atmpS3176;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3175
    = moonbit_add_string(_M0L6_2atmpS2824, (moonbit_string_t)moonbit_string_literal_31.data);
    moonbit_decref(_M0L6_2atmpS2824);
    _M0L6_2atmpS2823 = _M0L6_2atmpS3175;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2822
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2822)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2822)->$0
    = _M0L6_2atmpS2823;
    _result_3645.tag = 0;
    _result_3645.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2822;
    return _result_3645;
  } else {
    int32_t _M0L6_2atmpS2837;
    struct moonbit_result_0 _result_3646;
    moonbit_decref(_M0L4wantS1205);
    moonbit_decref(_M0L6actualS1203);
    moonbit_decref(_M0L9args__locS1201);
    moonbit_decref(_M0L3locS1199);
    _M0L6_2atmpS2837 = 0;
    _result_3646.tag = 1;
    _result_3646.data.ok = _M0L6_2atmpS2837;
    return _result_3646;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1198,
  int32_t _M0L13escape__slashS1170,
  int32_t _M0L6indentS1165,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1191
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1157;
  void** _M0L6_2atmpS2821;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1158;
  int32_t _M0Lm5depthS1159;
  void* _M0L6_2atmpS2820;
  void* _M0L8_2aparamS1160;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1157 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2821 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1158
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1158)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1158->$0 = _M0L6_2atmpS2821;
  _M0L5stackS1158->$1 = 0;
  _M0Lm5depthS1159 = 0;
  _M0L6_2atmpS2820 = _M0L4selfS1198;
  _M0L8_2aparamS1160 = _M0L6_2atmpS2820;
  _2aloop_1176:;
  while (1) {
    if (_M0L8_2aparamS1160 == 0) {
      int32_t _M0L3lenS2782;
      if (_M0L8_2aparamS1160) {
        moonbit_decref(_M0L8_2aparamS1160);
      }
      _M0L3lenS2782 = _M0L5stackS1158->$1;
      if (_M0L3lenS2782 == 0) {
        if (_M0L8replacerS1191) {
          moonbit_decref(_M0L8replacerS1191);
        }
        moonbit_decref(_M0L5stackS1158);
        break;
      } else {
        void** _M0L8_2afieldS3190 = _M0L5stackS1158->$0;
        void** _M0L3bufS2806 = _M0L8_2afieldS3190;
        int32_t _M0L3lenS2808 = _M0L5stackS1158->$1;
        int32_t _M0L6_2atmpS2807 = _M0L3lenS2808 - 1;
        void* _M0L6_2atmpS3189 = (void*)_M0L3bufS2806[_M0L6_2atmpS2807];
        void* _M0L4_2axS1177 = _M0L6_2atmpS3189;
        switch (Moonbit_object_tag(_M0L4_2axS1177)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1178 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1177;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3185 =
              _M0L8_2aArrayS1178->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1179 =
              _M0L8_2afieldS3185;
            int32_t _M0L4_2aiS1180 = _M0L8_2aArrayS1178->$1;
            int32_t _M0L3lenS2794 = _M0L6_2aarrS1179->$1;
            if (_M0L4_2aiS1180 < _M0L3lenS2794) {
              int32_t _if__result_3648;
              void** _M0L8_2afieldS3184;
              void** _M0L3bufS2800;
              void* _M0L6_2atmpS3183;
              void* _M0L7elementS1181;
              int32_t _M0L6_2atmpS2795;
              void* _M0L6_2atmpS2798;
              if (_M0L4_2aiS1180 < 0) {
                _if__result_3648 = 1;
              } else {
                int32_t _M0L3lenS2799 = _M0L6_2aarrS1179->$1;
                _if__result_3648 = _M0L4_2aiS1180 >= _M0L3lenS2799;
              }
              if (_if__result_3648) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3184 = _M0L6_2aarrS1179->$0;
              _M0L3bufS2800 = _M0L8_2afieldS3184;
              _M0L6_2atmpS3183 = (void*)_M0L3bufS2800[_M0L4_2aiS1180];
              _M0L7elementS1181 = _M0L6_2atmpS3183;
              _M0L6_2atmpS2795 = _M0L4_2aiS1180 + 1;
              _M0L8_2aArrayS1178->$1 = _M0L6_2atmpS2795;
              if (_M0L4_2aiS1180 > 0) {
                int32_t _M0L6_2atmpS2797;
                moonbit_string_t _M0L6_2atmpS2796;
                moonbit_incref(_M0L7elementS1181);
                moonbit_incref(_M0L3bufS1157);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 44);
                _M0L6_2atmpS2797 = _M0Lm5depthS1159;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2796
                = _M0FPC14json11indent__str(_M0L6_2atmpS2797, _M0L6indentS1165);
                moonbit_incref(_M0L3bufS1157);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2796);
              } else {
                moonbit_incref(_M0L7elementS1181);
              }
              _M0L6_2atmpS2798 = _M0L7elementS1181;
              _M0L8_2aparamS1160 = _M0L6_2atmpS2798;
              goto _2aloop_1176;
            } else {
              int32_t _M0L6_2atmpS2801 = _M0Lm5depthS1159;
              void* _M0L6_2atmpS2802;
              int32_t _M0L6_2atmpS2804;
              moonbit_string_t _M0L6_2atmpS2803;
              void* _M0L6_2atmpS2805;
              _M0Lm5depthS1159 = _M0L6_2atmpS2801 - 1;
              moonbit_incref(_M0L5stackS1158);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2802
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1158);
              if (_M0L6_2atmpS2802) {
                moonbit_decref(_M0L6_2atmpS2802);
              }
              _M0L6_2atmpS2804 = _M0Lm5depthS1159;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2803
              = _M0FPC14json11indent__str(_M0L6_2atmpS2804, _M0L6indentS1165);
              moonbit_incref(_M0L3bufS1157);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2803);
              moonbit_incref(_M0L3bufS1157);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 93);
              _M0L6_2atmpS2805 = 0;
              _M0L8_2aparamS1160 = _M0L6_2atmpS2805;
              goto _2aloop_1176;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1182 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1177;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3188 =
              _M0L9_2aObjectS1182->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1183 =
              _M0L8_2afieldS3188;
            int32_t _M0L8_2afirstS1184 = _M0L9_2aObjectS1182->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1185;
            moonbit_incref(_M0L11_2aiteratorS1183);
            moonbit_incref(_M0L9_2aObjectS1182);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1185
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1183);
            if (_M0L7_2abindS1185 == 0) {
              int32_t _M0L6_2atmpS2783;
              void* _M0L6_2atmpS2784;
              int32_t _M0L6_2atmpS2786;
              moonbit_string_t _M0L6_2atmpS2785;
              void* _M0L6_2atmpS2787;
              if (_M0L7_2abindS1185) {
                moonbit_decref(_M0L7_2abindS1185);
              }
              moonbit_decref(_M0L9_2aObjectS1182);
              _M0L6_2atmpS2783 = _M0Lm5depthS1159;
              _M0Lm5depthS1159 = _M0L6_2atmpS2783 - 1;
              moonbit_incref(_M0L5stackS1158);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2784
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1158);
              if (_M0L6_2atmpS2784) {
                moonbit_decref(_M0L6_2atmpS2784);
              }
              _M0L6_2atmpS2786 = _M0Lm5depthS1159;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2785
              = _M0FPC14json11indent__str(_M0L6_2atmpS2786, _M0L6indentS1165);
              moonbit_incref(_M0L3bufS1157);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2785);
              moonbit_incref(_M0L3bufS1157);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 125);
              _M0L6_2atmpS2787 = 0;
              _M0L8_2aparamS1160 = _M0L6_2atmpS2787;
              goto _2aloop_1176;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1186 = _M0L7_2abindS1185;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1187 = _M0L7_2aSomeS1186;
              moonbit_string_t _M0L8_2afieldS3187 = _M0L4_2axS1187->$0;
              moonbit_string_t _M0L4_2akS1188 = _M0L8_2afieldS3187;
              void* _M0L8_2afieldS3186 = _M0L4_2axS1187->$1;
              int32_t _M0L6_2acntS3502 =
                Moonbit_object_header(_M0L4_2axS1187)->rc;
              void* _M0L4_2avS1189;
              void* _M0Lm2v2S1190;
              moonbit_string_t _M0L6_2atmpS2791;
              void* _M0L6_2atmpS2793;
              void* _M0L6_2atmpS2792;
              if (_M0L6_2acntS3502 > 1) {
                int32_t _M0L11_2anew__cntS3503 = _M0L6_2acntS3502 - 1;
                Moonbit_object_header(_M0L4_2axS1187)->rc
                = _M0L11_2anew__cntS3503;
                moonbit_incref(_M0L8_2afieldS3186);
                moonbit_incref(_M0L4_2akS1188);
              } else if (_M0L6_2acntS3502 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1187);
              }
              _M0L4_2avS1189 = _M0L8_2afieldS3186;
              _M0Lm2v2S1190 = _M0L4_2avS1189;
              if (_M0L8replacerS1191 == 0) {
                moonbit_incref(_M0Lm2v2S1190);
                moonbit_decref(_M0L4_2avS1189);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1192 =
                  _M0L8replacerS1191;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1193 =
                  _M0L7_2aSomeS1192;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1194 =
                  _M0L11_2areplacerS1193;
                void* _M0L7_2abindS1195;
                moonbit_incref(_M0L7_2afuncS1194);
                moonbit_incref(_M0L4_2akS1188);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1195
                = _M0L7_2afuncS1194->code(_M0L7_2afuncS1194, _M0L4_2akS1188, _M0L4_2avS1189);
                if (_M0L7_2abindS1195 == 0) {
                  void* _M0L6_2atmpS2788;
                  if (_M0L7_2abindS1195) {
                    moonbit_decref(_M0L7_2abindS1195);
                  }
                  moonbit_decref(_M0L4_2akS1188);
                  moonbit_decref(_M0L9_2aObjectS1182);
                  _M0L6_2atmpS2788 = 0;
                  _M0L8_2aparamS1160 = _M0L6_2atmpS2788;
                  goto _2aloop_1176;
                } else {
                  void* _M0L7_2aSomeS1196 = _M0L7_2abindS1195;
                  void* _M0L4_2avS1197 = _M0L7_2aSomeS1196;
                  _M0Lm2v2S1190 = _M0L4_2avS1197;
                }
              }
              if (!_M0L8_2afirstS1184) {
                int32_t _M0L6_2atmpS2790;
                moonbit_string_t _M0L6_2atmpS2789;
                moonbit_incref(_M0L3bufS1157);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 44);
                _M0L6_2atmpS2790 = _M0Lm5depthS1159;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2789
                = _M0FPC14json11indent__str(_M0L6_2atmpS2790, _M0L6indentS1165);
                moonbit_incref(_M0L3bufS1157);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2789);
              }
              moonbit_incref(_M0L3bufS1157);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2791
              = _M0FPC14json6escape(_M0L4_2akS1188, _M0L13escape__slashS1170);
              moonbit_incref(_M0L3bufS1157);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2791);
              moonbit_incref(_M0L3bufS1157);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 34);
              moonbit_incref(_M0L3bufS1157);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 58);
              if (_M0L6indentS1165 > 0) {
                moonbit_incref(_M0L3bufS1157);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 32);
              }
              _M0L9_2aObjectS1182->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1182);
              _M0L6_2atmpS2793 = _M0Lm2v2S1190;
              _M0L6_2atmpS2792 = _M0L6_2atmpS2793;
              _M0L8_2aparamS1160 = _M0L6_2atmpS2792;
              goto _2aloop_1176;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1161 = _M0L8_2aparamS1160;
      void* _M0L8_2avalueS1162 = _M0L7_2aSomeS1161;
      void* _M0L6_2atmpS2819;
      switch (Moonbit_object_tag(_M0L8_2avalueS1162)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1163 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1162;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3191 =
            _M0L9_2aObjectS1163->$0;
          int32_t _M0L6_2acntS3504 =
            Moonbit_object_header(_M0L9_2aObjectS1163)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1164;
          if (_M0L6_2acntS3504 > 1) {
            int32_t _M0L11_2anew__cntS3505 = _M0L6_2acntS3504 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1163)->rc
            = _M0L11_2anew__cntS3505;
            moonbit_incref(_M0L8_2afieldS3191);
          } else if (_M0L6_2acntS3504 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1163);
          }
          _M0L10_2amembersS1164 = _M0L8_2afieldS3191;
          moonbit_incref(_M0L10_2amembersS1164);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1164)) {
            moonbit_decref(_M0L10_2amembersS1164);
            moonbit_incref(_M0L3bufS1157);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, (moonbit_string_t)moonbit_string_literal_32.data);
          } else {
            int32_t _M0L6_2atmpS2814 = _M0Lm5depthS1159;
            int32_t _M0L6_2atmpS2816;
            moonbit_string_t _M0L6_2atmpS2815;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2818;
            void* _M0L6ObjectS2817;
            _M0Lm5depthS1159 = _M0L6_2atmpS2814 + 1;
            moonbit_incref(_M0L3bufS1157);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 123);
            _M0L6_2atmpS2816 = _M0Lm5depthS1159;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2815
            = _M0FPC14json11indent__str(_M0L6_2atmpS2816, _M0L6indentS1165);
            moonbit_incref(_M0L3bufS1157);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2815);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2818
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1164);
            _M0L6ObjectS2817
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2817)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2817)->$0
            = _M0L6_2atmpS2818;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2817)->$1
            = 1;
            moonbit_incref(_M0L5stackS1158);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1158, _M0L6ObjectS2817);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1166 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1162;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3192 =
            _M0L8_2aArrayS1166->$0;
          int32_t _M0L6_2acntS3506 =
            Moonbit_object_header(_M0L8_2aArrayS1166)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1167;
          if (_M0L6_2acntS3506 > 1) {
            int32_t _M0L11_2anew__cntS3507 = _M0L6_2acntS3506 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1166)->rc
            = _M0L11_2anew__cntS3507;
            moonbit_incref(_M0L8_2afieldS3192);
          } else if (_M0L6_2acntS3506 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1166);
          }
          _M0L6_2aarrS1167 = _M0L8_2afieldS3192;
          moonbit_incref(_M0L6_2aarrS1167);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1167)) {
            moonbit_decref(_M0L6_2aarrS1167);
            moonbit_incref(_M0L3bufS1157);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, (moonbit_string_t)moonbit_string_literal_33.data);
          } else {
            int32_t _M0L6_2atmpS2810 = _M0Lm5depthS1159;
            int32_t _M0L6_2atmpS2812;
            moonbit_string_t _M0L6_2atmpS2811;
            void* _M0L5ArrayS2813;
            _M0Lm5depthS1159 = _M0L6_2atmpS2810 + 1;
            moonbit_incref(_M0L3bufS1157);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 91);
            _M0L6_2atmpS2812 = _M0Lm5depthS1159;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2811
            = _M0FPC14json11indent__str(_M0L6_2atmpS2812, _M0L6indentS1165);
            moonbit_incref(_M0L3bufS1157);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2811);
            _M0L5ArrayS2813
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2813)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2813)->$0
            = _M0L6_2aarrS1167;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2813)->$1
            = 0;
            moonbit_incref(_M0L5stackS1158);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1158, _M0L5ArrayS2813);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1168 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1162;
          moonbit_string_t _M0L8_2afieldS3193 = _M0L9_2aStringS1168->$0;
          int32_t _M0L6_2acntS3508 =
            Moonbit_object_header(_M0L9_2aStringS1168)->rc;
          moonbit_string_t _M0L4_2asS1169;
          moonbit_string_t _M0L6_2atmpS2809;
          if (_M0L6_2acntS3508 > 1) {
            int32_t _M0L11_2anew__cntS3509 = _M0L6_2acntS3508 - 1;
            Moonbit_object_header(_M0L9_2aStringS1168)->rc
            = _M0L11_2anew__cntS3509;
            moonbit_incref(_M0L8_2afieldS3193);
          } else if (_M0L6_2acntS3508 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1168);
          }
          _M0L4_2asS1169 = _M0L8_2afieldS3193;
          moonbit_incref(_M0L3bufS1157);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2809
          = _M0FPC14json6escape(_M0L4_2asS1169, _M0L13escape__slashS1170);
          moonbit_incref(_M0L3bufS1157);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L6_2atmpS2809);
          moonbit_incref(_M0L3bufS1157);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1157, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1171 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1162;
          double _M0L4_2anS1172 = _M0L9_2aNumberS1171->$0;
          moonbit_string_t _M0L8_2afieldS3194 = _M0L9_2aNumberS1171->$1;
          int32_t _M0L6_2acntS3510 =
            Moonbit_object_header(_M0L9_2aNumberS1171)->rc;
          moonbit_string_t _M0L7_2areprS1173;
          if (_M0L6_2acntS3510 > 1) {
            int32_t _M0L11_2anew__cntS3511 = _M0L6_2acntS3510 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1171)->rc
            = _M0L11_2anew__cntS3511;
            if (_M0L8_2afieldS3194) {
              moonbit_incref(_M0L8_2afieldS3194);
            }
          } else if (_M0L6_2acntS3510 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1171);
          }
          _M0L7_2areprS1173 = _M0L8_2afieldS3194;
          if (_M0L7_2areprS1173 == 0) {
            if (_M0L7_2areprS1173) {
              moonbit_decref(_M0L7_2areprS1173);
            }
            moonbit_incref(_M0L3bufS1157);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1157, _M0L4_2anS1172);
          } else {
            moonbit_string_t _M0L7_2aSomeS1174 = _M0L7_2areprS1173;
            moonbit_string_t _M0L4_2arS1175 = _M0L7_2aSomeS1174;
            moonbit_incref(_M0L3bufS1157);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, _M0L4_2arS1175);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1157);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, (moonbit_string_t)moonbit_string_literal_34.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1157);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, (moonbit_string_t)moonbit_string_literal_35.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1162);
          moonbit_incref(_M0L3bufS1157);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1157, (moonbit_string_t)moonbit_string_literal_36.data);
          break;
        }
      }
      _M0L6_2atmpS2819 = 0;
      _M0L8_2aparamS1160 = _M0L6_2atmpS2819;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1157);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1156,
  int32_t _M0L6indentS1154
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1154 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1155 = _M0L6indentS1154 * _M0L5levelS1156;
    switch (_M0L6spacesS1155) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_37.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_38.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_39.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_40.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_41.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_42.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_43.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_44.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_45.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2781;
        moonbit_string_t _M0L6_2atmpS3195;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2781
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_46.data, _M0L6spacesS1155);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3195
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS2781);
        moonbit_decref(_M0L6_2atmpS2781);
        return _M0L6_2atmpS3195;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1146,
  int32_t _M0L13escape__slashS1151
) {
  int32_t _M0L6_2atmpS2780;
  struct _M0TPB13StringBuilder* _M0L3bufS1145;
  struct _M0TWEOc* _M0L5_2aitS1147;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2780 = Moonbit_array_length(_M0L3strS1146);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1145 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2780);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1147 = _M0MPC16string6String4iter(_M0L3strS1146);
  while (1) {
    int32_t _M0L7_2abindS1148;
    moonbit_incref(_M0L5_2aitS1147);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1148 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1147);
    if (_M0L7_2abindS1148 == -1) {
      moonbit_decref(_M0L5_2aitS1147);
    } else {
      int32_t _M0L7_2aSomeS1149 = _M0L7_2abindS1148;
      int32_t _M0L4_2acS1150 = _M0L7_2aSomeS1149;
      if (_M0L4_2acS1150 == 34) {
        moonbit_incref(_M0L3bufS1145);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_47.data);
      } else if (_M0L4_2acS1150 == 92) {
        moonbit_incref(_M0L3bufS1145);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_48.data);
      } else if (_M0L4_2acS1150 == 47) {
        if (_M0L13escape__slashS1151) {
          moonbit_incref(_M0L3bufS1145);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_49.data);
        } else {
          moonbit_incref(_M0L3bufS1145);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1145, _M0L4_2acS1150);
        }
      } else if (_M0L4_2acS1150 == 10) {
        moonbit_incref(_M0L3bufS1145);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_50.data);
      } else if (_M0L4_2acS1150 == 13) {
        moonbit_incref(_M0L3bufS1145);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_51.data);
      } else if (_M0L4_2acS1150 == 8) {
        moonbit_incref(_M0L3bufS1145);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_52.data);
      } else if (_M0L4_2acS1150 == 9) {
        moonbit_incref(_M0L3bufS1145);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_53.data);
      } else {
        int32_t _M0L4codeS1152 = _M0L4_2acS1150;
        if (_M0L4codeS1152 == 12) {
          moonbit_incref(_M0L3bufS1145);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_54.data);
        } else if (_M0L4codeS1152 < 32) {
          int32_t _M0L6_2atmpS2779;
          moonbit_string_t _M0L6_2atmpS2778;
          moonbit_incref(_M0L3bufS1145);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, (moonbit_string_t)moonbit_string_literal_55.data);
          _M0L6_2atmpS2779 = _M0L4codeS1152 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2778 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2779);
          moonbit_incref(_M0L3bufS1145);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1145, _M0L6_2atmpS2778);
        } else {
          moonbit_incref(_M0L3bufS1145);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1145, _M0L4_2acS1150);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1145);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1144
) {
  int32_t _M0L8_2afieldS3196;
  int32_t _M0L3lenS2777;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3196 = _M0L4selfS1144->$1;
  moonbit_decref(_M0L4selfS1144);
  _M0L3lenS2777 = _M0L8_2afieldS3196;
  return _M0L3lenS2777 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1141
) {
  int32_t _M0L3lenS1140;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1140 = _M0L4selfS1141->$1;
  if (_M0L3lenS1140 == 0) {
    moonbit_decref(_M0L4selfS1141);
    return 0;
  } else {
    int32_t _M0L5indexS1142 = _M0L3lenS1140 - 1;
    void** _M0L8_2afieldS3200 = _M0L4selfS1141->$0;
    void** _M0L3bufS2776 = _M0L8_2afieldS3200;
    void* _M0L6_2atmpS3199 = (void*)_M0L3bufS2776[_M0L5indexS1142];
    void* _M0L1vS1143 = _M0L6_2atmpS3199;
    void** _M0L8_2afieldS3198 = _M0L4selfS1141->$0;
    void** _M0L3bufS2775 = _M0L8_2afieldS3198;
    void* _M0L6_2aoldS3197;
    if (
      _M0L5indexS1142 < 0
      || _M0L5indexS1142 >= Moonbit_array_length(_M0L3bufS2775)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3197 = (void*)_M0L3bufS2775[_M0L5indexS1142];
    moonbit_incref(_M0L1vS1143);
    moonbit_decref(_M0L6_2aoldS3197);
    if (
      _M0L5indexS1142 < 0
      || _M0L5indexS1142 >= Moonbit_array_length(_M0L3bufS2775)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2775[_M0L5indexS1142]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1141->$1 = _M0L5indexS1142;
    moonbit_decref(_M0L4selfS1141);
    return _M0L1vS1143;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1138,
  struct _M0TPB6Logger _M0L6loggerS1139
) {
  moonbit_string_t _M0L6_2atmpS2774;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2773;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2774 = _M0L4selfS1138;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2773 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2774);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2773, _M0L6loggerS1139);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1115,
  struct _M0TPB6Logger _M0L6loggerS1137
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3209;
  struct _M0TPC16string10StringView _M0L3pkgS1114;
  moonbit_string_t _M0L7_2adataS1116;
  int32_t _M0L8_2astartS1117;
  int32_t _M0L6_2atmpS2772;
  int32_t _M0L6_2aendS1118;
  int32_t _M0Lm9_2acursorS1119;
  int32_t _M0Lm13accept__stateS1120;
  int32_t _M0Lm10match__endS1121;
  int32_t _M0Lm20match__tag__saver__0S1122;
  int32_t _M0Lm6tag__0S1123;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1124;
  struct _M0TPC16string10StringView _M0L8_2afieldS3208;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1133;
  void* _M0L8_2afieldS3207;
  int32_t _M0L6_2acntS3512;
  void* _M0L16_2apackage__nameS1134;
  struct _M0TPC16string10StringView _M0L8_2afieldS3205;
  struct _M0TPC16string10StringView _M0L8filenameS2749;
  struct _M0TPC16string10StringView _M0L8_2afieldS3204;
  struct _M0TPC16string10StringView _M0L11start__lineS2750;
  struct _M0TPC16string10StringView _M0L8_2afieldS3203;
  struct _M0TPC16string10StringView _M0L13start__columnS2751;
  struct _M0TPC16string10StringView _M0L8_2afieldS3202;
  struct _M0TPC16string10StringView _M0L9end__lineS2752;
  struct _M0TPC16string10StringView _M0L8_2afieldS3201;
  int32_t _M0L6_2acntS3516;
  struct _M0TPC16string10StringView _M0L11end__columnS2753;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3209
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1115->$0_1, _M0L4selfS1115->$0_2, _M0L4selfS1115->$0_0
  };
  _M0L3pkgS1114 = _M0L8_2afieldS3209;
  moonbit_incref(_M0L3pkgS1114.$0);
  moonbit_incref(_M0L3pkgS1114.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1116 = _M0MPC16string10StringView4data(_M0L3pkgS1114);
  moonbit_incref(_M0L3pkgS1114.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1117
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1114);
  moonbit_incref(_M0L3pkgS1114.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2772 = _M0MPC16string10StringView6length(_M0L3pkgS1114);
  _M0L6_2aendS1118 = _M0L8_2astartS1117 + _M0L6_2atmpS2772;
  _M0Lm9_2acursorS1119 = _M0L8_2astartS1117;
  _M0Lm13accept__stateS1120 = -1;
  _M0Lm10match__endS1121 = -1;
  _M0Lm20match__tag__saver__0S1122 = -1;
  _M0Lm6tag__0S1123 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2764 = _M0Lm9_2acursorS1119;
    if (_M0L6_2atmpS2764 < _M0L6_2aendS1118) {
      int32_t _M0L6_2atmpS2771 = _M0Lm9_2acursorS1119;
      int32_t _M0L10next__charS1128;
      int32_t _M0L6_2atmpS2765;
      moonbit_incref(_M0L7_2adataS1116);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1128
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1116, _M0L6_2atmpS2771);
      _M0L6_2atmpS2765 = _M0Lm9_2acursorS1119;
      _M0Lm9_2acursorS1119 = _M0L6_2atmpS2765 + 1;
      if (_M0L10next__charS1128 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2766;
          _M0Lm6tag__0S1123 = _M0Lm9_2acursorS1119;
          _M0L6_2atmpS2766 = _M0Lm9_2acursorS1119;
          if (_M0L6_2atmpS2766 < _M0L6_2aendS1118) {
            int32_t _M0L6_2atmpS2770 = _M0Lm9_2acursorS1119;
            int32_t _M0L10next__charS1129;
            int32_t _M0L6_2atmpS2767;
            moonbit_incref(_M0L7_2adataS1116);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1129
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1116, _M0L6_2atmpS2770);
            _M0L6_2atmpS2767 = _M0Lm9_2acursorS1119;
            _M0Lm9_2acursorS1119 = _M0L6_2atmpS2767 + 1;
            if (_M0L10next__charS1129 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2768 = _M0Lm9_2acursorS1119;
                if (_M0L6_2atmpS2768 < _M0L6_2aendS1118) {
                  int32_t _M0L6_2atmpS2769 = _M0Lm9_2acursorS1119;
                  _M0Lm9_2acursorS1119 = _M0L6_2atmpS2769 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1122 = _M0Lm6tag__0S1123;
                  _M0Lm13accept__stateS1120 = 0;
                  _M0Lm10match__endS1121 = _M0Lm9_2acursorS1119;
                  goto join_1125;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1125;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1125;
    }
    break;
  }
  goto joinlet_3650;
  join_1125:;
  switch (_M0Lm13accept__stateS1120) {
    case 0: {
      int32_t _M0L6_2atmpS2762;
      int32_t _M0L6_2atmpS2761;
      int64_t _M0L6_2atmpS2758;
      int32_t _M0L6_2atmpS2760;
      int64_t _M0L6_2atmpS2759;
      struct _M0TPC16string10StringView _M0L13package__nameS1126;
      int64_t _M0L6_2atmpS2755;
      int32_t _M0L6_2atmpS2757;
      int64_t _M0L6_2atmpS2756;
      struct _M0TPC16string10StringView _M0L12module__nameS1127;
      void* _M0L4SomeS2754;
      moonbit_decref(_M0L3pkgS1114.$0);
      _M0L6_2atmpS2762 = _M0Lm20match__tag__saver__0S1122;
      _M0L6_2atmpS2761 = _M0L6_2atmpS2762 + 1;
      _M0L6_2atmpS2758 = (int64_t)_M0L6_2atmpS2761;
      _M0L6_2atmpS2760 = _M0Lm10match__endS1121;
      _M0L6_2atmpS2759 = (int64_t)_M0L6_2atmpS2760;
      moonbit_incref(_M0L7_2adataS1116);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1126
      = _M0MPC16string6String4view(_M0L7_2adataS1116, _M0L6_2atmpS2758, _M0L6_2atmpS2759);
      _M0L6_2atmpS2755 = (int64_t)_M0L8_2astartS1117;
      _M0L6_2atmpS2757 = _M0Lm20match__tag__saver__0S1122;
      _M0L6_2atmpS2756 = (int64_t)_M0L6_2atmpS2757;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1127
      = _M0MPC16string6String4view(_M0L7_2adataS1116, _M0L6_2atmpS2755, _M0L6_2atmpS2756);
      _M0L4SomeS2754
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2754)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2754)->$0_0
      = _M0L13package__nameS1126.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2754)->$0_1
      = _M0L13package__nameS1126.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2754)->$0_2
      = _M0L13package__nameS1126.$2;
      _M0L7_2abindS1124
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1124)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1124->$0_0 = _M0L12module__nameS1127.$0;
      _M0L7_2abindS1124->$0_1 = _M0L12module__nameS1127.$1;
      _M0L7_2abindS1124->$0_2 = _M0L12module__nameS1127.$2;
      _M0L7_2abindS1124->$1 = _M0L4SomeS2754;
      break;
    }
    default: {
      void* _M0L4NoneS2763;
      moonbit_decref(_M0L7_2adataS1116);
      _M0L4NoneS2763
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1124
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1124)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1124->$0_0 = _M0L3pkgS1114.$0;
      _M0L7_2abindS1124->$0_1 = _M0L3pkgS1114.$1;
      _M0L7_2abindS1124->$0_2 = _M0L3pkgS1114.$2;
      _M0L7_2abindS1124->$1 = _M0L4NoneS2763;
      break;
    }
  }
  joinlet_3650:;
  _M0L8_2afieldS3208
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1124->$0_1, _M0L7_2abindS1124->$0_2, _M0L7_2abindS1124->$0_0
  };
  _M0L15_2amodule__nameS1133 = _M0L8_2afieldS3208;
  _M0L8_2afieldS3207 = _M0L7_2abindS1124->$1;
  _M0L6_2acntS3512 = Moonbit_object_header(_M0L7_2abindS1124)->rc;
  if (_M0L6_2acntS3512 > 1) {
    int32_t _M0L11_2anew__cntS3513 = _M0L6_2acntS3512 - 1;
    Moonbit_object_header(_M0L7_2abindS1124)->rc = _M0L11_2anew__cntS3513;
    moonbit_incref(_M0L8_2afieldS3207);
    moonbit_incref(_M0L15_2amodule__nameS1133.$0);
  } else if (_M0L6_2acntS3512 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1124);
  }
  _M0L16_2apackage__nameS1134 = _M0L8_2afieldS3207;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1134)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1135 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1134;
      struct _M0TPC16string10StringView _M0L8_2afieldS3206 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1135->$0_1,
                                              _M0L7_2aSomeS1135->$0_2,
                                              _M0L7_2aSomeS1135->$0_0};
      int32_t _M0L6_2acntS3514 = Moonbit_object_header(_M0L7_2aSomeS1135)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1136;
      if (_M0L6_2acntS3514 > 1) {
        int32_t _M0L11_2anew__cntS3515 = _M0L6_2acntS3514 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1135)->rc = _M0L11_2anew__cntS3515;
        moonbit_incref(_M0L8_2afieldS3206.$0);
      } else if (_M0L6_2acntS3514 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1135);
      }
      _M0L12_2apkg__nameS1136 = _M0L8_2afieldS3206;
      if (_M0L6loggerS1137.$1) {
        moonbit_incref(_M0L6loggerS1137.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1137.$0->$method_2(_M0L6loggerS1137.$1, _M0L12_2apkg__nameS1136);
      if (_M0L6loggerS1137.$1) {
        moonbit_incref(_M0L6loggerS1137.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1137.$0->$method_3(_M0L6loggerS1137.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1134);
      break;
    }
  }
  _M0L8_2afieldS3205
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1115->$1_1, _M0L4selfS1115->$1_2, _M0L4selfS1115->$1_0
  };
  _M0L8filenameS2749 = _M0L8_2afieldS3205;
  moonbit_incref(_M0L8filenameS2749.$0);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_2(_M0L6loggerS1137.$1, _M0L8filenameS2749);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_3(_M0L6loggerS1137.$1, 58);
  _M0L8_2afieldS3204
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1115->$2_1, _M0L4selfS1115->$2_2, _M0L4selfS1115->$2_0
  };
  _M0L11start__lineS2750 = _M0L8_2afieldS3204;
  moonbit_incref(_M0L11start__lineS2750.$0);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_2(_M0L6loggerS1137.$1, _M0L11start__lineS2750);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_3(_M0L6loggerS1137.$1, 58);
  _M0L8_2afieldS3203
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1115->$3_1, _M0L4selfS1115->$3_2, _M0L4selfS1115->$3_0
  };
  _M0L13start__columnS2751 = _M0L8_2afieldS3203;
  moonbit_incref(_M0L13start__columnS2751.$0);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_2(_M0L6loggerS1137.$1, _M0L13start__columnS2751);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_3(_M0L6loggerS1137.$1, 45);
  _M0L8_2afieldS3202
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1115->$4_1, _M0L4selfS1115->$4_2, _M0L4selfS1115->$4_0
  };
  _M0L9end__lineS2752 = _M0L8_2afieldS3202;
  moonbit_incref(_M0L9end__lineS2752.$0);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_2(_M0L6loggerS1137.$1, _M0L9end__lineS2752);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_3(_M0L6loggerS1137.$1, 58);
  _M0L8_2afieldS3201
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1115->$5_1, _M0L4selfS1115->$5_2, _M0L4selfS1115->$5_0
  };
  _M0L6_2acntS3516 = Moonbit_object_header(_M0L4selfS1115)->rc;
  if (_M0L6_2acntS3516 > 1) {
    int32_t _M0L11_2anew__cntS3522 = _M0L6_2acntS3516 - 1;
    Moonbit_object_header(_M0L4selfS1115)->rc = _M0L11_2anew__cntS3522;
    moonbit_incref(_M0L8_2afieldS3201.$0);
  } else if (_M0L6_2acntS3516 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3521 =
      (struct _M0TPC16string10StringView){_M0L4selfS1115->$4_1,
                                            _M0L4selfS1115->$4_2,
                                            _M0L4selfS1115->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3520;
    struct _M0TPC16string10StringView _M0L8_2afieldS3519;
    struct _M0TPC16string10StringView _M0L8_2afieldS3518;
    struct _M0TPC16string10StringView _M0L8_2afieldS3517;
    moonbit_decref(_M0L8_2afieldS3521.$0);
    _M0L8_2afieldS3520
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1115->$3_1, _M0L4selfS1115->$3_2, _M0L4selfS1115->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3520.$0);
    _M0L8_2afieldS3519
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1115->$2_1, _M0L4selfS1115->$2_2, _M0L4selfS1115->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3519.$0);
    _M0L8_2afieldS3518
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1115->$1_1, _M0L4selfS1115->$1_2, _M0L4selfS1115->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3518.$0);
    _M0L8_2afieldS3517
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1115->$0_1, _M0L4selfS1115->$0_2, _M0L4selfS1115->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3517.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1115);
  }
  _M0L11end__columnS2753 = _M0L8_2afieldS3201;
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_2(_M0L6loggerS1137.$1, _M0L11end__columnS2753);
  if (_M0L6loggerS1137.$1) {
    moonbit_incref(_M0L6loggerS1137.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_3(_M0L6loggerS1137.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1137.$0->$method_2(_M0L6loggerS1137.$1, _M0L15_2amodule__nameS1133);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1113) {
  moonbit_string_t _M0L6_2atmpS2748;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2748
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1113);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2748);
  moonbit_decref(_M0L6_2atmpS2748);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1112,
  struct _M0TPB6Logger _M0L6loggerS1111
) {
  moonbit_string_t _M0L6_2atmpS2747;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2747 = _M0MPC16double6Double10to__string(_M0L4selfS1112);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1111.$0->$method_0(_M0L6loggerS1111.$1, _M0L6_2atmpS2747);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1110) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1110);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1097) {
  uint64_t _M0L4bitsS1098;
  uint64_t _M0L6_2atmpS2746;
  uint64_t _M0L6_2atmpS2745;
  int32_t _M0L8ieeeSignS1099;
  uint64_t _M0L12ieeeMantissaS1100;
  uint64_t _M0L6_2atmpS2744;
  uint64_t _M0L6_2atmpS2743;
  int32_t _M0L12ieeeExponentS1101;
  int32_t _if__result_3654;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1102;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1103;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2742;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1097 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_56.data;
  }
  _M0L4bitsS1098 = *(int64_t*)&_M0L3valS1097;
  _M0L6_2atmpS2746 = _M0L4bitsS1098 >> 63;
  _M0L6_2atmpS2745 = _M0L6_2atmpS2746 & 1ull;
  _M0L8ieeeSignS1099 = _M0L6_2atmpS2745 != 0ull;
  _M0L12ieeeMantissaS1100 = _M0L4bitsS1098 & 4503599627370495ull;
  _M0L6_2atmpS2744 = _M0L4bitsS1098 >> 52;
  _M0L6_2atmpS2743 = _M0L6_2atmpS2744 & 2047ull;
  _M0L12ieeeExponentS1101 = (int32_t)_M0L6_2atmpS2743;
  if (_M0L12ieeeExponentS1101 == 2047) {
    _if__result_3654 = 1;
  } else if (_M0L12ieeeExponentS1101 == 0) {
    _if__result_3654 = _M0L12ieeeMantissaS1100 == 0ull;
  } else {
    _if__result_3654 = 0;
  }
  if (_if__result_3654) {
    int32_t _M0L6_2atmpS2731 = _M0L12ieeeExponentS1101 != 0;
    int32_t _M0L6_2atmpS2732 = _M0L12ieeeMantissaS1100 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1099, _M0L6_2atmpS2731, _M0L6_2atmpS2732);
  }
  _M0Lm1vS1102 = _M0FPB31ryu__to__string_2erecord_2f1096;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1103
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1100, _M0L12ieeeExponentS1101);
  if (_M0L5smallS1103 == 0) {
    uint32_t _M0L6_2atmpS2733;
    if (_M0L5smallS1103) {
      moonbit_decref(_M0L5smallS1103);
    }
    _M0L6_2atmpS2733 = *(uint32_t*)&_M0L12ieeeExponentS1101;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1102 = _M0FPB3d2d(_M0L12ieeeMantissaS1100, _M0L6_2atmpS2733);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1104 = _M0L5smallS1103;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1105 = _M0L7_2aSomeS1104;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1106 = _M0L4_2afS1105;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2741 = _M0Lm1xS1106;
      uint64_t _M0L8_2afieldS3212 = _M0L6_2atmpS2741->$0;
      uint64_t _M0L8mantissaS2740 = _M0L8_2afieldS3212;
      uint64_t _M0L1qS1107 = _M0L8mantissaS2740 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2739 = _M0Lm1xS1106;
      uint64_t _M0L8_2afieldS3211 = _M0L6_2atmpS2739->$0;
      uint64_t _M0L8mantissaS2737 = _M0L8_2afieldS3211;
      uint64_t _M0L6_2atmpS2738 = 10ull * _M0L1qS1107;
      uint64_t _M0L1rS1108 = _M0L8mantissaS2737 - _M0L6_2atmpS2738;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2736;
      int32_t _M0L8_2afieldS3210;
      int32_t _M0L8exponentS2735;
      int32_t _M0L6_2atmpS2734;
      if (_M0L1rS1108 != 0ull) {
        break;
      }
      _M0L6_2atmpS2736 = _M0Lm1xS1106;
      _M0L8_2afieldS3210 = _M0L6_2atmpS2736->$1;
      moonbit_decref(_M0L6_2atmpS2736);
      _M0L8exponentS2735 = _M0L8_2afieldS3210;
      _M0L6_2atmpS2734 = _M0L8exponentS2735 + 1;
      _M0Lm1xS1106
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1106)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1106->$0 = _M0L1qS1107;
      _M0Lm1xS1106->$1 = _M0L6_2atmpS2734;
      continue;
      break;
    }
    _M0Lm1vS1102 = _M0Lm1xS1106;
  }
  _M0L6_2atmpS2742 = _M0Lm1vS1102;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2742, _M0L8ieeeSignS1099);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1091,
  int32_t _M0L12ieeeExponentS1093
) {
  uint64_t _M0L2m2S1090;
  int32_t _M0L6_2atmpS2730;
  int32_t _M0L2e2S1092;
  int32_t _M0L6_2atmpS2729;
  uint64_t _M0L6_2atmpS2728;
  uint64_t _M0L4maskS1094;
  uint64_t _M0L8fractionS1095;
  int32_t _M0L6_2atmpS2727;
  uint64_t _M0L6_2atmpS2726;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2725;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1090 = 4503599627370496ull | _M0L12ieeeMantissaS1091;
  _M0L6_2atmpS2730 = _M0L12ieeeExponentS1093 - 1023;
  _M0L2e2S1092 = _M0L6_2atmpS2730 - 52;
  if (_M0L2e2S1092 > 0) {
    return 0;
  }
  if (_M0L2e2S1092 < -52) {
    return 0;
  }
  _M0L6_2atmpS2729 = -_M0L2e2S1092;
  _M0L6_2atmpS2728 = 1ull << (_M0L6_2atmpS2729 & 63);
  _M0L4maskS1094 = _M0L6_2atmpS2728 - 1ull;
  _M0L8fractionS1095 = _M0L2m2S1090 & _M0L4maskS1094;
  if (_M0L8fractionS1095 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2727 = -_M0L2e2S1092;
  _M0L6_2atmpS2726 = _M0L2m2S1090 >> (_M0L6_2atmpS2727 & 63);
  _M0L6_2atmpS2725
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2725)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2725->$0 = _M0L6_2atmpS2726;
  _M0L6_2atmpS2725->$1 = 0;
  return _M0L6_2atmpS2725;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1064,
  int32_t _M0L4signS1062
) {
  int32_t _M0L6_2atmpS2724;
  moonbit_bytes_t _M0L6resultS1060;
  int32_t _M0Lm5indexS1061;
  uint64_t _M0Lm6outputS1063;
  uint64_t _M0L6_2atmpS2723;
  int32_t _M0L7olengthS1065;
  int32_t _M0L8_2afieldS3213;
  int32_t _M0L8exponentS2722;
  int32_t _M0L6_2atmpS2721;
  int32_t _M0Lm3expS1066;
  int32_t _M0L6_2atmpS2720;
  int32_t _M0L6_2atmpS2718;
  int32_t _M0L18scientificNotationS1067;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2724 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1060
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2724);
  _M0Lm5indexS1061 = 0;
  if (_M0L4signS1062) {
    int32_t _M0L6_2atmpS2593 = _M0Lm5indexS1061;
    int32_t _M0L6_2atmpS2594;
    if (
      _M0L6_2atmpS2593 < 0
      || _M0L6_2atmpS2593 >= Moonbit_array_length(_M0L6resultS1060)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1060[_M0L6_2atmpS2593] = 45;
    _M0L6_2atmpS2594 = _M0Lm5indexS1061;
    _M0Lm5indexS1061 = _M0L6_2atmpS2594 + 1;
  }
  _M0Lm6outputS1063 = _M0L1vS1064->$0;
  _M0L6_2atmpS2723 = _M0Lm6outputS1063;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1065 = _M0FPB17decimal__length17(_M0L6_2atmpS2723);
  _M0L8_2afieldS3213 = _M0L1vS1064->$1;
  moonbit_decref(_M0L1vS1064);
  _M0L8exponentS2722 = _M0L8_2afieldS3213;
  _M0L6_2atmpS2721 = _M0L8exponentS2722 + _M0L7olengthS1065;
  _M0Lm3expS1066 = _M0L6_2atmpS2721 - 1;
  _M0L6_2atmpS2720 = _M0Lm3expS1066;
  if (_M0L6_2atmpS2720 >= -6) {
    int32_t _M0L6_2atmpS2719 = _M0Lm3expS1066;
    _M0L6_2atmpS2718 = _M0L6_2atmpS2719 < 21;
  } else {
    _M0L6_2atmpS2718 = 0;
  }
  _M0L18scientificNotationS1067 = !_M0L6_2atmpS2718;
  if (_M0L18scientificNotationS1067) {
    int32_t _M0L7_2abindS1068 = _M0L7olengthS1065 - 1;
    int32_t _M0L1iS1069 = 0;
    int32_t _M0L6_2atmpS2604;
    uint64_t _M0L6_2atmpS2609;
    int32_t _M0L6_2atmpS2608;
    int32_t _M0L6_2atmpS2607;
    int32_t _M0L6_2atmpS2606;
    int32_t _M0L6_2atmpS2605;
    int32_t _M0L6_2atmpS2613;
    int32_t _M0L6_2atmpS2614;
    int32_t _M0L6_2atmpS2615;
    int32_t _M0L6_2atmpS2616;
    int32_t _M0L6_2atmpS2617;
    int32_t _M0L6_2atmpS2623;
    int32_t _M0L6_2atmpS2656;
    while (1) {
      if (_M0L1iS1069 < _M0L7_2abindS1068) {
        uint64_t _M0L6_2atmpS2602 = _M0Lm6outputS1063;
        uint64_t _M0L1cS1070 = _M0L6_2atmpS2602 % 10ull;
        uint64_t _M0L6_2atmpS2595 = _M0Lm6outputS1063;
        int32_t _M0L6_2atmpS2601;
        int32_t _M0L6_2atmpS2600;
        int32_t _M0L6_2atmpS2596;
        int32_t _M0L6_2atmpS2599;
        int32_t _M0L6_2atmpS2598;
        int32_t _M0L6_2atmpS2597;
        int32_t _M0L6_2atmpS2603;
        _M0Lm6outputS1063 = _M0L6_2atmpS2595 / 10ull;
        _M0L6_2atmpS2601 = _M0Lm5indexS1061;
        _M0L6_2atmpS2600 = _M0L6_2atmpS2601 + _M0L7olengthS1065;
        _M0L6_2atmpS2596 = _M0L6_2atmpS2600 - _M0L1iS1069;
        _M0L6_2atmpS2599 = (int32_t)_M0L1cS1070;
        _M0L6_2atmpS2598 = 48 + _M0L6_2atmpS2599;
        _M0L6_2atmpS2597 = _M0L6_2atmpS2598 & 0xff;
        if (
          _M0L6_2atmpS2596 < 0
          || _M0L6_2atmpS2596 >= Moonbit_array_length(_M0L6resultS1060)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1060[_M0L6_2atmpS2596] = _M0L6_2atmpS2597;
        _M0L6_2atmpS2603 = _M0L1iS1069 + 1;
        _M0L1iS1069 = _M0L6_2atmpS2603;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2604 = _M0Lm5indexS1061;
    _M0L6_2atmpS2609 = _M0Lm6outputS1063;
    _M0L6_2atmpS2608 = (int32_t)_M0L6_2atmpS2609;
    _M0L6_2atmpS2607 = _M0L6_2atmpS2608 % 10;
    _M0L6_2atmpS2606 = 48 + _M0L6_2atmpS2607;
    _M0L6_2atmpS2605 = _M0L6_2atmpS2606 & 0xff;
    if (
      _M0L6_2atmpS2604 < 0
      || _M0L6_2atmpS2604 >= Moonbit_array_length(_M0L6resultS1060)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1060[_M0L6_2atmpS2604] = _M0L6_2atmpS2605;
    if (_M0L7olengthS1065 > 1) {
      int32_t _M0L6_2atmpS2611 = _M0Lm5indexS1061;
      int32_t _M0L6_2atmpS2610 = _M0L6_2atmpS2611 + 1;
      if (
        _M0L6_2atmpS2610 < 0
        || _M0L6_2atmpS2610 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2610] = 46;
    } else {
      int32_t _M0L6_2atmpS2612 = _M0Lm5indexS1061;
      _M0Lm5indexS1061 = _M0L6_2atmpS2612 - 1;
    }
    _M0L6_2atmpS2613 = _M0Lm5indexS1061;
    _M0L6_2atmpS2614 = _M0L7olengthS1065 + 1;
    _M0Lm5indexS1061 = _M0L6_2atmpS2613 + _M0L6_2atmpS2614;
    _M0L6_2atmpS2615 = _M0Lm5indexS1061;
    if (
      _M0L6_2atmpS2615 < 0
      || _M0L6_2atmpS2615 >= Moonbit_array_length(_M0L6resultS1060)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1060[_M0L6_2atmpS2615] = 101;
    _M0L6_2atmpS2616 = _M0Lm5indexS1061;
    _M0Lm5indexS1061 = _M0L6_2atmpS2616 + 1;
    _M0L6_2atmpS2617 = _M0Lm3expS1066;
    if (_M0L6_2atmpS2617 < 0) {
      int32_t _M0L6_2atmpS2618 = _M0Lm5indexS1061;
      int32_t _M0L6_2atmpS2619;
      int32_t _M0L6_2atmpS2620;
      if (
        _M0L6_2atmpS2618 < 0
        || _M0L6_2atmpS2618 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2618] = 45;
      _M0L6_2atmpS2619 = _M0Lm5indexS1061;
      _M0Lm5indexS1061 = _M0L6_2atmpS2619 + 1;
      _M0L6_2atmpS2620 = _M0Lm3expS1066;
      _M0Lm3expS1066 = -_M0L6_2atmpS2620;
    } else {
      int32_t _M0L6_2atmpS2621 = _M0Lm5indexS1061;
      int32_t _M0L6_2atmpS2622;
      if (
        _M0L6_2atmpS2621 < 0
        || _M0L6_2atmpS2621 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2621] = 43;
      _M0L6_2atmpS2622 = _M0Lm5indexS1061;
      _M0Lm5indexS1061 = _M0L6_2atmpS2622 + 1;
    }
    _M0L6_2atmpS2623 = _M0Lm3expS1066;
    if (_M0L6_2atmpS2623 >= 100) {
      int32_t _M0L6_2atmpS2639 = _M0Lm3expS1066;
      int32_t _M0L1aS1072 = _M0L6_2atmpS2639 / 100;
      int32_t _M0L6_2atmpS2638 = _M0Lm3expS1066;
      int32_t _M0L6_2atmpS2637 = _M0L6_2atmpS2638 / 10;
      int32_t _M0L1bS1073 = _M0L6_2atmpS2637 % 10;
      int32_t _M0L6_2atmpS2636 = _M0Lm3expS1066;
      int32_t _M0L1cS1074 = _M0L6_2atmpS2636 % 10;
      int32_t _M0L6_2atmpS2624 = _M0Lm5indexS1061;
      int32_t _M0L6_2atmpS2626 = 48 + _M0L1aS1072;
      int32_t _M0L6_2atmpS2625 = _M0L6_2atmpS2626 & 0xff;
      int32_t _M0L6_2atmpS2630;
      int32_t _M0L6_2atmpS2627;
      int32_t _M0L6_2atmpS2629;
      int32_t _M0L6_2atmpS2628;
      int32_t _M0L6_2atmpS2634;
      int32_t _M0L6_2atmpS2631;
      int32_t _M0L6_2atmpS2633;
      int32_t _M0L6_2atmpS2632;
      int32_t _M0L6_2atmpS2635;
      if (
        _M0L6_2atmpS2624 < 0
        || _M0L6_2atmpS2624 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2624] = _M0L6_2atmpS2625;
      _M0L6_2atmpS2630 = _M0Lm5indexS1061;
      _M0L6_2atmpS2627 = _M0L6_2atmpS2630 + 1;
      _M0L6_2atmpS2629 = 48 + _M0L1bS1073;
      _M0L6_2atmpS2628 = _M0L6_2atmpS2629 & 0xff;
      if (
        _M0L6_2atmpS2627 < 0
        || _M0L6_2atmpS2627 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2627] = _M0L6_2atmpS2628;
      _M0L6_2atmpS2634 = _M0Lm5indexS1061;
      _M0L6_2atmpS2631 = _M0L6_2atmpS2634 + 2;
      _M0L6_2atmpS2633 = 48 + _M0L1cS1074;
      _M0L6_2atmpS2632 = _M0L6_2atmpS2633 & 0xff;
      if (
        _M0L6_2atmpS2631 < 0
        || _M0L6_2atmpS2631 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2631] = _M0L6_2atmpS2632;
      _M0L6_2atmpS2635 = _M0Lm5indexS1061;
      _M0Lm5indexS1061 = _M0L6_2atmpS2635 + 3;
    } else {
      int32_t _M0L6_2atmpS2640 = _M0Lm3expS1066;
      if (_M0L6_2atmpS2640 >= 10) {
        int32_t _M0L6_2atmpS2650 = _M0Lm3expS1066;
        int32_t _M0L1aS1075 = _M0L6_2atmpS2650 / 10;
        int32_t _M0L6_2atmpS2649 = _M0Lm3expS1066;
        int32_t _M0L1bS1076 = _M0L6_2atmpS2649 % 10;
        int32_t _M0L6_2atmpS2641 = _M0Lm5indexS1061;
        int32_t _M0L6_2atmpS2643 = 48 + _M0L1aS1075;
        int32_t _M0L6_2atmpS2642 = _M0L6_2atmpS2643 & 0xff;
        int32_t _M0L6_2atmpS2647;
        int32_t _M0L6_2atmpS2644;
        int32_t _M0L6_2atmpS2646;
        int32_t _M0L6_2atmpS2645;
        int32_t _M0L6_2atmpS2648;
        if (
          _M0L6_2atmpS2641 < 0
          || _M0L6_2atmpS2641 >= Moonbit_array_length(_M0L6resultS1060)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1060[_M0L6_2atmpS2641] = _M0L6_2atmpS2642;
        _M0L6_2atmpS2647 = _M0Lm5indexS1061;
        _M0L6_2atmpS2644 = _M0L6_2atmpS2647 + 1;
        _M0L6_2atmpS2646 = 48 + _M0L1bS1076;
        _M0L6_2atmpS2645 = _M0L6_2atmpS2646 & 0xff;
        if (
          _M0L6_2atmpS2644 < 0
          || _M0L6_2atmpS2644 >= Moonbit_array_length(_M0L6resultS1060)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1060[_M0L6_2atmpS2644] = _M0L6_2atmpS2645;
        _M0L6_2atmpS2648 = _M0Lm5indexS1061;
        _M0Lm5indexS1061 = _M0L6_2atmpS2648 + 2;
      } else {
        int32_t _M0L6_2atmpS2651 = _M0Lm5indexS1061;
        int32_t _M0L6_2atmpS2654 = _M0Lm3expS1066;
        int32_t _M0L6_2atmpS2653 = 48 + _M0L6_2atmpS2654;
        int32_t _M0L6_2atmpS2652 = _M0L6_2atmpS2653 & 0xff;
        int32_t _M0L6_2atmpS2655;
        if (
          _M0L6_2atmpS2651 < 0
          || _M0L6_2atmpS2651 >= Moonbit_array_length(_M0L6resultS1060)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1060[_M0L6_2atmpS2651] = _M0L6_2atmpS2652;
        _M0L6_2atmpS2655 = _M0Lm5indexS1061;
        _M0Lm5indexS1061 = _M0L6_2atmpS2655 + 1;
      }
    }
    _M0L6_2atmpS2656 = _M0Lm5indexS1061;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1060, 0, _M0L6_2atmpS2656);
  } else {
    int32_t _M0L6_2atmpS2657 = _M0Lm3expS1066;
    int32_t _M0L6_2atmpS2717;
    if (_M0L6_2atmpS2657 < 0) {
      int32_t _M0L6_2atmpS2658 = _M0Lm5indexS1061;
      int32_t _M0L6_2atmpS2659;
      int32_t _M0L6_2atmpS2660;
      int32_t _M0L6_2atmpS2661;
      int32_t _M0L1iS1077;
      int32_t _M0L7currentS1079;
      int32_t _M0L1iS1080;
      if (
        _M0L6_2atmpS2658 < 0
        || _M0L6_2atmpS2658 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2658] = 48;
      _M0L6_2atmpS2659 = _M0Lm5indexS1061;
      _M0Lm5indexS1061 = _M0L6_2atmpS2659 + 1;
      _M0L6_2atmpS2660 = _M0Lm5indexS1061;
      if (
        _M0L6_2atmpS2660 < 0
        || _M0L6_2atmpS2660 >= Moonbit_array_length(_M0L6resultS1060)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1060[_M0L6_2atmpS2660] = 46;
      _M0L6_2atmpS2661 = _M0Lm5indexS1061;
      _M0Lm5indexS1061 = _M0L6_2atmpS2661 + 1;
      _M0L1iS1077 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2662 = _M0Lm3expS1066;
        if (_M0L1iS1077 > _M0L6_2atmpS2662) {
          int32_t _M0L6_2atmpS2663 = _M0Lm5indexS1061;
          int32_t _M0L6_2atmpS2664;
          int32_t _M0L6_2atmpS2665;
          if (
            _M0L6_2atmpS2663 < 0
            || _M0L6_2atmpS2663 >= Moonbit_array_length(_M0L6resultS1060)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1060[_M0L6_2atmpS2663] = 48;
          _M0L6_2atmpS2664 = _M0Lm5indexS1061;
          _M0Lm5indexS1061 = _M0L6_2atmpS2664 + 1;
          _M0L6_2atmpS2665 = _M0L1iS1077 - 1;
          _M0L1iS1077 = _M0L6_2atmpS2665;
          continue;
        }
        break;
      }
      _M0L7currentS1079 = _M0Lm5indexS1061;
      _M0L1iS1080 = 0;
      while (1) {
        if (_M0L1iS1080 < _M0L7olengthS1065) {
          int32_t _M0L6_2atmpS2673 = _M0L7currentS1079 + _M0L7olengthS1065;
          int32_t _M0L6_2atmpS2672 = _M0L6_2atmpS2673 - _M0L1iS1080;
          int32_t _M0L6_2atmpS2666 = _M0L6_2atmpS2672 - 1;
          uint64_t _M0L6_2atmpS2671 = _M0Lm6outputS1063;
          uint64_t _M0L6_2atmpS2670 = _M0L6_2atmpS2671 % 10ull;
          int32_t _M0L6_2atmpS2669 = (int32_t)_M0L6_2atmpS2670;
          int32_t _M0L6_2atmpS2668 = 48 + _M0L6_2atmpS2669;
          int32_t _M0L6_2atmpS2667 = _M0L6_2atmpS2668 & 0xff;
          uint64_t _M0L6_2atmpS2674;
          int32_t _M0L6_2atmpS2675;
          int32_t _M0L6_2atmpS2676;
          if (
            _M0L6_2atmpS2666 < 0
            || _M0L6_2atmpS2666 >= Moonbit_array_length(_M0L6resultS1060)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1060[_M0L6_2atmpS2666] = _M0L6_2atmpS2667;
          _M0L6_2atmpS2674 = _M0Lm6outputS1063;
          _M0Lm6outputS1063 = _M0L6_2atmpS2674 / 10ull;
          _M0L6_2atmpS2675 = _M0Lm5indexS1061;
          _M0Lm5indexS1061 = _M0L6_2atmpS2675 + 1;
          _M0L6_2atmpS2676 = _M0L1iS1080 + 1;
          _M0L1iS1080 = _M0L6_2atmpS2676;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2678 = _M0Lm3expS1066;
      int32_t _M0L6_2atmpS2677 = _M0L6_2atmpS2678 + 1;
      if (_M0L6_2atmpS2677 >= _M0L7olengthS1065) {
        int32_t _M0L1iS1082 = 0;
        int32_t _M0L6_2atmpS2690;
        int32_t _M0L6_2atmpS2694;
        int32_t _M0L7_2abindS1084;
        int32_t _M0L2__S1085;
        while (1) {
          if (_M0L1iS1082 < _M0L7olengthS1065) {
            int32_t _M0L6_2atmpS2687 = _M0Lm5indexS1061;
            int32_t _M0L6_2atmpS2686 = _M0L6_2atmpS2687 + _M0L7olengthS1065;
            int32_t _M0L6_2atmpS2685 = _M0L6_2atmpS2686 - _M0L1iS1082;
            int32_t _M0L6_2atmpS2679 = _M0L6_2atmpS2685 - 1;
            uint64_t _M0L6_2atmpS2684 = _M0Lm6outputS1063;
            uint64_t _M0L6_2atmpS2683 = _M0L6_2atmpS2684 % 10ull;
            int32_t _M0L6_2atmpS2682 = (int32_t)_M0L6_2atmpS2683;
            int32_t _M0L6_2atmpS2681 = 48 + _M0L6_2atmpS2682;
            int32_t _M0L6_2atmpS2680 = _M0L6_2atmpS2681 & 0xff;
            uint64_t _M0L6_2atmpS2688;
            int32_t _M0L6_2atmpS2689;
            if (
              _M0L6_2atmpS2679 < 0
              || _M0L6_2atmpS2679 >= Moonbit_array_length(_M0L6resultS1060)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1060[_M0L6_2atmpS2679] = _M0L6_2atmpS2680;
            _M0L6_2atmpS2688 = _M0Lm6outputS1063;
            _M0Lm6outputS1063 = _M0L6_2atmpS2688 / 10ull;
            _M0L6_2atmpS2689 = _M0L1iS1082 + 1;
            _M0L1iS1082 = _M0L6_2atmpS2689;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2690 = _M0Lm5indexS1061;
        _M0Lm5indexS1061 = _M0L6_2atmpS2690 + _M0L7olengthS1065;
        _M0L6_2atmpS2694 = _M0Lm3expS1066;
        _M0L7_2abindS1084 = _M0L6_2atmpS2694 + 1;
        _M0L2__S1085 = _M0L7olengthS1065;
        while (1) {
          if (_M0L2__S1085 < _M0L7_2abindS1084) {
            int32_t _M0L6_2atmpS2691 = _M0Lm5indexS1061;
            int32_t _M0L6_2atmpS2692;
            int32_t _M0L6_2atmpS2693;
            if (
              _M0L6_2atmpS2691 < 0
              || _M0L6_2atmpS2691 >= Moonbit_array_length(_M0L6resultS1060)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1060[_M0L6_2atmpS2691] = 48;
            _M0L6_2atmpS2692 = _M0Lm5indexS1061;
            _M0Lm5indexS1061 = _M0L6_2atmpS2692 + 1;
            _M0L6_2atmpS2693 = _M0L2__S1085 + 1;
            _M0L2__S1085 = _M0L6_2atmpS2693;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2716 = _M0Lm5indexS1061;
        int32_t _M0Lm7currentS1087 = _M0L6_2atmpS2716 + 1;
        int32_t _M0L1iS1088 = 0;
        int32_t _M0L6_2atmpS2714;
        int32_t _M0L6_2atmpS2715;
        while (1) {
          if (_M0L1iS1088 < _M0L7olengthS1065) {
            int32_t _M0L6_2atmpS2697 = _M0L7olengthS1065 - _M0L1iS1088;
            int32_t _M0L6_2atmpS2695 = _M0L6_2atmpS2697 - 1;
            int32_t _M0L6_2atmpS2696 = _M0Lm3expS1066;
            int32_t _M0L6_2atmpS2711;
            int32_t _M0L6_2atmpS2710;
            int32_t _M0L6_2atmpS2709;
            int32_t _M0L6_2atmpS2703;
            uint64_t _M0L6_2atmpS2708;
            uint64_t _M0L6_2atmpS2707;
            int32_t _M0L6_2atmpS2706;
            int32_t _M0L6_2atmpS2705;
            int32_t _M0L6_2atmpS2704;
            uint64_t _M0L6_2atmpS2712;
            int32_t _M0L6_2atmpS2713;
            if (_M0L6_2atmpS2695 == _M0L6_2atmpS2696) {
              int32_t _M0L6_2atmpS2701 = _M0Lm7currentS1087;
              int32_t _M0L6_2atmpS2700 = _M0L6_2atmpS2701 + _M0L7olengthS1065;
              int32_t _M0L6_2atmpS2699 = _M0L6_2atmpS2700 - _M0L1iS1088;
              int32_t _M0L6_2atmpS2698 = _M0L6_2atmpS2699 - 1;
              int32_t _M0L6_2atmpS2702;
              if (
                _M0L6_2atmpS2698 < 0
                || _M0L6_2atmpS2698 >= Moonbit_array_length(_M0L6resultS1060)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1060[_M0L6_2atmpS2698] = 46;
              _M0L6_2atmpS2702 = _M0Lm7currentS1087;
              _M0Lm7currentS1087 = _M0L6_2atmpS2702 - 1;
            }
            _M0L6_2atmpS2711 = _M0Lm7currentS1087;
            _M0L6_2atmpS2710 = _M0L6_2atmpS2711 + _M0L7olengthS1065;
            _M0L6_2atmpS2709 = _M0L6_2atmpS2710 - _M0L1iS1088;
            _M0L6_2atmpS2703 = _M0L6_2atmpS2709 - 1;
            _M0L6_2atmpS2708 = _M0Lm6outputS1063;
            _M0L6_2atmpS2707 = _M0L6_2atmpS2708 % 10ull;
            _M0L6_2atmpS2706 = (int32_t)_M0L6_2atmpS2707;
            _M0L6_2atmpS2705 = 48 + _M0L6_2atmpS2706;
            _M0L6_2atmpS2704 = _M0L6_2atmpS2705 & 0xff;
            if (
              _M0L6_2atmpS2703 < 0
              || _M0L6_2atmpS2703 >= Moonbit_array_length(_M0L6resultS1060)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1060[_M0L6_2atmpS2703] = _M0L6_2atmpS2704;
            _M0L6_2atmpS2712 = _M0Lm6outputS1063;
            _M0Lm6outputS1063 = _M0L6_2atmpS2712 / 10ull;
            _M0L6_2atmpS2713 = _M0L1iS1088 + 1;
            _M0L1iS1088 = _M0L6_2atmpS2713;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2714 = _M0Lm5indexS1061;
        _M0L6_2atmpS2715 = _M0L7olengthS1065 + 1;
        _M0Lm5indexS1061 = _M0L6_2atmpS2714 + _M0L6_2atmpS2715;
      }
    }
    _M0L6_2atmpS2717 = _M0Lm5indexS1061;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1060, 0, _M0L6_2atmpS2717);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1006,
  uint32_t _M0L12ieeeExponentS1005
) {
  int32_t _M0Lm2e2S1003;
  uint64_t _M0Lm2m2S1004;
  uint64_t _M0L6_2atmpS2592;
  uint64_t _M0L6_2atmpS2591;
  int32_t _M0L4evenS1007;
  uint64_t _M0L6_2atmpS2590;
  uint64_t _M0L2mvS1008;
  int32_t _M0L7mmShiftS1009;
  uint64_t _M0Lm2vrS1010;
  uint64_t _M0Lm2vpS1011;
  uint64_t _M0Lm2vmS1012;
  int32_t _M0Lm3e10S1013;
  int32_t _M0Lm17vmIsTrailingZerosS1014;
  int32_t _M0Lm17vrIsTrailingZerosS1015;
  int32_t _M0L6_2atmpS2492;
  int32_t _M0Lm7removedS1034;
  int32_t _M0Lm16lastRemovedDigitS1035;
  uint64_t _M0Lm6outputS1036;
  int32_t _M0L6_2atmpS2588;
  int32_t _M0L6_2atmpS2589;
  int32_t _M0L3expS1059;
  uint64_t _M0L6_2atmpS2587;
  struct _M0TPB17FloatingDecimal64* _block_3667;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S1003 = 0;
  _M0Lm2m2S1004 = 0ull;
  if (_M0L12ieeeExponentS1005 == 0u) {
    _M0Lm2e2S1003 = -1076;
    _M0Lm2m2S1004 = _M0L12ieeeMantissaS1006;
  } else {
    int32_t _M0L6_2atmpS2491 = *(int32_t*)&_M0L12ieeeExponentS1005;
    int32_t _M0L6_2atmpS2490 = _M0L6_2atmpS2491 - 1023;
    int32_t _M0L6_2atmpS2489 = _M0L6_2atmpS2490 - 52;
    _M0Lm2e2S1003 = _M0L6_2atmpS2489 - 2;
    _M0Lm2m2S1004 = 4503599627370496ull | _M0L12ieeeMantissaS1006;
  }
  _M0L6_2atmpS2592 = _M0Lm2m2S1004;
  _M0L6_2atmpS2591 = _M0L6_2atmpS2592 & 1ull;
  _M0L4evenS1007 = _M0L6_2atmpS2591 == 0ull;
  _M0L6_2atmpS2590 = _M0Lm2m2S1004;
  _M0L2mvS1008 = 4ull * _M0L6_2atmpS2590;
  if (_M0L12ieeeMantissaS1006 != 0ull) {
    _M0L7mmShiftS1009 = 1;
  } else {
    _M0L7mmShiftS1009 = _M0L12ieeeExponentS1005 <= 1u;
  }
  _M0Lm2vrS1010 = 0ull;
  _M0Lm2vpS1011 = 0ull;
  _M0Lm2vmS1012 = 0ull;
  _M0Lm3e10S1013 = 0;
  _M0Lm17vmIsTrailingZerosS1014 = 0;
  _M0Lm17vrIsTrailingZerosS1015 = 0;
  _M0L6_2atmpS2492 = _M0Lm2e2S1003;
  if (_M0L6_2atmpS2492 >= 0) {
    int32_t _M0L6_2atmpS2514 = _M0Lm2e2S1003;
    int32_t _M0L6_2atmpS2510;
    int32_t _M0L6_2atmpS2513;
    int32_t _M0L6_2atmpS2512;
    int32_t _M0L6_2atmpS2511;
    int32_t _M0L1qS1016;
    int32_t _M0L6_2atmpS2509;
    int32_t _M0L6_2atmpS2508;
    int32_t _M0L1kS1017;
    int32_t _M0L6_2atmpS2507;
    int32_t _M0L6_2atmpS2506;
    int32_t _M0L6_2atmpS2505;
    int32_t _M0L1iS1018;
    struct _M0TPB8Pow5Pair _M0L4pow5S1019;
    uint64_t _M0L6_2atmpS2504;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1020;
    uint64_t _M0L8_2avrOutS1021;
    uint64_t _M0L8_2avpOutS1022;
    uint64_t _M0L8_2avmOutS1023;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2510 = _M0FPB9log10Pow2(_M0L6_2atmpS2514);
    _M0L6_2atmpS2513 = _M0Lm2e2S1003;
    _M0L6_2atmpS2512 = _M0L6_2atmpS2513 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2511 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2512);
    _M0L1qS1016 = _M0L6_2atmpS2510 - _M0L6_2atmpS2511;
    _M0Lm3e10S1013 = _M0L1qS1016;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2509 = _M0FPB8pow5bits(_M0L1qS1016);
    _M0L6_2atmpS2508 = 125 + _M0L6_2atmpS2509;
    _M0L1kS1017 = _M0L6_2atmpS2508 - 1;
    _M0L6_2atmpS2507 = _M0Lm2e2S1003;
    _M0L6_2atmpS2506 = -_M0L6_2atmpS2507;
    _M0L6_2atmpS2505 = _M0L6_2atmpS2506 + _M0L1qS1016;
    _M0L1iS1018 = _M0L6_2atmpS2505 + _M0L1kS1017;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1019 = _M0FPB22double__computeInvPow5(_M0L1qS1016);
    _M0L6_2atmpS2504 = _M0Lm2m2S1004;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1020
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2504, _M0L4pow5S1019, _M0L1iS1018, _M0L7mmShiftS1009);
    _M0L8_2avrOutS1021 = _M0L7_2abindS1020.$0;
    _M0L8_2avpOutS1022 = _M0L7_2abindS1020.$1;
    _M0L8_2avmOutS1023 = _M0L7_2abindS1020.$2;
    _M0Lm2vrS1010 = _M0L8_2avrOutS1021;
    _M0Lm2vpS1011 = _M0L8_2avpOutS1022;
    _M0Lm2vmS1012 = _M0L8_2avmOutS1023;
    if (_M0L1qS1016 <= 21) {
      int32_t _M0L6_2atmpS2500 = (int32_t)_M0L2mvS1008;
      uint64_t _M0L6_2atmpS2503 = _M0L2mvS1008 / 5ull;
      int32_t _M0L6_2atmpS2502 = (int32_t)_M0L6_2atmpS2503;
      int32_t _M0L6_2atmpS2501 = 5 * _M0L6_2atmpS2502;
      int32_t _M0L6mvMod5S1024 = _M0L6_2atmpS2500 - _M0L6_2atmpS2501;
      if (_M0L6mvMod5S1024 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1015
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1008, _M0L1qS1016);
      } else if (_M0L4evenS1007) {
        uint64_t _M0L6_2atmpS2494 = _M0L2mvS1008 - 1ull;
        uint64_t _M0L6_2atmpS2495;
        uint64_t _M0L6_2atmpS2493;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2495 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1009);
        _M0L6_2atmpS2493 = _M0L6_2atmpS2494 - _M0L6_2atmpS2495;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1014
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2493, _M0L1qS1016);
      } else {
        uint64_t _M0L6_2atmpS2496 = _M0Lm2vpS1011;
        uint64_t _M0L6_2atmpS2499 = _M0L2mvS1008 + 2ull;
        int32_t _M0L6_2atmpS2498;
        uint64_t _M0L6_2atmpS2497;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2498
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2499, _M0L1qS1016);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2497 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2498);
        _M0Lm2vpS1011 = _M0L6_2atmpS2496 - _M0L6_2atmpS2497;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2528 = _M0Lm2e2S1003;
    int32_t _M0L6_2atmpS2527 = -_M0L6_2atmpS2528;
    int32_t _M0L6_2atmpS2522;
    int32_t _M0L6_2atmpS2526;
    int32_t _M0L6_2atmpS2525;
    int32_t _M0L6_2atmpS2524;
    int32_t _M0L6_2atmpS2523;
    int32_t _M0L1qS1025;
    int32_t _M0L6_2atmpS2515;
    int32_t _M0L6_2atmpS2521;
    int32_t _M0L6_2atmpS2520;
    int32_t _M0L1iS1026;
    int32_t _M0L6_2atmpS2519;
    int32_t _M0L1kS1027;
    int32_t _M0L1jS1028;
    struct _M0TPB8Pow5Pair _M0L4pow5S1029;
    uint64_t _M0L6_2atmpS2518;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1030;
    uint64_t _M0L8_2avrOutS1031;
    uint64_t _M0L8_2avpOutS1032;
    uint64_t _M0L8_2avmOutS1033;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2522 = _M0FPB9log10Pow5(_M0L6_2atmpS2527);
    _M0L6_2atmpS2526 = _M0Lm2e2S1003;
    _M0L6_2atmpS2525 = -_M0L6_2atmpS2526;
    _M0L6_2atmpS2524 = _M0L6_2atmpS2525 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2523 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2524);
    _M0L1qS1025 = _M0L6_2atmpS2522 - _M0L6_2atmpS2523;
    _M0L6_2atmpS2515 = _M0Lm2e2S1003;
    _M0Lm3e10S1013 = _M0L1qS1025 + _M0L6_2atmpS2515;
    _M0L6_2atmpS2521 = _M0Lm2e2S1003;
    _M0L6_2atmpS2520 = -_M0L6_2atmpS2521;
    _M0L1iS1026 = _M0L6_2atmpS2520 - _M0L1qS1025;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2519 = _M0FPB8pow5bits(_M0L1iS1026);
    _M0L1kS1027 = _M0L6_2atmpS2519 - 125;
    _M0L1jS1028 = _M0L1qS1025 - _M0L1kS1027;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1029 = _M0FPB19double__computePow5(_M0L1iS1026);
    _M0L6_2atmpS2518 = _M0Lm2m2S1004;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1030
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2518, _M0L4pow5S1029, _M0L1jS1028, _M0L7mmShiftS1009);
    _M0L8_2avrOutS1031 = _M0L7_2abindS1030.$0;
    _M0L8_2avpOutS1032 = _M0L7_2abindS1030.$1;
    _M0L8_2avmOutS1033 = _M0L7_2abindS1030.$2;
    _M0Lm2vrS1010 = _M0L8_2avrOutS1031;
    _M0Lm2vpS1011 = _M0L8_2avpOutS1032;
    _M0Lm2vmS1012 = _M0L8_2avmOutS1033;
    if (_M0L1qS1025 <= 1) {
      _M0Lm17vrIsTrailingZerosS1015 = 1;
      if (_M0L4evenS1007) {
        int32_t _M0L6_2atmpS2516;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2516 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1009);
        _M0Lm17vmIsTrailingZerosS1014 = _M0L6_2atmpS2516 == 1;
      } else {
        uint64_t _M0L6_2atmpS2517 = _M0Lm2vpS1011;
        _M0Lm2vpS1011 = _M0L6_2atmpS2517 - 1ull;
      }
    } else if (_M0L1qS1025 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1015
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1008, _M0L1qS1025);
    }
  }
  _M0Lm7removedS1034 = 0;
  _M0Lm16lastRemovedDigitS1035 = 0;
  _M0Lm6outputS1036 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1014 || _M0Lm17vrIsTrailingZerosS1015) {
    int32_t _if__result_3664;
    uint64_t _M0L6_2atmpS2558;
    uint64_t _M0L6_2atmpS2564;
    uint64_t _M0L6_2atmpS2565;
    int32_t _if__result_3665;
    int32_t _M0L6_2atmpS2561;
    int64_t _M0L6_2atmpS2560;
    uint64_t _M0L6_2atmpS2559;
    while (1) {
      uint64_t _M0L6_2atmpS2541 = _M0Lm2vpS1011;
      uint64_t _M0L7vpDiv10S1037 = _M0L6_2atmpS2541 / 10ull;
      uint64_t _M0L6_2atmpS2540 = _M0Lm2vmS1012;
      uint64_t _M0L7vmDiv10S1038 = _M0L6_2atmpS2540 / 10ull;
      uint64_t _M0L6_2atmpS2539;
      int32_t _M0L6_2atmpS2536;
      int32_t _M0L6_2atmpS2538;
      int32_t _M0L6_2atmpS2537;
      int32_t _M0L7vmMod10S1040;
      uint64_t _M0L6_2atmpS2535;
      uint64_t _M0L7vrDiv10S1041;
      uint64_t _M0L6_2atmpS2534;
      int32_t _M0L6_2atmpS2531;
      int32_t _M0L6_2atmpS2533;
      int32_t _M0L6_2atmpS2532;
      int32_t _M0L7vrMod10S1042;
      int32_t _M0L6_2atmpS2530;
      if (_M0L7vpDiv10S1037 <= _M0L7vmDiv10S1038) {
        break;
      }
      _M0L6_2atmpS2539 = _M0Lm2vmS1012;
      _M0L6_2atmpS2536 = (int32_t)_M0L6_2atmpS2539;
      _M0L6_2atmpS2538 = (int32_t)_M0L7vmDiv10S1038;
      _M0L6_2atmpS2537 = 10 * _M0L6_2atmpS2538;
      _M0L7vmMod10S1040 = _M0L6_2atmpS2536 - _M0L6_2atmpS2537;
      _M0L6_2atmpS2535 = _M0Lm2vrS1010;
      _M0L7vrDiv10S1041 = _M0L6_2atmpS2535 / 10ull;
      _M0L6_2atmpS2534 = _M0Lm2vrS1010;
      _M0L6_2atmpS2531 = (int32_t)_M0L6_2atmpS2534;
      _M0L6_2atmpS2533 = (int32_t)_M0L7vrDiv10S1041;
      _M0L6_2atmpS2532 = 10 * _M0L6_2atmpS2533;
      _M0L7vrMod10S1042 = _M0L6_2atmpS2531 - _M0L6_2atmpS2532;
      if (_M0Lm17vmIsTrailingZerosS1014) {
        _M0Lm17vmIsTrailingZerosS1014 = _M0L7vmMod10S1040 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1014 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1015) {
        int32_t _M0L6_2atmpS2529 = _M0Lm16lastRemovedDigitS1035;
        _M0Lm17vrIsTrailingZerosS1015 = _M0L6_2atmpS2529 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1015 = 0;
      }
      _M0Lm16lastRemovedDigitS1035 = _M0L7vrMod10S1042;
      _M0Lm2vrS1010 = _M0L7vrDiv10S1041;
      _M0Lm2vpS1011 = _M0L7vpDiv10S1037;
      _M0Lm2vmS1012 = _M0L7vmDiv10S1038;
      _M0L6_2atmpS2530 = _M0Lm7removedS1034;
      _M0Lm7removedS1034 = _M0L6_2atmpS2530 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1014) {
      while (1) {
        uint64_t _M0L6_2atmpS2554 = _M0Lm2vmS1012;
        uint64_t _M0L7vmDiv10S1043 = _M0L6_2atmpS2554 / 10ull;
        uint64_t _M0L6_2atmpS2553 = _M0Lm2vmS1012;
        int32_t _M0L6_2atmpS2550 = (int32_t)_M0L6_2atmpS2553;
        int32_t _M0L6_2atmpS2552 = (int32_t)_M0L7vmDiv10S1043;
        int32_t _M0L6_2atmpS2551 = 10 * _M0L6_2atmpS2552;
        int32_t _M0L7vmMod10S1044 = _M0L6_2atmpS2550 - _M0L6_2atmpS2551;
        uint64_t _M0L6_2atmpS2549;
        uint64_t _M0L7vpDiv10S1046;
        uint64_t _M0L6_2atmpS2548;
        uint64_t _M0L7vrDiv10S1047;
        uint64_t _M0L6_2atmpS2547;
        int32_t _M0L6_2atmpS2544;
        int32_t _M0L6_2atmpS2546;
        int32_t _M0L6_2atmpS2545;
        int32_t _M0L7vrMod10S1048;
        int32_t _M0L6_2atmpS2543;
        if (_M0L7vmMod10S1044 != 0) {
          break;
        }
        _M0L6_2atmpS2549 = _M0Lm2vpS1011;
        _M0L7vpDiv10S1046 = _M0L6_2atmpS2549 / 10ull;
        _M0L6_2atmpS2548 = _M0Lm2vrS1010;
        _M0L7vrDiv10S1047 = _M0L6_2atmpS2548 / 10ull;
        _M0L6_2atmpS2547 = _M0Lm2vrS1010;
        _M0L6_2atmpS2544 = (int32_t)_M0L6_2atmpS2547;
        _M0L6_2atmpS2546 = (int32_t)_M0L7vrDiv10S1047;
        _M0L6_2atmpS2545 = 10 * _M0L6_2atmpS2546;
        _M0L7vrMod10S1048 = _M0L6_2atmpS2544 - _M0L6_2atmpS2545;
        if (_M0Lm17vrIsTrailingZerosS1015) {
          int32_t _M0L6_2atmpS2542 = _M0Lm16lastRemovedDigitS1035;
          _M0Lm17vrIsTrailingZerosS1015 = _M0L6_2atmpS2542 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1015 = 0;
        }
        _M0Lm16lastRemovedDigitS1035 = _M0L7vrMod10S1048;
        _M0Lm2vrS1010 = _M0L7vrDiv10S1047;
        _M0Lm2vpS1011 = _M0L7vpDiv10S1046;
        _M0Lm2vmS1012 = _M0L7vmDiv10S1043;
        _M0L6_2atmpS2543 = _M0Lm7removedS1034;
        _M0Lm7removedS1034 = _M0L6_2atmpS2543 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1015) {
      int32_t _M0L6_2atmpS2557 = _M0Lm16lastRemovedDigitS1035;
      if (_M0L6_2atmpS2557 == 5) {
        uint64_t _M0L6_2atmpS2556 = _M0Lm2vrS1010;
        uint64_t _M0L6_2atmpS2555 = _M0L6_2atmpS2556 % 2ull;
        _if__result_3664 = _M0L6_2atmpS2555 == 0ull;
      } else {
        _if__result_3664 = 0;
      }
    } else {
      _if__result_3664 = 0;
    }
    if (_if__result_3664) {
      _M0Lm16lastRemovedDigitS1035 = 4;
    }
    _M0L6_2atmpS2558 = _M0Lm2vrS1010;
    _M0L6_2atmpS2564 = _M0Lm2vrS1010;
    _M0L6_2atmpS2565 = _M0Lm2vmS1012;
    if (_M0L6_2atmpS2564 == _M0L6_2atmpS2565) {
      if (!_M0L4evenS1007) {
        _if__result_3665 = 1;
      } else {
        int32_t _M0L6_2atmpS2563 = _M0Lm17vmIsTrailingZerosS1014;
        _if__result_3665 = !_M0L6_2atmpS2563;
      }
    } else {
      _if__result_3665 = 0;
    }
    if (_if__result_3665) {
      _M0L6_2atmpS2561 = 1;
    } else {
      int32_t _M0L6_2atmpS2562 = _M0Lm16lastRemovedDigitS1035;
      _M0L6_2atmpS2561 = _M0L6_2atmpS2562 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2560 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2561);
    _M0L6_2atmpS2559 = *(uint64_t*)&_M0L6_2atmpS2560;
    _M0Lm6outputS1036 = _M0L6_2atmpS2558 + _M0L6_2atmpS2559;
  } else {
    int32_t _M0Lm7roundUpS1049 = 0;
    uint64_t _M0L6_2atmpS2586 = _M0Lm2vpS1011;
    uint64_t _M0L8vpDiv100S1050 = _M0L6_2atmpS2586 / 100ull;
    uint64_t _M0L6_2atmpS2585 = _M0Lm2vmS1012;
    uint64_t _M0L8vmDiv100S1051 = _M0L6_2atmpS2585 / 100ull;
    uint64_t _M0L6_2atmpS2580;
    uint64_t _M0L6_2atmpS2583;
    uint64_t _M0L6_2atmpS2584;
    int32_t _M0L6_2atmpS2582;
    uint64_t _M0L6_2atmpS2581;
    if (_M0L8vpDiv100S1050 > _M0L8vmDiv100S1051) {
      uint64_t _M0L6_2atmpS2571 = _M0Lm2vrS1010;
      uint64_t _M0L8vrDiv100S1052 = _M0L6_2atmpS2571 / 100ull;
      uint64_t _M0L6_2atmpS2570 = _M0Lm2vrS1010;
      int32_t _M0L6_2atmpS2567 = (int32_t)_M0L6_2atmpS2570;
      int32_t _M0L6_2atmpS2569 = (int32_t)_M0L8vrDiv100S1052;
      int32_t _M0L6_2atmpS2568 = 100 * _M0L6_2atmpS2569;
      int32_t _M0L8vrMod100S1053 = _M0L6_2atmpS2567 - _M0L6_2atmpS2568;
      int32_t _M0L6_2atmpS2566;
      _M0Lm7roundUpS1049 = _M0L8vrMod100S1053 >= 50;
      _M0Lm2vrS1010 = _M0L8vrDiv100S1052;
      _M0Lm2vpS1011 = _M0L8vpDiv100S1050;
      _M0Lm2vmS1012 = _M0L8vmDiv100S1051;
      _M0L6_2atmpS2566 = _M0Lm7removedS1034;
      _M0Lm7removedS1034 = _M0L6_2atmpS2566 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2579 = _M0Lm2vpS1011;
      uint64_t _M0L7vpDiv10S1054 = _M0L6_2atmpS2579 / 10ull;
      uint64_t _M0L6_2atmpS2578 = _M0Lm2vmS1012;
      uint64_t _M0L7vmDiv10S1055 = _M0L6_2atmpS2578 / 10ull;
      uint64_t _M0L6_2atmpS2577;
      uint64_t _M0L7vrDiv10S1057;
      uint64_t _M0L6_2atmpS2576;
      int32_t _M0L6_2atmpS2573;
      int32_t _M0L6_2atmpS2575;
      int32_t _M0L6_2atmpS2574;
      int32_t _M0L7vrMod10S1058;
      int32_t _M0L6_2atmpS2572;
      if (_M0L7vpDiv10S1054 <= _M0L7vmDiv10S1055) {
        break;
      }
      _M0L6_2atmpS2577 = _M0Lm2vrS1010;
      _M0L7vrDiv10S1057 = _M0L6_2atmpS2577 / 10ull;
      _M0L6_2atmpS2576 = _M0Lm2vrS1010;
      _M0L6_2atmpS2573 = (int32_t)_M0L6_2atmpS2576;
      _M0L6_2atmpS2575 = (int32_t)_M0L7vrDiv10S1057;
      _M0L6_2atmpS2574 = 10 * _M0L6_2atmpS2575;
      _M0L7vrMod10S1058 = _M0L6_2atmpS2573 - _M0L6_2atmpS2574;
      _M0Lm7roundUpS1049 = _M0L7vrMod10S1058 >= 5;
      _M0Lm2vrS1010 = _M0L7vrDiv10S1057;
      _M0Lm2vpS1011 = _M0L7vpDiv10S1054;
      _M0Lm2vmS1012 = _M0L7vmDiv10S1055;
      _M0L6_2atmpS2572 = _M0Lm7removedS1034;
      _M0Lm7removedS1034 = _M0L6_2atmpS2572 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2580 = _M0Lm2vrS1010;
    _M0L6_2atmpS2583 = _M0Lm2vrS1010;
    _M0L6_2atmpS2584 = _M0Lm2vmS1012;
    _M0L6_2atmpS2582
    = _M0L6_2atmpS2583 == _M0L6_2atmpS2584 || _M0Lm7roundUpS1049;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2581 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2582);
    _M0Lm6outputS1036 = _M0L6_2atmpS2580 + _M0L6_2atmpS2581;
  }
  _M0L6_2atmpS2588 = _M0Lm3e10S1013;
  _M0L6_2atmpS2589 = _M0Lm7removedS1034;
  _M0L3expS1059 = _M0L6_2atmpS2588 + _M0L6_2atmpS2589;
  _M0L6_2atmpS2587 = _M0Lm6outputS1036;
  _block_3667
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3667)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3667->$0 = _M0L6_2atmpS2587;
  _block_3667->$1 = _M0L3expS1059;
  return _block_3667;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1002) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1002) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1001) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1001) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1000) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1000) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS999) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS999 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS999 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS999 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS999 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS999 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS999 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS999 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS999 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS999 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS999 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS999 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS999 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS999 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS999 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS999 >= 100ull) {
    return 3;
  }
  if (_M0L1vS999 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS982) {
  int32_t _M0L6_2atmpS2488;
  int32_t _M0L6_2atmpS2487;
  int32_t _M0L4baseS981;
  int32_t _M0L5base2S983;
  int32_t _M0L6offsetS984;
  int32_t _M0L6_2atmpS2486;
  uint64_t _M0L4mul0S985;
  int32_t _M0L6_2atmpS2485;
  int32_t _M0L6_2atmpS2484;
  uint64_t _M0L4mul1S986;
  uint64_t _M0L1mS987;
  struct _M0TPB7Umul128 _M0L7_2abindS988;
  uint64_t _M0L7_2alow1S989;
  uint64_t _M0L8_2ahigh1S990;
  struct _M0TPB7Umul128 _M0L7_2abindS991;
  uint64_t _M0L7_2alow0S992;
  uint64_t _M0L8_2ahigh0S993;
  uint64_t _M0L3sumS994;
  uint64_t _M0Lm5high1S995;
  int32_t _M0L6_2atmpS2482;
  int32_t _M0L6_2atmpS2483;
  int32_t _M0L5deltaS996;
  uint64_t _M0L6_2atmpS2481;
  uint64_t _M0L6_2atmpS2473;
  int32_t _M0L6_2atmpS2480;
  uint32_t _M0L6_2atmpS2477;
  int32_t _M0L6_2atmpS2479;
  int32_t _M0L6_2atmpS2478;
  uint32_t _M0L6_2atmpS2476;
  uint32_t _M0L6_2atmpS2475;
  uint64_t _M0L6_2atmpS2474;
  uint64_t _M0L1aS997;
  uint64_t _M0L6_2atmpS2472;
  uint64_t _M0L1bS998;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2488 = _M0L1iS982 + 26;
  _M0L6_2atmpS2487 = _M0L6_2atmpS2488 - 1;
  _M0L4baseS981 = _M0L6_2atmpS2487 / 26;
  _M0L5base2S983 = _M0L4baseS981 * 26;
  _M0L6offsetS984 = _M0L5base2S983 - _M0L1iS982;
  _M0L6_2atmpS2486 = _M0L4baseS981 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S985
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2486);
  _M0L6_2atmpS2485 = _M0L4baseS981 * 2;
  _M0L6_2atmpS2484 = _M0L6_2atmpS2485 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S986
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2484);
  if (_M0L6offsetS984 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S985, _M0L4mul1S986};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS987
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS984);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS988 = _M0FPB7umul128(_M0L1mS987, _M0L4mul1S986);
  _M0L7_2alow1S989 = _M0L7_2abindS988.$0;
  _M0L8_2ahigh1S990 = _M0L7_2abindS988.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS991 = _M0FPB7umul128(_M0L1mS987, _M0L4mul0S985);
  _M0L7_2alow0S992 = _M0L7_2abindS991.$0;
  _M0L8_2ahigh0S993 = _M0L7_2abindS991.$1;
  _M0L3sumS994 = _M0L8_2ahigh0S993 + _M0L7_2alow1S989;
  _M0Lm5high1S995 = _M0L8_2ahigh1S990;
  if (_M0L3sumS994 < _M0L8_2ahigh0S993) {
    uint64_t _M0L6_2atmpS2471 = _M0Lm5high1S995;
    _M0Lm5high1S995 = _M0L6_2atmpS2471 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2482 = _M0FPB8pow5bits(_M0L5base2S983);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2483 = _M0FPB8pow5bits(_M0L1iS982);
  _M0L5deltaS996 = _M0L6_2atmpS2482 - _M0L6_2atmpS2483;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2481
  = _M0FPB13shiftright128(_M0L7_2alow0S992, _M0L3sumS994, _M0L5deltaS996);
  _M0L6_2atmpS2473 = _M0L6_2atmpS2481 + 1ull;
  _M0L6_2atmpS2480 = _M0L1iS982 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2477
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2480);
  _M0L6_2atmpS2479 = _M0L1iS982 % 16;
  _M0L6_2atmpS2478 = _M0L6_2atmpS2479 << 1;
  _M0L6_2atmpS2476 = _M0L6_2atmpS2477 >> (_M0L6_2atmpS2478 & 31);
  _M0L6_2atmpS2475 = _M0L6_2atmpS2476 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2474 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2475);
  _M0L1aS997 = _M0L6_2atmpS2473 + _M0L6_2atmpS2474;
  _M0L6_2atmpS2472 = _M0Lm5high1S995;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS998
  = _M0FPB13shiftright128(_M0L3sumS994, _M0L6_2atmpS2472, _M0L5deltaS996);
  return (struct _M0TPB8Pow5Pair){_M0L1aS997, _M0L1bS998};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS964) {
  int32_t _M0L4baseS963;
  int32_t _M0L5base2S965;
  int32_t _M0L6offsetS966;
  int32_t _M0L6_2atmpS2470;
  uint64_t _M0L4mul0S967;
  int32_t _M0L6_2atmpS2469;
  int32_t _M0L6_2atmpS2468;
  uint64_t _M0L4mul1S968;
  uint64_t _M0L1mS969;
  struct _M0TPB7Umul128 _M0L7_2abindS970;
  uint64_t _M0L7_2alow1S971;
  uint64_t _M0L8_2ahigh1S972;
  struct _M0TPB7Umul128 _M0L7_2abindS973;
  uint64_t _M0L7_2alow0S974;
  uint64_t _M0L8_2ahigh0S975;
  uint64_t _M0L3sumS976;
  uint64_t _M0Lm5high1S977;
  int32_t _M0L6_2atmpS2466;
  int32_t _M0L6_2atmpS2467;
  int32_t _M0L5deltaS978;
  uint64_t _M0L6_2atmpS2458;
  int32_t _M0L6_2atmpS2465;
  uint32_t _M0L6_2atmpS2462;
  int32_t _M0L6_2atmpS2464;
  int32_t _M0L6_2atmpS2463;
  uint32_t _M0L6_2atmpS2461;
  uint32_t _M0L6_2atmpS2460;
  uint64_t _M0L6_2atmpS2459;
  uint64_t _M0L1aS979;
  uint64_t _M0L6_2atmpS2457;
  uint64_t _M0L1bS980;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS963 = _M0L1iS964 / 26;
  _M0L5base2S965 = _M0L4baseS963 * 26;
  _M0L6offsetS966 = _M0L1iS964 - _M0L5base2S965;
  _M0L6_2atmpS2470 = _M0L4baseS963 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S967
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2470);
  _M0L6_2atmpS2469 = _M0L4baseS963 * 2;
  _M0L6_2atmpS2468 = _M0L6_2atmpS2469 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S968
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2468);
  if (_M0L6offsetS966 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S967, _M0L4mul1S968};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS969
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS966);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS970 = _M0FPB7umul128(_M0L1mS969, _M0L4mul1S968);
  _M0L7_2alow1S971 = _M0L7_2abindS970.$0;
  _M0L8_2ahigh1S972 = _M0L7_2abindS970.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS973 = _M0FPB7umul128(_M0L1mS969, _M0L4mul0S967);
  _M0L7_2alow0S974 = _M0L7_2abindS973.$0;
  _M0L8_2ahigh0S975 = _M0L7_2abindS973.$1;
  _M0L3sumS976 = _M0L8_2ahigh0S975 + _M0L7_2alow1S971;
  _M0Lm5high1S977 = _M0L8_2ahigh1S972;
  if (_M0L3sumS976 < _M0L8_2ahigh0S975) {
    uint64_t _M0L6_2atmpS2456 = _M0Lm5high1S977;
    _M0Lm5high1S977 = _M0L6_2atmpS2456 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2466 = _M0FPB8pow5bits(_M0L1iS964);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2467 = _M0FPB8pow5bits(_M0L5base2S965);
  _M0L5deltaS978 = _M0L6_2atmpS2466 - _M0L6_2atmpS2467;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2458
  = _M0FPB13shiftright128(_M0L7_2alow0S974, _M0L3sumS976, _M0L5deltaS978);
  _M0L6_2atmpS2465 = _M0L1iS964 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2462
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2465);
  _M0L6_2atmpS2464 = _M0L1iS964 % 16;
  _M0L6_2atmpS2463 = _M0L6_2atmpS2464 << 1;
  _M0L6_2atmpS2461 = _M0L6_2atmpS2462 >> (_M0L6_2atmpS2463 & 31);
  _M0L6_2atmpS2460 = _M0L6_2atmpS2461 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2459 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2460);
  _M0L1aS979 = _M0L6_2atmpS2458 + _M0L6_2atmpS2459;
  _M0L6_2atmpS2457 = _M0Lm5high1S977;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS980
  = _M0FPB13shiftright128(_M0L3sumS976, _M0L6_2atmpS2457, _M0L5deltaS978);
  return (struct _M0TPB8Pow5Pair){_M0L1aS979, _M0L1bS980};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS937,
  struct _M0TPB8Pow5Pair _M0L3mulS934,
  int32_t _M0L1jS950,
  int32_t _M0L7mmShiftS952
) {
  uint64_t _M0L7_2amul0S933;
  uint64_t _M0L7_2amul1S935;
  uint64_t _M0L1mS936;
  struct _M0TPB7Umul128 _M0L7_2abindS938;
  uint64_t _M0L5_2aloS939;
  uint64_t _M0L6_2atmpS940;
  struct _M0TPB7Umul128 _M0L7_2abindS941;
  uint64_t _M0L6_2alo2S942;
  uint64_t _M0L6_2ahi2S943;
  uint64_t _M0L3midS944;
  uint64_t _M0L6_2atmpS2455;
  uint64_t _M0L2hiS945;
  uint64_t _M0L3lo2S946;
  uint64_t _M0L6_2atmpS2453;
  uint64_t _M0L6_2atmpS2454;
  uint64_t _M0L4mid2S947;
  uint64_t _M0L6_2atmpS2452;
  uint64_t _M0L3hi2S948;
  int32_t _M0L6_2atmpS2451;
  int32_t _M0L6_2atmpS2450;
  uint64_t _M0L2vpS949;
  uint64_t _M0Lm2vmS951;
  int32_t _M0L6_2atmpS2449;
  int32_t _M0L6_2atmpS2448;
  uint64_t _M0L2vrS962;
  uint64_t _M0L6_2atmpS2447;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S933 = _M0L3mulS934.$0;
  _M0L7_2amul1S935 = _M0L3mulS934.$1;
  _M0L1mS936 = _M0L1mS937 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS938 = _M0FPB7umul128(_M0L1mS936, _M0L7_2amul0S933);
  _M0L5_2aloS939 = _M0L7_2abindS938.$0;
  _M0L6_2atmpS940 = _M0L7_2abindS938.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS941 = _M0FPB7umul128(_M0L1mS936, _M0L7_2amul1S935);
  _M0L6_2alo2S942 = _M0L7_2abindS941.$0;
  _M0L6_2ahi2S943 = _M0L7_2abindS941.$1;
  _M0L3midS944 = _M0L6_2atmpS940 + _M0L6_2alo2S942;
  if (_M0L3midS944 < _M0L6_2atmpS940) {
    _M0L6_2atmpS2455 = 1ull;
  } else {
    _M0L6_2atmpS2455 = 0ull;
  }
  _M0L2hiS945 = _M0L6_2ahi2S943 + _M0L6_2atmpS2455;
  _M0L3lo2S946 = _M0L5_2aloS939 + _M0L7_2amul0S933;
  _M0L6_2atmpS2453 = _M0L3midS944 + _M0L7_2amul1S935;
  if (_M0L3lo2S946 < _M0L5_2aloS939) {
    _M0L6_2atmpS2454 = 1ull;
  } else {
    _M0L6_2atmpS2454 = 0ull;
  }
  _M0L4mid2S947 = _M0L6_2atmpS2453 + _M0L6_2atmpS2454;
  if (_M0L4mid2S947 < _M0L3midS944) {
    _M0L6_2atmpS2452 = 1ull;
  } else {
    _M0L6_2atmpS2452 = 0ull;
  }
  _M0L3hi2S948 = _M0L2hiS945 + _M0L6_2atmpS2452;
  _M0L6_2atmpS2451 = _M0L1jS950 - 64;
  _M0L6_2atmpS2450 = _M0L6_2atmpS2451 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS949
  = _M0FPB13shiftright128(_M0L4mid2S947, _M0L3hi2S948, _M0L6_2atmpS2450);
  _M0Lm2vmS951 = 0ull;
  if (_M0L7mmShiftS952) {
    uint64_t _M0L3lo3S953 = _M0L5_2aloS939 - _M0L7_2amul0S933;
    uint64_t _M0L6_2atmpS2437 = _M0L3midS944 - _M0L7_2amul1S935;
    uint64_t _M0L6_2atmpS2438;
    uint64_t _M0L4mid3S954;
    uint64_t _M0L6_2atmpS2436;
    uint64_t _M0L3hi3S955;
    int32_t _M0L6_2atmpS2435;
    int32_t _M0L6_2atmpS2434;
    if (_M0L5_2aloS939 < _M0L3lo3S953) {
      _M0L6_2atmpS2438 = 1ull;
    } else {
      _M0L6_2atmpS2438 = 0ull;
    }
    _M0L4mid3S954 = _M0L6_2atmpS2437 - _M0L6_2atmpS2438;
    if (_M0L3midS944 < _M0L4mid3S954) {
      _M0L6_2atmpS2436 = 1ull;
    } else {
      _M0L6_2atmpS2436 = 0ull;
    }
    _M0L3hi3S955 = _M0L2hiS945 - _M0L6_2atmpS2436;
    _M0L6_2atmpS2435 = _M0L1jS950 - 64;
    _M0L6_2atmpS2434 = _M0L6_2atmpS2435 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS951
    = _M0FPB13shiftright128(_M0L4mid3S954, _M0L3hi3S955, _M0L6_2atmpS2434);
  } else {
    uint64_t _M0L3lo3S956 = _M0L5_2aloS939 + _M0L5_2aloS939;
    uint64_t _M0L6_2atmpS2445 = _M0L3midS944 + _M0L3midS944;
    uint64_t _M0L6_2atmpS2446;
    uint64_t _M0L4mid3S957;
    uint64_t _M0L6_2atmpS2443;
    uint64_t _M0L6_2atmpS2444;
    uint64_t _M0L3hi3S958;
    uint64_t _M0L3lo4S959;
    uint64_t _M0L6_2atmpS2441;
    uint64_t _M0L6_2atmpS2442;
    uint64_t _M0L4mid4S960;
    uint64_t _M0L6_2atmpS2440;
    uint64_t _M0L3hi4S961;
    int32_t _M0L6_2atmpS2439;
    if (_M0L3lo3S956 < _M0L5_2aloS939) {
      _M0L6_2atmpS2446 = 1ull;
    } else {
      _M0L6_2atmpS2446 = 0ull;
    }
    _M0L4mid3S957 = _M0L6_2atmpS2445 + _M0L6_2atmpS2446;
    _M0L6_2atmpS2443 = _M0L2hiS945 + _M0L2hiS945;
    if (_M0L4mid3S957 < _M0L3midS944) {
      _M0L6_2atmpS2444 = 1ull;
    } else {
      _M0L6_2atmpS2444 = 0ull;
    }
    _M0L3hi3S958 = _M0L6_2atmpS2443 + _M0L6_2atmpS2444;
    _M0L3lo4S959 = _M0L3lo3S956 - _M0L7_2amul0S933;
    _M0L6_2atmpS2441 = _M0L4mid3S957 - _M0L7_2amul1S935;
    if (_M0L3lo3S956 < _M0L3lo4S959) {
      _M0L6_2atmpS2442 = 1ull;
    } else {
      _M0L6_2atmpS2442 = 0ull;
    }
    _M0L4mid4S960 = _M0L6_2atmpS2441 - _M0L6_2atmpS2442;
    if (_M0L4mid3S957 < _M0L4mid4S960) {
      _M0L6_2atmpS2440 = 1ull;
    } else {
      _M0L6_2atmpS2440 = 0ull;
    }
    _M0L3hi4S961 = _M0L3hi3S958 - _M0L6_2atmpS2440;
    _M0L6_2atmpS2439 = _M0L1jS950 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS951
    = _M0FPB13shiftright128(_M0L4mid4S960, _M0L3hi4S961, _M0L6_2atmpS2439);
  }
  _M0L6_2atmpS2449 = _M0L1jS950 - 64;
  _M0L6_2atmpS2448 = _M0L6_2atmpS2449 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS962
  = _M0FPB13shiftright128(_M0L3midS944, _M0L2hiS945, _M0L6_2atmpS2448);
  _M0L6_2atmpS2447 = _M0Lm2vmS951;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS962,
                                                _M0L2vpS949,
                                                _M0L6_2atmpS2447};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS931,
  int32_t _M0L1pS932
) {
  uint64_t _M0L6_2atmpS2433;
  uint64_t _M0L6_2atmpS2432;
  uint64_t _M0L6_2atmpS2431;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2433 = 1ull << (_M0L1pS932 & 63);
  _M0L6_2atmpS2432 = _M0L6_2atmpS2433 - 1ull;
  _M0L6_2atmpS2431 = _M0L5valueS931 & _M0L6_2atmpS2432;
  return _M0L6_2atmpS2431 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS929,
  int32_t _M0L1pS930
) {
  int32_t _M0L6_2atmpS2430;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2430 = _M0FPB10pow5Factor(_M0L5valueS929);
  return _M0L6_2atmpS2430 >= _M0L1pS930;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS925) {
  uint64_t _M0L6_2atmpS2418;
  uint64_t _M0L6_2atmpS2419;
  uint64_t _M0L6_2atmpS2420;
  uint64_t _M0L6_2atmpS2421;
  int32_t _M0Lm5countS926;
  uint64_t _M0Lm5valueS927;
  uint64_t _M0L6_2atmpS2429;
  moonbit_string_t _M0L6_2atmpS2428;
  moonbit_string_t _M0L6_2atmpS3214;
  moonbit_string_t _M0L6_2atmpS2427;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2418 = _M0L5valueS925 % 5ull;
  if (_M0L6_2atmpS2418 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2419 = _M0L5valueS925 % 25ull;
  if (_M0L6_2atmpS2419 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2420 = _M0L5valueS925 % 125ull;
  if (_M0L6_2atmpS2420 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2421 = _M0L5valueS925 % 625ull;
  if (_M0L6_2atmpS2421 != 0ull) {
    return 3;
  }
  _M0Lm5countS926 = 4;
  _M0Lm5valueS927 = _M0L5valueS925 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2422 = _M0Lm5valueS927;
    if (_M0L6_2atmpS2422 > 0ull) {
      uint64_t _M0L6_2atmpS2424 = _M0Lm5valueS927;
      uint64_t _M0L6_2atmpS2423 = _M0L6_2atmpS2424 % 5ull;
      uint64_t _M0L6_2atmpS2425;
      int32_t _M0L6_2atmpS2426;
      if (_M0L6_2atmpS2423 != 0ull) {
        return _M0Lm5countS926;
      }
      _M0L6_2atmpS2425 = _M0Lm5valueS927;
      _M0Lm5valueS927 = _M0L6_2atmpS2425 / 5ull;
      _M0L6_2atmpS2426 = _M0Lm5countS926;
      _M0Lm5countS926 = _M0L6_2atmpS2426 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2429 = _M0Lm5valueS927;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2428
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2429);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3214
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_57.data, _M0L6_2atmpS2428);
  moonbit_decref(_M0L6_2atmpS2428);
  _M0L6_2atmpS2427 = _M0L6_2atmpS3214;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2427, (moonbit_string_t)moonbit_string_literal_58.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS924,
  uint64_t _M0L2hiS922,
  int32_t _M0L4distS923
) {
  int32_t _M0L6_2atmpS2417;
  uint64_t _M0L6_2atmpS2415;
  uint64_t _M0L6_2atmpS2416;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2417 = 64 - _M0L4distS923;
  _M0L6_2atmpS2415 = _M0L2hiS922 << (_M0L6_2atmpS2417 & 63);
  _M0L6_2atmpS2416 = _M0L2loS924 >> (_M0L4distS923 & 63);
  return _M0L6_2atmpS2415 | _M0L6_2atmpS2416;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS912,
  uint64_t _M0L1bS915
) {
  uint64_t _M0L3aLoS911;
  uint64_t _M0L3aHiS913;
  uint64_t _M0L3bLoS914;
  uint64_t _M0L3bHiS916;
  uint64_t _M0L1xS917;
  uint64_t _M0L6_2atmpS2413;
  uint64_t _M0L6_2atmpS2414;
  uint64_t _M0L1yS918;
  uint64_t _M0L6_2atmpS2411;
  uint64_t _M0L6_2atmpS2412;
  uint64_t _M0L1zS919;
  uint64_t _M0L6_2atmpS2409;
  uint64_t _M0L6_2atmpS2410;
  uint64_t _M0L6_2atmpS2407;
  uint64_t _M0L6_2atmpS2408;
  uint64_t _M0L1wS920;
  uint64_t _M0L2loS921;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS911 = _M0L1aS912 & 4294967295ull;
  _M0L3aHiS913 = _M0L1aS912 >> 32;
  _M0L3bLoS914 = _M0L1bS915 & 4294967295ull;
  _M0L3bHiS916 = _M0L1bS915 >> 32;
  _M0L1xS917 = _M0L3aLoS911 * _M0L3bLoS914;
  _M0L6_2atmpS2413 = _M0L3aHiS913 * _M0L3bLoS914;
  _M0L6_2atmpS2414 = _M0L1xS917 >> 32;
  _M0L1yS918 = _M0L6_2atmpS2413 + _M0L6_2atmpS2414;
  _M0L6_2atmpS2411 = _M0L3aLoS911 * _M0L3bHiS916;
  _M0L6_2atmpS2412 = _M0L1yS918 & 4294967295ull;
  _M0L1zS919 = _M0L6_2atmpS2411 + _M0L6_2atmpS2412;
  _M0L6_2atmpS2409 = _M0L3aHiS913 * _M0L3bHiS916;
  _M0L6_2atmpS2410 = _M0L1yS918 >> 32;
  _M0L6_2atmpS2407 = _M0L6_2atmpS2409 + _M0L6_2atmpS2410;
  _M0L6_2atmpS2408 = _M0L1zS919 >> 32;
  _M0L1wS920 = _M0L6_2atmpS2407 + _M0L6_2atmpS2408;
  _M0L2loS921 = _M0L1aS912 * _M0L1bS915;
  return (struct _M0TPB7Umul128){_M0L2loS921, _M0L1wS920};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS906,
  int32_t _M0L4fromS910,
  int32_t _M0L2toS908
) {
  int32_t _M0L6_2atmpS2406;
  struct _M0TPB13StringBuilder* _M0L3bufS905;
  int32_t _M0L1iS907;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2406 = Moonbit_array_length(_M0L5bytesS906);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS905 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2406);
  _M0L1iS907 = _M0L4fromS910;
  while (1) {
    if (_M0L1iS907 < _M0L2toS908) {
      int32_t _M0L6_2atmpS2404;
      int32_t _M0L6_2atmpS2403;
      int32_t _M0L6_2atmpS2405;
      if (
        _M0L1iS907 < 0 || _M0L1iS907 >= Moonbit_array_length(_M0L5bytesS906)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2404 = (int32_t)_M0L5bytesS906[_M0L1iS907];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2403 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2404);
      moonbit_incref(_M0L3bufS905);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS905, _M0L6_2atmpS2403);
      _M0L6_2atmpS2405 = _M0L1iS907 + 1;
      _M0L1iS907 = _M0L6_2atmpS2405;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS906);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS905);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS904) {
  int32_t _M0L6_2atmpS2402;
  uint32_t _M0L6_2atmpS2401;
  uint32_t _M0L6_2atmpS2400;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2402 = _M0L1eS904 * 78913;
  _M0L6_2atmpS2401 = *(uint32_t*)&_M0L6_2atmpS2402;
  _M0L6_2atmpS2400 = _M0L6_2atmpS2401 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2400;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS903) {
  int32_t _M0L6_2atmpS2399;
  uint32_t _M0L6_2atmpS2398;
  uint32_t _M0L6_2atmpS2397;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2399 = _M0L1eS903 * 732923;
  _M0L6_2atmpS2398 = *(uint32_t*)&_M0L6_2atmpS2399;
  _M0L6_2atmpS2397 = _M0L6_2atmpS2398 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2397;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS901,
  int32_t _M0L8exponentS902,
  int32_t _M0L8mantissaS899
) {
  moonbit_string_t _M0L1sS900;
  moonbit_string_t _M0L6_2atmpS3215;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS899) {
    return (moonbit_string_t)moonbit_string_literal_59.data;
  }
  if (_M0L4signS901) {
    _M0L1sS900 = (moonbit_string_t)moonbit_string_literal_60.data;
  } else {
    _M0L1sS900 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS902) {
    moonbit_string_t _M0L6_2atmpS3216;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3216
    = moonbit_add_string(_M0L1sS900, (moonbit_string_t)moonbit_string_literal_61.data);
    moonbit_decref(_M0L1sS900);
    return _M0L6_2atmpS3216;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3215
  = moonbit_add_string(_M0L1sS900, (moonbit_string_t)moonbit_string_literal_62.data);
  moonbit_decref(_M0L1sS900);
  return _M0L6_2atmpS3215;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS898) {
  int32_t _M0L6_2atmpS2396;
  uint32_t _M0L6_2atmpS2395;
  uint32_t _M0L6_2atmpS2394;
  int32_t _M0L6_2atmpS2393;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2396 = _M0L1eS898 * 1217359;
  _M0L6_2atmpS2395 = *(uint32_t*)&_M0L6_2atmpS2396;
  _M0L6_2atmpS2394 = _M0L6_2atmpS2395 >> 19;
  _M0L6_2atmpS2393 = *(int32_t*)&_M0L6_2atmpS2394;
  return _M0L6_2atmpS2393 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS897,
  struct _M0TPB6Hasher* _M0L6hasherS896
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS896, _M0L4selfS897);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS895,
  struct _M0TPB6Hasher* _M0L6hasherS894
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS894, _M0L4selfS895);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS892,
  moonbit_string_t _M0L5valueS890
) {
  int32_t _M0L7_2abindS889;
  int32_t _M0L1iS891;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS889 = Moonbit_array_length(_M0L5valueS890);
  _M0L1iS891 = 0;
  while (1) {
    if (_M0L1iS891 < _M0L7_2abindS889) {
      int32_t _M0L6_2atmpS2391 = _M0L5valueS890[_M0L1iS891];
      int32_t _M0L6_2atmpS2390 = (int32_t)_M0L6_2atmpS2391;
      uint32_t _M0L6_2atmpS2389 = *(uint32_t*)&_M0L6_2atmpS2390;
      int32_t _M0L6_2atmpS2392;
      moonbit_incref(_M0L4selfS892);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS892, _M0L6_2atmpS2389);
      _M0L6_2atmpS2392 = _M0L1iS891 + 1;
      _M0L1iS891 = _M0L6_2atmpS2392;
      continue;
    } else {
      moonbit_decref(_M0L4selfS892);
      moonbit_decref(_M0L5valueS890);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS887,
  int32_t _M0L3idxS888
) {
  int32_t _M0L6_2atmpS3217;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3217 = _M0L4selfS887[_M0L3idxS888];
  moonbit_decref(_M0L4selfS887);
  return _M0L6_2atmpS3217;
}

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS886
) {
  #line 904 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  #line 905 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPB4Iter4nextGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(_M0L4selfS886);
}

void* _M0IPC16option6OptionPB6ToJson8to__jsonGiE(int64_t _M0L4selfS883) {
  #line 287 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS883 == 4294967296ll) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    int64_t _M0L7_2aSomeS884 = _M0L4selfS883;
    int32_t _M0L8_2avalueS885 = (int32_t)_M0L7_2aSomeS884;
    void* _M0L6_2atmpS2388;
    void** _M0L6_2atmpS2387;
    struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2386;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L6_2atmpS2388 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_2avalueS885);
    _M0L6_2atmpS2387 = (void**)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS2387[0] = _M0L6_2atmpS2388;
    _M0L6_2atmpS2386
    = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
    Moonbit_object_header(_M0L6_2atmpS2386)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2386->$0 = _M0L6_2atmpS2387;
    _M0L6_2atmpS2386->$1 = 1;
    #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json5array(_M0L6_2atmpS2386);
  }
}

void* _M0IPB3MapPB6ToJson8to__jsonGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS875
) {
  int32_t _M0L8capacityS2385;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS874;
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5_2aitS876;
  void* _block_3672;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L8capacityS2385 = _M0L4selfS875->$2;
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6objectS874 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L8capacityS2385);
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L5_2aitS876
  = _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS875);
  while (1) {
    struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS877;
    moonbit_incref(_M0L5_2aitS876);
    #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L7_2abindS877
    = _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L5_2aitS876);
    if (_M0L7_2abindS877 == 0) {
      if (_M0L7_2abindS877) {
        moonbit_decref(_M0L7_2abindS877);
      }
      moonbit_decref(_M0L5_2aitS876);
    } else {
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS878 =
        _M0L7_2abindS877;
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4_2axS879 =
        _M0L7_2aSomeS878;
      moonbit_string_t _M0L8_2afieldS3219 = _M0L4_2axS879->$0;
      moonbit_string_t _M0L4_2akS880 = _M0L8_2afieldS3219;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3218 =
        _M0L4_2axS879->$1;
      int32_t _M0L6_2acntS3523 = Moonbit_object_header(_M0L4_2axS879)->rc;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L4_2avS881;
      moonbit_string_t _M0L6_2atmpS2383;
      void* _M0L6_2atmpS2384;
      if (_M0L6_2acntS3523 > 1) {
        int32_t _M0L11_2anew__cntS3524 = _M0L6_2acntS3523 - 1;
        Moonbit_object_header(_M0L4_2axS879)->rc = _M0L11_2anew__cntS3524;
        moonbit_incref(_M0L8_2afieldS3218);
        moonbit_incref(_M0L4_2akS880);
      } else if (_M0L6_2acntS3523 == 1) {
        #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        moonbit_free(_M0L4_2axS879);
      }
      _M0L4_2avS881 = _M0L8_2afieldS3218;
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2383
      = _M0IPC16string6StringPB4Show10to__string(_M0L4_2akS880);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2384
      = _M0IP48clawteam8clawteam8internal3lru5EntryPB6ToJson8to__jsonGiE(_M0L4_2avS881);
      moonbit_incref(_M0L6objectS874);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L6objectS874, _M0L6_2atmpS2383, _M0L6_2atmpS2384);
      continue;
    }
    break;
  }
  _block_3672 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3672)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3672)->$0 = _M0L6objectS874;
  return _block_3672;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS873) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS873;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS872) {
  double _M0L6_2atmpS2381;
  moonbit_string_t _M0L6_2atmpS2382;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2381 = (double)_M0L4selfS872;
  _M0L6_2atmpS2382 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2381, _M0L6_2atmpS2382);
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS871) {
  void* _block_3673;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3673 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3673)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3673)->$0 = _M0L6objectS871;
  return _block_3673;
}

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS870
) {
  #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 606 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS870);
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS855
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3220;
  int32_t _M0L6_2acntS3525;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2374;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS854;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__* _closure_3674;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2369;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3220 = _M0L4selfS855->$5;
  _M0L6_2acntS3525 = Moonbit_object_header(_M0L4selfS855)->rc;
  if (_M0L6_2acntS3525 > 1) {
    int32_t _M0L11_2anew__cntS3527 = _M0L6_2acntS3525 - 1;
    Moonbit_object_header(_M0L4selfS855)->rc = _M0L11_2anew__cntS3527;
    if (_M0L8_2afieldS3220) {
      moonbit_incref(_M0L8_2afieldS3220);
    }
  } else if (_M0L6_2acntS3525 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3526 = _M0L4selfS855->$0;
    moonbit_decref(_M0L8_2afieldS3526);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS855);
  }
  _M0L4headS2374 = _M0L8_2afieldS3220;
  _M0L11curr__entryS854
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS854)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS854->$0 = _M0L4headS2374;
  _closure_3674
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__));
  Moonbit_object_header(_closure_3674)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__, $0) >> 2, 1, 0);
  _closure_3674->code = &_M0MPB3Map4iterGsRPB4JsonEC2370l591;
  _closure_3674->$0 = _M0L11curr__entryS854;
  _M0L6_2atmpS2369 = (struct _M0TWEOUsRPB4JsonE*)_closure_3674;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2369);
}

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS863
) {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3221;
  int32_t _M0L6_2acntS3528;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4headS2380;
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE* _M0L11curr__entryS862;
  struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__* _closure_3675;
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2375;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3221 = _M0L4selfS863->$5;
  _M0L6_2acntS3528 = Moonbit_object_header(_M0L4selfS863)->rc;
  if (_M0L6_2acntS3528 > 1) {
    int32_t _M0L11_2anew__cntS3530 = _M0L6_2acntS3528 - 1;
    Moonbit_object_header(_M0L4selfS863)->rc = _M0L11_2anew__cntS3530;
    if (_M0L8_2afieldS3221) {
      moonbit_incref(_M0L8_2afieldS3221);
    }
  } else if (_M0L6_2acntS3528 == 1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3529 =
      _M0L4selfS863->$0;
    moonbit_decref(_M0L8_2afieldS3529);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS863);
  }
  _M0L4headS2380 = _M0L8_2afieldS3221;
  _M0L11curr__entryS862
  = (struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE));
  Moonbit_object_header(_M0L11curr__entryS862)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS862->$0 = _M0L4headS2380;
  _closure_3675
  = (struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__*)moonbit_malloc(sizeof(struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__));
  Moonbit_object_header(_closure_3675)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__, $0) >> 2, 1, 0);
  _closure_3675->code
  = &_M0MPB3Map4iterGsRP48clawteam8clawteam8internal3lru5EntryGiEEC2376l591;
  _closure_3675->$0 = _M0L11curr__entryS862;
  _M0L6_2atmpS2375
  = (struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_closure_3675;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(_M0L6_2atmpS2375);
}

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal3lru5EntryGiEEC2376l591(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aenvS2377
) {
  struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__* _M0L14_2acasted__envS2378;
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE* _M0L8_2afieldS3227;
  int32_t _M0L6_2acntS3531;
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE* _M0L11curr__entryS862;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3226;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS864;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2378
  = (struct _M0R112Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2flru_2fEntry_5bInt_5d_5d_7c_2eanon__u2376__l591__*)_M0L6_2aenvS2377;
  _M0L8_2afieldS3227 = _M0L14_2acasted__envS2378->$0;
  _M0L6_2acntS3531 = Moonbit_object_header(_M0L14_2acasted__envS2378)->rc;
  if (_M0L6_2acntS3531 > 1) {
    int32_t _M0L11_2anew__cntS3532 = _M0L6_2acntS3531 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2378)->rc
    = _M0L11_2anew__cntS3532;
    moonbit_incref(_M0L8_2afieldS3227);
  } else if (_M0L6_2acntS3531 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2378);
  }
  _M0L11curr__entryS862 = _M0L8_2afieldS3227;
  _M0L8_2afieldS3226 = _M0L11curr__entryS862->$0;
  _M0L7_2abindS864 = _M0L8_2afieldS3226;
  if (_M0L7_2abindS864 == 0) {
    moonbit_decref(_M0L11curr__entryS862);
    return 0;
  } else {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS865 =
      _M0L7_2abindS864;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4_2axS866 =
      _M0L7_2aSomeS865;
    moonbit_string_t _M0L8_2afieldS3225 = _M0L4_2axS866->$4;
    moonbit_string_t _M0L6_2akeyS867 = _M0L8_2afieldS3225;
    struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3224 =
      _M0L4_2axS866->$5;
    struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2avalueS868 =
      _M0L8_2afieldS3224;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3223 =
      _M0L4_2axS866->$1;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2anextS869 =
      _M0L8_2afieldS3223;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3222 =
      _M0L11curr__entryS862->$0;
    struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2atupleS2379;
    if (_M0L7_2anextS869) {
      moonbit_incref(_M0L7_2anextS869);
    }
    moonbit_incref(_M0L8_2avalueS868);
    moonbit_incref(_M0L6_2akeyS867);
    if (_M0L6_2aoldS3222) {
      moonbit_decref(_M0L6_2aoldS3222);
    }
    _M0L11curr__entryS862->$0 = _M0L7_2anextS869;
    moonbit_decref(_M0L11curr__entryS862);
    _M0L8_2atupleS2379
    = (struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE));
    Moonbit_object_header(_M0L8_2atupleS2379)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2379->$0 = _M0L6_2akeyS867;
    _M0L8_2atupleS2379->$1 = _M0L8_2avalueS868;
    return _M0L8_2atupleS2379;
  }
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2370l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2371
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__* _M0L14_2acasted__envS2372;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3233;
  int32_t _M0L6_2acntS3533;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS854;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3232;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS856;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2372
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2370__l591__*)_M0L6_2aenvS2371;
  _M0L8_2afieldS3233 = _M0L14_2acasted__envS2372->$0;
  _M0L6_2acntS3533 = Moonbit_object_header(_M0L14_2acasted__envS2372)->rc;
  if (_M0L6_2acntS3533 > 1) {
    int32_t _M0L11_2anew__cntS3534 = _M0L6_2acntS3533 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2372)->rc
    = _M0L11_2anew__cntS3534;
    moonbit_incref(_M0L8_2afieldS3233);
  } else if (_M0L6_2acntS3533 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2372);
  }
  _M0L11curr__entryS854 = _M0L8_2afieldS3233;
  _M0L8_2afieldS3232 = _M0L11curr__entryS854->$0;
  _M0L7_2abindS856 = _M0L8_2afieldS3232;
  if (_M0L7_2abindS856 == 0) {
    moonbit_decref(_M0L11curr__entryS854);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS857 = _M0L7_2abindS856;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS858 = _M0L7_2aSomeS857;
    moonbit_string_t _M0L8_2afieldS3231 = _M0L4_2axS858->$4;
    moonbit_string_t _M0L6_2akeyS859 = _M0L8_2afieldS3231;
    void* _M0L8_2afieldS3230 = _M0L4_2axS858->$5;
    void* _M0L8_2avalueS860 = _M0L8_2afieldS3230;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3229 = _M0L4_2axS858->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS861 = _M0L8_2afieldS3229;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3228 =
      _M0L11curr__entryS854->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2373;
    if (_M0L7_2anextS861) {
      moonbit_incref(_M0L7_2anextS861);
    }
    moonbit_incref(_M0L8_2avalueS860);
    moonbit_incref(_M0L6_2akeyS859);
    if (_M0L6_2aoldS3228) {
      moonbit_decref(_M0L6_2aoldS3228);
    }
    _M0L11curr__entryS854->$0 = _M0L7_2anextS861;
    moonbit_decref(_M0L11curr__entryS854);
    _M0L8_2atupleS2373
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2373)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2373->$0 = _M0L6_2akeyS859;
    _M0L8_2atupleS2373->$1 = _M0L8_2avalueS860;
    return _M0L8_2atupleS2373;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS853
) {
  int32_t _M0L8_2afieldS3234;
  int32_t _M0L4sizeS2368;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3234 = _M0L4selfS853->$1;
  moonbit_decref(_M0L4selfS853);
  _M0L4sizeS2368 = _M0L8_2afieldS3234;
  return _M0L4sizeS2368 == 0;
}

int32_t _M0MPB3Map6lengthGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS852
) {
  int32_t _M0L8_2afieldS3235;
  #line 528 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3235 = _M0L4selfS852->$1;
  moonbit_decref(_M0L4selfS852);
  return _M0L8_2afieldS3235;
}

int32_t _M0MPB3Map6removeGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS850,
  moonbit_string_t _M0L3keyS851
) {
  int32_t _M0L6_2atmpS2367;
  #line 417 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS851);
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2367 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS851);
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS850, _M0L3keyS851, _M0L6_2atmpS2367);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS844,
  moonbit_string_t _M0L3keyS848,
  int32_t _M0L4hashS847
) {
  int32_t _M0L14capacity__maskS2366;
  int32_t _M0L6_2atmpS2365;
  int32_t _M0L1iS841;
  int32_t _M0L3idxS842;
  #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2366 = _M0L4selfS844->$3;
  _M0L6_2atmpS2365 = _M0L4hashS847 & _M0L14capacity__maskS2366;
  _M0L1iS841 = 0;
  _M0L3idxS842 = _M0L6_2atmpS2365;
  while (1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3240 =
      _M0L4selfS844->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2364 =
      _M0L8_2afieldS3240;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3239;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS843;
    if (
      _M0L3idxS842 < 0
      || _M0L3idxS842 >= Moonbit_array_length(_M0L7entriesS2364)
    ) {
      #line 428 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3239
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2364[
        _M0L3idxS842
      ];
    _M0L7_2abindS843 = _M0L6_2atmpS3239;
    if (_M0L7_2abindS843 == 0) {
      if (_M0L7_2abindS843) {
        moonbit_incref(_M0L7_2abindS843);
      }
      moonbit_decref(_M0L3keyS848);
      moonbit_decref(_M0L4selfS844);
      if (_M0L7_2abindS843) {
        moonbit_decref(_M0L7_2abindS843);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS845 =
        _M0L7_2abindS843;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2aentryS846 =
        _M0L7_2aSomeS845;
      int32_t _M0L4hashS2356 = _M0L8_2aentryS846->$3;
      int32_t _if__result_3677;
      int32_t _M0L8_2afieldS3236;
      int32_t _M0L3pslS2359;
      int32_t _M0L6_2atmpS2360;
      int32_t _M0L6_2atmpS2362;
      int32_t _M0L14capacity__maskS2363;
      int32_t _M0L6_2atmpS2361;
      if (_M0L4hashS2356 == _M0L4hashS847) {
        moonbit_string_t _M0L8_2afieldS3238 = _M0L8_2aentryS846->$4;
        moonbit_string_t _M0L3keyS2355 = _M0L8_2afieldS3238;
        int32_t _M0L6_2atmpS3237;
        #line 429 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3237
        = moonbit_val_array_equal(_M0L3keyS2355, _M0L3keyS848);
        _if__result_3677 = _M0L6_2atmpS3237;
      } else {
        _if__result_3677 = 0;
      }
      if (_if__result_3677) {
        int32_t _M0L4sizeS2358;
        int32_t _M0L6_2atmpS2357;
        moonbit_incref(_M0L8_2aentryS846);
        moonbit_decref(_M0L3keyS848);
        moonbit_incref(_M0L4selfS844);
        #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS844, _M0L8_2aentryS846);
        moonbit_incref(_M0L4selfS844);
        #line 431 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS844, _M0L3idxS842);
        _M0L4sizeS2358 = _M0L4selfS844->$1;
        _M0L6_2atmpS2357 = _M0L4sizeS2358 - 1;
        _M0L4selfS844->$1 = _M0L6_2atmpS2357;
        moonbit_decref(_M0L4selfS844);
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS846);
      }
      _M0L8_2afieldS3236 = _M0L8_2aentryS846->$2;
      moonbit_decref(_M0L8_2aentryS846);
      _M0L3pslS2359 = _M0L8_2afieldS3236;
      if (_M0L1iS841 > _M0L3pslS2359) {
        moonbit_decref(_M0L3keyS848);
        moonbit_decref(_M0L4selfS844);
        break;
      }
      _M0L6_2atmpS2360 = _M0L1iS841 + 1;
      _M0L6_2atmpS2362 = _M0L3idxS842 + 1;
      _M0L14capacity__maskS2363 = _M0L4selfS844->$3;
      _M0L6_2atmpS2361 = _M0L6_2atmpS2362 & _M0L14capacity__maskS2363;
      _M0L1iS841 = _M0L6_2atmpS2360;
      _M0L3idxS842 = _M0L6_2atmpS2361;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS833,
  int32_t _M0L3idxS840
) {
  int32_t _M0L3idxS831;
  #line 470 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3idxS831 = _M0L3idxS840;
  shift__back_839:;
  while (1) {
    int32_t _M0L6_2atmpS2353 = _M0L3idxS831 + 1;
    int32_t _M0L14capacity__maskS2354 = _M0L4selfS833->$3;
    int32_t _M0L4nextS832 = _M0L6_2atmpS2353 & _M0L14capacity__maskS2354;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3244 =
      _M0L4selfS833->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2352 =
      _M0L8_2afieldS3244;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3243;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS835;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3242;
    int32_t _M0L6_2acntS3535;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2348;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2349;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3241;
    if (
      _M0L4nextS832 < 0
      || _M0L4nextS832 >= Moonbit_array_length(_M0L7entriesS2352)
    ) {
      #line 472 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3243
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2352[
        _M0L4nextS832
      ];
    _M0L7_2abindS835 = _M0L6_2atmpS3243;
    if (_M0L7_2abindS835 == 0) {
      goto join_834;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS836 =
        _M0L7_2abindS835;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4_2axS837 =
        _M0L7_2aSomeS836;
      int32_t _M0L4_2axS838 = _M0L4_2axS837->$2;
      switch (_M0L4_2axS838) {
        case 0: {
          goto join_834;
          break;
        }
        default: {
          int32_t _M0L3pslS2351 = _M0L4_2axS837->$2;
          int32_t _M0L6_2atmpS2350 = _M0L3pslS2351 - 1;
          _M0L4_2axS837->$2 = _M0L6_2atmpS2350;
          moonbit_incref(_M0L4_2axS837);
          moonbit_incref(_M0L4selfS833);
          #line 476 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS833, _M0L4_2axS837, _M0L3idxS831);
          _M0L3idxS831 = _M0L4nextS832;
          goto shift__back_839;
          break;
        }
      }
    }
    goto joinlet_3679;
    join_834:;
    _M0L8_2afieldS3242 = _M0L4selfS833->$0;
    _M0L6_2acntS3535 = Moonbit_object_header(_M0L4selfS833)->rc;
    if (_M0L6_2acntS3535 > 1) {
      int32_t _M0L11_2anew__cntS3537 = _M0L6_2acntS3535 - 1;
      Moonbit_object_header(_M0L4selfS833)->rc = _M0L11_2anew__cntS3537;
      moonbit_incref(_M0L8_2afieldS3242);
    } else if (_M0L6_2acntS3535 == 1) {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3536 =
        _M0L4selfS833->$5;
      if (_M0L8_2afieldS3536) {
        moonbit_decref(_M0L8_2afieldS3536);
      }
      #line 473 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_free(_M0L4selfS833);
    }
    _M0L7entriesS2348 = _M0L8_2afieldS3242;
    _M0L6_2atmpS2349 = 0;
    if (
      _M0L3idxS831 < 0
      || _M0L3idxS831 >= Moonbit_array_length(_M0L7entriesS2348)
    ) {
      #line 473 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3241
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2348[
        _M0L3idxS831
      ];
    if (_M0L6_2aoldS3241) {
      moonbit_decref(_M0L6_2aoldS3241);
    }
    _M0L7entriesS2348[_M0L3idxS831] = _M0L6_2atmpS2349;
    moonbit_decref(_M0L7entriesS2348);
    joinlet_3679:;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS827,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5entryS826
) {
  int32_t _M0L7_2abindS825;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3247;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS828;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS825 = _M0L5entryS826->$0;
  switch (_M0L7_2abindS825) {
    case -1: {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3249 =
        _M0L5entryS826->$1;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4nextS2341 =
        _M0L8_2afieldS3249;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3248 =
        _M0L4selfS827->$5;
      if (_M0L4nextS2341) {
        moonbit_incref(_M0L4nextS2341);
      }
      if (_M0L6_2aoldS3248) {
        moonbit_decref(_M0L6_2aoldS3248);
      }
      _M0L4selfS827->$5 = _M0L4nextS2341;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3253 =
        _M0L4selfS827->$0;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2345 =
        _M0L8_2afieldS3253;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3252;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2344;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2342;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3251;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4nextS2343;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3250;
      if (
        _M0L7_2abindS825 < 0
        || _M0L7_2abindS825 >= Moonbit_array_length(_M0L7entriesS2345)
      ) {
        #line 461 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3252
      = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2345[
          _M0L7_2abindS825
        ];
      _M0L6_2atmpS2344 = _M0L6_2atmpS3252;
      if (_M0L6_2atmpS2344) {
        moonbit_incref(_M0L6_2atmpS2344);
      }
      #line 461 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2342
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE(_M0L6_2atmpS2344);
      _M0L8_2afieldS3251 = _M0L5entryS826->$1;
      _M0L4nextS2343 = _M0L8_2afieldS3251;
      _M0L6_2aoldS3250 = _M0L6_2atmpS2342->$1;
      if (_M0L4nextS2343) {
        moonbit_incref(_M0L4nextS2343);
      }
      if (_M0L6_2aoldS3250) {
        moonbit_decref(_M0L6_2aoldS3250);
      }
      _M0L6_2atmpS2342->$1 = _M0L4nextS2343;
      moonbit_decref(_M0L6_2atmpS2342);
      break;
    }
  }
  _M0L8_2afieldS3247 = _M0L5entryS826->$1;
  _M0L7_2abindS828 = _M0L8_2afieldS3247;
  if (_M0L7_2abindS828 == 0) {
    int32_t _M0L8_2afieldS3245 = _M0L5entryS826->$0;
    int32_t _M0L4prevS2346;
    moonbit_decref(_M0L5entryS826);
    _M0L4prevS2346 = _M0L8_2afieldS3245;
    _M0L4selfS827->$6 = _M0L4prevS2346;
    moonbit_decref(_M0L4selfS827);
  } else {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS829;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2anextS830;
    int32_t _M0L8_2afieldS3246;
    int32_t _M0L4prevS2347;
    if (_M0L7_2abindS828) {
      moonbit_incref(_M0L7_2abindS828);
    }
    moonbit_decref(_M0L4selfS827);
    _M0L7_2aSomeS829 = _M0L7_2abindS828;
    _M0L7_2anextS830 = _M0L7_2aSomeS829;
    _M0L8_2afieldS3246 = _M0L5entryS826->$0;
    moonbit_decref(_M0L5entryS826);
    _M0L4prevS2347 = _M0L8_2afieldS3246;
    _M0L7_2anextS830->$0 = _M0L4prevS2347;
    moonbit_decref(_M0L7_2anextS830);
  }
  return 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS803,
  int32_t _M0L3keyS799
) {
  int32_t _M0L4hashS798;
  int32_t _M0L14capacity__maskS2312;
  int32_t _M0L6_2atmpS2311;
  int32_t _M0L1iS800;
  int32_t _M0L3idxS801;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS798 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS799);
  _M0L14capacity__maskS2312 = _M0L4selfS803->$3;
  _M0L6_2atmpS2311 = _M0L4hashS798 & _M0L14capacity__maskS2312;
  _M0L1iS800 = 0;
  _M0L3idxS801 = _M0L6_2atmpS2311;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3257 =
      _M0L4selfS803->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2310 =
      _M0L8_2afieldS3257;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3256;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS802;
    if (
      _M0L3idxS801 < 0
      || _M0L3idxS801 >= Moonbit_array_length(_M0L7entriesS2310)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3256
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2310[
        _M0L3idxS801
      ];
    _M0L7_2abindS802 = _M0L6_2atmpS3256;
    if (_M0L7_2abindS802 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2299;
      if (_M0L7_2abindS802) {
        moonbit_incref(_M0L7_2abindS802);
      }
      moonbit_decref(_M0L4selfS803);
      if (_M0L7_2abindS802) {
        moonbit_decref(_M0L7_2abindS802);
      }
      _M0L6_2atmpS2299 = 0;
      return _M0L6_2atmpS2299;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS804 =
        _M0L7_2abindS802;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS805 =
        _M0L7_2aSomeS804;
      int32_t _M0L4hashS2301 = _M0L8_2aentryS805->$3;
      int32_t _if__result_3681;
      int32_t _M0L8_2afieldS3254;
      int32_t _M0L3pslS2304;
      int32_t _M0L6_2atmpS2306;
      int32_t _M0L6_2atmpS2308;
      int32_t _M0L14capacity__maskS2309;
      int32_t _M0L6_2atmpS2307;
      if (_M0L4hashS2301 == _M0L4hashS798) {
        int32_t _M0L3keyS2300 = _M0L8_2aentryS805->$4;
        _if__result_3681 = _M0L3keyS2300 == _M0L3keyS799;
      } else {
        _if__result_3681 = 0;
      }
      if (_if__result_3681) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3255;
        int32_t _M0L6_2acntS3538;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2303;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2302;
        moonbit_incref(_M0L8_2aentryS805);
        moonbit_decref(_M0L4selfS803);
        _M0L8_2afieldS3255 = _M0L8_2aentryS805->$5;
        _M0L6_2acntS3538 = Moonbit_object_header(_M0L8_2aentryS805)->rc;
        if (_M0L6_2acntS3538 > 1) {
          int32_t _M0L11_2anew__cntS3540 = _M0L6_2acntS3538 - 1;
          Moonbit_object_header(_M0L8_2aentryS805)->rc
          = _M0L11_2anew__cntS3540;
          moonbit_incref(_M0L8_2afieldS3255);
        } else if (_M0L6_2acntS3538 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3539 =
            _M0L8_2aentryS805->$1;
          if (_M0L8_2afieldS3539) {
            moonbit_decref(_M0L8_2afieldS3539);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS805);
        }
        _M0L5valueS2303 = _M0L8_2afieldS3255;
        _M0L6_2atmpS2302 = _M0L5valueS2303;
        return _M0L6_2atmpS2302;
      } else {
        moonbit_incref(_M0L8_2aentryS805);
      }
      _M0L8_2afieldS3254 = _M0L8_2aentryS805->$2;
      moonbit_decref(_M0L8_2aentryS805);
      _M0L3pslS2304 = _M0L8_2afieldS3254;
      if (_M0L1iS800 > _M0L3pslS2304) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2305;
        moonbit_decref(_M0L4selfS803);
        _M0L6_2atmpS2305 = 0;
        return _M0L6_2atmpS2305;
      }
      _M0L6_2atmpS2306 = _M0L1iS800 + 1;
      _M0L6_2atmpS2308 = _M0L3idxS801 + 1;
      _M0L14capacity__maskS2309 = _M0L4selfS803->$3;
      _M0L6_2atmpS2307 = _M0L6_2atmpS2308 & _M0L14capacity__maskS2309;
      _M0L1iS800 = _M0L6_2atmpS2306;
      _M0L3idxS801 = _M0L6_2atmpS2307;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS812,
  moonbit_string_t _M0L3keyS808
) {
  int32_t _M0L4hashS807;
  int32_t _M0L14capacity__maskS2326;
  int32_t _M0L6_2atmpS2325;
  int32_t _M0L1iS809;
  int32_t _M0L3idxS810;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS808);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS807 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS808);
  _M0L14capacity__maskS2326 = _M0L4selfS812->$3;
  _M0L6_2atmpS2325 = _M0L4hashS807 & _M0L14capacity__maskS2326;
  _M0L1iS809 = 0;
  _M0L3idxS810 = _M0L6_2atmpS2325;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3263 =
      _M0L4selfS812->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2324 =
      _M0L8_2afieldS3263;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3262;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS811;
    if (
      _M0L3idxS810 < 0
      || _M0L3idxS810 >= Moonbit_array_length(_M0L7entriesS2324)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3262
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2324[
        _M0L3idxS810
      ];
    _M0L7_2abindS811 = _M0L6_2atmpS3262;
    if (_M0L7_2abindS811 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2313;
      if (_M0L7_2abindS811) {
        moonbit_incref(_M0L7_2abindS811);
      }
      moonbit_decref(_M0L4selfS812);
      if (_M0L7_2abindS811) {
        moonbit_decref(_M0L7_2abindS811);
      }
      moonbit_decref(_M0L3keyS808);
      _M0L6_2atmpS2313 = 0;
      return _M0L6_2atmpS2313;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS813 =
        _M0L7_2abindS811;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS814 =
        _M0L7_2aSomeS813;
      int32_t _M0L4hashS2315 = _M0L8_2aentryS814->$3;
      int32_t _if__result_3683;
      int32_t _M0L8_2afieldS3258;
      int32_t _M0L3pslS2318;
      int32_t _M0L6_2atmpS2320;
      int32_t _M0L6_2atmpS2322;
      int32_t _M0L14capacity__maskS2323;
      int32_t _M0L6_2atmpS2321;
      if (_M0L4hashS2315 == _M0L4hashS807) {
        moonbit_string_t _M0L8_2afieldS3261 = _M0L8_2aentryS814->$4;
        moonbit_string_t _M0L3keyS2314 = _M0L8_2afieldS3261;
        int32_t _M0L6_2atmpS3260;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3260
        = moonbit_val_array_equal(_M0L3keyS2314, _M0L3keyS808);
        _if__result_3683 = _M0L6_2atmpS3260;
      } else {
        _if__result_3683 = 0;
      }
      if (_if__result_3683) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3259;
        int32_t _M0L6_2acntS3541;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2317;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2316;
        moonbit_incref(_M0L8_2aentryS814);
        moonbit_decref(_M0L4selfS812);
        moonbit_decref(_M0L3keyS808);
        _M0L8_2afieldS3259 = _M0L8_2aentryS814->$5;
        _M0L6_2acntS3541 = Moonbit_object_header(_M0L8_2aentryS814)->rc;
        if (_M0L6_2acntS3541 > 1) {
          int32_t _M0L11_2anew__cntS3544 = _M0L6_2acntS3541 - 1;
          Moonbit_object_header(_M0L8_2aentryS814)->rc
          = _M0L11_2anew__cntS3544;
          moonbit_incref(_M0L8_2afieldS3259);
        } else if (_M0L6_2acntS3541 == 1) {
          moonbit_string_t _M0L8_2afieldS3543 = _M0L8_2aentryS814->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3542;
          moonbit_decref(_M0L8_2afieldS3543);
          _M0L8_2afieldS3542 = _M0L8_2aentryS814->$1;
          if (_M0L8_2afieldS3542) {
            moonbit_decref(_M0L8_2afieldS3542);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS814);
        }
        _M0L5valueS2317 = _M0L8_2afieldS3259;
        _M0L6_2atmpS2316 = _M0L5valueS2317;
        return _M0L6_2atmpS2316;
      } else {
        moonbit_incref(_M0L8_2aentryS814);
      }
      _M0L8_2afieldS3258 = _M0L8_2aentryS814->$2;
      moonbit_decref(_M0L8_2aentryS814);
      _M0L3pslS2318 = _M0L8_2afieldS3258;
      if (_M0L1iS809 > _M0L3pslS2318) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2319;
        moonbit_decref(_M0L4selfS812);
        moonbit_decref(_M0L3keyS808);
        _M0L6_2atmpS2319 = 0;
        return _M0L6_2atmpS2319;
      }
      _M0L6_2atmpS2320 = _M0L1iS809 + 1;
      _M0L6_2atmpS2322 = _M0L3idxS810 + 1;
      _M0L14capacity__maskS2323 = _M0L4selfS812->$3;
      _M0L6_2atmpS2321 = _M0L6_2atmpS2322 & _M0L14capacity__maskS2323;
      _M0L1iS809 = _M0L6_2atmpS2320;
      _M0L3idxS810 = _M0L6_2atmpS2321;
      continue;
    }
    break;
  }
}

struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0MPB3Map3getGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS821,
  moonbit_string_t _M0L3keyS817
) {
  int32_t _M0L4hashS816;
  int32_t _M0L14capacity__maskS2340;
  int32_t _M0L6_2atmpS2339;
  int32_t _M0L1iS818;
  int32_t _M0L3idxS819;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS817);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS816 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS817);
  _M0L14capacity__maskS2340 = _M0L4selfS821->$3;
  _M0L6_2atmpS2339 = _M0L4hashS816 & _M0L14capacity__maskS2340;
  _M0L1iS818 = 0;
  _M0L3idxS819 = _M0L6_2atmpS2339;
  while (1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3269 =
      _M0L4selfS821->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2338 =
      _M0L8_2afieldS3269;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3268;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS820;
    if (
      _M0L3idxS819 < 0
      || _M0L3idxS819 >= Moonbit_array_length(_M0L7entriesS2338)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3268
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2338[
        _M0L3idxS819
      ];
    _M0L7_2abindS820 = _M0L6_2atmpS3268;
    if (_M0L7_2abindS820 == 0) {
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L6_2atmpS2327;
      if (_M0L7_2abindS820) {
        moonbit_incref(_M0L7_2abindS820);
      }
      moonbit_decref(_M0L4selfS821);
      if (_M0L7_2abindS820) {
        moonbit_decref(_M0L7_2abindS820);
      }
      moonbit_decref(_M0L3keyS817);
      _M0L6_2atmpS2327 = 0;
      return _M0L6_2atmpS2327;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS822 =
        _M0L7_2abindS820;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2aentryS823 =
        _M0L7_2aSomeS822;
      int32_t _M0L4hashS2329 = _M0L8_2aentryS823->$3;
      int32_t _if__result_3685;
      int32_t _M0L8_2afieldS3264;
      int32_t _M0L3pslS2332;
      int32_t _M0L6_2atmpS2334;
      int32_t _M0L6_2atmpS2336;
      int32_t _M0L14capacity__maskS2337;
      int32_t _M0L6_2atmpS2335;
      if (_M0L4hashS2329 == _M0L4hashS816) {
        moonbit_string_t _M0L8_2afieldS3267 = _M0L8_2aentryS823->$4;
        moonbit_string_t _M0L3keyS2328 = _M0L8_2afieldS3267;
        int32_t _M0L6_2atmpS3266;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3266
        = moonbit_val_array_equal(_M0L3keyS2328, _M0L3keyS817);
        _if__result_3685 = _M0L6_2atmpS3266;
      } else {
        _if__result_3685 = 0;
      }
      if (_if__result_3685) {
        struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3265;
        int32_t _M0L6_2acntS3545;
        struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L5valueS2331;
        struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L6_2atmpS2330;
        moonbit_incref(_M0L8_2aentryS823);
        moonbit_decref(_M0L4selfS821);
        moonbit_decref(_M0L3keyS817);
        _M0L8_2afieldS3265 = _M0L8_2aentryS823->$5;
        _M0L6_2acntS3545 = Moonbit_object_header(_M0L8_2aentryS823)->rc;
        if (_M0L6_2acntS3545 > 1) {
          int32_t _M0L11_2anew__cntS3548 = _M0L6_2acntS3545 - 1;
          Moonbit_object_header(_M0L8_2aentryS823)->rc
          = _M0L11_2anew__cntS3548;
          moonbit_incref(_M0L8_2afieldS3265);
        } else if (_M0L6_2acntS3545 == 1) {
          moonbit_string_t _M0L8_2afieldS3547 = _M0L8_2aentryS823->$4;
          struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3546;
          moonbit_decref(_M0L8_2afieldS3547);
          _M0L8_2afieldS3546 = _M0L8_2aentryS823->$1;
          if (_M0L8_2afieldS3546) {
            moonbit_decref(_M0L8_2afieldS3546);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS823);
        }
        _M0L5valueS2331 = _M0L8_2afieldS3265;
        _M0L6_2atmpS2330 = _M0L5valueS2331;
        return _M0L6_2atmpS2330;
      } else {
        moonbit_incref(_M0L8_2aentryS823);
      }
      _M0L8_2afieldS3264 = _M0L8_2aentryS823->$2;
      moonbit_decref(_M0L8_2aentryS823);
      _M0L3pslS2332 = _M0L8_2afieldS3264;
      if (_M0L1iS818 > _M0L3pslS2332) {
        struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L6_2atmpS2333;
        moonbit_decref(_M0L4selfS821);
        moonbit_decref(_M0L3keyS817);
        _M0L6_2atmpS2333 = 0;
        return _M0L6_2atmpS2333;
      }
      _M0L6_2atmpS2334 = _M0L1iS818 + 1;
      _M0L6_2atmpS2336 = _M0L3idxS819 + 1;
      _M0L14capacity__maskS2337 = _M0L4selfS821->$3;
      _M0L6_2atmpS2335 = _M0L6_2atmpS2336 & _M0L14capacity__maskS2337;
      _M0L1iS818 = _M0L6_2atmpS2334;
      _M0L3idxS819 = _M0L6_2atmpS2335;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS767
) {
  int32_t _M0L6lengthS766;
  int32_t _M0Lm8capacityS768;
  int32_t _M0L6_2atmpS2252;
  int32_t _M0L6_2atmpS2251;
  int32_t _M0L6_2atmpS2262;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS769;
  int32_t _M0L3endS2260;
  int32_t _M0L5startS2261;
  int32_t _M0L7_2abindS770;
  int32_t _M0L2__S771;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS767.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS766
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS767);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS768 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS766);
  _M0L6_2atmpS2252 = _M0Lm8capacityS768;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2251 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2252);
  if (_M0L6lengthS766 > _M0L6_2atmpS2251) {
    int32_t _M0L6_2atmpS2253 = _M0Lm8capacityS768;
    _M0Lm8capacityS768 = _M0L6_2atmpS2253 * 2;
  }
  _M0L6_2atmpS2262 = _M0Lm8capacityS768;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS769
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2262);
  _M0L3endS2260 = _M0L3arrS767.$2;
  _M0L5startS2261 = _M0L3arrS767.$1;
  _M0L7_2abindS770 = _M0L3endS2260 - _M0L5startS2261;
  _M0L2__S771 = 0;
  while (1) {
    if (_M0L2__S771 < _M0L7_2abindS770) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3273 =
        _M0L3arrS767.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2257 =
        _M0L8_2afieldS3273;
      int32_t _M0L5startS2259 = _M0L3arrS767.$1;
      int32_t _M0L6_2atmpS2258 = _M0L5startS2259 + _M0L2__S771;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3272 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2257[
          _M0L6_2atmpS2258
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS772 =
        _M0L6_2atmpS3272;
      moonbit_string_t _M0L8_2afieldS3271 = _M0L1eS772->$0;
      moonbit_string_t _M0L6_2atmpS2254 = _M0L8_2afieldS3271;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3270 =
        _M0L1eS772->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2255 =
        _M0L8_2afieldS3270;
      int32_t _M0L6_2atmpS2256;
      moonbit_incref(_M0L6_2atmpS2255);
      moonbit_incref(_M0L6_2atmpS2254);
      moonbit_incref(_M0L1mS769);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS769, _M0L6_2atmpS2254, _M0L6_2atmpS2255);
      _M0L6_2atmpS2256 = _M0L2__S771 + 1;
      _M0L2__S771 = _M0L6_2atmpS2256;
      continue;
    } else {
      moonbit_decref(_M0L3arrS767.$0);
    }
    break;
  }
  return _M0L1mS769;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS775
) {
  int32_t _M0L6lengthS774;
  int32_t _M0Lm8capacityS776;
  int32_t _M0L6_2atmpS2264;
  int32_t _M0L6_2atmpS2263;
  int32_t _M0L6_2atmpS2274;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS777;
  int32_t _M0L3endS2272;
  int32_t _M0L5startS2273;
  int32_t _M0L7_2abindS778;
  int32_t _M0L2__S779;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS775.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS774
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS775);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS776 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS774);
  _M0L6_2atmpS2264 = _M0Lm8capacityS776;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2263 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2264);
  if (_M0L6lengthS774 > _M0L6_2atmpS2263) {
    int32_t _M0L6_2atmpS2265 = _M0Lm8capacityS776;
    _M0Lm8capacityS776 = _M0L6_2atmpS2265 * 2;
  }
  _M0L6_2atmpS2274 = _M0Lm8capacityS776;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS777
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2274);
  _M0L3endS2272 = _M0L3arrS775.$2;
  _M0L5startS2273 = _M0L3arrS775.$1;
  _M0L7_2abindS778 = _M0L3endS2272 - _M0L5startS2273;
  _M0L2__S779 = 0;
  while (1) {
    if (_M0L2__S779 < _M0L7_2abindS778) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3276 =
        _M0L3arrS775.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2269 =
        _M0L8_2afieldS3276;
      int32_t _M0L5startS2271 = _M0L3arrS775.$1;
      int32_t _M0L6_2atmpS2270 = _M0L5startS2271 + _M0L2__S779;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3275 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2269[
          _M0L6_2atmpS2270
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS780 = _M0L6_2atmpS3275;
      int32_t _M0L6_2atmpS2266 = _M0L1eS780->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3274 =
        _M0L1eS780->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2267 =
        _M0L8_2afieldS3274;
      int32_t _M0L6_2atmpS2268;
      moonbit_incref(_M0L6_2atmpS2267);
      moonbit_incref(_M0L1mS777);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS777, _M0L6_2atmpS2266, _M0L6_2atmpS2267);
      _M0L6_2atmpS2268 = _M0L2__S779 + 1;
      _M0L2__S779 = _M0L6_2atmpS2268;
      continue;
    } else {
      moonbit_decref(_M0L3arrS775.$0);
    }
    break;
  }
  return _M0L1mS777;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS783
) {
  int32_t _M0L6lengthS782;
  int32_t _M0Lm8capacityS784;
  int32_t _M0L6_2atmpS2276;
  int32_t _M0L6_2atmpS2275;
  int32_t _M0L6_2atmpS2286;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS785;
  int32_t _M0L3endS2284;
  int32_t _M0L5startS2285;
  int32_t _M0L7_2abindS786;
  int32_t _M0L2__S787;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS783.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS782 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS783);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS784 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS782);
  _M0L6_2atmpS2276 = _M0Lm8capacityS784;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2275 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2276);
  if (_M0L6lengthS782 > _M0L6_2atmpS2275) {
    int32_t _M0L6_2atmpS2277 = _M0Lm8capacityS784;
    _M0Lm8capacityS784 = _M0L6_2atmpS2277 * 2;
  }
  _M0L6_2atmpS2286 = _M0Lm8capacityS784;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS785 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2286);
  _M0L3endS2284 = _M0L3arrS783.$2;
  _M0L5startS2285 = _M0L3arrS783.$1;
  _M0L7_2abindS786 = _M0L3endS2284 - _M0L5startS2285;
  _M0L2__S787 = 0;
  while (1) {
    if (_M0L2__S787 < _M0L7_2abindS786) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS3280 = _M0L3arrS783.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2281 = _M0L8_2afieldS3280;
      int32_t _M0L5startS2283 = _M0L3arrS783.$1;
      int32_t _M0L6_2atmpS2282 = _M0L5startS2283 + _M0L2__S787;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS3279 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2281[_M0L6_2atmpS2282];
      struct _M0TUsRPB4JsonE* _M0L1eS788 = _M0L6_2atmpS3279;
      moonbit_string_t _M0L8_2afieldS3278 = _M0L1eS788->$0;
      moonbit_string_t _M0L6_2atmpS2278 = _M0L8_2afieldS3278;
      void* _M0L8_2afieldS3277 = _M0L1eS788->$1;
      void* _M0L6_2atmpS2279 = _M0L8_2afieldS3277;
      int32_t _M0L6_2atmpS2280;
      moonbit_incref(_M0L6_2atmpS2279);
      moonbit_incref(_M0L6_2atmpS2278);
      moonbit_incref(_M0L1mS785);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS785, _M0L6_2atmpS2278, _M0L6_2atmpS2279);
      _M0L6_2atmpS2280 = _M0L2__S787 + 1;
      _M0L2__S787 = _M0L6_2atmpS2280;
      continue;
    } else {
      moonbit_decref(_M0L3arrS783.$0);
    }
    break;
  }
  return _M0L1mS785;
}

struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE _M0L3arrS791
) {
  int32_t _M0L6lengthS790;
  int32_t _M0Lm8capacityS792;
  int32_t _M0L6_2atmpS2288;
  int32_t _M0L6_2atmpS2287;
  int32_t _M0L6_2atmpS2298;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L1mS793;
  int32_t _M0L3endS2296;
  int32_t _M0L5startS2297;
  int32_t _M0L7_2abindS794;
  int32_t _M0L2__S795;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS791.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS790
  = _M0MPC15array9ArrayView6lengthGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(_M0L3arrS791);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS792 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS790);
  _M0L6_2atmpS2288 = _M0Lm8capacityS792;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2287 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2288);
  if (_M0L6lengthS790 > _M0L6_2atmpS2287) {
    int32_t _M0L6_2atmpS2289 = _M0Lm8capacityS792;
    _M0Lm8capacityS792 = _M0L6_2atmpS2289 * 2;
  }
  _M0L6_2atmpS2298 = _M0Lm8capacityS792;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS793
  = _M0MPB3Map11new_2einnerGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L6_2atmpS2298);
  _M0L3endS2296 = _M0L3arrS791.$2;
  _M0L5startS2297 = _M0L3arrS791.$1;
  _M0L7_2abindS794 = _M0L3endS2296 - _M0L5startS2297;
  _M0L2__S795 = 0;
  while (1) {
    if (_M0L2__S795 < _M0L7_2abindS794) {
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3284 =
        _M0L3arrS791.$0;
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L3bufS2293 =
        _M0L8_2afieldS3284;
      int32_t _M0L5startS2295 = _M0L3arrS791.$1;
      int32_t _M0L6_2atmpS2294 = _M0L5startS2295 + _M0L2__S795;
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3283 =
        (struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L3bufS2293[
          _M0L6_2atmpS2294
        ];
      struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L1eS796 =
        _M0L6_2atmpS3283;
      moonbit_string_t _M0L8_2afieldS3282 = _M0L1eS796->$0;
      moonbit_string_t _M0L6_2atmpS2290 = _M0L8_2afieldS3282;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3281 =
        _M0L1eS796->$1;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L6_2atmpS2291 =
        _M0L8_2afieldS3281;
      int32_t _M0L6_2atmpS2292;
      moonbit_incref(_M0L6_2atmpS2291);
      moonbit_incref(_M0L6_2atmpS2290);
      moonbit_incref(_M0L1mS793);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L1mS793, _M0L6_2atmpS2290, _M0L6_2atmpS2291);
      _M0L6_2atmpS2292 = _M0L2__S795 + 1;
      _M0L2__S795 = _M0L6_2atmpS2292;
      continue;
    } else {
      moonbit_decref(_M0L3arrS791.$0);
    }
    break;
  }
  return _M0L1mS793;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS754,
  moonbit_string_t _M0L3keyS755,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS756
) {
  int32_t _M0L6_2atmpS2247;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS755);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2247 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS755);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS754, _M0L3keyS755, _M0L5valueS756, _M0L6_2atmpS2247);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS757,
  int32_t _M0L3keyS758,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS759
) {
  int32_t _M0L6_2atmpS2248;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2248 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS758);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS757, _M0L3keyS758, _M0L5valueS759, _M0L6_2atmpS2248);
  return 0;
}

int32_t _M0MPB3Map3setGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS760,
  moonbit_string_t _M0L3keyS761,
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L5valueS762
) {
  int32_t _M0L6_2atmpS2249;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS761);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2249 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS761);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS760, _M0L3keyS761, _M0L5valueS762, _M0L6_2atmpS2249);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS763,
  moonbit_string_t _M0L3keyS764,
  void* _M0L5valueS765
) {
  int32_t _M0L6_2atmpS2250;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS764);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2250 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS764);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS763, _M0L3keyS764, _M0L5valueS765, _M0L6_2atmpS2250);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS711
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3291;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS710;
  int32_t _M0L8capacityS2225;
  int32_t _M0L13new__capacityS712;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2220;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2219;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3290;
  int32_t _M0L6_2atmpS2221;
  int32_t _M0L8capacityS2223;
  int32_t _M0L6_2atmpS2222;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2224;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3289;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS713;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3291 = _M0L4selfS711->$5;
  _M0L9old__headS710 = _M0L8_2afieldS3291;
  _M0L8capacityS2225 = _M0L4selfS711->$2;
  _M0L13new__capacityS712 = _M0L8capacityS2225 << 1;
  _M0L6_2atmpS2220 = 0;
  _M0L6_2atmpS2219
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS712, _M0L6_2atmpS2220);
  _M0L6_2aoldS3290 = _M0L4selfS711->$0;
  if (_M0L9old__headS710) {
    moonbit_incref(_M0L9old__headS710);
  }
  moonbit_decref(_M0L6_2aoldS3290);
  _M0L4selfS711->$0 = _M0L6_2atmpS2219;
  _M0L4selfS711->$2 = _M0L13new__capacityS712;
  _M0L6_2atmpS2221 = _M0L13new__capacityS712 - 1;
  _M0L4selfS711->$3 = _M0L6_2atmpS2221;
  _M0L8capacityS2223 = _M0L4selfS711->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2222 = _M0FPB21calc__grow__threshold(_M0L8capacityS2223);
  _M0L4selfS711->$4 = _M0L6_2atmpS2222;
  _M0L4selfS711->$1 = 0;
  _M0L6_2atmpS2224 = 0;
  _M0L6_2aoldS3289 = _M0L4selfS711->$5;
  if (_M0L6_2aoldS3289) {
    moonbit_decref(_M0L6_2aoldS3289);
  }
  _M0L4selfS711->$5 = _M0L6_2atmpS2224;
  _M0L4selfS711->$6 = -1;
  _M0L8_2aparamS713 = _M0L9old__headS710;
  while (1) {
    if (_M0L8_2aparamS713 == 0) {
      if (_M0L8_2aparamS713) {
        moonbit_decref(_M0L8_2aparamS713);
      }
      moonbit_decref(_M0L4selfS711);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS714 =
        _M0L8_2aparamS713;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS715 =
        _M0L7_2aSomeS714;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3288 =
        _M0L4_2axS715->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS716 =
        _M0L8_2afieldS3288;
      moonbit_string_t _M0L8_2afieldS3287 = _M0L4_2axS715->$4;
      moonbit_string_t _M0L6_2akeyS717 = _M0L8_2afieldS3287;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3286 =
        _M0L4_2axS715->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS718 =
        _M0L8_2afieldS3286;
      int32_t _M0L8_2afieldS3285 = _M0L4_2axS715->$3;
      int32_t _M0L6_2acntS3549 = Moonbit_object_header(_M0L4_2axS715)->rc;
      int32_t _M0L7_2ahashS719;
      if (_M0L6_2acntS3549 > 1) {
        int32_t _M0L11_2anew__cntS3550 = _M0L6_2acntS3549 - 1;
        Moonbit_object_header(_M0L4_2axS715)->rc = _M0L11_2anew__cntS3550;
        moonbit_incref(_M0L8_2avalueS718);
        moonbit_incref(_M0L6_2akeyS717);
        if (_M0L7_2anextS716) {
          moonbit_incref(_M0L7_2anextS716);
        }
      } else if (_M0L6_2acntS3549 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS715);
      }
      _M0L7_2ahashS719 = _M0L8_2afieldS3285;
      moonbit_incref(_M0L4selfS711);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS711, _M0L6_2akeyS717, _M0L8_2avalueS718, _M0L7_2ahashS719);
      _M0L8_2aparamS713 = _M0L7_2anextS716;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS722
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3297;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS721;
  int32_t _M0L8capacityS2232;
  int32_t _M0L13new__capacityS723;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2227;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2226;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3296;
  int32_t _M0L6_2atmpS2228;
  int32_t _M0L8capacityS2230;
  int32_t _M0L6_2atmpS2229;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2231;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3295;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS724;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3297 = _M0L4selfS722->$5;
  _M0L9old__headS721 = _M0L8_2afieldS3297;
  _M0L8capacityS2232 = _M0L4selfS722->$2;
  _M0L13new__capacityS723 = _M0L8capacityS2232 << 1;
  _M0L6_2atmpS2227 = 0;
  _M0L6_2atmpS2226
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS723, _M0L6_2atmpS2227);
  _M0L6_2aoldS3296 = _M0L4selfS722->$0;
  if (_M0L9old__headS721) {
    moonbit_incref(_M0L9old__headS721);
  }
  moonbit_decref(_M0L6_2aoldS3296);
  _M0L4selfS722->$0 = _M0L6_2atmpS2226;
  _M0L4selfS722->$2 = _M0L13new__capacityS723;
  _M0L6_2atmpS2228 = _M0L13new__capacityS723 - 1;
  _M0L4selfS722->$3 = _M0L6_2atmpS2228;
  _M0L8capacityS2230 = _M0L4selfS722->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2229 = _M0FPB21calc__grow__threshold(_M0L8capacityS2230);
  _M0L4selfS722->$4 = _M0L6_2atmpS2229;
  _M0L4selfS722->$1 = 0;
  _M0L6_2atmpS2231 = 0;
  _M0L6_2aoldS3295 = _M0L4selfS722->$5;
  if (_M0L6_2aoldS3295) {
    moonbit_decref(_M0L6_2aoldS3295);
  }
  _M0L4selfS722->$5 = _M0L6_2atmpS2231;
  _M0L4selfS722->$6 = -1;
  _M0L8_2aparamS724 = _M0L9old__headS721;
  while (1) {
    if (_M0L8_2aparamS724 == 0) {
      if (_M0L8_2aparamS724) {
        moonbit_decref(_M0L8_2aparamS724);
      }
      moonbit_decref(_M0L4selfS722);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS725 =
        _M0L8_2aparamS724;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS726 =
        _M0L7_2aSomeS725;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3294 =
        _M0L4_2axS726->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS727 =
        _M0L8_2afieldS3294;
      int32_t _M0L6_2akeyS728 = _M0L4_2axS726->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3293 =
        _M0L4_2axS726->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS729 =
        _M0L8_2afieldS3293;
      int32_t _M0L8_2afieldS3292 = _M0L4_2axS726->$3;
      int32_t _M0L6_2acntS3551 = Moonbit_object_header(_M0L4_2axS726)->rc;
      int32_t _M0L7_2ahashS730;
      if (_M0L6_2acntS3551 > 1) {
        int32_t _M0L11_2anew__cntS3552 = _M0L6_2acntS3551 - 1;
        Moonbit_object_header(_M0L4_2axS726)->rc = _M0L11_2anew__cntS3552;
        moonbit_incref(_M0L8_2avalueS729);
        if (_M0L7_2anextS727) {
          moonbit_incref(_M0L7_2anextS727);
        }
      } else if (_M0L6_2acntS3551 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS726);
      }
      _M0L7_2ahashS730 = _M0L8_2afieldS3292;
      moonbit_incref(_M0L4selfS722);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS722, _M0L6_2akeyS728, _M0L8_2avalueS729, _M0L7_2ahashS730);
      _M0L8_2aparamS724 = _M0L7_2anextS727;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS733
) {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3304;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L9old__headS732;
  int32_t _M0L8capacityS2239;
  int32_t _M0L13new__capacityS734;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2234;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L6_2atmpS2233;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L6_2aoldS3303;
  int32_t _M0L6_2atmpS2235;
  int32_t _M0L8capacityS2237;
  int32_t _M0L6_2atmpS2236;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2238;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3302;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2aparamS735;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3304 = _M0L4selfS733->$5;
  _M0L9old__headS732 = _M0L8_2afieldS3304;
  _M0L8capacityS2239 = _M0L4selfS733->$2;
  _M0L13new__capacityS734 = _M0L8capacityS2239 << 1;
  _M0L6_2atmpS2234 = 0;
  _M0L6_2atmpS2233
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE**)moonbit_make_ref_array(_M0L13new__capacityS734, _M0L6_2atmpS2234);
  _M0L6_2aoldS3303 = _M0L4selfS733->$0;
  if (_M0L9old__headS732) {
    moonbit_incref(_M0L9old__headS732);
  }
  moonbit_decref(_M0L6_2aoldS3303);
  _M0L4selfS733->$0 = _M0L6_2atmpS2233;
  _M0L4selfS733->$2 = _M0L13new__capacityS734;
  _M0L6_2atmpS2235 = _M0L13new__capacityS734 - 1;
  _M0L4selfS733->$3 = _M0L6_2atmpS2235;
  _M0L8capacityS2237 = _M0L4selfS733->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2236 = _M0FPB21calc__grow__threshold(_M0L8capacityS2237);
  _M0L4selfS733->$4 = _M0L6_2atmpS2236;
  _M0L4selfS733->$1 = 0;
  _M0L6_2atmpS2238 = 0;
  _M0L6_2aoldS3302 = _M0L4selfS733->$5;
  if (_M0L6_2aoldS3302) {
    moonbit_decref(_M0L6_2aoldS3302);
  }
  _M0L4selfS733->$5 = _M0L6_2atmpS2238;
  _M0L4selfS733->$6 = -1;
  _M0L8_2aparamS735 = _M0L9old__headS732;
  while (1) {
    if (_M0L8_2aparamS735 == 0) {
      if (_M0L8_2aparamS735) {
        moonbit_decref(_M0L8_2aparamS735);
      }
      moonbit_decref(_M0L4selfS733);
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS736 =
        _M0L8_2aparamS735;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4_2axS737 =
        _M0L7_2aSomeS736;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3301 =
        _M0L4_2axS737->$1;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2anextS738 =
        _M0L8_2afieldS3301;
      moonbit_string_t _M0L8_2afieldS3300 = _M0L4_2axS737->$4;
      moonbit_string_t _M0L6_2akeyS739 = _M0L8_2afieldS3300;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3299 =
        _M0L4_2axS737->$5;
      struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2avalueS740 =
        _M0L8_2afieldS3299;
      int32_t _M0L8_2afieldS3298 = _M0L4_2axS737->$3;
      int32_t _M0L6_2acntS3553 = Moonbit_object_header(_M0L4_2axS737)->rc;
      int32_t _M0L7_2ahashS741;
      if (_M0L6_2acntS3553 > 1) {
        int32_t _M0L11_2anew__cntS3554 = _M0L6_2acntS3553 - 1;
        Moonbit_object_header(_M0L4_2axS737)->rc = _M0L11_2anew__cntS3554;
        moonbit_incref(_M0L8_2avalueS740);
        moonbit_incref(_M0L6_2akeyS739);
        if (_M0L7_2anextS738) {
          moonbit_incref(_M0L7_2anextS738);
        }
      } else if (_M0L6_2acntS3553 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS737);
      }
      _M0L7_2ahashS741 = _M0L8_2afieldS3298;
      moonbit_incref(_M0L4selfS733);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS733, _M0L6_2akeyS739, _M0L8_2avalueS740, _M0L7_2ahashS741);
      _M0L8_2aparamS735 = _M0L7_2anextS738;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS744
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3311;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS743;
  int32_t _M0L8capacityS2246;
  int32_t _M0L13new__capacityS745;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2241;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2240;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS3310;
  int32_t _M0L6_2atmpS2242;
  int32_t _M0L8capacityS2244;
  int32_t _M0L6_2atmpS2243;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2245;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3309;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS746;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3311 = _M0L4selfS744->$5;
  _M0L9old__headS743 = _M0L8_2afieldS3311;
  _M0L8capacityS2246 = _M0L4selfS744->$2;
  _M0L13new__capacityS745 = _M0L8capacityS2246 << 1;
  _M0L6_2atmpS2241 = 0;
  _M0L6_2atmpS2240
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS745, _M0L6_2atmpS2241);
  _M0L6_2aoldS3310 = _M0L4selfS744->$0;
  if (_M0L9old__headS743) {
    moonbit_incref(_M0L9old__headS743);
  }
  moonbit_decref(_M0L6_2aoldS3310);
  _M0L4selfS744->$0 = _M0L6_2atmpS2240;
  _M0L4selfS744->$2 = _M0L13new__capacityS745;
  _M0L6_2atmpS2242 = _M0L13new__capacityS745 - 1;
  _M0L4selfS744->$3 = _M0L6_2atmpS2242;
  _M0L8capacityS2244 = _M0L4selfS744->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2243 = _M0FPB21calc__grow__threshold(_M0L8capacityS2244);
  _M0L4selfS744->$4 = _M0L6_2atmpS2243;
  _M0L4selfS744->$1 = 0;
  _M0L6_2atmpS2245 = 0;
  _M0L6_2aoldS3309 = _M0L4selfS744->$5;
  if (_M0L6_2aoldS3309) {
    moonbit_decref(_M0L6_2aoldS3309);
  }
  _M0L4selfS744->$5 = _M0L6_2atmpS2245;
  _M0L4selfS744->$6 = -1;
  _M0L8_2aparamS746 = _M0L9old__headS743;
  while (1) {
    if (_M0L8_2aparamS746 == 0) {
      if (_M0L8_2aparamS746) {
        moonbit_decref(_M0L8_2aparamS746);
      }
      moonbit_decref(_M0L4selfS744);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS747 = _M0L8_2aparamS746;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS748 = _M0L7_2aSomeS747;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3308 = _M0L4_2axS748->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS749 = _M0L8_2afieldS3308;
      moonbit_string_t _M0L8_2afieldS3307 = _M0L4_2axS748->$4;
      moonbit_string_t _M0L6_2akeyS750 = _M0L8_2afieldS3307;
      void* _M0L8_2afieldS3306 = _M0L4_2axS748->$5;
      void* _M0L8_2avalueS751 = _M0L8_2afieldS3306;
      int32_t _M0L8_2afieldS3305 = _M0L4_2axS748->$3;
      int32_t _M0L6_2acntS3555 = Moonbit_object_header(_M0L4_2axS748)->rc;
      int32_t _M0L7_2ahashS752;
      if (_M0L6_2acntS3555 > 1) {
        int32_t _M0L11_2anew__cntS3556 = _M0L6_2acntS3555 - 1;
        Moonbit_object_header(_M0L4_2axS748)->rc = _M0L11_2anew__cntS3556;
        moonbit_incref(_M0L8_2avalueS751);
        moonbit_incref(_M0L6_2akeyS750);
        if (_M0L7_2anextS749) {
          moonbit_incref(_M0L7_2anextS749);
        }
      } else if (_M0L6_2acntS3555 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS748);
      }
      _M0L7_2ahashS752 = _M0L8_2afieldS3305;
      moonbit_incref(_M0L4selfS744);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS744, _M0L6_2akeyS750, _M0L8_2avalueS751, _M0L7_2ahashS752);
      _M0L8_2aparamS746 = _M0L7_2anextS749;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS649,
  moonbit_string_t _M0L3keyS655,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS656,
  int32_t _M0L4hashS651
) {
  int32_t _M0L14capacity__maskS2164;
  int32_t _M0L6_2atmpS2163;
  int32_t _M0L3pslS646;
  int32_t _M0L3idxS647;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2164 = _M0L4selfS649->$3;
  _M0L6_2atmpS2163 = _M0L4hashS651 & _M0L14capacity__maskS2164;
  _M0L3pslS646 = 0;
  _M0L3idxS647 = _M0L6_2atmpS2163;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3316 =
      _M0L4selfS649->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2162 =
      _M0L8_2afieldS3316;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3315;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS648;
    if (
      _M0L3idxS647 < 0
      || _M0L3idxS647 >= Moonbit_array_length(_M0L7entriesS2162)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3315
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2162[
        _M0L3idxS647
      ];
    _M0L7_2abindS648 = _M0L6_2atmpS3315;
    if (_M0L7_2abindS648 == 0) {
      int32_t _M0L4sizeS2147 = _M0L4selfS649->$1;
      int32_t _M0L8grow__atS2148 = _M0L4selfS649->$4;
      int32_t _M0L7_2abindS652;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS653;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS654;
      if (_M0L4sizeS2147 >= _M0L8grow__atS2148) {
        int32_t _M0L14capacity__maskS2150;
        int32_t _M0L6_2atmpS2149;
        moonbit_incref(_M0L4selfS649);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS649);
        _M0L14capacity__maskS2150 = _M0L4selfS649->$3;
        _M0L6_2atmpS2149 = _M0L4hashS651 & _M0L14capacity__maskS2150;
        _M0L3pslS646 = 0;
        _M0L3idxS647 = _M0L6_2atmpS2149;
        continue;
      }
      _M0L7_2abindS652 = _M0L4selfS649->$6;
      _M0L7_2abindS653 = 0;
      _M0L5entryS654
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS654)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS654->$0 = _M0L7_2abindS652;
      _M0L5entryS654->$1 = _M0L7_2abindS653;
      _M0L5entryS654->$2 = _M0L3pslS646;
      _M0L5entryS654->$3 = _M0L4hashS651;
      _M0L5entryS654->$4 = _M0L3keyS655;
      _M0L5entryS654->$5 = _M0L5valueS656;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS649, _M0L3idxS647, _M0L5entryS654);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS657 =
        _M0L7_2abindS648;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS658 =
        _M0L7_2aSomeS657;
      int32_t _M0L4hashS2152 = _M0L14_2acurr__entryS658->$3;
      int32_t _if__result_3695;
      int32_t _M0L3pslS2153;
      int32_t _M0L6_2atmpS2158;
      int32_t _M0L6_2atmpS2160;
      int32_t _M0L14capacity__maskS2161;
      int32_t _M0L6_2atmpS2159;
      if (_M0L4hashS2152 == _M0L4hashS651) {
        moonbit_string_t _M0L8_2afieldS3314 = _M0L14_2acurr__entryS658->$4;
        moonbit_string_t _M0L3keyS2151 = _M0L8_2afieldS3314;
        int32_t _M0L6_2atmpS3313;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3313
        = moonbit_val_array_equal(_M0L3keyS2151, _M0L3keyS655);
        _if__result_3695 = _M0L6_2atmpS3313;
      } else {
        _if__result_3695 = 0;
      }
      if (_if__result_3695) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3312;
        moonbit_incref(_M0L14_2acurr__entryS658);
        moonbit_decref(_M0L3keyS655);
        moonbit_decref(_M0L4selfS649);
        _M0L6_2aoldS3312 = _M0L14_2acurr__entryS658->$5;
        moonbit_decref(_M0L6_2aoldS3312);
        _M0L14_2acurr__entryS658->$5 = _M0L5valueS656;
        moonbit_decref(_M0L14_2acurr__entryS658);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS658);
      }
      _M0L3pslS2153 = _M0L14_2acurr__entryS658->$2;
      if (_M0L3pslS646 > _M0L3pslS2153) {
        int32_t _M0L4sizeS2154 = _M0L4selfS649->$1;
        int32_t _M0L8grow__atS2155 = _M0L4selfS649->$4;
        int32_t _M0L7_2abindS659;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS660;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS661;
        if (_M0L4sizeS2154 >= _M0L8grow__atS2155) {
          int32_t _M0L14capacity__maskS2157;
          int32_t _M0L6_2atmpS2156;
          moonbit_decref(_M0L14_2acurr__entryS658);
          moonbit_incref(_M0L4selfS649);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS649);
          _M0L14capacity__maskS2157 = _M0L4selfS649->$3;
          _M0L6_2atmpS2156 = _M0L4hashS651 & _M0L14capacity__maskS2157;
          _M0L3pslS646 = 0;
          _M0L3idxS647 = _M0L6_2atmpS2156;
          continue;
        }
        moonbit_incref(_M0L4selfS649);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS649, _M0L3idxS647, _M0L14_2acurr__entryS658);
        _M0L7_2abindS659 = _M0L4selfS649->$6;
        _M0L7_2abindS660 = 0;
        _M0L5entryS661
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS661)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS661->$0 = _M0L7_2abindS659;
        _M0L5entryS661->$1 = _M0L7_2abindS660;
        _M0L5entryS661->$2 = _M0L3pslS646;
        _M0L5entryS661->$3 = _M0L4hashS651;
        _M0L5entryS661->$4 = _M0L3keyS655;
        _M0L5entryS661->$5 = _M0L5valueS656;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS649, _M0L3idxS647, _M0L5entryS661);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS658);
      }
      _M0L6_2atmpS2158 = _M0L3pslS646 + 1;
      _M0L6_2atmpS2160 = _M0L3idxS647 + 1;
      _M0L14capacity__maskS2161 = _M0L4selfS649->$3;
      _M0L6_2atmpS2159 = _M0L6_2atmpS2160 & _M0L14capacity__maskS2161;
      _M0L3pslS646 = _M0L6_2atmpS2158;
      _M0L3idxS647 = _M0L6_2atmpS2159;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS665,
  int32_t _M0L3keyS671,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS672,
  int32_t _M0L4hashS667
) {
  int32_t _M0L14capacity__maskS2182;
  int32_t _M0L6_2atmpS2181;
  int32_t _M0L3pslS662;
  int32_t _M0L3idxS663;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2182 = _M0L4selfS665->$3;
  _M0L6_2atmpS2181 = _M0L4hashS667 & _M0L14capacity__maskS2182;
  _M0L3pslS662 = 0;
  _M0L3idxS663 = _M0L6_2atmpS2181;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3319 =
      _M0L4selfS665->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2180 =
      _M0L8_2afieldS3319;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3318;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS664;
    if (
      _M0L3idxS663 < 0
      || _M0L3idxS663 >= Moonbit_array_length(_M0L7entriesS2180)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3318
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2180[
        _M0L3idxS663
      ];
    _M0L7_2abindS664 = _M0L6_2atmpS3318;
    if (_M0L7_2abindS664 == 0) {
      int32_t _M0L4sizeS2165 = _M0L4selfS665->$1;
      int32_t _M0L8grow__atS2166 = _M0L4selfS665->$4;
      int32_t _M0L7_2abindS668;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS669;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS670;
      if (_M0L4sizeS2165 >= _M0L8grow__atS2166) {
        int32_t _M0L14capacity__maskS2168;
        int32_t _M0L6_2atmpS2167;
        moonbit_incref(_M0L4selfS665);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS665);
        _M0L14capacity__maskS2168 = _M0L4selfS665->$3;
        _M0L6_2atmpS2167 = _M0L4hashS667 & _M0L14capacity__maskS2168;
        _M0L3pslS662 = 0;
        _M0L3idxS663 = _M0L6_2atmpS2167;
        continue;
      }
      _M0L7_2abindS668 = _M0L4selfS665->$6;
      _M0L7_2abindS669 = 0;
      _M0L5entryS670
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS670)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS670->$0 = _M0L7_2abindS668;
      _M0L5entryS670->$1 = _M0L7_2abindS669;
      _M0L5entryS670->$2 = _M0L3pslS662;
      _M0L5entryS670->$3 = _M0L4hashS667;
      _M0L5entryS670->$4 = _M0L3keyS671;
      _M0L5entryS670->$5 = _M0L5valueS672;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS665, _M0L3idxS663, _M0L5entryS670);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS673 =
        _M0L7_2abindS664;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS674 =
        _M0L7_2aSomeS673;
      int32_t _M0L4hashS2170 = _M0L14_2acurr__entryS674->$3;
      int32_t _if__result_3697;
      int32_t _M0L3pslS2171;
      int32_t _M0L6_2atmpS2176;
      int32_t _M0L6_2atmpS2178;
      int32_t _M0L14capacity__maskS2179;
      int32_t _M0L6_2atmpS2177;
      if (_M0L4hashS2170 == _M0L4hashS667) {
        int32_t _M0L3keyS2169 = _M0L14_2acurr__entryS674->$4;
        _if__result_3697 = _M0L3keyS2169 == _M0L3keyS671;
      } else {
        _if__result_3697 = 0;
      }
      if (_if__result_3697) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3317;
        moonbit_incref(_M0L14_2acurr__entryS674);
        moonbit_decref(_M0L4selfS665);
        _M0L6_2aoldS3317 = _M0L14_2acurr__entryS674->$5;
        moonbit_decref(_M0L6_2aoldS3317);
        _M0L14_2acurr__entryS674->$5 = _M0L5valueS672;
        moonbit_decref(_M0L14_2acurr__entryS674);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS674);
      }
      _M0L3pslS2171 = _M0L14_2acurr__entryS674->$2;
      if (_M0L3pslS662 > _M0L3pslS2171) {
        int32_t _M0L4sizeS2172 = _M0L4selfS665->$1;
        int32_t _M0L8grow__atS2173 = _M0L4selfS665->$4;
        int32_t _M0L7_2abindS675;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS676;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS677;
        if (_M0L4sizeS2172 >= _M0L8grow__atS2173) {
          int32_t _M0L14capacity__maskS2175;
          int32_t _M0L6_2atmpS2174;
          moonbit_decref(_M0L14_2acurr__entryS674);
          moonbit_incref(_M0L4selfS665);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS665);
          _M0L14capacity__maskS2175 = _M0L4selfS665->$3;
          _M0L6_2atmpS2174 = _M0L4hashS667 & _M0L14capacity__maskS2175;
          _M0L3pslS662 = 0;
          _M0L3idxS663 = _M0L6_2atmpS2174;
          continue;
        }
        moonbit_incref(_M0L4selfS665);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS665, _M0L3idxS663, _M0L14_2acurr__entryS674);
        _M0L7_2abindS675 = _M0L4selfS665->$6;
        _M0L7_2abindS676 = 0;
        _M0L5entryS677
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS677)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS677->$0 = _M0L7_2abindS675;
        _M0L5entryS677->$1 = _M0L7_2abindS676;
        _M0L5entryS677->$2 = _M0L3pslS662;
        _M0L5entryS677->$3 = _M0L4hashS667;
        _M0L5entryS677->$4 = _M0L3keyS671;
        _M0L5entryS677->$5 = _M0L5valueS672;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS665, _M0L3idxS663, _M0L5entryS677);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS674);
      }
      _M0L6_2atmpS2176 = _M0L3pslS662 + 1;
      _M0L6_2atmpS2178 = _M0L3idxS663 + 1;
      _M0L14capacity__maskS2179 = _M0L4selfS665->$3;
      _M0L6_2atmpS2177 = _M0L6_2atmpS2178 & _M0L14capacity__maskS2179;
      _M0L3pslS662 = _M0L6_2atmpS2176;
      _M0L3idxS663 = _M0L6_2atmpS2177;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS681,
  moonbit_string_t _M0L3keyS687,
  struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L5valueS688,
  int32_t _M0L4hashS683
) {
  int32_t _M0L14capacity__maskS2200;
  int32_t _M0L6_2atmpS2199;
  int32_t _M0L3pslS678;
  int32_t _M0L3idxS679;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2200 = _M0L4selfS681->$3;
  _M0L6_2atmpS2199 = _M0L4hashS683 & _M0L14capacity__maskS2200;
  _M0L3pslS678 = 0;
  _M0L3idxS679 = _M0L6_2atmpS2199;
  while (1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3324 =
      _M0L4selfS681->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2198 =
      _M0L8_2afieldS3324;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3323;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS680;
    if (
      _M0L3idxS679 < 0
      || _M0L3idxS679 >= Moonbit_array_length(_M0L7entriesS2198)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3323
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2198[
        _M0L3idxS679
      ];
    _M0L7_2abindS680 = _M0L6_2atmpS3323;
    if (_M0L7_2abindS680 == 0) {
      int32_t _M0L4sizeS2183 = _M0L4selfS681->$1;
      int32_t _M0L8grow__atS2184 = _M0L4selfS681->$4;
      int32_t _M0L7_2abindS684;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS685;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5entryS686;
      if (_M0L4sizeS2183 >= _M0L8grow__atS2184) {
        int32_t _M0L14capacity__maskS2186;
        int32_t _M0L6_2atmpS2185;
        moonbit_incref(_M0L4selfS681);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS681);
        _M0L14capacity__maskS2186 = _M0L4selfS681->$3;
        _M0L6_2atmpS2185 = _M0L4hashS683 & _M0L14capacity__maskS2186;
        _M0L3pslS678 = 0;
        _M0L3idxS679 = _M0L6_2atmpS2185;
        continue;
      }
      _M0L7_2abindS684 = _M0L4selfS681->$6;
      _M0L7_2abindS685 = 0;
      _M0L5entryS686
      = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE));
      Moonbit_object_header(_M0L5entryS686)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE, $1) >> 2, 3, 0);
      _M0L5entryS686->$0 = _M0L7_2abindS684;
      _M0L5entryS686->$1 = _M0L7_2abindS685;
      _M0L5entryS686->$2 = _M0L3pslS678;
      _M0L5entryS686->$3 = _M0L4hashS683;
      _M0L5entryS686->$4 = _M0L3keyS687;
      _M0L5entryS686->$5 = _M0L5valueS688;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS681, _M0L3idxS679, _M0L5entryS686);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS689 =
        _M0L7_2abindS680;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L14_2acurr__entryS690 =
        _M0L7_2aSomeS689;
      int32_t _M0L4hashS2188 = _M0L14_2acurr__entryS690->$3;
      int32_t _if__result_3699;
      int32_t _M0L3pslS2189;
      int32_t _M0L6_2atmpS2194;
      int32_t _M0L6_2atmpS2196;
      int32_t _M0L14capacity__maskS2197;
      int32_t _M0L6_2atmpS2195;
      if (_M0L4hashS2188 == _M0L4hashS683) {
        moonbit_string_t _M0L8_2afieldS3322 = _M0L14_2acurr__entryS690->$4;
        moonbit_string_t _M0L3keyS2187 = _M0L8_2afieldS3322;
        int32_t _M0L6_2atmpS3321;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3321
        = moonbit_val_array_equal(_M0L3keyS2187, _M0L3keyS687);
        _if__result_3699 = _M0L6_2atmpS3321;
      } else {
        _if__result_3699 = 0;
      }
      if (_if__result_3699) {
        struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L6_2aoldS3320;
        moonbit_incref(_M0L14_2acurr__entryS690);
        moonbit_decref(_M0L3keyS687);
        moonbit_decref(_M0L4selfS681);
        _M0L6_2aoldS3320 = _M0L14_2acurr__entryS690->$5;
        moonbit_decref(_M0L6_2aoldS3320);
        _M0L14_2acurr__entryS690->$5 = _M0L5valueS688;
        moonbit_decref(_M0L14_2acurr__entryS690);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS690);
      }
      _M0L3pslS2189 = _M0L14_2acurr__entryS690->$2;
      if (_M0L3pslS678 > _M0L3pslS2189) {
        int32_t _M0L4sizeS2190 = _M0L4selfS681->$1;
        int32_t _M0L8grow__atS2191 = _M0L4selfS681->$4;
        int32_t _M0L7_2abindS691;
        struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS692;
        struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5entryS693;
        if (_M0L4sizeS2190 >= _M0L8grow__atS2191) {
          int32_t _M0L14capacity__maskS2193;
          int32_t _M0L6_2atmpS2192;
          moonbit_decref(_M0L14_2acurr__entryS690);
          moonbit_incref(_M0L4selfS681);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS681);
          _M0L14capacity__maskS2193 = _M0L4selfS681->$3;
          _M0L6_2atmpS2192 = _M0L4hashS683 & _M0L14capacity__maskS2193;
          _M0L3pslS678 = 0;
          _M0L3idxS679 = _M0L6_2atmpS2192;
          continue;
        }
        moonbit_incref(_M0L4selfS681);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS681, _M0L3idxS679, _M0L14_2acurr__entryS690);
        _M0L7_2abindS691 = _M0L4selfS681->$6;
        _M0L7_2abindS692 = 0;
        _M0L5entryS693
        = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE));
        Moonbit_object_header(_M0L5entryS693)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE, $1) >> 2, 3, 0);
        _M0L5entryS693->$0 = _M0L7_2abindS691;
        _M0L5entryS693->$1 = _M0L7_2abindS692;
        _M0L5entryS693->$2 = _M0L3pslS678;
        _M0L5entryS693->$3 = _M0L4hashS683;
        _M0L5entryS693->$4 = _M0L3keyS687;
        _M0L5entryS693->$5 = _M0L5valueS688;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS681, _M0L3idxS679, _M0L5entryS693);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS690);
      }
      _M0L6_2atmpS2194 = _M0L3pslS678 + 1;
      _M0L6_2atmpS2196 = _M0L3idxS679 + 1;
      _M0L14capacity__maskS2197 = _M0L4selfS681->$3;
      _M0L6_2atmpS2195 = _M0L6_2atmpS2196 & _M0L14capacity__maskS2197;
      _M0L3pslS678 = _M0L6_2atmpS2194;
      _M0L3idxS679 = _M0L6_2atmpS2195;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS697,
  moonbit_string_t _M0L3keyS703,
  void* _M0L5valueS704,
  int32_t _M0L4hashS699
) {
  int32_t _M0L14capacity__maskS2218;
  int32_t _M0L6_2atmpS2217;
  int32_t _M0L3pslS694;
  int32_t _M0L3idxS695;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2218 = _M0L4selfS697->$3;
  _M0L6_2atmpS2217 = _M0L4hashS699 & _M0L14capacity__maskS2218;
  _M0L3pslS694 = 0;
  _M0L3idxS695 = _M0L6_2atmpS2217;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3329 = _M0L4selfS697->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2216 = _M0L8_2afieldS3329;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3328;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS696;
    if (
      _M0L3idxS695 < 0
      || _M0L3idxS695 >= Moonbit_array_length(_M0L7entriesS2216)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3328
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2216[_M0L3idxS695];
    _M0L7_2abindS696 = _M0L6_2atmpS3328;
    if (_M0L7_2abindS696 == 0) {
      int32_t _M0L4sizeS2201 = _M0L4selfS697->$1;
      int32_t _M0L8grow__atS2202 = _M0L4selfS697->$4;
      int32_t _M0L7_2abindS700;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS701;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS702;
      if (_M0L4sizeS2201 >= _M0L8grow__atS2202) {
        int32_t _M0L14capacity__maskS2204;
        int32_t _M0L6_2atmpS2203;
        moonbit_incref(_M0L4selfS697);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS697);
        _M0L14capacity__maskS2204 = _M0L4selfS697->$3;
        _M0L6_2atmpS2203 = _M0L4hashS699 & _M0L14capacity__maskS2204;
        _M0L3pslS694 = 0;
        _M0L3idxS695 = _M0L6_2atmpS2203;
        continue;
      }
      _M0L7_2abindS700 = _M0L4selfS697->$6;
      _M0L7_2abindS701 = 0;
      _M0L5entryS702
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS702)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS702->$0 = _M0L7_2abindS700;
      _M0L5entryS702->$1 = _M0L7_2abindS701;
      _M0L5entryS702->$2 = _M0L3pslS694;
      _M0L5entryS702->$3 = _M0L4hashS699;
      _M0L5entryS702->$4 = _M0L3keyS703;
      _M0L5entryS702->$5 = _M0L5valueS704;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS702);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS705 = _M0L7_2abindS696;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS706 =
        _M0L7_2aSomeS705;
      int32_t _M0L4hashS2206 = _M0L14_2acurr__entryS706->$3;
      int32_t _if__result_3701;
      int32_t _M0L3pslS2207;
      int32_t _M0L6_2atmpS2212;
      int32_t _M0L6_2atmpS2214;
      int32_t _M0L14capacity__maskS2215;
      int32_t _M0L6_2atmpS2213;
      if (_M0L4hashS2206 == _M0L4hashS699) {
        moonbit_string_t _M0L8_2afieldS3327 = _M0L14_2acurr__entryS706->$4;
        moonbit_string_t _M0L3keyS2205 = _M0L8_2afieldS3327;
        int32_t _M0L6_2atmpS3326;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3326
        = moonbit_val_array_equal(_M0L3keyS2205, _M0L3keyS703);
        _if__result_3701 = _M0L6_2atmpS3326;
      } else {
        _if__result_3701 = 0;
      }
      if (_if__result_3701) {
        void* _M0L6_2aoldS3325;
        moonbit_incref(_M0L14_2acurr__entryS706);
        moonbit_decref(_M0L3keyS703);
        moonbit_decref(_M0L4selfS697);
        _M0L6_2aoldS3325 = _M0L14_2acurr__entryS706->$5;
        moonbit_decref(_M0L6_2aoldS3325);
        _M0L14_2acurr__entryS706->$5 = _M0L5valueS704;
        moonbit_decref(_M0L14_2acurr__entryS706);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS706);
      }
      _M0L3pslS2207 = _M0L14_2acurr__entryS706->$2;
      if (_M0L3pslS694 > _M0L3pslS2207) {
        int32_t _M0L4sizeS2208 = _M0L4selfS697->$1;
        int32_t _M0L8grow__atS2209 = _M0L4selfS697->$4;
        int32_t _M0L7_2abindS707;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS708;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS709;
        if (_M0L4sizeS2208 >= _M0L8grow__atS2209) {
          int32_t _M0L14capacity__maskS2211;
          int32_t _M0L6_2atmpS2210;
          moonbit_decref(_M0L14_2acurr__entryS706);
          moonbit_incref(_M0L4selfS697);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS697);
          _M0L14capacity__maskS2211 = _M0L4selfS697->$3;
          _M0L6_2atmpS2210 = _M0L4hashS699 & _M0L14capacity__maskS2211;
          _M0L3pslS694 = 0;
          _M0L3idxS695 = _M0L6_2atmpS2210;
          continue;
        }
        moonbit_incref(_M0L4selfS697);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS697, _M0L3idxS695, _M0L14_2acurr__entryS706);
        _M0L7_2abindS707 = _M0L4selfS697->$6;
        _M0L7_2abindS708 = 0;
        _M0L5entryS709
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS709)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS709->$0 = _M0L7_2abindS707;
        _M0L5entryS709->$1 = _M0L7_2abindS708;
        _M0L5entryS709->$2 = _M0L3pslS694;
        _M0L5entryS709->$3 = _M0L4hashS699;
        _M0L5entryS709->$4 = _M0L3keyS703;
        _M0L5entryS709->$5 = _M0L5valueS704;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS709);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS706);
      }
      _M0L6_2atmpS2212 = _M0L3pslS694 + 1;
      _M0L6_2atmpS2214 = _M0L3idxS695 + 1;
      _M0L14capacity__maskS2215 = _M0L4selfS697->$3;
      _M0L6_2atmpS2213 = _M0L6_2atmpS2214 & _M0L14capacity__maskS2215;
      _M0L3pslS694 = _M0L6_2atmpS2212;
      _M0L3idxS695 = _M0L6_2atmpS2213;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS610,
  int32_t _M0L3idxS615,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS614
) {
  int32_t _M0L3pslS2098;
  int32_t _M0L6_2atmpS2094;
  int32_t _M0L6_2atmpS2096;
  int32_t _M0L14capacity__maskS2097;
  int32_t _M0L6_2atmpS2095;
  int32_t _M0L3pslS606;
  int32_t _M0L3idxS607;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS608;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2098 = _M0L5entryS614->$2;
  _M0L6_2atmpS2094 = _M0L3pslS2098 + 1;
  _M0L6_2atmpS2096 = _M0L3idxS615 + 1;
  _M0L14capacity__maskS2097 = _M0L4selfS610->$3;
  _M0L6_2atmpS2095 = _M0L6_2atmpS2096 & _M0L14capacity__maskS2097;
  _M0L3pslS606 = _M0L6_2atmpS2094;
  _M0L3idxS607 = _M0L6_2atmpS2095;
  _M0L5entryS608 = _M0L5entryS614;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3331 =
      _M0L4selfS610->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2093 =
      _M0L8_2afieldS3331;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3330;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS609;
    if (
      _M0L3idxS607 < 0
      || _M0L3idxS607 >= Moonbit_array_length(_M0L7entriesS2093)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3330
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2093[
        _M0L3idxS607
      ];
    _M0L7_2abindS609 = _M0L6_2atmpS3330;
    if (_M0L7_2abindS609 == 0) {
      _M0L5entryS608->$2 = _M0L3pslS606;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS610, _M0L5entryS608, _M0L3idxS607);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS612 =
        _M0L7_2abindS609;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS613 =
        _M0L7_2aSomeS612;
      int32_t _M0L3pslS2083 = _M0L14_2acurr__entryS613->$2;
      if (_M0L3pslS606 > _M0L3pslS2083) {
        int32_t _M0L3pslS2088;
        int32_t _M0L6_2atmpS2084;
        int32_t _M0L6_2atmpS2086;
        int32_t _M0L14capacity__maskS2087;
        int32_t _M0L6_2atmpS2085;
        _M0L5entryS608->$2 = _M0L3pslS606;
        moonbit_incref(_M0L14_2acurr__entryS613);
        moonbit_incref(_M0L4selfS610);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS610, _M0L5entryS608, _M0L3idxS607);
        _M0L3pslS2088 = _M0L14_2acurr__entryS613->$2;
        _M0L6_2atmpS2084 = _M0L3pslS2088 + 1;
        _M0L6_2atmpS2086 = _M0L3idxS607 + 1;
        _M0L14capacity__maskS2087 = _M0L4selfS610->$3;
        _M0L6_2atmpS2085 = _M0L6_2atmpS2086 & _M0L14capacity__maskS2087;
        _M0L3pslS606 = _M0L6_2atmpS2084;
        _M0L3idxS607 = _M0L6_2atmpS2085;
        _M0L5entryS608 = _M0L14_2acurr__entryS613;
        continue;
      } else {
        int32_t _M0L6_2atmpS2089 = _M0L3pslS606 + 1;
        int32_t _M0L6_2atmpS2091 = _M0L3idxS607 + 1;
        int32_t _M0L14capacity__maskS2092 = _M0L4selfS610->$3;
        int32_t _M0L6_2atmpS2090 =
          _M0L6_2atmpS2091 & _M0L14capacity__maskS2092;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3703 =
          _M0L5entryS608;
        _M0L3pslS606 = _M0L6_2atmpS2089;
        _M0L3idxS607 = _M0L6_2atmpS2090;
        _M0L5entryS608 = _tmp_3703;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS620,
  int32_t _M0L3idxS625,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS624
) {
  int32_t _M0L3pslS2114;
  int32_t _M0L6_2atmpS2110;
  int32_t _M0L6_2atmpS2112;
  int32_t _M0L14capacity__maskS2113;
  int32_t _M0L6_2atmpS2111;
  int32_t _M0L3pslS616;
  int32_t _M0L3idxS617;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS618;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2114 = _M0L5entryS624->$2;
  _M0L6_2atmpS2110 = _M0L3pslS2114 + 1;
  _M0L6_2atmpS2112 = _M0L3idxS625 + 1;
  _M0L14capacity__maskS2113 = _M0L4selfS620->$3;
  _M0L6_2atmpS2111 = _M0L6_2atmpS2112 & _M0L14capacity__maskS2113;
  _M0L3pslS616 = _M0L6_2atmpS2110;
  _M0L3idxS617 = _M0L6_2atmpS2111;
  _M0L5entryS618 = _M0L5entryS624;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3333 =
      _M0L4selfS620->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2109 =
      _M0L8_2afieldS3333;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3332;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS619;
    if (
      _M0L3idxS617 < 0
      || _M0L3idxS617 >= Moonbit_array_length(_M0L7entriesS2109)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3332
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2109[
        _M0L3idxS617
      ];
    _M0L7_2abindS619 = _M0L6_2atmpS3332;
    if (_M0L7_2abindS619 == 0) {
      _M0L5entryS618->$2 = _M0L3pslS616;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS620, _M0L5entryS618, _M0L3idxS617);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS622 =
        _M0L7_2abindS619;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS623 =
        _M0L7_2aSomeS622;
      int32_t _M0L3pslS2099 = _M0L14_2acurr__entryS623->$2;
      if (_M0L3pslS616 > _M0L3pslS2099) {
        int32_t _M0L3pslS2104;
        int32_t _M0L6_2atmpS2100;
        int32_t _M0L6_2atmpS2102;
        int32_t _M0L14capacity__maskS2103;
        int32_t _M0L6_2atmpS2101;
        _M0L5entryS618->$2 = _M0L3pslS616;
        moonbit_incref(_M0L14_2acurr__entryS623);
        moonbit_incref(_M0L4selfS620);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS620, _M0L5entryS618, _M0L3idxS617);
        _M0L3pslS2104 = _M0L14_2acurr__entryS623->$2;
        _M0L6_2atmpS2100 = _M0L3pslS2104 + 1;
        _M0L6_2atmpS2102 = _M0L3idxS617 + 1;
        _M0L14capacity__maskS2103 = _M0L4selfS620->$3;
        _M0L6_2atmpS2101 = _M0L6_2atmpS2102 & _M0L14capacity__maskS2103;
        _M0L3pslS616 = _M0L6_2atmpS2100;
        _M0L3idxS617 = _M0L6_2atmpS2101;
        _M0L5entryS618 = _M0L14_2acurr__entryS623;
        continue;
      } else {
        int32_t _M0L6_2atmpS2105 = _M0L3pslS616 + 1;
        int32_t _M0L6_2atmpS2107 = _M0L3idxS617 + 1;
        int32_t _M0L14capacity__maskS2108 = _M0L4selfS620->$3;
        int32_t _M0L6_2atmpS2106 =
          _M0L6_2atmpS2107 & _M0L14capacity__maskS2108;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3705 =
          _M0L5entryS618;
        _M0L3pslS616 = _M0L6_2atmpS2105;
        _M0L3idxS617 = _M0L6_2atmpS2106;
        _M0L5entryS618 = _tmp_3705;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS630,
  int32_t _M0L3idxS635,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5entryS634
) {
  int32_t _M0L3pslS2130;
  int32_t _M0L6_2atmpS2126;
  int32_t _M0L6_2atmpS2128;
  int32_t _M0L14capacity__maskS2129;
  int32_t _M0L6_2atmpS2127;
  int32_t _M0L3pslS626;
  int32_t _M0L3idxS627;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5entryS628;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2130 = _M0L5entryS634->$2;
  _M0L6_2atmpS2126 = _M0L3pslS2130 + 1;
  _M0L6_2atmpS2128 = _M0L3idxS635 + 1;
  _M0L14capacity__maskS2129 = _M0L4selfS630->$3;
  _M0L6_2atmpS2127 = _M0L6_2atmpS2128 & _M0L14capacity__maskS2129;
  _M0L3pslS626 = _M0L6_2atmpS2126;
  _M0L3idxS627 = _M0L6_2atmpS2127;
  _M0L5entryS628 = _M0L5entryS634;
  while (1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3335 =
      _M0L4selfS630->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2125 =
      _M0L8_2afieldS3335;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3334;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS629;
    if (
      _M0L3idxS627 < 0
      || _M0L3idxS627 >= Moonbit_array_length(_M0L7entriesS2125)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3334
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2125[
        _M0L3idxS627
      ];
    _M0L7_2abindS629 = _M0L6_2atmpS3334;
    if (_M0L7_2abindS629 == 0) {
      _M0L5entryS628->$2 = _M0L3pslS626;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS630, _M0L5entryS628, _M0L3idxS627);
      break;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS632 =
        _M0L7_2abindS629;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L14_2acurr__entryS633 =
        _M0L7_2aSomeS632;
      int32_t _M0L3pslS2115 = _M0L14_2acurr__entryS633->$2;
      if (_M0L3pslS626 > _M0L3pslS2115) {
        int32_t _M0L3pslS2120;
        int32_t _M0L6_2atmpS2116;
        int32_t _M0L6_2atmpS2118;
        int32_t _M0L14capacity__maskS2119;
        int32_t _M0L6_2atmpS2117;
        _M0L5entryS628->$2 = _M0L3pslS626;
        moonbit_incref(_M0L14_2acurr__entryS633);
        moonbit_incref(_M0L4selfS630);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(_M0L4selfS630, _M0L5entryS628, _M0L3idxS627);
        _M0L3pslS2120 = _M0L14_2acurr__entryS633->$2;
        _M0L6_2atmpS2116 = _M0L3pslS2120 + 1;
        _M0L6_2atmpS2118 = _M0L3idxS627 + 1;
        _M0L14capacity__maskS2119 = _M0L4selfS630->$3;
        _M0L6_2atmpS2117 = _M0L6_2atmpS2118 & _M0L14capacity__maskS2119;
        _M0L3pslS626 = _M0L6_2atmpS2116;
        _M0L3idxS627 = _M0L6_2atmpS2117;
        _M0L5entryS628 = _M0L14_2acurr__entryS633;
        continue;
      } else {
        int32_t _M0L6_2atmpS2121 = _M0L3pslS626 + 1;
        int32_t _M0L6_2atmpS2123 = _M0L3idxS627 + 1;
        int32_t _M0L14capacity__maskS2124 = _M0L4selfS630->$3;
        int32_t _M0L6_2atmpS2122 =
          _M0L6_2atmpS2123 & _M0L14capacity__maskS2124;
        struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _tmp_3707 =
          _M0L5entryS628;
        _M0L3pslS626 = _M0L6_2atmpS2121;
        _M0L3idxS627 = _M0L6_2atmpS2122;
        _M0L5entryS628 = _tmp_3707;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS640,
  int32_t _M0L3idxS645,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS644
) {
  int32_t _M0L3pslS2146;
  int32_t _M0L6_2atmpS2142;
  int32_t _M0L6_2atmpS2144;
  int32_t _M0L14capacity__maskS2145;
  int32_t _M0L6_2atmpS2143;
  int32_t _M0L3pslS636;
  int32_t _M0L3idxS637;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS638;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2146 = _M0L5entryS644->$2;
  _M0L6_2atmpS2142 = _M0L3pslS2146 + 1;
  _M0L6_2atmpS2144 = _M0L3idxS645 + 1;
  _M0L14capacity__maskS2145 = _M0L4selfS640->$3;
  _M0L6_2atmpS2143 = _M0L6_2atmpS2144 & _M0L14capacity__maskS2145;
  _M0L3pslS636 = _M0L6_2atmpS2142;
  _M0L3idxS637 = _M0L6_2atmpS2143;
  _M0L5entryS638 = _M0L5entryS644;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3337 = _M0L4selfS640->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2141 = _M0L8_2afieldS3337;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3336;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS639;
    if (
      _M0L3idxS637 < 0
      || _M0L3idxS637 >= Moonbit_array_length(_M0L7entriesS2141)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3336
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2141[_M0L3idxS637];
    _M0L7_2abindS639 = _M0L6_2atmpS3336;
    if (_M0L7_2abindS639 == 0) {
      _M0L5entryS638->$2 = _M0L3pslS636;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS640, _M0L5entryS638, _M0L3idxS637);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS642 = _M0L7_2abindS639;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS643 =
        _M0L7_2aSomeS642;
      int32_t _M0L3pslS2131 = _M0L14_2acurr__entryS643->$2;
      if (_M0L3pslS636 > _M0L3pslS2131) {
        int32_t _M0L3pslS2136;
        int32_t _M0L6_2atmpS2132;
        int32_t _M0L6_2atmpS2134;
        int32_t _M0L14capacity__maskS2135;
        int32_t _M0L6_2atmpS2133;
        _M0L5entryS638->$2 = _M0L3pslS636;
        moonbit_incref(_M0L14_2acurr__entryS643);
        moonbit_incref(_M0L4selfS640);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS640, _M0L5entryS638, _M0L3idxS637);
        _M0L3pslS2136 = _M0L14_2acurr__entryS643->$2;
        _M0L6_2atmpS2132 = _M0L3pslS2136 + 1;
        _M0L6_2atmpS2134 = _M0L3idxS637 + 1;
        _M0L14capacity__maskS2135 = _M0L4selfS640->$3;
        _M0L6_2atmpS2133 = _M0L6_2atmpS2134 & _M0L14capacity__maskS2135;
        _M0L3pslS636 = _M0L6_2atmpS2132;
        _M0L3idxS637 = _M0L6_2atmpS2133;
        _M0L5entryS638 = _M0L14_2acurr__entryS643;
        continue;
      } else {
        int32_t _M0L6_2atmpS2137 = _M0L3pslS636 + 1;
        int32_t _M0L6_2atmpS2139 = _M0L3idxS637 + 1;
        int32_t _M0L14capacity__maskS2140 = _M0L4selfS640->$3;
        int32_t _M0L6_2atmpS2138 =
          _M0L6_2atmpS2139 & _M0L14capacity__maskS2140;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_3709 = _M0L5entryS638;
        _M0L3pslS636 = _M0L6_2atmpS2137;
        _M0L3idxS637 = _M0L6_2atmpS2138;
        _M0L5entryS638 = _tmp_3709;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS582,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS584,
  int32_t _M0L8new__idxS583
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3340;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2075;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2076;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3339;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3338;
  int32_t _M0L6_2acntS3557;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS585;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3340 = _M0L4selfS582->$0;
  _M0L7entriesS2075 = _M0L8_2afieldS3340;
  moonbit_incref(_M0L5entryS584);
  _M0L6_2atmpS2076 = _M0L5entryS584;
  if (
    _M0L8new__idxS583 < 0
    || _M0L8new__idxS583 >= Moonbit_array_length(_M0L7entriesS2075)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3339
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2075[
      _M0L8new__idxS583
    ];
  if (_M0L6_2aoldS3339) {
    moonbit_decref(_M0L6_2aoldS3339);
  }
  _M0L7entriesS2075[_M0L8new__idxS583] = _M0L6_2atmpS2076;
  _M0L8_2afieldS3338 = _M0L5entryS584->$1;
  _M0L6_2acntS3557 = Moonbit_object_header(_M0L5entryS584)->rc;
  if (_M0L6_2acntS3557 > 1) {
    int32_t _M0L11_2anew__cntS3560 = _M0L6_2acntS3557 - 1;
    Moonbit_object_header(_M0L5entryS584)->rc = _M0L11_2anew__cntS3560;
    if (_M0L8_2afieldS3338) {
      moonbit_incref(_M0L8_2afieldS3338);
    }
  } else if (_M0L6_2acntS3557 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3559 =
      _M0L5entryS584->$5;
    moonbit_string_t _M0L8_2afieldS3558;
    moonbit_decref(_M0L8_2afieldS3559);
    _M0L8_2afieldS3558 = _M0L5entryS584->$4;
    moonbit_decref(_M0L8_2afieldS3558);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS584);
  }
  _M0L7_2abindS585 = _M0L8_2afieldS3338;
  if (_M0L7_2abindS585 == 0) {
    if (_M0L7_2abindS585) {
      moonbit_decref(_M0L7_2abindS585);
    }
    _M0L4selfS582->$6 = _M0L8new__idxS583;
    moonbit_decref(_M0L4selfS582);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS586;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS587;
    moonbit_decref(_M0L4selfS582);
    _M0L7_2aSomeS586 = _M0L7_2abindS585;
    _M0L7_2anextS587 = _M0L7_2aSomeS586;
    _M0L7_2anextS587->$0 = _M0L8new__idxS583;
    moonbit_decref(_M0L7_2anextS587);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS588,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS590,
  int32_t _M0L8new__idxS589
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3343;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2077;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2078;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3342;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3341;
  int32_t _M0L6_2acntS3561;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS591;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3343 = _M0L4selfS588->$0;
  _M0L7entriesS2077 = _M0L8_2afieldS3343;
  moonbit_incref(_M0L5entryS590);
  _M0L6_2atmpS2078 = _M0L5entryS590;
  if (
    _M0L8new__idxS589 < 0
    || _M0L8new__idxS589 >= Moonbit_array_length(_M0L7entriesS2077)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3342
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2077[
      _M0L8new__idxS589
    ];
  if (_M0L6_2aoldS3342) {
    moonbit_decref(_M0L6_2aoldS3342);
  }
  _M0L7entriesS2077[_M0L8new__idxS589] = _M0L6_2atmpS2078;
  _M0L8_2afieldS3341 = _M0L5entryS590->$1;
  _M0L6_2acntS3561 = Moonbit_object_header(_M0L5entryS590)->rc;
  if (_M0L6_2acntS3561 > 1) {
    int32_t _M0L11_2anew__cntS3563 = _M0L6_2acntS3561 - 1;
    Moonbit_object_header(_M0L5entryS590)->rc = _M0L11_2anew__cntS3563;
    if (_M0L8_2afieldS3341) {
      moonbit_incref(_M0L8_2afieldS3341);
    }
  } else if (_M0L6_2acntS3561 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3562 =
      _M0L5entryS590->$5;
    moonbit_decref(_M0L8_2afieldS3562);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS590);
  }
  _M0L7_2abindS591 = _M0L8_2afieldS3341;
  if (_M0L7_2abindS591 == 0) {
    if (_M0L7_2abindS591) {
      moonbit_decref(_M0L7_2abindS591);
    }
    _M0L4selfS588->$6 = _M0L8new__idxS589;
    moonbit_decref(_M0L4selfS588);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS592;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS593;
    moonbit_decref(_M0L4selfS588);
    _M0L7_2aSomeS592 = _M0L7_2abindS591;
    _M0L7_2anextS593 = _M0L7_2aSomeS592;
    _M0L7_2anextS593->$0 = _M0L8new__idxS589;
    moonbit_decref(_M0L7_2anextS593);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS594,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5entryS596,
  int32_t _M0L8new__idxS595
) {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3346;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2079;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2080;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3345;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L8_2afieldS3344;
  int32_t _M0L6_2acntS3564;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS597;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3346 = _M0L4selfS594->$0;
  _M0L7entriesS2079 = _M0L8_2afieldS3346;
  moonbit_incref(_M0L5entryS596);
  _M0L6_2atmpS2080 = _M0L5entryS596;
  if (
    _M0L8new__idxS595 < 0
    || _M0L8new__idxS595 >= Moonbit_array_length(_M0L7entriesS2079)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3345
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2079[
      _M0L8new__idxS595
    ];
  if (_M0L6_2aoldS3345) {
    moonbit_decref(_M0L6_2aoldS3345);
  }
  _M0L7entriesS2079[_M0L8new__idxS595] = _M0L6_2atmpS2080;
  _M0L8_2afieldS3344 = _M0L5entryS596->$1;
  _M0L6_2acntS3564 = Moonbit_object_header(_M0L5entryS596)->rc;
  if (_M0L6_2acntS3564 > 1) {
    int32_t _M0L11_2anew__cntS3567 = _M0L6_2acntS3564 - 1;
    Moonbit_object_header(_M0L5entryS596)->rc = _M0L11_2anew__cntS3567;
    if (_M0L8_2afieldS3344) {
      moonbit_incref(_M0L8_2afieldS3344);
    }
  } else if (_M0L6_2acntS3564 == 1) {
    struct _M0TP48clawteam8clawteam8internal3lru5EntryGiE* _M0L8_2afieldS3566 =
      _M0L5entryS596->$5;
    moonbit_string_t _M0L8_2afieldS3565;
    moonbit_decref(_M0L8_2afieldS3566);
    _M0L8_2afieldS3565 = _M0L5entryS596->$4;
    moonbit_decref(_M0L8_2afieldS3565);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS596);
  }
  _M0L7_2abindS597 = _M0L8_2afieldS3344;
  if (_M0L7_2abindS597 == 0) {
    if (_M0L7_2abindS597) {
      moonbit_decref(_M0L7_2abindS597);
    }
    _M0L4selfS594->$6 = _M0L8new__idxS595;
    moonbit_decref(_M0L4selfS594);
  } else {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS598;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2anextS599;
    moonbit_decref(_M0L4selfS594);
    _M0L7_2aSomeS598 = _M0L7_2abindS597;
    _M0L7_2anextS599 = _M0L7_2aSomeS598;
    _M0L7_2anextS599->$0 = _M0L8new__idxS595;
    moonbit_decref(_M0L7_2anextS599);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS600,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS602,
  int32_t _M0L8new__idxS601
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3349;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2081;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2082;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3348;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3347;
  int32_t _M0L6_2acntS3568;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS603;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3349 = _M0L4selfS600->$0;
  _M0L7entriesS2081 = _M0L8_2afieldS3349;
  moonbit_incref(_M0L5entryS602);
  _M0L6_2atmpS2082 = _M0L5entryS602;
  if (
    _M0L8new__idxS601 < 0
    || _M0L8new__idxS601 >= Moonbit_array_length(_M0L7entriesS2081)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3348
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2081[_M0L8new__idxS601];
  if (_M0L6_2aoldS3348) {
    moonbit_decref(_M0L6_2aoldS3348);
  }
  _M0L7entriesS2081[_M0L8new__idxS601] = _M0L6_2atmpS2082;
  _M0L8_2afieldS3347 = _M0L5entryS602->$1;
  _M0L6_2acntS3568 = Moonbit_object_header(_M0L5entryS602)->rc;
  if (_M0L6_2acntS3568 > 1) {
    int32_t _M0L11_2anew__cntS3571 = _M0L6_2acntS3568 - 1;
    Moonbit_object_header(_M0L5entryS602)->rc = _M0L11_2anew__cntS3571;
    if (_M0L8_2afieldS3347) {
      moonbit_incref(_M0L8_2afieldS3347);
    }
  } else if (_M0L6_2acntS3568 == 1) {
    void* _M0L8_2afieldS3570 = _M0L5entryS602->$5;
    moonbit_string_t _M0L8_2afieldS3569;
    moonbit_decref(_M0L8_2afieldS3570);
    _M0L8_2afieldS3569 = _M0L5entryS602->$4;
    moonbit_decref(_M0L8_2afieldS3569);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS602);
  }
  _M0L7_2abindS603 = _M0L8_2afieldS3347;
  if (_M0L7_2abindS603 == 0) {
    if (_M0L7_2abindS603) {
      moonbit_decref(_M0L7_2abindS603);
    }
    _M0L4selfS600->$6 = _M0L8new__idxS601;
    moonbit_decref(_M0L4selfS600);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS604;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS605;
    moonbit_decref(_M0L4selfS600);
    _M0L7_2aSomeS604 = _M0L7_2abindS603;
    _M0L7_2anextS605 = _M0L7_2aSomeS604;
    _M0L7_2anextS605->$0 = _M0L8new__idxS601;
    moonbit_decref(_M0L7_2anextS605);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS567,
  int32_t _M0L3idxS569,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS568
) {
  int32_t _M0L7_2abindS566;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3351;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2044;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2045;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3350;
  int32_t _M0L4sizeS2047;
  int32_t _M0L6_2atmpS2046;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS566 = _M0L4selfS567->$6;
  switch (_M0L7_2abindS566) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2039;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3352;
      moonbit_incref(_M0L5entryS568);
      _M0L6_2atmpS2039 = _M0L5entryS568;
      _M0L6_2aoldS3352 = _M0L4selfS567->$5;
      if (_M0L6_2aoldS3352) {
        moonbit_decref(_M0L6_2aoldS3352);
      }
      _M0L4selfS567->$5 = _M0L6_2atmpS2039;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3355 =
        _M0L4selfS567->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2043 =
        _M0L8_2afieldS3355;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3354;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2042;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2040;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2041;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3353;
      if (
        _M0L7_2abindS566 < 0
        || _M0L7_2abindS566 >= Moonbit_array_length(_M0L7entriesS2043)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3354
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2043[
          _M0L7_2abindS566
        ];
      _M0L6_2atmpS2042 = _M0L6_2atmpS3354;
      if (_M0L6_2atmpS2042) {
        moonbit_incref(_M0L6_2atmpS2042);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2040
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2042);
      moonbit_incref(_M0L5entryS568);
      _M0L6_2atmpS2041 = _M0L5entryS568;
      _M0L6_2aoldS3353 = _M0L6_2atmpS2040->$1;
      if (_M0L6_2aoldS3353) {
        moonbit_decref(_M0L6_2aoldS3353);
      }
      _M0L6_2atmpS2040->$1 = _M0L6_2atmpS2041;
      moonbit_decref(_M0L6_2atmpS2040);
      break;
    }
  }
  _M0L4selfS567->$6 = _M0L3idxS569;
  _M0L8_2afieldS3351 = _M0L4selfS567->$0;
  _M0L7entriesS2044 = _M0L8_2afieldS3351;
  _M0L6_2atmpS2045 = _M0L5entryS568;
  if (
    _M0L3idxS569 < 0
    || _M0L3idxS569 >= Moonbit_array_length(_M0L7entriesS2044)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3350
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2044[
      _M0L3idxS569
    ];
  if (_M0L6_2aoldS3350) {
    moonbit_decref(_M0L6_2aoldS3350);
  }
  _M0L7entriesS2044[_M0L3idxS569] = _M0L6_2atmpS2045;
  _M0L4sizeS2047 = _M0L4selfS567->$1;
  _M0L6_2atmpS2046 = _M0L4sizeS2047 + 1;
  _M0L4selfS567->$1 = _M0L6_2atmpS2046;
  moonbit_decref(_M0L4selfS567);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS571,
  int32_t _M0L3idxS573,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS572
) {
  int32_t _M0L7_2abindS570;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3357;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2053;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2054;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3356;
  int32_t _M0L4sizeS2056;
  int32_t _M0L6_2atmpS2055;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS570 = _M0L4selfS571->$6;
  switch (_M0L7_2abindS570) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2048;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3358;
      moonbit_incref(_M0L5entryS572);
      _M0L6_2atmpS2048 = _M0L5entryS572;
      _M0L6_2aoldS3358 = _M0L4selfS571->$5;
      if (_M0L6_2aoldS3358) {
        moonbit_decref(_M0L6_2aoldS3358);
      }
      _M0L4selfS571->$5 = _M0L6_2atmpS2048;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3361 =
        _M0L4selfS571->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2052 =
        _M0L8_2afieldS3361;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3360;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2051;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2049;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2050;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3359;
      if (
        _M0L7_2abindS570 < 0
        || _M0L7_2abindS570 >= Moonbit_array_length(_M0L7entriesS2052)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3360
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2052[
          _M0L7_2abindS570
        ];
      _M0L6_2atmpS2051 = _M0L6_2atmpS3360;
      if (_M0L6_2atmpS2051) {
        moonbit_incref(_M0L6_2atmpS2051);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2049
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2051);
      moonbit_incref(_M0L5entryS572);
      _M0L6_2atmpS2050 = _M0L5entryS572;
      _M0L6_2aoldS3359 = _M0L6_2atmpS2049->$1;
      if (_M0L6_2aoldS3359) {
        moonbit_decref(_M0L6_2aoldS3359);
      }
      _M0L6_2atmpS2049->$1 = _M0L6_2atmpS2050;
      moonbit_decref(_M0L6_2atmpS2049);
      break;
    }
  }
  _M0L4selfS571->$6 = _M0L3idxS573;
  _M0L8_2afieldS3357 = _M0L4selfS571->$0;
  _M0L7entriesS2053 = _M0L8_2afieldS3357;
  _M0L6_2atmpS2054 = _M0L5entryS572;
  if (
    _M0L3idxS573 < 0
    || _M0L3idxS573 >= Moonbit_array_length(_M0L7entriesS2053)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3356
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2053[
      _M0L3idxS573
    ];
  if (_M0L6_2aoldS3356) {
    moonbit_decref(_M0L6_2aoldS3356);
  }
  _M0L7entriesS2053[_M0L3idxS573] = _M0L6_2atmpS2054;
  _M0L4sizeS2056 = _M0L4selfS571->$1;
  _M0L6_2atmpS2055 = _M0L4sizeS2056 + 1;
  _M0L4selfS571->$1 = _M0L6_2atmpS2055;
  moonbit_decref(_M0L4selfS571);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS575,
  int32_t _M0L3idxS577,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L5entryS576
) {
  int32_t _M0L7_2abindS574;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3363;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2062;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2063;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3362;
  int32_t _M0L4sizeS2065;
  int32_t _M0L6_2atmpS2064;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS574 = _M0L4selfS575->$6;
  switch (_M0L7_2abindS574) {
    case -1: {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2057;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3364;
      moonbit_incref(_M0L5entryS576);
      _M0L6_2atmpS2057 = _M0L5entryS576;
      _M0L6_2aoldS3364 = _M0L4selfS575->$5;
      if (_M0L6_2aoldS3364) {
        moonbit_decref(_M0L6_2aoldS3364);
      }
      _M0L4selfS575->$5 = _M0L6_2atmpS2057;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L8_2afieldS3367 =
        _M0L4selfS575->$0;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7entriesS2061 =
        _M0L8_2afieldS3367;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS3366;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2060;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2058;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2059;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2aoldS3365;
      if (
        _M0L7_2abindS574 < 0
        || _M0L7_2abindS574 >= Moonbit_array_length(_M0L7entriesS2061)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3366
      = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2061[
          _M0L7_2abindS574
        ];
      _M0L6_2atmpS2060 = _M0L6_2atmpS3366;
      if (_M0L6_2atmpS2060) {
        moonbit_incref(_M0L6_2atmpS2060);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2058
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE(_M0L6_2atmpS2060);
      moonbit_incref(_M0L5entryS576);
      _M0L6_2atmpS2059 = _M0L5entryS576;
      _M0L6_2aoldS3365 = _M0L6_2atmpS2058->$1;
      if (_M0L6_2aoldS3365) {
        moonbit_decref(_M0L6_2aoldS3365);
      }
      _M0L6_2atmpS2058->$1 = _M0L6_2atmpS2059;
      moonbit_decref(_M0L6_2atmpS2058);
      break;
    }
  }
  _M0L4selfS575->$6 = _M0L3idxS577;
  _M0L8_2afieldS3363 = _M0L4selfS575->$0;
  _M0L7entriesS2062 = _M0L8_2afieldS3363;
  _M0L6_2atmpS2063 = _M0L5entryS576;
  if (
    _M0L3idxS577 < 0
    || _M0L3idxS577 >= Moonbit_array_length(_M0L7entriesS2062)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3362
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)_M0L7entriesS2062[
      _M0L3idxS577
    ];
  if (_M0L6_2aoldS3362) {
    moonbit_decref(_M0L6_2aoldS3362);
  }
  _M0L7entriesS2062[_M0L3idxS577] = _M0L6_2atmpS2063;
  _M0L4sizeS2065 = _M0L4selfS575->$1;
  _M0L6_2atmpS2064 = _M0L4sizeS2065 + 1;
  _M0L4selfS575->$1 = _M0L6_2atmpS2064;
  moonbit_decref(_M0L4selfS575);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS579,
  int32_t _M0L3idxS581,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS580
) {
  int32_t _M0L7_2abindS578;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3369;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2071;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2072;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3368;
  int32_t _M0L4sizeS2074;
  int32_t _M0L6_2atmpS2073;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS578 = _M0L4selfS579->$6;
  switch (_M0L7_2abindS578) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2066;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3370;
      moonbit_incref(_M0L5entryS580);
      _M0L6_2atmpS2066 = _M0L5entryS580;
      _M0L6_2aoldS3370 = _M0L4selfS579->$5;
      if (_M0L6_2aoldS3370) {
        moonbit_decref(_M0L6_2aoldS3370);
      }
      _M0L4selfS579->$5 = _M0L6_2atmpS2066;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3373 = _M0L4selfS579->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2070 = _M0L8_2afieldS3373;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3372;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2069;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2067;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2068;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3371;
      if (
        _M0L7_2abindS578 < 0
        || _M0L7_2abindS578 >= Moonbit_array_length(_M0L7entriesS2070)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3372
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2070[_M0L7_2abindS578];
      _M0L6_2atmpS2069 = _M0L6_2atmpS3372;
      if (_M0L6_2atmpS2069) {
        moonbit_incref(_M0L6_2atmpS2069);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2067
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2069);
      moonbit_incref(_M0L5entryS580);
      _M0L6_2atmpS2068 = _M0L5entryS580;
      _M0L6_2aoldS3371 = _M0L6_2atmpS2067->$1;
      if (_M0L6_2aoldS3371) {
        moonbit_decref(_M0L6_2aoldS3371);
      }
      _M0L6_2atmpS2067->$1 = _M0L6_2atmpS2068;
      moonbit_decref(_M0L6_2atmpS2067);
      break;
    }
  }
  _M0L4selfS579->$6 = _M0L3idxS581;
  _M0L8_2afieldS3369 = _M0L4selfS579->$0;
  _M0L7entriesS2071 = _M0L8_2afieldS3369;
  _M0L6_2atmpS2072 = _M0L5entryS580;
  if (
    _M0L3idxS581 < 0
    || _M0L3idxS581 >= Moonbit_array_length(_M0L7entriesS2071)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3368
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2071[_M0L3idxS581];
  if (_M0L6_2aoldS3368) {
    moonbit_decref(_M0L6_2aoldS3368);
  }
  _M0L7entriesS2071[_M0L3idxS581] = _M0L6_2atmpS2072;
  _M0L4sizeS2074 = _M0L4selfS579->$1;
  _M0L6_2atmpS2073 = _M0L4sizeS2074 + 1;
  _M0L4selfS579->$1 = _M0L6_2atmpS2073;
  moonbit_decref(_M0L4selfS579);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS543
) {
  int32_t _M0L8capacityS542;
  int32_t _M0L7_2abindS544;
  int32_t _M0L7_2abindS545;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2035;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS546;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS547;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3710;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS542
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS543);
  _M0L7_2abindS544 = _M0L8capacityS542 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS545 = _M0FPB21calc__grow__threshold(_M0L8capacityS542);
  _M0L6_2atmpS2035 = 0;
  _M0L7_2abindS546
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS542, _M0L6_2atmpS2035);
  _M0L7_2abindS547 = 0;
  _block_3710
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3710)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3710->$0 = _M0L7_2abindS546;
  _block_3710->$1 = 0;
  _block_3710->$2 = _M0L8capacityS542;
  _block_3710->$3 = _M0L7_2abindS544;
  _block_3710->$4 = _M0L7_2abindS545;
  _block_3710->$5 = _M0L7_2abindS547;
  _block_3710->$6 = -1;
  return _block_3710;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS549
) {
  int32_t _M0L8capacityS548;
  int32_t _M0L7_2abindS550;
  int32_t _M0L7_2abindS551;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2036;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS552;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS553;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3711;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS548
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS549);
  _M0L7_2abindS550 = _M0L8capacityS548 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS551 = _M0FPB21calc__grow__threshold(_M0L8capacityS548);
  _M0L6_2atmpS2036 = 0;
  _M0L7_2abindS552
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS548, _M0L6_2atmpS2036);
  _M0L7_2abindS553 = 0;
  _block_3711
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3711)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3711->$0 = _M0L7_2abindS552;
  _block_3711->$1 = 0;
  _block_3711->$2 = _M0L8capacityS548;
  _block_3711->$3 = _M0L7_2abindS550;
  _block_3711->$4 = _M0L7_2abindS551;
  _block_3711->$5 = _M0L7_2abindS553;
  _block_3711->$6 = -1;
  return _block_3711;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS555
) {
  int32_t _M0L8capacityS554;
  int32_t _M0L7_2abindS556;
  int32_t _M0L7_2abindS557;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2037;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS558;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS559;
  struct _M0TPB3MapGsRPB4JsonE* _block_3712;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS554
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS555);
  _M0L7_2abindS556 = _M0L8capacityS554 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS557 = _M0FPB21calc__grow__threshold(_M0L8capacityS554);
  _M0L6_2atmpS2037 = 0;
  _M0L7_2abindS558
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS554, _M0L6_2atmpS2037);
  _M0L7_2abindS559 = 0;
  _block_3712
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_3712)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_3712->$0 = _M0L7_2abindS558;
  _block_3712->$1 = 0;
  _block_3712->$2 = _M0L8capacityS554;
  _block_3712->$3 = _M0L7_2abindS556;
  _block_3712->$4 = _M0L7_2abindS557;
  _block_3712->$5 = _M0L7_2abindS559;
  _block_3712->$6 = -1;
  return _block_3712;
}

struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB3Map11new_2einnerGsRP48clawteam8clawteam8internal3lru5EntryGiEE(
  int32_t _M0L8capacityS561
) {
  int32_t _M0L8capacityS560;
  int32_t _M0L7_2abindS562;
  int32_t _M0L7_2abindS563;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L6_2atmpS2038;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE** _M0L7_2abindS564;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2abindS565;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _block_3713;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS560
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS561);
  _M0L7_2abindS562 = _M0L8capacityS560 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS563 = _M0FPB21calc__grow__threshold(_M0L8capacityS560);
  _M0L6_2atmpS2038 = 0;
  _M0L7_2abindS564
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE**)moonbit_make_ref_array(_M0L8capacityS560, _M0L6_2atmpS2038);
  _M0L7_2abindS565 = 0;
  _block_3713
  = (struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE));
  Moonbit_object_header(_block_3713)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRP48clawteam8clawteam8internal3lru5EntryGiEE, $0) >> 2, 2, 0);
  _block_3713->$0 = _M0L7_2abindS564;
  _block_3713->$1 = 0;
  _block_3713->$2 = _M0L8capacityS560;
  _block_3713->$3 = _M0L7_2abindS562;
  _block_3713->$4 = _M0L7_2abindS563;
  _block_3713->$5 = _M0L7_2abindS565;
  _block_3713->$6 = -1;
  return _block_3713;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS541) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS541 >= 0) {
    int32_t _M0L6_2atmpS2034;
    int32_t _M0L6_2atmpS2033;
    int32_t _M0L6_2atmpS2032;
    int32_t _M0L6_2atmpS2031;
    if (_M0L4selfS541 <= 1) {
      return 1;
    }
    if (_M0L4selfS541 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2034 = _M0L4selfS541 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2033 = moonbit_clz32(_M0L6_2atmpS2034);
    _M0L6_2atmpS2032 = _M0L6_2atmpS2033 - 1;
    _M0L6_2atmpS2031 = 2147483647 >> (_M0L6_2atmpS2032 & 31);
    return _M0L6_2atmpS2031 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS540) {
  int32_t _M0L6_2atmpS2030;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2030 = _M0L8capacityS540 * 13;
  return _M0L6_2atmpS2030 / 16;
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS530) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS530 == 4294967296ll) {
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS531 = _M0L4selfS530;
    return (int32_t)_M0L7_2aSomeS531;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS532
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS532 == 0) {
    if (_M0L4selfS532) {
      moonbit_decref(_M0L4selfS532);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS533 =
      _M0L4selfS532;
    return _M0L7_2aSomeS533;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS534
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS534 == 0) {
    if (_M0L4selfS534) {
      moonbit_decref(_M0L4selfS534);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS535 =
      _M0L4selfS534;
    return _M0L7_2aSomeS535;
  }
}

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS536
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS536 == 0) {
    if (_M0L4selfS536) {
      moonbit_decref(_M0L4selfS536);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2aSomeS537 =
      _M0L4selfS536;
    return _M0L7_2aSomeS537;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS538
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS538 == 0) {
    if (_M0L4selfS538) {
      moonbit_decref(_M0L4selfS538);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS539 = _M0L4selfS538;
    return _M0L7_2aSomeS539;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS529
) {
  moonbit_string_t* _M0L6_2atmpS2029;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2029 = _M0L4selfS529;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2029);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS525,
  int32_t _M0L5indexS526
) {
  uint64_t* _M0L6_2atmpS2027;
  uint64_t _M0L6_2atmpS3374;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2027 = _M0L4selfS525;
  if (
    _M0L5indexS526 < 0
    || _M0L5indexS526 >= Moonbit_array_length(_M0L6_2atmpS2027)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3374 = (uint64_t)_M0L6_2atmpS2027[_M0L5indexS526];
  moonbit_decref(_M0L6_2atmpS2027);
  return _M0L6_2atmpS3374;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS527,
  int32_t _M0L5indexS528
) {
  uint32_t* _M0L6_2atmpS2028;
  uint32_t _M0L6_2atmpS3375;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2028 = _M0L4selfS527;
  if (
    _M0L5indexS528 < 0
    || _M0L5indexS528 >= Moonbit_array_length(_M0L6_2atmpS2028)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3375 = (uint32_t)_M0L6_2atmpS2028[_M0L5indexS528];
  moonbit_decref(_M0L6_2atmpS2028);
  return _M0L6_2atmpS3375;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS524
) {
  moonbit_string_t* _M0L6_2atmpS2025;
  int32_t _M0L6_2atmpS3376;
  int32_t _M0L6_2atmpS2026;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2024;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS524);
  _M0L6_2atmpS2025 = _M0L4selfS524;
  _M0L6_2atmpS3376 = Moonbit_array_length(_M0L4selfS524);
  moonbit_decref(_M0L4selfS524);
  _M0L6_2atmpS2026 = _M0L6_2atmpS3376;
  _M0L6_2atmpS2024
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2026, _M0L6_2atmpS2025
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2024);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS522
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS521;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__* _closure_3714;
  struct _M0TWEOs* _M0L6_2atmpS2012;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS521
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS521)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS521->$0 = 0;
  _closure_3714
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__));
  Moonbit_object_header(_closure_3714)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__, $0_0) >> 2, 2, 0);
  _closure_3714->code = &_M0MPC15array9ArrayView4iterGsEC2013l570;
  _closure_3714->$0_0 = _M0L4selfS522.$0;
  _closure_3714->$0_1 = _M0L4selfS522.$1;
  _closure_3714->$0_2 = _M0L4selfS522.$2;
  _closure_3714->$1 = _M0L1iS521;
  _M0L6_2atmpS2012 = (struct _M0TWEOs*)_closure_3714;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2012);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2013l570(
  struct _M0TWEOs* _M0L6_2aenvS2014
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__* _M0L14_2acasted__envS2015;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3381;
  struct _M0TPC13ref3RefGiE* _M0L1iS521;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3380;
  int32_t _M0L6_2acntS3572;
  struct _M0TPB9ArrayViewGsE _M0L4selfS522;
  int32_t _M0L3valS2016;
  int32_t _M0L6_2atmpS2017;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2015
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2013__l570__*)_M0L6_2aenvS2014;
  _M0L8_2afieldS3381 = _M0L14_2acasted__envS2015->$1;
  _M0L1iS521 = _M0L8_2afieldS3381;
  _M0L8_2afieldS3380
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2015->$0_1,
      _M0L14_2acasted__envS2015->$0_2,
      _M0L14_2acasted__envS2015->$0_0
  };
  _M0L6_2acntS3572 = Moonbit_object_header(_M0L14_2acasted__envS2015)->rc;
  if (_M0L6_2acntS3572 > 1) {
    int32_t _M0L11_2anew__cntS3573 = _M0L6_2acntS3572 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2015)->rc
    = _M0L11_2anew__cntS3573;
    moonbit_incref(_M0L1iS521);
    moonbit_incref(_M0L8_2afieldS3380.$0);
  } else if (_M0L6_2acntS3572 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2015);
  }
  _M0L4selfS522 = _M0L8_2afieldS3380;
  _M0L3valS2016 = _M0L1iS521->$0;
  moonbit_incref(_M0L4selfS522.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2017 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS522);
  if (_M0L3valS2016 < _M0L6_2atmpS2017) {
    moonbit_string_t* _M0L8_2afieldS3379 = _M0L4selfS522.$0;
    moonbit_string_t* _M0L3bufS2020 = _M0L8_2afieldS3379;
    int32_t _M0L8_2afieldS3378 = _M0L4selfS522.$1;
    int32_t _M0L5startS2022 = _M0L8_2afieldS3378;
    int32_t _M0L3valS2023 = _M0L1iS521->$0;
    int32_t _M0L6_2atmpS2021 = _M0L5startS2022 + _M0L3valS2023;
    moonbit_string_t _M0L6_2atmpS3377 =
      (moonbit_string_t)_M0L3bufS2020[_M0L6_2atmpS2021];
    moonbit_string_t _M0L4elemS523;
    int32_t _M0L3valS2019;
    int32_t _M0L6_2atmpS2018;
    moonbit_incref(_M0L6_2atmpS3377);
    moonbit_decref(_M0L3bufS2020);
    _M0L4elemS523 = _M0L6_2atmpS3377;
    _M0L3valS2019 = _M0L1iS521->$0;
    _M0L6_2atmpS2018 = _M0L3valS2019 + 1;
    _M0L1iS521->$0 = _M0L6_2atmpS2018;
    moonbit_decref(_M0L1iS521);
    return _M0L4elemS523;
  } else {
    moonbit_decref(_M0L4selfS522.$0);
    moonbit_decref(_M0L1iS521);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS520
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS520;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS519,
  struct _M0TPB6Logger _M0L6loggerS518
) {
  moonbit_string_t _M0L6_2atmpS2011;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2011
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS519, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS518.$0->$method_0(_M0L6loggerS518.$1, _M0L6_2atmpS2011);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS517,
  struct _M0TPB6Logger _M0L6loggerS516
) {
  moonbit_string_t _M0L6_2atmpS2010;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2010 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS517, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS516.$0->$method_0(_M0L6loggerS516.$1, _M0L6_2atmpS2010);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS511) {
  int32_t _M0L3lenS510;
  struct _M0TPC13ref3RefGiE* _M0L5indexS512;
  struct _M0R38String_3a_3aiter_2eanon__u1994__l247__* _closure_3715;
  struct _M0TWEOc* _M0L6_2atmpS1993;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS510 = Moonbit_array_length(_M0L4selfS511);
  _M0L5indexS512
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS512)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS512->$0 = 0;
  _closure_3715
  = (struct _M0R38String_3a_3aiter_2eanon__u1994__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u1994__l247__));
  Moonbit_object_header(_closure_3715)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u1994__l247__, $0) >> 2, 2, 0);
  _closure_3715->code = &_M0MPC16string6String4iterC1994l247;
  _closure_3715->$0 = _M0L5indexS512;
  _closure_3715->$1 = _M0L4selfS511;
  _closure_3715->$2 = _M0L3lenS510;
  _M0L6_2atmpS1993 = (struct _M0TWEOc*)_closure_3715;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1993);
}

int32_t _M0MPC16string6String4iterC1994l247(
  struct _M0TWEOc* _M0L6_2aenvS1995
) {
  struct _M0R38String_3a_3aiter_2eanon__u1994__l247__* _M0L14_2acasted__envS1996;
  int32_t _M0L3lenS510;
  moonbit_string_t _M0L8_2afieldS3384;
  moonbit_string_t _M0L4selfS511;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3383;
  int32_t _M0L6_2acntS3574;
  struct _M0TPC13ref3RefGiE* _M0L5indexS512;
  int32_t _M0L3valS1997;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS1996
  = (struct _M0R38String_3a_3aiter_2eanon__u1994__l247__*)_M0L6_2aenvS1995;
  _M0L3lenS510 = _M0L14_2acasted__envS1996->$2;
  _M0L8_2afieldS3384 = _M0L14_2acasted__envS1996->$1;
  _M0L4selfS511 = _M0L8_2afieldS3384;
  _M0L8_2afieldS3383 = _M0L14_2acasted__envS1996->$0;
  _M0L6_2acntS3574 = Moonbit_object_header(_M0L14_2acasted__envS1996)->rc;
  if (_M0L6_2acntS3574 > 1) {
    int32_t _M0L11_2anew__cntS3575 = _M0L6_2acntS3574 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1996)->rc
    = _M0L11_2anew__cntS3575;
    moonbit_incref(_M0L4selfS511);
    moonbit_incref(_M0L8_2afieldS3383);
  } else if (_M0L6_2acntS3574 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS1996);
  }
  _M0L5indexS512 = _M0L8_2afieldS3383;
  _M0L3valS1997 = _M0L5indexS512->$0;
  if (_M0L3valS1997 < _M0L3lenS510) {
    int32_t _M0L3valS2009 = _M0L5indexS512->$0;
    int32_t _M0L2c1S513 = _M0L4selfS511[_M0L3valS2009];
    int32_t _if__result_3716;
    int32_t _M0L3valS2007;
    int32_t _M0L6_2atmpS2006;
    int32_t _M0L6_2atmpS2008;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S513)) {
      int32_t _M0L3valS1999 = _M0L5indexS512->$0;
      int32_t _M0L6_2atmpS1998 = _M0L3valS1999 + 1;
      _if__result_3716 = _M0L6_2atmpS1998 < _M0L3lenS510;
    } else {
      _if__result_3716 = 0;
    }
    if (_if__result_3716) {
      int32_t _M0L3valS2005 = _M0L5indexS512->$0;
      int32_t _M0L6_2atmpS2004 = _M0L3valS2005 + 1;
      int32_t _M0L6_2atmpS3382 = _M0L4selfS511[_M0L6_2atmpS2004];
      int32_t _M0L2c2S514;
      moonbit_decref(_M0L4selfS511);
      _M0L2c2S514 = _M0L6_2atmpS3382;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S514)) {
        int32_t _M0L6_2atmpS2002 = (int32_t)_M0L2c1S513;
        int32_t _M0L6_2atmpS2003 = (int32_t)_M0L2c2S514;
        int32_t _M0L1cS515;
        int32_t _M0L3valS2001;
        int32_t _M0L6_2atmpS2000;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS515
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2002, _M0L6_2atmpS2003);
        _M0L3valS2001 = _M0L5indexS512->$0;
        _M0L6_2atmpS2000 = _M0L3valS2001 + 2;
        _M0L5indexS512->$0 = _M0L6_2atmpS2000;
        moonbit_decref(_M0L5indexS512);
        return _M0L1cS515;
      }
    } else {
      moonbit_decref(_M0L4selfS511);
    }
    _M0L3valS2007 = _M0L5indexS512->$0;
    _M0L6_2atmpS2006 = _M0L3valS2007 + 1;
    _M0L5indexS512->$0 = _M0L6_2atmpS2006;
    moonbit_decref(_M0L5indexS512);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2008 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S513);
    return _M0L6_2atmpS2008;
  } else {
    moonbit_decref(_M0L5indexS512);
    moonbit_decref(_M0L4selfS511);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS501,
  moonbit_string_t _M0L5valueS503
) {
  int32_t _M0L3lenS1978;
  moonbit_string_t* _M0L6_2atmpS1980;
  int32_t _M0L6_2atmpS3387;
  int32_t _M0L6_2atmpS1979;
  int32_t _M0L6lengthS502;
  moonbit_string_t* _M0L8_2afieldS3386;
  moonbit_string_t* _M0L3bufS1981;
  moonbit_string_t _M0L6_2aoldS3385;
  int32_t _M0L6_2atmpS1982;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1978 = _M0L4selfS501->$1;
  moonbit_incref(_M0L4selfS501);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1980 = _M0MPC15array5Array6bufferGsE(_M0L4selfS501);
  _M0L6_2atmpS3387 = Moonbit_array_length(_M0L6_2atmpS1980);
  moonbit_decref(_M0L6_2atmpS1980);
  _M0L6_2atmpS1979 = _M0L6_2atmpS3387;
  if (_M0L3lenS1978 == _M0L6_2atmpS1979) {
    moonbit_incref(_M0L4selfS501);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS501);
  }
  _M0L6lengthS502 = _M0L4selfS501->$1;
  _M0L8_2afieldS3386 = _M0L4selfS501->$0;
  _M0L3bufS1981 = _M0L8_2afieldS3386;
  _M0L6_2aoldS3385 = (moonbit_string_t)_M0L3bufS1981[_M0L6lengthS502];
  moonbit_decref(_M0L6_2aoldS3385);
  _M0L3bufS1981[_M0L6lengthS502] = _M0L5valueS503;
  _M0L6_2atmpS1982 = _M0L6lengthS502 + 1;
  _M0L4selfS501->$1 = _M0L6_2atmpS1982;
  moonbit_decref(_M0L4selfS501);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS504,
  struct _M0TUsiE* _M0L5valueS506
) {
  int32_t _M0L3lenS1983;
  struct _M0TUsiE** _M0L6_2atmpS1985;
  int32_t _M0L6_2atmpS3390;
  int32_t _M0L6_2atmpS1984;
  int32_t _M0L6lengthS505;
  struct _M0TUsiE** _M0L8_2afieldS3389;
  struct _M0TUsiE** _M0L3bufS1986;
  struct _M0TUsiE* _M0L6_2aoldS3388;
  int32_t _M0L6_2atmpS1987;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1983 = _M0L4selfS504->$1;
  moonbit_incref(_M0L4selfS504);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1985 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS504);
  _M0L6_2atmpS3390 = Moonbit_array_length(_M0L6_2atmpS1985);
  moonbit_decref(_M0L6_2atmpS1985);
  _M0L6_2atmpS1984 = _M0L6_2atmpS3390;
  if (_M0L3lenS1983 == _M0L6_2atmpS1984) {
    moonbit_incref(_M0L4selfS504);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS504);
  }
  _M0L6lengthS505 = _M0L4selfS504->$1;
  _M0L8_2afieldS3389 = _M0L4selfS504->$0;
  _M0L3bufS1986 = _M0L8_2afieldS3389;
  _M0L6_2aoldS3388 = (struct _M0TUsiE*)_M0L3bufS1986[_M0L6lengthS505];
  if (_M0L6_2aoldS3388) {
    moonbit_decref(_M0L6_2aoldS3388);
  }
  _M0L3bufS1986[_M0L6lengthS505] = _M0L5valueS506;
  _M0L6_2atmpS1987 = _M0L6lengthS505 + 1;
  _M0L4selfS504->$1 = _M0L6_2atmpS1987;
  moonbit_decref(_M0L4selfS504);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS507,
  void* _M0L5valueS509
) {
  int32_t _M0L3lenS1988;
  void** _M0L6_2atmpS1990;
  int32_t _M0L6_2atmpS3393;
  int32_t _M0L6_2atmpS1989;
  int32_t _M0L6lengthS508;
  void** _M0L8_2afieldS3392;
  void** _M0L3bufS1991;
  void* _M0L6_2aoldS3391;
  int32_t _M0L6_2atmpS1992;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1988 = _M0L4selfS507->$1;
  moonbit_incref(_M0L4selfS507);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1990
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS507);
  _M0L6_2atmpS3393 = Moonbit_array_length(_M0L6_2atmpS1990);
  moonbit_decref(_M0L6_2atmpS1990);
  _M0L6_2atmpS1989 = _M0L6_2atmpS3393;
  if (_M0L3lenS1988 == _M0L6_2atmpS1989) {
    moonbit_incref(_M0L4selfS507);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS507);
  }
  _M0L6lengthS508 = _M0L4selfS507->$1;
  _M0L8_2afieldS3392 = _M0L4selfS507->$0;
  _M0L3bufS1991 = _M0L8_2afieldS3392;
  _M0L6_2aoldS3391 = (void*)_M0L3bufS1991[_M0L6lengthS508];
  moonbit_decref(_M0L6_2aoldS3391);
  _M0L3bufS1991[_M0L6lengthS508] = _M0L5valueS509;
  _M0L6_2atmpS1992 = _M0L6lengthS508 + 1;
  _M0L4selfS507->$1 = _M0L6_2atmpS1992;
  moonbit_decref(_M0L4selfS507);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS493) {
  int32_t _M0L8old__capS492;
  int32_t _M0L8new__capS494;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS492 = _M0L4selfS493->$1;
  if (_M0L8old__capS492 == 0) {
    _M0L8new__capS494 = 8;
  } else {
    _M0L8new__capS494 = _M0L8old__capS492 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS493, _M0L8new__capS494);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS496
) {
  int32_t _M0L8old__capS495;
  int32_t _M0L8new__capS497;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS495 = _M0L4selfS496->$1;
  if (_M0L8old__capS495 == 0) {
    _M0L8new__capS497 = 8;
  } else {
    _M0L8new__capS497 = _M0L8old__capS495 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS496, _M0L8new__capS497);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS499
) {
  int32_t _M0L8old__capS498;
  int32_t _M0L8new__capS500;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS498 = _M0L4selfS499->$1;
  if (_M0L8old__capS498 == 0) {
    _M0L8new__capS500 = 8;
  } else {
    _M0L8new__capS500 = _M0L8old__capS498 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS499, _M0L8new__capS500);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS477,
  int32_t _M0L13new__capacityS475
) {
  moonbit_string_t* _M0L8new__bufS474;
  moonbit_string_t* _M0L8_2afieldS3395;
  moonbit_string_t* _M0L8old__bufS476;
  int32_t _M0L8old__capS478;
  int32_t _M0L9copy__lenS479;
  moonbit_string_t* _M0L6_2aoldS3394;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS474
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS475, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3395 = _M0L4selfS477->$0;
  _M0L8old__bufS476 = _M0L8_2afieldS3395;
  _M0L8old__capS478 = Moonbit_array_length(_M0L8old__bufS476);
  if (_M0L8old__capS478 < _M0L13new__capacityS475) {
    _M0L9copy__lenS479 = _M0L8old__capS478;
  } else {
    _M0L9copy__lenS479 = _M0L13new__capacityS475;
  }
  moonbit_incref(_M0L8old__bufS476);
  moonbit_incref(_M0L8new__bufS474);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS474, 0, _M0L8old__bufS476, 0, _M0L9copy__lenS479);
  _M0L6_2aoldS3394 = _M0L4selfS477->$0;
  moonbit_decref(_M0L6_2aoldS3394);
  _M0L4selfS477->$0 = _M0L8new__bufS474;
  moonbit_decref(_M0L4selfS477);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS483,
  int32_t _M0L13new__capacityS481
) {
  struct _M0TUsiE** _M0L8new__bufS480;
  struct _M0TUsiE** _M0L8_2afieldS3397;
  struct _M0TUsiE** _M0L8old__bufS482;
  int32_t _M0L8old__capS484;
  int32_t _M0L9copy__lenS485;
  struct _M0TUsiE** _M0L6_2aoldS3396;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS480
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS481, 0);
  _M0L8_2afieldS3397 = _M0L4selfS483->$0;
  _M0L8old__bufS482 = _M0L8_2afieldS3397;
  _M0L8old__capS484 = Moonbit_array_length(_M0L8old__bufS482);
  if (_M0L8old__capS484 < _M0L13new__capacityS481) {
    _M0L9copy__lenS485 = _M0L8old__capS484;
  } else {
    _M0L9copy__lenS485 = _M0L13new__capacityS481;
  }
  moonbit_incref(_M0L8old__bufS482);
  moonbit_incref(_M0L8new__bufS480);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS480, 0, _M0L8old__bufS482, 0, _M0L9copy__lenS485);
  _M0L6_2aoldS3396 = _M0L4selfS483->$0;
  moonbit_decref(_M0L6_2aoldS3396);
  _M0L4selfS483->$0 = _M0L8new__bufS480;
  moonbit_decref(_M0L4selfS483);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS489,
  int32_t _M0L13new__capacityS487
) {
  void** _M0L8new__bufS486;
  void** _M0L8_2afieldS3399;
  void** _M0L8old__bufS488;
  int32_t _M0L8old__capS490;
  int32_t _M0L9copy__lenS491;
  void** _M0L6_2aoldS3398;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS486
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS487, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3399 = _M0L4selfS489->$0;
  _M0L8old__bufS488 = _M0L8_2afieldS3399;
  _M0L8old__capS490 = Moonbit_array_length(_M0L8old__bufS488);
  if (_M0L8old__capS490 < _M0L13new__capacityS487) {
    _M0L9copy__lenS491 = _M0L8old__capS490;
  } else {
    _M0L9copy__lenS491 = _M0L13new__capacityS487;
  }
  moonbit_incref(_M0L8old__bufS488);
  moonbit_incref(_M0L8new__bufS486);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS486, 0, _M0L8old__bufS488, 0, _M0L9copy__lenS491);
  _M0L6_2aoldS3398 = _M0L4selfS489->$0;
  moonbit_decref(_M0L6_2aoldS3398);
  _M0L4selfS489->$0 = _M0L8new__bufS486;
  moonbit_decref(_M0L4selfS489);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS473
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS473 == 0) {
    moonbit_string_t* _M0L6_2atmpS1976 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3717 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3717)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3717->$0 = _M0L6_2atmpS1976;
    _block_3717->$1 = 0;
    return _block_3717;
  } else {
    moonbit_string_t* _M0L6_2atmpS1977 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS473, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3718 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3718)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3718->$0 = _M0L6_2atmpS1977;
    _block_3718->$1 = 0;
    return _block_3718;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS467,
  int32_t _M0L1nS466
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS466 <= 0) {
    moonbit_decref(_M0L4selfS467);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS466 == 1) {
    return _M0L4selfS467;
  } else {
    int32_t _M0L3lenS468 = Moonbit_array_length(_M0L4selfS467);
    int32_t _M0L6_2atmpS1975 = _M0L3lenS468 * _M0L1nS466;
    struct _M0TPB13StringBuilder* _M0L3bufS469;
    moonbit_string_t _M0L3strS470;
    int32_t _M0L2__S471;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS469 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1975);
    _M0L3strS470 = _M0L4selfS467;
    _M0L2__S471 = 0;
    while (1) {
      if (_M0L2__S471 < _M0L1nS466) {
        int32_t _M0L6_2atmpS1974;
        moonbit_incref(_M0L3strS470);
        moonbit_incref(_M0L3bufS469);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS469, _M0L3strS470);
        _M0L6_2atmpS1974 = _M0L2__S471 + 1;
        _M0L2__S471 = _M0L6_2atmpS1974;
        continue;
      } else {
        moonbit_decref(_M0L3strS470);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS469);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS464,
  struct _M0TPC16string10StringView _M0L3strS465
) {
  int32_t _M0L3lenS1962;
  int32_t _M0L6_2atmpS1964;
  int32_t _M0L6_2atmpS1963;
  int32_t _M0L6_2atmpS1961;
  moonbit_bytes_t _M0L8_2afieldS3400;
  moonbit_bytes_t _M0L4dataS1965;
  int32_t _M0L3lenS1966;
  moonbit_string_t _M0L6_2atmpS1967;
  int32_t _M0L6_2atmpS1968;
  int32_t _M0L6_2atmpS1969;
  int32_t _M0L3lenS1971;
  int32_t _M0L6_2atmpS1973;
  int32_t _M0L6_2atmpS1972;
  int32_t _M0L6_2atmpS1970;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1962 = _M0L4selfS464->$1;
  moonbit_incref(_M0L3strS465.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1964 = _M0MPC16string10StringView6length(_M0L3strS465);
  _M0L6_2atmpS1963 = _M0L6_2atmpS1964 * 2;
  _M0L6_2atmpS1961 = _M0L3lenS1962 + _M0L6_2atmpS1963;
  moonbit_incref(_M0L4selfS464);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS464, _M0L6_2atmpS1961);
  _M0L8_2afieldS3400 = _M0L4selfS464->$0;
  _M0L4dataS1965 = _M0L8_2afieldS3400;
  _M0L3lenS1966 = _M0L4selfS464->$1;
  moonbit_incref(_M0L4dataS1965);
  moonbit_incref(_M0L3strS465.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1967 = _M0MPC16string10StringView4data(_M0L3strS465);
  moonbit_incref(_M0L3strS465.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1968 = _M0MPC16string10StringView13start__offset(_M0L3strS465);
  moonbit_incref(_M0L3strS465.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1969 = _M0MPC16string10StringView6length(_M0L3strS465);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1965, _M0L3lenS1966, _M0L6_2atmpS1967, _M0L6_2atmpS1968, _M0L6_2atmpS1969);
  _M0L3lenS1971 = _M0L4selfS464->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1973 = _M0MPC16string10StringView6length(_M0L3strS465);
  _M0L6_2atmpS1972 = _M0L6_2atmpS1973 * 2;
  _M0L6_2atmpS1970 = _M0L3lenS1971 + _M0L6_2atmpS1972;
  _M0L4selfS464->$1 = _M0L6_2atmpS1970;
  moonbit_decref(_M0L4selfS464);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS456,
  int32_t _M0L3lenS459,
  int32_t _M0L13start__offsetS463,
  int64_t _M0L11end__offsetS454
) {
  int32_t _M0L11end__offsetS453;
  int32_t _M0L5indexS457;
  int32_t _M0L5countS458;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS454 == 4294967296ll) {
    _M0L11end__offsetS453 = Moonbit_array_length(_M0L4selfS456);
  } else {
    int64_t _M0L7_2aSomeS455 = _M0L11end__offsetS454;
    _M0L11end__offsetS453 = (int32_t)_M0L7_2aSomeS455;
  }
  _M0L5indexS457 = _M0L13start__offsetS463;
  _M0L5countS458 = 0;
  while (1) {
    int32_t _if__result_3721;
    if (_M0L5indexS457 < _M0L11end__offsetS453) {
      _if__result_3721 = _M0L5countS458 < _M0L3lenS459;
    } else {
      _if__result_3721 = 0;
    }
    if (_if__result_3721) {
      int32_t _M0L2c1S460 = _M0L4selfS456[_M0L5indexS457];
      int32_t _if__result_3722;
      int32_t _M0L6_2atmpS1959;
      int32_t _M0L6_2atmpS1960;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S460)) {
        int32_t _M0L6_2atmpS1955 = _M0L5indexS457 + 1;
        _if__result_3722 = _M0L6_2atmpS1955 < _M0L11end__offsetS453;
      } else {
        _if__result_3722 = 0;
      }
      if (_if__result_3722) {
        int32_t _M0L6_2atmpS1958 = _M0L5indexS457 + 1;
        int32_t _M0L2c2S461 = _M0L4selfS456[_M0L6_2atmpS1958];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S461)) {
          int32_t _M0L6_2atmpS1956 = _M0L5indexS457 + 2;
          int32_t _M0L6_2atmpS1957 = _M0L5countS458 + 1;
          _M0L5indexS457 = _M0L6_2atmpS1956;
          _M0L5countS458 = _M0L6_2atmpS1957;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_63.data, (moonbit_string_t)moonbit_string_literal_64.data);
        }
      }
      _M0L6_2atmpS1959 = _M0L5indexS457 + 1;
      _M0L6_2atmpS1960 = _M0L5countS458 + 1;
      _M0L5indexS457 = _M0L6_2atmpS1959;
      _M0L5countS458 = _M0L6_2atmpS1960;
      continue;
    } else {
      moonbit_decref(_M0L4selfS456);
      return _M0L5countS458 >= _M0L3lenS459;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS448
) {
  int32_t _M0L3endS1945;
  int32_t _M0L8_2afieldS3401;
  int32_t _M0L5startS1946;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1945 = _M0L4selfS448.$2;
  _M0L8_2afieldS3401 = _M0L4selfS448.$1;
  moonbit_decref(_M0L4selfS448.$0);
  _M0L5startS1946 = _M0L8_2afieldS3401;
  return _M0L3endS1945 - _M0L5startS1946;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS449
) {
  int32_t _M0L3endS1947;
  int32_t _M0L8_2afieldS3402;
  int32_t _M0L5startS1948;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1947 = _M0L4selfS449.$2;
  _M0L8_2afieldS3402 = _M0L4selfS449.$1;
  moonbit_decref(_M0L4selfS449.$0);
  _M0L5startS1948 = _M0L8_2afieldS3402;
  return _M0L3endS1947 - _M0L5startS1948;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS450
) {
  int32_t _M0L3endS1949;
  int32_t _M0L8_2afieldS3403;
  int32_t _M0L5startS1950;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1949 = _M0L4selfS450.$2;
  _M0L8_2afieldS3403 = _M0L4selfS450.$1;
  moonbit_decref(_M0L4selfS450.$0);
  _M0L5startS1950 = _M0L8_2afieldS3403;
  return _M0L3endS1949 - _M0L5startS1950;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS451
) {
  int32_t _M0L3endS1951;
  int32_t _M0L8_2afieldS3404;
  int32_t _M0L5startS1952;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1951 = _M0L4selfS451.$2;
  _M0L8_2afieldS3404 = _M0L4selfS451.$1;
  moonbit_decref(_M0L4selfS451.$0);
  _M0L5startS1952 = _M0L8_2afieldS3404;
  return _M0L3endS1951 - _M0L5startS1952;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE _M0L4selfS452
) {
  int32_t _M0L3endS1953;
  int32_t _M0L8_2afieldS3405;
  int32_t _M0L5startS1954;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1953 = _M0L4selfS452.$2;
  _M0L8_2afieldS3405 = _M0L4selfS452.$1;
  moonbit_decref(_M0L4selfS452.$0);
  _M0L5startS1954 = _M0L8_2afieldS3405;
  return _M0L3endS1953 - _M0L5startS1954;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS446,
  int64_t _M0L19start__offset_2eoptS444,
  int64_t _M0L11end__offsetS447
) {
  int32_t _M0L13start__offsetS443;
  if (_M0L19start__offset_2eoptS444 == 4294967296ll) {
    _M0L13start__offsetS443 = 0;
  } else {
    int64_t _M0L7_2aSomeS445 = _M0L19start__offset_2eoptS444;
    _M0L13start__offsetS443 = (int32_t)_M0L7_2aSomeS445;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS446, _M0L13start__offsetS443, _M0L11end__offsetS447);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS441,
  int32_t _M0L13start__offsetS442,
  int64_t _M0L11end__offsetS439
) {
  int32_t _M0L11end__offsetS438;
  int32_t _if__result_3723;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS439 == 4294967296ll) {
    _M0L11end__offsetS438 = Moonbit_array_length(_M0L4selfS441);
  } else {
    int64_t _M0L7_2aSomeS440 = _M0L11end__offsetS439;
    _M0L11end__offsetS438 = (int32_t)_M0L7_2aSomeS440;
  }
  if (_M0L13start__offsetS442 >= 0) {
    if (_M0L13start__offsetS442 <= _M0L11end__offsetS438) {
      int32_t _M0L6_2atmpS1944 = Moonbit_array_length(_M0L4selfS441);
      _if__result_3723 = _M0L11end__offsetS438 <= _M0L6_2atmpS1944;
    } else {
      _if__result_3723 = 0;
    }
  } else {
    _if__result_3723 = 0;
  }
  if (_if__result_3723) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS442,
                                                 _M0L11end__offsetS438,
                                                 _M0L4selfS441};
  } else {
    moonbit_decref(_M0L4selfS441);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_65.data, (moonbit_string_t)moonbit_string_literal_66.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS437
) {
  moonbit_string_t _M0L8_2afieldS3407;
  moonbit_string_t _M0L3strS1941;
  int32_t _M0L5startS1942;
  int32_t _M0L8_2afieldS3406;
  int32_t _M0L3endS1943;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3407 = _M0L4selfS437.$0;
  _M0L3strS1941 = _M0L8_2afieldS3407;
  _M0L5startS1942 = _M0L4selfS437.$1;
  _M0L8_2afieldS3406 = _M0L4selfS437.$2;
  _M0L3endS1943 = _M0L8_2afieldS3406;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1941, _M0L5startS1942, _M0L3endS1943);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS435,
  struct _M0TPB6Logger _M0L6loggerS436
) {
  moonbit_string_t _M0L8_2afieldS3409;
  moonbit_string_t _M0L3strS1938;
  int32_t _M0L5startS1939;
  int32_t _M0L8_2afieldS3408;
  int32_t _M0L3endS1940;
  moonbit_string_t _M0L6substrS434;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3409 = _M0L4selfS435.$0;
  _M0L3strS1938 = _M0L8_2afieldS3409;
  _M0L5startS1939 = _M0L4selfS435.$1;
  _M0L8_2afieldS3408 = _M0L4selfS435.$2;
  _M0L3endS1940 = _M0L8_2afieldS3408;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS434
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1938, _M0L5startS1939, _M0L3endS1940);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS434, _M0L6loggerS436);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS426,
  struct _M0TPB6Logger _M0L6loggerS424
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS425;
  int32_t _M0L3lenS427;
  int32_t _M0L1iS428;
  int32_t _M0L3segS429;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS424.$1) {
    moonbit_incref(_M0L6loggerS424.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS424.$0->$method_3(_M0L6loggerS424.$1, 34);
  moonbit_incref(_M0L4selfS426);
  if (_M0L6loggerS424.$1) {
    moonbit_incref(_M0L6loggerS424.$1);
  }
  _M0L6_2aenvS425
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS425)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS425->$0 = _M0L4selfS426;
  _M0L6_2aenvS425->$1_0 = _M0L6loggerS424.$0;
  _M0L6_2aenvS425->$1_1 = _M0L6loggerS424.$1;
  _M0L3lenS427 = Moonbit_array_length(_M0L4selfS426);
  _M0L1iS428 = 0;
  _M0L3segS429 = 0;
  _2afor_430:;
  while (1) {
    int32_t _M0L4codeS431;
    int32_t _M0L1cS433;
    int32_t _M0L6_2atmpS1922;
    int32_t _M0L6_2atmpS1923;
    int32_t _M0L6_2atmpS1924;
    int32_t _tmp_3727;
    int32_t _tmp_3728;
    if (_M0L1iS428 >= _M0L3lenS427) {
      moonbit_decref(_M0L4selfS426);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
      break;
    }
    _M0L4codeS431 = _M0L4selfS426[_M0L1iS428];
    switch (_M0L4codeS431) {
      case 34: {
        _M0L1cS433 = _M0L4codeS431;
        goto join_432;
        break;
      }
      
      case 92: {
        _M0L1cS433 = _M0L4codeS431;
        goto join_432;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1925;
        int32_t _M0L6_2atmpS1926;
        moonbit_incref(_M0L6_2aenvS425);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_50.data);
        _M0L6_2atmpS1925 = _M0L1iS428 + 1;
        _M0L6_2atmpS1926 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1925;
        _M0L3segS429 = _M0L6_2atmpS1926;
        goto _2afor_430;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1927;
        int32_t _M0L6_2atmpS1928;
        moonbit_incref(_M0L6_2aenvS425);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_51.data);
        _M0L6_2atmpS1927 = _M0L1iS428 + 1;
        _M0L6_2atmpS1928 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1927;
        _M0L3segS429 = _M0L6_2atmpS1928;
        goto _2afor_430;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1929;
        int32_t _M0L6_2atmpS1930;
        moonbit_incref(_M0L6_2aenvS425);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_52.data);
        _M0L6_2atmpS1929 = _M0L1iS428 + 1;
        _M0L6_2atmpS1930 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1929;
        _M0L3segS429 = _M0L6_2atmpS1930;
        goto _2afor_430;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1931;
        int32_t _M0L6_2atmpS1932;
        moonbit_incref(_M0L6_2aenvS425);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_53.data);
        _M0L6_2atmpS1931 = _M0L1iS428 + 1;
        _M0L6_2atmpS1932 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1931;
        _M0L3segS429 = _M0L6_2atmpS1932;
        goto _2afor_430;
        break;
      }
      default: {
        if (_M0L4codeS431 < 32) {
          int32_t _M0L6_2atmpS1934;
          moonbit_string_t _M0L6_2atmpS1933;
          int32_t _M0L6_2atmpS1935;
          int32_t _M0L6_2atmpS1936;
          moonbit_incref(_M0L6_2aenvS425);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
          if (_M0L6loggerS424.$1) {
            moonbit_incref(_M0L6loggerS424.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_67.data);
          _M0L6_2atmpS1934 = _M0L4codeS431 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1933 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1934);
          if (_M0L6loggerS424.$1) {
            moonbit_incref(_M0L6loggerS424.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, _M0L6_2atmpS1933);
          if (_M0L6loggerS424.$1) {
            moonbit_incref(_M0L6loggerS424.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS424.$0->$method_3(_M0L6loggerS424.$1, 125);
          _M0L6_2atmpS1935 = _M0L1iS428 + 1;
          _M0L6_2atmpS1936 = _M0L1iS428 + 1;
          _M0L1iS428 = _M0L6_2atmpS1935;
          _M0L3segS429 = _M0L6_2atmpS1936;
          goto _2afor_430;
        } else {
          int32_t _M0L6_2atmpS1937 = _M0L1iS428 + 1;
          int32_t _tmp_3726 = _M0L3segS429;
          _M0L1iS428 = _M0L6_2atmpS1937;
          _M0L3segS429 = _tmp_3726;
          goto _2afor_430;
        }
        break;
      }
    }
    goto joinlet_3725;
    join_432:;
    moonbit_incref(_M0L6_2aenvS425);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
    if (_M0L6loggerS424.$1) {
      moonbit_incref(_M0L6loggerS424.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS424.$0->$method_3(_M0L6loggerS424.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1922 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS433);
    if (_M0L6loggerS424.$1) {
      moonbit_incref(_M0L6loggerS424.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS424.$0->$method_3(_M0L6loggerS424.$1, _M0L6_2atmpS1922);
    _M0L6_2atmpS1923 = _M0L1iS428 + 1;
    _M0L6_2atmpS1924 = _M0L1iS428 + 1;
    _M0L1iS428 = _M0L6_2atmpS1923;
    _M0L3segS429 = _M0L6_2atmpS1924;
    continue;
    joinlet_3725:;
    _tmp_3727 = _M0L1iS428;
    _tmp_3728 = _M0L3segS429;
    _M0L1iS428 = _tmp_3727;
    _M0L3segS429 = _tmp_3728;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS424.$0->$method_3(_M0L6loggerS424.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS420,
  int32_t _M0L3segS423,
  int32_t _M0L1iS422
) {
  struct _M0TPB6Logger _M0L8_2afieldS3411;
  struct _M0TPB6Logger _M0L6loggerS419;
  moonbit_string_t _M0L8_2afieldS3410;
  int32_t _M0L6_2acntS3576;
  moonbit_string_t _M0L4selfS421;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3411
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS420->$1_0, _M0L6_2aenvS420->$1_1
  };
  _M0L6loggerS419 = _M0L8_2afieldS3411;
  _M0L8_2afieldS3410 = _M0L6_2aenvS420->$0;
  _M0L6_2acntS3576 = Moonbit_object_header(_M0L6_2aenvS420)->rc;
  if (_M0L6_2acntS3576 > 1) {
    int32_t _M0L11_2anew__cntS3577 = _M0L6_2acntS3576 - 1;
    Moonbit_object_header(_M0L6_2aenvS420)->rc = _M0L11_2anew__cntS3577;
    if (_M0L6loggerS419.$1) {
      moonbit_incref(_M0L6loggerS419.$1);
    }
    moonbit_incref(_M0L8_2afieldS3410);
  } else if (_M0L6_2acntS3576 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS420);
  }
  _M0L4selfS421 = _M0L8_2afieldS3410;
  if (_M0L1iS422 > _M0L3segS423) {
    int32_t _M0L6_2atmpS1921 = _M0L1iS422 - _M0L3segS423;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS419.$0->$method_1(_M0L6loggerS419.$1, _M0L4selfS421, _M0L3segS423, _M0L6_2atmpS1921);
  } else {
    moonbit_decref(_M0L4selfS421);
    if (_M0L6loggerS419.$1) {
      moonbit_decref(_M0L6loggerS419.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS418) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS417;
  int32_t _M0L6_2atmpS1918;
  int32_t _M0L6_2atmpS1917;
  int32_t _M0L6_2atmpS1920;
  int32_t _M0L6_2atmpS1919;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1916;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS417 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1918 = _M0IPC14byte4BytePB3Div3div(_M0L1bS418, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1917
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1918);
  moonbit_incref(_M0L7_2aselfS417);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS417, _M0L6_2atmpS1917);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1920 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS418, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1919
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1920);
  moonbit_incref(_M0L7_2aselfS417);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS417, _M0L6_2atmpS1919);
  _M0L6_2atmpS1916 = _M0L7_2aselfS417;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1916);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS416) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS416 < 10) {
    int32_t _M0L6_2atmpS1913;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1913 = _M0IPC14byte4BytePB3Add3add(_M0L1iS416, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1913);
  } else {
    int32_t _M0L6_2atmpS1915;
    int32_t _M0L6_2atmpS1914;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1915 = _M0IPC14byte4BytePB3Add3add(_M0L1iS416, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1914 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1915, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1914);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS414,
  int32_t _M0L4thatS415
) {
  int32_t _M0L6_2atmpS1911;
  int32_t _M0L6_2atmpS1912;
  int32_t _M0L6_2atmpS1910;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1911 = (int32_t)_M0L4selfS414;
  _M0L6_2atmpS1912 = (int32_t)_M0L4thatS415;
  _M0L6_2atmpS1910 = _M0L6_2atmpS1911 - _M0L6_2atmpS1912;
  return _M0L6_2atmpS1910 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS412,
  int32_t _M0L4thatS413
) {
  int32_t _M0L6_2atmpS1908;
  int32_t _M0L6_2atmpS1909;
  int32_t _M0L6_2atmpS1907;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1908 = (int32_t)_M0L4selfS412;
  _M0L6_2atmpS1909 = (int32_t)_M0L4thatS413;
  _M0L6_2atmpS1907 = _M0L6_2atmpS1908 % _M0L6_2atmpS1909;
  return _M0L6_2atmpS1907 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS410,
  int32_t _M0L4thatS411
) {
  int32_t _M0L6_2atmpS1905;
  int32_t _M0L6_2atmpS1906;
  int32_t _M0L6_2atmpS1904;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1905 = (int32_t)_M0L4selfS410;
  _M0L6_2atmpS1906 = (int32_t)_M0L4thatS411;
  _M0L6_2atmpS1904 = _M0L6_2atmpS1905 / _M0L6_2atmpS1906;
  return _M0L6_2atmpS1904 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS408,
  int32_t _M0L4thatS409
) {
  int32_t _M0L6_2atmpS1902;
  int32_t _M0L6_2atmpS1903;
  int32_t _M0L6_2atmpS1901;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1902 = (int32_t)_M0L4selfS408;
  _M0L6_2atmpS1903 = (int32_t)_M0L4thatS409;
  _M0L6_2atmpS1901 = _M0L6_2atmpS1902 + _M0L6_2atmpS1903;
  return _M0L6_2atmpS1901 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS405,
  int32_t _M0L5startS403,
  int32_t _M0L3endS404
) {
  int32_t _if__result_3729;
  int32_t _M0L3lenS406;
  int32_t _M0L6_2atmpS1899;
  int32_t _M0L6_2atmpS1900;
  moonbit_bytes_t _M0L5bytesS407;
  moonbit_bytes_t _M0L6_2atmpS1898;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS403 == 0) {
    int32_t _M0L6_2atmpS1897 = Moonbit_array_length(_M0L3strS405);
    _if__result_3729 = _M0L3endS404 == _M0L6_2atmpS1897;
  } else {
    _if__result_3729 = 0;
  }
  if (_if__result_3729) {
    return _M0L3strS405;
  }
  _M0L3lenS406 = _M0L3endS404 - _M0L5startS403;
  _M0L6_2atmpS1899 = _M0L3lenS406 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1900 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS407
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1899, _M0L6_2atmpS1900);
  moonbit_incref(_M0L5bytesS407);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS407, 0, _M0L3strS405, _M0L5startS403, _M0L3lenS406);
  _M0L6_2atmpS1898 = _M0L5bytesS407;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1898, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS399) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS399;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS400
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS400;
}

struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB4Iter3newGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L1fS401
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS401;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS402) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS402;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS391,
  int32_t _M0L5radixS390
) {
  int32_t _if__result_3730;
  uint16_t* _M0L6bufferS392;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS390 < 2) {
    _if__result_3730 = 1;
  } else {
    _if__result_3730 = _M0L5radixS390 > 36;
  }
  if (_if__result_3730) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_68.data, (moonbit_string_t)moonbit_string_literal_69.data);
  }
  if (_M0L4selfS391 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_56.data;
  }
  switch (_M0L5radixS390) {
    case 10: {
      int32_t _M0L3lenS393;
      uint16_t* _M0L6bufferS394;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS393 = _M0FPB12dec__count64(_M0L4selfS391);
      _M0L6bufferS394 = (uint16_t*)moonbit_make_string(_M0L3lenS393, 0);
      moonbit_incref(_M0L6bufferS394);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS394, _M0L4selfS391, 0, _M0L3lenS393);
      _M0L6bufferS392 = _M0L6bufferS394;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS395;
      uint16_t* _M0L6bufferS396;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS395 = _M0FPB12hex__count64(_M0L4selfS391);
      _M0L6bufferS396 = (uint16_t*)moonbit_make_string(_M0L3lenS395, 0);
      moonbit_incref(_M0L6bufferS396);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS396, _M0L4selfS391, 0, _M0L3lenS395);
      _M0L6bufferS392 = _M0L6bufferS396;
      break;
    }
    default: {
      int32_t _M0L3lenS397;
      uint16_t* _M0L6bufferS398;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS397 = _M0FPB14radix__count64(_M0L4selfS391, _M0L5radixS390);
      _M0L6bufferS398 = (uint16_t*)moonbit_make_string(_M0L3lenS397, 0);
      moonbit_incref(_M0L6bufferS398);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS398, _M0L4selfS391, 0, _M0L3lenS397, _M0L5radixS390);
      _M0L6bufferS392 = _M0L6bufferS398;
      break;
    }
  }
  return _M0L6bufferS392;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS380,
  uint64_t _M0L3numS368,
  int32_t _M0L12digit__startS371,
  int32_t _M0L10total__lenS370
) {
  uint64_t _M0Lm3numS367;
  int32_t _M0Lm6offsetS369;
  uint64_t _M0L6_2atmpS1896;
  int32_t _M0Lm9remainingS382;
  int32_t _M0L6_2atmpS1877;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS367 = _M0L3numS368;
  _M0Lm6offsetS369 = _M0L10total__lenS370 - _M0L12digit__startS371;
  while (1) {
    uint64_t _M0L6_2atmpS1840 = _M0Lm3numS367;
    if (_M0L6_2atmpS1840 >= 10000ull) {
      uint64_t _M0L6_2atmpS1863 = _M0Lm3numS367;
      uint64_t _M0L1tS372 = _M0L6_2atmpS1863 / 10000ull;
      uint64_t _M0L6_2atmpS1862 = _M0Lm3numS367;
      uint64_t _M0L6_2atmpS1861 = _M0L6_2atmpS1862 % 10000ull;
      int32_t _M0L1rS373 = (int32_t)_M0L6_2atmpS1861;
      int32_t _M0L2d1S374;
      int32_t _M0L2d2S375;
      int32_t _M0L6_2atmpS1841;
      int32_t _M0L6_2atmpS1860;
      int32_t _M0L6_2atmpS1859;
      int32_t _M0L6d1__hiS376;
      int32_t _M0L6_2atmpS1858;
      int32_t _M0L6_2atmpS1857;
      int32_t _M0L6d1__loS377;
      int32_t _M0L6_2atmpS1856;
      int32_t _M0L6_2atmpS1855;
      int32_t _M0L6d2__hiS378;
      int32_t _M0L6_2atmpS1854;
      int32_t _M0L6_2atmpS1853;
      int32_t _M0L6d2__loS379;
      int32_t _M0L6_2atmpS1843;
      int32_t _M0L6_2atmpS1842;
      int32_t _M0L6_2atmpS1846;
      int32_t _M0L6_2atmpS1845;
      int32_t _M0L6_2atmpS1844;
      int32_t _M0L6_2atmpS1849;
      int32_t _M0L6_2atmpS1848;
      int32_t _M0L6_2atmpS1847;
      int32_t _M0L6_2atmpS1852;
      int32_t _M0L6_2atmpS1851;
      int32_t _M0L6_2atmpS1850;
      _M0Lm3numS367 = _M0L1tS372;
      _M0L2d1S374 = _M0L1rS373 / 100;
      _M0L2d2S375 = _M0L1rS373 % 100;
      _M0L6_2atmpS1841 = _M0Lm6offsetS369;
      _M0Lm6offsetS369 = _M0L6_2atmpS1841 - 4;
      _M0L6_2atmpS1860 = _M0L2d1S374 / 10;
      _M0L6_2atmpS1859 = 48 + _M0L6_2atmpS1860;
      _M0L6d1__hiS376 = (uint16_t)_M0L6_2atmpS1859;
      _M0L6_2atmpS1858 = _M0L2d1S374 % 10;
      _M0L6_2atmpS1857 = 48 + _M0L6_2atmpS1858;
      _M0L6d1__loS377 = (uint16_t)_M0L6_2atmpS1857;
      _M0L6_2atmpS1856 = _M0L2d2S375 / 10;
      _M0L6_2atmpS1855 = 48 + _M0L6_2atmpS1856;
      _M0L6d2__hiS378 = (uint16_t)_M0L6_2atmpS1855;
      _M0L6_2atmpS1854 = _M0L2d2S375 % 10;
      _M0L6_2atmpS1853 = 48 + _M0L6_2atmpS1854;
      _M0L6d2__loS379 = (uint16_t)_M0L6_2atmpS1853;
      _M0L6_2atmpS1843 = _M0Lm6offsetS369;
      _M0L6_2atmpS1842 = _M0L12digit__startS371 + _M0L6_2atmpS1843;
      _M0L6bufferS380[_M0L6_2atmpS1842] = _M0L6d1__hiS376;
      _M0L6_2atmpS1846 = _M0Lm6offsetS369;
      _M0L6_2atmpS1845 = _M0L12digit__startS371 + _M0L6_2atmpS1846;
      _M0L6_2atmpS1844 = _M0L6_2atmpS1845 + 1;
      _M0L6bufferS380[_M0L6_2atmpS1844] = _M0L6d1__loS377;
      _M0L6_2atmpS1849 = _M0Lm6offsetS369;
      _M0L6_2atmpS1848 = _M0L12digit__startS371 + _M0L6_2atmpS1849;
      _M0L6_2atmpS1847 = _M0L6_2atmpS1848 + 2;
      _M0L6bufferS380[_M0L6_2atmpS1847] = _M0L6d2__hiS378;
      _M0L6_2atmpS1852 = _M0Lm6offsetS369;
      _M0L6_2atmpS1851 = _M0L12digit__startS371 + _M0L6_2atmpS1852;
      _M0L6_2atmpS1850 = _M0L6_2atmpS1851 + 3;
      _M0L6bufferS380[_M0L6_2atmpS1850] = _M0L6d2__loS379;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1896 = _M0Lm3numS367;
  _M0Lm9remainingS382 = (int32_t)_M0L6_2atmpS1896;
  while (1) {
    int32_t _M0L6_2atmpS1864 = _M0Lm9remainingS382;
    if (_M0L6_2atmpS1864 >= 100) {
      int32_t _M0L6_2atmpS1876 = _M0Lm9remainingS382;
      int32_t _M0L1tS383 = _M0L6_2atmpS1876 / 100;
      int32_t _M0L6_2atmpS1875 = _M0Lm9remainingS382;
      int32_t _M0L1dS384 = _M0L6_2atmpS1875 % 100;
      int32_t _M0L6_2atmpS1865;
      int32_t _M0L6_2atmpS1874;
      int32_t _M0L6_2atmpS1873;
      int32_t _M0L5d__hiS385;
      int32_t _M0L6_2atmpS1872;
      int32_t _M0L6_2atmpS1871;
      int32_t _M0L5d__loS386;
      int32_t _M0L6_2atmpS1867;
      int32_t _M0L6_2atmpS1866;
      int32_t _M0L6_2atmpS1870;
      int32_t _M0L6_2atmpS1869;
      int32_t _M0L6_2atmpS1868;
      _M0Lm9remainingS382 = _M0L1tS383;
      _M0L6_2atmpS1865 = _M0Lm6offsetS369;
      _M0Lm6offsetS369 = _M0L6_2atmpS1865 - 2;
      _M0L6_2atmpS1874 = _M0L1dS384 / 10;
      _M0L6_2atmpS1873 = 48 + _M0L6_2atmpS1874;
      _M0L5d__hiS385 = (uint16_t)_M0L6_2atmpS1873;
      _M0L6_2atmpS1872 = _M0L1dS384 % 10;
      _M0L6_2atmpS1871 = 48 + _M0L6_2atmpS1872;
      _M0L5d__loS386 = (uint16_t)_M0L6_2atmpS1871;
      _M0L6_2atmpS1867 = _M0Lm6offsetS369;
      _M0L6_2atmpS1866 = _M0L12digit__startS371 + _M0L6_2atmpS1867;
      _M0L6bufferS380[_M0L6_2atmpS1866] = _M0L5d__hiS385;
      _M0L6_2atmpS1870 = _M0Lm6offsetS369;
      _M0L6_2atmpS1869 = _M0L12digit__startS371 + _M0L6_2atmpS1870;
      _M0L6_2atmpS1868 = _M0L6_2atmpS1869 + 1;
      _M0L6bufferS380[_M0L6_2atmpS1868] = _M0L5d__loS386;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1877 = _M0Lm9remainingS382;
  if (_M0L6_2atmpS1877 >= 10) {
    int32_t _M0L6_2atmpS1878 = _M0Lm6offsetS369;
    int32_t _M0L6_2atmpS1889;
    int32_t _M0L6_2atmpS1888;
    int32_t _M0L6_2atmpS1887;
    int32_t _M0L5d__hiS388;
    int32_t _M0L6_2atmpS1886;
    int32_t _M0L6_2atmpS1885;
    int32_t _M0L6_2atmpS1884;
    int32_t _M0L5d__loS389;
    int32_t _M0L6_2atmpS1880;
    int32_t _M0L6_2atmpS1879;
    int32_t _M0L6_2atmpS1883;
    int32_t _M0L6_2atmpS1882;
    int32_t _M0L6_2atmpS1881;
    _M0Lm6offsetS369 = _M0L6_2atmpS1878 - 2;
    _M0L6_2atmpS1889 = _M0Lm9remainingS382;
    _M0L6_2atmpS1888 = _M0L6_2atmpS1889 / 10;
    _M0L6_2atmpS1887 = 48 + _M0L6_2atmpS1888;
    _M0L5d__hiS388 = (uint16_t)_M0L6_2atmpS1887;
    _M0L6_2atmpS1886 = _M0Lm9remainingS382;
    _M0L6_2atmpS1885 = _M0L6_2atmpS1886 % 10;
    _M0L6_2atmpS1884 = 48 + _M0L6_2atmpS1885;
    _M0L5d__loS389 = (uint16_t)_M0L6_2atmpS1884;
    _M0L6_2atmpS1880 = _M0Lm6offsetS369;
    _M0L6_2atmpS1879 = _M0L12digit__startS371 + _M0L6_2atmpS1880;
    _M0L6bufferS380[_M0L6_2atmpS1879] = _M0L5d__hiS388;
    _M0L6_2atmpS1883 = _M0Lm6offsetS369;
    _M0L6_2atmpS1882 = _M0L12digit__startS371 + _M0L6_2atmpS1883;
    _M0L6_2atmpS1881 = _M0L6_2atmpS1882 + 1;
    _M0L6bufferS380[_M0L6_2atmpS1881] = _M0L5d__loS389;
    moonbit_decref(_M0L6bufferS380);
  } else {
    int32_t _M0L6_2atmpS1890 = _M0Lm6offsetS369;
    int32_t _M0L6_2atmpS1895;
    int32_t _M0L6_2atmpS1891;
    int32_t _M0L6_2atmpS1894;
    int32_t _M0L6_2atmpS1893;
    int32_t _M0L6_2atmpS1892;
    _M0Lm6offsetS369 = _M0L6_2atmpS1890 - 1;
    _M0L6_2atmpS1895 = _M0Lm6offsetS369;
    _M0L6_2atmpS1891 = _M0L12digit__startS371 + _M0L6_2atmpS1895;
    _M0L6_2atmpS1894 = _M0Lm9remainingS382;
    _M0L6_2atmpS1893 = 48 + _M0L6_2atmpS1894;
    _M0L6_2atmpS1892 = (uint16_t)_M0L6_2atmpS1893;
    _M0L6bufferS380[_M0L6_2atmpS1891] = _M0L6_2atmpS1892;
    moonbit_decref(_M0L6bufferS380);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS362,
  uint64_t _M0L3numS356,
  int32_t _M0L12digit__startS354,
  int32_t _M0L10total__lenS353,
  int32_t _M0L5radixS358
) {
  int32_t _M0Lm6offsetS352;
  uint64_t _M0Lm1nS355;
  uint64_t _M0L4baseS357;
  int32_t _M0L6_2atmpS1822;
  int32_t _M0L6_2atmpS1821;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS352 = _M0L10total__lenS353 - _M0L12digit__startS354;
  _M0Lm1nS355 = _M0L3numS356;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS357 = _M0MPC13int3Int10to__uint64(_M0L5radixS358);
  _M0L6_2atmpS1822 = _M0L5radixS358 - 1;
  _M0L6_2atmpS1821 = _M0L5radixS358 & _M0L6_2atmpS1822;
  if (_M0L6_2atmpS1821 == 0) {
    int32_t _M0L5shiftS359;
    uint64_t _M0L4maskS360;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS359 = moonbit_ctz32(_M0L5radixS358);
    _M0L4maskS360 = _M0L4baseS357 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1823 = _M0Lm1nS355;
      if (_M0L6_2atmpS1823 > 0ull) {
        int32_t _M0L6_2atmpS1824 = _M0Lm6offsetS352;
        uint64_t _M0L6_2atmpS1830;
        uint64_t _M0L6_2atmpS1829;
        int32_t _M0L5digitS361;
        int32_t _M0L6_2atmpS1827;
        int32_t _M0L6_2atmpS1825;
        int32_t _M0L6_2atmpS1826;
        uint64_t _M0L6_2atmpS1828;
        _M0Lm6offsetS352 = _M0L6_2atmpS1824 - 1;
        _M0L6_2atmpS1830 = _M0Lm1nS355;
        _M0L6_2atmpS1829 = _M0L6_2atmpS1830 & _M0L4maskS360;
        _M0L5digitS361 = (int32_t)_M0L6_2atmpS1829;
        _M0L6_2atmpS1827 = _M0Lm6offsetS352;
        _M0L6_2atmpS1825 = _M0L12digit__startS354 + _M0L6_2atmpS1827;
        _M0L6_2atmpS1826
        = ((moonbit_string_t)moonbit_string_literal_70.data)[
          _M0L5digitS361
        ];
        _M0L6bufferS362[_M0L6_2atmpS1825] = _M0L6_2atmpS1826;
        _M0L6_2atmpS1828 = _M0Lm1nS355;
        _M0Lm1nS355 = _M0L6_2atmpS1828 >> (_M0L5shiftS359 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS362);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1831 = _M0Lm1nS355;
      if (_M0L6_2atmpS1831 > 0ull) {
        int32_t _M0L6_2atmpS1832 = _M0Lm6offsetS352;
        uint64_t _M0L6_2atmpS1839;
        uint64_t _M0L1qS364;
        uint64_t _M0L6_2atmpS1837;
        uint64_t _M0L6_2atmpS1838;
        uint64_t _M0L6_2atmpS1836;
        int32_t _M0L5digitS365;
        int32_t _M0L6_2atmpS1835;
        int32_t _M0L6_2atmpS1833;
        int32_t _M0L6_2atmpS1834;
        _M0Lm6offsetS352 = _M0L6_2atmpS1832 - 1;
        _M0L6_2atmpS1839 = _M0Lm1nS355;
        _M0L1qS364 = _M0L6_2atmpS1839 / _M0L4baseS357;
        _M0L6_2atmpS1837 = _M0Lm1nS355;
        _M0L6_2atmpS1838 = _M0L1qS364 * _M0L4baseS357;
        _M0L6_2atmpS1836 = _M0L6_2atmpS1837 - _M0L6_2atmpS1838;
        _M0L5digitS365 = (int32_t)_M0L6_2atmpS1836;
        _M0L6_2atmpS1835 = _M0Lm6offsetS352;
        _M0L6_2atmpS1833 = _M0L12digit__startS354 + _M0L6_2atmpS1835;
        _M0L6_2atmpS1834
        = ((moonbit_string_t)moonbit_string_literal_70.data)[
          _M0L5digitS365
        ];
        _M0L6bufferS362[_M0L6_2atmpS1833] = _M0L6_2atmpS1834;
        _M0Lm1nS355 = _M0L1qS364;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS362);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS349,
  uint64_t _M0L3numS345,
  int32_t _M0L12digit__startS343,
  int32_t _M0L10total__lenS342
) {
  int32_t _M0Lm6offsetS341;
  uint64_t _M0Lm1nS344;
  int32_t _M0L6_2atmpS1817;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS341 = _M0L10total__lenS342 - _M0L12digit__startS343;
  _M0Lm1nS344 = _M0L3numS345;
  while (1) {
    int32_t _M0L6_2atmpS1805 = _M0Lm6offsetS341;
    if (_M0L6_2atmpS1805 >= 2) {
      int32_t _M0L6_2atmpS1806 = _M0Lm6offsetS341;
      uint64_t _M0L6_2atmpS1816;
      uint64_t _M0L6_2atmpS1815;
      int32_t _M0L9byte__valS346;
      int32_t _M0L2hiS347;
      int32_t _M0L2loS348;
      int32_t _M0L6_2atmpS1809;
      int32_t _M0L6_2atmpS1807;
      int32_t _M0L6_2atmpS1808;
      int32_t _M0L6_2atmpS1813;
      int32_t _M0L6_2atmpS1812;
      int32_t _M0L6_2atmpS1810;
      int32_t _M0L6_2atmpS1811;
      uint64_t _M0L6_2atmpS1814;
      _M0Lm6offsetS341 = _M0L6_2atmpS1806 - 2;
      _M0L6_2atmpS1816 = _M0Lm1nS344;
      _M0L6_2atmpS1815 = _M0L6_2atmpS1816 & 255ull;
      _M0L9byte__valS346 = (int32_t)_M0L6_2atmpS1815;
      _M0L2hiS347 = _M0L9byte__valS346 / 16;
      _M0L2loS348 = _M0L9byte__valS346 % 16;
      _M0L6_2atmpS1809 = _M0Lm6offsetS341;
      _M0L6_2atmpS1807 = _M0L12digit__startS343 + _M0L6_2atmpS1809;
      _M0L6_2atmpS1808
      = ((moonbit_string_t)moonbit_string_literal_70.data)[
        _M0L2hiS347
      ];
      _M0L6bufferS349[_M0L6_2atmpS1807] = _M0L6_2atmpS1808;
      _M0L6_2atmpS1813 = _M0Lm6offsetS341;
      _M0L6_2atmpS1812 = _M0L12digit__startS343 + _M0L6_2atmpS1813;
      _M0L6_2atmpS1810 = _M0L6_2atmpS1812 + 1;
      _M0L6_2atmpS1811
      = ((moonbit_string_t)moonbit_string_literal_70.data)[
        _M0L2loS348
      ];
      _M0L6bufferS349[_M0L6_2atmpS1810] = _M0L6_2atmpS1811;
      _M0L6_2atmpS1814 = _M0Lm1nS344;
      _M0Lm1nS344 = _M0L6_2atmpS1814 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1817 = _M0Lm6offsetS341;
  if (_M0L6_2atmpS1817 == 1) {
    uint64_t _M0L6_2atmpS1820 = _M0Lm1nS344;
    uint64_t _M0L6_2atmpS1819 = _M0L6_2atmpS1820 & 15ull;
    int32_t _M0L6nibbleS351 = (int32_t)_M0L6_2atmpS1819;
    int32_t _M0L6_2atmpS1818 =
      ((moonbit_string_t)moonbit_string_literal_70.data)[_M0L6nibbleS351];
    _M0L6bufferS349[_M0L12digit__startS343] = _M0L6_2atmpS1818;
    moonbit_decref(_M0L6bufferS349);
  } else {
    moonbit_decref(_M0L6bufferS349);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS335,
  int32_t _M0L5radixS338
) {
  uint64_t _M0Lm3numS336;
  uint64_t _M0L4baseS337;
  int32_t _M0Lm5countS339;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS335 == 0ull) {
    return 1;
  }
  _M0Lm3numS336 = _M0L5valueS335;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS337 = _M0MPC13int3Int10to__uint64(_M0L5radixS338);
  _M0Lm5countS339 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1802 = _M0Lm3numS336;
    if (_M0L6_2atmpS1802 > 0ull) {
      int32_t _M0L6_2atmpS1803 = _M0Lm5countS339;
      uint64_t _M0L6_2atmpS1804;
      _M0Lm5countS339 = _M0L6_2atmpS1803 + 1;
      _M0L6_2atmpS1804 = _M0Lm3numS336;
      _M0Lm3numS336 = _M0L6_2atmpS1804 / _M0L4baseS337;
      continue;
    }
    break;
  }
  return _M0Lm5countS339;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS333) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS333 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS334;
    int32_t _M0L6_2atmpS1801;
    int32_t _M0L6_2atmpS1800;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS334 = moonbit_clz64(_M0L5valueS333);
    _M0L6_2atmpS1801 = 63 - _M0L14leading__zerosS334;
    _M0L6_2atmpS1800 = _M0L6_2atmpS1801 / 4;
    return _M0L6_2atmpS1800 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS332) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS332 >= 10000000000ull) {
    if (_M0L5valueS332 >= 100000000000000ull) {
      if (_M0L5valueS332 >= 10000000000000000ull) {
        if (_M0L5valueS332 >= 1000000000000000000ull) {
          if (_M0L5valueS332 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS332 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS332 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS332 >= 1000000000000ull) {
      if (_M0L5valueS332 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS332 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS332 >= 100000ull) {
    if (_M0L5valueS332 >= 10000000ull) {
      if (_M0L5valueS332 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS332 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS332 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS332 >= 1000ull) {
    if (_M0L5valueS332 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS332 >= 100ull) {
    return 3;
  } else if (_M0L5valueS332 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS316,
  int32_t _M0L5radixS315
) {
  int32_t _if__result_3737;
  int32_t _M0L12is__negativeS317;
  uint32_t _M0L3numS318;
  uint16_t* _M0L6bufferS319;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS315 < 2) {
    _if__result_3737 = 1;
  } else {
    _if__result_3737 = _M0L5radixS315 > 36;
  }
  if (_if__result_3737) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_68.data, (moonbit_string_t)moonbit_string_literal_71.data);
  }
  if (_M0L4selfS316 == 0) {
    return (moonbit_string_t)moonbit_string_literal_56.data;
  }
  _M0L12is__negativeS317 = _M0L4selfS316 < 0;
  if (_M0L12is__negativeS317) {
    int32_t _M0L6_2atmpS1799 = -_M0L4selfS316;
    _M0L3numS318 = *(uint32_t*)&_M0L6_2atmpS1799;
  } else {
    _M0L3numS318 = *(uint32_t*)&_M0L4selfS316;
  }
  switch (_M0L5radixS315) {
    case 10: {
      int32_t _M0L10digit__lenS320;
      int32_t _M0L6_2atmpS1796;
      int32_t _M0L10total__lenS321;
      uint16_t* _M0L6bufferS322;
      int32_t _M0L12digit__startS323;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS320 = _M0FPB12dec__count32(_M0L3numS318);
      if (_M0L12is__negativeS317) {
        _M0L6_2atmpS1796 = 1;
      } else {
        _M0L6_2atmpS1796 = 0;
      }
      _M0L10total__lenS321 = _M0L10digit__lenS320 + _M0L6_2atmpS1796;
      _M0L6bufferS322
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS321, 0);
      if (_M0L12is__negativeS317) {
        _M0L12digit__startS323 = 1;
      } else {
        _M0L12digit__startS323 = 0;
      }
      moonbit_incref(_M0L6bufferS322);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS322, _M0L3numS318, _M0L12digit__startS323, _M0L10total__lenS321);
      _M0L6bufferS319 = _M0L6bufferS322;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS324;
      int32_t _M0L6_2atmpS1797;
      int32_t _M0L10total__lenS325;
      uint16_t* _M0L6bufferS326;
      int32_t _M0L12digit__startS327;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS324 = _M0FPB12hex__count32(_M0L3numS318);
      if (_M0L12is__negativeS317) {
        _M0L6_2atmpS1797 = 1;
      } else {
        _M0L6_2atmpS1797 = 0;
      }
      _M0L10total__lenS325 = _M0L10digit__lenS324 + _M0L6_2atmpS1797;
      _M0L6bufferS326
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS325, 0);
      if (_M0L12is__negativeS317) {
        _M0L12digit__startS327 = 1;
      } else {
        _M0L12digit__startS327 = 0;
      }
      moonbit_incref(_M0L6bufferS326);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS326, _M0L3numS318, _M0L12digit__startS327, _M0L10total__lenS325);
      _M0L6bufferS319 = _M0L6bufferS326;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS328;
      int32_t _M0L6_2atmpS1798;
      int32_t _M0L10total__lenS329;
      uint16_t* _M0L6bufferS330;
      int32_t _M0L12digit__startS331;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS328
      = _M0FPB14radix__count32(_M0L3numS318, _M0L5radixS315);
      if (_M0L12is__negativeS317) {
        _M0L6_2atmpS1798 = 1;
      } else {
        _M0L6_2atmpS1798 = 0;
      }
      _M0L10total__lenS329 = _M0L10digit__lenS328 + _M0L6_2atmpS1798;
      _M0L6bufferS330
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS329, 0);
      if (_M0L12is__negativeS317) {
        _M0L12digit__startS331 = 1;
      } else {
        _M0L12digit__startS331 = 0;
      }
      moonbit_incref(_M0L6bufferS330);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS330, _M0L3numS318, _M0L12digit__startS331, _M0L10total__lenS329, _M0L5radixS315);
      _M0L6bufferS319 = _M0L6bufferS330;
      break;
    }
  }
  if (_M0L12is__negativeS317) {
    _M0L6bufferS319[0] = 45;
  }
  return _M0L6bufferS319;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS309,
  int32_t _M0L5radixS312
) {
  uint32_t _M0Lm3numS310;
  uint32_t _M0L4baseS311;
  int32_t _M0Lm5countS313;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS309 == 0u) {
    return 1;
  }
  _M0Lm3numS310 = _M0L5valueS309;
  _M0L4baseS311 = *(uint32_t*)&_M0L5radixS312;
  _M0Lm5countS313 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1793 = _M0Lm3numS310;
    if (_M0L6_2atmpS1793 > 0u) {
      int32_t _M0L6_2atmpS1794 = _M0Lm5countS313;
      uint32_t _M0L6_2atmpS1795;
      _M0Lm5countS313 = _M0L6_2atmpS1794 + 1;
      _M0L6_2atmpS1795 = _M0Lm3numS310;
      _M0Lm3numS310 = _M0L6_2atmpS1795 / _M0L4baseS311;
      continue;
    }
    break;
  }
  return _M0Lm5countS313;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS307) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS307 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS308;
    int32_t _M0L6_2atmpS1792;
    int32_t _M0L6_2atmpS1791;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS308 = moonbit_clz32(_M0L5valueS307);
    _M0L6_2atmpS1792 = 31 - _M0L14leading__zerosS308;
    _M0L6_2atmpS1791 = _M0L6_2atmpS1792 / 4;
    return _M0L6_2atmpS1791 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS306) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS306 >= 100000u) {
    if (_M0L5valueS306 >= 10000000u) {
      if (_M0L5valueS306 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS306 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS306 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS306 >= 1000u) {
    if (_M0L5valueS306 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS306 >= 100u) {
    return 3;
  } else if (_M0L5valueS306 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS296,
  uint32_t _M0L3numS284,
  int32_t _M0L12digit__startS287,
  int32_t _M0L10total__lenS286
) {
  uint32_t _M0Lm3numS283;
  int32_t _M0Lm6offsetS285;
  uint32_t _M0L6_2atmpS1790;
  int32_t _M0Lm9remainingS298;
  int32_t _M0L6_2atmpS1771;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS283 = _M0L3numS284;
  _M0Lm6offsetS285 = _M0L10total__lenS286 - _M0L12digit__startS287;
  while (1) {
    uint32_t _M0L6_2atmpS1734 = _M0Lm3numS283;
    if (_M0L6_2atmpS1734 >= 10000u) {
      uint32_t _M0L6_2atmpS1757 = _M0Lm3numS283;
      uint32_t _M0L1tS288 = _M0L6_2atmpS1757 / 10000u;
      uint32_t _M0L6_2atmpS1756 = _M0Lm3numS283;
      uint32_t _M0L6_2atmpS1755 = _M0L6_2atmpS1756 % 10000u;
      int32_t _M0L1rS289 = *(int32_t*)&_M0L6_2atmpS1755;
      int32_t _M0L2d1S290;
      int32_t _M0L2d2S291;
      int32_t _M0L6_2atmpS1735;
      int32_t _M0L6_2atmpS1754;
      int32_t _M0L6_2atmpS1753;
      int32_t _M0L6d1__hiS292;
      int32_t _M0L6_2atmpS1752;
      int32_t _M0L6_2atmpS1751;
      int32_t _M0L6d1__loS293;
      int32_t _M0L6_2atmpS1750;
      int32_t _M0L6_2atmpS1749;
      int32_t _M0L6d2__hiS294;
      int32_t _M0L6_2atmpS1748;
      int32_t _M0L6_2atmpS1747;
      int32_t _M0L6d2__loS295;
      int32_t _M0L6_2atmpS1737;
      int32_t _M0L6_2atmpS1736;
      int32_t _M0L6_2atmpS1740;
      int32_t _M0L6_2atmpS1739;
      int32_t _M0L6_2atmpS1738;
      int32_t _M0L6_2atmpS1743;
      int32_t _M0L6_2atmpS1742;
      int32_t _M0L6_2atmpS1741;
      int32_t _M0L6_2atmpS1746;
      int32_t _M0L6_2atmpS1745;
      int32_t _M0L6_2atmpS1744;
      _M0Lm3numS283 = _M0L1tS288;
      _M0L2d1S290 = _M0L1rS289 / 100;
      _M0L2d2S291 = _M0L1rS289 % 100;
      _M0L6_2atmpS1735 = _M0Lm6offsetS285;
      _M0Lm6offsetS285 = _M0L6_2atmpS1735 - 4;
      _M0L6_2atmpS1754 = _M0L2d1S290 / 10;
      _M0L6_2atmpS1753 = 48 + _M0L6_2atmpS1754;
      _M0L6d1__hiS292 = (uint16_t)_M0L6_2atmpS1753;
      _M0L6_2atmpS1752 = _M0L2d1S290 % 10;
      _M0L6_2atmpS1751 = 48 + _M0L6_2atmpS1752;
      _M0L6d1__loS293 = (uint16_t)_M0L6_2atmpS1751;
      _M0L6_2atmpS1750 = _M0L2d2S291 / 10;
      _M0L6_2atmpS1749 = 48 + _M0L6_2atmpS1750;
      _M0L6d2__hiS294 = (uint16_t)_M0L6_2atmpS1749;
      _M0L6_2atmpS1748 = _M0L2d2S291 % 10;
      _M0L6_2atmpS1747 = 48 + _M0L6_2atmpS1748;
      _M0L6d2__loS295 = (uint16_t)_M0L6_2atmpS1747;
      _M0L6_2atmpS1737 = _M0Lm6offsetS285;
      _M0L6_2atmpS1736 = _M0L12digit__startS287 + _M0L6_2atmpS1737;
      _M0L6bufferS296[_M0L6_2atmpS1736] = _M0L6d1__hiS292;
      _M0L6_2atmpS1740 = _M0Lm6offsetS285;
      _M0L6_2atmpS1739 = _M0L12digit__startS287 + _M0L6_2atmpS1740;
      _M0L6_2atmpS1738 = _M0L6_2atmpS1739 + 1;
      _M0L6bufferS296[_M0L6_2atmpS1738] = _M0L6d1__loS293;
      _M0L6_2atmpS1743 = _M0Lm6offsetS285;
      _M0L6_2atmpS1742 = _M0L12digit__startS287 + _M0L6_2atmpS1743;
      _M0L6_2atmpS1741 = _M0L6_2atmpS1742 + 2;
      _M0L6bufferS296[_M0L6_2atmpS1741] = _M0L6d2__hiS294;
      _M0L6_2atmpS1746 = _M0Lm6offsetS285;
      _M0L6_2atmpS1745 = _M0L12digit__startS287 + _M0L6_2atmpS1746;
      _M0L6_2atmpS1744 = _M0L6_2atmpS1745 + 3;
      _M0L6bufferS296[_M0L6_2atmpS1744] = _M0L6d2__loS295;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1790 = _M0Lm3numS283;
  _M0Lm9remainingS298 = *(int32_t*)&_M0L6_2atmpS1790;
  while (1) {
    int32_t _M0L6_2atmpS1758 = _M0Lm9remainingS298;
    if (_M0L6_2atmpS1758 >= 100) {
      int32_t _M0L6_2atmpS1770 = _M0Lm9remainingS298;
      int32_t _M0L1tS299 = _M0L6_2atmpS1770 / 100;
      int32_t _M0L6_2atmpS1769 = _M0Lm9remainingS298;
      int32_t _M0L1dS300 = _M0L6_2atmpS1769 % 100;
      int32_t _M0L6_2atmpS1759;
      int32_t _M0L6_2atmpS1768;
      int32_t _M0L6_2atmpS1767;
      int32_t _M0L5d__hiS301;
      int32_t _M0L6_2atmpS1766;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L5d__loS302;
      int32_t _M0L6_2atmpS1761;
      int32_t _M0L6_2atmpS1760;
      int32_t _M0L6_2atmpS1764;
      int32_t _M0L6_2atmpS1763;
      int32_t _M0L6_2atmpS1762;
      _M0Lm9remainingS298 = _M0L1tS299;
      _M0L6_2atmpS1759 = _M0Lm6offsetS285;
      _M0Lm6offsetS285 = _M0L6_2atmpS1759 - 2;
      _M0L6_2atmpS1768 = _M0L1dS300 / 10;
      _M0L6_2atmpS1767 = 48 + _M0L6_2atmpS1768;
      _M0L5d__hiS301 = (uint16_t)_M0L6_2atmpS1767;
      _M0L6_2atmpS1766 = _M0L1dS300 % 10;
      _M0L6_2atmpS1765 = 48 + _M0L6_2atmpS1766;
      _M0L5d__loS302 = (uint16_t)_M0L6_2atmpS1765;
      _M0L6_2atmpS1761 = _M0Lm6offsetS285;
      _M0L6_2atmpS1760 = _M0L12digit__startS287 + _M0L6_2atmpS1761;
      _M0L6bufferS296[_M0L6_2atmpS1760] = _M0L5d__hiS301;
      _M0L6_2atmpS1764 = _M0Lm6offsetS285;
      _M0L6_2atmpS1763 = _M0L12digit__startS287 + _M0L6_2atmpS1764;
      _M0L6_2atmpS1762 = _M0L6_2atmpS1763 + 1;
      _M0L6bufferS296[_M0L6_2atmpS1762] = _M0L5d__loS302;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1771 = _M0Lm9remainingS298;
  if (_M0L6_2atmpS1771 >= 10) {
    int32_t _M0L6_2atmpS1772 = _M0Lm6offsetS285;
    int32_t _M0L6_2atmpS1783;
    int32_t _M0L6_2atmpS1782;
    int32_t _M0L6_2atmpS1781;
    int32_t _M0L5d__hiS304;
    int32_t _M0L6_2atmpS1780;
    int32_t _M0L6_2atmpS1779;
    int32_t _M0L6_2atmpS1778;
    int32_t _M0L5d__loS305;
    int32_t _M0L6_2atmpS1774;
    int32_t _M0L6_2atmpS1773;
    int32_t _M0L6_2atmpS1777;
    int32_t _M0L6_2atmpS1776;
    int32_t _M0L6_2atmpS1775;
    _M0Lm6offsetS285 = _M0L6_2atmpS1772 - 2;
    _M0L6_2atmpS1783 = _M0Lm9remainingS298;
    _M0L6_2atmpS1782 = _M0L6_2atmpS1783 / 10;
    _M0L6_2atmpS1781 = 48 + _M0L6_2atmpS1782;
    _M0L5d__hiS304 = (uint16_t)_M0L6_2atmpS1781;
    _M0L6_2atmpS1780 = _M0Lm9remainingS298;
    _M0L6_2atmpS1779 = _M0L6_2atmpS1780 % 10;
    _M0L6_2atmpS1778 = 48 + _M0L6_2atmpS1779;
    _M0L5d__loS305 = (uint16_t)_M0L6_2atmpS1778;
    _M0L6_2atmpS1774 = _M0Lm6offsetS285;
    _M0L6_2atmpS1773 = _M0L12digit__startS287 + _M0L6_2atmpS1774;
    _M0L6bufferS296[_M0L6_2atmpS1773] = _M0L5d__hiS304;
    _M0L6_2atmpS1777 = _M0Lm6offsetS285;
    _M0L6_2atmpS1776 = _M0L12digit__startS287 + _M0L6_2atmpS1777;
    _M0L6_2atmpS1775 = _M0L6_2atmpS1776 + 1;
    _M0L6bufferS296[_M0L6_2atmpS1775] = _M0L5d__loS305;
    moonbit_decref(_M0L6bufferS296);
  } else {
    int32_t _M0L6_2atmpS1784 = _M0Lm6offsetS285;
    int32_t _M0L6_2atmpS1789;
    int32_t _M0L6_2atmpS1785;
    int32_t _M0L6_2atmpS1788;
    int32_t _M0L6_2atmpS1787;
    int32_t _M0L6_2atmpS1786;
    _M0Lm6offsetS285 = _M0L6_2atmpS1784 - 1;
    _M0L6_2atmpS1789 = _M0Lm6offsetS285;
    _M0L6_2atmpS1785 = _M0L12digit__startS287 + _M0L6_2atmpS1789;
    _M0L6_2atmpS1788 = _M0Lm9remainingS298;
    _M0L6_2atmpS1787 = 48 + _M0L6_2atmpS1788;
    _M0L6_2atmpS1786 = (uint16_t)_M0L6_2atmpS1787;
    _M0L6bufferS296[_M0L6_2atmpS1785] = _M0L6_2atmpS1786;
    moonbit_decref(_M0L6bufferS296);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS278,
  uint32_t _M0L3numS272,
  int32_t _M0L12digit__startS270,
  int32_t _M0L10total__lenS269,
  int32_t _M0L5radixS274
) {
  int32_t _M0Lm6offsetS268;
  uint32_t _M0Lm1nS271;
  uint32_t _M0L4baseS273;
  int32_t _M0L6_2atmpS1716;
  int32_t _M0L6_2atmpS1715;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS268 = _M0L10total__lenS269 - _M0L12digit__startS270;
  _M0Lm1nS271 = _M0L3numS272;
  _M0L4baseS273 = *(uint32_t*)&_M0L5radixS274;
  _M0L6_2atmpS1716 = _M0L5radixS274 - 1;
  _M0L6_2atmpS1715 = _M0L5radixS274 & _M0L6_2atmpS1716;
  if (_M0L6_2atmpS1715 == 0) {
    int32_t _M0L5shiftS275;
    uint32_t _M0L4maskS276;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS275 = moonbit_ctz32(_M0L5radixS274);
    _M0L4maskS276 = _M0L4baseS273 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1717 = _M0Lm1nS271;
      if (_M0L6_2atmpS1717 > 0u) {
        int32_t _M0L6_2atmpS1718 = _M0Lm6offsetS268;
        uint32_t _M0L6_2atmpS1724;
        uint32_t _M0L6_2atmpS1723;
        int32_t _M0L5digitS277;
        int32_t _M0L6_2atmpS1721;
        int32_t _M0L6_2atmpS1719;
        int32_t _M0L6_2atmpS1720;
        uint32_t _M0L6_2atmpS1722;
        _M0Lm6offsetS268 = _M0L6_2atmpS1718 - 1;
        _M0L6_2atmpS1724 = _M0Lm1nS271;
        _M0L6_2atmpS1723 = _M0L6_2atmpS1724 & _M0L4maskS276;
        _M0L5digitS277 = *(int32_t*)&_M0L6_2atmpS1723;
        _M0L6_2atmpS1721 = _M0Lm6offsetS268;
        _M0L6_2atmpS1719 = _M0L12digit__startS270 + _M0L6_2atmpS1721;
        _M0L6_2atmpS1720
        = ((moonbit_string_t)moonbit_string_literal_70.data)[
          _M0L5digitS277
        ];
        _M0L6bufferS278[_M0L6_2atmpS1719] = _M0L6_2atmpS1720;
        _M0L6_2atmpS1722 = _M0Lm1nS271;
        _M0Lm1nS271 = _M0L6_2atmpS1722 >> (_M0L5shiftS275 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS278);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1725 = _M0Lm1nS271;
      if (_M0L6_2atmpS1725 > 0u) {
        int32_t _M0L6_2atmpS1726 = _M0Lm6offsetS268;
        uint32_t _M0L6_2atmpS1733;
        uint32_t _M0L1qS280;
        uint32_t _M0L6_2atmpS1731;
        uint32_t _M0L6_2atmpS1732;
        uint32_t _M0L6_2atmpS1730;
        int32_t _M0L5digitS281;
        int32_t _M0L6_2atmpS1729;
        int32_t _M0L6_2atmpS1727;
        int32_t _M0L6_2atmpS1728;
        _M0Lm6offsetS268 = _M0L6_2atmpS1726 - 1;
        _M0L6_2atmpS1733 = _M0Lm1nS271;
        _M0L1qS280 = _M0L6_2atmpS1733 / _M0L4baseS273;
        _M0L6_2atmpS1731 = _M0Lm1nS271;
        _M0L6_2atmpS1732 = _M0L1qS280 * _M0L4baseS273;
        _M0L6_2atmpS1730 = _M0L6_2atmpS1731 - _M0L6_2atmpS1732;
        _M0L5digitS281 = *(int32_t*)&_M0L6_2atmpS1730;
        _M0L6_2atmpS1729 = _M0Lm6offsetS268;
        _M0L6_2atmpS1727 = _M0L12digit__startS270 + _M0L6_2atmpS1729;
        _M0L6_2atmpS1728
        = ((moonbit_string_t)moonbit_string_literal_70.data)[
          _M0L5digitS281
        ];
        _M0L6bufferS278[_M0L6_2atmpS1727] = _M0L6_2atmpS1728;
        _M0Lm1nS271 = _M0L1qS280;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS278);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS265,
  uint32_t _M0L3numS261,
  int32_t _M0L12digit__startS259,
  int32_t _M0L10total__lenS258
) {
  int32_t _M0Lm6offsetS257;
  uint32_t _M0Lm1nS260;
  int32_t _M0L6_2atmpS1711;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS257 = _M0L10total__lenS258 - _M0L12digit__startS259;
  _M0Lm1nS260 = _M0L3numS261;
  while (1) {
    int32_t _M0L6_2atmpS1699 = _M0Lm6offsetS257;
    if (_M0L6_2atmpS1699 >= 2) {
      int32_t _M0L6_2atmpS1700 = _M0Lm6offsetS257;
      uint32_t _M0L6_2atmpS1710;
      uint32_t _M0L6_2atmpS1709;
      int32_t _M0L9byte__valS262;
      int32_t _M0L2hiS263;
      int32_t _M0L2loS264;
      int32_t _M0L6_2atmpS1703;
      int32_t _M0L6_2atmpS1701;
      int32_t _M0L6_2atmpS1702;
      int32_t _M0L6_2atmpS1707;
      int32_t _M0L6_2atmpS1706;
      int32_t _M0L6_2atmpS1704;
      int32_t _M0L6_2atmpS1705;
      uint32_t _M0L6_2atmpS1708;
      _M0Lm6offsetS257 = _M0L6_2atmpS1700 - 2;
      _M0L6_2atmpS1710 = _M0Lm1nS260;
      _M0L6_2atmpS1709 = _M0L6_2atmpS1710 & 255u;
      _M0L9byte__valS262 = *(int32_t*)&_M0L6_2atmpS1709;
      _M0L2hiS263 = _M0L9byte__valS262 / 16;
      _M0L2loS264 = _M0L9byte__valS262 % 16;
      _M0L6_2atmpS1703 = _M0Lm6offsetS257;
      _M0L6_2atmpS1701 = _M0L12digit__startS259 + _M0L6_2atmpS1703;
      _M0L6_2atmpS1702
      = ((moonbit_string_t)moonbit_string_literal_70.data)[
        _M0L2hiS263
      ];
      _M0L6bufferS265[_M0L6_2atmpS1701] = _M0L6_2atmpS1702;
      _M0L6_2atmpS1707 = _M0Lm6offsetS257;
      _M0L6_2atmpS1706 = _M0L12digit__startS259 + _M0L6_2atmpS1707;
      _M0L6_2atmpS1704 = _M0L6_2atmpS1706 + 1;
      _M0L6_2atmpS1705
      = ((moonbit_string_t)moonbit_string_literal_70.data)[
        _M0L2loS264
      ];
      _M0L6bufferS265[_M0L6_2atmpS1704] = _M0L6_2atmpS1705;
      _M0L6_2atmpS1708 = _M0Lm1nS260;
      _M0Lm1nS260 = _M0L6_2atmpS1708 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1711 = _M0Lm6offsetS257;
  if (_M0L6_2atmpS1711 == 1) {
    uint32_t _M0L6_2atmpS1714 = _M0Lm1nS260;
    uint32_t _M0L6_2atmpS1713 = _M0L6_2atmpS1714 & 15u;
    int32_t _M0L6nibbleS267 = *(int32_t*)&_M0L6_2atmpS1713;
    int32_t _M0L6_2atmpS1712 =
      ((moonbit_string_t)moonbit_string_literal_70.data)[_M0L6nibbleS267];
    _M0L6bufferS265[_M0L12digit__startS259] = _M0L6_2atmpS1712;
    moonbit_decref(_M0L6bufferS265);
  } else {
    moonbit_decref(_M0L6bufferS265);
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

struct _M0TUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0MPB4Iter4nextGUsRP48clawteam8clawteam8internal3lru5EntryGiEEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L4selfS254
) {
  struct _M0TWEOUsRP48clawteam8clawteam8internal3lru5EntryGiEE* _M0L7_2afuncS253;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS253 = _M0L4selfS254;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS253->code(_M0L7_2afuncS253);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS256) {
  struct _M0TWEOc* _M0L7_2afuncS255;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS255 = _M0L4selfS256;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS255->code(_M0L7_2afuncS255);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS242
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS241;
  struct _M0TPB6Logger _M0L6_2atmpS1695;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS241 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS241);
  _M0L6_2atmpS1695
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS241
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS242, _M0L6_2atmpS1695);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS241);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS244
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS243;
  struct _M0TPB6Logger _M0L6_2atmpS1696;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS243 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS243);
  _M0L6_2atmpS1696
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS243
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS244, _M0L6_2atmpS1696);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS243);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS246
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS245;
  struct _M0TPB6Logger _M0L6_2atmpS1697;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS245 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS245);
  _M0L6_2atmpS1697
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS245
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS246, _M0L6_2atmpS1697);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS245);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS247;
  struct _M0TPB6Logger _M0L6_2atmpS1698;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS247);
  _M0L6_2atmpS1698
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS247
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS248, _M0L6_2atmpS1698);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS247);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS240
) {
  int32_t _M0L8_2afieldS3412;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3412 = _M0L4selfS240.$1;
  moonbit_decref(_M0L4selfS240.$0);
  return _M0L8_2afieldS3412;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS239
) {
  int32_t _M0L3endS1693;
  int32_t _M0L8_2afieldS3413;
  int32_t _M0L5startS1694;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1693 = _M0L4selfS239.$2;
  _M0L8_2afieldS3413 = _M0L4selfS239.$1;
  moonbit_decref(_M0L4selfS239.$0);
  _M0L5startS1694 = _M0L8_2afieldS3413;
  return _M0L3endS1693 - _M0L5startS1694;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS238
) {
  moonbit_string_t _M0L8_2afieldS3414;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3414 = _M0L4selfS238.$0;
  return _M0L8_2afieldS3414;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS234,
  moonbit_string_t _M0L5valueS235,
  int32_t _M0L5startS236,
  int32_t _M0L3lenS237
) {
  int32_t _M0L6_2atmpS1692;
  int64_t _M0L6_2atmpS1691;
  struct _M0TPC16string10StringView _M0L6_2atmpS1690;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1692 = _M0L5startS236 + _M0L3lenS237;
  _M0L6_2atmpS1691 = (int64_t)_M0L6_2atmpS1692;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1690
  = _M0MPC16string6String11sub_2einner(_M0L5valueS235, _M0L5startS236, _M0L6_2atmpS1691);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS234, _M0L6_2atmpS1690);
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
  int32_t _if__result_3744;
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
      _if__result_3744 = _M0L3endS228 <= _M0L3lenS226;
    } else {
      _if__result_3744 = 0;
    }
  } else {
    _if__result_3744 = 0;
  }
  if (_if__result_3744) {
    if (_M0L5startS232 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1687 = _M0L4selfS227[_M0L5startS232];
      int32_t _M0L6_2atmpS1686;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1686
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1687);
      if (!_M0L6_2atmpS1686) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS228 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1689 = _M0L4selfS227[_M0L3endS228];
      int32_t _M0L6_2atmpS1688;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1688
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1689);
      if (!_M0L6_2atmpS1688) {
        
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
  uint32_t _M0L6_2atmpS1685;
  uint32_t _M0L6_2atmpS1684;
  struct _M0TPB6Hasher* _block_3745;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1685 = *(uint32_t*)&_M0L4seedS218;
  _M0L6_2atmpS1684 = _M0L6_2atmpS1685 + 374761393u;
  _block_3745
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3745)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3745->$0 = _M0L6_2atmpS1684;
  return _block_3745;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS217) {
  uint32_t _M0L6_2atmpS1683;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1683 = _M0MPB6Hasher9avalanche(_M0L4selfS217);
  return *(int32_t*)&_M0L6_2atmpS1683;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS216) {
  uint32_t _M0L8_2afieldS3415;
  uint32_t _M0Lm3accS215;
  uint32_t _M0L6_2atmpS1672;
  uint32_t _M0L6_2atmpS1674;
  uint32_t _M0L6_2atmpS1673;
  uint32_t _M0L6_2atmpS1675;
  uint32_t _M0L6_2atmpS1676;
  uint32_t _M0L6_2atmpS1678;
  uint32_t _M0L6_2atmpS1677;
  uint32_t _M0L6_2atmpS1679;
  uint32_t _M0L6_2atmpS1680;
  uint32_t _M0L6_2atmpS1682;
  uint32_t _M0L6_2atmpS1681;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3415 = _M0L4selfS216->$0;
  moonbit_decref(_M0L4selfS216);
  _M0Lm3accS215 = _M0L8_2afieldS3415;
  _M0L6_2atmpS1672 = _M0Lm3accS215;
  _M0L6_2atmpS1674 = _M0Lm3accS215;
  _M0L6_2atmpS1673 = _M0L6_2atmpS1674 >> 15;
  _M0Lm3accS215 = _M0L6_2atmpS1672 ^ _M0L6_2atmpS1673;
  _M0L6_2atmpS1675 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1675 * 2246822519u;
  _M0L6_2atmpS1676 = _M0Lm3accS215;
  _M0L6_2atmpS1678 = _M0Lm3accS215;
  _M0L6_2atmpS1677 = _M0L6_2atmpS1678 >> 13;
  _M0Lm3accS215 = _M0L6_2atmpS1676 ^ _M0L6_2atmpS1677;
  _M0L6_2atmpS1679 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1679 * 3266489917u;
  _M0L6_2atmpS1680 = _M0Lm3accS215;
  _M0L6_2atmpS1682 = _M0Lm3accS215;
  _M0L6_2atmpS1681 = _M0L6_2atmpS1682 >> 16;
  _M0Lm3accS215 = _M0L6_2atmpS1680 ^ _M0L6_2atmpS1681;
  return _M0Lm3accS215;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS213,
  moonbit_string_t _M0L1yS214
) {
  int32_t _M0L6_2atmpS3416;
  int32_t _M0L6_2atmpS1671;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3416 = moonbit_val_array_equal(_M0L1xS213, _M0L1yS214);
  moonbit_decref(_M0L1xS213);
  moonbit_decref(_M0L1yS214);
  _M0L6_2atmpS1671 = _M0L6_2atmpS3416;
  return !_M0L6_2atmpS1671;
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
  int64_t _M0L6_2atmpS1670;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1670 = (int64_t)_M0L4selfS208;
  return *(uint64_t*)&_M0L6_2atmpS1670;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS206,
  moonbit_string_t _M0L4reprS207
) {
  void* _block_3746;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3746 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_3746)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_3746)->$0 = _M0L6numberS206;
  ((struct _M0DTPB4Json6Number*)_block_3746)->$1 = _M0L4reprS207;
  return _block_3746;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS204,
  int32_t _M0L5valueS205
) {
  uint32_t _M0L6_2atmpS1669;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1669 = *(uint32_t*)&_M0L5valueS205;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS204, _M0L6_2atmpS1669);
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
      int32_t _if__result_3748;
      moonbit_string_t* _M0L8_2afieldS3418;
      moonbit_string_t* _M0L3bufS1667;
      moonbit_string_t _M0L6_2atmpS3417;
      moonbit_string_t _M0L4itemS200;
      int32_t _M0L6_2atmpS1668;
      if (_M0L1iS199 != 0) {
        moonbit_incref(_M0L3bufS195);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS195, (moonbit_string_t)moonbit_string_literal_72.data);
      }
      if (_M0L1iS199 < 0) {
        _if__result_3748 = 1;
      } else {
        int32_t _M0L3lenS1666 = _M0L7_2aselfS196->$1;
        _if__result_3748 = _M0L1iS199 >= _M0L3lenS1666;
      }
      if (_if__result_3748) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3418 = _M0L7_2aselfS196->$0;
      _M0L3bufS1667 = _M0L8_2afieldS3418;
      _M0L6_2atmpS3417 = (moonbit_string_t)_M0L3bufS1667[_M0L1iS199];
      _M0L4itemS200 = _M0L6_2atmpS3417;
      if (_M0L4itemS200 == 0) {
        moonbit_incref(_M0L3bufS195);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS195, (moonbit_string_t)moonbit_string_literal_36.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS201 = _M0L4itemS200;
        moonbit_string_t _M0L6_2alocS202 = _M0L7_2aSomeS201;
        moonbit_string_t _M0L6_2atmpS1665;
        moonbit_incref(_M0L6_2alocS202);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1665
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS202);
        moonbit_incref(_M0L3bufS195);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS195, _M0L6_2atmpS1665);
      }
      _M0L6_2atmpS1668 = _M0L1iS199 + 1;
      _M0L1iS199 = _M0L6_2atmpS1668;
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
  moonbit_string_t _M0L6_2atmpS1664;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1663;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1664 = _M0L4selfS194;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1663 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1664);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1663);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS193
) {
  struct _M0TPB13StringBuilder* _M0L2sbS192;
  struct _M0TPC16string10StringView _M0L8_2afieldS3431;
  struct _M0TPC16string10StringView _M0L3pkgS1648;
  moonbit_string_t _M0L6_2atmpS1647;
  moonbit_string_t _M0L6_2atmpS3430;
  moonbit_string_t _M0L6_2atmpS1646;
  moonbit_string_t _M0L6_2atmpS3429;
  moonbit_string_t _M0L6_2atmpS1645;
  struct _M0TPC16string10StringView _M0L8_2afieldS3428;
  struct _M0TPC16string10StringView _M0L8filenameS1649;
  struct _M0TPC16string10StringView _M0L8_2afieldS3427;
  struct _M0TPC16string10StringView _M0L11start__lineS1652;
  moonbit_string_t _M0L6_2atmpS1651;
  moonbit_string_t _M0L6_2atmpS3426;
  moonbit_string_t _M0L6_2atmpS1650;
  struct _M0TPC16string10StringView _M0L8_2afieldS3425;
  struct _M0TPC16string10StringView _M0L13start__columnS1655;
  moonbit_string_t _M0L6_2atmpS1654;
  moonbit_string_t _M0L6_2atmpS3424;
  moonbit_string_t _M0L6_2atmpS1653;
  struct _M0TPC16string10StringView _M0L8_2afieldS3423;
  struct _M0TPC16string10StringView _M0L9end__lineS1658;
  moonbit_string_t _M0L6_2atmpS1657;
  moonbit_string_t _M0L6_2atmpS3422;
  moonbit_string_t _M0L6_2atmpS1656;
  struct _M0TPC16string10StringView _M0L8_2afieldS3421;
  int32_t _M0L6_2acntS3578;
  struct _M0TPC16string10StringView _M0L11end__columnS1662;
  moonbit_string_t _M0L6_2atmpS1661;
  moonbit_string_t _M0L6_2atmpS3420;
  moonbit_string_t _M0L6_2atmpS1660;
  moonbit_string_t _M0L6_2atmpS3419;
  moonbit_string_t _M0L6_2atmpS1659;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS192 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3431
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$0_1, _M0L4selfS193->$0_2, _M0L4selfS193->$0_0
  };
  _M0L3pkgS1648 = _M0L8_2afieldS3431;
  moonbit_incref(_M0L3pkgS1648.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1647
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1648);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3430
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_73.data, _M0L6_2atmpS1647);
  moonbit_decref(_M0L6_2atmpS1647);
  _M0L6_2atmpS1646 = _M0L6_2atmpS3430;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3429
  = moonbit_add_string(_M0L6_2atmpS1646, (moonbit_string_t)moonbit_string_literal_74.data);
  moonbit_decref(_M0L6_2atmpS1646);
  _M0L6_2atmpS1645 = _M0L6_2atmpS3429;
  moonbit_incref(_M0L2sbS192);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1645);
  moonbit_incref(_M0L2sbS192);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, (moonbit_string_t)moonbit_string_literal_75.data);
  _M0L8_2afieldS3428
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$1_1, _M0L4selfS193->$1_2, _M0L4selfS193->$1_0
  };
  _M0L8filenameS1649 = _M0L8_2afieldS3428;
  moonbit_incref(_M0L8filenameS1649.$0);
  moonbit_incref(_M0L2sbS192);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS192, _M0L8filenameS1649);
  _M0L8_2afieldS3427
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$2_1, _M0L4selfS193->$2_2, _M0L4selfS193->$2_0
  };
  _M0L11start__lineS1652 = _M0L8_2afieldS3427;
  moonbit_incref(_M0L11start__lineS1652.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1651
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1652);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3426
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_76.data, _M0L6_2atmpS1651);
  moonbit_decref(_M0L6_2atmpS1651);
  _M0L6_2atmpS1650 = _M0L6_2atmpS3426;
  moonbit_incref(_M0L2sbS192);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1650);
  _M0L8_2afieldS3425
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$3_1, _M0L4selfS193->$3_2, _M0L4selfS193->$3_0
  };
  _M0L13start__columnS1655 = _M0L8_2afieldS3425;
  moonbit_incref(_M0L13start__columnS1655.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1654
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1655);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3424
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_77.data, _M0L6_2atmpS1654);
  moonbit_decref(_M0L6_2atmpS1654);
  _M0L6_2atmpS1653 = _M0L6_2atmpS3424;
  moonbit_incref(_M0L2sbS192);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1653);
  _M0L8_2afieldS3423
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$4_1, _M0L4selfS193->$4_2, _M0L4selfS193->$4_0
  };
  _M0L9end__lineS1658 = _M0L8_2afieldS3423;
  moonbit_incref(_M0L9end__lineS1658.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1657
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1658);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3422
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_78.data, _M0L6_2atmpS1657);
  moonbit_decref(_M0L6_2atmpS1657);
  _M0L6_2atmpS1656 = _M0L6_2atmpS3422;
  moonbit_incref(_M0L2sbS192);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1656);
  _M0L8_2afieldS3421
  = (struct _M0TPC16string10StringView){
    _M0L4selfS193->$5_1, _M0L4selfS193->$5_2, _M0L4selfS193->$5_0
  };
  _M0L6_2acntS3578 = Moonbit_object_header(_M0L4selfS193)->rc;
  if (_M0L6_2acntS3578 > 1) {
    int32_t _M0L11_2anew__cntS3584 = _M0L6_2acntS3578 - 1;
    Moonbit_object_header(_M0L4selfS193)->rc = _M0L11_2anew__cntS3584;
    moonbit_incref(_M0L8_2afieldS3421.$0);
  } else if (_M0L6_2acntS3578 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3583 =
      (struct _M0TPC16string10StringView){_M0L4selfS193->$4_1,
                                            _M0L4selfS193->$4_2,
                                            _M0L4selfS193->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3582;
    struct _M0TPC16string10StringView _M0L8_2afieldS3581;
    struct _M0TPC16string10StringView _M0L8_2afieldS3580;
    struct _M0TPC16string10StringView _M0L8_2afieldS3579;
    moonbit_decref(_M0L8_2afieldS3583.$0);
    _M0L8_2afieldS3582
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$3_1, _M0L4selfS193->$3_2, _M0L4selfS193->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3582.$0);
    _M0L8_2afieldS3581
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$2_1, _M0L4selfS193->$2_2, _M0L4selfS193->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3581.$0);
    _M0L8_2afieldS3580
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$1_1, _M0L4selfS193->$1_2, _M0L4selfS193->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3580.$0);
    _M0L8_2afieldS3579
    = (struct _M0TPC16string10StringView){
      _M0L4selfS193->$0_1, _M0L4selfS193->$0_2, _M0L4selfS193->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3579.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS193);
  }
  _M0L11end__columnS1662 = _M0L8_2afieldS3421;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1661
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1662);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3420
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_79.data, _M0L6_2atmpS1661);
  moonbit_decref(_M0L6_2atmpS1661);
  _M0L6_2atmpS1660 = _M0L6_2atmpS3420;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3419
  = moonbit_add_string(_M0L6_2atmpS1660, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1660);
  _M0L6_2atmpS1659 = _M0L6_2atmpS3419;
  moonbit_incref(_M0L2sbS192);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS192, _M0L6_2atmpS1659);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS192);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS190,
  moonbit_string_t _M0L3strS191
) {
  int32_t _M0L3lenS1635;
  int32_t _M0L6_2atmpS1637;
  int32_t _M0L6_2atmpS1636;
  int32_t _M0L6_2atmpS1634;
  moonbit_bytes_t _M0L8_2afieldS3433;
  moonbit_bytes_t _M0L4dataS1638;
  int32_t _M0L3lenS1639;
  int32_t _M0L6_2atmpS1640;
  int32_t _M0L3lenS1642;
  int32_t _M0L6_2atmpS3432;
  int32_t _M0L6_2atmpS1644;
  int32_t _M0L6_2atmpS1643;
  int32_t _M0L6_2atmpS1641;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1635 = _M0L4selfS190->$1;
  _M0L6_2atmpS1637 = Moonbit_array_length(_M0L3strS191);
  _M0L6_2atmpS1636 = _M0L6_2atmpS1637 * 2;
  _M0L6_2atmpS1634 = _M0L3lenS1635 + _M0L6_2atmpS1636;
  moonbit_incref(_M0L4selfS190);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS190, _M0L6_2atmpS1634);
  _M0L8_2afieldS3433 = _M0L4selfS190->$0;
  _M0L4dataS1638 = _M0L8_2afieldS3433;
  _M0L3lenS1639 = _M0L4selfS190->$1;
  _M0L6_2atmpS1640 = Moonbit_array_length(_M0L3strS191);
  moonbit_incref(_M0L4dataS1638);
  moonbit_incref(_M0L3strS191);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1638, _M0L3lenS1639, _M0L3strS191, 0, _M0L6_2atmpS1640);
  _M0L3lenS1642 = _M0L4selfS190->$1;
  _M0L6_2atmpS3432 = Moonbit_array_length(_M0L3strS191);
  moonbit_decref(_M0L3strS191);
  _M0L6_2atmpS1644 = _M0L6_2atmpS3432;
  _M0L6_2atmpS1643 = _M0L6_2atmpS1644 * 2;
  _M0L6_2atmpS1641 = _M0L3lenS1642 + _M0L6_2atmpS1643;
  _M0L4selfS190->$1 = _M0L6_2atmpS1641;
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
  int32_t _M0L6_2atmpS1633;
  int32_t _M0L6_2atmpS1632;
  int32_t _M0L2e1S176;
  int32_t _M0L6_2atmpS1631;
  int32_t _M0L2e2S179;
  int32_t _M0L4len1S181;
  int32_t _M0L4len2S183;
  int32_t _if__result_3749;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1633 = _M0L6lengthS178 * 2;
  _M0L6_2atmpS1632 = _M0L13bytes__offsetS177 + _M0L6_2atmpS1633;
  _M0L2e1S176 = _M0L6_2atmpS1632 - 1;
  _M0L6_2atmpS1631 = _M0L11str__offsetS180 + _M0L6lengthS178;
  _M0L2e2S179 = _M0L6_2atmpS1631 - 1;
  _M0L4len1S181 = Moonbit_array_length(_M0L4selfS182);
  _M0L4len2S183 = Moonbit_array_length(_M0L3strS184);
  if (_M0L6lengthS178 >= 0) {
    if (_M0L13bytes__offsetS177 >= 0) {
      if (_M0L2e1S176 < _M0L4len1S181) {
        if (_M0L11str__offsetS180 >= 0) {
          _if__result_3749 = _M0L2e2S179 < _M0L4len2S183;
        } else {
          _if__result_3749 = 0;
        }
      } else {
        _if__result_3749 = 0;
      }
    } else {
      _if__result_3749 = 0;
    }
  } else {
    _if__result_3749 = 0;
  }
  if (_if__result_3749) {
    int32_t _M0L16end__str__offsetS185 =
      _M0L11str__offsetS180 + _M0L6lengthS178;
    int32_t _M0L1iS186 = _M0L11str__offsetS180;
    int32_t _M0L1jS187 = _M0L13bytes__offsetS177;
    while (1) {
      if (_M0L1iS186 < _M0L16end__str__offsetS185) {
        int32_t _M0L6_2atmpS1628 = _M0L3strS184[_M0L1iS186];
        int32_t _M0L6_2atmpS1627 = (int32_t)_M0L6_2atmpS1628;
        uint32_t _M0L1cS188 = *(uint32_t*)&_M0L6_2atmpS1627;
        uint32_t _M0L6_2atmpS1623 = _M0L1cS188 & 255u;
        int32_t _M0L6_2atmpS1622;
        int32_t _M0L6_2atmpS1624;
        uint32_t _M0L6_2atmpS1626;
        int32_t _M0L6_2atmpS1625;
        int32_t _M0L6_2atmpS1629;
        int32_t _M0L6_2atmpS1630;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1622 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1623);
        if (
          _M0L1jS187 < 0 || _M0L1jS187 >= Moonbit_array_length(_M0L4selfS182)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS182[_M0L1jS187] = _M0L6_2atmpS1622;
        _M0L6_2atmpS1624 = _M0L1jS187 + 1;
        _M0L6_2atmpS1626 = _M0L1cS188 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1625 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1626);
        if (
          _M0L6_2atmpS1624 < 0
          || _M0L6_2atmpS1624 >= Moonbit_array_length(_M0L4selfS182)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS182[_M0L6_2atmpS1624] = _M0L6_2atmpS1625;
        _M0L6_2atmpS1629 = _M0L1iS186 + 1;
        _M0L6_2atmpS1630 = _M0L1jS187 + 2;
        _M0L1iS186 = _M0L6_2atmpS1629;
        _M0L1jS187 = _M0L6_2atmpS1630;
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
  struct _M0TPB6Logger _M0L6_2atmpS1620;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1620
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS173
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS172, _M0L6_2atmpS1620);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS175,
  struct _M0TPC16string10StringView _M0L3objS174
) {
  struct _M0TPB6Logger _M0L6_2atmpS1621;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1621
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS175
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS174, _M0L6_2atmpS1621);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS118
) {
  int32_t _M0L6_2atmpS1619;
  struct _M0TPC16string10StringView _M0L7_2abindS117;
  moonbit_string_t _M0L7_2adataS119;
  int32_t _M0L8_2astartS120;
  int32_t _M0L6_2atmpS1618;
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
  int32_t _M0L6_2atmpS1576;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1619 = Moonbit_array_length(_M0L4reprS118);
  _M0L7_2abindS117
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1619, _M0L4reprS118
  };
  moonbit_incref(_M0L7_2abindS117.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS119 = _M0MPC16string10StringView4data(_M0L7_2abindS117);
  moonbit_incref(_M0L7_2abindS117.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS120
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS117);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1618 = _M0MPC16string10StringView6length(_M0L7_2abindS117);
  _M0L6_2aendS121 = _M0L8_2astartS120 + _M0L6_2atmpS1618;
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
  _M0L6_2atmpS1576 = _M0Lm9_2acursorS122;
  if (_M0L6_2atmpS1576 < _M0L6_2aendS121) {
    int32_t _M0L6_2atmpS1578 = _M0Lm9_2acursorS122;
    int32_t _M0L6_2atmpS1577;
    moonbit_incref(_M0L7_2adataS119);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1577
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1578);
    if (_M0L6_2atmpS1577 == 64) {
      int32_t _M0L6_2atmpS1579 = _M0Lm9_2acursorS122;
      _M0Lm9_2acursorS122 = _M0L6_2atmpS1579 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1580;
        _M0Lm6tag__0S130 = _M0Lm9_2acursorS122;
        _M0L6_2atmpS1580 = _M0Lm9_2acursorS122;
        if (_M0L6_2atmpS1580 < _M0L6_2aendS121) {
          int32_t _M0L6_2atmpS1617 = _M0Lm9_2acursorS122;
          int32_t _M0L10next__charS145;
          int32_t _M0L6_2atmpS1581;
          moonbit_incref(_M0L7_2adataS119);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS145
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1617);
          _M0L6_2atmpS1581 = _M0Lm9_2acursorS122;
          _M0Lm9_2acursorS122 = _M0L6_2atmpS1581 + 1;
          if (_M0L10next__charS145 == 58) {
            int32_t _M0L6_2atmpS1582 = _M0Lm9_2acursorS122;
            if (_M0L6_2atmpS1582 < _M0L6_2aendS121) {
              int32_t _M0L6_2atmpS1583 = _M0Lm9_2acursorS122;
              int32_t _M0L12dispatch__15S146;
              _M0Lm9_2acursorS122 = _M0L6_2atmpS1583 + 1;
              _M0L12dispatch__15S146 = 0;
              loop__label__15_149:;
              while (1) {
                int32_t _M0L6_2atmpS1584;
                switch (_M0L12dispatch__15S146) {
                  case 3: {
                    int32_t _M0L6_2atmpS1587;
                    _M0Lm9tag__1__2S133 = _M0Lm9tag__1__1S132;
                    _M0Lm9tag__1__1S132 = _M0Lm6tag__1S131;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1587 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1587 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1592 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS153;
                      int32_t _M0L6_2atmpS1588;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS153
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1592);
                      _M0L6_2atmpS1588 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1588 + 1;
                      if (_M0L10next__charS153 < 58) {
                        if (_M0L10next__charS153 < 48) {
                          goto join_152;
                        } else {
                          int32_t _M0L6_2atmpS1589;
                          _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                          _M0Lm9tag__2__1S136 = _M0Lm6tag__2S135;
                          _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                          _M0Lm6tag__3S134 = _M0Lm9_2acursorS122;
                          _M0L6_2atmpS1589 = _M0Lm9_2acursorS122;
                          if (_M0L6_2atmpS1589 < _M0L6_2aendS121) {
                            int32_t _M0L6_2atmpS1591 = _M0Lm9_2acursorS122;
                            int32_t _M0L10next__charS155;
                            int32_t _M0L6_2atmpS1590;
                            moonbit_incref(_M0L7_2adataS119);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS155
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1591);
                            _M0L6_2atmpS1590 = _M0Lm9_2acursorS122;
                            _M0Lm9_2acursorS122 = _M0L6_2atmpS1590 + 1;
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
                    int32_t _M0L6_2atmpS1593;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1593 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1593 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1595 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS157;
                      int32_t _M0L6_2atmpS1594;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS157
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1595);
                      _M0L6_2atmpS1594 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1594 + 1;
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
                    int32_t _M0L6_2atmpS1596;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1596 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1596 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1598 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS158;
                      int32_t _M0L6_2atmpS1597;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS158
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1598);
                      _M0L6_2atmpS1597 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1597 + 1;
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
                    int32_t _M0L6_2atmpS1599;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__4S137 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1599 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1599 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1607 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1600;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1607);
                      _M0L6_2atmpS1600 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1600 + 1;
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
                        int32_t _M0L6_2atmpS1601;
                        _M0Lm9tag__1__2S133 = _M0Lm9tag__1__1S132;
                        _M0Lm9tag__1__1S132 = _M0Lm6tag__1S131;
                        _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                        _M0L6_2atmpS1601 = _M0Lm9_2acursorS122;
                        if (_M0L6_2atmpS1601 < _M0L6_2aendS121) {
                          int32_t _M0L6_2atmpS1606 = _M0Lm9_2acursorS122;
                          int32_t _M0L10next__charS162;
                          int32_t _M0L6_2atmpS1602;
                          moonbit_incref(_M0L7_2adataS119);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS162
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1606);
                          _M0L6_2atmpS1602 = _M0Lm9_2acursorS122;
                          _M0Lm9_2acursorS122 = _M0L6_2atmpS1602 + 1;
                          if (_M0L10next__charS162 < 58) {
                            if (_M0L10next__charS162 < 48) {
                              goto join_161;
                            } else {
                              int32_t _M0L6_2atmpS1603;
                              _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                              _M0Lm9tag__2__1S136 = _M0Lm6tag__2S135;
                              _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                              _M0L6_2atmpS1603 = _M0Lm9_2acursorS122;
                              if (_M0L6_2atmpS1603 < _M0L6_2aendS121) {
                                int32_t _M0L6_2atmpS1605 =
                                  _M0Lm9_2acursorS122;
                                int32_t _M0L10next__charS164;
                                int32_t _M0L6_2atmpS1604;
                                moonbit_incref(_M0L7_2adataS119);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS164
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1605);
                                _M0L6_2atmpS1604 = _M0Lm9_2acursorS122;
                                _M0Lm9_2acursorS122 = _M0L6_2atmpS1604 + 1;
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
                    int32_t _M0L6_2atmpS1608;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1608 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1608 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1610 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS166;
                      int32_t _M0L6_2atmpS1609;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS166
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1610);
                      _M0L6_2atmpS1609 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1609 + 1;
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
                    int32_t _M0L6_2atmpS1611;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__2S135 = _M0Lm9_2acursorS122;
                    _M0Lm6tag__3S134 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1611 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1611 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1613 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS168;
                      int32_t _M0L6_2atmpS1612;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS168
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1613);
                      _M0L6_2atmpS1612 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1612 + 1;
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
                    int32_t _M0L6_2atmpS1614;
                    _M0Lm9tag__1__1S132 = _M0Lm6tag__1S131;
                    _M0Lm6tag__1S131 = _M0Lm9_2acursorS122;
                    _M0L6_2atmpS1614 = _M0Lm9_2acursorS122;
                    if (_M0L6_2atmpS1614 < _M0L6_2aendS121) {
                      int32_t _M0L6_2atmpS1616 = _M0Lm9_2acursorS122;
                      int32_t _M0L10next__charS170;
                      int32_t _M0L6_2atmpS1615;
                      moonbit_incref(_M0L7_2adataS119);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS170
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1616);
                      _M0L6_2atmpS1615 = _M0Lm9_2acursorS122;
                      _M0Lm9_2acursorS122 = _M0L6_2atmpS1615 + 1;
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
                _M0L6_2atmpS1584 = _M0Lm9_2acursorS122;
                if (_M0L6_2atmpS1584 < _M0L6_2aendS121) {
                  int32_t _M0L6_2atmpS1586 = _M0Lm9_2acursorS122;
                  int32_t _M0L10next__charS150;
                  int32_t _M0L6_2atmpS1585;
                  moonbit_incref(_M0L7_2adataS119);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS150
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS119, _M0L6_2atmpS1586);
                  _M0L6_2atmpS1585 = _M0Lm9_2acursorS122;
                  _M0Lm9_2acursorS122 = _M0L6_2atmpS1585 + 1;
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
      int32_t _M0L6_2atmpS1575 = _M0Lm20match__tag__saver__1S126;
      int32_t _M0L6_2atmpS1574 = _M0L6_2atmpS1575 + 1;
      int64_t _M0L6_2atmpS1571 = (int64_t)_M0L6_2atmpS1574;
      int32_t _M0L6_2atmpS1573 = _M0Lm20match__tag__saver__2S127;
      int64_t _M0L6_2atmpS1572 = (int64_t)_M0L6_2atmpS1573;
      struct _M0TPC16string10StringView _M0L11start__lineS139;
      int32_t _M0L6_2atmpS1570;
      int32_t _M0L6_2atmpS1569;
      int64_t _M0L6_2atmpS1566;
      int32_t _M0L6_2atmpS1568;
      int64_t _M0L6_2atmpS1567;
      struct _M0TPC16string10StringView _M0L13start__columnS140;
      int32_t _M0L6_2atmpS1565;
      int64_t _M0L6_2atmpS1562;
      int32_t _M0L6_2atmpS1564;
      int64_t _M0L6_2atmpS1563;
      struct _M0TPC16string10StringView _M0L3pkgS141;
      int32_t _M0L6_2atmpS1561;
      int32_t _M0L6_2atmpS1560;
      int64_t _M0L6_2atmpS1557;
      int32_t _M0L6_2atmpS1559;
      int64_t _M0L6_2atmpS1558;
      struct _M0TPC16string10StringView _M0L8filenameS142;
      int32_t _M0L6_2atmpS1556;
      int32_t _M0L6_2atmpS1555;
      int64_t _M0L6_2atmpS1552;
      int32_t _M0L6_2atmpS1554;
      int64_t _M0L6_2atmpS1553;
      struct _M0TPC16string10StringView _M0L9end__lineS143;
      int32_t _M0L6_2atmpS1551;
      int32_t _M0L6_2atmpS1550;
      int64_t _M0L6_2atmpS1547;
      int32_t _M0L6_2atmpS1549;
      int64_t _M0L6_2atmpS1548;
      struct _M0TPC16string10StringView _M0L11end__columnS144;
      struct _M0TPB13SourceLocRepr* _block_3766;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS139
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1571, _M0L6_2atmpS1572);
      _M0L6_2atmpS1570 = _M0Lm20match__tag__saver__2S127;
      _M0L6_2atmpS1569 = _M0L6_2atmpS1570 + 1;
      _M0L6_2atmpS1566 = (int64_t)_M0L6_2atmpS1569;
      _M0L6_2atmpS1568 = _M0Lm20match__tag__saver__3S128;
      _M0L6_2atmpS1567 = (int64_t)_M0L6_2atmpS1568;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS140
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1566, _M0L6_2atmpS1567);
      _M0L6_2atmpS1565 = _M0L8_2astartS120 + 1;
      _M0L6_2atmpS1562 = (int64_t)_M0L6_2atmpS1565;
      _M0L6_2atmpS1564 = _M0Lm20match__tag__saver__0S125;
      _M0L6_2atmpS1563 = (int64_t)_M0L6_2atmpS1564;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS141
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1562, _M0L6_2atmpS1563);
      _M0L6_2atmpS1561 = _M0Lm20match__tag__saver__0S125;
      _M0L6_2atmpS1560 = _M0L6_2atmpS1561 + 1;
      _M0L6_2atmpS1557 = (int64_t)_M0L6_2atmpS1560;
      _M0L6_2atmpS1559 = _M0Lm20match__tag__saver__1S126;
      _M0L6_2atmpS1558 = (int64_t)_M0L6_2atmpS1559;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS142
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1557, _M0L6_2atmpS1558);
      _M0L6_2atmpS1556 = _M0Lm20match__tag__saver__3S128;
      _M0L6_2atmpS1555 = _M0L6_2atmpS1556 + 1;
      _M0L6_2atmpS1552 = (int64_t)_M0L6_2atmpS1555;
      _M0L6_2atmpS1554 = _M0Lm20match__tag__saver__4S129;
      _M0L6_2atmpS1553 = (int64_t)_M0L6_2atmpS1554;
      moonbit_incref(_M0L7_2adataS119);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS143
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1552, _M0L6_2atmpS1553);
      _M0L6_2atmpS1551 = _M0Lm20match__tag__saver__4S129;
      _M0L6_2atmpS1550 = _M0L6_2atmpS1551 + 1;
      _M0L6_2atmpS1547 = (int64_t)_M0L6_2atmpS1550;
      _M0L6_2atmpS1549 = _M0Lm10match__endS124;
      _M0L6_2atmpS1548 = (int64_t)_M0L6_2atmpS1549;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS144
      = _M0MPC16string6String4view(_M0L7_2adataS119, _M0L6_2atmpS1547, _M0L6_2atmpS1548);
      _block_3766
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3766)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3766->$0_0 = _M0L3pkgS141.$0;
      _block_3766->$0_1 = _M0L3pkgS141.$1;
      _block_3766->$0_2 = _M0L3pkgS141.$2;
      _block_3766->$1_0 = _M0L8filenameS142.$0;
      _block_3766->$1_1 = _M0L8filenameS142.$1;
      _block_3766->$1_2 = _M0L8filenameS142.$2;
      _block_3766->$2_0 = _M0L11start__lineS139.$0;
      _block_3766->$2_1 = _M0L11start__lineS139.$1;
      _block_3766->$2_2 = _M0L11start__lineS139.$2;
      _block_3766->$3_0 = _M0L13start__columnS140.$0;
      _block_3766->$3_1 = _M0L13start__columnS140.$1;
      _block_3766->$3_2 = _M0L13start__columnS140.$2;
      _block_3766->$4_0 = _M0L9end__lineS143.$0;
      _block_3766->$4_1 = _M0L9end__lineS143.$1;
      _block_3766->$4_2 = _M0L9end__lineS143.$2;
      _block_3766->$5_0 = _M0L11end__columnS144.$0;
      _block_3766->$5_1 = _M0L11end__columnS144.$1;
      _block_3766->$5_2 = _M0L11end__columnS144.$2;
      return _block_3766;
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
  int32_t _if__result_3767;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS114 = _M0L4selfS115->$1;
  if (_M0L5indexS116 >= 0) {
    _if__result_3767 = _M0L5indexS116 < _M0L3lenS114;
  } else {
    _if__result_3767 = 0;
  }
  if (_if__result_3767) {
    moonbit_string_t* _M0L6_2atmpS1546;
    moonbit_string_t _M0L6_2atmpS3434;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1546 = _M0MPC15array5Array6bufferGsE(_M0L4selfS115);
    if (
      _M0L5indexS116 < 0
      || _M0L5indexS116 >= Moonbit_array_length(_M0L6_2atmpS1546)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3434 = (moonbit_string_t)_M0L6_2atmpS1546[_M0L5indexS116];
    moonbit_incref(_M0L6_2atmpS3434);
    moonbit_decref(_M0L6_2atmpS1546);
    return _M0L6_2atmpS3434;
  } else {
    moonbit_decref(_M0L4selfS115);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS111
) {
  moonbit_string_t* _M0L8_2afieldS3435;
  int32_t _M0L6_2acntS3585;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3435 = _M0L4selfS111->$0;
  _M0L6_2acntS3585 = Moonbit_object_header(_M0L4selfS111)->rc;
  if (_M0L6_2acntS3585 > 1) {
    int32_t _M0L11_2anew__cntS3586 = _M0L6_2acntS3585 - 1;
    Moonbit_object_header(_M0L4selfS111)->rc = _M0L11_2anew__cntS3586;
    moonbit_incref(_M0L8_2afieldS3435);
  } else if (_M0L6_2acntS3585 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS111);
  }
  return _M0L8_2afieldS3435;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS112
) {
  struct _M0TUsiE** _M0L8_2afieldS3436;
  int32_t _M0L6_2acntS3587;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3436 = _M0L4selfS112->$0;
  _M0L6_2acntS3587 = Moonbit_object_header(_M0L4selfS112)->rc;
  if (_M0L6_2acntS3587 > 1) {
    int32_t _M0L11_2anew__cntS3588 = _M0L6_2acntS3587 - 1;
    Moonbit_object_header(_M0L4selfS112)->rc = _M0L11_2anew__cntS3588;
    moonbit_incref(_M0L8_2afieldS3436);
  } else if (_M0L6_2acntS3587 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS112);
  }
  return _M0L8_2afieldS3436;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS113
) {
  void** _M0L8_2afieldS3437;
  int32_t _M0L6_2acntS3589;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3437 = _M0L4selfS113->$0;
  _M0L6_2acntS3589 = Moonbit_object_header(_M0L4selfS113)->rc;
  if (_M0L6_2acntS3589 > 1) {
    int32_t _M0L11_2anew__cntS3590 = _M0L6_2acntS3589 - 1;
    Moonbit_object_header(_M0L4selfS113)->rc = _M0L11_2anew__cntS3590;
    moonbit_incref(_M0L8_2afieldS3437);
  } else if (_M0L6_2acntS3589 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS113);
  }
  return _M0L8_2afieldS3437;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS110) {
  struct _M0TPB13StringBuilder* _M0L3bufS109;
  struct _M0TPB6Logger _M0L6_2atmpS1545;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS109 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS109);
  _M0L6_2atmpS1545
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS109
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS110, _M0L6_2atmpS1545);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS109);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS108) {
  int32_t _M0L6_2atmpS1544;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1544 = (int32_t)_M0L4selfS108;
  return _M0L6_2atmpS1544;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS106,
  int32_t _M0L8trailingS107
) {
  int32_t _M0L6_2atmpS1543;
  int32_t _M0L6_2atmpS1542;
  int32_t _M0L6_2atmpS1541;
  int32_t _M0L6_2atmpS1540;
  int32_t _M0L6_2atmpS1539;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1543 = _M0L7leadingS106 - 55296;
  _M0L6_2atmpS1542 = _M0L6_2atmpS1543 * 1024;
  _M0L6_2atmpS1541 = _M0L6_2atmpS1542 + _M0L8trailingS107;
  _M0L6_2atmpS1540 = _M0L6_2atmpS1541 - 56320;
  _M0L6_2atmpS1539 = _M0L6_2atmpS1540 + 65536;
  return _M0L6_2atmpS1539;
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
  int32_t _M0L3lenS1534;
  int32_t _M0L6_2atmpS1533;
  moonbit_bytes_t _M0L8_2afieldS3438;
  moonbit_bytes_t _M0L4dataS1537;
  int32_t _M0L3lenS1538;
  int32_t _M0L3incS102;
  int32_t _M0L3lenS1536;
  int32_t _M0L6_2atmpS1535;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1534 = _M0L4selfS101->$1;
  _M0L6_2atmpS1533 = _M0L3lenS1534 + 4;
  moonbit_incref(_M0L4selfS101);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS101, _M0L6_2atmpS1533);
  _M0L8_2afieldS3438 = _M0L4selfS101->$0;
  _M0L4dataS1537 = _M0L8_2afieldS3438;
  _M0L3lenS1538 = _M0L4selfS101->$1;
  moonbit_incref(_M0L4dataS1537);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS102
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1537, _M0L3lenS1538, _M0L2chS103);
  _M0L3lenS1536 = _M0L4selfS101->$1;
  _M0L6_2atmpS1535 = _M0L3lenS1536 + _M0L3incS102;
  _M0L4selfS101->$1 = _M0L6_2atmpS1535;
  moonbit_decref(_M0L4selfS101);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS96,
  int32_t _M0L8requiredS97
) {
  moonbit_bytes_t _M0L8_2afieldS3442;
  moonbit_bytes_t _M0L4dataS1532;
  int32_t _M0L6_2atmpS3441;
  int32_t _M0L12current__lenS95;
  int32_t _M0Lm13enough__spaceS98;
  int32_t _M0L6_2atmpS1530;
  int32_t _M0L6_2atmpS1531;
  moonbit_bytes_t _M0L9new__dataS100;
  moonbit_bytes_t _M0L8_2afieldS3440;
  moonbit_bytes_t _M0L4dataS1528;
  int32_t _M0L3lenS1529;
  moonbit_bytes_t _M0L6_2aoldS3439;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3442 = _M0L4selfS96->$0;
  _M0L4dataS1532 = _M0L8_2afieldS3442;
  _M0L6_2atmpS3441 = Moonbit_array_length(_M0L4dataS1532);
  _M0L12current__lenS95 = _M0L6_2atmpS3441;
  if (_M0L8requiredS97 <= _M0L12current__lenS95) {
    moonbit_decref(_M0L4selfS96);
    return 0;
  }
  _M0Lm13enough__spaceS98 = _M0L12current__lenS95;
  while (1) {
    int32_t _M0L6_2atmpS1526 = _M0Lm13enough__spaceS98;
    if (_M0L6_2atmpS1526 < _M0L8requiredS97) {
      int32_t _M0L6_2atmpS1527 = _M0Lm13enough__spaceS98;
      _M0Lm13enough__spaceS98 = _M0L6_2atmpS1527 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1530 = _M0Lm13enough__spaceS98;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1531 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS100
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1530, _M0L6_2atmpS1531);
  _M0L8_2afieldS3440 = _M0L4selfS96->$0;
  _M0L4dataS1528 = _M0L8_2afieldS3440;
  _M0L3lenS1529 = _M0L4selfS96->$1;
  moonbit_incref(_M0L4dataS1528);
  moonbit_incref(_M0L9new__dataS100);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS100, 0, _M0L4dataS1528, 0, _M0L3lenS1529);
  _M0L6_2aoldS3439 = _M0L4selfS96->$0;
  moonbit_decref(_M0L6_2aoldS3439);
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
    uint32_t _M0L6_2atmpS1509 = _M0L4codeS88 & 255u;
    int32_t _M0L6_2atmpS1508;
    int32_t _M0L6_2atmpS1510;
    uint32_t _M0L6_2atmpS1512;
    int32_t _M0L6_2atmpS1511;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1508 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1509);
    if (
      _M0L6offsetS91 < 0
      || _M0L6offsetS91 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6offsetS91] = _M0L6_2atmpS1508;
    _M0L6_2atmpS1510 = _M0L6offsetS91 + 1;
    _M0L6_2atmpS1512 = _M0L4codeS88 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1511 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1512);
    if (
      _M0L6_2atmpS1510 < 0
      || _M0L6_2atmpS1510 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1510] = _M0L6_2atmpS1511;
    moonbit_decref(_M0L4selfS90);
    return 2;
  } else if (_M0L4codeS88 < 1114112u) {
    uint32_t _M0L2hiS92 = _M0L4codeS88 - 65536u;
    uint32_t _M0L6_2atmpS1525 = _M0L2hiS92 >> 10;
    uint32_t _M0L2loS93 = _M0L6_2atmpS1525 | 55296u;
    uint32_t _M0L6_2atmpS1524 = _M0L2hiS92 & 1023u;
    uint32_t _M0L2hiS94 = _M0L6_2atmpS1524 | 56320u;
    uint32_t _M0L6_2atmpS1514 = _M0L2loS93 & 255u;
    int32_t _M0L6_2atmpS1513;
    int32_t _M0L6_2atmpS1515;
    uint32_t _M0L6_2atmpS1517;
    int32_t _M0L6_2atmpS1516;
    int32_t _M0L6_2atmpS1518;
    uint32_t _M0L6_2atmpS1520;
    int32_t _M0L6_2atmpS1519;
    int32_t _M0L6_2atmpS1521;
    uint32_t _M0L6_2atmpS1523;
    int32_t _M0L6_2atmpS1522;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1513 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1514);
    if (
      _M0L6offsetS91 < 0
      || _M0L6offsetS91 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6offsetS91] = _M0L6_2atmpS1513;
    _M0L6_2atmpS1515 = _M0L6offsetS91 + 1;
    _M0L6_2atmpS1517 = _M0L2loS93 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1516 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1517);
    if (
      _M0L6_2atmpS1515 < 0
      || _M0L6_2atmpS1515 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1515] = _M0L6_2atmpS1516;
    _M0L6_2atmpS1518 = _M0L6offsetS91 + 2;
    _M0L6_2atmpS1520 = _M0L2hiS94 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1519 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1520);
    if (
      _M0L6_2atmpS1518 < 0
      || _M0L6_2atmpS1518 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1518] = _M0L6_2atmpS1519;
    _M0L6_2atmpS1521 = _M0L6offsetS91 + 3;
    _M0L6_2atmpS1523 = _M0L2hiS94 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1522 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1523);
    if (
      _M0L6_2atmpS1521 < 0
      || _M0L6_2atmpS1521 >= Moonbit_array_length(_M0L4selfS90)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS90[_M0L6_2atmpS1521] = _M0L6_2atmpS1522;
    moonbit_decref(_M0L4selfS90);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS90);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_80.data, (moonbit_string_t)moonbit_string_literal_81.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS87) {
  int32_t _M0L6_2atmpS1507;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1507 = *(int32_t*)&_M0L4selfS87;
  return _M0L6_2atmpS1507 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS86) {
  int32_t _M0L6_2atmpS1506;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1506 = _M0L4selfS86;
  return *(uint32_t*)&_M0L6_2atmpS1506;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS85
) {
  moonbit_bytes_t _M0L8_2afieldS3444;
  moonbit_bytes_t _M0L4dataS1505;
  moonbit_bytes_t _M0L6_2atmpS1502;
  int32_t _M0L8_2afieldS3443;
  int32_t _M0L3lenS1504;
  int64_t _M0L6_2atmpS1503;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3444 = _M0L4selfS85->$0;
  _M0L4dataS1505 = _M0L8_2afieldS3444;
  moonbit_incref(_M0L4dataS1505);
  _M0L6_2atmpS1502 = _M0L4dataS1505;
  _M0L8_2afieldS3443 = _M0L4selfS85->$1;
  moonbit_decref(_M0L4selfS85);
  _M0L3lenS1504 = _M0L8_2afieldS3443;
  _M0L6_2atmpS1503 = (int64_t)_M0L3lenS1504;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1502, 0, _M0L6_2atmpS1503);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS80,
  int32_t _M0L6offsetS84,
  int64_t _M0L6lengthS82
) {
  int32_t _M0L3lenS79;
  int32_t _M0L6lengthS81;
  int32_t _if__result_3769;
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
      int32_t _M0L6_2atmpS1501 = _M0L6offsetS84 + _M0L6lengthS81;
      _if__result_3769 = _M0L6_2atmpS1501 <= _M0L3lenS79;
    } else {
      _if__result_3769 = 0;
    }
  } else {
    _if__result_3769 = 0;
  }
  if (_if__result_3769) {
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
  struct _M0TPB13StringBuilder* _block_3770;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS77 < 1) {
    _M0L7initialS76 = 1;
  } else {
    _M0L7initialS76 = _M0L10size__hintS77;
  }
  _M0L4dataS78 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS76, 0);
  _block_3770
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3770)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3770->$0 = _M0L4dataS78;
  _block_3770->$1 = 0;
  return _block_3770;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS75) {
  int32_t _M0L6_2atmpS1500;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1500 = (int32_t)_M0L4selfS75;
  return _M0L6_2atmpS1500;
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
  int32_t _if__result_3771;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS24 == _M0L3srcS25) {
    _if__result_3771 = _M0L11dst__offsetS26 < _M0L11src__offsetS27;
  } else {
    _if__result_3771 = 0;
  }
  if (_if__result_3771) {
    int32_t _M0L1iS28 = 0;
    while (1) {
      if (_M0L1iS28 < _M0L3lenS29) {
        int32_t _M0L6_2atmpS1464 = _M0L11dst__offsetS26 + _M0L1iS28;
        int32_t _M0L6_2atmpS1466 = _M0L11src__offsetS27 + _M0L1iS28;
        int32_t _M0L6_2atmpS1465;
        int32_t _M0L6_2atmpS1467;
        if (
          _M0L6_2atmpS1466 < 0
          || _M0L6_2atmpS1466 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1465 = (int32_t)_M0L3srcS25[_M0L6_2atmpS1466];
        if (
          _M0L6_2atmpS1464 < 0
          || _M0L6_2atmpS1464 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS24[_M0L6_2atmpS1464] = _M0L6_2atmpS1465;
        _M0L6_2atmpS1467 = _M0L1iS28 + 1;
        _M0L1iS28 = _M0L6_2atmpS1467;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1472 = _M0L3lenS29 - 1;
    int32_t _M0L1iS31 = _M0L6_2atmpS1472;
    while (1) {
      if (_M0L1iS31 >= 0) {
        int32_t _M0L6_2atmpS1468 = _M0L11dst__offsetS26 + _M0L1iS31;
        int32_t _M0L6_2atmpS1470 = _M0L11src__offsetS27 + _M0L1iS31;
        int32_t _M0L6_2atmpS1469;
        int32_t _M0L6_2atmpS1471;
        if (
          _M0L6_2atmpS1470 < 0
          || _M0L6_2atmpS1470 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1469 = (int32_t)_M0L3srcS25[_M0L6_2atmpS1470];
        if (
          _M0L6_2atmpS1468 < 0
          || _M0L6_2atmpS1468 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS24[_M0L6_2atmpS1468] = _M0L6_2atmpS1469;
        _M0L6_2atmpS1471 = _M0L1iS31 - 1;
        _M0L1iS31 = _M0L6_2atmpS1471;
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
  int32_t _if__result_3774;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS33 == _M0L3srcS34) {
    _if__result_3774 = _M0L11dst__offsetS35 < _M0L11src__offsetS36;
  } else {
    _if__result_3774 = 0;
  }
  if (_if__result_3774) {
    int32_t _M0L1iS37 = 0;
    while (1) {
      if (_M0L1iS37 < _M0L3lenS38) {
        int32_t _M0L6_2atmpS1473 = _M0L11dst__offsetS35 + _M0L1iS37;
        int32_t _M0L6_2atmpS1475 = _M0L11src__offsetS36 + _M0L1iS37;
        moonbit_string_t _M0L6_2atmpS3446;
        moonbit_string_t _M0L6_2atmpS1474;
        moonbit_string_t _M0L6_2aoldS3445;
        int32_t _M0L6_2atmpS1476;
        if (
          _M0L6_2atmpS1475 < 0
          || _M0L6_2atmpS1475 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3446 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS1475];
        _M0L6_2atmpS1474 = _M0L6_2atmpS3446;
        if (
          _M0L6_2atmpS1473 < 0
          || _M0L6_2atmpS1473 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3445 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS1473];
        moonbit_incref(_M0L6_2atmpS1474);
        moonbit_decref(_M0L6_2aoldS3445);
        _M0L3dstS33[_M0L6_2atmpS1473] = _M0L6_2atmpS1474;
        _M0L6_2atmpS1476 = _M0L1iS37 + 1;
        _M0L1iS37 = _M0L6_2atmpS1476;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1481 = _M0L3lenS38 - 1;
    int32_t _M0L1iS40 = _M0L6_2atmpS1481;
    while (1) {
      if (_M0L1iS40 >= 0) {
        int32_t _M0L6_2atmpS1477 = _M0L11dst__offsetS35 + _M0L1iS40;
        int32_t _M0L6_2atmpS1479 = _M0L11src__offsetS36 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS3448;
        moonbit_string_t _M0L6_2atmpS1478;
        moonbit_string_t _M0L6_2aoldS3447;
        int32_t _M0L6_2atmpS1480;
        if (
          _M0L6_2atmpS1479 < 0
          || _M0L6_2atmpS1479 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3448 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS1479];
        _M0L6_2atmpS1478 = _M0L6_2atmpS3448;
        if (
          _M0L6_2atmpS1477 < 0
          || _M0L6_2atmpS1477 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3447 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS1477];
        moonbit_incref(_M0L6_2atmpS1478);
        moonbit_decref(_M0L6_2aoldS3447);
        _M0L3dstS33[_M0L6_2atmpS1477] = _M0L6_2atmpS1478;
        _M0L6_2atmpS1480 = _M0L1iS40 - 1;
        _M0L1iS40 = _M0L6_2atmpS1480;
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
  int32_t _if__result_3777;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS42 == _M0L3srcS43) {
    _if__result_3777 = _M0L11dst__offsetS44 < _M0L11src__offsetS45;
  } else {
    _if__result_3777 = 0;
  }
  if (_if__result_3777) {
    int32_t _M0L1iS46 = 0;
    while (1) {
      if (_M0L1iS46 < _M0L3lenS47) {
        int32_t _M0L6_2atmpS1482 = _M0L11dst__offsetS44 + _M0L1iS46;
        int32_t _M0L6_2atmpS1484 = _M0L11src__offsetS45 + _M0L1iS46;
        struct _M0TUsiE* _M0L6_2atmpS3450;
        struct _M0TUsiE* _M0L6_2atmpS1483;
        struct _M0TUsiE* _M0L6_2aoldS3449;
        int32_t _M0L6_2atmpS1485;
        if (
          _M0L6_2atmpS1484 < 0
          || _M0L6_2atmpS1484 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3450 = (struct _M0TUsiE*)_M0L3srcS43[_M0L6_2atmpS1484];
        _M0L6_2atmpS1483 = _M0L6_2atmpS3450;
        if (
          _M0L6_2atmpS1482 < 0
          || _M0L6_2atmpS1482 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3449 = (struct _M0TUsiE*)_M0L3dstS42[_M0L6_2atmpS1482];
        if (_M0L6_2atmpS1483) {
          moonbit_incref(_M0L6_2atmpS1483);
        }
        if (_M0L6_2aoldS3449) {
          moonbit_decref(_M0L6_2aoldS3449);
        }
        _M0L3dstS42[_M0L6_2atmpS1482] = _M0L6_2atmpS1483;
        _M0L6_2atmpS1485 = _M0L1iS46 + 1;
        _M0L1iS46 = _M0L6_2atmpS1485;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1490 = _M0L3lenS47 - 1;
    int32_t _M0L1iS49 = _M0L6_2atmpS1490;
    while (1) {
      if (_M0L1iS49 >= 0) {
        int32_t _M0L6_2atmpS1486 = _M0L11dst__offsetS44 + _M0L1iS49;
        int32_t _M0L6_2atmpS1488 = _M0L11src__offsetS45 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS3452;
        struct _M0TUsiE* _M0L6_2atmpS1487;
        struct _M0TUsiE* _M0L6_2aoldS3451;
        int32_t _M0L6_2atmpS1489;
        if (
          _M0L6_2atmpS1488 < 0
          || _M0L6_2atmpS1488 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3452 = (struct _M0TUsiE*)_M0L3srcS43[_M0L6_2atmpS1488];
        _M0L6_2atmpS1487 = _M0L6_2atmpS3452;
        if (
          _M0L6_2atmpS1486 < 0
          || _M0L6_2atmpS1486 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3451 = (struct _M0TUsiE*)_M0L3dstS42[_M0L6_2atmpS1486];
        if (_M0L6_2atmpS1487) {
          moonbit_incref(_M0L6_2atmpS1487);
        }
        if (_M0L6_2aoldS3451) {
          moonbit_decref(_M0L6_2aoldS3451);
        }
        _M0L3dstS42[_M0L6_2atmpS1486] = _M0L6_2atmpS1487;
        _M0L6_2atmpS1489 = _M0L1iS49 - 1;
        _M0L1iS49 = _M0L6_2atmpS1489;
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
  int32_t _if__result_3780;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS51 == _M0L3srcS52) {
    _if__result_3780 = _M0L11dst__offsetS53 < _M0L11src__offsetS54;
  } else {
    _if__result_3780 = 0;
  }
  if (_if__result_3780) {
    int32_t _M0L1iS55 = 0;
    while (1) {
      if (_M0L1iS55 < _M0L3lenS56) {
        int32_t _M0L6_2atmpS1491 = _M0L11dst__offsetS53 + _M0L1iS55;
        int32_t _M0L6_2atmpS1493 = _M0L11src__offsetS54 + _M0L1iS55;
        void* _M0L6_2atmpS3454;
        void* _M0L6_2atmpS1492;
        void* _M0L6_2aoldS3453;
        int32_t _M0L6_2atmpS1494;
        if (
          _M0L6_2atmpS1493 < 0
          || _M0L6_2atmpS1493 >= Moonbit_array_length(_M0L3srcS52)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3454 = (void*)_M0L3srcS52[_M0L6_2atmpS1493];
        _M0L6_2atmpS1492 = _M0L6_2atmpS3454;
        if (
          _M0L6_2atmpS1491 < 0
          || _M0L6_2atmpS1491 >= Moonbit_array_length(_M0L3dstS51)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3453 = (void*)_M0L3dstS51[_M0L6_2atmpS1491];
        moonbit_incref(_M0L6_2atmpS1492);
        moonbit_decref(_M0L6_2aoldS3453);
        _M0L3dstS51[_M0L6_2atmpS1491] = _M0L6_2atmpS1492;
        _M0L6_2atmpS1494 = _M0L1iS55 + 1;
        _M0L1iS55 = _M0L6_2atmpS1494;
        continue;
      } else {
        moonbit_decref(_M0L3srcS52);
        moonbit_decref(_M0L3dstS51);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1499 = _M0L3lenS56 - 1;
    int32_t _M0L1iS58 = _M0L6_2atmpS1499;
    while (1) {
      if (_M0L1iS58 >= 0) {
        int32_t _M0L6_2atmpS1495 = _M0L11dst__offsetS53 + _M0L1iS58;
        int32_t _M0L6_2atmpS1497 = _M0L11src__offsetS54 + _M0L1iS58;
        void* _M0L6_2atmpS3456;
        void* _M0L6_2atmpS1496;
        void* _M0L6_2aoldS3455;
        int32_t _M0L6_2atmpS1498;
        if (
          _M0L6_2atmpS1497 < 0
          || _M0L6_2atmpS1497 >= Moonbit_array_length(_M0L3srcS52)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3456 = (void*)_M0L3srcS52[_M0L6_2atmpS1497];
        _M0L6_2atmpS1496 = _M0L6_2atmpS3456;
        if (
          _M0L6_2atmpS1495 < 0
          || _M0L6_2atmpS1495 >= Moonbit_array_length(_M0L3dstS51)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3455 = (void*)_M0L3dstS51[_M0L6_2atmpS1495];
        moonbit_incref(_M0L6_2atmpS1496);
        moonbit_decref(_M0L6_2aoldS3455);
        _M0L3dstS51[_M0L6_2atmpS1495] = _M0L6_2atmpS1496;
        _M0L6_2atmpS1498 = _M0L1iS58 - 1;
        _M0L1iS58 = _M0L6_2atmpS1498;
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
  moonbit_string_t _M0L6_2atmpS1453;
  moonbit_string_t _M0L6_2atmpS3459;
  moonbit_string_t _M0L6_2atmpS1451;
  moonbit_string_t _M0L6_2atmpS1452;
  moonbit_string_t _M0L6_2atmpS3458;
  moonbit_string_t _M0L6_2atmpS1450;
  moonbit_string_t _M0L6_2atmpS3457;
  moonbit_string_t _M0L6_2atmpS1449;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1453 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3459
  = moonbit_add_string(_M0L6_2atmpS1453, (moonbit_string_t)moonbit_string_literal_82.data);
  moonbit_decref(_M0L6_2atmpS1453);
  _M0L6_2atmpS1451 = _M0L6_2atmpS3459;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1452
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3458 = moonbit_add_string(_M0L6_2atmpS1451, _M0L6_2atmpS1452);
  moonbit_decref(_M0L6_2atmpS1451);
  moonbit_decref(_M0L6_2atmpS1452);
  _M0L6_2atmpS1450 = _M0L6_2atmpS3458;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3457
  = moonbit_add_string(_M0L6_2atmpS1450, (moonbit_string_t)moonbit_string_literal_37.data);
  moonbit_decref(_M0L6_2atmpS1450);
  _M0L6_2atmpS1449 = _M0L6_2atmpS3457;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1449);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1458;
  moonbit_string_t _M0L6_2atmpS3462;
  moonbit_string_t _M0L6_2atmpS1456;
  moonbit_string_t _M0L6_2atmpS1457;
  moonbit_string_t _M0L6_2atmpS3461;
  moonbit_string_t _M0L6_2atmpS1455;
  moonbit_string_t _M0L6_2atmpS3460;
  moonbit_string_t _M0L6_2atmpS1454;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1458 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3462
  = moonbit_add_string(_M0L6_2atmpS1458, (moonbit_string_t)moonbit_string_literal_82.data);
  moonbit_decref(_M0L6_2atmpS1458);
  _M0L6_2atmpS1456 = _M0L6_2atmpS3462;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1457
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3461 = moonbit_add_string(_M0L6_2atmpS1456, _M0L6_2atmpS1457);
  moonbit_decref(_M0L6_2atmpS1456);
  moonbit_decref(_M0L6_2atmpS1457);
  _M0L6_2atmpS1455 = _M0L6_2atmpS3461;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3460
  = moonbit_add_string(_M0L6_2atmpS1455, (moonbit_string_t)moonbit_string_literal_37.data);
  moonbit_decref(_M0L6_2atmpS1455);
  _M0L6_2atmpS1454 = _M0L6_2atmpS3460;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1454);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1463;
  moonbit_string_t _M0L6_2atmpS3465;
  moonbit_string_t _M0L6_2atmpS1461;
  moonbit_string_t _M0L6_2atmpS1462;
  moonbit_string_t _M0L6_2atmpS3464;
  moonbit_string_t _M0L6_2atmpS1460;
  moonbit_string_t _M0L6_2atmpS3463;
  moonbit_string_t _M0L6_2atmpS1459;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1463 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3465
  = moonbit_add_string(_M0L6_2atmpS1463, (moonbit_string_t)moonbit_string_literal_82.data);
  moonbit_decref(_M0L6_2atmpS1463);
  _M0L6_2atmpS1461 = _M0L6_2atmpS3465;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1462
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3464 = moonbit_add_string(_M0L6_2atmpS1461, _M0L6_2atmpS1462);
  moonbit_decref(_M0L6_2atmpS1461);
  moonbit_decref(_M0L6_2atmpS1462);
  _M0L6_2atmpS1460 = _M0L6_2atmpS3464;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3463
  = moonbit_add_string(_M0L6_2atmpS1460, (moonbit_string_t)moonbit_string_literal_37.data);
  moonbit_decref(_M0L6_2atmpS1460);
  _M0L6_2atmpS1459 = _M0L6_2atmpS3463;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1459);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5valueS17
) {
  uint32_t _M0L3accS1448;
  uint32_t _M0L6_2atmpS1447;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1448 = _M0L4selfS16->$0;
  _M0L6_2atmpS1447 = _M0L3accS1448 + 4u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1447;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS16, _M0L5valueS17);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5inputS15
) {
  uint32_t _M0L3accS1445;
  uint32_t _M0L6_2atmpS1446;
  uint32_t _M0L6_2atmpS1444;
  uint32_t _M0L6_2atmpS1443;
  uint32_t _M0L6_2atmpS1442;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1445 = _M0L4selfS14->$0;
  _M0L6_2atmpS1446 = _M0L5inputS15 * 3266489917u;
  _M0L6_2atmpS1444 = _M0L3accS1445 + _M0L6_2atmpS1446;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1443 = _M0FPB4rotl(_M0L6_2atmpS1444, 17);
  _M0L6_2atmpS1442 = _M0L6_2atmpS1443 * 668265263u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1442;
  moonbit_decref(_M0L4selfS14);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS12, int32_t _M0L1rS13) {
  uint32_t _M0L6_2atmpS1439;
  int32_t _M0L6_2atmpS1441;
  uint32_t _M0L6_2atmpS1440;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1439 = _M0L1xS12 << (_M0L1rS13 & 31);
  _M0L6_2atmpS1441 = 32 - _M0L1rS13;
  _M0L6_2atmpS1440 = _M0L1xS12 >> (_M0L6_2atmpS1441 & 31);
  return _M0L6_2atmpS1439 | _M0L6_2atmpS1440;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S8,
  struct _M0TPB6Logger _M0L10_2ax__4934S11
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS9;
  moonbit_string_t _M0L8_2afieldS3466;
  int32_t _M0L6_2acntS3591;
  moonbit_string_t _M0L15_2a_2aarg__4935S10;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS9
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S8;
  _M0L8_2afieldS3466 = _M0L10_2aFailureS9->$0;
  _M0L6_2acntS3591 = Moonbit_object_header(_M0L10_2aFailureS9)->rc;
  if (_M0L6_2acntS3591 > 1) {
    int32_t _M0L11_2anew__cntS3592 = _M0L6_2acntS3591 - 1;
    Moonbit_object_header(_M0L10_2aFailureS9)->rc = _M0L11_2anew__cntS3592;
    moonbit_incref(_M0L8_2afieldS3466);
  } else if (_M0L6_2acntS3591 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS9);
  }
  _M0L15_2a_2aarg__4935S10 = _M0L8_2afieldS3466;
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_83.data);
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S11, _M0L15_2a_2aarg__4935S10);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_84.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS7) {
  void* _block_3783;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3783 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3783)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3783)->$0 = _M0L4selfS7;
  return _block_3783;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS6) {
  void* _block_3784;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3784 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3784)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3784)->$0 = _M0L5arrayS6;
  return _block_3784;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1395) {
  switch (Moonbit_object_tag(_M0L4_2aeS1395)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1395);
      return (moonbit_string_t)moonbit_string_literal_85.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1395);
      return (moonbit_string_t)moonbit_string_literal_86.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1395);
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS1395);
      return (moonbit_string_t)moonbit_string_literal_87.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1395);
      return (moonbit_string_t)moonbit_string_literal_88.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1420,
  int32_t _M0L8_2aparamS1419
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1418 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1420;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1418, _M0L8_2aparamS1419);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1417,
  struct _M0TPC16string10StringView _M0L8_2aparamS1416
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1415 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1417;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1415, _M0L8_2aparamS1416);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1414,
  moonbit_string_t _M0L8_2aparamS1411,
  int32_t _M0L8_2aparamS1412,
  int32_t _M0L8_2aparamS1413
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1410 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1414;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1410, _M0L8_2aparamS1411, _M0L8_2aparamS1412, _M0L8_2aparamS1413);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1409,
  moonbit_string_t _M0L8_2aparamS1408
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1407 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1409;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1407, _M0L8_2aparamS1408);
  return 0;
}

void* _M0IP48clawteam8clawteam8internal3lru5CachePB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsiE(
  void* _M0L11_2aobj__ptrS1406
) {
  struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE* _M0L7_2aselfS1405 =
    (struct _M0TP48clawteam8clawteam8internal3lru5CacheGsiE*)_M0L11_2aobj__ptrS1406;
  return _M0IP48clawteam8clawteam8internal3lru5CachePB6ToJson8to__jsonGsiE(_M0L7_2aselfS1405);
}

void* _M0IPC16option6OptionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGiE(
  void* _M0L11_2aobj__ptrS1403
) {
  struct _M0Y5Int64* _M0L14_2aboxed__selfS1404 =
    (struct _M0Y5Int64*)_M0L11_2aobj__ptrS1403;
  int64_t _M0L8_2afieldS3467 = _M0L14_2aboxed__selfS1404->$0;
  int64_t _M0L7_2aselfS1402;
  moonbit_decref(_M0L14_2aboxed__selfS1404);
  _M0L7_2aselfS1402 = _M0L8_2afieldS3467;
  return _M0IPC16option6OptionPB6ToJson8to__jsonGiE(_M0L7_2aselfS1402);
}

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1400
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1401 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1400;
  int32_t _M0L8_2afieldS3468 = _M0L14_2aboxed__selfS1401->$0;
  int32_t _M0L7_2aselfS1399;
  moonbit_decref(_M0L14_2aboxed__selfS1401);
  _M0L7_2aselfS1399 = _M0L8_2afieldS3468;
  return _M0IPC13int3IntPB6ToJson8to__json(_M0L7_2aselfS1399);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1438;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1437;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1436;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1321;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1435;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1434;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1433;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1428;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1322;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1432;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1431;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1430;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1429;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1320;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1427;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1426;
  _M0FPB4null = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  moonbit_incref(_M0FPB4null);
  _M0FPC17prelude4null = _M0FPB4null;
  _M0L6_2atmpS1438 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1438[0] = (moonbit_string_t)moonbit_string_literal_89.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal19lru__blackbox__test47____test__63616368655f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1437
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1437)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1437->$0
  = _M0FP48clawteam8clawteam8internal19lru__blackbox__test47____test__63616368655f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1437->$1 = _M0L6_2atmpS1438;
  _M0L8_2atupleS1436
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1436)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1436->$0 = 0;
  _M0L8_2atupleS1436->$1 = _M0L8_2atupleS1437;
  _M0L7_2abindS1321
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1321[0] = _M0L8_2atupleS1436;
  _M0L6_2atmpS1435 = _M0L7_2abindS1321;
  _M0L6_2atmpS1434
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1435
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1433
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1434);
  _M0L8_2atupleS1428
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1428)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1428->$0 = (moonbit_string_t)moonbit_string_literal_90.data;
  _M0L8_2atupleS1428->$1 = _M0L6_2atmpS1433;
  _M0L7_2abindS1322
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1432 = _M0L7_2abindS1322;
  _M0L6_2atmpS1431
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1432
  };
  #line 401 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1430
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1431);
  _M0L8_2atupleS1429
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1429)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1429->$0 = (moonbit_string_t)moonbit_string_literal_91.data;
  _M0L8_2atupleS1429->$1 = _M0L6_2atmpS1430;
  _M0L7_2abindS1320
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1320[0] = _M0L8_2atupleS1428;
  _M0L7_2abindS1320[1] = _M0L8_2atupleS1429;
  _M0L6_2atmpS1427 = _M0L7_2abindS1320;
  _M0L6_2atmpS1426
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1427
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal19lru__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1426);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1425;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1389;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1390;
  int32_t _M0L7_2abindS1391;
  int32_t _M0L2__S1392;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1425
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1389
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1389)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1389->$0 = _M0L6_2atmpS1425;
  _M0L12async__testsS1389->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1390
  = _M0FP48clawteam8clawteam8internal19lru__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1391 = _M0L7_2abindS1390->$1;
  _M0L2__S1392 = 0;
  while (1) {
    if (_M0L2__S1392 < _M0L7_2abindS1391) {
      struct _M0TUsiE** _M0L8_2afieldS3472 = _M0L7_2abindS1390->$0;
      struct _M0TUsiE** _M0L3bufS1424 = _M0L8_2afieldS3472;
      struct _M0TUsiE* _M0L6_2atmpS3471 =
        (struct _M0TUsiE*)_M0L3bufS1424[_M0L2__S1392];
      struct _M0TUsiE* _M0L3argS1393 = _M0L6_2atmpS3471;
      moonbit_string_t _M0L8_2afieldS3470 = _M0L3argS1393->$0;
      moonbit_string_t _M0L6_2atmpS1421 = _M0L8_2afieldS3470;
      int32_t _M0L8_2afieldS3469 = _M0L3argS1393->$1;
      int32_t _M0L6_2atmpS1422 = _M0L8_2afieldS3469;
      int32_t _M0L6_2atmpS1423;
      moonbit_incref(_M0L6_2atmpS1421);
      moonbit_incref(_M0L12async__testsS1389);
      #line 441 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal19lru__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1389, _M0L6_2atmpS1421, _M0L6_2atmpS1422);
      _M0L6_2atmpS1423 = _M0L2__S1392 + 1;
      _M0L2__S1392 = _M0L6_2atmpS1423;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1390);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\internal\\lru\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal19lru__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal19lru__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1389);
  return 0;
}