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

struct _M0DTPC15error5Error115clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools15write__to__file33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192;

struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0R38String_3a_3aiter_2eanon__u1816__l247__;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0KTPB6ToJsonTP48clawteam8clawteam5tools15write__to__file6Params;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0TP48clawteam8clawteam5tools15write__to__file6Params;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools15write__to__file33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPB9ArrayViewGsE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

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

struct _M0DTPC15error5Error115clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools15write__to__file33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
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

struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0TPB9ArrayViewGUsRPB4JsonEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB4JsonE** $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
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

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0R38String_3a_3aiter_2eanon__u1816__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0KTPB6ToJsonTP48clawteam8clawteam5tools15write__to__file6Params {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
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

struct _M0TP48clawteam8clawteam5tools15write__to__file6Params {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools15write__to__file33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
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

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools15write__to__file57____test__77726974655f746f5f66696c652e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1201(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN14handle__resultS1192(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testC2712l425(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testC2708l426(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools15write__to__file45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1126(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1121(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1108(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools15write__to__file34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools15write__to__file47____test__77726974655f746f5f66696c652e6d6274__0(
  
);

struct _M0TP48clawteam8clawteam5tools15write__to__file6Params* _M0FP48clawteam8clawteam5tools15write__to__file6params(
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

void* _M0IP48clawteam8clawteam5tools15write__to__file6ParamsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam5tools15write__to__file6Params*
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

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2085l591(
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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1835l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC1816l247(struct _M0TWEOc*);

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

void* _M0IP48clawteam8clawteam5tools15write__to__file6ParamsPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    72, 101, 108, 108, 111, 44, 32, 87, 111, 114, 108, 100, 33, 0
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
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    112, 97, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[104]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 103), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 119, 114, 105, 116, 101, 
    95, 116, 111, 95, 102, 105, 108, 101, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 
    114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 119, 114, 105, 116, 
    101, 95, 116, 111, 95, 102, 105, 108, 101, 58, 119, 114, 105, 116, 
    101, 95, 116, 111, 95, 102, 105, 108, 101, 46, 109, 98, 116, 58, 
    52, 54, 58, 53, 45, 52, 54, 58, 55, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 119, 114, 105, 116, 
    101, 95, 116, 111, 95, 102, 105, 108, 101, 58, 119, 114, 105, 116, 
    101, 95, 116, 111, 95, 102, 105, 108, 101, 46, 109, 98, 116, 58, 
    52, 55, 58, 49, 51, 45, 53, 49, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_21 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_27 =
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
} const moonbit_string_literal_74 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    119, 114, 105, 116, 101, 95, 116, 111, 95, 102, 105, 108, 101, 46, 
    109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_36 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_9 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[106]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 105), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 119, 114, 105, 116, 101, 
    95, 116, 111, 95, 102, 105, 108, 101, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 
    105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_66 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    115, 101, 112, 97, 114, 97, 116, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_67 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    101, 120, 97, 109, 112, 108, 101, 46, 116, 120, 116, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_19 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_50 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    99, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 119, 114, 105, 116, 
    101, 95, 116, 111, 95, 102, 105, 108, 101, 58, 119, 114, 105, 116, 
    101, 95, 116, 111, 95, 102, 105, 108, 101, 46, 109, 98, 116, 58, 
    52, 53, 58, 51, 45, 53, 50, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_64 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 116, 111, 111, 108, 115, 47, 119, 114, 105, 116, 101, 95, 116, 
    111, 95, 102, 105, 108, 101, 34, 44, 32, 34, 102, 105, 108, 101, 
    110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1201$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1201
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam5tools15write__to__file57____test__77726974655f746f5f66696c652e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools15write__to__file57____test__77726974655f746f5f66696c652e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam5tools15write__to__file53____test__77726974655f746f5f66696c652e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam5tools15write__to__file57____test__77726974655f746f5f66696c652e6d6274__0_2edyncall$closure.data;

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
} _M0FP0129clawteam_2fclawteam_2ftools_2fwrite__to__file_2fParams_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam5tools15write__to__file6ParamsPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0129clawteam_2fclawteam_2ftools_2fwrite__to__file_2fParams_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0129clawteam_2fclawteam_2ftools_2fwrite__to__file_2fParams_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB30ryu__to__string_2erecord_2f962$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB30ryu__to__string_2erecord_2f962 =
  &_M0FPB30ryu__to__string_2erecord_2f962$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam5tools15write__to__file48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools15write__to__file57____test__77726974655f746f5f66696c652e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2743
) {
  return _M0FP48clawteam8clawteam5tools15write__to__file47____test__77726974655f746f5f66696c652e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1222,
  moonbit_string_t _M0L8filenameS1197,
  int32_t _M0L5indexS1200
) {
  struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192* _closure_3107;
  struct _M0TWssbEu* _M0L14handle__resultS1192;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1201;
  void* _M0L11_2atry__errS1216;
  struct moonbit_result_0 _tmp_3109;
  int32_t _handle__error__result_3110;
  int32_t _M0L6_2atmpS2731;
  void* _M0L3errS1217;
  moonbit_string_t _M0L4nameS1219;
  struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1220;
  moonbit_string_t _M0L8_2afieldS2744;
  int32_t _M0L6_2acntS3014;
  moonbit_string_t _M0L7_2anameS1221;
  #line 524 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1197);
  _closure_3107
  = (struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192*)moonbit_malloc(sizeof(struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192));
  Moonbit_object_header(_closure_3107)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192, $1) >> 2, 1, 0);
  _closure_3107->code
  = &_M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN14handle__resultS1192;
  _closure_3107->$0 = _M0L5indexS1200;
  _closure_3107->$1 = _M0L8filenameS1197;
  _M0L14handle__resultS1192 = (struct _M0TWssbEu*)_closure_3107;
  _M0L17error__to__stringS1201
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1201$closure.data;
  moonbit_incref(_M0L12async__testsS1222);
  moonbit_incref(_M0L17error__to__stringS1201);
  moonbit_incref(_M0L8filenameS1197);
  moonbit_incref(_M0L14handle__resultS1192);
  #line 558 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _tmp_3109
  = _M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__test(_M0L12async__testsS1222, _M0L8filenameS1197, _M0L5indexS1200, _M0L14handle__resultS1192, _M0L17error__to__stringS1201);
  if (_tmp_3109.tag) {
    int32_t const _M0L5_2aokS2740 = _tmp_3109.data.ok;
    _handle__error__result_3110 = _M0L5_2aokS2740;
  } else {
    void* const _M0L6_2aerrS2741 = _tmp_3109.data.err;
    moonbit_decref(_M0L12async__testsS1222);
    moonbit_decref(_M0L17error__to__stringS1201);
    moonbit_decref(_M0L8filenameS1197);
    _M0L11_2atry__errS1216 = _M0L6_2aerrS2741;
    goto join_1215;
  }
  if (_handle__error__result_3110) {
    moonbit_decref(_M0L12async__testsS1222);
    moonbit_decref(_M0L17error__to__stringS1201);
    moonbit_decref(_M0L8filenameS1197);
    _M0L6_2atmpS2731 = 1;
  } else {
    struct moonbit_result_0 _tmp_3111;
    int32_t _handle__error__result_3112;
    moonbit_incref(_M0L12async__testsS1222);
    moonbit_incref(_M0L17error__to__stringS1201);
    moonbit_incref(_M0L8filenameS1197);
    moonbit_incref(_M0L14handle__resultS1192);
    #line 561 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    _tmp_3111
    = _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1222, _M0L8filenameS1197, _M0L5indexS1200, _M0L14handle__resultS1192, _M0L17error__to__stringS1201);
    if (_tmp_3111.tag) {
      int32_t const _M0L5_2aokS2738 = _tmp_3111.data.ok;
      _handle__error__result_3112 = _M0L5_2aokS2738;
    } else {
      void* const _M0L6_2aerrS2739 = _tmp_3111.data.err;
      moonbit_decref(_M0L12async__testsS1222);
      moonbit_decref(_M0L17error__to__stringS1201);
      moonbit_decref(_M0L8filenameS1197);
      _M0L11_2atry__errS1216 = _M0L6_2aerrS2739;
      goto join_1215;
    }
    if (_handle__error__result_3112) {
      moonbit_decref(_M0L12async__testsS1222);
      moonbit_decref(_M0L17error__to__stringS1201);
      moonbit_decref(_M0L8filenameS1197);
      _M0L6_2atmpS2731 = 1;
    } else {
      struct moonbit_result_0 _tmp_3113;
      int32_t _handle__error__result_3114;
      moonbit_incref(_M0L12async__testsS1222);
      moonbit_incref(_M0L17error__to__stringS1201);
      moonbit_incref(_M0L8filenameS1197);
      moonbit_incref(_M0L14handle__resultS1192);
      #line 564 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _tmp_3113
      = _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1222, _M0L8filenameS1197, _M0L5indexS1200, _M0L14handle__resultS1192, _M0L17error__to__stringS1201);
      if (_tmp_3113.tag) {
        int32_t const _M0L5_2aokS2736 = _tmp_3113.data.ok;
        _handle__error__result_3114 = _M0L5_2aokS2736;
      } else {
        void* const _M0L6_2aerrS2737 = _tmp_3113.data.err;
        moonbit_decref(_M0L12async__testsS1222);
        moonbit_decref(_M0L17error__to__stringS1201);
        moonbit_decref(_M0L8filenameS1197);
        _M0L11_2atry__errS1216 = _M0L6_2aerrS2737;
        goto join_1215;
      }
      if (_handle__error__result_3114) {
        moonbit_decref(_M0L12async__testsS1222);
        moonbit_decref(_M0L17error__to__stringS1201);
        moonbit_decref(_M0L8filenameS1197);
        _M0L6_2atmpS2731 = 1;
      } else {
        struct moonbit_result_0 _tmp_3115;
        int32_t _handle__error__result_3116;
        moonbit_incref(_M0L12async__testsS1222);
        moonbit_incref(_M0L17error__to__stringS1201);
        moonbit_incref(_M0L8filenameS1197);
        moonbit_incref(_M0L14handle__resultS1192);
        #line 567 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        _tmp_3115
        = _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1222, _M0L8filenameS1197, _M0L5indexS1200, _M0L14handle__resultS1192, _M0L17error__to__stringS1201);
        if (_tmp_3115.tag) {
          int32_t const _M0L5_2aokS2734 = _tmp_3115.data.ok;
          _handle__error__result_3116 = _M0L5_2aokS2734;
        } else {
          void* const _M0L6_2aerrS2735 = _tmp_3115.data.err;
          moonbit_decref(_M0L12async__testsS1222);
          moonbit_decref(_M0L17error__to__stringS1201);
          moonbit_decref(_M0L8filenameS1197);
          _M0L11_2atry__errS1216 = _M0L6_2aerrS2735;
          goto join_1215;
        }
        if (_handle__error__result_3116) {
          moonbit_decref(_M0L12async__testsS1222);
          moonbit_decref(_M0L17error__to__stringS1201);
          moonbit_decref(_M0L8filenameS1197);
          _M0L6_2atmpS2731 = 1;
        } else {
          struct moonbit_result_0 _tmp_3117;
          moonbit_incref(_M0L14handle__resultS1192);
          #line 570 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
          _tmp_3117
          = _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1222, _M0L8filenameS1197, _M0L5indexS1200, _M0L14handle__resultS1192, _M0L17error__to__stringS1201);
          if (_tmp_3117.tag) {
            int32_t const _M0L5_2aokS2732 = _tmp_3117.data.ok;
            _M0L6_2atmpS2731 = _M0L5_2aokS2732;
          } else {
            void* const _M0L6_2aerrS2733 = _tmp_3117.data.err;
            _M0L11_2atry__errS1216 = _M0L6_2aerrS2733;
            goto join_1215;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2731) {
    void* _M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2742 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2742)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2742)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1216
    = _M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2742;
    goto join_1215;
  } else {
    moonbit_decref(_M0L14handle__resultS1192);
  }
  goto joinlet_3108;
  join_1215:;
  _M0L3errS1217 = _M0L11_2atry__errS1216;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1220
  = (struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1217;
  _M0L8_2afieldS2744 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1220->$0;
  _M0L6_2acntS3014
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1220)->rc;
  if (_M0L6_2acntS3014 > 1) {
    int32_t _M0L11_2anew__cntS3015 = _M0L6_2acntS3014 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1220)->rc
    = _M0L11_2anew__cntS3015;
    moonbit_incref(_M0L8_2afieldS2744);
  } else if (_M0L6_2acntS3014 == 1) {
    #line 577 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1220);
  }
  _M0L7_2anameS1221 = _M0L8_2afieldS2744;
  _M0L4nameS1219 = _M0L7_2anameS1221;
  goto join_1218;
  goto joinlet_3118;
  join_1218:;
  #line 578 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN14handle__resultS1192(_M0L14handle__resultS1192, _M0L4nameS1219, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3118:;
  joinlet_3108:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1201(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2730,
  void* _M0L3errS1202
) {
  void* _M0L1eS1204;
  moonbit_string_t _M0L1eS1206;
  #line 547 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS2730);
  switch (Moonbit_object_tag(_M0L3errS1202)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1207 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1202;
      moonbit_string_t _M0L8_2afieldS2745 = _M0L10_2aFailureS1207->$0;
      int32_t _M0L6_2acntS3016 =
        Moonbit_object_header(_M0L10_2aFailureS1207)->rc;
      moonbit_string_t _M0L4_2aeS1208;
      if (_M0L6_2acntS3016 > 1) {
        int32_t _M0L11_2anew__cntS3017 = _M0L6_2acntS3016 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1207)->rc
        = _M0L11_2anew__cntS3017;
        moonbit_incref(_M0L8_2afieldS2745);
      } else if (_M0L6_2acntS3016 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1207);
      }
      _M0L4_2aeS1208 = _M0L8_2afieldS2745;
      _M0L1eS1206 = _M0L4_2aeS1208;
      goto join_1205;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1209 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1202;
      moonbit_string_t _M0L8_2afieldS2746 = _M0L15_2aInspectErrorS1209->$0;
      int32_t _M0L6_2acntS3018 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1209)->rc;
      moonbit_string_t _M0L4_2aeS1210;
      if (_M0L6_2acntS3018 > 1) {
        int32_t _M0L11_2anew__cntS3019 = _M0L6_2acntS3018 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1209)->rc
        = _M0L11_2anew__cntS3019;
        moonbit_incref(_M0L8_2afieldS2746);
      } else if (_M0L6_2acntS3018 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1209);
      }
      _M0L4_2aeS1210 = _M0L8_2afieldS2746;
      _M0L1eS1206 = _M0L4_2aeS1210;
      goto join_1205;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1211 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1202;
      moonbit_string_t _M0L8_2afieldS2747 = _M0L16_2aSnapshotErrorS1211->$0;
      int32_t _M0L6_2acntS3020 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1211)->rc;
      moonbit_string_t _M0L4_2aeS1212;
      if (_M0L6_2acntS3020 > 1) {
        int32_t _M0L11_2anew__cntS3021 = _M0L6_2acntS3020 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1211)->rc
        = _M0L11_2anew__cntS3021;
        moonbit_incref(_M0L8_2afieldS2747);
      } else if (_M0L6_2acntS3020 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1211);
      }
      _M0L4_2aeS1212 = _M0L8_2afieldS2747;
      _M0L1eS1206 = _M0L4_2aeS1212;
      goto join_1205;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error115clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1213 =
        (struct _M0DTPC15error5Error115clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1202;
      moonbit_string_t _M0L8_2afieldS2748 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1213->$0;
      int32_t _M0L6_2acntS3022 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1213)->rc;
      moonbit_string_t _M0L4_2aeS1214;
      if (_M0L6_2acntS3022 > 1) {
        int32_t _M0L11_2anew__cntS3023 = _M0L6_2acntS3022 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1213)->rc
        = _M0L11_2anew__cntS3023;
        moonbit_incref(_M0L8_2afieldS2748);
      } else if (_M0L6_2acntS3022 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1213);
      }
      _M0L4_2aeS1214 = _M0L8_2afieldS2748;
      _M0L1eS1206 = _M0L4_2aeS1214;
      goto join_1205;
      break;
    }
    default: {
      _M0L1eS1204 = _M0L3errS1202;
      goto join_1203;
      break;
    }
  }
  join_1205:;
  return _M0L1eS1206;
  join_1203:;
  #line 553 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1204);
}

int32_t _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__executeN14handle__resultS1192(
  struct _M0TWssbEu* _M0L6_2aenvS2716,
  moonbit_string_t _M0L8testnameS1193,
  moonbit_string_t _M0L7messageS1194,
  int32_t _M0L7skippedS1195
) {
  struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192* _M0L14_2acasted__envS2717;
  moonbit_string_t _M0L8_2afieldS2758;
  moonbit_string_t _M0L8filenameS1197;
  int32_t _M0L8_2afieldS2757;
  int32_t _M0L6_2acntS3024;
  int32_t _M0L5indexS1200;
  int32_t _if__result_3121;
  moonbit_string_t _M0L10file__nameS1196;
  moonbit_string_t _M0L10test__nameS1198;
  moonbit_string_t _M0L7messageS1199;
  moonbit_string_t _M0L6_2atmpS2729;
  moonbit_string_t _M0L6_2atmpS2756;
  moonbit_string_t _M0L6_2atmpS2728;
  moonbit_string_t _M0L6_2atmpS2755;
  moonbit_string_t _M0L6_2atmpS2726;
  moonbit_string_t _M0L6_2atmpS2727;
  moonbit_string_t _M0L6_2atmpS2754;
  moonbit_string_t _M0L6_2atmpS2725;
  moonbit_string_t _M0L6_2atmpS2753;
  moonbit_string_t _M0L6_2atmpS2723;
  moonbit_string_t _M0L6_2atmpS2724;
  moonbit_string_t _M0L6_2atmpS2752;
  moonbit_string_t _M0L6_2atmpS2722;
  moonbit_string_t _M0L6_2atmpS2751;
  moonbit_string_t _M0L6_2atmpS2720;
  moonbit_string_t _M0L6_2atmpS2721;
  moonbit_string_t _M0L6_2atmpS2750;
  moonbit_string_t _M0L6_2atmpS2719;
  moonbit_string_t _M0L6_2atmpS2749;
  moonbit_string_t _M0L6_2atmpS2718;
  #line 531 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS2717
  = (struct _M0R119_24clawteam_2fclawteam_2ftools_2fwrite__to__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1192*)_M0L6_2aenvS2716;
  _M0L8_2afieldS2758 = _M0L14_2acasted__envS2717->$1;
  _M0L8filenameS1197 = _M0L8_2afieldS2758;
  _M0L8_2afieldS2757 = _M0L14_2acasted__envS2717->$0;
  _M0L6_2acntS3024 = Moonbit_object_header(_M0L14_2acasted__envS2717)->rc;
  if (_M0L6_2acntS3024 > 1) {
    int32_t _M0L11_2anew__cntS3025 = _M0L6_2acntS3024 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2717)->rc
    = _M0L11_2anew__cntS3025;
    moonbit_incref(_M0L8filenameS1197);
  } else if (_M0L6_2acntS3024 == 1) {
    #line 531 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2717);
  }
  _M0L5indexS1200 = _M0L8_2afieldS2757;
  if (!_M0L7skippedS1195) {
    _if__result_3121 = 1;
  } else {
    _if__result_3121 = 0;
  }
  if (_if__result_3121) {
    
  }
  #line 537 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1196 = _M0MPC16string6String6escape(_M0L8filenameS1197);
  #line 538 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1198 = _M0MPC16string6String6escape(_M0L8testnameS1193);
  #line 539 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1199 = _M0MPC16string6String6escape(_M0L7messageS1194);
  #line 540 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 542 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2729
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1196);
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2756
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2729);
  moonbit_decref(_M0L6_2atmpS2729);
  _M0L6_2atmpS2728 = _M0L6_2atmpS2756;
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2755
  = moonbit_add_string(_M0L6_2atmpS2728, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2728);
  _M0L6_2atmpS2726 = _M0L6_2atmpS2755;
  #line 542 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2727
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1200);
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2754 = moonbit_add_string(_M0L6_2atmpS2726, _M0L6_2atmpS2727);
  moonbit_decref(_M0L6_2atmpS2726);
  moonbit_decref(_M0L6_2atmpS2727);
  _M0L6_2atmpS2725 = _M0L6_2atmpS2754;
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2753
  = moonbit_add_string(_M0L6_2atmpS2725, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2725);
  _M0L6_2atmpS2723 = _M0L6_2atmpS2753;
  #line 542 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2724
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1198);
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2752 = moonbit_add_string(_M0L6_2atmpS2723, _M0L6_2atmpS2724);
  moonbit_decref(_M0L6_2atmpS2723);
  moonbit_decref(_M0L6_2atmpS2724);
  _M0L6_2atmpS2722 = _M0L6_2atmpS2752;
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2751
  = moonbit_add_string(_M0L6_2atmpS2722, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2722);
  _M0L6_2atmpS2720 = _M0L6_2atmpS2751;
  #line 542 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2721
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1199);
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2750 = moonbit_add_string(_M0L6_2atmpS2720, _M0L6_2atmpS2721);
  moonbit_decref(_M0L6_2atmpS2720);
  moonbit_decref(_M0L6_2atmpS2721);
  _M0L6_2atmpS2719 = _M0L6_2atmpS2750;
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2749
  = moonbit_add_string(_M0L6_2atmpS2719, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2719);
  _M0L6_2atmpS2718 = _M0L6_2atmpS2749;
  #line 541 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2718);
  #line 544 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1191,
  moonbit_string_t _M0L8filenameS1188,
  int32_t _M0L5indexS1182,
  struct _M0TWssbEu* _M0L14handle__resultS1178,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1180
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1158;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1187;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1160;
  moonbit_string_t* _M0L5attrsS1161;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1181;
  moonbit_string_t _M0L4nameS1164;
  moonbit_string_t _M0L4nameS1162;
  int32_t _M0L6_2atmpS2715;
  struct _M0TWEOs* _M0L5_2aitS1166;
  struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__* _closure_3130;
  struct _M0TWEOc* _M0L6_2atmpS2706;
  struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__* _closure_3131;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2707;
  struct moonbit_result_0 _result_3132;
  #line 405 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1191);
  moonbit_incref(_M0FP48clawteam8clawteam5tools15write__to__file48moonbit__test__driver__internal__no__args__tests);
  #line 412 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1187
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam5tools15write__to__file48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1188);
  if (_M0L7_2abindS1187 == 0) {
    struct moonbit_result_0 _result_3123;
    if (_M0L7_2abindS1187) {
      moonbit_decref(_M0L7_2abindS1187);
    }
    moonbit_decref(_M0L17error__to__stringS1180);
    moonbit_decref(_M0L14handle__resultS1178);
    _result_3123.tag = 1;
    _result_3123.data.ok = 0;
    return _result_3123;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1189 =
      _M0L7_2abindS1187;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1190 =
      _M0L7_2aSomeS1189;
    _M0L10index__mapS1158 = _M0L13_2aindex__mapS1190;
    goto join_1157;
  }
  join_1157:;
  #line 414 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1181
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1158, _M0L5indexS1182);
  if (_M0L7_2abindS1181 == 0) {
    struct moonbit_result_0 _result_3125;
    if (_M0L7_2abindS1181) {
      moonbit_decref(_M0L7_2abindS1181);
    }
    moonbit_decref(_M0L17error__to__stringS1180);
    moonbit_decref(_M0L14handle__resultS1178);
    _result_3125.tag = 1;
    _result_3125.data.ok = 0;
    return _result_3125;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1183 =
      _M0L7_2abindS1181;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1184 = _M0L7_2aSomeS1183;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS2762 = _M0L4_2axS1184->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1185 = _M0L8_2afieldS2762;
    moonbit_string_t* _M0L8_2afieldS2761 = _M0L4_2axS1184->$1;
    int32_t _M0L6_2acntS3026 = Moonbit_object_header(_M0L4_2axS1184)->rc;
    moonbit_string_t* _M0L8_2aattrsS1186;
    if (_M0L6_2acntS3026 > 1) {
      int32_t _M0L11_2anew__cntS3027 = _M0L6_2acntS3026 - 1;
      Moonbit_object_header(_M0L4_2axS1184)->rc = _M0L11_2anew__cntS3027;
      moonbit_incref(_M0L8_2afieldS2761);
      moonbit_incref(_M0L4_2afS1185);
    } else if (_M0L6_2acntS3026 == 1) {
      #line 412 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1184);
    }
    _M0L8_2aattrsS1186 = _M0L8_2afieldS2761;
    _M0L1fS1160 = _M0L4_2afS1185;
    _M0L5attrsS1161 = _M0L8_2aattrsS1186;
    goto join_1159;
  }
  join_1159:;
  _M0L6_2atmpS2715 = Moonbit_array_length(_M0L5attrsS1161);
  if (_M0L6_2atmpS2715 >= 1) {
    moonbit_string_t _M0L6_2atmpS2760 = (moonbit_string_t)_M0L5attrsS1161[0];
    moonbit_string_t _M0L7_2anameS1165 = _M0L6_2atmpS2760;
    moonbit_incref(_M0L7_2anameS1165);
    _M0L4nameS1164 = _M0L7_2anameS1165;
    goto join_1163;
  } else {
    _M0L4nameS1162 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3126;
  join_1163:;
  _M0L4nameS1162 = _M0L4nameS1164;
  joinlet_3126:;
  #line 415 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1166 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1161);
  while (1) {
    moonbit_string_t _M0L4attrS1168;
    moonbit_string_t _M0L7_2abindS1175;
    int32_t _M0L6_2atmpS2699;
    int64_t _M0L6_2atmpS2698;
    moonbit_incref(_M0L5_2aitS1166);
    #line 417 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1175 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1166);
    if (_M0L7_2abindS1175 == 0) {
      if (_M0L7_2abindS1175) {
        moonbit_decref(_M0L7_2abindS1175);
      }
      moonbit_decref(_M0L5_2aitS1166);
    } else {
      moonbit_string_t _M0L7_2aSomeS1176 = _M0L7_2abindS1175;
      moonbit_string_t _M0L7_2aattrS1177 = _M0L7_2aSomeS1176;
      _M0L4attrS1168 = _M0L7_2aattrS1177;
      goto join_1167;
    }
    goto joinlet_3128;
    join_1167:;
    _M0L6_2atmpS2699 = Moonbit_array_length(_M0L4attrS1168);
    _M0L6_2atmpS2698 = (int64_t)_M0L6_2atmpS2699;
    moonbit_incref(_M0L4attrS1168);
    #line 418 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1168, 5, 0, _M0L6_2atmpS2698)
    ) {
      int32_t _M0L6_2atmpS2705 = _M0L4attrS1168[0];
      int32_t _M0L4_2axS1169 = _M0L6_2atmpS2705;
      if (_M0L4_2axS1169 == 112) {
        int32_t _M0L6_2atmpS2704 = _M0L4attrS1168[1];
        int32_t _M0L4_2axS1170 = _M0L6_2atmpS2704;
        if (_M0L4_2axS1170 == 97) {
          int32_t _M0L6_2atmpS2703 = _M0L4attrS1168[2];
          int32_t _M0L4_2axS1171 = _M0L6_2atmpS2703;
          if (_M0L4_2axS1171 == 110) {
            int32_t _M0L6_2atmpS2702 = _M0L4attrS1168[3];
            int32_t _M0L4_2axS1172 = _M0L6_2atmpS2702;
            if (_M0L4_2axS1172 == 105) {
              int32_t _M0L6_2atmpS2759 = _M0L4attrS1168[4];
              int32_t _M0L6_2atmpS2701;
              int32_t _M0L4_2axS1173;
              moonbit_decref(_M0L4attrS1168);
              _M0L6_2atmpS2701 = _M0L6_2atmpS2759;
              _M0L4_2axS1173 = _M0L6_2atmpS2701;
              if (_M0L4_2axS1173 == 99) {
                void* _M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2700;
                struct moonbit_result_0 _result_3129;
                moonbit_decref(_M0L17error__to__stringS1180);
                moonbit_decref(_M0L14handle__resultS1178);
                moonbit_decref(_M0L5_2aitS1166);
                moonbit_decref(_M0L1fS1160);
                _M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2700
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2700)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2700)->$0
                = _M0L4nameS1162;
                _result_3129.tag = 0;
                _result_3129.data.err
                = _M0L117clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2700;
                return _result_3129;
              }
            } else {
              moonbit_decref(_M0L4attrS1168);
            }
          } else {
            moonbit_decref(_M0L4attrS1168);
          }
        } else {
          moonbit_decref(_M0L4attrS1168);
        }
      } else {
        moonbit_decref(_M0L4attrS1168);
      }
    } else {
      moonbit_decref(_M0L4attrS1168);
    }
    continue;
    joinlet_3128:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1178);
  moonbit_incref(_M0L4nameS1162);
  _closure_3130
  = (struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__*)moonbit_malloc(sizeof(struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__));
  Moonbit_object_header(_closure_3130)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__, $0) >> 2, 2, 0);
  _closure_3130->code
  = &_M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testC2712l425;
  _closure_3130->$0 = _M0L14handle__resultS1178;
  _closure_3130->$1 = _M0L4nameS1162;
  _M0L6_2atmpS2706 = (struct _M0TWEOc*)_closure_3130;
  _closure_3131
  = (struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__*)moonbit_malloc(sizeof(struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__));
  Moonbit_object_header(_closure_3131)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__, $0) >> 2, 3, 0);
  _closure_3131->code
  = &_M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testC2708l426;
  _closure_3131->$0 = _M0L17error__to__stringS1180;
  _closure_3131->$1 = _M0L14handle__resultS1178;
  _closure_3131->$2 = _M0L4nameS1162;
  _M0L6_2atmpS2707 = (struct _M0TWRPC15error5ErrorEu*)_closure_3131;
  #line 423 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools15write__to__file45moonbit__test__driver__internal__catch__error(_M0L1fS1160, _M0L6_2atmpS2706, _M0L6_2atmpS2707);
  _result_3132.tag = 1;
  _result_3132.data.ok = 1;
  return _result_3132;
}

int32_t _M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testC2712l425(
  struct _M0TWEOc* _M0L6_2aenvS2713
) {
  struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__* _M0L14_2acasted__envS2714;
  moonbit_string_t _M0L8_2afieldS2764;
  moonbit_string_t _M0L4nameS1162;
  struct _M0TWssbEu* _M0L8_2afieldS2763;
  int32_t _M0L6_2acntS3028;
  struct _M0TWssbEu* _M0L14handle__resultS1178;
  #line 425 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS2714
  = (struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2712__l425__*)_M0L6_2aenvS2713;
  _M0L8_2afieldS2764 = _M0L14_2acasted__envS2714->$1;
  _M0L4nameS1162 = _M0L8_2afieldS2764;
  _M0L8_2afieldS2763 = _M0L14_2acasted__envS2714->$0;
  _M0L6_2acntS3028 = Moonbit_object_header(_M0L14_2acasted__envS2714)->rc;
  if (_M0L6_2acntS3028 > 1) {
    int32_t _M0L11_2anew__cntS3029 = _M0L6_2acntS3028 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2714)->rc
    = _M0L11_2anew__cntS3029;
    moonbit_incref(_M0L4nameS1162);
    moonbit_incref(_M0L8_2afieldS2763);
  } else if (_M0L6_2acntS3028 == 1) {
    #line 425 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2714);
  }
  _M0L14handle__resultS1178 = _M0L8_2afieldS2763;
  #line 425 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1178->code(_M0L14handle__resultS1178, _M0L4nameS1162, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam5tools15write__to__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testC2708l426(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2709,
  void* _M0L3errS1179
) {
  struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__* _M0L14_2acasted__envS2710;
  moonbit_string_t _M0L8_2afieldS2767;
  moonbit_string_t _M0L4nameS1162;
  struct _M0TWssbEu* _M0L8_2afieldS2766;
  struct _M0TWssbEu* _M0L14handle__resultS1178;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2765;
  int32_t _M0L6_2acntS3030;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1180;
  moonbit_string_t _M0L6_2atmpS2711;
  #line 426 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS2710
  = (struct _M0R207_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fwrite__to__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2708__l426__*)_M0L6_2aenvS2709;
  _M0L8_2afieldS2767 = _M0L14_2acasted__envS2710->$2;
  _M0L4nameS1162 = _M0L8_2afieldS2767;
  _M0L8_2afieldS2766 = _M0L14_2acasted__envS2710->$1;
  _M0L14handle__resultS1178 = _M0L8_2afieldS2766;
  _M0L8_2afieldS2765 = _M0L14_2acasted__envS2710->$0;
  _M0L6_2acntS3030 = Moonbit_object_header(_M0L14_2acasted__envS2710)->rc;
  if (_M0L6_2acntS3030 > 1) {
    int32_t _M0L11_2anew__cntS3031 = _M0L6_2acntS3030 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2710)->rc
    = _M0L11_2anew__cntS3031;
    moonbit_incref(_M0L4nameS1162);
    moonbit_incref(_M0L14handle__resultS1178);
    moonbit_incref(_M0L8_2afieldS2765);
  } else if (_M0L6_2acntS3030 == 1) {
    #line 426 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2710);
  }
  _M0L17error__to__stringS1180 = _M0L8_2afieldS2765;
  #line 426 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2711
  = _M0L17error__to__stringS1180->code(_M0L17error__to__stringS1180, _M0L3errS1179);
  #line 426 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1178->code(_M0L14handle__resultS1178, _M0L4nameS1162, _M0L6_2atmpS2711, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam5tools15write__to__file45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1153,
  struct _M0TWEOc* _M0L6on__okS1154,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1151
) {
  void* _M0L11_2atry__errS1149;
  struct moonbit_result_0 _tmp_3134;
  void* _M0L3errS1150;
  #line 375 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _tmp_3134 = _M0L1fS1153->code(_M0L1fS1153);
  if (_tmp_3134.tag) {
    int32_t const _M0L5_2aokS2696 = _tmp_3134.data.ok;
    moonbit_decref(_M0L7on__errS1151);
  } else {
    void* const _M0L6_2aerrS2697 = _tmp_3134.data.err;
    moonbit_decref(_M0L6on__okS1154);
    _M0L11_2atry__errS1149 = _M0L6_2aerrS2697;
    goto join_1148;
  }
  #line 382 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1154->code(_M0L6on__okS1154);
  goto joinlet_3133;
  join_1148:;
  _M0L3errS1150 = _M0L11_2atry__errS1149;
  #line 383 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1151->code(_M0L7on__errS1151, _M0L3errS1150);
  joinlet_3133:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1108;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1121;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1126;
  struct _M0TUsiE** _M0L6_2atmpS2695;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1133;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1134;
  moonbit_string_t _M0L6_2atmpS2694;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1135;
  int32_t _M0L7_2abindS1136;
  int32_t _M0L2__S1137;
  #line 193 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1108 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1121
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1126 = 0;
  _M0L6_2atmpS2695 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1133
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1133)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1133->$0 = _M0L6_2atmpS2695;
  _M0L16file__and__indexS1133->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1134
  = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1121(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1121);
  #line 284 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2694 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1134, 1);
  #line 283 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1135
  = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1126(_M0L51moonbit__test__driver__internal__split__mbt__stringS1126, _M0L6_2atmpS2694, 47);
  _M0L7_2abindS1136 = _M0L10test__argsS1135->$1;
  _M0L2__S1137 = 0;
  while (1) {
    if (_M0L2__S1137 < _M0L7_2abindS1136) {
      moonbit_string_t* _M0L8_2afieldS2769 = _M0L10test__argsS1135->$0;
      moonbit_string_t* _M0L3bufS2693 = _M0L8_2afieldS2769;
      moonbit_string_t _M0L6_2atmpS2768 =
        (moonbit_string_t)_M0L3bufS2693[_M0L2__S1137];
      moonbit_string_t _M0L3argS1138 = _M0L6_2atmpS2768;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1139;
      moonbit_string_t _M0L4fileS1140;
      moonbit_string_t _M0L5rangeS1141;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1142;
      moonbit_string_t _M0L6_2atmpS2691;
      int32_t _M0L5startS1143;
      moonbit_string_t _M0L6_2atmpS2690;
      int32_t _M0L3endS1144;
      int32_t _M0L1iS1145;
      int32_t _M0L6_2atmpS2692;
      moonbit_incref(_M0L3argS1138);
      #line 288 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1139
      = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1126(_M0L51moonbit__test__driver__internal__split__mbt__stringS1126, _M0L3argS1138, 58);
      moonbit_incref(_M0L16file__and__rangeS1139);
      #line 289 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1140
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1139, 0);
      #line 290 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1141
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1139, 1);
      #line 291 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1142
      = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1126(_M0L51moonbit__test__driver__internal__split__mbt__stringS1126, _M0L5rangeS1141, 45);
      moonbit_incref(_M0L15start__and__endS1142);
      #line 294 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS2691
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1142, 0);
      #line 294 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1143
      = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1108(_M0L45moonbit__test__driver__internal__parse__int__S1108, _M0L6_2atmpS2691);
      #line 295 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS2690
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1142, 1);
      #line 295 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1144
      = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1108(_M0L45moonbit__test__driver__internal__parse__int__S1108, _M0L6_2atmpS2690);
      _M0L1iS1145 = _M0L5startS1143;
      while (1) {
        if (_M0L1iS1145 < _M0L3endS1144) {
          struct _M0TUsiE* _M0L8_2atupleS2688;
          int32_t _M0L6_2atmpS2689;
          moonbit_incref(_M0L4fileS1140);
          _M0L8_2atupleS2688
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2688)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2688->$0 = _M0L4fileS1140;
          _M0L8_2atupleS2688->$1 = _M0L1iS1145;
          moonbit_incref(_M0L16file__and__indexS1133);
          #line 297 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1133, _M0L8_2atupleS2688);
          _M0L6_2atmpS2689 = _M0L1iS1145 + 1;
          _M0L1iS1145 = _M0L6_2atmpS2689;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1140);
        }
        break;
      }
      _M0L6_2atmpS2692 = _M0L2__S1137 + 1;
      _M0L2__S1137 = _M0L6_2atmpS2692;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1135);
    }
    break;
  }
  return _M0L16file__and__indexS1133;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1126(
  int32_t _M0L6_2aenvS2669,
  moonbit_string_t _M0L1sS1127,
  int32_t _M0L3sepS1128
) {
  moonbit_string_t* _M0L6_2atmpS2687;
  struct _M0TPB5ArrayGsE* _M0L3resS1129;
  struct _M0TPC13ref3RefGiE* _M0L1iS1130;
  struct _M0TPC13ref3RefGiE* _M0L5startS1131;
  int32_t _M0L3valS2682;
  int32_t _M0L6_2atmpS2683;
  #line 261 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2687 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1129
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1129)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1129->$0 = _M0L6_2atmpS2687;
  _M0L3resS1129->$1 = 0;
  _M0L1iS1130
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1130)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1130->$0 = 0;
  _M0L5startS1131
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1131)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1131->$0 = 0;
  while (1) {
    int32_t _M0L3valS2670 = _M0L1iS1130->$0;
    int32_t _M0L6_2atmpS2671 = Moonbit_array_length(_M0L1sS1127);
    if (_M0L3valS2670 < _M0L6_2atmpS2671) {
      int32_t _M0L3valS2674 = _M0L1iS1130->$0;
      int32_t _M0L6_2atmpS2673;
      int32_t _M0L6_2atmpS2672;
      int32_t _M0L3valS2681;
      int32_t _M0L6_2atmpS2680;
      if (
        _M0L3valS2674 < 0
        || _M0L3valS2674 >= Moonbit_array_length(_M0L1sS1127)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2673 = _M0L1sS1127[_M0L3valS2674];
      _M0L6_2atmpS2672 = _M0L6_2atmpS2673;
      if (_M0L6_2atmpS2672 == _M0L3sepS1128) {
        int32_t _M0L3valS2676 = _M0L5startS1131->$0;
        int32_t _M0L3valS2677 = _M0L1iS1130->$0;
        moonbit_string_t _M0L6_2atmpS2675;
        int32_t _M0L3valS2679;
        int32_t _M0L6_2atmpS2678;
        moonbit_incref(_M0L1sS1127);
        #line 270 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS2675
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1127, _M0L3valS2676, _M0L3valS2677);
        moonbit_incref(_M0L3resS1129);
        #line 270 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1129, _M0L6_2atmpS2675);
        _M0L3valS2679 = _M0L1iS1130->$0;
        _M0L6_2atmpS2678 = _M0L3valS2679 + 1;
        _M0L5startS1131->$0 = _M0L6_2atmpS2678;
      }
      _M0L3valS2681 = _M0L1iS1130->$0;
      _M0L6_2atmpS2680 = _M0L3valS2681 + 1;
      _M0L1iS1130->$0 = _M0L6_2atmpS2680;
      continue;
    } else {
      moonbit_decref(_M0L1iS1130);
    }
    break;
  }
  _M0L3valS2682 = _M0L5startS1131->$0;
  _M0L6_2atmpS2683 = Moonbit_array_length(_M0L1sS1127);
  if (_M0L3valS2682 < _M0L6_2atmpS2683) {
    int32_t _M0L8_2afieldS2770 = _M0L5startS1131->$0;
    int32_t _M0L3valS2685;
    int32_t _M0L6_2atmpS2686;
    moonbit_string_t _M0L6_2atmpS2684;
    moonbit_decref(_M0L5startS1131);
    _M0L3valS2685 = _M0L8_2afieldS2770;
    _M0L6_2atmpS2686 = Moonbit_array_length(_M0L1sS1127);
    #line 276 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS2684
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1127, _M0L3valS2685, _M0L6_2atmpS2686);
    moonbit_incref(_M0L3resS1129);
    #line 276 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1129, _M0L6_2atmpS2684);
  } else {
    moonbit_decref(_M0L5startS1131);
    moonbit_decref(_M0L1sS1127);
  }
  return _M0L3resS1129;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1121(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114
) {
  moonbit_bytes_t* _M0L3tmpS1122;
  int32_t _M0L6_2atmpS2668;
  struct _M0TPB5ArrayGsE* _M0L3resS1123;
  int32_t _M0L1iS1124;
  #line 250 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1122
  = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2668 = Moonbit_array_length(_M0L3tmpS1122);
  #line 254 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1123 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2668);
  _M0L1iS1124 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2664 = Moonbit_array_length(_M0L3tmpS1122);
    if (_M0L1iS1124 < _M0L6_2atmpS2664) {
      moonbit_bytes_t _M0L6_2atmpS2771;
      moonbit_bytes_t _M0L6_2atmpS2666;
      moonbit_string_t _M0L6_2atmpS2665;
      int32_t _M0L6_2atmpS2667;
      if (
        _M0L1iS1124 < 0 || _M0L1iS1124 >= Moonbit_array_length(_M0L3tmpS1122)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2771 = (moonbit_bytes_t)_M0L3tmpS1122[_M0L1iS1124];
      _M0L6_2atmpS2666 = _M0L6_2atmpS2771;
      moonbit_incref(_M0L6_2atmpS2666);
      #line 256 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS2665
      = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114, _M0L6_2atmpS2666);
      moonbit_incref(_M0L3resS1123);
      #line 256 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1123, _M0L6_2atmpS2665);
      _M0L6_2atmpS2667 = _M0L1iS1124 + 1;
      _M0L1iS1124 = _M0L6_2atmpS2667;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1122);
    }
    break;
  }
  return _M0L3resS1123;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1114(
  int32_t _M0L6_2aenvS2578,
  moonbit_bytes_t _M0L5bytesS1115
) {
  struct _M0TPB13StringBuilder* _M0L3resS1116;
  int32_t _M0L3lenS1117;
  struct _M0TPC13ref3RefGiE* _M0L1iS1118;
  #line 206 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1116 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1117 = Moonbit_array_length(_M0L5bytesS1115);
  _M0L1iS1118
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1118)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1118->$0 = 0;
  while (1) {
    int32_t _M0L3valS2579 = _M0L1iS1118->$0;
    if (_M0L3valS2579 < _M0L3lenS1117) {
      int32_t _M0L3valS2663 = _M0L1iS1118->$0;
      int32_t _M0L6_2atmpS2662;
      int32_t _M0L6_2atmpS2661;
      struct _M0TPC13ref3RefGiE* _M0L1cS1119;
      int32_t _M0L3valS2580;
      if (
        _M0L3valS2663 < 0
        || _M0L3valS2663 >= Moonbit_array_length(_M0L5bytesS1115)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2662 = _M0L5bytesS1115[_M0L3valS2663];
      _M0L6_2atmpS2661 = (int32_t)_M0L6_2atmpS2662;
      _M0L1cS1119
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1119)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1119->$0 = _M0L6_2atmpS2661;
      _M0L3valS2580 = _M0L1cS1119->$0;
      if (_M0L3valS2580 < 128) {
        int32_t _M0L8_2afieldS2772 = _M0L1cS1119->$0;
        int32_t _M0L3valS2582;
        int32_t _M0L6_2atmpS2581;
        int32_t _M0L3valS2584;
        int32_t _M0L6_2atmpS2583;
        moonbit_decref(_M0L1cS1119);
        _M0L3valS2582 = _M0L8_2afieldS2772;
        _M0L6_2atmpS2581 = _M0L3valS2582;
        moonbit_incref(_M0L3resS1116);
        #line 215 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1116, _M0L6_2atmpS2581);
        _M0L3valS2584 = _M0L1iS1118->$0;
        _M0L6_2atmpS2583 = _M0L3valS2584 + 1;
        _M0L1iS1118->$0 = _M0L6_2atmpS2583;
      } else {
        int32_t _M0L3valS2585 = _M0L1cS1119->$0;
        if (_M0L3valS2585 < 224) {
          int32_t _M0L3valS2587 = _M0L1iS1118->$0;
          int32_t _M0L6_2atmpS2586 = _M0L3valS2587 + 1;
          int32_t _M0L3valS2596;
          int32_t _M0L6_2atmpS2595;
          int32_t _M0L6_2atmpS2589;
          int32_t _M0L3valS2594;
          int32_t _M0L6_2atmpS2593;
          int32_t _M0L6_2atmpS2592;
          int32_t _M0L6_2atmpS2591;
          int32_t _M0L6_2atmpS2590;
          int32_t _M0L6_2atmpS2588;
          int32_t _M0L8_2afieldS2773;
          int32_t _M0L3valS2598;
          int32_t _M0L6_2atmpS2597;
          int32_t _M0L3valS2600;
          int32_t _M0L6_2atmpS2599;
          if (_M0L6_2atmpS2586 >= _M0L3lenS1117) {
            moonbit_decref(_M0L1cS1119);
            moonbit_decref(_M0L1iS1118);
            moonbit_decref(_M0L5bytesS1115);
            break;
          }
          _M0L3valS2596 = _M0L1cS1119->$0;
          _M0L6_2atmpS2595 = _M0L3valS2596 & 31;
          _M0L6_2atmpS2589 = _M0L6_2atmpS2595 << 6;
          _M0L3valS2594 = _M0L1iS1118->$0;
          _M0L6_2atmpS2593 = _M0L3valS2594 + 1;
          if (
            _M0L6_2atmpS2593 < 0
            || _M0L6_2atmpS2593 >= Moonbit_array_length(_M0L5bytesS1115)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2592 = _M0L5bytesS1115[_M0L6_2atmpS2593];
          _M0L6_2atmpS2591 = (int32_t)_M0L6_2atmpS2592;
          _M0L6_2atmpS2590 = _M0L6_2atmpS2591 & 63;
          _M0L6_2atmpS2588 = _M0L6_2atmpS2589 | _M0L6_2atmpS2590;
          _M0L1cS1119->$0 = _M0L6_2atmpS2588;
          _M0L8_2afieldS2773 = _M0L1cS1119->$0;
          moonbit_decref(_M0L1cS1119);
          _M0L3valS2598 = _M0L8_2afieldS2773;
          _M0L6_2atmpS2597 = _M0L3valS2598;
          moonbit_incref(_M0L3resS1116);
          #line 222 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1116, _M0L6_2atmpS2597);
          _M0L3valS2600 = _M0L1iS1118->$0;
          _M0L6_2atmpS2599 = _M0L3valS2600 + 2;
          _M0L1iS1118->$0 = _M0L6_2atmpS2599;
        } else {
          int32_t _M0L3valS2601 = _M0L1cS1119->$0;
          if (_M0L3valS2601 < 240) {
            int32_t _M0L3valS2603 = _M0L1iS1118->$0;
            int32_t _M0L6_2atmpS2602 = _M0L3valS2603 + 2;
            int32_t _M0L3valS2619;
            int32_t _M0L6_2atmpS2618;
            int32_t _M0L6_2atmpS2611;
            int32_t _M0L3valS2617;
            int32_t _M0L6_2atmpS2616;
            int32_t _M0L6_2atmpS2615;
            int32_t _M0L6_2atmpS2614;
            int32_t _M0L6_2atmpS2613;
            int32_t _M0L6_2atmpS2612;
            int32_t _M0L6_2atmpS2605;
            int32_t _M0L3valS2610;
            int32_t _M0L6_2atmpS2609;
            int32_t _M0L6_2atmpS2608;
            int32_t _M0L6_2atmpS2607;
            int32_t _M0L6_2atmpS2606;
            int32_t _M0L6_2atmpS2604;
            int32_t _M0L8_2afieldS2774;
            int32_t _M0L3valS2621;
            int32_t _M0L6_2atmpS2620;
            int32_t _M0L3valS2623;
            int32_t _M0L6_2atmpS2622;
            if (_M0L6_2atmpS2602 >= _M0L3lenS1117) {
              moonbit_decref(_M0L1cS1119);
              moonbit_decref(_M0L1iS1118);
              moonbit_decref(_M0L5bytesS1115);
              break;
            }
            _M0L3valS2619 = _M0L1cS1119->$0;
            _M0L6_2atmpS2618 = _M0L3valS2619 & 15;
            _M0L6_2atmpS2611 = _M0L6_2atmpS2618 << 12;
            _M0L3valS2617 = _M0L1iS1118->$0;
            _M0L6_2atmpS2616 = _M0L3valS2617 + 1;
            if (
              _M0L6_2atmpS2616 < 0
              || _M0L6_2atmpS2616 >= Moonbit_array_length(_M0L5bytesS1115)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2615 = _M0L5bytesS1115[_M0L6_2atmpS2616];
            _M0L6_2atmpS2614 = (int32_t)_M0L6_2atmpS2615;
            _M0L6_2atmpS2613 = _M0L6_2atmpS2614 & 63;
            _M0L6_2atmpS2612 = _M0L6_2atmpS2613 << 6;
            _M0L6_2atmpS2605 = _M0L6_2atmpS2611 | _M0L6_2atmpS2612;
            _M0L3valS2610 = _M0L1iS1118->$0;
            _M0L6_2atmpS2609 = _M0L3valS2610 + 2;
            if (
              _M0L6_2atmpS2609 < 0
              || _M0L6_2atmpS2609 >= Moonbit_array_length(_M0L5bytesS1115)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2608 = _M0L5bytesS1115[_M0L6_2atmpS2609];
            _M0L6_2atmpS2607 = (int32_t)_M0L6_2atmpS2608;
            _M0L6_2atmpS2606 = _M0L6_2atmpS2607 & 63;
            _M0L6_2atmpS2604 = _M0L6_2atmpS2605 | _M0L6_2atmpS2606;
            _M0L1cS1119->$0 = _M0L6_2atmpS2604;
            _M0L8_2afieldS2774 = _M0L1cS1119->$0;
            moonbit_decref(_M0L1cS1119);
            _M0L3valS2621 = _M0L8_2afieldS2774;
            _M0L6_2atmpS2620 = _M0L3valS2621;
            moonbit_incref(_M0L3resS1116);
            #line 231 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1116, _M0L6_2atmpS2620);
            _M0L3valS2623 = _M0L1iS1118->$0;
            _M0L6_2atmpS2622 = _M0L3valS2623 + 3;
            _M0L1iS1118->$0 = _M0L6_2atmpS2622;
          } else {
            int32_t _M0L3valS2625 = _M0L1iS1118->$0;
            int32_t _M0L6_2atmpS2624 = _M0L3valS2625 + 3;
            int32_t _M0L3valS2648;
            int32_t _M0L6_2atmpS2647;
            int32_t _M0L6_2atmpS2640;
            int32_t _M0L3valS2646;
            int32_t _M0L6_2atmpS2645;
            int32_t _M0L6_2atmpS2644;
            int32_t _M0L6_2atmpS2643;
            int32_t _M0L6_2atmpS2642;
            int32_t _M0L6_2atmpS2641;
            int32_t _M0L6_2atmpS2633;
            int32_t _M0L3valS2639;
            int32_t _M0L6_2atmpS2638;
            int32_t _M0L6_2atmpS2637;
            int32_t _M0L6_2atmpS2636;
            int32_t _M0L6_2atmpS2635;
            int32_t _M0L6_2atmpS2634;
            int32_t _M0L6_2atmpS2627;
            int32_t _M0L3valS2632;
            int32_t _M0L6_2atmpS2631;
            int32_t _M0L6_2atmpS2630;
            int32_t _M0L6_2atmpS2629;
            int32_t _M0L6_2atmpS2628;
            int32_t _M0L6_2atmpS2626;
            int32_t _M0L3valS2650;
            int32_t _M0L6_2atmpS2649;
            int32_t _M0L3valS2654;
            int32_t _M0L6_2atmpS2653;
            int32_t _M0L6_2atmpS2652;
            int32_t _M0L6_2atmpS2651;
            int32_t _M0L8_2afieldS2775;
            int32_t _M0L3valS2658;
            int32_t _M0L6_2atmpS2657;
            int32_t _M0L6_2atmpS2656;
            int32_t _M0L6_2atmpS2655;
            int32_t _M0L3valS2660;
            int32_t _M0L6_2atmpS2659;
            if (_M0L6_2atmpS2624 >= _M0L3lenS1117) {
              moonbit_decref(_M0L1cS1119);
              moonbit_decref(_M0L1iS1118);
              moonbit_decref(_M0L5bytesS1115);
              break;
            }
            _M0L3valS2648 = _M0L1cS1119->$0;
            _M0L6_2atmpS2647 = _M0L3valS2648 & 7;
            _M0L6_2atmpS2640 = _M0L6_2atmpS2647 << 18;
            _M0L3valS2646 = _M0L1iS1118->$0;
            _M0L6_2atmpS2645 = _M0L3valS2646 + 1;
            if (
              _M0L6_2atmpS2645 < 0
              || _M0L6_2atmpS2645 >= Moonbit_array_length(_M0L5bytesS1115)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2644 = _M0L5bytesS1115[_M0L6_2atmpS2645];
            _M0L6_2atmpS2643 = (int32_t)_M0L6_2atmpS2644;
            _M0L6_2atmpS2642 = _M0L6_2atmpS2643 & 63;
            _M0L6_2atmpS2641 = _M0L6_2atmpS2642 << 12;
            _M0L6_2atmpS2633 = _M0L6_2atmpS2640 | _M0L6_2atmpS2641;
            _M0L3valS2639 = _M0L1iS1118->$0;
            _M0L6_2atmpS2638 = _M0L3valS2639 + 2;
            if (
              _M0L6_2atmpS2638 < 0
              || _M0L6_2atmpS2638 >= Moonbit_array_length(_M0L5bytesS1115)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2637 = _M0L5bytesS1115[_M0L6_2atmpS2638];
            _M0L6_2atmpS2636 = (int32_t)_M0L6_2atmpS2637;
            _M0L6_2atmpS2635 = _M0L6_2atmpS2636 & 63;
            _M0L6_2atmpS2634 = _M0L6_2atmpS2635 << 6;
            _M0L6_2atmpS2627 = _M0L6_2atmpS2633 | _M0L6_2atmpS2634;
            _M0L3valS2632 = _M0L1iS1118->$0;
            _M0L6_2atmpS2631 = _M0L3valS2632 + 3;
            if (
              _M0L6_2atmpS2631 < 0
              || _M0L6_2atmpS2631 >= Moonbit_array_length(_M0L5bytesS1115)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2630 = _M0L5bytesS1115[_M0L6_2atmpS2631];
            _M0L6_2atmpS2629 = (int32_t)_M0L6_2atmpS2630;
            _M0L6_2atmpS2628 = _M0L6_2atmpS2629 & 63;
            _M0L6_2atmpS2626 = _M0L6_2atmpS2627 | _M0L6_2atmpS2628;
            _M0L1cS1119->$0 = _M0L6_2atmpS2626;
            _M0L3valS2650 = _M0L1cS1119->$0;
            _M0L6_2atmpS2649 = _M0L3valS2650 - 65536;
            _M0L1cS1119->$0 = _M0L6_2atmpS2649;
            _M0L3valS2654 = _M0L1cS1119->$0;
            _M0L6_2atmpS2653 = _M0L3valS2654 >> 10;
            _M0L6_2atmpS2652 = _M0L6_2atmpS2653 + 55296;
            _M0L6_2atmpS2651 = _M0L6_2atmpS2652;
            moonbit_incref(_M0L3resS1116);
            #line 242 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1116, _M0L6_2atmpS2651);
            _M0L8_2afieldS2775 = _M0L1cS1119->$0;
            moonbit_decref(_M0L1cS1119);
            _M0L3valS2658 = _M0L8_2afieldS2775;
            _M0L6_2atmpS2657 = _M0L3valS2658 & 1023;
            _M0L6_2atmpS2656 = _M0L6_2atmpS2657 + 56320;
            _M0L6_2atmpS2655 = _M0L6_2atmpS2656;
            moonbit_incref(_M0L3resS1116);
            #line 243 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1116, _M0L6_2atmpS2655);
            _M0L3valS2660 = _M0L1iS1118->$0;
            _M0L6_2atmpS2659 = _M0L3valS2660 + 4;
            _M0L1iS1118->$0 = _M0L6_2atmpS2659;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1118);
      moonbit_decref(_M0L5bytesS1115);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1116);
}

int32_t _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1108(
  int32_t _M0L6_2aenvS2571,
  moonbit_string_t _M0L1sS1109
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1110;
  int32_t _M0L3lenS1111;
  int32_t _M0L1iS1112;
  int32_t _M0L8_2afieldS2776;
  #line 197 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1110
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1110)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1110->$0 = 0;
  _M0L3lenS1111 = Moonbit_array_length(_M0L1sS1109);
  _M0L1iS1112 = 0;
  while (1) {
    if (_M0L1iS1112 < _M0L3lenS1111) {
      int32_t _M0L3valS2576 = _M0L3resS1110->$0;
      int32_t _M0L6_2atmpS2573 = _M0L3valS2576 * 10;
      int32_t _M0L6_2atmpS2575;
      int32_t _M0L6_2atmpS2574;
      int32_t _M0L6_2atmpS2572;
      int32_t _M0L6_2atmpS2577;
      if (
        _M0L1iS1112 < 0 || _M0L1iS1112 >= Moonbit_array_length(_M0L1sS1109)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2575 = _M0L1sS1109[_M0L1iS1112];
      _M0L6_2atmpS2574 = _M0L6_2atmpS2575 - 48;
      _M0L6_2atmpS2572 = _M0L6_2atmpS2573 + _M0L6_2atmpS2574;
      _M0L3resS1110->$0 = _M0L6_2atmpS2572;
      _M0L6_2atmpS2577 = _M0L1iS1112 + 1;
      _M0L1iS1112 = _M0L6_2atmpS2577;
      continue;
    } else {
      moonbit_decref(_M0L1sS1109);
    }
    break;
  }
  _M0L8_2afieldS2776 = _M0L3resS1110->$0;
  moonbit_decref(_M0L3resS1110);
  return _M0L8_2afieldS2776;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1088,
  moonbit_string_t _M0L12_2adiscard__S1089,
  int32_t _M0L12_2adiscard__S1090,
  struct _M0TWssbEu* _M0L12_2adiscard__S1091,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1092
) {
  struct moonbit_result_0 _result_3141;
  #line 34 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1092);
  moonbit_decref(_M0L12_2adiscard__S1091);
  moonbit_decref(_M0L12_2adiscard__S1089);
  moonbit_decref(_M0L12_2adiscard__S1088);
  _result_3141.tag = 1;
  _result_3141.data.ok = 0;
  return _result_3141;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1093,
  moonbit_string_t _M0L12_2adiscard__S1094,
  int32_t _M0L12_2adiscard__S1095,
  struct _M0TWssbEu* _M0L12_2adiscard__S1096,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1097
) {
  struct moonbit_result_0 _result_3142;
  #line 34 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1097);
  moonbit_decref(_M0L12_2adiscard__S1096);
  moonbit_decref(_M0L12_2adiscard__S1094);
  moonbit_decref(_M0L12_2adiscard__S1093);
  _result_3142.tag = 1;
  _result_3142.data.ok = 0;
  return _result_3142;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1098,
  moonbit_string_t _M0L12_2adiscard__S1099,
  int32_t _M0L12_2adiscard__S1100,
  struct _M0TWssbEu* _M0L12_2adiscard__S1101,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1102
) {
  struct moonbit_result_0 _result_3143;
  #line 34 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1102);
  moonbit_decref(_M0L12_2adiscard__S1101);
  moonbit_decref(_M0L12_2adiscard__S1099);
  moonbit_decref(_M0L12_2adiscard__S1098);
  _result_3143.tag = 1;
  _result_3143.data.ok = 0;
  return _result_3143;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools15write__to__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1103,
  moonbit_string_t _M0L12_2adiscard__S1104,
  int32_t _M0L12_2adiscard__S1105,
  struct _M0TWssbEu* _M0L12_2adiscard__S1106,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1107
) {
  struct moonbit_result_0 _result_3144;
  #line 34 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1107);
  moonbit_decref(_M0L12_2adiscard__S1106);
  moonbit_decref(_M0L12_2adiscard__S1104);
  moonbit_decref(_M0L12_2adiscard__S1103);
  _result_3144.tag = 1;
  _result_3144.data.ok = 0;
  return _result_3144;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools15write__to__file34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1087
) {
  #line 12 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1087);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools15write__to__file47____test__77726974655f746f5f66696c652e6d6274__0(
  
) {
  moonbit_string_t _M0L6_2atmpS2570;
  struct _M0TP48clawteam8clawteam5tools15write__to__file6Params* _M0L6_2atmpS2569;
  struct _M0TPB6ToJson _M0L6_2atmpS2551;
  void* _M0L6_2atmpS2568;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2563;
  void* _M0L6_2atmpS2567;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2564;
  void* _M0L6_2atmpS2566;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2565;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1086;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2562;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2561;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2560;
  void* _M0L6_2atmpS2559;
  void* _M0L6_2atmpS2552;
  moonbit_string_t _M0L6_2atmpS2555;
  moonbit_string_t _M0L6_2atmpS2556;
  moonbit_string_t _M0L6_2atmpS2557;
  moonbit_string_t _M0L6_2atmpS2558;
  moonbit_string_t* _M0L6_2atmpS2554;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2553;
  #line 44 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2570 = (moonbit_string_t)moonbit_string_literal_9.data;
  #line 46 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2569
  = _M0FP48clawteam8clawteam5tools15write__to__file6params((moonbit_string_t)moonbit_string_literal_10.data, (moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS2570);
  _M0L6_2atmpS2551
  = (struct _M0TPB6ToJson){
    _M0FP0129clawteam_2fclawteam_2ftools_2fwrite__to__file_2fParams_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS2569
  };
  #line 48 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2568
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2563
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2563)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2563->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS2563->$1 = _M0L6_2atmpS2568;
  #line 49 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2567
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L8_2atupleS2564
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2564)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2564->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2564->$1 = _M0L6_2atmpS2567;
  #line 50 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2566
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L8_2atupleS2565
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2565)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2565->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS2565->$1 = _M0L6_2atmpS2566;
  _M0L7_2abindS1086 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1086[0] = _M0L8_2atupleS2563;
  _M0L7_2abindS1086[1] = _M0L8_2atupleS2564;
  _M0L7_2abindS1086[2] = _M0L8_2atupleS2565;
  _M0L6_2atmpS2562 = _M0L7_2abindS1086;
  _M0L6_2atmpS2561
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS2562
  };
  #line 47 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2560 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2561);
  #line 47 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2559 = _M0MPC14json4Json6object(_M0L6_2atmpS2560);
  _M0L6_2atmpS2552 = _M0L6_2atmpS2559;
  _M0L6_2atmpS2555 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS2556 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS2557 = 0;
  _M0L6_2atmpS2558 = 0;
  _M0L6_2atmpS2554 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2554[0] = _M0L6_2atmpS2555;
  _M0L6_2atmpS2554[1] = _M0L6_2atmpS2556;
  _M0L6_2atmpS2554[2] = _M0L6_2atmpS2557;
  _M0L6_2atmpS2554[3] = _M0L6_2atmpS2558;
  _M0L6_2atmpS2553
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2553)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2553->$0 = _M0L6_2atmpS2554;
  _M0L6_2atmpS2553->$1 = 4;
  #line 45 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2551, _M0L6_2atmpS2552, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS2553);
}

struct _M0TP48clawteam8clawteam5tools15write__to__file6Params* _M0FP48clawteam8clawteam5tools15write__to__file6params(
  moonbit_string_t _M0L7contentS1083,
  moonbit_string_t _M0L4pathS1084,
  moonbit_string_t _M0L9separatorS1085
) {
  struct _M0TP48clawteam8clawteam5tools15write__to__file6Params* _block_3145;
  #line 30 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _block_3145
  = (struct _M0TP48clawteam8clawteam5tools15write__to__file6Params*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam5tools15write__to__file6Params));
  Moonbit_object_header(_block_3145)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam5tools15write__to__file6Params, $0) >> 2, 3, 0);
  _block_3145->$0 = _M0L7contentS1083;
  _block_3145->$1 = _M0L4pathS1084;
  _block_3145->$2 = _M0L9separatorS1085;
  return _block_3145;
}

void* _M0IP48clawteam8clawteam5tools15write__to__file6ParamsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam5tools15write__to__file6Params* _M0L9_2ax__116S1077
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1076;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2550;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2549;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1075;
  moonbit_string_t _M0L8_2afieldS2779;
  moonbit_string_t _M0L7contentS2545;
  void* _M0L6_2atmpS2544;
  moonbit_string_t _M0L8_2afieldS2778;
  moonbit_string_t _M0L4pathS2547;
  void* _M0L6_2atmpS2546;
  moonbit_string_t _M0L8_24innerS1079;
  moonbit_string_t _M0L8_2afieldS2777;
  int32_t _M0L6_2acntS3032;
  moonbit_string_t _M0L7_2abindS1080;
  void* _M0L6_2atmpS2548;
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L7_2abindS1076 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2550 = _M0L7_2abindS1076;
  _M0L6_2atmpS2549
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS2550
  };
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_24mapS1075 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2549);
  _M0L8_2afieldS2779 = _M0L9_2ax__116S1077->$0;
  _M0L7contentS2545 = _M0L8_2afieldS2779;
  moonbit_incref(_M0L7contentS2545);
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2544
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L7contentS2545);
  moonbit_incref(_M0L6_24mapS1075);
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1075, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS2544);
  _M0L8_2afieldS2778 = _M0L9_2ax__116S1077->$1;
  _M0L4pathS2547 = _M0L8_2afieldS2778;
  moonbit_incref(_M0L4pathS2547);
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2546 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4pathS2547);
  moonbit_incref(_M0L6_24mapS1075);
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1075, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2546);
  _M0L8_2afieldS2777 = _M0L9_2ax__116S1077->$2;
  _M0L6_2acntS3032 = Moonbit_object_header(_M0L9_2ax__116S1077)->rc;
  if (_M0L6_2acntS3032 > 1) {
    int32_t _M0L11_2anew__cntS3035 = _M0L6_2acntS3032 - 1;
    Moonbit_object_header(_M0L9_2ax__116S1077)->rc = _M0L11_2anew__cntS3035;
    if (_M0L8_2afieldS2777) {
      moonbit_incref(_M0L8_2afieldS2777);
    }
  } else if (_M0L6_2acntS3032 == 1) {
    moonbit_string_t _M0L8_2afieldS3034 = _M0L9_2ax__116S1077->$1;
    moonbit_string_t _M0L8_2afieldS3033;
    moonbit_decref(_M0L8_2afieldS3034);
    _M0L8_2afieldS3033 = _M0L9_2ax__116S1077->$0;
    moonbit_decref(_M0L8_2afieldS3033);
    #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
    moonbit_free(_M0L9_2ax__116S1077);
  }
  _M0L7_2abindS1080 = _M0L8_2afieldS2777;
  if (_M0L7_2abindS1080 == 0) {
    if (_M0L7_2abindS1080) {
      moonbit_decref(_M0L7_2abindS1080);
    }
  } else {
    moonbit_string_t _M0L7_2aSomeS1081 = _M0L7_2abindS1080;
    moonbit_string_t _M0L11_2a_24innerS1082 = _M0L7_2aSomeS1081;
    _M0L8_24innerS1079 = _M0L11_2a_24innerS1082;
    goto join_1078;
  }
  goto joinlet_3146;
  join_1078:;
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0L6_2atmpS2548
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L8_24innerS1079);
  moonbit_incref(_M0L6_24mapS1075);
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1075, (moonbit_string_t)moonbit_string_literal_14.data, _M0L6_2atmpS2548);
  joinlet_3146:;
  #line 23 "E:\\moonbit\\clawteam\\tools\\write_to_file\\write_to_file.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1075);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1070,
  void* _M0L7contentS1072,
  moonbit_string_t _M0L3locS1066,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1068
) {
  moonbit_string_t _M0L3locS1065;
  moonbit_string_t _M0L9args__locS1067;
  void* _M0L6_2atmpS2542;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2543;
  moonbit_string_t _M0L6actualS1069;
  moonbit_string_t _M0L4wantS1071;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1065 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1066);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1067 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1068);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2542 = _M0L3objS1070.$0->$method_0(_M0L3objS1070.$1);
  _M0L6_2atmpS2543 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1069
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2542, 0, 0, _M0L6_2atmpS2543);
  if (_M0L7contentS1072 == 0) {
    void* _M0L6_2atmpS2539;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2540;
    if (_M0L7contentS1072) {
      moonbit_decref(_M0L7contentS1072);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2539
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2540 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1071
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2539, 0, 0, _M0L6_2atmpS2540);
  } else {
    void* _M0L7_2aSomeS1073 = _M0L7contentS1072;
    void* _M0L4_2axS1074 = _M0L7_2aSomeS1073;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2541 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1071
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1074, 0, 0, _M0L6_2atmpS2541);
  }
  moonbit_incref(_M0L4wantS1071);
  moonbit_incref(_M0L6actualS1069);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1069, _M0L4wantS1071)
  ) {
    moonbit_string_t _M0L6_2atmpS2537;
    moonbit_string_t _M0L6_2atmpS2787;
    moonbit_string_t _M0L6_2atmpS2536;
    moonbit_string_t _M0L6_2atmpS2786;
    moonbit_string_t _M0L6_2atmpS2534;
    moonbit_string_t _M0L6_2atmpS2535;
    moonbit_string_t _M0L6_2atmpS2785;
    moonbit_string_t _M0L6_2atmpS2533;
    moonbit_string_t _M0L6_2atmpS2784;
    moonbit_string_t _M0L6_2atmpS2530;
    moonbit_string_t _M0L6_2atmpS2532;
    moonbit_string_t _M0L6_2atmpS2531;
    moonbit_string_t _M0L6_2atmpS2783;
    moonbit_string_t _M0L6_2atmpS2529;
    moonbit_string_t _M0L6_2atmpS2782;
    moonbit_string_t _M0L6_2atmpS2526;
    moonbit_string_t _M0L6_2atmpS2528;
    moonbit_string_t _M0L6_2atmpS2527;
    moonbit_string_t _M0L6_2atmpS2781;
    moonbit_string_t _M0L6_2atmpS2525;
    moonbit_string_t _M0L6_2atmpS2780;
    moonbit_string_t _M0L6_2atmpS2524;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2523;
    struct moonbit_result_0 _result_3147;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2537
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1065);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2787
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_18.data, _M0L6_2atmpS2537);
    moonbit_decref(_M0L6_2atmpS2537);
    _M0L6_2atmpS2536 = _M0L6_2atmpS2787;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2786
    = moonbit_add_string(_M0L6_2atmpS2536, (moonbit_string_t)moonbit_string_literal_19.data);
    moonbit_decref(_M0L6_2atmpS2536);
    _M0L6_2atmpS2534 = _M0L6_2atmpS2786;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2535
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1067);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2785 = moonbit_add_string(_M0L6_2atmpS2534, _M0L6_2atmpS2535);
    moonbit_decref(_M0L6_2atmpS2534);
    moonbit_decref(_M0L6_2atmpS2535);
    _M0L6_2atmpS2533 = _M0L6_2atmpS2785;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2784
    = moonbit_add_string(_M0L6_2atmpS2533, (moonbit_string_t)moonbit_string_literal_20.data);
    moonbit_decref(_M0L6_2atmpS2533);
    _M0L6_2atmpS2530 = _M0L6_2atmpS2784;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2532 = _M0MPC16string6String6escape(_M0L4wantS1071);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2531
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2532);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2783 = moonbit_add_string(_M0L6_2atmpS2530, _M0L6_2atmpS2531);
    moonbit_decref(_M0L6_2atmpS2530);
    moonbit_decref(_M0L6_2atmpS2531);
    _M0L6_2atmpS2529 = _M0L6_2atmpS2783;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2782
    = moonbit_add_string(_M0L6_2atmpS2529, (moonbit_string_t)moonbit_string_literal_21.data);
    moonbit_decref(_M0L6_2atmpS2529);
    _M0L6_2atmpS2526 = _M0L6_2atmpS2782;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2528 = _M0MPC16string6String6escape(_M0L6actualS1069);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2527
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2528);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2781 = moonbit_add_string(_M0L6_2atmpS2526, _M0L6_2atmpS2527);
    moonbit_decref(_M0L6_2atmpS2526);
    moonbit_decref(_M0L6_2atmpS2527);
    _M0L6_2atmpS2525 = _M0L6_2atmpS2781;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2780
    = moonbit_add_string(_M0L6_2atmpS2525, (moonbit_string_t)moonbit_string_literal_22.data);
    moonbit_decref(_M0L6_2atmpS2525);
    _M0L6_2atmpS2524 = _M0L6_2atmpS2780;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2523
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2523)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2523)->$0
    = _M0L6_2atmpS2524;
    _result_3147.tag = 0;
    _result_3147.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2523;
    return _result_3147;
  } else {
    int32_t _M0L6_2atmpS2538;
    struct moonbit_result_0 _result_3148;
    moonbit_decref(_M0L4wantS1071);
    moonbit_decref(_M0L6actualS1069);
    moonbit_decref(_M0L9args__locS1067);
    moonbit_decref(_M0L3locS1065);
    _M0L6_2atmpS2538 = 0;
    _result_3148.tag = 1;
    _result_3148.data.ok = _M0L6_2atmpS2538;
    return _result_3148;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1064,
  int32_t _M0L13escape__slashS1036,
  int32_t _M0L6indentS1031,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1057
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1023;
  void** _M0L6_2atmpS2522;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1024;
  int32_t _M0Lm5depthS1025;
  void* _M0L6_2atmpS2521;
  void* _M0L8_2aparamS1026;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1023 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2522 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1024
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1024)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1024->$0 = _M0L6_2atmpS2522;
  _M0L5stackS1024->$1 = 0;
  _M0Lm5depthS1025 = 0;
  _M0L6_2atmpS2521 = _M0L4selfS1064;
  _M0L8_2aparamS1026 = _M0L6_2atmpS2521;
  _2aloop_1042:;
  while (1) {
    if (_M0L8_2aparamS1026 == 0) {
      int32_t _M0L3lenS2483;
      if (_M0L8_2aparamS1026) {
        moonbit_decref(_M0L8_2aparamS1026);
      }
      _M0L3lenS2483 = _M0L5stackS1024->$1;
      if (_M0L3lenS2483 == 0) {
        if (_M0L8replacerS1057) {
          moonbit_decref(_M0L8replacerS1057);
        }
        moonbit_decref(_M0L5stackS1024);
        break;
      } else {
        void** _M0L8_2afieldS2795 = _M0L5stackS1024->$0;
        void** _M0L3bufS2507 = _M0L8_2afieldS2795;
        int32_t _M0L3lenS2509 = _M0L5stackS1024->$1;
        int32_t _M0L6_2atmpS2508 = _M0L3lenS2509 - 1;
        void* _M0L6_2atmpS2794 = (void*)_M0L3bufS2507[_M0L6_2atmpS2508];
        void* _M0L4_2axS1043 = _M0L6_2atmpS2794;
        switch (Moonbit_object_tag(_M0L4_2axS1043)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1044 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1043;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2790 =
              _M0L8_2aArrayS1044->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1045 =
              _M0L8_2afieldS2790;
            int32_t _M0L4_2aiS1046 = _M0L8_2aArrayS1044->$1;
            int32_t _M0L3lenS2495 = _M0L6_2aarrS1045->$1;
            if (_M0L4_2aiS1046 < _M0L3lenS2495) {
              int32_t _if__result_3150;
              void** _M0L8_2afieldS2789;
              void** _M0L3bufS2501;
              void* _M0L6_2atmpS2788;
              void* _M0L7elementS1047;
              int32_t _M0L6_2atmpS2496;
              void* _M0L6_2atmpS2499;
              if (_M0L4_2aiS1046 < 0) {
                _if__result_3150 = 1;
              } else {
                int32_t _M0L3lenS2500 = _M0L6_2aarrS1045->$1;
                _if__result_3150 = _M0L4_2aiS1046 >= _M0L3lenS2500;
              }
              if (_if__result_3150) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS2789 = _M0L6_2aarrS1045->$0;
              _M0L3bufS2501 = _M0L8_2afieldS2789;
              _M0L6_2atmpS2788 = (void*)_M0L3bufS2501[_M0L4_2aiS1046];
              _M0L7elementS1047 = _M0L6_2atmpS2788;
              _M0L6_2atmpS2496 = _M0L4_2aiS1046 + 1;
              _M0L8_2aArrayS1044->$1 = _M0L6_2atmpS2496;
              if (_M0L4_2aiS1046 > 0) {
                int32_t _M0L6_2atmpS2498;
                moonbit_string_t _M0L6_2atmpS2497;
                moonbit_incref(_M0L7elementS1047);
                moonbit_incref(_M0L3bufS1023);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 44);
                _M0L6_2atmpS2498 = _M0Lm5depthS1025;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2497
                = _M0FPC14json11indent__str(_M0L6_2atmpS2498, _M0L6indentS1031);
                moonbit_incref(_M0L3bufS1023);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2497);
              } else {
                moonbit_incref(_M0L7elementS1047);
              }
              _M0L6_2atmpS2499 = _M0L7elementS1047;
              _M0L8_2aparamS1026 = _M0L6_2atmpS2499;
              goto _2aloop_1042;
            } else {
              int32_t _M0L6_2atmpS2502 = _M0Lm5depthS1025;
              void* _M0L6_2atmpS2503;
              int32_t _M0L6_2atmpS2505;
              moonbit_string_t _M0L6_2atmpS2504;
              void* _M0L6_2atmpS2506;
              _M0Lm5depthS1025 = _M0L6_2atmpS2502 - 1;
              moonbit_incref(_M0L5stackS1024);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2503
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1024);
              if (_M0L6_2atmpS2503) {
                moonbit_decref(_M0L6_2atmpS2503);
              }
              _M0L6_2atmpS2505 = _M0Lm5depthS1025;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2504
              = _M0FPC14json11indent__str(_M0L6_2atmpS2505, _M0L6indentS1031);
              moonbit_incref(_M0L3bufS1023);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2504);
              moonbit_incref(_M0L3bufS1023);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 93);
              _M0L6_2atmpS2506 = 0;
              _M0L8_2aparamS1026 = _M0L6_2atmpS2506;
              goto _2aloop_1042;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1048 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1043;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS2793 =
              _M0L9_2aObjectS1048->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1049 =
              _M0L8_2afieldS2793;
            int32_t _M0L8_2afirstS1050 = _M0L9_2aObjectS1048->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1051;
            moonbit_incref(_M0L11_2aiteratorS1049);
            moonbit_incref(_M0L9_2aObjectS1048);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1051
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1049);
            if (_M0L7_2abindS1051 == 0) {
              int32_t _M0L6_2atmpS2484;
              void* _M0L6_2atmpS2485;
              int32_t _M0L6_2atmpS2487;
              moonbit_string_t _M0L6_2atmpS2486;
              void* _M0L6_2atmpS2488;
              if (_M0L7_2abindS1051) {
                moonbit_decref(_M0L7_2abindS1051);
              }
              moonbit_decref(_M0L9_2aObjectS1048);
              _M0L6_2atmpS2484 = _M0Lm5depthS1025;
              _M0Lm5depthS1025 = _M0L6_2atmpS2484 - 1;
              moonbit_incref(_M0L5stackS1024);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2485
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1024);
              if (_M0L6_2atmpS2485) {
                moonbit_decref(_M0L6_2atmpS2485);
              }
              _M0L6_2atmpS2487 = _M0Lm5depthS1025;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2486
              = _M0FPC14json11indent__str(_M0L6_2atmpS2487, _M0L6indentS1031);
              moonbit_incref(_M0L3bufS1023);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2486);
              moonbit_incref(_M0L3bufS1023);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 125);
              _M0L6_2atmpS2488 = 0;
              _M0L8_2aparamS1026 = _M0L6_2atmpS2488;
              goto _2aloop_1042;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1052 = _M0L7_2abindS1051;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1053 = _M0L7_2aSomeS1052;
              moonbit_string_t _M0L8_2afieldS2792 = _M0L4_2axS1053->$0;
              moonbit_string_t _M0L4_2akS1054 = _M0L8_2afieldS2792;
              void* _M0L8_2afieldS2791 = _M0L4_2axS1053->$1;
              int32_t _M0L6_2acntS3036 =
                Moonbit_object_header(_M0L4_2axS1053)->rc;
              void* _M0L4_2avS1055;
              void* _M0Lm2v2S1056;
              moonbit_string_t _M0L6_2atmpS2492;
              void* _M0L6_2atmpS2494;
              void* _M0L6_2atmpS2493;
              if (_M0L6_2acntS3036 > 1) {
                int32_t _M0L11_2anew__cntS3037 = _M0L6_2acntS3036 - 1;
                Moonbit_object_header(_M0L4_2axS1053)->rc
                = _M0L11_2anew__cntS3037;
                moonbit_incref(_M0L8_2afieldS2791);
                moonbit_incref(_M0L4_2akS1054);
              } else if (_M0L6_2acntS3036 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1053);
              }
              _M0L4_2avS1055 = _M0L8_2afieldS2791;
              _M0Lm2v2S1056 = _M0L4_2avS1055;
              if (_M0L8replacerS1057 == 0) {
                moonbit_incref(_M0Lm2v2S1056);
                moonbit_decref(_M0L4_2avS1055);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1058 =
                  _M0L8replacerS1057;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1059 =
                  _M0L7_2aSomeS1058;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1060 =
                  _M0L11_2areplacerS1059;
                void* _M0L7_2abindS1061;
                moonbit_incref(_M0L7_2afuncS1060);
                moonbit_incref(_M0L4_2akS1054);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1061
                = _M0L7_2afuncS1060->code(_M0L7_2afuncS1060, _M0L4_2akS1054, _M0L4_2avS1055);
                if (_M0L7_2abindS1061 == 0) {
                  void* _M0L6_2atmpS2489;
                  if (_M0L7_2abindS1061) {
                    moonbit_decref(_M0L7_2abindS1061);
                  }
                  moonbit_decref(_M0L4_2akS1054);
                  moonbit_decref(_M0L9_2aObjectS1048);
                  _M0L6_2atmpS2489 = 0;
                  _M0L8_2aparamS1026 = _M0L6_2atmpS2489;
                  goto _2aloop_1042;
                } else {
                  void* _M0L7_2aSomeS1062 = _M0L7_2abindS1061;
                  void* _M0L4_2avS1063 = _M0L7_2aSomeS1062;
                  _M0Lm2v2S1056 = _M0L4_2avS1063;
                }
              }
              if (!_M0L8_2afirstS1050) {
                int32_t _M0L6_2atmpS2491;
                moonbit_string_t _M0L6_2atmpS2490;
                moonbit_incref(_M0L3bufS1023);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 44);
                _M0L6_2atmpS2491 = _M0Lm5depthS1025;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2490
                = _M0FPC14json11indent__str(_M0L6_2atmpS2491, _M0L6indentS1031);
                moonbit_incref(_M0L3bufS1023);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2490);
              }
              moonbit_incref(_M0L3bufS1023);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2492
              = _M0FPC14json6escape(_M0L4_2akS1054, _M0L13escape__slashS1036);
              moonbit_incref(_M0L3bufS1023);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2492);
              moonbit_incref(_M0L3bufS1023);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 34);
              moonbit_incref(_M0L3bufS1023);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 58);
              if (_M0L6indentS1031 > 0) {
                moonbit_incref(_M0L3bufS1023);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 32);
              }
              _M0L9_2aObjectS1048->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1048);
              _M0L6_2atmpS2494 = _M0Lm2v2S1056;
              _M0L6_2atmpS2493 = _M0L6_2atmpS2494;
              _M0L8_2aparamS1026 = _M0L6_2atmpS2493;
              goto _2aloop_1042;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1027 = _M0L8_2aparamS1026;
      void* _M0L8_2avalueS1028 = _M0L7_2aSomeS1027;
      void* _M0L6_2atmpS2520;
      switch (Moonbit_object_tag(_M0L8_2avalueS1028)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1029 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1028;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS2796 =
            _M0L9_2aObjectS1029->$0;
          int32_t _M0L6_2acntS3038 =
            Moonbit_object_header(_M0L9_2aObjectS1029)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1030;
          if (_M0L6_2acntS3038 > 1) {
            int32_t _M0L11_2anew__cntS3039 = _M0L6_2acntS3038 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1029)->rc
            = _M0L11_2anew__cntS3039;
            moonbit_incref(_M0L8_2afieldS2796);
          } else if (_M0L6_2acntS3038 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1029);
          }
          _M0L10_2amembersS1030 = _M0L8_2afieldS2796;
          moonbit_incref(_M0L10_2amembersS1030);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1030)) {
            moonbit_decref(_M0L10_2amembersS1030);
            moonbit_incref(_M0L3bufS1023);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, (moonbit_string_t)moonbit_string_literal_23.data);
          } else {
            int32_t _M0L6_2atmpS2515 = _M0Lm5depthS1025;
            int32_t _M0L6_2atmpS2517;
            moonbit_string_t _M0L6_2atmpS2516;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2519;
            void* _M0L6ObjectS2518;
            _M0Lm5depthS1025 = _M0L6_2atmpS2515 + 1;
            moonbit_incref(_M0L3bufS1023);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 123);
            _M0L6_2atmpS2517 = _M0Lm5depthS1025;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2516
            = _M0FPC14json11indent__str(_M0L6_2atmpS2517, _M0L6indentS1031);
            moonbit_incref(_M0L3bufS1023);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2516);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2519
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1030);
            _M0L6ObjectS2518
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2518)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2518)->$0
            = _M0L6_2atmpS2519;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2518)->$1
            = 1;
            moonbit_incref(_M0L5stackS1024);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1024, _M0L6ObjectS2518);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1032 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1028;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2797 =
            _M0L8_2aArrayS1032->$0;
          int32_t _M0L6_2acntS3040 =
            Moonbit_object_header(_M0L8_2aArrayS1032)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1033;
          if (_M0L6_2acntS3040 > 1) {
            int32_t _M0L11_2anew__cntS3041 = _M0L6_2acntS3040 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1032)->rc
            = _M0L11_2anew__cntS3041;
            moonbit_incref(_M0L8_2afieldS2797);
          } else if (_M0L6_2acntS3040 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1032);
          }
          _M0L6_2aarrS1033 = _M0L8_2afieldS2797;
          moonbit_incref(_M0L6_2aarrS1033);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1033)) {
            moonbit_decref(_M0L6_2aarrS1033);
            moonbit_incref(_M0L3bufS1023);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, (moonbit_string_t)moonbit_string_literal_24.data);
          } else {
            int32_t _M0L6_2atmpS2511 = _M0Lm5depthS1025;
            int32_t _M0L6_2atmpS2513;
            moonbit_string_t _M0L6_2atmpS2512;
            void* _M0L5ArrayS2514;
            _M0Lm5depthS1025 = _M0L6_2atmpS2511 + 1;
            moonbit_incref(_M0L3bufS1023);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 91);
            _M0L6_2atmpS2513 = _M0Lm5depthS1025;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2512
            = _M0FPC14json11indent__str(_M0L6_2atmpS2513, _M0L6indentS1031);
            moonbit_incref(_M0L3bufS1023);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2512);
            _M0L5ArrayS2514
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2514)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2514)->$0
            = _M0L6_2aarrS1033;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2514)->$1
            = 0;
            moonbit_incref(_M0L5stackS1024);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1024, _M0L5ArrayS2514);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1034 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1028;
          moonbit_string_t _M0L8_2afieldS2798 = _M0L9_2aStringS1034->$0;
          int32_t _M0L6_2acntS3042 =
            Moonbit_object_header(_M0L9_2aStringS1034)->rc;
          moonbit_string_t _M0L4_2asS1035;
          moonbit_string_t _M0L6_2atmpS2510;
          if (_M0L6_2acntS3042 > 1) {
            int32_t _M0L11_2anew__cntS3043 = _M0L6_2acntS3042 - 1;
            Moonbit_object_header(_M0L9_2aStringS1034)->rc
            = _M0L11_2anew__cntS3043;
            moonbit_incref(_M0L8_2afieldS2798);
          } else if (_M0L6_2acntS3042 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1034);
          }
          _M0L4_2asS1035 = _M0L8_2afieldS2798;
          moonbit_incref(_M0L3bufS1023);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2510
          = _M0FPC14json6escape(_M0L4_2asS1035, _M0L13escape__slashS1036);
          moonbit_incref(_M0L3bufS1023);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L6_2atmpS2510);
          moonbit_incref(_M0L3bufS1023);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1023, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1037 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1028;
          double _M0L4_2anS1038 = _M0L9_2aNumberS1037->$0;
          moonbit_string_t _M0L8_2afieldS2799 = _M0L9_2aNumberS1037->$1;
          int32_t _M0L6_2acntS3044 =
            Moonbit_object_header(_M0L9_2aNumberS1037)->rc;
          moonbit_string_t _M0L7_2areprS1039;
          if (_M0L6_2acntS3044 > 1) {
            int32_t _M0L11_2anew__cntS3045 = _M0L6_2acntS3044 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1037)->rc
            = _M0L11_2anew__cntS3045;
            if (_M0L8_2afieldS2799) {
              moonbit_incref(_M0L8_2afieldS2799);
            }
          } else if (_M0L6_2acntS3044 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1037);
          }
          _M0L7_2areprS1039 = _M0L8_2afieldS2799;
          if (_M0L7_2areprS1039 == 0) {
            if (_M0L7_2areprS1039) {
              moonbit_decref(_M0L7_2areprS1039);
            }
            moonbit_incref(_M0L3bufS1023);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1023, _M0L4_2anS1038);
          } else {
            moonbit_string_t _M0L7_2aSomeS1040 = _M0L7_2areprS1039;
            moonbit_string_t _M0L4_2arS1041 = _M0L7_2aSomeS1040;
            moonbit_incref(_M0L3bufS1023);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, _M0L4_2arS1041);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1023);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, (moonbit_string_t)moonbit_string_literal_25.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1023);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, (moonbit_string_t)moonbit_string_literal_26.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1028);
          moonbit_incref(_M0L3bufS1023);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1023, (moonbit_string_t)moonbit_string_literal_27.data);
          break;
        }
      }
      _M0L6_2atmpS2520 = 0;
      _M0L8_2aparamS1026 = _M0L6_2atmpS2520;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1023);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1022,
  int32_t _M0L6indentS1020
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1020 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1021 = _M0L6indentS1020 * _M0L5levelS1022;
    switch (_M0L6spacesS1021) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_9.data;
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
        moonbit_string_t _M0L6_2atmpS2482;
        moonbit_string_t _M0L6_2atmpS2800;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2482
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_36.data, _M0L6spacesS1021);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2800
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS2482);
        moonbit_decref(_M0L6_2atmpS2482);
        return _M0L6_2atmpS2800;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1012,
  int32_t _M0L13escape__slashS1017
) {
  int32_t _M0L6_2atmpS2481;
  struct _M0TPB13StringBuilder* _M0L3bufS1011;
  struct _M0TWEOc* _M0L5_2aitS1013;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2481 = Moonbit_array_length(_M0L3strS1012);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1011 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2481);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1013 = _M0MPC16string6String4iter(_M0L3strS1012);
  while (1) {
    int32_t _M0L7_2abindS1014;
    moonbit_incref(_M0L5_2aitS1013);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1014 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1013);
    if (_M0L7_2abindS1014 == -1) {
      moonbit_decref(_M0L5_2aitS1013);
    } else {
      int32_t _M0L7_2aSomeS1015 = _M0L7_2abindS1014;
      int32_t _M0L4_2acS1016 = _M0L7_2aSomeS1015;
      if (_M0L4_2acS1016 == 34) {
        moonbit_incref(_M0L3bufS1011);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_37.data);
      } else if (_M0L4_2acS1016 == 92) {
        moonbit_incref(_M0L3bufS1011);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_38.data);
      } else if (_M0L4_2acS1016 == 47) {
        if (_M0L13escape__slashS1017) {
          moonbit_incref(_M0L3bufS1011);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_39.data);
        } else {
          moonbit_incref(_M0L3bufS1011);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1011, _M0L4_2acS1016);
        }
      } else if (_M0L4_2acS1016 == 10) {
        moonbit_incref(_M0L3bufS1011);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_40.data);
      } else if (_M0L4_2acS1016 == 13) {
        moonbit_incref(_M0L3bufS1011);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_41.data);
      } else if (_M0L4_2acS1016 == 8) {
        moonbit_incref(_M0L3bufS1011);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_42.data);
      } else if (_M0L4_2acS1016 == 9) {
        moonbit_incref(_M0L3bufS1011);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_43.data);
      } else {
        int32_t _M0L4codeS1018 = _M0L4_2acS1016;
        if (_M0L4codeS1018 == 12) {
          moonbit_incref(_M0L3bufS1011);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_44.data);
        } else if (_M0L4codeS1018 < 32) {
          int32_t _M0L6_2atmpS2480;
          moonbit_string_t _M0L6_2atmpS2479;
          moonbit_incref(_M0L3bufS1011);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, (moonbit_string_t)moonbit_string_literal_45.data);
          _M0L6_2atmpS2480 = _M0L4codeS1018 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2479 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2480);
          moonbit_incref(_M0L3bufS1011);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1011, _M0L6_2atmpS2479);
        } else {
          moonbit_incref(_M0L3bufS1011);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1011, _M0L4_2acS1016);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1011);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1010
) {
  int32_t _M0L8_2afieldS2801;
  int32_t _M0L3lenS2478;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS2801 = _M0L4selfS1010->$1;
  moonbit_decref(_M0L4selfS1010);
  _M0L3lenS2478 = _M0L8_2afieldS2801;
  return _M0L3lenS2478 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1007
) {
  int32_t _M0L3lenS1006;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1006 = _M0L4selfS1007->$1;
  if (_M0L3lenS1006 == 0) {
    moonbit_decref(_M0L4selfS1007);
    return 0;
  } else {
    int32_t _M0L5indexS1008 = _M0L3lenS1006 - 1;
    void** _M0L8_2afieldS2805 = _M0L4selfS1007->$0;
    void** _M0L3bufS2477 = _M0L8_2afieldS2805;
    void* _M0L6_2atmpS2804 = (void*)_M0L3bufS2477[_M0L5indexS1008];
    void* _M0L1vS1009 = _M0L6_2atmpS2804;
    void** _M0L8_2afieldS2803 = _M0L4selfS1007->$0;
    void** _M0L3bufS2476 = _M0L8_2afieldS2803;
    void* _M0L6_2aoldS2802;
    if (
      _M0L5indexS1008 < 0
      || _M0L5indexS1008 >= Moonbit_array_length(_M0L3bufS2476)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS2802 = (void*)_M0L3bufS2476[_M0L5indexS1008];
    moonbit_incref(_M0L1vS1009);
    moonbit_decref(_M0L6_2aoldS2802);
    if (
      _M0L5indexS1008 < 0
      || _M0L5indexS1008 >= Moonbit_array_length(_M0L3bufS2476)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2476[_M0L5indexS1008]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1007->$1 = _M0L5indexS1008;
    moonbit_decref(_M0L4selfS1007);
    return _M0L1vS1009;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1004,
  struct _M0TPB6Logger _M0L6loggerS1005
) {
  moonbit_string_t _M0L6_2atmpS2475;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2474;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2475 = _M0L4selfS1004;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2474 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2475);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2474, _M0L6loggerS1005);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS981,
  struct _M0TPB6Logger _M0L6loggerS1003
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2814;
  struct _M0TPC16string10StringView _M0L3pkgS980;
  moonbit_string_t _M0L7_2adataS982;
  int32_t _M0L8_2astartS983;
  int32_t _M0L6_2atmpS2473;
  int32_t _M0L6_2aendS984;
  int32_t _M0Lm9_2acursorS985;
  int32_t _M0Lm13accept__stateS986;
  int32_t _M0Lm10match__endS987;
  int32_t _M0Lm20match__tag__saver__0S988;
  int32_t _M0Lm6tag__0S989;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS990;
  struct _M0TPC16string10StringView _M0L8_2afieldS2813;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS999;
  void* _M0L8_2afieldS2812;
  int32_t _M0L6_2acntS3046;
  void* _M0L16_2apackage__nameS1000;
  struct _M0TPC16string10StringView _M0L8_2afieldS2810;
  struct _M0TPC16string10StringView _M0L8filenameS2450;
  struct _M0TPC16string10StringView _M0L8_2afieldS2809;
  struct _M0TPC16string10StringView _M0L11start__lineS2451;
  struct _M0TPC16string10StringView _M0L8_2afieldS2808;
  struct _M0TPC16string10StringView _M0L13start__columnS2452;
  struct _M0TPC16string10StringView _M0L8_2afieldS2807;
  struct _M0TPC16string10StringView _M0L9end__lineS2453;
  struct _M0TPC16string10StringView _M0L8_2afieldS2806;
  int32_t _M0L6_2acntS3050;
  struct _M0TPC16string10StringView _M0L11end__columnS2454;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS2814
  = (struct _M0TPC16string10StringView){
    _M0L4selfS981->$0_1, _M0L4selfS981->$0_2, _M0L4selfS981->$0_0
  };
  _M0L3pkgS980 = _M0L8_2afieldS2814;
  moonbit_incref(_M0L3pkgS980.$0);
  moonbit_incref(_M0L3pkgS980.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS982 = _M0MPC16string10StringView4data(_M0L3pkgS980);
  moonbit_incref(_M0L3pkgS980.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS983 = _M0MPC16string10StringView13start__offset(_M0L3pkgS980);
  moonbit_incref(_M0L3pkgS980.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2473 = _M0MPC16string10StringView6length(_M0L3pkgS980);
  _M0L6_2aendS984 = _M0L8_2astartS983 + _M0L6_2atmpS2473;
  _M0Lm9_2acursorS985 = _M0L8_2astartS983;
  _M0Lm13accept__stateS986 = -1;
  _M0Lm10match__endS987 = -1;
  _M0Lm20match__tag__saver__0S988 = -1;
  _M0Lm6tag__0S989 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2465 = _M0Lm9_2acursorS985;
    if (_M0L6_2atmpS2465 < _M0L6_2aendS984) {
      int32_t _M0L6_2atmpS2472 = _M0Lm9_2acursorS985;
      int32_t _M0L10next__charS994;
      int32_t _M0L6_2atmpS2466;
      moonbit_incref(_M0L7_2adataS982);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS994
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS982, _M0L6_2atmpS2472);
      _M0L6_2atmpS2466 = _M0Lm9_2acursorS985;
      _M0Lm9_2acursorS985 = _M0L6_2atmpS2466 + 1;
      if (_M0L10next__charS994 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2467;
          _M0Lm6tag__0S989 = _M0Lm9_2acursorS985;
          _M0L6_2atmpS2467 = _M0Lm9_2acursorS985;
          if (_M0L6_2atmpS2467 < _M0L6_2aendS984) {
            int32_t _M0L6_2atmpS2471 = _M0Lm9_2acursorS985;
            int32_t _M0L10next__charS995;
            int32_t _M0L6_2atmpS2468;
            moonbit_incref(_M0L7_2adataS982);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS995
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS982, _M0L6_2atmpS2471);
            _M0L6_2atmpS2468 = _M0Lm9_2acursorS985;
            _M0Lm9_2acursorS985 = _M0L6_2atmpS2468 + 1;
            if (_M0L10next__charS995 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2469 = _M0Lm9_2acursorS985;
                if (_M0L6_2atmpS2469 < _M0L6_2aendS984) {
                  int32_t _M0L6_2atmpS2470 = _M0Lm9_2acursorS985;
                  _M0Lm9_2acursorS985 = _M0L6_2atmpS2470 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S988 = _M0Lm6tag__0S989;
                  _M0Lm13accept__stateS986 = 0;
                  _M0Lm10match__endS987 = _M0Lm9_2acursorS985;
                  goto join_991;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_991;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_991;
    }
    break;
  }
  goto joinlet_3152;
  join_991:;
  switch (_M0Lm13accept__stateS986) {
    case 0: {
      int32_t _M0L6_2atmpS2463;
      int32_t _M0L6_2atmpS2462;
      int64_t _M0L6_2atmpS2459;
      int32_t _M0L6_2atmpS2461;
      int64_t _M0L6_2atmpS2460;
      struct _M0TPC16string10StringView _M0L13package__nameS992;
      int64_t _M0L6_2atmpS2456;
      int32_t _M0L6_2atmpS2458;
      int64_t _M0L6_2atmpS2457;
      struct _M0TPC16string10StringView _M0L12module__nameS993;
      void* _M0L4SomeS2455;
      moonbit_decref(_M0L3pkgS980.$0);
      _M0L6_2atmpS2463 = _M0Lm20match__tag__saver__0S988;
      _M0L6_2atmpS2462 = _M0L6_2atmpS2463 + 1;
      _M0L6_2atmpS2459 = (int64_t)_M0L6_2atmpS2462;
      _M0L6_2atmpS2461 = _M0Lm10match__endS987;
      _M0L6_2atmpS2460 = (int64_t)_M0L6_2atmpS2461;
      moonbit_incref(_M0L7_2adataS982);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS992
      = _M0MPC16string6String4view(_M0L7_2adataS982, _M0L6_2atmpS2459, _M0L6_2atmpS2460);
      _M0L6_2atmpS2456 = (int64_t)_M0L8_2astartS983;
      _M0L6_2atmpS2458 = _M0Lm20match__tag__saver__0S988;
      _M0L6_2atmpS2457 = (int64_t)_M0L6_2atmpS2458;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS993
      = _M0MPC16string6String4view(_M0L7_2adataS982, _M0L6_2atmpS2456, _M0L6_2atmpS2457);
      _M0L4SomeS2455
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2455)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2455)->$0_0
      = _M0L13package__nameS992.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2455)->$0_1
      = _M0L13package__nameS992.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2455)->$0_2
      = _M0L13package__nameS992.$2;
      _M0L7_2abindS990
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS990)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS990->$0_0 = _M0L12module__nameS993.$0;
      _M0L7_2abindS990->$0_1 = _M0L12module__nameS993.$1;
      _M0L7_2abindS990->$0_2 = _M0L12module__nameS993.$2;
      _M0L7_2abindS990->$1 = _M0L4SomeS2455;
      break;
    }
    default: {
      void* _M0L4NoneS2464;
      moonbit_decref(_M0L7_2adataS982);
      _M0L4NoneS2464
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS990
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS990)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS990->$0_0 = _M0L3pkgS980.$0;
      _M0L7_2abindS990->$0_1 = _M0L3pkgS980.$1;
      _M0L7_2abindS990->$0_2 = _M0L3pkgS980.$2;
      _M0L7_2abindS990->$1 = _M0L4NoneS2464;
      break;
    }
  }
  joinlet_3152:;
  _M0L8_2afieldS2813
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS990->$0_1, _M0L7_2abindS990->$0_2, _M0L7_2abindS990->$0_0
  };
  _M0L15_2amodule__nameS999 = _M0L8_2afieldS2813;
  _M0L8_2afieldS2812 = _M0L7_2abindS990->$1;
  _M0L6_2acntS3046 = Moonbit_object_header(_M0L7_2abindS990)->rc;
  if (_M0L6_2acntS3046 > 1) {
    int32_t _M0L11_2anew__cntS3047 = _M0L6_2acntS3046 - 1;
    Moonbit_object_header(_M0L7_2abindS990)->rc = _M0L11_2anew__cntS3047;
    moonbit_incref(_M0L8_2afieldS2812);
    moonbit_incref(_M0L15_2amodule__nameS999.$0);
  } else if (_M0L6_2acntS3046 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS990);
  }
  _M0L16_2apackage__nameS1000 = _M0L8_2afieldS2812;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1000)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1001 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1000;
      struct _M0TPC16string10StringView _M0L8_2afieldS2811 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1001->$0_1,
                                              _M0L7_2aSomeS1001->$0_2,
                                              _M0L7_2aSomeS1001->$0_0};
      int32_t _M0L6_2acntS3048 = Moonbit_object_header(_M0L7_2aSomeS1001)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1002;
      if (_M0L6_2acntS3048 > 1) {
        int32_t _M0L11_2anew__cntS3049 = _M0L6_2acntS3048 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1001)->rc = _M0L11_2anew__cntS3049;
        moonbit_incref(_M0L8_2afieldS2811.$0);
      } else if (_M0L6_2acntS3048 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1001);
      }
      _M0L12_2apkg__nameS1002 = _M0L8_2afieldS2811;
      if (_M0L6loggerS1003.$1) {
        moonbit_incref(_M0L6loggerS1003.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1003.$0->$method_2(_M0L6loggerS1003.$1, _M0L12_2apkg__nameS1002);
      if (_M0L6loggerS1003.$1) {
        moonbit_incref(_M0L6loggerS1003.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1003.$0->$method_3(_M0L6loggerS1003.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1000);
      break;
    }
  }
  _M0L8_2afieldS2810
  = (struct _M0TPC16string10StringView){
    _M0L4selfS981->$1_1, _M0L4selfS981->$1_2, _M0L4selfS981->$1_0
  };
  _M0L8filenameS2450 = _M0L8_2afieldS2810;
  moonbit_incref(_M0L8filenameS2450.$0);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_2(_M0L6loggerS1003.$1, _M0L8filenameS2450);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_3(_M0L6loggerS1003.$1, 58);
  _M0L8_2afieldS2809
  = (struct _M0TPC16string10StringView){
    _M0L4selfS981->$2_1, _M0L4selfS981->$2_2, _M0L4selfS981->$2_0
  };
  _M0L11start__lineS2451 = _M0L8_2afieldS2809;
  moonbit_incref(_M0L11start__lineS2451.$0);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_2(_M0L6loggerS1003.$1, _M0L11start__lineS2451);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_3(_M0L6loggerS1003.$1, 58);
  _M0L8_2afieldS2808
  = (struct _M0TPC16string10StringView){
    _M0L4selfS981->$3_1, _M0L4selfS981->$3_2, _M0L4selfS981->$3_0
  };
  _M0L13start__columnS2452 = _M0L8_2afieldS2808;
  moonbit_incref(_M0L13start__columnS2452.$0);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_2(_M0L6loggerS1003.$1, _M0L13start__columnS2452);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_3(_M0L6loggerS1003.$1, 45);
  _M0L8_2afieldS2807
  = (struct _M0TPC16string10StringView){
    _M0L4selfS981->$4_1, _M0L4selfS981->$4_2, _M0L4selfS981->$4_0
  };
  _M0L9end__lineS2453 = _M0L8_2afieldS2807;
  moonbit_incref(_M0L9end__lineS2453.$0);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_2(_M0L6loggerS1003.$1, _M0L9end__lineS2453);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_3(_M0L6loggerS1003.$1, 58);
  _M0L8_2afieldS2806
  = (struct _M0TPC16string10StringView){
    _M0L4selfS981->$5_1, _M0L4selfS981->$5_2, _M0L4selfS981->$5_0
  };
  _M0L6_2acntS3050 = Moonbit_object_header(_M0L4selfS981)->rc;
  if (_M0L6_2acntS3050 > 1) {
    int32_t _M0L11_2anew__cntS3056 = _M0L6_2acntS3050 - 1;
    Moonbit_object_header(_M0L4selfS981)->rc = _M0L11_2anew__cntS3056;
    moonbit_incref(_M0L8_2afieldS2806.$0);
  } else if (_M0L6_2acntS3050 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3055 =
      (struct _M0TPC16string10StringView){_M0L4selfS981->$4_1,
                                            _M0L4selfS981->$4_2,
                                            _M0L4selfS981->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3054;
    struct _M0TPC16string10StringView _M0L8_2afieldS3053;
    struct _M0TPC16string10StringView _M0L8_2afieldS3052;
    struct _M0TPC16string10StringView _M0L8_2afieldS3051;
    moonbit_decref(_M0L8_2afieldS3055.$0);
    _M0L8_2afieldS3054
    = (struct _M0TPC16string10StringView){
      _M0L4selfS981->$3_1, _M0L4selfS981->$3_2, _M0L4selfS981->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3054.$0);
    _M0L8_2afieldS3053
    = (struct _M0TPC16string10StringView){
      _M0L4selfS981->$2_1, _M0L4selfS981->$2_2, _M0L4selfS981->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3053.$0);
    _M0L8_2afieldS3052
    = (struct _M0TPC16string10StringView){
      _M0L4selfS981->$1_1, _M0L4selfS981->$1_2, _M0L4selfS981->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3052.$0);
    _M0L8_2afieldS3051
    = (struct _M0TPC16string10StringView){
      _M0L4selfS981->$0_1, _M0L4selfS981->$0_2, _M0L4selfS981->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3051.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS981);
  }
  _M0L11end__columnS2454 = _M0L8_2afieldS2806;
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_2(_M0L6loggerS1003.$1, _M0L11end__columnS2454);
  if (_M0L6loggerS1003.$1) {
    moonbit_incref(_M0L6loggerS1003.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_3(_M0L6loggerS1003.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1003.$0->$method_2(_M0L6loggerS1003.$1, _M0L15_2amodule__nameS999);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS979) {
  moonbit_string_t _M0L6_2atmpS2449;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2449 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS979);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2449);
  moonbit_decref(_M0L6_2atmpS2449);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS978,
  struct _M0TPB6Logger _M0L6loggerS977
) {
  moonbit_string_t _M0L6_2atmpS2448;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2448 = _M0MPC16double6Double10to__string(_M0L4selfS978);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS977.$0->$method_0(_M0L6loggerS977.$1, _M0L6_2atmpS2448);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS976) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS976);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS963) {
  uint64_t _M0L4bitsS964;
  uint64_t _M0L6_2atmpS2447;
  uint64_t _M0L6_2atmpS2446;
  int32_t _M0L8ieeeSignS965;
  uint64_t _M0L12ieeeMantissaS966;
  uint64_t _M0L6_2atmpS2445;
  uint64_t _M0L6_2atmpS2444;
  int32_t _M0L12ieeeExponentS967;
  int32_t _if__result_3156;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS968;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS969;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2443;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS963 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_46.data;
  }
  _M0L4bitsS964 = *(int64_t*)&_M0L3valS963;
  _M0L6_2atmpS2447 = _M0L4bitsS964 >> 63;
  _M0L6_2atmpS2446 = _M0L6_2atmpS2447 & 1ull;
  _M0L8ieeeSignS965 = _M0L6_2atmpS2446 != 0ull;
  _M0L12ieeeMantissaS966 = _M0L4bitsS964 & 4503599627370495ull;
  _M0L6_2atmpS2445 = _M0L4bitsS964 >> 52;
  _M0L6_2atmpS2444 = _M0L6_2atmpS2445 & 2047ull;
  _M0L12ieeeExponentS967 = (int32_t)_M0L6_2atmpS2444;
  if (_M0L12ieeeExponentS967 == 2047) {
    _if__result_3156 = 1;
  } else if (_M0L12ieeeExponentS967 == 0) {
    _if__result_3156 = _M0L12ieeeMantissaS966 == 0ull;
  } else {
    _if__result_3156 = 0;
  }
  if (_if__result_3156) {
    int32_t _M0L6_2atmpS2432 = _M0L12ieeeExponentS967 != 0;
    int32_t _M0L6_2atmpS2433 = _M0L12ieeeMantissaS966 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS965, _M0L6_2atmpS2432, _M0L6_2atmpS2433);
  }
  _M0Lm1vS968 = _M0FPB30ryu__to__string_2erecord_2f962;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS969
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS966, _M0L12ieeeExponentS967);
  if (_M0L5smallS969 == 0) {
    uint32_t _M0L6_2atmpS2434;
    if (_M0L5smallS969) {
      moonbit_decref(_M0L5smallS969);
    }
    _M0L6_2atmpS2434 = *(uint32_t*)&_M0L12ieeeExponentS967;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS968 = _M0FPB3d2d(_M0L12ieeeMantissaS966, _M0L6_2atmpS2434);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS970 = _M0L5smallS969;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS971 = _M0L7_2aSomeS970;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS972 = _M0L4_2afS971;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2442 = _M0Lm1xS972;
      uint64_t _M0L8_2afieldS2817 = _M0L6_2atmpS2442->$0;
      uint64_t _M0L8mantissaS2441 = _M0L8_2afieldS2817;
      uint64_t _M0L1qS973 = _M0L8mantissaS2441 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2440 = _M0Lm1xS972;
      uint64_t _M0L8_2afieldS2816 = _M0L6_2atmpS2440->$0;
      uint64_t _M0L8mantissaS2438 = _M0L8_2afieldS2816;
      uint64_t _M0L6_2atmpS2439 = 10ull * _M0L1qS973;
      uint64_t _M0L1rS974 = _M0L8mantissaS2438 - _M0L6_2atmpS2439;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2437;
      int32_t _M0L8_2afieldS2815;
      int32_t _M0L8exponentS2436;
      int32_t _M0L6_2atmpS2435;
      if (_M0L1rS974 != 0ull) {
        break;
      }
      _M0L6_2atmpS2437 = _M0Lm1xS972;
      _M0L8_2afieldS2815 = _M0L6_2atmpS2437->$1;
      moonbit_decref(_M0L6_2atmpS2437);
      _M0L8exponentS2436 = _M0L8_2afieldS2815;
      _M0L6_2atmpS2435 = _M0L8exponentS2436 + 1;
      _M0Lm1xS972
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS972)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS972->$0 = _M0L1qS973;
      _M0Lm1xS972->$1 = _M0L6_2atmpS2435;
      continue;
      break;
    }
    _M0Lm1vS968 = _M0Lm1xS972;
  }
  _M0L6_2atmpS2443 = _M0Lm1vS968;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2443, _M0L8ieeeSignS965);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS957,
  int32_t _M0L12ieeeExponentS959
) {
  uint64_t _M0L2m2S956;
  int32_t _M0L6_2atmpS2431;
  int32_t _M0L2e2S958;
  int32_t _M0L6_2atmpS2430;
  uint64_t _M0L6_2atmpS2429;
  uint64_t _M0L4maskS960;
  uint64_t _M0L8fractionS961;
  int32_t _M0L6_2atmpS2428;
  uint64_t _M0L6_2atmpS2427;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2426;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S956 = 4503599627370496ull | _M0L12ieeeMantissaS957;
  _M0L6_2atmpS2431 = _M0L12ieeeExponentS959 - 1023;
  _M0L2e2S958 = _M0L6_2atmpS2431 - 52;
  if (_M0L2e2S958 > 0) {
    return 0;
  }
  if (_M0L2e2S958 < -52) {
    return 0;
  }
  _M0L6_2atmpS2430 = -_M0L2e2S958;
  _M0L6_2atmpS2429 = 1ull << (_M0L6_2atmpS2430 & 63);
  _M0L4maskS960 = _M0L6_2atmpS2429 - 1ull;
  _M0L8fractionS961 = _M0L2m2S956 & _M0L4maskS960;
  if (_M0L8fractionS961 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2428 = -_M0L2e2S958;
  _M0L6_2atmpS2427 = _M0L2m2S956 >> (_M0L6_2atmpS2428 & 63);
  _M0L6_2atmpS2426
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2426)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2426->$0 = _M0L6_2atmpS2427;
  _M0L6_2atmpS2426->$1 = 0;
  return _M0L6_2atmpS2426;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS930,
  int32_t _M0L4signS928
) {
  int32_t _M0L6_2atmpS2425;
  moonbit_bytes_t _M0L6resultS926;
  int32_t _M0Lm5indexS927;
  uint64_t _M0Lm6outputS929;
  uint64_t _M0L6_2atmpS2424;
  int32_t _M0L7olengthS931;
  int32_t _M0L8_2afieldS2818;
  int32_t _M0L8exponentS2423;
  int32_t _M0L6_2atmpS2422;
  int32_t _M0Lm3expS932;
  int32_t _M0L6_2atmpS2421;
  int32_t _M0L6_2atmpS2419;
  int32_t _M0L18scientificNotationS933;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2425 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS926 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2425);
  _M0Lm5indexS927 = 0;
  if (_M0L4signS928) {
    int32_t _M0L6_2atmpS2294 = _M0Lm5indexS927;
    int32_t _M0L6_2atmpS2295;
    if (
      _M0L6_2atmpS2294 < 0
      || _M0L6_2atmpS2294 >= Moonbit_array_length(_M0L6resultS926)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS926[_M0L6_2atmpS2294] = 45;
    _M0L6_2atmpS2295 = _M0Lm5indexS927;
    _M0Lm5indexS927 = _M0L6_2atmpS2295 + 1;
  }
  _M0Lm6outputS929 = _M0L1vS930->$0;
  _M0L6_2atmpS2424 = _M0Lm6outputS929;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS931 = _M0FPB17decimal__length17(_M0L6_2atmpS2424);
  _M0L8_2afieldS2818 = _M0L1vS930->$1;
  moonbit_decref(_M0L1vS930);
  _M0L8exponentS2423 = _M0L8_2afieldS2818;
  _M0L6_2atmpS2422 = _M0L8exponentS2423 + _M0L7olengthS931;
  _M0Lm3expS932 = _M0L6_2atmpS2422 - 1;
  _M0L6_2atmpS2421 = _M0Lm3expS932;
  if (_M0L6_2atmpS2421 >= -6) {
    int32_t _M0L6_2atmpS2420 = _M0Lm3expS932;
    _M0L6_2atmpS2419 = _M0L6_2atmpS2420 < 21;
  } else {
    _M0L6_2atmpS2419 = 0;
  }
  _M0L18scientificNotationS933 = !_M0L6_2atmpS2419;
  if (_M0L18scientificNotationS933) {
    int32_t _M0L7_2abindS934 = _M0L7olengthS931 - 1;
    int32_t _M0L1iS935 = 0;
    int32_t _M0L6_2atmpS2305;
    uint64_t _M0L6_2atmpS2310;
    int32_t _M0L6_2atmpS2309;
    int32_t _M0L6_2atmpS2308;
    int32_t _M0L6_2atmpS2307;
    int32_t _M0L6_2atmpS2306;
    int32_t _M0L6_2atmpS2314;
    int32_t _M0L6_2atmpS2315;
    int32_t _M0L6_2atmpS2316;
    int32_t _M0L6_2atmpS2317;
    int32_t _M0L6_2atmpS2318;
    int32_t _M0L6_2atmpS2324;
    int32_t _M0L6_2atmpS2357;
    while (1) {
      if (_M0L1iS935 < _M0L7_2abindS934) {
        uint64_t _M0L6_2atmpS2303 = _M0Lm6outputS929;
        uint64_t _M0L1cS936 = _M0L6_2atmpS2303 % 10ull;
        uint64_t _M0L6_2atmpS2296 = _M0Lm6outputS929;
        int32_t _M0L6_2atmpS2302;
        int32_t _M0L6_2atmpS2301;
        int32_t _M0L6_2atmpS2297;
        int32_t _M0L6_2atmpS2300;
        int32_t _M0L6_2atmpS2299;
        int32_t _M0L6_2atmpS2298;
        int32_t _M0L6_2atmpS2304;
        _M0Lm6outputS929 = _M0L6_2atmpS2296 / 10ull;
        _M0L6_2atmpS2302 = _M0Lm5indexS927;
        _M0L6_2atmpS2301 = _M0L6_2atmpS2302 + _M0L7olengthS931;
        _M0L6_2atmpS2297 = _M0L6_2atmpS2301 - _M0L1iS935;
        _M0L6_2atmpS2300 = (int32_t)_M0L1cS936;
        _M0L6_2atmpS2299 = 48 + _M0L6_2atmpS2300;
        _M0L6_2atmpS2298 = _M0L6_2atmpS2299 & 0xff;
        if (
          _M0L6_2atmpS2297 < 0
          || _M0L6_2atmpS2297 >= Moonbit_array_length(_M0L6resultS926)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS926[_M0L6_2atmpS2297] = _M0L6_2atmpS2298;
        _M0L6_2atmpS2304 = _M0L1iS935 + 1;
        _M0L1iS935 = _M0L6_2atmpS2304;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2305 = _M0Lm5indexS927;
    _M0L6_2atmpS2310 = _M0Lm6outputS929;
    _M0L6_2atmpS2309 = (int32_t)_M0L6_2atmpS2310;
    _M0L6_2atmpS2308 = _M0L6_2atmpS2309 % 10;
    _M0L6_2atmpS2307 = 48 + _M0L6_2atmpS2308;
    _M0L6_2atmpS2306 = _M0L6_2atmpS2307 & 0xff;
    if (
      _M0L6_2atmpS2305 < 0
      || _M0L6_2atmpS2305 >= Moonbit_array_length(_M0L6resultS926)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS926[_M0L6_2atmpS2305] = _M0L6_2atmpS2306;
    if (_M0L7olengthS931 > 1) {
      int32_t _M0L6_2atmpS2312 = _M0Lm5indexS927;
      int32_t _M0L6_2atmpS2311 = _M0L6_2atmpS2312 + 1;
      if (
        _M0L6_2atmpS2311 < 0
        || _M0L6_2atmpS2311 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2311] = 46;
    } else {
      int32_t _M0L6_2atmpS2313 = _M0Lm5indexS927;
      _M0Lm5indexS927 = _M0L6_2atmpS2313 - 1;
    }
    _M0L6_2atmpS2314 = _M0Lm5indexS927;
    _M0L6_2atmpS2315 = _M0L7olengthS931 + 1;
    _M0Lm5indexS927 = _M0L6_2atmpS2314 + _M0L6_2atmpS2315;
    _M0L6_2atmpS2316 = _M0Lm5indexS927;
    if (
      _M0L6_2atmpS2316 < 0
      || _M0L6_2atmpS2316 >= Moonbit_array_length(_M0L6resultS926)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS926[_M0L6_2atmpS2316] = 101;
    _M0L6_2atmpS2317 = _M0Lm5indexS927;
    _M0Lm5indexS927 = _M0L6_2atmpS2317 + 1;
    _M0L6_2atmpS2318 = _M0Lm3expS932;
    if (_M0L6_2atmpS2318 < 0) {
      int32_t _M0L6_2atmpS2319 = _M0Lm5indexS927;
      int32_t _M0L6_2atmpS2320;
      int32_t _M0L6_2atmpS2321;
      if (
        _M0L6_2atmpS2319 < 0
        || _M0L6_2atmpS2319 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2319] = 45;
      _M0L6_2atmpS2320 = _M0Lm5indexS927;
      _M0Lm5indexS927 = _M0L6_2atmpS2320 + 1;
      _M0L6_2atmpS2321 = _M0Lm3expS932;
      _M0Lm3expS932 = -_M0L6_2atmpS2321;
    } else {
      int32_t _M0L6_2atmpS2322 = _M0Lm5indexS927;
      int32_t _M0L6_2atmpS2323;
      if (
        _M0L6_2atmpS2322 < 0
        || _M0L6_2atmpS2322 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2322] = 43;
      _M0L6_2atmpS2323 = _M0Lm5indexS927;
      _M0Lm5indexS927 = _M0L6_2atmpS2323 + 1;
    }
    _M0L6_2atmpS2324 = _M0Lm3expS932;
    if (_M0L6_2atmpS2324 >= 100) {
      int32_t _M0L6_2atmpS2340 = _M0Lm3expS932;
      int32_t _M0L1aS938 = _M0L6_2atmpS2340 / 100;
      int32_t _M0L6_2atmpS2339 = _M0Lm3expS932;
      int32_t _M0L6_2atmpS2338 = _M0L6_2atmpS2339 / 10;
      int32_t _M0L1bS939 = _M0L6_2atmpS2338 % 10;
      int32_t _M0L6_2atmpS2337 = _M0Lm3expS932;
      int32_t _M0L1cS940 = _M0L6_2atmpS2337 % 10;
      int32_t _M0L6_2atmpS2325 = _M0Lm5indexS927;
      int32_t _M0L6_2atmpS2327 = 48 + _M0L1aS938;
      int32_t _M0L6_2atmpS2326 = _M0L6_2atmpS2327 & 0xff;
      int32_t _M0L6_2atmpS2331;
      int32_t _M0L6_2atmpS2328;
      int32_t _M0L6_2atmpS2330;
      int32_t _M0L6_2atmpS2329;
      int32_t _M0L6_2atmpS2335;
      int32_t _M0L6_2atmpS2332;
      int32_t _M0L6_2atmpS2334;
      int32_t _M0L6_2atmpS2333;
      int32_t _M0L6_2atmpS2336;
      if (
        _M0L6_2atmpS2325 < 0
        || _M0L6_2atmpS2325 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2325] = _M0L6_2atmpS2326;
      _M0L6_2atmpS2331 = _M0Lm5indexS927;
      _M0L6_2atmpS2328 = _M0L6_2atmpS2331 + 1;
      _M0L6_2atmpS2330 = 48 + _M0L1bS939;
      _M0L6_2atmpS2329 = _M0L6_2atmpS2330 & 0xff;
      if (
        _M0L6_2atmpS2328 < 0
        || _M0L6_2atmpS2328 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2328] = _M0L6_2atmpS2329;
      _M0L6_2atmpS2335 = _M0Lm5indexS927;
      _M0L6_2atmpS2332 = _M0L6_2atmpS2335 + 2;
      _M0L6_2atmpS2334 = 48 + _M0L1cS940;
      _M0L6_2atmpS2333 = _M0L6_2atmpS2334 & 0xff;
      if (
        _M0L6_2atmpS2332 < 0
        || _M0L6_2atmpS2332 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2332] = _M0L6_2atmpS2333;
      _M0L6_2atmpS2336 = _M0Lm5indexS927;
      _M0Lm5indexS927 = _M0L6_2atmpS2336 + 3;
    } else {
      int32_t _M0L6_2atmpS2341 = _M0Lm3expS932;
      if (_M0L6_2atmpS2341 >= 10) {
        int32_t _M0L6_2atmpS2351 = _M0Lm3expS932;
        int32_t _M0L1aS941 = _M0L6_2atmpS2351 / 10;
        int32_t _M0L6_2atmpS2350 = _M0Lm3expS932;
        int32_t _M0L1bS942 = _M0L6_2atmpS2350 % 10;
        int32_t _M0L6_2atmpS2342 = _M0Lm5indexS927;
        int32_t _M0L6_2atmpS2344 = 48 + _M0L1aS941;
        int32_t _M0L6_2atmpS2343 = _M0L6_2atmpS2344 & 0xff;
        int32_t _M0L6_2atmpS2348;
        int32_t _M0L6_2atmpS2345;
        int32_t _M0L6_2atmpS2347;
        int32_t _M0L6_2atmpS2346;
        int32_t _M0L6_2atmpS2349;
        if (
          _M0L6_2atmpS2342 < 0
          || _M0L6_2atmpS2342 >= Moonbit_array_length(_M0L6resultS926)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS926[_M0L6_2atmpS2342] = _M0L6_2atmpS2343;
        _M0L6_2atmpS2348 = _M0Lm5indexS927;
        _M0L6_2atmpS2345 = _M0L6_2atmpS2348 + 1;
        _M0L6_2atmpS2347 = 48 + _M0L1bS942;
        _M0L6_2atmpS2346 = _M0L6_2atmpS2347 & 0xff;
        if (
          _M0L6_2atmpS2345 < 0
          || _M0L6_2atmpS2345 >= Moonbit_array_length(_M0L6resultS926)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS926[_M0L6_2atmpS2345] = _M0L6_2atmpS2346;
        _M0L6_2atmpS2349 = _M0Lm5indexS927;
        _M0Lm5indexS927 = _M0L6_2atmpS2349 + 2;
      } else {
        int32_t _M0L6_2atmpS2352 = _M0Lm5indexS927;
        int32_t _M0L6_2atmpS2355 = _M0Lm3expS932;
        int32_t _M0L6_2atmpS2354 = 48 + _M0L6_2atmpS2355;
        int32_t _M0L6_2atmpS2353 = _M0L6_2atmpS2354 & 0xff;
        int32_t _M0L6_2atmpS2356;
        if (
          _M0L6_2atmpS2352 < 0
          || _M0L6_2atmpS2352 >= Moonbit_array_length(_M0L6resultS926)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS926[_M0L6_2atmpS2352] = _M0L6_2atmpS2353;
        _M0L6_2atmpS2356 = _M0Lm5indexS927;
        _M0Lm5indexS927 = _M0L6_2atmpS2356 + 1;
      }
    }
    _M0L6_2atmpS2357 = _M0Lm5indexS927;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS926, 0, _M0L6_2atmpS2357);
  } else {
    int32_t _M0L6_2atmpS2358 = _M0Lm3expS932;
    int32_t _M0L6_2atmpS2418;
    if (_M0L6_2atmpS2358 < 0) {
      int32_t _M0L6_2atmpS2359 = _M0Lm5indexS927;
      int32_t _M0L6_2atmpS2360;
      int32_t _M0L6_2atmpS2361;
      int32_t _M0L6_2atmpS2362;
      int32_t _M0L1iS943;
      int32_t _M0L7currentS945;
      int32_t _M0L1iS946;
      if (
        _M0L6_2atmpS2359 < 0
        || _M0L6_2atmpS2359 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2359] = 48;
      _M0L6_2atmpS2360 = _M0Lm5indexS927;
      _M0Lm5indexS927 = _M0L6_2atmpS2360 + 1;
      _M0L6_2atmpS2361 = _M0Lm5indexS927;
      if (
        _M0L6_2atmpS2361 < 0
        || _M0L6_2atmpS2361 >= Moonbit_array_length(_M0L6resultS926)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS926[_M0L6_2atmpS2361] = 46;
      _M0L6_2atmpS2362 = _M0Lm5indexS927;
      _M0Lm5indexS927 = _M0L6_2atmpS2362 + 1;
      _M0L1iS943 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2363 = _M0Lm3expS932;
        if (_M0L1iS943 > _M0L6_2atmpS2363) {
          int32_t _M0L6_2atmpS2364 = _M0Lm5indexS927;
          int32_t _M0L6_2atmpS2365;
          int32_t _M0L6_2atmpS2366;
          if (
            _M0L6_2atmpS2364 < 0
            || _M0L6_2atmpS2364 >= Moonbit_array_length(_M0L6resultS926)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS926[_M0L6_2atmpS2364] = 48;
          _M0L6_2atmpS2365 = _M0Lm5indexS927;
          _M0Lm5indexS927 = _M0L6_2atmpS2365 + 1;
          _M0L6_2atmpS2366 = _M0L1iS943 - 1;
          _M0L1iS943 = _M0L6_2atmpS2366;
          continue;
        }
        break;
      }
      _M0L7currentS945 = _M0Lm5indexS927;
      _M0L1iS946 = 0;
      while (1) {
        if (_M0L1iS946 < _M0L7olengthS931) {
          int32_t _M0L6_2atmpS2374 = _M0L7currentS945 + _M0L7olengthS931;
          int32_t _M0L6_2atmpS2373 = _M0L6_2atmpS2374 - _M0L1iS946;
          int32_t _M0L6_2atmpS2367 = _M0L6_2atmpS2373 - 1;
          uint64_t _M0L6_2atmpS2372 = _M0Lm6outputS929;
          uint64_t _M0L6_2atmpS2371 = _M0L6_2atmpS2372 % 10ull;
          int32_t _M0L6_2atmpS2370 = (int32_t)_M0L6_2atmpS2371;
          int32_t _M0L6_2atmpS2369 = 48 + _M0L6_2atmpS2370;
          int32_t _M0L6_2atmpS2368 = _M0L6_2atmpS2369 & 0xff;
          uint64_t _M0L6_2atmpS2375;
          int32_t _M0L6_2atmpS2376;
          int32_t _M0L6_2atmpS2377;
          if (
            _M0L6_2atmpS2367 < 0
            || _M0L6_2atmpS2367 >= Moonbit_array_length(_M0L6resultS926)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS926[_M0L6_2atmpS2367] = _M0L6_2atmpS2368;
          _M0L6_2atmpS2375 = _M0Lm6outputS929;
          _M0Lm6outputS929 = _M0L6_2atmpS2375 / 10ull;
          _M0L6_2atmpS2376 = _M0Lm5indexS927;
          _M0Lm5indexS927 = _M0L6_2atmpS2376 + 1;
          _M0L6_2atmpS2377 = _M0L1iS946 + 1;
          _M0L1iS946 = _M0L6_2atmpS2377;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2379 = _M0Lm3expS932;
      int32_t _M0L6_2atmpS2378 = _M0L6_2atmpS2379 + 1;
      if (_M0L6_2atmpS2378 >= _M0L7olengthS931) {
        int32_t _M0L1iS948 = 0;
        int32_t _M0L6_2atmpS2391;
        int32_t _M0L6_2atmpS2395;
        int32_t _M0L7_2abindS950;
        int32_t _M0L2__S951;
        while (1) {
          if (_M0L1iS948 < _M0L7olengthS931) {
            int32_t _M0L6_2atmpS2388 = _M0Lm5indexS927;
            int32_t _M0L6_2atmpS2387 = _M0L6_2atmpS2388 + _M0L7olengthS931;
            int32_t _M0L6_2atmpS2386 = _M0L6_2atmpS2387 - _M0L1iS948;
            int32_t _M0L6_2atmpS2380 = _M0L6_2atmpS2386 - 1;
            uint64_t _M0L6_2atmpS2385 = _M0Lm6outputS929;
            uint64_t _M0L6_2atmpS2384 = _M0L6_2atmpS2385 % 10ull;
            int32_t _M0L6_2atmpS2383 = (int32_t)_M0L6_2atmpS2384;
            int32_t _M0L6_2atmpS2382 = 48 + _M0L6_2atmpS2383;
            int32_t _M0L6_2atmpS2381 = _M0L6_2atmpS2382 & 0xff;
            uint64_t _M0L6_2atmpS2389;
            int32_t _M0L6_2atmpS2390;
            if (
              _M0L6_2atmpS2380 < 0
              || _M0L6_2atmpS2380 >= Moonbit_array_length(_M0L6resultS926)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS926[_M0L6_2atmpS2380] = _M0L6_2atmpS2381;
            _M0L6_2atmpS2389 = _M0Lm6outputS929;
            _M0Lm6outputS929 = _M0L6_2atmpS2389 / 10ull;
            _M0L6_2atmpS2390 = _M0L1iS948 + 1;
            _M0L1iS948 = _M0L6_2atmpS2390;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2391 = _M0Lm5indexS927;
        _M0Lm5indexS927 = _M0L6_2atmpS2391 + _M0L7olengthS931;
        _M0L6_2atmpS2395 = _M0Lm3expS932;
        _M0L7_2abindS950 = _M0L6_2atmpS2395 + 1;
        _M0L2__S951 = _M0L7olengthS931;
        while (1) {
          if (_M0L2__S951 < _M0L7_2abindS950) {
            int32_t _M0L6_2atmpS2392 = _M0Lm5indexS927;
            int32_t _M0L6_2atmpS2393;
            int32_t _M0L6_2atmpS2394;
            if (
              _M0L6_2atmpS2392 < 0
              || _M0L6_2atmpS2392 >= Moonbit_array_length(_M0L6resultS926)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS926[_M0L6_2atmpS2392] = 48;
            _M0L6_2atmpS2393 = _M0Lm5indexS927;
            _M0Lm5indexS927 = _M0L6_2atmpS2393 + 1;
            _M0L6_2atmpS2394 = _M0L2__S951 + 1;
            _M0L2__S951 = _M0L6_2atmpS2394;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2417 = _M0Lm5indexS927;
        int32_t _M0Lm7currentS953 = _M0L6_2atmpS2417 + 1;
        int32_t _M0L1iS954 = 0;
        int32_t _M0L6_2atmpS2415;
        int32_t _M0L6_2atmpS2416;
        while (1) {
          if (_M0L1iS954 < _M0L7olengthS931) {
            int32_t _M0L6_2atmpS2398 = _M0L7olengthS931 - _M0L1iS954;
            int32_t _M0L6_2atmpS2396 = _M0L6_2atmpS2398 - 1;
            int32_t _M0L6_2atmpS2397 = _M0Lm3expS932;
            int32_t _M0L6_2atmpS2412;
            int32_t _M0L6_2atmpS2411;
            int32_t _M0L6_2atmpS2410;
            int32_t _M0L6_2atmpS2404;
            uint64_t _M0L6_2atmpS2409;
            uint64_t _M0L6_2atmpS2408;
            int32_t _M0L6_2atmpS2407;
            int32_t _M0L6_2atmpS2406;
            int32_t _M0L6_2atmpS2405;
            uint64_t _M0L6_2atmpS2413;
            int32_t _M0L6_2atmpS2414;
            if (_M0L6_2atmpS2396 == _M0L6_2atmpS2397) {
              int32_t _M0L6_2atmpS2402 = _M0Lm7currentS953;
              int32_t _M0L6_2atmpS2401 = _M0L6_2atmpS2402 + _M0L7olengthS931;
              int32_t _M0L6_2atmpS2400 = _M0L6_2atmpS2401 - _M0L1iS954;
              int32_t _M0L6_2atmpS2399 = _M0L6_2atmpS2400 - 1;
              int32_t _M0L6_2atmpS2403;
              if (
                _M0L6_2atmpS2399 < 0
                || _M0L6_2atmpS2399 >= Moonbit_array_length(_M0L6resultS926)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS926[_M0L6_2atmpS2399] = 46;
              _M0L6_2atmpS2403 = _M0Lm7currentS953;
              _M0Lm7currentS953 = _M0L6_2atmpS2403 - 1;
            }
            _M0L6_2atmpS2412 = _M0Lm7currentS953;
            _M0L6_2atmpS2411 = _M0L6_2atmpS2412 + _M0L7olengthS931;
            _M0L6_2atmpS2410 = _M0L6_2atmpS2411 - _M0L1iS954;
            _M0L6_2atmpS2404 = _M0L6_2atmpS2410 - 1;
            _M0L6_2atmpS2409 = _M0Lm6outputS929;
            _M0L6_2atmpS2408 = _M0L6_2atmpS2409 % 10ull;
            _M0L6_2atmpS2407 = (int32_t)_M0L6_2atmpS2408;
            _M0L6_2atmpS2406 = 48 + _M0L6_2atmpS2407;
            _M0L6_2atmpS2405 = _M0L6_2atmpS2406 & 0xff;
            if (
              _M0L6_2atmpS2404 < 0
              || _M0L6_2atmpS2404 >= Moonbit_array_length(_M0L6resultS926)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS926[_M0L6_2atmpS2404] = _M0L6_2atmpS2405;
            _M0L6_2atmpS2413 = _M0Lm6outputS929;
            _M0Lm6outputS929 = _M0L6_2atmpS2413 / 10ull;
            _M0L6_2atmpS2414 = _M0L1iS954 + 1;
            _M0L1iS954 = _M0L6_2atmpS2414;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2415 = _M0Lm5indexS927;
        _M0L6_2atmpS2416 = _M0L7olengthS931 + 1;
        _M0Lm5indexS927 = _M0L6_2atmpS2415 + _M0L6_2atmpS2416;
      }
    }
    _M0L6_2atmpS2418 = _M0Lm5indexS927;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS926, 0, _M0L6_2atmpS2418);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS872,
  uint32_t _M0L12ieeeExponentS871
) {
  int32_t _M0Lm2e2S869;
  uint64_t _M0Lm2m2S870;
  uint64_t _M0L6_2atmpS2293;
  uint64_t _M0L6_2atmpS2292;
  int32_t _M0L4evenS873;
  uint64_t _M0L6_2atmpS2291;
  uint64_t _M0L2mvS874;
  int32_t _M0L7mmShiftS875;
  uint64_t _M0Lm2vrS876;
  uint64_t _M0Lm2vpS877;
  uint64_t _M0Lm2vmS878;
  int32_t _M0Lm3e10S879;
  int32_t _M0Lm17vmIsTrailingZerosS880;
  int32_t _M0Lm17vrIsTrailingZerosS881;
  int32_t _M0L6_2atmpS2193;
  int32_t _M0Lm7removedS900;
  int32_t _M0Lm16lastRemovedDigitS901;
  uint64_t _M0Lm6outputS902;
  int32_t _M0L6_2atmpS2289;
  int32_t _M0L6_2atmpS2290;
  int32_t _M0L3expS925;
  uint64_t _M0L6_2atmpS2288;
  struct _M0TPB17FloatingDecimal64* _block_3169;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S869 = 0;
  _M0Lm2m2S870 = 0ull;
  if (_M0L12ieeeExponentS871 == 0u) {
    _M0Lm2e2S869 = -1076;
    _M0Lm2m2S870 = _M0L12ieeeMantissaS872;
  } else {
    int32_t _M0L6_2atmpS2192 = *(int32_t*)&_M0L12ieeeExponentS871;
    int32_t _M0L6_2atmpS2191 = _M0L6_2atmpS2192 - 1023;
    int32_t _M0L6_2atmpS2190 = _M0L6_2atmpS2191 - 52;
    _M0Lm2e2S869 = _M0L6_2atmpS2190 - 2;
    _M0Lm2m2S870 = 4503599627370496ull | _M0L12ieeeMantissaS872;
  }
  _M0L6_2atmpS2293 = _M0Lm2m2S870;
  _M0L6_2atmpS2292 = _M0L6_2atmpS2293 & 1ull;
  _M0L4evenS873 = _M0L6_2atmpS2292 == 0ull;
  _M0L6_2atmpS2291 = _M0Lm2m2S870;
  _M0L2mvS874 = 4ull * _M0L6_2atmpS2291;
  if (_M0L12ieeeMantissaS872 != 0ull) {
    _M0L7mmShiftS875 = 1;
  } else {
    _M0L7mmShiftS875 = _M0L12ieeeExponentS871 <= 1u;
  }
  _M0Lm2vrS876 = 0ull;
  _M0Lm2vpS877 = 0ull;
  _M0Lm2vmS878 = 0ull;
  _M0Lm3e10S879 = 0;
  _M0Lm17vmIsTrailingZerosS880 = 0;
  _M0Lm17vrIsTrailingZerosS881 = 0;
  _M0L6_2atmpS2193 = _M0Lm2e2S869;
  if (_M0L6_2atmpS2193 >= 0) {
    int32_t _M0L6_2atmpS2215 = _M0Lm2e2S869;
    int32_t _M0L6_2atmpS2211;
    int32_t _M0L6_2atmpS2214;
    int32_t _M0L6_2atmpS2213;
    int32_t _M0L6_2atmpS2212;
    int32_t _M0L1qS882;
    int32_t _M0L6_2atmpS2210;
    int32_t _M0L6_2atmpS2209;
    int32_t _M0L1kS883;
    int32_t _M0L6_2atmpS2208;
    int32_t _M0L6_2atmpS2207;
    int32_t _M0L6_2atmpS2206;
    int32_t _M0L1iS884;
    struct _M0TPB8Pow5Pair _M0L4pow5S885;
    uint64_t _M0L6_2atmpS2205;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS886;
    uint64_t _M0L8_2avrOutS887;
    uint64_t _M0L8_2avpOutS888;
    uint64_t _M0L8_2avmOutS889;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2211 = _M0FPB9log10Pow2(_M0L6_2atmpS2215);
    _M0L6_2atmpS2214 = _M0Lm2e2S869;
    _M0L6_2atmpS2213 = _M0L6_2atmpS2214 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2212 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2213);
    _M0L1qS882 = _M0L6_2atmpS2211 - _M0L6_2atmpS2212;
    _M0Lm3e10S879 = _M0L1qS882;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2210 = _M0FPB8pow5bits(_M0L1qS882);
    _M0L6_2atmpS2209 = 125 + _M0L6_2atmpS2210;
    _M0L1kS883 = _M0L6_2atmpS2209 - 1;
    _M0L6_2atmpS2208 = _M0Lm2e2S869;
    _M0L6_2atmpS2207 = -_M0L6_2atmpS2208;
    _M0L6_2atmpS2206 = _M0L6_2atmpS2207 + _M0L1qS882;
    _M0L1iS884 = _M0L6_2atmpS2206 + _M0L1kS883;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S885 = _M0FPB22double__computeInvPow5(_M0L1qS882);
    _M0L6_2atmpS2205 = _M0Lm2m2S870;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS886
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2205, _M0L4pow5S885, _M0L1iS884, _M0L7mmShiftS875);
    _M0L8_2avrOutS887 = _M0L7_2abindS886.$0;
    _M0L8_2avpOutS888 = _M0L7_2abindS886.$1;
    _M0L8_2avmOutS889 = _M0L7_2abindS886.$2;
    _M0Lm2vrS876 = _M0L8_2avrOutS887;
    _M0Lm2vpS877 = _M0L8_2avpOutS888;
    _M0Lm2vmS878 = _M0L8_2avmOutS889;
    if (_M0L1qS882 <= 21) {
      int32_t _M0L6_2atmpS2201 = (int32_t)_M0L2mvS874;
      uint64_t _M0L6_2atmpS2204 = _M0L2mvS874 / 5ull;
      int32_t _M0L6_2atmpS2203 = (int32_t)_M0L6_2atmpS2204;
      int32_t _M0L6_2atmpS2202 = 5 * _M0L6_2atmpS2203;
      int32_t _M0L6mvMod5S890 = _M0L6_2atmpS2201 - _M0L6_2atmpS2202;
      if (_M0L6mvMod5S890 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS881
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS874, _M0L1qS882);
      } else if (_M0L4evenS873) {
        uint64_t _M0L6_2atmpS2195 = _M0L2mvS874 - 1ull;
        uint64_t _M0L6_2atmpS2196;
        uint64_t _M0L6_2atmpS2194;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2196 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS875);
        _M0L6_2atmpS2194 = _M0L6_2atmpS2195 - _M0L6_2atmpS2196;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS880
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2194, _M0L1qS882);
      } else {
        uint64_t _M0L6_2atmpS2197 = _M0Lm2vpS877;
        uint64_t _M0L6_2atmpS2200 = _M0L2mvS874 + 2ull;
        int32_t _M0L6_2atmpS2199;
        uint64_t _M0L6_2atmpS2198;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2199
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2200, _M0L1qS882);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2198 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2199);
        _M0Lm2vpS877 = _M0L6_2atmpS2197 - _M0L6_2atmpS2198;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2229 = _M0Lm2e2S869;
    int32_t _M0L6_2atmpS2228 = -_M0L6_2atmpS2229;
    int32_t _M0L6_2atmpS2223;
    int32_t _M0L6_2atmpS2227;
    int32_t _M0L6_2atmpS2226;
    int32_t _M0L6_2atmpS2225;
    int32_t _M0L6_2atmpS2224;
    int32_t _M0L1qS891;
    int32_t _M0L6_2atmpS2216;
    int32_t _M0L6_2atmpS2222;
    int32_t _M0L6_2atmpS2221;
    int32_t _M0L1iS892;
    int32_t _M0L6_2atmpS2220;
    int32_t _M0L1kS893;
    int32_t _M0L1jS894;
    struct _M0TPB8Pow5Pair _M0L4pow5S895;
    uint64_t _M0L6_2atmpS2219;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS896;
    uint64_t _M0L8_2avrOutS897;
    uint64_t _M0L8_2avpOutS898;
    uint64_t _M0L8_2avmOutS899;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2223 = _M0FPB9log10Pow5(_M0L6_2atmpS2228);
    _M0L6_2atmpS2227 = _M0Lm2e2S869;
    _M0L6_2atmpS2226 = -_M0L6_2atmpS2227;
    _M0L6_2atmpS2225 = _M0L6_2atmpS2226 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2224 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2225);
    _M0L1qS891 = _M0L6_2atmpS2223 - _M0L6_2atmpS2224;
    _M0L6_2atmpS2216 = _M0Lm2e2S869;
    _M0Lm3e10S879 = _M0L1qS891 + _M0L6_2atmpS2216;
    _M0L6_2atmpS2222 = _M0Lm2e2S869;
    _M0L6_2atmpS2221 = -_M0L6_2atmpS2222;
    _M0L1iS892 = _M0L6_2atmpS2221 - _M0L1qS891;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2220 = _M0FPB8pow5bits(_M0L1iS892);
    _M0L1kS893 = _M0L6_2atmpS2220 - 125;
    _M0L1jS894 = _M0L1qS891 - _M0L1kS893;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S895 = _M0FPB19double__computePow5(_M0L1iS892);
    _M0L6_2atmpS2219 = _M0Lm2m2S870;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS896
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2219, _M0L4pow5S895, _M0L1jS894, _M0L7mmShiftS875);
    _M0L8_2avrOutS897 = _M0L7_2abindS896.$0;
    _M0L8_2avpOutS898 = _M0L7_2abindS896.$1;
    _M0L8_2avmOutS899 = _M0L7_2abindS896.$2;
    _M0Lm2vrS876 = _M0L8_2avrOutS897;
    _M0Lm2vpS877 = _M0L8_2avpOutS898;
    _M0Lm2vmS878 = _M0L8_2avmOutS899;
    if (_M0L1qS891 <= 1) {
      _M0Lm17vrIsTrailingZerosS881 = 1;
      if (_M0L4evenS873) {
        int32_t _M0L6_2atmpS2217;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2217 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS875);
        _M0Lm17vmIsTrailingZerosS880 = _M0L6_2atmpS2217 == 1;
      } else {
        uint64_t _M0L6_2atmpS2218 = _M0Lm2vpS877;
        _M0Lm2vpS877 = _M0L6_2atmpS2218 - 1ull;
      }
    } else if (_M0L1qS891 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS881
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS874, _M0L1qS891);
    }
  }
  _M0Lm7removedS900 = 0;
  _M0Lm16lastRemovedDigitS901 = 0;
  _M0Lm6outputS902 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS880 || _M0Lm17vrIsTrailingZerosS881) {
    int32_t _if__result_3166;
    uint64_t _M0L6_2atmpS2259;
    uint64_t _M0L6_2atmpS2265;
    uint64_t _M0L6_2atmpS2266;
    int32_t _if__result_3167;
    int32_t _M0L6_2atmpS2262;
    int64_t _M0L6_2atmpS2261;
    uint64_t _M0L6_2atmpS2260;
    while (1) {
      uint64_t _M0L6_2atmpS2242 = _M0Lm2vpS877;
      uint64_t _M0L7vpDiv10S903 = _M0L6_2atmpS2242 / 10ull;
      uint64_t _M0L6_2atmpS2241 = _M0Lm2vmS878;
      uint64_t _M0L7vmDiv10S904 = _M0L6_2atmpS2241 / 10ull;
      uint64_t _M0L6_2atmpS2240;
      int32_t _M0L6_2atmpS2237;
      int32_t _M0L6_2atmpS2239;
      int32_t _M0L6_2atmpS2238;
      int32_t _M0L7vmMod10S906;
      uint64_t _M0L6_2atmpS2236;
      uint64_t _M0L7vrDiv10S907;
      uint64_t _M0L6_2atmpS2235;
      int32_t _M0L6_2atmpS2232;
      int32_t _M0L6_2atmpS2234;
      int32_t _M0L6_2atmpS2233;
      int32_t _M0L7vrMod10S908;
      int32_t _M0L6_2atmpS2231;
      if (_M0L7vpDiv10S903 <= _M0L7vmDiv10S904) {
        break;
      }
      _M0L6_2atmpS2240 = _M0Lm2vmS878;
      _M0L6_2atmpS2237 = (int32_t)_M0L6_2atmpS2240;
      _M0L6_2atmpS2239 = (int32_t)_M0L7vmDiv10S904;
      _M0L6_2atmpS2238 = 10 * _M0L6_2atmpS2239;
      _M0L7vmMod10S906 = _M0L6_2atmpS2237 - _M0L6_2atmpS2238;
      _M0L6_2atmpS2236 = _M0Lm2vrS876;
      _M0L7vrDiv10S907 = _M0L6_2atmpS2236 / 10ull;
      _M0L6_2atmpS2235 = _M0Lm2vrS876;
      _M0L6_2atmpS2232 = (int32_t)_M0L6_2atmpS2235;
      _M0L6_2atmpS2234 = (int32_t)_M0L7vrDiv10S907;
      _M0L6_2atmpS2233 = 10 * _M0L6_2atmpS2234;
      _M0L7vrMod10S908 = _M0L6_2atmpS2232 - _M0L6_2atmpS2233;
      if (_M0Lm17vmIsTrailingZerosS880) {
        _M0Lm17vmIsTrailingZerosS880 = _M0L7vmMod10S906 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS880 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS881) {
        int32_t _M0L6_2atmpS2230 = _M0Lm16lastRemovedDigitS901;
        _M0Lm17vrIsTrailingZerosS881 = _M0L6_2atmpS2230 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS881 = 0;
      }
      _M0Lm16lastRemovedDigitS901 = _M0L7vrMod10S908;
      _M0Lm2vrS876 = _M0L7vrDiv10S907;
      _M0Lm2vpS877 = _M0L7vpDiv10S903;
      _M0Lm2vmS878 = _M0L7vmDiv10S904;
      _M0L6_2atmpS2231 = _M0Lm7removedS900;
      _M0Lm7removedS900 = _M0L6_2atmpS2231 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS880) {
      while (1) {
        uint64_t _M0L6_2atmpS2255 = _M0Lm2vmS878;
        uint64_t _M0L7vmDiv10S909 = _M0L6_2atmpS2255 / 10ull;
        uint64_t _M0L6_2atmpS2254 = _M0Lm2vmS878;
        int32_t _M0L6_2atmpS2251 = (int32_t)_M0L6_2atmpS2254;
        int32_t _M0L6_2atmpS2253 = (int32_t)_M0L7vmDiv10S909;
        int32_t _M0L6_2atmpS2252 = 10 * _M0L6_2atmpS2253;
        int32_t _M0L7vmMod10S910 = _M0L6_2atmpS2251 - _M0L6_2atmpS2252;
        uint64_t _M0L6_2atmpS2250;
        uint64_t _M0L7vpDiv10S912;
        uint64_t _M0L6_2atmpS2249;
        uint64_t _M0L7vrDiv10S913;
        uint64_t _M0L6_2atmpS2248;
        int32_t _M0L6_2atmpS2245;
        int32_t _M0L6_2atmpS2247;
        int32_t _M0L6_2atmpS2246;
        int32_t _M0L7vrMod10S914;
        int32_t _M0L6_2atmpS2244;
        if (_M0L7vmMod10S910 != 0) {
          break;
        }
        _M0L6_2atmpS2250 = _M0Lm2vpS877;
        _M0L7vpDiv10S912 = _M0L6_2atmpS2250 / 10ull;
        _M0L6_2atmpS2249 = _M0Lm2vrS876;
        _M0L7vrDiv10S913 = _M0L6_2atmpS2249 / 10ull;
        _M0L6_2atmpS2248 = _M0Lm2vrS876;
        _M0L6_2atmpS2245 = (int32_t)_M0L6_2atmpS2248;
        _M0L6_2atmpS2247 = (int32_t)_M0L7vrDiv10S913;
        _M0L6_2atmpS2246 = 10 * _M0L6_2atmpS2247;
        _M0L7vrMod10S914 = _M0L6_2atmpS2245 - _M0L6_2atmpS2246;
        if (_M0Lm17vrIsTrailingZerosS881) {
          int32_t _M0L6_2atmpS2243 = _M0Lm16lastRemovedDigitS901;
          _M0Lm17vrIsTrailingZerosS881 = _M0L6_2atmpS2243 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS881 = 0;
        }
        _M0Lm16lastRemovedDigitS901 = _M0L7vrMod10S914;
        _M0Lm2vrS876 = _M0L7vrDiv10S913;
        _M0Lm2vpS877 = _M0L7vpDiv10S912;
        _M0Lm2vmS878 = _M0L7vmDiv10S909;
        _M0L6_2atmpS2244 = _M0Lm7removedS900;
        _M0Lm7removedS900 = _M0L6_2atmpS2244 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS881) {
      int32_t _M0L6_2atmpS2258 = _M0Lm16lastRemovedDigitS901;
      if (_M0L6_2atmpS2258 == 5) {
        uint64_t _M0L6_2atmpS2257 = _M0Lm2vrS876;
        uint64_t _M0L6_2atmpS2256 = _M0L6_2atmpS2257 % 2ull;
        _if__result_3166 = _M0L6_2atmpS2256 == 0ull;
      } else {
        _if__result_3166 = 0;
      }
    } else {
      _if__result_3166 = 0;
    }
    if (_if__result_3166) {
      _M0Lm16lastRemovedDigitS901 = 4;
    }
    _M0L6_2atmpS2259 = _M0Lm2vrS876;
    _M0L6_2atmpS2265 = _M0Lm2vrS876;
    _M0L6_2atmpS2266 = _M0Lm2vmS878;
    if (_M0L6_2atmpS2265 == _M0L6_2atmpS2266) {
      if (!_M0L4evenS873) {
        _if__result_3167 = 1;
      } else {
        int32_t _M0L6_2atmpS2264 = _M0Lm17vmIsTrailingZerosS880;
        _if__result_3167 = !_M0L6_2atmpS2264;
      }
    } else {
      _if__result_3167 = 0;
    }
    if (_if__result_3167) {
      _M0L6_2atmpS2262 = 1;
    } else {
      int32_t _M0L6_2atmpS2263 = _M0Lm16lastRemovedDigitS901;
      _M0L6_2atmpS2262 = _M0L6_2atmpS2263 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2261 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2262);
    _M0L6_2atmpS2260 = *(uint64_t*)&_M0L6_2atmpS2261;
    _M0Lm6outputS902 = _M0L6_2atmpS2259 + _M0L6_2atmpS2260;
  } else {
    int32_t _M0Lm7roundUpS915 = 0;
    uint64_t _M0L6_2atmpS2287 = _M0Lm2vpS877;
    uint64_t _M0L8vpDiv100S916 = _M0L6_2atmpS2287 / 100ull;
    uint64_t _M0L6_2atmpS2286 = _M0Lm2vmS878;
    uint64_t _M0L8vmDiv100S917 = _M0L6_2atmpS2286 / 100ull;
    uint64_t _M0L6_2atmpS2281;
    uint64_t _M0L6_2atmpS2284;
    uint64_t _M0L6_2atmpS2285;
    int32_t _M0L6_2atmpS2283;
    uint64_t _M0L6_2atmpS2282;
    if (_M0L8vpDiv100S916 > _M0L8vmDiv100S917) {
      uint64_t _M0L6_2atmpS2272 = _M0Lm2vrS876;
      uint64_t _M0L8vrDiv100S918 = _M0L6_2atmpS2272 / 100ull;
      uint64_t _M0L6_2atmpS2271 = _M0Lm2vrS876;
      int32_t _M0L6_2atmpS2268 = (int32_t)_M0L6_2atmpS2271;
      int32_t _M0L6_2atmpS2270 = (int32_t)_M0L8vrDiv100S918;
      int32_t _M0L6_2atmpS2269 = 100 * _M0L6_2atmpS2270;
      int32_t _M0L8vrMod100S919 = _M0L6_2atmpS2268 - _M0L6_2atmpS2269;
      int32_t _M0L6_2atmpS2267;
      _M0Lm7roundUpS915 = _M0L8vrMod100S919 >= 50;
      _M0Lm2vrS876 = _M0L8vrDiv100S918;
      _M0Lm2vpS877 = _M0L8vpDiv100S916;
      _M0Lm2vmS878 = _M0L8vmDiv100S917;
      _M0L6_2atmpS2267 = _M0Lm7removedS900;
      _M0Lm7removedS900 = _M0L6_2atmpS2267 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2280 = _M0Lm2vpS877;
      uint64_t _M0L7vpDiv10S920 = _M0L6_2atmpS2280 / 10ull;
      uint64_t _M0L6_2atmpS2279 = _M0Lm2vmS878;
      uint64_t _M0L7vmDiv10S921 = _M0L6_2atmpS2279 / 10ull;
      uint64_t _M0L6_2atmpS2278;
      uint64_t _M0L7vrDiv10S923;
      uint64_t _M0L6_2atmpS2277;
      int32_t _M0L6_2atmpS2274;
      int32_t _M0L6_2atmpS2276;
      int32_t _M0L6_2atmpS2275;
      int32_t _M0L7vrMod10S924;
      int32_t _M0L6_2atmpS2273;
      if (_M0L7vpDiv10S920 <= _M0L7vmDiv10S921) {
        break;
      }
      _M0L6_2atmpS2278 = _M0Lm2vrS876;
      _M0L7vrDiv10S923 = _M0L6_2atmpS2278 / 10ull;
      _M0L6_2atmpS2277 = _M0Lm2vrS876;
      _M0L6_2atmpS2274 = (int32_t)_M0L6_2atmpS2277;
      _M0L6_2atmpS2276 = (int32_t)_M0L7vrDiv10S923;
      _M0L6_2atmpS2275 = 10 * _M0L6_2atmpS2276;
      _M0L7vrMod10S924 = _M0L6_2atmpS2274 - _M0L6_2atmpS2275;
      _M0Lm7roundUpS915 = _M0L7vrMod10S924 >= 5;
      _M0Lm2vrS876 = _M0L7vrDiv10S923;
      _M0Lm2vpS877 = _M0L7vpDiv10S920;
      _M0Lm2vmS878 = _M0L7vmDiv10S921;
      _M0L6_2atmpS2273 = _M0Lm7removedS900;
      _M0Lm7removedS900 = _M0L6_2atmpS2273 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2281 = _M0Lm2vrS876;
    _M0L6_2atmpS2284 = _M0Lm2vrS876;
    _M0L6_2atmpS2285 = _M0Lm2vmS878;
    _M0L6_2atmpS2283
    = _M0L6_2atmpS2284 == _M0L6_2atmpS2285 || _M0Lm7roundUpS915;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2282 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2283);
    _M0Lm6outputS902 = _M0L6_2atmpS2281 + _M0L6_2atmpS2282;
  }
  _M0L6_2atmpS2289 = _M0Lm3e10S879;
  _M0L6_2atmpS2290 = _M0Lm7removedS900;
  _M0L3expS925 = _M0L6_2atmpS2289 + _M0L6_2atmpS2290;
  _M0L6_2atmpS2288 = _M0Lm6outputS902;
  _block_3169
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3169)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3169->$0 = _M0L6_2atmpS2288;
  _block_3169->$1 = _M0L3expS925;
  return _block_3169;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS868) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS868) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS867) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS867) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS866) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS866) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS865) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS865 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS865 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS865 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS865 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS865 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS865 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS865 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS865 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS865 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS865 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS865 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS865 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS865 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS865 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS865 >= 100ull) {
    return 3;
  }
  if (_M0L1vS865 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS848) {
  int32_t _M0L6_2atmpS2189;
  int32_t _M0L6_2atmpS2188;
  int32_t _M0L4baseS847;
  int32_t _M0L5base2S849;
  int32_t _M0L6offsetS850;
  int32_t _M0L6_2atmpS2187;
  uint64_t _M0L4mul0S851;
  int32_t _M0L6_2atmpS2186;
  int32_t _M0L6_2atmpS2185;
  uint64_t _M0L4mul1S852;
  uint64_t _M0L1mS853;
  struct _M0TPB7Umul128 _M0L7_2abindS854;
  uint64_t _M0L7_2alow1S855;
  uint64_t _M0L8_2ahigh1S856;
  struct _M0TPB7Umul128 _M0L7_2abindS857;
  uint64_t _M0L7_2alow0S858;
  uint64_t _M0L8_2ahigh0S859;
  uint64_t _M0L3sumS860;
  uint64_t _M0Lm5high1S861;
  int32_t _M0L6_2atmpS2183;
  int32_t _M0L6_2atmpS2184;
  int32_t _M0L5deltaS862;
  uint64_t _M0L6_2atmpS2182;
  uint64_t _M0L6_2atmpS2174;
  int32_t _M0L6_2atmpS2181;
  uint32_t _M0L6_2atmpS2178;
  int32_t _M0L6_2atmpS2180;
  int32_t _M0L6_2atmpS2179;
  uint32_t _M0L6_2atmpS2177;
  uint32_t _M0L6_2atmpS2176;
  uint64_t _M0L6_2atmpS2175;
  uint64_t _M0L1aS863;
  uint64_t _M0L6_2atmpS2173;
  uint64_t _M0L1bS864;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2189 = _M0L1iS848 + 26;
  _M0L6_2atmpS2188 = _M0L6_2atmpS2189 - 1;
  _M0L4baseS847 = _M0L6_2atmpS2188 / 26;
  _M0L5base2S849 = _M0L4baseS847 * 26;
  _M0L6offsetS850 = _M0L5base2S849 - _M0L1iS848;
  _M0L6_2atmpS2187 = _M0L4baseS847 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S851
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2187);
  _M0L6_2atmpS2186 = _M0L4baseS847 * 2;
  _M0L6_2atmpS2185 = _M0L6_2atmpS2186 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S852
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2185);
  if (_M0L6offsetS850 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S851, _M0L4mul1S852};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS853
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS850);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS854 = _M0FPB7umul128(_M0L1mS853, _M0L4mul1S852);
  _M0L7_2alow1S855 = _M0L7_2abindS854.$0;
  _M0L8_2ahigh1S856 = _M0L7_2abindS854.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS857 = _M0FPB7umul128(_M0L1mS853, _M0L4mul0S851);
  _M0L7_2alow0S858 = _M0L7_2abindS857.$0;
  _M0L8_2ahigh0S859 = _M0L7_2abindS857.$1;
  _M0L3sumS860 = _M0L8_2ahigh0S859 + _M0L7_2alow1S855;
  _M0Lm5high1S861 = _M0L8_2ahigh1S856;
  if (_M0L3sumS860 < _M0L8_2ahigh0S859) {
    uint64_t _M0L6_2atmpS2172 = _M0Lm5high1S861;
    _M0Lm5high1S861 = _M0L6_2atmpS2172 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2183 = _M0FPB8pow5bits(_M0L5base2S849);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2184 = _M0FPB8pow5bits(_M0L1iS848);
  _M0L5deltaS862 = _M0L6_2atmpS2183 - _M0L6_2atmpS2184;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2182
  = _M0FPB13shiftright128(_M0L7_2alow0S858, _M0L3sumS860, _M0L5deltaS862);
  _M0L6_2atmpS2174 = _M0L6_2atmpS2182 + 1ull;
  _M0L6_2atmpS2181 = _M0L1iS848 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2178
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2181);
  _M0L6_2atmpS2180 = _M0L1iS848 % 16;
  _M0L6_2atmpS2179 = _M0L6_2atmpS2180 << 1;
  _M0L6_2atmpS2177 = _M0L6_2atmpS2178 >> (_M0L6_2atmpS2179 & 31);
  _M0L6_2atmpS2176 = _M0L6_2atmpS2177 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2175 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2176);
  _M0L1aS863 = _M0L6_2atmpS2174 + _M0L6_2atmpS2175;
  _M0L6_2atmpS2173 = _M0Lm5high1S861;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS864
  = _M0FPB13shiftright128(_M0L3sumS860, _M0L6_2atmpS2173, _M0L5deltaS862);
  return (struct _M0TPB8Pow5Pair){_M0L1aS863, _M0L1bS864};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS830) {
  int32_t _M0L4baseS829;
  int32_t _M0L5base2S831;
  int32_t _M0L6offsetS832;
  int32_t _M0L6_2atmpS2171;
  uint64_t _M0L4mul0S833;
  int32_t _M0L6_2atmpS2170;
  int32_t _M0L6_2atmpS2169;
  uint64_t _M0L4mul1S834;
  uint64_t _M0L1mS835;
  struct _M0TPB7Umul128 _M0L7_2abindS836;
  uint64_t _M0L7_2alow1S837;
  uint64_t _M0L8_2ahigh1S838;
  struct _M0TPB7Umul128 _M0L7_2abindS839;
  uint64_t _M0L7_2alow0S840;
  uint64_t _M0L8_2ahigh0S841;
  uint64_t _M0L3sumS842;
  uint64_t _M0Lm5high1S843;
  int32_t _M0L6_2atmpS2167;
  int32_t _M0L6_2atmpS2168;
  int32_t _M0L5deltaS844;
  uint64_t _M0L6_2atmpS2159;
  int32_t _M0L6_2atmpS2166;
  uint32_t _M0L6_2atmpS2163;
  int32_t _M0L6_2atmpS2165;
  int32_t _M0L6_2atmpS2164;
  uint32_t _M0L6_2atmpS2162;
  uint32_t _M0L6_2atmpS2161;
  uint64_t _M0L6_2atmpS2160;
  uint64_t _M0L1aS845;
  uint64_t _M0L6_2atmpS2158;
  uint64_t _M0L1bS846;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS829 = _M0L1iS830 / 26;
  _M0L5base2S831 = _M0L4baseS829 * 26;
  _M0L6offsetS832 = _M0L1iS830 - _M0L5base2S831;
  _M0L6_2atmpS2171 = _M0L4baseS829 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S833
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2171);
  _M0L6_2atmpS2170 = _M0L4baseS829 * 2;
  _M0L6_2atmpS2169 = _M0L6_2atmpS2170 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S834
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2169);
  if (_M0L6offsetS832 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S833, _M0L4mul1S834};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS835
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS832);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS836 = _M0FPB7umul128(_M0L1mS835, _M0L4mul1S834);
  _M0L7_2alow1S837 = _M0L7_2abindS836.$0;
  _M0L8_2ahigh1S838 = _M0L7_2abindS836.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS839 = _M0FPB7umul128(_M0L1mS835, _M0L4mul0S833);
  _M0L7_2alow0S840 = _M0L7_2abindS839.$0;
  _M0L8_2ahigh0S841 = _M0L7_2abindS839.$1;
  _M0L3sumS842 = _M0L8_2ahigh0S841 + _M0L7_2alow1S837;
  _M0Lm5high1S843 = _M0L8_2ahigh1S838;
  if (_M0L3sumS842 < _M0L8_2ahigh0S841) {
    uint64_t _M0L6_2atmpS2157 = _M0Lm5high1S843;
    _M0Lm5high1S843 = _M0L6_2atmpS2157 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2167 = _M0FPB8pow5bits(_M0L1iS830);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2168 = _M0FPB8pow5bits(_M0L5base2S831);
  _M0L5deltaS844 = _M0L6_2atmpS2167 - _M0L6_2atmpS2168;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2159
  = _M0FPB13shiftright128(_M0L7_2alow0S840, _M0L3sumS842, _M0L5deltaS844);
  _M0L6_2atmpS2166 = _M0L1iS830 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2163
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2166);
  _M0L6_2atmpS2165 = _M0L1iS830 % 16;
  _M0L6_2atmpS2164 = _M0L6_2atmpS2165 << 1;
  _M0L6_2atmpS2162 = _M0L6_2atmpS2163 >> (_M0L6_2atmpS2164 & 31);
  _M0L6_2atmpS2161 = _M0L6_2atmpS2162 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2160 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2161);
  _M0L1aS845 = _M0L6_2atmpS2159 + _M0L6_2atmpS2160;
  _M0L6_2atmpS2158 = _M0Lm5high1S843;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS846
  = _M0FPB13shiftright128(_M0L3sumS842, _M0L6_2atmpS2158, _M0L5deltaS844);
  return (struct _M0TPB8Pow5Pair){_M0L1aS845, _M0L1bS846};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS803,
  struct _M0TPB8Pow5Pair _M0L3mulS800,
  int32_t _M0L1jS816,
  int32_t _M0L7mmShiftS818
) {
  uint64_t _M0L7_2amul0S799;
  uint64_t _M0L7_2amul1S801;
  uint64_t _M0L1mS802;
  struct _M0TPB7Umul128 _M0L7_2abindS804;
  uint64_t _M0L5_2aloS805;
  uint64_t _M0L6_2atmpS806;
  struct _M0TPB7Umul128 _M0L7_2abindS807;
  uint64_t _M0L6_2alo2S808;
  uint64_t _M0L6_2ahi2S809;
  uint64_t _M0L3midS810;
  uint64_t _M0L6_2atmpS2156;
  uint64_t _M0L2hiS811;
  uint64_t _M0L3lo2S812;
  uint64_t _M0L6_2atmpS2154;
  uint64_t _M0L6_2atmpS2155;
  uint64_t _M0L4mid2S813;
  uint64_t _M0L6_2atmpS2153;
  uint64_t _M0L3hi2S814;
  int32_t _M0L6_2atmpS2152;
  int32_t _M0L6_2atmpS2151;
  uint64_t _M0L2vpS815;
  uint64_t _M0Lm2vmS817;
  int32_t _M0L6_2atmpS2150;
  int32_t _M0L6_2atmpS2149;
  uint64_t _M0L2vrS828;
  uint64_t _M0L6_2atmpS2148;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S799 = _M0L3mulS800.$0;
  _M0L7_2amul1S801 = _M0L3mulS800.$1;
  _M0L1mS802 = _M0L1mS803 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS804 = _M0FPB7umul128(_M0L1mS802, _M0L7_2amul0S799);
  _M0L5_2aloS805 = _M0L7_2abindS804.$0;
  _M0L6_2atmpS806 = _M0L7_2abindS804.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS807 = _M0FPB7umul128(_M0L1mS802, _M0L7_2amul1S801);
  _M0L6_2alo2S808 = _M0L7_2abindS807.$0;
  _M0L6_2ahi2S809 = _M0L7_2abindS807.$1;
  _M0L3midS810 = _M0L6_2atmpS806 + _M0L6_2alo2S808;
  if (_M0L3midS810 < _M0L6_2atmpS806) {
    _M0L6_2atmpS2156 = 1ull;
  } else {
    _M0L6_2atmpS2156 = 0ull;
  }
  _M0L2hiS811 = _M0L6_2ahi2S809 + _M0L6_2atmpS2156;
  _M0L3lo2S812 = _M0L5_2aloS805 + _M0L7_2amul0S799;
  _M0L6_2atmpS2154 = _M0L3midS810 + _M0L7_2amul1S801;
  if (_M0L3lo2S812 < _M0L5_2aloS805) {
    _M0L6_2atmpS2155 = 1ull;
  } else {
    _M0L6_2atmpS2155 = 0ull;
  }
  _M0L4mid2S813 = _M0L6_2atmpS2154 + _M0L6_2atmpS2155;
  if (_M0L4mid2S813 < _M0L3midS810) {
    _M0L6_2atmpS2153 = 1ull;
  } else {
    _M0L6_2atmpS2153 = 0ull;
  }
  _M0L3hi2S814 = _M0L2hiS811 + _M0L6_2atmpS2153;
  _M0L6_2atmpS2152 = _M0L1jS816 - 64;
  _M0L6_2atmpS2151 = _M0L6_2atmpS2152 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS815
  = _M0FPB13shiftright128(_M0L4mid2S813, _M0L3hi2S814, _M0L6_2atmpS2151);
  _M0Lm2vmS817 = 0ull;
  if (_M0L7mmShiftS818) {
    uint64_t _M0L3lo3S819 = _M0L5_2aloS805 - _M0L7_2amul0S799;
    uint64_t _M0L6_2atmpS2138 = _M0L3midS810 - _M0L7_2amul1S801;
    uint64_t _M0L6_2atmpS2139;
    uint64_t _M0L4mid3S820;
    uint64_t _M0L6_2atmpS2137;
    uint64_t _M0L3hi3S821;
    int32_t _M0L6_2atmpS2136;
    int32_t _M0L6_2atmpS2135;
    if (_M0L5_2aloS805 < _M0L3lo3S819) {
      _M0L6_2atmpS2139 = 1ull;
    } else {
      _M0L6_2atmpS2139 = 0ull;
    }
    _M0L4mid3S820 = _M0L6_2atmpS2138 - _M0L6_2atmpS2139;
    if (_M0L3midS810 < _M0L4mid3S820) {
      _M0L6_2atmpS2137 = 1ull;
    } else {
      _M0L6_2atmpS2137 = 0ull;
    }
    _M0L3hi3S821 = _M0L2hiS811 - _M0L6_2atmpS2137;
    _M0L6_2atmpS2136 = _M0L1jS816 - 64;
    _M0L6_2atmpS2135 = _M0L6_2atmpS2136 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS817
    = _M0FPB13shiftright128(_M0L4mid3S820, _M0L3hi3S821, _M0L6_2atmpS2135);
  } else {
    uint64_t _M0L3lo3S822 = _M0L5_2aloS805 + _M0L5_2aloS805;
    uint64_t _M0L6_2atmpS2146 = _M0L3midS810 + _M0L3midS810;
    uint64_t _M0L6_2atmpS2147;
    uint64_t _M0L4mid3S823;
    uint64_t _M0L6_2atmpS2144;
    uint64_t _M0L6_2atmpS2145;
    uint64_t _M0L3hi3S824;
    uint64_t _M0L3lo4S825;
    uint64_t _M0L6_2atmpS2142;
    uint64_t _M0L6_2atmpS2143;
    uint64_t _M0L4mid4S826;
    uint64_t _M0L6_2atmpS2141;
    uint64_t _M0L3hi4S827;
    int32_t _M0L6_2atmpS2140;
    if (_M0L3lo3S822 < _M0L5_2aloS805) {
      _M0L6_2atmpS2147 = 1ull;
    } else {
      _M0L6_2atmpS2147 = 0ull;
    }
    _M0L4mid3S823 = _M0L6_2atmpS2146 + _M0L6_2atmpS2147;
    _M0L6_2atmpS2144 = _M0L2hiS811 + _M0L2hiS811;
    if (_M0L4mid3S823 < _M0L3midS810) {
      _M0L6_2atmpS2145 = 1ull;
    } else {
      _M0L6_2atmpS2145 = 0ull;
    }
    _M0L3hi3S824 = _M0L6_2atmpS2144 + _M0L6_2atmpS2145;
    _M0L3lo4S825 = _M0L3lo3S822 - _M0L7_2amul0S799;
    _M0L6_2atmpS2142 = _M0L4mid3S823 - _M0L7_2amul1S801;
    if (_M0L3lo3S822 < _M0L3lo4S825) {
      _M0L6_2atmpS2143 = 1ull;
    } else {
      _M0L6_2atmpS2143 = 0ull;
    }
    _M0L4mid4S826 = _M0L6_2atmpS2142 - _M0L6_2atmpS2143;
    if (_M0L4mid3S823 < _M0L4mid4S826) {
      _M0L6_2atmpS2141 = 1ull;
    } else {
      _M0L6_2atmpS2141 = 0ull;
    }
    _M0L3hi4S827 = _M0L3hi3S824 - _M0L6_2atmpS2141;
    _M0L6_2atmpS2140 = _M0L1jS816 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS817
    = _M0FPB13shiftright128(_M0L4mid4S826, _M0L3hi4S827, _M0L6_2atmpS2140);
  }
  _M0L6_2atmpS2150 = _M0L1jS816 - 64;
  _M0L6_2atmpS2149 = _M0L6_2atmpS2150 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS828
  = _M0FPB13shiftright128(_M0L3midS810, _M0L2hiS811, _M0L6_2atmpS2149);
  _M0L6_2atmpS2148 = _M0Lm2vmS817;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS828,
                                                _M0L2vpS815,
                                                _M0L6_2atmpS2148};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS797,
  int32_t _M0L1pS798
) {
  uint64_t _M0L6_2atmpS2134;
  uint64_t _M0L6_2atmpS2133;
  uint64_t _M0L6_2atmpS2132;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2134 = 1ull << (_M0L1pS798 & 63);
  _M0L6_2atmpS2133 = _M0L6_2atmpS2134 - 1ull;
  _M0L6_2atmpS2132 = _M0L5valueS797 & _M0L6_2atmpS2133;
  return _M0L6_2atmpS2132 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS795,
  int32_t _M0L1pS796
) {
  int32_t _M0L6_2atmpS2131;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2131 = _M0FPB10pow5Factor(_M0L5valueS795);
  return _M0L6_2atmpS2131 >= _M0L1pS796;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS791) {
  uint64_t _M0L6_2atmpS2119;
  uint64_t _M0L6_2atmpS2120;
  uint64_t _M0L6_2atmpS2121;
  uint64_t _M0L6_2atmpS2122;
  int32_t _M0Lm5countS792;
  uint64_t _M0Lm5valueS793;
  uint64_t _M0L6_2atmpS2130;
  moonbit_string_t _M0L6_2atmpS2129;
  moonbit_string_t _M0L6_2atmpS2819;
  moonbit_string_t _M0L6_2atmpS2128;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2119 = _M0L5valueS791 % 5ull;
  if (_M0L6_2atmpS2119 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2120 = _M0L5valueS791 % 25ull;
  if (_M0L6_2atmpS2120 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2121 = _M0L5valueS791 % 125ull;
  if (_M0L6_2atmpS2121 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2122 = _M0L5valueS791 % 625ull;
  if (_M0L6_2atmpS2122 != 0ull) {
    return 3;
  }
  _M0Lm5countS792 = 4;
  _M0Lm5valueS793 = _M0L5valueS791 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2123 = _M0Lm5valueS793;
    if (_M0L6_2atmpS2123 > 0ull) {
      uint64_t _M0L6_2atmpS2125 = _M0Lm5valueS793;
      uint64_t _M0L6_2atmpS2124 = _M0L6_2atmpS2125 % 5ull;
      uint64_t _M0L6_2atmpS2126;
      int32_t _M0L6_2atmpS2127;
      if (_M0L6_2atmpS2124 != 0ull) {
        return _M0Lm5countS792;
      }
      _M0L6_2atmpS2126 = _M0Lm5valueS793;
      _M0Lm5valueS793 = _M0L6_2atmpS2126 / 5ull;
      _M0L6_2atmpS2127 = _M0Lm5countS792;
      _M0Lm5countS792 = _M0L6_2atmpS2127 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2130 = _M0Lm5valueS793;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2129
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2130);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2819
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS2129);
  moonbit_decref(_M0L6_2atmpS2129);
  _M0L6_2atmpS2128 = _M0L6_2atmpS2819;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2128, (moonbit_string_t)moonbit_string_literal_48.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS790,
  uint64_t _M0L2hiS788,
  int32_t _M0L4distS789
) {
  int32_t _M0L6_2atmpS2118;
  uint64_t _M0L6_2atmpS2116;
  uint64_t _M0L6_2atmpS2117;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2118 = 64 - _M0L4distS789;
  _M0L6_2atmpS2116 = _M0L2hiS788 << (_M0L6_2atmpS2118 & 63);
  _M0L6_2atmpS2117 = _M0L2loS790 >> (_M0L4distS789 & 63);
  return _M0L6_2atmpS2116 | _M0L6_2atmpS2117;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS778,
  uint64_t _M0L1bS781
) {
  uint64_t _M0L3aLoS777;
  uint64_t _M0L3aHiS779;
  uint64_t _M0L3bLoS780;
  uint64_t _M0L3bHiS782;
  uint64_t _M0L1xS783;
  uint64_t _M0L6_2atmpS2114;
  uint64_t _M0L6_2atmpS2115;
  uint64_t _M0L1yS784;
  uint64_t _M0L6_2atmpS2112;
  uint64_t _M0L6_2atmpS2113;
  uint64_t _M0L1zS785;
  uint64_t _M0L6_2atmpS2110;
  uint64_t _M0L6_2atmpS2111;
  uint64_t _M0L6_2atmpS2108;
  uint64_t _M0L6_2atmpS2109;
  uint64_t _M0L1wS786;
  uint64_t _M0L2loS787;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS777 = _M0L1aS778 & 4294967295ull;
  _M0L3aHiS779 = _M0L1aS778 >> 32;
  _M0L3bLoS780 = _M0L1bS781 & 4294967295ull;
  _M0L3bHiS782 = _M0L1bS781 >> 32;
  _M0L1xS783 = _M0L3aLoS777 * _M0L3bLoS780;
  _M0L6_2atmpS2114 = _M0L3aHiS779 * _M0L3bLoS780;
  _M0L6_2atmpS2115 = _M0L1xS783 >> 32;
  _M0L1yS784 = _M0L6_2atmpS2114 + _M0L6_2atmpS2115;
  _M0L6_2atmpS2112 = _M0L3aLoS777 * _M0L3bHiS782;
  _M0L6_2atmpS2113 = _M0L1yS784 & 4294967295ull;
  _M0L1zS785 = _M0L6_2atmpS2112 + _M0L6_2atmpS2113;
  _M0L6_2atmpS2110 = _M0L3aHiS779 * _M0L3bHiS782;
  _M0L6_2atmpS2111 = _M0L1yS784 >> 32;
  _M0L6_2atmpS2108 = _M0L6_2atmpS2110 + _M0L6_2atmpS2111;
  _M0L6_2atmpS2109 = _M0L1zS785 >> 32;
  _M0L1wS786 = _M0L6_2atmpS2108 + _M0L6_2atmpS2109;
  _M0L2loS787 = _M0L1aS778 * _M0L1bS781;
  return (struct _M0TPB7Umul128){_M0L2loS787, _M0L1wS786};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS772,
  int32_t _M0L4fromS776,
  int32_t _M0L2toS774
) {
  int32_t _M0L6_2atmpS2107;
  struct _M0TPB13StringBuilder* _M0L3bufS771;
  int32_t _M0L1iS773;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2107 = Moonbit_array_length(_M0L5bytesS772);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS771 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2107);
  _M0L1iS773 = _M0L4fromS776;
  while (1) {
    if (_M0L1iS773 < _M0L2toS774) {
      int32_t _M0L6_2atmpS2105;
      int32_t _M0L6_2atmpS2104;
      int32_t _M0L6_2atmpS2106;
      if (
        _M0L1iS773 < 0 || _M0L1iS773 >= Moonbit_array_length(_M0L5bytesS772)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2105 = (int32_t)_M0L5bytesS772[_M0L1iS773];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2104 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2105);
      moonbit_incref(_M0L3bufS771);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS771, _M0L6_2atmpS2104);
      _M0L6_2atmpS2106 = _M0L1iS773 + 1;
      _M0L1iS773 = _M0L6_2atmpS2106;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS772);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS771);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS770) {
  int32_t _M0L6_2atmpS2103;
  uint32_t _M0L6_2atmpS2102;
  uint32_t _M0L6_2atmpS2101;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2103 = _M0L1eS770 * 78913;
  _M0L6_2atmpS2102 = *(uint32_t*)&_M0L6_2atmpS2103;
  _M0L6_2atmpS2101 = _M0L6_2atmpS2102 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2101;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS769) {
  int32_t _M0L6_2atmpS2100;
  uint32_t _M0L6_2atmpS2099;
  uint32_t _M0L6_2atmpS2098;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2100 = _M0L1eS769 * 732923;
  _M0L6_2atmpS2099 = *(uint32_t*)&_M0L6_2atmpS2100;
  _M0L6_2atmpS2098 = _M0L6_2atmpS2099 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2098;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS767,
  int32_t _M0L8exponentS768,
  int32_t _M0L8mantissaS765
) {
  moonbit_string_t _M0L1sS766;
  moonbit_string_t _M0L6_2atmpS2820;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS765) {
    return (moonbit_string_t)moonbit_string_literal_49.data;
  }
  if (_M0L4signS767) {
    _M0L1sS766 = (moonbit_string_t)moonbit_string_literal_50.data;
  } else {
    _M0L1sS766 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS768) {
    moonbit_string_t _M0L6_2atmpS2821;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2821
    = moonbit_add_string(_M0L1sS766, (moonbit_string_t)moonbit_string_literal_51.data);
    moonbit_decref(_M0L1sS766);
    return _M0L6_2atmpS2821;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2820
  = moonbit_add_string(_M0L1sS766, (moonbit_string_t)moonbit_string_literal_52.data);
  moonbit_decref(_M0L1sS766);
  return _M0L6_2atmpS2820;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS764) {
  int32_t _M0L6_2atmpS2097;
  uint32_t _M0L6_2atmpS2096;
  uint32_t _M0L6_2atmpS2095;
  int32_t _M0L6_2atmpS2094;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2097 = _M0L1eS764 * 1217359;
  _M0L6_2atmpS2096 = *(uint32_t*)&_M0L6_2atmpS2097;
  _M0L6_2atmpS2095 = _M0L6_2atmpS2096 >> 19;
  _M0L6_2atmpS2094 = *(int32_t*)&_M0L6_2atmpS2095;
  return _M0L6_2atmpS2094 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS763,
  struct _M0TPB6Hasher* _M0L6hasherS762
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS762, _M0L4selfS763);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS761,
  struct _M0TPB6Hasher* _M0L6hasherS760
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS760, _M0L4selfS761);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS758,
  moonbit_string_t _M0L5valueS756
) {
  int32_t _M0L7_2abindS755;
  int32_t _M0L1iS757;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS755 = Moonbit_array_length(_M0L5valueS756);
  _M0L1iS757 = 0;
  while (1) {
    if (_M0L1iS757 < _M0L7_2abindS755) {
      int32_t _M0L6_2atmpS2092 = _M0L5valueS756[_M0L1iS757];
      int32_t _M0L6_2atmpS2091 = (int32_t)_M0L6_2atmpS2092;
      uint32_t _M0L6_2atmpS2090 = *(uint32_t*)&_M0L6_2atmpS2091;
      int32_t _M0L6_2atmpS2093;
      moonbit_incref(_M0L4selfS758);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS758, _M0L6_2atmpS2090);
      _M0L6_2atmpS2093 = _M0L1iS757 + 1;
      _M0L1iS757 = _M0L6_2atmpS2093;
      continue;
    } else {
      moonbit_decref(_M0L4selfS758);
      moonbit_decref(_M0L5valueS756);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS753,
  int32_t _M0L3idxS754
) {
  int32_t _M0L6_2atmpS2822;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2822 = _M0L4selfS753[_M0L3idxS754];
  moonbit_decref(_M0L4selfS753);
  return _M0L6_2atmpS2822;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS752) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS752;
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS751) {
  void* _block_3173;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3173 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3173)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3173)->$0 = _M0L6objectS751;
  return _block_3173;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS750) {
  void* _block_3174;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3174 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3174)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3174)->$0 = _M0L6stringS750;
  return _block_3174;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS743
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2823;
  int32_t _M0L6_2acntS3057;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2089;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS742;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__* _closure_3175;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2084;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2823 = _M0L4selfS743->$5;
  _M0L6_2acntS3057 = Moonbit_object_header(_M0L4selfS743)->rc;
  if (_M0L6_2acntS3057 > 1) {
    int32_t _M0L11_2anew__cntS3059 = _M0L6_2acntS3057 - 1;
    Moonbit_object_header(_M0L4selfS743)->rc = _M0L11_2anew__cntS3059;
    if (_M0L8_2afieldS2823) {
      moonbit_incref(_M0L8_2afieldS2823);
    }
  } else if (_M0L6_2acntS3057 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3058 = _M0L4selfS743->$0;
    moonbit_decref(_M0L8_2afieldS3058);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS743);
  }
  _M0L4headS2089 = _M0L8_2afieldS2823;
  _M0L11curr__entryS742
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS742)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS742->$0 = _M0L4headS2089;
  _closure_3175
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__));
  Moonbit_object_header(_closure_3175)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__, $0) >> 2, 1, 0);
  _closure_3175->code = &_M0MPB3Map4iterGsRPB4JsonEC2085l591;
  _closure_3175->$0 = _M0L11curr__entryS742;
  _M0L6_2atmpS2084 = (struct _M0TWEOUsRPB4JsonE*)_closure_3175;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2084);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2085l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2086
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__* _M0L14_2acasted__envS2087;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS2829;
  int32_t _M0L6_2acntS3060;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS742;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2828;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS744;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2087
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2085__l591__*)_M0L6_2aenvS2086;
  _M0L8_2afieldS2829 = _M0L14_2acasted__envS2087->$0;
  _M0L6_2acntS3060 = Moonbit_object_header(_M0L14_2acasted__envS2087)->rc;
  if (_M0L6_2acntS3060 > 1) {
    int32_t _M0L11_2anew__cntS3061 = _M0L6_2acntS3060 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2087)->rc
    = _M0L11_2anew__cntS3061;
    moonbit_incref(_M0L8_2afieldS2829);
  } else if (_M0L6_2acntS3060 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2087);
  }
  _M0L11curr__entryS742 = _M0L8_2afieldS2829;
  _M0L8_2afieldS2828 = _M0L11curr__entryS742->$0;
  _M0L7_2abindS744 = _M0L8_2afieldS2828;
  if (_M0L7_2abindS744 == 0) {
    moonbit_decref(_M0L11curr__entryS742);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS745 = _M0L7_2abindS744;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS746 = _M0L7_2aSomeS745;
    moonbit_string_t _M0L8_2afieldS2827 = _M0L4_2axS746->$4;
    moonbit_string_t _M0L6_2akeyS747 = _M0L8_2afieldS2827;
    void* _M0L8_2afieldS2826 = _M0L4_2axS746->$5;
    void* _M0L8_2avalueS748 = _M0L8_2afieldS2826;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2825 = _M0L4_2axS746->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS749 = _M0L8_2afieldS2825;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2824 =
      _M0L11curr__entryS742->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2088;
    if (_M0L7_2anextS749) {
      moonbit_incref(_M0L7_2anextS749);
    }
    moonbit_incref(_M0L8_2avalueS748);
    moonbit_incref(_M0L6_2akeyS747);
    if (_M0L6_2aoldS2824) {
      moonbit_decref(_M0L6_2aoldS2824);
    }
    _M0L11curr__entryS742->$0 = _M0L7_2anextS749;
    moonbit_decref(_M0L11curr__entryS742);
    _M0L8_2atupleS2088
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2088)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2088->$0 = _M0L6_2akeyS747;
    _M0L8_2atupleS2088->$1 = _M0L8_2avalueS748;
    return _M0L8_2atupleS2088;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS741
) {
  int32_t _M0L8_2afieldS2830;
  int32_t _M0L4sizeS2083;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2830 = _M0L4selfS741->$1;
  moonbit_decref(_M0L4selfS741);
  _M0L4sizeS2083 = _M0L8_2afieldS2830;
  return _M0L4sizeS2083 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS728,
  int32_t _M0L3keyS724
) {
  int32_t _M0L4hashS723;
  int32_t _M0L14capacity__maskS2068;
  int32_t _M0L6_2atmpS2067;
  int32_t _M0L1iS725;
  int32_t _M0L3idxS726;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS723 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS724);
  _M0L14capacity__maskS2068 = _M0L4selfS728->$3;
  _M0L6_2atmpS2067 = _M0L4hashS723 & _M0L14capacity__maskS2068;
  _M0L1iS725 = 0;
  _M0L3idxS726 = _M0L6_2atmpS2067;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2834 =
      _M0L4selfS728->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2066 =
      _M0L8_2afieldS2834;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2833;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS727;
    if (
      _M0L3idxS726 < 0
      || _M0L3idxS726 >= Moonbit_array_length(_M0L7entriesS2066)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2833
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2066[
        _M0L3idxS726
      ];
    _M0L7_2abindS727 = _M0L6_2atmpS2833;
    if (_M0L7_2abindS727 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2055;
      if (_M0L7_2abindS727) {
        moonbit_incref(_M0L7_2abindS727);
      }
      moonbit_decref(_M0L4selfS728);
      if (_M0L7_2abindS727) {
        moonbit_decref(_M0L7_2abindS727);
      }
      _M0L6_2atmpS2055 = 0;
      return _M0L6_2atmpS2055;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS729 =
        _M0L7_2abindS727;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS730 =
        _M0L7_2aSomeS729;
      int32_t _M0L4hashS2057 = _M0L8_2aentryS730->$3;
      int32_t _if__result_3177;
      int32_t _M0L8_2afieldS2831;
      int32_t _M0L3pslS2060;
      int32_t _M0L6_2atmpS2062;
      int32_t _M0L6_2atmpS2064;
      int32_t _M0L14capacity__maskS2065;
      int32_t _M0L6_2atmpS2063;
      if (_M0L4hashS2057 == _M0L4hashS723) {
        int32_t _M0L3keyS2056 = _M0L8_2aentryS730->$4;
        _if__result_3177 = _M0L3keyS2056 == _M0L3keyS724;
      } else {
        _if__result_3177 = 0;
      }
      if (_if__result_3177) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2832;
        int32_t _M0L6_2acntS3062;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2059;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2058;
        moonbit_incref(_M0L8_2aentryS730);
        moonbit_decref(_M0L4selfS728);
        _M0L8_2afieldS2832 = _M0L8_2aentryS730->$5;
        _M0L6_2acntS3062 = Moonbit_object_header(_M0L8_2aentryS730)->rc;
        if (_M0L6_2acntS3062 > 1) {
          int32_t _M0L11_2anew__cntS3064 = _M0L6_2acntS3062 - 1;
          Moonbit_object_header(_M0L8_2aentryS730)->rc
          = _M0L11_2anew__cntS3064;
          moonbit_incref(_M0L8_2afieldS2832);
        } else if (_M0L6_2acntS3062 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3063 =
            _M0L8_2aentryS730->$1;
          if (_M0L8_2afieldS3063) {
            moonbit_decref(_M0L8_2afieldS3063);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS730);
        }
        _M0L5valueS2059 = _M0L8_2afieldS2832;
        _M0L6_2atmpS2058 = _M0L5valueS2059;
        return _M0L6_2atmpS2058;
      } else {
        moonbit_incref(_M0L8_2aentryS730);
      }
      _M0L8_2afieldS2831 = _M0L8_2aentryS730->$2;
      moonbit_decref(_M0L8_2aentryS730);
      _M0L3pslS2060 = _M0L8_2afieldS2831;
      if (_M0L1iS725 > _M0L3pslS2060) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2061;
        moonbit_decref(_M0L4selfS728);
        _M0L6_2atmpS2061 = 0;
        return _M0L6_2atmpS2061;
      }
      _M0L6_2atmpS2062 = _M0L1iS725 + 1;
      _M0L6_2atmpS2064 = _M0L3idxS726 + 1;
      _M0L14capacity__maskS2065 = _M0L4selfS728->$3;
      _M0L6_2atmpS2063 = _M0L6_2atmpS2064 & _M0L14capacity__maskS2065;
      _M0L1iS725 = _M0L6_2atmpS2062;
      _M0L3idxS726 = _M0L6_2atmpS2063;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS737,
  moonbit_string_t _M0L3keyS733
) {
  int32_t _M0L4hashS732;
  int32_t _M0L14capacity__maskS2082;
  int32_t _M0L6_2atmpS2081;
  int32_t _M0L1iS734;
  int32_t _M0L3idxS735;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS733);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS732 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS733);
  _M0L14capacity__maskS2082 = _M0L4selfS737->$3;
  _M0L6_2atmpS2081 = _M0L4hashS732 & _M0L14capacity__maskS2082;
  _M0L1iS734 = 0;
  _M0L3idxS735 = _M0L6_2atmpS2081;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2840 =
      _M0L4selfS737->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2080 =
      _M0L8_2afieldS2840;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2839;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS736;
    if (
      _M0L3idxS735 < 0
      || _M0L3idxS735 >= Moonbit_array_length(_M0L7entriesS2080)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2839
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2080[
        _M0L3idxS735
      ];
    _M0L7_2abindS736 = _M0L6_2atmpS2839;
    if (_M0L7_2abindS736 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2069;
      if (_M0L7_2abindS736) {
        moonbit_incref(_M0L7_2abindS736);
      }
      moonbit_decref(_M0L4selfS737);
      if (_M0L7_2abindS736) {
        moonbit_decref(_M0L7_2abindS736);
      }
      moonbit_decref(_M0L3keyS733);
      _M0L6_2atmpS2069 = 0;
      return _M0L6_2atmpS2069;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS738 =
        _M0L7_2abindS736;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS739 =
        _M0L7_2aSomeS738;
      int32_t _M0L4hashS2071 = _M0L8_2aentryS739->$3;
      int32_t _if__result_3179;
      int32_t _M0L8_2afieldS2835;
      int32_t _M0L3pslS2074;
      int32_t _M0L6_2atmpS2076;
      int32_t _M0L6_2atmpS2078;
      int32_t _M0L14capacity__maskS2079;
      int32_t _M0L6_2atmpS2077;
      if (_M0L4hashS2071 == _M0L4hashS732) {
        moonbit_string_t _M0L8_2afieldS2838 = _M0L8_2aentryS739->$4;
        moonbit_string_t _M0L3keyS2070 = _M0L8_2afieldS2838;
        int32_t _M0L6_2atmpS2837;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2837
        = moonbit_val_array_equal(_M0L3keyS2070, _M0L3keyS733);
        _if__result_3179 = _M0L6_2atmpS2837;
      } else {
        _if__result_3179 = 0;
      }
      if (_if__result_3179) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2836;
        int32_t _M0L6_2acntS3065;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2073;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2072;
        moonbit_incref(_M0L8_2aentryS739);
        moonbit_decref(_M0L4selfS737);
        moonbit_decref(_M0L3keyS733);
        _M0L8_2afieldS2836 = _M0L8_2aentryS739->$5;
        _M0L6_2acntS3065 = Moonbit_object_header(_M0L8_2aentryS739)->rc;
        if (_M0L6_2acntS3065 > 1) {
          int32_t _M0L11_2anew__cntS3068 = _M0L6_2acntS3065 - 1;
          Moonbit_object_header(_M0L8_2aentryS739)->rc
          = _M0L11_2anew__cntS3068;
          moonbit_incref(_M0L8_2afieldS2836);
        } else if (_M0L6_2acntS3065 == 1) {
          moonbit_string_t _M0L8_2afieldS3067 = _M0L8_2aentryS739->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3066;
          moonbit_decref(_M0L8_2afieldS3067);
          _M0L8_2afieldS3066 = _M0L8_2aentryS739->$1;
          if (_M0L8_2afieldS3066) {
            moonbit_decref(_M0L8_2afieldS3066);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS739);
        }
        _M0L5valueS2073 = _M0L8_2afieldS2836;
        _M0L6_2atmpS2072 = _M0L5valueS2073;
        return _M0L6_2atmpS2072;
      } else {
        moonbit_incref(_M0L8_2aentryS739);
      }
      _M0L8_2afieldS2835 = _M0L8_2aentryS739->$2;
      moonbit_decref(_M0L8_2aentryS739);
      _M0L3pslS2074 = _M0L8_2afieldS2835;
      if (_M0L1iS734 > _M0L3pslS2074) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2075;
        moonbit_decref(_M0L4selfS737);
        moonbit_decref(_M0L3keyS733);
        _M0L6_2atmpS2075 = 0;
        return _M0L6_2atmpS2075;
      }
      _M0L6_2atmpS2076 = _M0L1iS734 + 1;
      _M0L6_2atmpS2078 = _M0L3idxS735 + 1;
      _M0L14capacity__maskS2079 = _M0L4selfS737->$3;
      _M0L6_2atmpS2077 = _M0L6_2atmpS2078 & _M0L14capacity__maskS2079;
      _M0L1iS734 = _M0L6_2atmpS2076;
      _M0L3idxS735 = _M0L6_2atmpS2077;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS700
) {
  int32_t _M0L6lengthS699;
  int32_t _M0Lm8capacityS701;
  int32_t _M0L6_2atmpS2020;
  int32_t _M0L6_2atmpS2019;
  int32_t _M0L6_2atmpS2030;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS702;
  int32_t _M0L3endS2028;
  int32_t _M0L5startS2029;
  int32_t _M0L7_2abindS703;
  int32_t _M0L2__S704;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS700.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS699
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS700);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS701 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS699);
  _M0L6_2atmpS2020 = _M0Lm8capacityS701;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2019 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2020);
  if (_M0L6lengthS699 > _M0L6_2atmpS2019) {
    int32_t _M0L6_2atmpS2021 = _M0Lm8capacityS701;
    _M0Lm8capacityS701 = _M0L6_2atmpS2021 * 2;
  }
  _M0L6_2atmpS2030 = _M0Lm8capacityS701;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS702
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2030);
  _M0L3endS2028 = _M0L3arrS700.$2;
  _M0L5startS2029 = _M0L3arrS700.$1;
  _M0L7_2abindS703 = _M0L3endS2028 - _M0L5startS2029;
  _M0L2__S704 = 0;
  while (1) {
    if (_M0L2__S704 < _M0L7_2abindS703) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2844 =
        _M0L3arrS700.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2025 =
        _M0L8_2afieldS2844;
      int32_t _M0L5startS2027 = _M0L3arrS700.$1;
      int32_t _M0L6_2atmpS2026 = _M0L5startS2027 + _M0L2__S704;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2843 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2025[
          _M0L6_2atmpS2026
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS705 =
        _M0L6_2atmpS2843;
      moonbit_string_t _M0L8_2afieldS2842 = _M0L1eS705->$0;
      moonbit_string_t _M0L6_2atmpS2022 = _M0L8_2afieldS2842;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2841 =
        _M0L1eS705->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2023 =
        _M0L8_2afieldS2841;
      int32_t _M0L6_2atmpS2024;
      moonbit_incref(_M0L6_2atmpS2023);
      moonbit_incref(_M0L6_2atmpS2022);
      moonbit_incref(_M0L1mS702);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS702, _M0L6_2atmpS2022, _M0L6_2atmpS2023);
      _M0L6_2atmpS2024 = _M0L2__S704 + 1;
      _M0L2__S704 = _M0L6_2atmpS2024;
      continue;
    } else {
      moonbit_decref(_M0L3arrS700.$0);
    }
    break;
  }
  return _M0L1mS702;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS708
) {
  int32_t _M0L6lengthS707;
  int32_t _M0Lm8capacityS709;
  int32_t _M0L6_2atmpS2032;
  int32_t _M0L6_2atmpS2031;
  int32_t _M0L6_2atmpS2042;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS710;
  int32_t _M0L3endS2040;
  int32_t _M0L5startS2041;
  int32_t _M0L7_2abindS711;
  int32_t _M0L2__S712;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS708.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS707
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS708);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS709 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS707);
  _M0L6_2atmpS2032 = _M0Lm8capacityS709;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2031 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2032);
  if (_M0L6lengthS707 > _M0L6_2atmpS2031) {
    int32_t _M0L6_2atmpS2033 = _M0Lm8capacityS709;
    _M0Lm8capacityS709 = _M0L6_2atmpS2033 * 2;
  }
  _M0L6_2atmpS2042 = _M0Lm8capacityS709;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS710
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2042);
  _M0L3endS2040 = _M0L3arrS708.$2;
  _M0L5startS2041 = _M0L3arrS708.$1;
  _M0L7_2abindS711 = _M0L3endS2040 - _M0L5startS2041;
  _M0L2__S712 = 0;
  while (1) {
    if (_M0L2__S712 < _M0L7_2abindS711) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2847 =
        _M0L3arrS708.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2037 =
        _M0L8_2afieldS2847;
      int32_t _M0L5startS2039 = _M0L3arrS708.$1;
      int32_t _M0L6_2atmpS2038 = _M0L5startS2039 + _M0L2__S712;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2846 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2037[
          _M0L6_2atmpS2038
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS713 = _M0L6_2atmpS2846;
      int32_t _M0L6_2atmpS2034 = _M0L1eS713->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2845 =
        _M0L1eS713->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2035 =
        _M0L8_2afieldS2845;
      int32_t _M0L6_2atmpS2036;
      moonbit_incref(_M0L6_2atmpS2035);
      moonbit_incref(_M0L1mS710);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS710, _M0L6_2atmpS2034, _M0L6_2atmpS2035);
      _M0L6_2atmpS2036 = _M0L2__S712 + 1;
      _M0L2__S712 = _M0L6_2atmpS2036;
      continue;
    } else {
      moonbit_decref(_M0L3arrS708.$0);
    }
    break;
  }
  return _M0L1mS710;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS716
) {
  int32_t _M0L6lengthS715;
  int32_t _M0Lm8capacityS717;
  int32_t _M0L6_2atmpS2044;
  int32_t _M0L6_2atmpS2043;
  int32_t _M0L6_2atmpS2054;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS718;
  int32_t _M0L3endS2052;
  int32_t _M0L5startS2053;
  int32_t _M0L7_2abindS719;
  int32_t _M0L2__S720;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS716.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS715 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS716);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS717 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS715);
  _M0L6_2atmpS2044 = _M0Lm8capacityS717;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2043 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2044);
  if (_M0L6lengthS715 > _M0L6_2atmpS2043) {
    int32_t _M0L6_2atmpS2045 = _M0Lm8capacityS717;
    _M0Lm8capacityS717 = _M0L6_2atmpS2045 * 2;
  }
  _M0L6_2atmpS2054 = _M0Lm8capacityS717;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS718 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2054);
  _M0L3endS2052 = _M0L3arrS716.$2;
  _M0L5startS2053 = _M0L3arrS716.$1;
  _M0L7_2abindS719 = _M0L3endS2052 - _M0L5startS2053;
  _M0L2__S720 = 0;
  while (1) {
    if (_M0L2__S720 < _M0L7_2abindS719) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS2851 = _M0L3arrS716.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2049 = _M0L8_2afieldS2851;
      int32_t _M0L5startS2051 = _M0L3arrS716.$1;
      int32_t _M0L6_2atmpS2050 = _M0L5startS2051 + _M0L2__S720;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS2850 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2049[_M0L6_2atmpS2050];
      struct _M0TUsRPB4JsonE* _M0L1eS721 = _M0L6_2atmpS2850;
      moonbit_string_t _M0L8_2afieldS2849 = _M0L1eS721->$0;
      moonbit_string_t _M0L6_2atmpS2046 = _M0L8_2afieldS2849;
      void* _M0L8_2afieldS2848 = _M0L1eS721->$1;
      void* _M0L6_2atmpS2047 = _M0L8_2afieldS2848;
      int32_t _M0L6_2atmpS2048;
      moonbit_incref(_M0L6_2atmpS2047);
      moonbit_incref(_M0L6_2atmpS2046);
      moonbit_incref(_M0L1mS718);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS718, _M0L6_2atmpS2046, _M0L6_2atmpS2047);
      _M0L6_2atmpS2048 = _M0L2__S720 + 1;
      _M0L2__S720 = _M0L6_2atmpS2048;
      continue;
    } else {
      moonbit_decref(_M0L3arrS716.$0);
    }
    break;
  }
  return _M0L1mS718;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS690,
  moonbit_string_t _M0L3keyS691,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS692
) {
  int32_t _M0L6_2atmpS2016;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS691);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2016 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS691);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS690, _M0L3keyS691, _M0L5valueS692, _M0L6_2atmpS2016);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS693,
  int32_t _M0L3keyS694,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS695
) {
  int32_t _M0L6_2atmpS2017;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2017 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS694);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS693, _M0L3keyS694, _M0L5valueS695, _M0L6_2atmpS2017);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS696,
  moonbit_string_t _M0L3keyS697,
  void* _M0L5valueS698
) {
  int32_t _M0L6_2atmpS2018;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS697);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2018 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS697);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS696, _M0L3keyS697, _M0L5valueS698, _M0L6_2atmpS2018);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS658
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2858;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS657;
  int32_t _M0L8capacityS2001;
  int32_t _M0L13new__capacityS659;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1996;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1995;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2857;
  int32_t _M0L6_2atmpS1997;
  int32_t _M0L8capacityS1999;
  int32_t _M0L6_2atmpS1998;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2000;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2856;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS660;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2858 = _M0L4selfS658->$5;
  _M0L9old__headS657 = _M0L8_2afieldS2858;
  _M0L8capacityS2001 = _M0L4selfS658->$2;
  _M0L13new__capacityS659 = _M0L8capacityS2001 << 1;
  _M0L6_2atmpS1996 = 0;
  _M0L6_2atmpS1995
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS659, _M0L6_2atmpS1996);
  _M0L6_2aoldS2857 = _M0L4selfS658->$0;
  if (_M0L9old__headS657) {
    moonbit_incref(_M0L9old__headS657);
  }
  moonbit_decref(_M0L6_2aoldS2857);
  _M0L4selfS658->$0 = _M0L6_2atmpS1995;
  _M0L4selfS658->$2 = _M0L13new__capacityS659;
  _M0L6_2atmpS1997 = _M0L13new__capacityS659 - 1;
  _M0L4selfS658->$3 = _M0L6_2atmpS1997;
  _M0L8capacityS1999 = _M0L4selfS658->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1998 = _M0FPB21calc__grow__threshold(_M0L8capacityS1999);
  _M0L4selfS658->$4 = _M0L6_2atmpS1998;
  _M0L4selfS658->$1 = 0;
  _M0L6_2atmpS2000 = 0;
  _M0L6_2aoldS2856 = _M0L4selfS658->$5;
  if (_M0L6_2aoldS2856) {
    moonbit_decref(_M0L6_2aoldS2856);
  }
  _M0L4selfS658->$5 = _M0L6_2atmpS2000;
  _M0L4selfS658->$6 = -1;
  _M0L8_2aparamS660 = _M0L9old__headS657;
  while (1) {
    if (_M0L8_2aparamS660 == 0) {
      if (_M0L8_2aparamS660) {
        moonbit_decref(_M0L8_2aparamS660);
      }
      moonbit_decref(_M0L4selfS658);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS661 =
        _M0L8_2aparamS660;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS662 =
        _M0L7_2aSomeS661;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2855 =
        _M0L4_2axS662->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS663 =
        _M0L8_2afieldS2855;
      moonbit_string_t _M0L8_2afieldS2854 = _M0L4_2axS662->$4;
      moonbit_string_t _M0L6_2akeyS664 = _M0L8_2afieldS2854;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2853 =
        _M0L4_2axS662->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS665 =
        _M0L8_2afieldS2853;
      int32_t _M0L8_2afieldS2852 = _M0L4_2axS662->$3;
      int32_t _M0L6_2acntS3069 = Moonbit_object_header(_M0L4_2axS662)->rc;
      int32_t _M0L7_2ahashS666;
      if (_M0L6_2acntS3069 > 1) {
        int32_t _M0L11_2anew__cntS3070 = _M0L6_2acntS3069 - 1;
        Moonbit_object_header(_M0L4_2axS662)->rc = _M0L11_2anew__cntS3070;
        moonbit_incref(_M0L8_2avalueS665);
        moonbit_incref(_M0L6_2akeyS664);
        if (_M0L7_2anextS663) {
          moonbit_incref(_M0L7_2anextS663);
        }
      } else if (_M0L6_2acntS3069 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS662);
      }
      _M0L7_2ahashS666 = _M0L8_2afieldS2852;
      moonbit_incref(_M0L4selfS658);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS658, _M0L6_2akeyS664, _M0L8_2avalueS665, _M0L7_2ahashS666);
      _M0L8_2aparamS660 = _M0L7_2anextS663;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS669
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2864;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS668;
  int32_t _M0L8capacityS2008;
  int32_t _M0L13new__capacityS670;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2003;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2002;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2863;
  int32_t _M0L6_2atmpS2004;
  int32_t _M0L8capacityS2006;
  int32_t _M0L6_2atmpS2005;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2007;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2862;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS671;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2864 = _M0L4selfS669->$5;
  _M0L9old__headS668 = _M0L8_2afieldS2864;
  _M0L8capacityS2008 = _M0L4selfS669->$2;
  _M0L13new__capacityS670 = _M0L8capacityS2008 << 1;
  _M0L6_2atmpS2003 = 0;
  _M0L6_2atmpS2002
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS670, _M0L6_2atmpS2003);
  _M0L6_2aoldS2863 = _M0L4selfS669->$0;
  if (_M0L9old__headS668) {
    moonbit_incref(_M0L9old__headS668);
  }
  moonbit_decref(_M0L6_2aoldS2863);
  _M0L4selfS669->$0 = _M0L6_2atmpS2002;
  _M0L4selfS669->$2 = _M0L13new__capacityS670;
  _M0L6_2atmpS2004 = _M0L13new__capacityS670 - 1;
  _M0L4selfS669->$3 = _M0L6_2atmpS2004;
  _M0L8capacityS2006 = _M0L4selfS669->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2005 = _M0FPB21calc__grow__threshold(_M0L8capacityS2006);
  _M0L4selfS669->$4 = _M0L6_2atmpS2005;
  _M0L4selfS669->$1 = 0;
  _M0L6_2atmpS2007 = 0;
  _M0L6_2aoldS2862 = _M0L4selfS669->$5;
  if (_M0L6_2aoldS2862) {
    moonbit_decref(_M0L6_2aoldS2862);
  }
  _M0L4selfS669->$5 = _M0L6_2atmpS2007;
  _M0L4selfS669->$6 = -1;
  _M0L8_2aparamS671 = _M0L9old__headS668;
  while (1) {
    if (_M0L8_2aparamS671 == 0) {
      if (_M0L8_2aparamS671) {
        moonbit_decref(_M0L8_2aparamS671);
      }
      moonbit_decref(_M0L4selfS669);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS672 =
        _M0L8_2aparamS671;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS673 =
        _M0L7_2aSomeS672;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2861 =
        _M0L4_2axS673->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS674 =
        _M0L8_2afieldS2861;
      int32_t _M0L6_2akeyS675 = _M0L4_2axS673->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2860 =
        _M0L4_2axS673->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS676 =
        _M0L8_2afieldS2860;
      int32_t _M0L8_2afieldS2859 = _M0L4_2axS673->$3;
      int32_t _M0L6_2acntS3071 = Moonbit_object_header(_M0L4_2axS673)->rc;
      int32_t _M0L7_2ahashS677;
      if (_M0L6_2acntS3071 > 1) {
        int32_t _M0L11_2anew__cntS3072 = _M0L6_2acntS3071 - 1;
        Moonbit_object_header(_M0L4_2axS673)->rc = _M0L11_2anew__cntS3072;
        moonbit_incref(_M0L8_2avalueS676);
        if (_M0L7_2anextS674) {
          moonbit_incref(_M0L7_2anextS674);
        }
      } else if (_M0L6_2acntS3071 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS673);
      }
      _M0L7_2ahashS677 = _M0L8_2afieldS2859;
      moonbit_incref(_M0L4selfS669);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS669, _M0L6_2akeyS675, _M0L8_2avalueS676, _M0L7_2ahashS677);
      _M0L8_2aparamS671 = _M0L7_2anextS674;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS680
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2871;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS679;
  int32_t _M0L8capacityS2015;
  int32_t _M0L13new__capacityS681;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2010;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2009;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS2870;
  int32_t _M0L6_2atmpS2011;
  int32_t _M0L8capacityS2013;
  int32_t _M0L6_2atmpS2012;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2014;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2869;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS682;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2871 = _M0L4selfS680->$5;
  _M0L9old__headS679 = _M0L8_2afieldS2871;
  _M0L8capacityS2015 = _M0L4selfS680->$2;
  _M0L13new__capacityS681 = _M0L8capacityS2015 << 1;
  _M0L6_2atmpS2010 = 0;
  _M0L6_2atmpS2009
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS681, _M0L6_2atmpS2010);
  _M0L6_2aoldS2870 = _M0L4selfS680->$0;
  if (_M0L9old__headS679) {
    moonbit_incref(_M0L9old__headS679);
  }
  moonbit_decref(_M0L6_2aoldS2870);
  _M0L4selfS680->$0 = _M0L6_2atmpS2009;
  _M0L4selfS680->$2 = _M0L13new__capacityS681;
  _M0L6_2atmpS2011 = _M0L13new__capacityS681 - 1;
  _M0L4selfS680->$3 = _M0L6_2atmpS2011;
  _M0L8capacityS2013 = _M0L4selfS680->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2012 = _M0FPB21calc__grow__threshold(_M0L8capacityS2013);
  _M0L4selfS680->$4 = _M0L6_2atmpS2012;
  _M0L4selfS680->$1 = 0;
  _M0L6_2atmpS2014 = 0;
  _M0L6_2aoldS2869 = _M0L4selfS680->$5;
  if (_M0L6_2aoldS2869) {
    moonbit_decref(_M0L6_2aoldS2869);
  }
  _M0L4selfS680->$5 = _M0L6_2atmpS2014;
  _M0L4selfS680->$6 = -1;
  _M0L8_2aparamS682 = _M0L9old__headS679;
  while (1) {
    if (_M0L8_2aparamS682 == 0) {
      if (_M0L8_2aparamS682) {
        moonbit_decref(_M0L8_2aparamS682);
      }
      moonbit_decref(_M0L4selfS680);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS683 = _M0L8_2aparamS682;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS684 = _M0L7_2aSomeS683;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2868 = _M0L4_2axS684->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS685 = _M0L8_2afieldS2868;
      moonbit_string_t _M0L8_2afieldS2867 = _M0L4_2axS684->$4;
      moonbit_string_t _M0L6_2akeyS686 = _M0L8_2afieldS2867;
      void* _M0L8_2afieldS2866 = _M0L4_2axS684->$5;
      void* _M0L8_2avalueS687 = _M0L8_2afieldS2866;
      int32_t _M0L8_2afieldS2865 = _M0L4_2axS684->$3;
      int32_t _M0L6_2acntS3073 = Moonbit_object_header(_M0L4_2axS684)->rc;
      int32_t _M0L7_2ahashS688;
      if (_M0L6_2acntS3073 > 1) {
        int32_t _M0L11_2anew__cntS3074 = _M0L6_2acntS3073 - 1;
        Moonbit_object_header(_M0L4_2axS684)->rc = _M0L11_2anew__cntS3074;
        moonbit_incref(_M0L8_2avalueS687);
        moonbit_incref(_M0L6_2akeyS686);
        if (_M0L7_2anextS685) {
          moonbit_incref(_M0L7_2anextS685);
        }
      } else if (_M0L6_2acntS3073 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS684);
      }
      _M0L7_2ahashS688 = _M0L8_2afieldS2865;
      moonbit_incref(_M0L4selfS680);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS680, _M0L6_2akeyS686, _M0L8_2avalueS687, _M0L7_2ahashS688);
      _M0L8_2aparamS682 = _M0L7_2anextS685;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS612,
  moonbit_string_t _M0L3keyS618,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS619,
  int32_t _M0L4hashS614
) {
  int32_t _M0L14capacity__maskS1958;
  int32_t _M0L6_2atmpS1957;
  int32_t _M0L3pslS609;
  int32_t _M0L3idxS610;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1958 = _M0L4selfS612->$3;
  _M0L6_2atmpS1957 = _M0L4hashS614 & _M0L14capacity__maskS1958;
  _M0L3pslS609 = 0;
  _M0L3idxS610 = _M0L6_2atmpS1957;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2876 =
      _M0L4selfS612->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1956 =
      _M0L8_2afieldS2876;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2875;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS611;
    if (
      _M0L3idxS610 < 0
      || _M0L3idxS610 >= Moonbit_array_length(_M0L7entriesS1956)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2875
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1956[
        _M0L3idxS610
      ];
    _M0L7_2abindS611 = _M0L6_2atmpS2875;
    if (_M0L7_2abindS611 == 0) {
      int32_t _M0L4sizeS1941 = _M0L4selfS612->$1;
      int32_t _M0L8grow__atS1942 = _M0L4selfS612->$4;
      int32_t _M0L7_2abindS615;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS616;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS617;
      if (_M0L4sizeS1941 >= _M0L8grow__atS1942) {
        int32_t _M0L14capacity__maskS1944;
        int32_t _M0L6_2atmpS1943;
        moonbit_incref(_M0L4selfS612);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS612);
        _M0L14capacity__maskS1944 = _M0L4selfS612->$3;
        _M0L6_2atmpS1943 = _M0L4hashS614 & _M0L14capacity__maskS1944;
        _M0L3pslS609 = 0;
        _M0L3idxS610 = _M0L6_2atmpS1943;
        continue;
      }
      _M0L7_2abindS615 = _M0L4selfS612->$6;
      _M0L7_2abindS616 = 0;
      _M0L5entryS617
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS617)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS617->$0 = _M0L7_2abindS615;
      _M0L5entryS617->$1 = _M0L7_2abindS616;
      _M0L5entryS617->$2 = _M0L3pslS609;
      _M0L5entryS617->$3 = _M0L4hashS614;
      _M0L5entryS617->$4 = _M0L3keyS618;
      _M0L5entryS617->$5 = _M0L5valueS619;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS612, _M0L3idxS610, _M0L5entryS617);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS620 =
        _M0L7_2abindS611;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS621 =
        _M0L7_2aSomeS620;
      int32_t _M0L4hashS1946 = _M0L14_2acurr__entryS621->$3;
      int32_t _if__result_3187;
      int32_t _M0L3pslS1947;
      int32_t _M0L6_2atmpS1952;
      int32_t _M0L6_2atmpS1954;
      int32_t _M0L14capacity__maskS1955;
      int32_t _M0L6_2atmpS1953;
      if (_M0L4hashS1946 == _M0L4hashS614) {
        moonbit_string_t _M0L8_2afieldS2874 = _M0L14_2acurr__entryS621->$4;
        moonbit_string_t _M0L3keyS1945 = _M0L8_2afieldS2874;
        int32_t _M0L6_2atmpS2873;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2873
        = moonbit_val_array_equal(_M0L3keyS1945, _M0L3keyS618);
        _if__result_3187 = _M0L6_2atmpS2873;
      } else {
        _if__result_3187 = 0;
      }
      if (_if__result_3187) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2872;
        moonbit_incref(_M0L14_2acurr__entryS621);
        moonbit_decref(_M0L3keyS618);
        moonbit_decref(_M0L4selfS612);
        _M0L6_2aoldS2872 = _M0L14_2acurr__entryS621->$5;
        moonbit_decref(_M0L6_2aoldS2872);
        _M0L14_2acurr__entryS621->$5 = _M0L5valueS619;
        moonbit_decref(_M0L14_2acurr__entryS621);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS621);
      }
      _M0L3pslS1947 = _M0L14_2acurr__entryS621->$2;
      if (_M0L3pslS609 > _M0L3pslS1947) {
        int32_t _M0L4sizeS1948 = _M0L4selfS612->$1;
        int32_t _M0L8grow__atS1949 = _M0L4selfS612->$4;
        int32_t _M0L7_2abindS622;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS623;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS624;
        if (_M0L4sizeS1948 >= _M0L8grow__atS1949) {
          int32_t _M0L14capacity__maskS1951;
          int32_t _M0L6_2atmpS1950;
          moonbit_decref(_M0L14_2acurr__entryS621);
          moonbit_incref(_M0L4selfS612);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS612);
          _M0L14capacity__maskS1951 = _M0L4selfS612->$3;
          _M0L6_2atmpS1950 = _M0L4hashS614 & _M0L14capacity__maskS1951;
          _M0L3pslS609 = 0;
          _M0L3idxS610 = _M0L6_2atmpS1950;
          continue;
        }
        moonbit_incref(_M0L4selfS612);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS612, _M0L3idxS610, _M0L14_2acurr__entryS621);
        _M0L7_2abindS622 = _M0L4selfS612->$6;
        _M0L7_2abindS623 = 0;
        _M0L5entryS624
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS624)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS624->$0 = _M0L7_2abindS622;
        _M0L5entryS624->$1 = _M0L7_2abindS623;
        _M0L5entryS624->$2 = _M0L3pslS609;
        _M0L5entryS624->$3 = _M0L4hashS614;
        _M0L5entryS624->$4 = _M0L3keyS618;
        _M0L5entryS624->$5 = _M0L5valueS619;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS612, _M0L3idxS610, _M0L5entryS624);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS621);
      }
      _M0L6_2atmpS1952 = _M0L3pslS609 + 1;
      _M0L6_2atmpS1954 = _M0L3idxS610 + 1;
      _M0L14capacity__maskS1955 = _M0L4selfS612->$3;
      _M0L6_2atmpS1953 = _M0L6_2atmpS1954 & _M0L14capacity__maskS1955;
      _M0L3pslS609 = _M0L6_2atmpS1952;
      _M0L3idxS610 = _M0L6_2atmpS1953;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS628,
  int32_t _M0L3keyS634,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS635,
  int32_t _M0L4hashS630
) {
  int32_t _M0L14capacity__maskS1976;
  int32_t _M0L6_2atmpS1975;
  int32_t _M0L3pslS625;
  int32_t _M0L3idxS626;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1976 = _M0L4selfS628->$3;
  _M0L6_2atmpS1975 = _M0L4hashS630 & _M0L14capacity__maskS1976;
  _M0L3pslS625 = 0;
  _M0L3idxS626 = _M0L6_2atmpS1975;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2879 =
      _M0L4selfS628->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1974 =
      _M0L8_2afieldS2879;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2878;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS627;
    if (
      _M0L3idxS626 < 0
      || _M0L3idxS626 >= Moonbit_array_length(_M0L7entriesS1974)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2878
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1974[
        _M0L3idxS626
      ];
    _M0L7_2abindS627 = _M0L6_2atmpS2878;
    if (_M0L7_2abindS627 == 0) {
      int32_t _M0L4sizeS1959 = _M0L4selfS628->$1;
      int32_t _M0L8grow__atS1960 = _M0L4selfS628->$4;
      int32_t _M0L7_2abindS631;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS632;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS633;
      if (_M0L4sizeS1959 >= _M0L8grow__atS1960) {
        int32_t _M0L14capacity__maskS1962;
        int32_t _M0L6_2atmpS1961;
        moonbit_incref(_M0L4selfS628);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS628);
        _M0L14capacity__maskS1962 = _M0L4selfS628->$3;
        _M0L6_2atmpS1961 = _M0L4hashS630 & _M0L14capacity__maskS1962;
        _M0L3pslS625 = 0;
        _M0L3idxS626 = _M0L6_2atmpS1961;
        continue;
      }
      _M0L7_2abindS631 = _M0L4selfS628->$6;
      _M0L7_2abindS632 = 0;
      _M0L5entryS633
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS633)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS633->$0 = _M0L7_2abindS631;
      _M0L5entryS633->$1 = _M0L7_2abindS632;
      _M0L5entryS633->$2 = _M0L3pslS625;
      _M0L5entryS633->$3 = _M0L4hashS630;
      _M0L5entryS633->$4 = _M0L3keyS634;
      _M0L5entryS633->$5 = _M0L5valueS635;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS628, _M0L3idxS626, _M0L5entryS633);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS636 =
        _M0L7_2abindS627;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS637 =
        _M0L7_2aSomeS636;
      int32_t _M0L4hashS1964 = _M0L14_2acurr__entryS637->$3;
      int32_t _if__result_3189;
      int32_t _M0L3pslS1965;
      int32_t _M0L6_2atmpS1970;
      int32_t _M0L6_2atmpS1972;
      int32_t _M0L14capacity__maskS1973;
      int32_t _M0L6_2atmpS1971;
      if (_M0L4hashS1964 == _M0L4hashS630) {
        int32_t _M0L3keyS1963 = _M0L14_2acurr__entryS637->$4;
        _if__result_3189 = _M0L3keyS1963 == _M0L3keyS634;
      } else {
        _if__result_3189 = 0;
      }
      if (_if__result_3189) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2877;
        moonbit_incref(_M0L14_2acurr__entryS637);
        moonbit_decref(_M0L4selfS628);
        _M0L6_2aoldS2877 = _M0L14_2acurr__entryS637->$5;
        moonbit_decref(_M0L6_2aoldS2877);
        _M0L14_2acurr__entryS637->$5 = _M0L5valueS635;
        moonbit_decref(_M0L14_2acurr__entryS637);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS637);
      }
      _M0L3pslS1965 = _M0L14_2acurr__entryS637->$2;
      if (_M0L3pslS625 > _M0L3pslS1965) {
        int32_t _M0L4sizeS1966 = _M0L4selfS628->$1;
        int32_t _M0L8grow__atS1967 = _M0L4selfS628->$4;
        int32_t _M0L7_2abindS638;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS639;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS640;
        if (_M0L4sizeS1966 >= _M0L8grow__atS1967) {
          int32_t _M0L14capacity__maskS1969;
          int32_t _M0L6_2atmpS1968;
          moonbit_decref(_M0L14_2acurr__entryS637);
          moonbit_incref(_M0L4selfS628);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS628);
          _M0L14capacity__maskS1969 = _M0L4selfS628->$3;
          _M0L6_2atmpS1968 = _M0L4hashS630 & _M0L14capacity__maskS1969;
          _M0L3pslS625 = 0;
          _M0L3idxS626 = _M0L6_2atmpS1968;
          continue;
        }
        moonbit_incref(_M0L4selfS628);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS628, _M0L3idxS626, _M0L14_2acurr__entryS637);
        _M0L7_2abindS638 = _M0L4selfS628->$6;
        _M0L7_2abindS639 = 0;
        _M0L5entryS640
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS640)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS640->$0 = _M0L7_2abindS638;
        _M0L5entryS640->$1 = _M0L7_2abindS639;
        _M0L5entryS640->$2 = _M0L3pslS625;
        _M0L5entryS640->$3 = _M0L4hashS630;
        _M0L5entryS640->$4 = _M0L3keyS634;
        _M0L5entryS640->$5 = _M0L5valueS635;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS628, _M0L3idxS626, _M0L5entryS640);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS637);
      }
      _M0L6_2atmpS1970 = _M0L3pslS625 + 1;
      _M0L6_2atmpS1972 = _M0L3idxS626 + 1;
      _M0L14capacity__maskS1973 = _M0L4selfS628->$3;
      _M0L6_2atmpS1971 = _M0L6_2atmpS1972 & _M0L14capacity__maskS1973;
      _M0L3pslS625 = _M0L6_2atmpS1970;
      _M0L3idxS626 = _M0L6_2atmpS1971;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS644,
  moonbit_string_t _M0L3keyS650,
  void* _M0L5valueS651,
  int32_t _M0L4hashS646
) {
  int32_t _M0L14capacity__maskS1994;
  int32_t _M0L6_2atmpS1993;
  int32_t _M0L3pslS641;
  int32_t _M0L3idxS642;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1994 = _M0L4selfS644->$3;
  _M0L6_2atmpS1993 = _M0L4hashS646 & _M0L14capacity__maskS1994;
  _M0L3pslS641 = 0;
  _M0L3idxS642 = _M0L6_2atmpS1993;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS2884 = _M0L4selfS644->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS1992 = _M0L8_2afieldS2884;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2883;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS643;
    if (
      _M0L3idxS642 < 0
      || _M0L3idxS642 >= Moonbit_array_length(_M0L7entriesS1992)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2883
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS1992[_M0L3idxS642];
    _M0L7_2abindS643 = _M0L6_2atmpS2883;
    if (_M0L7_2abindS643 == 0) {
      int32_t _M0L4sizeS1977 = _M0L4selfS644->$1;
      int32_t _M0L8grow__atS1978 = _M0L4selfS644->$4;
      int32_t _M0L7_2abindS647;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS648;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS649;
      if (_M0L4sizeS1977 >= _M0L8grow__atS1978) {
        int32_t _M0L14capacity__maskS1980;
        int32_t _M0L6_2atmpS1979;
        moonbit_incref(_M0L4selfS644);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS644);
        _M0L14capacity__maskS1980 = _M0L4selfS644->$3;
        _M0L6_2atmpS1979 = _M0L4hashS646 & _M0L14capacity__maskS1980;
        _M0L3pslS641 = 0;
        _M0L3idxS642 = _M0L6_2atmpS1979;
        continue;
      }
      _M0L7_2abindS647 = _M0L4selfS644->$6;
      _M0L7_2abindS648 = 0;
      _M0L5entryS649
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS649)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS649->$0 = _M0L7_2abindS647;
      _M0L5entryS649->$1 = _M0L7_2abindS648;
      _M0L5entryS649->$2 = _M0L3pslS641;
      _M0L5entryS649->$3 = _M0L4hashS646;
      _M0L5entryS649->$4 = _M0L3keyS650;
      _M0L5entryS649->$5 = _M0L5valueS651;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS644, _M0L3idxS642, _M0L5entryS649);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS652 = _M0L7_2abindS643;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS653 =
        _M0L7_2aSomeS652;
      int32_t _M0L4hashS1982 = _M0L14_2acurr__entryS653->$3;
      int32_t _if__result_3191;
      int32_t _M0L3pslS1983;
      int32_t _M0L6_2atmpS1988;
      int32_t _M0L6_2atmpS1990;
      int32_t _M0L14capacity__maskS1991;
      int32_t _M0L6_2atmpS1989;
      if (_M0L4hashS1982 == _M0L4hashS646) {
        moonbit_string_t _M0L8_2afieldS2882 = _M0L14_2acurr__entryS653->$4;
        moonbit_string_t _M0L3keyS1981 = _M0L8_2afieldS2882;
        int32_t _M0L6_2atmpS2881;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2881
        = moonbit_val_array_equal(_M0L3keyS1981, _M0L3keyS650);
        _if__result_3191 = _M0L6_2atmpS2881;
      } else {
        _if__result_3191 = 0;
      }
      if (_if__result_3191) {
        void* _M0L6_2aoldS2880;
        moonbit_incref(_M0L14_2acurr__entryS653);
        moonbit_decref(_M0L3keyS650);
        moonbit_decref(_M0L4selfS644);
        _M0L6_2aoldS2880 = _M0L14_2acurr__entryS653->$5;
        moonbit_decref(_M0L6_2aoldS2880);
        _M0L14_2acurr__entryS653->$5 = _M0L5valueS651;
        moonbit_decref(_M0L14_2acurr__entryS653);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS653);
      }
      _M0L3pslS1983 = _M0L14_2acurr__entryS653->$2;
      if (_M0L3pslS641 > _M0L3pslS1983) {
        int32_t _M0L4sizeS1984 = _M0L4selfS644->$1;
        int32_t _M0L8grow__atS1985 = _M0L4selfS644->$4;
        int32_t _M0L7_2abindS654;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS655;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS656;
        if (_M0L4sizeS1984 >= _M0L8grow__atS1985) {
          int32_t _M0L14capacity__maskS1987;
          int32_t _M0L6_2atmpS1986;
          moonbit_decref(_M0L14_2acurr__entryS653);
          moonbit_incref(_M0L4selfS644);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS644);
          _M0L14capacity__maskS1987 = _M0L4selfS644->$3;
          _M0L6_2atmpS1986 = _M0L4hashS646 & _M0L14capacity__maskS1987;
          _M0L3pslS641 = 0;
          _M0L3idxS642 = _M0L6_2atmpS1986;
          continue;
        }
        moonbit_incref(_M0L4selfS644);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS644, _M0L3idxS642, _M0L14_2acurr__entryS653);
        _M0L7_2abindS654 = _M0L4selfS644->$6;
        _M0L7_2abindS655 = 0;
        _M0L5entryS656
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS656)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS656->$0 = _M0L7_2abindS654;
        _M0L5entryS656->$1 = _M0L7_2abindS655;
        _M0L5entryS656->$2 = _M0L3pslS641;
        _M0L5entryS656->$3 = _M0L4hashS646;
        _M0L5entryS656->$4 = _M0L3keyS650;
        _M0L5entryS656->$5 = _M0L5valueS651;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS644, _M0L3idxS642, _M0L5entryS656);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS653);
      }
      _M0L6_2atmpS1988 = _M0L3pslS641 + 1;
      _M0L6_2atmpS1990 = _M0L3idxS642 + 1;
      _M0L14capacity__maskS1991 = _M0L4selfS644->$3;
      _M0L6_2atmpS1989 = _M0L6_2atmpS1990 & _M0L14capacity__maskS1991;
      _M0L3pslS641 = _M0L6_2atmpS1988;
      _M0L3idxS642 = _M0L6_2atmpS1989;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS583,
  int32_t _M0L3idxS588,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS587
) {
  int32_t _M0L3pslS1908;
  int32_t _M0L6_2atmpS1904;
  int32_t _M0L6_2atmpS1906;
  int32_t _M0L14capacity__maskS1907;
  int32_t _M0L6_2atmpS1905;
  int32_t _M0L3pslS579;
  int32_t _M0L3idxS580;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS581;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1908 = _M0L5entryS587->$2;
  _M0L6_2atmpS1904 = _M0L3pslS1908 + 1;
  _M0L6_2atmpS1906 = _M0L3idxS588 + 1;
  _M0L14capacity__maskS1907 = _M0L4selfS583->$3;
  _M0L6_2atmpS1905 = _M0L6_2atmpS1906 & _M0L14capacity__maskS1907;
  _M0L3pslS579 = _M0L6_2atmpS1904;
  _M0L3idxS580 = _M0L6_2atmpS1905;
  _M0L5entryS581 = _M0L5entryS587;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2886 =
      _M0L4selfS583->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1903 =
      _M0L8_2afieldS2886;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2885;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS582;
    if (
      _M0L3idxS580 < 0
      || _M0L3idxS580 >= Moonbit_array_length(_M0L7entriesS1903)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2885
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1903[
        _M0L3idxS580
      ];
    _M0L7_2abindS582 = _M0L6_2atmpS2885;
    if (_M0L7_2abindS582 == 0) {
      _M0L5entryS581->$2 = _M0L3pslS579;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS585 =
        _M0L7_2abindS582;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS586 =
        _M0L7_2aSomeS585;
      int32_t _M0L3pslS1893 = _M0L14_2acurr__entryS586->$2;
      if (_M0L3pslS579 > _M0L3pslS1893) {
        int32_t _M0L3pslS1898;
        int32_t _M0L6_2atmpS1894;
        int32_t _M0L6_2atmpS1896;
        int32_t _M0L14capacity__maskS1897;
        int32_t _M0L6_2atmpS1895;
        _M0L5entryS581->$2 = _M0L3pslS579;
        moonbit_incref(_M0L14_2acurr__entryS586);
        moonbit_incref(_M0L4selfS583);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
        _M0L3pslS1898 = _M0L14_2acurr__entryS586->$2;
        _M0L6_2atmpS1894 = _M0L3pslS1898 + 1;
        _M0L6_2atmpS1896 = _M0L3idxS580 + 1;
        _M0L14capacity__maskS1897 = _M0L4selfS583->$3;
        _M0L6_2atmpS1895 = _M0L6_2atmpS1896 & _M0L14capacity__maskS1897;
        _M0L3pslS579 = _M0L6_2atmpS1894;
        _M0L3idxS580 = _M0L6_2atmpS1895;
        _M0L5entryS581 = _M0L14_2acurr__entryS586;
        continue;
      } else {
        int32_t _M0L6_2atmpS1899 = _M0L3pslS579 + 1;
        int32_t _M0L6_2atmpS1901 = _M0L3idxS580 + 1;
        int32_t _M0L14capacity__maskS1902 = _M0L4selfS583->$3;
        int32_t _M0L6_2atmpS1900 =
          _M0L6_2atmpS1901 & _M0L14capacity__maskS1902;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3193 =
          _M0L5entryS581;
        _M0L3pslS579 = _M0L6_2atmpS1899;
        _M0L3idxS580 = _M0L6_2atmpS1900;
        _M0L5entryS581 = _tmp_3193;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS593,
  int32_t _M0L3idxS598,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS597
) {
  int32_t _M0L3pslS1924;
  int32_t _M0L6_2atmpS1920;
  int32_t _M0L6_2atmpS1922;
  int32_t _M0L14capacity__maskS1923;
  int32_t _M0L6_2atmpS1921;
  int32_t _M0L3pslS589;
  int32_t _M0L3idxS590;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS591;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1924 = _M0L5entryS597->$2;
  _M0L6_2atmpS1920 = _M0L3pslS1924 + 1;
  _M0L6_2atmpS1922 = _M0L3idxS598 + 1;
  _M0L14capacity__maskS1923 = _M0L4selfS593->$3;
  _M0L6_2atmpS1921 = _M0L6_2atmpS1922 & _M0L14capacity__maskS1923;
  _M0L3pslS589 = _M0L6_2atmpS1920;
  _M0L3idxS590 = _M0L6_2atmpS1921;
  _M0L5entryS591 = _M0L5entryS597;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2888 =
      _M0L4selfS593->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1919 =
      _M0L8_2afieldS2888;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2887;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS592;
    if (
      _M0L3idxS590 < 0
      || _M0L3idxS590 >= Moonbit_array_length(_M0L7entriesS1919)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2887
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1919[
        _M0L3idxS590
      ];
    _M0L7_2abindS592 = _M0L6_2atmpS2887;
    if (_M0L7_2abindS592 == 0) {
      _M0L5entryS591->$2 = _M0L3pslS589;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS595 =
        _M0L7_2abindS592;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS596 =
        _M0L7_2aSomeS595;
      int32_t _M0L3pslS1909 = _M0L14_2acurr__entryS596->$2;
      if (_M0L3pslS589 > _M0L3pslS1909) {
        int32_t _M0L3pslS1914;
        int32_t _M0L6_2atmpS1910;
        int32_t _M0L6_2atmpS1912;
        int32_t _M0L14capacity__maskS1913;
        int32_t _M0L6_2atmpS1911;
        _M0L5entryS591->$2 = _M0L3pslS589;
        moonbit_incref(_M0L14_2acurr__entryS596);
        moonbit_incref(_M0L4selfS593);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
        _M0L3pslS1914 = _M0L14_2acurr__entryS596->$2;
        _M0L6_2atmpS1910 = _M0L3pslS1914 + 1;
        _M0L6_2atmpS1912 = _M0L3idxS590 + 1;
        _M0L14capacity__maskS1913 = _M0L4selfS593->$3;
        _M0L6_2atmpS1911 = _M0L6_2atmpS1912 & _M0L14capacity__maskS1913;
        _M0L3pslS589 = _M0L6_2atmpS1910;
        _M0L3idxS590 = _M0L6_2atmpS1911;
        _M0L5entryS591 = _M0L14_2acurr__entryS596;
        continue;
      } else {
        int32_t _M0L6_2atmpS1915 = _M0L3pslS589 + 1;
        int32_t _M0L6_2atmpS1917 = _M0L3idxS590 + 1;
        int32_t _M0L14capacity__maskS1918 = _M0L4selfS593->$3;
        int32_t _M0L6_2atmpS1916 =
          _M0L6_2atmpS1917 & _M0L14capacity__maskS1918;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3195 =
          _M0L5entryS591;
        _M0L3pslS589 = _M0L6_2atmpS1915;
        _M0L3idxS590 = _M0L6_2atmpS1916;
        _M0L5entryS591 = _tmp_3195;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS603,
  int32_t _M0L3idxS608,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS607
) {
  int32_t _M0L3pslS1940;
  int32_t _M0L6_2atmpS1936;
  int32_t _M0L6_2atmpS1938;
  int32_t _M0L14capacity__maskS1939;
  int32_t _M0L6_2atmpS1937;
  int32_t _M0L3pslS599;
  int32_t _M0L3idxS600;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS601;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1940 = _M0L5entryS607->$2;
  _M0L6_2atmpS1936 = _M0L3pslS1940 + 1;
  _M0L6_2atmpS1938 = _M0L3idxS608 + 1;
  _M0L14capacity__maskS1939 = _M0L4selfS603->$3;
  _M0L6_2atmpS1937 = _M0L6_2atmpS1938 & _M0L14capacity__maskS1939;
  _M0L3pslS599 = _M0L6_2atmpS1936;
  _M0L3idxS600 = _M0L6_2atmpS1937;
  _M0L5entryS601 = _M0L5entryS607;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS2890 = _M0L4selfS603->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS1935 = _M0L8_2afieldS2890;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2889;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS602;
    if (
      _M0L3idxS600 < 0
      || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS1935)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2889
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS1935[_M0L3idxS600];
    _M0L7_2abindS602 = _M0L6_2atmpS2889;
    if (_M0L7_2abindS602 == 0) {
      _M0L5entryS601->$2 = _M0L3pslS599;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS605 = _M0L7_2abindS602;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS606 =
        _M0L7_2aSomeS605;
      int32_t _M0L3pslS1925 = _M0L14_2acurr__entryS606->$2;
      if (_M0L3pslS599 > _M0L3pslS1925) {
        int32_t _M0L3pslS1930;
        int32_t _M0L6_2atmpS1926;
        int32_t _M0L6_2atmpS1928;
        int32_t _M0L14capacity__maskS1929;
        int32_t _M0L6_2atmpS1927;
        _M0L5entryS601->$2 = _M0L3pslS599;
        moonbit_incref(_M0L14_2acurr__entryS606);
        moonbit_incref(_M0L4selfS603);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
        _M0L3pslS1930 = _M0L14_2acurr__entryS606->$2;
        _M0L6_2atmpS1926 = _M0L3pslS1930 + 1;
        _M0L6_2atmpS1928 = _M0L3idxS600 + 1;
        _M0L14capacity__maskS1929 = _M0L4selfS603->$3;
        _M0L6_2atmpS1927 = _M0L6_2atmpS1928 & _M0L14capacity__maskS1929;
        _M0L3pslS599 = _M0L6_2atmpS1926;
        _M0L3idxS600 = _M0L6_2atmpS1927;
        _M0L5entryS601 = _M0L14_2acurr__entryS606;
        continue;
      } else {
        int32_t _M0L6_2atmpS1931 = _M0L3pslS599 + 1;
        int32_t _M0L6_2atmpS1933 = _M0L3idxS600 + 1;
        int32_t _M0L14capacity__maskS1934 = _M0L4selfS603->$3;
        int32_t _M0L6_2atmpS1932 =
          _M0L6_2atmpS1933 & _M0L14capacity__maskS1934;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_3197 = _M0L5entryS601;
        _M0L3pslS599 = _M0L6_2atmpS1931;
        _M0L3idxS600 = _M0L6_2atmpS1932;
        _M0L5entryS601 = _tmp_3197;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS561,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS563,
  int32_t _M0L8new__idxS562
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2893;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1887;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1888;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2892;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2891;
  int32_t _M0L6_2acntS3075;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS564;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2893 = _M0L4selfS561->$0;
  _M0L7entriesS1887 = _M0L8_2afieldS2893;
  moonbit_incref(_M0L5entryS563);
  _M0L6_2atmpS1888 = _M0L5entryS563;
  if (
    _M0L8new__idxS562 < 0
    || _M0L8new__idxS562 >= Moonbit_array_length(_M0L7entriesS1887)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2892
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1887[
      _M0L8new__idxS562
    ];
  if (_M0L6_2aoldS2892) {
    moonbit_decref(_M0L6_2aoldS2892);
  }
  _M0L7entriesS1887[_M0L8new__idxS562] = _M0L6_2atmpS1888;
  _M0L8_2afieldS2891 = _M0L5entryS563->$1;
  _M0L6_2acntS3075 = Moonbit_object_header(_M0L5entryS563)->rc;
  if (_M0L6_2acntS3075 > 1) {
    int32_t _M0L11_2anew__cntS3078 = _M0L6_2acntS3075 - 1;
    Moonbit_object_header(_M0L5entryS563)->rc = _M0L11_2anew__cntS3078;
    if (_M0L8_2afieldS2891) {
      moonbit_incref(_M0L8_2afieldS2891);
    }
  } else if (_M0L6_2acntS3075 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3077 =
      _M0L5entryS563->$5;
    moonbit_string_t _M0L8_2afieldS3076;
    moonbit_decref(_M0L8_2afieldS3077);
    _M0L8_2afieldS3076 = _M0L5entryS563->$4;
    moonbit_decref(_M0L8_2afieldS3076);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS563);
  }
  _M0L7_2abindS564 = _M0L8_2afieldS2891;
  if (_M0L7_2abindS564 == 0) {
    if (_M0L7_2abindS564) {
      moonbit_decref(_M0L7_2abindS564);
    }
    _M0L4selfS561->$6 = _M0L8new__idxS562;
    moonbit_decref(_M0L4selfS561);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS565;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS566;
    moonbit_decref(_M0L4selfS561);
    _M0L7_2aSomeS565 = _M0L7_2abindS564;
    _M0L7_2anextS566 = _M0L7_2aSomeS565;
    _M0L7_2anextS566->$0 = _M0L8new__idxS562;
    moonbit_decref(_M0L7_2anextS566);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS567,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS569,
  int32_t _M0L8new__idxS568
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2896;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1889;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1890;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2895;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2894;
  int32_t _M0L6_2acntS3079;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS570;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2896 = _M0L4selfS567->$0;
  _M0L7entriesS1889 = _M0L8_2afieldS2896;
  moonbit_incref(_M0L5entryS569);
  _M0L6_2atmpS1890 = _M0L5entryS569;
  if (
    _M0L8new__idxS568 < 0
    || _M0L8new__idxS568 >= Moonbit_array_length(_M0L7entriesS1889)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2895
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1889[
      _M0L8new__idxS568
    ];
  if (_M0L6_2aoldS2895) {
    moonbit_decref(_M0L6_2aoldS2895);
  }
  _M0L7entriesS1889[_M0L8new__idxS568] = _M0L6_2atmpS1890;
  _M0L8_2afieldS2894 = _M0L5entryS569->$1;
  _M0L6_2acntS3079 = Moonbit_object_header(_M0L5entryS569)->rc;
  if (_M0L6_2acntS3079 > 1) {
    int32_t _M0L11_2anew__cntS3081 = _M0L6_2acntS3079 - 1;
    Moonbit_object_header(_M0L5entryS569)->rc = _M0L11_2anew__cntS3081;
    if (_M0L8_2afieldS2894) {
      moonbit_incref(_M0L8_2afieldS2894);
    }
  } else if (_M0L6_2acntS3079 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3080 =
      _M0L5entryS569->$5;
    moonbit_decref(_M0L8_2afieldS3080);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS569);
  }
  _M0L7_2abindS570 = _M0L8_2afieldS2894;
  if (_M0L7_2abindS570 == 0) {
    if (_M0L7_2abindS570) {
      moonbit_decref(_M0L7_2abindS570);
    }
    _M0L4selfS567->$6 = _M0L8new__idxS568;
    moonbit_decref(_M0L4selfS567);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS571;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS572;
    moonbit_decref(_M0L4selfS567);
    _M0L7_2aSomeS571 = _M0L7_2abindS570;
    _M0L7_2anextS572 = _M0L7_2aSomeS571;
    _M0L7_2anextS572->$0 = _M0L8new__idxS568;
    moonbit_decref(_M0L7_2anextS572);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS573,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS575,
  int32_t _M0L8new__idxS574
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS2899;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS1891;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS1892;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2898;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2897;
  int32_t _M0L6_2acntS3082;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS576;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2899 = _M0L4selfS573->$0;
  _M0L7entriesS1891 = _M0L8_2afieldS2899;
  moonbit_incref(_M0L5entryS575);
  _M0L6_2atmpS1892 = _M0L5entryS575;
  if (
    _M0L8new__idxS574 < 0
    || _M0L8new__idxS574 >= Moonbit_array_length(_M0L7entriesS1891)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2898
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS1891[_M0L8new__idxS574];
  if (_M0L6_2aoldS2898) {
    moonbit_decref(_M0L6_2aoldS2898);
  }
  _M0L7entriesS1891[_M0L8new__idxS574] = _M0L6_2atmpS1892;
  _M0L8_2afieldS2897 = _M0L5entryS575->$1;
  _M0L6_2acntS3082 = Moonbit_object_header(_M0L5entryS575)->rc;
  if (_M0L6_2acntS3082 > 1) {
    int32_t _M0L11_2anew__cntS3085 = _M0L6_2acntS3082 - 1;
    Moonbit_object_header(_M0L5entryS575)->rc = _M0L11_2anew__cntS3085;
    if (_M0L8_2afieldS2897) {
      moonbit_incref(_M0L8_2afieldS2897);
    }
  } else if (_M0L6_2acntS3082 == 1) {
    void* _M0L8_2afieldS3084 = _M0L5entryS575->$5;
    moonbit_string_t _M0L8_2afieldS3083;
    moonbit_decref(_M0L8_2afieldS3084);
    _M0L8_2afieldS3083 = _M0L5entryS575->$4;
    moonbit_decref(_M0L8_2afieldS3083);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS575);
  }
  _M0L7_2abindS576 = _M0L8_2afieldS2897;
  if (_M0L7_2abindS576 == 0) {
    if (_M0L7_2abindS576) {
      moonbit_decref(_M0L7_2abindS576);
    }
    _M0L4selfS573->$6 = _M0L8new__idxS574;
    moonbit_decref(_M0L4selfS573);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS577;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS578;
    moonbit_decref(_M0L4selfS573);
    _M0L7_2aSomeS577 = _M0L7_2abindS576;
    _M0L7_2anextS578 = _M0L7_2aSomeS577;
    _M0L7_2anextS578->$0 = _M0L8new__idxS574;
    moonbit_decref(_M0L7_2anextS578);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS550,
  int32_t _M0L3idxS552,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS551
) {
  int32_t _M0L7_2abindS549;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2901;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1865;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1866;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2900;
  int32_t _M0L4sizeS1868;
  int32_t _M0L6_2atmpS1867;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS549 = _M0L4selfS550->$6;
  switch (_M0L7_2abindS549) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1860;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2902;
      moonbit_incref(_M0L5entryS551);
      _M0L6_2atmpS1860 = _M0L5entryS551;
      _M0L6_2aoldS2902 = _M0L4selfS550->$5;
      if (_M0L6_2aoldS2902) {
        moonbit_decref(_M0L6_2aoldS2902);
      }
      _M0L4selfS550->$5 = _M0L6_2atmpS1860;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2905 =
        _M0L4selfS550->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1864 =
        _M0L8_2afieldS2905;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2904;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1863;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1861;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1862;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2903;
      if (
        _M0L7_2abindS549 < 0
        || _M0L7_2abindS549 >= Moonbit_array_length(_M0L7entriesS1864)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2904
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1864[
          _M0L7_2abindS549
        ];
      _M0L6_2atmpS1863 = _M0L6_2atmpS2904;
      if (_M0L6_2atmpS1863) {
        moonbit_incref(_M0L6_2atmpS1863);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1861
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1863);
      moonbit_incref(_M0L5entryS551);
      _M0L6_2atmpS1862 = _M0L5entryS551;
      _M0L6_2aoldS2903 = _M0L6_2atmpS1861->$1;
      if (_M0L6_2aoldS2903) {
        moonbit_decref(_M0L6_2aoldS2903);
      }
      _M0L6_2atmpS1861->$1 = _M0L6_2atmpS1862;
      moonbit_decref(_M0L6_2atmpS1861);
      break;
    }
  }
  _M0L4selfS550->$6 = _M0L3idxS552;
  _M0L8_2afieldS2901 = _M0L4selfS550->$0;
  _M0L7entriesS1865 = _M0L8_2afieldS2901;
  _M0L6_2atmpS1866 = _M0L5entryS551;
  if (
    _M0L3idxS552 < 0
    || _M0L3idxS552 >= Moonbit_array_length(_M0L7entriesS1865)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2900
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1865[
      _M0L3idxS552
    ];
  if (_M0L6_2aoldS2900) {
    moonbit_decref(_M0L6_2aoldS2900);
  }
  _M0L7entriesS1865[_M0L3idxS552] = _M0L6_2atmpS1866;
  _M0L4sizeS1868 = _M0L4selfS550->$1;
  _M0L6_2atmpS1867 = _M0L4sizeS1868 + 1;
  _M0L4selfS550->$1 = _M0L6_2atmpS1867;
  moonbit_decref(_M0L4selfS550);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS554,
  int32_t _M0L3idxS556,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS555
) {
  int32_t _M0L7_2abindS553;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2907;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1874;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1875;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2906;
  int32_t _M0L4sizeS1877;
  int32_t _M0L6_2atmpS1876;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS553 = _M0L4selfS554->$6;
  switch (_M0L7_2abindS553) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1869;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2908;
      moonbit_incref(_M0L5entryS555);
      _M0L6_2atmpS1869 = _M0L5entryS555;
      _M0L6_2aoldS2908 = _M0L4selfS554->$5;
      if (_M0L6_2aoldS2908) {
        moonbit_decref(_M0L6_2aoldS2908);
      }
      _M0L4selfS554->$5 = _M0L6_2atmpS1869;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2911 =
        _M0L4selfS554->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1873 =
        _M0L8_2afieldS2911;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2910;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1872;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1870;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1871;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2909;
      if (
        _M0L7_2abindS553 < 0
        || _M0L7_2abindS553 >= Moonbit_array_length(_M0L7entriesS1873)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2910
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1873[
          _M0L7_2abindS553
        ];
      _M0L6_2atmpS1872 = _M0L6_2atmpS2910;
      if (_M0L6_2atmpS1872) {
        moonbit_incref(_M0L6_2atmpS1872);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1870
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1872);
      moonbit_incref(_M0L5entryS555);
      _M0L6_2atmpS1871 = _M0L5entryS555;
      _M0L6_2aoldS2909 = _M0L6_2atmpS1870->$1;
      if (_M0L6_2aoldS2909) {
        moonbit_decref(_M0L6_2aoldS2909);
      }
      _M0L6_2atmpS1870->$1 = _M0L6_2atmpS1871;
      moonbit_decref(_M0L6_2atmpS1870);
      break;
    }
  }
  _M0L4selfS554->$6 = _M0L3idxS556;
  _M0L8_2afieldS2907 = _M0L4selfS554->$0;
  _M0L7entriesS1874 = _M0L8_2afieldS2907;
  _M0L6_2atmpS1875 = _M0L5entryS555;
  if (
    _M0L3idxS556 < 0
    || _M0L3idxS556 >= Moonbit_array_length(_M0L7entriesS1874)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2906
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1874[
      _M0L3idxS556
    ];
  if (_M0L6_2aoldS2906) {
    moonbit_decref(_M0L6_2aoldS2906);
  }
  _M0L7entriesS1874[_M0L3idxS556] = _M0L6_2atmpS1875;
  _M0L4sizeS1877 = _M0L4selfS554->$1;
  _M0L6_2atmpS1876 = _M0L4sizeS1877 + 1;
  _M0L4selfS554->$1 = _M0L6_2atmpS1876;
  moonbit_decref(_M0L4selfS554);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS558,
  int32_t _M0L3idxS560,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS559
) {
  int32_t _M0L7_2abindS557;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS2913;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS1883;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS1884;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2912;
  int32_t _M0L4sizeS1886;
  int32_t _M0L6_2atmpS1885;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS557 = _M0L4selfS558->$6;
  switch (_M0L7_2abindS557) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS1878;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2914;
      moonbit_incref(_M0L5entryS559);
      _M0L6_2atmpS1878 = _M0L5entryS559;
      _M0L6_2aoldS2914 = _M0L4selfS558->$5;
      if (_M0L6_2aoldS2914) {
        moonbit_decref(_M0L6_2aoldS2914);
      }
      _M0L4selfS558->$5 = _M0L6_2atmpS1878;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS2917 = _M0L4selfS558->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS1882 = _M0L8_2afieldS2917;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2916;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS1881;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS1879;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS1880;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2915;
      if (
        _M0L7_2abindS557 < 0
        || _M0L7_2abindS557 >= Moonbit_array_length(_M0L7entriesS1882)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2916
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS1882[_M0L7_2abindS557];
      _M0L6_2atmpS1881 = _M0L6_2atmpS2916;
      if (_M0L6_2atmpS1881) {
        moonbit_incref(_M0L6_2atmpS1881);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1879
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS1881);
      moonbit_incref(_M0L5entryS559);
      _M0L6_2atmpS1880 = _M0L5entryS559;
      _M0L6_2aoldS2915 = _M0L6_2atmpS1879->$1;
      if (_M0L6_2aoldS2915) {
        moonbit_decref(_M0L6_2aoldS2915);
      }
      _M0L6_2atmpS1879->$1 = _M0L6_2atmpS1880;
      moonbit_decref(_M0L6_2atmpS1879);
      break;
    }
  }
  _M0L4selfS558->$6 = _M0L3idxS560;
  _M0L8_2afieldS2913 = _M0L4selfS558->$0;
  _M0L7entriesS1883 = _M0L8_2afieldS2913;
  _M0L6_2atmpS1884 = _M0L5entryS559;
  if (
    _M0L3idxS560 < 0
    || _M0L3idxS560 >= Moonbit_array_length(_M0L7entriesS1883)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2912
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS1883[_M0L3idxS560];
  if (_M0L6_2aoldS2912) {
    moonbit_decref(_M0L6_2aoldS2912);
  }
  _M0L7entriesS1883[_M0L3idxS560] = _M0L6_2atmpS1884;
  _M0L4sizeS1886 = _M0L4selfS558->$1;
  _M0L6_2atmpS1885 = _M0L4sizeS1886 + 1;
  _M0L4selfS558->$1 = _M0L6_2atmpS1885;
  moonbit_decref(_M0L4selfS558);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS532
) {
  int32_t _M0L8capacityS531;
  int32_t _M0L7_2abindS533;
  int32_t _M0L7_2abindS534;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1857;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS535;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS536;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3198;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS531
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS532);
  _M0L7_2abindS533 = _M0L8capacityS531 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS534 = _M0FPB21calc__grow__threshold(_M0L8capacityS531);
  _M0L6_2atmpS1857 = 0;
  _M0L7_2abindS535
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS531, _M0L6_2atmpS1857);
  _M0L7_2abindS536 = 0;
  _block_3198
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3198)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3198->$0 = _M0L7_2abindS535;
  _block_3198->$1 = 0;
  _block_3198->$2 = _M0L8capacityS531;
  _block_3198->$3 = _M0L7_2abindS533;
  _block_3198->$4 = _M0L7_2abindS534;
  _block_3198->$5 = _M0L7_2abindS536;
  _block_3198->$6 = -1;
  return _block_3198;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS538
) {
  int32_t _M0L8capacityS537;
  int32_t _M0L7_2abindS539;
  int32_t _M0L7_2abindS540;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1858;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS541;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS542;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3199;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS537
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS538);
  _M0L7_2abindS539 = _M0L8capacityS537 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS540 = _M0FPB21calc__grow__threshold(_M0L8capacityS537);
  _M0L6_2atmpS1858 = 0;
  _M0L7_2abindS541
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS537, _M0L6_2atmpS1858);
  _M0L7_2abindS542 = 0;
  _block_3199
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3199)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3199->$0 = _M0L7_2abindS541;
  _block_3199->$1 = 0;
  _block_3199->$2 = _M0L8capacityS537;
  _block_3199->$3 = _M0L7_2abindS539;
  _block_3199->$4 = _M0L7_2abindS540;
  _block_3199->$5 = _M0L7_2abindS542;
  _block_3199->$6 = -1;
  return _block_3199;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS544
) {
  int32_t _M0L8capacityS543;
  int32_t _M0L7_2abindS545;
  int32_t _M0L7_2abindS546;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS1859;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS547;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS548;
  struct _M0TPB3MapGsRPB4JsonE* _block_3200;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS543
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS544);
  _M0L7_2abindS545 = _M0L8capacityS543 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS546 = _M0FPB21calc__grow__threshold(_M0L8capacityS543);
  _M0L6_2atmpS1859 = 0;
  _M0L7_2abindS547
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS543, _M0L6_2atmpS1859);
  _M0L7_2abindS548 = 0;
  _block_3200
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_3200)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_3200->$0 = _M0L7_2abindS547;
  _block_3200->$1 = 0;
  _block_3200->$2 = _M0L8capacityS543;
  _block_3200->$3 = _M0L7_2abindS545;
  _block_3200->$4 = _M0L7_2abindS546;
  _block_3200->$5 = _M0L7_2abindS548;
  _block_3200->$6 = -1;
  return _block_3200;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS530) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS530 >= 0) {
    int32_t _M0L6_2atmpS1856;
    int32_t _M0L6_2atmpS1855;
    int32_t _M0L6_2atmpS1854;
    int32_t _M0L6_2atmpS1853;
    if (_M0L4selfS530 <= 1) {
      return 1;
    }
    if (_M0L4selfS530 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1856 = _M0L4selfS530 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1855 = moonbit_clz32(_M0L6_2atmpS1856);
    _M0L6_2atmpS1854 = _M0L6_2atmpS1855 - 1;
    _M0L6_2atmpS1853 = 2147483647 >> (_M0L6_2atmpS1854 & 31);
    return _M0L6_2atmpS1853 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS529) {
  int32_t _M0L6_2atmpS1852;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1852 = _M0L8capacityS529 * 13;
  return _M0L6_2atmpS1852 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS523
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS523 == 0) {
    if (_M0L4selfS523) {
      moonbit_decref(_M0L4selfS523);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS524 =
      _M0L4selfS523;
    return _M0L7_2aSomeS524;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS525
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS525 == 0) {
    if (_M0L4selfS525) {
      moonbit_decref(_M0L4selfS525);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS526 =
      _M0L4selfS525;
    return _M0L7_2aSomeS526;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS527
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS527 == 0) {
    if (_M0L4selfS527) {
      moonbit_decref(_M0L4selfS527);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS528 = _M0L4selfS527;
    return _M0L7_2aSomeS528;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS522
) {
  moonbit_string_t* _M0L6_2atmpS1851;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1851 = _M0L4selfS522;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1851);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS518,
  int32_t _M0L5indexS519
) {
  uint64_t* _M0L6_2atmpS1849;
  uint64_t _M0L6_2atmpS2918;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1849 = _M0L4selfS518;
  if (
    _M0L5indexS519 < 0
    || _M0L5indexS519 >= Moonbit_array_length(_M0L6_2atmpS1849)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2918 = (uint64_t)_M0L6_2atmpS1849[_M0L5indexS519];
  moonbit_decref(_M0L6_2atmpS1849);
  return _M0L6_2atmpS2918;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS520,
  int32_t _M0L5indexS521
) {
  uint32_t* _M0L6_2atmpS1850;
  uint32_t _M0L6_2atmpS2919;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1850 = _M0L4selfS520;
  if (
    _M0L5indexS521 < 0
    || _M0L5indexS521 >= Moonbit_array_length(_M0L6_2atmpS1850)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2919 = (uint32_t)_M0L6_2atmpS1850[_M0L5indexS521];
  moonbit_decref(_M0L6_2atmpS1850);
  return _M0L6_2atmpS2919;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS517
) {
  moonbit_string_t* _M0L6_2atmpS1847;
  int32_t _M0L6_2atmpS2920;
  int32_t _M0L6_2atmpS1848;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1846;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS517);
  _M0L6_2atmpS1847 = _M0L4selfS517;
  _M0L6_2atmpS2920 = Moonbit_array_length(_M0L4selfS517);
  moonbit_decref(_M0L4selfS517);
  _M0L6_2atmpS1848 = _M0L6_2atmpS2920;
  _M0L6_2atmpS1846
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1848, _M0L6_2atmpS1847
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1846);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS515
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS514;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__* _closure_3201;
  struct _M0TWEOs* _M0L6_2atmpS1834;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS514
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS514)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS514->$0 = 0;
  _closure_3201
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__));
  Moonbit_object_header(_closure_3201)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__, $0_0) >> 2, 2, 0);
  _closure_3201->code = &_M0MPC15array9ArrayView4iterGsEC1835l570;
  _closure_3201->$0_0 = _M0L4selfS515.$0;
  _closure_3201->$0_1 = _M0L4selfS515.$1;
  _closure_3201->$0_2 = _M0L4selfS515.$2;
  _closure_3201->$1 = _M0L1iS514;
  _M0L6_2atmpS1834 = (struct _M0TWEOs*)_closure_3201;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1834);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1835l570(
  struct _M0TWEOs* _M0L6_2aenvS1836
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__* _M0L14_2acasted__envS1837;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2925;
  struct _M0TPC13ref3RefGiE* _M0L1iS514;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2924;
  int32_t _M0L6_2acntS3086;
  struct _M0TPB9ArrayViewGsE _M0L4selfS515;
  int32_t _M0L3valS1838;
  int32_t _M0L6_2atmpS1839;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1837
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1835__l570__*)_M0L6_2aenvS1836;
  _M0L8_2afieldS2925 = _M0L14_2acasted__envS1837->$1;
  _M0L1iS514 = _M0L8_2afieldS2925;
  _M0L8_2afieldS2924
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1837->$0_1,
      _M0L14_2acasted__envS1837->$0_2,
      _M0L14_2acasted__envS1837->$0_0
  };
  _M0L6_2acntS3086 = Moonbit_object_header(_M0L14_2acasted__envS1837)->rc;
  if (_M0L6_2acntS3086 > 1) {
    int32_t _M0L11_2anew__cntS3087 = _M0L6_2acntS3086 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1837)->rc
    = _M0L11_2anew__cntS3087;
    moonbit_incref(_M0L1iS514);
    moonbit_incref(_M0L8_2afieldS2924.$0);
  } else if (_M0L6_2acntS3086 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1837);
  }
  _M0L4selfS515 = _M0L8_2afieldS2924;
  _M0L3valS1838 = _M0L1iS514->$0;
  moonbit_incref(_M0L4selfS515.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1839 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS515);
  if (_M0L3valS1838 < _M0L6_2atmpS1839) {
    moonbit_string_t* _M0L8_2afieldS2923 = _M0L4selfS515.$0;
    moonbit_string_t* _M0L3bufS1842 = _M0L8_2afieldS2923;
    int32_t _M0L8_2afieldS2922 = _M0L4selfS515.$1;
    int32_t _M0L5startS1844 = _M0L8_2afieldS2922;
    int32_t _M0L3valS1845 = _M0L1iS514->$0;
    int32_t _M0L6_2atmpS1843 = _M0L5startS1844 + _M0L3valS1845;
    moonbit_string_t _M0L6_2atmpS2921 =
      (moonbit_string_t)_M0L3bufS1842[_M0L6_2atmpS1843];
    moonbit_string_t _M0L4elemS516;
    int32_t _M0L3valS1841;
    int32_t _M0L6_2atmpS1840;
    moonbit_incref(_M0L6_2atmpS2921);
    moonbit_decref(_M0L3bufS1842);
    _M0L4elemS516 = _M0L6_2atmpS2921;
    _M0L3valS1841 = _M0L1iS514->$0;
    _M0L6_2atmpS1840 = _M0L3valS1841 + 1;
    _M0L1iS514->$0 = _M0L6_2atmpS1840;
    moonbit_decref(_M0L1iS514);
    return _M0L4elemS516;
  } else {
    moonbit_decref(_M0L4selfS515.$0);
    moonbit_decref(_M0L1iS514);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS513
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS513;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS512,
  struct _M0TPB6Logger _M0L6loggerS511
) {
  moonbit_string_t _M0L6_2atmpS1833;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1833
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS512, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS511.$0->$method_0(_M0L6loggerS511.$1, _M0L6_2atmpS1833);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS510,
  struct _M0TPB6Logger _M0L6loggerS509
) {
  moonbit_string_t _M0L6_2atmpS1832;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1832 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS510, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS509.$0->$method_0(_M0L6loggerS509.$1, _M0L6_2atmpS1832);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS504) {
  int32_t _M0L3lenS503;
  struct _M0TPC13ref3RefGiE* _M0L5indexS505;
  struct _M0R38String_3a_3aiter_2eanon__u1816__l247__* _closure_3202;
  struct _M0TWEOc* _M0L6_2atmpS1815;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS503 = Moonbit_array_length(_M0L4selfS504);
  _M0L5indexS505
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS505)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS505->$0 = 0;
  _closure_3202
  = (struct _M0R38String_3a_3aiter_2eanon__u1816__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u1816__l247__));
  Moonbit_object_header(_closure_3202)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u1816__l247__, $0) >> 2, 2, 0);
  _closure_3202->code = &_M0MPC16string6String4iterC1816l247;
  _closure_3202->$0 = _M0L5indexS505;
  _closure_3202->$1 = _M0L4selfS504;
  _closure_3202->$2 = _M0L3lenS503;
  _M0L6_2atmpS1815 = (struct _M0TWEOc*)_closure_3202;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1815);
}

int32_t _M0MPC16string6String4iterC1816l247(
  struct _M0TWEOc* _M0L6_2aenvS1817
) {
  struct _M0R38String_3a_3aiter_2eanon__u1816__l247__* _M0L14_2acasted__envS1818;
  int32_t _M0L3lenS503;
  moonbit_string_t _M0L8_2afieldS2928;
  moonbit_string_t _M0L4selfS504;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2927;
  int32_t _M0L6_2acntS3088;
  struct _M0TPC13ref3RefGiE* _M0L5indexS505;
  int32_t _M0L3valS1819;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS1818
  = (struct _M0R38String_3a_3aiter_2eanon__u1816__l247__*)_M0L6_2aenvS1817;
  _M0L3lenS503 = _M0L14_2acasted__envS1818->$2;
  _M0L8_2afieldS2928 = _M0L14_2acasted__envS1818->$1;
  _M0L4selfS504 = _M0L8_2afieldS2928;
  _M0L8_2afieldS2927 = _M0L14_2acasted__envS1818->$0;
  _M0L6_2acntS3088 = Moonbit_object_header(_M0L14_2acasted__envS1818)->rc;
  if (_M0L6_2acntS3088 > 1) {
    int32_t _M0L11_2anew__cntS3089 = _M0L6_2acntS3088 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1818)->rc
    = _M0L11_2anew__cntS3089;
    moonbit_incref(_M0L4selfS504);
    moonbit_incref(_M0L8_2afieldS2927);
  } else if (_M0L6_2acntS3088 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS1818);
  }
  _M0L5indexS505 = _M0L8_2afieldS2927;
  _M0L3valS1819 = _M0L5indexS505->$0;
  if (_M0L3valS1819 < _M0L3lenS503) {
    int32_t _M0L3valS1831 = _M0L5indexS505->$0;
    int32_t _M0L2c1S506 = _M0L4selfS504[_M0L3valS1831];
    int32_t _if__result_3203;
    int32_t _M0L3valS1829;
    int32_t _M0L6_2atmpS1828;
    int32_t _M0L6_2atmpS1830;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S506)) {
      int32_t _M0L3valS1821 = _M0L5indexS505->$0;
      int32_t _M0L6_2atmpS1820 = _M0L3valS1821 + 1;
      _if__result_3203 = _M0L6_2atmpS1820 < _M0L3lenS503;
    } else {
      _if__result_3203 = 0;
    }
    if (_if__result_3203) {
      int32_t _M0L3valS1827 = _M0L5indexS505->$0;
      int32_t _M0L6_2atmpS1826 = _M0L3valS1827 + 1;
      int32_t _M0L6_2atmpS2926 = _M0L4selfS504[_M0L6_2atmpS1826];
      int32_t _M0L2c2S507;
      moonbit_decref(_M0L4selfS504);
      _M0L2c2S507 = _M0L6_2atmpS2926;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S507)) {
        int32_t _M0L6_2atmpS1824 = (int32_t)_M0L2c1S506;
        int32_t _M0L6_2atmpS1825 = (int32_t)_M0L2c2S507;
        int32_t _M0L1cS508;
        int32_t _M0L3valS1823;
        int32_t _M0L6_2atmpS1822;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS508
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1824, _M0L6_2atmpS1825);
        _M0L3valS1823 = _M0L5indexS505->$0;
        _M0L6_2atmpS1822 = _M0L3valS1823 + 2;
        _M0L5indexS505->$0 = _M0L6_2atmpS1822;
        moonbit_decref(_M0L5indexS505);
        return _M0L1cS508;
      }
    } else {
      moonbit_decref(_M0L4selfS504);
    }
    _M0L3valS1829 = _M0L5indexS505->$0;
    _M0L6_2atmpS1828 = _M0L3valS1829 + 1;
    _M0L5indexS505->$0 = _M0L6_2atmpS1828;
    moonbit_decref(_M0L5indexS505);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS1830 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S506);
    return _M0L6_2atmpS1830;
  } else {
    moonbit_decref(_M0L5indexS505);
    moonbit_decref(_M0L4selfS504);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS494,
  moonbit_string_t _M0L5valueS496
) {
  int32_t _M0L3lenS1800;
  moonbit_string_t* _M0L6_2atmpS1802;
  int32_t _M0L6_2atmpS2931;
  int32_t _M0L6_2atmpS1801;
  int32_t _M0L6lengthS495;
  moonbit_string_t* _M0L8_2afieldS2930;
  moonbit_string_t* _M0L3bufS1803;
  moonbit_string_t _M0L6_2aoldS2929;
  int32_t _M0L6_2atmpS1804;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1800 = _M0L4selfS494->$1;
  moonbit_incref(_M0L4selfS494);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1802 = _M0MPC15array5Array6bufferGsE(_M0L4selfS494);
  _M0L6_2atmpS2931 = Moonbit_array_length(_M0L6_2atmpS1802);
  moonbit_decref(_M0L6_2atmpS1802);
  _M0L6_2atmpS1801 = _M0L6_2atmpS2931;
  if (_M0L3lenS1800 == _M0L6_2atmpS1801) {
    moonbit_incref(_M0L4selfS494);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS494);
  }
  _M0L6lengthS495 = _M0L4selfS494->$1;
  _M0L8_2afieldS2930 = _M0L4selfS494->$0;
  _M0L3bufS1803 = _M0L8_2afieldS2930;
  _M0L6_2aoldS2929 = (moonbit_string_t)_M0L3bufS1803[_M0L6lengthS495];
  moonbit_decref(_M0L6_2aoldS2929);
  _M0L3bufS1803[_M0L6lengthS495] = _M0L5valueS496;
  _M0L6_2atmpS1804 = _M0L6lengthS495 + 1;
  _M0L4selfS494->$1 = _M0L6_2atmpS1804;
  moonbit_decref(_M0L4selfS494);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS497,
  struct _M0TUsiE* _M0L5valueS499
) {
  int32_t _M0L3lenS1805;
  struct _M0TUsiE** _M0L6_2atmpS1807;
  int32_t _M0L6_2atmpS2934;
  int32_t _M0L6_2atmpS1806;
  int32_t _M0L6lengthS498;
  struct _M0TUsiE** _M0L8_2afieldS2933;
  struct _M0TUsiE** _M0L3bufS1808;
  struct _M0TUsiE* _M0L6_2aoldS2932;
  int32_t _M0L6_2atmpS1809;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1805 = _M0L4selfS497->$1;
  moonbit_incref(_M0L4selfS497);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1807 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS497);
  _M0L6_2atmpS2934 = Moonbit_array_length(_M0L6_2atmpS1807);
  moonbit_decref(_M0L6_2atmpS1807);
  _M0L6_2atmpS1806 = _M0L6_2atmpS2934;
  if (_M0L3lenS1805 == _M0L6_2atmpS1806) {
    moonbit_incref(_M0L4selfS497);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS497);
  }
  _M0L6lengthS498 = _M0L4selfS497->$1;
  _M0L8_2afieldS2933 = _M0L4selfS497->$0;
  _M0L3bufS1808 = _M0L8_2afieldS2933;
  _M0L6_2aoldS2932 = (struct _M0TUsiE*)_M0L3bufS1808[_M0L6lengthS498];
  if (_M0L6_2aoldS2932) {
    moonbit_decref(_M0L6_2aoldS2932);
  }
  _M0L3bufS1808[_M0L6lengthS498] = _M0L5valueS499;
  _M0L6_2atmpS1809 = _M0L6lengthS498 + 1;
  _M0L4selfS497->$1 = _M0L6_2atmpS1809;
  moonbit_decref(_M0L4selfS497);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS500,
  void* _M0L5valueS502
) {
  int32_t _M0L3lenS1810;
  void** _M0L6_2atmpS1812;
  int32_t _M0L6_2atmpS2937;
  int32_t _M0L6_2atmpS1811;
  int32_t _M0L6lengthS501;
  void** _M0L8_2afieldS2936;
  void** _M0L3bufS1813;
  void* _M0L6_2aoldS2935;
  int32_t _M0L6_2atmpS1814;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1810 = _M0L4selfS500->$1;
  moonbit_incref(_M0L4selfS500);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1812
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS500);
  _M0L6_2atmpS2937 = Moonbit_array_length(_M0L6_2atmpS1812);
  moonbit_decref(_M0L6_2atmpS1812);
  _M0L6_2atmpS1811 = _M0L6_2atmpS2937;
  if (_M0L3lenS1810 == _M0L6_2atmpS1811) {
    moonbit_incref(_M0L4selfS500);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS500);
  }
  _M0L6lengthS501 = _M0L4selfS500->$1;
  _M0L8_2afieldS2936 = _M0L4selfS500->$0;
  _M0L3bufS1813 = _M0L8_2afieldS2936;
  _M0L6_2aoldS2935 = (void*)_M0L3bufS1813[_M0L6lengthS501];
  moonbit_decref(_M0L6_2aoldS2935);
  _M0L3bufS1813[_M0L6lengthS501] = _M0L5valueS502;
  _M0L6_2atmpS1814 = _M0L6lengthS501 + 1;
  _M0L4selfS500->$1 = _M0L6_2atmpS1814;
  moonbit_decref(_M0L4selfS500);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS486) {
  int32_t _M0L8old__capS485;
  int32_t _M0L8new__capS487;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS485 = _M0L4selfS486->$1;
  if (_M0L8old__capS485 == 0) {
    _M0L8new__capS487 = 8;
  } else {
    _M0L8new__capS487 = _M0L8old__capS485 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS486, _M0L8new__capS487);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS489
) {
  int32_t _M0L8old__capS488;
  int32_t _M0L8new__capS490;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS488 = _M0L4selfS489->$1;
  if (_M0L8old__capS488 == 0) {
    _M0L8new__capS490 = 8;
  } else {
    _M0L8new__capS490 = _M0L8old__capS488 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS489, _M0L8new__capS490);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS492
) {
  int32_t _M0L8old__capS491;
  int32_t _M0L8new__capS493;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS491 = _M0L4selfS492->$1;
  if (_M0L8old__capS491 == 0) {
    _M0L8new__capS493 = 8;
  } else {
    _M0L8new__capS493 = _M0L8old__capS491 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS492, _M0L8new__capS493);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS470,
  int32_t _M0L13new__capacityS468
) {
  moonbit_string_t* _M0L8new__bufS467;
  moonbit_string_t* _M0L8_2afieldS2939;
  moonbit_string_t* _M0L8old__bufS469;
  int32_t _M0L8old__capS471;
  int32_t _M0L9copy__lenS472;
  moonbit_string_t* _M0L6_2aoldS2938;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS467
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS468, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS2939 = _M0L4selfS470->$0;
  _M0L8old__bufS469 = _M0L8_2afieldS2939;
  _M0L8old__capS471 = Moonbit_array_length(_M0L8old__bufS469);
  if (_M0L8old__capS471 < _M0L13new__capacityS468) {
    _M0L9copy__lenS472 = _M0L8old__capS471;
  } else {
    _M0L9copy__lenS472 = _M0L13new__capacityS468;
  }
  moonbit_incref(_M0L8old__bufS469);
  moonbit_incref(_M0L8new__bufS467);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS467, 0, _M0L8old__bufS469, 0, _M0L9copy__lenS472);
  _M0L6_2aoldS2938 = _M0L4selfS470->$0;
  moonbit_decref(_M0L6_2aoldS2938);
  _M0L4selfS470->$0 = _M0L8new__bufS467;
  moonbit_decref(_M0L4selfS470);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS476,
  int32_t _M0L13new__capacityS474
) {
  struct _M0TUsiE** _M0L8new__bufS473;
  struct _M0TUsiE** _M0L8_2afieldS2941;
  struct _M0TUsiE** _M0L8old__bufS475;
  int32_t _M0L8old__capS477;
  int32_t _M0L9copy__lenS478;
  struct _M0TUsiE** _M0L6_2aoldS2940;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS473
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS474, 0);
  _M0L8_2afieldS2941 = _M0L4selfS476->$0;
  _M0L8old__bufS475 = _M0L8_2afieldS2941;
  _M0L8old__capS477 = Moonbit_array_length(_M0L8old__bufS475);
  if (_M0L8old__capS477 < _M0L13new__capacityS474) {
    _M0L9copy__lenS478 = _M0L8old__capS477;
  } else {
    _M0L9copy__lenS478 = _M0L13new__capacityS474;
  }
  moonbit_incref(_M0L8old__bufS475);
  moonbit_incref(_M0L8new__bufS473);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS473, 0, _M0L8old__bufS475, 0, _M0L9copy__lenS478);
  _M0L6_2aoldS2940 = _M0L4selfS476->$0;
  moonbit_decref(_M0L6_2aoldS2940);
  _M0L4selfS476->$0 = _M0L8new__bufS473;
  moonbit_decref(_M0L4selfS476);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS482,
  int32_t _M0L13new__capacityS480
) {
  void** _M0L8new__bufS479;
  void** _M0L8_2afieldS2943;
  void** _M0L8old__bufS481;
  int32_t _M0L8old__capS483;
  int32_t _M0L9copy__lenS484;
  void** _M0L6_2aoldS2942;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS479
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS480, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS2943 = _M0L4selfS482->$0;
  _M0L8old__bufS481 = _M0L8_2afieldS2943;
  _M0L8old__capS483 = Moonbit_array_length(_M0L8old__bufS481);
  if (_M0L8old__capS483 < _M0L13new__capacityS480) {
    _M0L9copy__lenS484 = _M0L8old__capS483;
  } else {
    _M0L9copy__lenS484 = _M0L13new__capacityS480;
  }
  moonbit_incref(_M0L8old__bufS481);
  moonbit_incref(_M0L8new__bufS479);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS479, 0, _M0L8old__bufS481, 0, _M0L9copy__lenS484);
  _M0L6_2aoldS2942 = _M0L4selfS482->$0;
  moonbit_decref(_M0L6_2aoldS2942);
  _M0L4selfS482->$0 = _M0L8new__bufS479;
  moonbit_decref(_M0L4selfS482);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS466
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS466 == 0) {
    moonbit_string_t* _M0L6_2atmpS1798 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3204 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3204)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3204->$0 = _M0L6_2atmpS1798;
    _block_3204->$1 = 0;
    return _block_3204;
  } else {
    moonbit_string_t* _M0L6_2atmpS1799 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS466, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3205 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3205)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3205->$0 = _M0L6_2atmpS1799;
    _block_3205->$1 = 0;
    return _block_3205;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS460,
  int32_t _M0L1nS459
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS459 <= 0) {
    moonbit_decref(_M0L4selfS460);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS459 == 1) {
    return _M0L4selfS460;
  } else {
    int32_t _M0L3lenS461 = Moonbit_array_length(_M0L4selfS460);
    int32_t _M0L6_2atmpS1797 = _M0L3lenS461 * _M0L1nS459;
    struct _M0TPB13StringBuilder* _M0L3bufS462;
    moonbit_string_t _M0L3strS463;
    int32_t _M0L2__S464;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS462 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1797);
    _M0L3strS463 = _M0L4selfS460;
    _M0L2__S464 = 0;
    while (1) {
      if (_M0L2__S464 < _M0L1nS459) {
        int32_t _M0L6_2atmpS1796;
        moonbit_incref(_M0L3strS463);
        moonbit_incref(_M0L3bufS462);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS462, _M0L3strS463);
        _M0L6_2atmpS1796 = _M0L2__S464 + 1;
        _M0L2__S464 = _M0L6_2atmpS1796;
        continue;
      } else {
        moonbit_decref(_M0L3strS463);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS462);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS457,
  struct _M0TPC16string10StringView _M0L3strS458
) {
  int32_t _M0L3lenS1784;
  int32_t _M0L6_2atmpS1786;
  int32_t _M0L6_2atmpS1785;
  int32_t _M0L6_2atmpS1783;
  moonbit_bytes_t _M0L8_2afieldS2944;
  moonbit_bytes_t _M0L4dataS1787;
  int32_t _M0L3lenS1788;
  moonbit_string_t _M0L6_2atmpS1789;
  int32_t _M0L6_2atmpS1790;
  int32_t _M0L6_2atmpS1791;
  int32_t _M0L3lenS1793;
  int32_t _M0L6_2atmpS1795;
  int32_t _M0L6_2atmpS1794;
  int32_t _M0L6_2atmpS1792;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1784 = _M0L4selfS457->$1;
  moonbit_incref(_M0L3strS458.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1786 = _M0MPC16string10StringView6length(_M0L3strS458);
  _M0L6_2atmpS1785 = _M0L6_2atmpS1786 * 2;
  _M0L6_2atmpS1783 = _M0L3lenS1784 + _M0L6_2atmpS1785;
  moonbit_incref(_M0L4selfS457);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS457, _M0L6_2atmpS1783);
  _M0L8_2afieldS2944 = _M0L4selfS457->$0;
  _M0L4dataS1787 = _M0L8_2afieldS2944;
  _M0L3lenS1788 = _M0L4selfS457->$1;
  moonbit_incref(_M0L4dataS1787);
  moonbit_incref(_M0L3strS458.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1789 = _M0MPC16string10StringView4data(_M0L3strS458);
  moonbit_incref(_M0L3strS458.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1790 = _M0MPC16string10StringView13start__offset(_M0L3strS458);
  moonbit_incref(_M0L3strS458.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1791 = _M0MPC16string10StringView6length(_M0L3strS458);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1787, _M0L3lenS1788, _M0L6_2atmpS1789, _M0L6_2atmpS1790, _M0L6_2atmpS1791);
  _M0L3lenS1793 = _M0L4selfS457->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1795 = _M0MPC16string10StringView6length(_M0L3strS458);
  _M0L6_2atmpS1794 = _M0L6_2atmpS1795 * 2;
  _M0L6_2atmpS1792 = _M0L3lenS1793 + _M0L6_2atmpS1794;
  _M0L4selfS457->$1 = _M0L6_2atmpS1792;
  moonbit_decref(_M0L4selfS457);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS449,
  int32_t _M0L3lenS452,
  int32_t _M0L13start__offsetS456,
  int64_t _M0L11end__offsetS447
) {
  int32_t _M0L11end__offsetS446;
  int32_t _M0L5indexS450;
  int32_t _M0L5countS451;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS447 == 4294967296ll) {
    _M0L11end__offsetS446 = Moonbit_array_length(_M0L4selfS449);
  } else {
    int64_t _M0L7_2aSomeS448 = _M0L11end__offsetS447;
    _M0L11end__offsetS446 = (int32_t)_M0L7_2aSomeS448;
  }
  _M0L5indexS450 = _M0L13start__offsetS456;
  _M0L5countS451 = 0;
  while (1) {
    int32_t _if__result_3208;
    if (_M0L5indexS450 < _M0L11end__offsetS446) {
      _if__result_3208 = _M0L5countS451 < _M0L3lenS452;
    } else {
      _if__result_3208 = 0;
    }
    if (_if__result_3208) {
      int32_t _M0L2c1S453 = _M0L4selfS449[_M0L5indexS450];
      int32_t _if__result_3209;
      int32_t _M0L6_2atmpS1781;
      int32_t _M0L6_2atmpS1782;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S453)) {
        int32_t _M0L6_2atmpS1777 = _M0L5indexS450 + 1;
        _if__result_3209 = _M0L6_2atmpS1777 < _M0L11end__offsetS446;
      } else {
        _if__result_3209 = 0;
      }
      if (_if__result_3209) {
        int32_t _M0L6_2atmpS1780 = _M0L5indexS450 + 1;
        int32_t _M0L2c2S454 = _M0L4selfS449[_M0L6_2atmpS1780];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S454)) {
          int32_t _M0L6_2atmpS1778 = _M0L5indexS450 + 2;
          int32_t _M0L6_2atmpS1779 = _M0L5countS451 + 1;
          _M0L5indexS450 = _M0L6_2atmpS1778;
          _M0L5countS451 = _M0L6_2atmpS1779;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_53.data, (moonbit_string_t)moonbit_string_literal_54.data);
        }
      }
      _M0L6_2atmpS1781 = _M0L5indexS450 + 1;
      _M0L6_2atmpS1782 = _M0L5countS451 + 1;
      _M0L5indexS450 = _M0L6_2atmpS1781;
      _M0L5countS451 = _M0L6_2atmpS1782;
      continue;
    } else {
      moonbit_decref(_M0L4selfS449);
      return _M0L5countS451 >= _M0L3lenS452;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS442
) {
  int32_t _M0L3endS1769;
  int32_t _M0L8_2afieldS2945;
  int32_t _M0L5startS1770;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1769 = _M0L4selfS442.$2;
  _M0L8_2afieldS2945 = _M0L4selfS442.$1;
  moonbit_decref(_M0L4selfS442.$0);
  _M0L5startS1770 = _M0L8_2afieldS2945;
  return _M0L3endS1769 - _M0L5startS1770;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS443
) {
  int32_t _M0L3endS1771;
  int32_t _M0L8_2afieldS2946;
  int32_t _M0L5startS1772;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1771 = _M0L4selfS443.$2;
  _M0L8_2afieldS2946 = _M0L4selfS443.$1;
  moonbit_decref(_M0L4selfS443.$0);
  _M0L5startS1772 = _M0L8_2afieldS2946;
  return _M0L3endS1771 - _M0L5startS1772;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS444
) {
  int32_t _M0L3endS1773;
  int32_t _M0L8_2afieldS2947;
  int32_t _M0L5startS1774;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1773 = _M0L4selfS444.$2;
  _M0L8_2afieldS2947 = _M0L4selfS444.$1;
  moonbit_decref(_M0L4selfS444.$0);
  _M0L5startS1774 = _M0L8_2afieldS2947;
  return _M0L3endS1773 - _M0L5startS1774;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS445
) {
  int32_t _M0L3endS1775;
  int32_t _M0L8_2afieldS2948;
  int32_t _M0L5startS1776;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1775 = _M0L4selfS445.$2;
  _M0L8_2afieldS2948 = _M0L4selfS445.$1;
  moonbit_decref(_M0L4selfS445.$0);
  _M0L5startS1776 = _M0L8_2afieldS2948;
  return _M0L3endS1775 - _M0L5startS1776;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS440,
  int64_t _M0L19start__offset_2eoptS438,
  int64_t _M0L11end__offsetS441
) {
  int32_t _M0L13start__offsetS437;
  if (_M0L19start__offset_2eoptS438 == 4294967296ll) {
    _M0L13start__offsetS437 = 0;
  } else {
    int64_t _M0L7_2aSomeS439 = _M0L19start__offset_2eoptS438;
    _M0L13start__offsetS437 = (int32_t)_M0L7_2aSomeS439;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS440, _M0L13start__offsetS437, _M0L11end__offsetS441);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS435,
  int32_t _M0L13start__offsetS436,
  int64_t _M0L11end__offsetS433
) {
  int32_t _M0L11end__offsetS432;
  int32_t _if__result_3210;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS433 == 4294967296ll) {
    _M0L11end__offsetS432 = Moonbit_array_length(_M0L4selfS435);
  } else {
    int64_t _M0L7_2aSomeS434 = _M0L11end__offsetS433;
    _M0L11end__offsetS432 = (int32_t)_M0L7_2aSomeS434;
  }
  if (_M0L13start__offsetS436 >= 0) {
    if (_M0L13start__offsetS436 <= _M0L11end__offsetS432) {
      int32_t _M0L6_2atmpS1768 = Moonbit_array_length(_M0L4selfS435);
      _if__result_3210 = _M0L11end__offsetS432 <= _M0L6_2atmpS1768;
    } else {
      _if__result_3210 = 0;
    }
  } else {
    _if__result_3210 = 0;
  }
  if (_if__result_3210) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS436,
                                                 _M0L11end__offsetS432,
                                                 _M0L4selfS435};
  } else {
    moonbit_decref(_M0L4selfS435);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_56.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS431
) {
  moonbit_string_t _M0L8_2afieldS2950;
  moonbit_string_t _M0L3strS1765;
  int32_t _M0L5startS1766;
  int32_t _M0L8_2afieldS2949;
  int32_t _M0L3endS1767;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2950 = _M0L4selfS431.$0;
  _M0L3strS1765 = _M0L8_2afieldS2950;
  _M0L5startS1766 = _M0L4selfS431.$1;
  _M0L8_2afieldS2949 = _M0L4selfS431.$2;
  _M0L3endS1767 = _M0L8_2afieldS2949;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1765, _M0L5startS1766, _M0L3endS1767);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS429,
  struct _M0TPB6Logger _M0L6loggerS430
) {
  moonbit_string_t _M0L8_2afieldS2952;
  moonbit_string_t _M0L3strS1762;
  int32_t _M0L5startS1763;
  int32_t _M0L8_2afieldS2951;
  int32_t _M0L3endS1764;
  moonbit_string_t _M0L6substrS428;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2952 = _M0L4selfS429.$0;
  _M0L3strS1762 = _M0L8_2afieldS2952;
  _M0L5startS1763 = _M0L4selfS429.$1;
  _M0L8_2afieldS2951 = _M0L4selfS429.$2;
  _M0L3endS1764 = _M0L8_2afieldS2951;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS428
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1762, _M0L5startS1763, _M0L3endS1764);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS428, _M0L6loggerS430);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS420,
  struct _M0TPB6Logger _M0L6loggerS418
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS419;
  int32_t _M0L3lenS421;
  int32_t _M0L1iS422;
  int32_t _M0L3segS423;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS418.$1) {
    moonbit_incref(_M0L6loggerS418.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS418.$0->$method_3(_M0L6loggerS418.$1, 34);
  moonbit_incref(_M0L4selfS420);
  if (_M0L6loggerS418.$1) {
    moonbit_incref(_M0L6loggerS418.$1);
  }
  _M0L6_2aenvS419
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS419)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS419->$0 = _M0L4selfS420;
  _M0L6_2aenvS419->$1_0 = _M0L6loggerS418.$0;
  _M0L6_2aenvS419->$1_1 = _M0L6loggerS418.$1;
  _M0L3lenS421 = Moonbit_array_length(_M0L4selfS420);
  _M0L1iS422 = 0;
  _M0L3segS423 = 0;
  _2afor_424:;
  while (1) {
    int32_t _M0L4codeS425;
    int32_t _M0L1cS427;
    int32_t _M0L6_2atmpS1746;
    int32_t _M0L6_2atmpS1747;
    int32_t _M0L6_2atmpS1748;
    int32_t _tmp_3214;
    int32_t _tmp_3215;
    if (_M0L1iS422 >= _M0L3lenS421) {
      moonbit_decref(_M0L4selfS420);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS419, _M0L3segS423, _M0L1iS422);
      break;
    }
    _M0L4codeS425 = _M0L4selfS420[_M0L1iS422];
    switch (_M0L4codeS425) {
      case 34: {
        _M0L1cS427 = _M0L4codeS425;
        goto join_426;
        break;
      }
      
      case 92: {
        _M0L1cS427 = _M0L4codeS425;
        goto join_426;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1749;
        int32_t _M0L6_2atmpS1750;
        moonbit_incref(_M0L6_2aenvS419);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS419, _M0L3segS423, _M0L1iS422);
        if (_M0L6loggerS418.$1) {
          moonbit_incref(_M0L6loggerS418.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS418.$0->$method_0(_M0L6loggerS418.$1, (moonbit_string_t)moonbit_string_literal_40.data);
        _M0L6_2atmpS1749 = _M0L1iS422 + 1;
        _M0L6_2atmpS1750 = _M0L1iS422 + 1;
        _M0L1iS422 = _M0L6_2atmpS1749;
        _M0L3segS423 = _M0L6_2atmpS1750;
        goto _2afor_424;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1751;
        int32_t _M0L6_2atmpS1752;
        moonbit_incref(_M0L6_2aenvS419);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS419, _M0L3segS423, _M0L1iS422);
        if (_M0L6loggerS418.$1) {
          moonbit_incref(_M0L6loggerS418.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS418.$0->$method_0(_M0L6loggerS418.$1, (moonbit_string_t)moonbit_string_literal_41.data);
        _M0L6_2atmpS1751 = _M0L1iS422 + 1;
        _M0L6_2atmpS1752 = _M0L1iS422 + 1;
        _M0L1iS422 = _M0L6_2atmpS1751;
        _M0L3segS423 = _M0L6_2atmpS1752;
        goto _2afor_424;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1753;
        int32_t _M0L6_2atmpS1754;
        moonbit_incref(_M0L6_2aenvS419);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS419, _M0L3segS423, _M0L1iS422);
        if (_M0L6loggerS418.$1) {
          moonbit_incref(_M0L6loggerS418.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS418.$0->$method_0(_M0L6loggerS418.$1, (moonbit_string_t)moonbit_string_literal_42.data);
        _M0L6_2atmpS1753 = _M0L1iS422 + 1;
        _M0L6_2atmpS1754 = _M0L1iS422 + 1;
        _M0L1iS422 = _M0L6_2atmpS1753;
        _M0L3segS423 = _M0L6_2atmpS1754;
        goto _2afor_424;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1755;
        int32_t _M0L6_2atmpS1756;
        moonbit_incref(_M0L6_2aenvS419);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS419, _M0L3segS423, _M0L1iS422);
        if (_M0L6loggerS418.$1) {
          moonbit_incref(_M0L6loggerS418.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS418.$0->$method_0(_M0L6loggerS418.$1, (moonbit_string_t)moonbit_string_literal_43.data);
        _M0L6_2atmpS1755 = _M0L1iS422 + 1;
        _M0L6_2atmpS1756 = _M0L1iS422 + 1;
        _M0L1iS422 = _M0L6_2atmpS1755;
        _M0L3segS423 = _M0L6_2atmpS1756;
        goto _2afor_424;
        break;
      }
      default: {
        if (_M0L4codeS425 < 32) {
          int32_t _M0L6_2atmpS1758;
          moonbit_string_t _M0L6_2atmpS1757;
          int32_t _M0L6_2atmpS1759;
          int32_t _M0L6_2atmpS1760;
          moonbit_incref(_M0L6_2aenvS419);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS419, _M0L3segS423, _M0L1iS422);
          if (_M0L6loggerS418.$1) {
            moonbit_incref(_M0L6loggerS418.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS418.$0->$method_0(_M0L6loggerS418.$1, (moonbit_string_t)moonbit_string_literal_57.data);
          _M0L6_2atmpS1758 = _M0L4codeS425 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1757 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1758);
          if (_M0L6loggerS418.$1) {
            moonbit_incref(_M0L6loggerS418.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS418.$0->$method_0(_M0L6loggerS418.$1, _M0L6_2atmpS1757);
          if (_M0L6loggerS418.$1) {
            moonbit_incref(_M0L6loggerS418.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS418.$0->$method_3(_M0L6loggerS418.$1, 125);
          _M0L6_2atmpS1759 = _M0L1iS422 + 1;
          _M0L6_2atmpS1760 = _M0L1iS422 + 1;
          _M0L1iS422 = _M0L6_2atmpS1759;
          _M0L3segS423 = _M0L6_2atmpS1760;
          goto _2afor_424;
        } else {
          int32_t _M0L6_2atmpS1761 = _M0L1iS422 + 1;
          int32_t _tmp_3213 = _M0L3segS423;
          _M0L1iS422 = _M0L6_2atmpS1761;
          _M0L3segS423 = _tmp_3213;
          goto _2afor_424;
        }
        break;
      }
    }
    goto joinlet_3212;
    join_426:;
    moonbit_incref(_M0L6_2aenvS419);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS419, _M0L3segS423, _M0L1iS422);
    if (_M0L6loggerS418.$1) {
      moonbit_incref(_M0L6loggerS418.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS418.$0->$method_3(_M0L6loggerS418.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1746 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS427);
    if (_M0L6loggerS418.$1) {
      moonbit_incref(_M0L6loggerS418.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS418.$0->$method_3(_M0L6loggerS418.$1, _M0L6_2atmpS1746);
    _M0L6_2atmpS1747 = _M0L1iS422 + 1;
    _M0L6_2atmpS1748 = _M0L1iS422 + 1;
    _M0L1iS422 = _M0L6_2atmpS1747;
    _M0L3segS423 = _M0L6_2atmpS1748;
    continue;
    joinlet_3212:;
    _tmp_3214 = _M0L1iS422;
    _tmp_3215 = _M0L3segS423;
    _M0L1iS422 = _tmp_3214;
    _M0L3segS423 = _tmp_3215;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS418.$0->$method_3(_M0L6loggerS418.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS414,
  int32_t _M0L3segS417,
  int32_t _M0L1iS416
) {
  struct _M0TPB6Logger _M0L8_2afieldS2954;
  struct _M0TPB6Logger _M0L6loggerS413;
  moonbit_string_t _M0L8_2afieldS2953;
  int32_t _M0L6_2acntS3090;
  moonbit_string_t _M0L4selfS415;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS2954
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS414->$1_0, _M0L6_2aenvS414->$1_1
  };
  _M0L6loggerS413 = _M0L8_2afieldS2954;
  _M0L8_2afieldS2953 = _M0L6_2aenvS414->$0;
  _M0L6_2acntS3090 = Moonbit_object_header(_M0L6_2aenvS414)->rc;
  if (_M0L6_2acntS3090 > 1) {
    int32_t _M0L11_2anew__cntS3091 = _M0L6_2acntS3090 - 1;
    Moonbit_object_header(_M0L6_2aenvS414)->rc = _M0L11_2anew__cntS3091;
    if (_M0L6loggerS413.$1) {
      moonbit_incref(_M0L6loggerS413.$1);
    }
    moonbit_incref(_M0L8_2afieldS2953);
  } else if (_M0L6_2acntS3090 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS414);
  }
  _M0L4selfS415 = _M0L8_2afieldS2953;
  if (_M0L1iS416 > _M0L3segS417) {
    int32_t _M0L6_2atmpS1745 = _M0L1iS416 - _M0L3segS417;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS413.$0->$method_1(_M0L6loggerS413.$1, _M0L4selfS415, _M0L3segS417, _M0L6_2atmpS1745);
  } else {
    moonbit_decref(_M0L4selfS415);
    if (_M0L6loggerS413.$1) {
      moonbit_decref(_M0L6loggerS413.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS412) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS411;
  int32_t _M0L6_2atmpS1742;
  int32_t _M0L6_2atmpS1741;
  int32_t _M0L6_2atmpS1744;
  int32_t _M0L6_2atmpS1743;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1740;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS411 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1742 = _M0IPC14byte4BytePB3Div3div(_M0L1bS412, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1741
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1742);
  moonbit_incref(_M0L7_2aselfS411);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS411, _M0L6_2atmpS1741);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1744 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS412, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1743
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1744);
  moonbit_incref(_M0L7_2aselfS411);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS411, _M0L6_2atmpS1743);
  _M0L6_2atmpS1740 = _M0L7_2aselfS411;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1740);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS410) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS410 < 10) {
    int32_t _M0L6_2atmpS1737;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1737 = _M0IPC14byte4BytePB3Add3add(_M0L1iS410, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1737);
  } else {
    int32_t _M0L6_2atmpS1739;
    int32_t _M0L6_2atmpS1738;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1739 = _M0IPC14byte4BytePB3Add3add(_M0L1iS410, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1738 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1739, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1738);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS408,
  int32_t _M0L4thatS409
) {
  int32_t _M0L6_2atmpS1735;
  int32_t _M0L6_2atmpS1736;
  int32_t _M0L6_2atmpS1734;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1735 = (int32_t)_M0L4selfS408;
  _M0L6_2atmpS1736 = (int32_t)_M0L4thatS409;
  _M0L6_2atmpS1734 = _M0L6_2atmpS1735 - _M0L6_2atmpS1736;
  return _M0L6_2atmpS1734 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS406,
  int32_t _M0L4thatS407
) {
  int32_t _M0L6_2atmpS1732;
  int32_t _M0L6_2atmpS1733;
  int32_t _M0L6_2atmpS1731;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1732 = (int32_t)_M0L4selfS406;
  _M0L6_2atmpS1733 = (int32_t)_M0L4thatS407;
  _M0L6_2atmpS1731 = _M0L6_2atmpS1732 % _M0L6_2atmpS1733;
  return _M0L6_2atmpS1731 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS404,
  int32_t _M0L4thatS405
) {
  int32_t _M0L6_2atmpS1729;
  int32_t _M0L6_2atmpS1730;
  int32_t _M0L6_2atmpS1728;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1729 = (int32_t)_M0L4selfS404;
  _M0L6_2atmpS1730 = (int32_t)_M0L4thatS405;
  _M0L6_2atmpS1728 = _M0L6_2atmpS1729 / _M0L6_2atmpS1730;
  return _M0L6_2atmpS1728 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS402,
  int32_t _M0L4thatS403
) {
  int32_t _M0L6_2atmpS1726;
  int32_t _M0L6_2atmpS1727;
  int32_t _M0L6_2atmpS1725;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1726 = (int32_t)_M0L4selfS402;
  _M0L6_2atmpS1727 = (int32_t)_M0L4thatS403;
  _M0L6_2atmpS1725 = _M0L6_2atmpS1726 + _M0L6_2atmpS1727;
  return _M0L6_2atmpS1725 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS399,
  int32_t _M0L5startS397,
  int32_t _M0L3endS398
) {
  int32_t _if__result_3216;
  int32_t _M0L3lenS400;
  int32_t _M0L6_2atmpS1723;
  int32_t _M0L6_2atmpS1724;
  moonbit_bytes_t _M0L5bytesS401;
  moonbit_bytes_t _M0L6_2atmpS1722;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS397 == 0) {
    int32_t _M0L6_2atmpS1721 = Moonbit_array_length(_M0L3strS399);
    _if__result_3216 = _M0L3endS398 == _M0L6_2atmpS1721;
  } else {
    _if__result_3216 = 0;
  }
  if (_if__result_3216) {
    return _M0L3strS399;
  }
  _M0L3lenS400 = _M0L3endS398 - _M0L5startS397;
  _M0L6_2atmpS1723 = _M0L3lenS400 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1724 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS401
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1723, _M0L6_2atmpS1724);
  moonbit_incref(_M0L5bytesS401);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS401, 0, _M0L3strS399, _M0L5startS397, _M0L3lenS400);
  _M0L6_2atmpS1722 = _M0L5bytesS401;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1722, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS394) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS394;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS395
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS395;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS396) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS396;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS386,
  int32_t _M0L5radixS385
) {
  int32_t _if__result_3217;
  uint16_t* _M0L6bufferS387;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS385 < 2) {
    _if__result_3217 = 1;
  } else {
    _if__result_3217 = _M0L5radixS385 > 36;
  }
  if (_if__result_3217) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_58.data, (moonbit_string_t)moonbit_string_literal_59.data);
  }
  if (_M0L4selfS386 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_46.data;
  }
  switch (_M0L5radixS385) {
    case 10: {
      int32_t _M0L3lenS388;
      uint16_t* _M0L6bufferS389;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS388 = _M0FPB12dec__count64(_M0L4selfS386);
      _M0L6bufferS389 = (uint16_t*)moonbit_make_string(_M0L3lenS388, 0);
      moonbit_incref(_M0L6bufferS389);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS389, _M0L4selfS386, 0, _M0L3lenS388);
      _M0L6bufferS387 = _M0L6bufferS389;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS390;
      uint16_t* _M0L6bufferS391;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS390 = _M0FPB12hex__count64(_M0L4selfS386);
      _M0L6bufferS391 = (uint16_t*)moonbit_make_string(_M0L3lenS390, 0);
      moonbit_incref(_M0L6bufferS391);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS391, _M0L4selfS386, 0, _M0L3lenS390);
      _M0L6bufferS387 = _M0L6bufferS391;
      break;
    }
    default: {
      int32_t _M0L3lenS392;
      uint16_t* _M0L6bufferS393;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS392 = _M0FPB14radix__count64(_M0L4selfS386, _M0L5radixS385);
      _M0L6bufferS393 = (uint16_t*)moonbit_make_string(_M0L3lenS392, 0);
      moonbit_incref(_M0L6bufferS393);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS393, _M0L4selfS386, 0, _M0L3lenS392, _M0L5radixS385);
      _M0L6bufferS387 = _M0L6bufferS393;
      break;
    }
  }
  return _M0L6bufferS387;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS375,
  uint64_t _M0L3numS363,
  int32_t _M0L12digit__startS366,
  int32_t _M0L10total__lenS365
) {
  uint64_t _M0Lm3numS362;
  int32_t _M0Lm6offsetS364;
  uint64_t _M0L6_2atmpS1720;
  int32_t _M0Lm9remainingS377;
  int32_t _M0L6_2atmpS1701;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS362 = _M0L3numS363;
  _M0Lm6offsetS364 = _M0L10total__lenS365 - _M0L12digit__startS366;
  while (1) {
    uint64_t _M0L6_2atmpS1664 = _M0Lm3numS362;
    if (_M0L6_2atmpS1664 >= 10000ull) {
      uint64_t _M0L6_2atmpS1687 = _M0Lm3numS362;
      uint64_t _M0L1tS367 = _M0L6_2atmpS1687 / 10000ull;
      uint64_t _M0L6_2atmpS1686 = _M0Lm3numS362;
      uint64_t _M0L6_2atmpS1685 = _M0L6_2atmpS1686 % 10000ull;
      int32_t _M0L1rS368 = (int32_t)_M0L6_2atmpS1685;
      int32_t _M0L2d1S369;
      int32_t _M0L2d2S370;
      int32_t _M0L6_2atmpS1665;
      int32_t _M0L6_2atmpS1684;
      int32_t _M0L6_2atmpS1683;
      int32_t _M0L6d1__hiS371;
      int32_t _M0L6_2atmpS1682;
      int32_t _M0L6_2atmpS1681;
      int32_t _M0L6d1__loS372;
      int32_t _M0L6_2atmpS1680;
      int32_t _M0L6_2atmpS1679;
      int32_t _M0L6d2__hiS373;
      int32_t _M0L6_2atmpS1678;
      int32_t _M0L6_2atmpS1677;
      int32_t _M0L6d2__loS374;
      int32_t _M0L6_2atmpS1667;
      int32_t _M0L6_2atmpS1666;
      int32_t _M0L6_2atmpS1670;
      int32_t _M0L6_2atmpS1669;
      int32_t _M0L6_2atmpS1668;
      int32_t _M0L6_2atmpS1673;
      int32_t _M0L6_2atmpS1672;
      int32_t _M0L6_2atmpS1671;
      int32_t _M0L6_2atmpS1676;
      int32_t _M0L6_2atmpS1675;
      int32_t _M0L6_2atmpS1674;
      _M0Lm3numS362 = _M0L1tS367;
      _M0L2d1S369 = _M0L1rS368 / 100;
      _M0L2d2S370 = _M0L1rS368 % 100;
      _M0L6_2atmpS1665 = _M0Lm6offsetS364;
      _M0Lm6offsetS364 = _M0L6_2atmpS1665 - 4;
      _M0L6_2atmpS1684 = _M0L2d1S369 / 10;
      _M0L6_2atmpS1683 = 48 + _M0L6_2atmpS1684;
      _M0L6d1__hiS371 = (uint16_t)_M0L6_2atmpS1683;
      _M0L6_2atmpS1682 = _M0L2d1S369 % 10;
      _M0L6_2atmpS1681 = 48 + _M0L6_2atmpS1682;
      _M0L6d1__loS372 = (uint16_t)_M0L6_2atmpS1681;
      _M0L6_2atmpS1680 = _M0L2d2S370 / 10;
      _M0L6_2atmpS1679 = 48 + _M0L6_2atmpS1680;
      _M0L6d2__hiS373 = (uint16_t)_M0L6_2atmpS1679;
      _M0L6_2atmpS1678 = _M0L2d2S370 % 10;
      _M0L6_2atmpS1677 = 48 + _M0L6_2atmpS1678;
      _M0L6d2__loS374 = (uint16_t)_M0L6_2atmpS1677;
      _M0L6_2atmpS1667 = _M0Lm6offsetS364;
      _M0L6_2atmpS1666 = _M0L12digit__startS366 + _M0L6_2atmpS1667;
      _M0L6bufferS375[_M0L6_2atmpS1666] = _M0L6d1__hiS371;
      _M0L6_2atmpS1670 = _M0Lm6offsetS364;
      _M0L6_2atmpS1669 = _M0L12digit__startS366 + _M0L6_2atmpS1670;
      _M0L6_2atmpS1668 = _M0L6_2atmpS1669 + 1;
      _M0L6bufferS375[_M0L6_2atmpS1668] = _M0L6d1__loS372;
      _M0L6_2atmpS1673 = _M0Lm6offsetS364;
      _M0L6_2atmpS1672 = _M0L12digit__startS366 + _M0L6_2atmpS1673;
      _M0L6_2atmpS1671 = _M0L6_2atmpS1672 + 2;
      _M0L6bufferS375[_M0L6_2atmpS1671] = _M0L6d2__hiS373;
      _M0L6_2atmpS1676 = _M0Lm6offsetS364;
      _M0L6_2atmpS1675 = _M0L12digit__startS366 + _M0L6_2atmpS1676;
      _M0L6_2atmpS1674 = _M0L6_2atmpS1675 + 3;
      _M0L6bufferS375[_M0L6_2atmpS1674] = _M0L6d2__loS374;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1720 = _M0Lm3numS362;
  _M0Lm9remainingS377 = (int32_t)_M0L6_2atmpS1720;
  while (1) {
    int32_t _M0L6_2atmpS1688 = _M0Lm9remainingS377;
    if (_M0L6_2atmpS1688 >= 100) {
      int32_t _M0L6_2atmpS1700 = _M0Lm9remainingS377;
      int32_t _M0L1tS378 = _M0L6_2atmpS1700 / 100;
      int32_t _M0L6_2atmpS1699 = _M0Lm9remainingS377;
      int32_t _M0L1dS379 = _M0L6_2atmpS1699 % 100;
      int32_t _M0L6_2atmpS1689;
      int32_t _M0L6_2atmpS1698;
      int32_t _M0L6_2atmpS1697;
      int32_t _M0L5d__hiS380;
      int32_t _M0L6_2atmpS1696;
      int32_t _M0L6_2atmpS1695;
      int32_t _M0L5d__loS381;
      int32_t _M0L6_2atmpS1691;
      int32_t _M0L6_2atmpS1690;
      int32_t _M0L6_2atmpS1694;
      int32_t _M0L6_2atmpS1693;
      int32_t _M0L6_2atmpS1692;
      _M0Lm9remainingS377 = _M0L1tS378;
      _M0L6_2atmpS1689 = _M0Lm6offsetS364;
      _M0Lm6offsetS364 = _M0L6_2atmpS1689 - 2;
      _M0L6_2atmpS1698 = _M0L1dS379 / 10;
      _M0L6_2atmpS1697 = 48 + _M0L6_2atmpS1698;
      _M0L5d__hiS380 = (uint16_t)_M0L6_2atmpS1697;
      _M0L6_2atmpS1696 = _M0L1dS379 % 10;
      _M0L6_2atmpS1695 = 48 + _M0L6_2atmpS1696;
      _M0L5d__loS381 = (uint16_t)_M0L6_2atmpS1695;
      _M0L6_2atmpS1691 = _M0Lm6offsetS364;
      _M0L6_2atmpS1690 = _M0L12digit__startS366 + _M0L6_2atmpS1691;
      _M0L6bufferS375[_M0L6_2atmpS1690] = _M0L5d__hiS380;
      _M0L6_2atmpS1694 = _M0Lm6offsetS364;
      _M0L6_2atmpS1693 = _M0L12digit__startS366 + _M0L6_2atmpS1694;
      _M0L6_2atmpS1692 = _M0L6_2atmpS1693 + 1;
      _M0L6bufferS375[_M0L6_2atmpS1692] = _M0L5d__loS381;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1701 = _M0Lm9remainingS377;
  if (_M0L6_2atmpS1701 >= 10) {
    int32_t _M0L6_2atmpS1702 = _M0Lm6offsetS364;
    int32_t _M0L6_2atmpS1713;
    int32_t _M0L6_2atmpS1712;
    int32_t _M0L6_2atmpS1711;
    int32_t _M0L5d__hiS383;
    int32_t _M0L6_2atmpS1710;
    int32_t _M0L6_2atmpS1709;
    int32_t _M0L6_2atmpS1708;
    int32_t _M0L5d__loS384;
    int32_t _M0L6_2atmpS1704;
    int32_t _M0L6_2atmpS1703;
    int32_t _M0L6_2atmpS1707;
    int32_t _M0L6_2atmpS1706;
    int32_t _M0L6_2atmpS1705;
    _M0Lm6offsetS364 = _M0L6_2atmpS1702 - 2;
    _M0L6_2atmpS1713 = _M0Lm9remainingS377;
    _M0L6_2atmpS1712 = _M0L6_2atmpS1713 / 10;
    _M0L6_2atmpS1711 = 48 + _M0L6_2atmpS1712;
    _M0L5d__hiS383 = (uint16_t)_M0L6_2atmpS1711;
    _M0L6_2atmpS1710 = _M0Lm9remainingS377;
    _M0L6_2atmpS1709 = _M0L6_2atmpS1710 % 10;
    _M0L6_2atmpS1708 = 48 + _M0L6_2atmpS1709;
    _M0L5d__loS384 = (uint16_t)_M0L6_2atmpS1708;
    _M0L6_2atmpS1704 = _M0Lm6offsetS364;
    _M0L6_2atmpS1703 = _M0L12digit__startS366 + _M0L6_2atmpS1704;
    _M0L6bufferS375[_M0L6_2atmpS1703] = _M0L5d__hiS383;
    _M0L6_2atmpS1707 = _M0Lm6offsetS364;
    _M0L6_2atmpS1706 = _M0L12digit__startS366 + _M0L6_2atmpS1707;
    _M0L6_2atmpS1705 = _M0L6_2atmpS1706 + 1;
    _M0L6bufferS375[_M0L6_2atmpS1705] = _M0L5d__loS384;
    moonbit_decref(_M0L6bufferS375);
  } else {
    int32_t _M0L6_2atmpS1714 = _M0Lm6offsetS364;
    int32_t _M0L6_2atmpS1719;
    int32_t _M0L6_2atmpS1715;
    int32_t _M0L6_2atmpS1718;
    int32_t _M0L6_2atmpS1717;
    int32_t _M0L6_2atmpS1716;
    _M0Lm6offsetS364 = _M0L6_2atmpS1714 - 1;
    _M0L6_2atmpS1719 = _M0Lm6offsetS364;
    _M0L6_2atmpS1715 = _M0L12digit__startS366 + _M0L6_2atmpS1719;
    _M0L6_2atmpS1718 = _M0Lm9remainingS377;
    _M0L6_2atmpS1717 = 48 + _M0L6_2atmpS1718;
    _M0L6_2atmpS1716 = (uint16_t)_M0L6_2atmpS1717;
    _M0L6bufferS375[_M0L6_2atmpS1715] = _M0L6_2atmpS1716;
    moonbit_decref(_M0L6bufferS375);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS357,
  uint64_t _M0L3numS351,
  int32_t _M0L12digit__startS349,
  int32_t _M0L10total__lenS348,
  int32_t _M0L5radixS353
) {
  int32_t _M0Lm6offsetS347;
  uint64_t _M0Lm1nS350;
  uint64_t _M0L4baseS352;
  int32_t _M0L6_2atmpS1646;
  int32_t _M0L6_2atmpS1645;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS347 = _M0L10total__lenS348 - _M0L12digit__startS349;
  _M0Lm1nS350 = _M0L3numS351;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS352 = _M0MPC13int3Int10to__uint64(_M0L5radixS353);
  _M0L6_2atmpS1646 = _M0L5radixS353 - 1;
  _M0L6_2atmpS1645 = _M0L5radixS353 & _M0L6_2atmpS1646;
  if (_M0L6_2atmpS1645 == 0) {
    int32_t _M0L5shiftS354;
    uint64_t _M0L4maskS355;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS354 = moonbit_ctz32(_M0L5radixS353);
    _M0L4maskS355 = _M0L4baseS352 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1647 = _M0Lm1nS350;
      if (_M0L6_2atmpS1647 > 0ull) {
        int32_t _M0L6_2atmpS1648 = _M0Lm6offsetS347;
        uint64_t _M0L6_2atmpS1654;
        uint64_t _M0L6_2atmpS1653;
        int32_t _M0L5digitS356;
        int32_t _M0L6_2atmpS1651;
        int32_t _M0L6_2atmpS1649;
        int32_t _M0L6_2atmpS1650;
        uint64_t _M0L6_2atmpS1652;
        _M0Lm6offsetS347 = _M0L6_2atmpS1648 - 1;
        _M0L6_2atmpS1654 = _M0Lm1nS350;
        _M0L6_2atmpS1653 = _M0L6_2atmpS1654 & _M0L4maskS355;
        _M0L5digitS356 = (int32_t)_M0L6_2atmpS1653;
        _M0L6_2atmpS1651 = _M0Lm6offsetS347;
        _M0L6_2atmpS1649 = _M0L12digit__startS349 + _M0L6_2atmpS1651;
        _M0L6_2atmpS1650
        = ((moonbit_string_t)moonbit_string_literal_60.data)[
          _M0L5digitS356
        ];
        _M0L6bufferS357[_M0L6_2atmpS1649] = _M0L6_2atmpS1650;
        _M0L6_2atmpS1652 = _M0Lm1nS350;
        _M0Lm1nS350 = _M0L6_2atmpS1652 >> (_M0L5shiftS354 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS357);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1655 = _M0Lm1nS350;
      if (_M0L6_2atmpS1655 > 0ull) {
        int32_t _M0L6_2atmpS1656 = _M0Lm6offsetS347;
        uint64_t _M0L6_2atmpS1663;
        uint64_t _M0L1qS359;
        uint64_t _M0L6_2atmpS1661;
        uint64_t _M0L6_2atmpS1662;
        uint64_t _M0L6_2atmpS1660;
        int32_t _M0L5digitS360;
        int32_t _M0L6_2atmpS1659;
        int32_t _M0L6_2atmpS1657;
        int32_t _M0L6_2atmpS1658;
        _M0Lm6offsetS347 = _M0L6_2atmpS1656 - 1;
        _M0L6_2atmpS1663 = _M0Lm1nS350;
        _M0L1qS359 = _M0L6_2atmpS1663 / _M0L4baseS352;
        _M0L6_2atmpS1661 = _M0Lm1nS350;
        _M0L6_2atmpS1662 = _M0L1qS359 * _M0L4baseS352;
        _M0L6_2atmpS1660 = _M0L6_2atmpS1661 - _M0L6_2atmpS1662;
        _M0L5digitS360 = (int32_t)_M0L6_2atmpS1660;
        _M0L6_2atmpS1659 = _M0Lm6offsetS347;
        _M0L6_2atmpS1657 = _M0L12digit__startS349 + _M0L6_2atmpS1659;
        _M0L6_2atmpS1658
        = ((moonbit_string_t)moonbit_string_literal_60.data)[
          _M0L5digitS360
        ];
        _M0L6bufferS357[_M0L6_2atmpS1657] = _M0L6_2atmpS1658;
        _M0Lm1nS350 = _M0L1qS359;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS357);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS344,
  uint64_t _M0L3numS340,
  int32_t _M0L12digit__startS338,
  int32_t _M0L10total__lenS337
) {
  int32_t _M0Lm6offsetS336;
  uint64_t _M0Lm1nS339;
  int32_t _M0L6_2atmpS1641;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS336 = _M0L10total__lenS337 - _M0L12digit__startS338;
  _M0Lm1nS339 = _M0L3numS340;
  while (1) {
    int32_t _M0L6_2atmpS1629 = _M0Lm6offsetS336;
    if (_M0L6_2atmpS1629 >= 2) {
      int32_t _M0L6_2atmpS1630 = _M0Lm6offsetS336;
      uint64_t _M0L6_2atmpS1640;
      uint64_t _M0L6_2atmpS1639;
      int32_t _M0L9byte__valS341;
      int32_t _M0L2hiS342;
      int32_t _M0L2loS343;
      int32_t _M0L6_2atmpS1633;
      int32_t _M0L6_2atmpS1631;
      int32_t _M0L6_2atmpS1632;
      int32_t _M0L6_2atmpS1637;
      int32_t _M0L6_2atmpS1636;
      int32_t _M0L6_2atmpS1634;
      int32_t _M0L6_2atmpS1635;
      uint64_t _M0L6_2atmpS1638;
      _M0Lm6offsetS336 = _M0L6_2atmpS1630 - 2;
      _M0L6_2atmpS1640 = _M0Lm1nS339;
      _M0L6_2atmpS1639 = _M0L6_2atmpS1640 & 255ull;
      _M0L9byte__valS341 = (int32_t)_M0L6_2atmpS1639;
      _M0L2hiS342 = _M0L9byte__valS341 / 16;
      _M0L2loS343 = _M0L9byte__valS341 % 16;
      _M0L6_2atmpS1633 = _M0Lm6offsetS336;
      _M0L6_2atmpS1631 = _M0L12digit__startS338 + _M0L6_2atmpS1633;
      _M0L6_2atmpS1632
      = ((moonbit_string_t)moonbit_string_literal_60.data)[
        _M0L2hiS342
      ];
      _M0L6bufferS344[_M0L6_2atmpS1631] = _M0L6_2atmpS1632;
      _M0L6_2atmpS1637 = _M0Lm6offsetS336;
      _M0L6_2atmpS1636 = _M0L12digit__startS338 + _M0L6_2atmpS1637;
      _M0L6_2atmpS1634 = _M0L6_2atmpS1636 + 1;
      _M0L6_2atmpS1635
      = ((moonbit_string_t)moonbit_string_literal_60.data)[
        _M0L2loS343
      ];
      _M0L6bufferS344[_M0L6_2atmpS1634] = _M0L6_2atmpS1635;
      _M0L6_2atmpS1638 = _M0Lm1nS339;
      _M0Lm1nS339 = _M0L6_2atmpS1638 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1641 = _M0Lm6offsetS336;
  if (_M0L6_2atmpS1641 == 1) {
    uint64_t _M0L6_2atmpS1644 = _M0Lm1nS339;
    uint64_t _M0L6_2atmpS1643 = _M0L6_2atmpS1644 & 15ull;
    int32_t _M0L6nibbleS346 = (int32_t)_M0L6_2atmpS1643;
    int32_t _M0L6_2atmpS1642 =
      ((moonbit_string_t)moonbit_string_literal_60.data)[_M0L6nibbleS346];
    _M0L6bufferS344[_M0L12digit__startS338] = _M0L6_2atmpS1642;
    moonbit_decref(_M0L6bufferS344);
  } else {
    moonbit_decref(_M0L6bufferS344);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS330,
  int32_t _M0L5radixS333
) {
  uint64_t _M0Lm3numS331;
  uint64_t _M0L4baseS332;
  int32_t _M0Lm5countS334;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS330 == 0ull) {
    return 1;
  }
  _M0Lm3numS331 = _M0L5valueS330;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS332 = _M0MPC13int3Int10to__uint64(_M0L5radixS333);
  _M0Lm5countS334 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1626 = _M0Lm3numS331;
    if (_M0L6_2atmpS1626 > 0ull) {
      int32_t _M0L6_2atmpS1627 = _M0Lm5countS334;
      uint64_t _M0L6_2atmpS1628;
      _M0Lm5countS334 = _M0L6_2atmpS1627 + 1;
      _M0L6_2atmpS1628 = _M0Lm3numS331;
      _M0Lm3numS331 = _M0L6_2atmpS1628 / _M0L4baseS332;
      continue;
    }
    break;
  }
  return _M0Lm5countS334;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS328) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS328 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS329;
    int32_t _M0L6_2atmpS1625;
    int32_t _M0L6_2atmpS1624;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS329 = moonbit_clz64(_M0L5valueS328);
    _M0L6_2atmpS1625 = 63 - _M0L14leading__zerosS329;
    _M0L6_2atmpS1624 = _M0L6_2atmpS1625 / 4;
    return _M0L6_2atmpS1624 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS327) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS327 >= 10000000000ull) {
    if (_M0L5valueS327 >= 100000000000000ull) {
      if (_M0L5valueS327 >= 10000000000000000ull) {
        if (_M0L5valueS327 >= 1000000000000000000ull) {
          if (_M0L5valueS327 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS327 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS327 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS327 >= 1000000000000ull) {
      if (_M0L5valueS327 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS327 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS327 >= 100000ull) {
    if (_M0L5valueS327 >= 10000000ull) {
      if (_M0L5valueS327 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS327 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS327 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS327 >= 1000ull) {
    if (_M0L5valueS327 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS327 >= 100ull) {
    return 3;
  } else if (_M0L5valueS327 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS311,
  int32_t _M0L5radixS310
) {
  int32_t _if__result_3224;
  int32_t _M0L12is__negativeS312;
  uint32_t _M0L3numS313;
  uint16_t* _M0L6bufferS314;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS310 < 2) {
    _if__result_3224 = 1;
  } else {
    _if__result_3224 = _M0L5radixS310 > 36;
  }
  if (_if__result_3224) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_58.data, (moonbit_string_t)moonbit_string_literal_61.data);
  }
  if (_M0L4selfS311 == 0) {
    return (moonbit_string_t)moonbit_string_literal_46.data;
  }
  _M0L12is__negativeS312 = _M0L4selfS311 < 0;
  if (_M0L12is__negativeS312) {
    int32_t _M0L6_2atmpS1623 = -_M0L4selfS311;
    _M0L3numS313 = *(uint32_t*)&_M0L6_2atmpS1623;
  } else {
    _M0L3numS313 = *(uint32_t*)&_M0L4selfS311;
  }
  switch (_M0L5radixS310) {
    case 10: {
      int32_t _M0L10digit__lenS315;
      int32_t _M0L6_2atmpS1620;
      int32_t _M0L10total__lenS316;
      uint16_t* _M0L6bufferS317;
      int32_t _M0L12digit__startS318;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS315 = _M0FPB12dec__count32(_M0L3numS313);
      if (_M0L12is__negativeS312) {
        _M0L6_2atmpS1620 = 1;
      } else {
        _M0L6_2atmpS1620 = 0;
      }
      _M0L10total__lenS316 = _M0L10digit__lenS315 + _M0L6_2atmpS1620;
      _M0L6bufferS317
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS316, 0);
      if (_M0L12is__negativeS312) {
        _M0L12digit__startS318 = 1;
      } else {
        _M0L12digit__startS318 = 0;
      }
      moonbit_incref(_M0L6bufferS317);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS317, _M0L3numS313, _M0L12digit__startS318, _M0L10total__lenS316);
      _M0L6bufferS314 = _M0L6bufferS317;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS319;
      int32_t _M0L6_2atmpS1621;
      int32_t _M0L10total__lenS320;
      uint16_t* _M0L6bufferS321;
      int32_t _M0L12digit__startS322;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS319 = _M0FPB12hex__count32(_M0L3numS313);
      if (_M0L12is__negativeS312) {
        _M0L6_2atmpS1621 = 1;
      } else {
        _M0L6_2atmpS1621 = 0;
      }
      _M0L10total__lenS320 = _M0L10digit__lenS319 + _M0L6_2atmpS1621;
      _M0L6bufferS321
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS320, 0);
      if (_M0L12is__negativeS312) {
        _M0L12digit__startS322 = 1;
      } else {
        _M0L12digit__startS322 = 0;
      }
      moonbit_incref(_M0L6bufferS321);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS321, _M0L3numS313, _M0L12digit__startS322, _M0L10total__lenS320);
      _M0L6bufferS314 = _M0L6bufferS321;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS323;
      int32_t _M0L6_2atmpS1622;
      int32_t _M0L10total__lenS324;
      uint16_t* _M0L6bufferS325;
      int32_t _M0L12digit__startS326;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS323
      = _M0FPB14radix__count32(_M0L3numS313, _M0L5radixS310);
      if (_M0L12is__negativeS312) {
        _M0L6_2atmpS1622 = 1;
      } else {
        _M0L6_2atmpS1622 = 0;
      }
      _M0L10total__lenS324 = _M0L10digit__lenS323 + _M0L6_2atmpS1622;
      _M0L6bufferS325
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS324, 0);
      if (_M0L12is__negativeS312) {
        _M0L12digit__startS326 = 1;
      } else {
        _M0L12digit__startS326 = 0;
      }
      moonbit_incref(_M0L6bufferS325);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS325, _M0L3numS313, _M0L12digit__startS326, _M0L10total__lenS324, _M0L5radixS310);
      _M0L6bufferS314 = _M0L6bufferS325;
      break;
    }
  }
  if (_M0L12is__negativeS312) {
    _M0L6bufferS314[0] = 45;
  }
  return _M0L6bufferS314;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS304,
  int32_t _M0L5radixS307
) {
  uint32_t _M0Lm3numS305;
  uint32_t _M0L4baseS306;
  int32_t _M0Lm5countS308;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS304 == 0u) {
    return 1;
  }
  _M0Lm3numS305 = _M0L5valueS304;
  _M0L4baseS306 = *(uint32_t*)&_M0L5radixS307;
  _M0Lm5countS308 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1617 = _M0Lm3numS305;
    if (_M0L6_2atmpS1617 > 0u) {
      int32_t _M0L6_2atmpS1618 = _M0Lm5countS308;
      uint32_t _M0L6_2atmpS1619;
      _M0Lm5countS308 = _M0L6_2atmpS1618 + 1;
      _M0L6_2atmpS1619 = _M0Lm3numS305;
      _M0Lm3numS305 = _M0L6_2atmpS1619 / _M0L4baseS306;
      continue;
    }
    break;
  }
  return _M0Lm5countS308;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS302) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS302 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS303;
    int32_t _M0L6_2atmpS1616;
    int32_t _M0L6_2atmpS1615;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS303 = moonbit_clz32(_M0L5valueS302);
    _M0L6_2atmpS1616 = 31 - _M0L14leading__zerosS303;
    _M0L6_2atmpS1615 = _M0L6_2atmpS1616 / 4;
    return _M0L6_2atmpS1615 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS301) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS301 >= 100000u) {
    if (_M0L5valueS301 >= 10000000u) {
      if (_M0L5valueS301 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS301 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS301 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS301 >= 1000u) {
    if (_M0L5valueS301 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS301 >= 100u) {
    return 3;
  } else if (_M0L5valueS301 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS291,
  uint32_t _M0L3numS279,
  int32_t _M0L12digit__startS282,
  int32_t _M0L10total__lenS281
) {
  uint32_t _M0Lm3numS278;
  int32_t _M0Lm6offsetS280;
  uint32_t _M0L6_2atmpS1614;
  int32_t _M0Lm9remainingS293;
  int32_t _M0L6_2atmpS1595;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS278 = _M0L3numS279;
  _M0Lm6offsetS280 = _M0L10total__lenS281 - _M0L12digit__startS282;
  while (1) {
    uint32_t _M0L6_2atmpS1558 = _M0Lm3numS278;
    if (_M0L6_2atmpS1558 >= 10000u) {
      uint32_t _M0L6_2atmpS1581 = _M0Lm3numS278;
      uint32_t _M0L1tS283 = _M0L6_2atmpS1581 / 10000u;
      uint32_t _M0L6_2atmpS1580 = _M0Lm3numS278;
      uint32_t _M0L6_2atmpS1579 = _M0L6_2atmpS1580 % 10000u;
      int32_t _M0L1rS284 = *(int32_t*)&_M0L6_2atmpS1579;
      int32_t _M0L2d1S285;
      int32_t _M0L2d2S286;
      int32_t _M0L6_2atmpS1559;
      int32_t _M0L6_2atmpS1578;
      int32_t _M0L6_2atmpS1577;
      int32_t _M0L6d1__hiS287;
      int32_t _M0L6_2atmpS1576;
      int32_t _M0L6_2atmpS1575;
      int32_t _M0L6d1__loS288;
      int32_t _M0L6_2atmpS1574;
      int32_t _M0L6_2atmpS1573;
      int32_t _M0L6d2__hiS289;
      int32_t _M0L6_2atmpS1572;
      int32_t _M0L6_2atmpS1571;
      int32_t _M0L6d2__loS290;
      int32_t _M0L6_2atmpS1561;
      int32_t _M0L6_2atmpS1560;
      int32_t _M0L6_2atmpS1564;
      int32_t _M0L6_2atmpS1563;
      int32_t _M0L6_2atmpS1562;
      int32_t _M0L6_2atmpS1567;
      int32_t _M0L6_2atmpS1566;
      int32_t _M0L6_2atmpS1565;
      int32_t _M0L6_2atmpS1570;
      int32_t _M0L6_2atmpS1569;
      int32_t _M0L6_2atmpS1568;
      _M0Lm3numS278 = _M0L1tS283;
      _M0L2d1S285 = _M0L1rS284 / 100;
      _M0L2d2S286 = _M0L1rS284 % 100;
      _M0L6_2atmpS1559 = _M0Lm6offsetS280;
      _M0Lm6offsetS280 = _M0L6_2atmpS1559 - 4;
      _M0L6_2atmpS1578 = _M0L2d1S285 / 10;
      _M0L6_2atmpS1577 = 48 + _M0L6_2atmpS1578;
      _M0L6d1__hiS287 = (uint16_t)_M0L6_2atmpS1577;
      _M0L6_2atmpS1576 = _M0L2d1S285 % 10;
      _M0L6_2atmpS1575 = 48 + _M0L6_2atmpS1576;
      _M0L6d1__loS288 = (uint16_t)_M0L6_2atmpS1575;
      _M0L6_2atmpS1574 = _M0L2d2S286 / 10;
      _M0L6_2atmpS1573 = 48 + _M0L6_2atmpS1574;
      _M0L6d2__hiS289 = (uint16_t)_M0L6_2atmpS1573;
      _M0L6_2atmpS1572 = _M0L2d2S286 % 10;
      _M0L6_2atmpS1571 = 48 + _M0L6_2atmpS1572;
      _M0L6d2__loS290 = (uint16_t)_M0L6_2atmpS1571;
      _M0L6_2atmpS1561 = _M0Lm6offsetS280;
      _M0L6_2atmpS1560 = _M0L12digit__startS282 + _M0L6_2atmpS1561;
      _M0L6bufferS291[_M0L6_2atmpS1560] = _M0L6d1__hiS287;
      _M0L6_2atmpS1564 = _M0Lm6offsetS280;
      _M0L6_2atmpS1563 = _M0L12digit__startS282 + _M0L6_2atmpS1564;
      _M0L6_2atmpS1562 = _M0L6_2atmpS1563 + 1;
      _M0L6bufferS291[_M0L6_2atmpS1562] = _M0L6d1__loS288;
      _M0L6_2atmpS1567 = _M0Lm6offsetS280;
      _M0L6_2atmpS1566 = _M0L12digit__startS282 + _M0L6_2atmpS1567;
      _M0L6_2atmpS1565 = _M0L6_2atmpS1566 + 2;
      _M0L6bufferS291[_M0L6_2atmpS1565] = _M0L6d2__hiS289;
      _M0L6_2atmpS1570 = _M0Lm6offsetS280;
      _M0L6_2atmpS1569 = _M0L12digit__startS282 + _M0L6_2atmpS1570;
      _M0L6_2atmpS1568 = _M0L6_2atmpS1569 + 3;
      _M0L6bufferS291[_M0L6_2atmpS1568] = _M0L6d2__loS290;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1614 = _M0Lm3numS278;
  _M0Lm9remainingS293 = *(int32_t*)&_M0L6_2atmpS1614;
  while (1) {
    int32_t _M0L6_2atmpS1582 = _M0Lm9remainingS293;
    if (_M0L6_2atmpS1582 >= 100) {
      int32_t _M0L6_2atmpS1594 = _M0Lm9remainingS293;
      int32_t _M0L1tS294 = _M0L6_2atmpS1594 / 100;
      int32_t _M0L6_2atmpS1593 = _M0Lm9remainingS293;
      int32_t _M0L1dS295 = _M0L6_2atmpS1593 % 100;
      int32_t _M0L6_2atmpS1583;
      int32_t _M0L6_2atmpS1592;
      int32_t _M0L6_2atmpS1591;
      int32_t _M0L5d__hiS296;
      int32_t _M0L6_2atmpS1590;
      int32_t _M0L6_2atmpS1589;
      int32_t _M0L5d__loS297;
      int32_t _M0L6_2atmpS1585;
      int32_t _M0L6_2atmpS1584;
      int32_t _M0L6_2atmpS1588;
      int32_t _M0L6_2atmpS1587;
      int32_t _M0L6_2atmpS1586;
      _M0Lm9remainingS293 = _M0L1tS294;
      _M0L6_2atmpS1583 = _M0Lm6offsetS280;
      _M0Lm6offsetS280 = _M0L6_2atmpS1583 - 2;
      _M0L6_2atmpS1592 = _M0L1dS295 / 10;
      _M0L6_2atmpS1591 = 48 + _M0L6_2atmpS1592;
      _M0L5d__hiS296 = (uint16_t)_M0L6_2atmpS1591;
      _M0L6_2atmpS1590 = _M0L1dS295 % 10;
      _M0L6_2atmpS1589 = 48 + _M0L6_2atmpS1590;
      _M0L5d__loS297 = (uint16_t)_M0L6_2atmpS1589;
      _M0L6_2atmpS1585 = _M0Lm6offsetS280;
      _M0L6_2atmpS1584 = _M0L12digit__startS282 + _M0L6_2atmpS1585;
      _M0L6bufferS291[_M0L6_2atmpS1584] = _M0L5d__hiS296;
      _M0L6_2atmpS1588 = _M0Lm6offsetS280;
      _M0L6_2atmpS1587 = _M0L12digit__startS282 + _M0L6_2atmpS1588;
      _M0L6_2atmpS1586 = _M0L6_2atmpS1587 + 1;
      _M0L6bufferS291[_M0L6_2atmpS1586] = _M0L5d__loS297;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1595 = _M0Lm9remainingS293;
  if (_M0L6_2atmpS1595 >= 10) {
    int32_t _M0L6_2atmpS1596 = _M0Lm6offsetS280;
    int32_t _M0L6_2atmpS1607;
    int32_t _M0L6_2atmpS1606;
    int32_t _M0L6_2atmpS1605;
    int32_t _M0L5d__hiS299;
    int32_t _M0L6_2atmpS1604;
    int32_t _M0L6_2atmpS1603;
    int32_t _M0L6_2atmpS1602;
    int32_t _M0L5d__loS300;
    int32_t _M0L6_2atmpS1598;
    int32_t _M0L6_2atmpS1597;
    int32_t _M0L6_2atmpS1601;
    int32_t _M0L6_2atmpS1600;
    int32_t _M0L6_2atmpS1599;
    _M0Lm6offsetS280 = _M0L6_2atmpS1596 - 2;
    _M0L6_2atmpS1607 = _M0Lm9remainingS293;
    _M0L6_2atmpS1606 = _M0L6_2atmpS1607 / 10;
    _M0L6_2atmpS1605 = 48 + _M0L6_2atmpS1606;
    _M0L5d__hiS299 = (uint16_t)_M0L6_2atmpS1605;
    _M0L6_2atmpS1604 = _M0Lm9remainingS293;
    _M0L6_2atmpS1603 = _M0L6_2atmpS1604 % 10;
    _M0L6_2atmpS1602 = 48 + _M0L6_2atmpS1603;
    _M0L5d__loS300 = (uint16_t)_M0L6_2atmpS1602;
    _M0L6_2atmpS1598 = _M0Lm6offsetS280;
    _M0L6_2atmpS1597 = _M0L12digit__startS282 + _M0L6_2atmpS1598;
    _M0L6bufferS291[_M0L6_2atmpS1597] = _M0L5d__hiS299;
    _M0L6_2atmpS1601 = _M0Lm6offsetS280;
    _M0L6_2atmpS1600 = _M0L12digit__startS282 + _M0L6_2atmpS1601;
    _M0L6_2atmpS1599 = _M0L6_2atmpS1600 + 1;
    _M0L6bufferS291[_M0L6_2atmpS1599] = _M0L5d__loS300;
    moonbit_decref(_M0L6bufferS291);
  } else {
    int32_t _M0L6_2atmpS1608 = _M0Lm6offsetS280;
    int32_t _M0L6_2atmpS1613;
    int32_t _M0L6_2atmpS1609;
    int32_t _M0L6_2atmpS1612;
    int32_t _M0L6_2atmpS1611;
    int32_t _M0L6_2atmpS1610;
    _M0Lm6offsetS280 = _M0L6_2atmpS1608 - 1;
    _M0L6_2atmpS1613 = _M0Lm6offsetS280;
    _M0L6_2atmpS1609 = _M0L12digit__startS282 + _M0L6_2atmpS1613;
    _M0L6_2atmpS1612 = _M0Lm9remainingS293;
    _M0L6_2atmpS1611 = 48 + _M0L6_2atmpS1612;
    _M0L6_2atmpS1610 = (uint16_t)_M0L6_2atmpS1611;
    _M0L6bufferS291[_M0L6_2atmpS1609] = _M0L6_2atmpS1610;
    moonbit_decref(_M0L6bufferS291);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS273,
  uint32_t _M0L3numS267,
  int32_t _M0L12digit__startS265,
  int32_t _M0L10total__lenS264,
  int32_t _M0L5radixS269
) {
  int32_t _M0Lm6offsetS263;
  uint32_t _M0Lm1nS266;
  uint32_t _M0L4baseS268;
  int32_t _M0L6_2atmpS1540;
  int32_t _M0L6_2atmpS1539;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS263 = _M0L10total__lenS264 - _M0L12digit__startS265;
  _M0Lm1nS266 = _M0L3numS267;
  _M0L4baseS268 = *(uint32_t*)&_M0L5radixS269;
  _M0L6_2atmpS1540 = _M0L5radixS269 - 1;
  _M0L6_2atmpS1539 = _M0L5radixS269 & _M0L6_2atmpS1540;
  if (_M0L6_2atmpS1539 == 0) {
    int32_t _M0L5shiftS270;
    uint32_t _M0L4maskS271;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS270 = moonbit_ctz32(_M0L5radixS269);
    _M0L4maskS271 = _M0L4baseS268 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1541 = _M0Lm1nS266;
      if (_M0L6_2atmpS1541 > 0u) {
        int32_t _M0L6_2atmpS1542 = _M0Lm6offsetS263;
        uint32_t _M0L6_2atmpS1548;
        uint32_t _M0L6_2atmpS1547;
        int32_t _M0L5digitS272;
        int32_t _M0L6_2atmpS1545;
        int32_t _M0L6_2atmpS1543;
        int32_t _M0L6_2atmpS1544;
        uint32_t _M0L6_2atmpS1546;
        _M0Lm6offsetS263 = _M0L6_2atmpS1542 - 1;
        _M0L6_2atmpS1548 = _M0Lm1nS266;
        _M0L6_2atmpS1547 = _M0L6_2atmpS1548 & _M0L4maskS271;
        _M0L5digitS272 = *(int32_t*)&_M0L6_2atmpS1547;
        _M0L6_2atmpS1545 = _M0Lm6offsetS263;
        _M0L6_2atmpS1543 = _M0L12digit__startS265 + _M0L6_2atmpS1545;
        _M0L6_2atmpS1544
        = ((moonbit_string_t)moonbit_string_literal_60.data)[
          _M0L5digitS272
        ];
        _M0L6bufferS273[_M0L6_2atmpS1543] = _M0L6_2atmpS1544;
        _M0L6_2atmpS1546 = _M0Lm1nS266;
        _M0Lm1nS266 = _M0L6_2atmpS1546 >> (_M0L5shiftS270 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS273);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1549 = _M0Lm1nS266;
      if (_M0L6_2atmpS1549 > 0u) {
        int32_t _M0L6_2atmpS1550 = _M0Lm6offsetS263;
        uint32_t _M0L6_2atmpS1557;
        uint32_t _M0L1qS275;
        uint32_t _M0L6_2atmpS1555;
        uint32_t _M0L6_2atmpS1556;
        uint32_t _M0L6_2atmpS1554;
        int32_t _M0L5digitS276;
        int32_t _M0L6_2atmpS1553;
        int32_t _M0L6_2atmpS1551;
        int32_t _M0L6_2atmpS1552;
        _M0Lm6offsetS263 = _M0L6_2atmpS1550 - 1;
        _M0L6_2atmpS1557 = _M0Lm1nS266;
        _M0L1qS275 = _M0L6_2atmpS1557 / _M0L4baseS268;
        _M0L6_2atmpS1555 = _M0Lm1nS266;
        _M0L6_2atmpS1556 = _M0L1qS275 * _M0L4baseS268;
        _M0L6_2atmpS1554 = _M0L6_2atmpS1555 - _M0L6_2atmpS1556;
        _M0L5digitS276 = *(int32_t*)&_M0L6_2atmpS1554;
        _M0L6_2atmpS1553 = _M0Lm6offsetS263;
        _M0L6_2atmpS1551 = _M0L12digit__startS265 + _M0L6_2atmpS1553;
        _M0L6_2atmpS1552
        = ((moonbit_string_t)moonbit_string_literal_60.data)[
          _M0L5digitS276
        ];
        _M0L6bufferS273[_M0L6_2atmpS1551] = _M0L6_2atmpS1552;
        _M0Lm1nS266 = _M0L1qS275;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS273);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS260,
  uint32_t _M0L3numS256,
  int32_t _M0L12digit__startS254,
  int32_t _M0L10total__lenS253
) {
  int32_t _M0Lm6offsetS252;
  uint32_t _M0Lm1nS255;
  int32_t _M0L6_2atmpS1535;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS252 = _M0L10total__lenS253 - _M0L12digit__startS254;
  _M0Lm1nS255 = _M0L3numS256;
  while (1) {
    int32_t _M0L6_2atmpS1523 = _M0Lm6offsetS252;
    if (_M0L6_2atmpS1523 >= 2) {
      int32_t _M0L6_2atmpS1524 = _M0Lm6offsetS252;
      uint32_t _M0L6_2atmpS1534;
      uint32_t _M0L6_2atmpS1533;
      int32_t _M0L9byte__valS257;
      int32_t _M0L2hiS258;
      int32_t _M0L2loS259;
      int32_t _M0L6_2atmpS1527;
      int32_t _M0L6_2atmpS1525;
      int32_t _M0L6_2atmpS1526;
      int32_t _M0L6_2atmpS1531;
      int32_t _M0L6_2atmpS1530;
      int32_t _M0L6_2atmpS1528;
      int32_t _M0L6_2atmpS1529;
      uint32_t _M0L6_2atmpS1532;
      _M0Lm6offsetS252 = _M0L6_2atmpS1524 - 2;
      _M0L6_2atmpS1534 = _M0Lm1nS255;
      _M0L6_2atmpS1533 = _M0L6_2atmpS1534 & 255u;
      _M0L9byte__valS257 = *(int32_t*)&_M0L6_2atmpS1533;
      _M0L2hiS258 = _M0L9byte__valS257 / 16;
      _M0L2loS259 = _M0L9byte__valS257 % 16;
      _M0L6_2atmpS1527 = _M0Lm6offsetS252;
      _M0L6_2atmpS1525 = _M0L12digit__startS254 + _M0L6_2atmpS1527;
      _M0L6_2atmpS1526
      = ((moonbit_string_t)moonbit_string_literal_60.data)[
        _M0L2hiS258
      ];
      _M0L6bufferS260[_M0L6_2atmpS1525] = _M0L6_2atmpS1526;
      _M0L6_2atmpS1531 = _M0Lm6offsetS252;
      _M0L6_2atmpS1530 = _M0L12digit__startS254 + _M0L6_2atmpS1531;
      _M0L6_2atmpS1528 = _M0L6_2atmpS1530 + 1;
      _M0L6_2atmpS1529
      = ((moonbit_string_t)moonbit_string_literal_60.data)[
        _M0L2loS259
      ];
      _M0L6bufferS260[_M0L6_2atmpS1528] = _M0L6_2atmpS1529;
      _M0L6_2atmpS1532 = _M0Lm1nS255;
      _M0Lm1nS255 = _M0L6_2atmpS1532 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1535 = _M0Lm6offsetS252;
  if (_M0L6_2atmpS1535 == 1) {
    uint32_t _M0L6_2atmpS1538 = _M0Lm1nS255;
    uint32_t _M0L6_2atmpS1537 = _M0L6_2atmpS1538 & 15u;
    int32_t _M0L6nibbleS262 = *(int32_t*)&_M0L6_2atmpS1537;
    int32_t _M0L6_2atmpS1536 =
      ((moonbit_string_t)moonbit_string_literal_60.data)[_M0L6nibbleS262];
    _M0L6bufferS260[_M0L12digit__startS254] = _M0L6_2atmpS1536;
    moonbit_decref(_M0L6bufferS260);
  } else {
    moonbit_decref(_M0L6bufferS260);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS247) {
  struct _M0TWEOs* _M0L7_2afuncS246;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS246 = _M0L4selfS247;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS246->code(_M0L7_2afuncS246);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS249
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS248;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS248 = _M0L4selfS249;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS248->code(_M0L7_2afuncS248);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS251) {
  struct _M0TWEOc* _M0L7_2afuncS250;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS250 = _M0L4selfS251;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS250->code(_M0L7_2afuncS250);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS239
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS238;
  struct _M0TPB6Logger _M0L6_2atmpS1519;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS238 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS238);
  _M0L6_2atmpS1519
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS238
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS239, _M0L6_2atmpS1519);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS238);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS241
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS240;
  struct _M0TPB6Logger _M0L6_2atmpS1520;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS240 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS240);
  _M0L6_2atmpS1520
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS240
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS241, _M0L6_2atmpS1520);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS240);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS243
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS242;
  struct _M0TPB6Logger _M0L6_2atmpS1521;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS242 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS242);
  _M0L6_2atmpS1521
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS242
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS243, _M0L6_2atmpS1521);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS242);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS245
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS244;
  struct _M0TPB6Logger _M0L6_2atmpS1522;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS244 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS244);
  _M0L6_2atmpS1522
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS244
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS245, _M0L6_2atmpS1522);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS244);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS237
) {
  int32_t _M0L8_2afieldS2955;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2955 = _M0L4selfS237.$1;
  moonbit_decref(_M0L4selfS237.$0);
  return _M0L8_2afieldS2955;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS236
) {
  int32_t _M0L3endS1517;
  int32_t _M0L8_2afieldS2956;
  int32_t _M0L5startS1518;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1517 = _M0L4selfS236.$2;
  _M0L8_2afieldS2956 = _M0L4selfS236.$1;
  moonbit_decref(_M0L4selfS236.$0);
  _M0L5startS1518 = _M0L8_2afieldS2956;
  return _M0L3endS1517 - _M0L5startS1518;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS235
) {
  moonbit_string_t _M0L8_2afieldS2957;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2957 = _M0L4selfS235.$0;
  return _M0L8_2afieldS2957;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS231,
  moonbit_string_t _M0L5valueS232,
  int32_t _M0L5startS233,
  int32_t _M0L3lenS234
) {
  int32_t _M0L6_2atmpS1516;
  int64_t _M0L6_2atmpS1515;
  struct _M0TPC16string10StringView _M0L6_2atmpS1514;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1516 = _M0L5startS233 + _M0L3lenS234;
  _M0L6_2atmpS1515 = (int64_t)_M0L6_2atmpS1516;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1514
  = _M0MPC16string6String11sub_2einner(_M0L5valueS232, _M0L5startS233, _M0L6_2atmpS1515);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS231, _M0L6_2atmpS1514);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS224,
  int32_t _M0L5startS230,
  int64_t _M0L3endS226
) {
  int32_t _M0L3lenS223;
  int32_t _M0L3endS225;
  int32_t _M0L5startS229;
  int32_t _if__result_3231;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS223 = Moonbit_array_length(_M0L4selfS224);
  if (_M0L3endS226 == 4294967296ll) {
    _M0L3endS225 = _M0L3lenS223;
  } else {
    int64_t _M0L7_2aSomeS227 = _M0L3endS226;
    int32_t _M0L6_2aendS228 = (int32_t)_M0L7_2aSomeS227;
    if (_M0L6_2aendS228 < 0) {
      _M0L3endS225 = _M0L3lenS223 + _M0L6_2aendS228;
    } else {
      _M0L3endS225 = _M0L6_2aendS228;
    }
  }
  if (_M0L5startS230 < 0) {
    _M0L5startS229 = _M0L3lenS223 + _M0L5startS230;
  } else {
    _M0L5startS229 = _M0L5startS230;
  }
  if (_M0L5startS229 >= 0) {
    if (_M0L5startS229 <= _M0L3endS225) {
      _if__result_3231 = _M0L3endS225 <= _M0L3lenS223;
    } else {
      _if__result_3231 = 0;
    }
  } else {
    _if__result_3231 = 0;
  }
  if (_if__result_3231) {
    if (_M0L5startS229 < _M0L3lenS223) {
      int32_t _M0L6_2atmpS1511 = _M0L4selfS224[_M0L5startS229];
      int32_t _M0L6_2atmpS1510;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1510
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1511);
      if (!_M0L6_2atmpS1510) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS225 < _M0L3lenS223) {
      int32_t _M0L6_2atmpS1513 = _M0L4selfS224[_M0L3endS225];
      int32_t _M0L6_2atmpS1512;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1512
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1513);
      if (!_M0L6_2atmpS1512) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS229,
                                                 _M0L3endS225,
                                                 _M0L4selfS224};
  } else {
    moonbit_decref(_M0L4selfS224);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS220) {
  struct _M0TPB6Hasher* _M0L1hS219;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS219 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS219);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS219, _M0L4selfS220);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS219);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS222
) {
  struct _M0TPB6Hasher* _M0L1hS221;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS221 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS221);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS221, _M0L4selfS222);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS221);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS217) {
  int32_t _M0L4seedS216;
  if (_M0L10seed_2eoptS217 == 4294967296ll) {
    _M0L4seedS216 = 0;
  } else {
    int64_t _M0L7_2aSomeS218 = _M0L10seed_2eoptS217;
    _M0L4seedS216 = (int32_t)_M0L7_2aSomeS218;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS216);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS215) {
  uint32_t _M0L6_2atmpS1509;
  uint32_t _M0L6_2atmpS1508;
  struct _M0TPB6Hasher* _block_3232;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1509 = *(uint32_t*)&_M0L4seedS215;
  _M0L6_2atmpS1508 = _M0L6_2atmpS1509 + 374761393u;
  _block_3232
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3232)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3232->$0 = _M0L6_2atmpS1508;
  return _block_3232;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS214) {
  uint32_t _M0L6_2atmpS1507;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1507 = _M0MPB6Hasher9avalanche(_M0L4selfS214);
  return *(int32_t*)&_M0L6_2atmpS1507;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS213) {
  uint32_t _M0L8_2afieldS2958;
  uint32_t _M0Lm3accS212;
  uint32_t _M0L6_2atmpS1496;
  uint32_t _M0L6_2atmpS1498;
  uint32_t _M0L6_2atmpS1497;
  uint32_t _M0L6_2atmpS1499;
  uint32_t _M0L6_2atmpS1500;
  uint32_t _M0L6_2atmpS1502;
  uint32_t _M0L6_2atmpS1501;
  uint32_t _M0L6_2atmpS1503;
  uint32_t _M0L6_2atmpS1504;
  uint32_t _M0L6_2atmpS1506;
  uint32_t _M0L6_2atmpS1505;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS2958 = _M0L4selfS213->$0;
  moonbit_decref(_M0L4selfS213);
  _M0Lm3accS212 = _M0L8_2afieldS2958;
  _M0L6_2atmpS1496 = _M0Lm3accS212;
  _M0L6_2atmpS1498 = _M0Lm3accS212;
  _M0L6_2atmpS1497 = _M0L6_2atmpS1498 >> 15;
  _M0Lm3accS212 = _M0L6_2atmpS1496 ^ _M0L6_2atmpS1497;
  _M0L6_2atmpS1499 = _M0Lm3accS212;
  _M0Lm3accS212 = _M0L6_2atmpS1499 * 2246822519u;
  _M0L6_2atmpS1500 = _M0Lm3accS212;
  _M0L6_2atmpS1502 = _M0Lm3accS212;
  _M0L6_2atmpS1501 = _M0L6_2atmpS1502 >> 13;
  _M0Lm3accS212 = _M0L6_2atmpS1500 ^ _M0L6_2atmpS1501;
  _M0L6_2atmpS1503 = _M0Lm3accS212;
  _M0Lm3accS212 = _M0L6_2atmpS1503 * 3266489917u;
  _M0L6_2atmpS1504 = _M0Lm3accS212;
  _M0L6_2atmpS1506 = _M0Lm3accS212;
  _M0L6_2atmpS1505 = _M0L6_2atmpS1506 >> 16;
  _M0Lm3accS212 = _M0L6_2atmpS1504 ^ _M0L6_2atmpS1505;
  return _M0Lm3accS212;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS210,
  moonbit_string_t _M0L1yS211
) {
  int32_t _M0L6_2atmpS2959;
  int32_t _M0L6_2atmpS1495;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2959 = moonbit_val_array_equal(_M0L1xS210, _M0L1yS211);
  moonbit_decref(_M0L1xS210);
  moonbit_decref(_M0L1yS211);
  _M0L6_2atmpS1495 = _M0L6_2atmpS2959;
  return !_M0L6_2atmpS1495;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS207,
  int32_t _M0L5valueS206
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS206, _M0L4selfS207);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS209,
  moonbit_string_t _M0L5valueS208
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS208, _M0L4selfS209);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS205) {
  int64_t _M0L6_2atmpS1494;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1494 = (int64_t)_M0L4selfS205;
  return *(uint64_t*)&_M0L6_2atmpS1494;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS203,
  int32_t _M0L5valueS204
) {
  uint32_t _M0L6_2atmpS1493;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1493 = *(uint32_t*)&_M0L5valueS204;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS203, _M0L6_2atmpS1493);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS196
) {
  struct _M0TPB13StringBuilder* _M0L3bufS194;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS195;
  int32_t _M0L7_2abindS197;
  int32_t _M0L1iS198;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS194 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS195 = _M0L4selfS196;
  moonbit_incref(_M0L3bufS194);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS194, 91);
  _M0L7_2abindS197 = _M0L7_2aselfS195->$1;
  _M0L1iS198 = 0;
  while (1) {
    if (_M0L1iS198 < _M0L7_2abindS197) {
      int32_t _if__result_3234;
      moonbit_string_t* _M0L8_2afieldS2961;
      moonbit_string_t* _M0L3bufS1491;
      moonbit_string_t _M0L6_2atmpS2960;
      moonbit_string_t _M0L4itemS199;
      int32_t _M0L6_2atmpS1492;
      if (_M0L1iS198 != 0) {
        moonbit_incref(_M0L3bufS194);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS194, (moonbit_string_t)moonbit_string_literal_62.data);
      }
      if (_M0L1iS198 < 0) {
        _if__result_3234 = 1;
      } else {
        int32_t _M0L3lenS1490 = _M0L7_2aselfS195->$1;
        _if__result_3234 = _M0L1iS198 >= _M0L3lenS1490;
      }
      if (_if__result_3234) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS2961 = _M0L7_2aselfS195->$0;
      _M0L3bufS1491 = _M0L8_2afieldS2961;
      _M0L6_2atmpS2960 = (moonbit_string_t)_M0L3bufS1491[_M0L1iS198];
      _M0L4itemS199 = _M0L6_2atmpS2960;
      if (_M0L4itemS199 == 0) {
        moonbit_incref(_M0L3bufS194);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS194, (moonbit_string_t)moonbit_string_literal_27.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS200 = _M0L4itemS199;
        moonbit_string_t _M0L6_2alocS201 = _M0L7_2aSomeS200;
        moonbit_string_t _M0L6_2atmpS1489;
        moonbit_incref(_M0L6_2alocS201);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1489
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS201);
        moonbit_incref(_M0L3bufS194);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS194, _M0L6_2atmpS1489);
      }
      _M0L6_2atmpS1492 = _M0L1iS198 + 1;
      _M0L1iS198 = _M0L6_2atmpS1492;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS195);
    }
    break;
  }
  moonbit_incref(_M0L3bufS194);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS194, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS194);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS193
) {
  moonbit_string_t _M0L6_2atmpS1488;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1487;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1488 = _M0L4selfS193;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1487 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1488);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1487);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS192
) {
  struct _M0TPB13StringBuilder* _M0L2sbS191;
  struct _M0TPC16string10StringView _M0L8_2afieldS2974;
  struct _M0TPC16string10StringView _M0L3pkgS1472;
  moonbit_string_t _M0L6_2atmpS1471;
  moonbit_string_t _M0L6_2atmpS2973;
  moonbit_string_t _M0L6_2atmpS1470;
  moonbit_string_t _M0L6_2atmpS2972;
  moonbit_string_t _M0L6_2atmpS1469;
  struct _M0TPC16string10StringView _M0L8_2afieldS2971;
  struct _M0TPC16string10StringView _M0L8filenameS1473;
  struct _M0TPC16string10StringView _M0L8_2afieldS2970;
  struct _M0TPC16string10StringView _M0L11start__lineS1476;
  moonbit_string_t _M0L6_2atmpS1475;
  moonbit_string_t _M0L6_2atmpS2969;
  moonbit_string_t _M0L6_2atmpS1474;
  struct _M0TPC16string10StringView _M0L8_2afieldS2968;
  struct _M0TPC16string10StringView _M0L13start__columnS1479;
  moonbit_string_t _M0L6_2atmpS1478;
  moonbit_string_t _M0L6_2atmpS2967;
  moonbit_string_t _M0L6_2atmpS1477;
  struct _M0TPC16string10StringView _M0L8_2afieldS2966;
  struct _M0TPC16string10StringView _M0L9end__lineS1482;
  moonbit_string_t _M0L6_2atmpS1481;
  moonbit_string_t _M0L6_2atmpS2965;
  moonbit_string_t _M0L6_2atmpS1480;
  struct _M0TPC16string10StringView _M0L8_2afieldS2964;
  int32_t _M0L6_2acntS3092;
  struct _M0TPC16string10StringView _M0L11end__columnS1486;
  moonbit_string_t _M0L6_2atmpS1485;
  moonbit_string_t _M0L6_2atmpS2963;
  moonbit_string_t _M0L6_2atmpS1484;
  moonbit_string_t _M0L6_2atmpS2962;
  moonbit_string_t _M0L6_2atmpS1483;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS191 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS2974
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$0_1, _M0L4selfS192->$0_2, _M0L4selfS192->$0_0
  };
  _M0L3pkgS1472 = _M0L8_2afieldS2974;
  moonbit_incref(_M0L3pkgS1472.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1471
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1472);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2973
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_63.data, _M0L6_2atmpS1471);
  moonbit_decref(_M0L6_2atmpS1471);
  _M0L6_2atmpS1470 = _M0L6_2atmpS2973;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2972
  = moonbit_add_string(_M0L6_2atmpS1470, (moonbit_string_t)moonbit_string_literal_64.data);
  moonbit_decref(_M0L6_2atmpS1470);
  _M0L6_2atmpS1469 = _M0L6_2atmpS2972;
  moonbit_incref(_M0L2sbS191);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1469);
  moonbit_incref(_M0L2sbS191);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, (moonbit_string_t)moonbit_string_literal_65.data);
  _M0L8_2afieldS2971
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$1_1, _M0L4selfS192->$1_2, _M0L4selfS192->$1_0
  };
  _M0L8filenameS1473 = _M0L8_2afieldS2971;
  moonbit_incref(_M0L8filenameS1473.$0);
  moonbit_incref(_M0L2sbS191);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS191, _M0L8filenameS1473);
  _M0L8_2afieldS2970
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$2_1, _M0L4selfS192->$2_2, _M0L4selfS192->$2_0
  };
  _M0L11start__lineS1476 = _M0L8_2afieldS2970;
  moonbit_incref(_M0L11start__lineS1476.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1475
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1476);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2969
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS1475);
  moonbit_decref(_M0L6_2atmpS1475);
  _M0L6_2atmpS1474 = _M0L6_2atmpS2969;
  moonbit_incref(_M0L2sbS191);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1474);
  _M0L8_2afieldS2968
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$3_1, _M0L4selfS192->$3_2, _M0L4selfS192->$3_0
  };
  _M0L13start__columnS1479 = _M0L8_2afieldS2968;
  moonbit_incref(_M0L13start__columnS1479.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1478
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1479);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2967
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_67.data, _M0L6_2atmpS1478);
  moonbit_decref(_M0L6_2atmpS1478);
  _M0L6_2atmpS1477 = _M0L6_2atmpS2967;
  moonbit_incref(_M0L2sbS191);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1477);
  _M0L8_2afieldS2966
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$4_1, _M0L4selfS192->$4_2, _M0L4selfS192->$4_0
  };
  _M0L9end__lineS1482 = _M0L8_2afieldS2966;
  moonbit_incref(_M0L9end__lineS1482.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1481
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1482);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2965
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_68.data, _M0L6_2atmpS1481);
  moonbit_decref(_M0L6_2atmpS1481);
  _M0L6_2atmpS1480 = _M0L6_2atmpS2965;
  moonbit_incref(_M0L2sbS191);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1480);
  _M0L8_2afieldS2964
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$5_1, _M0L4selfS192->$5_2, _M0L4selfS192->$5_0
  };
  _M0L6_2acntS3092 = Moonbit_object_header(_M0L4selfS192)->rc;
  if (_M0L6_2acntS3092 > 1) {
    int32_t _M0L11_2anew__cntS3098 = _M0L6_2acntS3092 - 1;
    Moonbit_object_header(_M0L4selfS192)->rc = _M0L11_2anew__cntS3098;
    moonbit_incref(_M0L8_2afieldS2964.$0);
  } else if (_M0L6_2acntS3092 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3097 =
      (struct _M0TPC16string10StringView){_M0L4selfS192->$4_1,
                                            _M0L4selfS192->$4_2,
                                            _M0L4selfS192->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3096;
    struct _M0TPC16string10StringView _M0L8_2afieldS3095;
    struct _M0TPC16string10StringView _M0L8_2afieldS3094;
    struct _M0TPC16string10StringView _M0L8_2afieldS3093;
    moonbit_decref(_M0L8_2afieldS3097.$0);
    _M0L8_2afieldS3096
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$3_1, _M0L4selfS192->$3_2, _M0L4selfS192->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3096.$0);
    _M0L8_2afieldS3095
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$2_1, _M0L4selfS192->$2_2, _M0L4selfS192->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3095.$0);
    _M0L8_2afieldS3094
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$1_1, _M0L4selfS192->$1_2, _M0L4selfS192->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3094.$0);
    _M0L8_2afieldS3093
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$0_1, _M0L4selfS192->$0_2, _M0L4selfS192->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3093.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS192);
  }
  _M0L11end__columnS1486 = _M0L8_2afieldS2964;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1485
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1486);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2963
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_69.data, _M0L6_2atmpS1485);
  moonbit_decref(_M0L6_2atmpS1485);
  _M0L6_2atmpS1484 = _M0L6_2atmpS2963;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2962
  = moonbit_add_string(_M0L6_2atmpS1484, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1484);
  _M0L6_2atmpS1483 = _M0L6_2atmpS2962;
  moonbit_incref(_M0L2sbS191);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1483);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS191);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS189,
  moonbit_string_t _M0L3strS190
) {
  int32_t _M0L3lenS1459;
  int32_t _M0L6_2atmpS1461;
  int32_t _M0L6_2atmpS1460;
  int32_t _M0L6_2atmpS1458;
  moonbit_bytes_t _M0L8_2afieldS2976;
  moonbit_bytes_t _M0L4dataS1462;
  int32_t _M0L3lenS1463;
  int32_t _M0L6_2atmpS1464;
  int32_t _M0L3lenS1466;
  int32_t _M0L6_2atmpS2975;
  int32_t _M0L6_2atmpS1468;
  int32_t _M0L6_2atmpS1467;
  int32_t _M0L6_2atmpS1465;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1459 = _M0L4selfS189->$1;
  _M0L6_2atmpS1461 = Moonbit_array_length(_M0L3strS190);
  _M0L6_2atmpS1460 = _M0L6_2atmpS1461 * 2;
  _M0L6_2atmpS1458 = _M0L3lenS1459 + _M0L6_2atmpS1460;
  moonbit_incref(_M0L4selfS189);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS189, _M0L6_2atmpS1458);
  _M0L8_2afieldS2976 = _M0L4selfS189->$0;
  _M0L4dataS1462 = _M0L8_2afieldS2976;
  _M0L3lenS1463 = _M0L4selfS189->$1;
  _M0L6_2atmpS1464 = Moonbit_array_length(_M0L3strS190);
  moonbit_incref(_M0L4dataS1462);
  moonbit_incref(_M0L3strS190);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1462, _M0L3lenS1463, _M0L3strS190, 0, _M0L6_2atmpS1464);
  _M0L3lenS1466 = _M0L4selfS189->$1;
  _M0L6_2atmpS2975 = Moonbit_array_length(_M0L3strS190);
  moonbit_decref(_M0L3strS190);
  _M0L6_2atmpS1468 = _M0L6_2atmpS2975;
  _M0L6_2atmpS1467 = _M0L6_2atmpS1468 * 2;
  _M0L6_2atmpS1465 = _M0L3lenS1466 + _M0L6_2atmpS1467;
  _M0L4selfS189->$1 = _M0L6_2atmpS1465;
  moonbit_decref(_M0L4selfS189);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS181,
  int32_t _M0L13bytes__offsetS176,
  moonbit_string_t _M0L3strS183,
  int32_t _M0L11str__offsetS179,
  int32_t _M0L6lengthS177
) {
  int32_t _M0L6_2atmpS1457;
  int32_t _M0L6_2atmpS1456;
  int32_t _M0L2e1S175;
  int32_t _M0L6_2atmpS1455;
  int32_t _M0L2e2S178;
  int32_t _M0L4len1S180;
  int32_t _M0L4len2S182;
  int32_t _if__result_3235;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1457 = _M0L6lengthS177 * 2;
  _M0L6_2atmpS1456 = _M0L13bytes__offsetS176 + _M0L6_2atmpS1457;
  _M0L2e1S175 = _M0L6_2atmpS1456 - 1;
  _M0L6_2atmpS1455 = _M0L11str__offsetS179 + _M0L6lengthS177;
  _M0L2e2S178 = _M0L6_2atmpS1455 - 1;
  _M0L4len1S180 = Moonbit_array_length(_M0L4selfS181);
  _M0L4len2S182 = Moonbit_array_length(_M0L3strS183);
  if (_M0L6lengthS177 >= 0) {
    if (_M0L13bytes__offsetS176 >= 0) {
      if (_M0L2e1S175 < _M0L4len1S180) {
        if (_M0L11str__offsetS179 >= 0) {
          _if__result_3235 = _M0L2e2S178 < _M0L4len2S182;
        } else {
          _if__result_3235 = 0;
        }
      } else {
        _if__result_3235 = 0;
      }
    } else {
      _if__result_3235 = 0;
    }
  } else {
    _if__result_3235 = 0;
  }
  if (_if__result_3235) {
    int32_t _M0L16end__str__offsetS184 =
      _M0L11str__offsetS179 + _M0L6lengthS177;
    int32_t _M0L1iS185 = _M0L11str__offsetS179;
    int32_t _M0L1jS186 = _M0L13bytes__offsetS176;
    while (1) {
      if (_M0L1iS185 < _M0L16end__str__offsetS184) {
        int32_t _M0L6_2atmpS1452 = _M0L3strS183[_M0L1iS185];
        int32_t _M0L6_2atmpS1451 = (int32_t)_M0L6_2atmpS1452;
        uint32_t _M0L1cS187 = *(uint32_t*)&_M0L6_2atmpS1451;
        uint32_t _M0L6_2atmpS1447 = _M0L1cS187 & 255u;
        int32_t _M0L6_2atmpS1446;
        int32_t _M0L6_2atmpS1448;
        uint32_t _M0L6_2atmpS1450;
        int32_t _M0L6_2atmpS1449;
        int32_t _M0L6_2atmpS1453;
        int32_t _M0L6_2atmpS1454;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1446 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1447);
        if (
          _M0L1jS186 < 0 || _M0L1jS186 >= Moonbit_array_length(_M0L4selfS181)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS181[_M0L1jS186] = _M0L6_2atmpS1446;
        _M0L6_2atmpS1448 = _M0L1jS186 + 1;
        _M0L6_2atmpS1450 = _M0L1cS187 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1449 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1450);
        if (
          _M0L6_2atmpS1448 < 0
          || _M0L6_2atmpS1448 >= Moonbit_array_length(_M0L4selfS181)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS181[_M0L6_2atmpS1448] = _M0L6_2atmpS1449;
        _M0L6_2atmpS1453 = _M0L1iS185 + 1;
        _M0L6_2atmpS1454 = _M0L1jS186 + 2;
        _M0L1iS185 = _M0L6_2atmpS1453;
        _M0L1jS186 = _M0L6_2atmpS1454;
        continue;
      } else {
        moonbit_decref(_M0L3strS183);
        moonbit_decref(_M0L4selfS181);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS183);
    moonbit_decref(_M0L4selfS181);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  double _M0L3objS171
) {
  struct _M0TPB6Logger _M0L6_2atmpS1444;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1444
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS172
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS171, _M0L6_2atmpS1444);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS174,
  struct _M0TPC16string10StringView _M0L3objS173
) {
  struct _M0TPB6Logger _M0L6_2atmpS1445;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1445
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS174
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS173, _M0L6_2atmpS1445);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS117
) {
  int32_t _M0L6_2atmpS1443;
  struct _M0TPC16string10StringView _M0L7_2abindS116;
  moonbit_string_t _M0L7_2adataS118;
  int32_t _M0L8_2astartS119;
  int32_t _M0L6_2atmpS1442;
  int32_t _M0L6_2aendS120;
  int32_t _M0Lm9_2acursorS121;
  int32_t _M0Lm13accept__stateS122;
  int32_t _M0Lm10match__endS123;
  int32_t _M0Lm20match__tag__saver__0S124;
  int32_t _M0Lm20match__tag__saver__1S125;
  int32_t _M0Lm20match__tag__saver__2S126;
  int32_t _M0Lm20match__tag__saver__3S127;
  int32_t _M0Lm20match__tag__saver__4S128;
  int32_t _M0Lm6tag__0S129;
  int32_t _M0Lm6tag__1S130;
  int32_t _M0Lm9tag__1__1S131;
  int32_t _M0Lm9tag__1__2S132;
  int32_t _M0Lm6tag__3S133;
  int32_t _M0Lm6tag__2S134;
  int32_t _M0Lm9tag__2__1S135;
  int32_t _M0Lm6tag__4S136;
  int32_t _M0L6_2atmpS1400;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1443 = Moonbit_array_length(_M0L4reprS117);
  _M0L7_2abindS116
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1443, _M0L4reprS117
  };
  moonbit_incref(_M0L7_2abindS116.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS118 = _M0MPC16string10StringView4data(_M0L7_2abindS116);
  moonbit_incref(_M0L7_2abindS116.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS119
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS116);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1442 = _M0MPC16string10StringView6length(_M0L7_2abindS116);
  _M0L6_2aendS120 = _M0L8_2astartS119 + _M0L6_2atmpS1442;
  _M0Lm9_2acursorS121 = _M0L8_2astartS119;
  _M0Lm13accept__stateS122 = -1;
  _M0Lm10match__endS123 = -1;
  _M0Lm20match__tag__saver__0S124 = -1;
  _M0Lm20match__tag__saver__1S125 = -1;
  _M0Lm20match__tag__saver__2S126 = -1;
  _M0Lm20match__tag__saver__3S127 = -1;
  _M0Lm20match__tag__saver__4S128 = -1;
  _M0Lm6tag__0S129 = -1;
  _M0Lm6tag__1S130 = -1;
  _M0Lm9tag__1__1S131 = -1;
  _M0Lm9tag__1__2S132 = -1;
  _M0Lm6tag__3S133 = -1;
  _M0Lm6tag__2S134 = -1;
  _M0Lm9tag__2__1S135 = -1;
  _M0Lm6tag__4S136 = -1;
  _M0L6_2atmpS1400 = _M0Lm9_2acursorS121;
  if (_M0L6_2atmpS1400 < _M0L6_2aendS120) {
    int32_t _M0L6_2atmpS1402 = _M0Lm9_2acursorS121;
    int32_t _M0L6_2atmpS1401;
    moonbit_incref(_M0L7_2adataS118);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1401
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1402);
    if (_M0L6_2atmpS1401 == 64) {
      int32_t _M0L6_2atmpS1403 = _M0Lm9_2acursorS121;
      _M0Lm9_2acursorS121 = _M0L6_2atmpS1403 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1404;
        _M0Lm6tag__0S129 = _M0Lm9_2acursorS121;
        _M0L6_2atmpS1404 = _M0Lm9_2acursorS121;
        if (_M0L6_2atmpS1404 < _M0L6_2aendS120) {
          int32_t _M0L6_2atmpS1441 = _M0Lm9_2acursorS121;
          int32_t _M0L10next__charS144;
          int32_t _M0L6_2atmpS1405;
          moonbit_incref(_M0L7_2adataS118);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS144
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1441);
          _M0L6_2atmpS1405 = _M0Lm9_2acursorS121;
          _M0Lm9_2acursorS121 = _M0L6_2atmpS1405 + 1;
          if (_M0L10next__charS144 == 58) {
            int32_t _M0L6_2atmpS1406 = _M0Lm9_2acursorS121;
            if (_M0L6_2atmpS1406 < _M0L6_2aendS120) {
              int32_t _M0L6_2atmpS1407 = _M0Lm9_2acursorS121;
              int32_t _M0L12dispatch__15S145;
              _M0Lm9_2acursorS121 = _M0L6_2atmpS1407 + 1;
              _M0L12dispatch__15S145 = 0;
              loop__label__15_148:;
              while (1) {
                int32_t _M0L6_2atmpS1408;
                switch (_M0L12dispatch__15S145) {
                  case 3: {
                    int32_t _M0L6_2atmpS1411;
                    _M0Lm9tag__1__2S132 = _M0Lm9tag__1__1S131;
                    _M0Lm9tag__1__1S131 = _M0Lm6tag__1S130;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1411 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1411 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1416 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS152;
                      int32_t _M0L6_2atmpS1412;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS152
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1416);
                      _M0L6_2atmpS1412 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1412 + 1;
                      if (_M0L10next__charS152 < 58) {
                        if (_M0L10next__charS152 < 48) {
                          goto join_151;
                        } else {
                          int32_t _M0L6_2atmpS1413;
                          _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                          _M0Lm9tag__2__1S135 = _M0Lm6tag__2S134;
                          _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                          _M0Lm6tag__3S133 = _M0Lm9_2acursorS121;
                          _M0L6_2atmpS1413 = _M0Lm9_2acursorS121;
                          if (_M0L6_2atmpS1413 < _M0L6_2aendS120) {
                            int32_t _M0L6_2atmpS1415 = _M0Lm9_2acursorS121;
                            int32_t _M0L10next__charS154;
                            int32_t _M0L6_2atmpS1414;
                            moonbit_incref(_M0L7_2adataS118);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS154
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1415);
                            _M0L6_2atmpS1414 = _M0Lm9_2acursorS121;
                            _M0Lm9_2acursorS121 = _M0L6_2atmpS1414 + 1;
                            if (_M0L10next__charS154 < 48) {
                              if (_M0L10next__charS154 == 45) {
                                goto join_146;
                              } else {
                                goto join_153;
                              }
                            } else if (_M0L10next__charS154 > 57) {
                              if (_M0L10next__charS154 < 59) {
                                _M0L12dispatch__15S145 = 3;
                                goto loop__label__15_148;
                              } else {
                                goto join_153;
                              }
                            } else {
                              _M0L12dispatch__15S145 = 6;
                              goto loop__label__15_148;
                            }
                            join_153:;
                            _M0L12dispatch__15S145 = 0;
                            goto loop__label__15_148;
                          } else {
                            goto join_137;
                          }
                        }
                      } else if (_M0L10next__charS152 > 58) {
                        goto join_151;
                      } else {
                        _M0L12dispatch__15S145 = 1;
                        goto loop__label__15_148;
                      }
                      join_151:;
                      _M0L12dispatch__15S145 = 0;
                      goto loop__label__15_148;
                    } else {
                      goto join_137;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1417;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1417 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1417 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1419 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS156;
                      int32_t _M0L6_2atmpS1418;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS156
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1419);
                      _M0L6_2atmpS1418 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1418 + 1;
                      if (_M0L10next__charS156 < 58) {
                        if (_M0L10next__charS156 < 48) {
                          goto join_155;
                        } else {
                          _M0L12dispatch__15S145 = 2;
                          goto loop__label__15_148;
                        }
                      } else if (_M0L10next__charS156 > 58) {
                        goto join_155;
                      } else {
                        _M0L12dispatch__15S145 = 3;
                        goto loop__label__15_148;
                      }
                      join_155:;
                      _M0L12dispatch__15S145 = 0;
                      goto loop__label__15_148;
                    } else {
                      goto join_137;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1420;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1420 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1420 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1422 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS157;
                      int32_t _M0L6_2atmpS1421;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS157
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1422);
                      _M0L6_2atmpS1421 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1421 + 1;
                      if (_M0L10next__charS157 == 58) {
                        _M0L12dispatch__15S145 = 1;
                        goto loop__label__15_148;
                      } else {
                        _M0L12dispatch__15S145 = 0;
                        goto loop__label__15_148;
                      }
                    } else {
                      goto join_137;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1423;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__4S136 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1423 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1423 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1431 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS159;
                      int32_t _M0L6_2atmpS1424;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS159
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1431);
                      _M0L6_2atmpS1424 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1424 + 1;
                      if (_M0L10next__charS159 < 58) {
                        if (_M0L10next__charS159 < 48) {
                          goto join_158;
                        } else {
                          _M0L12dispatch__15S145 = 4;
                          goto loop__label__15_148;
                        }
                      } else if (_M0L10next__charS159 > 58) {
                        goto join_158;
                      } else {
                        int32_t _M0L6_2atmpS1425;
                        _M0Lm9tag__1__2S132 = _M0Lm9tag__1__1S131;
                        _M0Lm9tag__1__1S131 = _M0Lm6tag__1S130;
                        _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                        _M0L6_2atmpS1425 = _M0Lm9_2acursorS121;
                        if (_M0L6_2atmpS1425 < _M0L6_2aendS120) {
                          int32_t _M0L6_2atmpS1430 = _M0Lm9_2acursorS121;
                          int32_t _M0L10next__charS161;
                          int32_t _M0L6_2atmpS1426;
                          moonbit_incref(_M0L7_2adataS118);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS161
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1430);
                          _M0L6_2atmpS1426 = _M0Lm9_2acursorS121;
                          _M0Lm9_2acursorS121 = _M0L6_2atmpS1426 + 1;
                          if (_M0L10next__charS161 < 58) {
                            if (_M0L10next__charS161 < 48) {
                              goto join_160;
                            } else {
                              int32_t _M0L6_2atmpS1427;
                              _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                              _M0Lm9tag__2__1S135 = _M0Lm6tag__2S134;
                              _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                              _M0L6_2atmpS1427 = _M0Lm9_2acursorS121;
                              if (_M0L6_2atmpS1427 < _M0L6_2aendS120) {
                                int32_t _M0L6_2atmpS1429 =
                                  _M0Lm9_2acursorS121;
                                int32_t _M0L10next__charS163;
                                int32_t _M0L6_2atmpS1428;
                                moonbit_incref(_M0L7_2adataS118);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS163
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1429);
                                _M0L6_2atmpS1428 = _M0Lm9_2acursorS121;
                                _M0Lm9_2acursorS121 = _M0L6_2atmpS1428 + 1;
                                if (_M0L10next__charS163 < 58) {
                                  if (_M0L10next__charS163 < 48) {
                                    goto join_162;
                                  } else {
                                    _M0L12dispatch__15S145 = 5;
                                    goto loop__label__15_148;
                                  }
                                } else if (_M0L10next__charS163 > 58) {
                                  goto join_162;
                                } else {
                                  _M0L12dispatch__15S145 = 3;
                                  goto loop__label__15_148;
                                }
                                join_162:;
                                _M0L12dispatch__15S145 = 0;
                                goto loop__label__15_148;
                              } else {
                                goto join_150;
                              }
                            }
                          } else if (_M0L10next__charS161 > 58) {
                            goto join_160;
                          } else {
                            _M0L12dispatch__15S145 = 1;
                            goto loop__label__15_148;
                          }
                          join_160:;
                          _M0L12dispatch__15S145 = 0;
                          goto loop__label__15_148;
                        } else {
                          goto join_137;
                        }
                      }
                      join_158:;
                      _M0L12dispatch__15S145 = 0;
                      goto loop__label__15_148;
                    } else {
                      goto join_137;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1432;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1432 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1432 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1434 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS165;
                      int32_t _M0L6_2atmpS1433;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS165
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1434);
                      _M0L6_2atmpS1433 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1433 + 1;
                      if (_M0L10next__charS165 < 58) {
                        if (_M0L10next__charS165 < 48) {
                          goto join_164;
                        } else {
                          _M0L12dispatch__15S145 = 5;
                          goto loop__label__15_148;
                        }
                      } else if (_M0L10next__charS165 > 58) {
                        goto join_164;
                      } else {
                        _M0L12dispatch__15S145 = 3;
                        goto loop__label__15_148;
                      }
                      join_164:;
                      _M0L12dispatch__15S145 = 0;
                      goto loop__label__15_148;
                    } else {
                      goto join_150;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1435;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__3S133 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1435 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1435 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1437 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS167;
                      int32_t _M0L6_2atmpS1436;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS167
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1437);
                      _M0L6_2atmpS1436 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1436 + 1;
                      if (_M0L10next__charS167 < 48) {
                        if (_M0L10next__charS167 == 45) {
                          goto join_146;
                        } else {
                          goto join_166;
                        }
                      } else if (_M0L10next__charS167 > 57) {
                        if (_M0L10next__charS167 < 59) {
                          _M0L12dispatch__15S145 = 3;
                          goto loop__label__15_148;
                        } else {
                          goto join_166;
                        }
                      } else {
                        _M0L12dispatch__15S145 = 6;
                        goto loop__label__15_148;
                      }
                      join_166:;
                      _M0L12dispatch__15S145 = 0;
                      goto loop__label__15_148;
                    } else {
                      goto join_137;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1438;
                    _M0Lm9tag__1__1S131 = _M0Lm6tag__1S130;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1438 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1438 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1440 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS169;
                      int32_t _M0L6_2atmpS1439;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS169
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1440);
                      _M0L6_2atmpS1439 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1439 + 1;
                      if (_M0L10next__charS169 < 58) {
                        if (_M0L10next__charS169 < 48) {
                          goto join_168;
                        } else {
                          _M0L12dispatch__15S145 = 2;
                          goto loop__label__15_148;
                        }
                      } else if (_M0L10next__charS169 > 58) {
                        goto join_168;
                      } else {
                        _M0L12dispatch__15S145 = 1;
                        goto loop__label__15_148;
                      }
                      join_168:;
                      _M0L12dispatch__15S145 = 0;
                      goto loop__label__15_148;
                    } else {
                      goto join_137;
                    }
                    break;
                  }
                  default: {
                    goto join_137;
                    break;
                  }
                }
                join_150:;
                _M0Lm6tag__1S130 = _M0Lm9tag__1__2S132;
                _M0Lm6tag__2S134 = _M0Lm9tag__2__1S135;
                _M0Lm20match__tag__saver__0S124 = _M0Lm6tag__0S129;
                _M0Lm20match__tag__saver__1S125 = _M0Lm6tag__1S130;
                _M0Lm20match__tag__saver__2S126 = _M0Lm6tag__2S134;
                _M0Lm20match__tag__saver__3S127 = _M0Lm6tag__3S133;
                _M0Lm20match__tag__saver__4S128 = _M0Lm6tag__4S136;
                _M0Lm13accept__stateS122 = 0;
                _M0Lm10match__endS123 = _M0Lm9_2acursorS121;
                goto join_137;
                join_146:;
                _M0Lm9tag__1__1S131 = _M0Lm9tag__1__2S132;
                _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                _M0Lm6tag__2S134 = _M0Lm9tag__2__1S135;
                _M0L6_2atmpS1408 = _M0Lm9_2acursorS121;
                if (_M0L6_2atmpS1408 < _M0L6_2aendS120) {
                  int32_t _M0L6_2atmpS1410 = _M0Lm9_2acursorS121;
                  int32_t _M0L10next__charS149;
                  int32_t _M0L6_2atmpS1409;
                  moonbit_incref(_M0L7_2adataS118);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS149
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1410);
                  _M0L6_2atmpS1409 = _M0Lm9_2acursorS121;
                  _M0Lm9_2acursorS121 = _M0L6_2atmpS1409 + 1;
                  if (_M0L10next__charS149 < 58) {
                    if (_M0L10next__charS149 < 48) {
                      goto join_147;
                    } else {
                      _M0L12dispatch__15S145 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS149 > 58) {
                    goto join_147;
                  } else {
                    _M0L12dispatch__15S145 = 1;
                    continue;
                  }
                  join_147:;
                  _M0L12dispatch__15S145 = 0;
                  continue;
                } else {
                  goto join_137;
                }
                break;
              }
            } else {
              goto join_137;
            }
          } else {
            continue;
          }
        } else {
          goto join_137;
        }
        break;
      }
    } else {
      goto join_137;
    }
  } else {
    goto join_137;
  }
  join_137:;
  switch (_M0Lm13accept__stateS122) {
    case 0: {
      int32_t _M0L6_2atmpS1399 = _M0Lm20match__tag__saver__1S125;
      int32_t _M0L6_2atmpS1398 = _M0L6_2atmpS1399 + 1;
      int64_t _M0L6_2atmpS1395 = (int64_t)_M0L6_2atmpS1398;
      int32_t _M0L6_2atmpS1397 = _M0Lm20match__tag__saver__2S126;
      int64_t _M0L6_2atmpS1396 = (int64_t)_M0L6_2atmpS1397;
      struct _M0TPC16string10StringView _M0L11start__lineS138;
      int32_t _M0L6_2atmpS1394;
      int32_t _M0L6_2atmpS1393;
      int64_t _M0L6_2atmpS1390;
      int32_t _M0L6_2atmpS1392;
      int64_t _M0L6_2atmpS1391;
      struct _M0TPC16string10StringView _M0L13start__columnS139;
      int32_t _M0L6_2atmpS1389;
      int64_t _M0L6_2atmpS1386;
      int32_t _M0L6_2atmpS1388;
      int64_t _M0L6_2atmpS1387;
      struct _M0TPC16string10StringView _M0L3pkgS140;
      int32_t _M0L6_2atmpS1385;
      int32_t _M0L6_2atmpS1384;
      int64_t _M0L6_2atmpS1381;
      int32_t _M0L6_2atmpS1383;
      int64_t _M0L6_2atmpS1382;
      struct _M0TPC16string10StringView _M0L8filenameS141;
      int32_t _M0L6_2atmpS1380;
      int32_t _M0L6_2atmpS1379;
      int64_t _M0L6_2atmpS1376;
      int32_t _M0L6_2atmpS1378;
      int64_t _M0L6_2atmpS1377;
      struct _M0TPC16string10StringView _M0L9end__lineS142;
      int32_t _M0L6_2atmpS1375;
      int32_t _M0L6_2atmpS1374;
      int64_t _M0L6_2atmpS1371;
      int32_t _M0L6_2atmpS1373;
      int64_t _M0L6_2atmpS1372;
      struct _M0TPC16string10StringView _M0L11end__columnS143;
      struct _M0TPB13SourceLocRepr* _block_3252;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS138
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1395, _M0L6_2atmpS1396);
      _M0L6_2atmpS1394 = _M0Lm20match__tag__saver__2S126;
      _M0L6_2atmpS1393 = _M0L6_2atmpS1394 + 1;
      _M0L6_2atmpS1390 = (int64_t)_M0L6_2atmpS1393;
      _M0L6_2atmpS1392 = _M0Lm20match__tag__saver__3S127;
      _M0L6_2atmpS1391 = (int64_t)_M0L6_2atmpS1392;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS139
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1390, _M0L6_2atmpS1391);
      _M0L6_2atmpS1389 = _M0L8_2astartS119 + 1;
      _M0L6_2atmpS1386 = (int64_t)_M0L6_2atmpS1389;
      _M0L6_2atmpS1388 = _M0Lm20match__tag__saver__0S124;
      _M0L6_2atmpS1387 = (int64_t)_M0L6_2atmpS1388;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS140
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1386, _M0L6_2atmpS1387);
      _M0L6_2atmpS1385 = _M0Lm20match__tag__saver__0S124;
      _M0L6_2atmpS1384 = _M0L6_2atmpS1385 + 1;
      _M0L6_2atmpS1381 = (int64_t)_M0L6_2atmpS1384;
      _M0L6_2atmpS1383 = _M0Lm20match__tag__saver__1S125;
      _M0L6_2atmpS1382 = (int64_t)_M0L6_2atmpS1383;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS141
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1381, _M0L6_2atmpS1382);
      _M0L6_2atmpS1380 = _M0Lm20match__tag__saver__3S127;
      _M0L6_2atmpS1379 = _M0L6_2atmpS1380 + 1;
      _M0L6_2atmpS1376 = (int64_t)_M0L6_2atmpS1379;
      _M0L6_2atmpS1378 = _M0Lm20match__tag__saver__4S128;
      _M0L6_2atmpS1377 = (int64_t)_M0L6_2atmpS1378;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS142
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1376, _M0L6_2atmpS1377);
      _M0L6_2atmpS1375 = _M0Lm20match__tag__saver__4S128;
      _M0L6_2atmpS1374 = _M0L6_2atmpS1375 + 1;
      _M0L6_2atmpS1371 = (int64_t)_M0L6_2atmpS1374;
      _M0L6_2atmpS1373 = _M0Lm10match__endS123;
      _M0L6_2atmpS1372 = (int64_t)_M0L6_2atmpS1373;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS143
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1371, _M0L6_2atmpS1372);
      _block_3252
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3252)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3252->$0_0 = _M0L3pkgS140.$0;
      _block_3252->$0_1 = _M0L3pkgS140.$1;
      _block_3252->$0_2 = _M0L3pkgS140.$2;
      _block_3252->$1_0 = _M0L8filenameS141.$0;
      _block_3252->$1_1 = _M0L8filenameS141.$1;
      _block_3252->$1_2 = _M0L8filenameS141.$2;
      _block_3252->$2_0 = _M0L11start__lineS138.$0;
      _block_3252->$2_1 = _M0L11start__lineS138.$1;
      _block_3252->$2_2 = _M0L11start__lineS138.$2;
      _block_3252->$3_0 = _M0L13start__columnS139.$0;
      _block_3252->$3_1 = _M0L13start__columnS139.$1;
      _block_3252->$3_2 = _M0L13start__columnS139.$2;
      _block_3252->$4_0 = _M0L9end__lineS142.$0;
      _block_3252->$4_1 = _M0L9end__lineS142.$1;
      _block_3252->$4_2 = _M0L9end__lineS142.$2;
      _block_3252->$5_0 = _M0L11end__columnS143.$0;
      _block_3252->$5_1 = _M0L11end__columnS143.$1;
      _block_3252->$5_2 = _M0L11end__columnS143.$2;
      return _block_3252;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS118);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS114,
  int32_t _M0L5indexS115
) {
  int32_t _M0L3lenS113;
  int32_t _if__result_3253;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS113 = _M0L4selfS114->$1;
  if (_M0L5indexS115 >= 0) {
    _if__result_3253 = _M0L5indexS115 < _M0L3lenS113;
  } else {
    _if__result_3253 = 0;
  }
  if (_if__result_3253) {
    moonbit_string_t* _M0L6_2atmpS1370;
    moonbit_string_t _M0L6_2atmpS2977;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1370 = _M0MPC15array5Array6bufferGsE(_M0L4selfS114);
    if (
      _M0L5indexS115 < 0
      || _M0L5indexS115 >= Moonbit_array_length(_M0L6_2atmpS1370)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2977 = (moonbit_string_t)_M0L6_2atmpS1370[_M0L5indexS115];
    moonbit_incref(_M0L6_2atmpS2977);
    moonbit_decref(_M0L6_2atmpS1370);
    return _M0L6_2atmpS2977;
  } else {
    moonbit_decref(_M0L4selfS114);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS110
) {
  moonbit_string_t* _M0L8_2afieldS2978;
  int32_t _M0L6_2acntS3099;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2978 = _M0L4selfS110->$0;
  _M0L6_2acntS3099 = Moonbit_object_header(_M0L4selfS110)->rc;
  if (_M0L6_2acntS3099 > 1) {
    int32_t _M0L11_2anew__cntS3100 = _M0L6_2acntS3099 - 1;
    Moonbit_object_header(_M0L4selfS110)->rc = _M0L11_2anew__cntS3100;
    moonbit_incref(_M0L8_2afieldS2978);
  } else if (_M0L6_2acntS3099 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS110);
  }
  return _M0L8_2afieldS2978;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS111
) {
  struct _M0TUsiE** _M0L8_2afieldS2979;
  int32_t _M0L6_2acntS3101;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2979 = _M0L4selfS111->$0;
  _M0L6_2acntS3101 = Moonbit_object_header(_M0L4selfS111)->rc;
  if (_M0L6_2acntS3101 > 1) {
    int32_t _M0L11_2anew__cntS3102 = _M0L6_2acntS3101 - 1;
    Moonbit_object_header(_M0L4selfS111)->rc = _M0L11_2anew__cntS3102;
    moonbit_incref(_M0L8_2afieldS2979);
  } else if (_M0L6_2acntS3101 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS111);
  }
  return _M0L8_2afieldS2979;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS112
) {
  void** _M0L8_2afieldS2980;
  int32_t _M0L6_2acntS3103;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2980 = _M0L4selfS112->$0;
  _M0L6_2acntS3103 = Moonbit_object_header(_M0L4selfS112)->rc;
  if (_M0L6_2acntS3103 > 1) {
    int32_t _M0L11_2anew__cntS3104 = _M0L6_2acntS3103 - 1;
    Moonbit_object_header(_M0L4selfS112)->rc = _M0L11_2anew__cntS3104;
    moonbit_incref(_M0L8_2afieldS2980);
  } else if (_M0L6_2acntS3103 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS112);
  }
  return _M0L8_2afieldS2980;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS109) {
  struct _M0TPB13StringBuilder* _M0L3bufS108;
  struct _M0TPB6Logger _M0L6_2atmpS1369;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS108 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS108);
  _M0L6_2atmpS1369
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS108
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS109, _M0L6_2atmpS1369);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS108);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS107) {
  int32_t _M0L6_2atmpS1368;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1368 = (int32_t)_M0L4selfS107;
  return _M0L6_2atmpS1368;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS105,
  int32_t _M0L8trailingS106
) {
  int32_t _M0L6_2atmpS1367;
  int32_t _M0L6_2atmpS1366;
  int32_t _M0L6_2atmpS1365;
  int32_t _M0L6_2atmpS1364;
  int32_t _M0L6_2atmpS1363;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1367 = _M0L7leadingS105 - 55296;
  _M0L6_2atmpS1366 = _M0L6_2atmpS1367 * 1024;
  _M0L6_2atmpS1365 = _M0L6_2atmpS1366 + _M0L8trailingS106;
  _M0L6_2atmpS1364 = _M0L6_2atmpS1365 - 56320;
  _M0L6_2atmpS1363 = _M0L6_2atmpS1364 + 65536;
  return _M0L6_2atmpS1363;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS104) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS104 >= 56320) {
    return _M0L4selfS104 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS103) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS103 >= 55296) {
    return _M0L4selfS103 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS100,
  int32_t _M0L2chS102
) {
  int32_t _M0L3lenS1358;
  int32_t _M0L6_2atmpS1357;
  moonbit_bytes_t _M0L8_2afieldS2981;
  moonbit_bytes_t _M0L4dataS1361;
  int32_t _M0L3lenS1362;
  int32_t _M0L3incS101;
  int32_t _M0L3lenS1360;
  int32_t _M0L6_2atmpS1359;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1358 = _M0L4selfS100->$1;
  _M0L6_2atmpS1357 = _M0L3lenS1358 + 4;
  moonbit_incref(_M0L4selfS100);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS100, _M0L6_2atmpS1357);
  _M0L8_2afieldS2981 = _M0L4selfS100->$0;
  _M0L4dataS1361 = _M0L8_2afieldS2981;
  _M0L3lenS1362 = _M0L4selfS100->$1;
  moonbit_incref(_M0L4dataS1361);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS101
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1361, _M0L3lenS1362, _M0L2chS102);
  _M0L3lenS1360 = _M0L4selfS100->$1;
  _M0L6_2atmpS1359 = _M0L3lenS1360 + _M0L3incS101;
  _M0L4selfS100->$1 = _M0L6_2atmpS1359;
  moonbit_decref(_M0L4selfS100);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS95,
  int32_t _M0L8requiredS96
) {
  moonbit_bytes_t _M0L8_2afieldS2985;
  moonbit_bytes_t _M0L4dataS1356;
  int32_t _M0L6_2atmpS2984;
  int32_t _M0L12current__lenS94;
  int32_t _M0Lm13enough__spaceS97;
  int32_t _M0L6_2atmpS1354;
  int32_t _M0L6_2atmpS1355;
  moonbit_bytes_t _M0L9new__dataS99;
  moonbit_bytes_t _M0L8_2afieldS2983;
  moonbit_bytes_t _M0L4dataS1352;
  int32_t _M0L3lenS1353;
  moonbit_bytes_t _M0L6_2aoldS2982;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2985 = _M0L4selfS95->$0;
  _M0L4dataS1356 = _M0L8_2afieldS2985;
  _M0L6_2atmpS2984 = Moonbit_array_length(_M0L4dataS1356);
  _M0L12current__lenS94 = _M0L6_2atmpS2984;
  if (_M0L8requiredS96 <= _M0L12current__lenS94) {
    moonbit_decref(_M0L4selfS95);
    return 0;
  }
  _M0Lm13enough__spaceS97 = _M0L12current__lenS94;
  while (1) {
    int32_t _M0L6_2atmpS1350 = _M0Lm13enough__spaceS97;
    if (_M0L6_2atmpS1350 < _M0L8requiredS96) {
      int32_t _M0L6_2atmpS1351 = _M0Lm13enough__spaceS97;
      _M0Lm13enough__spaceS97 = _M0L6_2atmpS1351 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1354 = _M0Lm13enough__spaceS97;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1355 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS99
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1354, _M0L6_2atmpS1355);
  _M0L8_2afieldS2983 = _M0L4selfS95->$0;
  _M0L4dataS1352 = _M0L8_2afieldS2983;
  _M0L3lenS1353 = _M0L4selfS95->$1;
  moonbit_incref(_M0L4dataS1352);
  moonbit_incref(_M0L9new__dataS99);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS99, 0, _M0L4dataS1352, 0, _M0L3lenS1353);
  _M0L6_2aoldS2982 = _M0L4selfS95->$0;
  moonbit_decref(_M0L6_2aoldS2982);
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
    uint32_t _M0L6_2atmpS1333 = _M0L4codeS87 & 255u;
    int32_t _M0L6_2atmpS1332;
    int32_t _M0L6_2atmpS1334;
    uint32_t _M0L6_2atmpS1336;
    int32_t _M0L6_2atmpS1335;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1332 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1333);
    if (
      _M0L6offsetS90 < 0
      || _M0L6offsetS90 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6offsetS90] = _M0L6_2atmpS1332;
    _M0L6_2atmpS1334 = _M0L6offsetS90 + 1;
    _M0L6_2atmpS1336 = _M0L4codeS87 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1335 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1336);
    if (
      _M0L6_2atmpS1334 < 0
      || _M0L6_2atmpS1334 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1334] = _M0L6_2atmpS1335;
    moonbit_decref(_M0L4selfS89);
    return 2;
  } else if (_M0L4codeS87 < 1114112u) {
    uint32_t _M0L2hiS91 = _M0L4codeS87 - 65536u;
    uint32_t _M0L6_2atmpS1349 = _M0L2hiS91 >> 10;
    uint32_t _M0L2loS92 = _M0L6_2atmpS1349 | 55296u;
    uint32_t _M0L6_2atmpS1348 = _M0L2hiS91 & 1023u;
    uint32_t _M0L2hiS93 = _M0L6_2atmpS1348 | 56320u;
    uint32_t _M0L6_2atmpS1338 = _M0L2loS92 & 255u;
    int32_t _M0L6_2atmpS1337;
    int32_t _M0L6_2atmpS1339;
    uint32_t _M0L6_2atmpS1341;
    int32_t _M0L6_2atmpS1340;
    int32_t _M0L6_2atmpS1342;
    uint32_t _M0L6_2atmpS1344;
    int32_t _M0L6_2atmpS1343;
    int32_t _M0L6_2atmpS1345;
    uint32_t _M0L6_2atmpS1347;
    int32_t _M0L6_2atmpS1346;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1337 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1338);
    if (
      _M0L6offsetS90 < 0
      || _M0L6offsetS90 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6offsetS90] = _M0L6_2atmpS1337;
    _M0L6_2atmpS1339 = _M0L6offsetS90 + 1;
    _M0L6_2atmpS1341 = _M0L2loS92 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1340 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1341);
    if (
      _M0L6_2atmpS1339 < 0
      || _M0L6_2atmpS1339 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1339] = _M0L6_2atmpS1340;
    _M0L6_2atmpS1342 = _M0L6offsetS90 + 2;
    _M0L6_2atmpS1344 = _M0L2hiS93 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1343 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1344);
    if (
      _M0L6_2atmpS1342 < 0
      || _M0L6_2atmpS1342 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1342] = _M0L6_2atmpS1343;
    _M0L6_2atmpS1345 = _M0L6offsetS90 + 3;
    _M0L6_2atmpS1347 = _M0L2hiS93 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1346 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1347);
    if (
      _M0L6_2atmpS1345 < 0
      || _M0L6_2atmpS1345 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1345] = _M0L6_2atmpS1346;
    moonbit_decref(_M0L4selfS89);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS89);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_70.data, (moonbit_string_t)moonbit_string_literal_71.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS86) {
  int32_t _M0L6_2atmpS1331;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1331 = *(int32_t*)&_M0L4selfS86;
  return _M0L6_2atmpS1331 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS85) {
  int32_t _M0L6_2atmpS1330;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1330 = _M0L4selfS85;
  return *(uint32_t*)&_M0L6_2atmpS1330;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS84
) {
  moonbit_bytes_t _M0L8_2afieldS2987;
  moonbit_bytes_t _M0L4dataS1329;
  moonbit_bytes_t _M0L6_2atmpS1326;
  int32_t _M0L8_2afieldS2986;
  int32_t _M0L3lenS1328;
  int64_t _M0L6_2atmpS1327;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2987 = _M0L4selfS84->$0;
  _M0L4dataS1329 = _M0L8_2afieldS2987;
  moonbit_incref(_M0L4dataS1329);
  _M0L6_2atmpS1326 = _M0L4dataS1329;
  _M0L8_2afieldS2986 = _M0L4selfS84->$1;
  moonbit_decref(_M0L4selfS84);
  _M0L3lenS1328 = _M0L8_2afieldS2986;
  _M0L6_2atmpS1327 = (int64_t)_M0L3lenS1328;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1326, 0, _M0L6_2atmpS1327);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS79,
  int32_t _M0L6offsetS83,
  int64_t _M0L6lengthS81
) {
  int32_t _M0L3lenS78;
  int32_t _M0L6lengthS80;
  int32_t _if__result_3255;
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
      int32_t _M0L6_2atmpS1325 = _M0L6offsetS83 + _M0L6lengthS80;
      _if__result_3255 = _M0L6_2atmpS1325 <= _M0L3lenS78;
    } else {
      _if__result_3255 = 0;
    }
  } else {
    _if__result_3255 = 0;
  }
  if (_if__result_3255) {
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
  struct _M0TPB13StringBuilder* _block_3256;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS76 < 1) {
    _M0L7initialS75 = 1;
  } else {
    _M0L7initialS75 = _M0L10size__hintS76;
  }
  _M0L4dataS77 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS75, 0);
  _block_3256
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3256)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3256->$0 = _M0L4dataS77;
  _block_3256->$1 = 0;
  return _block_3256;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS74) {
  int32_t _M0L6_2atmpS1324;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1324 = (int32_t)_M0L4selfS74;
  return _M0L6_2atmpS1324;
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
  int32_t _if__result_3257;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_3257 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_3257 = 0;
  }
  if (_if__result_3257) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS1288 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS1290 = _M0L11src__offsetS26 + _M0L1iS27;
        int32_t _M0L6_2atmpS1289;
        int32_t _M0L6_2atmpS1291;
        if (
          _M0L6_2atmpS1290 < 0
          || _M0L6_2atmpS1290 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1289 = (int32_t)_M0L3srcS24[_M0L6_2atmpS1290];
        if (
          _M0L6_2atmpS1288 < 0
          || _M0L6_2atmpS1288 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS23[_M0L6_2atmpS1288] = _M0L6_2atmpS1289;
        _M0L6_2atmpS1291 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS1291;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1296 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS1296;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS1292 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS1294 = _M0L11src__offsetS26 + _M0L1iS30;
        int32_t _M0L6_2atmpS1293;
        int32_t _M0L6_2atmpS1295;
        if (
          _M0L6_2atmpS1294 < 0
          || _M0L6_2atmpS1294 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1293 = (int32_t)_M0L3srcS24[_M0L6_2atmpS1294];
        if (
          _M0L6_2atmpS1292 < 0
          || _M0L6_2atmpS1292 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS23[_M0L6_2atmpS1292] = _M0L6_2atmpS1293;
        _M0L6_2atmpS1295 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS1295;
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
  int32_t _if__result_3260;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_3260 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_3260 = 0;
  }
  if (_if__result_3260) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS1297 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS1299 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS2989;
        moonbit_string_t _M0L6_2atmpS1298;
        moonbit_string_t _M0L6_2aoldS2988;
        int32_t _M0L6_2atmpS1300;
        if (
          _M0L6_2atmpS1299 < 0
          || _M0L6_2atmpS1299 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2989 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1299];
        _M0L6_2atmpS1298 = _M0L6_2atmpS2989;
        if (
          _M0L6_2atmpS1297 < 0
          || _M0L6_2atmpS1297 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2988 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1297];
        moonbit_incref(_M0L6_2atmpS1298);
        moonbit_decref(_M0L6_2aoldS2988);
        _M0L3dstS32[_M0L6_2atmpS1297] = _M0L6_2atmpS1298;
        _M0L6_2atmpS1300 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS1300;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1305 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS1305;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS1301 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS1303 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS2991;
        moonbit_string_t _M0L6_2atmpS1302;
        moonbit_string_t _M0L6_2aoldS2990;
        int32_t _M0L6_2atmpS1304;
        if (
          _M0L6_2atmpS1303 < 0
          || _M0L6_2atmpS1303 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2991 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1303];
        _M0L6_2atmpS1302 = _M0L6_2atmpS2991;
        if (
          _M0L6_2atmpS1301 < 0
          || _M0L6_2atmpS1301 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2990 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1301];
        moonbit_incref(_M0L6_2atmpS1302);
        moonbit_decref(_M0L6_2aoldS2990);
        _M0L3dstS32[_M0L6_2atmpS1301] = _M0L6_2atmpS1302;
        _M0L6_2atmpS1304 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS1304;
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
  int32_t _if__result_3263;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_3263 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_3263 = 0;
  }
  if (_if__result_3263) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS1306 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS1308 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsiE* _M0L6_2atmpS2993;
        struct _M0TUsiE* _M0L6_2atmpS1307;
        struct _M0TUsiE* _M0L6_2aoldS2992;
        int32_t _M0L6_2atmpS1309;
        if (
          _M0L6_2atmpS1308 < 0
          || _M0L6_2atmpS1308 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2993 = (struct _M0TUsiE*)_M0L3srcS42[_M0L6_2atmpS1308];
        _M0L6_2atmpS1307 = _M0L6_2atmpS2993;
        if (
          _M0L6_2atmpS1306 < 0
          || _M0L6_2atmpS1306 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2992 = (struct _M0TUsiE*)_M0L3dstS41[_M0L6_2atmpS1306];
        if (_M0L6_2atmpS1307) {
          moonbit_incref(_M0L6_2atmpS1307);
        }
        if (_M0L6_2aoldS2992) {
          moonbit_decref(_M0L6_2aoldS2992);
        }
        _M0L3dstS41[_M0L6_2atmpS1306] = _M0L6_2atmpS1307;
        _M0L6_2atmpS1309 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS1309;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1314 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS1314;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS1310 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS1312 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsiE* _M0L6_2atmpS2995;
        struct _M0TUsiE* _M0L6_2atmpS1311;
        struct _M0TUsiE* _M0L6_2aoldS2994;
        int32_t _M0L6_2atmpS1313;
        if (
          _M0L6_2atmpS1312 < 0
          || _M0L6_2atmpS1312 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2995 = (struct _M0TUsiE*)_M0L3srcS42[_M0L6_2atmpS1312];
        _M0L6_2atmpS1311 = _M0L6_2atmpS2995;
        if (
          _M0L6_2atmpS1310 < 0
          || _M0L6_2atmpS1310 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2994 = (struct _M0TUsiE*)_M0L3dstS41[_M0L6_2atmpS1310];
        if (_M0L6_2atmpS1311) {
          moonbit_incref(_M0L6_2atmpS1311);
        }
        if (_M0L6_2aoldS2994) {
          moonbit_decref(_M0L6_2aoldS2994);
        }
        _M0L3dstS41[_M0L6_2atmpS1310] = _M0L6_2atmpS1311;
        _M0L6_2atmpS1313 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS1313;
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
  int32_t _if__result_3266;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS50 == _M0L3srcS51) {
    _if__result_3266 = _M0L11dst__offsetS52 < _M0L11src__offsetS53;
  } else {
    _if__result_3266 = 0;
  }
  if (_if__result_3266) {
    int32_t _M0L1iS54 = 0;
    while (1) {
      if (_M0L1iS54 < _M0L3lenS55) {
        int32_t _M0L6_2atmpS1315 = _M0L11dst__offsetS52 + _M0L1iS54;
        int32_t _M0L6_2atmpS1317 = _M0L11src__offsetS53 + _M0L1iS54;
        void* _M0L6_2atmpS2997;
        void* _M0L6_2atmpS1316;
        void* _M0L6_2aoldS2996;
        int32_t _M0L6_2atmpS1318;
        if (
          _M0L6_2atmpS1317 < 0
          || _M0L6_2atmpS1317 >= Moonbit_array_length(_M0L3srcS51)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2997 = (void*)_M0L3srcS51[_M0L6_2atmpS1317];
        _M0L6_2atmpS1316 = _M0L6_2atmpS2997;
        if (
          _M0L6_2atmpS1315 < 0
          || _M0L6_2atmpS1315 >= Moonbit_array_length(_M0L3dstS50)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2996 = (void*)_M0L3dstS50[_M0L6_2atmpS1315];
        moonbit_incref(_M0L6_2atmpS1316);
        moonbit_decref(_M0L6_2aoldS2996);
        _M0L3dstS50[_M0L6_2atmpS1315] = _M0L6_2atmpS1316;
        _M0L6_2atmpS1318 = _M0L1iS54 + 1;
        _M0L1iS54 = _M0L6_2atmpS1318;
        continue;
      } else {
        moonbit_decref(_M0L3srcS51);
        moonbit_decref(_M0L3dstS50);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1323 = _M0L3lenS55 - 1;
    int32_t _M0L1iS57 = _M0L6_2atmpS1323;
    while (1) {
      if (_M0L1iS57 >= 0) {
        int32_t _M0L6_2atmpS1319 = _M0L11dst__offsetS52 + _M0L1iS57;
        int32_t _M0L6_2atmpS1321 = _M0L11src__offsetS53 + _M0L1iS57;
        void* _M0L6_2atmpS2999;
        void* _M0L6_2atmpS1320;
        void* _M0L6_2aoldS2998;
        int32_t _M0L6_2atmpS1322;
        if (
          _M0L6_2atmpS1321 < 0
          || _M0L6_2atmpS1321 >= Moonbit_array_length(_M0L3srcS51)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2999 = (void*)_M0L3srcS51[_M0L6_2atmpS1321];
        _M0L6_2atmpS1320 = _M0L6_2atmpS2999;
        if (
          _M0L6_2atmpS1319 < 0
          || _M0L6_2atmpS1319 >= Moonbit_array_length(_M0L3dstS50)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2998 = (void*)_M0L3dstS50[_M0L6_2atmpS1319];
        moonbit_incref(_M0L6_2atmpS1320);
        moonbit_decref(_M0L6_2aoldS2998);
        _M0L3dstS50[_M0L6_2atmpS1319] = _M0L6_2atmpS1320;
        _M0L6_2atmpS1322 = _M0L1iS57 - 1;
        _M0L1iS57 = _M0L6_2atmpS1322;
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
  moonbit_string_t _M0L6_2atmpS1277;
  moonbit_string_t _M0L6_2atmpS3002;
  moonbit_string_t _M0L6_2atmpS1275;
  moonbit_string_t _M0L6_2atmpS1276;
  moonbit_string_t _M0L6_2atmpS3001;
  moonbit_string_t _M0L6_2atmpS1274;
  moonbit_string_t _M0L6_2atmpS3000;
  moonbit_string_t _M0L6_2atmpS1273;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1277 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3002
  = moonbit_add_string(_M0L6_2atmpS1277, (moonbit_string_t)moonbit_string_literal_72.data);
  moonbit_decref(_M0L6_2atmpS1277);
  _M0L6_2atmpS1275 = _M0L6_2atmpS3002;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1276
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3001 = moonbit_add_string(_M0L6_2atmpS1275, _M0L6_2atmpS1276);
  moonbit_decref(_M0L6_2atmpS1275);
  moonbit_decref(_M0L6_2atmpS1276);
  _M0L6_2atmpS1274 = _M0L6_2atmpS3001;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3000
  = moonbit_add_string(_M0L6_2atmpS1274, (moonbit_string_t)moonbit_string_literal_9.data);
  moonbit_decref(_M0L6_2atmpS1274);
  _M0L6_2atmpS1273 = _M0L6_2atmpS3000;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1273);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS19,
  moonbit_string_t _M0L3locS20
) {
  moonbit_string_t _M0L6_2atmpS1282;
  moonbit_string_t _M0L6_2atmpS3005;
  moonbit_string_t _M0L6_2atmpS1280;
  moonbit_string_t _M0L6_2atmpS1281;
  moonbit_string_t _M0L6_2atmpS3004;
  moonbit_string_t _M0L6_2atmpS1279;
  moonbit_string_t _M0L6_2atmpS3003;
  moonbit_string_t _M0L6_2atmpS1278;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1282 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3005
  = moonbit_add_string(_M0L6_2atmpS1282, (moonbit_string_t)moonbit_string_literal_72.data);
  moonbit_decref(_M0L6_2atmpS1282);
  _M0L6_2atmpS1280 = _M0L6_2atmpS3005;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1281
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3004 = moonbit_add_string(_M0L6_2atmpS1280, _M0L6_2atmpS1281);
  moonbit_decref(_M0L6_2atmpS1280);
  moonbit_decref(_M0L6_2atmpS1281);
  _M0L6_2atmpS1279 = _M0L6_2atmpS3004;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3003
  = moonbit_add_string(_M0L6_2atmpS1279, (moonbit_string_t)moonbit_string_literal_9.data);
  moonbit_decref(_M0L6_2atmpS1279);
  _M0L6_2atmpS1278 = _M0L6_2atmpS3003;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1278);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1287;
  moonbit_string_t _M0L6_2atmpS3008;
  moonbit_string_t _M0L6_2atmpS1285;
  moonbit_string_t _M0L6_2atmpS1286;
  moonbit_string_t _M0L6_2atmpS3007;
  moonbit_string_t _M0L6_2atmpS1284;
  moonbit_string_t _M0L6_2atmpS3006;
  moonbit_string_t _M0L6_2atmpS1283;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1287 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3008
  = moonbit_add_string(_M0L6_2atmpS1287, (moonbit_string_t)moonbit_string_literal_72.data);
  moonbit_decref(_M0L6_2atmpS1287);
  _M0L6_2atmpS1285 = _M0L6_2atmpS3008;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1286
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3007 = moonbit_add_string(_M0L6_2atmpS1285, _M0L6_2atmpS1286);
  moonbit_decref(_M0L6_2atmpS1285);
  moonbit_decref(_M0L6_2atmpS1286);
  _M0L6_2atmpS1284 = _M0L6_2atmpS3007;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3006
  = moonbit_add_string(_M0L6_2atmpS1284, (moonbit_string_t)moonbit_string_literal_9.data);
  moonbit_decref(_M0L6_2atmpS1284);
  _M0L6_2atmpS1283 = _M0L6_2atmpS3006;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1283);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5valueS16
) {
  uint32_t _M0L3accS1272;
  uint32_t _M0L6_2atmpS1271;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1272 = _M0L4selfS15->$0;
  _M0L6_2atmpS1271 = _M0L3accS1272 + 4u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1271;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS15, _M0L5valueS16);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS13,
  uint32_t _M0L5inputS14
) {
  uint32_t _M0L3accS1269;
  uint32_t _M0L6_2atmpS1270;
  uint32_t _M0L6_2atmpS1268;
  uint32_t _M0L6_2atmpS1267;
  uint32_t _M0L6_2atmpS1266;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1269 = _M0L4selfS13->$0;
  _M0L6_2atmpS1270 = _M0L5inputS14 * 3266489917u;
  _M0L6_2atmpS1268 = _M0L3accS1269 + _M0L6_2atmpS1270;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1267 = _M0FPB4rotl(_M0L6_2atmpS1268, 17);
  _M0L6_2atmpS1266 = _M0L6_2atmpS1267 * 668265263u;
  _M0L4selfS13->$0 = _M0L6_2atmpS1266;
  moonbit_decref(_M0L4selfS13);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS11, int32_t _M0L1rS12) {
  uint32_t _M0L6_2atmpS1263;
  int32_t _M0L6_2atmpS1265;
  uint32_t _M0L6_2atmpS1264;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1263 = _M0L1xS11 << (_M0L1rS12 & 31);
  _M0L6_2atmpS1265 = 32 - _M0L1rS12;
  _M0L6_2atmpS1264 = _M0L1xS11 >> (_M0L6_2atmpS1265 & 31);
  return _M0L6_2atmpS1263 | _M0L6_2atmpS1264;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S7,
  struct _M0TPB6Logger _M0L10_2ax__4934S10
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS8;
  moonbit_string_t _M0L8_2afieldS3009;
  int32_t _M0L6_2acntS3105;
  moonbit_string_t _M0L15_2a_2aarg__4935S9;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS8
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S7;
  _M0L8_2afieldS3009 = _M0L10_2aFailureS8->$0;
  _M0L6_2acntS3105 = Moonbit_object_header(_M0L10_2aFailureS8)->rc;
  if (_M0L6_2acntS3105 > 1) {
    int32_t _M0L11_2anew__cntS3106 = _M0L6_2acntS3105 - 1;
    Moonbit_object_header(_M0L10_2aFailureS8)->rc = _M0L11_2anew__cntS3106;
    moonbit_incref(_M0L8_2afieldS3009);
  } else if (_M0L6_2acntS3105 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS8);
  }
  _M0L15_2a_2aarg__4935S9 = _M0L8_2afieldS3009;
  if (_M0L10_2ax__4934S10.$1) {
    moonbit_incref(_M0L10_2ax__4934S10.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S10.$0->$method_0(_M0L10_2ax__4934S10.$1, (moonbit_string_t)moonbit_string_literal_73.data);
  if (_M0L10_2ax__4934S10.$1) {
    moonbit_incref(_M0L10_2ax__4934S10.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S10, _M0L15_2a_2aarg__4935S9);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S10.$0->$method_0(_M0L10_2ax__4934S10.$1, (moonbit_string_t)moonbit_string_literal_74.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS6) {
  void* _block_3269;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3269 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3269)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3269)->$0 = _M0L4selfS6;
  return _block_3269;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1229) {
  switch (Moonbit_object_tag(_M0L4_2aeS1229)) {
    case 4: {
      moonbit_decref(_M0L4_2aeS1229);
      return (moonbit_string_t)moonbit_string_literal_75.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1229);
      return (moonbit_string_t)moonbit_string_literal_76.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1229);
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS1229);
      return (moonbit_string_t)moonbit_string_literal_77.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1229);
      return (moonbit_string_t)moonbit_string_literal_78.data;
      break;
    }
  }
}

void* _M0IP48clawteam8clawteam5tools15write__to__file6ParamsPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1248
) {
  struct _M0TP48clawteam8clawteam5tools15write__to__file6Params* _M0L7_2aselfS1247 =
    (struct _M0TP48clawteam8clawteam5tools15write__to__file6Params*)_M0L11_2aobj__ptrS1248;
  return _M0IP48clawteam8clawteam5tools15write__to__file6ParamsPB6ToJson8to__json(_M0L7_2aselfS1247);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1246,
  int32_t _M0L8_2aparamS1245
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1244 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1246;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1244, _M0L8_2aparamS1245);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1243,
  struct _M0TPC16string10StringView _M0L8_2aparamS1242
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1241 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1243;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1241, _M0L8_2aparamS1242);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1240,
  moonbit_string_t _M0L8_2aparamS1237,
  int32_t _M0L8_2aparamS1238,
  int32_t _M0L8_2aparamS1239
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1236 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1240;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1236, _M0L8_2aparamS1237, _M0L8_2aparamS1238, _M0L8_2aparamS1239);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1235,
  moonbit_string_t _M0L8_2aparamS1234
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1233 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1235;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1233, _M0L8_2aparamS1234);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1262 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1261;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1260;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1156;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1259;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1258;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1257;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1256;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1155;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1255;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1254;
  _M0L6_2atmpS1262[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam5tools15write__to__file53____test__77726974655f746f5f66696c652e6d6274__0_2eclo);
  _M0L8_2atupleS1261
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1261)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1261->$0
  = _M0FP48clawteam8clawteam5tools15write__to__file53____test__77726974655f746f5f66696c652e6d6274__0_2eclo;
  _M0L8_2atupleS1261->$1 = _M0L6_2atmpS1262;
  _M0L8_2atupleS1260
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1260)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1260->$0 = 0;
  _M0L8_2atupleS1260->$1 = _M0L8_2atupleS1261;
  _M0L7_2abindS1156
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1156[0] = _M0L8_2atupleS1260;
  _M0L6_2atmpS1259 = _M0L7_2abindS1156;
  _M0L6_2atmpS1258
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1259
  };
  #line 398 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1257
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1258);
  _M0L8_2atupleS1256
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1256)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1256->$0 = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L8_2atupleS1256->$1 = _M0L6_2atmpS1257;
  _M0L7_2abindS1155
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1155[0] = _M0L8_2atupleS1256;
  _M0L6_2atmpS1255 = _M0L7_2abindS1155;
  _M0L6_2atmpS1254
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS1255
  };
  #line 397 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools15write__to__file48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1254);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1253;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1223;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1224;
  int32_t _M0L7_2abindS1225;
  int32_t _M0L2__S1226;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1253
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1223
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1223)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1223->$0 = _M0L6_2atmpS1253;
  _M0L12async__testsS1223->$1 = 0;
  #line 438 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1224
  = _M0FP48clawteam8clawteam5tools15write__to__file52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1225 = _M0L7_2abindS1224->$1;
  _M0L2__S1226 = 0;
  while (1) {
    if (_M0L2__S1226 < _M0L7_2abindS1225) {
      struct _M0TUsiE** _M0L8_2afieldS3013 = _M0L7_2abindS1224->$0;
      struct _M0TUsiE** _M0L3bufS1252 = _M0L8_2afieldS3013;
      struct _M0TUsiE* _M0L6_2atmpS3012 =
        (struct _M0TUsiE*)_M0L3bufS1252[_M0L2__S1226];
      struct _M0TUsiE* _M0L3argS1227 = _M0L6_2atmpS3012;
      moonbit_string_t _M0L8_2afieldS3011 = _M0L3argS1227->$0;
      moonbit_string_t _M0L6_2atmpS1249 = _M0L8_2afieldS3011;
      int32_t _M0L8_2afieldS3010 = _M0L3argS1227->$1;
      int32_t _M0L6_2atmpS1250 = _M0L8_2afieldS3010;
      int32_t _M0L6_2atmpS1251;
      moonbit_incref(_M0L6_2atmpS1249);
      moonbit_incref(_M0L12async__testsS1223);
      #line 439 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam5tools15write__to__file44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1223, _M0L6_2atmpS1249, _M0L6_2atmpS1250);
      _M0L6_2atmpS1251 = _M0L2__S1226 + 1;
      _M0L2__S1226 = _M0L6_2atmpS1251;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1224);
    }
    break;
  }
  #line 441 "E:\\moonbit\\clawteam\\tools\\write_to_file\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam5tools15write__to__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools15write__to__file34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1223);
  return 0;
}